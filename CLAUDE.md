# elf 게임 안드로이드 런처 — 작업 규칙

## 목표
- 기존 한글패치 지원 게임 + 꽃과뱀(花と蛇, 2005, AI5WIN) 같은 새 elf 게임을 고를 수 있는 안드로이드 런처 APK를 직접 빌드한다.
- 1차: 꽃과뱀이 PC 빌드에서 타이틀 → 본편까지 동작
- 2차: 같은 엔진이 안드로이드 APK로 빌드되고, 한글 표시·터치 입력이 기존 한글패치 APK 수준으로 동작
- 3차: 런처에서 게임을 골라 실행 (기존 지원작 + 새 게임)
- 별도 과제: 귀작 등 AI6WIN 게임은 엔진이 달라 이 저장소 대상이 아님

## 사용자와 소통
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
- **지금 막힌 곳**: 선택지. LIBLARY.LIB `defsub 37`(오프셋 0x2415~0x33d4)이 선택지 메뉴인데, 그 안 `Util(2000)` 반복에서 멈춤.
  - 선택지 구조(대본으로 확인): proc 1 = 버튼 그리기(`sel.gcc`를 4번 그림판에 읽고 `[11]` op 8로 6번→5번 그림판에 버튼 조립, `[14](문자열 포인터)`로 글자, `[20](포인터)`→var32[18]=글자 수),
    proc 0 = 마우스 올린 항목 강조(var16[9]=올린 항목 순번, 0xffff=없음), proc 2 = 클릭 처리. 선택지 개수 = System.cg_w(sysvar16[19]) - var32[15]*4, 문자열 포인터 표 = file_data dword[i + var32[15]*4].
  - `Util(2000)` = 메뉴 실행(procs 사용) 후 var16[18]에 결과 (0이면 오른쪽 클릭 → 시스템 메뉴 sub 123/135)
  - `Util(200,x,y,w,h,id)` 클릭 영역 등록, `Util(201,id)` 지우기, `Util(202,x,y,w,h,src,_,_,id)` 버튼 그리기, `Util(203,id)` ?, `Util(18,n)` ?
  - HH2D.exe: Util 2000은 `FUN_00421bd0` 안 `uVar14 == 2000` (FUN_00409590). 200~203은 이 함수에 없음 → 다른 분기 찾기
  - `Util(1000)`~`(1100)`은 0x12d100 크기 객체(+0x10)를 만드는 다른 시스템(2D 맵으로 추정)
- **남은 일 (우선순위)**: 선택지(Util 1000번대, [11] op 8/9/10~14) → 검은 화면 독백 글자 → 2D 이동 화면(MAP2DCREATE.MES, data.arc MAP/TBL) → 세이브/로드([4], [26], [24] 목록) → 백로그([19]) → 나머지 Util
- System 함수 표: 0 글꼴, 2 커서, 5 BGM, 6 효과음, 7 음성, 8 파일/라이브러리, 9 그림, 10 화면 갱신(미구현), 11 그림 연산, 12 대기, 13 색, 18 입력, 19 백로그, 24 화면 기록 목록, 26 ?, 27 INI, 29 시간
- Util: 23 전환 속도, 50/51 인물, 502/503 대사창 보이기/숨기기, 그 외 대부분 미구현(로그만)
- Util 분기 함수: HH2D.exe `FUN_00421bd0` (Ghidra 결과에서 `case 0x3e8` 등으로 찾기)
