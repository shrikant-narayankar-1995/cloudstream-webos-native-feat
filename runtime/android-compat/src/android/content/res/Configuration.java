// SPDX-License-Identifier: GPL-3.0-or-later
// Configuration of the TV UI plane: landscape, television UI mode, 960x540 dp at xhdpi (1920x1080 px,
// the drawable measured on the reference TV, see docs/research/platform-capability-matrix.md).
package android.content.res;

import java.util.Locale;

public final class Configuration {
    public static final int ORIENTATION_UNDEFINED = 0, ORIENTATION_PORTRAIT = 1, ORIENTATION_LANDSCAPE = 2;
    public static final int UI_MODE_TYPE_MASK = 0x0f, UI_MODE_TYPE_NORMAL = 1, UI_MODE_TYPE_TELEVISION = 4;
    public static final int UI_MODE_NIGHT_MASK = 0x30, UI_MODE_NIGHT_NO = 0x10, UI_MODE_NIGHT_YES = 0x20;
    public static final int TOUCHSCREEN_NOTOUCH = 1;
    public static final int SCREENLAYOUT_SIZE_MASK = 0x0f, SCREENLAYOUT_SIZE_XLARGE = 4;

    public int orientation;
    public int uiMode;
    public int screenWidthDp;
    public int screenHeightDp;
    public int smallestScreenWidthDp;
    public int densityDpi;
    public int touchscreen;
    public int screenLayout;
    public float fontScale;
    public Locale locale;

    public void setToDefaults() {
        orientation = ORIENTATION_LANDSCAPE;
        uiMode = UI_MODE_TYPE_TELEVISION | UI_MODE_NIGHT_YES;
        screenWidthDp = 960;
        screenHeightDp = 540;
        smallestScreenWidthDp = 540;
        densityDpi = 320;
        touchscreen = TOUCHSCREEN_NOTOUCH;
        screenLayout = SCREENLAYOUT_SIZE_XLARGE;
        fontScale = 1f;
        locale = Locale.getDefault();
    }

    public Locale getLocales() {
        return locale;
    }
}
