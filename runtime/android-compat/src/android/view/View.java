// SPDX-License-Identifier: GPL-3.0-or-later
// A view-tree node without drawing, measuring or input: plugin settings screens are described to the
// native app (org.csweb.compat.ui.DialogProtocol), which renders them. Only state is kept.
package android.view;

import android.content.Context;
import android.util.AttributeSet;

public class View {
    public static final int NO_ID = -1;
    public static final int VISIBLE = 0x0, INVISIBLE = 0x4, GONE = 0x8;

    public interface OnClickListener {
        void onClick(View v);
    }

    private final Context context;
    private int id = NO_ID;
    private Object tag;
    private int visibility = VISIBLE;
    private boolean enabled = true;
    private ViewParent parent;
    private ViewGroup.LayoutParams layoutParams;
    private OnClickListener onClick;

    public View(Context context) {
        this.context = context;
    }

    public View(Context context, AttributeSet attrs) {
        this(context);
    }

    public View(Context context, AttributeSet attrs, int defStyleAttr) {
        this(context);
    }

    public final Context getContext() {
        return context;
    }

    public int getId() {
        return id;
    }

    public void setId(int id) {
        this.id = id;
    }

    public Object getTag() {
        return tag;
    }

    public void setTag(Object tag) {
        this.tag = tag;
    }

    public int getVisibility() {
        return visibility;
    }

    public void setVisibility(int visibility) {
        this.visibility = visibility;
    }

    public boolean isEnabled() {
        return enabled;
    }

    public void setEnabled(boolean enabled) {
        this.enabled = enabled;
    }

    public final ViewParent getParent() {
        return parent;
    }

    void assignParent(ViewParent parent) {
        this.parent = parent;
    }

    public ViewGroup.LayoutParams getLayoutParams() {
        return layoutParams;
    }

    public void setLayoutParams(ViewGroup.LayoutParams params) {
        if (params == null) throw new NullPointerException("Layout parameters cannot be null");
        layoutParams = params;
    }

    public void setPadding(int left, int top, int right, int bottom) {}

    public void setMinimumHeight(int minHeight) {}

    public void setMinimumWidth(int minWidth) {}

    public void setOnClickListener(OnClickListener l) {
        onClick = l;
    }

    public boolean hasOnClickListeners() {
        return onClick != null;
    }

    public boolean performClick() {
        if (onClick == null) return false;
        onClick.onClick(this);
        return true;
    }

    public boolean requestFocus() {
        return false;
    }

    public final <T extends View> T findViewById(int id) {
        if (id == NO_ID) return null;
        return findViewTraversal(id);
    }

    public final <T extends View> T findViewWithTag(Object tag) {
        if (tag == null) return null;
        return findViewWithTagTraversal(tag);
    }

    @SuppressWarnings("unchecked")
    <T extends View> T findViewTraversal(int id) {
        return id == this.id ? (T) this : null;
    }

    @SuppressWarnings("unchecked")
    <T extends View> T findViewWithTagTraversal(Object tag) {
        return tag.equals(this.tag) ? (T) this : null;
    }
}
