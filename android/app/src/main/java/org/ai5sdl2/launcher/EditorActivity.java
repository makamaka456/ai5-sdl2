// AI5 launcher for Android: edit a game's on-screen buttons without starting
// the game (landscape, black screen, the same editor as the game's ⚙).
// SPDX-License-Identifier: GPL-2.0-or-later
package org.ai5sdl2.launcher;

import android.app.Activity;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.TextView;

public class EditorActivity extends Activity {
    public static final String EXTRA_GAME = "game";
    public static final String EXTRA_NAME = "name";

    private ButtonLayer layer;

    @Override
    @SuppressWarnings("deprecation")
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams lp = getWindow().getAttributes();
            lp.layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(lp);
        }
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);

        String game = getIntent().getStringExtra(EXTRA_GAME);
        if (game == null)
            game = "default";
        FrameLayout root = new FrameLayout(this);
        root.setBackgroundColor(Color.BLACK);

        // where the game picture goes (4:3 in the middle), to help placing buttons
        TextView hint = new TextView(this);
        String name = getIntent().getStringExtra(EXTRA_NAME);
        hint.setText((name != null ? name + "\n" : "") + "가상 버튼 편집 — 게임 화면 대신 까만 화면");
        hint.setTextColor(0xFF666666);
        hint.setTextSize(16);
        hint.setGravity(Gravity.CENTER);
        root.addView(hint, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.CENTER));

        FrameLayout overlay = new FrameLayout(this);
        root.addView(overlay, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
        setContentView(root);
        layer = new ButtonLayer(this, overlay, game, new ButtonLayer.Host() {
            @Override public void keyDown(int k) {}
            @Override public void keyUp(int k) {}
            @Override public void onGear() {}
            @Override public void onEditEnd() { finish(); }
        });
        overlay.post(() -> layer.startEditing());
    }

    @Override
    public void onBackPressed() {
        if (layer != null && layer.editing)
            layer.cancelEditing();
        else
            finish();
    }
}
