#!/bin/bash
#
#%stage: partitions
#
if [ -x /sbin/dmsetup ]; then
	kpartx_blockdev=

	# always activate kpartx when using dm block
	[ "$DM_BLOCK" ] && root_kpartx=1
	# always activate kpartx for multipathing
	use_script multipath && root_kpartx=1
	for bd in $blockdev ; do
	    update_blockdev $bd
	    if [ "$blockdriver" = device-mapper ]; then
		dm_uuid=$(dmsetup info -c --noheadings -o uuid -j $blockmajor -m $blockminor)
		case $dm_uuid in
		    part*)
		    	dm_uuid="${dm_uuid#*-}"
				kpartx_blockdev="$kpartx_blockdev $(majorminor2blockdev $(dmsetup info -u $dm_uuid --noheadings -c -o major,minor))"
				root_kpartx=1
				;;
			*)
				kpartx_blockdev="$kpartx_blockdev $bd"
				;;
		esac
	    else
		kpartx_blockdev="$kpartx_blockdev $bd"
	    fi
	done

	blockdev="$kpartx_blockdev"
fi

if use_script kpartx; then 
	cp /etc/udev/rules.d/70-kpartx.rules $tmp_mnt/etc/udev/rules.d
fi
