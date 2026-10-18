// SPDX-License-Identifier: GPL-3.0-or-later
// No StrictMode policy is ever installed in the headless runtime, so, as on Android without a policy,
// these calls have no effect.
package android.os;

public final class StrictMode {
    private StrictMode() {}

    public static void noteSlowCall(String name) {}
}
