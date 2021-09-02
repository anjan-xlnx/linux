#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include "scull.h"

struct scull_dev *devs;
int scull_major = 0;


void scull_trim(struct scull_dev *dev)
{
	struct scull_qset *tmp = dev->qset, *prev;
	int i;

	while (tmp) {
		if (tmp->data) {
			for (i = 0; i < dev->qset_cnt; i++)
				kfree(tmp->data[i]);
			kfree(tmp->data);
		}
		prev = tmp;
		tmp = tmp->next;
		kfree(prev);
	}
	dev->size  = 0;
	dev->qset = NULL;
}

int scull_open(struct inode *inode, struct file *fp)
{
	struct scull_dev *dev;

	dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	fp->private_data = dev;

	if ((fp->f_flags & O_ACCMODE) == O_WRONLY)
		scull_trim(dev);

	return 0;
}

struct scull_qset *scull_follow(struct scull_dev *dev, int num)
{
	struct scull_qset *qset = dev->qset;

	if (!qset) {
		dev->qset = kmalloc(sizeof (*qset), GFP_KERNEL);
		if (!dev->qset)
			return NULL;
		qset = dev->qset;
		memset(qset, 0, sizeof(*qset));
	}

	while (num) {
		if (!qset->next) {
			qset->next = kmalloc(sizeof (*qset), GFP_KERNEL);
			if (!qset->next)
				return NULL;

			memset(qset->next, 0, sizeof(*qset->next));
		}
		qset = qset->next;
		num--;
	}
	return qset;
}

ssize_t scull_read(struct file *fp, char __user* dest, size_t len,
	loff_t *f_pos)
{
	struct scull_dev *dev = fp->private_data;
	struct scull_qset *qset;
	ssize_t ret = 0;
	unsigned long qset_nr;
	int quantum_nr, quanta_ofs;
	int q_set_size = SCULL_QSET * SCULL_QUNATUM;

	if (dev->size <= *f_pos)
		return ret; // End of file

	if (*f_pos + len > dev->size)
		len = dev->size - *f_pos;

	/* Find 1. which Qset
		2. Which Quantum
		3. Position in the Quantum
	*/
	qset_nr = (long)*f_pos / q_set_size;
	quantum_nr = ((long) *f_pos % q_set_size) / SCULL_QUNATUM;
	quanta_ofs = (long) *f_pos % SCULL_QUNATUM;
	qset = scull_follow(dev, qset_nr);
	if (!qset || !qset->data || !qset->data[quantum_nr])
		return 0; // EOF

	if (len > (SCULL_QUNATUM - quanta_ofs)) // Limit read from only one Quanta at a time
		len = SCULL_QUNATUM - quanta_ofs;

	if (copy_to_user(dest, qset->data[quantum_nr] + quanta_ofs, len))
		return -EFAULT;

	*f_pos += len;
	return len;
}

ssize_t scull_write(struct file *fp, const char __user* buf, size_t len,
	loff_t *f_pos)
{
	struct scull_dev *dev = fp->private_data;
	struct scull_qset *qset;
	unsigned long qset_nr;
	int quantum_nr, quanta_ofs;
	int q_set_size = SCULL_QSET * SCULL_QUNATUM;

	/* Find 1. which Qset
		2. Which Quantum
		3. Position in the Quantum
	*/
	qset_nr = (long)*f_pos / q_set_size;
	quantum_nr = ((long) *f_pos % q_set_size) / SCULL_QUNATUM;
	quanta_ofs = (long) *f_pos % SCULL_QUNATUM;
	qset = scull_follow(dev, qset_nr);

	if (!qset)
		return -ENOMEM;

	if (!qset->data) {
		qset->data = kmalloc(sizeof(char *) * SCULL_QSET,
						GFP_KERNEL);
		if (!qset->data)
			return -ENOMEM;
		memset(qset->data, 0, sizeof(char *) * SCULL_QSET);
	}

	if (!qset->data[quantum_nr])
		qset->data[quantum_nr] = kmalloc(SCULL_QUNATUM, GFP_KERNEL);
		if (!qset->data[quantum_nr])
			return -ENOMEM;

	/* write data in only 1 quanta at a time */
	if (len > SCULL_QUNATUM - quanta_ofs)
		len = SCULL_QUNATUM - quanta_ofs;

	if (copy_from_user(qset->data[quantum_nr] + quanta_ofs,
				buf, len))
		return -EFAULT;

	*f_pos += len;
	if (dev->size < *f_pos)
		dev->size = *f_pos;

	return len;
}

int scull_release (struct inode *inode, struct file *fp)
{
	return 0;
}

struct file_operations fops =
{
	.owner = THIS_MODULE,
	.open = scull_open,
	.read = scull_read,
	.write = scull_write,
	.release = scull_release,
};

int scull_cdev_init(int idx, struct scull_dev *dev)
{
	int rc = 0;

	cdev_init(&dev->cdev, &fops);
	dev->cdev.owner = THIS_MODULE;
	rc = cdev_add(&dev->cdev, MKDEV(scull_major, idx), 1);
	if (rc)
		printk("Error [%d] while adding cdev to dev idx %d\n", rc, idx);
	return rc;
}

void scull_cleanup(void)
{
	int i = 0;

	/* remove cdevs */
	for (i = 0; i < SCULL_DEV_CNT; i++) {
		/* trim it */
		scull_trim(devs + i);
		cdev_del(&devs[i].cdev);
	}
	unregister_chrdev_region(MKDEV(scull_major, 0), SCULL_DEV_CNT);
	kfree(devs);
}

static int __init scull_init(void)
{
	int rc = 0, i;
	dev_t dev;

	rc = alloc_chrdev_region(&dev, 0, SCULL_DEV_CNT,
			SCULL_DEV_NAME);
	if (rc < 0) {
		printk("Failed to dynamically alloc dev numbers\n");
		return -EFAULT;
	}

	scull_major = MAJOR(dev);
	printk("Anji Major number is %d\n", MAJOR(dev));

	devs = kmalloc(SCULL_DEV_CNT * sizeof (*devs), GFP_KERNEL);
	if (!devs) {
		printk("Failed to allocate memory for devs\n");
		goto fail;
	}
	memset(devs, 0, sizeof (*devs) * SCULL_DEV_CNT);

	for (i = 0; i < SCULL_DEV_CNT; i++) {
		devs[rc].quantum = SCULL_QUNATUM;
		devs[rc].qset_cnt = SCULL_QSET;
		rc = scull_cdev_init(i, &devs[i]);
		if (rc)
			/* clear stuff so far created */
			goto fail;
	}
	printk("Scull Loaded\n");
	return 0;

fail:
	scull_cleanup();
	return -EFAULT;
}

static void __exit scull_exit(void)
{
	scull_cleanup();
	printk("Scull exited\n");
}

module_init(scull_init);
module_exit(scull_exit);

MODULE_LICENSE("GPL");
