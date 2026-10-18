// SPDX-License-Identifier: GPL-3.0-or-later
// CloudStream's manifest declares android:usesCleartextTraffic="true" and no network security config
// (app/src/main/AndroidManifest.xml), so cleartext is permitted for every host, as here.
package android.security;

public class NetworkSecurityPolicy {
    private static final NetworkSecurityPolicy INSTANCE = new NetworkSecurityPolicy();

    private NetworkSecurityPolicy() {}

    public static NetworkSecurityPolicy getInstance() {
        return INSTANCE;
    }

    public boolean isCleartextTrafficPermitted() {
        return true;
    }

    public boolean isCleartextTrafficPermitted(String hostname) {
        return true;
    }

    public boolean isCertificateTransparencyVerificationRequired(String hostname) {
        return false;
    }
}
