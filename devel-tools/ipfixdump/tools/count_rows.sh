#!/bin/bash
if [ -z $1 ]; then
    echo "specify fastbit directory"
else
    count=0;
    cd $1;
    for dir in *; do
        cnt=`grep rows $dir/-part.txt | awk '{print $3;}'`;
        echo $dir: $cnt;
        let count=$cnt+$count;
    done;
    echo sum: $count;
fi
