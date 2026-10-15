/* Finger-operated crosshair colour wheel. GPL-3.0-or-later. */
package ru.kurostreet.counterzombiemobile;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RadialGradient;
import android.graphics.Shader;
import android.graphics.SweepGradient;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import java.nio.charset.StandardCharsets;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicReference;
import org.libsdl.app.SDLActivity;

final class CzmColorPicker {
    private static CzmDialog editor;
    private static final AtomicReference<byte[]> RESULT = new AtomicReference<>();
    private CzmColorPicker() {}
    static byte[] event() { return RESULT.getAndSet(null); }
    static void show(int red, int green, int blue, boolean ru) {
        Context context = SDLActivity.getContext();
        if (!(context instanceof Activity)) return;
        Activity a = (Activity)context;
        a.runOnUiThread(() -> {
            if (a.isFinishing() || (editor != null && editor.isShowing())) return;
            RESULT.set(null);
            CzmDialog d = new CzmDialog(a, ru ? "Цвет прицела" : "Crosshair color",
                ru ? "Веди пальцем по кругу, затем настрой яркость" : "Drag on the wheel, then adjust brightness",
                ru ? "Сохранить" : "Save", ru ? "Отмена" : "Cancel", ru, false);
            editor = d;
            LinearLayout row = new LinearLayout(a); row.setGravity(Gravity.CENTER_VERTICAL);
            Wheel wheel = new Wheel(a, ColorWheelData.hsv(red, green, blue));
            wheel.setContentDescription(ru ? "Выбор цвета прицела" : "Choose crosshair color");
            row.addView(wheel, new LinearLayout.LayoutParams(0, -1, 1));
            LinearLayout side = new LinearLayout(a); side.setOrientation(LinearLayout.VERTICAL);
            side.setPadding(CzmDialog.dp(a, 12), 0, 0, 0);
            View preview = new View(a) {
                final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
                @Override protected void onDraw(Canvas c) {
                    p.setColor(Color.rgb(29, 34, 44)); c.drawRoundRect(0, 0, getWidth(), getHeight(), 12, 12, p);
                    p.setColor(wheel.color()); p.setStrokeWidth(CzmDialog.dp(a, 2));
                    float cx = getWidth() / 2f, cy = getHeight() / 2f, gap = CzmDialog.dp(a, 4), len = CzmDialog.dp(a, 12);
                    c.drawLine(cx-gap-len, cy, cx-gap, cy, p); c.drawLine(cx+gap, cy, cx+gap+len, cy, p);
                    c.drawLine(cx, cy-gap-len, cx, cy-gap, p); c.drawLine(cx, cy+gap, cx, cy+gap+len, p);
                }
            };
            preview.setContentDescription(ru ? "Предпросмотр прицела" : "Crosshair preview");
            side.addView(preview, new LinearLayout.LayoutParams(-1, CzmDialog.dp(a, 64)));
            TextView hex = d.text("", 15, Color.WHITE); hex.setGravity(Gravity.CENTER); hex.setPadding(0, CzmDialog.dp(a, 8), 0, CzmDialog.dp(a, 12)); side.addView(hex);
            side.addView(d.text(ru ? "Яркость" : "Brightness", 12, CzmDialog.MUTED));
            SeekBar brightness = new SeekBar(a); brightness.setMax(255); brightness.setProgress(Math.round(wheel.hsv[2] * 255));
            brightness.setProgressTintList(android.content.res.ColorStateList.valueOf(CzmDialog.RED));
            brightness.setThumbTintList(android.content.res.ColorStateList.valueOf(Color.WHITE));
            brightness.setContentDescription(ru ? "Яркость цвета прицела" : "Crosshair brightness");
            side.addView(brightness, new LinearLayout.LayoutParams(-1, CzmDialog.dp(a, 44)));
            row.addView(side, new LinearLayout.LayoutParams(CzmDialog.dp(a, 130), -2));
            wheel.changed = () -> {
                hex.setText(String.format(Locale.ROOT, "#%06X", wheel.color() & 0xffffff));
                preview.invalidate();
            };
            brightness.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                @Override public void onProgressChanged(SeekBar bar, int value, boolean user) {
                    wheel.hsv[2] = value / 255f; wheel.invalidate(); wheel.changed.run();
                }
                @Override public void onStartTrackingTouch(SeekBar bar) {}
                @Override public void onStopTrackingTouch(SeekBar bar) {}
            });
            d.body.addView(row, new LinearLayout.LayoutParams(-1, CzmDialog.dp(a, 196)));
            wheel.changed.run();
            d.submit.setOnClickListener(v -> {
                int color = wheel.color();
                RESULT.set((Color.red(color) + " " + Color.green(color) + " " + Color.blue(color)).getBytes(StandardCharsets.UTF_8));
                d.dismiss();
            });
            d.setOnDismissListener(v -> { if (editor == d) editor = null; });
            d.show();
        });
    }

    private static final class Wheel extends View {
        final float[] hsv;
        Runnable changed = () -> {};
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private Shader colors, white;
        private float cx, cy, radius;
        private int pointer = -1;
        Wheel(Activity a, float[] initial) {
            super(a); hsv = initial; setFocusable(true);
        }
        int color() { return ColorWheelData.rgb(hsv[0], hsv[1], hsv[2]); }
        @Override protected void onSizeChanged(int w, int h, int oldw, int oldh) {
            cx = w / 2f; cy = h / 2f; radius = Math.max(1, Math.min(w, h) / 2f - CzmDialog.dp((Activity)getContext(), 10));
            colors = new SweepGradient(cx, cy, new int[]{Color.RED, Color.YELLOW, Color.GREEN, Color.CYAN, Color.BLUE, Color.MAGENTA, Color.RED}, null);
            white = new RadialGradient(cx, cy, radius, Color.WHITE, 0x00ffffff, Shader.TileMode.CLAMP);
        }
        @Override protected void onDraw(Canvas canvas) {
            paint.setStyle(Paint.Style.FILL); paint.setShader(colors); canvas.drawCircle(cx, cy, radius, paint);
            paint.setShader(white); canvas.drawCircle(cx, cy, radius, paint); paint.setShader(null);
            float angle = (float)Math.toRadians(hsv[0]);
            float x = cx + (float)Math.cos(angle) * hsv[1] * radius, y = cy + (float)Math.sin(angle) * hsv[1] * radius;
            // The wheel shows hue/saturation; the separate preview includes brightness.
            paint.setColor(Color.BLACK); canvas.drawCircle(x, y, 8 * getResources().getDisplayMetrics().density, paint);
            paint.setColor(ColorWheelData.rgb(hsv[0], hsv[1], 1)); canvas.drawCircle(x, y, 6 * getResources().getDisplayMetrics().density, paint);
            paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(2 * getResources().getDisplayMetrics().density); paint.setColor(Color.WHITE);
            canvas.drawCircle(x, y, 7 * getResources().getDisplayMetrics().density, paint);
        }
        private void select(float x, float y) {
            float[] selected = ColorWheelData.point(x - cx, y - cy, radius);
            hsv[0] = selected[0]; hsv[1] = selected[1]; invalidate(); changed.run();
        }
        @Override public boolean onTouchEvent(MotionEvent e) {
            switch (e.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    if (Math.hypot(e.getX() - cx, e.getY() - cy) > radius + 12 * getResources().getDisplayMetrics().density) return false;
                    pointer = e.getPointerId(0); getParent().requestDisallowInterceptTouchEvent(true); select(e.getX(), e.getY()); return true;
                case MotionEvent.ACTION_MOVE:
                    int index = e.findPointerIndex(pointer);
                    if (index >= 0) select(e.getX(index), e.getY(index));
                    return pointer >= 0;
                case MotionEvent.ACTION_UP:
                    if (pointer < 0) return false;
                    select(e.getX(), e.getY()); performClick(); pointer = -1;
                    getParent().requestDisallowInterceptTouchEvent(false); return true;
                case MotionEvent.ACTION_POINTER_UP:
                    if (e.getPointerId(e.getActionIndex()) != pointer) return true;
                    // Finish the active finger without switching to another touch.
                case MotionEvent.ACTION_CANCEL:
                    pointer = -1; getParent().requestDisallowInterceptTouchEvent(false); return true;
                default: return pointer >= 0;
            }
        }
        @Override public boolean performClick() { super.performClick(); return true; }
    }
}
