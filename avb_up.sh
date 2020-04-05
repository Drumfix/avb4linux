#!/bin/bash
# Simple script to run igb_avb

export AVBHOME=$HOME/.avb
export INTERFACE=$1
export SAMPLERATE=$2

rmmod igb
rmmod igb_avb
modprobe i2c_algo_bit
#modprobe dca
modprobe ptp
insmod $AVBHOME/igb_avb.ko samplerate=$SAMPLERATE

ethtool -i $INTERFACE

sleep 1
ifconfig $INTERFACE down
echo 0 > /sys/class/net/$INTERFACE/queues/tx-0/xps_cpus
echo 0 > /sys/class/net/$INTERFACE/queues/tx-1/xps_cpus
echo f > /sys/class/net/$INTERFACE/queues/tx-2/xps_cpus
echo f > /sys/class/net/$INTERFACE/queues/tx-3/xps_cpus
ifconfig $INTERFACE up promisc

sleep 1

echo "Starting daemons on "$INTERFACE

groupadd ptp > /dev/null 2>&1
$AVBHOME/daemon_cl $INTERFACE &
$AVBHOME/mrpd -mvs -i $INTERFACE &
$AVBHOME/maap_daemon -i $INTERFACE -d /dev/null &
$AVBHOME/shaper_daemon -d &

sleep 10

$AVBHOME/avb-user $INTERFACE $SAMPLERATE

