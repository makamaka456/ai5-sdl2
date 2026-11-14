# ai5-sdl2 코드 구조 (정찰 결과)

1단계 정찰에서 확인한 내용. 새 게임(꽃과뱀)을 붙일 때 어디를 고쳐야 하는지 정리했다.

## 큰 그림

```
ai5-sdl2/
  src/                엔진 본체 (SDL2로 화면·소리·입력 처리)
    main.c            시작점. 게임 판별, 설정 읽기, 초기화
    vm.c              스크립트(.MES) 실행기
    gfx.c, text.c     화면·글자 그리기
    yuno.c 등         게임 전용 파일 (게임 1개 = 파일 1개 또는 폴더 1개)
    isaku/, shuusaku/ 큰 게임은 폴더로 나뉨
  include/game.h      게임 전용 코드가 채워야 하는 틀(struct game)
  subprojects/libai5/ 파일 형식 라이브러리 (아카이브·그림·스크립트 해석)
    include/ai5/game.h  게임 ID 목록(enum ai5_game_id)
    src/game.c          게임 ID ↔ 이름 표
    src/mes/codes.c     게임별 스크립트 명령어 표
    src/mes/parse*.c    스크립트 해석기 (AI5용, AIWIN용 두 가지)
  meson.build         빌드 설정 (새 .c 파일은 여기에 추가)
```

## 게임이 등록되는 곳 (4군데)

새 게임을 넣으려면 아래 네 곳을 모두 손대야 한다.

| 순서 | 파일 | 하는 일 |
|---|---|---|
| 1 | `subprojects/libai5/include/ai5/game.h` | `enum ai5_game_id`에 게임 ID 추가 (예: `GAME_HANAHEBI`) |
| 2 | `subprojects/libai5/src/game.c` | `ai5_games[]` 표에 `{ "hanahebi", GAME_HANAHEBI, "花と蛇" }` 추가 |
| 3 | `subprojects/libai5/src/mes/codes.c` | 그 게임의 스크립트 명령어 표(`mes_sys_*`, `mes_util_*`) 연결 |
| 4 | `src/main.c` | `set_game()`에서 `game = &game_hanahebi` 연결, `set_game_from_config()`에서 INI의 제목으로 자동 판별 |

그리고 `src/hanahebi.c`(게임 전용 파일)를 새로 만들고 `meson.build`의 `sources`와 `include/game.h`의 `extern` 목록에 추가한다.

## 게임 판별 방식 (`src/main.c`)

1. 게임 폴더에서 `AI5ENG.INI` → `AI5WIN.INI` → `syuusaku.ini` → `aiwin.ini` 순서로 찾는다.
2. INI 안의 제목(`config.title`)을 보고 게임을 정한다. 예: `遺作９８` → isaku, `臭作` → shuusaku.
3. 제목으로 못 정하면 `--game=<ID>` 옵션이 필요하다.
4. `AI5SDL2.INI`가 있으면 추가 설정으로 읽는다. (런처가 게임별 설정을 넘길 때 쓸 수 있다)

→ 꽃과뱀은 `AI5WIN.INI`의 제목 문자열을 확인해서 2번에 한 줄 추가하면 된다. (제목은 게임 파일을 받아야 확인 가능 — 아직 모름)

## 게임 전용 파일이 채우는 것 (`struct game`, `include/game.h`)

| 항목 | 뜻 |
|---|---|
| `id` | 게임 ID |
| `view`, `surface_sizes` | 화면 크기, 그림판(서피스) 크기 |
| `bpp` | 색 깊이 (8=256색, 16, 24) |
| `vm` | 스크립트 실행 방식: `VM_AI5` 또는 `VM_AIW` |
| `init`, `update`, `handle_event` | 시작·매 프레임·입력 처리 시 부를 함수 |
| `draw_text_zen/han` | 전각/반각 글자 그리기 (한글 표시를 손댈 곳 후보) |
| `stmt_op`, `expr_op`, `sys`, `util` | 스크립트 명령어 → 함수 연결 표 |
| `flags` | 게임마다 다른 플래그 번호 연결 |

## 현재 지원 게임과 파일

| 게임 | ID | 전용 파일 | 줄 수 |
|---|---|---|---|
| YU-NO | yuno | `src/yuno.c` + `src/classics.c` | 약 1070 |
| 동급생 | doukyuusei | `src/doukyuusei.c` + `map.c` | 약 1480 |
| 동급생2 | doukyuusei2 | `src/doukyuusei2.c` (+ 팔레트 표) | 약 1210 |
| 하급생 | kakyuusei | `src/kakyuusei.c` + `dungeon.c` | 약 1190 |
| 유작 리뉴얼 | isaku | `src/isaku/` | 약 1080+ |
| 취작 | shuusaku | `src/shuusaku/` (AIWIN 방식) | 약 6000 |
| 아이시마이 | aishimai | `src/aishimai.c` | 약 1320 |
| 샹그릴라, Be-Yond | shangrlia, beyond | 디버그 빌드에서만 켜짐 (미완성) | |

## 꽃과뱀 작업에 대한 메모 (추측 포함)

- **확인한 것**: libai5에 게임 ID가 18개 있고, 2003년 이후 작품(河原崎家, 野々村, 鬼作 애니판, 동급생2 DL판)도 ID는 있다. 꽃과뱀(2005)은 ID가 없다.
- **추측**: 꽃과뱀은 2005년작이라 시기가 가장 가까운 `nonomura`(2003)·`kisaku-anim`(2011) 쪽 스크립트 형식에 가까울 수 있다. 하지만 이 둘은 ai5-sdl2에 실행 모듈이 없다. 실행 모듈이 있는 것 중 출발점은 계획서대로 유작(`src/isaku/`) 또는 취작(`src/shuusaku/`)으로 하고, 게임 파일을 받으면 실제 스크립트를 열어 보고 정한다.
- libai5에는 스크립트를 글로 풀어 보는 도구(`mes` 인쇄 기능)가 있어서, 꽃과뱀 스크립트 분석에 쓸 수 있다.
