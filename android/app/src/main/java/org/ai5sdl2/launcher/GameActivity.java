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
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import org.libsdl.app.SDLActivity;

import java.util.ArrayList;

public class GameActivity extends SDLActivity {
    public static final String EXTRA_PATH = "path";
    public static final String EXTRA_ARGS = "args";
    public static final String EXTRA_GAME = "game";
    private static final int REQ_IMAGE = 10;

    // on-screen buttons: id, label, default position (fraction of the screen)
    private static final String[][] BUTTONS = {
        { "menu",  "메뉴",      "right", "0.22" },
        { "skip",  "넘기기",    "right", "0.42" },
        { "gear",  "⚙",        "right", "0.78" },
        { "hide",  "숨기기",    "left",  "0.30" },
        { "log",   "지난 대사", "left",  "0.55" },
        { "enter", "결정",      "right", "0.62" },  // optional
        { "shift", "효과 빨리", "left",  "0.80" },  // optional
    };

    private FrameLayout overlay;
    private final ArrayList<Button> buttons = new ArrayList<>();
    private Button skipButton;
    private boolean ctrlHeld = false;
    private boolean editing = false;
    private boolean hidden = false;
    private TextView editHint;

    private String gameId() {
        String g = getIntent().getStringExtra(EXTRA_GAME);
        return g != null ? g : "default";
    }

    private SharedPreferences prefs() {
        return Settings.prefs(this);
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
        nativeSetenv("AI5_CONTROLLER", Settings.controllerConfig(this));
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
        nativeSetenv("AI5_STRETCH", Settings.stretch(this) ? "1" : "0");
        nativeSetenv("AI5_BACKGROUND", Settings.backgroundPath(this, gameId()));
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
        if (mLayout != null)
            addControls();
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

    // button size: 0 small, 1 normal, 2 large, 3 very large
    private float sizeScale() {
        float[] s = { 0.8f, 1.0f, 1.3f, 1.6f };
        int i = prefs().getInt("btn.size", 1);
        return s[Math.max(0, Math.min(3, i))];
    }

    private float alpha() {
        return prefs().getInt("btn.alpha", 70) / 100f;
    }

    private boolean extrasShown() {
        return prefs().getBoolean("btn.extra", false);
    }

    private Button makeButton(String id, String text) {
        Button b = new Button(this);
        b.setText(text);
        b.setTag(id);
        b.setTextColor(Color.WHITE);
        b.setAllCaps(false);
        float s = sizeScale();
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP, (id.equals("gear") ? 26 : 18) * s);
        b.setPadding(dp(14 * s), dp(8 * s), dp(14 * s), dp(8 * s));
        b.setMinWidth(dp(72 * s));
        b.setMinHeight(dp(52 * s));
        b.setMinimumWidth(dp(72 * s));
        b.setMinimumHeight(dp(52 * s));
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(0xCC000000);
        bg.setStroke(dp(2), 0xFFC03030);
        bg.setCornerRadius(dp(14));
        b.setBackground(bg);
        b.setAlpha(alpha());
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
        for (String[] def : BUTTONS) {
            boolean optional = def[0].equals("enter") || def[0].equals("shift");
            if (optional && !extrasShown())
                continue;
            Button b = makeButton(def[0], def[0].equals("skip") && ctrlHeld ? "넘기기 ■" : def[1]);
            if (def[0].equals("skip"))
                skipButton = b;
            b.setOnClickListener(v -> onButton((String) v.getTag()));
            b.setOnTouchListener(this::dragTouch);
            overlay.addView(b, new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            buttons.add(b);
        }
        overlay.setVisibility(hidden ? View.GONE : View.VISIBLE);
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
            String[] def = null;
            for (String[] d : BUTTONS) {
                if (d[0].equals(id))
                    def = d;
            }
            if (def == null)
                continue;
            int bw = b.getWidth() > 0 ? b.getWidth() : b.getMeasuredWidth();
            int bh = b.getHeight() > 0 ? b.getHeight() : b.getMeasuredHeight();
            float fx = prefs().getFloat("btn.pos." + id + ".x", -1);
            float fy = prefs().getFloat("btn.pos." + id + ".y", -1);
            float x, y;
            if (fx >= 0 && fy >= 0) {
                x = fx * w - bw / 2f;
                y = fy * h - bh / 2f;
            } else {
                x = def[2].equals("left") ? margin : w - bw - margin;
                y = Float.parseFloat(def[3]) * h - bh / 2f;
            }
            b.setX(Math.max(0, Math.min(w - bw, x)));
            b.setY(Math.max(0, Math.min(h - bh, y)));
        }
    }

    // in edit mode the buttons can be dragged
    private float dragDX, dragDY;

    private boolean dragTouch(View v, MotionEvent e) {
        if (!editing)
            return false;
        switch (e.getActionMasked()) {
        case MotionEvent.ACTION_DOWN:
            dragDX = v.getX() - e.getRawX();
            dragDY = v.getY() - e.getRawY();
            return true;
        case MotionEvent.ACTION_MOVE:
            v.setX(Math.max(0, Math.min(overlay.getWidth() - v.getWidth(), e.getRawX() + dragDX)));
            v.setY(Math.max(0, Math.min(overlay.getHeight() - v.getHeight(), e.getRawY() + dragDY)));
            return true;
        case MotionEvent.ACTION_UP:
            prefs().edit()
                    .putFloat("btn.pos." + v.getTag() + ".x", (v.getX() + v.getWidth() / 2f) / overlay.getWidth())
                    .putFloat("btn.pos." + v.getTag() + ".y", (v.getY() + v.getHeight() / 2f) / overlay.getHeight())
                    .apply();
            return true;
        }
        return true;
    }

    private void startEditing() {
        editing = true;
        hidden = false;
        overlay.setVisibility(View.VISIBLE);
        editHint = new TextView(this);
        editHint.setText("버튼을 끌어서 옮기세요.  [여기를 누르면 완료]");
        editHint.setTextColor(Color.WHITE);
        editHint.setTextSize(18);
        editHint.setBackgroundColor(0xCC802020);
        editHint.setPadding(dp(16), dp(10), dp(16), dp(10));
        editHint.setOnClickListener(v -> stopEditing());
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.CENTER_HORIZONTAL | Gravity.TOP);
        lp.topMargin = dp(12);
        overlay.addView(editHint, lp);
    }

    private void stopEditing() {
        editing = false;
        if (editHint != null)
            overlay.removeView(editHint);
        editHint = null;
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
        boolean stretch = Settings.stretch(this);
        boolean hasBg = !Settings.backgroundPath(this, gameId()).isEmpty();
        String[] items = {
            "화면 비율: " + (stretch ? "꽉 채우기 → 원래 비율(4:3)로" : "원래 비율(4:3) → 꽉 채우기로"),
            hasBg ? "배경화면 바꾸기" : "배경화면 고르기",
            hasBg ? "배경화면 없애기" : "(배경화면 없음)",
            "가상 버튼 크기",
            "가상 버튼 투명도",
            "가상 버튼 위치 바꾸기 (끌어서 옮기기)",
            "가상 버튼 위치 처음대로",
            (extrasShown() ? "[결정]·[효과 빨리] 버튼 숨기기" : "[결정]·[효과 빨리] 버튼 보이기"),
            "게임패드 버튼 설정 (다음 실행부터 적용)",
            "가상 버튼 모두 숨기기 (뒤로 가기로 다시 표시)",
            "게임 끝내기",
        };
        new AlertDialog.Builder(this)
                .setTitle("설정")
                .setItems(items, (d, which) -> {
                    switch (which) {
                    case 0:
                        prefs().edit().putBoolean("stretch", !stretch).apply();
                        applyDisplaySettings();
                        break;
                    case 1:
                        pickBackground();
                        break;
                    case 2:
                        Settings.clearBackground(this, gameId());
                        applyDisplaySettings();
                        break;
                    case 3:
                        chooseSize();
                        break;
                    case 4:
                        chooseAlpha();
                        break;
                    case 5:
                        startEditing();
                        break;
                    case 6: {
                        SharedPreferences.Editor e = prefs().edit();
                        for (String k : prefs().getAll().keySet()) {
                            if (k.startsWith("btn.pos."))
                                e.remove(k);
                        }
                        e.apply();
                        placeButtons();
                        break;
                    }
                    case 7:
                        prefs().edit().putBoolean("btn.extra", !extrasShown()).apply();
                        buildButtons();
                        break;
                    case 8:
                        Settings.padSettings(this, "(다음 실행부터)");
                        break;
                    case 9:
                        hidden = true;
                        overlay.setVisibility(View.GONE);
                        break;
                    case 10:
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

    private void chooseSize() {
        String[] sizes = { "작게", "보통", "크게", "아주 크게" };
        new AlertDialog.Builder(this)
                .setTitle("가상 버튼 크기")
                .setSingleChoiceItems(sizes, prefs().getInt("btn.size", 1), (d, which) -> {
                    prefs().edit().putInt("btn.size", which).apply();
                    buildButtons();
                    d.dismiss();
                })
                .show();
    }

    private void chooseAlpha() {
        SeekBar bar = new SeekBar(this);
        bar.setMax(90);
        bar.setProgress(prefs().getInt("btn.alpha", 70) - 10);
        bar.setPadding(dp(24), dp(24), dp(24), dp(24));
        bar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar s, int p, boolean user) {
                prefs().edit().putInt("btn.alpha", p + 10).apply();
                for (Button b : buttons)
                    b.setAlpha((p + 10) / 100f);
            }

            @Override
            public void onStartTrackingTouch(SeekBar s) {}

            @Override
            public void onStopTrackingTouch(SeekBar s) {}
        });
        new AlertDialog.Builder(this)
                .setTitle("가상 버튼 투명도 (왼쪽 = 투명)")
                .setView(bar)
                .setPositiveButton("확인", null)
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
            if (Settings.saveBackground(this, gameId(), data.getData())) {
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
            stopEditing();
            return;
        }
        if (hidden) {
            hidden = false;
            overlay.setVisibility(View.VISIBLE);
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
