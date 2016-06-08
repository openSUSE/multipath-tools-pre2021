#
# spec file for package multipath-tools
#
# Copyright (c) 2014 SUSE LINUX Products GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#


Name:           multipath-tools
BuildRequires:  device-mapper-devel
BuildRequires:  libaio-devel
BuildRequires:  libudev-devel
BuildRequires:  readline-devel
BuildRequires:  pkgconfig(systemd)
BuildRequires:  pkgconfig(libsystemd)
BuildRequires:  udev
Url:            http://christophe.varoqui.free.fr/
Requires:       device-mapper >= 1.2.78
Requires:       kpartx
PreReq:         %insserv_prereq %fillup_prereq coreutils grep
Version:        0.5.0
Release:        0
Summary:        Tools to Manage Multipathed Devices with the device-mapper
License:        GPL-2.0
Group:          System/Base
%{?systemd_requires}
%define has_systemd 1
Source:         http://christophe.varoqui.free.fr/multipath-tools/multipath-tools-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Patch0:         %{name}-%{version}-sles12-sp1.diff.bz2
%if %suse_version > 1220
%define         _sysdir usr/lib
%else
%define         _sysdir lib
%endif

%description
This package provides the tools to manage multipathed devices by
instructing the device-mapper multipath module what to do. The tools
are:

- multipath: scans the system for multipathed devices, assembles
   them, and updates the device-mapper's maps

- multipathd: waits for maps events then execs multipath

- devmap-name: provides a meaningful device name to udev for devmaps

- kpartx: maps linear devmaps to device partitions, which makes
multipath maps partionable



Authors:
--------
    Christophe Varoqui <christophe.varoqui@free.fr>

%package -n kpartx
Summary:        Manages partition tables on device-mapper devices
Group:          System/Base
Requires:       device-mapper

%description -n kpartx
The kpartx program maps linear devmaps to device partitions, which
makes multipath maps partionable.



Authors:
--------
    Christophe Varoqui <christophe.varoqui@free.fr>

%package devel
Summary:        Development libraries for multipath-tools
Group:          Development/Libraries/Other
Requires:       device-mapper

%description devel
This package contains the development libraries for multipath-tools
and libmpath_persist.


Authors:
--------
    Christophe Varoqui <christophe.varoqui@free.fr>

%prep
%setup -q -n multipath-tools-%{version}
%patch0 -p1

%build
make CC="%__cc" OPTFLAGS="$RPM_OPT_FLAGS" LIB=%{_lib} SYSTEMDPATH=%{_sysdir}

%install
make DESTDIR=$RPM_BUILD_ROOT LIB=%{_lib} SYSTEMDPATH=%{_sysdir} install
mkdir -p $RPM_BUILD_ROOT/var/cache/multipath/
rm $RPM_BUILD_ROOT/%_lib/libmpathpersist.so
mkdir -p $RPM_BUILD_ROOT/usr/sbin
ln -sf /sbin/service $RPM_BUILD_ROOT/usr/sbin/rcmultipathd

%clean
rm -rf $RPM_BUILD_ROOT;

%pre
[ -f /.buildenv ] && exit 0
if [ -f /etc/init.d/multipathd ] && dmsetup --target multipath table | grep -q multipath ; then
  /etc/init.d/multipathd stop
  %service_add_pre multipathd.socket
  %service_add_pre multipathd.service
fi

%post
[ -f /.buildenv ] && exit 0
%{run_ldconfig}
if dmsetup --target multipath table | grep -q multipath ; then
  %service_add_post multipathd.socket
  %service_add_post multipathd.service
fi
%{?regenerate_initrd_post}
exit 0

%preun
%service_del_preun multipathd.service
%service_del_preun multipathd.socket

%postun
%{?regenerate_initrd_post}
%service_del_postun multipathd.service
%service_del_postun multipathd.socket
%{run_ldconfig}

%posttrans
%{?regenerate_initrd_posttrans}

%files
%defattr(-,root,root)
%doc AUTHOR COPYING README ChangeLog
%doc multipath.conf*
%{_udevrulesdir}/11-dm-mpath.rules
%{_udevrulesdir}/56-multipath.rules
/%{_lib}/libmultipath.so.0
/%{_lib}/libmpathpersist.so.0
/%{_lib}/multipath
/sbin/multipath
/sbin/multipathd
/sbin/mpathpersist
/usr/sbin/rcmultipathd
%attr (0700, root, root) /var/cache/multipath
%dir /%{_sysdir}/systemd/system
/%{_sysdir}/systemd/system/multipathd.service
/%{_sysdir}/systemd/system/multipathd.socket
%{_mandir}/man8/multipath.8*
%{_mandir}/man5/multipath.conf.5*
%{_mandir}/man8/multipathd.8*
%{_mandir}/man8/mpathpersist.8*

%files devel
%defattr(-,root,root)
/usr/include/mpath_persist.h
%{_mandir}/man3/mpath_persistent_*

%files -n kpartx
%defattr(-,root,root)
/sbin/kpartx
%{_udevrulesdir}/66-kpartx.rules
%{_udevrulesdir}/67-kpartx-compat.rules
/%{_sysdir}/udev/kpartx_id
%{_mandir}/man8/kpartx.8*

%changelog
