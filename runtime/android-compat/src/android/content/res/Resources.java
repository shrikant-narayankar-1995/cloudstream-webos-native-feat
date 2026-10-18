// SPDX-License-Identifier: GPL-3.0-or-later
// Only the system Resources exist in the headless runtime: display metrics and configuration of the TV
// UI plane. There is no resource table (no APK), so looking up resources by id fails explicitly, and
// apps cannot construct their own Resources.
package android.content.res;

import android.util.DisplayMetrics;

public class Resources {
    public static class NotFoundException extends RuntimeException {
        private static final long serialVersionUID = 1L;

        public NotFoundException() {}

        public NotFoundException(String name) {
            super(name);
        }
    }

    private static final Resources SYSTEM = new Resources(true);

    private final DisplayMetrics metrics = new DisplayMetrics();
    private final Configuration configuration = new Configuration();

    private Resources(boolean system) {
        metrics.setToDefaults();
        configuration.setToDefaults();
    }

    public Resources(Object assets, DisplayMetrics metrics, Configuration config) {
        throw org.csweb.compat.Unsupported.of("android.content.res.Resources(AssetManager, ...)");
    }

    public static Resources getSystem() {
        return SYSTEM;
    }

    public DisplayMetrics getDisplayMetrics() {
        return metrics;
    }

    public Configuration getConfiguration() {
        return configuration;
    }

    public String getString(int id) {
        throw new NotFoundException("String resource ID #0x" + Integer.toHexString(id) + " (no resource table on webOS)");
    }

    public int getIdentifier(String name, String defType, String defPackage) {
        return 0;  // Android's answer for "no such resource"
    }
}
