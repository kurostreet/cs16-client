package ru.kurostreet.counterzombiemobile;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.os.SystemClock;
import android.view.View;
import java.io.InputStream;
import java.util.Locale;

/** One continuous branded screen for resource preparation and engine startup. */
public final class CzmStartupView extends View {
    private static Bitmap background;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final RectF rect = new RectF();
    private final long start = SystemClock.uptimeMillis();
    private int progress = -1;
    private String status;

    public static boolean russian() {
        String lang = Locale.getDefault().getLanguage();
        return lang.equals("ru") || lang.equals("uk") || lang.equals("be") || lang.equals("kk");
    }

    public CzmStartupView(Context context) {
        super(context);
        setBackgroundColor(Color.rgb(10, 13, 19));
        status = russian() ? "Подготовка игры…" : "Preparing game…";
        if (background == null) {
            try (InputStream in = context.getAssets().open("gfx/czm/menu_bg.png")) {
                BitmapFactory.Options options = new BitmapFactory.Options();
                options.inSampleSize = 2;
                background = BitmapFactory.decodeStream(in, null, options);
            } catch (Exception ignored) { /* The gradient and title also work without an image. */ }
        }
    }

    public void setProgress(String message, int percent) {
        status = message;
        progress = Math.max(-2, Math.min(100, percent));
        invalidate();
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        float w = getWidth(), h = getHeight();
        if (w <= 0 || h <= 0) return;
        float t = (SystemClock.uptimeMillis() - start) / 1000f;
        if (background != null) {
            float scale = Math.max(w / background.getWidth(), h / background.getHeight());
            float bw = background.getWidth() * scale, bh = background.getHeight() * scale;
            rect.set((w - bw) / 2, (h - bh) / 2, (w + bw) / 2, (h + bh) / 2);
            paint.setShader(null);
            paint.setColor(Color.WHITE);
            canvas.drawBitmap(background, null, rect, paint);
        }
        paint.setShader(new LinearGradient(0, 0, 0, h,
                new int[]{0x65050a12, 0x38080a10, 0xf0080b12}, null, Shader.TileMode.CLAMP));
        canvas.drawRect(0, 0, w, h, paint);
        paint.setShader(null);
        // Small deterministic embers: no video decoder or extra startup dependency.
        for (int i = 0; i < 16; i++) {
            float x = w * ((i * .6180339f) % 1f);
            float y = h * (1f - ((i * .137f + t * .045f) % 1f));
            paint.setColor(Color.argb(35 + i * 5, 255, 90 + i * 4, 35));
            canvas.drawCircle(x, y, Math.max(1, h * (.0015f + (i % 3) * .0008f)), paint);
        }
        float titleSize = Math.min(h * .115f, w * .075f);
        paint.setTypeface(Typeface.create("sans-serif-condensed", Typeface.BOLD_ITALIC));
        paint.setTextAlign(Paint.Align.CENTER);
        paint.setTextSize(titleSize);
        paint.setShadowLayer(h * .025f, 0, 0, Color.argb(70 + (int)(25 * Math.sin(t * 1.7f)), 225, 35, 45));
        paint.setColor(Color.WHITE);
        canvas.drawText("COUNTER", w / 2, h * .41f, paint);
        paint.setColor(Color.rgb(239, 38, 48));
        canvas.drawText("ZOMBIE", w / 2, h * .41f + titleSize * 1.03f, paint);
        paint.clearShadowLayer();
        paint.setTypeface(Typeface.create("sans-serif", Typeface.BOLD));
        paint.setTextSize(titleSize * .22f);
        paint.setColor(Color.rgb(247, 198, 93));
        canvas.drawText("MOBILE  ·  32 PLAYERS", w / 2, h * .41f + titleSize * 1.48f, paint);
        float left = w * .18f, right = w * .82f, top = h * .84f;
        paint.setTypeface(Typeface.create("sans-serif", Typeface.NORMAL));
        paint.setTextSize(Math.min(h * .032f, w * .03f));
        paint.setColor(Color.rgb(222, 228, 239));
        String label = status == null ? "" : status;
        if (progress >= 0) label += "  " + progress + "%";
        while (label.length() > 1 && paint.measureText(label) > right - left)
            label = label.substring(0, label.length() - 1);
        canvas.drawText(label, w / 2, top - h * .03f, paint);
        if (progress != -2) {
            float radius = Math.max(2, h * .005f);
            paint.setColor(0x42ffffff);
            rect.set(left, top, right, top + radius * 2);
            canvas.drawRoundRect(rect, radius, radius, paint);
            paint.setColor(Color.rgb(235, 40, 52));
            float a = left, b = left + (right - left) * progress / 100f;
            if (progress < 0) {
                float length = (right - left) * .22f;
                a = left - length + ((t * .48f) % 1f) * (right - left + length);
                b = Math.min(right, a + length);
                a = Math.max(left, a);
            }
            rect.set(a, top, Math.max(a, b), top + radius * 2);
            canvas.drawRoundRect(rect, radius, radius, paint);
        }
        if (isShown()) postInvalidateDelayed(33);
    }
}
