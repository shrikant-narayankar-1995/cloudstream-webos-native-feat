// SPDX-License-Identifier: GPL-3.0-or-later
package android.view;

public class InflateException extends RuntimeException {
    private static final long serialVersionUID = 1L;

    public InflateException(String message) {
        super(message);
    }

    public InflateException(String message, Throwable cause) {
        super(message, cause);
    }
}
