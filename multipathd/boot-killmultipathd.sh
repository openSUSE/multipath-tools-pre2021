#!/bin/bash
#
#%stage: setup
#%provides: killprogs
#%depends: killudev
#
#%dontshow
#
##### kill multipathd 
##
## Kills multipathd. It will be started from the real root again.
##
## Command line parameters
## -----------------------
##

wait_for_events
/sbin/multipathd -k'shutdown'

