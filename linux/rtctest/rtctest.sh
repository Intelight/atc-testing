#!/bin/sh
# rtctest.sh
#
# Copyright (C) 2017 Intelight Inc. <mike.gallagher@intelight-its.com>
#
# A test for the RTC operation on ATC controller.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
printf "RTC test...\n"
#Get timestamp from file /etc/os-release
dt="$(date -r /etc/os-release +%F) $(date -r /etc/os-release +%T)"
#Set system clock to reference date/time
printf "setting OS to reference date/time\n"
date "$dt"
#Wait for tod driver to update RTC from system clock
usleep 5500000
#Compare RTC to system clock
printf "comparing RTC to OS date/time\n"
hc="$(cat /sys/class/rtc/rtc0/since_epoch)"
sc="$(date +%s)"
if [ "$hc" != "$sc" ]
then
	printf "Failed (hc=%s sc=%s)\n" "$hc" "$sc" 
	exit 1
fi
printf "Passed\n"

