#include <linux/module.h>
#include <linux/platform_device.h>
#include "pcd_pf_dev.h"

struct pcd_dev_info pcd_pl_dev_info[] = {
	[0] = {.size = 512, .perm = RD_WR, .serial_num = "pcd-pl-dev-1"},
	[1] = {.size = 512, .perm = RD_ONLY, .serial_num = "pcd-pl-dev-2"},
	[2] = {.size = 512, .perm = RD_ONLY, .serial_num = "pcd-pl-dev-3"},
	[3] = {.size = 512, .perm = RD_WR, .serial_num = "pcd-pl-dev-4"},
};

void pcd_dev_release(struct device *dev)
{
	printk("PF device released\n");
}

struct platform_device pcd_dev1 = {
	.name = "pcd-vA",
	.id = 0,
	.dev = {
		.platform_data = &pcd_pl_dev_info[0],
		.release = pcd_dev_release,
	},
};

struct platform_device pcd_dev2 = {
	.name = "pcd-vB",
	.id = 1,
	.dev = {
		.platform_data = &pcd_pl_dev_info[1],
		.release = pcd_dev_release,
	},
};


struct platform_device pcd_dev3 = {
	.name = "pcd-vC",
	.id = 2,
	.dev = {
		.platform_data = &pcd_pl_dev_info[2],
		.release = pcd_dev_release,
	},
};

struct platform_device pcd_dev4 = {
	.name = "pcd-vD",
	.id = 3,
	.dev = {
		.platform_data = &pcd_pl_dev_info[3],
		.release = pcd_dev_release,
	},
};

struct platform_device *pf_devs[] = {
	&pcd_dev1,
	&pcd_dev2,
	&pcd_dev3,
	&pcd_dev4
};

static __init int pcd_pl_dev_init(void)
{
//	platform_device_register(&pcd_dev1);
//	platform_device_register(&pcd_dev2);
	platform_add_devices(pf_devs, ARRAY_SIZE(pf_devs));
	pr_info("Devices are registered\n");
	return 0;
}

static __exit void pcd_pl_dev_exit(void)
{
	platform_device_unregister(&pcd_dev1);
	platform_device_unregister(&pcd_dev2);
	platform_device_unregister(&pcd_dev3);
	platform_device_unregister(&pcd_dev4);
	pr_info("Devices are removed\n");
}

module_init(pcd_pl_dev_init);
module_exit(pcd_pl_dev_exit);

MODULE_LICENSE("GPL");
