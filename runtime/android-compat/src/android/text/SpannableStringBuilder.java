// SPDX-License-Identifier: GPL-3.0-or-later
package android.text;

/** Editable text without spans. */
public class SpannableStringBuilder implements Editable {
    private final StringBuilder text;

    public SpannableStringBuilder() {
        this("");
    }

    public SpannableStringBuilder(CharSequence text) {
        this.text = new StringBuilder(text == null ? "" : text);
    }

    @Override public Editable replace(int st, int en, CharSequence source) {
        text.replace(st, en, String.valueOf(source));
        return this;
    }

    @Override public Editable insert(int where, CharSequence s) {
        text.insert(where, s);
        return this;
    }

    @Override public Editable delete(int st, int en) {
        text.delete(st, en);
        return this;
    }

    @Override public Editable append(CharSequence s) {
        text.append(s);
        return this;
    }

    @Override public Editable append(CharSequence s, int start, int end) {
        text.append(s, start, end);
        return this;
    }

    @Override public Editable append(char c) {
        text.append(c);
        return this;
    }

    @Override public void clear() {
        text.setLength(0);
    }

    @Override public void clearSpans() {}

    @Override public int length() {
        return text.length();
    }

    @Override public char charAt(int index) {
        return text.charAt(index);
    }

    @Override public CharSequence subSequence(int start, int end) {
        return new SpannableStringBuilder(text.subSequence(start, end));
    }

    @Override public String toString() {
        return text.toString();
    }
}
