# 역분석·테스트 도구 (꽃과뱀 작업용)

작업 공간이 새로 시작되면 아래를 다시 준비한다.

## 준비
```
apt-get install -y meson libsdl2-dev libsdl2-ttf-dev libsndfile1-dev libavcodec-dev libavformat-dev libswscale-dev xdotool gdb fonts-nanum
git submodule update --init --recursive && meson setup build && ninja -C build
mkdir -p /usr/local/share/ai5-sdl2 && ln -sfn $PWD/fonts /usr/local/share/ai5-sdl2/fonts
# 게임 파일 (비공개 저장소 릴리즈): hanahebi / test-data(isaku98.zip)
gh api -H "Accept: application/octet-stream" repos/makamaka456/elf-games-private/releases/assets/<id> > x.zip
#   id 목록: gh api repos/makamaka456/elf-games-private/releases --jq '.[]|{tag_name,assets:[.assets[]|{id,name}]}'
#   압축 풀기: unzip -q -O cp949 games-hanahebi.zip -d /home/user/gamedata/hanahebi
```
도구 컴파일 (libai5 정적 라이브러리 사용):
```
B=/home/user/ai5-sdl2; gcc -o arcdump arcdump.c -I$B/subprojects/libai5/include -I$B/subprojects/libai5/nulib/include $B/build/subprojects/libai5/libai5.a -lpng -lz -lm
```
- `arcdump ARC` 목록, `arcdump ARC NAME OUT` 꺼내기
- `CP949=1 mesdis hanahebi FILE.MES` 대본을 글로 풀기
- `cginfo gcc.arc NAME...` 그림 정보

## 화면 테스트 (가상 화면)
```
Xvfb :99 -screen 0 1280x800x24 &
runhh.sh <log> --korean      # 꽃과뱀 실행 (예전 프로세스 종료 후)
drive.sh c w1 sNAME          # c=클릭, wN=N초 대기, sNAME=/tmp/claude-0/ss/NAME.png 스크린샷
mont.py OUT a b c            # 스크린샷 모아 보기
AI5_TRACE=1                  # 실행되는 대본을 한 줄씩 출력
HH_DEBUG_TEXT=1              # 대사 위치·대사창 보이기/숨기기 기록
```
경로가 /tmp/claude-0/ 로 되어 있으니 필요하면 고친다.

## Ghidra (사용자 허락 받음)
- 설치: /opt/ghidra (ghidra_11.2.1_PUBLIC). 없으면 github 릴리즈에서 받는다.
- 전체 디컴파일 결과: 비공개 저장소 `notes/ghidra/HH2D.decompiled.c` (`fn.sh 주소`로 함수 보기)
- 놓친 함수: `analyzeHeadless gproj2 hh2d -import HH2D.exe` 한 번 → `dec.sh out.c 0x주소...`
