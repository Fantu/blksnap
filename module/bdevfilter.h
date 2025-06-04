/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023 Veeam Software Group GmbH */
#ifndef _UAPI_LINUX_BDEVFILTER_H
#define _UAPI_LINUX_BDEVFILTER_H

#include <linux/types.h>

#define BDEVFILTER "bdevfilter"
#define BDEVFILTER_NAME_LENGTH	32

/**
 * struct bdevfilter_attach - parameter for BLKFILTER_ATTACH ioctl.
 *
 * @name:       Name of block device filter.
 * @opt:	Userspace buffer with options.
 * @optlen:	Size of data at @opt.
 */
struct bdevfilter_attach {
	__u64 devpath;
	__u8 name[BDEVFILTER_NAME_LENGTH];
	__u64 opt;
	__u32 optlen;
};

/**
 * struct bdevfilter_name - parameter for BLKFILTER_DETACH ioctl.
 *
 * @name:       Name of block device filter.
 */
struct bdevfilter_name {
	__u64 devpath;
	__u8 name[BDEVFILTER_NAME_LENGTH];
};

/**
 * struct bdevfilter_ctl - parameter for bdevfilter_ctl ioctl
 *
 * @name:	Name of block device filter.
 * @cmd:	The filter-specific operation code of the command.
 * @optlen:	Size of data at @opt.
 * @opt:	Userspace buffer with options.
 */
struct bdevfilter_ctl {
	__u64 devpath;
	__u8 name[BDEVFILTER_NAME_LENGTH];
	__u32 cmd;
	__u32 optlen;
	__u64 opt;
};

/**
 * @tz_minuteswest:
 *  Time zone offset in minutes.
 *  The system time is in UTC. In order for the module to write local time
 *  to the log, its offset should be specified.
 * @level:
 *  0 - disable logging to file
 *  3 - only error messages
 *  4 - log warnings
 *  6 - log info messages
 *  7 - log debug messages
 * @filepath_size:
 *  Count of bytes in &filepath.
 * @filename:
 *  Pointer to full path for log file.
 */
struct bdevfilter_setlog {
    __s32 tz_minuteswest;
    __s32 level;
    __u32 filepath_size;
    __u64 filepath;
};

#define BDEVFILTER_ATTACH	_IOWR('F', 140, struct bdevfilter_attach)
#define BDEVFILTER_DETACH	_IOWR('F', 141, struct bdevfilter_name)
#define BDEVFILTER_CTL		_IOWR('F', 142, struct bdevfilter_ctl)
#define BDEVFILTER_SETLOG       _IOW ('F', 143, struct bdevfilter_setlog)

#endif /* _UAPI_LINUX_BDEVFILTER_H */
