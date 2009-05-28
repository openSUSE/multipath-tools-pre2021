#
# spec file for package multipath-tools (Version 0.4.7)
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



Name:           multipath-tools
BuildRequires:  device-mapper device-mapper-devel readline-devel sysfsutils
Url:            http://christophe.varoqui.free.fr/
License:        BSD 3-Clause; GPL v2 or later; LGPL v2.1 or later; Public Domain, Freeware; X11/MIT
Group:          System/Base
Requires:       device-mapper
%if %suse_version > 800
PreReq:         %insserv_prereq
%endif
AutoReqProv:    on
Version:        0.4.7
Release:        34.<RELEASE45>
Summary:        Tools to Manage Multipathed Devices with the device-mapper
Source:         multipath-tools-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Patch0:         %{name}-%{version}-sles10-sp3.diff.bz2

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

%{name} provides the tools to manage multipathed devices by

%prep
%setup -n multipath-tools-%{version}
%patch0 -p1

%build
make OPTFLAGS="$RPM_OPT_FLAGS" BUILD=glibc

%install
mkdir -p $RPM_BUILD_ROOT/sbin
mkdir -p $RPM_BUILD_ROOT%{_mandir}/man5
mkdir -p $RPM_BUILD_ROOT%{_mandir}/man8
make DESTDIR=$RPM_BUILD_ROOT install
ln -s mpath_prio_ontap $RPM_BUILD_ROOT/sbin/mpath_prio_netapp
mkdir -p $RPM_BUILD_ROOT/var/cache/multipath/
(cd ${RPM_BUILD_ROOT}/sbin; ln -sf /etc/init.d/multipathd rcmultipathd) 

%clean
[ "$RPM_BUILD_ROOT" != / ] && [ -d $RPM_BUILD_ROOT ] && rm -rf $RPM_BUILD_ROOT;

%pre
if [ -f /etc/init.d/multipathd ] && dmsetup table | grep -q multipath ; then
    /etc/init.d/multipathd stop
fi

%post
if dmsetup table | grep -q multipath ; then
    /etc/init.d/multipathd start
fi
#%{insserv /etc/init.d/multipathd}
#%{fillup_and_insserv boot.multipath}

%postun
%{insserv_cleanup}

%files
%defattr (-,root,root)
%doc AUTHOR COPYING README ChangeLog 
%doc multipath.conf*
%config /etc/init.d/multipathd
%config /etc/init.d/boot.multipath
%config /etc/udev
/sbin/devmap_name
/sbin/multipath
/sbin/kpartx
/sbin/mpath_id
/sbin/kpartx_id
/sbin/multipathd
/sbin/mpath_prio_ontap
/sbin/mpath_prio_balance_units
/sbin/mpath_prio_random
/sbin/mpath_prio_alua
/sbin/mpath_prio_emc
/sbin/mpath_prio_rdac
/sbin/mpath_prio_hds_modular
/sbin/mpath_prio_hp_sw
/sbin/mpath_prio_netapp
/sbin/mpath_prio_pp
/sbin/rcmultipathd
%attr (0700, root, root) /var/cache/multipath
%{_mandir}/man5/multipath.conf.5*
%{_mandir}/man8/devmap_name.8*
%{_mandir}/man8/multipath.8*
%{_mandir}/man8/kpartx.8*
%{_mandir}/man8/multipathd.8*
%{_mandir}/man8/mpath_prio_alua.8*

%changelog
* Fri Apr 03 2009 - ro@suse.de
- buildfix: add missing defattr line in filelist
* Fri Mar 13 2009 - hare@suse.de
- Invalid callout formatting for cciss (bnc#419123)
- 'no_partitons' feature doesn't work with aliases (bnc#465009)
* Wed Mar 11 2009 - hare@suse.de
- Check for NULL argument in vector_foreach_slot (bnc#479572)
* Wed Mar 04 2009 - hare@suse.de
- Backport SLES11 fixes (bnc#446580):
  * Error checking in VECTOR_XXX defines (bnc#469269)
  * Correct definition of dbg_malloc()
  * Double free on path release
  * Use noflush for kpartx (bnc#473352)
  * multipathd dies immediately after start (bnc#473029)
  * Fixup multibus zero-path handling (bnc#476330)
  * Use lists for uevent processing (bnc#478874)
  * Set stacksize of uevent handling thread (bnc#478874)
  * Fix multipathd signal deadlock
  * Stack overflow in uev_trigger (bnc#476540)
* Mon Feb 23 2009 - hare@suse.de
- Select features keyword from multipaths (bnc#465009)
- Remove stray 'umask' call (bnc#458598)
* Fri Nov 07 2008 - hare@suse.de
- use pthread_join during shutdown (bnc#383138)
- Handle arguments for multipathd from init file
- Handle device argument to multipath correctly (bnc#415381)
* Tue Nov 04 2008 - hare@suse.de
- Implement 'bindings_file' option (bnc#393917)
- Valgrind fixes
- Clean up ev_remove_path() (bnc#394996)
- Allow zero paths for multipath tables
- Implement 'no_partitions' feature (bnc#402922)
- Priority inversion in rdac priority checker (bnc#401345)
- Update hwtable for old Compaq/HP arrays (bnc#370214)
- Check return value for get_inq (bnc#419123)
- Read verbosity from configuration file (bnc#388284)
- kpartx -l doesn't remove its loop device (bnc#417266)
- Unable to blacklist S/390 FBA DASDs (bnc#419770)
- Add IBM DS3400 to hardware table
- Don't strip binaries on install
- Missing fixes for libprio ontap.c (bnc#426975)
- Fixup %%n to handle '!' special case (bnc#419123)
- Use pthread_join() in shutdown (bnc#437245)
- Compile fix for select_prio()
- Don't call mpvec garbage collector for emtpy paths (bnc#437245)
* Fri Apr 18 2008 - hare@suse.de
- Add hardware table for IBM DS4800 (bnc#378094)
- Adjust priority to handle preferred path (bnc#368214)
* Mon Apr 07 2008 - hare@suse.de
- Increase priority for RDAC machines (bnc#368214)
* Mon Mar 31 2008 - hare@suse.de
- libdevmapper displays garbage on shutdown (bnc#375383)
- Update HP hardware table definitions (bnc#370214)
- Device mapper name confusion (bnc#297495)
- Add rcmultipathd symlink (bnc#371792)
* Thu Mar 20 2008 - hare@suse.de
- Don't display error messages from callouts (bnc#358551)
- Initialize static path priority correctly (bnc#359829)
- Fix user_friendly_names multipath naming (bnc#364021)
- Sanitize regular expression for HP arrays
* Mon Mar 17 2008 - hare@suse.de
- Update multipath.conf manpage (bnc#368119)
- Update HP hardware table entries (bnc#370214)
- Retry sg_read and tur checker on UNIT ATTENTION
  (bnc#343019,bnc#329922)
* Tue Mar 11 2008 - hare@suse.de
- Document pg_timeout behaviour (bnc#288677)
* Mon Mar 03 2008 - hare@suse.de
- Sync with git tree; move all files into patches
- Initialize hardware table correctly (bnc#364858)
- Rename netapp prio_name priority checker to ontap (#361459)
- Add static priority callout (bnc#359829)
* Wed Feb 27 2008 - hare@suse.de
- Add udev rule to pass events to multipathd (bnc#365305)
- Fixup varargs usage (bnc#361469)
- Don't call kpartx on shutdown (bnc#350930)
* Wed Feb 06 2008 - hare@suse.de
- Wait for all device to appear before calling kpartx (bnc#358849)
* Fri Feb 01 2008 - hare@suse.de
- Backport changes from upstream to support the new
  libprio framework (bnc#340849)
* Mon Jan 14 2008 - hare@suse.de
- Add SUN/LSI 2540 hardware entry (#348903)
- Use renamed 'rdac' priority callout
- Sort device-mapper names to avoid mixup (#297495)
- Use 'mpatha' for user_friendly_names (#262487)
* Mon Nov 26 2007 - hare@suse.de
- Update filelist to include new NetApp priotizer
* Mon Nov 19 2007 - hare@suse.de
- Rename NetApp priotizer (#309559)
- Add cciss support (FATE#302613)
- Add new hw entry for SUN CSM200 (#292722)
- Add new hw entry for IBM DS4700
- Backport git changes
- Fix crash on shutdown
- Add option 'status' to boot.multipath (#291384)
* Tue Jul 03 2007 - hare@suse.de
- Undo PATH_GHOST handling changes (#276513)
- Add IBM FAStT 1722-600 hardware entry (#282755)
* Thu May 03 2007 - hare@suse.de
- Update udev rules to check for accessability of a
  device-mapper device (#241945)
- Create dummy device-mapper devices for extended partitions
  (#241945)
* Wed Apr 25 2007 - hare@suse.de
- Fix typo in blacklist_exceptions handling (#267892)
- Update udev rules and mpath_id to handle persistent
  symlinks on DASD correctly (#267923)
- Increase logging level to not annoy users with
  'remove multipath map' messages.
* Mon Apr 23 2007 - hare@suse.de
- Correct parameter for DASD uid callout (#245342)
* Fri Apr 20 2007 - hare@suse.de
- Select correct rdac options for FASTt (#265834)
- Add missing fixes for RDAC support (#256358)
- Ignore map offline events (#266203)
* Thu Mar 29 2007 - hare@suse.de
- Update multipath.conf.5 manpage
- Fix booting from multipath devices (#241945)
* Fri Mar 23 2007 - hare@suse.de
- Display '-t' correctly (#256118)
- Use 'queue_if_no_path' for hp_sw (#256577)
- Add rdac path checker (#256358)
* Fri Mar 16 2007 - hare@suse.de
- Add 'mpath_prio_hp_sw' priority checker to group
  HP Active/Standby paths correctly (#249761).
* Thu Mar 15 2007 - hare@suse.de
- Use async I/O for directio checker to avoid hangs (#254082)
- Use 'queue_if_no_path' for DASDs (#254116)
- Fix overflow in circular log buffer (#245886)
- Block SIGPIPE before writing to a PIPE (#249746)
* Mon Mar 05 2007 - hare@suse.de
- use dasdinfo instead of dasd_id for S/390 DASDs (#245342)
* Tue Feb 27 2007 - hare@suse.de
- Race condition between multipath and device-mapper (#247668)
- Fix ALUA handling with new EMC Clariion firmware (#246900)
* Thu Feb 22 2007 - hare@suse.de
- Fix queue_if_no_path feature for RDAC machines (#224480)
- Include hwtable entry for SGI Infinite Storage
- Add option '-t' to dump internal hardware table (#224480)
* Thu Feb 08 2007 - hare@suse.de
- kpartx crashes during boot (#243520)
* Wed Jan 31 2007 - hare@suse.de
- Re-add patch for garbled output; got lost during the update
  (#238543)
* Mon Jan 15 2007 - hare@suse.de
- Update to official version 0.4.7
- Rediff patches
- Include multipath.conf manpage
- Fixup cross-references in manpages (#223131)
* Thu Jan 11 2007 - hare@suse.de
- Add %%pre and %%post scripts to stop multipathd during update
  (#229950)
- Update hwtable for HP storage arrays (#220749).
- Fixup hwtable entry for IBM DS8000 (#220488).
* Wed Nov 08 2006 - hare@suse.de
- Do not reference the 'hp_sw' hardware handler (#202225)
- Fixup name for Hitachi HDS prioritizer (#191535)
- Garbled multipath output upon renaming (#184166)
- Do not print the same info multiple times when called with '-d'
- Fixup EMC checker handling for LUNZ
- Support high minor numbers for ALUA prioritizer (#208264)
- Fix segmentation fault on incomplete config file
- Increase timeout for checker commands
- multipath tables are not created when using and rr_weight
  setting of 'priorities' (#203823)
- boot.multipath start deletes path if invoked twice (#192619)
* Wed Jun 28 2006 - hare@suse.de
- Remove blacklisting of dasd device node,
  use product_blacklist instead (#188688)
* Mon Jun 12 2006 - hare@suse.de
- Add 72-multipath-compat.rules to create 'by-name'
  symlink again for compability (#183663)
* Fri Jun 02 2006 - hare@suse.de
- Merge in fixed from upstream
  - Set device-mapper name correctly (#181127)
* Thu Jun 01 2006 - hare@suse.de
- Merge in fixes from upstream
  - Fixup aliasing handling
  - Fix string length in pp_alua
- Use correct regexp patterns for hwtable (#177842)
- Fixup 71-multipath.rules to work correctly with the
  updated device-mapper online/offline events (#176516 - LTC23961)
- Add kpartx_id for the updated 71-multipath.rules
* Tue May 02 2006 - hare@suse.de
- Merge in fixes from upstream
  - Merged local patches
  - Added hds_modular prioritizer
- Remove merged patches
- Allow for setting of maximum number of open files (#149979)
- Implement 'stop' for init scripts
* Mon Apr 10 2006 - hare@suse.de
- Lowering priority for pp_tpc
- Split off DS6000 to fixup priority handler (#161347)
* Wed Apr 05 2006 - hare@suse.de
- Disable debug messages in pp_tpc.
* Wed Mar 29 2006 - hare@suse.de
- Explicitely create partitions at boot time (#159927)
* Thu Mar 23 2006 - hare@suse.de
- Add hwtable entry for IBM 3526.
* Tue Mar 14 2006 - hare@suse.de
- Fix another typo in mpath_id.
* Tue Mar 14 2006 - hare@suse.de
- Fix typo in mpath_id.
* Mon Mar 13 2006 - hare@suse.de
- Fix autobuild warnings.
- Include some minor fixed from upstream.
* Thu Mar 09 2006 - hare@suse.de
- Add mpath_id program to call kpartx only on multipathed
  devices (#149995 - LTC21557).
- Include latest fixes from upstream.
* Wed Feb 08 2006 - hare@suse.de
- Add device blacklisting (#85778)
- Further manpage installation fixes (#146179, #147053, #147911)
* Wed Jan 25 2006 - mls@suse.de
- converted neededforbuild to BuildRequires
* Thu Jan 19 2006 - hare@suse.de
- Fix manpage installation.
* Tue Jan 17 2006 - hare@suse.de
- Include latest git fixes
- Remove old patches which are now upstream
- Add new hwtable entry for shark (#142176)
* Fri Dec 16 2005 - hare@suse.de
- Fix dependencies for /etc/init.d/boot.multipath
- Fix kpartx rules to generate proper partition names.
* Wed Dec 07 2005 - hare@suse.de
- Update to multipath-tools-0.4.6
- Include latest git fixes
- Port patches from SLES9 SP3
* Mon Oct 31 2005 - dmueller@suse.de
- don't build as root
* Mon Sep 05 2005 - matz@suse.de
- Fix broken usage of self-defined syscall [#114933].
* Fri Aug 26 2005 - hare@suse.de
- Fix kpartx DASD partition support
* Thu Aug 04 2005 - hare@suse.de
- Add 'directio' path checker
- Add support for S/390 DASD (PAV enablement).
- Update to package from SLES9
* Thu Jun 23 2005 - lmb@suse.de
- LUs with a WWN containing "fd" were blacklisted (#93562).
* Thu Jun 16 2005 - lmb@suse.de
- Remove stray newline character from /dev/disk/by-name/ entries
  (#85798, #86763)
- Clear /dev/disk/by-name/ on boot. (#85978)
- scsi_id now handles EMC Symmetrix; remove work-around for #86760.
* Wed Jun 15 2005 - meissner@suse.de
- use RPM_OPT_FLAGS.
* Mon Jun 13 2005 - ro@suse.de
- neededforbuild: udev -> sysfsutils
* Tue Jun 07 2005 - lmb@suse.de
- Import fixes from upstream.
- Hardware table updates for IBM ESS and EMC CX (#81688).
- Reinstate paths correctly after failure/restore cycle (#85781,
  [#86444]).
- Create map names again and fix segfault in devmap_name (#85798).
* Tue May 24 2005 - hare@suse.de
- Fix segmentation fault with EMC Symmetrix (#85614).
- Update EMC Symmetrix entry in hwtable.
* Mon May 23 2005 - hare@suse.de
- Add hwtable entry for IBM DS6000. (#63903)
- Do a rescan for devices if multipath command line option is set.
* Fri May 20 2005 - hare@suse.de
- Fix devmap_name to use mapname and return proper status (#84748).
* Thu May 12 2005 - lmb@suse.de
- Don't complain about default prio callout command (#81695).
- Reflect recent changes in boot.multipath as well as multipathd init
  scripts.
- Actually fail paths when they are detected to be failed by multipathd
  (#81679).
- killproc/startproc/checkproc can't be used with multipathd because of
  the way the daemon switches to its own namespace (#80443).
* Mon May 09 2005 - hare@suse.de
- Use proper path checker for SGI TPC arrays.
- Update hwtable entries for SGI TP9400 and SGI TP9500.
- Write correct PID file (#80443).
* Mon Apr 25 2005 - lmb@suse.de
- Update to 0.4.4: pp_alua now licensed as GPL (#78628).
- multipath-tools-oom-adj.patch: oom_adj to a valid value.
* Thu Apr 21 2005 - lmb@suse.de
- Update to 0.4.4-pre18 which fixes the multipathd to initialize
  correctly in the absence of a configuration file (79239).
* Wed Apr 20 2005 - lmb@suse.de
- Put multipath cache back into /dev because /var might not be mounted.
- Correct hwtable entry SGI TP9400, TP9500 and IBM 3542.
* Wed Apr 20 2005 - lmb@suse.de
- Update to 0.4.4-pre16
- Build against device-mapper.1.01.xx correctly.
* Tue Apr 19 2005 - lmb@suse.de
- Build w/o device-mapper update again.
* Mon Apr 18 2005 - lmb@suse.de
- Update to 0.4.4-pre14
- Build versus device-mapper-1.01.01 to prevent deadlocks in
  kernel-space.
- Fix devmap_name to work with udev.
- Fix startup of multipathd w/o configuration file present.
* Fri Apr 15 2005 - lmb@suse.de
- Add path priority checker for EMC CLARiiON and make necessary
  adjustments so that it gets called by default (#62491).
- Set the default udev dir to '/dev'
* Fri Apr 15 2005 - hare@suse.de
- Fix to allocate default strings (#78056)
- Fix default entry for TPC9500.
* Wed Apr 13 2005 - hare@suse.de
- Added pp_alua path priority checker.
- Update to multipath-tools-0.4.4-pre12.
* Mon Apr 11 2005 - hare@suse.de
- Update to multipath-tools-0.4.4-pre10.
* Fri Apr 08 2005 - hare@suse.de
- Update multipath to handle only true multipath devices (#62491).
- Update kpartx to use the device mapper target name if available.
- Add boot.multipath script for early set up of multipath targets.
* Thu Mar 31 2005 - hare@suse.de
- Update devmap_name to select targets by table type (#62493).
* Tue Jan 25 2005 - lmb@suse.de
- Update to 0.4.2 and fix some bugs + add support for the extended DM
  multipath kernel module. (#47491)
* Thu Nov 11 2004 - hare@suse.de
- Fix bugs to make it work on S/390 (#47491).
* Fri Nov 05 2004 - hare@suse.de
- Update to version 0.3.6 (#47491).
- Fix multipath init script
- Install configuration file example.
- Install multipathd in /sbin instead of /usr/bin.
* Tue Jul 20 2004 - fehr@suse.de
- updated README mp-tools-issues.pdf (see #40640)
* Wed Jun 09 2004 - fehr@suse.de
- added pdf with README to package (see #40640)
* Thu Jun 03 2004 - fehr@suse.de
- updated to version 0.2.1
- removed patches zero-currpath.patch and rm-newline-in-name.patch
  already contained in 0.2.1
* Thu Jun 03 2004 - fehr@suse.de
- added patch zero-currpath.patch (see bugzilla #40640)
* Wed May 26 2004 - uli@suse.de
- fixed to build on s390x
* Wed May 26 2004 - fehr@suse.de
- added patch rm-newline-in-name.patch (see bugzilla #40640)
* Tue May 25 2004 - fehr@suse.de
- created initial version of a SuSE package from version 0.2.0 of
  multipath tools
