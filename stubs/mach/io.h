#ifndef _STUB_MACH_IO_H
#define _STUB_MACH_IO_H
#include <linux/io.h>
#include <linux/types.h>

/* Virtual base for NFC regs; set in probe via of_iomap / ioremap. */
extern void __iomem *IO_NAND_BASE;
#ifndef IO_CBUS_BASE
#define IO_CBUS_BASE ((void __iomem *)0)
#endif

#endif
