import ru.kurostreet.counterzombiemobile.ApkUpdate;
import java.io.File;
import java.io.InterruptedIOException;

/** Command-line bridge for the Python HTTP / cross-language integration tests. */
public final class ApkUpdateTest {
    public static void main(String[] args) throws Exception {
        String cancel = args[args.length - 1];
        ApkUpdate.Progress progress = new ApkUpdate.Progress() {
            boolean stopped;
            @Override public boolean cancelled() { return stopped; }
            @Override public void update(boolean verifying, long done, long total) {
                if (("download".equals(cancel) && !verifying && done > 0) || "verify".equals(cancel)) stopped = true;
                System.out.println("PROGRESS " + (verifying ? "verify" : "download") + " " + done + " " + total);
            }
        };
        try {
            File result = ApkUpdate.prepare(new File(args[0]), new File(args[1]), args[2], args[3], Long.parseLong(args[4]),
                args[5], args[6], Long.parseLong(args[7]), args[8], progress);
            System.out.println("READY " + result.getAbsolutePath());
        } catch (InterruptedIOException e) {
            System.out.println("CANCELLED");
        } catch (Exception e) {
            System.err.println("ERROR " + e.getClass().getSimpleName() + ": " + e.getMessage());
            System.exit(2);
        }
    }
}
