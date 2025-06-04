#!/bin/bash -e
#
# SPDX-License-Identifier: GPL-2.0+

. ../functions.sh
. ../blksnap.sh

echo "---"
echo "FIO deep depth read test"

fio --version
blksnap_load
blksnap_version
blksnap_log_debug "/var/log/veeam"

if [ -z $1 ]
then
	TEST_DIR=${HOME}/blksnap-test
else
	TEST_DIR=$(realpath $1"/blksnap-test")
fi
mkdir -p ${TEST_DIR}
rm -rf ${TEST_DIR}/*

MP_TEST_DIR=$(stat -c %m ${TEST_DIR})
DEVICE="/dev/block/"$(mountpoint -d ${MP_TEST_DIR})
echo "Test directory [${TEST_DIR}] on device [${DEVICE}] selected"

MP_DIR=/mnt/blksnap-test
rm -rf ${MP_DIR}
mkdir -p ${MP_DIR}

fio --directory "${MP_TEST_DIR}" --section read_deep_depth ./blksnap.fio

# blksnap_snapshot_create "${DEVICE}" "/dev/shm" "1G"
DIFF_STORAGE="$(pwd)/diff_storage"
fallocate --length 3GiB ${DIFF_STORAGE}
blksnap_snapshot_create "${DEVICE}" "${DIFF_STORAGE}" "3G"
blksnap_snapshot_watcher
blksnap_snapshot_take


IMAGE=${MP_DIR}/image0
mkdir -p ${IMAGE}

echo "Mount image"
DEVICE_IMAGE=$(blksnap_get_image ${DEVICE})
mount ${DEVICE_IMAGE} ${IMAGE}

fio --directory "${IMAGE}/${MP_TEST_DIR}" --section read_deep_depth ./blksnap.fio

echo "Umount image"
umount ${IMAGE}

echo "Destroy snapshot"
blksnap_snapshot_destroy
blksnap_watcher_wait
blksnap_detach ${DEVICE}

blksnap_unload

echo "FIO deep depth read test finish"
echo "---"
