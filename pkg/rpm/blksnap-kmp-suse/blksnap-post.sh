if mokutil --sb-state | grep -qw "SecureBoot enabled"
then
	if ! mokutil --list-enrolled | grep -qw "CN=Veeam Software Group GmbH"
	then
		echo "ERROR: [TBD]The Veeam Software certificate should be installed."
		echo "ERROR: [TBD]Please install ueficert package and complete MOK enrollment to continue."
		exit 1
	fi
fi
