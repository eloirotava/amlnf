/*
 * Aml nftl block device access
 *
 * (C) 2012 8
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mii.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/device.h>
#include <linux/pagemap.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/blkpg.h>
#include <linux/freezer.h>
#include <linux/spinlock.h>
#include <linux/hdreg.h>
#include <linux/kthread.h>
#include <asm/uaccess.h>
#include <linux/hdreg.h>
#include <linux/blkdev.h>
#include <linux/reboot.h>
#include <linux/kmod.h>
//#include <linux/mtd/mtd.h>
//#include <linux/mtd/nand.h>
//#include <linux/mtd/blktrans.h>
#include <plat/regops.h>
#include <mach/am_regs.h>
#include "aml_nftl_block.h"
#include <linux/blk-mq.h>
#include <linux/bio.h>

//extern struct mutex ntd_table_mutex;
//EXPORT_SYMBOL(ntd_table_mutex);
extern void print_block_invalid_list(struct aml_nftl_part_t* part);
extern int print_discard_page_map(struct aml_nftl_part_t *part);
extern int check_storage_device(void);
extern int is_phydev_off_adjust(void);
extern int aml_nftl_initialize(struct aml_nftl_dev *nftl_dev,int no);
/* Defined in nfc_map.c. */
extern char amlnf_parts[64];
bool amlnf_part_enabled(const char *name);
extern uint32 nand_flush_write_cache(struct aml_nftl_blk* nftl_blk);
extern void aml_nftl_part_release(struct aml_nftl_part_t* part);
extern uint32 do_prio_gc(struct aml_nftl_part_t* part);
extern uint32 garbage_collect(struct aml_nftl_part_t* part);
extern uint32 do_static_wear_leveling(struct aml_nftl_part_t* part);
extern void *aml_nftl_malloc(uint32 size);
extern void aml_nftl_free(const void *ptr);
//extern int aml_nftl_dbg(const char * fmt,args...);
extern int aml_blktrans_initialize(struct aml_nftl_blk *nftl_blk,struct aml_nftl_dev *nftl_dev,uint64_t offset,uint64_t size);
extern void amlnf_ktime_get_ts(struct timespec64 *ts);
extern int aml_nftl_erase_part(struct aml_nftl_part_t *part);
extern int aml_nftl_set_status(struct aml_nftl_part_t *part,unsigned char status);
//static struct ntd_blktrans_dev *blktrans_dev_get_blk(struct gendisk *disk);
//static void blktrans_dev_put_blk(struct ntd_blktrans_dev *dev);
//int register_ntd_blktrans_blk(struct ntd_blktrans_ops *tr);
//int deregister_ntd_blktrans_blk(struct ntd_blktrans_ops *tr);
//int add_ntd_blktrans_dev_blk(struct ntd_blktrans_dev *new);
//int del_ntd_blktrans_dev_blk(struct ntd_blktrans_dev *old);
//static int blktrans_open_blk(struct block_device *bdev, fmode_t mode);
//static int blktrans_release_blk(struct gendisk *disk, fmode_t mode);
//static int blktrans_getgeo_blk(struct block_device *bdev, struct hd_geometry *geo);
//static int blktrans_ioctl_blk(struct block_device *bdev, fmode_t mode,unsigned int cmd, unsigned long arg);
//static void ntd_blktrans_request_blk(struct request_queue *rq);
//static int ntd_blktrans_thread_blk(void *arg);
//static void blktrans_dev_release_blk(struct kref *kref);


//static struct mutex aml_nftl_lock;
static int nftl_num;
static int dev_num;
//extern int test_flag;

int aml_ntd_nftl_flush(struct ntd_info *ntd);




int get_adjust_block_num(void)
{
	int ret = 0;
	#ifdef NAND_ADJUST_PART_TABLE
		ret = ADJUST_BLOCK_NUM;
	#endif
	return	ret ;
}


/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int aml_nftl_flush(struct ntd_blktrans_dev *dev)
{
    int error = 0;
    struct aml_nftl_dev *nftl_dev = (struct aml_nftl_dev *)(dev->ntd->nftl_priv);

    mutex_lock(nftl_dev->aml_nftl_lock);
    error = nftl_dev->flush_write_cache(nftl_dev);
    mutex_unlock(nftl_dev->aml_nftl_lock);

//    PRINT("aml_nftl_flush\n");

    return error;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int aml_ntd_nftl_flush(struct ntd_info *ntd)
{
    int error = 0;
    struct aml_nftl_dev *nftl_dev = (struct aml_nftl_dev *)ntd->nftl_priv;

    mutex_lock(nftl_dev->aml_nftl_lock);
    error = nftl_dev->flush_write_cache(nftl_dev);
    mutex_unlock(nftl_dev->aml_nftl_lock);

//    PRINT("aml_ntd_flush\n");
    return error;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
/* aml_nftl_calculate_sg removed — bounce SG unused on 6.12 */

static uint32 write_sync_flag(struct aml_nftl_blk *aml_nftl_blk)
{
	struct aml_nftl_dev *nftl_dev = aml_nftl_blk->nftl_dev;

	nftl_dev->sync_flag = 0;
	if (memcmp(aml_nftl_blk->name, "media", 5) == 0)
		return 0;
	if (aml_nftl_blk->req && (req_op(aml_nftl_blk->req) == REQ_OP_WRITE) &&
	    (aml_nftl_blk->req->cmd_flags & REQ_SYNC))
		nftl_dev->sync_flag = 1;
	return 0;
}

int aml_nftl_init_bounce_buf(struct ntd_blktrans_dev *dev, struct request_queue *rq)
{
	struct aml_nftl_blk *nftl_blk = (void *)dev;

	nftl_blk->queue = rq;
	/* Bounce SG path retired; bio segment iteration used instead. */
	return 0;
}


/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int do_nftltrans_request(struct ntd_blktrans_ops *tr,
				  struct ntd_blktrans_dev *dev,
				  struct request *req)
{
	struct aml_nftl_blk *nftl_blk = (void *)dev;
	struct req_iterator iter;
	struct bio_vec bvec;
	unsigned long block, nblk;
	sector_t pos;
	char *buf;
	int ret = 0;

	if (nftl_blk->nftl_dev->reboot_flag)
		return 0;

	pos = blk_rq_pos(req);
	block = pos << SHIFT_PER_SECTOR >> tr->blkshift;
	nblk = blk_rq_sectors(req);

	if (pos + blk_rq_sectors(req) > get_capacity(dev->disk))
		return -EIO;

	if (req_op(req) == REQ_OP_DISCARD || req_op(req) == REQ_OP_WRITE_ZEROES) {
		mutex_lock(nftl_blk->nftl_dev->aml_nftl_lock);
		nftl_blk->discard_data(nftl_blk, block, nblk);
		mutex_unlock(nftl_blk->nftl_dev->aml_nftl_lock);
		return 0;
	}

	nftl_blk->queue = req->q;
	nftl_blk->req = req;
	mutex_lock(nftl_blk->nftl_dev->aml_nftl_lock);
	rq_for_each_segment(bvec, req, iter) {
		block = pos << SHIFT_PER_SECTOR >> tr->blkshift;
		nblk = bvec.bv_len >> tr->blkshift;
		buf = kmap_local_page(bvec.bv_page) + bvec.bv_offset;
		if (req_op(req) == REQ_OP_READ) {
			if (nftl_blk->read_data(nftl_blk, block, nblk, buf))
				ret = -EIO;
		} else if (req_op(req) == REQ_OP_WRITE) {
			write_sync_flag(nftl_blk);
			if (nftl_blk->write_data(nftl_blk, block, nblk, buf))
				ret = -EIO;
		} else {
			ret = -EIO;
		}
		kunmap_local(buf);
		if (ret)
			break;
		pos += bvec.bv_len >> 9;
	}
	mutex_unlock(nftl_blk->nftl_dev->aml_nftl_lock);
	return ret;
}


/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int aml_nftl_writesect(struct ntd_blktrans_dev *dev, unsigned long block, char *buf)
{
    return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int aml_nftl_thread(void *arg)
{
    struct aml_nftl_dev *nftl_dev= arg;
    unsigned long period = NFTL_MAX_SCHEDULE_TIMEOUT / 10;
    struct timespec64 ts_nftl_current;

    while (!kthread_should_stop()) {
//      struct aml_nftl_part_t *aml_nftl_part = nftl_blk->aml_nftl_part;

        mutex_lock(nftl_dev->aml_nftl_lock);

        if(aml_nftl_get_part_write_cache_nums(nftl_dev->aml_nftl_part) > 0){
            amlnf_ktime_get_ts(&ts_nftl_current);
            if ((ts_nftl_current.tv_sec - nftl_dev->ts_write_start.tv_sec) >= NFTL_FLUSH_DATA_TIME){
                //aml_nftl_dbg("aml_nftl_thread flush data: %d:%s\n", aml_nftl_part->cache.cache_write_nums,nftl_blk->nbd.ntd->name);
                nftl_dev->flush_write_cache(nftl_dev);
            }
        }

#if  SUPPORT_WEAR_LEVELING
        if(do_static_wear_leveling(nftl_dev->aml_nftl_part) != 0){
            PRINT("aml_nftl_thread do_static_wear_leveling error!\n");
        }
#endif

        if(garbage_collect(nftl_dev->aml_nftl_part) != 0){
            PRINT("aml_nftl_thread garbage_collect error!\n");
        }

        if(do_prio_gc(nftl_dev->aml_nftl_part) != 0){
            PRINT("aml_nftl_thread do_prio_gc error!\n");
        }

        mutex_unlock(nftl_dev->aml_nftl_lock);

        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(period);
    }

    nftl_dev->nftl_thread=NULL;
    return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int aml_nftl_reboot_notifier(struct notifier_block *nb, unsigned long priority, void * arg)
{
    int error = 0;
    struct aml_nftl_dev *nftl_dev = nftl_notifier_to_dev(nb);

    //just return since notifier only need once
    if(nftl_dev->reboot_flag){
        PRINT("nand reboot notify Just ignore here for %s\n", nftl_dev->ntd->name);
        return error;
    }

    mutex_lock(nftl_dev->aml_nftl_lock);
    error = nftl_dev->flush_write_cache(nftl_dev);

    error |= nftl_dev->flush_discard_cache(nftl_dev);
    //print_block_invalid_list(nftl_dev->aml_nftl_part);
    //print_discard_page_map(nftl_dev->aml_nftl_part);

    mutex_unlock(nftl_dev->aml_nftl_lock);

    if(nftl_dev->nftl_thread!=NULL){
        kthread_stop(nftl_dev->nftl_thread); //add stop thread to ensure nftl quit safely
        nftl_dev->nftl_thread=NULL;
    }
    mutex_lock(nftl_dev->aml_nftl_lock);
    error |= nftl_dev->write_pair_page(nftl_dev);
    mutex_unlock(nftl_dev->aml_nftl_lock);
    nftl_dev->reboot_flag = 1;

    return error;
}

int aml_nftl_reinit_part(struct aml_nftl_blk *nftl_blk)
{
       struct aml_nftl_part_t *part = NULL;
       struct ntd_partition *logic_partition = NULL;
       int ret =0,i=0;
       uint64_t tmp_offset = 0;
       struct aml_nftl_dev * nftl_dev = nftl_blk->nftl_dev;
       part = nftl_dev->aml_nftl_part;

       aml_nftl_set_status(part,0);
       if(nftl_dev->nftl_thread!=NULL){
            kthread_stop(nftl_dev->nftl_thread); //add stop thread to ensure nftl quit safely
        }
      mutex_lock(nftl_dev->aml_nftl_lock);
        // do not erase this part because this phy partition has more than 1 logic partition.
       if(nftl_dev->ntd->nr_partitions > 1)
       {
            // discard logic partition
            for(i=0;i<nftl_dev->ntd->nr_partitions;i++)
            {
                logic_partition = nftl_dev->ntd->parts+i;
                PRINT("show logic_partition name:%s,size:%llx,tmp_offset:%llx\n",logic_partition->name,logic_partition->size,tmp_offset);
                if(memcmp(logic_partition->name,nftl_blk->name, strlen(nftl_blk->name))==0)
                {
                    PRINT("logic_partition name:%s,size:%llx,tmp_offset:%llx\n",logic_partition->name,logic_partition->size,tmp_offset);
                    if((logic_partition->offset != 0xffffffffffffffff)&&(logic_partition->size != 0xffffffffffffffff))
                    {
                        // first check if this logic partition has valid mapping.
                        if(nftl_dev->check_mapping(nftl_dev,tmp_offset,logic_partition->size))
                        {
                            //discard all pages of this partition
                            PRINT("this partition has valid mapping\n");
                            nftl_dev->discard_partition(nftl_dev,tmp_offset,logic_partition->size);
                        }else{
                            // do no thing
						PRINT("this partition has no valid mapping\n");
                        }
                    }
                    else
                    {
                        //the last partition
                        PRINT("last logic partition name:%s,size:%llx,offset:%llx,tmp_offset:%llx\n",logic_partition->name,logic_partition->size,logic_partition->offset,tmp_offset);
                        // first check if this logic partition has valid mapping.
                        if(nftl_dev->check_mapping(nftl_dev,tmp_offset,0xffffffffffffffff))
                        {
                            //discard all pages of this partition
                            PRINT("this partition has valid mapping\n");
                            nftl_dev->discard_partition(nftl_dev,tmp_offset,0xffffffffffffffff);
                        }else{
                            // do no thing
						PRINT("this partition has no valid mapping\n");
                        }
                    }
                }

                if(logic_partition->size != 0xffffffffffffffff)
                {
                    tmp_offset = tmp_offset+logic_partition->size;
                }
            }
       }
       else
       {
           ret = aml_nftl_erase_part(part);
           if(ret){
                   PRINT("aml_nftl_erase_part : failed\n");
           }

       if(aml_nftl_initialize(nftl_dev,-1)){
          PRINT("aml_nftl_reinit_part : aml_nftl_initialize failed\n");
       }
       }
      mutex_unlock(nftl_dev->aml_nftl_lock);
      if(nftl_dev->nftl_thread!=NULL){
       wake_up_process(nftl_dev->nftl_thread);
       }
       return ret ;
}

static int aml_nftl_wipe_part(struct ntd_blktrans_dev *dev)
{
	struct aml_nftl_blk *nftl_blk = (void *)dev;
	int error = 0;
    printk("%s,%d nftl_blk->name:%s,nftl_blk->offset:%llx,nftl_blk->size:%llx\n",__func__,__LINE__,nftl_blk->name,nftl_blk->offset,nftl_blk->size);
	//struct aml_nftl_dev * nftl_dev = nftl_blk->nftl_dev;

	error = aml_nftl_reinit_part(nftl_blk);
	if(error){
		PRINT("aml_nftl_reinit_part: failed\n");
	}
	return error;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static void aml_nftl_add_ntd(struct ntd_blktrans_ops *tr, struct ntd_info *ntd)
{
    int    i;
    struct aml_nftl_dev *nftl_dev;
    struct aml_nftl_blk *nftl_blk;
    uint64_t cur_offset = 0;
    //uint64_t cur_size;
    struct ntd_partition *part;

    PRINT("ntd->name: %s\n",ntd->name);

    /*
     * Decide here, before anything is allocated or registered.  The first
     * version of this gate lived down in aml_nftl_initialize(), which is
     * already past the reboot notifier and past ntd->nftl_priv, and the
     * skipped partition took the box down with a NULL dereference on the
     * way out.
     */
    if (!amlnf_part_enabled(ntd->name)) {
        PRINT("amlnf_m3: pulando particao %s (parts=%s)\n",
              ntd->name, amlnf_parts);
        return;
    }

    nftl_dev = aml_nftl_malloc(sizeof(struct aml_nftl_dev));
    if (!nftl_dev)
        return;

	nftl_dev->aml_nftl_lock = aml_nftl_malloc(sizeof(struct mutex));
	if (!nftl_dev->aml_nftl_lock)
	       return;

	mutex_init(nftl_dev->aml_nftl_lock);

    nftl_dev->init_flag = 0;
    nftl_dev->ntd = ntd;
    nftl_dev->nb.notifier_call = aml_nftl_reboot_notifier;
    register_reboot_notifier(&nftl_dev->nb);

    if (aml_nftl_initialize(nftl_dev,nftl_num)){
        aml_nftl_dbg("aml_nftl_initialize failed\n");
        /* the notifier is already registered at this point */
        unregister_reboot_notifier(&nftl_dev->nb);
        return;
    }
    nftl_dev->init_flag = 1;
    nftl_dev->reboot_flag = 0;
    ntd->nftl_priv = (void*)nftl_dev;

    nftl_dev->nftl_thread = kthread_run(aml_nftl_thread, nftl_dev, "%sd", "aml_nftl");
    if (IS_ERR(nftl_dev->nftl_thread))
        return;

    for(i=0;i<ntd->nr_partitions;i++)
    {
        part = ntd->parts+i;

        nftl_blk = aml_nftl_malloc(sizeof(struct aml_nftl_blk));
        if (!nftl_blk)
            return;

//        nftl_blk->nbd.ntd = ntd;
//        nftl_blk->nbd.devnum = (ntd->index<<2)+i;

        nftl_blk->nbd.devnum = dev_num;
        dev_num++;
        nftl_blk->nbd.tr = tr;
        nftl_blk->nbd.ntd = ntd;

        snprintf(nftl_blk->name, sizeof(nftl_blk->name),"%s", part->name);

        if(aml_blktrans_initialize(nftl_blk,nftl_dev,cur_offset,part->size)){
            aml_nftl_dbg("aml_blktrans_initialize failed\n");
            return;
        }

     //   printk("nftl_blk->name %s \n",nftl_blk->name);
    //    printk("nftl_blk->offset 0x%llx \n",nftl_blk->offset);
    //    printk("nftl_blk->size 0x%llx \n",nftl_blk->size);

        nftl_blk->nbd.size = (unsigned long)nftl_blk->size;
        nftl_blk->nbd.priv = (void*)nftl_blk;

        memcpy(nftl_blk->nbd.name,part->name,strlen(part->name)+1);

        if (add_ntd_blktrans_dev(&nftl_blk->nbd)){
            aml_nftl_dbg("nftl add blk disk dev failed\n");
            return;
        }
        if (aml_nftl_init_bounce_buf(&nftl_blk->nbd, nftl_blk->nbd.rq)){
            aml_nftl_dbg("aml_nftl_init_bounce_buf  failed\n");
            return;
        }

        cur_offset += part->size;
    }

    nftl_num++;
    aml_nftl_dbg("aml_nftl_add_ntd ok\n");

    return;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int aml_nftl_open(struct ntd_blktrans_dev *nbd)
{
    aml_nftl_dbg("aml_nftl_open ok!\n");
    return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int aml_nftl_release(struct ntd_blktrans_dev *nbd)
{
    int error = 0;
    struct aml_nftl_blk *nftl_blk = (void *)nbd;

    mutex_lock(nftl_blk->lock);

    error = nftl_blk->flush_write_cache(nftl_blk);

    mutex_unlock(nftl_blk->lock);

    return error;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static void aml_nftl_blk_release(struct aml_nftl_blk *nftl_blk)
{
//    aml_nftl_part_release(nftl_blk->nftl_dev->aml_nftl_part);
    if (nftl_blk->bounce_sg)
        aml_nftl_free(nftl_blk->bounce_sg);
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static void aml_nftl_remove_dev(struct ntd_blktrans_dev *dev)
{
    struct aml_nftl_blk *nftl_blk = (void *)dev;

    unregister_reboot_notifier(&nftl_blk->nftl_dev->nb);
    del_ntd_blktrans_dev(dev);
    aml_nftl_blk_release(nftl_blk);
    aml_nftl_free(nftl_blk);
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static struct ntd_blktrans_ops aml_nftl_tr = {
    .name       = "avnftl",
    .major      = AML_NFTL_MAJOR,
    .part_bits  = 0,
    .blksize    = BYTES_PER_SECTOR,
    .open       = aml_nftl_open,
    .release    = aml_nftl_release,
    .do_blktrans_request = do_nftltrans_request,
    .writesect  = aml_nftl_writesect,
    .flush      = aml_nftl_flush,
    .add_ntd    = aml_nftl_add_ntd,
    .remove_dev = aml_nftl_remove_dev,
    .wipe_part	= aml_nftl_wipe_part,
    .owner      = THIS_MODULE,
};

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int init_aml_nftl(void)
{
    int ret;

//    aml_nftl_lock = aml_nftl_malloc(sizeof(struct mutex));
//    if (!aml_nftl_lock)
//        return -1;

    //mutex_init(&aml_nftl_lock);
        if(check_storage_device() < 0){
		return 0;
     }
    nftl_num = 0;
    dev_num = 0;
    ret = register_ntd_blktrans(&aml_nftl_tr);
    PRINT("init_aml_nftl end\n");

    return ret;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
void cleanup_aml_nftl(void)
{
    deregister_ntd_blktrans(&aml_nftl_tr);
}




/* init_aml_nftl called from amlnf probe after NTD registration */



MODULE_LICENSE("GPL");
MODULE_AUTHOR("AML nand team");
MODULE_DESCRIPTION("aml nftl block interface");

