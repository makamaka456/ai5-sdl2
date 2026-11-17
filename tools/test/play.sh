#!/bin/bash
# 무작위 입력으로 게임을 계속 진행시킴 (Ctrl 누른 채 = 대사 넘기기)
# 사용: play.sh <이름> <DISPLAY> <반복 수> <게임 pid>
#  60% 대사창 클릭, 20% 아래 화살표 0~21번 + Enter(긴 메뉴의 아래 항목까지), 나머지 화면 아무 데나 클릭,
#  4% "방에서 나가기"(조사 화면 오른쪽 아래). 로그 크기가 안 변하면 ESC (멈춤 풀기)
H=$(cd "$(dirname "$0")" && pwd); . "$H/env.sh"
NAME=$1; export DISPLAY=$2; N=$3; PID=$4
LOG=$OUT/$NAME.log; D=$OUT/$NAME; mkdir -p $D
xdotool keydown ctrl
last=""; same=0
for i in $(seq 1 $N); do
  kill -0 $PID 2>/dev/null || { echo "DIED at $i" >> $D/events.txt; tail -30 $LOG >> $D/events.txt; break; }
  r=$((RANDOM % 10))
  if [ $((RANDOM % 25)) -eq 0 ]; then xdotool mousemove 885 687 click 1
  elif [ $r -lt 6 ]; then xdotool mousemove 640 620 click 1
  elif [ $r -lt 8 ]; then for k in $(seq 0 $((RANDOM % 21))); do xdotool key Down; done; xdotool key Return
  else xdotool mousemove $((320 + RANDOM % 640)) $((240 + RANDOM % 480)) click 1; fi
  sleep 0.05
  if [ $((i % 200)) -eq 0 ]; then
    mes=$(grep -ao "load MES [A-Za-z0-9_]*" $LOG | tail -1)
    sz=$(stat -c %s $LOG)
    import -window root -crop $SHOT_CROP -resize 50% $D/s$(printf %06d $i).png
    echo "$i $mes $sz" >> $D/progress.txt
    if [ "$sz" = "$last" ]; then same=$((same+1)); else same=0; fi
    last=$sz
    if [ $same -ge 3 ]; then echo "STUCK at $i $mes" >> $D/events.txt; xdotool key Escape; same=0; fi
  fi
done
xdotool keyup ctrl
echo "END $i" >> $D/events.txt
