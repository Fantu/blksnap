Name:           blksnap
Version:        #PACKAGE_VERSION#
Release:        1
BuildArch:      noarch
Summary:        Veeam Agent for Linux (kernel module)
Packager:       Veeam Software Group GmbH
Vendor:         Veeam Software Group GmbH
Group:          System Environment/Kernel
License:        GPL-2.0
AutoReqProv:    no
Requires:       dkms
Requires:       gcc, make, perl
Requires:       kernel-devel
Provides:       %{name} = %{version}
Conflicts:      kmod-%{name}
Conflicts:      veeamsnap

%description
This kernel module implements snapshot and changed block tracking
functionality used by Veeam Agent for Linux - simple and FREE backup agent
designed to ensure the Availability of your Linux server instances, whether
they reside in the public cloud or on premises.

%post
for POSTINST in /usr/lib/dkms/common.postinst; do
  if [ -f $POSTINST ]; then
    $POSTINST %{name} %{version}
    RETVAL=$?
    if [ -z "$(dkms status -m %{name} -v %{version} -k $(uname -r) | grep 'installed')" ] ; then
        echo "WARNING: Package not configured! See output!"
        exit 1
    fi
    exit $RETVAL
  fi
  echo "WARNING: $POSTINST does not exist."
done
echo -e "ERROR: DKMS version is too old and %{name} was not"
echo -e "built with legacy DKMS support."
echo -e "You must either rebuild %{name} with legacy postinst"
echo -e "support or upgrade DKMS to a more current version."
exit 1

%postun
exit 0

%preun
checkModule()
{
  if ! lsmod | grep "$1" > /dev/null
  then
    return 1
  fi
  return 0
}

if [ -e "/dev/blksnap" ]
then
  modprobe -r blksnap 2>/dev/null || true
fi
if [ -e "/dev/veeamblksnap" ]
then
  modprobe -r veeamblksnap 2>/dev/null || true
fi

if checkModule bdevfilter
then
  if [ -e /sys/kernel/livepatch/bdevfilter/enabled ]
  then
    echo 0 > /sys/kernel/livepatch/bdevfilter/enabled || true
    while [ -e /sys/kernel/livepatch/bdevfilter/enabled ]
    do
      sleep 1s
    done
  fi
  modprobe -r bdevfilter 2>/dev/null || true
fi

if [  "$(dkms status -m %{name} -v %{version})" ]; then
  dkms remove -m %{name} -v %{version} --all
fi
exit 0

%files
/usr/src/%{name}-%{version}/*

#build definition
%define _rpmdir ./

%changelog
* Fri Dec 1 2021 Veeam Software GmbH veeam_team@veeam.com - #PACKAGE_VERSION#
- First release
