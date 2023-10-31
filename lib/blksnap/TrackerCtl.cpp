/*
 * Copyright (C) 2022 Veeam Software Group GmbH <https://www.veeam.com/contacts.html>
 *
 * This file is part of libblksnap
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Lesser Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <blksnap/TrackerCtl.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>

using namespace blksnap;

#define BLKSNAP_FILTER_NAME {'b','l','k','s','n','a','p','\0'}

CTrackerCtl::CTrackerCtl(const std::string& devicePath)
{
    m_fd = ::open(devicePath.c_str(), O_DIRECT, 0600);
    if (m_fd < 0)
        throw std::system_error(errno, std::generic_category(),
            "Failed to open block device ["+devicePath+"].");
}
CTrackerCtl::~CTrackerCtl()
{
    if (m_fd > 0) {
        ::close(m_fd);
        m_fd = 0;
    }
}

bool CTrackerCtl::Attach()
{
    struct blkfilter_name name = {
        .name = BLKSNAP_FILTER_NAME,
    };

    if (::ioctl(m_fd, BLKFILTER_ATTACH, &name) < 0) {
        if (errno == EALREADY)
            return false;
        else
            throw std::system_error(errno, std::generic_category(),
                "Failed to attach 'blksnap' filter.");
    }
    return true;
}
void CTrackerCtl::Detach()
{
    struct blkfilter_name name = {
        .name = BLKSNAP_FILTER_NAME,
    };

    if (::ioctl(m_fd, BLKFILTER_DETACH, &name) < 0)
        throw std::system_error(errno, std::generic_category(),
            "Failed to detach 'blksnap' filter.");

}

void CTrackerCtl::CbtInfo(struct blksnap_cbtinfo& cbtInfo)
{
    struct blkfilter_ctl ctl = {
        .name = BLKSNAP_FILTER_NAME,
        .cmd = blkfilter_ctl_blksnap_cbtinfo,
        .optlen = sizeof(cbtInfo),
        .opt = (__u64)&cbtInfo,
    };

    if (::ioctl(m_fd, BLKFILTER_CTL, &ctl) < 0)
        throw std::system_error(errno, std::generic_category(),
            "Failed to get CBT information.");
}
void CTrackerCtl::ReadCbtMap(unsigned int offset, unsigned int length, uint8_t* buff)
{
    struct blksnap_cbtmap arg = {
        .offset = offset,
        .buffer = (__u64)buff
    };
    struct blkfilter_ctl ctl = {
        .name = BLKSNAP_FILTER_NAME,
        .cmd = blkfilter_ctl_blksnap_cbtmap,
        .optlen = sizeof(arg),
        .opt = (__u64)&arg,
    };

    if (::ioctl(m_fd, BLKFILTER_CTL, &ctl) < 0)
        throw std::system_error(errno, std::generic_category(),
            "Failed to read CBT map.");

}
void CTrackerCtl::MarkDirtyBlock(std::vector<struct blksnap_sectors>& ranges)
{
    struct blksnap_cbtdirty arg = {
        .count = static_cast<unsigned int>(ranges.size()),
        .dirty_sectors = (__u64)ranges.data(),
    };
    struct blkfilter_ctl ctl = {
        .name = BLKSNAP_FILTER_NAME,
        .cmd = blkfilter_ctl_blksnap_cbtdirty,
        .optlen = sizeof(arg),
        .opt = (__u64)&arg,
    };

    if (::ioctl(m_fd, BLKFILTER_CTL, &ctl) < 0)
        throw std::system_error(errno, std::generic_category(),
            "Failed to mark block as 'dirty' in CBT map.");
}
void CTrackerCtl::SnapshotAdd(const uuid_t& id)
{
    struct blksnap_snapshotadd arg;
    uuid_copy(arg.id.b, id);

    struct blkfilter_ctl ctl = {
        .name = BLKSNAP_FILTER_NAME,
        .cmd = blkfilter_ctl_blksnap_snapshotadd,
        .optlen = sizeof(arg),
        .opt = (__u64)&arg,
    };

    if (::ioctl(m_fd, BLKFILTER_CTL, &ctl) < 0)
        throw std::system_error(errno, std::generic_category(),
            "Failed to add device to snapshot.");
}
void CTrackerCtl::SnapshotInfo(struct blksnap_snapshotinfo& snapshotinfo)
{
    struct blkfilter_ctl ctl = {
        .name = BLKSNAP_FILTER_NAME,
        .cmd = blkfilter_ctl_blksnap_snapshotinfo,
        .optlen = sizeof(snapshotinfo),
        .opt = (__u64)&snapshotinfo,
    };

    if (::ioctl(m_fd, BLKFILTER_CTL, &ctl) < 0)
        throw std::system_error(errno, std::generic_category(),
            "Failed to get snapshot information.");
}



