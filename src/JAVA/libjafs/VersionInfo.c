#include "JAFS_Version.h"
#include "org_openafs_jafs_VersionInfo.h"
 
JNIEXPORT jstring JNICALL Java_org_openafs_jafs_VersionInfo_getVersionOfLibjafs
  (JNIEnv *env, jobject obj)
{
	return (*env)->NewStringUTF(env, VERSION_LIB_JAVA_OPENAFS);
}

JNIEXPORT jstring JNICALL Java_org_openafs_jafs_VersionInfo_getBuildInfoOfLibjafs
  (JNIEnv *env, jobject obj)
{
	return (*env)->NewStringUTF(env, cml_version_number);
}
