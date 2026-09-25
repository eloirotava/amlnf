/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Glue between the vendor code and a current kernel: the calls that
 * cross files and were declared ad hoc in each of them.
 */
#ifndef __AMLNF_GLUE_H
#define __AMLNF_GLUE_H

#include <linux/platform_device.h>
#include <linux/time64.h>
#include <linux/types.h>

struct class;
struct aml_nftl_blk;
struct aml_nftl_dev;

/* nfc_map.c: controller mapping and the write/partition guards */
int amlnf_map_nfc(struct platform_device *pdev);
bool amlnf_part_enabled(const char *name);
void amlnf_refuse_write(const char *what, unsigned int page);
int amlnf_refused_count(void);

/* ntd/aml_ntd_blkdevs.c */
int amlnf_class_register(struct class *cls);
void amlnf_ktime_get_ts(struct timespec64 *ts);

/* block/ */
int init_aml_nftl(void);
void cleanup_aml_nftl(void);
int aml_blktrans_initialize(struct aml_nftl_blk *nftl_blk,
							struct aml_nftl_dev *nftl_dev, u64 offset, u64 size);

#endif
