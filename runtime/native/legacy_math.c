// SPDX-License-Identifier: GPL-3.0-or-later
// ART's ARM JNI trampoline passes FP arguments/results using AAPCS core
// registers. Alpine armv7 libopenjdk was compiled for AAPCS-VFP instead.
// These entry points translate that boundary; calls to musl keep hard-float.
#include <jni.h>

#if !defined(__arm__) || !defined(__ARM_PCS_VFP)
#error "legacy_math requires the ARM hard-float compiler target"
#endif
#define ART_JNI __attribute__((pcs("aapcs")))

// Standard libm declarations, avoiding host math headers in this cross-build.
#define UNARY_LIST(X) \
    X(acos) X(asin) X(atan) X(cbrt) X(cos) X(ceil) X(cosh) X(exp) \
    X(expm1) X(floor) X(log) X(log10) X(log1p) X(rint) X(sin) \
    X(sinh) X(sqrt) X(tan) X(tanh)
#define BINARY_LIST(X) X(atan2) X(hypot) X(pow)
#define UNARY(name) \
    extern double name(double); \
    static jdouble ART_JNI bridge_##name(JNIEnv *env, jclass cls, jdouble x) { \
        (void)env; (void)cls; return name(x); \
    }
#define BINARY(name) \
    extern double name(double, double); \
    static jdouble ART_JNI bridge_##name(JNIEnv *env, jclass cls, jdouble x, jdouble y) { \
        (void)env; (void)cls; return name(x, y); \
    }
UNARY_LIST(UNARY)
BINARY_LIST(BINARY)
BINARY(remainder)
#define ENTRY1(name) {#name, "(D)D", (void *)bridge_##name},
#define ENTRY2(name) {#name, "(DD)D", (void *)bridge_##name},
static const JNINativeMethod methods[] = {
    UNARY_LIST(ENTRY1)
    BINARY_LIST(ENTRY2)
    {"IEEEremainder", "(DD)D", (void *)bridge_remainder},
};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void)reserved;
    JNIEnv *env = 0;
    if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;
    jclass math = (*env)->FindClass(env, "java/lang/Math");
    if (!math) return JNI_ERR;
    jint result = (*env)->RegisterNatives(env, math, methods, sizeof(methods) / sizeof(methods[0]));
    (*env)->DeleteLocalRef(env, math);
    return result == JNI_OK ? JNI_VERSION_1_6 : JNI_ERR;
}
