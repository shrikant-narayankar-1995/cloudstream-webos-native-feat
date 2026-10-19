// SPDX-License-Identifier: GPL-3.0-or-later
// A dialog the native app renders: show() hands it to the UI host (org.csweb.compat.ui.UiHost), which
// describes it to the app and applies the user's answer. There is no window or drawing.
package android.app;

import android.content.Context;
import android.content.DialogInterface;
import android.view.View;

import org.csweb.compat.ui.UiHost;

public class Dialog implements DialogInterface {
    private final Context context;
    CharSequence title;
    View content;
    boolean cancelable = true;
    private boolean showing;
    private OnCancelListener onCancel;
    private OnDismissListener onDismiss;
    private OnShowListener onShow;

    public Dialog(Context context) {
        this(context, 0);
    }

    public Dialog(Context context, int themeResId) {
        if (context == null) throw new NullPointerException("context");
        this.context = context;
    }

    public final Context getContext() {
        return context;
    }

    public void setTitle(CharSequence title) {
        this.title = title;
    }

    public void setTitle(int titleId) {
        setTitle(context.getText(titleId));
    }

    /** The title last set; used by the UI host. */
    public CharSequence getTitleText() {
        return title;
    }

    public void setContentView(View view) {
        content = view;
    }

    public void setContentView(int layoutResID) {
        setContentView(android.view.LayoutInflater.from(context).inflate(layoutResID, null));
    }

    /** The content view; used by the UI host. */
    public View getContentView() {
        return content;
    }

    public <T extends View> T findViewById(int id) {
        return content == null ? null : content.<T>findViewById(id);
    }

    public void setCancelable(boolean flag) {
        cancelable = flag;
    }

    public boolean isCancelable() {
        return cancelable;
    }

    public void setCanceledOnTouchOutside(boolean cancel) {}

    public void setOnCancelListener(OnCancelListener listener) {
        onCancel = listener;
    }

    public void setOnDismissListener(OnDismissListener listener) {
        onDismiss = listener;
    }

    public void setOnShowListener(OnShowListener listener) {
        onShow = listener;
    }

    private boolean created;

    /** AlertDialog builds its content here, before the first show(), as Dialog.onCreate does on Android. */
    void build() {}

    public void create() {
        if (!created) {
            created = true;
            build();
        }
    }

    public void show() {
        if (showing) return;
        create();
        UiHost host = UiHost.get("Dialog.show");
        showing = true;
        host.shown(this);
        if (onShow != null) onShow.onShow(this);
    }

    public boolean isShowing() {
        return showing;
    }

    @Override
    public void dismiss() {
        if (!showing) return;
        showing = false;
        UiHost.get("Dialog.dismiss").dismissed(this);
        if (onDismiss != null) onDismiss.onDismiss(this);
    }

    @Override
    public void cancel() {
        if (onCancel != null && showing) onCancel.onCancel(this);
        dismiss();
    }

    public void hide() {
        dismiss();
    }
}
