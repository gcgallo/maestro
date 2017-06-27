#!/bin/bash

usage() {
    echo `basename $0`: ERROR: $* 1>&2
    echo usage: `basename $0` '[ -o = cansetup | -s = cansniffer | -r = candump | -k = kill candump | -p = start playback | -e = end playback ]'
    exit 1
}

setup() {

    screen /dev/ttyACM$1
    pgrep screen -n >> ./filter$1.pid
    #echo $! >> ./can$1.pid

}


delete() {

   kill $(cat filter$1.pid)
   rm ./filter$1.pid
}


while getopts "o:s:rkped:" opt; do
   case $opt in
      o ) setup $OPTARG;;
      d ) delete $OPTARG;;
      * ) usage;;
   esac
done
