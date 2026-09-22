// SPDX-License-Identifier: GPL-2.0
/* Legacy symbols the FTL blob still references. */
#include <linux/kernel.h>
#include <linux/string.h>

#undef printk
int printk(const char *fmt, ...)
{
	va_list args;
	int r;

	va_start(args, fmt);
	r = vprintk(fmt, args);
	va_end(args);
	return r;
}
EXPORT_SYMBOL_GPL(printk);

void __memzero(void *ptr, __kernel_size_t n)
{
	memset(ptr, 0, n);
}
EXPORT_SYMBOL_GPL(__memzero);
