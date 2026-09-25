// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Aml nftl init
 *
 * (C) 2012 8
 */


//#include <linux/mtd/mtd.h>
//#include <linux/mtd/blktrans.h>

#include "amlnf_glue.h"
#include "aml_nftl_block.h"

/* Defined in nfc_map.c. */
extern char amlnf_parts[64];
extern int amlnf_use_cache;
extern int print_discard_page_map(struct aml_nftl_part_t *part);
extern int is_phydev_off_adjust(void);
extern int aml_nftl_start(void *priv, void *cfg, struct aml_nftl_part_t **ppart, uint64_t size, unsigned int erasesize, unsigned int writesize, unsigned int oobavail, char *name, int no, char type, int init_flag);
extern uint32 gc_all(struct aml_nftl_part_t *part);
extern uint32 gc_one(struct aml_nftl_part_t *part);
extern void print_nftl_part(struct aml_nftl_part_t *part);
extern int part_param_init(struct aml_nftl_part_t *part, uint16 start_block, uint32_t logic_sects, uint32_t backup_cap_in_sects, int init_flag);
extern uint32 is_no_use_device(struct aml_nftl_part_t *part, uint32 size);
extern uint32 create_part_list_first(struct aml_nftl_part_t *part, uint32 size);
extern uint32 create_part_list(struct aml_nftl_part_t *part);
//extern int nand_test(struct aml_nftl_dev *nftl_dev,unsigned char flag,uint32 blocks);
extern int part_param_exit(struct aml_nftl_part_t *part);
extern int cache_init(struct aml_nftl_part_t *part);
extern int cache_exit(struct aml_nftl_part_t *part);
extern uint32 get_vaild_blocks(struct aml_nftl_part_t *part, uint32 start_block, uint32 blocks);
extern uint32 __nand_read(struct aml_nftl_part_t *part, uint32 start_sector, uint32 len, unsigned char *buf);
extern uint32 __nand_write(struct aml_nftl_part_t *part, uint32 start_sector, uint32 len, unsigned char *buf, int sync_flag);
extern uint32 __nand_discard(struct aml_nftl_part_t *part, uint32 start_sector, uint32 len, int sync_flag);
extern uint32 __nand_flush_write_cache(struct aml_nftl_part_t *part);
extern uint32 __nand_flush_discard_cache(struct aml_nftl_part_t *part);
extern uint32 __nand_write_pair_page(struct aml_nftl_part_t *part);
extern uint32 __check_mapping(struct aml_nftl_part_t *part, uint64_t offset, uint64_t size);
extern uint32 __discard_partition(struct aml_nftl_part_t *part, uint64_t offset, uint64_t size);
extern void print_free_list(struct aml_nftl_part_t *part);
extern void print_block_invalid_list(struct aml_nftl_part_t *part);
extern int nand_discard_logic_page(struct aml_nftl_part_t *part, uint32 page_no);
extern  int get_adjust_block_num(void);
extern int aml_nftl_erase_part(struct aml_nftl_part_t *part);
extern int aml_nftl_set_status(struct aml_nftl_part_t *part, unsigned char status);
uint32 _nand_read(struct aml_nftl_dev *nftl_dev, unsigned long start_sector, unsigned int len, unsigned char *buf);
uint32 _nand_write(struct aml_nftl_dev *nftl_dev, unsigned long  start_sector, unsigned int len, unsigned char *buf);
uint32 _nand_discard(struct aml_nftl_dev *nftl_dev, unsigned long start_sector, unsigned int len);
uint32 _nand_flush_write_cache(struct aml_nftl_dev *nftl_dev);
uint32 _nand_flush_discard_cache(struct aml_nftl_dev *nftl_dev);
uint32 _nand_write_pair_page(struct aml_nftl_dev *nftl_dev);
uint32 _check_mapping(struct aml_nftl_dev *nftl_dev, uint64_t offset, uint64_t size);
uint32 _discard_partition(struct aml_nftl_dev *nftl_dev, uint64_t offset, uint64_t size);
uint32 _blk_nand_flush_write_cache(struct aml_nftl_blk *nftl_blk);
uint32 _blk_nand_write(struct aml_nftl_blk *nftl_blk, unsigned long start_sector, unsigned int  len, unsigned char *buf);
uint32 _blk_nand_discard(struct aml_nftl_blk *nftl_blk, unsigned long start_sector, unsigned int len);
uint32 _blk_nand_read(struct aml_nftl_blk *nftl_blk, unsigned long start_sector, unsigned int len, unsigned char *buf);

void *aml_nftl_malloc(uint32 size);
void aml_nftl_free(const void *ptr);
//int aml_nftl_dbg(const char * fmt,args...);

//static ssize_t discard_page(struct class *class, struct class_attribute *attr, const char *buf);

static struct class_attribute nftl_class_attrs[] = {
};

int aml_nftl_initialize(struct aml_nftl_dev *nftl_dev, int no);


/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
void *aml_nftl_malloc(uint32 size)
{
	return kzalloc(size, GFP_KERNEL);
}

void aml_nftl_free(const void *ptr)
{
	kfree(ptr);
}

//int aml_nftl_dbg(const char * fmt,args...)
//{
//    //return printk(fmt,##__VA_ARGS__);
//    //return printk(KERN_WARNING "AML NFTL warning: %s: line:%d " fmt "\n",  __func__, __LINE__, ##__VA_ARGS__);
//    return printk( fmt,## args);
//    //return printk(KERN_ERR "AML NFTL error: %s: line:%d " fmt "\n", __func__, __LINE__, ##__VA_ARGS__);
//}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int aml_nftl_initialize(struct aml_nftl_dev *nftl_dev, int no)
{
	struct ntd_info *ntd = nftl_dev->ntd;
	int error = 0;

	//uint32_t phys_erase_shift;
	uint32_t ret;

	if (ntd->oobsize < MIN_BYTES_OF_USER_PER_PAGE)
		return -EPERM;


	nftl_dev->nftl_cfg.nftl_use_cache = amlnf_use_cache ? 1
															: NFTL_DONT_CACHE_DATA;
	nftl_dev->nftl_cfg.nftl_support_gc_read_reclaim = SUPPORT_GC_READ_RECLAIM;
	nftl_dev->nftl_cfg.nftl_support_wear_leveling = SUPPORT_WEAR_LEVELING;
	nftl_dev->nftl_cfg.nftl_need_erase = NFTL_ERASE;
	if (!is_phydev_off_adjust()) {
		nftl_dev->nftl_cfg.nftl_part_reserved_block_ratio = 8;
	} else {
		nftl_dev->nftl_cfg.nftl_part_reserved_block_ratio = 10;
	}
	nftl_dev->nftl_cfg.nftl_part_adjust_block_num = get_adjust_block_num();
	printk("adjust_block_num : %d,reserved_block_ratio %d\n", nftl_dev->nftl_cfg.nftl_part_adjust_block_num, nftl_dev->nftl_cfg.nftl_part_reserved_block_ratio);
	nftl_dev->nftl_cfg.nftl_min_free_block_num = MIN_FREE_BLOCK_NUM;
	nftl_dev->nftl_cfg.nftl_min_free_block = MIN_FREE_BLOCK;
	nftl_dev->nftl_cfg.nftl_gc_threshold_free_block_num = GC_THRESHOLD_FREE_BLOCK_NUM;
	nftl_dev->nftl_cfg.nftl_gc_threshold_ratio_numerator = GC_THRESHOLD_RATIO_NUMERATOR;
	nftl_dev->nftl_cfg.nftl_gc_threshold_ratio_denominator = GC_THRESHOLD_RATIO_DENOMINATOR;
	nftl_dev->nftl_cfg.nftl_max_cache_write_num = MAX_CACHE_WRITE_NUM;

	ret = aml_nftl_start((void *)nftl_dev, &nftl_dev->nftl_cfg, &nftl_dev->aml_nftl_part, ntd->size, ntd->blocksize, ntd->pagesize, ntd->oobsize, ntd->name, no, 0, nftl_dev->init_flag);
	if (ret != 0)
	{
	//if(memcmp(ntd->name, "nfcache", 7)==0)
	{
	if (nftl_dev->init_flag == 0)
	{
		aml_nftl_set_status(nftl_dev->aml_nftl_part, 1);
	}
		   // return ret;
	}
	}
	nftl_dev->size = aml_nftl_get_part_cap(nftl_dev->aml_nftl_part);
	nftl_dev->read_data = _nand_read;
	nftl_dev->write_data = _nand_write;
	nftl_dev->discard_data = _nand_discard;
	nftl_dev->flush_write_cache = _nand_flush_write_cache;
	nftl_dev->flush_discard_cache = _nand_flush_discard_cache;
	nftl_dev->write_pair_page = _nand_write_pair_page;
	nftl_dev->check_mapping = _check_mapping;
	nftl_dev->discard_partition = _discard_partition;
	if (no < 0) {
		return ret; // for erase init FTL part
	}

	//setup class
	if (memcmp(ntd->name, "nfcode", 6) == 0)
	{
		nftl_dev->debug.name = kzalloc(strlen((const char *)AML_NFTL1_MAGIC)+1, GFP_KERNEL);
	strcpy((char *)nftl_dev->debug.name, (char *)AML_NFTL1_MAGIC);
	/* class_attrs removed in 6.x */ (void)nftl_class_attrs;
		error = amlnf_class_register(&nftl_dev->debug);
		if (error)
			printk(" class register nand_class fail!\n");
	}

	if (memcmp(ntd->name, "nfdata", 6) == 0)
	{
		nftl_dev->debug.name = kzalloc(strlen((const char *)AML_NFTL2_MAGIC)+1, GFP_KERNEL);
	strcpy((char *)nftl_dev->debug.name, (char *)AML_NFTL2_MAGIC);
	/* class_attrs removed in 6.x */ (void)nftl_class_attrs;
		error = amlnf_class_register(&nftl_dev->debug);
		if (error)
			printk(" class register nand_class fail!\n");
	}

	return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int aml_blktrans_initialize(struct aml_nftl_blk *nftl_blk, struct aml_nftl_dev *nftl_dev, uint64_t offset, uint64_t size)
{
	uint64_t offset_t, size_t;

	offset_t = offset >> 9;
	size_t = size >> 9;

	nftl_blk->nftl_dev = nftl_dev;
	/*
	 * nftl_blk->lock was declared and then never assigned anywhere in the
	 * tree, so aml_nftl_release() was doing mutex_lock(NULL) on every
	 * close of the block device.  It went unnoticed because the box used
	 * to die before anything got as far as closing one.  The device and
	 * the block views of it share the one lock, which is what the rest of
	 * this file already assumes.
	 */
	nftl_blk->lock = nftl_dev->aml_nftl_lock;

	if (offset_t < nftl_dev->size)
	{
			nftl_blk->offset = offset_t;
	} else
	{
			printk("aml_blktrans_initialize2 %llx  %llx\n", offset_t, nftl_dev->size);
	return 1;
	}

	if ((nftl_blk->offset + size_t) <= nftl_dev->size)
	{
			nftl_blk->size = size_t;
	} else
	{
			nftl_blk->size = nftl_dev->size - nftl_blk->offset;
	}

	nftl_blk->read_data = _blk_nand_read;
	nftl_blk->write_data = _blk_nand_write;
	nftl_blk->discard_data = _blk_nand_discard;
	nftl_blk->flush_write_cache = _blk_nand_flush_write_cache;

	//printk("aml_blktrans_initialize 0x%llx\n",nftl_blk->size);

	return 0;
}


/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
uint32 _nand_read(struct aml_nftl_dev *nftl_dev, unsigned long start_sector, unsigned int len, unsigned char *buf)
{
	return __nand_read(nftl_dev->aml_nftl_part, start_sector, len, buf);
}

uint32 _blk_nand_read(struct aml_nftl_blk *nftl_blk, unsigned long  start_sector, unsigned int len, unsigned char *buf)
{
	int ret = 0;

	//mutex_lock(nftl_blk->nftl_dev->aml_nftl_lock);
	ret = _nand_read(nftl_blk->nftl_dev, start_sector + nftl_blk->offset, len, buf);
	//mutex_unlock(nftl_blk->nftl_dev->aml_nftl_lock);

	return ret;
}
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
uint32 _nand_write(struct aml_nftl_dev *nftl_dev, unsigned long start_sector, unsigned int len, unsigned char *buf)
{
	uint32 ret;

	ret = __nand_write(nftl_dev->aml_nftl_part, start_sector, len, buf, nftl_dev->sync_flag);
	amlnf_ktime_get_ts(&nftl_dev->ts_write_start);
	return ret;
}
uint32 _nand_discard(struct aml_nftl_dev *nftl_dev, unsigned long start_sector, unsigned int len)
{
	uint32 ret;
	ret = __nand_discard(nftl_dev->aml_nftl_part, start_sector, len, nftl_dev->sync_flag);
	amlnf_ktime_get_ts(&nftl_dev->ts_write_start);
	//Log discard requests
	printk(KERN_DEBUG "nftl _nand_discard, start sector=%lu, length=%u\n", start_sector, len);
	return ret;
}

uint32 _blk_nand_write(struct aml_nftl_blk *nftl_blk, unsigned long start_sector, unsigned int  len, unsigned char *buf)
{
	uint32 ret;

	ret = _nand_write(nftl_blk->nftl_dev, start_sector + nftl_blk->offset, len, buf);

	return ret;
}
uint32 _blk_nand_discard(struct aml_nftl_blk *nftl_blk, unsigned long start_sector, unsigned int len)
{
	uint32 ret;

	ret = _nand_discard(nftl_blk->nftl_dev, start_sector + nftl_blk->offset, len);

	return ret;
}
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
uint32 _nand_flush_write_cache(struct aml_nftl_dev *nftl_dev)
{
	return __nand_flush_write_cache(nftl_dev->aml_nftl_part);
}
uint32 _nand_flush_discard_cache(struct aml_nftl_dev *nftl_dev)
{
	return __nand_flush_discard_cache(nftl_dev->aml_nftl_part);
}
uint32 _nand_write_pair_page(struct aml_nftl_dev *nftl_dev)
{
	return __nand_write_pair_page(nftl_dev->aml_nftl_part);
}
uint32 _check_mapping(struct aml_nftl_dev *nftl_dev, uint64_t offset, uint64_t size)
{
	return __check_mapping(nftl_dev->aml_nftl_part, offset, size);
}
uint32 _discard_partition(struct aml_nftl_dev *nftl_dev, uint64_t offset, uint64_t size)
{
	return __discard_partition(nftl_dev->aml_nftl_part, offset, size);
}

uint32 _blk_nand_flush_write_cache(struct aml_nftl_blk *nftl_blk)
{
	int ret = 0;

	//mutex_lock(nftl_blk->nftl_dev->aml_nftl_lock);
	ret =  _nand_flush_write_cache(nftl_blk->nftl_dev);
	//mutex_unlock(nftl_blk->nftl_dev->aml_nftl_lock);

	return ret;
}
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
