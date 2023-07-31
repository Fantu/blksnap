#!/bin/bash -e
# SPDX-License-Identifier: GPL-2.0
MODULE_NAME=veeamblksnap
FILTER_NAME=bdevfilter
CMD=$1
if [ -n "$2" ]
then
	if [ "-" = "$2" ]
	then
		echo "kernel version is not set"
    else
	    KERNEL_RELEASE="$2"
    fi
else
	KERNEL_RELEASE="$(uname -r)"
fi
MODULE_PATH=/lib/modules/${KERNEL_RELEASE}/kernel/drivers/block

case "$CMD" in
	build)
		echo Making ...
		make -j`nproc` -C /lib/modules/${KERNEL_RELEASE}/build M=$(pwd) modules
		echo Completed.
		;;
	clean)
		echo Cleaning ...
		make -C /lib/modules/${KERNEL_RELEASE}/build M=$(pwd) clean
		echo Completed.
		;;
	install)
		echo "Installing ${FILTER_NAME}"
		mkdir -p ${MODULE_PATH}
		cp ${FILTER_NAME}.ko ${MODULE_PATH}
		depmod
		echo Completed.
		echo "Installing ${MODULE_NAME}"
		mkdir -p ${MODULE_PATH}
		cp ${MODULE_NAME}.ko ${MODULE_PATH}
		depmod
		echo Completed.
		;;
	uninstall)
		echo "Uninstalling ${MODULE_NAME}"
		rm -f ${MODULE_PATH}/${MODULE_NAME}.ko
		echo "Uninstalling ${FILTER_NAME}"
		rm -f ${MODULE_PATH}/${FILTER_NAME}.ko
		;;
	load)
		echo "Loading ${FILTER_NAME} kernel module from current folder"
		insmod ./${FILTER_NAME}.ko
		echo "Loading ${MODULE_NAME} kernel module from current folder"
		insmod ./${MODULE_NAME}.ko
		;;
	unload)
		echo "Unloading ${MODULE_NAME} kernel module"
		rmmod ${MODULE_NAME}
		echo "Unloading ${FILTER_NAME} kernel module"
		rmmod ${FILTER_NAME}
		;;
	*)
		echo "Usage "
		echo "Compile project: "
		echo "	$0 {build | clean} [<kernel release>]"
		echo "for ${MODULE_NAME} module : "
		echo "	$0 {install | uninstall | load | unload}  [<kernel release>]"
		exit 1
esac
