package ru.kurostreet.counterzombiemobile;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashSet;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/** Extracts data with bounds and path checks before any file is written. */
public final class ArchiveExtractor {
    public interface Progress { void update(long bytes, long total); }
    private ArchiveExtractor() {}

    public static void extract(InputStream input, File destination, long expectedBytes,
                               int expectedFiles, Progress progress) throws IOException {
        if (expectedBytes < 0 || expectedFiles < 1 || expectedFiles > 20000)
            throw new IOException("Invalid archive limits");
        if (!destination.isDirectory() && !destination.mkdirs())
            throw new IOException("Cannot create data directory");
        String root = destination.getCanonicalPath() + File.separator;
        long written = 0;
        int files = 0;
        Set<String> names = new HashSet<>();
        byte[] buffer = new byte[128 * 1024];
        try (ZipInputStream zip = new ZipInputStream(new BufferedInputStream(input))) {
            ZipEntry entry;
            while ((entry = zip.getNextEntry()) != null) {
                String name = entry.getName();
                if (name.isEmpty() || name.startsWith("/") || name.contains("\\") || name.contains(":"))
                    throw new IOException("Invalid archive path");
                for (String part : name.split("/")) {
                    if (part.equals("..") || part.equals(".")) throw new IOException("Unsafe archive path");
                }
                File target = new File(destination, name);
                if (!target.getCanonicalPath().startsWith(root) || !names.add(name))
                    throw new IOException("Unsafe or duplicate archive path");
                if (entry.isDirectory()) {
                    if (!target.isDirectory() && !target.mkdirs())
                        throw new IOException("Cannot create archive directory");
                    continue;
                }
                if (++files > expectedFiles || entry.getSize() > expectedBytes)
                    throw new IOException("Archive exceeds declared size");
                File parent = target.getParentFile();
                if (!parent.isDirectory() && !parent.mkdirs())
                    throw new IOException("Cannot create archive directory");
                try (BufferedOutputStream output = new BufferedOutputStream(new FileOutputStream(target))) {
                    int count;
                    while ((count = zip.read(buffer)) != -1) {
                        written += count;
                        if (written > expectedBytes) throw new IOException("Archive exceeds declared size");
                        output.write(buffer, 0, count);
                        progress.update(written, expectedBytes);
                    }
                }
                // ZipInputStream checks each entry's CRC before advancing.
                zip.closeEntry();
            }
        }
        if (files != expectedFiles || written != expectedBytes) throw new IOException("Archive is incomplete");
    }
}
