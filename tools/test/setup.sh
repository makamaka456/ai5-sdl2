#!/bin/bash
# 작업 공간 준비: 가상 화면(Xvfb), 시험용 엔진 사본. 사용: tools/test/setup.sh [화면번호=89]
H=$(cd "$(dirname "$0")" && pwd); . "$H/env.sh"
D=${1:-89}
pgrep -f "Xvfb :$D " >/dev/null || (Xvfb :$D -screen 0 1280x960x24 >/dev/null 2>&1 &)
ninja -C "$REPO/build" >/dev/null && cp "$REPO/build/ai5" "$OUT/ai5-test"
echo "DISPLAY=:$D, 엔진 사본 $OUT/ai5-test (실행 중엔 덮어쓰기 안 됨: 먼저 pkill -9 -f ai5-test)"
