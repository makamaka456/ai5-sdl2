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
    private static final String PREFS = "games";

    static class Game {
        String name;
        String path;
        boolean korean;
        String note;
    }

    private final List<Game> games = new ArrayList<>();
    private ArrayAdapter<String> adapter;
    private TextView status;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        copyFonts();
        loadGames();
        buildUi();
    }

    @Override
    protected void onResume() {
        super.onResume();
        updateStatus();
    }

    // ---------------------------------------------------------------- UI

    private void buildUi() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(32, 32, 32, 32);
        root.setBackgroundColor(0xFF1A0A0E);

        TextView title = new TextView(this);
        title.setText("AI5 런처");
        title.setTextSize(26);
        title.setTextColor(0xFFF4E8C0);
        root.addView(title);

        TextView info = new TextView(this);
        info.setText("elf AI5WIN 게임(유작·취작·동급생·동급생2·하급생·애자매·YU-NO·꽃과뱀 등, "
                + "일본어판·한글판)을 실행합니다. 게임 파일은 APK에 들어 있지 않습니다. "
                + "게임 폴더를 휴대폰 저장소에 넣고 \"게임 폴더 추가\"로 고르세요.");
        info.setTextColor(0xFFCCCCCC);
        info.setPadding(0, 12, 0, 12);
        root.addView(info);

        status = new TextView(this);
        status.setTextColor(0xFFFF8080);
        root.addView(status);

        LinearLayout buttons = new LinearLayout(this);
        buttons.setOrientation(LinearLayout.HORIZONTAL);
        Button add = new Button(this);
        add.setText("게임 폴더 추가");
        add.setOnClickListener(v -> addGame());
        buttons.addView(add);
        Button perm = new Button(this);
        perm.setText("저장소 권한");
        perm.setOnClickListener(v -> requestStorage());
        buttons.addView(perm);
        root.addView(buttons);

        ListView list = new ListView(this);
        adapter = new ArrayAdapter<String>(this, android.R.layout.simple_list_item_2,
                android.R.id.text1) {
            @Override
            public View getView(int pos, View convert, ViewGroup parent) {
                View v = super.getView(pos, convert, parent);
                Game g = games.get(pos);
                TextView t1 = v.findViewById(android.R.id.text1);
                TextView t2 = v.findViewById(android.R.id.text2);
                t1.setText(g.name + (g.korean ? "  [한글판]" : ""));
                t1.setTextColor(Color.WHITE);
                t2.setText(g.path + (g.note != null && !g.note.isEmpty() ? "\n" + g.note : ""));
                t2.setTextColor(0xFFAAAAAA);
                return v;
            }
        };
        list.setAdapter(adapter);
        list.setOnItemClickListener((p, v, pos, id) -> startGame(games.get(pos)));
        list.setOnItemLongClickListener((p, v, pos, id) -> {
            editGame(pos);
            return true;
        });
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1);
        root.addView(list, lp);

        TextView hint = new TextView(this);
        hint.setText("누르면 실행, 길게 누르면 이름 바꾸기·한글판 설정·삭제. "
                + "게임 중: 화면 터치 = 클릭, 두 손가락 탭·뒤로 가기·[메뉴] = 오른쪽 클릭(메뉴/취소), "
                + "[넘기기] = 대사 빨리 넘기기. 꽃과뱀 한글판은 자동으로 감지됩니다.");
        hint.setTextColor(0xFF999999);
        hint.setTextSize(12);
        root.addView(hint);

        setContentView(root);
        refresh();
    }

    private void refresh() {
        adapter.clear();
        for (Game g : games)
            adapter.add(g.name);
        adapter.notifyDataSetChanged();
    }

    private void updateStatus() {
        if (status == null)
            return;
        status.setText(hasStorage() ? "" : "저장소 권한이 필요합니다. [저장소 권한]을 눌러 허용하세요.");
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
                Intent i = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                        Uri.parse("package:" + getPackageName()));
                startActivity(i);
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

    private void status(String s) {
        if (status != null)
            status.setText(s);
    }

    // ---------------------------------------------------------------- games

    private void loadGames() {
        games.clear();
        SharedPreferences p = getSharedPreferences(PREFS, MODE_PRIVATE);
        try {
            JSONArray a = new JSONArray(p.getString("list", "[]"));
            for (int i = 0; i < a.length(); i++) {
                JSONObject o = a.getJSONObject(i);
                Game g = new Game();
                g.name = o.getString("name");
                g.path = o.getString("path");
                g.korean = o.optBoolean("korean");
                g.note = o.optString("note");
                games.add(g);
            }
        } catch (Exception e) {
            // start with an empty list
        }
    }

    private void saveGames() {
        JSONArray a = new JSONArray();
        try {
            for (Game g : games) {
                JSONObject o = new JSONObject();
                o.put("name", g.name);
                o.put("path", g.path);
                o.put("korean", g.korean);
                o.put("note", g.note);
                a.put(o);
            }
        } catch (Exception e) {
            return;
        }
        getSharedPreferences(PREFS, MODE_PRIVATE).edit().putString("list", a.toString()).apply();
    }

    private void addGame() {
        if (!hasStorage()) {
            requestStorage();
            return;
        }
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        try {
            startActivityForResult(i, REQ_TREE);
        } catch (Exception e) {
            askPath();
        }
    }

    // fallback when there is no folder picker: type the path
    private void askPath() {
        EditText e = new EditText(this);
        e.setInputType(InputType.TYPE_CLASS_TEXT);
        e.setText(Environment.getExternalStorageDirectory().getAbsolutePath() + "/");
        new AlertDialog.Builder(this)
                .setTitle("게임 폴더 경로")
                .setView(e)
                .setPositiveButton("확인", (d, w) -> addFolder(new File(e.getText().toString().trim())))
                .setNegativeButton("취소", null)
                .show();
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
        if (req != REQ_TREE || res != RESULT_OK || data == null || data.getData() == null)
            return;
        File dir = treeUriToFile(data.getData());
        if (dir == null) {
            status("이 폴더는 경로로 바꿀 수 없습니다. 내부 저장소나 SD 카드의 폴더를 고르세요.");
            askPath();
            return;
        }
        addFolder(dir);
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

    private static File findIcase(File dir, String name) {
        String[] list = dir.list();
        if (list == null)
            return null;
        for (String n : list) {
            if (n.equalsIgnoreCase(name))
                return new File(dir, n);
        }
        return null;
    }

    private static boolean hasSuffix(File dir, String suffix) {
        String[] list = dir.list();
        if (list == null)
            return false;
        for (String n : list) {
            if (n.toUpperCase().endsWith(suffix))
                return true;
        }
        return false;
    }

    // TITLE= from the game's INI (Shift-JIS)
    private static String iniTitle(File dir) {
        String[] list = dir.list();
        if (list == null)
            return null;
        for (String n : list) {
            if (!n.toUpperCase().endsWith(".INI"))
                continue;
            try (BufferedReader r = new BufferedReader(new InputStreamReader(
                    new FileInputStream(new File(dir, n)), Charset.forName("Shift_JIS")))) {
                String line;
                while ((line = r.readLine()) != null) {
                    line = line.trim();
                    if (line.toUpperCase().startsWith("TITLE="))
                        return line.substring(6).trim();
                }
            } catch (Exception e) {
                // ignore
            }
        }
        return null;
    }

    private void addFolder(File dir) {
        if (!dir.isDirectory() || dir.list() == null) {
            status("폴더를 읽을 수 없습니다: " + dir + "\n(저장소 권한을 확인하세요)");
            return;
        }
        Game g = new Game();
        g.path = dir.getAbsolutePath();
        g.name = dir.getName();
        g.note = "";
        if (findIcase(dir, "AI6WIN.exe") != null && findIcase(dir, "AI5WIN.EXE") == null) {
            new AlertDialog.Builder(this)
                    .setMessage("AI6WIN 게임(예: 귀작)은 아직 지원하지 않습니다.")
                    .setPositiveButton("확인", null).show();
            return;
        }
        if (findIcase(dir, "HH2D.exe") != null) {
            g.name = "꽃과뱀 (花と蛇)";
            g.korean = findIcase(dir, "AI5WIN.EXE.bak") != null;
        } else {
            String t = iniTitle(dir);
            if (t != null && !t.isEmpty())
                g.name = dir.getName() + " (" + t + ")";
            g.korean = findIcase(dir, "Isaku98.ini") != null || hasSuffix(dir, ".KOR")
                    || findIcase(dir, "PATCH") != null;
        }
        if (findIcase(dir, "AI5WIN.EXE") == null && findIcase(dir, "HH2D.exe") == null
                && findIcase(dir, "AIWIN.EXE") == null)
            g.note = "AI5WIN.EXE가 없습니다. 게임 폴더가 맞는지 확인하세요.";
        games.add(g);
        saveGames();
        refresh();
        editGame(games.size() - 1);
    }

    private void editGame(int pos) {
        Game g = games.get(pos);
        LinearLayout l = new LinearLayout(this);
        l.setOrientation(LinearLayout.VERTICAL);
        l.setPadding(40, 20, 40, 0);
        EditText name = new EditText(this);
        name.setText(g.name);
        l.addView(name);
        CheckBox korean = new CheckBox(this);
        korean.setText("한글판 (CP949 대사, 한글 글꼴, PATCH 폴더)");
        korean.setChecked(g.korean);
        l.addView(korean);
        TextView path = new TextView(this);
        path.setText(g.path);
        l.addView(path);
        new AlertDialog.Builder(this)
                .setTitle("게임 설정")
                .setView(l)
                .setPositiveButton("저장", (d, w) -> {
                    g.name = name.getText().toString();
                    g.korean = korean.isChecked();
                    saveGames();
                    refresh();
                })
                .setNeutralButton("삭제", (d, w) -> {
                    games.remove(pos);
                    saveGames();
                    refresh();
                })
                .setNegativeButton("취소", null)
                .show();
    }

    private void startGame(Game g) {
        if (!hasStorage()) {
            requestStorage();
            return;
        }
        if (!new File(g.path).isDirectory()) {
            status("폴더가 없습니다: " + g.path);
            return;
        }
        Intent i = new Intent(this, GameActivity.class);
        i.putExtra(GameActivity.EXTRA_PATH, g.path);
        ArrayList<String> args = new ArrayList<>();
        if (g.korean)
            args.add("--korean");
        i.putExtra(GameActivity.EXTRA_ARGS, args.toArray(new String[0]));
        startActivity(i);
    }
}
