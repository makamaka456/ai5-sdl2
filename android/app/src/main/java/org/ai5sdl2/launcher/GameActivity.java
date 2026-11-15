// AI5 launcher for Android: the game screen (SDL + the ai5-sdl2 engine).
// SPDX-License-Identifier: GPL-2.0-or-later
package org.ai5sdl2.launcher;

import android.app.AlertDialog;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

import org.libsdl.app.SDLActivity;

import java.util.ArrayList;

public class GameActivity extends SDLActivity {
    public static final String EXTRA_PATH = "path";
    public static final String EXTRA_ARGS = "args";
    public static final String EXTRA_STRETCH = "stretch";
    public static final String EXTRA_CONTROLLER = "controller";

    private LinearLayout controls;
    private boolean ctrlHeld = false;
    private Button skipButton;

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "SDL2_ttf", "SDL2_mixer", "main" };
    }

    @Override
    protected String[] getArguments() {
        // fonts are copied to <files>/fonts by the launcher
        nativeSetenv("AI5_DATA_DIR", getFilesDir().getAbsolutePath());
        // a tap is a left click; the back button is handled below
        nativeSetenv("SDL_ANDROID_TRAP_BACK_BUTTON", "1");
        // fill the whole screen (ignore the 4:3 aspect ratio) if chosen in the launcher
        String pad = getIntent().getStringExtra(EXTRA_CONTROLLER);
        if (pad != null)
            nativeSetenv("AI5_CONTROLLER", pad);
        nativeSetenv("AI5_STRETCH", getIntent().getBooleanExtra(EXTRA_STRETCH, true) ? "1" : "0");
        ArrayList<String> args = new ArrayList<>();
        String[] extra = getIntent().getStringArrayExtra(EXTRA_ARGS);
        if (extra != null) {
            for (String a : extra)
                args.add(a);
        }
        args.add(getIntent().getStringExtra(EXTRA_PATH));
        return args.toArray(new String[0]);
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

    private Button makeButton(String text) {
        Button b = new Button(this);
        b.setText(text);
        b.setTextColor(Color.WHITE);
        b.setTextSize(14);
        b.setAllCaps(false);
        b.setPadding(16, 4, 16, 4);
        b.setMinWidth(0);
        b.setMinHeight(0);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(0x88000000);
        bg.setStroke(2, 0xAAC03030);
        bg.setCornerRadius(12);
        b.setBackground(bg);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.setMargins(6, 6, 6, 6);
        b.setLayoutParams(lp);
        return b;
    }

    // press and release a key in the engine
    private static void tapKey(int keycode) {
        onNativeKeyDown(keycode);
        onNativeKeyUp(keycode);
    }

    private void addControls() {
        controls = new LinearLayout(this);
        controls.setOrientation(LinearLayout.VERTICAL);
        controls.setGravity(Gravity.END);

        // right click: opens the system menu / cancels (ESC)
        Button menu = makeButton("메뉴");
        menu.setOnClickListener(v -> tapKey(KeyEvent.KEYCODE_ESCAPE));
        controls.addView(menu);

        // skip: holds Ctrl while active
        skipButton = makeButton("넘기기");
        skipButton.setOnClickListener(v -> setSkip(!ctrlHeld));
        controls.addView(skipButton);

        // hide the message window (Space)
        Button hide = makeButton("숨기기");
        hide.setOnClickListener(v -> tapKey(KeyEvent.KEYCODE_SPACE));
        controls.addView(hide);

        // backlog (Backspace)
        Button log = makeButton("지난 대사");
        log.setOnClickListener(v -> tapKey(KeyEvent.KEYCODE_DEL));
        controls.addView(log);

        Button more = makeButton("⋮");
        more.setOnClickListener(v -> showMoreMenu());
        controls.addView(more);

        RelativeLayout.LayoutParams lp = new RelativeLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.addRule(RelativeLayout.ALIGN_PARENT_END);
        lp.addRule(RelativeLayout.CENTER_VERTICAL);
        mLayout.addView(controls, lp);
        // keep the buttons away from rounded corners and camera cutouts
        controls.setOnApplyWindowInsetsListener((v, insets) -> {
            int right = 8;
            if (Build.VERSION.SDK_INT >= 28 && insets.getDisplayCutout() != null)
                right += insets.getDisplayCutout().getSafeInsetRight();
            v.setPadding(0, 0, right, 0);
            return insets;
        });
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

    private void showMoreMenu() {
        String[] items = { "화면 버튼 숨기기 (뒤로 가기로 다시 표시)", "엔터 (결정)",
                "Shift (화면 효과 빨리)", "게임 끝내기" };
        new AlertDialog.Builder(this)
                .setItems(items, (d, which) -> {
                    if (which == 0) {
                        controls.setVisibility(View.GONE);
                    } else if (which == 1) {
                        tapKey(KeyEvent.KEYCODE_ENTER);
                    } else if (which == 2) {
                        tapKey(KeyEvent.KEYCODE_SHIFT_LEFT);
                    } else {
                        new AlertDialog.Builder(this)
                                .setMessage("게임을 끝낼까요? 저장하지 않은 진행은 사라집니다.")
                                .setPositiveButton("끝내기", (d2, w) -> quitGame())
                                .setNegativeButton("취소", null)
                                .show();
                    }
                })
                .show();
    }

    private void quitGame() {
        finish();
        // the engine runs in its own process (":game")
        android.os.Process.killProcess(android.os.Process.myPid());
    }

    @Override
    public void onBackPressed() {
        if (controls != null && controls.getVisibility() != View.VISIBLE) {
            controls.setVisibility(View.VISIBLE);
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
