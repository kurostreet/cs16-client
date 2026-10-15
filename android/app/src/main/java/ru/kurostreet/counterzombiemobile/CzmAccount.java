/* Account forms, HTTPS sessions and game-thread profile handoff. GPL-3.0-or-later. */
package ru.kurostreet.counterzombiemobile;

import android.app.Activity;
import android.content.Context;
import android.text.InputType;
import android.view.View;
import android.widget.EditText;
import android.widget.Toast;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import org.json.JSONObject;
import org.libsdl.app.SDLActivity;

final class CzmAccount {
    private static final String API = "https://counter-zombie.ru/api/v1/auth/";
    private static final ExecutorService WORK = Executors.newSingleThreadExecutor();
    private static String token = "", event = "";
    private static boolean loaded, synced;
    private static int generation;
    private static CzmDialog editor;
    private CzmAccount() {}

    static boolean serviceUrl(String address) {
        try {
            URL url = new URL(address);
            return url.getProtocol().equals("https") && (url.getPort() == -1 || url.getPort() == 443)
                && url.getUserInfo() == null && (url.getHost().equals("counter-zombie.ru") || url.getHost().equals("kontrafps.ru"));
        } catch (Exception e) { return false; }
    }

    private static synchronized void load() {
        if (loaded) return;
        Context ctx = SDLActivity.getContext();
        if (ctx == null) return;
        try { token = CzmServerAuth.session(ctx); }
        catch (Exception e) { token = ""; } // The UI can replace an unreadable session by logging in again.
        loaded = true;
    }

    static synchronized int generation() { load(); return generation; }

    private static JSONObject object(String response) throws Exception {
        int line = response.indexOf('\n');
        if (line < 0) throw new IllegalArgumentException();
        return new JSONObject(response.substring(line + 1));
    }
    private static int status(String response) {
        try { return Integer.parseInt(response.substring(0, response.indexOf('\n'))); }
        catch (Exception e) { return 0; }
    }
    private static String raw(String action, String credential, JSONObject body) {
        return CzmNet.call("POST", API + action, credential, body.toString().getBytes(StandardCharsets.UTF_8), "application/json");
    }
    private static synchronized void profile(JSONObject profile) {
        event = new JSONObject().toString();
        try { event = new JSONObject().put("profile", profile).toString(); }
        catch (Exception ignored) {}
        synced = true;
    }
    private static synchronized void clear(String expected) throws Exception {
        if (!token.equals(expected)) return;
        CzmServerAuth.session(SDLActivity.getContext(), "");
        token = ""; synced = false; ++generation; event = "{\"guest\":true}";
    }
    private static void expired(String expected) {
        try { clear(expected); } catch (Exception ignored) {}
    }

    /** Native receives public profile data only. Session tokens never enter cfg, cvars or userinfo. */
    static synchronized byte[] event() {
        load();
        if (event.isEmpty()) return null;
        byte[] result = event.getBytes(StandardCharsets.UTF_8); event = ""; return result;
    }

    /** Captured generation prevents an old request from being attributed to a new account. */
    static String call(int expectedGeneration, String method, String url, String legacy, byte[] body, String contentType) {
        final String credential;
        final boolean needsSync, hello;
        synchronized (CzmAccount.class) {
            load();
            if (expectedGeneration != generation) return "409\n";
            credential = token;
            hello = serviceUrl(url) && url.endsWith("/api/v1/hello");
            needsSync = hello && !credential.isEmpty() && !synced;
        }
        if (needsSync) {
            String me = raw("me", credential, new JSONObject());
            synchronized (CzmAccount.class) {
                if (!token.equals(credential) || generation != expectedGeneration) return "409\n";
                try { if (status(me) == 200) profile(object(me)); }
                catch (Exception ignored) { return "0\n"; }
            }
            if (status(me) == 401) expired(credential);
            return status(me) == 200 ? "409\n" : me;
        }
        String key = legacy;
        if (!credential.isEmpty() && serviceUrl(url) && (hello || (legacy != null && !legacy.isEmpty()))) {
            key = credential;
            synchronized (CzmAccount.class) { if (hello && !event.isEmpty()) return "409\n"; }
            if (hello) {
                try { body = new JSONObject(new String(body, StandardCharsets.UTF_8)).put("key", credential).toString().getBytes(StandardCharsets.UTF_8); }
                catch (Exception e) { return "0\n"; }
            }
        }
        String result = CzmNet.call(method, url, key, body, contentType);
        synchronized (CzmAccount.class) { if (generation != expectedGeneration) return "409\n"; }
        if (status(result) == 401 && !credential.isEmpty()) expired(credential);
        else if (hello && status(result) == 401) {
            synchronized (CzmAccount.class) { event = "{\"guest\":true}"; ++generation; }
        }
        return result;
    }

    private static String error(String code, boolean ru) {
        switch (code) {
        case "credentials": return ru ? "Неверный ник для входа или пароль." : "Incorrect login or password.";
        case "taken": return ru ? "Этот ник для входа уже занят." : "This login is already taken.";
        case "nick": return ru ? "Ник: 2–24 символа, без команд и служебных знаков." : "Login: 2–24 characters, no commands or control characters.";
        case "password": return ru ? "Пароль: от 8 до 128 символов." : "Password: 8–128 characters.";
        case "repeat": return ru ? "Пароли не совпадают." : "Passwords do not match.";
        case "slow": return ru ? "Слишком много попыток. Попробуйте позже." : "Too many attempts. Try again later.";
        case "mail_unavailable": return ru ? "Отправка писем ещё не настроена." : "Email delivery has not been configured yet.";
        case "email": return ru ? "Проверьте адрес почты." : "Check the email address.";
        case "email_used": return ru ? "Эта почта привязана к другому аккаунту." : "This email belongs to another account.";
        case "code": return ru ? "Код неверен или истёк. Запросите новый." : "Invalid or expired code. Request a new one.";
        case "session": return ru ? "Войдите в аккаунт снова." : "Please sign in again.";
        case "profile": return ru ? "Дождитесь подключения профиля и повторите." : "Wait for your profile to connect and retry.";
        default: return ru ? "Не удалось выполнить. Проверьте соединение и повторите." : "Could not complete. Check the connection and retry.";
        }
    }

    private interface Done { void run(JSONObject result) throws Exception; }
    private static void send(CzmDialog d, String action, String credential, JSONObject body, boolean ru, Done done) {
        d.busy(true);
        WORK.execute(() -> {
            String response = raw(action, credential, body);
            d.activity.runOnUiThread(() -> {
                if (d.activity.isFinishing()) return;
                d.busy(false);
                if (!d.isShowing()) return;
                try {
                    if (status(response) != 200) {
                        String code = status(response) == 0 ? "network" : object(response).optString("error");
                        if (status(response) == 401 && code.equals("session")) expired(credential);
                        d.fail(error(code, ru)); return;
                    }
                    done.run(object(response));
                } catch (Exception e) { d.fail(ru ? "Не удалось сохранить сеанс. Войдите ещё раз." : "Could not save the session. Sign in again."); }
            });
        });
    }
    private static CzmDialog panel(Activity a, String title, String subtitle, String action, boolean ru) {
        if (editor != null) editor.dismiss();
        CzmDialog d = new CzmDialog(a, title, subtitle, action, ru ? "Закрыть" : "Close", ru); editor = d;
        d.setOnDismissListener(v -> { if (editor == d) editor = null; });
        return d;
    }

    static void show(String legacy, String nick, boolean ru) {
        Context context = SDLActivity.getContext(); if (!(context instanceof Activity)) return;
        Activity a = (Activity)context;
        a.runOnUiThread(() -> {
            if (a.isFinishing() || (editor != null && editor.isShowing())) return;
            load();
            final String saved;
            synchronized (CzmAccount.class) { saved = token; }
            if (saved.isEmpty()) form(a, legacy, nick, "login", ru);
            else {
                CzmDialog d = panel(a, ru ? "Аккаунт CZM" : "CZM account", ru ? "Проверяем сеанс…" : "Checking session…", "…", ru);
                d.submit.setVisibility(View.GONE);
                d.show();
                send(d, "me", saved, new JSONObject(), ru, info -> details(a, legacy, nick, saved, info, ru));
            }
        });
    }

    private static void form(Activity a, String legacy, String nick, String mode, boolean ru) {
        boolean register = mode.equals("register"), recovery = mode.equals("recover"), reset = mode.equals("reset");
        String title = register ? (ru ? "Создать аккаунт" : "Create account") : recovery || reset ? (ru ? "Восстановить пароль" : "Reset password") : (ru ? "Вход в CZM" : "Sign in to CZM");
        CzmDialog d = panel(a, title, register ? (ru ? "Сохраним текущий профиль и аватар" : "Keep your current profile and avatar")
            : (ru ? "Один профиль на всех ваших устройствах" : "One profile on all your devices"),
            recovery ? (ru ? "Получить код" : "Get code") : reset ? (ru ? "Сменить пароль" : "Reset password") : register ? (ru ? "Создать" : "Create") : (ru ? "Войти" : "Sign in"), ru);
        EditText login = d.field(ru ? "Ник для входа" : "Login nickname", false, 24); login.setText(register || recovery || reset ? nick : "");
        EditText code = reset ? d.field(ru ? "Код из письма" : "Email code", false, 6) : null;
        if (code != null) code.setInputType(InputType.TYPE_CLASS_NUMBER);
        EditText password = recovery ? null : d.field(ru ? "Пароль · минимум 8 символов" : "Password · at least 8 characters", true, 128);
        EditText repeat = register || reset ? d.field(ru ? "Повтор пароля" : "Repeat password", true, 128) : null;
        if (register) d.note(ru ? "Ник для входа уникален. Игровой ник можно менять отдельно. Почта для восстановления добавляется после регистрации."
            : "Your login is unique. Your in-game nickname can be changed separately. Add recovery email after registration.");
        if (recovery) {
            d.note(ru ? "Код придёт на подтверждённую почту аккаунта, если она привязана." : "The code goes to your account's verified recovery email, if one is linked.");
            d.link(ru ? "У меня уже есть код" : "I already have a code", v -> form(a, legacy, login.getText().toString(), "reset", ru));
        }
        if (!register && !recovery && !reset) {
            d.link(ru ? "Создать аккаунт" : "Create an account", v -> form(a, legacy, nick, "register", ru));
            d.link(ru ? "Забыли пароль?" : "Forgot password?", v -> form(a, legacy, login.getText().toString(), "recover", ru));
        }
        if (register || recovery || reset) d.link(ru ? "Назад ко входу" : "Back to sign in", v -> form(a, legacy, nick, "login", ru));
        d.submit.setOnClickListener(v -> {
            try {
                JSONObject body = new JSONObject().put("nick", login.getText().toString());
                if (password != null) body.put("password", password.getText().toString());
                if (repeat != null) body.put("repeat", repeat.getText().toString());
                if (code != null) body.put("code", code.getText().toString());
                if (repeat != null && !password.getText().toString().equals(repeat.getText().toString())) { d.fail(error("repeat", ru)); return; }
                send(d, mode, register ? legacy : "", body, ru, result -> {
                    if (recovery) {
                        form(a, legacy, login.getText().toString(), "reset", ru);
                        editor.note(ru ? "Если у аккаунта есть подтверждённая почта, код отправлен. Проверьте также «Спам»." : "If the account has a verified email, a code was sent. Also check Spam.");
                    } else if (reset) {
                        form(a, legacy, login.getText().toString(), "login", ru);
                        editor.note(ru ? "Пароль изменён. Войдите с новым паролем." : "Password changed. Sign in with the new password.");
                    } else {
                        synchronized (CzmAccount.class) {
                            String next = result.getString("token");
                            if (!next.matches("czms1_[A-Za-z0-9_-]{43}")) throw new IllegalArgumentException();
                            CzmServerAuth.session(a, next); token = next; ++generation; profile(result.getJSONObject("profile"));
                        }
                        d.dismiss(); Toast.makeText(a, ru ? "Вход выполнен" : "Signed in", Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) { d.fail(error("network", ru)); }
        });
        d.setOnDismissListener(v -> { if (password != null) password.setText(""); if (repeat != null) repeat.setText(""); if (editor == d) editor = null; });
        d.show();
    }

    private static void details(Activity a, String legacy, String nick, String credential, JSONObject info, boolean ru) {
        CzmDialog d = panel(a, ru ? "Аккаунт CZM" : "CZM account", info.optString("login") + " · ID " + info.optInt("id"), ru ? "Выйти" : "Sign out", ru);
        d.note((ru ? "Почта для восстановления: " : "Recovery email: ") + (info.optString("email").isEmpty() ? (ru ? "не привязана" : "not linked") : info.optString("email")));
        if (!info.optBoolean("mail_ready")) d.note(ru ? "Отправка писем ещё не настроена. Восстановление пока недоступно. Сохраните пароль."
            : "Email delivery is not configured yet. Recovery is unavailable. Keep your password safe.");
        else d.link(ru ? "Привязать / изменить почту" : "Link / change email", v -> email(a, legacy, nick, credential, ru, false));
        d.link(ru ? "Выйти на всех устройствах" : "Sign out on all devices", v -> {
            // An explicit second tap keeps this separate from signing out of the current phone.
            CzmDialog confirm = panel(a, ru ? "Завершить все сеансы?" : "End all sessions?", ru ? "Профиль и аватар сохранятся" : "Your profile and avatar will stay", ru ? "Выйти везде" : "Sign out everywhere", ru);
            confirm.submit.setOnClickListener(w -> send(confirm, "logout-all", credential, new JSONObject(), ru, r -> { clear(credential); confirm.dismiss(); })); confirm.show();
        });
        d.submit.setOnClickListener(v -> send(d, "logout", credential, new JSONObject(), ru, result -> { clear(credential); d.dismiss(); }));
        d.show();
    }

    private static void email(Activity a, String legacy, String nick, String credential, boolean ru, boolean confirm) {
        CzmDialog d = panel(a, ru ? "Почта для восстановления" : "Recovery email", confirm ? (ru ? "Введите код из письма" : "Enter the email code") : "",
            confirm ? (ru ? "Подтвердить" : "Confirm") : (ru ? "Получить код" : "Get code"), ru);
        EditText input = d.field(confirm ? (ru ? "Код" : "Code") : "Email", false, confirm ? 6 : 254);
        input.setInputType(confirm ? InputType.TYPE_CLASS_NUMBER : InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS);
        if (!confirm) d.link(ru ? "У меня уже есть код" : "I already have a code", v -> email(a, legacy, nick, credential, ru, true));
        d.submit.setOnClickListener(v -> {
            try {
                send(d, confirm ? "email-confirm" : "email", credential, new JSONObject().put(confirm ? "code" : "email", input.getText().toString()), ru,
                    result -> { if (confirm) details(a, legacy, nick, credential, result, ru); else email(a, legacy, nick, credential, ru, true); });
            } catch (Exception e) { d.fail(error("email", ru)); }
        });
        d.show();
    }
}
