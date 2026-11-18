// AI5 launcher for Android: list of game folders, add/remove, start.
// SPDX-License-Identifier: GPL-2.0-or-later
package org.ai5sdl2.launcher;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.List;

public class LauncherActivity extends Activity {
    private static final int REQ_TREE = 1;
    private static final int REQ_PERM = 2;
    private static final int REQ_IMAGE = 3;
    private static final String PREFS = "launcher";

    // games supported by the engine: id (--game=), name shown, note,
    // file that marks the Korean translation ("" = no Korean version or the
    // engine detects it itself).  One row per game: Korean / Japanese is
    // chosen automatically from the folder or in the game's [설정].
    static final String[][] GAMES = {
        { "hanahebi",           "꽃과뱀 (花と蛇)",                 "한글패치판·일본어판 자동 감지", "" },
        { "isaku",              "유작 리뉴얼",                      "", "Isaku98.ini" },
        { "shuusaku",           "취작",                             "", "AIWINK.INI" },
        { "doukyuusei2",        "동급생2",                          "", "AI5J2K.INI" },
        { "doukyuusei",         "동급생 Windows판",                 "", "" },
        { "kakyuusei",          "하급생",                           "", "" },
        { "aishimai",           "애자매 ～두 사람의 과실～",        "", "" },
        { "yuno",               "YU-NO (일본어판)",                 "", "" },
        { "yuno-eng",           "YU-NO (영문 패치판)",              "", "" },
        { "beyond",             "비욘드 (ビ・ヨンド)",              "", "" },
        { "shangrlia",          "샹그릴라 (SHANGRLIA)",             "", "" },
    };

    private ArrayAdapter<String> adapter;
    private TextView status;
    private Button permButton;
    private int pickingFor = -1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        migrateKoreanRows();
        copyFonts();
        buildUi();
    }

    @Override
    protected void onResume() {
        super.onResume();
        updateStatus();
        if (adapter != null)
            adapter.notifyDataSetChanged();
    }

    private SharedPreferences prefs() {
        return AppSettings.prefs(this);
    }

    // ---------------------------------------------------------------- language

    // Before v0.6 the Korean versions had their own rows ("<game>-korean").
    // Move their folder and settings to the game's single row (once).
    private void migrateKoreanRows() {
        SharedPreferences p = prefs();
        if (p.getBoolean("migrated.korean_rows", false))
            return;
        SharedPreferences.Editor e = p.edit();
        for (String[] g : GAMES) {
            if (g[3].isEmpty())
                continue;
            String ko = g[0] + "-korean";
            String koFolder = p.getString("folder." + ko, null);
            if (koFolder != null) {
                // the user reads Korean: the Korean folder wins
                e.putString("folder." + g[0], koFolder);
                e.remove("folder." + ko);
            }
            String prefix = "g." + ko + ".";
            for (java.util.Map.Entry<String, ?> en : p.getAll().entrySet()) {
                String k = en.getKey();
                if (!k.startsWith(prefix))
                    continue;
                String nk = "g." + g[0] + "." + k.substring(prefix.length());
                Object v = en.getValue();
                if (!p.contains(nk)) {
                    if (v instanceof String) e.putString(nk, (String) v);
                    else if (v instanceof Boolean) e.putBoolean(nk, (Boolean) v);
                    else if (v instanceof Integer) e.putInt(nk, (Integer) v);
                    else if (v instanceof Float) e.putFloat(nk, (Float) v);
                    else if (v instanceof Long) e.putLong(nk, (Long) v);
                }
                e.remove(k);
            }
            File koBg = AppSettings.backgroundFile(this, ko);
            File bg = AppSettings.backgroundFile(this, g[0]);
            if (koBg.exists()) {
                if (!bg.exists())
                    koBg.renameTo(bg);
                else
                    koBg.delete();
            }
        }
        e.putBoolean("migrated.korean_rows", true);
        e.commit();
    }

    // language setting: "auto" (default), "ko" or "ja"
    private String langOf(int game) {
        return prefs().getString(AppSettings.gk(GAMES[game][0], "lang"), "auto");
    }

    private void setLang(int game, String lang) {
        prefs().edit().putString(AppSettings.gk(GAMES[game][0], "lang"), lang).commit();
    }

    // true if the folder holds the Korean translation (marker file)
    private boolean detectKorean(int game, File dir) {
        return !GAMES[game][3].isEmpty() && dir != null && hasFile(dir, GAMES[game][3]);
    }

    private boolean runKorean(int game) {
        String l = langOf(game);
        if (l.equals("ko"))
            return true;
        if (l.equals("ja"))
            return false;
        String f = folderOf(game);
        return f != null && detectKorean(game, new File(f));
    }

    private static String langName(String l) {
        return l.equals("ko") ? "한글판" : l.equals("ja") ? "일본어판" : "자동";
    }

    private String folderOf(int i) {
        return prefs().getString("folder." + GAMES[i][0], null);
    }

    private void setFolder(int i, String path) {
        SharedPreferences.Editor e = prefs().edit();
        if (path == null)
            e.remove("folder." + GAMES[i][0]);
        else
            e.putString("folder." + GAMES[i][0], path);
        e.apply();
        adapter.notifyDataSetChanged();
    }

    // ---------------------------------------------------------------- UI

    private void buildUi() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(0xFF1A0A0E);
        // Android 15 draws apps under the status/navigation bars: keep the
        // content inside them
        root.setOnApplyWindowInsetsListener((v, insets) -> {
            v.setPadding(24 + insets.getSystemWindowInsetLeft(),
                    16 + insets.getSystemWindowInsetTop(),
                    24 + insets.getSystemWindowInsetRight(),
                    16 + insets.getSystemWindowInsetBottom());
            return insets.consumeSystemWindowInsets();
        });

        TextView title = new TextView(this);
        title.setText("AI5 런처");
        title.setTextSize(24);
        title.setTextColor(0xFFF4E8C0);
        root.addView(title);

        TextView info = new TextView(this);
        info.setText("지원하는 elf 게임 목록입니다. 게임을 누르고 그 게임 폴더를 고르세요"
                + " (AI5WIN.EXE가 있는 폴더). 게임 파일은 APK에 들어 있지 않습니다.");
        info.setTextColor(0xFFCCCCCC);
        info.setPadding(0, 8, 0, 8);
        root.addView(info);

        status = new TextView(this);
        status.setTextColor(0xFFFF8080);
        root.addView(status);

        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        permButton = new Button(this);
        permButton.setText("저장소 권한 허용");
        permButton.setOnClickListener(v -> requestStorage());
        row.addView(permButton);
        root.addView(row);

        ListView list = new ListView(this);
        adapter = new ArrayAdapter<String>(this, android.R.layout.simple_list_item_2,
                android.R.id.text1) {
            @Override
            public int getCount() {
                return GAMES.length;
            }

            @Override
            public View getView(int pos, View convert, ViewGroup parent) {
                return gameRow(pos);
            }
        };
        for (String[] g : GAMES)
            adapter.add(g[1]);
        list.setAdapter(adapter);
        list.setOnItemClickListener((p, v, pos, id) -> {
            if (folderOf(pos) == null)
                pickFolder(pos);
            else
                startGame(pos);
        });
        list.setOnItemLongClickListener((p, v, pos, id) -> {
            editGame(pos);
            return true;
        });
        root.addView(list, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));

        TextView hint = new TextView(this);
        hint.setText("[설정] = 그 게임의 화면 비율·배경화면·가상 버튼·키맵핑. 길게 누르면 폴더 바꾸기·지정 해제.\n"
                + "게임 중: 터치 = 클릭, 0.5초 누르기·두 손가락 탭·뒤로 가기 = 오른쪽 클릭(메뉴/취소).");
        hint.setTextColor(0xFF999999);
        hint.setTextSize(12);
        root.addView(hint);

        setContentView(root);
    }

    private void updateStatus() {
        boolean ok = hasStorage();
        if (status != null)
            status.setText(ok ? "" : "저장소 권한이 필요합니다. [저장소 권한 허용]을 눌러 \"모든 파일 접근\"을 허용하세요.");
        if (permButton != null)
            permButton.setVisibility(ok ? View.GONE : View.VISIBLE);
    }

    private void status(String s) {
        if (status != null)
            status.setText(s);
    }

    // ---------------------------------------------------------------- storage

    private boolean hasStorage() {
        if (Build.VERSION.SDK_INT >= 30)
            return Environment.isExternalStorageManager();
        if (Build.VERSION.SDK_INT >= 23)
            return checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                    == PackageManager.PERMISSION_GRANTED;
        return true;
    }

    private void requestStorage() {
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                        Uri.parse("package:" + getPackageName())));
            } catch (Exception e) {
                startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
            }
        } else if (Build.VERSION.SDK_INT >= 23) {
            requestPermissions(new String[] {
                    Manifest.permission.READ_EXTERNAL_STORAGE,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE }, REQ_PERM);
        }
    }

    @Override
    public void onRequestPermissionsResult(int req, String[] perms, int[] results) {
        updateStatus();
    }

    // ---------------------------------------------------------------- fonts

    // The engine reads fonts from files: copy assets/fonts to <files>/fonts.
    private void copyFonts() {
        AssetManager am = getAssets();
        File dir = new File(getFilesDir(), "fonts");
        dir.mkdirs();
        try {
            String[] names = am.list("fonts");
            if (names == null)
                return;
            for (String n : names) {
                File out = new File(dir, n);
                try (InputStream in = am.open("fonts/" + n)) {
                    if (out.exists() && out.length() == in.available())
                        continue;
                    try (OutputStream os = new FileOutputStream(out)) {
                        byte[] buf = new byte[65536];
                        int r;
                        while ((r = in.read(buf)) > 0)
                            os.write(buf, 0, r);
                    }
                }
            }
        } catch (Exception e) {
            status("글꼴 복사 실패: " + e);
        }
    }

    // ---------------------------------------------------------------- folders

    private void pickFolder(int game) {
        if (!hasStorage()) {
            requestStorage();
            return;
        }
        pickingFor = game;
        try {
            startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), REQ_TREE);
        } catch (Exception e) {
            askPath(game);
        }
    }

    // fallback when there is no folder picker: type the path
    private void askPath(int game) {
        EditText e = new EditText(this);
        e.setInputType(InputType.TYPE_CLASS_TEXT);
        String cur = folderOf(game);
        e.setText(cur != null ? cur : Environment.getExternalStorageDirectory().getAbsolutePath() + "/");
        new AlertDialog.Builder(this)
                .setTitle(GAMES[game][1] + " 폴더 경로")
                .setView(e)
                .setPositiveButton("확인", (d, w) -> assign(game, new File(e.getText().toString().trim())))
                .setNegativeButton("취소", null)
                .show();
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
        if (req == REQ_IMAGE && res == RESULT_OK && data != null && data.getData() != null
                && pickingFor >= 0) {
            if (!AppSettings.saveBackground(this, GAMES[pickingFor][0], data.getData()))
                status("그림을 읽을 수 없습니다.");
            adapter.notifyDataSetChanged();
            gameSettings(pickingFor);
            return;
        }
        if (req != REQ_TREE || res != RESULT_OK || data == null || data.getData() == null
                || pickingFor < 0)
            return;
        File dir = treeUriToFile(data.getData());
        if (dir == null) {
            status("이 폴더는 경로로 바꿀 수 없습니다. 경로를 직접 입력하세요.");
            askPath(pickingFor);
            return;
        }
        assign(pickingFor, dir);
    }

    // content://.../tree/primary:Games/hanahebi -> /storage/emulated/0/Games/hanahebi
    static File treeUriToFile(Uri uri) {
        try {
            String id = DocumentsContract.getTreeDocumentId(uri);
            int colon = id.indexOf(':');
            if (colon < 0)
                return null;
            String vol = id.substring(0, colon), rel = id.substring(colon + 1);
            File base = vol.equalsIgnoreCase("primary")
                    ? Environment.getExternalStorageDirectory()
                    : new File("/storage/" + vol);
            return rel.isEmpty() ? base : new File(base, rel);
        } catch (Exception e) {
            return null;
        }
    }

    private static boolean hasFile(File dir, String name) {
        String[] list = dir.list();
        if (list == null)
            return false;
        for (String n : list) {
            if (n.equalsIgnoreCase(name))
                return true;
        }
        return false;
    }

    private void assign(int game, File dir) {
        if (!dir.isDirectory() || dir.list() == null) {
            status("폴더를 읽을 수 없습니다: " + dir + " (저장소 권한을 확인하세요)");
            return;
        }
        String id = GAMES[game][0];
        String warn = null;
        if (hasFile(dir, "AI6WIN.exe") && !hasFile(dir, "AI5WIN.EXE"))
            warn = "AI6WIN 게임(예: 귀작)은 지원하지 않습니다.";
        else if (id.equals("hanahebi") && !hasFile(dir, "HH2D.exe"))
            warn = "꽃과뱀 폴더에는 HH2D.exe가 있어야 합니다.";
        else if (!hasFile(dir, "AI5WIN.EXE") && !hasFile(dir, "HH2D.exe"))
            warn = "AI5WIN.EXE가 없습니다. 게임 폴더가 맞는지 확인하세요.";
        else if (langOf(game).equals("ko") && !GAMES[game][3].isEmpty()
                && !hasFile(dir, GAMES[game][3]))
            warn = "언어가 한글판으로 지정되어 있지만 " + GAMES[game][3]
                    + "이(가) 없습니다. 한글패치가 된 폴더인지 확인하세요.";
        setFolder(game, dir.getAbsolutePath());
        status("");
        if (warn != null) {
            new AlertDialog.Builder(this)
                    .setTitle(GAMES[game][1])
                    .setMessage(warn + "\n\n" + dir.getAbsolutePath())
                    .setPositiveButton("확인", null)
                    .show();
        }
    }

    private void editGame(int game) {
        String[] items = { "게임 폴더 바꾸기", "폴더 경로 직접 입력", "폴더 지정 해제" };
        new AlertDialog.Builder(this)
                .setTitle(GAMES[game][1])
                .setItems(items, (d, which) -> {
                    if (which == 0)
                        pickFolder(game);
                    else if (which == 1)
                        askPath(game);
                    else
                        setFolder(game, null);
                })
                .show();
    }

    private void startGame(int game) {
        if (!hasStorage()) {
            requestStorage();
            return;
        }
        String folder = folderOf(game);
        if (folder == null || !new File(folder).isDirectory()) {
            status("폴더가 없습니다: " + folder);
            return;
        }
        Intent i = new Intent(this, GameActivity.class);
        i.putExtra(GameActivity.EXTRA_PATH, folder);
        String gameArg = GAMES[game][0] + (runKorean(game) ? "-korean" : "");
        i.putExtra(GameActivity.EXTRA_ARGS, new String[] { "--game=" + gameArg });
        i.putExtra(GameActivity.EXTRA_GAME, GAMES[game][0]);
        startActivity(i);
    }


    // one line of the game list: name, folder and a [배경화면] button
    private View gameRow(int pos) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(8, 12, 8, 12);
        row.setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);
        LinearLayout texts = new LinearLayout(this);
        texts.setOrientation(LinearLayout.VERTICAL);
        TextView t1 = new TextView(this);
        TextView t2 = new TextView(this);
        String folder = folderOf(pos);
        t1.setText(GAMES[pos][1]);
        t1.setTextSize(17);
        t1.setTextColor(folder != null ? Color.WHITE : 0xFF999999);
        String noteText = GAMES[pos][2];
        if (!GAMES[pos][3].isEmpty()) {
            // Korean / Japanese: what will run, and whether it was detected
            String l = langOf(pos);
            if (folder != null)
                noteText = (runKorean(pos) ? "한글판" : "일본어판")
                        + (l.equals("auto") ? " 자동 감지" : " 지정");
            else
                noteText = "한글판·일본어판 자동 감지";
        }
        String note = noteText.isEmpty() ? "" : "  (" + noteText + ")";
        t2.setText((folder != null ? folder : "폴더 미지정 — 눌러서 게임 폴더 고르기") + note);
        t2.setTextSize(12);
        t2.setTextColor(folder != null ? 0xFF9AD29A : 0xFF888888);
        texts.addView(t1);
        texts.addView(t2);
        row.addView(texts, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        Button set = new Button(this);
        set.setText("설정");
        set.setFocusable(false);
        set.setOnClickListener(v -> gameSettings(pos));
        row.addView(set);
        return row;
    }

    // settings of one game: screen ratio, background, on-screen buttons, gamepad
    private void gameSettings(int game) {
        String id = GAMES[game][0];
        boolean stretch = AppSettings.stretch(this, id);
        boolean hasBg = !AppSettings.backgroundPath(this, id).isEmpty();
        int shown = 0;
        for (String[] d : AppSettings.gameButtons(id)) {
            if (!AppSettings.btnCfg(this, id, d[0]).hidden)
                shown++;
        }
        String[] items = {
            "화면 비율:  " + (stretch ? "16:9 꽉 채우기" : "4:3 원래 비율") + "  (누르면 바꾸기)",
            hasBg ? "배경화면 바꾸기 (4:3일 때 양옆)" : "배경화면 고르기 (4:3일 때 양옆)",
            hasBg ? "배경화면 없애기" : "배경화면 없애기  (지금 없음)",
            "가상 버튼: 표시할 버튼 고르기  (지금 " + shown + "개)",
            "가상 버튼: 위치·크기·투명도 편집 (가로 화면)",
            "키맵핑 (게임패드)",
            AppSettings.supportsOpenAll(id)
                    ? "회상·감상 처음부터 열기: " + (AppSettings.openAll(this, id) ? "켜짐" : "꺼짐") + "  (누르면 바꾸기)"
                    : "회상·감상 처음부터 열기: 이 게임은 아직 지원 안 함",
            GAMES[game][3].isEmpty()
                    ? "언어: 자동 (이 게임은 엔진이 판단)"
                    : "언어: " + langName(langOf(game))
                            + (langOf(game).equals("auto") ? " → 지금 " + (runKorean(game) ? "한글판" : "일본어판") : "")
                            + "  (누르면 고르기)",
        };
        new AlertDialog.Builder(this)
                .setTitle(GAMES[game][1] + " 설정")
                .setItems(items, (d, which) -> {
                    switch (which) {
                    case 0:
                        AppSettings.setStretch(this, id, !stretch);
                        gameSettings(game);
                        break;
                    case 1:
                        pickImage(game);
                        break;
                    case 2:
                        AppSettings.clearBackground(this, id);
                        gameSettings(game);
                        break;
                    case 3: {
                        String[][] b = AppSettings.gameButtons(id);
                        boolean[] on = new boolean[b.length];
                        for (int i = 0; i < b.length; i++)
                            on[i] = !AppSettings.btnCfg(this, id, b[i][0]).hidden;
                        AppSettings.chooseVisible(this, id, on, () -> {
                            for (int i = 0; i < b.length; i++) {
                                AppSettings.BtnCfg c = AppSettings.btnCfg(this, id, b[i][0]);
                                c.hidden = !on[i] && !b[i][0].equals("gear");
                                AppSettings.saveBtn(this, id, b[i][0], c);
                            }
                            gameSettings(game);
                        });
                        break;
                    }
                    case 4: {
                        Intent e = new Intent(this, EditorActivity.class);
                        e.putExtra(EditorActivity.EXTRA_GAME, id);
                        e.putExtra(EditorActivity.EXTRA_NAME, GAMES[game][1]);
                        startActivity(e);
                        break;
                    }
                    case 5:
                        AppSettings.padSettings(this, id, null);
                        break;
                    case 6:
                        if (AppSettings.supportsOpenAll(id))
                            AppSettings.setOpenAll(this, id, !AppSettings.openAll(this, id));
                        gameSettings(game);
                        break;
                    case 7:
                        if (GAMES[game][3].isEmpty())
                            gameSettings(game);
                        else
                            chooseLang(game);
                        break;
                    }
                })
                .setNegativeButton("닫기", null)
                .show();
    }

    private void chooseLang(int game) {
        String[] codes = { "auto", "ko", "ja" };
        String[] names = {
            "자동 (폴더에 " + GAMES[game][3] + "이(가) 있으면 한글판)",
            "한글판",
            "일본어판",
        };
        int cur = java.util.Arrays.asList(codes).indexOf(langOf(game));
        new AlertDialog.Builder(this)
                .setTitle(GAMES[game][1] + " 언어")
                .setSingleChoiceItems(names, cur, (d, w) -> {
                    setLang(game, codes[w]);
                    d.dismiss();
                    adapter.notifyDataSetChanged();
                    gameSettings(game);
                })
                .setNegativeButton("취소", (d, w) -> gameSettings(game))
                .show();
    }

    private void pickImage(int game) {
        pickingFor = game;
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("image/*");
        try {
            startActivityForResult(i, REQ_IMAGE);
        } catch (Exception e) {
            status("그림을 고를 수 있는 앱이 없습니다.");
        }
    }
}
