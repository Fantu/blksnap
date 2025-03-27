Name:    blksnap
Version: #PACKAGE_VERSION#
Release: %{?release}%{?dist}
Packager: "#PACKAGE_VENDOR#"
Vendor:  "#PACKAGE_VENDOR#"
Group:   System Environment/Kernel
License: GPL-2
Summary: Veeam Agent for Linux (kernel module)
URL:     http://github.com/veeam/blksnap
ExclusiveOS:    linux
ExclusiveArch:  %{ix86} x86_64

Source0: %{name}-%{version}.tar.gz

BuildRoot: %{_tmppath}/%{name}-buildroot

%description
This kernel module implements snapshot and changed block tracking
functionality used by Veeam Agent for Linux - simple and FREE backup agent
designed to ensure the Availability of your Linux server instances, whether
they reside in the public cloud or on premises.

%package -n kmod-%{name}-patch
Summary: Veeam Agent for Linux (kernel module)
Group: System Environment/Kernel
Requires: python3, kmod-blksnap = %{version}

%description -n kmod-%{name}-patch
This kernel module implements snapshot and changed block tracking
functionality used by Veeam Agent for Linux - simple and FREE backup agent
designed to ensure the Availability of your Linux server instances, whether
they reside in the public cloud or on premises.

# Disable the building of the debug package(s).
%define debug_package %{nil}

%prep
%setup -q -n %{name}-%{version}

%build
for kver in %{kversion}; do
	KSRC=%{_usrsrc}/kernels/${kver}

	mkdir -p $PWD/${kver}
	cp -rf $PWD/module/ $PWD/${kver}/
	%{__make} -j$(nproc) -C "${KSRC}" %{?_smp_mflags} modules M=$PWD/module
done

for kver in %{kversion}; do
	KSRC=%{_usrsrc}/kernels/${kver}

	export INSTALL_MOD_PATH=%{buildroot}
	export INSTALL_MOD_DIR=extra
	export INSTALL_MOD_STRIP=--strip-debug

	%{__make} -C "${KSRC}" modules_install M=$PWD/module
	%{__rm} -f %{buildroot}/lib/modules/${kver}/modules.*
done

%files -n kmod-%{name}-patch
%defattr(644,root,root,755)
/lib/modules/

%clean
%{__rm} -rf %{buildroot}

%changelog
