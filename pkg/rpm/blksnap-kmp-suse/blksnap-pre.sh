if mokutil --sb-state | grep -qw "SecureBoot enabled"
then
	echo "[TBD]Secure Boot is enabled."
	if mokutil --list-enrolled | grep -qw "CN=Veeam Software Group GmbH"
	then
		echo "[TBD]The Veeam Software certificate was found."
	else
		echo "[TBD]The Veeam Software certificate should be installed."
		echo "[TBD]Please install ueficert package and complete MOK enrollment to continue."
		exit 1
	fi
else
	echo "[TBD]Secure Boot is disabled."
fi
