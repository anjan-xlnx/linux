#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>

#define DEV_CNT	1
#define MINOR_DEV_START	0
#define DEV_CNT_NAME "pcd_devs"
#define DEV_NAME "pcd"

#define DATA_BUF_SIZE 512	

char data_buf[DATA_BUF_SIZE];

struct test_ctx {
	dev_t	dev_num;
	struct class *cls;
	struct device *dev;
	struct cdev   cdev;
} context;

int pcd_open (struct inode *inode, struct file *fp)
{
	printk("pcd open called\n");
	return 0;
}

int pcd_release(struct inode *inode, struct file *fp)
{
	printk("pcd release called\n");
	return 0;
}

ssize_t pcd_write(struct file *fp, const char __user *buf, size_t count, loff_t *fpos)
{
	printk("Write requested for %zu bytes. Latest fpos is %lld\n", count, *fpos);
	/* adjust the count */
	if ((*fpos + count) > DATA_BUF_SIZE)
		count = DATA_BUF_SIZE - *fpos;

	if (!count) {
		printk("Device is out of memory\n");
		return -ENOMEM;
	}

	if (copy_from_user(&data_buf[*fpos], buf, count))
		return -EFAULT;

	*fpos += count;
	printk("Total written bytes : %zu latest fpos is %lld\n", count, *fpos);
	return count;
}

ssize_t pcd_read(struct file *fp, char __user *buf, size_t count, loff_t *fpos)
{
	printk("read requested for %zu bytes. Latest fpos is %lld\n", count, *fpos);

	/* adjust the count */
	if ((*fpos + count) > DATA_BUF_SIZE)
		count = DATA_BUF_SIZE - *fpos;

	if (copy_to_user(buf, &data_buf[*fpos], count))
		return -EFAULT;

	*fpos += count;
	printk("Total read bytes : %zu latest fpos is %lld\n", count, *fpos);
	return count;
}

loff_t pcd_lseek(struct file *fp, loff_t off, int whence)
{
	loff_t temp;

	printk("leek fpos is %lld\n", fp->f_pos);
	switch (whence) {
	case SEEK_SET:
		if ((off > DATA_BUF_SIZE) || (off < 0))
			return -EINVAL;
		fp->f_pos = off;
		break;
	case SEEK_CUR:
		temp = fp->f_pos + off;

		if ((temp > DATA_BUF_SIZE) || (temp < 0))
			return -EINVAL;
		fp->f_pos += off;
		break;
	case SEEK_END:
		temp = DATA_BUF_SIZE + off;
		if ((temp > DATA_BUF_SIZE) || (temp < 0))
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	printk("leek new fpos is %lld\n", fp->f_pos);
	return fp->f_pos;
}

struct file_operations fops = {
	.open = pcd_open,
	.release = pcd_release,
	.write = pcd_write,
	.read = pcd_read,
	.llseek = pcd_lseek,
	.owner = THIS_MODULE,
};

int init_char_drv(struct test_ctx *ctx)
{
	int ret;

	/* allocate device numbers */
	ret = alloc_chrdev_region(&ctx->dev_num, MINOR_DEV_START,
			DEV_CNT, DEV_CNT_NAME);
	if (ret < 0) {
		goto alloc_dev_num_fail;
	}

	/* CDEV init to tell the VFS about the inode info*/
	cdev_init(&ctx->cdev, &fops);
	ctx->cdev.owner = THIS_MODULE;

	if ((ret = cdev_add(&ctx->cdev, ctx->dev_num, DEV_CNT)) < 0) {
		printk("Failed to add CDEV\n");
		goto cdev_fail;
	}

	/* create class and device, to generate an uevent for udev to create device file in /dev */

	ctx->cls = class_create(THIS_MODULE, "pcd_class");
	if (IS_ERR(ctx->cls)) {
		printk("Failed to create class \n");
		goto cls_fail;
	}
	
	/* create device */
	ctx->dev = device_create(ctx->cls, NULL /* parent */, ctx->dev_num,
			NULL /* private data */, DEV_NAME);
	if (IS_ERR(ctx->dev)) {
                printk("Failed to create device \n");
                goto dev_fail;
        }
	return 0;

dev_fail:
	class_destroy(ctx->cls);
cls_fail:
	cdev_del(&ctx->cdev);
cdev_fail:
	unregister_chrdev_region(ctx->dev_num, DEV_CNT);
alloc_dev_num_fail:
	return ret;
}

static int __init pcd_init(void)
{
	printk("PCD init called\n");

	init_char_drv(&context);
	return 0;
}

static void __exit pcd_exit(void)
{
	printk("Calling exit\n");
	device_destroy(context.cls, context.dev_num);
	class_destroy(context.cls);
	cdev_del(&context.cdev);
	unregister_chrdev_region(context.dev_num, DEV_CNT);
}

module_init(pcd_init)
module_exit(pcd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("me");
MODULE_DESCRIPTION("Anjis first module");
