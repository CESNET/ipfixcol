#!/usr/bin/env bash

# ipfixcol Test Tool
# author Michal Kozubik <kozubik@cesnet.cz>
# 
# Copyright (C) 2014 CESNET, z.s.p.o.
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

BASE="$PWD"

cd ../../src/

IPFIXCOL="$PWD/ipfixcol "
IPFIXSEND="$PWD/utils/ipfixsend/ipfixsend "

cd "$BASE"

EXPECTED="expected"
OUTPUT="output"

STARTUP="startup.xml"
PARFILE="params"
PREPROC="preproc.sh"
POSTPROC="postproc.sh"
IPFIXSEND_RUN="ipfixsend.sh"
INTERNAL=$(readlink -fe -- configs/internalcfg.xml)

BASE="$PWD"
LF="test.log"
LOG_FILE="$BASE/$LF"
TESTDIR="$BASE/tests"

fails=0
oks=0
total=0

function usage()
{
	echo -e "Usage: $0 /path/to/internalcfg.xml"
	exit 1
}

if [ $# -ge 1 ]; then
	if [ $1 = "-h" ]; then
		usage;
	fi
	INTERNAL=$(readlink -fe -- "$1");
	if [ -z $INTERNAL ]; then
		echo "Internal configuration file $1 not found!"
		exit 1
	fi
else 
	# if configs/internalcfg.xml does not exists, create it
	if [ -z $INTERNAL ]; then
		./create_internal.sh
		INTERNAL=$(readlink -fe -- configs/internalcfg.xml)
		if [ -z $INTERNAL ]; then
			echo "Cannot create config/internalcfg.xml!"
			exit 1
		fi
	fi
fi

export IPFIX_TEST_INTERNALCFG=$INTERNAL
export IPFIX_TEST_IPFIXCOL=$IPFIXCOL
export IPFIX_TEST_IPFIXSEND=$IPFIXSEND

echo -n "" > "$LOG_FILE" 
echo -e "Using internal configuration file: $INTERNAL\n" | tee -a "$LOG_FILE"

cd $TESTDIR;
# do tests
for test in *; do
	if [ "$test" = "ipfix_data" ]; then 
		continue;
	fi
        echo "testing: $test"
	cd "$TESTDIR/$test"
	PARAMS="-v -1 -c $STARTUP";
	if [ -f "$PREPROC" ]; then
		sh "$PREPROC";
	fi
	
	if [ -f "$PARFILE" ]; then
		PARAMS=$(cat "$PARFILE")
	fi
	
        if [[ "$test" == "ipfixsend "* ]]; then
            $IPFIXCOL $PARAMS -i $INTERNAL > "$OUTPUT" 2>&1 &
            echo $! > tmp_pid
            sleep 1 # ipfixcol initialization
            
            sh $IPFIXSEND_RUN

            sleep 1 # ipfixcol processing

            kill $(cat tmp_pid)
            rm -f tmp_pid
        else
            $IPFIXCOL $PARAMS -i $INTERNAL > "$OUTPUT" 2>&1
        fi
	
	if [ -f "$POSTPROC" ]; then
		sh "$POSTPROC" ];
	fi
done

cd "$TESTDIR"
echo ""

# check results
for test in *; do
	if [ "$test" = "ipfix_data" ]; then
		 continue;
	fi
	cd "$TESTDIR/$test"
	echo -n "Test ${test}: " | tee -a "$LOG_FILE"
	diff "$OUTPUT" "$EXPECTED" >> "$LOG_FILE" 2>&1
	if [ $? = 0 ]; then
		echo "OK" | tee -a "$LOG_FILE"
		oks=$(( oks + 1 ))
	else
		echo "FAIL" | tee -a "$LOG_FILE"
		fails=$(( fails + 1 ))
	fi
	total=$(( total + 1 ))
	rm -f "$OUTPUT"
done

cd "$BASE"

echo -e "\nTesting done, $oks/$total passed" | tee -a "$LOG_FILE"
echo "Results saved into file $LF"

