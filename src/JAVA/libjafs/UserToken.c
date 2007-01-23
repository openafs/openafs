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

#include "Internal.h"
#include "org_openafs_jafs_Token.h"

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <afs/vice.h>
#include <netinet/in.h>
#include <afs/venus.h>
#include <afs/afs_args.h>
/*#include <afs/afs_osi.h>
  #include <afs/afs_usrops.h>*/
#include <pthread.h>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

pthread_mutex_t jafs_init_lock;
extern pthread_mutex_t jafs_login_lock;
extern int readCacheParms(char *afsMountPoint, char *afsConfDir,
                          char *afsCacheDir,  int *cacheBlocks, 
                          int *cacheFiles, int *cacheStatEntries,
                          int *dCacheSize, int *vCacheSize, int *chunkSize,
                          int *closeSynch, int *debug, int *nDaemons, 
                          int *cacheFlags, char *logFile);

/**
 * Be carefull with the memory management:
 *
 * - For every GetStringUTFChars call the corresponding ReleaseStringUTFChars.
 * - For every Get<type>ArrayElements call the corresponding
 *   Release<type>ArrayElements
 * - For every malloc call the corresponding free.
 */

/*JNIEXPORT void JNICALL Java_org_openafs_jafs_Token_callDebugger
  (JNIEnv *env, jobject obj)
{
    fprintf(stderr, "callDebugger called\n");
    __asm__("int $0x3");
}*/

/**
 * Initialize the user space library.
 *
 * The user space client must be initialized prior to any
 * user space related methods, including: klog, unlog, relog,
 * and shutdown.
 *
 * env      the Java environment
 * cls      the current Java class
 *
 * throws AFSException
 */
JNIEXPORT void JNICALL Java_org_openafs_jafs_Token_initUserSpace
  (JNIEnv *env, jclass cls)
{
  char afsMountPoint[100], afsConfDir[100], afsCacheDir[100], logFile[100];
  jfieldID fid;
  int pagval;

  /* Initialize each init parameter with its associated default value */
  int cacheBlocks = 100000;
  int cacheFiles  = 12500;
  int cacheStatEntries = 8192;
  int dCacheSize  = 11398;
  int vCacheSize  = 128;
  int chunkSize   = 0;
  int closeSynch  = 0;
  int debug       = 0;
  int nDaemons    = 3; 
  int cacheFlags  = -1;

  /* Initialize each init parameter with its associated default value */
  strcpy(afsMountPoint, "/afs");
  strcpy(afsConfDir,  "/usr/afswsp/etc");
  strcpy(afsCacheDir, "/usr/afswsp/cache");
  strcpy(logFile,     "/usr/afswsp/log/libjafs.log");

  pthread_mutex_init(&jafs_init_lock, NULL);
  pthread_mutex_lock(&jafs_init_lock);

  readCacheParms(afsMountPoint, afsConfDir, afsCacheDir,
                 &cacheBlocks, &cacheFiles, &cacheStatEntries,
                 &dCacheSize, &vCacheSize, &chunkSize,
                 &closeSynch, &debug, &nDaemons, &cacheFlags,
                 logFile);

  /* See cache.tune for configuration details */
  if (debug) {
    fprintf(stderr, "uafs_Init(\"init_native\", \"%s\", \"%s\", \"%s\"",
                      "%d, %d, %d,"
                      "%d, %d, %d,"
                      "%d, %d, %d, %d, \"%s\");\n",
              afsMountPoint, afsConfDir, afsCacheDir,
              cacheBlocks, cacheFiles, cacheStatEntries,
              dCacheSize, vCacheSize, chunkSize,
              closeSynch, debug, nDaemons, cacheFlags, logFile);
  }
  uafs_Init("init_native", afsMountPoint, afsConfDir, afsCacheDir,
             cacheBlocks, cacheFiles, cacheStatEntries,
             dCacheSize, vCacheSize, chunkSize,
             closeSynch, debug, nDaemons, cacheFlags, logFile);


  /* make the initial pag the unauthenticated pag */
  afs_setpag();
  uafs_unlog();
  pagval = afs_getpag_val();

  fid = (*env)->GetStaticFieldID(env, cls, "ANYUSER_PAG_ID", "I");
  if (fid == 0) {
    fprintf(stderr,
    "UserToken::init(): GetFieldID (ANYUSER_PAG_ID) failed\n");
    return;
  }
    
  (*env)->SetStaticIntField(env, cls, fid, pagval);

  pthread_mutex_unlock(&jafs_init_lock);
}

/**
 * Authenticates a user in kas, and binds that authentication
 * to the current process.
 *
 * env      the Java environment
 * obj      the current Java class
 * loginUTF    the login to authenticate (expected as username@cellname)
 * passwordUTF the password of the login
 * id		the existing pag (or 0)
 *
 * returns the assigned pag
 *
 * throws AFSException
 */
JNIEXPORT jint JNICALL
Java_org_openafs_jafs_Token_klog (JNIEnv *env, jobject obj,
  jstring jusername, jstring jpassword, jstring jcell, jint id)
{
  char *username;
  char *password;
  char *cell;
  char *reason;
  jint rc = -1;
  int code;

  if( jcell != NULL ) { 
    cell = (char*) (*env)->GetStringUTFChars(env, jcell, 0);
    if( !cell ) {
      char *error = "UserToken::klog(): failed to get cell name\n";
      fprintf(stderr, error);
      throwMessageException( env, error );
      return -1;
    }
  } else {
    cell = NULL;
  }

  if( jusername != NULL ) {
    username = (char*) (*env)->GetStringUTFChars(env, jusername, 0);
    if( !username ) {
      char *error = "UserToken::klog(): failed to get username\n";
      (*env)->ReleaseStringUTFChars(env, jcell, cell);
      fprintf(stderr, error);
      throwMessageException( env, error );
      return -1;
    }
  } else {
    username = NULL;
  }
  if( jpassword != NULL ) {
    password = (char*) (*env)->GetStringUTFChars(env, jpassword, 0);
    if( !password ) {
      char *error = "UserToken::klog(): failed to get password\n";
      (*env)->ReleaseStringUTFChars(env, jcell, cell);
      (*env)->ReleaseStringUTFChars(env, jusername, username);
      fprintf(stderr, error);
      throwMessageException( env, error );
      return -1;
    }
  } else {
    password = NULL;
  }

  if (id == 0) {
    code = uafs_klog(username, cell, password, &reason);
  } else {
    /* Use existing PAG for this thread */
    code = afs_setpag_val(id);
    if (code != 0) code = 180492L;  /* KABADARGUMENT */
    if (!code) code = uafs_klog_nopag(username, cell, password, &reason);
  }

  if (code != 0) {
    if( cell != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jcell, cell);
    }
    if( username != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jusername, username);
    }
    if( password != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jpassword, password);
    }
    fprintf(stderr, "UserToken::klog(): uafs_klog failed to cell %s: %s\n",
                     cell, reason);
    fprintf(stderr, "code = %d\n", code);
    throwAFSException( env, code );
  }

  /* Get the PAG we were assigned as the return value */
  rc = afs_getpag_val();

  /* clean up */
  if( cell != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jcell, cell);
  }
  if( username != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jusername, username);
  }
  if( password != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jpassword, password);
  }

  /* return PAG ID */
  return rc;
}

/**
 * Authenticates a user in KAS by a previously acquired PAG ID, and binds 
 * that authentication to the current thread or native process.
 *
 * <P> This method does not require the user's username and password to
 * fully authenticate their request.  Rather it utilizes the user's PAG ID
 * to recapture the user's existing credentials.
 *
 * env      the Java environment
 * obj      the current Java class
 * id       User's current PAG (process authentication group) ID
 *
 * throws AFSException
 */
JNIEXPORT void JNICALL Java_org_openafs_jafs_Token_relog
  (JNIEnv *env, jobject obj, jint id)
{
  int rc;

  rc = afs_setpag_val(id);

  if (rc != 0) {
    throwAFSException( env, rc );
  }
}

/**
 * Authenticates a user in KAS, and binds that authentication
 * to the current process.
 *
 * env      the Java environment
 * obj      the current Java class
 *
 * throws AFSException
 */
JNIEXPORT void JNICALL Java_org_openafs_jafs_Token_unlog
  (JNIEnv *env, jobject obj)
{
  int rc;

  rc = uafs_unlog();

  if (rc != 0) {
    throwAFSException( env, rc );
  }
}

/**
 * Inform the native library that the application is 
 * shutting down and will be unloading.
 *
 * <p> The library will make a call informing the file server that it will 
 * no longer be available for callbacks.
 *
 * env      the Java environment
 * obj      the current Java class
 *
 * throws AFSException
 */
JNIEXPORT void JNICALL Java_org_openafs_jafs_Token_shutdown
  (JNIEnv *env, jobject obj)
{
  uafs_Shutdown();
}




