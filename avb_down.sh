#!/bin/bash
# Stop all daemons

export INTERFACE=$1

killall shaper_daemon
killall maap_daemon
killall mrpd
killall daemon_cl
killall avb-user

sleep 1

# remove igb_avb module

ifconfig $INTERFACE down
rmmod igb_avb

modprobe igb

