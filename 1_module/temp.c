#include<linux/init.h>
#include <linux/module.h>

MODULE_LICENSE("Dual BSD/GPL");

static int mod_init(void)
{
	printk("Hello world\n");
	return 0;
}

static void mod_exit(void)
{
	printk("Bye\n");
}

module_init(mod_init);
module_exit(mod_exit);
