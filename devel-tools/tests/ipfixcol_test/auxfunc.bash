#!/usr/bin/env bash

# ipfixcol Test Tool
# author Michal Srb <michal.srb@cesnet.cz>
# 
# Copyright (C) 2011 CESNET, z.s.p.o.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 3. Neither the name of the Company nor the names of its contributors
#    may be used to endorse or promote products derived from this
#    software without specific prior written permission.
# 
# ALTERNATIVELY, provided that this notice is retained in full, this
# product may be distributed under the terms of the GNU General Public
# License (GPL) version 2 or later, in which case the provisions
# of the GPL apply INSTEAD OF those given above.
# 
# This software is provided ``as is, and any express or implied
# warranties, including, but not limited to, the implied warranties of
# merchantability and fitness for a particular purpose are disclaimed.
# In no event shall the company or contributors be liable for any
# direct, indirect, incidental, special, exemplary, or consequential
# damages (including, but not limited to, procurement of substitute
# goods or services; loss of use, data, or profits; or business
# interruption) however caused and on any theory of liability, whether
# in contract, strict liability, or tort (including negligence or
# otherwise) arising in any way out of the use of this software, even
# if advised of the possibility of such damage.
#


# check for (maybe) non standard system utilities needed for testing
basic_system_check()
{
	local ret

	type $SENDDATA > /dev/null 2>&1 # do we have src/senddata built?
	ret=$?
	if [ $ret -ne 0 ]; then
		echo "Please build the 'src/senddata' utility using make"
        return ${ret}
	fi

	type hexdump > /dev/null 2>&1 # do we have hexdump?
	ret=$?
	if [ $ret -ne 0 ]; then
		echo "command 'hexdump' missing"
	fi

	return ${ret}
}

# check whether file is a valid IPFIX file
check_ipfix_file()
{
	local IPFIX_MAGIC_NO="0a00"
	local FILE_MAGIC_NO=

	if [ -z $1 ]; then
		echo "check_ipfix_file() - missing parameter"
		return 1
	fi

	if [ -f "$1" ]; then
		# separate out magic number from the file
		FILE_MAGIC_NO=`hexdump -n 2 $1 | tr -d '\n' | awk '{ print $2; }'`

		if [ ! -z $FILE_MAGIC_NO ]; then
			if [ $FILE_MAGIC_NO == $IPFIX_MAGIC_NO ]; then
				return 0
			fi
		fi
	fi

	return 1
}

# print usage
print_usage()
{
	echo "Usage: $1 tcp|upd path/to/test/message(s)"
}

# print message (if any) and quit
die()
{
	if [ $# -ne 0 ]; then
		echo -n "EXIT: "
	fi

	for i in $*; do
		echo -n "$i "
	done
	echo

	exit 1
}

