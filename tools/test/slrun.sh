#!/bin/bash
# 처음부터 엔딩까지 공략 경로로 진행하며 7번째 저장 지점마다 저장/불러오기 화면 비교
# 사용: slrun.sh [DISPLAY=:89]   결과: $OUT/sl.log ("save/load check ... differs", 큰 차이면 "screen records:")
#       차이 그림: $OUT/slimg/<대본>_<지점>_a.bmp(보통 진행) / _b.bmp(불러온 뒤)
H=$(cd "$(dirname "$0")" && pwd); . "$H/env.sh"
D=${1:-:89}; mkdir -p $OUT/slimg; rm -f $OUT/slimg/*
cd "$GAME"
DISPLAY=$D SDL_AUDIODRIVER=dummy AI5_LOG_MES=1 AI5_REDIRECT_MES="main.mes=FAS000.mes" HH_DEBUG_SAVELOAD=7 \
  HH_DEBUG_SAVELOAD_DIR=$OUT/slimg HH_DEBUG_ROUTE=1 HH_DEBUG_CHOICE="$ROUTE_CHOICE" \
  ${TESTBIN:-$OUT/ai5-test} . > $OUT/sl.log 2>&1 < /dev/null &
pid=$!; sleep 2
exec "$H/play.sh" sl $D 2000000 $pid
