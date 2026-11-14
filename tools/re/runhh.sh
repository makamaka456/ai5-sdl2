#!/bin/bash
# usage: runhh.sh logname [args...]  -- (re)start Xvfb if needed, kill old game, start hanahebi
pgrep -x Xvfb >/dev/null || { setsid nohup Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 < /dev/null & sleep 2; }
kill -9 $(pgrep -x ai5) 2>/dev/null; sleep 1
log=$1; shift
cd /home/user/gamedata/hanahebi
DISPLAY=:99 SDL_AUDIODRIVER=dummy setsid /home/user/ai5-sdl2/build/ai5 "$@" > /tmp/claude-0/ss/$log.log 2>&1 < /dev/null &
sleep 5
