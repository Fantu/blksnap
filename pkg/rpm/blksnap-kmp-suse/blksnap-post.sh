if [ -d /sys/firmware/efi ]
then
	if [ -f /sys/firmware/efi/efivars/SecureBootEnable-* ]
	then
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
	fi
fi
