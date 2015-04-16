#!/usr/bin/env bash

# 
# author Michal Kozubik <kozubik@cesnet.cz>
# 
# Copyright (C) 2015 CESNET, z.s.p.o.
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

# error report settings
MAIL_SUBJ="report"
MAIL_ADDR="petr.velan@cesnet.cz"

# initialize paths
DATA_DIR="";
REPORT_DIR="";
QUERIES="";

OFFSET=400;
ALIGN=300;

# print usage
function usage()
{
    echo "";
    echo "Usage: $0 -d <data_dir> -r <reports_dir> -q <queries_file>"
    echo "  -h                  Show this text"
    echo "  -d <data_dir>       Path to directory with fastbit data, can be set in fbitdump -R option notation (/some/dir/first:last)"
    echo "  -r <repors_dir>     Path to directory with reports, subdirs Year/month/day will be created automatically"
    echo "  -q <queries_file>   File with queries that will be performed (fbitdump arguments without specified data directory)"
    echo "";
    exit 1;
}

# parse arguments
while getopts ":hd:r:q:" opt; do
    case $opt in
    h)
        usage;
        ;;
    d)
        DATA_DIR=$OPTARG
        ;;
    r)
        REPORT_DIR=$OPTARG
        ;;
    q)
        QUERIES=$OPTARG
        ;;
    \?)
        echo "Invalid option: -$OPTARG" >&2
        usage;
        ;;
    :)
        echo "Option -$OPTARG requires an argument." >&2
        usage;
        ;;
    esac
done

# check directories
if [ -z $DATA_DIR ];   then echo "Path to data directory must be set"    >&2; exit 1; fi
if [ -z $REPORT_DIR ]; then echo "Path to reports directory must be set" >&2; exit 1; fi
if [ -z $QUERIES ];    then echo "Path to file with fbitdump queries"    >&2; exit 1; fi

# create directory for report
REPORTDIR="$REPORT_DIR/$(date +%Y/%m/%d)"
mkdir -p "$REPORTDIR"

DATAH=`nftime -f "%Y/%m/%d/ic%Y%m%d%H0000:%Y/%m/%d/ic%Y%m%d%H5500" -w $ALIGN -o $OFFSET`
DATADIR="$DATA_DIR/$DATAH"

# initialize report file
REPORT="$REPORTDIR/report-$(date +%H%M).txt"
rm -f "$REPORT"

# clear errors
ERRORS=""

# Create temporary stdout and stderr
OUT=`mktemp`
ERR=`mktemp`

# Do queries
while read query; 
do
    # ignore comments, process queries until END occurs
    if [ "${query:0:1}" = "#" ]; then continue; fi
    if [ "$query" = "END" ];     then break; fi

    # process test
    fbit="fbitdump -R $DATADIR $query";
    eval $fbit >"$OUT" 2>"$ERR"
    ret=$?
    
    # check return value
    if [ $ret == 0 ]; then
        # save data
        cat "$OUT" >> "$REPORT"
        echo "" >> "$REPORT"
    else 
        # save error output
        ERRORS="$ERRORS$fbit: $(cat $ERR)\n"
    fi

    # clear stdout and stderr
    echo "" > "$OUT"
    echo "" > "$ERR"
done < "$QUERIES"

# send mail if any error occured
if [ ! -z "$ERRORS" ]; then
    echo -e "$ERRORS" | mailx -s "$MAIL_SUBJ" "$MAIL_ADDR"
fi

# remove tmp files
rm -f "$OUT" "$ERR"