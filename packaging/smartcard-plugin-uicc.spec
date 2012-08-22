Name:       smartcard-plugin-uicc
Summary:    Smartcard plugin uicc
Version:    0.0.1
Release:    4
Group:      libs
License:    Samsung Proprietary License
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

%post
/sbin/ldconfig


%postun
/sbin/ldconfig

#%post
# -n nfc-common-lib -p /sbin/ldconfig

#%postun
# -n nfc-common-lib -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
/usr/lib/se/lib*.so
