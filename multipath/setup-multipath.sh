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
    bindings_dir_fb=/var/lib/multipath
    bindings_file_fb=${bindings_dir_fb}/bindings
    if (( $use_kdump )) && [ -f /etc/multipath.conf.kdump ] ; then
	multipath_conf=/etc/multipath.conf.kdump
    else
	multipath_conf=/etc/multipath.conf
    fi
    if [ -f $multipath_conf ] ; then
	cp -a $multipath_conf $tmp_mnt/etc/multipath.conf
	f=$(sed -n '/^[ \t]*#.*/b;s/.*bindings_file *\"\?\([^" ]*\)\"\? */\1/p' $multipath_conf)
	if [ "$f" ] ; then
	    bindings_file=$f
	    bindings_dir=${bindings_file%/*}
	fi
    fi
    if [ ! -f ${bindings_file} -a -f ${bindings_file_fb} ]; then
	# Fall back to old path to bindings file (bnc#723620)
	bindings_dir=${bindings_dir_fb}
	bindings_file=${bindings_file_fb}
    fi
    if [ -f $bindings_file ] ; then
	[ -d ${tmp_mnt}${bindings_dir} ] || mkdir -p ${tmp_mnt}${bindings_dir}
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
