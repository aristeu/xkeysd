# this is for RHEL6 machines so it'll generate compatible (S)RPMs
%define _source_filedigest_algorithm 0
%define _binary_filedigest_algorithm 0

Name:		xkeysd
Version:	1.0
Release:	1
Summary:	Userspace mapper for XKeys Jog and Shuttle Pro

Group:		User Interface/X Hardware Support
License:	GPL
URL:		https://github.com/aristeu/xkeysd
Source0:	%{name}-%{version}.tar.bz2

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}
BuildRequires:	glibc-devel, libconfig-devel
Requires:	udev >= 030-21, util-linux, libconfig

%description
Userspace daemon to map XKeys Jog and Shuttle Pro keys and dials to events and
macros

%prep
%setup -q

%build
export CFLAGS="$RPM_OPT_FLAGS"
make UINPUT_FILE="/dev/uinput"

%install
rm -rf $RPM_BUILD_ROOT

make DESTDIR=$RPM_BUILD_ROOT SBINDIR=usr/sbin install;
mkdir -p $RPM_BUILD_ROOT/etc/rc.d/init.d/
cp xkeysd.initrd $RPM_BUILD_ROOT/etc/rc.d/init.d/xkeysd

%clean
rm -rf $RPM_BUILD_ROOT

%post
if [ $1 = 1 ]; then
	/sbin/chkconfig --add %{name}
fi

%preun
if [ $1 = 0 ]; then
	/sbin/service %{name} stop > /dev/null 2>&1
	/sbin/chkconfig --del %{name}
fi

%postun
if [ $1 -ge 1 ]; then
	/sbin/service %{name} condrestart >/dev/null 2>&1
fi

%files
%defattr(-,root,root,-)
%doc AUTHORS LICENSE sample.conf
%{_sbindir}/xkeysd
%{_sysconfdir}/rc.d/init.d/xkeysd

%changelog
* Tue Sep 27 2011 Aristeu Rozanski <aris@redhat.com> 1.0-1
- version 1.0 packaged

