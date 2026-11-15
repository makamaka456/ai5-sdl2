// AI5 launcher for Android: the game screen (SDL + the ai5-sdl2 engine).
// SPDX-License-Identifier: GPL-2.0-or-later
package org.ai5sdl2.launcher;

import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import org.libsdl.app.SDLActivity;

import java.util.ArrayList;
import java.util.HashMap;

public class GameActivity extends SDLActivity {
    public static final String EXTRA_PATH = "path";
    public static final String EXTRA_ARGS = "args";
    public static final String EXTRA_GAME = "game";
    private static final int REQ_IMAGE = 10;

    // on-screen buttons: id, label, default side, default y (fraction of the screen), hidden by default
    private static final String[][] BUTTONS = {
        { "menu",  "메뉴",      "right", "0.22", "0" },
        { "skip",  "넘기기",    "right", "0.42", "0" },
        { "gear",  "⚙",        "right", "0.78", "0" },
        { "hide",  "숨기기",    "left",  "0.30", "0" },
        { "log",   "지난 대사", "left",  "0.55", "0" },
        { "enter", "결정",      "right", "0.62", "1" },
        { "shift", "효과 빨리", "left",  "0.80", "1" },
    };

    // settings of one button (position as a fraction of the screen, -1 = default)
    private static class BtnCfg {
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

    private FrameLayout overlay;
    private final ArrayList<Button> buttons = new ArrayList<>();
    private Button skipButton;
    private boolean ctrlHeld = false;

    // button editor: changes are kept here until [적용]
    private boolean editing = false;
    private final HashMap<String, BtnCfg> stage = new HashMap<>();
    private String selected;
    private LinearLayout editBar, editPanel;
    private TextView panelTitle;
    private SeekBar alphaBar, sizeBar;
    private CheckBox hideBox;
    private boolean panelUpdating;

    private String gameId() {
        String g = getIntent().getStringExtra(EXTRA_GAME);
        return g != null ? g : "default";
    }

    private SharedPreferences prefs() {
        return AppSettings.prefs(this);
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "SDL2_ttf", "SDL2_mixer", "main" };
    }

    @Override
    protected String[] getArguments() {
        // fonts are copied to <files>/fonts by the launcher
        nativeSetenv("AI5_DATA_DIR", getFilesDir().getAbsolutePath());
        nativeSetenv("SDL_ANDROID_TRAP_BACK_BUTTON", "1");
        applyPadSettings();
        applyDisplaySettings();
        ArrayList<String> args = new ArrayList<>();
        String[] extra = getIntent().getStringArrayExtra(EXTRA_ARGS);
        if (extra != null) {
            for (String a : extra)
                args.add(a);
        }
        args.add(getIntent().getStringExtra(EXTRA_PATH));
        return args.toArray(new String[0]);
    }

    // the engine reads these every frame, so changes apply at once
    private void applyDisplaySettings() {
        nativeSetenv("AI5_STRETCH", AppSettings.stretch(this) ? "1" : "0");
        nativeSetenv("AI5_BACKGROUND", AppSettings.backgroundPath(this, gameId()));
    }

    private void applyPadSettings() {
        nativeSetenv("AI5_CONTROLLER", AppSettings.controllerConfig(this));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // use the whole screen, including the area around a camera cutout
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams lp = getWindow().getAttributes();
            lp.layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(lp);
        }
        hideSystemBars();
        if (mLayout != null) {
            addControls();
            // CI screenshot of the button editor
            if (getIntent().getBooleanExtra("debug_edit", false))
                overlay.postDelayed(() -> { startEditing(); select("skip"); }, 3000);
        }
    }

    // full screen: hide the status and navigation bars (swipe to show them)
    @SuppressWarnings("deprecation")
    private void hideSystemBars() {
        View decor = getWindow().getDecorView();
        decor.setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController c = getWindow().getInsetsController();
            if (c != null) {
                c.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                c.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus)
            hideSystemBars();
    }

    // ------------------------------------------------------------ buttons

    private int dp(float v) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v,
                getResources().getDisplayMetrics());
    }

    private static String[] def(String id) {
        for (String[] d : BUTTONS) {
            if (d[0].equals(id))
                return d;
        }
        return BUTTONS[0];
    }

    private static BtnCfg defaultCfg(String id) {
        BtnCfg c = new BtnCfg();
        c.hidden = def(id)[4].equals("1");
        return c;
    }

    // saved settings of a button (older versions kept global size/alpha)
    private BtnCfg savedCfg(String id) {
        SharedPreferences p = prefs();
        String v = p.getString("btn." + id, null);
        if (v != null)
            return BtnCfg.parse(v, defaultCfg(id));
        BtnCfg c = defaultCfg(id);
        c.x = p.getFloat("btn.pos." + id + ".x", -1);
        c.y = p.getFloat("btn.pos." + id + ".y", -1);
        c.alpha = p.getInt("btn.alpha", 70);
        float[] sizes = { 80, 100, 130, 160 };
        c.size = (int) sizes[Math.max(0, Math.min(3, p.getInt("btn.size", 1)))];
        if (id.equals("enter") || id.equals("shift"))
            c.hidden = !p.getBoolean("btn.extra", false);
        return c;
    }

    private BtnCfg cfg(String id) {
        return editing && stage.containsKey(id) ? stage.get(id) : savedCfg(id);
    }

    private Button makeButton(String id, String text, BtnCfg c) {
        Button b = new Button(this);
        b.setText(text);
        b.setTag(id);
        b.setTextColor(Color.WHITE);
        b.setAllCaps(false);
        float s = c.size / 100f;
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP, (id.equals("gear") ? 26 : 18) * s);
        b.setPadding(dp(14 * s), dp(8 * s), dp(14 * s), dp(8 * s));
        b.setMinWidth(dp(72 * s));
        b.setMinHeight(dp(52 * s));
        b.setMinimumWidth(dp(72 * s));
        b.setMinimumHeight(dp(52 * s));
        boolean sel = editing && id.equals(selected);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(0xCC000000);
        bg.setStroke(dp(sel ? 4 : 2), sel ? 0xFFFFD040 : 0xFFC03030);
        bg.setCornerRadius(dp(14));
        b.setBackground(bg);
        if (editing)
            b.setAlpha(c.hidden ? 0.3f : 1.0f);   // in the editor: shown = dark, hidden = faint
        else
            b.setAlpha(c.alpha / 100f);
        return b;
    }

    // press and release a key in the engine
    private static void tapKey(int keycode) {
        onNativeKeyDown(keycode);
        onNativeKeyUp(keycode);
    }

    private void onButton(String id) {
        if (editing)
            return;
        switch (id) {
        case "menu":  tapKey(KeyEvent.KEYCODE_ESCAPE); break;
        case "skip":  setSkip(!ctrlHeld); break;
        case "hide":  tapKey(KeyEvent.KEYCODE_SPACE); break;
        case "log":   tapKey(KeyEvent.KEYCODE_DEL); break;
        case "enter": tapKey(KeyEvent.KEYCODE_ENTER); break;
        case "shift": tapKey(KeyEvent.KEYCODE_SHIFT_LEFT); break;
        case "gear":  showSettings(); break;
        }
    }

    private void addControls() {
        overlay = new FrameLayout(this);
        mLayout.addView(overlay, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        overlay.addOnLayoutChangeListener((v, l, t, r, b, ol, ot, or, ob) -> {
            if (r - l != or - ol || b - t != ob - ot)
                v.post(this::placeButtons);
        });
        buildButtons();
    }

    private void buildButtons() {
        for (Button b : buttons)
            overlay.removeView(b);
        buttons.clear();
        skipButton = null;
        for (String[] d : BUTTONS) {
            BtnCfg c = cfg(d[0]);
            if (c.hidden && !editing)
                continue;
            Button b = makeButton(d[0], d[0].equals("skip") && ctrlHeld ? "넘기기 ■" : d[1], c);
            if (d[0].equals("skip"))
                skipButton = b;
            b.setOnClickListener(v -> onButton((String) v.getTag()));
            b.setOnTouchListener(this::dragTouch);
            overlay.addView(b, 0, new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            buttons.add(b);
        }
        overlay.post(this::placeButtons);
    }

    // positions are kept as fractions of the screen so they survive rotation
    private void placeButtons() {
        int w = overlay.getWidth(), h = overlay.getHeight();
        if (w == 0 || h == 0)
            return;
        int margin = dp(10);
        for (Button b : buttons) {
            String id = (String) b.getTag();
            String[] d = def(id);
            BtnCfg c = cfg(id);
            int bw = b.getWidth() > 0 ? b.getWidth() : b.getMeasuredWidth();
            int bh = b.getHeight() > 0 ? b.getHeight() : b.getMeasuredHeight();
            float x, y;
            if (c.x >= 0 && c.y >= 0) {
                x = c.x * w - bw / 2f;
                y = c.y * h - bh / 2f;
            } else {
                x = d[2].equals("left") ? margin : w - bw - margin;
                y = Float.parseFloat(d[3]) * h - bh / 2f;
            }
            b.setX(Math.max(0, Math.min(w - bw, x)));
            b.setY(Math.max(0, Math.min(h - bh, y)));
        }
    }

    // ------------------------------------------------------------ button editor

    private float dragDX, dragDY, downX, downY;
    private boolean dragged;

    private boolean dragTouch(View v, MotionEvent e) {
        if (!editing)
            return false;
        String id = (String) v.getTag();
        switch (e.getActionMasked()) {
        case MotionEvent.ACTION_DOWN:
            dragDX = v.getX() - e.getRawX();
            dragDY = v.getY() - e.getRawY();
            downX = e.getRawX();
            downY = e.getRawY();
            dragged = false;
            return true;
        case MotionEvent.ACTION_MOVE:
            if (Math.abs(e.getRawX() - downX) + Math.abs(e.getRawY() - downY) > dp(8))
                dragged = true;
            if (dragged) {
                v.setX(Math.max(0, Math.min(overlay.getWidth() - v.getWidth(), e.getRawX() + dragDX)));
                v.setY(Math.max(0, Math.min(overlay.getHeight() - v.getHeight(), e.getRawY() + dragDY)));
            }
            return true;
        case MotionEvent.ACTION_UP:
            if (dragged) {
                BtnCfg c = stage.get(id);
                c.x = (v.getX() + v.getWidth() / 2f) / overlay.getWidth();
                c.y = (v.getY() + v.getHeight() / 2f) / overlay.getHeight();
            }
            select(id);
            return true;
        }
        return true;
    }

    private void startEditing() {
        if (ctrlHeld)
            setSkip(false);
        editing = true;
        stage.clear();
        for (String[] d : BUTTONS)
            stage.put(d[0], savedCfg(d[0]));
        selected = null;
        addEditBar();
        addEditPanel();
        buildButtons();
        updatePanel();
    }

    private Button barButton(String text, View.OnClickListener l) {
        Button b = new Button(this);
        b.setText(text);
        b.setAllCaps(false);
        b.setTextColor(Color.WHITE);
        b.setTextSize(15);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(0xEE402020);
        bg.setStroke(dp(1), 0xFFC03030);
        bg.setCornerRadius(dp(8));
        b.setBackground(bg);
        b.setPadding(dp(12), dp(4), dp(12), dp(4));
        b.setOnClickListener(l);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, dp(44));
        lp.setMargins(dp(4), 0, dp(4), 0);
        b.setLayoutParams(lp);
        return b;
    }

    // top bar: apply / cancel / presets / reset
    private void addEditBar() {
        editBar = new LinearLayout(this);
        editBar.setOrientation(LinearLayout.HORIZONTAL);
        editBar.setGravity(Gravity.CENTER);
        editBar.setPadding(dp(8), dp(6), dp(8), dp(6));
        editBar.setBackgroundColor(0xCC000000);
        Button apply = barButton("✔ 적용", v -> applyEditing());
        ((GradientDrawable) apply.getBackground()).setColor(0xFFB02828);
        editBar.addView(apply);
        editBar.addView(barButton("취소", v -> cancelEditing()));
        editBar.addView(barButton("프리셋 저장", v -> AppSettings.askName(this, "가상 버튼 프리셋 저장", name ->
                prefs().edit().putString("btnpreset." + name, serializeStage()).apply())));
        editBar.addView(barButton("프리셋 불러오기", v -> AppSettings.choosePreset(this, "btnpreset.",
                "가상 버튼 프리셋", val -> { loadStage(val); buildButtons(); updatePanel(); }, null)));
        editBar.addView(barButton("처음대로", v -> {
            for (String[] d : BUTTONS)
                stage.put(d[0], defaultCfg(d[0]));
            buildButtons();
            updatePanel();
        }));
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.CENTER_HORIZONTAL | Gravity.TOP);
        lp.topMargin = dp(6);
        overlay.addView(editBar, lp);
    }

    private SeekBar slider(int max) {
        SeekBar s = new SeekBar(this);
        s.setMax(max);
        s.setLayoutParams(new LinearLayout.LayoutParams(dp(220), ViewGroup.LayoutParams.WRAP_CONTENT));
        return s;
    }

    private TextView label(String text) {
        TextView t = new TextView(this);
        t.setText(text);
        t.setTextColor(Color.WHITE);
        t.setTextSize(15);
        t.setPadding(0, dp(4), dp(8), dp(4));
        return t;
    }

    private LinearLayout row(View a, View b) {
        LinearLayout r = new LinearLayout(this);
        r.setOrientation(LinearLayout.HORIZONTAL);
        r.setGravity(Gravity.CENTER_VERTICAL);
        r.addView(a);
        r.addView(b);
        return r;
    }

    // center panel: settings of the selected button
    private void addEditPanel() {
        editPanel = new LinearLayout(this);
        editPanel.setOrientation(LinearLayout.VERTICAL);
        editPanel.setPadding(dp(16), dp(10), dp(16), dp(10));
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(0xDD000000);
        bg.setStroke(dp(1), 0xFFC03030);
        bg.setCornerRadius(dp(10));
        editPanel.setBackground(bg);

        panelTitle = label("");
        panelTitle.setTextSize(16);
        editPanel.addView(panelTitle);

        alphaBar = slider(90);
        editPanel.addView(row(label("투명도"), alphaBar));
        sizeBar = slider(140);
        editPanel.addView(row(label("크기   "), sizeBar));
        hideBox = new CheckBox(this);
        hideBox.setText("이 버튼 숨기기");
        hideBox.setTextColor(Color.WHITE);
        editPanel.addView(hideBox);

        alphaBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar s, int p, boolean user) {
                if (panelUpdating || selected == null) return;
                stage.get(selected).alpha = p + 10;
                updatePanel();
                previewAlpha(selected);
            }
            @Override public void onStartTrackingTouch(SeekBar s) {}
            @Override public void onStopTrackingTouch(SeekBar s) {
                // back to the editor look (shown = dark) after previewing
                buildButtons();
            }
        });
        sizeBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar s, int p, boolean user) {
                if (panelUpdating || selected == null) return;
                stage.get(selected).size = p + 60;
                updatePanel();
            }
            @Override public void onStartTrackingTouch(SeekBar s) {}
            @Override public void onStopTrackingTouch(SeekBar s) { buildButtons(); }
        });
        hideBox.setOnCheckedChangeListener((v, checked) -> {
            if (panelUpdating || selected == null) return;
            stage.get(selected).hidden = checked;
            buildButtons();
        });

        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.CENTER);
        overlay.addView(editPanel, lp);
    }

    // while the opacity slider moves, show the selected button as it will look
    private void previewAlpha(String id) {
        for (Button b : buttons) {
            if (id.equals(b.getTag()))
                b.setAlpha(stage.get(id).alpha / 100f);
        }
    }

    private void select(String id) {
        selected = id;
        buildButtons();
        updatePanel();
    }

    private void updatePanel() {
        if (editPanel == null)
            return;
        panelUpdating = true;
        boolean has = selected != null;
        if (!has) {
            panelTitle.setText("버튼을 눌러 고르세요. 끌어서 옮길 수 있습니다.\n진한 버튼 = 보임, 연한 버튼 = 숨김");
        } else {
            BtnCfg c = stage.get(selected);
            panelTitle.setText("[" + def(selected)[1] + "]  투명도 " + c.alpha + "%  ·  크기 " + c.size + "%");
            alphaBar.setProgress(c.alpha - 10);
            sizeBar.setProgress(c.size - 60);
            hideBox.setChecked(c.hidden);
        }
        alphaBar.setEnabled(has);
        sizeBar.setEnabled(has);
        // the ⚙ button opens these settings: it can't be hidden
        hideBox.setEnabled(has && !selected.equals("gear"));
        hideBox.setText(has && selected.equals("gear") ? "⚙ 버튼은 숨길 수 없습니다" : "이 버튼 숨기기");
        panelUpdating = false;
    }

    private String serializeStage() {
        StringBuilder sb = new StringBuilder();
        for (String[] d : BUTTONS)
            sb.append(d[0]).append('=').append(stage.get(d[0]).serialize()).append(';');
        return sb.toString();
    }

    private void loadStage(String v) {
        for (String kv : v.split(";")) {
            int eq = kv.indexOf('=');
            if (eq <= 0)
                continue;
            String id = kv.substring(0, eq);
            if (stage.containsKey(id))
                stage.put(id, BtnCfg.parse(kv.substring(eq + 1), defaultCfg(id)));
        }
    }

    private void applyEditing() {
        SharedPreferences.Editor e = prefs().edit();
        for (String[] d : BUTTONS)
            e.putString("btn." + d[0], stage.get(d[0]).serialize());
        e.apply();
        endEditing();
    }

    private void cancelEditing() {
        endEditing();
    }

    private void endEditing() {
        editing = false;
        selected = null;
        if (editBar != null)
            overlay.removeView(editBar);
        if (editPanel != null)
            overlay.removeView(editPanel);
        editBar = null;
        editPanel = null;
        stage.clear();
        buildButtons();
    }

    private void setSkip(boolean on) {
        ctrlHeld = on;
        if (on)
            onNativeKeyDown(KeyEvent.KEYCODE_CTRL_LEFT);
        else
            onNativeKeyUp(KeyEvent.KEYCODE_CTRL_LEFT);
        if (skipButton != null)
            skipButton.setText(on ? "넘기기 ■" : "넘기기");
    }

    // ------------------------------------------------------------ settings (⚙)

    private void showSettings() {
        String[] items = {
            "🖥  화면 설정",
            "🔘  가상 버튼 (위치·크기·투명도·숨기기)",
            "🎮  키맵핑 (게임패드)",
            "⏻  게임 끝내기",
        };
        new AlertDialog.Builder(this)
                .setTitle("설정")
                .setItems(items, (d, which) -> {
                    switch (which) {
                    case 0: screenSettings(); break;
                    case 1: startEditing(); break;
                    case 2: AppSettings.padSettings(this, this::applyPadSettings); break;
                    case 3:
                        new AlertDialog.Builder(this)
                                .setMessage("게임을 끝낼까요? 저장하지 않은 진행은 사라집니다.")
                                .setPositiveButton("끝내기", (d2, w) -> quitGame())
                                .setNegativeButton("취소", null)
                                .show();
                        break;
                    }
                })
                .setNegativeButton("닫기", null)
                .show();
    }

    private void screenSettings() {
        boolean stretch = AppSettings.stretch(this);
        boolean hasBg = !AppSettings.backgroundPath(this, gameId()).isEmpty();
        String[] items = {
            "화면 비율:  " + (stretch ? "꽉 채우기  (누르면 → 원래 비율 4:3)" : "원래 비율 4:3  (누르면 → 꽉 채우기)"),
            hasBg ? "배경화면 바꾸기" : "배경화면 고르기",
            hasBg ? "배경화면 없애기" : "배경화면 없애기  (지금 없음)",
        };
        new AlertDialog.Builder(this)
                .setTitle("화면 설정")
                .setItems(items, (d, which) -> {
                    switch (which) {
                    case 0:
                        prefs().edit().putBoolean("stretch", !stretch).apply();
                        applyDisplaySettings();
                        screenSettings();
                        break;
                    case 1:
                        pickBackground();
                        break;
                    case 2:
                        AppSettings.clearBackground(this, gameId());
                        applyDisplaySettings();
                        screenSettings();
                        break;
                    }
                })
                .setNegativeButton("닫기", null)
                .show();
    }

    private void pickBackground() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("image/*");
        try {
            startActivityForResult(i, REQ_IMAGE);
        } catch (Exception e) {
            // no picker available
        }
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
        if (req == REQ_IMAGE && res == RESULT_OK && data != null && data.getData() != null) {
            if (AppSettings.saveBackground(this, gameId(), data.getData())) {
                // the file name stays the same: clear first so the engine reloads it
                nativeSetenv("AI5_BACKGROUND", "");
                overlay.postDelayed(this::applyDisplaySettings, 200);
            }
        }
    }

    private void quitGame() {
        finish();
        // the engine runs in its own process (":game")
        android.os.Process.killProcess(android.os.Process.myPid());
    }

    @Override
    public void onBackPressed() {
        if (editing) {
            cancelEditing();
            return;
        }
        // back = right click (menu / cancel) in the game
        tapKey(KeyEvent.KEYCODE_ESCAPE);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            if (event.getAction() == KeyEvent.ACTION_UP)
                onBackPressed();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    @Override
    protected void onPause() {
        if (ctrlHeld)
            setSkip(false);
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        // make sure the engine process does not linger
        android.os.Process.killProcess(android.os.Process.myPid());
    }
}
