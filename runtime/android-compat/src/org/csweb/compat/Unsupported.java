// SPDX-License-Identifier: GPL-3.0-or-later
package org.csweb.compat;

/**
 * Raised when plugin code reaches an Android API that has no meaning in a headless runtime on a webOS
 * TV (views, dialogs, activities, WebView...). The message names the API so compatibility reports can
 * say exactly what a plugin needed.
 */
public final class Unsupported extends UnsupportedOperationException {
    private static final long serialVersionUID = 1L;

    public Unsupported(String api) {
        super("not available on webOS: " + api);
    }

    public static Unsupported of(String api) {
        return new Unsupported(api);
    }
}
