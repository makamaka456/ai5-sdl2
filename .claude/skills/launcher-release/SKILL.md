---
name: launcher-release
description: 안드로이드 런처 APK를 빌드하거나 정식 릴리즈(v0.6 등)를 만들 때 쓴다. 사용자가 "APK 만들어", "릴리즈 하자", "버전 올려"라고 할 때만. 작업 공간에서는 태그 푸시가 막혀 있어 Actions 손 실행으로 태그·릴리즈를 만든다.
---

# APK 빌드·정식 릴리즈

- APK는 GitHub Actions `Android` 워크플로(`.github/workflows/android.yaml`)가 만든다(작업 공간은 dl.google.com이 막혀 직접 빌드 불가).
  평소 커밋은 `[skip ci]`로 빌드를 막아 둔다.

## APK만 (시험판)
`[skip ci]` 없이 커밋·푸시 → 브랜치 빌드(약 5분, 에뮬레이터 실행 시험 포함) → `android-latest` 사전 출시에 `ai5-launcher.apk`.
확인: `actions_list`(list_workflow_runs, android.yaml) → 성공이면 https://github.com/makamaka456/ai5-sdl2/releases/tag/android-latest

## 정식 릴리즈
1. `android/app/build.gradle`의 `versionCode`(+1 이상)·`versionName` 올리기, `android/release-notes.md`(한국어 설명: 되는 것·고친 것·알려진 문제) 고치기.
2. `[skip ci]` 없이 커밋·푸시 → 브랜치 빌드가 끝날 때까지 기다림(같은 브랜치 실행끼리는 나중 것이 앞 것을 취소).
3. `actions_run_trigger`: method=run_workflow, workflow_id=android.yaml, ref=`claude/lucid-tesla-qpa0cp`, inputs `{"release_tag": "v0.6"}`.
4. 약 6분 뒤 `get_release_by_tag`로 정식 릴리즈·`ai5-launcher.apk` 첨부 확인 → 사용자에게 릴리즈 주소와 APK 주소를 알림.
- 서명 키 `android/ai5-launcher.keystore`는 고정(바꾸면 사용자가 지우고 다시 설치해야 함).
- 첫 정식판: v0.5 (2026-11-17).
