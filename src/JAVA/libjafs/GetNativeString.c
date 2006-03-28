
#include <jni.h>
#include "GetNativeString.h"

char* GetNativeString(JNIEnv *env, jstring jstr){
  jbyteArray bytes = 0;
  jthrowable exc;
  char *result = 0;
  
  if ((*env)->EnsureLocalCapacity(env, 2) < 0) {
    return 0; /* out of memory error */
  }
  
  jclass stringClass=(*env)->FindClass(env,"java/lang/String");

  if(!stringClass){
    return 0;
  }

  jmethodID  MID_String_getBytes = (*env)->GetMethodID(env,stringClass,"getBytes","()[B");
  if(!MID_String_getBytes){
    return 0;
  }

  bytes = (*env)->CallObjectMethod(env, jstr,
				   MID_String_getBytes);
  exc = (*env)->ExceptionOccurred(env);
  if (!exc) {
    jint len = (*env)->GetArrayLength(env, bytes);
    result = (char *)malloc(len + 1);
    if (result == 0) {

      /*JNU_ThrowByName(env, "java/lang/OutOfMemoryError",
	0);*/

      (*env)->DeleteLocalRef(env, bytes);
      return 0;
    }
    (*env)->GetByteArrayRegion(env, bytes, 0, len,
			       (jbyte *)result);
    result[len] = 0; /* NULL-terminate */
  } else {
    (*env)->DeleteLocalRef(env, exc);
  }
  (*env)->DeleteLocalRef(env, bytes);
     return result;  
}

jstring GetJavaString(JNIEnv *env, char*);
