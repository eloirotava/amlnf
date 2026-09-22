// SPDX-License-Identifier: GPL-2.0
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

void __iomem *IO_NAND_BASE;
EXPORT_SYMBOL(IO_NAND_BASE);

/*
 * The real device.  The vendor's struct amlnand_chip embeds a "struct device"
 * BY VALUE and the 3.10 code assigned the platform device into it with
 * "aml_chip->device = pdev->dev".  That copy cannot be used for anything: its
 * devres_head is a copy of the original's list head, so the first devm_*()
 * call on it splices an entry between two lists and the release walk later
 * runs off into freed memory -- which is the shape of a hang with no output
 * at all.  Everything that needs a device uses this pointer instead.
 */
struct device *amlnf_dev;
EXPORT_SYMBOL(amlnf_dev);

/*
 * Write guard.  On this box the NAND still holds a working LibreELEC install
 * and the per-unit vendor keys, and the vendor init path is not passive: if
 * chipenv or boot_operation decide the metadata needs repair they rewrite it.
 * Until the port is proven, every write and erase is refused at the two
 * functions that issue the opcodes.  Set readonly=0 only deliberately.
 */
int amlnf_readonly = 1;
module_param_named(readonly, amlnf_readonly, int, 0644);
MODULE_PARM_DESC(readonly,
	"1 (default) refuses every program/erase with -EROFS; 0 allows writes");
EXPORT_SYMBOL(amlnf_readonly);

/*
 * Separate, and deliberately NOT released by readonly=0.
 *
 * Ordinary program/erase of a data block is undone by writing the block
 * again.  These two are different in kind:
 *
 *   - marking a block bad writes zeros into the out-of-band area of the
 *     block's first page.  Clearing it again means erasing that block,
 *     which is possible, but it also erases the only on-chip evidence of
 *     which blocks the factory marked bad.  That list is not derivable
 *     from anything else.
 *
 *   - the vendor metadata (nbbt, ncnf, nkey, nenv) is the bad-block
 *     table, the chip configuration, the per-unit keys and the U-Boot
 *     environment.  nkey in particular cannot be reconstructed by
 *     anything, including the Amlogic USB Burning Tool.
 *
 * So these stay refused even with writes allowed, until someone turns
 * them loose on purpose.
 */
int amlnf_allow_markbad;
module_param_named(allow_markbad, amlnf_allow_markbad, int, 0644);
MODULE_PARM_DESC(allow_markbad,
	"0 (default) refuses marking blocks bad and rewriting the BBT");
EXPORT_SYMBOL(amlnf_allow_markbad);

int amlnf_allow_meta;
module_param_named(allow_meta, amlnf_allow_meta, int, 0644);
MODULE_PARM_DESC(allow_meta,
	"0 (default) refuses writing vendor metadata (nbbt/ncnf/nkey/nenv)");
EXPORT_SYMBOL(amlnf_allow_meta);

/*
 * Which NTD partitions get an FTL attached.  Comma separated, "all" for
 * every one.  See the long comment in block/aml_nftl_init.c for why the
 * default is a single throwaway partition.
 */
char amlnf_parts[64] = "nfcache";
module_param_string(parts, amlnf_parts, sizeof(amlnf_parts), 0644);
MODULE_PARM_DESC(parts,
	"comma separated NTD partitions to attach the FTL to, or \"all\"");
EXPORT_SYMBOL(amlnf_parts);

bool amlnf_part_enabled(const char *name)
{
	const char *p = amlnf_parts;
	size_t len;

	if (!name)
		return false;
	if (!strcmp(amlnf_parts, "all"))
		return true;

	len = strlen(name);
	while (*p) {
		const char *comma = strchr(p, ',');
		size_t n = comma ? (size_t)(comma - p) : strlen(p);

		if (n == len && !strncmp(p, name, n))
			return true;
		if (!comma)
			break;
		p = comma + 1;
	}
	return false;
}
EXPORT_SYMBOL(amlnf_part_enabled);

/*
 * Cache de escrita da FTL.  O tree do fabricante fixa
 * NFTL_DONT_CACHE_DATA (0) em aml_nftl_init.c, e com ele a escrita fica em
 * ~1,8 MB/s contra 25 MB/s de leitura.  O blob e fechado, entao o valor
 * oposto e suposicao informada, nao documentacao -- por isso vira
 * parametro, para medir os dois lados em vez de discutir.
 */
int amlnf_use_cache;
module_param_named(use_cache, amlnf_use_cache, int, 0644);
MODULE_PARM_DESC(use_cache,
	"0 (padrao, igual ao vendor) nao guarda escrita em cache; 1 tenta guardar");
EXPORT_SYMBOL(amlnf_use_cache);

static atomic_t amlnf_refused = ATOMIC_INIT(0);

void amlnf_refuse_write(const char *what, unsigned int page)
{
	int n = atomic_inc_return(&amlnf_refused);

	/*
	 * Loud for the first few, then quiet: a retry loop in the vendor code
	 * must not turn into the printk storm that took the box down before.
	 */
	if (n <= 8)
		pr_warn("amlnf_m3: READONLY refusou %s pagina %u (%d ate agora)\n",
			what, page, n);
	else if (n == 9)
		pr_warn("amlnf_m3: READONLY seguira recusando em silencio\n");
}
EXPORT_SYMBOL(amlnf_refuse_write);

int amlnf_refused_count(void)
{
	return atomic_read(&amlnf_refused);
}
EXPORT_SYMBOL(amlnf_refused_count);

static struct clk *amlnf_core_clk;
static struct clk *amlnf_device_clk;

/* Called early from amlnf probe before phy init. */
int amlnf_map_nfc(struct platform_device *pdev)
{
	struct resource *res;
	int ret;
	unsigned long rate;

	amlnf_dev = &pdev->dev;

	if (!IO_NAND_BASE) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (res)
			IO_NAND_BASE = ioremap(res->start, resource_size(res));
		else if (pdev->dev.of_node)
			IO_NAND_BASE = of_iomap(pdev->dev.of_node, 0);

		if (!IO_NAND_BASE) {
			pr_err("amlnf_m3: failed to map NFC regs\n");
			return -ENOMEM;
		}
		pr_info("amlnf_m3: NFC mapped at %px (phys %pa)\n",
			IO_NAND_BASE, res ? &res->start : NULL);
	}

	/* Mainline meson8b-nfc: clocks "core" + "device" — required or FIFO spins forever. */
	if (!amlnf_core_clk) {
		amlnf_core_clk = devm_clk_get(&pdev->dev, "core");
		if (IS_ERR(amlnf_core_clk)) {
			pr_err("amlnf_m3: clk core: %ld\n", PTR_ERR(amlnf_core_clk));
			return PTR_ERR(amlnf_core_clk);
		}
		amlnf_device_clk = devm_clk_get(&pdev->dev, "device");
		if (IS_ERR(amlnf_device_clk)) {
			pr_err("amlnf_m3: clk device: %ld\n", PTR_ERR(amlnf_device_clk));
			return PTR_ERR(amlnf_device_clk);
		}
		ret = clk_prepare_enable(amlnf_core_clk);
		if (ret)
			return ret;
		ret = clk_set_rate(amlnf_device_clk, 200000000);
		if (ret)
			pr_warn("amlnf_m3: clk_set_rate device: %d (continuing)\n", ret);
		ret = clk_prepare_enable(amlnf_device_clk);
		if (ret) {
			clk_disable_unprepare(amlnf_core_clk);
			return ret;
		}
		rate = clk_get_rate(amlnf_device_clk);
		pr_info("amlnf_m3: clocks on, device_clk=%lu Hz\n", rate);
	}

	return 0;
}
EXPORT_SYMBOL(amlnf_map_nfc);
