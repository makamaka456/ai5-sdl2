#!/bin/bash
# 공략 경로로 진행해 <대본>의 저장 지점 <n>에서 칸 <slot>에 저장하고 끝냄 (원본 HH2D.exe에서 불러와 비교용)
# 사용: mksave.sh FAS022.MES 40 3 [DISPLAY=:90]   (같은 대본을 여러 번 지나면 HH_DEBUG_SAVENTH=k)
H=$(cd "$(dirname "$0")" && pwd); . "$H/env.sh"
D=${4:-:90}; cd "$GAME"
DISPLAY=$D SDL_AUDIODRIVER=dummy AI5_LOG_MES=1 AI5_REDIRECT_MES="main.mes=FAS000.mes" HH_DEBUG_ROUTE=1 \
  HH_DEBUG_SAVEMES="$1" HH_DEBUG_SAVELOAD="!$2:$3" HH_DEBUG_CHOICE="$ROUTE_CHOICE" \
  ${TESTBIN:-$OUT/ai5-test} . > $OUT/mk.log 2>&1 < /dev/null &
pid=$!; sleep 2
"$H/play.sh" mk $D 2000000 $pid
grep -a saved $OUT/mk.log
