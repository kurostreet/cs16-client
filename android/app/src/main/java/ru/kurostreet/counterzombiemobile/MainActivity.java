package ru.kurostreet.counterzombiemobile;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import java.lang.ref.WeakReference;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends Activity {
    private static final ExecutorService WORKER = Executors.newSingleThreadExecutor();
    private static final Handler MAIN = new Handler(Looper.getMainLooper());
    private TextView status;
    private ProgressBar progress;
    private Button retry;
    private GameDataInstaller.Installation installation;
    private boolean resumed;
    private boolean launched;

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);
        layout.setPadding(dp(28), dp(24), dp(28), dp(24));
        layout.setBackgroundColor(Color.rgb(13, 19, 27));
        ImageView logo = new ImageView(this);
        logo.setImageResource(R.mipmap.ic_launcher);
        layout.addView(logo, new LinearLayout.LayoutParams(dp(140), dp(140)));
        TextView title = new TextView(this);
        title.setText(R.string.app_name);
        title.setTextSize(23);
        title.setTextColor(Color.WHITE);
        title.setGravity(Gravity.CENTER);
        title.setPadding(0, dp(20), 0, dp(28));
        layout.addView(title);
        status = new TextView(this);
        status.setTextColor(Color.rgb(186, 214, 232));
        status.setGravity(Gravity.CENTER);
        status.setTextSize(16);
        layout.addView(status);
        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progress.setMax(100);
        LinearLayout.LayoutParams bar = new LinearLayout.LayoutParams(-1, dp(8));
        bar.topMargin = dp(18);
        layout.addView(progress, bar);
        retry = new Button(this);
        retry.setText("Повторить");
        retry.setVisibility(View.GONE);
        retry.setOnClickListener(view -> prepare());
        layout.addView(retry);
        setContentView(layout);
        prepare();
    }

    private void prepare() {
        retry.setVisibility(View.GONE);
        progress.setVisibility(View.VISIBLE);
        progress.setProgress(0);
        status.setText("Готовим первый запуск…");
        Context application = getApplicationContext();
        WeakReference<MainActivity> reference = new WeakReference<>(this);
        WORKER.execute(() -> {
            try {
                GameDataInstaller.Installation result = GameDataInstaller.prepare(application, (message, percent) ->
                        MAIN.post(() -> {
                            MainActivity activity = reference.get();
                            if (activity != null && !activity.isDestroyed()) {
                                activity.status.setText(message + " " + percent + "%");
                                activity.progress.setProgress(percent);
                            }
                        }));
                MAIN.post(() -> {
                    MainActivity activity = reference.get();
                    if (activity != null && !activity.isDestroyed()) {
                        activity.installation = result;
                        activity.launchWhenReady();
                    }
                });
            } catch (Exception e) {
                android.util.Log.e("CzmPreparation", "Preparation failed", e);
                MAIN.post(() -> {
                    MainActivity activity = reference.get();
                    if (activity != null && !activity.isDestroyed()) {
                        activity.status.setText(e.getMessage() == null ? "Не удалось подготовить игру." : e.getMessage());
                        activity.progress.setVisibility(View.GONE);
                        activity.retry.setVisibility(View.VISIBLE);
                    }
                });
            }
        });
    }

    private void launchWhenReady() {
        if (!resumed || launched || installation == null) return;
        launched = true;
        startActivity(new Intent(this, su.xash.engine.XashActivity.class)
                .putExtra("gamedir", "cstrike")
                .putExtra("gamelibdir", getApplicationInfo().nativeLibraryDir)
                .putExtra("basedir", installation.writable.getAbsolutePath())
                .putExtra("package", getPackageName())
                .putExtra("env", new String[]{
                        "XASH3D_RODIR", installation.data.getAbsolutePath(),
                        // the game menu picks Russian or English from the phone language on the first start
                        "CZM_LOCALE", java.util.Locale.getDefault().getLanguage(),
                        // shown in the About panel of the game menu
                        "CZM_APP_VERSION", appVersion()})
                .putExtra("argv", launchArguments()));
        finish();
    }

    private String appVersion() {
        try {
            return getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
        } catch (Exception e) {
            return "";
        }
    }

    // Players get no developer console. For debugging, create files/game/czm-dev.txt
    // (for example with "run-as" or root) to start the engine in developer mode.
    private String launchArguments() {
        boolean developer = installation != null && new java.io.File(installation.writable, "czm-dev.txt").isFile();
        return developer ? "-dev 2 -log -dll @yapb" : "-log -dll @yapb";
    }

    @Override protected void onResume() { super.onResume(); resumed = true; launchWhenReady(); }
    @Override protected void onPause() { resumed = false; super.onPause(); }
    private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }
}
