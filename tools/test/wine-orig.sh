#!/bin/bash
# 원본 HH2D.exe를 Wine으로 실행 (비교용). 처음 한 번은 tools/re/README-ko.md의 Wine 준비를 먼저.
# 사용: wine-orig.sh [DISPLAY=:99]   그 뒤 xdotool로 조작:
#  타이틀 続行(이어하기) (620,430) / 칸 No01 (380,110) No02 (380,195) No03 (380,280) No04 (380,357) No05 (380,442) / 決定 (330,354)
#  게임 중 오른쪽 클릭 = 메뉴(セーブ (620,110), ロード (620,160)), 저장 확인 決定 (260,352), 戻る (620,532)
#  게임 화면 스크린샷: import -window root -crop 640x480+80+82 out.png
#  원본 저장 목록은 FLAG000의 0x27c0+(칸-1)*0x38 (u8 사용, u8 장, u16 연월일시분). 비어 있으면 "データがありません"
H=$(cd "$(dirname "$0")" && pwd); . "$H/env.sh"
D=${1:-:99}
pgrep -f "Xvfb $D " >/dev/null || (Xvfb $D -screen 0 1280x800x24 >/dev/null 2>&1 &)
pgrep pulseaudio >/dev/null || { pulseaudio --start --exit-idle-time=-1; pactl load-module module-null-sink >/dev/null; }
cd "$GAME"; DISPLAY=$D LANG=ja_JP.UTF-8 setsid wine HH2D.exe > $OUT/wine.log 2>&1 &
echo "약 25초 뒤 타이틀. 끝낼 때: pkill -f HH2D.exe; wineserver -k"
