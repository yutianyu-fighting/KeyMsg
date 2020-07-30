#!/bin/bash


if [ $# -eq 0  ];then
    echo "please input ./XXX.sh run or stop"
    exit 1
fi

start="run"
stop="stop"

if [ $1 == $start  ];then
    ./bin/keymngserver
    ./bin/keymngclient
else
    ./myshell
fi
