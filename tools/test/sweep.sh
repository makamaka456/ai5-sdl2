#!/bin/bash
# 장면 대본을 하나씩 바로 시작해 무작위 진행 → 죽거나 멈추는 대본 찾기
# 사용: sweep.sh <대본 목록 파일> [DISPLAY=:89]   (목록: 대본 파일 이름 한 줄에 하나. mesdis용으로 mes.arc를 풀어 둔 폴더에서 ls)
H=$(cd "$(dirname "$0")" && pwd); . "$H/env.sh"
export DISPLAY=${2:-:89}; S=$OUT/sweep; mkdir -p $S
for f in $(cat "$1"); do
  name=${f%.*}; [ -f $S/$name.done ] && continue
  cd "$GAME"
  AI5_REDIRECT_MES="main.mes=$f" HH_DEBUG_MAP_RANDOM=1 SDL_AUDIODRIVER=dummy ${TESTBIN:-$OUT/ai5-test} . > $S/$name.log 2>&1 < /dev/null &
  pid=$!; sleep 3
  xdotool keydown ctrl
  for i in $(seq 1 450); do
    kill -0 $pid 2>/dev/null || { echo "DIED at $i" > $S/$name.died; break; }
    r=$((RANDOM % 10))
    if [ $r -lt 6 ]; then xdotool mousemove 640 620 click 1
    elif [ $r -lt 8 ]; then for k in $(seq 0 $((RANDOM % 21))); do xdotool key Down; done; xdotool key Return
    else xdotool mousemove $((320 + RANDOM % 640)) $((240 + RANDOM % 480)) click 1; fi
    sleep 0.05
  done
  xdotool keyup ctrl; kill -9 $pid 2>/dev/null; touch $S/$name.done
done
grep -lE "error|fatal" $S/*.log; ls $S/*.died 2>/dev/null
