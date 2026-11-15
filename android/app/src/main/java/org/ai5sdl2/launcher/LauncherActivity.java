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

    // games supported by the engine: id (--game=), name shown, note
    static final String[][] GAMES = {
        { "hanahebi",           "꽃과뱀 (花と蛇)",                 "한글패치판·일본어판 자동 감지" },
        { "isaku-korean",       "유작 한글판",                      "Isaku98.ini, Patch 폴더" },
        { "isaku",              "유작 리뉴얼 (일본어판)",           "" },
        { "shuusaku-korean",    "취작 한글판",                      "시험 중" },
        { "shuusaku",           "취작 (일본어판)",                  "" },
        { "doukyuusei2-korean", "동급생2 한글판",                   "시험 중" },
        { "doukyuusei2",        "동급생2 (일본어판)",               "" },
        { "doukyuusei",         "동급생 Windows판",                 "" },
        { "kakyuusei",          "하급생",                           "" },
        { "aishimai",           "애자매 ～두 사람의 과실～",        "" },
        { "yuno",               "YU-NO (일본어판)",                 "" },
        { "yuno-eng",           "YU-NO (영문 패치판)",              "" },
        { "beyond",             "비욘드 (ビ・ヨンド)",              "" },
        { "shangrlia",          "샹그릴라 (SHANGRLIA)",             "" },
    };

    private ArrayAdapter<String> adapter;
    private TextView status;
    private Button permButton;
    private int pickingFor = -1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
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
        CheckBox stretch = new CheckBox(this);
        stretch.setText("화면 꽉 채우기 (늘리기)");
        stretch.setTextColor(Color.WHITE);
        stretch.setChecked(prefs().getBoolean("stretch", true));
        stretch.setOnCheckedChangeListener((b, on) ->
                prefs().edit().putBoolean("stretch", on).apply());
        row.addView(stretch);
        root.addView(row);
        Button pad = new Button(this);
        pad.setText("게임패드 버튼 설정");
        pad.setOnClickListener(v -> AppSettings.padSettings(this, null));
        root.addView(pad, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

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
        hint.setText("길게 누르면 폴더 바꾸기·지정 해제. 게임 중: 터치 = 클릭, 두 손가락 탭·뒤로 가기·[메뉴]"
                + " = 오른쪽 클릭(메뉴/취소), [넘기기] = 대사 빨리 넘기기.");
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
        else if (id.equals("isaku-korean") && !hasFile(dir, "Isaku98.ini"))
            warn = "유작 한글판 폴더에는 Isaku98.ini가 있어야 합니다.";
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
        i.putExtra(GameActivity.EXTRA_ARGS, new String[] { "--game=" + GAMES[game][0] });
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
        String note = GAMES[pos][2].isEmpty() ? "" : "  (" + GAMES[pos][2] + ")";
        t2.setText((folder != null ? folder : "폴더 미지정 — 눌러서 게임 폴더 고르기") + note);
        t2.setTextSize(12);
        t2.setTextColor(folder != null ? 0xFF9AD29A : 0xFF888888);
        texts.addView(t1);
        texts.addView(t2);
        row.addView(texts, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        Button bg = new Button(this);
        boolean hasBg = !AppSettings.backgroundPath(this, GAMES[pos][0]).isEmpty();
        bg.setText(hasBg ? "배경화면 ✓" : "배경화면");
        bg.setFocusable(false);
        bg.setOnClickListener(v -> backgroundMenu(pos));
        row.addView(bg);
        return row;
    }

    private void backgroundMenu(int game) {
        String id = GAMES[game][0];
        if (AppSettings.backgroundPath(this, id).isEmpty()) {
            pickImage(game);
            return;
        }
        new AlertDialog.Builder(this)
                .setTitle(GAMES[game][1] + " 배경화면")
                .setItems(new String[] { "다른 그림으로 바꾸기", "배경화면 없애기" }, (d, which) -> {
                    if (which == 0) {
                        pickImage(game);
                    } else {
                        AppSettings.clearBackground(this, id);
                        adapter.notifyDataSetChanged();
                    }
                })
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
