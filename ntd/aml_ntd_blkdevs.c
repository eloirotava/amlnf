// SPDX-License-Identifier: GPL-2.0
/*
 * NTD blktrans layer — blk-mq port for Linux 6.12 (from 3.10 aml_ntd_blkdevs).
 * Pattern follows mainline mtd_blkdevs.c.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/blkpg.h>
#include <linux/spinlock.h>
#include <linux/hdreg.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include "amlnf_glue.h"
#include "aml_ntd.h"

static LIST_HEAD(ntd_blktrans_majors);
static DEFINE_MUTEX(blktrans_ref_mutex);

extern struct mutex ntd_table_mutex;

int amlnf_class_register(struct class *class)
{
	return class_register(class);
}

void amlnf_ktime_get_ts(struct timespec64 *ts)
{
	ktime_get_ts64(ts);
}

static void blktrans_dev_release(struct kref *kref)
{
	struct ntd_blktrans_dev *dev =
		container_of(kref, struct ntd_blktrans_dev, ref);

	put_disk(dev->disk);
	if (dev->tag_set) {
		blk_mq_free_tag_set(dev->tag_set);
		kfree(dev->tag_set);
		dev->tag_set = NULL;
	}
	list_del(&dev->list);
	kfree(dev);
}

static struct ntd_blktrans_dev *blktrans_dev_get(struct gendisk *disk)
{
	struct ntd_blktrans_dev *dev;

	mutex_lock(&blktrans_ref_mutex);
	dev = disk->private_data;
	if (dev)
		kref_get(&dev->ref);
	mutex_unlock(&blktrans_ref_mutex);
	return dev;
}

static void blktrans_dev_put(struct ntd_blktrans_dev *dev)
{
	kref_put(&dev->ref, blktrans_dev_release);
}

static blk_status_t do_blktrans_request_fallback(struct ntd_blktrans_ops *tr,
						 struct ntd_blktrans_dev *dev,
						 struct request *req)
{
	unsigned long block, nsect;
	char *buf;
	struct req_iterator iter;
	struct bio_vec bvec;

	block = blk_rq_pos(req) << 9 >> tr->blkshift;

	switch (req_op(req)) {
	case REQ_OP_FLUSH:
		if (tr->flush && tr->flush(dev))
			return BLK_STS_IOERR;
		return BLK_STS_OK;
	case REQ_OP_DISCARD:
	case REQ_OP_WRITE_ZEROES:
		nsect = blk_rq_sectors(req);
		if (tr->discard && tr->discard(dev, block, nsect))
			return BLK_STS_IOERR;
		return BLK_STS_OK;
	case REQ_OP_READ:
		rq_for_each_segment(bvec, req, iter) {
			buf = kmap_local_page(bvec.bv_page) + bvec.bv_offset;
			nsect = bvec.bv_len >> tr->blkshift;
			while (nsect--) {
				if (!tr->readsect || tr->readsect(dev, block, buf)) {
					kunmap_local(buf);
					return BLK_STS_IOERR;
				}
				buf += tr->blksize;
				block++;
			}
			kunmap_local(kmap_local_page(bvec.bv_page) + bvec.bv_offset);
		}
		return BLK_STS_OK;
	case REQ_OP_WRITE:
		if (!tr->writesect)
			return BLK_STS_IOERR;
		rq_for_each_segment(bvec, req, iter) {
			buf = kmap_local_page(bvec.bv_page) + bvec.bv_offset;
			nsect = bvec.bv_len >> tr->blkshift;
			while (nsect--) {
				if (tr->writesect(dev, block, buf)) {
					kunmap_local(buf);
					return BLK_STS_IOERR;
				}
				buf += tr->blksize;
				block++;
			}
			kunmap_local(kmap_local_page(bvec.bv_page) + bvec.bv_offset);
		}
		return BLK_STS_OK;
	default:
		return BLK_STS_IOERR;
	}
}

static struct request *ntd_next_request(struct ntd_blktrans_dev *dev)
{
	struct request *rq;

	rq = list_first_entry_or_null(&dev->rq_list, struct request, queuelist);
	if (rq) {
		list_del_init(&rq->queuelist);
		blk_mq_start_request(rq);
		return rq;
	}
	return NULL;
}

static void ntd_blktrans_work(struct ntd_blktrans_dev *dev)
{
	struct ntd_blktrans_ops *tr = dev->tr;
	struct request *req = NULL;

	while (1) {
		blk_status_t res;
		int err;

		if (!req && !(req = ntd_next_request(dev)))
			break;

		spin_unlock_irq(&dev->queue_lock);
		mutex_lock(&dev->lock);
		if (tr->do_blktrans_request) {
			err = tr->do_blktrans_request(tr, dev, req);
			res = err ? BLK_STS_IOERR : BLK_STS_OK;
		} else {
			res = do_blktrans_request_fallback(tr, dev, req);
		}
		mutex_unlock(&dev->lock);

		blk_mq_end_request(req, res);
		req = NULL;
		cond_resched();
		spin_lock_irq(&dev->queue_lock);
	}
}

static void ntd_blktrans_work_fn(struct work_struct *w)
{
	struct ntd_blktrans_dev *dev = container_of(w, struct ntd_blktrans_dev,
													work);

	spin_lock_irq(&dev->queue_lock);
	ntd_blktrans_work(dev);
	spin_unlock_irq(&dev->queue_lock);
}

/*
 * Enqueue and get out.  Nothing that touches the flash may run from here.
 *
 * blk-mq calls queue_rq with preemption disabled, so it must not sleep, and
 * every path below this one does: the NFTL takes a mutex, the physical layer
 * waits on DMA completions, and the controller polls with udelay().  The
 * first version of this port called ntd_blktrans_work() inline, from inside
 * spin_lock_irq(), which meant the whole NAND transfer ran with interrupts
 * off and a spinlock held.  It "worked" -- data landed on the flash and read
 * back correctly -- but every single request logged a
 *
 *   WARNING: CPU: 2 PID: ... at kernel/time/timer.c:1657 __timer_delete_sync
 *
 * from the timer that schedule_timeout() tears down, and with netconsole on
 * the box that turned into tens of thousands of backtraces a minute and a
 * machine too busy printing to answer ssh.  Copying a 1.2 GB root filesystem
 * was what made it obvious.
 *
 * This is the same shape mainline mtd_blkdevs.c settled on when it was
 * converted to blk-mq: queue_rq puts the request on the list and kicks an
 * ordered workqueue, and the work item does the transfer in a context where
 * sleeping is allowed.
 */
static blk_status_t ntd_queue_rq(struct blk_mq_hw_ctx *hctx,
				 const struct blk_mq_queue_data *bd)
{
	struct ntd_blktrans_dev *dev = hctx->queue->queuedata;

	if (!dev) {
		blk_mq_start_request(bd->rq);
		return BLK_STS_IOERR;
	}

	spin_lock_irq(&dev->queue_lock);
	list_add_tail(&bd->rq->queuelist, &dev->rq_list);
	spin_unlock_irq(&dev->queue_lock);

	queue_work(dev->wq, &dev->work);
	return BLK_STS_OK;
}

static int blktrans_open(struct gendisk *disk, blk_mode_t mode)
{
	struct ntd_blktrans_dev *dev = blktrans_dev_get(disk);
	int ret = 0;

	if (!dev)
		return -ENXIO;

	mutex_lock(&dev->lock);
	if (dev->open++)
		goto unlock;

	kref_get(&dev->ref);
	__module_get(dev->tr->owner);
	if (dev->tr->open) {
		ret = dev->tr->open(dev);
		if (ret)
			goto error_put;
	}
unlock:
	mutex_unlock(&dev->lock);
	blktrans_dev_put(dev);
	return ret;

error_put:
	module_put(dev->tr->owner);
	kref_put(&dev->ref, blktrans_dev_release);
	dev->open--;
	mutex_unlock(&dev->lock);
	blktrans_dev_put(dev);
	return ret;
}

static void blktrans_release(struct gendisk *disk)
{
	struct ntd_blktrans_dev *dev = blktrans_dev_get(disk);

	if (!dev)
		return;

	mutex_lock(&dev->lock);
	if (--dev->open)
		goto unlock;

	kref_put(&dev->ref, blktrans_dev_release);
	module_put(dev->tr->owner);
	if (dev->tr->release)
		dev->tr->release(dev);
unlock:
	mutex_unlock(&dev->lock);
	blktrans_dev_put(dev);
}

static int blktrans_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	struct ntd_blktrans_dev *dev = blktrans_dev_get(bdev->bd_disk);
	int ret = -ENXIO;

	if (!dev)
		return ret;
	mutex_lock(&dev->lock);
	ret = dev->tr->getgeo ? dev->tr->getgeo(dev, geo) : 0;
	mutex_unlock(&dev->lock);
	blktrans_dev_put(dev);
	return ret;
}

static const struct block_device_operations ntd_blktrans_ops = {
	.owner		= THIS_MODULE,
	.open		= blktrans_open,
	.release	= blktrans_release,
	.getgeo		= blktrans_getgeo,
};

static const struct blk_mq_ops ntd_mq_ops = {
	.queue_rq	= ntd_queue_rq,
};

int add_ntd_blktrans_dev(struct ntd_blktrans_dev *new)
{
	struct ntd_blktrans_ops *tr = new->tr;
	struct ntd_blktrans_dev *d;
	struct gendisk *gd;
	struct blk_mq_tag_set *set;
	int ret = -ENOMEM;

	mutex_lock(&blktrans_ref_mutex);
	list_for_each_entry(d, &tr->devs, list) {
		if (d->devnum == new->devnum) {
			mutex_unlock(&blktrans_ref_mutex);
			return -EBUSY;
		} else if (d->devnum > new->devnum) {
			list_add_tail(&new->list, &d->list);
			goto added;
		}
	}
	if (new->devnum > (MINORMASK >> tr->part_bits)) {
		mutex_unlock(&blktrans_ref_mutex);
		return -EBUSY;
	}
	list_add_tail(&new->list, &tr->devs);
added:
	mutex_unlock(&blktrans_ref_mutex);

	mutex_init(&new->lock);
	kref_init(&new->ref);
	INIT_LIST_HEAD(&new->rq_list);
	spin_lock_init(&new->queue_lock);
	INIT_WORK(&new->work, ntd_blktrans_work_fn);
	/*
	 * Ordered: the FTL is single threaded and the requests for one device
	 * have to reach it in order.
	 */
	new->wq = alloc_ordered_workqueue("%s%d", 0, tr->name, new->devnum);
	if (!new->wq)
		goto err_list;
	if (!tr->writesect)
		new->readonly = 1;

	set = kzalloc(sizeof(*set), GFP_KERNEL);
	if (!set)
		goto err_list;
	new->tag_set = set;
	set->ops = &ntd_mq_ops;
	set->nr_hw_queues = 1;
	set->queue_depth = 16;
	set->numa_node = NUMA_NO_NODE;
	set->flags = BLK_MQ_F_SHOULD_MERGE;
	ret = blk_mq_alloc_tag_set(set);
	if (ret)
		goto err_set;

	{
		struct queue_limits lim = {
			.logical_block_size = tr->blksize,
			.physical_block_size = tr->blksize,
			.max_hw_sectors = 1024,
		};

		gd = blk_mq_alloc_disk(set, &lim, new);
	}
	if (IS_ERR(gd)) {
		ret = PTR_ERR(gd);
		goto err_tags;
	}

	new->disk = gd;
	new->rq = gd->queue;
	gd->private_data = new;
	gd->major = tr->major;
	gd->first_minor = new->devnum << tr->part_bits;
	gd->minors = 1 << tr->part_bits;
	gd->fops = &ntd_blktrans_ops;
	snprintf(gd->disk_name, sizeof(gd->disk_name), "%s", new->name);
	set_capacity(gd, new->size);
	if (new->readonly)
		set_disk_ro(gd, 1);

	ret = add_disk(gd);
	if (ret)
		goto err_disk;

	pr_info("amlnf: added disk /dev/%s size=%lu sectors\n",
		gd->disk_name, new->size);
	return 0;

err_disk:
	put_disk(gd);
err_tags:
	blk_mq_free_tag_set(set);
err_set:
	kfree(set);
	new->tag_set = NULL;
err_list:
	mutex_lock(&blktrans_ref_mutex);
	list_del(&new->list);
	mutex_unlock(&blktrans_ref_mutex);
	return ret;
}

int del_ntd_blktrans_dev(struct ntd_blktrans_dev *old)
{
	del_gendisk(old->disk);
	blktrans_dev_put(old);
	return 0;
}

int register_ntd_blktrans(struct ntd_blktrans_ops *tr)
{
	struct ntd_info *ntd;
	int ret;

	pr_debug("amlnf: register_ntd_blktrans %s\n", tr->name);
	mutex_lock(&ntd_table_mutex);
	ret = register_blkdev(tr->major, tr->name);
	if (ret < 0) {
		pr_warn("Unable to register %s on major %d: %d\n",
			tr->name, tr->major, ret);
		mutex_unlock(&ntd_table_mutex);
		return ret;
	}
	if (ret)
		tr->major = ret;
	tr->blkshift = ffs(tr->blksize) - 1;
	INIT_LIST_HEAD(&tr->devs);
	list_add(&tr->list, &ntd_blktrans_majors);
	ntd_for_each_device(ntd) {
		pr_debug("amlnf: add_ntd for %s\n", ntd->name);
		tr->add_ntd(tr, ntd);
	}
	mutex_unlock(&ntd_table_mutex);
	return 0;
}

int deregister_ntd_blktrans(struct ntd_blktrans_ops *tr)
{
	struct ntd_blktrans_dev *dev, *next;

	mutex_lock(&ntd_table_mutex);
	list_del(&tr->list);
	list_for_each_entry_safe(dev, next, &tr->devs, list)
		tr->remove_dev(dev);
	unregister_blkdev(tr->major, tr->name);
	mutex_unlock(&ntd_table_mutex);
	return 0;
}


MODULE_LICENSE("GPL");
