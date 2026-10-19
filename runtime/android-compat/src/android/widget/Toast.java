// SPDX-License-Identifier: GPL-3.0-or-later
package android.widget;

import android.content.Context;

import org.csweb.compat.ui.UiHost;

/** A transient message; the UI host forwards it to the native app. */
public class Toast {
    public static final int LENGTH_SHORT = 0, LENGTH_LONG = 1;

    private final Context context;
    private CharSequence text;
    private int duration;

    public Toast(Context context) {
        this.context = context;
    }

    public static Toast makeText(Context context, CharSequence text, int duration) {
        Toast t = new Toast(context);
        t.text = text;
        t.duration = duration;
        return t;
    }

    public static Toast makeText(Context context, int resId, int duration) {
        return makeText(context, context.getText(resId), duration);
    }

    public void setText(CharSequence s) {
        text = s;
    }

    public void setText(int resId) {
        setText(context.getText(resId));
    }

    public void setDuration(int duration) {
        this.duration = duration;
    }

    public int getDuration() {
        return duration;
    }

    public void show() {
        UiHost.get("Toast.show").toast(text, duration);
    }

    public void cancel() {}
}
