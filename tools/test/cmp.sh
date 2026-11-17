#!/bin/bash
# 한 대본을 바로 시작해 천천히 클릭하며 저장 지점마다 스크린샷 (엔진 두 개를 나란히 돌려 고치기 전·후 비교용)
# 사용: cmp.sh <엔진> <DISPLAY> <출력폴더> <대본.mes> <클릭 수>   → 출력폴더/p<지점>.png
H=$(cd "$(dirname "$0")" && pwd); . "$H/env.sh"
cd "$GAME"; export DISPLAY=$2; O=$3; rm -rf $O; mkdir -p $O; L=$O/log
SDL_AUDIODRIVER=dummy AI5_LOG_MES=1 AI5_REDIRECT_MES="main.mes=$4" HH_DEBUG_CHOICE="$ROUTE_CHOICE" $1 . > $L 2>&1 < /dev/null & pid=$!; sleep 3
for i in $(seq 1 $5); do kill -0 $pid 2>/dev/null || break; p=$(grep -ao "save point [0-9]*" $L | tail -1 | awk '{print $3}'); import -window root -crop $SHOT_CROP -resize 50% $O/p$(printf %04d ${p:-0}).png; xdotool mousemove 640 620 click 1; sleep 1.3; done; kill $pid
