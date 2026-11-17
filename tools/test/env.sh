# 공통 설정 (다른 스크립트가 source 함). 필요하면 실행 전에 환경 변수로 바꾼다.
: ${GAME:=/home/user/gamedata/hanahebi}          # 꽃과뱀 게임 폴더 (한글패치판)
: ${REPO:=/home/user/ai5-sdl2}
: ${BIN:=$REPO/build/ai5}                       # 시험할 엔진 (오래 도는 시험은 사본을 쓰는 것이 안전: setup.sh가 만듦)
: ${OUT:=/tmp/hh-test}                          # 로그·스크린샷
mkdir -p "$OUT"
# 공략 경로 선택지 (2DFan 케이코 루트, 대본별 defmenu 번호)
: ${ROUTE_CHOICE:="FAS000=0,FAS001=0,FAS003=0,FAS020=0,FAS021=0,FAS022=0,FAS023=1,FAS024=0,FAS037=0,FAS038=0,FAS042=0,FAS058=1,FAS060=2,FAS062G=0,FAS065=1,FASMAP16=0"}
# 게임 창은 Xvfb 1280x960 화면의 (320,240)에 640x480으로 뜬다
SHOT_CROP=640x480+320+240
