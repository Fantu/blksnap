/* SPDX-License-Identifier: GPL-2.0 */
#pragma once
#include <linux/kref.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/rwsem.h>
#include <linux/blkdev.h>
#include <linux/fs.h>

#ifdef HAVE_LP_FILTER
#include "lp_filter.h"
#endif

struct cbt_map;
struct diff_area;

/**
 * struct tracker - Tracker for a block device.
 *
 * @kref:
 *	Protects the structure from being released during processing of
 *	an ioctl.
 * @dev_id:
 *	Original block device ID.
 * @submit_lock:
 *	Provides blocking of I/O operations for a block device.
 * @snapshot_is_taken:
 *	Indicates that a snapshot was taken for the device whose bios are
 *	handled by this tracker.
 * @cbt_map:
 *	Pointer to a change block tracker map.
 * @diff_area:
 *	Pointer to a difference area.
 *
 * The main goal of the tracker is to handle bios. The tracker detectes
 * the range of sectors that will change and transmits them to the CBT map
 * and to the difference area.
 */
struct tracker {
	struct bdev_filter flt;
	struct list_head link;
	dev_t dev_id;

	struct percpu_rw_semaphore submit_lock;
	atomic_t snapshot_is_taken;

	struct cbt_map *cbt_map;
	struct diff_area *diff_area;
};

static inline void tracker_lock(struct tracker *tracker)
{
	if (likely(tracker))
		percpu_down_write(&tracker->submit_lock);
};

static inline void tracker_unlock(struct tracker *tracker)
{
	if (likely(tracker))
		percpu_up_write(&tracker->submit_lock);
};

static inline void tracker_put(struct tracker *tracker)
{
	if (likely(tracker))
		bdev_filter_put(&tracker->flt);

};
struct tracker *tracker_get_by_dev(struct block_device *bdev);

int tracker_init(void);
void tracker_done(void);

struct tracker *tracker_create_or_get(dev_t dev_id);
int tracker_remove(dev_t dev_id);
int tracker_collect(int max_count, struct blk_snap_cbt_info *cbt_info,
		    int *pcount);
int tracker_read_cbt_bitmap(dev_t dev_id, unsigned int offset, size_t length,
			    char __user *user_buff);
int tracker_mark_dirty_blocks(dev_t dev_id,
			      struct blk_snap_block_range *block_ranges,
			      unsigned int count);

int tracker_take_snapshot(struct tracker *tracker);
void tracker_release_snapshot(struct tracker *tracker);

#if defined(HAVE_SUPER_BLOCK_FREEZE)
static inline int _freeze_bdev(struct block_device *bdev,
			       struct super_block **psuperblock)
{
	struct super_block *superblock;

	if (bdev->bd_super == NULL) {
		pr_warn("Unable to freeze device [%u:%u]: no superblock was found\n",
			MAJOR(bdev->bd_dev), MINOR(bdev->bd_dev));
		return 0;
	}

	superblock = freeze_bdev(bdev);
	if (IS_ERR_OR_NULL(superblock)) {
		int result;

		pr_err("Failed to freeze device [%u:%u]\n", MAJOR(bdev->bd_dev),
		       MINOR(bdev->bd_dev));

		if (superblock == NULL)
			result = -ENODEV;
		else {
			result = PTR_ERR(superblock);
			pr_err("Error code: %d\n", result);
		}
		return result;
	}

	pr_debug("Device [%u:%u] was frozen\n", MAJOR(bdev->bd_dev),
		 MINOR(bdev->bd_dev));
	*psuperblock = superblock;

	return 0;
}
static inline void _thaw_bdev(struct block_device *bdev,
			      struct super_block *superblock)
{
	if (superblock == NULL)
		return;

	if (thaw_bdev(bdev, superblock))
		pr_err("Failed to unfreeze device [%u:%u]\n",
		       MAJOR(bdev->bd_dev), MINOR(bdev->bd_dev));
	else
		pr_debug("Device [%u:%u] was unfrozen\n", MAJOR(bdev->bd_dev),
			 MINOR(bdev->bd_dev));
}
#endif
