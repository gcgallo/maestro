#!/bin/bash
sudo /home/pi/can.sh -o 0
tmux new-session -n maestro -d 'bash' #'./screen-startup.sh -o 1'
#tmux send "screen -ASdm can1 -X screen 'sudo /home/pi/can.sh -s'" 
#tmux send "screen -r can1"
tmux split-window -v 'python3 /home/pi/main.py'
tmux resize-pane -D 10
tmux select-pane -l
tmux split-window -h 'sudo /home/pi/can.sh -s 0'
tmux -2 attach-session -d 
