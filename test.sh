#!/bin/bash

PROXY=127.0.0.1:62185
#PROXY=ec2-54-242-84-63.compute-1.amazonaws.com:62185

if [ ! -d "result" ]; then
    mkdir result
fi


run_all() {
    index=0

    while read line
    do
        index=$(($index+1));
        echo "$index : $line"

        curl $line > "result/$index"_"0.noproxy"
#        curl --proxy $PROXY $line > "result/$index.proxy"
    done < url.txt
}

run_one() {
    echo "begin thread $1"

    tid=$1
    totalline=`wc -l < url.txt`
    lineno=$RANDOM
    let "lineno %= $totalline"
    let "lineno += 1"
    url=`sed -n -e $lineno'p' url.txt`

    curl --proxy $PROXY $url > "result/$lineno"_"$tid.proxy"

    echo "end thread $1"
}

# main

# multi-thread
nthrds=`wc -l < url.txt`
for tid in {1..50} ; do
    echo "spawn $tid"
    run_one $tid &
    sleep 0.5
done

# run curl without proxy
run_all
