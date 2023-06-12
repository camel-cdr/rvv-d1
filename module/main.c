#include <linux/module.h>
#include <linux/printk.h>

extern u64 rvv_enable(void);

int
init_module(void)
{
	pr_info("init rvv\n");
	pr_info("vlenb=%llu\n", rvv_enable());
	return 0;
}

void
cleanup_module(void)
{
	pr_info("cleanup rvv\n");
}

MODULE_LICENSE("GPL");
