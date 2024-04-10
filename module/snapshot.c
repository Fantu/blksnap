// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2023 Veeam Software Group GmbH */
#define pr_fmt(fmt) KBUILD_MODNAME "-snapshot: " fmt

#include <linux/slab.h>
#include <linux/sched/mm.h>
#include <linux/build_bug.h>
#ifdef BLKSNAP_STANDALONE
#include "veeamblksnap.h"
#include "compat.h"
#else
#include <uapi/linux/blksnap.h>
#endif
#include "snapshot.h"
#include "tracker.h"
#include "diff_storage.h"
#include "diff_area.h"
#include "snapimage.h"
#include "cbt_map.h"
#ifdef BLKSNAP_FILELOG
#include "log.h"
#endif
#ifdef BLKSNAP_MEMSTAT
#include "memstat.h"
#endif

static LIST_HEAD(snapshots);
static DECLARE_RWSEM(snapshots_lock);

static void snapshot_free(struct kref *kref)
{
	struct snapshot *snapshot = container_of(kref, struct snapshot, kref);

	pr_info("Release snapshot %pUb\n", &snapshot->id);
	while (!list_empty(&snapshot->trackers)) {
		struct tracker *tracker;

		tracker = list_first_entry(&snapshot->trackers, struct tracker,
					   link);
		list_del_init(&tracker->link);
		tracker_release_snapshot(tracker);
		tracker_put(tracker);
	}

	diff_storage_put(snapshot->diff_storage);
	snapshot->diff_storage = NULL;
#ifdef BLKSNAP_MEMSTAT
	__kfree(snapshot);
#else
	kfree(snapshot);
#endif
#ifdef BLKSNAP_MEMSTAT
	memstat_print();
#endif
}

static inline void snapshot_get(struct snapshot *snapshot)
{
	kref_get(&snapshot->kref);
};
static inline void snapshot_put(struct snapshot *snapshot)
{
	if (likely(snapshot))
		kref_put(&snapshot->kref, snapshot_free);
};

static struct snapshot *snapshot_new(void)
{
	int ret;
	struct snapshot *snapshot = NULL;

#ifdef BLKSNAP_MEMSTAT
	snapshot = __kzalloc(sizeof(struct snapshot), GFP_KERNEL);
#else
	snapshot = kzalloc(sizeof(struct snapshot), GFP_KERNEL);
#endif
	if (!snapshot)
		return ERR_PTR(-ENOMEM);

	snapshot->diff_storage = diff_storage_new();
	if (!snapshot->diff_storage) {
		ret = -ENOMEM;
		goto fail_free_snapshot;
	}

	INIT_LIST_HEAD(&snapshot->link);
	kref_init(&snapshot->kref);
	uuid_gen(&snapshot->id);
	init_rwsem(&snapshot->rw_lock);
	snapshot->is_taken = false;
	INIT_LIST_HEAD(&snapshot->trackers);

	return snapshot;

fail_free_snapshot:
#ifdef BLKSNAP_MEMSTAT
	__kfree(snapshot);
#else
	kfree(snapshot);
#endif
	return ERR_PTR(ret);
}

void __exit snapshot_done(void)
{
	struct snapshot *snapshot;

	pr_debug("Cleanup snapshots\n");
	do {
		down_write(&snapshots_lock);
		snapshot = list_first_entry_or_null(&snapshots, struct snapshot,
						    link);
		if (snapshot)
			list_del(&snapshot->link);
		up_write(&snapshots_lock);

		snapshot_put(snapshot);
	} while (snapshot);
}

int snapshot_create(const char *filename, sector_t limit_sect,
		    struct blksnap_uuid *id)
{
	int ret;
	struct snapshot *snapshot = NULL;

	snapshot = snapshot_new();
	if (IS_ERR(snapshot)) {
		pr_err("Unable to create snapshot: failed to allocate snapshot structure\n");
		return PTR_ERR(snapshot);
	}

	ret = diff_storage_set(snapshot->diff_storage, filename, limit_sect);
	if (ret) {
		pr_err("Unable to create snapshot: invalid difference storage file\n");
		snapshot_put(snapshot);
		return ret;
	}

	export_uuid(id->b, &snapshot->id);

	down_write(&snapshots_lock);
	list_add_tail(&snapshot->link, &snapshots);
	up_write(&snapshots_lock);

	pr_info("Snapshot %pUb was created\n", id->b);
	return 0;
}

static struct snapshot *snapshot_get_by_id(const uuid_t *id)
{
	struct snapshot *snapshot = NULL;
	struct snapshot *s;

	down_read(&snapshots_lock);
	if (list_empty(&snapshots))
		goto out;

	list_for_each_entry(s, &snapshots, link) {
		if (uuid_equal(&s->id, id)) {
			snapshot = s;
			snapshot_get(snapshot);
			break;
		}
	}
out:
	up_read(&snapshots_lock);
	return snapshot;
}

int snapshot_add_device(const uuid_t *id, struct tracker *tracker)
{
	int ret = 0;
	struct snapshot *snapshot = NULL;

#ifdef CONFIG_BLK_DEV_INTEGRITY
	if (tracker->orig_bdev->bd_disk->queue->integrity.profile) {
		pr_err("Blksnap is not compatible with data integrity\n");
		ret = -EPERM;
		goto out_up;
	} else
		pr_debug("Data integrity not found\n");
#endif

#ifdef CONFIG_BLK_INLINE_ENCRYPTION
#if defined(BLKSNAP_STANDALONE)
#if defined(HAVE_BLK_CRYPTO_PROFILE)
	if (tracker->orig_bdev->bd_disk->queue->crypto_profile) {
		pr_err("Blksnap is not compatible with hardware inline encryption\n");
		ret = -EPERM;
		goto out_up;
	} else
		pr_debug("Inline encryption not found\n");
#endif
#else
	if (tracker->orig_bdev->bd_disk->queue->crypto_profile) {
		pr_err("Blksnap is not compatible with hardware inline encryption\n");
		ret = -EPERM;
		goto out_up;
	} else
		pr_debug("Inline encryption not found\n");
#endif
#endif
	snapshot = snapshot_get_by_id(id);
	if (!snapshot)
		return -ESRCH;

	down_write(&snapshot->rw_lock);
	if (tracker->dev_id == snapshot->diff_storage->dev_id) {
		pr_err("The block device %d:%d is already being used as difference storage\n",
			MAJOR(tracker->dev_id), MINOR(tracker->dev_id));
		goto out_up;
	}
	if (!list_empty(&snapshot->trackers)) {
		struct tracker *tr;

		list_for_each_entry(tr, &snapshot->trackers, link) {
			if ((tr == tracker) ||
			    (tr->dev_id == tracker->dev_id)) {
				ret = -EALREADY;
				goto out_up;
			}
		}
	}
	if (list_empty(&tracker->link)) {
		tracker_get(tracker);
		list_add_tail(&tracker->link, &snapshot->trackers);
	} else
		ret = -EBUSY;
out_up:
	up_write(&snapshot->rw_lock);

	snapshot_put(snapshot);

	return ret;
}

int snapshot_destroy(const uuid_t *id)
{
	struct snapshot *snapshot = NULL;

	pr_info("Destroy snapshot %pUb\n", id);
	down_write(&snapshots_lock);
	if (!list_empty(&snapshots)) {
		struct snapshot *s = NULL;

		list_for_each_entry(s, &snapshots, link) {
			if (uuid_equal(&s->id, id)) {
				snapshot = s;
				list_del(&snapshot->link);
				break;
			}
		}
	}
	up_write(&snapshots_lock);

	if (!snapshot) {
		pr_err("Unable to destroy snapshot: cannot find snapshot by id %pUb\n",
		       id);
		return -ENODEV;
	}
	snapshot_put(snapshot);

	return 0;
}

static int snapshot_take_trackers(struct snapshot *snapshot)
{
	int ret = 0;
	struct tracker *tracker;
	unsigned int current_flag;

	down_write(&snapshot->rw_lock);

	if (list_empty(&snapshot->trackers)) {
		ret = -ENODEV;
		goto fail;
	}

	list_for_each_entry(tracker, &snapshot->trackers, link) {
		struct diff_area *diff_area =
			diff_area_new(tracker, snapshot->diff_storage);

		if (IS_ERR(diff_area)) {
			ret = PTR_ERR(diff_area);
			break;
		}
		tracker->diff_area = diff_area;
	}
	if (ret)
		goto fail;

	/*
	 * Try to flush and freeze file system on each original block device.
	 */
	list_for_each_entry(tracker, &snapshot->trackers, link) {
#if defined(HAVE_SUPER_BLOCK_FREEZE)
		_freeze_bdev(tracker->diff_area->orig_bdev, &tracker->diff_area->sb);
#else
#if defined(HAVE_BDEV_FREEZE)
		if (bdev_freeze(tracker->diff_area->orig_bdev))
#else
		if (freeze_bdev(tracker->diff_area->orig_bdev))
#endif
			pr_warn("Failed to freeze device [%u:%u]\n",
			       MAJOR(tracker->dev_id), MINOR(tracker->dev_id));
		else
			pr_debug("Device [%u:%u] was frozen\n",
				MAJOR(tracker->dev_id), MINOR(tracker->dev_id));
#endif
	}

	current_flag = memalloc_noio_save();
	/*
	 * Take snapshot - switch CBT tables and enable COW logic for each
	 * tracker.
	 */
	list_for_each_entry(tracker, &snapshot->trackers, link) {
		ret = tracker_take_snapshot(tracker);
		if (ret) {
			pr_err("Unable to take snapshot: failed to capture snapshot %pUb\n",
			       &snapshot->id);
			break;
		}
	}

	if (!ret)
		snapshot->is_taken = true;
	memalloc_noio_restore(current_flag);
	/*
	 * Thaw file systems on original block devices.
	 */
	list_for_each_entry(tracker, &snapshot->trackers, link) {
#if defined(HAVE_SUPER_BLOCK_FREEZE)
		_thaw_bdev(tracker->diff_area->orig_bdev, tracker->diff_area->sb);
#else
#if defined(HAVE_BDEV_FREEZE)
		if (bdev_thaw(tracker->diff_area->orig_bdev))
#else
		if (thaw_bdev(tracker->diff_area->orig_bdev))
#endif
			pr_warn("Failed to thaw device [%u:%u]\n",
			       MAJOR(tracker->dev_id), MINOR(tracker->dev_id));
		else
			pr_debug("Device [%u:%u] was unfrozen\n",
				MAJOR(tracker->dev_id), MINOR(tracker->dev_id));
#endif
	}
fail:
	if (ret) {
		list_for_each_entry(tracker, &snapshot->trackers, link) {
			if (tracker->diff_area) {
				diff_area_put(tracker->diff_area);
				tracker->diff_area = NULL;
			}
		}
	}
	up_write(&snapshot->rw_lock);
	return ret;
}

/*
 * Sometimes a snapshot is in the state of corrupt immediately after it is
 * taken.
 */
static int snapshot_check_trackers(struct snapshot *snapshot)
{
	int ret = 0;
	struct tracker *tracker;

	down_read(&snapshot->rw_lock);

	list_for_each_entry(tracker, &snapshot->trackers, link) {
		if (unlikely(diff_area_is_corrupted(tracker->diff_area))) {
			pr_err("Unable to create snapshot for device [%u:%u]: diff area is corrupted\n",
			       MAJOR(tracker->dev_id), MINOR(tracker->dev_id));
			ret = -EFAULT;
			break;
		}
	}

	up_read(&snapshot->rw_lock);

	return ret;
}

/*
 * Create all image block devices.
 */
static int snapshot_take_images(struct snapshot *snapshot)
{
	int ret = 0;
	struct tracker *tracker;

	down_write(&snapshot->rw_lock);

	list_for_each_entry(tracker, &snapshot->trackers, link) {
		ret = snapimage_create(tracker);

		if (ret) {
			pr_err("Failed to create snapshot image for device [%u:%u] with error=%d\n",
			       MAJOR(tracker->dev_id), MINOR(tracker->dev_id),
			       ret);
			break;
		}
	}

	up_write(&snapshot->rw_lock);
	return ret;
}

static int snapshot_release_trackers(struct snapshot *snapshot)
{
	int ret = 0;
	struct tracker *tracker;

	down_write(&snapshot->rw_lock);

	list_for_each_entry(tracker, &snapshot->trackers, link)
		tracker_release_snapshot(tracker);

	up_write(&snapshot->rw_lock);
	return ret;
}

int snapshot_take(const uuid_t *id)
{
	int ret = 0;
	struct snapshot *snapshot;

	snapshot = snapshot_get_by_id(id);
	if (!snapshot)
		return -ESRCH;

	if (!snapshot->is_taken) {
		ret = snapshot_take_trackers(snapshot);
		if (!ret) {
			ret = snapshot_check_trackers(snapshot);
			if (!ret)
				ret = snapshot_take_images(snapshot);
		}

		if (ret)
			snapshot_release_trackers(snapshot);
	} else
		ret = -EALREADY;

	snapshot_put(snapshot);

	if (ret)
		pr_err("Unable to take snapshot %pUb\n", &snapshot->id);
	else
		pr_info("Snapshot %pUb was taken successfully\n",
			&snapshot->id);
	return ret;
}

int snapshot_collect(unsigned int *pcount,
		     struct blksnap_uuid __user *id_array)
{
	int ret = 0;
	int inx = 0;
	struct snapshot *s;

	pr_debug("Collect snapshots\n");

	down_read(&snapshots_lock);
	if (list_empty(&snapshots))
		goto out;

	if (!id_array) {
		list_for_each_entry(s, &snapshots, link)
			inx++;
		goto out;
	}

	list_for_each_entry(s, &snapshots, link) {
		if (inx >= *pcount) {
			ret = -ENODATA;
			goto out;
		}

		if (copy_to_user(id_array[inx].b, &s->id.b, sizeof(uuid_t))) {
			pr_err("Unable to collect snapshots: failed to copy data to user buffer\n");
			goto out;
		}

		inx++;
	}
out:
	up_read(&snapshots_lock);
	*pcount = inx;
	return ret;
}

struct event *snapshot_wait_event(const uuid_t *id, unsigned long timeout_ms)
{
	struct snapshot *snapshot;
	struct event *event;

	snapshot = snapshot_get_by_id(id);
	if (!snapshot)
		return ERR_PTR(-ESRCH);

	event = event_wait(&snapshot->diff_storage->event_queue, timeout_ms);

	snapshot_put(snapshot);
	return event;
}

#ifdef BLKSNAP_MODIFICATION
int snapshot_append_storage(const uuid_t *id, const char *devpath, size_t count,
			    struct blksnap_sectors* __user ranges)
{
	int ret = 0;
	struct snapshot *snapshot;
#if defined(HAVE_BDEV_HANDLE)
	struct bdev_handle *bdev_handle;
#else
	struct block_device *bdev;
#endif
	dev_t dev_id;
	struct blksnap_sectors range;
	struct blksnap_sectors* __user src;

#if defined(HAVE_BDEV_HANDLE)
	bdev_handle = bdev_open_by_path(devpath,
					BLK_OPEN_READ | BLK_OPEN_WRITE,
					NULL, NULL);
	if (IS_ERR(bdev_handle))
		ret = PTR_ERR(bdev_handle);
	else
		dev_id = bdev_handle->bdev->bd_dev;
#else
#if defined(HAVE_BLK_HOLDER_OPS)
	bdev = blkdev_get_by_path(devpath, BLK_OPEN_READ | BLK_OPEN_WRITE, NULL, NULL);
#else
	bdev = blkdev_get_by_path(devpath, FMODE_READ | FMODE_WRITE, NULL);
#endif
	if (IS_ERR(bdev))
		ret = PTR_ERR(bdev);
	else
		dev_id = bdev->bd_dev;
#endif
	if (ret) {
		pr_err("Failed to open a block device '%s'\n", devpath);
		goto out;;
	}

	snapshot = snapshot_get_by_id(id);
	if (!snapshot) {
		ret = -ESRCH;
		goto out_bdev_release;
	}
	down_write(&snapshot->rw_lock);

#if defined(HAVE_BDEV_HANDLE)
	ret = diff_storage_add_bdev(snapshot->diff_storage, bdev_handle);

#else
	ret = diff_storage_add_bdev(snapshot->diff_storage, bdev);

#endif
	if (ret) {
		if (ret == -EALREADY)
			ret = 0;
		else {
			pr_err("Failed to add a block device\n");
			goto out_snapshot_put;
		}
	} else {
#if defined(HAVE_BDEV_HANDLE)
		bdev_handle = NULL;
#else
		bdev = NULL;
#endif
	}

	for (src = ranges; src < (ranges + count); src++) {
		if (copy_from_user(&range, src, sizeof(range))) {
			pr_err("Unable to get range: invalid user buffer\n");
			ret = -ENODATA;
			break;
		}

		ret = diff_storage_add_range(snapshot->diff_storage, dev_id, range);
		if (ret) {
			pr_err("Failed to add a range\n");
			break;
		}
	}
	{
		struct diff_storage *diff_storage = snapshot->diff_storage;

		if (atomic_read(&diff_storage->low_space_flag)) {
			if (diff_storage->capacity >= diff_storage->requested) {
				atomic_set(&diff_storage->low_space_flag, 0);
				pr_debug("Skip low_space_flag\n");
			}
		}

	}

out_snapshot_put:
	up_write(&snapshot->rw_lock);
	snapshot_put(snapshot);
out_bdev_release:
#if defined(HAVE_BDEV_HANDLE)
	if (bdev_handle)
		bdev_release(bdev_handle);
#elif defined(HAVE_BLK_HOLDER_OPS)
	if (bdev)
		blkdev_put(bdev, NULL);
#else
	if (bdev)
		blkdev_put(bdev, FMODE_READ | FMODE_WRITE);
#endif
out:
	return ret;
}
#endif
