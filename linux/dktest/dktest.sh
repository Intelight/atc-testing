#!/bin/sh -e
# dktest.sh
#
# Copyright (C) 2017 Intelight Inc. <mike.gallagher@intelight-its.com>
#
# A test for the datakey interface on ATC controller.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

#Check for the TEES ip address 10.20.70.51 in the Datakey header
ip="0a144633"

printf "Datakey test\n"

#Check datakey present input
kver=$(uname -r | cut -d'.' -f1-2)
if [ $kver == "3.0" ] || [ $kver == "3.4" ]; then
	present="00"
else
	present="01"
fi
if [ $(od -An -N1 -tx1 /dev/datakeypresent) != $present ]; then
	printf "datakey not present!\n"
	exit 1
fi

printf "checking TEES header data...\n"
if [ $(od -An -N4 -j16 -tx4 </dev/datakey) != $ip ]; then
	printf "TEES data not present\n"
	exit 1
fi

printf "saving copy of first sector of key...\n"
dd if=/dev/datakey of=/tmp/dk.sector1 bs=64K count=1 >/dev/null 2>&1

printf "erasing first sector of key...\n"
mtd_debug erase /dev/datakey 0 65536 >/dev/null 2>&1

#Check for TEES ip again (should fail!)
printf "confirming sector erase...\n"
if [ $(od -An -N4 -j16 -tx4 </dev/datakey) == $ip ]; then
	printf "Datakey could not be erased\n"
	exit 1
fi

printf "restoring first sector from copy...\n"
dd if=/tmp/dk.sector1 of=/dev/datakey bs=64K count=1 >/dev/null 2>&1

#Check TEES ip address in header data
printf "re-checking TEES header data...\n"
if [ $(od -An -N4 -j16 -tx4 </dev/datakey) != $ip ]; then
	printf "TEES data not restored\n"
	exit 1
fi

#Erase saved copy
rm /tmp/dk.sector1

printf "Passed\n"
exit 0

