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


# path to the collector executable
COLLECTOR_EXEC="./../../../ipfixcol"
# run collector with these args
COLLECTOR_ARGS="-v 5"
TEST_TOOL_PWD=${PWD}
COLLECTOR_CONFIG="-c ${PWD}/config/tcp-ipfix.xml"
# dir where the test messages are stored
TEST_MSGS_DIR="${2}"
MESSAGES=
# all input files combined into the single file
TMP_BIG_MESSAGE="/tmp/ipfixcol_test_tool_message-$RANDOM"
COLLECTOR_OUTPUT_LOG="collector_output.log"
VERSION=0.9
COLLECTOR_GRACEFUL_KILL="kill -s 2 "
COLLECTOR_VIOLENT_KILL="kill -s 9 "
PROGNAME=${0}
OUTPUT_DIR="/tmp/ipfixcol-test/"
CORRECT_OUTPUT=
NETCAT=
DESIRED_OUTPUT_HASH=
ACTUAL_OUTPUT_HASH=

source ./auxfunc.bash

echo "################################################################################"
echo "Starting ipfixcol Test Tool (`date`)"
echo "version $VERSION"
echo

basic_system_check
if [ $? -ne 0 ]; then
	die
fi

# check whether user provided required parameters
if [ $# -le 1 ]; then
	print_usage $PROGNAME
	die
fi

case "$1" in
	tcp) COLLECTOR_CONFIG="-c ${PWD}/configs/tcp-ipfix.xml"
		 NETCAT="nc localhost 4739" ;;
	udp) COLLECTOR_CONFIG="-c ${PWD}/configs/udp-ipfix.xml"
		 NETCAT="nc -u localhost 4739" ;;
	  *) echo "ERROR: unknown protocol $1"
	     print_usage
		 die ;;
esac
echo "output dir: $OUTPUT_DIR"


# check for presence of collector executable
if [ ! -e ${COLLECTOR_EXEC} ] && [ ! -x ${COLLECTOR_EXEC} ]; then
	die "$COLLECTOR_EXEC - no such file"
fi

# create output log
touch $"${TEST_TOOL_PWD}/${COLLECTOR_OUTPUT_LOG}"

# check for testing IPFIX messages
if [ ! -d ${TEST_MSGS_DIR} ]; then
	die "$TEST_MSGS_DIR - no such directory"
fi

# delete old output dir
rm -fr $OUTPUT_DIR

# create new output dir
mkdir $OUTPUT_DIR

MESSAGES=`ls $TEST_MSGS_DIR`
# find all test files and combine them into single file
echo "input files:"
for i in $MESSAGES; do
	TEST_FILE="${TEST_MSGS_DIR}/${i}"
	check_ipfix_file $TEST_FILE
	if [ $? -eq 0 ]; then
		M="${M} $TEST_FILE"
		echo "    `basename $TEST_FILE`"
	else
		# echo "$i is not a valid IPFIX file, skipping"
		echo -n    # bash the bash error
	fi
done
if [ -z "$M" ]; then
	die "no IPFIX file, nothing to do"
else
	cat $M > $TMP_BIG_MESSAGE
fi

BIG_MESSAGE_HASH=`md5sum $TMP_BIG_MESSAGE | awk '{ print $1; }'`

if [ -f "$TEST_MSGS_DIR/OUTPUT" ]; then
	CORRECT_OUTPUT="$TEST_MSGS_DIR/OUTPUT"
	echo "OUTPUT file found"
else 
	env --unset CORRECT_OUTPUT > /dev/null
fi

# start collector in background
COLLECTOR_DIR=`dirname ${COLLECTOR_EXEC}`
cd ${COLLECTOR_DIR}
echo "starting collector: ./ipfixcol $COLLECTOR_ARGS $COLLECTOR_CONFIG"
COLLECTOR="./ipfixcol $COLLECTOR_ARGS $COLLECTOR_CONFIG"
${COLLECTOR} > "${TEST_TOOL_PWD}/${COLLECTOR_OUTPUT_LOG}" 2>&1 &
COLLECTOR_PID=$!
echo collector\'s PID: $COLLECTOR_PID
cd - > /dev/null


# connect to the collector and send data
echo "sleeping 1 second (waiting for collector to initialize)"
sleep 1
echo "sending data to the collector"
if [ $1 == "tcp" ]; then
	${NETCAT} < ${TMP_BIG_MESSAGE}
else
	${NETCAT} < ${TMP_BIG_MESSAGE} &
	# netcat in udp mode doesn't exit on EOF(?) 
	sleep 1          # hope this will be enough
	kill -s 2 $!
fi
echo "all data sent"

# we don't need big test file anymore
rm ${TMP_BIG_MESSAGE}

# wait a second before killing the collector
sleep 1

# time to kill collector
COLLECTOR_GRACEFUL_KILL="$COLLECTOR_GRACEFUL_KILL $COLLECTOR_PID"
echo "sending SIGINT to the collector"
${COLLECTOR_GRACEFUL_KILL}
if [ $? -ne 0 ]; then
	echo "unable to kill collector gracefully"
	echo "sending SIGKILL to the collector"
	COLLECTOR_VIOLENT_KILL="$COLLECTOR_VIOLENT_KILL $COLLECTOR_PID"
	${COLLECTOR_VIOLENT_KILL}
	echo "collector killed"
else
	echo "waiting for collector to exit"
	wait ${COLLECTOR_PID}
	echo "collector exited with status $?"
fi


# analyze output log

fail=0
ok=0   # number of tests passed
if [ ! -z $CORRECT_OUTPUT ]; then
	i=1
	while read line
	do
		# skip empty lines
		if [ -z "$line" ]; then
			continue
		fi
		
		wanted_output[$i]="$line"
	
		((i=$i+1))
	done < $CORRECT_OUTPUT
	
	
	i=1
	while read line
	do
		# skip empty lines
		if [ -z "$line" ]; then
			continue
		fi
		
		actual_output[$i]="$line"
	
		((i=$i+1))
	done < $COLLECTOR_OUTPUT_LOG

	
	echo
	echo "checking output messages..."
	
	i=1
	ok=0
	for lineno in ${!actual_output[*]}; do
		line=${actual_output[$lineno]}


		if [ "${actual_output[$lineno]}" == "${wanted_output[$i]}" ]; then
			echo -n "[OK]      $line"
			wanted_output[$i]=""
			((i=$i+1))
			((ok=$ok+1))
			echo
		else
			# check if desired output isn't out of order
			for y in ${!actual_output[*]}; do
				if [ "${actual_output[$lineno]}" == "${wanted_output[$y]}" ]; then
					echo -n "[OK]      $line"
					wanted_output[$y]=""
					((y=$y+1))
					((ok=$ok+1))
					echo
				fi
			done
		fi
	done
fi


if [ $ok -ne ${#wanted_output[*]} ]; then
	for i in ${!wanted_output[*]}; do
		if [ ! -z "${wanted_output[$i]}" ]; then
			echo "[MISSING] ${wanted_output[$i]}"
		fi
	done
	echo "test FAILED"
	fail=1
else
	echo "test PASSED"
fi

echo

# get output full file name, the newest file in the OUTPUT_DIR
OUTPUT_FILE=$OUTPUT_DIR`ls $OUTPUT_DIR | head -1`

echo "checking output file ${OUTPUT_DIR}${OUTPUT_FILE} ..."
if [ -f "${TEST_MSGS_DIR}/HASH" ]; then
	read -r DESIRED_OUTPUT_HASH < "${TEST_MSGS_DIR}/HASH"

	DESIRED_OUTPUT_HASH=`echo -n "$DESIRED_OUTPUT_HASH" | awk '{ print $1; }'`
	echo "should be: $DESIRED_OUTPUT_HASH"

	ACTUAL_OUTPUT_HASH=`md5sum $OUTPUT_FILE 2> /dev/null | awk '{ print $1; }'`
	if [ ! -f "$OUTPUT_FILE" ]; then
		# looks like output file doesn't exist
		if [ "$DESIRED_OUTPUT_HASH" == "X" ]; then
			# this is what we wanted, there is no output file and it is
			# correct
			ACTUAL_OUTPUT_HASH="X"
			echo "is:        $ACTUAL_OUTPUT_HASH"
			
		fi
	else
		echo "is:        $ACTUAL_OUTPUT_HASH"
	fi

	if [ "$DESIRED_OUTPUT_HASH" == "$ACTUAL_OUTPUT_HASH" ]; then
		echo "test PASSED"
	else
		echo "test FAILED"
		fail=1
	fi
else
	echo "file HASH not found. skipping this test"
fi

echo
echo "overall status:"
if [ $fail -eq 0 ]; then
	echo "all OK"
	ret=0
else
	echo "test FAILED"
	ret=1
fi

exit $ret

