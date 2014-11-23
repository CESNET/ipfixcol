#!/usr/bin/env bash
# Michal Bandzi
# xbandz00@stud.fit.vutbr.cz

# Prints time of proces duration and total time of all processes.
# Usage: fbitdump_timer.sh [-m | -n] [-d DIR]" 
# Default time is in seconds. You can use minutess (-m) or nanoseconds (-n).

# Arguments of fbitdump for testing can be edited in file "fbitdump.args".
# Tests can be added, skipped or removed from "fbitdump.args" aswell.

# You can select files for fbitdump using [-d DIR] option.
# Otherways current directory '.' is used.
# Argument takes same input as fbitdump -R option.

# Output of fbitdump is thrown away during process.
# Error messages are printed normally to stderr.

START_TIME=0
TOTAL_TIME=0
FINAL_TIME=0

SECONDS=1 # time is in seconds by default
MINUTES=0
NANOSEC=0

TIME_FORM=+%s
TIME_UNIT="seconds"

FBITDUMP_ARGS=0
PATH_USED=0
SIZE_OF=0
COLON=0
DATA=0
FST=0
SND=0
DIR=.

while getopts "mnd:" ARG
do
	case $ARG in
		m)
			SECONDS=0
			MINUTES=1
			TIME_UNIT="minutes"
			TIME_FORM=+%M
			;;
		n)
			SECONDS=0
			NANOSEC=1
			TIME_UNIT="nanoseconds"
			TIME_FORM=+%s%N
			;;
		d)
			DIR=$OPTARG
			;;
		?) 
			echo "Invalid arguments. Usage: fbitdump_timer.sh [-m | -n] [-d DIRs]"
			exit 1
			;;
	esac
done

if [[ $MINUTES == 1 && $NANOSEC == 1 ]]; then 
echo "Invalid arguments. Usage: fbitdump_timer.sh [-m | -n] [-d DIRs]"
exit 1
fi

COLON=$(echo $DIR | grep -o ":")
if [[ $COLON == ":" ]]; then # size of range of files
{
	PATH_USED=${DIR%/*}
	FST=$(echo ${DIR##*/} | grep -o ".*:") # file range start
	SND=$(echo ${DIR##*/} | grep -o ":.*") # file range end
	FST=${FST%?}
	SND=${SND#?}
	FILE_RANGE=($(ls $PATH_USED | sed -n "/$FST/,/$SND/p"))
	
	for i in "${FILE_RANGE[@]}"
	do {
		TMP=$(du -c $PATH_USED/$i | grep "total" | grep -o "[0-9]*,*[0-9]*")
		SIZE_OF=$(($SIZE_OF + $TMP)) # get total size of file range
	} done
}
else 
{
	SIZE_OF=$(du -c $DIR | grep "total" | grep -o "[0-9]*,*[0-9]*")
}
fi

ARGS_COUNT=$(wc -l < fbitdump.args) # gets number of fbitdump tests
DATA=$(eval fbitdump -R $DIR -s%srcip4/%byt | grep -o '[0-9]* tables.*rows')
# gets number of rows and tables

for ((C=1; C<=${ARGS_COUNT}; C++))
do {
	
	FBITDUMP_ARGS=$(sed -n "${C}p" < fbitdump.args) # gets arguments for test
	FIRST_C=${FBITDUMP_ARGS:0:1}
	if [[ $FIRST_C == "#" ]]; then continue # ignore commneted out entries
	elif [[ $FIRST_C == "E" ]]; then # EOF
	{
		echo Data used: $'\''$DIR$'\''.
		echo Data size: $SIZE_OF$'K'.
		echo Data contains: $DATA.
		echo Total time elapsed: $TOTAL_TIME $TIME_UNIT.
		exit 0
	}
	fi

	echo Test arguments: $FBITDUMP_ARGS # print args
	echo 3 > /proc/sys/vm/drop_caches
	START_TIME=$(date $TIME_FORM)
	eval fbitdump -R $DIR $FBITDUMP_ARGS > /dev/null # start process
	FINAL_TIME=$(date $TIME_FORM)
	TIME=$(($FINAL_TIME - $START_TIME))
	TOTAL_TIME=$(($TOTAL_TIME + $TIME)) # time math

	echo Test time: $TIME $TIME_UNIT.
	echo "  "

} done
exit 0
