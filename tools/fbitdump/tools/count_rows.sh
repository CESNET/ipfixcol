#!/bin/bash
if [ -z $1 ]; then
    echo "specify fastbit directory"
else
    count=0;
    cd $1;
    for dir in *; do
        cd $dir;
        for subdir in *; do
            cnt=`grep rows $subdir/-part.txt | awk '{print $3;}'`;
            echo $dir/$subdir: $cnt;
            let count=$cnt+$count;
        done;
        cd -;
    done;
    echo sum: $count;
fi
