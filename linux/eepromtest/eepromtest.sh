#!/bin/sh
# eepromtest.sh
#
# Copyright (C) 2017 Intelight Inc. <mike.gallagher@intelight-its.com>
#
# A test for the eeprom interface on ATC controller.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
ip="2070-1C"

printf "EEPROM test\n"
printf "Initializing EEPROM content\n"
eeprom -u
printf "Reading expected data from EEPROM\n"
if [ $(dd if=/dev/eeprom bs=1 count=7 skip=7) == $ip ]; then
	printf "Passed\n"
else
	printf "Failed\n"
	exit 1
fi

