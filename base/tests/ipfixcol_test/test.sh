#!/usr/bin/env bash

IPFIXCOL="ipfixcol -v -1 -c"

CONF_FILE="test_conf.xml"
CONF_MED_PLUGINS=""

EXPECTED="expected.ipfix"
INPUT="input.ipfix"
OUTPUT="output.ipfix"

LOG_FILE="test_log"


# print xml configuration head with collecting process
# takes 1 argument: input file name
function print_head()
{
	echo '<?xml version="1.0" encoding="UTF-8"?>
<ipfix xmlns="urn:ietf:params:xml:ns:yang:ietf-ipfix-psamp">
    <collectingProcess>
        <name>TCP collector</name>
        <fileReader>
            <file>file:/'"$1"'</file>
        </fileReader>
        <exportingProcess>File writer TCP</exportingProcess>
    </collectingProcess>' > "$CONF_FILE"
}

# print xml configuration tail with exporting process
# takes 1 argument: output file name
function print_tail()
{
	echo '    <exportingProcess>
        <name>File writer TCP</name>
        <destination>
            <name>Write to /tmp folder</name>
            <fileWriter>
                <fileFormat>view</fileFormat>
                <file>file:/'"$1"'</file>
            </fileWriter>
        </destination>
    </exportingProcess>

</ipfix>' >> "$CONF_FILE"
}

# create intermediate plugins configuration and save it to variable
# takes 1 argument: list of plugins
function create_med_plugins_conf()
{
	CONF_MED_PLUGINS='\n    <ipfixcolCore>\n'
	for plugin in "${@}"; do
		CONF_MED_PLUGINS+='        <plugin>'"$plugin"'</plugin>\n'
	done
	CONF_MED_PLUGINS+='    </ipfixcolCore>\n'
	
	for plugin in "${@}"; do
		CONF_MED_PLUGINS+='\n    <intermediatePlugin>\n'
        CONF_MED_PLUGINS+='        <'"$plugin"'></'"$plugin"'>\n'
		CONF_MED_PLUGINS+='    </intermediatePlugin>\n'
	done
}

# create configuration file
# takes 2 arguments: input file, output file
function create_conf()
{
	print_head "$1"
	echo -e "$CONF_MED_PLUGINS" >> "$CONF_FILE"
	print_tail "$2"
}

aux_med_plugins=();

# process parameters
while getopts ":m:" opt; do
	case $opt in
		m)
			aux_med_plugins=($OPTARG);
			;;
		\?)
			echo "Invalid option: -$OPTARG"
			exit 1
			;;
		:)
			echo "Option -$OPTARG requires an argument"
			exit 1
			;;
	esac
done

shift $((OPTIND-1))

if [ $# != 0 ]; then
	echo "Unknown argument detected"
	exit 1
fi

# get list of intermediate plugins
med_plugins=($(echo ${aux_med_plugins[@]} | tr ':' '\n' | sort -u ))

if [ ${#med_plugins} != 0 ]; then
	create_med_plugins_conf "${med_plugins[@]}";
fi

create_conf "vstup" "vystup"

# do tests
for test in "$PWD/tests/"*; do
	create_conf "$test/$INPUT" "$test/$OUTPUT"
	$IPFIXCOL "$CONF_FILE" > "$test/$OUTPUT"
done

rm -f test_log

#TODO: choose testing dirs according to used intermediate plugin(s) (IP)
#		and parsed med_plugins array
# example structure:
# without any IP:
#	tests/01
#	tests/02
#	...
# with dummy IP chosen:
#	tests/dummy01
#	tests/dummy02
#	...
# with dummy and joinflows plugins:
#	tests/dummyjoinflows01
#	tests/dummyjoinflows02

# check results
for test in "$PWD/tests/"*; do
	echo -n "Test ${test#$PWD/tests/}: " | tee -a "$LOG_FILE"
	diff "$test/$OUTPUT" "$test/$EXPECTED" >> "$LOG_FILE" 2>&1
	if [ $? = 0 ]; then
		echo "OK" | tee -a "$LOG_FILE"
	else
		echo "FAIL" | tee -a "$LOG_FILE"
	fi
	rm -f "$test/$OUTPUT"
done

#rm "$CONF_FILE"

echo -e "\nTesting done, results saved into file 'test_log'"

