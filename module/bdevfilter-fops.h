/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023 Veeam Software Group GmbH */

static int get_symbol(const char *name, void **paddr);
static int __blkfilter_detach(dev_t dev_id, char *name, size_t name_length);

static inline void bdev_mutex_lock(struct block_device *bdev)
{
#ifdef HAVE_GENDISK_OPEN_MUTEX
	mutex_lock(&bdev->bd_disk->open_mutex);
#else
	mutex_lock(&bdev->bd_mutex);
#endif
}

static inline void bdev_mutex_unlock(struct block_device *bdev)
{
#ifdef HAVE_GENDISK_OPEN_MUTEX
	mutex_unlock(&bdev->bd_disk->open_mutex);
#else
	mutex_unlock(&bdev->bd_mutex);
#endif
}

#ifdef HAVE_NOT_FTRACE_FREE_FILTER
static unsigned long addr_ftrace_free_filter;

static inline int prepare_ftrace_free_filter(unsigned long kernel_base)
{
	int ret;
	void *addr;

	ret = get_symbol("ftrace_free_filter", &addr);
	if (ret) {
		pr_err("Failed to get address of the '%s'\n", "ftrace_free_filter");
		return ret;
	}

	addr_ftrace_free_filter = kernel_base + (unsigned long)addr;
	pr_debug("Function '%s' has been found\n", "ftrace_free_filter");
	return 0;
}

static inline void bdevfilter_ftrace_free_filter(struct ftrace_ops *ops)
{
	((void (*)(struct ftrace_ops *ops))addr_ftrace_free_filter)(ops);
}
#else
static inline int prepare_ftrace_free_filter(unsigned long kernel_base)
{
	(void)(kernel_base);
	return 0;
}
static inline void bdevfilter_ftrace_free_filter(struct ftrace_ops *ops)
{
	ftrace_free_filter(ops);
}
#endif

#ifdef HAVE_DISK_LIVE
static inline bool device_alive(struct block_device *bdev)
{
	return disk_live(bdev->bd_disk);
}
#else
static inline bool device_alive(struct block_device *bdev)
{
	return !inode_unhashed(BD_INODE(bdev));
}
#endif


#if defined(HAVE_BI_BDISK)
static inline struct hd_struct *bdevfilter_disk_get_part(struct gendisk *disk, int partno)
{
	struct disk_part_tbl *ptbl = rcu_dereference(disk->part_tbl);

	if (unlikely(partno < 0 || partno >= ptbl->len))
		return NULL;
	return rcu_dereference(ptbl->part[partno]);
}
static inline dev_t bdevfilter_disk_get_dev(struct gendisk *disk, int partno)
{
	dev_t dev_id = 0;
	struct hd_struct *part;

	rcu_read_lock();
	part = bdevfilter_disk_get_part(disk, partno);
	if (part)
		dev_id = part_devt(part);
	rcu_read_unlock();

	return dev_id;
}

static inline dev_t bdevfilter_dev_id_by_bio(struct bio *bio)
{
	return bdevfilter_disk_get_dev(bio->bi_disk, bio->bi_partno);
}
#else
static inline dev_t bdevfilter_dev_id_by_bio(struct bio *bio)
{
	return bio->bi_bdev->bd_dev;
}
#endif

struct bdev_extension {
	struct list_head link;
	dev_t dev_id;
	struct blkfilter *flt;
};

/* The list of extensions for this block device */
static LIST_HEAD(bdev_extension_list);

/* Lock the queue of block device to add or delete extension. */
static DEFINE_SPINLOCK(bdev_extension_list_lock);

static inline struct bdev_extension *bdev_extension_find(dev_t dev_id)
{
	struct bdev_extension *ext;

	if (list_empty(&bdev_extension_list))
		return NULL;

	list_for_each_entry (ext, &bdev_extension_list, link)
		if (dev_id == ext->dev_id)
			return ext;

	return NULL;
}


static LIST_HEAD(bdevfilters);
static DEFINE_SPINLOCK(bdevfilters_lock);

static inline struct bdevfilter_operations *__bdevfilter_operations_find(
							const char *name)
{
	struct bdevfilter_operations *fops;

	list_for_each_entry(fops, &bdevfilters, link)
		if (strncmp(fops->name, name, BDEVFILTER_NAME_LENGTH) == 0)
			return fops;
	return NULL;
}

static inline struct bdevfilter_operations *bdevfilter_operations_find(
							 const char *name)
{
	struct bdevfilter_operations *fops;

	spin_lock(&bdevfilters_lock);
	fops = __bdevfilter_operations_find(name);
	spin_unlock(&bdevfilters_lock);
	return fops;
}

