Name:             smartcard-plugin-uicc
Summary:          Smartcard plugin uicc
Version:          0.0.11
Release:          0
Group:            libs
License:          Apache-2.0
Source0:          %{name}-%{version}.tar.gz
BuildRequires:    cmake
BuildRequires:    pkgconfig(glib-2.0)
BuildRequires:    pkgconfig(dlog)
BuildRequires:    pkgconfig(tapi)
BuildRequires:    pkgconfig(smartcard-service-common)
Requires(post):   /sbin/ldconfig
Requires(postun): /sbin/ldconfig


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
%if 0%{?sec_build_binary_debug_enable}
export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"
%endif
mkdir obj-arm-limux-qnueabi
cd obj-arm-limux-qnueabi
cmake .. -DCMAKE_INSTALL_PREFIX=%{_prefix} \
%ifarch aarch64 x86_64
	 -DTIZEN_ARCH_64=1 \
%endif

#make %{?jobs:-j%jobs}


%install
cd obj-arm-limux-qnueabi
%make_install
mkdir -p %{buildroot}/usr/share/license
cp -af %{_builddir}/%{name}-%{version}/packaging/%{name} %{buildroot}/usr/share/license/


%post
/sbin/ldconfig


%postun
/sbin/ldconfig


%files
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/se/lib*.so
%{_datadir}/license/%{name}
