#!/bin/bash
# 인물 엔딩 시험: 최종장 FAS062에서 인물 p(0~3)·선택지 k로 엔딩까지 → $OUT/endings.txt
# 사용: endrun.sh <이름> <DISPLAY> <p> <k> "<HH_DEBUG_POKE16 값, 예 0x2180:100>"
H=$(cd "$(dirname "$0")" && pwd); . "$H/env.sh"
NAME=$1; D=$2; P=$3; K=$4; POKE=$5; L=$(printf 'ABCD' | cut -c$((P+1)))
cd "$GAME"
DISPLAY=$D SDL_AUDIODRIVER=dummy AI5_LOG_MES=1 AI5_REDIRECT_MES="main.mes=FAS062.mes" \
 HH_DEBUG_SET="785:9,1655:1,1657:1,1659:1,1661:1" HH_DEBUG_POKE16="$POKE" \
 HH_DEBUG_CHOICE="FAS062=$P,FAS064$L=$K,FAS065=1" ${TESTBIN:-$OUT/ai5-test} . > $OUT/$NAME.log 2>&1 < /dev/null &
pid=$!; sleep 2
"$H/play.sh" $NAME $D 200000 $pid >/dev/null 2>&1 < /dev/null & cp=$!
for t in $(seq 1 1800); do
  sleep 1; kill -0 $pid 2>/dev/null || break
  if grep -q 'load MES FAS_END' $OUT/$NAME.log && [ $(grep -c 'load MES FAS062.mes' $OUT/$NAME.log) -ge 2 ]; then break; fi
done
kill $cp 2>/dev/null; kill -9 $pid 2>/dev/null
res=$(grep -o 'load MES FAS_END[^ ]*\|load MES FAS06[56][^ ]*' $OUT/$NAME.log | awk '{print $3}' | tr '\n' ' ')
echo "$NAME p=$P k=$K t=${t}s errors=$(grep -ciE 'error|fatal' $OUT/$NAME.log) : $res" >> $OUT/endings.txt
