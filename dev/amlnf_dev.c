// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Aml nftl dev
 *
 * (C) 2012 8
 */

#include "amlnf_glue.h"
#include "../include/phynand.h"

//#define CONFIG_OF
#ifndef AML_NAND_UBOOT
int boot_device_flag = -1;
#endif

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/

#ifndef AML_NAND_UBOOT
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
/*
static ssize_t nfdev_debug(struct class *class,struct class_attribute *attr,char *buf)
{
	//struct amlnf_dev* nf_dev = container_of(class, struct amlnf_dev, debug);

	//print_nftl_part(nf_dev -> aml_nftl_part);

	return 0;
}
*/
static struct class_attribute phydev_class_attrs[] = {
	__ATTR_NULL
};
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
/*
static int phydev_cls_suspend(struct device *dev, pm_message_t state)
{

	return 0;

}
*/

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
/*
static int phydev_cls_resume(struct device *dev, pm_message_t state)
{
		return 0;
}
*/
int amlnf_pdev_register(struct amlnand_phydev *phydev)
{
	int ret = 0;

	//phydev->dev.class = &phydev_class;
	dev_set_name(&phydev->dev, phydev->name, 0);
	dev_set_drvdata(&phydev->dev, phydev);
	ret = device_register(&phydev->dev);
	if (ret != 0) {
		aml_nand_msg("device register failed for %s", phydev->name);
		aml_nand_free(phydev);
		goto exit_error0;
	}

	phydev->cls.name = aml_nand_malloc(strlen((const char *)phydev->name)+8);
	snprintf((char *)phydev->cls.name, (MAX_DEVICE_NAME_LEN+8),
		 "%s%s", "phy_", (char *)(phydev->name));
	/* class_attrs removed in modern kernels; skip debug attrs for M3. */
	(void)phydev_class_attrs;
	ret = class_register(&phydev->cls);
	if (ret) {
		aml_nand_msg(" class register nand_class fail for %s", phydev->name);
		goto exit_error1;
	}

	return 0;

exit_error1:
	aml_nand_free(phydev->cls.name);
exit_error0:
	return ret;
}


#endif
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
 int amlnf_logic_init(unsigned int flag)
 {
	struct amlnand_phydev *phydev = NULL;
	int ret = 0;

	 aml_nand_msg("amlnand_add_nftl:");
	 //amlnand_show_dev_partition(aml_chip);
	 list_for_each_entry(phydev, &nphy_dev_list, list) {
		if (phydev != NULL) {
			if (strncmp((char *)phydev->name, NAND_BOOT_NAME, strlen((const char *)NAND_BOOT_NAME)))
				{
				ret = add_ntd_partitions(phydev);
				if (ret < 0) {
					aml_nand_msg("nand add nftl failed");
					goto exit_error;
					}
				}
			if (!strncmp((char *)phydev->name, NAND_BOOT_NAME, strlen((const char *)NAND_BOOT_NAME))) {
				ret = boot_device_register(phydev);
				if (ret < 0) {
					aml_nand_msg("boot device registe failed");
					goto exit_error;
					}
				}
			}
	}
 exit_error:

		return ret;
 }

static struct class_attribute aml_version =
	__ATTR(version, 0444, NULL, NULL);
static struct class_attribute aml_part_table =
	__ATTR(part_table, 0444, NULL, NULL);
static struct class_attribute aml_store_device =
	__ATTR(store_device, 0444, NULL, NULL);


/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int amlnf_dev_init(unsigned int flag)
{
	struct amlnand_phydev *phydev = NULL;
	struct class *aml_store_class = NULL;
	int ret = 0;

	(void)flag;
	(void)aml_version;
	(void)aml_part_table;
	(void)aml_store_device;

#ifndef AML_NAND_UBOOT
	list_for_each_entry(phydev, &nphy_dev_list, list) {
		if (phydev &&
					strncmp((char *)phydev->name, NAND_BOOT_NAME,
							strlen((const char *)NAND_BOOT_NAME))) {
			ret = amlnf_pdev_register(phydev);
			if (ret < 0) {
				aml_nand_msg("nand add nftl failed");
				return ret;
			}
		}
	}

	aml_store_class = class_create("aml_store");
	if (IS_ERR(aml_store_class)) {
		aml_nand_msg("amlnf_dev_init : class cread failed");
		return -1;
	}
#endif
	return 0;
}

#ifdef AML_NAND_UBOOT
static int get_boot_device()
{


	if (POR_SPI_BOOT()) {
		boot_device_flag = 0; // spi boot
		aml_nand_msg("SPI BOOT: boot_device_flag %d", boot_device_flag);
		return 0;
	}

	if (POR_NAND_BOOT()) {
		boot_device_flag = 1; // nand boot
		aml_nand_msg("NAND BOOT: boot_device_flag %d", boot_device_flag);
		return 0;
	}

	if (POR_EMMC_BOOT()) {
		boot_device_flag = -1;
		aml_nand_msg("EMMC BOOT: not init nand");
		return -1;
	}
	if (POR_CARD_BOOT()) {
		boot_device_flag = -1;
		aml_nand_msg("CARD BOOT: not init nand");
		return -1;
	}

	return;
}
struct amlnand_phydev *aml_phy_get_dev(char *name)
{
	struct amlnand_phydev *phy_dev = NULL;

	list_for_each_entry(phy_dev, &nphy_dev_list, list) {
			if (!strncmp((char *)phy_dev->name, name, MAX_DEVICE_NAME_LEN)) {
				aml_nand_dbg("nand get phy dev %s ", name);
				return phy_dev;
			}
		}

	aml_nand_msg("nand get phy dev %s	failed", name);

	return NULL;
}


struct amlnf_dev *aml_nftl_get_dev(char *name)
{
	struct amlnf_dev *nf_dev = NULL;

	list_for_each_entry(nf_dev, &nf_dev_list, list) {
		if (!strncmp((char *)nf_dev->name, name, MAX_NAND_PART_NAME_LEN)) {
			aml_nand_dbg("nand get nftl dev %s ", name);
			return nf_dev;
		}
	}

	aml_nand_msg("nand get nftl dev %s  failed", name);

	return NULL;
}
#endif

#ifdef CONFIG_OF
static const struct of_device_id amlogic_nand_dt_match[] = {
	{	.compatible = "amlogic,meson8b-nfc",
	},
	{	.compatible = "amlogic,aml_nand",
	},
	{},
};
static inline struct aml_nand_device   *aml_get_driver_data(
			struct platform_device *pdev)
{
	if (pdev->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_node(amlogic_nand_dt_match, pdev->dev.of_node);
		return (struct aml_nand_device *)match->data;
	}
	return NULL;
}

static int get_nand_platform(struct aml_nand_device *aml_nand_dev, struct platform_device *pdev)
{
	int ret;
	//const char *name,*propname;
	//struct property *prop;
	//const __be32 *list;
	//int size,config,index;
	//int selector,match_mode;
	//const char *select;
	//phandle phandle;
	int plat_num = 0;
	//unsigned char  plat_name[16];
	//struct device_node *np_config;
	//struct device_node *np_part;
	struct device_node *np = pdev->dev.of_node;

	if (pdev->dev.of_node) {
		of_node_get(np);
		ret = of_property_read_u32(np, "plat-num", &plat_num);
		if (ret) {
			/* Mainline meson NFC DT has no plat-num; continue. */
			plat_num = 1;
			ret = 0;
		}
	}
	aml_nand_dbg("plat_num %d ", plat_num);

	return 0;
//err:
//	return -1;
}

#endif

#ifndef AML_NAND_UBOOT

#define R_BOOT_DEVICE_FLAG  READ_CBUS_REG(ASSIST_POR_CONFIG)

#ifdef CONFIG_NAND_AML_M8
#define POR_BOOT_VALUE	((((R_BOOT_DEVICE_FLAG>>9)&1)<<2)|((R_BOOT_DEVICE_FLAG>>6)&3))
//#define POR_SPI_BOOT()		((POR_BOOT_VALUE == 5) || (POR_BOOT_VALUE == 4))
#define POR_SPI_BOOT()	((IS_MESON_M8_CPU)?((POR_BOOT_VALUE == 5) || (POR_BOOT_VALUE == 4)) : (POR_BOOT_VALUE == 5))
//#define POR_EMMC_BOOT()	 (POR_BOOT_VALUE == 3)
#define POR_EMMC_BOOT()	((IS_MESON_M8_CPU)?(POR_BOOT_VALUE == 3):((POR_BOOT_VALUE == 3) || (POR_BOOT_VALUE == 1)))
#else
#define POR_BOOT_VALUE	(R_BOOT_DEVICE_FLAG & 7)
#define POR_SPI_BOOT()		((POR_BOOT_VALUE == 5) || (POR_BOOT_VALUE == 4))
#define POR_EMMC_BOOT()	 (POR_BOOT_VALUE == 3)
#endif

#define POR_NAND_BOOT()	 ((POR_BOOT_VALUE == 7) || (POR_BOOT_VALUE == 6))
#define POR_CARD_BOOT()	(POR_BOOT_VALUE == 0)


#define SPI_BOOT_FLAG			0
#define NAND_BOOT_FLAG		1
#define EMMC_BOOT_FLAG		2
#define CARD_BOOT_FLAG		3
#define SPI_NAND_FLAG			4
#define SPI_EMMC_FLAG			5

/***
*boot_device_flag = 0 ; indicate spi+nand boot
*boot_device_flag = 1;  indicate nand  boot
***/
/* OOT bring-up: POR CBUS stubs always read 0; force NAND present. */
int check_storage_device(void)
{
	boot_device_flag = 1;
	return 0;
}

/* early_param("storage", ...) dropped: check_storage_device forces NAND. */

#endif
#ifdef AML_NAND_UBOOT
int amlnf_init(unsigned int flag)
#else
static int amlnf_init(struct platform_device *pdev)
#endif
{
	int ret = 0;
#ifndef AML_NAND_UBOOT
	unsigned int flag = 0;

	ret = check_storage_device();
	if (ret < 0) {
		aml_nand_msg("do not init nand");
		return 0;
	}

	INIT_LIST_HEAD(&nphy_dev_list);

#ifdef CONFIG_OF

	pdev->dev.platform_data = aml_get_driver_data(pdev);
	printk("===========================================");
	printk("%s:%d,nand device tree ok,dev-name:%s\n", __func__, __LINE__, dev_name(&pdev->dev));
#endif
	{
		ret = amlnf_map_nfc(pdev);
		if (ret)
			return ret;
	}
	ret = get_nand_platform(pdev->dev.platform_data, pdev);
#endif

#ifdef AML_NAND_UBOOT
	ret = amlnf_phy_init(unsigned int flag);
#else
	ret = amlnf_phy_init(flag, pdev);
#endif
	if (ret) {
		aml_nand_msg("nandphy_init failed and ret=0x%x", ret);
		goto exit_error0;
	}

	ret = amlnf_logic_init(flag);
	if (ret < 0) {
		aml_nand_msg("amlnf_add_nftl failed and ret=0x%x", ret);
		goto exit_error0;
	}

	ret = amlnf_dev_init(flag);
	if (ret < 0) {
		aml_nand_msg("amlnf_add_nftl failed and ret=0x%x", ret);
		goto exit_error0;
	}
	{
		ret = init_aml_nftl();
		if (ret)
			aml_nand_msg("init_aml_nftl failed ret=%d", ret);
	}

exit_error0:
	return 0;//ret;   //fix crash bug for error case.
}

#ifdef AML_NAND_UBOOT
int amlnf_exit(unsigned int flag)
#else
static void amlnf_exit(struct platform_device *pdev)
#endif
{
	(void)pdev;
}


#ifndef AML_NAND_UBOOT
static void amlnf_shutdown(struct platform_device *pdev)
{
	struct amlnand_phydev *phy_dev = NULL;

	if (check_storage_device() < 0) {
		aml_nand_msg("without nand");
		return;
	}

	list_for_each_entry(phy_dev, &nphy_dev_list, list) {
		if (phy_dev) {
			struct amlnand_chip *aml_chip = (struct amlnand_chip *)phy_dev->priv;

			amlnand_get_device(aml_chip, CHIP_SHUTDOWN);
			phy_dev->option |= NAND_SHUT_DOWN;
			amlnand_release_device(aml_chip);
		}
	}

	return;
}

/* driver device registration */
static struct platform_driver amlnf_driver = {
	.probe		= amlnf_init,
	.remove		= amlnf_exit,
	.shutdown		= amlnf_shutdown,
	.driver		= {
		.name	= DRV_AMLNFDEV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = amlogic_nand_dt_match,
	},
};

static int __init amlnf_module_init(void)
{
	return platform_driver_register(&amlnf_driver);
}

/*
 * There is deliberately no module_exit here, which makes the module
 * permanent: rmmod refuses it instead of taking the box down.
 *
 * The driver's .remove is empty -- it is "(void)pdev;" and nothing else --
 * so unloading tears down none of what probe built: the controller's
 * hrtimer stays armed, the NFTL kthreads keep running, the block devices
 * stay registered, the reboot notifiers stay in their list and the DMA
 * buffers leak.  The first rmmod proved it: the call returned 0 and then
 * the kernel jumped to 0xbf6523f4, an address inside the module it had
 * just freed, from __timer_delete_sync.
 *
 * Writing a correct teardown is its own piece of work and is exactly where
 * use-after-free bugs live, so it does not get bolted on in the middle of
 * proving the read/write path.  Until it exists, reload by rebooting -- it
 * takes about forty seconds and it is honest.
 */
module_init(amlnf_module_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AML NAND TEAM");
MODULE_DESCRIPTION("aml nand flash driver");
#endif


