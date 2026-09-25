/* SPDX-License-Identifier: GPL-2.0 */
/*
 * What the vendor code expects from the 3.10 Amlogic tree (<mach/...>,
 * <plat/regops.h>, the key provider API, Android early suspend), reduced
 * to what a current kernel needs to build it.
 */
#ifndef __AMLNF_COMPAT_H
#define __AMLNF_COMPAT_H

#include <linux/io.h>
#include <linux/sizes.h>
#include <linux/types.h>

/* NFC registers: mapped in probe (nfc_map.c) */
extern void __iomem *IO_NAND_BASE;
#define IO_CBUS_BASE ((void __iomem *)0)

/* register helpers of <plat/regops.h> */
static inline u32 aml_read_reg32(void __iomem *addr)
{
	return readl(addr);
}
static inline void aml_write_reg32(void __iomem *addr, u32 val)
{
	writel(val, addr);
}
static inline void aml_set_reg32_mask(void __iomem *addr, u32 mask)
{
	writel(readl(addr) | mask, addr);
}
static inline void aml_clr_reg32_mask(void __iomem *addr, u32 mask)
{
	writel(readl(addr) & ~mask, addr);
}
static inline void aml_set_reg32_bits(void __iomem *addr, u32 val, u32 start, u32 len)
{
	u32 mask = ((1U << len) - 1U) << start;
	u32 v = readl(addr);

	v = (v & ~mask) | ((val << start) & mask);
	writel(v, addr);
}

/*
 * The CBUS accessors are deliberately empty: the only CBUS register the
 * vendor code touches is the NAND clock, which the clock framework now
 * owns (nfc_map.c).  Reads return 0.
 */
#define READ_CBUS_REG(reg)		(0U)
#define WRITE_CBUS_REG(reg, val)	do { (void)(val); } while (0)
#define SET_CBUS_REG_MASK(reg, mask)	do { (void)(mask); } while (0)
#define CLEAR_CBUS_REG_MASK(reg, mask)	do { (void)(mask); } while (0)
#define WRITE_CBUS_REG_BITS(reg, val, start, len) do { \
	(void)(val); (void)(start); (void)(len); } while (0)
#define READ_CBUS_REG_BITS(reg, start, len) (0U)

/* <mach/am_regs.h> */
/* Minimal POR / clock register names referenced by amlnf */
#define ASSIST_POR_CONFIG	0x1f90
#define HHI_NAND_CLK_CNTL	0x1097

/* Android early suspend, gone upstream */
struct early_suspend {};
#define register_early_suspend(x) do { } while (0)
#define unregister_early_suspend(x) do { } while (0)

/* <linux/amlogic/securitykey.h>: no key provider on this kernel */
struct aml_keybox_provider;

typedef struct aml_keybox_provider {
	char *name;
	int32_t (*read)(struct aml_keybox_provider *p, uint8_t *buf, int len, int flags);
	int32_t (*write)(struct aml_keybox_provider *p, uint8_t *buf, int len);
	void *priv;
} aml_keybox_provider_t;


static inline int aml_keybox_provider_register(aml_keybox_provider_t *p)
{
	(void)p;
	return 0;
}

static inline aml_keybox_provider_t *aml_keybox_provider_get(const char *name)
{
	(void)name;
	return NULL;
}

#endif
