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
	if [ "$blockdriver" = device-mapper ]; then
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
    bindings_dir=/etc/multipath
    bindings_file=${bindings_dir}/bindings
    if [ -f /etc/multipath.conf ] ; then
	cp -a /etc/multipath.conf $tmp_mnt/etc
	    f=$(sed -n '/^[ \t]*#.*/b;s/.*bindings_file *\"\?\([^" ]*\)\"\? */\1/p' /etc/multipath.conf)
	    if [ "$f" ] ; then
		bindings_file=$f
		bindings_dir=${bindings_file%/*}
    fi
    fi
    [ -d ${tmp_mnt}${bindings_dir} ] || mkdir -p ${tmp_mnt}${bindings_dir}
    if [ -f $bindings_file ] ; then
	    cp -a $bindings_file ${tmp_mnt}${bindings_file}
    fi
    if [ -e /etc/udev/rules.d/71-multipath.rules ]; then
	cp /etc/udev/rules.d/71-multipath.rules $tmp_mnt/etc/udev/rules.d
    fi
    if [ -e /etc/udev/rules.d/72-multipath-compat.rules ]; then
	cp /etc/udev/rules.d/72-multipath-compat.rules $tmp_mnt/etc/udev/rules.d
    fi
    if [ -d /@LIB@/multipath ]; then
	mkdir -p $tmp_mnt/@LIB@/multipath
    fi
fi

save_var root_mpath
