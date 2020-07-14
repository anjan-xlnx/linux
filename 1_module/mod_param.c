#include <linux/init.h>
#include <linux/module.h>

static int cnt = 1;

module_param(cnt, int, S_IRUGO);
static int arr[5], num;
module_param_array(arr, int, &num, S_IRUGO);


static int p;
module_param_named(price, p, int, S_IRUGO);

static char buf[10] = "Pulihora";
module_param_string(item, buf, 10, S_IRUGO);
static char *str = "world";
// The following one allocates memory for the given module parameter
// and assigns it to str
module_param(str, charp, S_IRUGO);

static int __init mod_init(void)
{
	int i = 0;

	printk("Array parameter cnt is %d\n", num);
	for (i = 0; i < num; i++)
		printk("%d ", arr[i]);
	printk("\n");
	for (i = 0; i < cnt; i++)
		printk("%s price is %d Rs. \n", buf, p);
	for (i = 0; i < cnt; i++)
		printk("Hello-%s\n", str);

	return 0;
}

static void __exit mod_exit(void)
{
	printk("Exit\n");
}


MODULE_AUTHOR("Anji");
MODULE_DESCRIPTION("Temp2");
module_init(mod_init);
module_exit(mod_exit);
