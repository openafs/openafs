/*
 * Copyright (c) 2001-2002 International Business Machines Corp.
 * All rights reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afs/param.h>
#include <afs/stds.h>

#include "Internal.h"
#include "org_openafs_jafs_ACL.h"

#include <stdio.h>
#include <sys/ioctl.h>
#include <afs/vice.h>
#include <netinet/in.h>
#include <afs/venus.h>
#include <afs/afs_args.h>

#include "GetNativeString.h"

/*
  #include <afs/afs_osi.h>
*/

/* just for debugging */
#define	MAXHOSTS 13
#define	OMAXHOSTS 8
#define MAXNAME 100
#define	MAXSIZE	2048
#define MAXINSIZE 1300    /* pioctl complains if data is larger than this */
#define VMSGSIZE 128      /* size of msg buf in volume hdr */

static char space[MAXSIZE];

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define ACL_LEN      1024

extern int errno;

/**
 * Returns a formatted string representing the ACL for the specified path.
 *
 * path     the directory path
 * returns NULL if an exception is encountered.
 */
char* getACL(char *path)
{
    struct ViceIoctl params;
    char *buffer;
    int rval1=0;

    buffer = (char*) malloc(ACL_LEN);
    params.in = NULL;
    params.out = NULL;
    params.in_size = params.out_size = 0;
    
    if (!buffer) {
      fprintf(stderr, "ERROR: ACL::getACL -> could not allocate buffer\n");
      return NULL;
    }

    params.out = buffer;
    params.out_size = ACL_LEN; 

#if defined(AFS_PPC64_LINUX20_ENV) || defined(AFS_S390X_LINUX20_ENV)
    if(pioctl(path, VIOCGETAL, &params, 1)) {
#else /* AFS_PPC_LINUX20_ENV */
    if(syscall(AFS_SYSCALL, AFSCALL_PIOCTL, path, VIOCGETAL, &params, 1)) {
#endif /* AFS_PPC_LINUX20_ENV */
      fprintf(stderr, "ERROR: ACL::getACL -> VIOCGETAL failed: %d, path: %s\n", errno, path);
      free(buffer);
      return NULL;
    }

    return params.out;
}

/**
 * Sets the ACL for the specified path using the provided string
 * representation.
 *
 * path       the directory path
 * aclString  string representation of ACL to be set
 * returns TRUE if the operation succeeds; otherwise FALSE;
 */
jboolean setACL(char *path, char *aclString)
{
    struct ViceIoctl params;
    char *redirect, *parentURI, *cptr;

    params.in = aclString;
    params.in_size = strlen(aclString) + 1;
    params.out = NULL;
    params.out_size = 0;

#if defined(AFS_PPC64_LINUX20_ENV) || defined(AFS_S390X_LINUX20_ENV)
    if(pioctl(path, VIOCSETAL, &params, 1)) {
#else /* AFS_PPC_LINUX20_ENV */
    if(syscall(AFS_SYSCALL, AFSCALL_PIOCTL, path, VIOCSETAL, &params, 1)) {
#endif /* AFS_PPC_LINUX20_ENV */
      fprintf(stderr, "ERROR: ACL::setACL -> VIOCSETAL failed: %d, path: %s\n", errno, path);
      return JNI_FALSE;
    }

    return JNI_TRUE;
}

/**
 * Returns a formatted string representing the ACL for the specified path.
 *
 * The string format is in the form of a ViceIoctl and is as follows:
 * printf("%d\n%d\n", positiveEntriesCount, negativeEntriesCount);
 * printf("%s\t%d\n", userOrGroupName, rightsMask);
 *
 * path     the directory path
 * returns NULL if an exception is encountered.
 */
JNIEXPORT jstring JNICALL Java_org_openafs_jafs_ACL_getACLString
  (JNIEnv *env, jobject obj, jstring pathUTF)
{
    char *path, *acl;
    jstring answer = NULL;

    /*jchar* wpath;
    path = (char*) (*env)->GetStringUTFChars(env, pathUTF, 0);
    wpath=(jchar*) (*env)->GetStringChars(env,pathUTF,0);*/

    path = GetNativeString(env,pathUTF);

    if(path == NULL) {
      fprintf(stderr, "ERROR: ACL::getACLString ->");
      fprintf(stderr, "path = NULL\n");
      throwMessageException( env, "Path is NULL" ); 
      return NULL;
    }

    acl = getACL(path);

    if(acl) {
      answer =  (*env) -> NewStringUTF(env, acl);
      free(acl);
    } else {
      throwAFSException( env, errno );
    }

    /*(*env)->ReleaseStringUTFChars(env, pathUTF, path);
      (*env)->ReleaseStringChars(env, pathUTF, (jchar*)wpath);*/

    free(path); //psomogyi memory leak - added
    return answer;
}

/**
 * Sets the ACL for the specified path using the provided string
 * representation.
 *
 * The string format is in the form of a ViceIoctl and is as follows:
 * printf("%d\n%d\n", positiveEntriesCount, negativeEntriesCount);
 * printf("%s\t%d\n", userOrGroupName, rightsMask);
 *
 * path       the directory path
 * aclString  string representation of ACL to be set
 * throws an afsAdminExceptionName if an internal exception is encountered.
 */
JNIEXPORT void JNICALL Java_org_openafs_jafs_ACL_setACLString
  (JNIEnv *env, jobject obj, jstring pathUTF, jstring aclStringUTF)
{
    char *path, *aclString;

    if(!pathUTF) {
      fprintf(stderr, "ERROR: ACL::setACLString -> pathUTF == NULL\n");
      throwMessageException( env, "pathUTF == NULL" );
      return;
    }

    /*path = (char*) (*env)->GetStringUTFChars(env, pathUTF, 0);*/
    path = GetNativeString(env,pathUTF);

    if(path == NULL) {
      fprintf(stderr, "ERROR: ACL::setACLString -> failed to get path\n");
      throwMessageException( env, "Failed to get path" );
      return;
    }

    if(!aclStringUTF) {
      fprintf(stderr, "ERROR: ACL::setACLString -> aclStringUTF == NULL\n");
      throwMessageException( env, "aclStringUTF == NULL" ); 
      return;
    }

    /*aclString = (char*) (*env)->GetStringUTFChars(env, aclStringUTF, 0);*/
    aclString = GetNativeString(env,aclStringUTF);

    if(aclString == NULL) {
      fprintf(stderr, "ERROR: ACL::setACLString -> failed to get aclString\n");
      (*env)->ReleaseStringUTFChars(env, pathUTF, path);
      throwMessageException( env, "aclString == NULL" ); 
      return;
    }

    if (!setACL(path, aclString)) {
      throwAFSException( env, errno );
    }

    /*(*env)->ReleaseStringUTFChars(env, pathUTF, path);
      (*env)->ReleaseStringUTFChars(env, aclStringUTF, aclString);*/

    free(path);
    free(aclString);
}



