// SPDX-License-Identifier: GPL-3.0-or-later
package android.widget;

import android.content.Context;
import android.text.Editable;
import android.text.SpannableStringBuilder;
import android.util.AttributeSet;

public class EditText extends TextView {
    public EditText(Context context) {
        super(context);
        setText("");
        setInputType(0x00000001);  // InputType.TYPE_CLASS_TEXT, the EditText default
    }

    public EditText(Context context, AttributeSet attrs) {
        this(context);
    }

    public EditText(Context context, AttributeSet attrs, int defStyleAttr) {
        this(context);
    }

    @Override
    public Editable getText() {
        return (Editable) super.getText();
    }

    @Override
    public void setText(CharSequence text) {
        super.setText(new SpannableStringBuilder(text));
    }

    public void setSelection(int index) {}

    public void setSelection(int start, int stop) {}

    public void selectAll() {}
}
