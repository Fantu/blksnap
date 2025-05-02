if [ ! -d /sys/firmware/efi ]
then
	exit 0
fi
if [ ! -f /sys/firmware/efi/efivars/SecureBoot-* ]
then
	exit 0
fi
SB_ENABLE=$(od --address-radix=n --format=u1 --skip=4 -N 1 /sys/firmware/efi/efivars/SecureBoot-* | tr -d ' ')
if [ ${SB_ENABLE} = "0" ]
then
	exit 0
fi
echo "[TBD]Secure boot enabled has been detected."
if which mokutil
then
	if mokutil --list-enrolled | grep -qw "CN=Veeam Software Group GmbH"
	then
		echo "[TBD]The Veeam Software certificate is installed."
	else
		echo "ERROR: [TBD]The Veeam Software certificate should be installed."
		echo "ERROR: [TBD]Please install 'veeam-uefi-cert' package and complete MOK enrollment to continue."
	fi
else
	echo "ERROR: [TBD]The 'mokutil' is not installed."
	echo "ERROR: [TBD]Please install 'mokutil' package, install 'veeam-uefi-cert' package and complete MOK enrollment to continue."
fi
