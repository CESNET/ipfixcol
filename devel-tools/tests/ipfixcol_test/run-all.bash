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


if [ $# -lt 2 ]; then
	echo "Usage: $0 tcp|udp path/to/the/directory"
	exit -1
fi

if [ "$1" != "tcp" ] && [ "$1" != "udp" ]; then
	echo "unknown protocol $1. valid protocols are \"tcp\" and \"udp\""
	exit -1
fi

if [ ! -d "$2" ]; then
	echo "$2 is not a directory"
	exit -1
fi

fails=0

TEST_DIRS=`find $2 -name HASH -exec dirname {} \;`
TEST_NO=1

# do not overwrite last test.log
if [ -f test.log ]; then
	rm -f test.log.old
	mv test.log test.log.old
fi

for TEST in $TEST_DIRS; do
	echo -n "Test $TEST_NO: "
	./run-test.bash $1 ${TEST} 2>&1 >> test.log
	if [ $? -ne 0 ]; then
		((fails=$fails+1))
		echo "FAILED ($TEST)"
	else
		echo "OK ($TEST)"
	fi
	((TEST_NO=$TEST_NO+1))
done

echo $fails tests failed

exit $fails

