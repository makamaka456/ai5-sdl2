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
import android.text.InputType;
import android.widget.EditText;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Map;

final class AppSettings {
    private AppSettings() {}

    // the launcher and the game run in different processes
    @SuppressWarnings("deprecation")
    static SharedPreferences prefs(Context c) {
        return c.getSharedPreferences("launcher", Context.MODE_MULTI_PROCESS);
    }

    // ------------------------------------------------------------ per game
    // Settings are kept per game ("g.<game>.<key>"); a game without its own
    // value uses the older launcher-wide value, then the default.

    static String gk(String game, String key) {
        return "g." + game + "." + key;
    }

    static String getStr(Context c, String game, String key, String def) {
        SharedPreferences p = prefs(c);
        String v = p.getString(gk(game, key), null);
        return v != null ? v : p.getString(key, def);
    }

    static void putStr(Context c, String game, String key, String value) {
        prefs(c).edit().putString(gk(game, key), value).commit();
    }

    // screen: true = 16:9 (fill the screen), false = 4:3 (original ratio)
    static boolean stretch(Context c, String game) {
        SharedPreferences p = prefs(c);
        if (p.contains(gk(game, "stretch")))
            return p.getBoolean(gk(game, "stretch"), true);
        return p.getBoolean("stretch", true);
    }

    static void setStretch(Context c, String game, boolean on) {
        prefs(c).edit().putBoolean(gk(game, "stretch"), on).commit();
    }

    // replays / CG / endings open from the start (AI5_OPEN_ALL; default on)
    static boolean openAll(Context c, String game) {
        return prefs(c).getBoolean(gk(game, "openall"), true);
    }

    static void setOpenAll(Context c, String game, boolean on) {
        prefs(c).edit().putBoolean(gk(game, "openall"), on).commit();
    }

    // games whose engine module supports AI5_OPEN_ALL
    static boolean supportsOpenAll(String game) {
        return game.equals("hanahebi");
    }

    // value for AI5_BACKGROUND: path + "#" + version so a new picture under
    // the same file name is loaded again
    static String backgroundEnv(Context c, String gameId) {
        File f = backgroundFile(c, gameId);
        return f.exists() ? f.getAbsolutePath() + "#" + f.lastModified() : "";
    }

    // ------------------------------------------------------------ on-screen buttons
    // All button kinds: id, label, side (left/right), y (fraction),
    // hidden by default, x (fraction or "").  Each game uses its own subset
    // with its own names and defaults (GAME_BUTTONS below).
    static final String[][] VBUTTONS = {
        { "menu",   "메뉴",       "right", "0.22", "0", "" },
        { "rclick", "메뉴",       "right", "0.22", "0", "" },
        { "mclick", "메뉴",       "right", "0.22", "0", "" },
        { "skip",   "넘기기",     "right", "0.42", "0", "" },
        { "enter",  "결정",       "right", "0.62", "1", "" },
        { "gear",   "⚙",         "right", "0.80", "0", "" },
        { "hide",   "숨기기",     "left",  "0.25", "0", "" },
        { "log",    "지난 대사",  "left",  "0.45", "0", "" },
        { "shift",  "효과 빨리",  "left",  "0.65", "1", "" },
        { "tab",    "Tab",        "left",  "0.45", "1", "" },
        { "f1",     "F1",         "left",  "0.45", "1", "" },
        { "skey",   "S",          "left",  "0.65", "1", "" },
        { "lkey",   "L",          "left",  "0.80", "1", "" },
        { "up",     "▲",         "left",  "0.62", "1", "0.13" },
        { "left",   "◀",         "left",  "0.75", "1", "0.06" },
        { "right",  "▶",         "left",  "0.75", "1", "0.20" },
        { "down",   "▼",         "left",  "0.88", "1", "0.13" },
        { "stick",  "",           "left",  "0.72", "1", "0.14" },
    };
    // key each kind sends (for the long names in lists)
    static final String[][] VBUTTON_KEYS = {
        { "menu", "ESC" }, { "rclick", "오른쪽 클릭" }, { "mclick", "가운데 클릭" },
        { "skip", "Ctrl, 켜고 끄기" }, { "enter", "Enter" }, { "gear", "" },
        { "hide", "Space" }, { "log", "Backspace" }, { "shift", "Shift" },
        { "tab", "Tab" }, { "f1", "F1" }, { "skey", "S" }, { "lkey", "L" },
        { "up", "↑" }, { "left", "←" }, { "right", "→" }, { "down", "↓" },
        { "stick", "끌어서 방향키" },
    };

    /*
     * Buttons of each game, from the keys its engine module uses:
     * "id|name|side|y|shown"  (side/y empty = from VBUTTONS, shown 1/0).
     * The first entry is also what the Android back key does.
     */
    private static final String ARROWS = "up|▲|||0;left|◀|||0;right|▶|||0;down|▼|||0;stick||||0";
    private static final String[][] GAME_BUTTONS = {
        { "hanahebi", "menu|메뉴|||1;skip|넘기기|||1;enter|결정|||0;gear|⚙|||1;"
                + "hide|숨기기|||1;log|지난 대사|||1;shift|효과 빨리|||0;" + ARROWS },
        // Isaku: right click = context menu, Space = item window,
        // S / L = save / load, Tab = hide message, ESC = cancel
        { "isaku", "rclick|메뉴|||1;skip|넘기기|||1;enter|결정|||0;gear|⚙|||1;"
                + "hide|아이템 창|left|0.20|1;tab|메시지 숨기기|left|0.36|1;"
                + "skey|저장|left|0.52|0;lkey|불러오기|left|0.68|0;"
                + "menu|취소|right|0.62|0;shift|효과 빨리|left|0.84|0;" + ARROWS },
        // Shuusaku: middle click = context menu, Space = schedule, F1 = status
        { "shuusaku", "mclick|메뉴|||1;skip|넘기기|||1;enter|결정|||0;gear|⚙|||1;"
                + "hide|일정표|left|0.20|1;f1|상태|left|0.36|1;"
                + "menu|취소|left|0.52|1;shift|효과 빨리|left|0.68|0;" + ARROWS },
        // Doukyuusei: Backspace is read by the scripts
        { "doukyuusei", "menu|메뉴|||1;skip|넘기기|||1;enter|결정|||0;gear|⚙|||1;"
                + "log|지난 대사|left|0.25|1;shift|효과 빨리|left|0.45|0;" + ARROWS },
        { "doukyuusei2", "menu|메뉴|||1;skip|넘기기|||1;enter|결정|||0;gear|⚙|||1;"
                + "shift|효과 빨리|left|0.25|0;" + ARROWS },
        { "kakyuusei", "menu|메뉴|||1;skip|넘기기|||1;enter|결정|||0;gear|⚙|||1;"
                + "shift|효과 빨리|left|0.25|0;" + ARROWS },
        { "*", "menu|메뉴|||1;skip|넘기기|||1;enter|결정|||0;gear|⚙|||1;"
                + "shift|효과 빨리|left|0.25|0;" + ARROWS },
    };

    // the buttons of a game, in VBUTTONS format with its own names/defaults
    static String[][] gameButtons(String game) {
        String spec = null;
        for (String[] g : GAME_BUTTONS) {
            if (g[0].equals(game) || (game != null && game.startsWith(g[0] + "-")))
                spec = g[1];
        }
        if (spec == null)
            spec = GAME_BUTTONS[GAME_BUTTONS.length - 1][1];
        ArrayList<String[]> out = new ArrayList<>();
        for (String item : spec.split(";")) {
            String[] f = item.split("\\|", -1);
            String[] base = vbutton(f[0]).clone();
            if (!f[1].isEmpty() || f[0].equals("stick"))
                base[1] = f[1];
            if (!f[2].isEmpty())
                base[2] = f[2];
            if (!f[3].isEmpty())
                base[3] = f[3];
            base[4] = f[4].equals("1") ? "0" : "1";
            out.add(base);
        }
        return out.toArray(new String[0][]);
    }

    // what the Android back key does in this game
    static String backAction(String game) {
        return gameButtons(game)[0][0];
    }

    static String[] vbutton(String id) {
        for (String[] d : VBUTTONS) {
            if (d[0].equals(id))
                return d;
        }
        return VBUTTONS[0];
    }

    static String[] vbutton(String game, String id) {
        for (String[] d : gameButtons(game)) {
            if (d[0].equals(id))
                return d;
        }
        return vbutton(id);
    }

    // long name for lists: "아이템 창 (Space)"
    static String vbuttonName(String game, String id) {
        String n = vbutton(game, id)[1];
        if (id.equals("gear"))
            return "⚙ 설정";
        if (id.equals("stick"))
            n = "방향 스틱";
        String k = label(VBUTTON_KEYS, id);
        return k.isEmpty() || k.equals(id) ? n : n + " (" + k + ")";
    }

    // settings of one button (position as a fraction of the screen, -1 = default)
    static class BtnCfg {
        float x = -1, y = -1;
        int alpha = 70;     // 10..100 %
        int size = 100;     // 60..200 %
        boolean hidden;

        BtnCfg copy() {
            BtnCfg c = new BtnCfg();
            c.x = x; c.y = y; c.alpha = alpha; c.size = size; c.hidden = hidden;
            return c;
        }

        String serialize() {
            return x + "," + y + "," + alpha + "," + size + "," + (hidden ? 1 : 0);
        }

        static BtnCfg parse(String v, BtnCfg def) {
            BtnCfg c = def.copy();
            try {
                String[] f = v.split(",");
                c.x = Float.parseFloat(f[0]); c.y = Float.parseFloat(f[1]);
                c.alpha = Integer.parseInt(f[2]); c.size = Integer.parseInt(f[3]);
                c.hidden = f[4].equals("1");
            } catch (Exception e) {
                // keep defaults
            }
            return c;
        }
    }

    static BtnCfg defaultBtn(String game, String id) {
        BtnCfg c = new BtnCfg();
        c.hidden = vbutton(game, id)[4].equals("1");
        return c;
    }

    static BtnCfg btnCfg(Context c, String game, String id) {
        String v = getStr(c, game, "btn." + id, null);
        BtnCfg b = v != null ? BtnCfg.parse(v, defaultBtn(game, id)) : defaultBtn(game, id);
        if (id.equals("gear"))
            b.hidden = false; // the settings button is always there
        return b;
    }

    static void saveBtn(Context c, String game, String id, BtnCfg b) {
        putStr(c, game, "btn." + id, b.serialize());
    }

    // check list of the buttons to show (⚙ is always shown); `shown` is
    // indexed like gameButtons(game)
    static void chooseVisible(Activity a, String game, boolean[] shown, Runnable done) {
        String[][] all = gameButtons(game);
        ArrayList<String> names = new ArrayList<>();
        ArrayList<Integer> idx = new ArrayList<>();
        for (int i = 0; i < all.length; i++) {
            if (all[i][0].equals("gear"))
                continue;
            names.add(vbuttonName(game, all[i][0]));
            idx.add(i);
        }
        boolean[] checked = new boolean[idx.size()];
        for (int k = 0; k < checked.length; k++)
            checked[k] = shown[idx.get(k)];
        new AlertDialog.Builder(a)
                .setTitle("표시할 버튼 (⚙는 항상 표시)")
                .setMultiChoiceItems(names.toArray(new String[0]), checked,
                        (d, which, on) -> checked[which] = on)
                .setPositiveButton("확인", (d, w) -> {
                    for (int k = 0; k < checked.length; k++)
                        shown[idx.get(k)] = checked[k];
                    if (done != null)
                        done.run();
                })
                .setNegativeButton("취소", null)
                .show();
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
        "ENTER", "ESCAPE", "CTRL", "SPACE", "BACKSPACE", "SHIFT", "BACKSPACE", "SPACE",
        "NONE", "ESCAPE", "NONE", "NONE", "UP", "DOWN", "LEFT", "RIGHT",
    };
    static final String[][] PAD_ACTIONS = {
        { "LMOUSE", "마우스 왼쪽 클릭" }, { "RMOUSE", "마우스 오른쪽 클릭" },
        { "ENTER", "결정 (Enter)" }, { "ESCAPE", "메뉴 / 취소 (ESC)" },
        { "CTRL", "넘기기 (누르는 동안 Ctrl)" }, { "SPACE", "대사창 숨기기 (Space)" },
        { "BACKSPACE", "지난 대사 (Backspace)" }, { "SHIFT", "화면 효과 빨리 (Shift)" },
        { "UP", "위" }, { "DOWN", "아래" }, { "LEFT", "왼쪽" }, { "RIGHT", "오른쪽" },
        { "NONE", "없음" },
    };
    static final String[][] STICK_MODES = {
        { "CURSOR", "마우스 커서 움직이기" }, { "DPAD", "방향키로 쓰기" }, { "DISABLED", "쓰지 않기" },
    };

    // engine action of each on-screen button kind (gamepad mapping)
    private static final String[][] BUTTON_PAD_ACTION = {
        { "menu", "ESCAPE" }, { "rclick", "RCLICK" }, { "mclick", "MCLICK" },
        { "skip", "CTRL" }, { "enter", "ENTER" }, { "hide", "SPACE" },
        { "log", "BACKSPACE" }, { "shift", "SHIFT" }, { "tab", "TAB" },
        { "f1", "F1" }, { "skey", "S" }, { "lkey", "L" },
    };

    // gamepad actions of a game, named like its on-screen buttons
    static String[][] padActions(String game) {
        ArrayList<String[]> out = new ArrayList<>();
        out.add(new String[] { "LMOUSE", "마우스 왼쪽 클릭" });
        for (String[] b : gameButtons(game)) {
            String act = label(BUTTON_PAD_ACTION, b[0]);
            if (act.equals(b[0]))
                continue; // gear, arrows, stick
            String name = vbuttonName(game, b[0]);
            if (act.equals("CTRL"))
                name = b[1] + " (누르는 동안 Ctrl)";
            out.add(new String[] { act, name });
        }
        out.add(new String[] { "UP", "위" });
        out.add(new String[] { "DOWN", "아래" });
        out.add(new String[] { "LEFT", "왼쪽" });
        out.add(new String[] { "RIGHT", "오른쪽" });
        out.add(new String[] { "NONE", "없음" });
        return out.toArray(new String[0][]);
    }

    // per-game default gamepad layout ("BUTTON=ACTION;..." over PAD_DEFAULTS)
    private static final String[][] GAME_PAD_DEFAULTS = {
        { "isaku", "START=RCLICK;Y=SPACE;LEFTSHOULDER=TAB;LEFTTRIGGER=S;RIGHTTRIGGER=L" },
        { "shuusaku", "START=MCLICK;Y=SPACE;LEFTSHOULDER=F1;LEFTTRIGGER=NONE;RIGHTTRIGGER=NONE" },
        { "doukyuusei", "Y=NONE;RIGHTTRIGGER=NONE" },
        { "hanahebi", "" },
        { "*", "Y=NONE;LEFTSHOULDER=NONE;LEFTTRIGGER=NONE;RIGHTTRIGGER=NONE" },
    };

    static String padDefault(String game, int i) {
        String spec = null;
        for (String[] g : GAME_PAD_DEFAULTS) {
            if (g[0].equals(game) || (game != null && game.startsWith(g[0] + "-")))
                spec = g[1];
        }
        if (spec == null)
            spec = GAME_PAD_DEFAULTS[GAME_PAD_DEFAULTS.length - 1][1];
        for (String kv : spec.split(";")) {
            int eq = kv.indexOf('=');
            if (eq > 0 && kv.substring(0, eq).equals(PAD_BUTTONS[i][0]))
                return kv.substring(eq + 1);
        }
        return PAD_DEFAULTS[i];
    }

    static String padValue(Context c, String game, int i) {
        // per game only: the old launcher-wide value used ESC for every menu
        String v = prefs(c).getString(gk(game, "pad." + PAD_BUTTONS[i][0]), null);
        return v != null ? v : padDefault(game, i);
    }

    static String stickValue(Context c, String game, boolean left) {
        return getStr(c, game, left ? "pad.LEFTANALOG" : "pad.RIGHTANALOG", left ? "CURSOR" : "DPAD");
    }

    static String speedValue(Context c, String game) {
        return getStr(c, game, "pad.CURSORSPEED", "16");
    }

    static String label(String[][] table, String value) {
        for (String[] e : table) {
            if (e[0].equals(value))
                return e[1];
        }
        return value;
    }

    // value for the engine: "A=ENTER;B=ESCAPE;...;LEFTANALOG=CURSOR"
    static String controllerConfig(Context c, String game) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < PAD_BUTTONS.length; i++)
            sb.append(PAD_BUTTONS[i][0]).append('=').append(padValue(c, game, i)).append(';');
        sb.append("LEFTANALOG=").append(stickValue(c, game, true)).append(';');
        sb.append("RIGHTANALOG=").append(stickValue(c, game, false)).append(';');
        sb.append("CURSORSPEED=").append(speedValue(c, game));
        return sb.toString();
    }

    // gamepad settings of a game; onChange runs after every change (the game applies it at once)
    static void padSettings(Activity a, String game, Runnable onChange) {
        int n = PAD_BUTTONS.length;
        String[] items = new String[n + 6];
        for (int i = 0; i < n; i++)
            items[i] = PAD_BUTTONS[i][1] + "  →  " + label(padActions(game), padValue(a, game, i));
        items[n] = "왼쪽 스틱  →  " + label(STICK_MODES, stickValue(a, game, true));
        items[n + 1] = "오른쪽 스틱  →  " + label(STICK_MODES, stickValue(a, game, false));
        items[n + 2] = "커서 속도  →  " + speedValue(a, game);
        items[n + 3] = "★ 프리셋 저장";
        items[n + 4] = "★ 프리셋 불러오기 / 지우기";
        items[n + 5] = "기본값으로 되돌리기";
        new AlertDialog.Builder(a)
                .setTitle("키맵핑 (게임패드) — 바로 적용")
                .setItems(items, (d, which) -> {
                    if (which < n) {
                        chooseAction(a, game, which, onChange);
                    } else if (which == n || which == n + 1) {
                        chooseStick(a, game, which == n ? "pad.LEFTANALOG" : "pad.RIGHTANALOG", onChange);
                    } else if (which == n + 2) {
                        chooseSpeed(a, game, onChange);
                    } else if (which == n + 3) {
                        askName(a, "게임패드 프리셋 저장", name -> {
                            prefs(a).edit().putString("padpreset." + name, padSerialize(a, game)).apply();
                            padSettings(a, game, onChange);
                        });
                    } else if (which == n + 4) {
                        choosePreset(a, "padpreset.", "게임패드 프리셋", v -> {
                            padDeserialize(a, game, v);
                            changed(onChange);
                            padSettings(a, game, onChange);
                        }, () -> padSettings(a, game, onChange));
                    } else {
                        padDeserialize(a, game, defaultPadString(game));
                        changed(onChange);
                        padSettings(a, game, onChange);
                    }
                })
                .setNegativeButton("닫기", null)
                .show();
    }

    static String defaultPadString(String game) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < PAD_BUTTONS.length; i++)
            sb.append("pad.").append(PAD_BUTTONS[i][0]).append('=').append(padDefault(game, i)).append(';');
        sb.append("pad.LEFTANALOG=CURSOR;pad.RIGHTANALOG=DPAD;pad.CURSORSPEED=16;");
        return sb.toString();
    }

    private static void changed(Runnable onChange) {
        if (onChange != null)
            onChange.run();
    }

    // the game's gamepad settings as "pad.A=ENTER;pad.B=ESCAPE;..."
    static String padSerialize(Context c, String game) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < PAD_BUTTONS.length; i++)
            sb.append("pad.").append(PAD_BUTTONS[i][0]).append('=').append(padValue(c, game, i)).append(';');
        sb.append("pad.LEFTANALOG=").append(stickValue(c, game, true)).append(';');
        sb.append("pad.RIGHTANALOG=").append(stickValue(c, game, false)).append(';');
        sb.append("pad.CURSORSPEED=").append(speedValue(c, game)).append(';');
        return sb.toString();
    }

    static void padDeserialize(Context c, String game, String v) {
        SharedPreferences.Editor e = prefs(c).edit();
        for (String kv : v.split(";")) {
            int eq = kv.indexOf('=');
            if (eq > 0 && kv.startsWith("pad."))
                e.putString(gk(game, kv.substring(0, eq)), kv.substring(eq + 1));
        }
        e.commit();
    }

    interface StringCallback {
        void run(String s);
    }

    // ask for a preset name
    static void askName(Activity a, String title, StringCallback done) {
        EditText t = new EditText(a);
        t.setInputType(InputType.TYPE_CLASS_TEXT);
        t.setHint("이름 (예: 레트로이드)");
        new AlertDialog.Builder(a)
                .setTitle(title)
                .setView(t)
                .setPositiveButton("저장", (d, w) -> {
                    String name = t.getText().toString().trim().replace(";", "").replace("=", "");
                    if (!name.isEmpty())
                        done.run(name);
                })
                .setNegativeButton("취소", null)
                .show();
    }

    // list presets stored under prefix; tap = load, long list entry "지우기" mode via button
    static void choosePreset(Activity a, String prefix, String title, StringCallback load, Runnable back) {
        ArrayList<String> names = new ArrayList<>();
        for (String k : prefs(a).getAll().keySet()) {
            if (k.startsWith(prefix))
                names.add(k.substring(prefix.length()));
        }
        Collections.sort(names);
        if (names.isEmpty()) {
            new AlertDialog.Builder(a)
                    .setMessage("저장된 프리셋이 없습니다.")
                    .setPositiveButton("확인", (d, w) -> { if (back != null) back.run(); })
                    .show();
            return;
        }
        String[] arr = names.toArray(new String[0]);
        new AlertDialog.Builder(a)
                .setTitle(title + " 불러오기")
                .setItems(arr, (d, w) -> load.run(prefs(a).getString(prefix + arr[w], "")))
                .setNeutralButton("지우기…", (d, w) -> new AlertDialog.Builder(a)
                        .setTitle(title + " 지우기")
                        .setItems(arr, (d2, w2) -> {
                            prefs(a).edit().remove(prefix + arr[w2]).apply();
                            if (back != null) back.run();
                        })
                        .setNegativeButton("취소", null)
                        .show())
                .setNegativeButton("닫기", (d, w) -> { if (back != null) back.run(); })
                .show();
    }

    private static void chooseAction(Activity a, String game, int button, Runnable onChange) {
        String[][] acts = padActions(game);
        String[] labels = new String[acts.length];
        for (int i = 0; i < labels.length; i++)
            labels[i] = acts[i][1];
        new AlertDialog.Builder(a)
                .setTitle(PAD_BUTTONS[button][1] + " 버튼")
                .setItems(labels, (d, which) -> {
                    putStr(a, game, "pad." + PAD_BUTTONS[button][0], acts[which][0]);
                    changed(onChange);
                    padSettings(a, game, onChange);
                })
                .show();
    }

    private static void chooseStick(Activity a, String game, String key, Runnable onChange) {
        String[] labels = new String[STICK_MODES.length];
        for (int i = 0; i < labels.length; i++)
            labels[i] = STICK_MODES[i][1];
        new AlertDialog.Builder(a)
                .setItems(labels, (d, which) -> {
                    putStr(a, game, key, STICK_MODES[which][0]);
                    changed(onChange);
                    padSettings(a, game, onChange);
                })
                .show();
    }

    private static void chooseSpeed(Activity a, String game, Runnable onChange) {
        String[] speeds = { "8", "12", "16", "24", "32" };
        new AlertDialog.Builder(a)
                .setTitle("커서 속도")
                .setItems(speeds, (d, which) -> {
                    putStr(a, game, "pad.CURSORSPEED", speeds[which]);
                    changed(onChange);
                    padSettings(a, game, onChange);
                })
                .show();
    }
}
