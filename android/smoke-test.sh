#!/bin/bash
# Emulator smoke test (CI): install the APK, start the launcher and the game
# activity (without game data), print logcat and small screenshots (base64).
set -x
APK=$(ls android/app/build/outputs/apk/release/*.apk | head -1)
PKG=org.ai5sdl2.launcher
shot() {
	adb exec-out screencap -p > /tmp/$1.png
}
print_shot() {
	convert /tmp/$1.png -resize 360x360 -quality 50 /tmp/$1.jpg 2>/dev/null || cp /tmp/$1.png /tmp/$1.jpg
	echo "SCREENSHOT-BEGIN $1"
	base64 -w0 /tmp/$1.jpg
	echo
	echo "SCREENSHOT-END $1"
}
adb install -r -g "$APK"
adb shell appops set $PKG MANAGE_EXTERNAL_STORAGE allow
adb logcat -c
adb shell am start -n $PKG/.LauncherActivity
sleep 8
shot launcher
# game activity with an empty folder: the engine should start and report the error
adb root; sleep 3
adb shell mkdir -p /sdcard/ai5test
adb shell am start -n $PKG/.GameActivity --es path /sdcard/ai5test
sleep 10
shot game
# the virtual button editor (opened by a debug extra)
adb shell am force-stop $PKG
adb shell am start -n $PKG/.GameActivity --es path /sdcard/ai5test --ez debug_edit true
sleep 10
shot editor
adb logcat -d | grep -E " (SDL|ai5|libc|DEBUG|AndroidRuntime) *:" | grep -v NetworkSecurity | tail -60
print_shot launcher
print_shot game
print_shot editor
exit 0
