
#include <jni.h>
#include <stdlib.h>

char* GetNativeString(JNIEnv *env, jstring jstr);
jstring GetJavaString(JNIEnv *env, char*);
