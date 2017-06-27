#!/bin/bash

usage() {
    echo `basename $0`: ERROR: $* 1>&2
    echo usage: `basename $0` '[ -o = cansetup | -s = cansniffer | -r = candump | -k = kill candump | -p = start playback | -e = end playback ]'
    exit 1
}

setup() {

    slcand -o -c -s6 /dev/ttyACM$1 can$1
    pgrep slcand -n >> ./can$1.pid
    #echo $! >> ./can$1.pid
    sleep 2
    slcand -o -c -s6 /dev/ttyACM$1 can$1
    pgrep slcand -n >> ./can$1.pid
    #echo $! >> ./can$1.pid
    ifconfig can$1 up
    ifconfig can$1 txqueuelen 1000

}

sniffer() {

    cansniffer -c -l 1 -t 0 can$1

}

record() {

     rm ./dump.log
     candump -L can0 > dump.log &
     echo $! > ./dump.pid

}

kill_record() {

    kill $(cat dump.pid)
    rm ./dump.pid

}

playback() {

    canplayer -I dump.log &
    echo $! > ./play.pid

}

end_playback() {

    kill $(cat play.pid)
    rm ./play.pid

}

delete_can() {

   ifconfig can$1 down 
   kill $(cat can$1.pid)
   rm ./can$1.pid
}


while getopts "o:s:rkped:" opt; do
   case $opt in
      o ) setup $OPTARG;;
      s ) sniffer $OPTARG;;
      r ) record;;
      k ) kill_record;;
      p ) playback;;
      e ) end_playback;;
      d ) delete_can $OPTARG;;
      * ) usage;;
   esac
done
