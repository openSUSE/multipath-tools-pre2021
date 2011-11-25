#!/bin/bash
#
#%stage: setup
#%provides: killprogs
#%depends: killudev
#
#%if: "$root_mpath"
#%dontshow
#
##### kill multipathd 
##
## Kills multipathd. It will be started from the real root again.
##
## Command line parameters
## -----------------------
##

if [ -x /sbin/multipathd ] ; then
    wait_for_events
    /sbin/multipathd -k'shutdown'
fi

