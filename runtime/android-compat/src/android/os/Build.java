// SPDX-License-Identifier: GPL-3.0-or-later
// Describes the runtime honestly: the Java API level is that of the bundled ART libcore (Android 10,
// API 29, see docs/research/dependency-inventory.md); the device fields identify a webOS TV runtime
// and carry no serial or per-device identifier.
package android.os;

public class Build {
    public static final String UNKNOWN = "unknown";
    public static final String BRAND = "lge";
    public static final String MANUFACTURER = "LGE";
    public static final String MODEL = "webOS TV (csweb runtime)";
    public static final String DEVICE = "webos";
    public static final String PRODUCT = "webos";
    public static final String HARDWARE = UNKNOWN;
    public static final String BOARD = UNKNOWN;
    public static final String FINGERPRINT = "lge/webos/webos:10/csweb/0:user/release-keys";
    public static final String TYPE = "user";
    public static final String TAGS = "release-keys";
    public static final String SERIAL = UNKNOWN;
    public static final String[] SUPPORTED_ABIS = {System.getProperty("os.arch", UNKNOWN)};

    public static class VERSION {
        public static final int SDK_INT = 29;
        public static final String RELEASE = "10";
        public static final String CODENAME = "REL";
        public static final String INCREMENTAL = "csweb";
        public static final int PREVIEW_SDK_INT = 0;
    }

    public static class VERSION_CODES {
        public static final int LOLLIPOP = 21, LOLLIPOP_MR1 = 22, M = 23, N = 24, N_MR1 = 25, O = 26, O_MR1 = 27,
                P = 28, Q = 29, R = 30, S = 31, S_V2 = 32, TIRAMISU = 33, UPSIDE_DOWN_CAKE = 34,
                VANILLA_ICE_CREAM = 35, BAKLAVA = 36, CUR_DEVELOPMENT = 10000;
    }

    public static String getRadioVersion() {
        return null;
    }
}
