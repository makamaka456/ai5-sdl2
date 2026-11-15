# elf 게임 안드로이드 런처 — 작업 규칙

## 목표
- 기존 한글패치 지원 게임 + 꽃과뱀(花と蛇, 2005, AI5WIN) 같은 새 elf 게임을 고를 수 있는 안드로이드 런처 APK를 직접 빌드한다.
- 1차: 꽃과뱀이 PC 빌드에서 타이틀 → 본편까지 동작
- 2차: 같은 엔진이 안드로이드 APK로 빌드되고, 한글 표시·터치 입력이 기존 한글패치 APK 수준으로 동작
- 3차: 런처에서 게임을 골라 실행 (기존 지원작 + 새 게임)
- 별도 과제: 귀작 등 AI6WIN 게임은 엔진이 달라 이 저장소 대상이 아님

## 사용자와 소통
- **(2026-11-15 사용자 강조) 허락이 꼭 필요한 일이 아니면 절대 중간에 멈추거나 보고하려고 턴을 끝내지 말 것.** 보고는 작업하면서 짧게 하고 계속 진행한다.
- 사용자는 영어를 못 한다. **모든 보고·질문·커밋 요약 설명은 한국어로** 한다.
- 단계가 끝날 때마다 "한 일 / 확인 결과(스크린샷) / 남은 문제 / 다음 단계" 네 줄 이내로 보고한다.
- 전문 용어는 처음 쓸 때 한 줄로 풀어 쓴다.
- 같은 방법을 세 번 넘게 반복하지 않는다. 막히면 무엇이 안 되는지와 선택지 두세 개를 묻는다.
- "됐다"고 하려면 눈으로 확인한 증거(스크린샷 등)가 있어야 한다. 빌드 성공만으로는 완료가 아니다.

## 저장소 구성
| 저장소 | 공개 | 들어가는 것 | 들어가면 안 되는 것 |
|---|---|---|---|
| `makamaka456/ai5-sdl2` (이 포크) | 공개 | 엔진 소스, 새 게임 모듈, `android/`, Actions, 분석 도구 스크립트, 이 문서 | 게임 원본, 한글패치, 참고 APK, APK에서 꺼낸 파일 |
| `makamaka456/elf-games-private` | 비공개 | 게임 원본, 한글패치, 참고 APK, 분석 결과 메모 | 엔진 소스 |

비공개 저장소 폴더 규칙:
```
elf-games-private/
  games/hanahebi/     # 꽃과뱀 원본 (게임 ID 폴더명)
  games/isaku/        # 비교 테스트용 기존 지원작
  patches/hanahebi-ko/
  reference-apk/AI5-SDL2-Android-v1.10-debug.apk
  reference-apk/unpacked/
  notes/apk-analysis.md
```
- 100MB 넘는 파일은 비공개 저장소의 **릴리즈 첨부 파일**로 올리고 작업 시 내려받는다.
- 서브모듈 `subprojects/libai5`도 고쳐야 하면 같이 포크한다.

## 꼭 지킬 규칙
- 게임 원본, 한글패치, 참고 APK와 거기서 꺼낸 파일은 **절대 공개 포크에 커밋하지 않는다**. 커밋 전 `git status`로 확인한다.
- 비공개 저장소는 읽기 위주. 지우거나 덮어써야 하면 먼저 사용자에게 묻는다.
- 참고 APK의 `.so`나 코드를 우리 APK에 복사하지 않는다. 방식만 참고해 우리 소스로 다시 만든다.
- 원본 엔진의 기존 지원 게임 동작을 깨뜨리지 않는다. 새 게임 코드는 그 게임 전용 파일에 둔다.
- 라이선스(GPL-2.0)를 유지하고, 바꾼 소스도 포크에 공개된 상태로 둔다.
- 모르는 사실(예: 꽃과뱀의 스크립트 버전)은 추측해서 단정하지 않는다. 확인한 것과 추측을 구분해 보고한다.

## 작전 (2026-11-14 사용자와 합의, 같은 날 수정)
- 목표는 **참고 APK가 지원하는 게임(일본어판·한글판) + 꽃과뱀**을 돌리는 런처. 참고 APK의 화면·동작을 똑같이 흉내 낼 필요는 없다.
- 꽃과뱀: 사용자가 한글패치 완료된 게임 폴더 zip(약 1.7GB)을 비공개 저장소 `hanahebi` 릴리즈에 올린다. 윈도우 PC·UMPC 원본 실행 파일로 완벽히 돌아가는 상태다.

## 작업 순서
1. 정찰 → `docs/structure-ko.md` ✅
2. PC 빌드 확인 ✅
3. 참고 APK 분석 → 비공개 `notes/apk-analysis.md` ✅
4. 유작 한글판 PC 확인 ✅
5. 한글판 모드(CP949·한글 폰트·PATCH/) ✅ (유작)
6. 꽃과뱀 PC 지원 (게임 파일 분석 → 모듈 추가 → 타이틀·본편 확인)
7. 안드로이드 빌드 (`android/`, NDK, Actions 산출물) + 런처 (지원 게임 목록 + 꽃과뱀)
8. 실기 검수 (사용자 폰)
- 취작·동급생2 한글판 등 다른 지원작은 파일이 있으면 같은 방식으로 확인한다.

## PC 빌드 (리눅스 작업 공간)
```
apt-get install -y meson libsdl2-dev libsdl2-ttf-dev libsndfile1-dev libavcodec-dev libavformat-dev libswscale-dev
git submodule update --init --recursive
meson setup build && ninja -C build
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/ai5 <게임폴더>
```
비공개 저장소는 `/home/user/elf-games-private` 에 클론해 쓴다.

## 배경 지식
- 게임 폴더에 `AI5WIN.EXE`가 있으면 AI5WIN, `AI6WIN.exe`가 있으면 AI6WIN.
- 같은 AI5WIN이라도 시기에 따라 스크립트 형식 차이가 커서 게임마다 전용 코드를 둔다.
- 구조 정리는 `docs/structure-ko.md` 참고.

## 한글판 모드 (5단계에서 추가)
- `--korean` 옵션, `--game=<게임>-korean`, 또는 INI의 대사 아카이브 이름이 `.KOR`면 켜진다.
- 켜지면: 대사를 CP949로 읽음(`src/encoding.c`, 표는 `tools/gen_cp949_table.py`로 생성), 폰트는 `fonts/NanumGothic.ttf`, 게임 폴더의 `PATCH/` 안 파일(그림·`DEFMAIN.MES`)을 아카이브보다 먼저 씀.
- `--korean`이고 INI를 안 주면 `Isaku98.ini` → `AIWINK.INI` 순서로 찾는다.
- 확인: 유작 한글판(`isaku98.zip`)으로 오프닝·본편 대사·날짜 그림(PATCH/7_22.G16)·안내문(PATCH/DEFMAIN.MES) 한글 표시 확인. 일본어판도 그대로 동작.

## 꽃과뱀 진행 상황 (이어서 할 사람용, 2026-11-14 기준)
사용자 지시: **중요한 결정이 아니면 묻지 말고 계속 진행. 한도 초기화 후에도 알아서 이어서 작업.**

- 코드: `src/hanahebi.c` (꽃과뱀 모듈), libai5 포크 `makamaka456/libai5` 브랜치 `claude/lucid-tesla-qpa0cp` (GAME_HANAHEBI, CP949, hanahebi sysvar 표)
- 도구·준비 방법: `tools/re/README-ko.md`
- 분석 메모·Ghidra 결과: 비공개 저장소 `notes/hanahebi-analysis.md`, `notes/ghidra/HH2D.decompiled.c`
- **된 것** (스크린샷 확인): LIBLARY.LIB 로드, 배경·전환 효과, 한글 대사창(오버레이), 인물 그림+표정(Util 50/51), BGM/효과음/음성, 클릭 대기 메뉴, 오프닝~본편 여러 장면 진행
- **선택지 완료** (스크린샷 확인): defmenu 항목 + sub 37 메뉴, 버튼(Util 200~203, 오버레이 1), [14] 항목 실행, [16] 메뉴 번호, [20] 글자 수. sysvar16 21=항목 수, 22=메뉴 번호, sysvar32 7=항목 주소표
  - 다듬을 것: 버튼 테두리 투명색(밝은 사각형 보임), 버튼 글자 뒤 검은 상자
- **2D 모드로 전환 완료**: HH2D.exe처럼 var4[4013]=1, sysvar16[23]=15(뒤 버퍼), flags|=0x402|0x2000, [10] 화면 표시(15→0, 16=이전 화면), 선택지 Util(2000) 직접 그림(오버레이 2), 버튼 반투명(그림판별 알파 보관), 글자 안티앨리어싱
- **떠 있는 얼굴 문제 해결**: 원인은 CG 캐시 공유본을 직접 수정한 것(위치 +160, 알파). `hh_cg_load()`로 사본을 만들어 씀
- **2D 이동 화면 조사 중**: 2D판은 평면도(미니맵)에서 방을 골라 이동하는 방식으로 보임
  - `MAPPT.GCC`(712x1131) = 평면도 조각·방 이름표(일본어) 모음 그림, `SUB00.GCC` 인물, `SUB_MAP.A6` 애니메이션
  - `MAPF.TBL`(1층, 82개)/`MAPB.TBL`(지하, 18개) = u32 개수 + 노드마다 u32×5(번호, 이웃 4방향, -1=없음)
  - `POINT.PT`(84개)/`POINT2.PT` = u32 개수 + (float x,y,z, u16 번호, u16 이웃×4)
  - data.arc의 `*.BMP`/`*.DAT`(BGA_xxx, MAPA01/02)는 그림이 아니라 640x480 바이트 영역표(클릭 영역 번호)
  - 비공개 메모 `notes/hanahebi-analysis.md` "2D 이동(맵) 모듈"에 Util 번호→함수 표
- **2D 이동: 방 선택 방식으로 동작** (스크린샷 확인): Util(1001) → 방 목록(1~18, 21; 이름은 MAPPT 이름표 순서) → var32[12]=방 번호, 오른쪽 클릭=100. 배경은 현재 지점 시점 그림(`.MAP` = 1024바이트 BGRx 팔레트 + 640x480 8비트, 이름 F<변형>_<지점>_<방향0~7>.map, B00=지하, 없으면 F00)
  - 원본 방식(지점 그래프를 걸으며 문 클릭)은 나중에: HH2D.exe 0x407f40 루프, 0x403c30 회전, 0x407560 클릭, 0x404aa0 마우스 판정, MM=클릭 영역표(A00_<지점>_<방향*2>.mm)
  - 테스트: `AI5_REDIRECT_MES="fas000.mes=FASmapbase.mes"` 로 바로 지도 대본 시작
- **세이브/로드 완료** (스크린샷 확인, 2026-11-15): 저장 폴더 `SAVEHANAHEBI/` (원본과 같은 파일 이름: FLAG%03u=메모리 16KB, FLAG000=공통 변수, SaveData%03u.dat=화면 기록, CG%03u.dat=96x72 썸네일, MesSkip.dat=읽음 표시)
  - 원리: 대본의 0x17 n = 저장 지점(sysvar32[8]=n), 불러오면 MES를 처음부터 다시 시작 → var4[4002]==1이면 화면 기록([24])으로 그림을 다시 그린 뒤 0x18로 주소표[n]에 점프
  - [4] 0 불러오기/1 저장/2 var4 읽기/3 공통 변수 합치기/7·8 0x2310 영역, [24] 화면 기록(46바이트×30), [26] 읽음 비트
  - Util 12 = 저장 화면(우리가 새로 만듦: 32칸, 4쪽, 썸네일·날짜), Util 5 = 오른쪽 클릭 시스템 메뉴(원본은 윈도우 메뉴바 → 팝업으로 대체)
- **백로그 완료**: [19] 0 지움/1 시작/2 끝/3 개수/4 주소, Util 24 = 지난 대사 보기(휠·화살표, 음성 다시 듣기). 휠 위로=백로그는 INI [Wheel] His=1일 때만(원본과 같음)
- **타이틀 메뉴 완료**: Util 11000 (title_p.gcc 버튼, 메뉴 0/1/2), 한국어판이면 버튼 글자를 한국어로 다시 그림
- 한글패치 자동 감지: 대본(FAS000/START/MAIN.MES) 글자의 첫 바이트 통계(CP949 0xB0~0xC8 vs SJIS 0x82·0x83·0x88~0x9F)로 판단 → `--korean` 없이 동작
- 볼륨: AI5WIN.INI [Music]/[Effect]/[Voice] Volume(0~9) 반영. 공통 변수 합치기에서 var4[4250·4251]은 최댓값이 아니라 현재 값(타이틀 메뉴 기억값)
- **여흥 메뉴 완료** (스크린샷 확인): 음악 감상(Util 24000, 곡 1~24·30=정지), 그림 감상(Util 21000, data.arc의 `<인물>_normal/choukyou.itm`: u32 그룹 수 + 0xff8바이트 그룹[u32 해제 플래그, u32 항목 수, 0x44바이트 항목(이름8+코드8 ×4, u32 플래그)]), 장면 회상(Util 23000, var16[9]=탭+1, var16[18]=번호+1, 플래그표 HH2D 0x5982b8), 엔딩 회상(Util 22000, 플래그표 0x597ba0)
  - 시험: `HH_DEBUG_UNLOCK=1`(전부 해제), `AI5_REDIRECT_MES="main.mes=cgmode.mes"` 등으로 바로 시작
  - 인물 이름: sizu=시즈코, kei=케이코, kyou=쿄코, mit=미츠코, cha=체코
- **추가 Util 완료**: 0 화면 흔들기(표시 위치 이동), 2 var4 구간 채우기(새 게임 때 줄거리 변수 초기화!), 3·4 타이머, 21 마스크 와이프(NN.msk), 60 파노라마 스크롤(큰 그림은 따로 보관), 64 방 조사(BGA_NNN.DAT 영역표 아래위 뒤집힘, ITM 6×13×3 아이템표, 버튼 20, 오른쪽 클릭 100), 2001/12000 크로스페이드, [10](0,색) 번쩍임
  - Util 14/15/66/67과 지도 Util 1003~1045 대부분은 3D판 전용이거나 지도 모듈 내부라 2D 진행에는 불필요(확인: var4[4013]==1이면 다른 경로)
  - 시험 옵션: `HH_DEBUG_SET="1662:1,785:5"` (시작 시 var4 값 지정)
- **남은 일 (우선순위)**: 자유 조교(Util 20000: (0,0) 인물 선택→var32[9], (1,인물) 설정→heap 바이트 0~4, var16[18]), 대사창 아이콘(Util 9000), 원본식 2D 이동, 나머지 Util(504, 60, 80, 1005/1006)
- System 함수 표: 0 글꼴, 2 커서, 5 BGM, 6 효과음, 7 음성, 8 파일/라이브러리, 9 그림, 10 화면 갱신(미구현), 11 그림 연산, 12 대기, 13 색, 18 입력, 19 백로그, 24 화면 기록 목록, 26 ?, 27 INI, 29 시간
- Util: 23 전환 속도, 50/51 인물, 502/503 대사창 보이기/숨기기, 그 외 대부분 미구현(로그만)
- Util 분기 함수: HH2D.exe `FUN_00421bd0` (Ghidra 결과에서 `case 0x3e8` 등으로 찾기)

## 안드로이드 (7단계, 2026-11-15 시작)
- 폴더 `android/`: Gradle 프로젝트(AGP 8.7, compileSdk 35, minSdk 21, arm64-v8a/armeabi-v7a)
  - `fetch-deps.sh`: SDL2 2.30.9, SDL2_ttf 2.22.0, SDL2_mixer 2.8.0, libpng 1.6.44를 `app/jni/deps/`에 받음 (커밋 안 함)
  - `app/jni/CMakeLists.txt`: 위 라이브러리 + libai5 + 엔진(`libmain.so`, 소리는 SDL_mixer 경로 `USE_SDL_MIXER`)
  - 리눅스에서 시험: `cmake android/app/jni && make main` (PC에서 CMake 설정 오류를 먼저 잡을 수 있음)
  - 자바: `LauncherActivity`(게임 폴더 목록, 폴더 추가→자동 감지, 길게 눌러 한글판 설정), `GameActivity`(SDLActivity, 화면 버튼 메뉴/넘기기/⋮, 뒤로 가기=오른쪽 클릭, 별도 프로세스 `:game`)
  - 게임 파일은 복사하지 않고 "모든 파일 접근" 권한으로 폴더를 직접 읽음. 글꼴은 assets → 앱 폴더로 복사 후 `AI5_DATA_DIR` 환경 변수로 엔진에 알림
- 런처(2026-11-15 개편): 지원 게임 목록 + 게임별 폴더, 각 줄 [배경화면] 버튼(그림 → 앱 폴더 `bg_<게임>.png`), 화면 꽉 채우기, 게임패드 설정. 공용 설정은 `AppSettings.java`(SharedPreferences "launcher", 두 프로세스가 같이 씀)
- 게임 화면: 가상 버튼(오른쪽 메뉴·넘기기·⚙, 왼쪽 숨기기·지난 대사, 선택: 결정·효과 빨리), ⚙ = 화면 비율·배경화면·버튼 크기/투명도/위치(끌어서)·게임패드·버튼 숨기기·끝내기
- 엔진 환경 변수: `AI5_STRETCH`(1=꽉 채우기), `AI5_BACKGROUND`(PNG 경로, 화면 전체 배경), 둘 다 매 프레임 확인해서 게임 중 바로 바뀜. `AI5_CONTROLLER`는 시작할 때만 읽음
- APK는 GitHub Actions `Android` 워크플로(`.github/workflows/android.yaml`) 산출물 `ai5-launcher-apk` (작업 공간에서는 dl.google.com이 막혀 직접 빌드 불가)

## 꽃과뱀 키 분석 (2026-11-15)
- 대본은 `System.function[18](키번호, 1)`로 키를 확인(결과 var32[18]). HH2D.exe 0x415210 키표(DirectInput):
  0 Enter/왼쪽 클릭, 1 ESC/오른쪽 클릭, 2~5 방향키, 6 Shift, 7 Ctrl, 8 Space, 9 Backspace, 10 PgUp, 11 PgDn, 12 Delete, 13~16 Z X C V, 17 Tab, 18 F11, 19 F12
- 대본이 실제 쓰는 것: 0 결정, 1 메뉴/취소, 6 Shift(화면 효과 빨리), 7 Ctrl(넘기기, 누르는 동안), 8 Space(대사창 숨기기), 9 Backspace(지난 대사), 휠 아래(INI Wheel Mes=1이면 대사 넘김)
- 안드로이드 화면 버튼: 메뉴(ESC)·넘기기(Ctrl 토글)·숨기기(Space)·지난 대사(Backspace)·⋮(Enter, Shift, 버튼 숨기기, 끝내기). 두 손가락 탭·뒤로 가기 = ESC
- 게임패드: 엔진의 `[CONTROLLER]` 설정을 환경 변수 `AI5_CONTROLLER="A=ENTER;B=ESCAPE;...;LEFTANALOG=CURSOR"`로 받음. 런처 "게임패드 버튼 설정"에서 버튼별 기능·스틱(커서/방향키/끄기)·커서 속도 지정
  - 기본값: A 결정, B·START 메뉴, X 넘기기, Y 숨기기, L1 지난 대사, R1 효과 빨리, L2/R2 PgUp/PgDn, 왼쪽 스틱 커서, 오른쪽 스틱 방향키
