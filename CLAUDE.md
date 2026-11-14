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

## 작전 (2026-11-14 사용자와 합의)
- **1막 — 참고 APK 재현**: 포크 소스 + 참고 APK 분석으로, 참고 APK가 하는 일을 우리 소스로 똑같이 해내는 APK를 만든다. (바이너리 복사가 아니라 다시 만들기)
- **2막 — 꽃과뱀 추가**: 1막이 끝나면 사용자가 한글패치 완료된 꽃과뱀 게임 폴더 zip(약 1.7GB)을 비공개 저장소 릴리즈에 올린다. 그걸로 엔진에 꽃과뱀 지원을 넣는다. 이 폴더는 윈도우 PC·UMPC 원본 실행 파일로 완벽히 돌아가는 상태다.
- **최종**: 참고 APK의 상위 호환 런처.

## 작업 순서
1. 정찰 → `docs/structure-ko.md` ✅
2. PC 빌드 확인 ✅
3. 참고 APK 분석 → 비공개 `notes/apk-analysis.md` ("찾은 것 / 우리 구현에 쓸 방법 / 모르는 것") ✅
4. 비교용 게임(참고 APK가 돌리는 한글패치 게임 1개)으로 PC에서 화면·한글 확인 ✅
5. 참고 APK와 다른 점(한글 처리 등)을 우리 소스에 구현 ✅ (유작)
6. 안드로이드 빌드 (`android/`, NDK, Actions 산출물) + 런처
7. 실기 검수 → 1막 끝
8. 꽃과뱀 모듈 추가 (유작 리뉴얼 또는 취작 모듈에서 시작) → PC 확인 → APK → 실기 검수

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
