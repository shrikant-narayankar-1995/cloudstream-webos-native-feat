// SPDX-License-Identifier: GPL-3.0-or-later
// android.net.ssl.SSLSockets (API 29) operates only on the platform's Conscrypt sockets. The headless
// runtime's TLS provider is wolfSSL (art_standalone), so no socket is supported, and the mutators throw
// IllegalArgumentException exactly as Android does for unsupported sockets.
package android.net.ssl;

import javax.net.ssl.SSLSocket;

public final class SSLSockets {
    private SSLSockets() {}

    public static boolean isSupportedSocket(SSLSocket socket) {
        return false;
    }

    public static void setUseSessionTickets(SSLSocket socket, boolean useSessionTickets) {
        throw new IllegalArgumentException("not a platform SSLSocket");
    }

    public static byte[] exportKeyingMaterial(SSLSocket socket, String label, byte[] context, int length) {
        throw new IllegalArgumentException("not a platform SSLSocket");
    }
}
