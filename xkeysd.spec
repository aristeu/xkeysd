# this is for RHEL6 machines so it'll generate compatible (S)RPMs
%define _source_filedigest_algorithm 0
%define _binary_filedigest_algorithm 0

Name:		xkeysd
Version:	0.2
Release:	1
Summary:	Remapping helper for XKeys Jog & Shuttle Pro

Group:		User Interface/X Hardware Support
License:	GPL
URL:		http://internal/~aris/xkeysd/
Source0:	http://internal/~aris/xkeysd/xkeysd-%{version}.tar.bz2

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}
BuildRequires:	libconfig-devel
Requires:	libconfig

%description
Helper application allowing the remapping of XKeys Jog & Shuttle Pro keys,
shuttle and jog knob.

%prep
%setup -q

%build
export CFLAGS="$RPM_OPT_FLAGS"
make UINPUT_FILE="/dev/uinput"

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT SBINDIR=%{_sbindir}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_sbindir}/xkeysd
%{_sysconfdir}/xkeysd.conf

%changelog
* Wed Jul 20 2011 Aristeu Rozanski <aris@redhat.com> 0.2-1
- Improved packaging
- Default configuration file is now on /etc/xkeysd.conf

* Tue Jul 19 2011 Aristeu Rozanski <aris@redhat.com> 0.1-1
- First test release

