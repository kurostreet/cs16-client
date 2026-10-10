package su.xash.engine.util;

import android.content.Context;
import android.os.Build;
import android.util.Log;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

/** Local diagnostics bridge for the embedded upstream activity. No automatic uploads. */
public final class CrashReports {
    public static final String STACKTRACE_NAME = "crash.log";
    private CrashReports() {}
    public static void writeSystemInfo(Context context) {
        write(context, "sysinfo.txt", "Package: " + context.getPackageName() + "\nAndroid: "
                + Build.VERSION.RELEASE + "\nModel: " + Build.MODEL + "\n");
    }
    public static void writeIntentInfo(Context context, String info) { write(context, "intent.txt", info); }
    private static void write(Context context, String name, String info) {
        File directory = new File(context.getFilesDir(), "crashes");
        if (!directory.isDirectory() && !directory.mkdirs()) return;
        try (FileOutputStream output = new FileOutputStream(new File(directory, name))) {
            output.write(info.getBytes(StandardCharsets.UTF_8));
        } catch (IOException e) { Log.w("CzmDiagnostics", "Cannot save diagnostic", e); }
    }
}
