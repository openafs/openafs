#ifndef _Jafsadm_Internal
#define _Jafsadm_Internal

#include <jni.h>
#include "Exceptions.h"

/** 
 * ERROR CODES
 *
 * Please add any internal error codes to the ErrorMessages.properties
 * file located in src/JAVA/classes/
 */
#define JAFSADMNOMEM          1050	// Memory problems
#define JAFSADMCLASSNOTFOUND  1051	// Trouble finding a Java class
#define JAFSADMMETHODNOTFOUND 1052	// Trouble finding a Java method
#define JAFSADMFIELDNOTFOUND  1053	// Trouble finding a Java field
#define JAFSNULLARG           1054	// Null argument (general)
#define JAFSNULLUSER          1055	// User argument null
#define JAFSNULLPASS          1056	// Password argument null
#define JAFSNULLGROUP         1057	// Group name argument null
#define JAFSNULLOWNER         1058	// Group owner name argument null
#define JAFSNULLVOLUME        1059	// Volume name argument null
#define JAFSNULLPART          1060	// Partition name argument null
#define JAFSNULLPROCESS       1061	// Process name argument null
#define JAFSNULLSERVER        1062	// Server name argument null
#define JAFSNULLCELL          1063	// Cell name argument null
#define JAFSNULLPATH          1064	// Path argument null
#define JAFSNULLACL           1065	// ACL string argument null

#ifndef LIBJUAFS
#include <afs_Admin.h>
#include <afs_kasAdmin.h>

// make an identity out of a full name (possibly including an instance ) 
void internal_makeKasIdentity(const char *fullName, kas_identity_p who);

void internal_getUserClass(JNIEnv * env, jobject user);
void internal_getGroupClass(JNIEnv * env, jobject group);
void internal_getServerClass(JNIEnv * env, jobject server);
void internal_getPartitionClass(JNIEnv * env, jobject partition);
void internal_getVolumeClass(JNIEnv * env, jobject volume);
void internal_getKeyClass(JNIEnv * env, jobject key);
void internal_getProcessClass(JNIEnv * env, jobject process);
#else
int openAFSFile(JNIEnv * env, jstring fileNameUTF, int flags, int mode,
		int *err);
int readCacheParms(char *afsMountPoint, char *afsConfDir, char *afsCacheDir,
		   int *cacheBlocks, int *cacheFiles, int *cacheStatEntries,
		   int *dCacheSize, int *vCacheSize, int *chunkSize,
		   int *closeSynch, int *debug, int *nDaemons,
		   int *cacheFlags, char *logFile);
#endif /* !LIBJUAFS */

// throw a non-AFS exception with a message
void throwMessageException(JNIEnv * env, char *msg);

// throw an AFS exception with a message
void throwAFSException(JNIEnv * env, int code);

// throw an AFS Admin exception with a message
void throwAFSException(JNIEnv * env, int code);

// throw an AFS File or I/O related exception with a message
void throwFileAdminException(JNIEnv * env, int code, char *msg);

// throw an AFS Security exception with a message
void throwAFSSecurityException(JNIEnv * env, int code);

// throw an exception with an error code
void throwException(JNIEnv * env, jclass * excCls, char *excClsName,
		    jmethodID * initID, int code);

// reclaim global memory used by exceptions 
void reclaimExceptionMemory(JNIEnv * env, jclass cls);

int setError(JNIEnv * env, jobject * obj, int code);
int setString(JNIEnv * env, jobject * obj, char *field, char *string);
char *getNativeString(JNIEnv * env, jstring jstr);

#endif
