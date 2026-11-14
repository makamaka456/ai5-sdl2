#!/bin/bash
# usage: drive.sh name  "action..."   actions: c(click) r(return) w<sec> s<label>
export DISPLAY=:99
SS=/tmp/claude-0/ss
for a in "$@"; do
  case $a in
    c) xdotool mousemove 640 600 click 1;;
    c*) xy=${a#c}; xdotool mousemove ${xy%,*} ${xy#*,} click 1;;
    r) xdotool key Return;;
    w*) sleep ${a#w};;
    s*) import -window root $SS/${a#s}.png;;
  esac
done
