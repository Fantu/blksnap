/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023 Veeam Software Group GmbH */

static int bdevfilter_set(struct ftrace_ops *ops, unsigned char *name);
static void bdevfilter_unset(struct ftrace_ops *ops);

static inline void __blkfilter_detach_disk(struct gendisk *disk)
{
#ifdef HAVE_DISK_PART_ITER
	struct disk_part_iter piter;
	struct hd_struct *part;
	struct block_device *bdev;

	disk_part_iter_init(&piter, disk, DISK_PITER_INCL_EMPTY);
	while ((part = disk_part_iter_next(&piter))) {
		bdev = bdget_disk(disk, part->partno);
		if (!bdev)
			continue;
		__blkfilter_detach(bdev->bd_dev, NULL, 0);
		bdput(bdev);
	}
	disk_part_iter_exit(&piter);
	bdev = bdget_disk(disk, 0);
	if (!bdev)
		return;
	__blkfilter_detach(bdev->bd_dev, NULL, 0);
	bdput(bdev);
#else
	struct block_device *part;
	unsigned long idx;

	xa_for_each_start(&disk->part_tbl, idx, part, 1)
		__blkfilter_detach(part->bd_dev, NULL, 0);
	__blkfilter_detach(disk->part0->bd_dev, NULL, 0);
#endif
}

/*
 * ftrace for the del_gendisk()
 */
static notrace __attribute__((optimize("no-optimize-sibling-calls")))
void del_gendisk_handler(struct gendisk *disk)
{
	pr_debug("Mark disk '%s' dead\n", disk->disk_name);
	__blkfilter_detach_disk(disk);
	del_gendisk(disk);
}

static notrace void ftrace_handler_del_gendisk(
	unsigned long ip, unsigned long parent_ip, struct ftrace_ops *fops,
#ifdef HAVE_FTRACE_REGS
	struct ftrace_regs *fregs
#else
	struct pt_regs *regs
#endif
	)
{
	if (within_module(parent_ip, THIS_MODULE))
		return;

#if defined(HAVE_FTRACE_REGS_SET_INSTRUCTION_POINTER)
	ftrace_regs_set_instruction_pointer(fregs, (unsigned long)del_gendisk_handler);
#elif defined(HAVE_FTRACE_REGS)
	ftrace_instruction_pointer_set(fregs, (unsigned long)del_gendisk_handler);
#else
	instruction_pointer_set(regs, (unsigned long)del_gendisk_handler);
#endif
}

static struct ftrace_ops ops_del_gendisk = {
	.func = ftrace_handler_del_gendisk,
	.flags = FTRACE_OPS_FL_DYNAMIC |
		FTRACE_OPS_FL_SAVE_REGS |
		FTRACE_OPS_FL_IPMODIFY |
		FTRACE_OPS_FL_PERMANENT,
};

/*
 * ftrace for the bdev_disk_changed())
 */

#if defined(HAVE_BDEV_DISK_CHANGED_DISK)
static notrace __attribute__((optimize("no-optimize-sibling-calls")))
int bdev_disk_changed_handler(struct gendisk *disk, bool invalidate)
{
#ifdef GENHD_FL_UP
	if (!(disk->flags & GENHD_FL_UP))
		goto out;
#else
	if (!disk_live(disk))
		goto out;
#endif
	if (disk->open_partitions)
		goto out;

	pr_debug("Mark disk '%s' changed\n", disk->disk_name);
	__blkfilter_detach_disk(disk);
out:
	return bdev_disk_changed(disk, invalidate);
}
#elif defined(HAVE_BDEV_DISK_CHANGED_BDEV)
static notrace __attribute__((optimize("no-optimize-sibling-calls")))
int bdev_disk_changed_handler(struct block_device *bdev, bool invalidate)
{
	struct gendisk *disk = bdev->bd_disk;

	if (!(disk->flags & GENHD_FL_UP))
		goto out;
	if (bdev->bd_part_count)
		goto out;

	pr_debug("Mark block device '%d:%d' changed\n",
		MAJOR(bdev->bd_dev), MINOR(bdev->bd_dev));
	__blkfilter_detach_disk(disk);
out:
	return bdev_disk_changed(bdev, invalidate);
}
#endif

static notrace void ftrace_handler_bdev_disk_changed(
	unsigned long ip, unsigned long parent_ip, struct ftrace_ops *fops,
#ifdef HAVE_FTRACE_REGS
	struct ftrace_regs *fregs
#else
	struct pt_regs *regs
#endif
	)
{
	if (within_module(parent_ip, THIS_MODULE))
		return;

#if defined(HAVE_FTRACE_REGS_SET_INSTRUCTION_POINTER)
	ftrace_regs_set_instruction_pointer(fregs, (unsigned long)bdev_disk_changed_handler);
#elif defined(HAVE_FTRACE_REGS)
	ftrace_instruction_pointer_set(fregs, (unsigned long)bdev_disk_changed_handler);
#else
	instruction_pointer_set(regs, (unsigned long)bdev_disk_changed_handler);
#endif
}

static struct ftrace_ops ops_bdev_disk_changed = {
	.func = ftrace_handler_bdev_disk_changed,
	.flags = FTRACE_OPS_FL_DYNAMIC |
		FTRACE_OPS_FL_SAVE_REGS |
		FTRACE_OPS_FL_IPMODIFY |
		FTRACE_OPS_FL_PERMANENT,
};

static inline int prepare_functions(unsigned long kernel_base)
{
	(void)(kernel_base);
	return 0;
}
static inline int set_functions(void)
{
	int ret;

	ret = bdevfilter_set(&ops_del_gendisk, "del_gendisk");
	if (ret)
		return ret;
	ret = bdevfilter_set(&ops_bdev_disk_changed, "bdev_disk_changed");
	if (ret)
		bdevfilter_unset(&ops_del_gendisk);
	return ret;

}

static inline void unset_functions(void)
{
	bdevfilter_unset(&ops_bdev_disk_changed);
	bdevfilter_unset(&ops_del_gendisk);
}
