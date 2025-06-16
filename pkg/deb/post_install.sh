if [ -z "$(dkms status -m blksnap -v #PACKAGE_VERSION# -k $(uname -r) | grep 'installed')" ]
then
	echo "WARNING: [TBD]The 'linux-headers' package for the current '$(uname -r)' kernel was not found."
	echo "WARNING: [TBD]Install the 'linux-headers-$(uname -r)' package into the system."
	echo "WARNING: [TBD]Or Install latest 'linux-image-$(dpkg --print-architecture)' and 'linux-headers-$(dpkg --print-architecture)' packages and reboot the system."
fi
