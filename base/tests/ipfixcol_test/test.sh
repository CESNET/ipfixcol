#!/usr/bin/env bash

BASE="$PWD"

cd ../../src/

IPFIXCOL="$PWD/ipfixcol "

cd "$BASE"

EXPECTED="expected"
OUTPUT="output"

STARTUP="startup.xml"
PARFILE="params"
PREPROC="preproc.sh"
POSTPROC="postproc.sh"
INTERNAL=/etc/ipfixcol/internalcfg.xml


BASE="$PWD"
LF="test_log"
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
fi

echo -n "" > "$LOG_FILE" 
echo "Using internal configuration file: $INTERNAL" | tee -a "$LOG_FILE"

cd $TESTDIR;
# do tests
for test in *; do
	if [ $test = "ipfix_data" ]; then 
		continue;
	fi
	cd "$TESTDIR/$test"
	PARAMS="-v -1 -c $STARTUP -i $INTERNAL";
	if [ -f "$PREPROC" ]; then
		sh "$PREPROC";
	fi
	
	if [ -f "$PARFILE" ]; then
		PARAMS=$(cat "$PARFILE")
	fi
	
	$IPFIXCOL $PARAMS > "$OUTPUT" 2>&1
	
	if [ -f "$POSTPROC" ]; then
		sh "$POSTPROC" ];
	fi
done

cd "$TESTDIR"

# check results
for test in *; do
	if [ $test = "ipfix_data" ]; then
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

