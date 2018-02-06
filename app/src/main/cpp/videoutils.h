#include <jni.h>

#ifndef _Included_com_ffmpegtest_VideoUtils
#define _Included_com_ffmpegtest_VideoUtils
#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL
Java_com_ffmpegtest_VideoUtils_decode(JNIEnv *, jclass, jstring, jstring);


JNIEXPORT void JNICALL
Java_com_ffmpegtest_VideoUtils_render(JNIEnv *, jobject , jstring ,jobject );

JNIEXPORT void JNICALL
Java_com_ffmpegtest_VideoUtils_sound(JNIEnv *, jobject , jstring ,jstring );
    #ifdef __cplusplus

#endif
#endif