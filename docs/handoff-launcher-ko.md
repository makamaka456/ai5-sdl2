# 인계: 런처 공통 작업 (2026-11-16 사용자와 합의, 다른 방에서 진행)

꽃과뱀 작업 방에서 정한 런처 공통 변경 3가지. 아직 **하나도 구현하지 않았다**.
진행 규칙·저장소 규칙은 `CLAUDE.md`를 따른다(한국어 보고, 눈으로 확인한 증거, 원본 파일 커밋 금지 등).

## 1. 저장 위치: 게임 폴더 안의 `launcher-save/`
- 사용자 요청: 런처에서 지정한 각 게임 폴더 안에 `launcher-save` 폴더를 만들고 거기에 저장.
  원본 PC판 세이브와 섞이거나 덮어쓰지 않게 하려는 것(원본 세이브 호환은 필요 없음).
- 지금 상태
  - 꽃과뱀: `src/hanahebi.c`의 `#define SAVE_DIR "SAVEHANAHEBI"` + `save_path()` 하나로 모든 파일
    (FLAG%03u, SaveData%03u.dat, CG%03u.dat, MesSkip.dat, ITEM%03u.dat, CONFIG.TXT)을 씀.
    대소문자 무시(`path_get_icase`)라 원본의 `savehanahebi` 폴더를 그대로 써서 원본 세이브와 섞인다.
  - 다른 게임: `src/savedata.c`가 대본이 준 이름(예: FLAG00)을 게임 폴더(현재 디렉터리) 기준으로 읽고 씀
    (`file_open_utf8(save_name, ...)`, `path_get_icase(save_name)`). 유작은 `src/isaku/isaku.c`에도 저장 코드가 있음.
- 할 일(제안)
  - 엔진에 저장 폴더 설정 하나: 환경 변수 `AI5_SAVE_DIR`(예: "launcher-save"). 있으면 모든 저장 파일 경로 앞에 붙이고 폴더가 없으면 만든다(`mkdir_p`).
  - 꽃과뱀은 `SAVE_DIR` 대신 이 값을 쓰기(없으면 지금처럼 SAVEHANAHEBI).
  - `src/savedata.c`, `src/isaku/isaku.c` 등 게임별 저장 코드의 파일 열기를 한 함수로 모아 같은 규칙 적용.
  - 자바 `GameActivity.getArguments()`에서 `nativeSetenv("AI5_SAVE_DIR", "launcher-save")`.
  - 이미 폰에 저장한 기록은 새 폴더에서 안 보임 → 사용자에게 안내(필요하면 파일 옮기기).
- 확인: 게임마다 저장 → 끄기 → 다시 켜서 불러오기, `launcher-save/`에 파일이 생기는지, 원본 세이브 폴더가 그대로인지.

## 2. 게임별 [설정] 버튼 (지금의 [배경화면] 버튼 대신)
- 사용자 요청: 런처 목록의 각 게임 줄에 [설정] 버튼. 그 안에서 그 게임만의 화면 비율, 배경화면, 키맵핑,
  가상 버튼 설정 등을 따로 한다.
- 지금 상태 (`android/app/src/main/java/org/ai5sdl2/launcher/`)
  - `LauncherActivity`: 각 줄 [배경화면] 버튼(그림 → `bg_<게임>.png`), 화면 위쪽 공통 "꽉 채우기"·"게임패드 설정".
  - `AppSettings`: SharedPreferences "launcher"(두 프로세스 공용). 키: `stretch`, `pad.*`(게임패드), `padpreset.*`,
    `btn.<id>`(가상 버튼 "x,y,alpha,size,hidden"), `btnpreset.*`. **모두 게임 구분 없이 공통.**
  - `GameActivity`: 게임 중 ⚙ = 화면 설정 / 가상 버튼 편집(적용·프리셋) / 키맵핑(바로 적용) / 끝내기.
    `EXTRA_GAME`으로 게임 id를 받음(배경화면만 게임별).
- 할 일(제안)
  - 설정 키를 게임별로: `g.<게임id>.stretch`, `g.<게임id>.pad.*`, `g.<게임id>.btn.*` … 없으면 공통 기본값.
    프리셋은 공통으로 두어 게임끼리 나눠 쓰기.
  - 런처 [설정] 화면: 화면 비율, 배경화면 고르기/없애기, 가상 버튼 편집(게임 없이 미리보기 화면에서 하거나 "게임 중 ⚙에서 편집" 안내),
    키맵핑, 언어(아래 3번), "다른 게임 설정 가져오기".
  - 게임 중 ⚙도 그 게임의 설정을 읽고 쓰기.
  - 엔진 쪽은 그대로(`AI5_STRETCH`, `AI5_BACKGROUND`, `AI5_CONTROLLER`는 매번 런처가 넘김; AI5_CONTROLLER는 실행 중 바뀌면 다시 읽음).
- 확인: 에뮬레이터 스모크 테스트(`android/smoke-test.sh`)에 런처 설정 화면 캡처 추가.

## 3. 한글판·일본판 목록 합치기
- 사용자 의견: 참고 APK는 실행 파일을 골라서 한글판/일본판 항목이 따로였지만, 우리는 폴더를 고르므로 나눌 필요 없음.
- 지금 상태: `LauncherActivity.GAMES`에 `isaku`/`isaku-korean`, `shuusaku`/`shuusaku-korean`,
  `doukyuusei2`/`doukyuusei2-korean`처럼 따로 있고 `--game=<id>`로 실행. 꽃과뱀만 하나(자동 감지).
- 할 일(제안)
  - 게임당 한 줄. 게임 설정에 "언어: 자동(기본) / 한글판 / 일본판".
  - 자동 감지 규칙(게임마다 확인 필요)
    - 꽃과뱀: 엔진이 대본 글자 통계로 판단(확인됨, `main.c hanahebi_text_is_cp949`)
    - 유작: `Isaku98.ini` 또는 대사 아카이브 이름 `.KOR`(확인됨)
    - 취작·동급생2 등: 아직 모름 → 실제 파일(비공개 저장소 릴리즈 `Shusaku`, `games-Doukyuusei_2`)로 확인
  - 자동이 틀리면 설정에서 직접 고르면 `--game=<id>-korean`/`--game=<id>`로 실행.

## 같이 처리할 다른 게임 문제 (CLAUDE.md "다른 게임 지원 때 검토할 것"과 같음)
- 유작 안드로이드 `SDL_CreateWindow: Android only supports one window` → 아이템 창은 덮개로 바꿈(폰 확인 필요),
  오른쪽 클릭 메뉴(`popup_menu.c`)·취작 일정표/상태 창(`shuusaku/schedule.c`, `status.c`)도 화면 안으로 다시 그려야 함.

## 이미 한 공통 입력 변경 (2026-11-16, 꽃과뱀 방에서)
- 터치: 탭=손 뗄 때 왼쪽 클릭, 0.5초 누르기=오른쪽 클릭, 끌기, 두 손가락 탭=오른쪽 클릭 (`src/input.c` touch_*)
- 스틱 커서(안드로이드): 엔진 자체 화살표 커서 (`cursor_virtual_move/off/pos`, `gfx_draw_virtual_cursor`). 다른 게임에서도 같은 방식. 폰 확인 필요

## 2026-11-16 꽃과뱀 방에서 처리함 (사용자 직접 요청)
- 2번(게임별 [설정] 버튼) 구현: 런처 줄마다 [설정] — 화면 비율 16:9/4:3, 배경화면, 가상 버튼 표시 목록·처음대로, 키맵핑. 게임별 키 `g.<게임>.*`
- 가상 버튼에 방향키·PgUp/PgDn 추가, 숨기기는 [표시할 버튼] 체크 목록으로
- 남은 것: 1번(launcher-save 저장 폴더), 3번(한글판·일본판 목록 합치기, 언어 설정)
