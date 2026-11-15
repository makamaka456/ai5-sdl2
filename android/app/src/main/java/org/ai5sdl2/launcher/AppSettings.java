// AI5 launcher for Android: settings shared by the launcher and the game screen.
// SPDX-License-Identifier: GPL-2.0-or-later
package org.ai5sdl2.launcher;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

final class AppSettings {
    private AppSettings() {}

    // the launcher and the game run in different processes
    @SuppressWarnings("deprecation")
    static SharedPreferences prefs(Context c) {
        return c.getSharedPreferences("launcher", Context.MODE_MULTI_PROCESS);
    }

    static boolean stretch(Context c) {
        return prefs(c).getBoolean("stretch", true);
    }

    // ------------------------------------------------------------ background

    static File backgroundFile(Context c, String gameId) {
        return new File(c.getFilesDir(), "bg_" + gameId + ".png");
    }

    static String backgroundPath(Context c, String gameId) {
        File f = backgroundFile(c, gameId);
        return f.exists() ? f.getAbsolutePath() : "";
    }

    // decode a picture chosen by the user and store it as PNG (max 2048 px)
    static boolean saveBackground(Context c, String gameId, Uri uri) {
        try {
            BitmapFactory.Options o = new BitmapFactory.Options();
            o.inJustDecodeBounds = true;
            try (InputStream in = c.getContentResolver().openInputStream(uri)) {
                BitmapFactory.decodeStream(in, null, o);
            }
            int sample = 1;
            while (o.outWidth / sample > 2048 || o.outHeight / sample > 2048)
                sample *= 2;
            BitmapFactory.Options o2 = new BitmapFactory.Options();
            o2.inSampleSize = sample;
            o2.inPreferredConfig = Bitmap.Config.ARGB_8888;
            Bitmap b;
            try (InputStream in = c.getContentResolver().openInputStream(uri)) {
                b = BitmapFactory.decodeStream(in, null, o2);
            }
            if (b == null)
                return false;
            File out = backgroundFile(c, gameId);
            try (FileOutputStream os = new FileOutputStream(out)) {
                b.compress(Bitmap.CompressFormat.PNG, 100, os);
            }
            b.recycle();
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    static void clearBackground(Context c, String gameId) {
        backgroundFile(c, gameId).delete();
    }

    // ------------------------------------------------------------ gamepad

    // controller buttons (engine INI [CONTROLLER] names) and their labels
    static final String[][] PAD_BUTTONS = {
        { "A", "A" }, { "B", "B" }, { "X", "X" }, { "Y", "Y" },
        { "LEFTSHOULDER", "L1" }, { "RIGHTSHOULDER", "R1" },
        { "LEFTTRIGGER", "L2" }, { "RIGHTTRIGGER", "R2" },
        { "BACK", "SELECT" }, { "START", "START" },
        { "LEFTSTICK", "L3 (왼쪽 스틱 누르기)" }, { "RIGHTSTICK", "R3 (오른쪽 스틱 누르기)" },
        { "UP", "방향키 위" }, { "DOWN", "방향키 아래" }, { "LEFT", "방향키 왼쪽" }, { "RIGHT", "방향키 오른쪽" },
    };
    // defaults (A decide, B menu, X skip, Y hide window, L1 backlog)
    static final String[] PAD_DEFAULTS = {
        "ENTER", "ESCAPE", "CTRL", "SPACE", "BACKSPACE", "SHIFT", "PAGEUP", "PAGEDOWN",
        "NONE", "ESCAPE", "NONE", "NONE", "UP", "DOWN", "LEFT", "RIGHT",
    };
    static final String[][] PAD_ACTIONS = {
        { "ENTER", "결정 / 클릭 (Enter)" }, { "ESCAPE", "메뉴 / 취소 (오른쪽 클릭)" },
        { "CTRL", "넘기기 (누르는 동안 Ctrl)" }, { "SPACE", "대사창 숨기기 (Space)" },
        { "BACKSPACE", "지난 대사 (Backspace)" }, { "SHIFT", "화면 효과 빨리 (Shift)" },
        { "UP", "위" }, { "DOWN", "아래" }, { "LEFT", "왼쪽" }, { "RIGHT", "오른쪽" },
        { "PAGEUP", "Page Up" }, { "PAGEDOWN", "Page Down" }, { "NONE", "없음" },
    };
    static final String[][] STICK_MODES = {
        { "CURSOR", "마우스 커서 움직이기" }, { "DPAD", "방향키로 쓰기" }, { "DISABLED", "쓰지 않기" },
    };

    static String padValue(Context c, int i) {
        return prefs(c).getString("pad." + PAD_BUTTONS[i][0], PAD_DEFAULTS[i]);
    }

    static String label(String[][] table, String value) {
        for (String[] e : table) {
            if (e[0].equals(value))
                return e[1];
        }
        return value;
    }

    // value for the engine: "A=ENTER;B=ESCAPE;...;LEFTANALOG=CURSOR"
    static String controllerConfig(Context c) {
        SharedPreferences p = prefs(c);
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < PAD_BUTTONS.length; i++)
            sb.append(PAD_BUTTONS[i][0]).append('=').append(padValue(c, i)).append(';');
        sb.append("LEFTANALOG=").append(p.getString("pad.LEFTANALOG", "CURSOR")).append(';');
        sb.append("RIGHTANALOG=").append(p.getString("pad.RIGHTANALOG", "DPAD")).append(';');
        sb.append("CURSORSPEED=").append(p.getString("pad.CURSORSPEED", "16"));
        return sb.toString();
    }

    static void padSettings(Activity a, String note) {
        SharedPreferences p = prefs(a);
        int n = PAD_BUTTONS.length;
        String[] items = new String[n + 4];
        for (int i = 0; i < n; i++)
            items[i] = PAD_BUTTONS[i][1] + "  →  " + label(PAD_ACTIONS, padValue(a, i));
        items[n] = "왼쪽 스틱  →  " + label(STICK_MODES, p.getString("pad.LEFTANALOG", "CURSOR"));
        items[n + 1] = "오른쪽 스틱  →  " + label(STICK_MODES, p.getString("pad.RIGHTANALOG", "DPAD"));
        items[n + 2] = "커서 속도  →  " + p.getString("pad.CURSORSPEED", "16");
        items[n + 3] = "기본값으로 되돌리기";
        new AlertDialog.Builder(a)
                .setTitle("게임패드 버튼 설정" + (note != null ? " " + note : ""))
                .setItems(items, (d, which) -> {
                    if (which < n) {
                        chooseAction(a, which, note);
                    } else if (which == n || which == n + 1) {
                        chooseStick(a, which == n ? "pad.LEFTANALOG" : "pad.RIGHTANALOG", note);
                    } else if (which == n + 2) {
                        chooseSpeed(a, note);
                    } else {
                        SharedPreferences.Editor e = p.edit();
                        for (String k : p.getAll().keySet()) {
                            if (k.startsWith("pad."))
                                e.remove(k);
                        }
                        e.apply();
                        padSettings(a, note);
                    }
                })
                .setNegativeButton("닫기", null)
                .show();
    }

    private static void chooseAction(Activity a, int button, String note) {
        String[] labels = new String[PAD_ACTIONS.length];
        for (int i = 0; i < labels.length; i++)
            labels[i] = PAD_ACTIONS[i][1];
        new AlertDialog.Builder(a)
                .setTitle(PAD_BUTTONS[button][1] + " 버튼")
                .setItems(labels, (d, which) -> {
                    prefs(a).edit().putString("pad." + PAD_BUTTONS[button][0], PAD_ACTIONS[which][0]).apply();
                    padSettings(a, note);
                })
                .show();
    }

    private static void chooseStick(Activity a, String key, String note) {
        String[] labels = new String[STICK_MODES.length];
        for (int i = 0; i < labels.length; i++)
            labels[i] = STICK_MODES[i][1];
        new AlertDialog.Builder(a)
                .setItems(labels, (d, which) -> {
                    prefs(a).edit().putString(key, STICK_MODES[which][0]).apply();
                    padSettings(a, note);
                })
                .show();
    }

    private static void chooseSpeed(Activity a, String note) {
        String[] speeds = { "8", "12", "16", "24", "32" };
        new AlertDialog.Builder(a)
                .setTitle("커서 속도")
                .setItems(speeds, (d, which) -> {
                    prefs(a).edit().putString("pad.CURSORSPEED", speeds[which]).apply();
                    padSettings(a, note);
                })
                .show();
    }
}
