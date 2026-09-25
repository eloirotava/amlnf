// SPDX-License-Identifier: GPL-2.0
/*
 * Symbols the FTL object still references by their 3.10 names.  The
 * object is linked into this module, so nothing here is exported.
 */
#include <linux/kernel.h>
#include <linux/string.h>

#undef printk
int printk(const char *fmt, ...);
void __memzero(void *ptr, __kernel_size_t n);

int printk(const char *fmt, ...)
{
	va_list args;
	int r;

	va_start(args, fmt);
	r = vprintk(fmt, args);
	va_end(args);
	return r;
}

void __memzero(void *ptr, __kernel_size_t n)
{
	memset(ptr, 0, n);
}
