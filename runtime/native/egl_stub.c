// SPDX-License-Identifier: GPL-3.0-or-later
// libEGL.so.1 for the headless runtime. bionic_translation's libdl_bio imports eglGetProcAddress only to
// resolve gl* symbols for Android native libraries; there is no EGL/GL in this runtime, so every lookup
// answers "not available" (NULL), as eglGetProcAddress does for unknown functions.
void *eglGetProcAddress(const char *procname) {
    (void)procname;
    return 0;
}
