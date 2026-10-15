/* Server credential storage and editor. GPL-3.0-or-later. */
package ru.kurostreet.counterzombiemobile;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.security.KeyPairGeneratorSpec;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;
import android.view.View;
import android.widget.EditText;
import android.widget.Toast;
import java.math.BigInteger;
import java.security.KeyPairGenerator;
import java.security.KeyStore;
import java.security.PrivateKey;
import java.util.Calendar;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import javax.security.auth.x500.X500Principal;
import org.libsdl.app.SDLActivity;

public final class CzmServerAuth {
    private static final String ALIAS = "czm.server.credentials.v1";
    private static final ExecutorService WORK = Executors.newSingleThreadExecutor();
    private static CzmDialog editor;
    private static long warnedAt;
    private CzmServerAuth() {}

    private static SharedPreferences prefs(Context context) {
        return context.getSharedPreferences("server_credentials", Context.MODE_PRIVATE);
    }

    private static KeyStore keys(Context context, boolean create) throws Exception {
        KeyStore store = KeyStore.getInstance("AndroidKeyStore"); store.load(null);
        if (!store.containsAlias(ALIAS) && create) {
            Calendar start = Calendar.getInstance(); start.add(Calendar.DAY_OF_YEAR, -1);
            Calendar end = Calendar.getInstance(); end.add(Calendar.YEAR, 30);
            KeyPairGenerator generator = KeyPairGenerator.getInstance("RSA", "AndroidKeyStore");
            if (Build.VERSION.SDK_INT >= 23) {
                generator.initialize(new KeyGenParameterSpec.Builder(ALIAS, KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                    .setKeySize(2048).setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_RSA_PKCS1)
                    .setCertificateSubject(new X500Principal("CN=Counter Zombie Mobile")).setCertificateSerialNumber(BigInteger.ONE)
                    .setCertificateNotBefore(start.getTime()).setCertificateNotAfter(end.getTime()).build());
            } else {
                generator.initialize(new KeyPairGeneratorSpec.Builder(context).setAlias(ALIAS).setKeySize(2048)
                    .setSubject(new X500Principal("CN=Counter Zombie Mobile")).setSerialNumber(BigInteger.ONE)
                    .setStartDate(start.getTime()).setEndDate(end.getTime()).build());
            }
            generator.generateKeyPair();
        }
        return store;
    }

    private static synchronized String[] load(Context context, String address) throws Exception {
        String saved = prefs(context).getString("entry:" + address, "");
        if (saved.isEmpty()) return new String[] {"_pw", ""};
        String[] parts = saved.split("\\.", -1);
        if (parts.length != 3) throw new IllegalArgumentException();
        byte[][] data = new byte[3][];
        for (int i = 0; i < 3; ++i) data[i] = Base64.decode(parts[i], Base64.NO_WRAP);
        return ServerAuthData.open((PrivateKey)keys(context, false).getKey(ALIAS, null), address, data);
    }

    private static synchronized boolean save(Context context, String address, String parameter, String password) throws Exception {
        SharedPreferences preferences = prefs(context);
        SharedPreferences.Editor edit = preferences.edit();
        if (password.isEmpty()) return edit.remove("entry:" + address).commit();
        KeyStore store = keys(context, true);
        byte[][] data = ServerAuthData.seal(store.getCertificate(ALIAS).getPublicKey(), address, parameter, password);
        String encrypted = Base64.encodeToString(data[0], Base64.NO_WRAP) + "." + Base64.encodeToString(data[1], Base64.NO_WRAP)
            + "." + Base64.encodeToString(data[2], Base64.NO_WRAP);
        Set<String> managed = new HashSet<>(preferences.getStringSet("managed", new HashSet<String>()));
        managed.add(parameter); // Retain old field names after edits/deletion, so they cannot leak from legacy cfg.
        return edit.putString("entry:" + address, encrypted).putStringSet("managed", managed).commit();
    }

    // Separate scope and prefs: the account session never enters a game server's userinfo.
    static synchronized String session(Context context) throws Exception {
        String raw = context.getSharedPreferences("czm_account", Context.MODE_PRIVATE).getString("session", "");
        if (raw.isEmpty()) return "";
        String[] parts = raw.split("\\.", -1);
        if (parts.length != 3) throw new IllegalArgumentException();
        byte[][] data = new byte[3][];
        for (int i = 0; i < 3; ++i) data[i] = Base64.decode(parts[i], Base64.NO_WRAP);
        return ServerAuthData.open((PrivateKey)keys(context, false).getKey(ALIAS, null), "czm-account-session-v1", data)[1];
    }

    static synchronized void session(Context context, String token) throws Exception {
        SharedPreferences.Editor edit = context.getSharedPreferences("czm_account", Context.MODE_PRIVATE).edit();
        if (token.isEmpty()) {
            if (!edit.remove("session").commit()) throw new IllegalStateException();
            return;
        }
        byte[][] data = ServerAuthData.seal(keys(context, true).getCertificate(ALIAS).getPublicKey(), "czm-account-session-v1", "_session", token);
        String value = Base64.encodeToString(data[0], Base64.NO_WRAP) + "." + Base64.encodeToString(data[1], Base64.NO_WRAP)
            + "." + Base64.encodeToString(data[2], Base64.NO_WRAP);
        if (!edit.putString("session", value).commit()) throw new IllegalStateException();
    }

    /** Called by the engine at the actual outbound connect packet, never by a game server. */
    public static synchronized String userinfo(String address, String base, int limit) {
        Context context = SDLActivity.getContext();
        if (context == null) return base;
        Set<String> managed = new HashSet<>(prefs(context).getStringSet("managed", new HashSet<String>()));
        String parameter = "", password = "";
        try {
            String[] credential = load(context, ServerAuthData.address(address));
            parameter = credential[0]; password = credential[1];
        } catch (Exception ignored) { /* Fail closed; never reuse another destination's credential. */ }
        String wire = ServerAuthData.packet(base, managed, parameter, password, limit);
        if (!password.isEmpty() && !wire.endsWith("\\" + parameter + "\\" + password)
                && android.os.SystemClock.elapsedRealtime() - warnedAt > 10000) {
            warnedAt = android.os.SystemClock.elapsedRealtime();
            if (context instanceof Activity) ((Activity)context).runOnUiThread(() -> Toast.makeText(context,
                "Admin/VIP: данные подключения слишком длинные / connection data too long", Toast.LENGTH_LONG).show());
        }
        return wire;
    }

    /** An Android password field prevents keyboard suggestions and masks the secret. */
    public static void show(final String endpoint, final String name, final boolean ru) {
        Context context = SDLActivity.getContext();
        if (!(context instanceof Activity)) return;
        final Activity activity = (Activity)context;
        activity.runOnUiThread(() -> {
            if (activity.isFinishing() || (editor != null && editor.isShowing())) return;
            final String address;
            try { address = ServerAuthData.address(endpoint); }
            catch (Exception e) { Toast.makeText(activity, ru ? "Неверный адрес сервера" : "Invalid server address", Toast.LENGTH_LONG).show(); return; }
            CzmDialog dialog = new CzmDialog(activity, ru ? "Админка/VIP" : "Admin/VIP", name + " · " + address,
                ru ? "Сохранить" : "Save", ru ? "Отмена" : "Cancel", ru);
            editor = dialog;
            EditText parameter = dialog.field(ru ? "Параметр сервера" : "Server parameter", false, 31);
            EditText password = dialog.field(ru ? "Пароль админа/VIP" : "Admin/VIP password", true, 63);
            try {
                String[] data = load(activity, address); parameter.setText(data[0]); password.setText(data[1]);
            } catch (Exception e) {
                parameter.setText("_pw");
                password.setError(ru ? "Сохранённый пароль недоступен. Введите заново." : "Saved password is unavailable. Enter it again.");
            }
            dialog.note(ru ? "Только для этого сервера. Пустой пароль удаляет сохранение."
                : "Only for this IP and port. An empty password removes the setting.");
            dialog.setOnDismissListener(d -> { password.setText(""); editor = null; });
            dialog.submit.setOnClickListener(v -> {
                    final String field = parameter.getText().toString(), secret = password.getText().toString();
                    if (!secret.isEmpty() && !ServerAuthData.key(field)) {
                        parameter.setError(ru ? "Начните с _, затем буквы, цифры или _" : "Start with _, then letters, digits or _"); return;
                    }
                    if (!secret.isEmpty() && !ServerAuthData.password(secret)) {
                        password.setError(ru ? "До 63 байт, без кавычек, точки с запятой и обратного слеша" : "Up to 63 bytes; no quotes, semicolon or backslash"); return;
                    }
                    dialog.busy(true);
                    WORK.execute(() -> {
                        boolean ok;
                        try { ok = save(activity, address, field, secret); } catch (Exception e) { ok = false; }
                        final boolean saved = ok;
                        activity.runOnUiThread(() -> {
                            if (!dialog.isShowing()) return;
                            if (saved) {
                                dialog.dismiss(); Toast.makeText(activity, ru ? "Сохранено. Применится при следующем подключении."
                                    : "Saved. Applies on the next connection.", Toast.LENGTH_LONG).show();
                            } else {
                                dialog.busy(false);
                                password.setError(ru ? "Не удалось сохранить. Попробуйте ещё раз." : "Could not save. Please try again.");
                            }
                        });
                    });
            });
            dialog.show();
        });
    }
}
