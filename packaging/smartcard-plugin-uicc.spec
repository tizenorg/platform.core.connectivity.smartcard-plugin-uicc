Name:       smartcard-plugin-uicc
Summary:    Smartcard plugin uicc
Version:          0.0.7
Release:    0
Group:      Telephony/Service
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1001: 	smartcard-plugin-uicc.manifest
#ExclusiveArch:    %%arm
BuildRequires:    cmake
BuildRequires:    pkgconfig(glib-2.0)
BuildRequires: pkgconfig(dlog)
BuildRequires: pkgconfig(tapi)
BuildRequires: pkgconfig(smartcard-service-common)
Requires(post):   /sbin/ldconfig
Requires(postun): /sbin/ldconfig


%description
Smartcard Service plugin uicc

%prep
%setup -q
cp %{SOURCE1001} .

%build
%cmake .

%install
%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/se/lib*.so
%license LICENSE.APLv2
