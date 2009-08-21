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
PreReq:         %insserv_prereq %fillup_prereq coreutils grep diffutils
AutoReqProv:    on
Version:        0.4.8
Release:        41
Summary:        Tools to Manage Multipathed Devices with the device-mapper
Source:         multipath-tools-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Patch0:         %{name}-opensuse-11.2.diff.bz2

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
%setup -n multipath-tools-%{version}
%patch0 -p1

%build
# This package failed when testing with -Wl,-as-needed being default.
# So we disable it here, if you want to retest, just delete this comment and the line below.
export SUSE_ASNEEDED=0
make OPTFLAGS="$RPM_OPT_FLAGS"

%install
make DESTDIR=$RPM_BUILD_ROOT install
mkdir -p $RPM_BUILD_ROOT/var/cache/multipath/

%clean
[ "$RPM_BUILD_ROOT" != / ] && [ -d $RPM_BUILD_ROOT ] && rm -rf $RPM_BUILD_ROOT;

%post
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
/lib/multipath
/sbin/multipath
/sbin/multipathd
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
* Fri Jun 19 2009 coolo@novell.com
- disable as-needed for this package as it fails to build with it
* Mon Jan 26 2009 hare@suse.de
- Don't access blocked devices in checkerloop()
- Check for validity in VECTOR_XXX defines (bnc#469269,bnc#457465)
* Fri Jan 23 2009 hare@suse.de
- Update dev_loss_tmo handling (bnc#458393)
- Fix directio error messages
- Update sdev_state handling
* Mon Jan 19 2009 hare@suse.de
- Add new HP machines to hardware table (bnc#442133)
- Print device-mapper tables for debugging
- Properly quote variables in mkinitrd scripts (bnc#447966)
- shuffle call to memarea_init
- Add xdr_setsite in init scripts (bnc#459529)
* Tue Jan 13 2009 hare@suse.de
- Do not run checkers or prioritizers on blocked devices
  (bnc#464155)
- Implement dev_loss_tmo and fast_io_fail_tmo (bnc#464155)
- Use default values for dev_loss_tmo and fast_io_fail_tmo
- Display checker and prio name on failure
- Add more debugging output
* Mon Jan 12 2009 hare@suse.de
- Set max_fds setting for multipath (bnc#457443)
- Enhance error messages for checker and prio loading (bnc#456214)
- Fix merge errors for prio_weightedpath (bnc#441007)
- Fix sysfs_attr_get_value (bnc#456747)
- Do not reinitialize prio and checker lists after config file
  has been parsed (bnc#464373)
- Minor cleanups
* Thu Dec 04 2008 hare@suse.de
- Split off IBM ESS hwtable entries (bnc#439763)
- Reload map when device R/O setting changes (bnc#440959)
* Fri Nov 21 2008 hare@suse.de
- Add 'Weighted Paths' prioritizer (bnc#441007)
- Fix crashes in update_multipath
* Thu Nov 20 2008 hare@suse.de
- Valgrind fixes
  * Add missing initialisation
  * Always allocate memory for alias
  * Check return value of basename
- Adapt to new sysfs layout (bnc#435215, bnc#445041)
- Use /sys/dev to speedup reverse lookups
- Rework sysfs device handling (bnc#435215, bnc#438031)
- Search for correct hardware entry during reconfigure (bnc#435688)
- Use local variables for device mapper params
- Allow zero paths for multipath maps
* Mon Nov 17 2008 hare@suse.de
- Update HP hardware table (bnc#442133)
- Zero out lines in print.c (bnc#445023)
* Mon Nov 10 2008 hare@suse.de
- Check for empty mpvecs in mpvec_garbage_collector() (bnc#437245)
- dmraid uuid starts with 'DMRAID' (bnc#439439)
- Handle arguments to multipathd from the init file
* Thu Nov 06 2008 ro@suse.de
- commenting multiline rpm macros is dangerous (read broken)
  remove percent sign in commented macro lines
* Mon Nov 03 2008 hare@suse.de
- Use pthread_join() during shutdown to avoid crash (bnc#437245)
- Fixup '%%n' to handle '!' kernel device name syntax (bnc#435172)
- Use correct commandline for cciss scsi_id callouts (bnc#435172)
- Do not check for valid mp context in get_state() (bnc#433659)
- Link directio checker against libaio (bnc#433659)
- Use regmatch when checking for duplicates in hwtable (bnc#439763)
* Mon Oct 27 2008 hare@suse.de
- Patches from mainline:
  * Increase bindings file lock timeout
  * Fixes for 'show paths format'
  * Add 'show wildcards' cli cmd
  * Add support for IBM storage devices
- fopen() returns NULL on failure (bnc#432598)
- Read verbosity level from configuration file (bnc#388284)
- kpartx -l does not remove it's loop device (bnc#417266)
- Missing fixes for libprio ontap.c (bnc#426975)
- Increase buffer size in find_loop_by_file (bnc#436428)
- Include SCSI device handler modules for initrd (bnc#431877)
* Fri Sep 26 2008 hare@suse.de
- Implement map resize (FATE#302007)
- Handle cciss devices correctly (bnc#419123)
* Wed Sep 24 2008 ro@suse.de
- use udevadm info instead of udevinfo
* Wed Sep 03 2008 hare@suse.de
- Merge in fixes from upstream:
  * Add IBM IPR to hardware table (bnc#419086)
  * Add IBM DS4300 to hardware table
  * Fix settings for scsi_id changes
- Call mkinitrd_setup during %%post and %%postun (bnc#413709)
* Thu Aug 28 2008 ro@suse.de
- fix init scripts
* Wed Jul 23 2008 hare@suse.de
- Update to version 0.4.8
- Include changes from SLES10
- Include mkinitrd scriptlets
* Wed May 28 2008 hare@suse.de
- Calculate correct partition offset in kpartx (bnc#394658)
* Fri May 09 2008 hare@suse.de
- Fixup kpartx rules (bnc#387667)
* Tue Apr 29 2008 hare@suse.de
- Merge in fixes from upstream
- Merge fixes from SLES10 SP2
* Wed Sep 19 2007 hare@suse.de
- Fixup generated dm_linear udev rules (#218122)
* Thu Aug 30 2007 hare@suse.de
- Rework dm_linear; udev rules should only be
  created if the feature was activated (#302422)
* Fri Aug 10 2007 hare@suse.de
- Implement dm_linear (#218122)
* Thu Aug 02 2007 hare@suse.de
- Merge in latest fixes from upstream
- Remove local patches; merge with upstream
- Fix kpartx handling of extended partitions
- Use underscores for partition names (#293792)
* Mon May 21 2007 hare@suse.de
- Rework udev handling
- Split off kpartx package
* Mon May 14 2007 hare@suse.de
- Merge in latest fixes from upstream
- Add all SuSE specific files to git repository.
* Fri May 11 2007 hare@suse.de
- Include latest changes from upstream
- Remove libsysfs (242766)
- Handle extended partitions for kpartx
* Mon Dec 04 2006 dmueller@suse.de
- don't build as root
* Fri Nov 17 2006 hare@suse.de
- integrate upstream fixes
- update udev rule for YaST2 dmraid support (217807])
* Mon Nov 06 2006 hare@suse.de
- Really fixup udev rule (216167).
* Fri Oct 20 2006 ro@suse.de
- make it build
* Mon Sep 25 2006 hare@suse.de
- update to latest fixes from git tree
- remove fixes integrated in upstream
- fixup udev rule (#203688)
* Thu Sep 14 2006 ro@suse.de
- use device-mapper-devel in BuildRequires
* Thu Aug 31 2006 hare@suse.de
- include latest fixes from git tree
- update kpartx_id and udev rules to work
  with dmraid.
- Fix return value for multipath -l
* Thu Aug 17 2006 ro@suse.de
- workaround problem in git-patch
  normal patch can't do a "rename", so copy file first
* Tue Jul 11 2006 hare@suse.de
- Update to official version 0.4.7
- Refactor git update to apply to 0.4.7
* Wed Jun 28 2006 hare@suse.de
- Remove blacklisting of dasd device node,
  use product_blacklist instead (#188688)
* Mon Jun 12 2006 hare@suse.de
- Add 72-multipath-compat.rules to create 'by-name'
  symlink again for compability (#183663)
* Fri Jun 02 2006 hare@suse.de
- Merge in fixed from upstream
  - Set device-mapper name correctly (#181127)
* Thu Jun 01 2006 hare@suse.de
- Merge in fixes from upstream
  - Fixup aliasing handling
  - Fix string length in pp_alua
- Use correct regexp patterns for hwtable (#177842)
- Fixup 71-multipath.rules to work correctly with the
  updated device-mapper online/offline events (#176516 - LTC23961)
- Add kpartx_id for the updated 71-multipath.rules
* Mon May 22 2006 schwab@suse.de
- Don't strip binaries.
* Tue May 02 2006 hare@suse.de
- Merge in fixes from upstream
  - Merged local patches
  - Added hds_modular prioritizer
- Remove merged patches
- Allow for setting of maximum number of open files (#149979)
- Implement 'stop' for init scripts
* Mon Apr 10 2006 hare@suse.de
- Lowering priority for pp_tpc
- Split off DS6000 to fixup priority handler (#161347)
* Wed Apr 05 2006 hare@suse.de
- Disable debug messages in pp_tpc.
* Wed Mar 29 2006 hare@suse.de
- Explicitely create partitions at boot time (#159927)
* Thu Mar 23 2006 hare@suse.de
- Add hwtable entry for IBM 3526.
* Tue Mar 14 2006 hare@suse.de
- Fix another typo in mpath_id.
* Tue Mar 14 2006 hare@suse.de
- Fix typo in mpath_id.
* Mon Mar 13 2006 hare@suse.de
- Fix autobuild warnings.
- Include some minor fixed from upstream.
* Thu Mar 09 2006 hare@suse.de
- Add mpath_id program to call kpartx only on multipathed
  devices (#149995 - LTC21557).
- Include latest fixes from upstream.
* Wed Feb 08 2006 hare@suse.de
- Add device blacklisting (#85778)
- Further manpage installation fixes (#146179, #147053, #147911)
* Wed Jan 25 2006 mls@suse.de
- converted neededforbuild to BuildRequires
* Thu Jan 19 2006 hare@suse.de
- Fix manpage installation.
* Tue Jan 17 2006 hare@suse.de
- Include latest git fixes
- Remove old patches which are now upstream
- Add new hwtable entry for shark (#142176)
* Fri Dec 16 2005 hare@suse.de
- Fix dependencies for /etc/init.d/boot.multipath
- Fix kpartx rules to generate proper partition names.
* Wed Dec 07 2005 hare@suse.de
- Update to multipath-tools-0.4.6
- Include latest git fixes
- Port patches from SLES9 SP3
* Mon Oct 31 2005 dmueller@suse.de
- don't build as root
* Mon Sep 05 2005 matz@suse.de
- Fix broken usage of self-defined syscall [#114933].
* Fri Aug 26 2005 hare@suse.de
- Fix kpartx DASD partition support
* Thu Aug 04 2005 hare@suse.de
- Add 'directio' path checker
- Add support for S/390 DASD (PAV enablement).
- Update to package from SLES9
* Thu Jun 23 2005 lmb@suse.de
- LUs with a WWN containing "fd" were blacklisted (#93562).
* Thu Jun 16 2005 lmb@suse.de
- Remove stray newline character from /dev/disk/by-name/ entries
  (#85798, #86763)
- Clear /dev/disk/by-name/ on boot. (#85978)
- scsi_id now handles EMC Symmetrix; remove work-around for #86760.
* Wed Jun 15 2005 meissner@suse.de
- use RPM_OPT_FLAGS.
* Mon Jun 13 2005 ro@suse.de
- neededforbuild: udev -> sysfsutils
* Tue Jun 07 2005 lmb@suse.de
- Import fixes from upstream.
- Hardware table updates for IBM ESS and EMC CX (#81688).
- Reinstate paths correctly after failure/restore cycle (#85781,
  [#86444]).
- Create map names again and fix segfault in devmap_name (#85798).
* Tue May 24 2005 hare@suse.de
- Fix segmentation fault with EMC Symmetrix (#85614).
- Update EMC Symmetrix entry in hwtable.
* Mon May 23 2005 hare@suse.de
- Add hwtable entry for IBM DS6000. (#63903)
- Do a rescan for devices if multipath command line option is set.
* Fri May 20 2005 hare@suse.de
- Fix devmap_name to use mapname and return proper status (#84748).
* Thu May 12 2005 lmb@suse.de
- Don't complain about default prio callout command (#81695).
- Reflect recent changes in boot.multipath as well as multipathd init
  scripts.
- Actually fail paths when they are detected to be failed by multipathd
  (#81679).
- killproc/startproc/checkproc can't be used with multipathd because of
  the way the daemon switches to its own namespace (#80443).
* Mon May 09 2005 hare@suse.de
- Use proper path checker for SGI TPC arrays.
- Update hwtable entries for SGI TP9400 and SGI TP9500.
- Write correct PID file (#80443).
* Mon Apr 25 2005 lmb@suse.de
- Update to 0.4.4: pp_alua now licensed as GPL (#78628).
- multipath-tools-oom-adj.patch: oom_adj to a valid value.
* Thu Apr 21 2005 lmb@suse.de
- Update to 0.4.4-pre18 which fixes the multipathd to initialize
  correctly in the absence of a configuration file (79239).
* Wed Apr 20 2005 lmb@suse.de
- Put multipath cache back into /dev because /var might not be mounted.
- Correct hwtable entry SGI TP9400, TP9500 and IBM 3542.
* Wed Apr 20 2005 lmb@suse.de
- Update to 0.4.4-pre16
- Build against device-mapper.1.01.xx correctly.
* Tue Apr 19 2005 lmb@suse.de
- Build w/o device-mapper update again.
* Mon Apr 18 2005 lmb@suse.de
- Update to 0.4.4-pre14
- Build versus device-mapper-1.01.01 to prevent deadlocks in
  kernel-space.
- Fix devmap_name to work with udev.
- Fix startup of multipathd w/o configuration file present.
* Fri Apr 15 2005 lmb@suse.de
- Add path priority checker for EMC CLARiiON and make necessary
  adjustments so that it gets called by default (#62491).
- Set the default udev dir to '/dev'
* Fri Apr 15 2005 hare@suse.de
- Fix to allocate default strings (#78056)
- Fix default entry for TPC9500.
* Wed Apr 13 2005 hare@suse.de
- Added pp_alua path priority checker.
- Update to multipath-tools-0.4.4-pre12.
* Mon Apr 11 2005 hare@suse.de
- Update to multipath-tools-0.4.4-pre10.
* Fri Apr 08 2005 hare@suse.de
- Update multipath to handle only true multipath devices (#62491).
- Update kpartx to use the device mapper target name if available.
- Add boot.multipath script for early set up of multipath targets.
* Thu Mar 31 2005 hare@suse.de
- Update devmap_name to select targets by table type (#62493).
* Tue Jan 25 2005 lmb@suse.de
- Update to 0.4.2 and fix some bugs + add support for the extended DM
  multipath kernel module. (#47491)
* Thu Nov 11 2004 hare@suse.de
- Fix bugs to make it work on S/390 (#47491).
* Fri Nov 05 2004 hare@suse.de
- Update to version 0.3.6 (#47491).
- Fix multipath init script
- Install configuration file example.
- Install multipathd in /sbin instead of /usr/bin.
* Tue Jul 20 2004 fehr@suse.de
- updated README mp-tools-issues.pdf (see #40640)
* Wed Jun 09 2004 fehr@suse.de
- added pdf with README to package (see #40640)
* Thu Jun 03 2004 fehr@suse.de
- updated to version 0.2.1
- removed patches zero-currpath.patch and rm-newline-in-name.patch
  already contained in 0.2.1
* Thu Jun 03 2004 fehr@suse.de
- added patch zero-currpath.patch (see bugzilla #40640)
* Wed May 26 2004 uli@suse.de
- fixed to build on s390x
* Wed May 26 2004 fehr@suse.de
- added patch rm-newline-in-name.patch (see bugzilla #40640)
* Tue May 25 2004 fehr@suse.de
- created initial version of a SuSE package from version 0.2.0 of
  multipath tools
