import ru.kurostreet.counterzombiemobile.ArchiveExtractor;
import java.io.*;
import java.nio.file.*;
import java.util.zip.*;

public final class ArchiveExtractorTest {
    static byte[] zip(String path, byte[] data) throws Exception {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ZipOutputStream zip = new ZipOutputStream(bytes)) {
            zip.putNextEntry(new ZipEntry(path));
            zip.write(data);
            zip.closeEntry();
        }
        return bytes.toByteArray();
    }
    static void rejected(byte[] data, File root, long bytes, int files) throws Exception {
        try {
            ArchiveExtractor.extract(new ByteArrayInputStream(data), root, bytes, files, (a,b)->{});
            throw new AssertionError("Invalid archive accepted");
        } catch (IOException expected) {}
    }
    public static void main(String[] args) throws Exception {
        Path root = Files.createTempDirectory("czm-extraction-");
        byte[] data = new byte[]{1,2,3};
        File normal = root.resolve("normal").toFile();
        ArchiveExtractor.extract(new ByteArrayInputStream(zip("cstrike/maps/test.bsp", data)),
                                 normal, 3, 1, (a,b)->{});
        if (!java.util.Arrays.equals(Files.readAllBytes(normal.toPath().resolve("cstrike/maps/test.bsp")), data))
            throw new AssertionError("File differs");
        rejected(zip("../escaped", data), root.resolve("slip").toFile(), 3, 1);
        rejected(zip("/absolute", data), root.resolve("absolute").toFile(), 3, 1);
        rejected(zip("cstrike\\escaped", data), root.resolve("windows").toFile(), 3, 1);
        rejected(zip("cstrike/test", data), root.resolve("too-large").toFile(), 2, 1);
        rejected(zip("cstrike/test", data), root.resolve("incomplete").toFile(), 4, 1);
        rejected(zip("cstrike/test", data), root.resolve("missing-file").toFile(), 3, 2);
        if (Files.exists(root.resolve("escaped"))) throw new AssertionError("Escaped staging");
        System.out.println("Archive extraction: normal files, traversal, size and incomplete data checks passed");
        try (var paths = Files.walk(root)) {
            paths.sorted(java.util.Comparator.reverseOrder()).forEach(p -> {
                try { Files.delete(p); } catch (IOException e) { throw new UncheckedIOException(e); }
            });
        }
    }
}
