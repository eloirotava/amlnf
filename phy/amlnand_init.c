/*****************************************************************
**
**  Copyright (C) 2012 Amlogic,Inc.  All rights reserved
**
**        Filename : driver_uboot.c
**        Revision : 1.001
**        Author: Benjamin Zhao
**        Description:
**			amlnand_init,  mainly init nand phy driver.
**
**
*****************************************************************/

#include "../include/phynand.h"

struct amlnand_chip * aml_nand_chip = NULL;

static void show_nand_driver_version(void)
{
	aml_nand_msg("Nand PHY driver Version: %d.%02d.%03d.%04d (c) 2013 Amlogic Inc.", (DRV_PHY_VERSION >> 24)&0xff,
		(DRV_PHY_VERSION >> 16)&0xff,(DRV_PHY_VERSION >> 8)&0xff,(DRV_PHY_VERSION)&0xff);
}

#ifdef AML_NAND_UBOOT
int amlnf_phy_init(unsigned char flag)
#else
	int amlnf_phy_init(unsigned char flag, struct platform_device *pdev )
#endif
{
	struct amlnand_chip *aml_chip = NULL;

	int ret = 0;

	//show nand version here
	show_nand_driver_version();

	//malloc
	aml_chip = aml_nand_malloc(sizeof(struct amlnand_chip));
	if (aml_chip == NULL) {
		aml_nand_msg("malloc failed for aml_chip:%x", sizeof(struct amlnand_chip));
		ret = -NAND_MALLOC_FAILURE;
		goto exit_error1;
	}

	memset(aml_chip , 0, sizeof(struct amlnand_chip));
	memset(aml_chip->reserved_blk, 0xff, RESERVED_BLOCK_CNT);
	aml_chip->init_flag = flag;
	aml_nand_msg("amlnf_phy_init : amlnf init flag %d",aml_chip->init_flag);
	aml_chip->nand_status = NAND_STATUS_NORMAL;
	aml_nand_chip = aml_chip;

#ifndef AML_NAND_UBOOT
#endif
#ifdef CONFIG_OF
	printk("amlnf_m3: pinctrl get...\n");
	aml_chip->nand_pinctrl = devm_pinctrl_get(amlnf_dev);
	if (IS_ERR(aml_chip->nand_pinctrl)) {
		printk("get pinctrl error\n");
		return PTR_ERR(aml_chip->nand_pinctrl);
	}
	/*
	 * Mainline meson8b-nfc DT only exposes pinctrl-names = "default".
	 * Vendor 3.10 used nand_rb_mod / nand_norb_mod / dummy.
	 */
	aml_chip->nand_rbstate = pinctrl_lookup_state(aml_chip->nand_pinctrl, "default");
	if (IS_ERR(aml_chip->nand_rbstate)) {
		printk("lookup pinctrl default (rb) failed\n");
		devm_pinctrl_put(aml_chip->nand_pinctrl);
		return PTR_ERR(aml_chip->nand_rbstate);
	}
	aml_chip->nand_norbstate = aml_chip->nand_rbstate;
	aml_chip->nand_idlestate = aml_chip->nand_rbstate;
	pinctrl_select_state(aml_chip->nand_pinctrl, aml_chip->nand_norbstate);
	printk("amlnf_m3: pinctrl ok\n");
#endif

	//Step 1: init hw controller
	printk("amlnf_m3: hwcontroller_init...\n");
	ret = amlnand_hwcontroller_init(aml_chip);
	if(ret < 0){
		aml_nand_msg("aml_hw_controller_init failed");
		ret = -NAND_FAILED;
		goto exit_error1;
	}
	printk("amlnf_m3: hwcontroller_init ok\n");

	//Step 2: init aml_chip operation
	printk("amlnf_m3: init_operation...\n");
	ret = amlnand_init_operation(aml_chip);
	if(ret < 0){
		aml_nand_msg("chip detect failed and ret:%x", ret);
		ret = -NAND_FAILED;
		goto exit_error1;
	}
	printk("amlnf_m3: init_operation ok\n");

	//Step 3: get nand flash and get hw flash information
	printk("amlnf_m3: chip_init...\n");
	ret = amlnand_chip_init(aml_chip);
	if(ret < 0){
		aml_nand_msg("chip detect failed and ret:%x", ret);
		ret = -NAND_FAILED;
		goto exit_error1;
	}
	printk("amlnf_m3: chip_init ok\n");

	//Step 4: get device configs
	printk("amlnf_m3: get_dev_configs...\n");
	ret = amlnand_get_dev_configs(aml_chip);
	if(ret < 0){
		aml_nand_msg("get device configs failed and ret:%x", ret);
		ret = -NAND_READ_FAILED;
		goto exit_error0;
	}
	printk("amlnf_m3: get_dev_configs ok\n");

	//Step 5: register nand device, and config device information
	printk("amlnf_m3: phydev_init...\n");
	ret = amlnand_phydev_init(aml_chip);
	if(ret < 0){
		aml_nand_msg("register nand device failed and ret:%x", ret);
		ret = -NAND_READ_FAILED;
		goto exit_error0;
	}
	printk("amlnf_m3: phydev_init ok\n");

	return ret;

exit_error1:
	aml_nand_free(aml_chip);
exit_error0:

	return ret;
}

