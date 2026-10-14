package org.libsdl.app;

import android.media.AudioTrack;

/** Gate only this game's output; never alter the phone's media volume. */
public final class CzmAudioGate {
    private CzmAudioGate() {}
    public static void mute(boolean muted) {
        AudioTrack track = SDLAudioManager.mAudioTrack;
        if (track != null) {
            try { track.setVolume(muted ? 0f : 1f); }
            catch (IllegalStateException ignored) { /* SDL may be replacing the track. */ }
        }
    }
}
