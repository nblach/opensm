%define RELEASE 0.1.20070531git
%define rel %{?CUSTOM_RELEASE} %{!?CUSTOM_RELEASE:%RELEASE}
%if %{?_with_console_socket:1}%{!?_with_console_socket:0}
%define _enable_console_socket --enable-console-socket
%endif
%if %{?_without_console_socket:1}%{!?_without_console_socket:0}
%define _disable_console_socket --disable-console-socket
%endif

%if %{?_with_perf_mgr:1}%{!?_with_perf_mgr:0}
%define _enable_perf_mgr --enable-perf-mgr
%endif
%if %{?_without_perf_mgr:1}%{!?_without_perf_mgr:0}
%define _disable_perf_mgr --disable-perf-mgr
%endif

Summary: InfiniBand subnet manager and administration
Name: opensm
Version: 3.1.0
Release: %rel%{?dist}
License: GPL/BSD
Group: System Environment/Daemons
URL: http://openfabrics.org/
Source: git://git.openfabrics.org/~halr/management/opensm-git.tgz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: libibumad-devel, autoconf, libtool, automake
Requires: %{name}-libs = %{version}-%{release}, logrotate
Requires(post): /sbin/service, /sbin/chkconfig
Requires(preun): /sbin/chkconfig, /sbin/service

%description
OpenSM provides an implementation of an InfiniBand Subnet Manager and
Administration. Such a software entity is required to run for in order
to initialize the InfiniBand hardware (at least one per each
InfiniBand subnet).

%package libs
Summary: Libraries from the opensm package
Group: System Environment/Libraries
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description libs
Shared libraries that are part of the opensm package but are also used by
other applications.  If you don't need opensm itself installed, these
libraries can be installed to satisfy dependencies of other applications.

%package devel
Summary: Development files for OpenSM
Group: System Environment/Libraries
Requires: %{name}-libs = %{version}-%{release} libibumad-devel

%description devel
Header files for OpenSM.

%package static
Summary: Static version of the opensm libraries
Group: System Environment/Libraries
Requires: %{name}-libs = %{version}-%{release} libibumad-devel

%description static
Static version of the opensm libraries

%prep
%setup -q

%build
./autogen.sh
%configure \
        %{?_enable_console_socket} \
        %{?_disable_console_socket} \
        %{?_enable_perf_mgr} \
        %{?_disable_perf_mgr}
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install
rm -f $RPM_BUILD_ROOT%{_libdir}/*.la
etc=$RPM_BUILD_ROOT%{_sysconfdir}
mkdir -p ${RPM_BUILD_ROOT}/var/cache/opensm
if [ -f /etc/redhat-release -o -s /etc/redhat-release ]; then
    REDHAT="redhat-"
else
    REDHAT=""
fi
mkdir -p $etc/{init.d,ofa,logrotate.d}
install -m 755 scripts/${REDHAT}opensm.init $etc/init.d/opensm
install -m 644 scripts/${REDHAT}opensm.conf $etc/ofa/opensm.conf
install -m 644 scripts/opensm.logrotate $etc/logrotate.d/opensm

%clean
rm -rf $RPM_BUILD_ROOT

%post
if [ $1 = 1 ]; then
    /sbin/chkconfig --add opensm
else
    /sbin/service opensm condrestart
fi

%preun
if [ $1 = 0 ]; then
    /sbin/service opensm stop
    /sbin/chkconfig --delete opensm
    rm -f /var/cache/opensm/*
fi

%post libs -p /sbin/ldconfig
%postun libs -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_sbindir}/opensm
%{_sbindir}/osmtest
%{_mandir}/man8/*
%doc AUTHORS COPYING README
%{_sysconfdir}/init.d/opensm
%config(noreplace) %{_sysconfdir}/ofa/opensm.conf
%config(noreplace) %{_sysconfdir}/logrotate.d/opensm
%dir /var/cache/opensm

%files libs
%defattr(-,root,root,-)
%{_libdir}/*.so.*

%files devel
%defattr(-,root,root,-)
%{_includedir}/infiniband/*
%{_libdir}/*.so

%files static
%defattr(-,root,root,-)
%{_libdir}/*.a

