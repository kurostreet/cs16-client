/* Compact CZM panels shared by account and server credentials. GPL-3.0-or-later. */
package ru.kurostreet.counterzombiemobile;

import android.app.Activity;
import android.app.Dialog;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.text.InputFilter;
import android.text.InputType;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

final class CzmDialog extends Dialog {
    static final int RED = Color.rgb(221, 31, 39), MUTED = Color.rgb(155, 163, 177);
    final Activity activity;
    final LinearLayout body;
    final Button cancel, submit;
    final TextView error;
    private final LinearLayout root;

    static int dp(Activity a, int value) { return Math.round(value * a.getResources().getDisplayMetrics().density); }
    static GradientDrawable shape(Activity a, int color, int border, int radius) {
        GradientDrawable d = new GradientDrawable(); d.setColor(color); d.setCornerRadius(dp(a, radius));
        if (border != 0) d.setStroke(dp(a, 1), border);
        return d;
    }

    CzmDialog(Activity a, String title, String subtitle, String action, String back) {
        super(a); activity = a;
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        root = new LinearLayout(a); root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(a, 18), dp(a, 12), dp(a, 18), dp(a, 12));
        root.setBackground(shape(a, Color.rgb(15, 18, 24), Color.rgb(62, 65, 74), 16));
        root.setFocusableInTouchMode(true);
        TextView heading = text(title, 19, Color.WHITE); heading.setTypeface(Typeface.DEFAULT, Typeface.BOLD_ITALIC);
        root.addView(heading);
        if (!subtitle.isEmpty()) {
            TextView sub = text(subtitle, 12, MUTED); sub.setMaxLines(2); sub.setEllipsize(TextUtils.TruncateAt.END);
            root.addView(sub);
        }
        View line = new View(a); line.setBackgroundColor(RED);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(dp(a, 36), dp(a, 2));
        lp.topMargin = dp(a, 9); lp.bottomMargin = dp(a, 6); root.addView(line, lp);
        ScrollView scroll = new ScrollView(a); scroll.setFillViewport(false);
        body = new LinearLayout(a); body.setOrientation(LinearLayout.VERTICAL);
        scroll.addView(body);
        root.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1));
        error = text("", 12, Color.rgb(255, 137, 137)); error.setVisibility(View.GONE); body.addView(error);
        LinearLayout buttons = new LinearLayout(a); buttons.setGravity(Gravity.END);
        cancel = button(back, false); submit = button(action, true);
        LinearLayout.LayoutParams b = new LinearLayout.LayoutParams(0, dp(a, 44), 1);
        b.topMargin = dp(a, 8); b.rightMargin = dp(a, 8); buttons.addView(cancel, b);
        LinearLayout.LayoutParams s = new LinearLayout.LayoutParams(0, dp(a, 44), 1); s.topMargin = dp(a, 8);
        buttons.addView(submit, s); root.addView(buttons);
        cancel.setOnClickListener(v -> dismiss());
        setContentView(root);
        Window w = getWindow();
        if (w != null) {
            w.setBackgroundDrawableResource(android.R.color.transparent);
            w.addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND | WindowManager.LayoutParams.FLAG_SECURE);
            WindowManager.LayoutParams attrs = w.getAttributes(); attrs.dimAmount = .68f; w.setAttributes(attrs);
            w.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE | WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN);
        }
    }

    TextView text(String value, int size, int color) {
        TextView v = new TextView(activity); v.setText(value); v.setTextSize(size); v.setTextColor(color);
        return v;
    }
    TextView note(String value) {
        TextView v = text(value, 12, MUTED); v.setPadding(0, dp(activity, 6), 0, dp(activity, 4)); body.addView(v); return v;
    }
    Button button(String value, boolean primary) {
        Button b = new Button(activity); b.setText(value); b.setTextSize(13); b.setTextColor(Color.WHITE);
        b.setAllCaps(false); b.setMinHeight(0); b.setMinimumHeight(0); b.setMinWidth(0); b.setMinimumWidth(0);
        b.setPadding(dp(activity, 8), 0, dp(activity, 8), 0);
        b.setBackground(shape(activity, primary ? RED : Color.rgb(26, 30, 38), primary ? 0 : Color.rgb(60, 66, 77), 22));
        return b;
    }
    Button link(String value, View.OnClickListener action) {
        Button b = button(value, false);
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(-1, dp(activity, 42)); p.topMargin = dp(activity, 6);
        body.addView(b, p); b.setOnClickListener(action); return b;
    }
    EditText field(String label, boolean password, int max) {
        note(label);
        EditText e = new EditText(activity); e.setTextSize(15); e.setTextColor(Color.WHITE); e.setHintTextColor(MUTED);
        e.setSingleLine(true); e.setPadding(dp(activity, 12), 0, dp(activity, 12), 0);
        e.setInputType(InputType.TYPE_CLASS_TEXT | (password ? InputType.TYPE_TEXT_VARIATION_PASSWORD : InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS));
        e.setFilters(new InputFilter[] {new InputFilter.LengthFilter(max)});
        e.setBackground(shape(activity, Color.rgb(24, 28, 36), Color.rgb(61, 67, 80), 10));
        if (Build.VERSION.SDK_INT >= 26) e.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO);
        body.addView(e, new LinearLayout.LayoutParams(-1, dp(activity, 44))); return e;
    }
    void fail(String message) { error.setText(message); error.setVisibility(View.VISIBLE); }
    void busy(boolean value) { submit.setEnabled(!value); cancel.setEnabled(!value); setCancelable(!value); }

    @Override public void show() {
        super.show();
        Window window = getWindow();
        if (window == null) return;
        android.util.DisplayMetrics m = activity.getResources().getDisplayMetrics();
        // A bounded compact panel; the body scrolls and the actions remain visible with the IME.
        window.setLayout(Math.min(dp(activity, 440), m.widthPixels - dp(activity, 40)),
            Math.min(dp(activity, 340), m.heightPixels - dp(activity, 32)));
        root.requestFocus();
    }
}
