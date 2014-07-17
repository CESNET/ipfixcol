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

BASE="$PWD"
LF="test_log"
LOG_FILE="$BASE/$LF"
TESTDIR="$BASE/tests"

fails=0
oks=0
total=0


cd $TESTDIR;

echo -n "" > "$LOG_FILE" 

# do tests
for test in *; do
	cd "$TESTDIR/$test"
	PARAMS="-v 0 -c $STARTUP";
	if [ -f "$PREPROC" ]; then
		sh "$PREPROC";
	fi
	
	if [ -f "$PARFILE" ]; then
		PARAMS=$(cat "$PARFILE")
	fi
	
	$IPFIXCOL $PARAMS > "$OUTPUT"
	
	if [ -f "$POSTPROC" ]; then
		sh "$POSTPROC" ];
	fi
done

cd "$TESTDIR"

# check results
for test in *; do
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

