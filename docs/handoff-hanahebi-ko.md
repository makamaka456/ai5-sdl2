# 인계: 꽃과뱀·런처 작업 (2026-11-17, v0.5 릴리즈 시점)

새 방(세션)에서 이어서 일할 사람을 위한 문서. **먼저 이 문서 → `CLAUDE.md` 순서로 읽는다.**
`CLAUDE.md`는 규칙과 기능별 상세 기록(원본 주소·변수 번호), 이 문서는 "어떻게 일하는지"와 "다음에 할 일"이다.

## 1. 사용자와 일하는 방식 (꼭 지킬 것)
- 사용자는 영어를 못 한다. **보고·질문·커밋 설명 모두 한국어**, 전문 용어는 처음 쓸 때 한 줄로 풀어 쓴다.
- 허락이 꼭 필요한 일이 아니면 **멈추지 말고 계속 진행**. 보고는 짧게, 일하면서.
- 보고 형식: 한 일 / 확인 결과(스크린샷) / 남은 문제 / 다음 단계.
- "됐다"는 **눈으로 본 증거(스크린샷)** 가 있을 때만. 빌드 성공은 완료가 아니다. 확인한 것과 추측을 나눠 말한다.
- 오래 걸리는 시험은 미리 "몇 분/몇 시간 걸린다"고 말하고, 중간중간 "지금 뭐 하는지"를 한 줄로 알려 준다
  (사용자가 "지금 뭐 하고 있는 거야?"라고 물은 적 있음).
- **APK는 사용자가 말할 때만 빌드**한다. 평소 커밋 메시지 끝에 `[skip ci]`를 붙여 Actions가 안 돌게 한다.
- 게임 원본·한글패치·참고 APK와 거기서 꺼낸 파일(대본 풀이 결과, 스크린샷 원본 등)은 **절대 공개 저장소에 커밋하지 않는다.** 커밋 전 `git status`.

## 2. 저장소 세 개
| 저장소 | 브랜치 | 내용 |
|---|---|---|
| `makamaka456/ai5-sdl2` (공개) | `claude/lucid-tesla-qpa0cp` | 엔진(`src/`), 꽃과뱀 모듈 `src/hanahebi.c`, 안드로이드 `android/`, 도구 `tools/`, 문서 |
| `makamaka456/libai5` (공개, 서브모듈 `subprojects/libai5`) | `claude/lucid-tesla-qpa0cp` | 대본 해석 라이브러리. GAME_HANAHEBI, CP949, 꽃과뱀 sysvar 표 |
| `makamaka456/elf-games-private` (비공개) | main | 게임 원본(릴리즈 첨부 zip), 분석 메모 `notes/hanahebi-analysis.md`, Ghidra 결과 `notes/ghidra/HH2D.decompiled.c`, 참고 APK 분석 `notes/apk-analysis.md` |

- 비공개 저장소는 읽기 위주. 지우거나 덮어쓸 일이 있으면 먼저 사용자에게 묻는다.
- libai5를 고치면 libai5 브랜치에 먼저 커밋·푸시하고, ai5-sdl2에서 서브모듈 커밋을 올린다.

## 3. 새 작업 공간 준비 (약 10~20분)
```
cd /home/user/ai5-sdl2
apt-get install -y meson libsdl2-dev libsdl2-ttf-dev libsndfile1-dev libavcodec-dev libavformat-dev libswscale-dev xdotool imagemagick xvfb fonts-nanum
git submodule update --init --recursive && meson setup build && ninja -C build
mkdir -p /usr/local/share/ai5-sdl2 && ln -sfn $PWD/fonts /usr/local/share/ai5-sdl2/fonts
# 비공개 저장소 클론: /home/user/elf-games-private
# 게임 파일(비공개 저장소 릴리즈 첨부): games-hanahebi.zip (꽃과뱀 한글패치판), isaku98.zip(유작 한글판), 동급생2·취작
gh api repos/makamaka456/elf-games-private/releases --jq '.[]|{tag_name,assets:[.assets[]|{id,name}]}'
gh api -H "Accept: application/octet-stream" repos/makamaka456/elf-games-private/releases/assets/<id> > /home/user/gamedata/games-hanahebi.zip
unzip -q -O cp949 /home/user/gamedata/games-hanahebi.zip -d /home/user/gamedata/hanahebi
```
- 분석 도구(대본 풀기 `mesdis`, 아카이브 꺼내기 `arcdump`, 그림 정보 `cginfo`) 컴파일: `tools/re/README-ko.md`.
- 원본 비교용 Wine 준비(i386 wine, pulseaudio null 출력, 레지스트리): `tools/re/README-ko.md` 끝.
- Ghidra 결과는 이미 비공개 저장소에 있어 보통 다시 돌릴 필요 없음(`fn.sh 주소`로 함수 보기).
- 원본 실행 파일 기계어 보기: `objdump -D -b binary -m i386 --adjust-vma=0x400000 --start-address=0x... --stop-address=0x... <HH2D 메모리 이미지>`
  (메모리 이미지는 섹션을 가상 주소대로 펼친 파일. 없으면 Ghidra 결과에서 찾거나 PE 섹션을 펼쳐 만든다)

## 4. 시험 도구 (`tools/test/`, 2026-11-17 정리)
모든 스크립트는 `env.sh`의 `GAME`(게임 폴더)·`OUT`(=/tmp/hh-test, 로그·그림)·`ROUTE_CHOICE`(공략 선택지)를 쓴다.

| 스크립트 | 하는 일 |
|---|---|
| `setup.sh [89]` | 가상 화면 Xvfb :89(1280x960)와 시험용 엔진 사본 `$OUT/ai5-test` 준비. 게임 창은 화면 (320,240)에 뜸 |
| `play.sh` | 무작위 입력으로 계속 진행(Ctrl 누름=대사 넘김, 긴 메뉴 아래 항목·"방에서 나가기"도 가끔 누름, 멈추면 ESC) |
| `slrun.sh [:89]` | **처음~엔딩 공략 경로 + 7번째 저장 지점마다 저장/불러오기 화면 비교** (약 50분에 엔딩 1개). 결과 `$OUT/sl.log`의 `save/load check ... differs`, 큰 차이면 `screen records:`(그때 화면 기록 목록), 그림 `$OUT/slimg/*_a.bmp`(보통 진행)·`_b.bmp`(불러온 뒤) |
| `mksave.sh FAS022.MES 40 3` | 공략 경로로 가서 그 지점에서 칸 3에 저장하고 끝 → 원본에서 불러와 비교 |
| `cmp.sh 엔진 :91 폴더 FAS000.mes 150` | 한 대본을 바로 시작해 지점별 스크린샷. 엔진 두 개(고치기 전·후)를 다른 화면에서 동시에 돌려 비교 |
| `sweep.sh 목록 [:89]` | 장면 대본을 하나씩 바로 시작해 무작위 진행 → 죽거나 오류 나는 대본 찾기 |
| `endrun.sh` | 최종장에서 인물·선택지·수치를 정해 인물 엔딩까지 진행 |
| `wine-orig.sh [:99]` | 원본 HH2D.exe를 Wine으로 실행. 클릭 좌표(이어하기·칸·決定·メニュー)는 스크립트 주석에 |

엔진의 시험 스위치(환경 변수, 자세한 뜻은 `CLAUDE.md`):
- `AI5_REDIRECT_MES="main.mes=FAS022.mes"` 대본 바꿔 시작, `AI5_LOG_MES=1` 대본·저장 지점 로그, `AI5_TRACE=1` 명령 한 줄씩
- `HH_DEBUG_ROUTE=1` 지도에서 공략 방 순서, `HH_DEBUG_CHOICE="FAS022=0,…"` 선택지 고정, `HH_DEBUG_MAP_RANDOM=1`
- `HH_DEBUG_SAVELOAD=7` / `@n` / `!n:칸`(+`HH_DEBUG_SAVEMES`, `HH_DEBUG_SAVENTH`), `HH_DEBUG_SAVELOAD_DIR`
- `HH_DEBUG_SET="785:5"`(var4), `HH_DEBUG_POKE="0x516:1"`(바이트), `HH_DEBUG_POKE16`, `HH_DEBUG_UNLOCK=1`, `HH_DEBUG_ITEM`, `HH_DEBUG_CALL`, `HH_MAP_START`, `HH_DEBUG_SPOTS=1`

## 5. 일하면서 얻은 노하우
### 원본과 맞추는 법
- **추측하지 말고 원본 기계어를 본다.** Util 분기는 `FUN_00421bd0`(분기표 0x425038은 1002부터), 화면 표시 [10]은 0x450620.
  Ghidra 디컴파일은 인자를 자주 빠뜨리므로(예: Util 50의 5·6이 같은 함수로 보이지만 인자 1/0이 다름) **호출 직전 `push`를 objdump로 확인**한다.
- **Wine 원본과 나란히 비교**가 가장 확실하다. 우리 엔진으로 `!n:칸` 저장 → 원본에서 불러오기 → 몇 번 클릭하며 스크린샷.
  원본이 쓴 저장도 우리 엔진으로 불러올 수 있다(파일 형식 같음). 화면 기록(SaveData)을 직접 고쳐 넣어 원본 반응을 보는 것도 유효했다.
  - 원본 저장 목록(FLAG000의 0x27c0+(칸−1)×0x38)이 비어 있으면 원본 불러오기 화면에 안 보인다. 다른 저장을 만들면 목록이 덮이니 필요하면 항목을 복사해 채운다.
- 시험용 게임 폴더의 저장 폴더(`savehanahebi`)는 시험 전에 백업하고 끝나면 되돌린다.

### 지난 버그에서 배운 것
- 그림 파일(.MAP, A00_*.A6)은 **아래 줄부터 저장**(BMP처럼). 이상하게 보이면 뒤집힘부터 의심.
- 대본 글자가 깨져 보이면 CP949를 SJIS로 읽은 것(풀이 도구 출력도 마찬가지). 대사는 엔진 화면에서 확인하는 게 빠르다.
- 화면 기록은 번호 순서로 재생된다 → 나중 번호(지도 17)가 앞 번호(인물 2·13)를 덮을 수 있다.
- "보통 진행 화면 ≠ 불러온 화면"인 차이 중 상당수는 **원본도 같다**(기록이 그렇게 남음). 고치기 전에 원본으로 꼭 확인.
- 엔진 사본을 실행 중에 덮어쓰면 "Text file busy" → 먼저 프로세스 종료.
- 여러 게임을 동시에 돌리면 메모리가 모자라 프로세스가 죽을 수 있다(동시 2~3개까지).
- 작업 공간 하네스는 `sleep` 연속 사용을 막는다 → 백그라운드 실행 + until 반복으로 기다린다.
- 작업 공간에서는 **git 태그 푸시가 막혀 있다** → 릴리즈는 Actions 손 실행(아래 7).

### 자동 시험의 한계
- 무작위 입력은 작은 버튼(조사 화면 "방에서 나가기"), 긴 메뉴 아래쪽 항목을 거의 못 눌러 몇 시간씩 맴돈 적이 있다(지금은 고침).
  진행이 멈춘 듯하면 `$OUT/<이름>/progress.txt`와 화면 스크린샷을 본다.
- Ctrl로 넘기면 대사가 안 그려져 저장/불러오기 비교가 거의 안 된다(비교는 첫 대사 화면에서 함).

## 6. 지금 상태 (v0.5)
- 꽃과뱀: 타이틀~엔딩, 공략 경로·인물 엔딩 12개·하렘 엔딩 자동 시험 오류 0. 저장/불러오기 전체 비교(5000회 이상)에서 남은 차이는 원본과 같은 유형만.
- 안드로이드 런처: 게임별 [설정], 가상 버튼 편집, 게임패드, 터치 커서, 회상 모두 열기. v0.5 정식 릴리즈
  https://github.com/makamaka456/ai5-sdl2/releases/tag/v0.5 (고정 최신 빌드: `android-latest`)
- **폰 실기 확인은 아직 사용자 몫** (터치·스틱 커서, v0.5 전체).

## 7. 릴리즈 만드는 법
1. `android/app/build.gradle`의 versionCode·versionName 올리기, `android/release-notes.md` 고치기
2. 커밋·푸시 (`[skip ci]` 없이 → 브랜치 빌드가 `android-latest`를 갱신)
3. 그 빌드가 끝난 뒤 Actions `Android` 워크플로를 손으로 실행(GitHub 도구 `actions_run_trigger`, ref=브랜치, 입력 `release_tag=v0.6`)
   → Actions가 태그를 만들고 정식 릴리즈에 `ai5-launcher.apk`를 올림 (같은 브랜치 실행끼리는 나중 것이 앞 것을 취소하니 순서대로)
4. `get_release_by_tag`로 APK 첨부 확인 후 주소를 사용자에게 알림

## 8. 앞으로 할 일 (우선순위 순, 제안)
1. **사용자 폰 확인 대응**: v0.5 설치 후 사용자가 보내는 스크린샷·문제부터 처리.
2. **런처 공통 작업**(`docs/handoff-launcher-ko.md`): ① 저장 위치를 게임 폴더 안 `launcher-save/`로(지금 꽃과뱀은 `SAVEHANAHEBI`라 원본 세이브와 섞임) ③ 한글판·일본판 목록 합치기. ②(게임별 [설정])는 됨. 다른 방에서 진행했을 수 있으니 코드로 먼저 확인.
3. **다른 지원작 안드로이드 오류**: 유작·취작의 추가 창(`popup_menu.c`, 취작 `schedule.c`·`status.c`)이 안드로이드에서 "창은 하나만" 오류 → 화면 안 메뉴로. 동급생2·취작 한글판 PC 확인(게임 파일은 비공개 릴리즈에 있음).
4. **꽃과뱀 남은 것**
   - 조교 미니게임 Util 66/67 (지금은 항상 자동, 직접 하면 최대 7점) — 공략에서 "고평가"라는 부분
   - call()로 부른 작은 장면 안에서 저장/불러오기 → 장면 끝에서 게임이 끝남 (원본 동작부터 Wine으로 확인)
   - 원본식 평면도 클릭 이동(MAPA01/02), System[28](2)(메뉴 코드 3) 미확인, Util 1005/1006은 지금 무시
   - 시스템 메뉴 "상태" 화면(Util 7000)은 글자 화면 → 원본 그림 화면으로
5. **시험 자동화 보강**: `slrun.sh`가 엔딩 1개에서 멈추게(지금은 계속 돎), 결과 요약 스크립트, 인물별 공략 경로(지금은 케이코 루트만).

## 9. 새 방 첫 메시지 예시 (사용자가 복사해 쓰기)
> 저장소 세 개(ai5-sdl2, libai5, elf-games-private)로 이어서 작업해. 먼저 `docs/handoff-hanahebi-ko.md`와 `CLAUDE.md`를 읽고, 작업 공간을 준비한 다음 "앞으로 할 일" 1번부터 진행해.
