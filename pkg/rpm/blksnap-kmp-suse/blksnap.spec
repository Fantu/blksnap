Name: blksnap
BuildRequires: kernel-syms modutils
License: GPL-2.0
Packager: "#PACKAGE_VENDOR#"
Vendor: "#PACKAGE_VENDOR#"
Group: System/Kernel
Summary: Veeam Agent for Linux (kernel module)
Version: #PACKAGE_VERSION#
Release: #PACKAGE_RELEASE#
Source0: %{name}-%{version}.tar.gz
Source10: %{name}-%{version}-preamble
Source20: kernel-module-subpackage
BuildRoot: %{_tmppath}/%{name}-%{version}-build
URL: http://github.com/veeam/%{name}
Requires: modutils suse-module-tools

%kernel_module_package -n %{name} -p %{_sourcedir}/%{name}-%{version}-preamble -t %{_sourcedir}/kernel-module-subpackage

%description
This kernel module implements snapshot and changed block tracking
functionality used by Veeam Agent for Linux - simple and FREE backup agent
designed to ensure the Availability of your Linux server instances, whether
they reside in the public cloud or on premises.

%package KMP
Summary: Veeam Agent for Linux (kernel module)
Group:	System/Kernel

%description KMP
This kernel module implements snapshot and changed block tracking
functionality used by Veeam Agent for Linux - simple and FREE backup agent
designed to ensure the Availability of your Linux server instances, whether
they reside in the public cloud or on premises.

%prep
%setup -q -n %{name}-%{version}
mkdir obj

%build
for flavor in %flavors_to_build; do
    rm -rf obj/$flavor
    cp -r source obj/$flavor
	%{__make} -C %{kernel_source $flavor} modules M=$PWD/obj/$flavor
done

%install
export INSTALL_MOD_PATH=$RPM_BUILD_ROOT
export INSTALL_MOD_DIR=updates/drivers
export INSTALL_MOD_STRIP=--strip-debug
for flavor in %flavors_to_build; do
	make -C %{kernel_source $flavor} modules_install M=$PWD/obj/$flavor
done

%clean
rm -rf %{buildroot}

%changelog
