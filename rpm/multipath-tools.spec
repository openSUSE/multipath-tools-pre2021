#
# spec file for package multipath-tools (Version 0.4.8)
#
# Copyright (c) 2009 SUSE LINUX Products GmbH, Nuernberg, Germany.
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

# norootforbuild


Name:           multipath-tools
BuildRequires:  device-mapper-devel libaio-devel readline-devel
Url:            http://christophe.varoqui.free.fr/
License:        BSD 3-clause (or similar) ; GPL v2 or later ; LGPL v2.1 or later ;  Public Domain, Freeware ; MIT License (or similar)
Group:          System/Base
Requires:       device-mapper kpartx
PreReq:         %insserv_prereq %fillup_prereq coreutils grep
AutoReqProv:    on
Version:        0.4.8
Release:        41
Summary:        Tools to Manage Multipathed Devices with the device-mapper
Source:         multipath-tools-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Patch0:         %{name}-%{version}-sles11-sp1.diff.bz2

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
License:        BSD 3-clause (or similar) ; GPL v2 or later ; LGPL v2.1 or later ;  Public Domain, Freeware ; MIT License (or similar)
Summary:        Manages partition tables on device-mapper devices
Group:          System/Base
Requires:       device-mapper

%description -n kpartx
The kpartx program maps linear devmaps to device partitions, which
makes multipath maps partionable.



Authors:
--------
    Christophe Varoqui <christophe.varoqui@free.fr>

%prep
%setup -q -n multipath-tools-%{version}
%patch0 -p1

%build
# This package failed when testing with -Wl,-as-needed being default.
# So we disable it here, if you want to retest, just delete this comment and the line below.
export SUSE_ASNEEDED=0
make OPTFLAGS="$RPM_OPT_FLAGS" LIB=%_lib

%install
make DESTDIR=$RPM_BUILD_ROOT LIB=%_lib install
mkdir -p $RPM_BUILD_ROOT/var/cache/multipath/
ln -sf ../etc/init.d/multipathd $RPM_BUILD_ROOT/sbin/rcmultipathd

%clean
[ "$RPM_BUILD_ROOT" != / ] && [ -d $RPM_BUILD_ROOT ] && rm -rf $RPM_BUILD_ROOT;

%pre
[ -f /.buildenv ] && exit 0
if [ -f /etc/init.d/multipathd ] && dmsetup --target multipath table | grep -q multipath ; then
    /etc/init.d/multipathd stop
fi

%post
[ -f /.buildenv ] && exit 0
if dmsetup --target multipath table | grep -q multipath ; then
    /etc/init.d/multipathd start
fi
[ -x /sbin/mkinitrd_setup ] && mkinitrd_setup
exit 0
#{insserv /etc/init.d/multipathd}
#{fillup_and_insserv boot.multipath}

%preun
%stop_on_removal multipathd

%postun
[ -x /sbin/mkinitrd_setup ] && mkinitrd_setup
%{insserv_cleanup}

%files
%defattr(-,root,root)
%doc AUTHOR COPYING README ChangeLog 
%doc multipath.conf*
%dir /etc/udev
%dir /etc/udev/rules.d
%config /etc/init.d/multipathd
%config /etc/init.d/boot.multipath
%config /etc/udev/rules.d/71-multipath.rules
/%{_lib}/libmultipath.so.0
/%{_lib}/multipath
/sbin/multipath
/sbin/multipathd
/sbin/rcmultipathd
%attr (0700, root, root) /var/cache/multipath
%dir /lib/mkinitrd
%dir /lib/mkinitrd/scripts
/lib/mkinitrd/scripts/boot-multipath.sh
/lib/mkinitrd/scripts/setup-multipath.sh
%{_mandir}/man8/multipath.8*
%{_mandir}/man5/multipath.conf.5*
%{_mandir}/man8/multipathd.8*

%files -n kpartx
%defattr(-,root,root)
%dir /etc/udev
%dir /etc/udev/rules.d
%config /etc/udev/rules.d/70-kpartx.rules
/sbin/kpartx
/sbin/activate_dm_linear
%dir /lib/udev
/lib/udev/kpartx_id
%dir /lib/mkinitrd
%dir /lib/mkinitrd/scripts
/lib/mkinitrd/scripts/boot-kpartx.sh
/lib/mkinitrd/scripts/setup-kpartx.sh
%{_mandir}/man8/kpartx.8*

%changelog
