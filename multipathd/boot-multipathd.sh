
#!/bin/bash
#%stage: boot
#%depends: dm
#%programs: /sbin/multipathd
#%if: "$root_mpath"
#%modules: dm-multipath dm-round-robin dm-queue-length dm-least-pending dm-service-time scsi-dh-emc scsi-dh-hp_sw scsi-dh-rdac scsi-dh-alua

load_modules

mpath_status=$(get_param multipath)

if [ "$mpath_status" != "off" ] ; then
    echo "Starting multipathd"
    /sbin/multipathd -v 3 -B
fi
