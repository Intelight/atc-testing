#!/bin/sh
# linesynctest.sh
#
# Copyright (C) 2017 Intelight Inc. <mike.gallagher@intelight-its.com>
#
# A test of Linesync operation on ATC controller.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
period=10
if [ $1 > $period ]; then
	period=$1
fi

printf "Linesync %d-second test...\n" $period

#save current timesrc
timesrc=$(timesrc | awk '{print substr($NF,1,length($NF)-3)}')

#change timesrc to CRYSTAL
timesrc CRYSTAL >/dev/null 2>&1

#Get cpu model from cpuinfo
cpu=$(awk '/model/ { print $3 }')</proc/cpuinfo
printf "cpu: %s\n" $cpu

#Get linesync irq
if [ $cpu == "EB885" ];then
	lsirq="24:"
elif [ $cpu == "EB8248" ];then
	lsirq="52:"
fi

lsT1=$(awk "/$lsirq/ "'{ print $2 }')</proc/interrupts
sysT1=$(awk "/expires_next/ "'{ print $3 }')</proc/timer_list

sleep $period

lsT2=$(awk "/$lsirq/ "'{ print $2 }')</proc/interrupts
sysT2=$(awk "/expires_next/ "'{ print $3 }')</proc/timer_list

lsticks=$(expr $lsT2 - $lsT1)
printf "%d ls ticks\n" $lsticks
lsticks=$(expr $lsticks \* 1000 \* 100)

systicks=$(expr $sysT2 - $sysT1)
systicks=$(expr $systicks \/ 1000000)
printf "%d sys ticks\n" $systicks

ticksec=$(expr $lsticks \/ $systicks)

printf "%d.%02d ticks per second\n" $((ticksec/100)) $((ticksec%(ticksec/100)))

#restore original timesrc
timesrc $timesrc >/dev/null 2>&1

if [ $ticksec -lt 11950 ] || [ $ticksec -gt 12050 ]
then
	printf "Failed\n"
	exit 1
fi
printf "Passed\n"
