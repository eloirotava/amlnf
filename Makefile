# SPDX-License-Identifier: GPL-2.0
obj-m := amlnf_m3.o

amlnf_m3-y := \
	shims.o \
	nfc_map.o \
	phy/amlnand_init.o \
	phy/hw_controller.o \
	phy/chip.o \
	phy/chip_operation.o \
	phy/chipenv.o \
	phy/id_table.o \
	phy/new_nand.o \
	phy/boot_operation.o \
	dev/amlnf_ctrl.o \
	dev/amlnf_dev.o \
	dev/amlnf_env.o \
	dev/phydev.o \
	dev/key_secure_stubs.o \
	ntd/aml_ntdcore.o \
	ntd/aml_ntdpart.o \
	ntd/aml_ntd_blkdevs.o \
	block/aml_nftl_hw_interface.o \
	block/aml_nftl_init.o \
	block/aml_nftl_block.o \
	nftl/aml_nftl_core_20141222.o

ccflags-y := -I$(src)/include -I$(src)/stubs -I$(src)/stubs/linux -I$(src)/ntd -I$(src)/block
ccflags-y += -Wno-error -Wno-unused -Wno-declaration-after-statement
ccflags-y += -Wno-missing-prototypes -Wno-missing-declarations
ccflags-y += -DAML_NAND_NEW_OOB

KDIR ?= /lib/modules/$(shell uname -r)/build
ARCH ?= arm
CROSS_COMPILE ?=

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
