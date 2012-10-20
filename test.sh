#!/bin/bash

PROXY=127.0.0.1:85621


if [ ! -d "result" ]; then
    mkdir result
fi

index=0

while read line
do
    index=$(($index+1));
    echo "$index : $line"

    curl $line > "result/$index.noproxy"
    curl --proxy $PROXY $line > "result/$index.proxy"
done < url.txt
