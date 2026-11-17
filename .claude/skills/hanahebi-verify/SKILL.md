---
name: hanahebi-verify
description: 꽃과뱀(HH2D, AI5WIN 2D판) 동작을 원본과 맞춰 확인·디버그할 때 쓴다. 자동 진행 시험, 처음~엔딩 저장/불러오기 화면 비교(slrun), 특정 지점 저장 만들기(mksave), 고치기 전·후 화면 비교(cmp), Wine으로 원본 HH2D.exe와 나란히 비교, 원본 기계어(objdump/Ghidra) 확인 순서.
---

# 꽃과뱀 확인·디버그 순서

시험 스크립트는 `tools/test/`(설명은 `docs/handoff-hanahebi-ko.md` 4장). 결과는 `/tmp/hh-test/`.

## 1. 문제 재현
- 특정 대본부터: `AI5_REDIRECT_MES="main.mes=FAS020.mes"`, 로그 `AI5_LOG_MES=1`(대본·저장 지점), `AI5_TRACE=1`(명령 한 줄씩).
- 특정 지점 저장: `tools/test/mksave.sh FAS022.MES 40 3` (공략 경로로 가서 칸 3에 저장 후 끝, 수십 분).
- 고치기 전·후 화면: 엔진 사본 두 개를 `tools/test/cmp.sh`로 다른 화면(:91, :92)에서 동시에 돌려 지점별 스크린샷 비교. 동시 실행은 2~3개까지(메모리).

## 2. 원본 확인 (추측 금지)
- 기계어: Util 분기 `FUN_00421bd0`(분기표 0x425038은 Util 1002부터), [10] 화면 표시 0x450620. 비공개 저장소 `notes/ghidra/HH2D.decompiled.c`.
  Ghidra는 인자를 빠뜨리므로 호출 직전 `push`를 objdump로 확인한다.
- Wine 원본: `tools/test/wine-orig.sh :99` → 25초 뒤 타이틀. 우리 엔진으로 만든 저장(`!n:칸`)을 원본에서 불러와
  몇 번 클릭하며 `import -window root -crop 640x480+80+82` 스크린샷. 원본 저장 목록(FLAG000 0x27c0+(칸−1)×0x38)이 비면 원본 화면에 안 보임.
- 원본이 쓴 저장(SaveData%03u.dat = u32 개수 + 46바이트 기록)도 우리 엔진으로 불러와 비교할 수 있다. 기록을 고쳐 넣어 원본 반응을 보는 것도 가능.

## 3. 회귀 시험 (고친 뒤)
- `tools/test/setup.sh 89 && tools/test/slrun.sh :89` (백그라운드). 엔딩(FAS_END*) 하나까지 약 50분.
  기다릴 때는 `until grep -aqi "load MES FAS_END" /tmp/hh-test/sl.log; do sleep 30; done` 식으로(연속 sleep은 막힘).
- 결과: `grep -a differs /tmp/hh-test/sl.log | grep -vE " [0-4]\.[0-9]% "`, 큰 차이 뒤 `screen records:` 줄, 그림 `/tmp/hh-test/slimg/*_a.bmp`(보통)·`_b.bmp`(불러온 뒤).
- 알려진 원본 동작(고치지 말 것): 보통 진행은 검은 화면/이벤트 그림인데 불러오면 복도(기록 16·17 때문, Wine 확인),
  기록 없이 그린 인물은 불러오면 없음. 목록은 `CLAUDE.md` "전체 비교 시험 결론".
- 진행이 멈춘 듯하면 `/tmp/hh-test/sl/progress.txt`와 스크린샷 확인(작은 버튼·긴 메뉴에서 맴도는 경우가 있었음).
- 시험이 저장 폴더(`savehanahebi`)를 건드리니 시작 전에 백업, 끝나면 되돌린다.

## 4. 보고
한국어, "한 일 / 확인 결과(스크린샷) / 남은 문제 / 다음 단계". 확인한 것과 추측을 나눈다. 커밋은 `[skip ci]`(APK는 사용자가 원할 때만).
