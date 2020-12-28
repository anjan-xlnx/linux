#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/slab.h>
#include "pcd_pf_dev.h"

#define MAX_DEVICES 10

struct pcd_dev_prv_data {
	struct pcd_dev_info pdev_info;
	char   *buf;
	dev_t	dev_num;
	struct cdev	cdev;
};

struct pcd_drv_prv_data {
	dev_t dev_num_base;
	struct class *cl;
	struct device *dev;
	int tot_devs;
};

struct pcd_drv_prv_data pcdrv_data;

int pcd_open (struct inode *inode, struct file *fp)
{
	printk("pcd open called\n");

	return -EPERM;
}

int pcd_release(struct inode *inode, struct file *fp)
{
	printk("pcd release called\n");
	return 0;
}

ssize_t pcd_write(struct file *fp, const char __user *buf, size_t count, loff_t *fpos)
{
	return -ENOMEM;
}

ssize_t pcd_read(struct file *fp, char __user *buf, size_t count, loff_t *fpos)
{
	return 0;
}

loff_t pcd_lseek(struct file *fp, loff_t off, int whence)
{
	return 0;
}

struct file_operations fops = {
	.open = pcd_open,
	.release = pcd_release,
	.write = pcd_write,
	.read = pcd_read,
	.llseek = pcd_lseek,
	.owner = THIS_MODULE,
};

int pcd_probe(struct platform_device *pdev)
{
	int ret;
	struct pcd_dev_info *pcd_dev_pf_data;
	struct pcd_dev_prv_data *pcd_dev_prv_data;

	pr_info("Device is detected\n");
	/* get platform device data */
//	pcd_dev_pf_data = pdev->dev.platform_data;
	pcd_dev_pf_data = (struct pcd_dev_info *)dev_get_platdata(&pdev->dev);
	if (!pcd_dev_pf_data)
		return -EINVAL;

	/* Dynamically allocated platform drv data */
	pcd_dev_prv_data = devm_kzalloc(&pdev->dev, sizeof(*pcd_dev_prv_data), GFP_KERNEL);
	if (!pcd_dev_prv_data) {
		pr_err("Failed to allocate memory :%d\n", __LINE__);
		return -ENOMEM;
	}

	/* copy all the data perm, size, serial num from dev setup file */
	pcd_dev_prv_data->pdev_info = *pcd_dev_pf_data;
	pr_info("Size is %d B, perm : %d serial : %s\n",
		pcd_dev_prv_data->pdev_info.size,
		pcd_dev_prv_data->pdev_info.perm,
		pcd_dev_prv_data->pdev_info.serial_num);

	/* allocate memory for buf */
	pcd_dev_prv_data->buf = devm_kmalloc(&pdev->dev, pcd_dev_prv_data->pdev_info.size,
					GFP_KERNEL);
	if (!pcd_dev_prv_data->buf) {
		pr_err("Failed to allocate memory for data\n");
		kfree(pcd_dev_prv_data);
		return -ENOMEM;
	}

	/* Get dev ID */
	pcd_dev_prv_data->dev_num = pcdrv_data.dev_num_base + pdev->id;
	/* cdev init and add */
	cdev_init(&pcd_dev_prv_data->cdev, &fops);
	pcd_dev_prv_data->cdev.owner = THIS_MODULE;
	ret = cdev_add(&pcd_dev_prv_data->cdev, pcd_dev_prv_data->dev_num, 1);
	if (ret) {
		pr_err("cdev add failed\n");
		goto cdev_fail;
	}


	/* create device */
	pcdrv_data.dev = device_create(pcdrv_data.cl, NULL, pcd_dev_prv_data->dev_num,
					NULL, "pcdev-%d", pdev->id);
	if (IS_ERR(pcdrv_data.dev)) {
		pr_err("Failed to create device\n");
		cdev_del(&pcd_dev_prv_data->cdev);
		ret = PTR_ERR(pcdrv_data.dev);
		goto cdev_fail;
		
	}

	/* Save drvier private data in the pdev */
//	pdev->dev.driver_data = pcd_dev_prv_data;
	dev_set_drvdata(&pdev->dev, pcd_dev_prv_data);
	pcdrv_data.tot_devs++;
	pr_info("Probe successful. Totl devs : %d\n", pcdrv_data.tot_devs);
	return 0;

cdev_fail:
	devm_kfree(&pdev->dev, pcd_dev_prv_data->buf);
	devm_kfree(&pdev->dev, pcd_dev_prv_data);
	return ret;
}


int pcd_remove(struct platform_device *pdev)
{
	struct pcd_dev_prv_data *pcd_dev_prv_data = dev_get_drvdata(&pdev->dev);

	pr_info("Pcd pf drv remove called\n");
	/* remove device */
	device_destroy(pcdrv_data.cl, pcd_dev_prv_data->dev_num);

	/* remove cdev */
	cdev_del(&pcd_dev_prv_data->cdev);
	/* remvove memory */
//	kfree(pcd_dev_prv_data->buf);
//	kfree(pcd_dev_prv_data);

	pcdrv_data.tot_devs--;
	pr_info("Remove successful. Totl devs : %d\n", pcdrv_data.tot_devs);
	return 0;
}

struct platform_driver pcd_pf_drv = {
	.probe = pcd_probe,
	.remove = pcd_remove,
	.driver = {
		.name = "pcd-platform-dev",
	},
};

static void __exit pcd_pf_drv_exit(void)
{
	platform_driver_unregister(&pcd_pf_drv);
	class_destroy(pcdrv_data.cl);
	unregister_chrdev_region(pcdrv_data.dev_num_base, MAX_DEVICES);
	pr_info("PCD PF driver exited\n");
}

static int __init pcd_pf_drv_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&pcdrv_data.dev_num_base, 0, MAX_DEVICES, "pcdevs");
	if (ret < 0)
		return ret;

	/* create class */
	pcdrv_data.cl = class_create(THIS_MODULE, "pcdclass");
	if (IS_ERR(pcdrv_data.cl)) {
		pr_info("Class creation failed\n");
		ret = PTR_ERR(pcdrv_data.cl);
		unregister_chrdev_region(pcdrv_data.dev_num_base, MAX_DEVICES);
		return ret;
	}

	platform_driver_register(&pcd_pf_drv);
	pr_info("PCD PF driver loaded\n");
	return 0;
}

module_init(pcd_pf_drv_init);
module_exit(pcd_pf_drv_exit);
MODULE_LICENSE("GPL");
