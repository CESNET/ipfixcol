Summary: Fastbit storage plugin for ipfixcol.
Name: ipfixcol_fastbit_output
Version: %(cut -f1 ./VERSION | tr -d '\n')
Release: 1
URL: http://www.liberouter.org/
Source: http://homeproj.cesnet.cz/rpm/liberouter/stable/SOURCES/%{name}-%{version}-%{release}.tar.gz
Group: Liberouter
License: BSD
Vendor: CESNET, z.s.p.o.
Packager: Petr Kramolis <kramolis@cesnet.cz>
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}

BuildRequires: gcc make doxygen pkgconfig
Requires: fastbit-liberouter
BuildRequires: fastbit-liberouter-devel
Requires: ipfixcol
BuildRequires: libcommlbr-devel >= 2.0.0 , libcommlbr-devel < 3.0.0
Requires: libcommlbr >= 2.0.0 , libcommlbr < 3.0.0
BuildRequires: libcombo-devel >= 1.4.0 , libcombo-devel < 2.0.0

%description
Fastbit storage plugin for ipfixcol.


%prep
%setup

%post

%preun

%postun


%build
./configure --with-distro=suse --prefix=%{_prefix} ;
make

%install
make DESTDIR=$RPM_BUILD_ROOT install

%files
#storage plugins
/usr/lib64/ipfixcol_fastbit_output.so
%{_mandir}/man1/ipfixcol_fastbit_output.1*
