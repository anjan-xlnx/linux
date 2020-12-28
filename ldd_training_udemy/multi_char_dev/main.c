#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>

#define DEV_CNT	4
#define MINOR_DEV_START	0
#define DEV_CNT_NAME "pcd_devs"
#define DEV_NAME "pcd"

#define DATA_BUF_SIZE 512	


#define DEV1_BUF_SIZE 512
#define DEV2_BUF_SIZE 1024
#define DEV3_BUF_SIZE 1536
#define DEV4_BUF_SIZE 2048

char data_buf1[DEV1_BUF_SIZE];
char data_buf2[DEV2_BUF_SIZE];
char data_buf3[DEV3_BUF_SIZE];
char data_buf4[DEV4_BUF_SIZE];


enum {
	RD_ONLY = 1,
	WR_ONLY = 0x10,
	RD_WR = 0x11,
};

struct dev_prv_data {
	int permissions;
	int size;
	char *data_buf;
	struct cdev   cdev;
};

struct test_ctx {
	int	tot_devs;
	dev_t	dev_num;
	struct class *cls;
	struct device *dev;
	struct dev_prv_data dprv_data[DEV_CNT];
};

struct test_ctx context = {
	.tot_devs = DEV_CNT,
	.dprv_data = {
		[0] = {
			.permissions = RD_ONLY,
			.size = DEV1_BUF_SIZE,
			.data_buf = data_buf1,
		},
		[1] = {
			.permissions = WR_ONLY,
			.size = DEV2_BUF_SIZE,
			.data_buf = data_buf2,
		},
		[2] = {
			.permissions = RD_WR,
			.size = DEV3_BUF_SIZE,
			.data_buf = data_buf3,
		},
		[3] = {
			.permissions = RD_WR,
			.size = DEV4_BUF_SIZE,
			.data_buf = data_buf4,
		},
	},
};

int chk_permission(int dev_perm, int access_mode)
{
	if (dev_perm == RD_WR)
		return 0;
	if ((dev_perm == RD_ONLY) && (access_mode & FMODE_READ) &&
				(!(access_mode & FMODE_WRITE)))
		return 0;
	if ((dev_perm == WR_ONLY) && (!(access_mode & FMODE_READ)) &&
				(access_mode & FMODE_WRITE))
		return 0;
	return -EPERM;
}

int pcd_open (struct inode *inode, struct file *fp)
{
	struct dev_prv_data *prv_data;

	printk("pcd open called\n");

	/* getting minor and major numbers from inode */
	printk("Device opened is %d:%d\n",
			MAJOR(inode->i_rdev), MINOR(inode->i_rdev));

	/* get prv data using cdev */
	prv_data = container_of (inode->i_cdev, struct dev_prv_data, cdev);
	fp->private_data = prv_data;

	if (chk_permission(prv_data->permissions, fp->f_mode)) {
		pr_info("Permission check failed\n");
		return -EPERM;
	}
	return 0;
}

int pcd_release(struct inode *inode, struct file *fp)
{
	printk("pcd release called\n");
	return 0;
}

ssize_t pcd_write(struct file *fp, const char __user *buf, size_t count, loff_t *fpos)
{
	struct dev_prv_data *prv_data = (struct dev_prv_data *) fp->private_data;

	printk("Write requested for %zu bytes. Latest fpos is %lld\n", count, *fpos);
	/* adjust the count */
	if ((*fpos + count) > prv_data->size)
		count = prv_data->size - *fpos;

	if (!count) {
		printk("Device is out of memory\n");
		return -ENOMEM;
	}

	if (copy_from_user(&prv_data->data_buf[*fpos], buf, count))
		return -EFAULT;

	*fpos += count;
	printk("Total written bytes : %zu latest fpos is %lld\n", count, *fpos);
	return count;
}

ssize_t pcd_read(struct file *fp, char __user *buf, size_t count, loff_t *fpos)
{
	struct dev_prv_data *prv_data = (struct dev_prv_data *) fp->private_data;

	printk("read requested for %zu bytes. Latest fpos is %lld\n", count, *fpos);

	/* adjust the count */
	if ((*fpos + count) > prv_data->size)
		count = prv_data->size - *fpos;

	if (copy_to_user(buf, &prv_data->data_buf[*fpos], count))
		return -EFAULT;

	*fpos += count;
	printk("Total read bytes : %zu latest fpos is %lld\n", count, *fpos);
	return count;
}

loff_t pcd_lseek(struct file *fp, loff_t off, int whence)
{
	struct dev_prv_data *prv_data = (struct dev_prv_data *) fp->private_data;
	loff_t temp;

	printk("leek fpos is %lld\n", fp->f_pos);
	switch (whence) {
	case SEEK_SET:
		if ((off > prv_data->size) || (off < 0))
			return -EINVAL;
		fp->f_pos = off;
		break;
	case SEEK_CUR:
		temp = fp->f_pos + off;

		if ((temp > prv_data->size) || (temp < 0))
			return -EINVAL;
		fp->f_pos += off;
		break;
	case SEEK_END:
		temp = prv_data->size + off;
		if ((temp > prv_data->size) || (temp < 0))
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
	int ret, i;

	/* allocate device numbers */
	ret = alloc_chrdev_region(&ctx->dev_num, MINOR_DEV_START,
			DEV_CNT, DEV_CNT_NAME);
	if (ret < 0) {
		goto alloc_dev_num_fail;
	}

	/* create a class in /sys/class for udev uevents */
	ctx->cls = class_create(THIS_MODULE, "pcd_class");
	if (IS_ERR(ctx->cls)) {
		printk("Failed to create class \n");
		ret = PTR_ERR(ctx->cls);
		goto cls_fail;
	}

	for (i = 0; i < DEV_CNT; i++) {
		/* CDEV init to tell the VFS about the inode info*/
		printk("Major: %d Minor: %d\n", MAJOR(ctx->dev_num + i),
						MINOR(ctx->dev_num + i));
		cdev_init(&ctx->dprv_data[i].cdev, &fops);
		ctx->dprv_data[i].cdev.owner = THIS_MODULE;

		if ((ret = cdev_add(&ctx->dprv_data[i].cdev, ctx->dev_num + i, 1)) < 0) {
			printk("Failed to add CDEV\n");
			goto cdev_fail;
		}

		/* create device */
		ctx->dev = device_create(ctx->cls, NULL /* parent */, ctx->dev_num + i,
				NULL /* private data */, DEV_NAME"-%d", (i + 1));
		if (IS_ERR(ctx->dev)) {
        	        printk("Failed to create device \n");
			ret = PTR_ERR(ctx->cls);
        	        goto dev_fail;
        	}
	}
	return 0;

cdev_fail:
dev_fail:
	for (; i; i--) {
		/* Device destroy takes care of un reigstered devices. If a device number
			which is not registerd via device_create passed in device destroy,
			nothing happens. Similarly for cdev too */
		device_destroy(ctx->cls, ctx->dev_num + i);
		cdev_del(&ctx->dprv_data[i].cdev);
	}
	class_destroy(ctx->cls);
cls_fail:
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
	int i;
	printk("Calling exit\n");

	for (i = 0; i < DEV_CNT; i++) {
		device_destroy(context.cls, context.dev_num + i);
		cdev_del(&context.dprv_data[i].cdev);
	}
	class_destroy(context.cls);
	unregister_chrdev_region(context.dev_num, DEV_CNT);
}

module_init(pcd_init)
module_exit(pcd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("me");
MODULE_DESCRIPTION("Anjis first module");
