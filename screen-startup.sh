#!/bin/bash
usage() {
    echo `basename $0`: ERROR: $* 1>&2
    echo usage: `basename $0` '[ -o = setup two screen windows ]'
    exit 1
}

setup() {

  #screen -AdSm cmd-control bash
  screen -AdSm sniffer bash
  screen -AdSmR filter ./filter.sh -o $1
  screen -r filter

}

while getopts "o:" opt; do
    case $opt in
       o ) setup $OPTARG;;
       * ) usage;;
    esac
done

