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

CREATE_BASE="$PWD"
CONFIGS_DIR="configs"
INTERNAL_ORIG="../../config/internalcfg.xml"
INTERNAL="$CONFIGS_DIR/internalcfg.xml"

if [ ! -f $INTERNAL_ORIG ]; then
	echo "Missing $INTERNAL_ORIG"
	exit 1
fi

if [ ! -d $CONFIGS_DIR ]; then
	mkdir $CONFIGS_DIR
fi

cp $INTERNAL_ORIG $INTERNAL

declare -A paths

cd ../../src/
paths["ipfixcol-ipfix-input"]="$PWD/input/ipfix/.libs"
paths["ipfixcol-udp-input"]="$PWD/input/udp/.libs"
paths["ipfixcol-tcp-input"]="$PWD/input/tcp/.libs"
paths["ipfixcol-sctp-input"]="$PWD/input/sctp/.libs"

paths["ipfixcol-anonymization-inter"]="$PWD/intermediate/anonymization/.libs"
paths["ipfixcol-dummy-inter"]="$PWD/intermediate/dummy/.libs"
paths["ipfixcol-filter-inter"]="$PWD/intermediate/filter/.libs"
paths["ipfixcol-joinflows-inter"]="$PWD/intermediate/joinflows/.libs"

paths["ipfixcol-dummy-output"]="$PWD/storage/dummy/.libs"
paths["ipfixcol-forwarding-output"]="$PWD/storage/forwarding/.libs"
paths["ipfixcol-ipfix-output"]="$PWD/storage/ipfix/.libs"
paths["ipfixcol-ipfixviewer-output"]="$PWD/ipfixviewer/.libs"

cd ../../plugins/storage/
paths["ipfixcol-fastbit-output"]="$PWD/fastbit/.libs"
paths["ipfixcol-nfdump-output"]="$PWD/nfdump/.libs"
paths["ipfixcol-postgres-output"]="$PWD/postgres/.libs"
paths["ipfixcol-statistics-output"]="$PWD/statistics/.libs"
paths["ipfixcol-unirec-output"]="$PWD/unirec/.libs"

cd "$CREATE_BASE"
for plugin in ${!paths[@]}; do
	sed -i 's,\(<file>\).*\('"$plugin"'\.so\)\(<\/file>\),\1'"${paths[$plugin]}"'\/\2\3,g' $INTERNAL
done