#!/bin/bash
#%stage: block
#%depends: dm
#%provides: dmroot
#%programs: /sbin/multipath /@LIB@/multipath/*
#%if: "$root_mpath"
#%modules: dm-multipath dm-round-robin scsi-dh-emc scsi-dh-hp_sw scsi-dh-rdac scsi-dh-alua
#
##### Multipath
##
## If the root device can be accessed using multiple device paths, 
## this initializes and waits for them
##
## Command line parameters
## -----------------------
##
## root_mpath=1		use multipath
## mpath_status=off	do not use multipath
## 

load_modules

# Better wait for all devices to settle
wait_for_events

# check for multipath parameter in /proc/cmdline
mpath_status=$(get_param multipath)

mpath_list=$(sed -n '/multipath/p' /proc/modules)
if [ -z "$mpath_list" ] ; then
  mpath_status=off
fi
if [ "$mpath_status" != "off" ] ; then
  # We are waiting for a device-mapper device
  root_major=$(sed -n 's/\(.*\) device-mapper/\1/p' /proc/devices)
  # Rescan for multipath
  echo -n "Setup multipath devices: "
  /sbin/multipath -v0 -B
  wait_for_events
  echo 'ok.'
fi

