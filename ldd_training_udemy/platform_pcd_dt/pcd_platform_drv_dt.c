#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/of_device.h>
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

struct device_config {
	int cfg1;
	int cfg2;
};

enum cfg_num {
	CFG_TYPE1,
	CFG_TYPE2,
	CFG_TYPE3,
	CFG_TYPE4,
};

struct platform_device_id pcdev_ids[] = {
	[0] = {.name = "pcd-vA", .driver_data = CFG_TYPE3},
	[1] = {.name = "pcd-vB", .driver_data = CFG_TYPE1},
	[2] = {.name = "pcd-vC", .driver_data = CFG_TYPE2},
	[3] = {.name = "pcd-vD", .driver_data = CFG_TYPE4},
};

struct of_device_id dev_ids[] = {
	{.compatible = "PCD-vA1x", .data = (void *)CFG_TYPE1},
	{.compatible = "PCD-vB1x", .data = (void *)CFG_TYPE2},
	{.compatible = "PCD-vC1x", .data = (void *)CFG_TYPE3},
	{.compatible = "PCD-vD1x", .data = (void *)CFG_TYPE4},
	{},
};

struct device_config dev_cfg[] = {
	[CFG_TYPE1] = {.cfg1 = 1, .cfg2 = 11},
	[CFG_TYPE2] = {.cfg1 = 2, .cfg2 = 22},
	[CFG_TYPE3] = {.cfg1 = 3, .cfg2 = 33},
	[CFG_TYPE4] = {.cfg1 = 4, .cfg2 = 44},
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

struct pcd_dev_info* pcd_get_pf_data_from_dt(struct device *dev)
{
	struct pcd_dev_info* info;
	struct device_node *dev_node = dev->of_node;

	/* 1. Check if the probe is called because of DT or not */
	if (!dev_node)
		/* This gets set only if probe got invoked because of DT */
		return NULL;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(dev, "Failed to alloc memory\n");
		return ERR_PTR(-ENOMEM);
	}

	/* 2. Get serial number */
	if (of_property_read_string(dev_node, "cud,serial-num", &info->serial_num)) {
		dev_err(dev, "Failed to read serial num\n");
		return ERR_PTR(-EFAULT);
	}

	/* 3. Failed to read size */
	if (of_property_read_u32(dev_node, "cud,size", &info->size)) {
		dev_err(dev, "Failed to read size\n");
		return ERR_PTR(-EFAULT);
	}

	/* read perm */
	if (of_property_read_u32(dev_node, "cud,perm", &info->perm)) {
		dev_err(dev, "Failed to read permissions\n");
		return ERR_PTR(-EFAULT);
	}

	return info;
}

int pcd_probe(struct platform_device *pdev)
{
	int ret;
	struct pcd_dev_info *pcd_dev_pf_data;
	struct pcd_dev_prv_data *pcd_dev_prv_data;
	int driver_data;
	const struct of_device_id *match;

	dev_info(&pdev->dev, "Device is detected\n");

	match = of_match_device(of_match_ptr(dev_ids), &pdev->dev);

	if (match) {
		/* This checks if the probe got called by a matching
			setup code or through device dtree and returns */
		pcd_dev_pf_data = pcd_get_pf_data_from_dt(&pdev->dev);
		if (IS_ERR(pcd_dev_pf_data)) {
			return PTR_ERR(pcd_dev_pf_data);
		}
		/* Get the matched entry from of match table and then get the data */
#if 1
		driver_data = (int)match->data;
#else
		driver_data = (int)of_device_get_match_data(&pdev->dev);
#endif
	} else {
		/* This else case can execute in 2 cases,
			1. When CONFIG_OF is not set, then the of_match_ptr(dev_ids)
				would result in NULL, which when passed to of_match_device
				wourld return a NULL,
			2. When the probe gets called via a device setup code */
		/* get platform device data */
#if 0
		pcd_dev_pf_data = pdev->dev.platform_data;
#else
		pcd_dev_pf_data = (struct pcd_dev_info *)dev_get_platdata(&pdev->dev);
#endif
		driver_data = pdev->id_entry->driver_data;
	}

	if (!pcd_dev_pf_data) {
		/* The probe got called by device matching through a device
		setup code */
		dev_err(&pdev->dev, "Failed to find device platform data \n");
		return -EINVAL;
	}

	/* Dynamically allocated platform drv data */
	pcd_dev_prv_data = devm_kzalloc(&pdev->dev, sizeof(*pcd_dev_prv_data), GFP_KERNEL);
	if (!pcd_dev_prv_data) {
		dev_err(&pdev->dev, "Failed to allocate memory :%d\n", __LINE__);
		return -ENOMEM;
	}

	/* copy all the data perm, size, serial num from dev setup file */
	pcd_dev_prv_data->pdev_info = *pcd_dev_pf_data;
	dev_info(&pdev->dev, "Size is %d B, perm : %d serial : %s\n",
		pcd_dev_prv_data->pdev_info.size,
		pcd_dev_prv_data->pdev_info.perm,
		pcd_dev_prv_data->pdev_info.serial_num);

	dev_info(&pdev->dev, "Cfg item 1 and 2 are %d : %d \n",
			dev_cfg[driver_data].cfg1,
			dev_cfg[driver_data].cfg2);
	/* allocate memory for buf */
	pcd_dev_prv_data->buf = devm_kmalloc(&pdev->dev, pcd_dev_prv_data->pdev_info.size,
					GFP_KERNEL);
	if (!pcd_dev_prv_data->buf) {
		dev_err(&pdev->dev, "Failed to allocate memory for data\n");
		return -ENOMEM;
	}

	/* Get dev ID */
	pcd_dev_prv_data->dev_num = pcdrv_data.dev_num_base + pcdrv_data.tot_devs;
	/* cdev init and add */
	cdev_init(&pcd_dev_prv_data->cdev, &fops);
	pcd_dev_prv_data->cdev.owner = THIS_MODULE;
	ret = cdev_add(&pcd_dev_prv_data->cdev, pcd_dev_prv_data->dev_num, 1);
	if (ret) {
		dev_err(&pdev->dev, "cdev add failed\n");
		return ret;
	}


	/* create device */
	pcdrv_data.dev = device_create(pcdrv_data.cl, &pdev->dev, pcd_dev_prv_data->dev_num,
					NULL, "pcdev-%d", pcdrv_data.tot_devs);
	if (IS_ERR(pcdrv_data.dev)) {
		dev_err(&pdev->dev, "Failed to create device\n");
		cdev_del(&pcd_dev_prv_data->cdev);
		ret = PTR_ERR(pcdrv_data.dev);
		return ret;
	}

	/* Save drvier private data in the pdev */
#if 0
	pdev->dev.driver_data = pcd_dev_prv_data;
#else
	dev_set_drvdata(&pdev->dev, pcd_dev_prv_data);
#endif
	pcdrv_data.tot_devs++;
	dev_info(&pdev->dev, "Probe successful. Totl devs : %d\n", pcdrv_data.tot_devs);
	return 0;
}


int pcd_remove(struct platform_device *pdev)
{
	struct pcd_dev_prv_data *pcd_dev_prv_data = dev_get_drvdata(&pdev->dev);

	dev_info(&pdev->dev, "Pcd pf drv remove called\n");
	/* remove device */
	device_destroy(pcdrv_data.cl, pcd_dev_prv_data->dev_num);

	/* remove cdev */
	cdev_del(&pcd_dev_prv_data->cdev);
	/* remvove memory */
//	kfree(pcd_dev_prv_data->buf);
//	kfree(pcd_dev_prv_data);

	pcdrv_data.tot_devs--;
	dev_info(&pdev->dev, "Remove successful. Totl devs : %d\n", pcdrv_data.tot_devs);
	return 0;
}

struct platform_driver pcd_pf_drv = {
	.probe = pcd_probe,
	.remove = pcd_remove,
	.id_table = pcdev_ids,
	.driver = {
		/* this is redundant */
		.name = "pcd-platform-dev",
#if 0
		.of_match_table = dev_ids,
#else
		/* When CONFIG_OF is not set then all of_*() funcionts
			are dummy.
		Then of_match_ptr when CONFIG_OF is enabled, returns the ptr
			passed to it, when disabled, it just returns a NULL.
			So this can be used as a way to identify that.
		*/
		.of_match_table = of_match_ptr(dev_ids),
#endif
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

//module_platform_driver(pcd_pf_drv);

module_init(pcd_pf_drv_init);
module_exit(pcd_pf_drv_exit);
MODULE_LICENSE("GPL");
