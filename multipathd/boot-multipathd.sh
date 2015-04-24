#!/bin/bash
#%stage: boot
#%depends: dm scsi_dh
#%programs: /sbin/multipathd
#%if: "$root_mpath"
#%modules: dm-multipath dm-round-robin dm-queue-length dm-least-pending dm-service-time

load_modules

mpath_status=$(get_param multipath)

if [ "$mpath_status" != "off" ] ; then
    echo "Starting multipathd"
    /sbin/multipathd -v 3
fi
