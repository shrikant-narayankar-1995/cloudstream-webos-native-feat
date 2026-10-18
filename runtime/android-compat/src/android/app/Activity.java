// SPDX-License-Identifier: GPL-3.0-or-later
// Present so that type checks such as `context is AppCompatActivity` resolve. No activity exists in
// the headless runtime, so constructing one fails explicitly.
package android.app;

public class Activity extends android.view.ContextThemeWrapper {
    public Activity() {
        throw org.csweb.compat.Unsupported.of("android.app.Activity");
    }
}
