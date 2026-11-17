---
name: elf-setup
description: 새 세션(새 방)에서 elf 게임 런처·꽃과뱀 작업을 시작할 때 작업 공간을 준비한다. 빌드 도구 설치, 서브모듈·빌드, 비공개 저장소의 게임 파일 받기, 가상 화면과 시험용 엔진 준비. "이어서 작업", "새 방", "작업 공간 준비" 또는 build/ai5나 /home/user/gamedata가 없을 때 쓴다.
---

# 작업 공간 준비

1. 먼저 읽기: `docs/handoff-hanahebi-ko.md`(인계·노하우·할 일) → `CLAUDE.md`(규칙·상세 기록). 사용자에게는 한국어로만 말한다.
2. 빌드
   ```
   cd /home/user/ai5-sdl2
   apt-get install -y meson libsdl2-dev libsdl2-ttf-dev libsndfile1-dev libavcodec-dev libavformat-dev libswscale-dev xdotool imagemagick xvfb fonts-nanum
   git submodule update --init --recursive && meson setup build && ninja -C build
   mkdir -p /usr/local/share/ai5-sdl2 && ln -sfn $PWD/fonts /usr/local/share/ai5-sdl2/fonts
   ```
   - 서브모듈 libai5는 `makamaka456/libai5` 브랜치 `claude/lucid-tesla-qpa0cp`.
3. 비공개 저장소 `makamaka456/elf-games-private`를 `/home/user/elf-games-private`에 클론(없으면 저장소 추가 도구로 붙인다).
4. 게임 파일: 비공개 저장소 **릴리즈 첨부**에서 받는다.
   ```
   gh api repos/makamaka456/elf-games-private/releases --jq '.[]|{tag_name,assets:[.assets[]|{id,name}]}'
   gh api -H "Accept: application/octet-stream" repos/makamaka456/elf-games-private/releases/assets/<id> > /home/user/gamedata/games-hanahebi.zip
   unzip -q -O cp949 /home/user/gamedata/games-hanahebi.zip -d /home/user/gamedata/hanahebi
   ```
   - zip의 맨 위 폴더 구조에 따라 `/home/user/gamedata/hanahebi/HH2D.exe`가 있게 맞춘다.
   - **게임 파일과 거기서 꺼낸 것은 공개 저장소에 절대 커밋 금지.**
5. 시험 준비: `tools/test/setup.sh 89` (Xvfb :89 1280x960 + `/tmp/hh-test/ai5-test` 사본).
   - 빠른 확인: `cd /home/user/gamedata/hanahebi && DISPLAY=:89 SDL_AUDIODRIVER=dummy /tmp/hh-test/ai5-test .` → 몇 초 뒤
     `DISPLAY=:89 import -window root -crop 640x480+320+240 /tmp/hh-test/title.png` 로 타이틀 스크린샷을 눈으로 확인.
6. 필요할 때만: 분석 도구(mesdis·arcdump·cginfo) 컴파일, Wine 원본 준비 → `tools/re/README-ko.md`.
