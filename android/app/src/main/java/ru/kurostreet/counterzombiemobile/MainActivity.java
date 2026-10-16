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
    private CzmStartupView startup;
    private Button retry;
    private GameDataInstaller.Installation installation;
    private boolean resumed;
    private boolean launched;

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        android.widget.FrameLayout layout = new android.widget.FrameLayout(this);
        startup = new CzmStartupView(this);
        layout.addView(startup, new android.widget.FrameLayout.LayoutParams(-1, -1));
        retry = new Button(this);
        retry.setText(CzmStartupView.russian() ? "Повторить" : "Retry");
        retry.setVisibility(View.GONE);
        retry.setOnClickListener(view -> prepare());
        android.widget.FrameLayout.LayoutParams button = new android.widget.FrameLayout.LayoutParams(-2, -2, Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL);
        button.bottomMargin = dp(12);
        layout.addView(retry, button);
        setContentView(layout);
        prepare();
    }

    private void prepare() {
        retry.setVisibility(View.GONE);
        startup.setProgress(CzmStartupView.russian() ? "Подготовка ресурсов…" : "Preparing resources…", 0);
        Context application = getApplicationContext();
        WeakReference<MainActivity> reference = new WeakReference<>(this);
        WORKER.execute(() -> {
            try {
                GameDataInstaller.Installation result = GameDataInstaller.prepare(application, (message, percent) ->
                        MAIN.post(() -> {
                            MainActivity activity = reference.get();
                            if (activity != null && !activity.isDestroyed()) {
                                activity.startup.setProgress(message, percent);
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
                        activity.startup.setProgress(e.getMessage() == null ? "Не удалось подготовить игру." : e.getMessage(), -2);
                        activity.retry.setVisibility(View.VISIBLE);
                    }
                });
            }
        });
    }

    private void launchWhenReady() {
        if (!resumed || launched || installation == null) return;
        launched = true;
        startActivity(new Intent(this, CzmGameActivity.class)
                .putExtra("czm_server", getIntent().getStringExtra("czm_server"))
                .putExtra("czm_protocol", getIntent().getStringExtra("czm_protocol"))
                .putExtra("gamedir", "cstrike")
                .putExtra("gamelibdir", getApplicationInfo().nativeLibraryDir)
                .putExtra("basedir", installation.writable.getAbsolutePath())
                .putExtra("package", getPackageName())
                .putExtra("env", new String[]{
                        "SDL_ANDROID_BLOCK_ON_PAUSE", "0",
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
