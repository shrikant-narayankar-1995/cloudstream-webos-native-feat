// SPDX-License-Identifier: GPL-3.0-or-later
// As on Android: verify the server chain with the wrapped trust manager, then return the chain from
// the leaf to a trusted anchor (each certificate signed by the next), which OkHttp uses for pinning.
package android.net.http;

import java.security.GeneralSecurityException;
import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import javax.net.ssl.X509TrustManager;

public class X509TrustManagerExtensions {
    private static final int MAX_CHAIN = 9;
    private final X509TrustManager tm;

    public X509TrustManagerExtensions(X509TrustManager tm) {
        if (tm == null) throw new IllegalArgumentException("tm is null");
        this.tm = tm;
    }

    public List<X509Certificate> checkServerTrusted(X509Certificate[] chain, String authType, String host)
            throws CertificateException {
        tm.checkServerTrusted(chain, authType);
        List<X509Certificate> pool = new ArrayList<>(Arrays.asList(chain));
        List<X509Certificate> anchors = Arrays.asList(tm.getAcceptedIssuers());
        List<X509Certificate> out = new ArrayList<>();
        X509Certificate cur = chain[0];
        out.add(cur);
        pool.remove(0);
        for (int i = 0; i < MAX_CHAIN; i++) {
            X509Certificate anchor = issuerIn(cur, anchors);
            if (anchor != null) {
                if (!anchor.equals(cur)) out.add(anchor);
                return out;
            }
            X509Certificate next = issuerIn(cur, pool);
            if (next == null || next.equals(cur)) break;
            out.add(next);
            pool.remove(next);
            cur = next;
        }
        throw new CertificateException("no path from the server certificate to a trust anchor");
    }

    public boolean isUserAddedCertificate(X509Certificate cert) {
        return false;
    }

    private static X509Certificate issuerIn(X509Certificate cert, List<X509Certificate> candidates) {
        for (X509Certificate c : candidates) {
            if (!c.getSubjectX500Principal().equals(cert.getIssuerX500Principal())) continue;
            try {
                cert.verify(c.getPublicKey());
                return c;
            } catch (GeneralSecurityException ignored) {
                // same name, different key: keep looking
            }
        }
        return null;
    }
}
