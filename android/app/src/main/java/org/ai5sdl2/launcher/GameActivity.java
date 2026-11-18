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
import org.ai5sdl2.launcher.AppSettings.BtnCfg;

import java.util.ArrayList;
import java.util.HashMap;

public class GameActivity extends SDLActivity {
    public static final String EXTRA_PATH = "path";
    public static final String EXTRA_ARGS = "args";
    public static final String EXTRA_GAME = "game";
    private static final int REQ_IMAGE = 10;

    private FrameLayout overlay;

    private ButtonLayer layer;

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
        // saves go to <game folder>/launcher-save, apart from the PC saves
        nativeSetenv("AI5_SAVE_DIR", "launcher-save");
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
        nativeSetenv("AI5_STRETCH", AppSettings.stretch(this, gameId()) ? "1" : "0");
        nativeSetenv("AI5_BACKGROUND", AppSettings.backgroundEnv(this, gameId()));
        nativeSetenv("AI5_OPEN_ALL", AppSettings.openAll(this, gameId()) ? "1" : "0");
    }

    private void applyPadSettings() {
        nativeSetenv("AI5_CONTROLLER", AppSettings.controllerConfig(this, gameId()));
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
                overlay.postDelayed(() -> { layer.startEditing(); layer.select("skip"); }, 3000);
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

    private void addControls() {
        overlay = new FrameLayout(this);
        mLayout.addView(overlay, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        layer = new ButtonLayer(this, overlay, gameId(), new ButtonLayer.Host() {
            @Override public void keyDown(int k) { onNativeKeyDown(k); }
            @Override public void keyUp(int k) { onNativeKeyUp(k); }
            @Override public void onGear() { showSettings(); }
            @Override public void onEditEnd() {}
        });
    }

    // ------------------------------------------------------------ settings (⚙)

    private void showSettings() {
        boolean oa = AppSettings.openAll(this, gameId());
        boolean hasOa = AppSettings.supportsOpenAll(gameId());
        String[] items = {
            "🖥  화면 설정",
            "🔘  가상 버튼 (위치·크기·투명도·표시할 버튼)",
            "🎮  키맵핑 (게임패드)",
            hasOa ? "🔓  회상·감상 처음부터 열기: " + (oa ? "켜짐" : "꺼짐") + "  (누르면 바꾸기)"
                  : "🔓  회상·감상 처음부터 열기: 이 게임은 아직 지원 안 함",
            "⏻  게임 끝내기",
        };
        new AlertDialog.Builder(this)
                .setTitle("설정")
                .setItems(items, (d, which) -> {
                    switch (which) {
                    case 0: screenSettings(); break;
                    case 1: layer.startEditing(); break;
                    case 2: AppSettings.padSettings(this, gameId(), this::applyPadSettings); break;
                    case 3:
                        if (hasOa) {
                            AppSettings.setOpenAll(this, gameId(), !oa);
                            applyDisplaySettings();
                        }
                        showSettings();
                        break;
                    case 4:
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
        boolean stretch = AppSettings.stretch(this, gameId());
        boolean hasBg = !AppSettings.backgroundPath(this, gameId()).isEmpty();
        String[] items = {
            "화면 비율:  " + (stretch ? "16:9 꽉 채우기  (누르면 → 4:3)" : "4:3 원래 비율  (누르면 → 16:9)"),
            hasBg ? "배경화면 바꾸기 (4:3일 때 양옆에 보임)" : "배경화면 고르기 (4:3일 때 양옆에 보임)",
            hasBg ? "배경화면 없애기" : "배경화면 없애기  (지금 없음)",
        };
        new AlertDialog.Builder(this)
                .setTitle("화면 설정 — 바로 적용")
                .setItems(items, (d, which) -> {
                    switch (which) {
                    case 0:
                        AppSettings.setStretch(this, gameId(), !stretch);
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
            if (AppSettings.saveBackground(this, gameId(), data.getData()))
                applyDisplaySettings(); // the value carries the file time: the engine reloads it
        }
    }

    private void quitGame() {
        finish();
        // the engine runs in its own process (":game")
        android.os.Process.killProcess(android.os.Process.myPid());
    }

    @Override
    public void onBackPressed() {
        if (layer != null && layer.editing) {
            layer.cancelEditing();
            return;
        }
        // back = the game's menu button (ESC, or a right / middle click)
        if (layer != null) {
            layer.backAction();
            return;
        }
        onNativeKeyDown(KeyEvent.KEYCODE_ESCAPE);
        onNativeKeyUp(KeyEvent.KEYCODE_ESCAPE);
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
        if (layer != null) {
            if (layer.ctrlHeld)
                layer.setSkip(false);
            layer.setStickKeys(false, false, false, false);
        }
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        // make sure the engine process does not linger
        android.os.Process.killProcess(android.os.Process.myPid());
    }
}
