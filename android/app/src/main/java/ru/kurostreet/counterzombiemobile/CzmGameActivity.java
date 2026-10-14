package ru.kurostreet.counterzombiemobile;

import android.content.Context;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.ViewGroup;
import org.libsdl.app.CzmAudioGate;
import org.libsdl.app.SDLActivity;
import su.xash.engine.XashActivity;

/** Keep the startup artwork above SDL until the native menu has drawn. */
public final class CzmGameActivity extends XashActivity implements AudioManager.OnAudioFocusChangeListener {
    private final Handler ui = new Handler(Looper.getMainLooper());
    private CzmStartupView startup;
    private AudioManager audio;
    private boolean foreground, focus, ready;
    private final Runnable silence = new Runnable() {
        @Override public void run() {
            if (foreground && !focus) {
                CzmAudioGate.mute(true);
                ui.postDelayed(this, 200);
            }
        }
    };

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        audio = (AudioManager)getSystemService(Context.AUDIO_SERVICE);
        if (!SDLActivity.mBrokenLibraries) {
            startup = new CzmStartupView(this);
            startup.setClickable(true);
            addContentView(startup, new ViewGroup.LayoutParams(-1, -1));
        }
    }

    public void menuReady() {
        ui.post(() -> {
            if (ready || startup == null || isFinishing()) return;
            ready = true;
            // Let the first native frame reach the surface before fading its cover.
            startup.postOnAnimation(() -> startup.postOnAnimation(() -> {
                if (startup == null) return;
                startup.animate().alpha(0f).setDuration(220).withEndAction(() -> {
                    if (startup != null && startup.getParent() instanceof ViewGroup)
                        ((ViewGroup)startup.getParent()).removeView(startup);
                    startup = null;
                }).start();
            }));
        });
    }

    private void updateAudio() {
        ui.removeCallbacks(silence);
        CzmAudioGate.mute(!foreground || !focus);
        if (foreground && !focus) ui.post(silence);
    }

    @Override protected void onResume() {
        super.onResume();
        foreground = true;
        focus = audio == null || audio.requestAudioFocus(this, AudioManager.STREAM_MUSIC,
                AudioManager.AUDIOFOCUS_GAIN) == AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
        updateAudio();
    }

    @Override protected void onPause() {
        foreground = false;
        updateAudio();
        if (audio != null) audio.abandonAudioFocus(this);
        focus = false;
        super.onPause();
    }

    @Override public void onAudioFocusChange(int change) {
        focus = change == AudioManager.AUDIOFOCUS_GAIN;
        updateAudio();
    }

    @Override public void onDestroy() {
        ui.removeCallbacksAndMessages(null);
        if (audio != null) audio.abandonAudioFocus(this);
        super.onDestroy();
    }
}
