// AI5 launcher for Android: on-screen buttons and their editor, used on the
// game screen and on the launcher's button editor screen.
// SPDX-License-Identifier: GPL-2.0-or-later
package org.ai5sdl2.launcher;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import org.ai5sdl2.launcher.AppSettings.BtnCfg;

import java.util.ArrayList;
import java.util.HashMap;

final class ButtonLayer {
    interface Host {
        void keyDown(int keycode);
        void keyUp(int keycode);
        void onGear();        // the ⚙ button
        void onEditEnd();     // the editor was closed (applied or cancelled)
    }

    private static final String[][] BUTTONS = AppSettings.VBUTTONS;

    private final Activity a;
    private final FrameLayout overlay;
    private final String game;
    private final Host host;
    private final ArrayList<Button> buttons = new ArrayList<>();
    private Button skipButton;
    boolean ctrlHeld = false;

    // button editor: changes are kept here until [적용]
    boolean editing = false;
    private final HashMap<String, BtnCfg> stage = new HashMap<>();
    private String selected;
    private LinearLayout editBar, editPanel;
    private TextView panelTitle;
    private SeekBar alphaBar, sizeBar;
    private boolean panelUpdating;

    ButtonLayer(Activity a, FrameLayout overlay, String game, Host host) {
        this.a = a;
        this.overlay = overlay;
        this.game = game;
        this.host = host;
        overlay.addOnLayoutChangeListener((v, l, t, r, b, ol, ot, or, ob) -> {
            if (r - l != or - ol || b - t != ob - ot)
                v.post(this::placeButtons);
        });
        buildButtons();
    }

    private int dp(float v) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v,
                a.getResources().getDisplayMetrics());
    }

    private static String[] def(String id) {
        return AppSettings.vbutton(id);
    }

    private static BtnCfg defaultCfg(String id) {
        return AppSettings.defaultBtn(id);
    }

    private BtnCfg savedCfg(String id) {
        return AppSettings.btnCfg(a, game, id);
    }

    private BtnCfg cfg(String id) {
        return editing && stage.containsKey(id) ? stage.get(id) : savedCfg(id);
    }

    // on-screen direction stick: a ring with a knob; drags press the arrow keys
    private class StickButton extends Button {
        float kx, ky; // knob offset (-1..1)

        StickButton() {
            super(ButtonLayer.this.a);
        }

        @Override
        protected void onDraw(android.graphics.Canvas cv) {
            float w = getWidth(), h = getHeight(), r = Math.min(w, h) / 2f;
            android.graphics.Paint p = new android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG);
            p.setColor(0xAA000000);
            cv.drawCircle(w / 2, h / 2, r - dp(2), p);
            p.setStyle(android.graphics.Paint.Style.STROKE);
            p.setStrokeWidth(dp(3));
            p.setColor(0xFFC03030);
            cv.drawCircle(w / 2, h / 2, r - dp(3), p);
            p.setStyle(android.graphics.Paint.Style.FILL);
            p.setColor(0xFFE0E0E0);
            float kr = r * 0.38f;
            cv.drawCircle(w / 2 + kx * (r - kr), h / 2 + ky * (r - kr), kr, p);
        }
    }

    private final boolean[] stickKeys = new boolean[4]; // up, down, left, right
    private static final int[] STICK_CODES = {
        KeyEvent.KEYCODE_DPAD_UP, KeyEvent.KEYCODE_DPAD_DOWN,
        KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.KEYCODE_DPAD_RIGHT,
    };

    void setStickKeys(boolean up, boolean down, boolean left, boolean right) {
        boolean[] want = { up, down, left, right };
        for (int i = 0; i < 4; i++) {
            if (want[i] != stickKeys[i]) {
                stickKeys[i] = want[i];
                if (want[i])
                    host.keyDown(STICK_CODES[i]);
                else
                    host.keyUp(STICK_CODES[i]);
            }
        }
    }

    private boolean stickTouch(StickButton v, MotionEvent e) {
        float r = Math.min(v.getWidth(), v.getHeight()) / 2f;
        float dx = (e.getX() - v.getWidth() / 2f) / r, dy = (e.getY() - v.getHeight() / 2f) / r;
        float len = (float) Math.sqrt(dx * dx + dy * dy);
        switch (e.getActionMasked()) {
        case MotionEvent.ACTION_DOWN:
        case MotionEvent.ACTION_MOVE:
            if (len > 1) {
                dx /= len;
                dy /= len;
            }
            v.kx = dx;
            v.ky = dy;
            // 8 directions; a dead zone in the middle
            boolean on = len > 0.3f;
            double ang = Math.atan2(dy, dx); // 0 = right, pi/2 = down
            boolean right = on && Math.abs(ang) < Math.PI * 3 / 8;
            boolean left = on && Math.abs(ang) > Math.PI * 5 / 8;
            boolean down = on && ang > Math.PI / 8 && ang < Math.PI * 7 / 8;
            boolean up = on && ang < -Math.PI / 8 && ang > -Math.PI * 7 / 8;
            setStickKeys(up, down, left, right);
            break;
        case MotionEvent.ACTION_UP:
        case MotionEvent.ACTION_CANCEL:
            v.kx = v.ky = 0;
            setStickKeys(false, false, false, false);
            break;
        }
        v.invalidate();
        return true;
    }

    private Button makeButton(String id, String text, BtnCfg c) {
        if (id.equals("stick")) {
            StickButton b = new StickButton();
            b.setTag(id);
            b.setText("");
            int d = dp(120 * c.size / 100f);
            b.setMinWidth(d);
            b.setMinHeight(d);
            b.setMinimumWidth(d);
            b.setMinimumHeight(d);
            b.setWidth(d);
            b.setHeight(d);
            b.setBackground(null);
            boolean sel = editing && id.equals(selected);
            if (editing)
                b.setAlpha(c.hidden ? 0.3f : (sel ? 1.0f : 0.85f));
            else
                b.setAlpha(c.alpha / 100f);
            return b;
        }
        Button b = new Button(a);
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
    private void tapKey(int keycode) {
        host.keyDown(keycode);
        host.keyUp(keycode);
    }

    // buttons that act while held (key down on touch, key up on release)
    private static int holdKey(String id) {
        switch (id) {
        case "up":    return KeyEvent.KEYCODE_DPAD_UP;
        case "down":  return KeyEvent.KEYCODE_DPAD_DOWN;
        case "left":  return KeyEvent.KEYCODE_DPAD_LEFT;
        case "right": return KeyEvent.KEYCODE_DPAD_RIGHT;
        }
        return 0;
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
        case "gear":  host.onGear(); break;
        }
    }

    void buildButtons() {
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
    void placeButtons() {
        int w = overlay.getWidth(), h = overlay.getHeight();
        if (w == 0 || h == 0)
            return;
        int margin = dp(10);
        for (Button b : buttons) {
            String id = (String) b.getTag();
            String[] d = def(id);
            BtnCfg c = cfg(id);
            // a new button may not be laid out yet: measure it ourselves
            // (with width 0 the right-hand buttons ended up off screen)
            if (b.getWidth() <= 0 || b.getHeight() <= 0)
                b.measure(View.MeasureSpec.makeMeasureSpec(w, View.MeasureSpec.AT_MOST),
                        View.MeasureSpec.makeMeasureSpec(h, View.MeasureSpec.AT_MOST));
            int bw = b.getWidth() > 0 ? b.getWidth() : b.getMeasuredWidth();
            int bh = b.getHeight() > 0 ? b.getHeight() : b.getMeasuredHeight();
            float x, y;
            if (c.x >= 0 && c.y >= 0) {
                x = c.x * w - bw / 2f;
                y = c.y * h - bh / 2f;
            } else if (!d[5].isEmpty()) {
                x = Float.parseFloat(d[5]) * w - bw / 2f;
                y = Float.parseFloat(d[3]) * h - bh / 2f;
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
        String id = (String) v.getTag();
        if (!editing) {
            if (v instanceof StickButton)
                return stickTouch((StickButton) v, e);
            int key = holdKey(id);
            if (key == 0)
                return false;
            switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                v.setPressed(true);
                host.keyDown(key);
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                v.setPressed(false);
                host.keyUp(key);
                break;
            }
            return true;
        }
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

    void startEditing() {
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
        Button b = new Button(a);
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
        editBar = new LinearLayout(a);
        editBar.setOrientation(LinearLayout.HORIZONTAL);
        editBar.setGravity(Gravity.CENTER);
        editBar.setPadding(dp(8), dp(6), dp(8), dp(6));
        editBar.setBackgroundColor(0xCC000000);
        Button apply = barButton("✔ 적용", v -> applyEditing());
        ((GradientDrawable) apply.getBackground()).setColor(0xFFB02828);
        editBar.addView(apply);
        editBar.addView(barButton("취소", v -> cancelEditing()));
        editBar.addView(barButton("표시할 버튼", v -> {
            boolean[] shown = new boolean[BUTTONS.length];
            for (int i = 0; i < BUTTONS.length; i++)
                shown[i] = !stage.get(BUTTONS[i][0]).hidden;
            AppSettings.chooseVisible(a, shown, () -> {
                for (int i = 0; i < BUTTONS.length; i++)
                    stage.get(BUTTONS[i][0]).hidden = !shown[i] && !BUTTONS[i][0].equals("gear");
                buildButtons();
                updatePanel();
            });
        }));
        editBar.addView(barButton("프리셋 저장", v -> AppSettings.askName(a, "가상 버튼 프리셋 저장", name ->
                AppSettings.prefs(a).edit().putString("btnpreset." + name, serializeStage()).apply())));
        editBar.addView(barButton("프리셋 불러오기", v -> AppSettings.choosePreset(a, "btnpreset.",
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
        SeekBar s = new SeekBar(a);
        s.setMax(max);
        s.setLayoutParams(new LinearLayout.LayoutParams(dp(220), ViewGroup.LayoutParams.WRAP_CONTENT));
        return s;
    }

    private TextView label(String text) {
        TextView t = new TextView(a);
        t.setText(text);
        t.setTextColor(Color.WHITE);
        t.setTextSize(15);
        t.setPadding(0, dp(4), dp(8), dp(4));
        return t;
    }

    private LinearLayout row(View a, View b) {
        LinearLayout r = new LinearLayout(a);
        r.setOrientation(LinearLayout.HORIZONTAL);
        r.setGravity(Gravity.CENTER_VERTICAL);
        r.addView(a);
        r.addView(b);
        return r;
    }

    // center panel: settings of the selected button
    private void addEditPanel() {
        editPanel = new LinearLayout(a);
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

    void select(String id) {
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
            panelTitle.setText("버튼을 눌러 고르면 투명도·크기를 바꿀 수 있고, 끌어서 옮길 수 있습니다.\n"
                    + "연한 버튼 = 숨겨진 버튼 ([표시할 버튼]에서 고르기)");
        } else {
            BtnCfg c = stage.get(selected);
            panelTitle.setText("[" + AppSettings.vbuttonName(selected) + "]" + (c.hidden ? " (숨김)" : "")
                    + "\n투명도 " + c.alpha + "%  ·  크기 " + c.size + "%");
            alphaBar.setProgress(c.alpha - 10);
            sizeBar.setProgress(c.size - 60);
        }
        alphaBar.setEnabled(has);
        sizeBar.setEnabled(has);
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

    void applyEditing() {
        for (String[] d : BUTTONS)
            AppSettings.saveBtn(a, game, d[0], stage.get(d[0]));
        endEditing();
    }

    void cancelEditing() {
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
        host.onEditEnd();
    }

    void setSkip(boolean on) {
        ctrlHeld = on;
        if (on)
            host.keyDown(KeyEvent.KEYCODE_CTRL_LEFT);
        else
            host.keyUp(KeyEvent.KEYCODE_CTRL_LEFT);
        if (skipButton != null)
            skipButton.setText(on ? "넘기기 ■" : "넘기기");
    }
}
