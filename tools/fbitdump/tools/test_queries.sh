#!/usr/bin/env bash

# 
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

queries=(
'fbitdump -R DATA_DIR -s%srcip4/%byt -o "fmt:%srcip4 %byt"'
'fbitdump -R DATA_DIR -s%dstip4/%byt -o "fmt:%dstip4 %byt"'
'fbitdump -R DATA_DIR -s%srcip6/%byt -o "fmt:%srcip6 %byt"'
'fbitdump -R DATA_DIR -s%dstip6/%byt -o "fmt:%dstip6 %byt"'
'fbitdump -R DATA_DIR -s%dstip4/%byt "(%srcport 80 or %srcport 443) and %proto tcp" -o "fmt:%dstip4 %byt"'
'fbitdump -R DATA_DIR -s%dstip6/%byt "(%srcport 80 or %srcport 443) and %proto tcp" -o "fmt:%dstip6 %byt"'
'fbitdump -R DATA_DIR -s%srcip4/%byt "(%srcport 80 or %srcport 443) and %proto tcp" -o "fmt:%srcip4 %byt"'
'fbitdump -R DATA_DIR -s%srcip6/%byt "(%srcport 80 or %srcport 443) and %proto tcp" -o "fmt:%srcip6 %byt"'
'fbitdump -R DATA_DIR -s%srcip4/%byt -o "fmt:%srcip4 %byt %fl"'
'fbitdump -R DATA_DIR -s%srcip6/%byt -o "fmt:%srcip6 %byt %fl"'
'fbitdump -R DATA_DIR -s%dstip4/%byt -o "fmt:%dstip4 %byt %bps"'
'fbitdump -R DATA_DIR -s%dstip6/%byt -o "fmt:%dstip6 %byt %bps"'
'fbitdump -R DATA_DIR -s%srcip4/%byt -o "fmt:%srcip4 %byt %bps"'
'fbitdump -R DATA_DIR -s%srcip6/%byt -o "fmt:%srcip6 %byt %bps"'
'fbitdump -R DATA_DIR -s%srcip4/%fl -o "fmt:%srcip4 %fl"'
'fbitdump -R DATA_DIR -s%dstip4/%fl -o "fmt:%dstip4 %fl"'
'fbitdump -R DATA_DIR -s%srcip6/%fl -o "fmt:%srcip6 %fl"'
'fbitdump -R DATA_DIR -s%dstip6/%fl -o "fmt:%dstip6 %fl"'
'fbitdump -R DATA_DIR -s%srcport/%byt "%proto UDP" -o "fmt:%srcport %byt"'
'fbitdump -R DATA_DIR -s%dstport/%byt "%proto UDP" -o "fmt:%dstport %byt"'
'fbitdump -R DATA_DIR -s%srcport/%byt "%proto TCP" -o "fmt:%srcport %byt"'
'fbitdump -R DATA_DIR -s%dstport/%byt "%proto TCP" -o "fmt:%srcport %byt"'
'fbitdump -R DATA_DIR -s%dstas/%byt  -o "fmt:%dstas %byt %bps"'
'fbitdump -R DATA_DIR -s%srcas/%byt  -o "fmt:%srcas %byt %bps"'
'fbitdump -R DATA_DIR -s%httph -o http4-invea -o "fmt:%httph %fl"'
)

# error report settings
MAIL_SUBJ="report"
MAIL_ADDR="petr.velan@cesnet.cz"

# print usage
function usage()
{
    echo -e "\nUsage: $0 /path/to/data/dir/ /path/to/reports/dir"
    echo -e "\n  data dir can be set in fbitdump -R option notation (/some/dir/first:last)\n"
    exit 1
}

# check arguments
if [ $# -ne 2 ] || [ $1 = "-h" ] || [ $1 = "--help" ]; then usage; fi

# save data directory
DATADIR="$1"

# create directory for report
REPORTDIR="$2/$(date +%Y/%m/%d)"
mkdir -p "$REPORTDIR"

# initialize report file
REPORT="$REPORTDIR/report-$(date +%H%M).txt"
rm -f "$REPORT"

# clear errors
ERRORS=""

# Create temporary stdout and stderr
OUT=`mktemp`
ERR=`mktemp`

# Do queries
for query in "${queries[@]}"
do 
    # substitute data directory
    fbit="${query/DATA_DIR/$DATADIR}"

    # perform operation
    eval "$fbit" 1>"$OUT" 2>"$ERR"
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
done

# send mail if any error occured
if [ ! -z "$ERRORS" ]; then
    echo -e "$ERRORS" #| mailx -s "$MAIL_SUBJ" "$MAIL_ADDR"
fi

# remove tmp files
rm -f "$OUT" "$ERR"