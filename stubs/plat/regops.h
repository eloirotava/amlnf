/* stub: Meson register helpers — regs are virtual __iomem addresses */
#ifndef _STUB_PLAT_REGOPS_H
#define _STUB_PLAT_REGOPS_H
#include <linux/io.h>
#include <linux/types.h>

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

#define READ_CBUS_REG(reg)		(0U)
#define WRITE_CBUS_REG(reg, val)	do { (void)(val); } while (0)
#define SET_CBUS_REG_MASK(reg, mask)	do { (void)(mask); } while (0)
#define CLEAR_CBUS_REG_MASK(reg, mask)	do { (void)(mask); } while (0)
#define WRITE_CBUS_REG_BITS(reg, val, start, len) do { \
	(void)(val); (void)(start); (void)(len); } while (0)
#define READ_CBUS_REG_BITS(reg, start, len) (0U)

#endif
