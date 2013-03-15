Name:       smartcard-plugin-uicc
Summary:    Smartcard plugin uicc
Version:    0.0.3
Release:    0
Group:      libs
License:    Apache License, Version 2.0
Source0:    %{name}-%{version}.tar.gz
BuildRequires: pkgconfig(dlog)
BuildRequires: pkgconfig(tapi)
BuildRequires: pkgconfig(smartcard-service)
BuildRequires: pkgconfig(smartcard-service-common)
BuildRequires: cmake
BuildRequires: gettext-tools
Requires(post):   /sbin/ldconfig
Requires(post):   /usr/bin/vconftool
requires(postun): /sbin/ldconfig

%description
Smartcard Service plugin uicc

%prep
%setup -q

%package    devel
Summary:    smartcard service uicc
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
smartcard service.

%build
mkdir obj-arm-limux-qnueabi
cd obj-arm-limux-qnueabi
cmake .. -DCMAKE_INSTALL_PREFIX=%{_prefix}
#make %{?jobs:-j%jobs}

%install
cd obj-arm-limux-qnueabi
%make_install
mkdir -p %{buildroot}/usr/share/license
cp -af %{_builddir}/%{name}-%{version}/packaging/smartcard-plugin-uicc %{buildroot}/usr/share/license/

%post
/sbin/ldconfig


%postun
/sbin/ldconfig

#%post
# -n nfc-common-lib -p /sbin/ldconfig

#%postun
# -n nfc-common-lib -p /sbin/ldconfig

%files
%manifest smartcard-plugin-uicc.manifest
%defattr(-,root,root,-)
/usr/lib/se/lib*.so
/usr/share/license/smartcard-plugin-uicc
