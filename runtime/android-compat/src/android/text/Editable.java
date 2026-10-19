// SPDX-License-Identifier: GPL-3.0-or-later
package android.text;

/** Mutable text, as returned by EditText.getText(). Spans are not kept (no text rendering). */
public interface Editable extends CharSequence, Appendable {
    Editable replace(int st, int en, CharSequence source);

    Editable insert(int where, CharSequence text);

    Editable delete(int st, int en);

    @Override
    Editable append(CharSequence text);

    @Override
    Editable append(CharSequence text, int start, int end);

    @Override
    Editable append(char text);

    void clear();

    void clearSpans();
}
