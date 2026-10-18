// SPDX-License-Identifier: GPL-3.0-or-later
// Runtime capability probe for an ART build: VM identity, Java 8 language features, threads,
// and an HTTPS GET through the runtime's own TLS stack. Contains no plugin code.
import java.io.InputStream;
import java.net.URL;
import java.util.concurrent.CompletableFuture;
import java.util.stream.Collectors;
import java.util.stream.IntStream;
import javax.net.ssl.HttpsURLConnection;

public class ArtProbe {
    interface Check { Object run() throws Exception; }

    static void check(String name, Check c) {
        try {
            System.out.println(name + "=" + c.run());
        } catch (Throwable t) {
            System.out.println(name + "=FAIL " + t);
        }
    }

    public static void main(String[] args) throws Exception {
        System.out.println("vm=" + System.getProperty("java.vm.name") + " " + System.getProperty("java.vm.version"));
        System.out.println("arch=" + System.getProperty("os.arch") + " cp=" + System.getProperty("java.boot.class.path", "?").length());
        check("lambda", () -> ((Check) () -> 41).run().hashCode() + 1);
        check("stream_tolist", () -> IntStream.range(0, 5).boxed().collect(Collectors.toList()));
        check("stream_joining", () -> IntStream.range(0, 5).mapToObj(Integer::toString).collect(Collectors.joining(",")));
        check("future", () -> CompletableFuture.supplyAsync(() -> Thread.currentThread().getName() != null).get());
        check("https", () -> https(args));
        System.out.println("done=1");
    }

    static String https(String[] args) throws Exception {
        String url = args.length > 0 ? args[0] : "https://archive.org/advancedsearch.php?q=title%3Anosferatu&fl%5B%5D=identifier&rows=1&output=json";
        HttpsURLConnection c = (HttpsURLConnection) new URL(url).openConnection();
        c.setConnectTimeout(10000);
        c.setReadTimeout(10000);
        int code = c.getResponseCode();
        byte[] buf = new byte[200];
        int n;
        try (InputStream in = c.getInputStream()) { n = in.read(buf); }
        return "status=" + code + " cipher=" + c.getCipherSuite() + " bytes=" + n;
    }
}
