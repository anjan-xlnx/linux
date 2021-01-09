#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/gpio/consumer.h>

#define MAX_DEVICES 10


struct gpio_dev_data {
	char lable[20];
	struct gpio_desc *desc;
};

struct gpio_drv_data {
	struct class *gpio_cls;
	int tot_devs;
	struct device **devs;
} gpio_drv;

#if 0
struct file_operations fops = {
	.open = pcd_open,
	.release = pcd_release,
	.write = pcd_write,
	.read = pcd_read,
	.llseek = pcd_lseek,
	.owner = THIS_MODULE,
};
#endif

ssize_t direction_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gpio_dev_data *dev_data = dev_get_drvdata(dev);
	int dir;

	dir = gpiod_get_direction(dev_data->desc);
	if (dir < 0)
		return dir;

	return sprintf(buf, "%s\n", dir? "in" : "out");
}

ssize_t direction_store(struct device *dev, struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct gpio_dev_data *dev_data = dev_get_drvdata(dev);
	int ret = -EINVAL;

	if (sysfs_streq(buf, "in"))
		ret = gpiod_direction_input(dev_data->desc);
	else if (sysfs_streq(buf, "out")) {
		ret = gpiod_direction_output(dev_data->desc, 0);
	}

	return ret? 0 : count;
}

ssize_t value_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gpio_dev_data *dev_data = dev_get_drvdata(dev);
	int val;

	val = gpiod_get_value(dev_data->desc);
	return sprintf(buf, "%d\n", val);
}

ssize_t value_store(struct device *dev, struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct gpio_dev_data *dev_data = dev_get_drvdata(dev);
	long val;
	int  ret;

	ret = kstrtol(buf, 0, &val);
	if (ret)
		return ret;
	gpiod_set_value(dev_data->desc, val);
	return count;

}

ssize_t label_show (struct device *dev, struct device_attribute *attr, char *buf)
{

	struct gpio_dev_data *dev_data = dev_get_drvdata(dev);

	return sprintf(buf, "%s", dev_data->lable);
}

/* The following macro creates a device_attibute structure, 
	with show function as param_show, store function as param_store */
DEVICE_ATTR_RW(direction); // creates a var dev_attr_<param>, i.e, dev_attr_direction
DEVICE_ATTR_RW(value);
DEVICE_ATTR_RO(label);

static struct attribute *sysfs_attrs[] = {
	&dev_attr_direction.attr,
	&dev_attr_value.attr,
	&dev_attr_label.attr,
	NULL,
};

static struct attribute_group attr_grp = {
	.attrs = sysfs_attrs,
};

const struct attribute_group *gpio_attr_grp[] = {
	&attr_grp,
	NULL,
};

int gpio_sysfs_probe(struct platform_device *pdev)
{
	struct device_node *parent = pdev->dev.of_node;
	struct device_node *child = NULL;
	struct gpio_dev_data *dev_data;
	const char *name;
	int i = 0, ret;

	gpio_drv.tot_devs = of_get_child_count(parent);

	dev_info(&pdev->dev, "Total devices are %d\n", gpio_drv.tot_devs);

	gpio_drv.devs = devm_kzalloc(&pdev->dev, sizeof(struct device *) * gpio_drv.tot_devs, GFP_KERNEL);
	if (!gpio_drv.devs) {
		dev_err(&pdev->dev, "Failed to alloc memory\n");
		return -ENOMEM;
	}

	for_each_available_child_of_node(parent, child) {
		dev_data = devm_kzalloc(&pdev->dev, sizeof(*dev_data), GFP_KERNEL);
		if (!dev_data) {
			dev_err(&pdev->dev, "Failed to alloc mem\n");
			return -ENOMEM;
		}

		if (of_property_read_string(child, "label", &name)) {
			dev_warn(&pdev->dev, "Missing labels\n");
			snprintf(dev_data->lable, sizeof(dev_data->lable),
						"unknowngpio-%d", i);
			
		} else {
			strcpy(dev_data->lable, name);
			dev_info(&pdev->dev, "Label is %s\n", dev_data->lable);
		}

		/* GPIO info is refersented as <fn>-gpios = <&gpio1 PinNo State>; as below
			in the Device tree.
                gpio2 {
                        label = "gpio2.3";
                        bone-gpios = <&gpio2 2 GPIO_ACTIVE_HIGH>;
                };
		Here, "bone" in bone-gpios is the connection id, which should be passed
			to the below function call
		*/

		dev_data->desc = devm_fwnode_get_gpiod_from_child(&pdev->dev, "bone",
			&child->fwnode, GPIOD_ASIS, dev_data->lable);
		if (IS_ERR(dev_data->desc)) {
			ret = PTR_ERR(dev_data->desc);
			if (ret == -ENOENT) {
				dev_err(&pdev->dev, "No GPIO assigned to the function\n");
				return ret;
			}
			return ret;
		}

		ret = gpiod_direction_output(dev_data->desc, 0);
		if (ret) {
			dev_err(&pdev->dev, "Failed to set the pin\n");
			return ret;
		}

		/* create a device and sysfs files for value, label and direction sysfs files */
		/* this can be done in 2 steps.
			1. Create a device using device_create
			2. Create a sysfs group
				(or)
			use the below API which can create a device and sysfs groups */

		/* As GIPOs here are controlled by sys files, we don't need to create a device file in /dev.
			A 0 passed to the dev_t dev_num would not create a device in this api */

		/* the following creates dev files in sysfs along with attrs */
		gpio_drv.devs[i] = device_create_with_groups(gpio_drv.gpio_cls, &pdev->dev, 0,
				(void *)dev_data, gpio_attr_grp,
				dev_data->lable);
		if (IS_ERR(gpio_drv.devs[i])) {
			dev_err(&pdev->dev,"failed to create syfs files\n");
			return PTR_ERR(gpio_drv.devs[i]);
		}
		i++;
	}
	return 0;
}


int gpio_sysfs_remove(struct platform_device *pdev)
{
	int i = 0;

	/* Since in probe dev_t was not passed and so device was not created,
		device_destroy can not be used, instead of device_unregister
		clears stuff */
	for (i = 0; i < gpio_drv.tot_devs; i++) {
		device_unregister(gpio_drv.devs[i]);
	}
	pr_info("Drv remove called\n");
	return 0;
}


struct of_device_id match_ids[] = {
	{.compatible = "bbb,gpio-sysfs"},
	{},
};

struct platform_driver gpio_sysfs_drv = {
	.probe = gpio_sysfs_probe,
	.remove = gpio_sysfs_remove,
	.driver = {
		.name = "gpio-sysfs",
		.of_match_table = of_match_ptr(match_ids),
	},
};

static void __exit gpio_sysfs_drv_exit(void)
{
	platform_driver_unregister(&gpio_sysfs_drv);
	class_destroy(gpio_drv.gpio_cls);
	pr_info("PCD PF driver exited\n");
}

static int __init gpio_syfs_drv_init(void)
{
//	int ret;

	/* create class */
	gpio_drv.gpio_cls = class_create(THIS_MODULE, "gpio-class");
	if (IS_ERR(gpio_drv.gpio_cls)) {
		pr_info("Class creation failed\n");
		return PTR_ERR(gpio_drv.gpio_cls);
	}

	platform_driver_register(&gpio_sysfs_drv);
	pr_info("PCD PF driver loaded\n");
	return 0;
}

//module_platform_driver(pcd_pf_drv);

module_init(gpio_syfs_drv_init);
module_exit(gpio_sysfs_drv_exit);
MODULE_LICENSE("GPL");
