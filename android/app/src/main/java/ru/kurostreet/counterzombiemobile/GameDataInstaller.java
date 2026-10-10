package ru.kurostreet.counterzombiemobile;

import android.content.Context;
import android.content.res.AssetManager;
import android.os.StatFs;
import android.util.Log;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

/** Bundled resources and writable player files have separate directories. */
public final class GameDataInstaller {
    public interface Progress { void update(String message, int percent); }
    public static final class Installation {
        public final File data;
        public final File writable;
        Installation(File data, File writable) { this.data = data; this.writable = writable; }
    }

    private static final String TAG = "CzmPreparation";
    private static final String[] PACKS = {"game-data", "engine-resources", "client-resources"};
    private GameDataInstaller() {}

    public static synchronized Installation prepare(Context context, Progress progress) throws IOException {
        try {
            AssetManager assets = context.getAssets();
            JSONObject[] manifests = new JSONObject[PACKS.length];
            StringBuilder version = new StringBuilder();
            long unpackedBytes = 0;
            for (int i = 0; i < PACKS.length; i++) {
                try (InputStream stream = assets.open("bundled/" + PACKS[i] + ".json")) {
                    manifests[i] = new JSONObject(new String(readSmall(stream), StandardCharsets.UTF_8));
                } catch (IOException e) {
                    throw new IOException("В этой сборке отсутствуют встроенные игровые файлы. Установите полный APK.", e);
                }
                String sha = manifests[i].getString("sha256");
                if (!sha.matches("[a-f0-9]{64}")) throw new IOException("Invalid bundle metadata");
                version.append(sha);
                long size = manifests[i].getLong("unpacked_bytes");
                if (size < 0) throw new IOException("Invalid bundle size");
                unpackedBytes += size;
                if (unpackedBytes <= 0 || unpackedBytes > 2L * 1024 * 1024 * 1024)
                    throw new IOException("Invalid bundle size");
            }
            String id = hex(digest().digest(version.toString().getBytes(StandardCharsets.UTF_8)));
            File parent = new File(context.getFilesDir(), "bundled-data");
            File data = new File(parent, id);
            File marker = new File(data, ".ready");
            File writable = new File(context.getFilesDir(), "game");
            if (marker.isFile() && ready(data, manifests)) {
                mkdir(writable);
                Log.i(TAG, "READY existing data=" + data + " writable=" + writable);
                return new Installation(data, writable);
            }
            mkdir(parent);
            long available = new StatFs(parent.getAbsolutePath()).getAvailableBytes();
            if (available < unpackedBytes + 128L * 1024 * 1024)
                throw new IOException("Недостаточно свободного места. Освободите около "
                        + ((unpackedBytes + 128L * 1024 * 1024) / (1024 * 1024)) + " МБ и повторите.");
            // Only our unfinished extraction is discarded. Player files are never touched.
            File staging = new File(parent, id + ".preparing");
            remove(staging);
            mkdir(staging);
            Log.i(TAG, "PREPARING data=" + staging + " bytes=" + unpackedBytes);
            long completed = 0;
            final long total = unpackedBytes;
            final long[] lastUpdate = {0};
            for (int i = 0; i < PACKS.length; i++) {
                JSONObject metadata = manifests[i];
                String path = "bundled/" + PACKS[i] + ".zip";
                progress.update("Проверяем игровые файлы…", (int) (completed * 100 / total));
                try (InputStream stream = assets.open(path, AssetManager.ACCESS_STREAMING)) {
                    if (!hash(stream).equals(metadata.getString("sha256")))
                        throw new IOException("Игровые файлы повреждены. Переустановите полный APK.");
                }
                long offset = completed;
                File destination = i == 0 ? staging : new File(staging, i == 1 ? "valve" : "cstrike");
                try (InputStream stream = assets.open(path, AssetManager.ACCESS_STREAMING)) {
                    ArchiveExtractor.extract(stream, destination, metadata.getLong("unpacked_bytes"),
                            metadata.getInt("files"), (bytes, packTotal) -> {
                                long now = System.currentTimeMillis();
                                if (now - lastUpdate[0] > 100) {
                                    lastUpdate[0] = now;
                                    progress.update("Подготавливаем игру…", (int) ((offset + bytes) * 100 / total));
                                }
                            });
                }
                completed += metadata.getLong("unpacked_bytes");
            }
            if (!ready(staging, manifests)) throw new IOException("Не удалось подготовить игровые файлы.");
            // The engine only sees complete files, including after an interrupted preparation.
            remove(data);
            if (!staging.renameTo(data)) throw new IOException("Не удалось сохранить игровые файлы.");
            try (FileOutputStream output = new FileOutputStream(marker)) {
                output.write(id.getBytes(StandardCharsets.UTF_8));
                output.getFD().sync();
            }
            mkdir(writable);
            progress.update("Запускаем игру…", 100);
            Log.i(TAG, "READY new data=" + data + " writable=" + writable);
            return new Installation(data, writable);
        } catch (JSONException e) {
            throw new IOException("Некорректные данные сборки.", e);
        }
    }

    private static boolean ready(File data, JSONObject[] manifests) throws JSONException, IOException {
        JSONArray required = manifests[0].getJSONArray("required");
        for (int i = 0; i < required.length(); i++) {
            JSONObject entry = required.getJSONObject(i);
            File file = new File(data, entry.getString("path"));
            if (!file.getCanonicalPath().startsWith(data.getCanonicalPath() + File.separator)
                    || !file.isFile() || file.length() != entry.getLong("bytes")) return false;
        }
        return new File(data, "cstrike/gfx/vgui/czm_logo.tga").isFile()
                && new File(data, "valve/gfx.wad").isFile();
    }

    private static byte[] readSmall(InputStream input) throws IOException {
        java.io.ByteArrayOutputStream output = new java.io.ByteArrayOutputStream();
        byte[] buffer = new byte[4096];
        int count;
        while ((count = input.read(buffer)) != -1) {
            if (output.size() + count > 1024 * 1024) throw new IOException("Metadata too large");
            output.write(buffer, 0, count);
        }
        return output.toByteArray();
    }
    private static MessageDigest digest() {
        try { return MessageDigest.getInstance("SHA-256"); }
        catch (NoSuchAlgorithmException e) { throw new AssertionError(e); }
    }
    private static String hash(InputStream input) throws IOException {
        MessageDigest digest = digest();
        byte[] buffer = new byte[128 * 1024];
        int count;
        while ((count = input.read(buffer)) != -1) digest.update(buffer, 0, count);
        return hex(digest.digest());
    }
    private static String hex(byte[] bytes) {
        StringBuilder result = new StringBuilder();
        for (byte b : bytes) result.append(String.format(java.util.Locale.ROOT, "%02x", b & 255));
        return result.toString();
    }
    private static void mkdir(File file) throws IOException {
        if (!file.isDirectory() && !file.mkdirs()) throw new IOException("Не удалось создать папку игры.");
    }
    private static void remove(File file) throws IOException {
        if (!file.exists()) return;
        File[] children = file.listFiles();
        if (children != null) for (File child : children) remove(child);
        if (!file.delete()) throw new IOException("Не удалось очистить незавершённую установку.");
    }
}
