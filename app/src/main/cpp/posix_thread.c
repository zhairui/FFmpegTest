#include <android/log.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <jni.h>
#include <pthread.h>

#define LOGI(FORMAT,...)__android_log_print(ANDROID_LOG_INFO,"zr",FORMAT,##__VA_ARGS__)
#define LOGE(FORMAT,...)__android_log_print(ANDROID_LOG_ERROR,"zr",FORMAT,##__VA_ARGS__)

JavaVM  *javaVM;
jobject utils_class_global;
jmethodID utils_get_mid;
JNIEXPORT jint JNI_OnLoad(JavaVM* vm,void* reserved){
    LOGI("%s","JNI_OnLoad");
    javaVM=vm;
    return JNI_VERSION_1_4;
}

void* th_fun(void* arg){
    JNIEnv * env=NULL;
    //通过JavaVM关联当前线程，获取当前线程的JNIEnv
    //JavaVMAttachArgs args={JNI_VERSION_1_4,"my_thread",NULL};
    char* no=(char*) arg;
    int i;
    for (i = 0; i <5; ++i) {
        LOGI("thread %s,i:%d",no,i);
        (*javaVM)->AttachCurrentThread(javaVM,&env,NULL);
        jobject uuid_jstr=(*env)->CallStaticObjectMethod(env,utils_class_global,utils_get_mid);
        char* uuid_cstr=(*env)->GetStringUTFChars(env,uuid_jstr,NULL);
        LOGI("%s",uuid_cstr);
        if(i==4){
            goto end;
        }
        sleep(1);
    }
end:
    (*javaVM)->DetachCurrentThread(env);
    pthread_exit((void*)0);
}
//可以通过JavaVM获取到每个线程关联的JNIEnv
//如何获取javaVM
//1.在JNI_OnLoad函数中获取
//2.(*env)->GetJavaVM(env,&javaVM);
//每个线程都有一个JNIEnv(用来访问java 的类，成员，对象)
JNIEXPORT void JNICALL
Java_com_ffmpegtest_VideoUtils_pthread(JNIEnv *env, jobject instance) {

    //获取class必须要在主线程
    jclass utils_class_tmp=(*env)->FindClass(env,"com/ffmpegtest/Utils");
    //创建全局引用
    utils_class_global=(*env)->NewGlobalRef(env,utils_class_tmp);

    //获取jmethodId也可以在子线程
    utils_get_mid=(*env)->GetStaticMethodID(env,utils_class_global,"get","()Ljava/lang/String;");
    //创建多线程
    pthread_t  tid;
    pthread_create(&tid,NULL,th_fun,(void*)"NO1");

}

JNIEXPORT void JNICALL
java_com_ffmpegtest_VideoUtils_destroy(JNIEnv *env,jobject obj){
    //释放全局引用
    (*env)->DeleteLocalRef(env,utils_class_global);
}