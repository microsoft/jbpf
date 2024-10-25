#!/bin/bash

if [[ $# -eq 0 ]] ; then
    echo 'Usage: ./kill-port-process.sh <port> <port> <port> ...'
    exit 0
fi

SUDO_CMD=""
if [[ $EUID -ne 0 && `which sudo` ]]; then
    SUDO_CMD="sudo"
fi

cnt=0
for port in $@; do
    while :; do
        processid=$($SUDO_CMD lsof -i :$port | tail -1 | awk '{print $2}')
        if [[ ! -z "$processid" ]]; then
            echo "Killing process $processid"
            $SUDO_CMD kill -9 $processid
            cnt=$((cnt+1))
        else
            break
        fi
    done
done

exit $cnt
