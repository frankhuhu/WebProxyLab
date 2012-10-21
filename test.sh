#!/bin/bash

PROXY=127.0.0.1:85621


if [ ! -d "result" ]; then
    mkdir result
fi


run_all() {
    index=0

    while read line
    do
        index=$(($index+1));
        echo "$index : $line"

        curl $line > "result/$index.noproxy"
#        curl --proxy $PROXY $line > "result/$index.proxy"
    done < url.txt
}

run_one() {
    echo "begin thread $1"

    lineno="$1p"
    url=`sed -n -e $lineno url.txt`

    curl --proxy $PROXY $url > "result/$1.proxy"

    echo "end thread $1"
}

# main
# first run curl without proxy
run_all

# multi-thread
nthrds=`wc -l < url.txt`
for tid in {1..13} ; do
    echo "spawn $tid"
    run_one $tid &
done
