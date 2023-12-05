/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023 Veeam Software Group GmbH */
#ifndef __BLKSNAP_COMPAT_H
#define __BLKSNAP_COMPAT_H

#include <linux/sched/mm.h>
#include <linux/blkdev.h>
#include <linux/slab.h>
#include <linux/bio.h>
#include <linux/fs.h>

#ifndef PAGE_SECTORS
#define PAGE_SECTORS	(1 << (PAGE_SHIFT - SECTOR_SHIFT))
#endif

#ifndef HAVE_MEMCPY_PAGE
static inline void memcpy_page(struct page *dst_page, size_t dst_off,
			       struct page *src_page, size_t src_off,
			       size_t len)
{
	char *dst = kmap_atomic(dst_page);
	char *src = kmap_atomic(src_page);

	memcpy(dst + dst_off, src + src_off, len);
	kunmap_atomic(src);
	kunmap_atomic(dst);
}
#endif

#ifndef HAVE_BVEC_SET_PAGE
static inline void bvec_set_page(struct bio_vec *bv, struct page *page,
		unsigned int len, unsigned int offset)
{
	bv->bv_page = page;
	bv->bv_len = len;
	bv->bv_offset = offset;
}
#endif

#if defined(HAVE_SUPER_BLOCK_FREEZE)
static inline int _freeze_bdev(struct block_device *bdev,
                               struct super_block **psuperblock)
{
        struct super_block *superblock;

        pr_debug("Freezing device [%u:%u]\n", MAJOR(bdev->bd_dev),
                 MINOR(bdev->bd_dev));

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

#ifndef BIO_MAX_VECS
static inline unsigned int bio_max_segs(unsigned int nr_segs)
{
        return min(nr_segs, 256U);
}
#endif

#ifndef HAVE_BDEV_NR_SECTORS
static inline sector_t bdev_nr_sectors(struct block_device *bdev)
{
        return i_size_read(bdev->bd_inode) >> 9;
};
#endif

#endif /* __BLKSNAP_COMPAT_H */
