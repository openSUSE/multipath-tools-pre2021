#!/bin/bash
#
#%stage: devicemapper
#%provides: dmroot
#
# force multipath and dm usage if multipath was forced
if use_script multipath; then
    root_mpath=1
    root_dm=1
fi

if [ -x /sbin/multipath -a -x /sbin/dmsetup ] ; then
    for bd in $blockdev ; do
	update_blockdev $bd
	if [ $blockdriver = device-mapper ]; then
	    dm_uuid=$(dmsetup info -c --noheadings -o uuid -j $blockmajor -m $blockminor)
	    dm_creator=${dm_uuid%-*}
	    if [ "$dm_creator" = "mpath" ]; then
		tmp_root_dm=1 # multipath needs dm
		root_mpath=1
	    fi
	fi
    done
fi

if use_script multipath; then
    if [ -f /etc/multipath.conf ] ; then
	cp -a /etc/multipath.conf $tmp_mnt/etc
    fi
    if [ -f /var/lib/multipath/bindings ] ; then
	mkdir -p /var/lib/multipath
	cp -a /var/lib/multipath/bindings $tmp_mnt/var/lib/multipath
    fi
    if [ -e /etc/udev/rules.d/71-multipath.rules ]; then
	cp /etc/udev/rules.d/71-multipath.rules $tmp_mnt/etc/udev/rules.d
    fi
    if [ -e /etc/udev/rules.d/72-multipath-compat.rules ]; then
	cp /etc/udev/rules.d/72-multipath-compat.rules $tmp_mnt/etc/udev/rules.d
    fi
    if [ -d /lib/multipath ]; then
	mkdir $tmp_mnt/lib/multipath
    fi
fi

save_var root_mpath
