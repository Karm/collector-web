///usr/bin/env jbang "$0" "$@" ; exit $?

import com.sun.net.httpserver.Filter;
import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpServer;
import com.sun.net.httpserver.SimpleFileServer;

import java.io.IOException;
import java.io.PrintStream;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.nio.file.Path;
import java.time.OffsetDateTime;
import java.time.format.DateTimeFormatter;

import static java.lang.System.exit;
import static java.lang.System.out;
import static java.nio.charset.StandardCharsets.UTF_8;

/**
 * Simple Java 19 file server to locally serve the WASM project.
 * @author "Karm" <karm@redhat.com>
 */
public class server {

    private static final DateTimeFormatter FORMATTER = DateTimeFormatter.ofPattern("dd/MMM/yyyy:HH:mm:ss Z");

    static class SlowDownFilter extends Filter {

        public final String resourceToSlowDown;
        public final long waitms;
        private final PrintStream ps = new PrintStream(out, true, UTF_8);

        public SlowDownFilter(String resourceToSlowDown, long waitms) {
            this.resourceToSlowDown = resourceToSlowDown;
            this.waitms = waitms;
        }

        @Override
        public void doFilter(HttpExchange exchange, Chain chain) throws IOException {
            if (resourceToSlowDown != null && exchange.getRequestURI().getPath().contains(resourceToSlowDown)) {
                ps.printf("[" + OffsetDateTime.now().format(FORMATTER) + "] Waiting %d ms to serve: %s ...", waitms, exchange.getRequestURI());
                try {
                    Thread.sleep(waitms);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
                ps.println("served");
            } else {
                ps.printf("[" + OffsetDateTime.now().format(FORMATTER) + "] Serving: %s\n", exchange.getRequestURI());
            }
            chain.doFilter(exchange);
        }

        @Override
        public String description() {
            return "Meh. Testing.";
        }
    }

    public static void main(String... args) throws IOException {
        if (args.length < 2 || (args.length > 2 && args.length != 4)) {
            out.println("Usage: server.java <port> <web dir path> [<resource to slow down> <ms>]");
            out.println("  e.g: $ ./server.java 7777 ../build_emscripten/src/");
            out.println("  e.g: $ ./server.java 7777 ../build_emscripten/src/ test.data 1500");
            exit(1);
        }
        final int port = Integer.parseInt(args[0]);
        final InetSocketAddress ADDR = new InetSocketAddress(InetAddress.getLoopbackAddress(), port);
        final Path root = Path.of(args[1]).toAbsolutePath();
        final String resourceToSlowDown = (args.length > 2) ? args[2] : null;
        final long waitms = (args.length > 2) ? Long.parseLong(args[3]) : 0L;
        final HttpServer server = HttpServer.create(ADDR, 0, "/", SimpleFileServer.createFileHandler(root), new SlowDownFilter(resourceToSlowDown, waitms));
        out.printf("Starting serving files from %s dir: http://%s:%d\n", root, ADDR.getAddress().getHostAddress(), port);
        if (args.length > 2) {
            out.printf("Will pause for %dms on resource containing string %s\n", waitms, resourceToSlowDown);
        }
        server.start();
    }
}
