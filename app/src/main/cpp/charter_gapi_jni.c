#include <jni.h>
#include "charter_gapi_core.h"

JNIEXPORT jintArray JNICALL
Java_network_supers_charter_CharterGapi_nativePalette(JNIEnv *env, jclass cls) {
    (void)cls;
    CGapiPalette palette = cgapi_palette_default_dark();
    jint values[] = {
        (jint)cgapi_color_argb32(palette.background),
        (jint)cgapi_color_argb32(palette.surface),
        (jint)cgapi_color_argb32(palette.accent),
        (jint)cgapi_color_argb32(palette.text),
        (jint)cgapi_color_argb32(palette.muted_text),
        (jint)cgapi_color_argb32(palette.border)
    };
    jintArray result = (*env)->NewIntArray(env, 6);
    if (result) (*env)->SetIntArrayRegion(env, result, 0, 6, values);
    return result;
}

JNIEXPORT jintArray JNICALL
Java_network_supers_charter_CharterGapi_nativeMetrics(JNIEnv *env, jclass cls,
                                                       jint width) {
    (void)cls;
    CGapiMetrics metrics = cgapi_metrics_for_width(width);
    jint values[] = {metrics.spacing, metrics.sidebar_width, metrics.media_height};
    jintArray result = (*env)->NewIntArray(env, 3);
    if (result) (*env)->SetIntArrayRegion(env, result, 0, 3, values);
    return result;
}
