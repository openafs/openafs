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
#include <errno.h>

#include "Internal.h"
#include "org_openafs_jafs_File.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <jni.h>
#include <pthread.h>
/*#include <afs/afs_usrops.h>*/
#include <afs/prs_fs.h>
#include <sys/time.h>
#include <unistd.h>

/* Access Rights */
#define UAFS_READ 1
#define UAFS_WRITE 2
#define UAFS_INSERT 4
#define UAFS_LOOKUP 8
#define UAFS_DELETE 16
#define UAFS_LOCK 32
#define UAFS_ADMIN 64

#ifdef DMALLOC
#include "dmalloc.h"
#endif

void setFileNotExistsAttributes(JNIEnv *env, jobject *obj);

typedef struct {
    char *fileName;
    void *next;
} FileNameNode;

/*static char compile_date[] = COMPILE_DATE;*/

int sub_time(struct timeval *tv1, struct timeval *tv2)
{
   if (tv2->tv_usec > tv1->tv_usec) {
     tv1->tv_usec += 1000000;
     tv1->tv_usec -= 1;
   }

   tv1->tv_usec -= tv2->tv_usec;
   tv1->tv_sec -= tv2->tv_sec;
}

/**
 * Sets dirName to the absolute path according to the path field
 * within the Java File object (obj).
 *
 * env      the Java environment
 * obj      the current Java object
 */
char* getAbsolutePath(JNIEnv *env, jobject *obj, char *dirName)
{
    jclass thisClass;
    jstring jDirName;
    jmethodID getAbsolutePathID;
    const char *auxDirName;
    jfieldID fid;

    thisClass = (*env)->GetObjectClass(env, *obj);
    if(thisClass == NULL) {
      fprintf(stderr, "File::getAbsolutePath(): GetObjectClass failed\n");
      return NULL;
    }
    fid = (*env)->GetFieldID(env, thisClass, "path", "Ljava/lang/String;");
    if (fid == 0) {
      fprintf(stderr, "File::getAbsolutePath(): GetFieldID (path) failed\n");
      return NULL;
    }
    jDirName = (*env)->GetObjectField(env, *obj, fid);

    if(jDirName == NULL) {
      fprintf(stderr, "File::getAbsolutePath(): failed to get file name\n");
      return NULL;
    }
    auxDirName = (*env) -> GetStringUTFChars(env, jDirName, 0);
    strcpy(dirName, auxDirName);
    (*env) -> ReleaseStringUTFChars(env, jDirName, auxDirName);
}

/**
 * Performs a file stat on the actual AFS file and populates  
 * the Java object's respective field members with the appropriate values.
 * method will, if authorized, perform a stat on the
 * actual AFS file and update its respective field members; defining
 * this file object's attributes.
 *
 * env      the Java environment
 * obj      the current Java object
 *
 * return  true if and only if the current user is allowed to stat the file;
 *         false otherwise.
 *
 * throws AFSException
 */
JNIEXPORT jboolean JNICALL Java_org_openafs_jafs_File_setAttributes
  (JNIEnv *env, jobject obj)
{
    char target[FILENAME_MAX+1];
    char dirName[FILENAME_MAX];
    jclass thisClass;
    jfieldID fid;
    struct stat st;
    int rc, fd;
    int mtpoint = 0;
    jboolean islink = JNI_FALSE;
    int i;
    struct timeval tv0, tv1;
    struct timezone tz;

    /*memset(target, 0, FILENAME_MAX);*/

    *dirName = '\0';
    getAbsolutePath(env, &obj, dirName);

    /*fprintf(stderr, "dirName=%s\n", dirName);*/

    if(*dirName == '\0') {
      fprintf(stderr, "File::setAttributes(): failed to get dirName\n");
      return JNI_FALSE;
    }

    thisClass = (*env) -> GetObjectClass(env, obj);
    if(thisClass == NULL) {
      fprintf(stderr, "File::setAttributes(): GetObjectClass failed\n");
      return JNI_FALSE;
    }

    gettimeofday(&tv0, &tz);
    if ((strcmp(dirName, "/afs") == 0) || (strcmp(dirName, "/afs/") == 0)) {
      rc = 1;   /* special case for /afs since statmountpoint fails on it */
    } else {
      rc = uafs_statmountpoint(dirName);
      gettimeofday(&tv1, &tz);
      sub_time(&tv1, &tv0);
      /*printf("%s: statmountpoint %d.%06d\n", dirName, tv1.tv_sec, tv1.tv_usec);*/
    }

    if(rc < 0) {
      setError(env, &obj, errno);
      if(errno == ENOENT) {
        setFileNotExistsAttributes(env, &obj);
        return JNI_TRUE;   /* not really an error */
      } else {
        fprintf(stderr, "File::setAttributes(): uafs_statmountpoint failed "
                        "for %s (%s)\n", dirName, error_message(errno));
        return JNI_FALSE;
      }
    }

    if (rc == 1) {
      /* this is an AFS mount point; we don't want to stat it */
      /* fake up a stat entry instead */
      mtpoint = 1;
      st.st_mtime = 0;
      st.st_size = 2048;
      st.st_mode = 0;  /* don't match anything */
    } else {
      mtpoint = 0;
      fid = (*env)->GetFieldID(env, thisClass, "isLink", "Z");
      if (fid == 0) {
        fprintf(stderr, "File::setAttributes(): GetFieldID (isLink) failed\n");
        setError(env, &obj, -1);
        return JNI_FALSE;
      }
      islink = (*env)->GetBooleanField(env, obj, fid);
      if (islink != JNI_TRUE) {
        rc = uafs_lstat(dirName, &st);
      } else {
        /* Here we are being called on a link object for the second time,
           so we want info on the object pointed to by the link. */
        fprintf(stderr, "islink = TRUE on file %s\n", dirName);
        rc = uafs_stat(dirName, &st);
      }

      if(rc < 0) {
        setError(env, &obj, errno);
        fprintf(stderr, "uafs_lstat failed for dir %s: error %d\n",
                         dirName, errno);
        if(errno == ENOENT) {
          setFileNotExistsAttributes(env, &obj);
          return JNI_TRUE;
        }
        fprintf(stderr, 
            "File::setAttributes(): uafs_stat failed for %s (%s)\n", 
             dirName, error_message(errno));
        return JNI_FALSE;
      }
    }

    fid = (*env)->GetFieldID(env, thisClass, "exists", "Z");
    if (fid == 0) {
      fprintf(stderr, 
            "File::setAttributes(): GetFieldID (exists) failed\n");
      setError(env, &obj, -1);
      return JNI_FALSE;
    }
    (*env)->SetBooleanField(env, obj, fid, JNI_TRUE);

    fid = (*env)->GetFieldID(env, thisClass, "isDirectory", "Z");
    if (fid == 0) {
      fprintf(stderr, 
            "File::setAttributes(): GetFieldID (isDirectory) failed\n");
      setError(env, &obj, -1);
      return JNI_FALSE;
    }
    if ((st.st_mode & S_IFMT) == S_IFDIR && !mtpoint) {
      (*env)->SetBooleanField(env, obj, fid, JNI_TRUE);
    } else {
      (*env)->SetBooleanField(env, obj, fid, JNI_FALSE);
    }

    fid = (*env)->GetFieldID(env, thisClass, "isFile", "Z");
    if (fid == 0) {
      fprintf(stderr, 
            "File::setAttributes(): GetFieldID (isFile) failed\n");
      setError(env, &obj, -1);
      return JNI_FALSE;
    }
    if ((st.st_mode & S_IFMT) == S_IFREG) {
      (*env)->SetBooleanField(env, obj, fid, JNI_TRUE);
    } else {
      (*env)->SetBooleanField(env, obj, fid, JNI_FALSE);
    }

    fid = (*env)->GetFieldID(env, thisClass, "isLink", "Z");
    if (fid == 0) {
      fprintf(stderr, 
            "File::setAttributes(): GetFieldID (isLink) failed\n");
      setError(env, &obj, -1);
      return JNI_FALSE;
    }

    if (islink != JNI_TRUE) {    /* don't re-process link */
      if ((st.st_mode & S_IFMT) == S_IFLNK) {
        (*env)->SetBooleanField(env, obj, fid, JNI_TRUE);

        /* Find the target of the link */
        rc = uafs_readlink(dirName, target, FILENAME_MAX);
        if (rc < 0) {
          fprintf(stderr, "File::setAttributes(): uafs_readlink failed\n");
          setError(env, &obj, errno);
          return JNI_FALSE;
        } else {
          target[rc] = 0;     /* weird that we have to do this */
          /*fprintf(stderr, "readlink %s succeeded, target=%s, rc=%d\n", dirName,
            target, rc);*/
        }

        rc = setString(env, &obj, "target", target);
        if (rc < 0) {
          fprintf(stderr, "setString dirName=%s target=%s failed\n",
                           dirName, target);
        }
      } else {
        (*env)->SetBooleanField(env, obj, fid, JNI_FALSE);
      }
    }

    fid = (*env)->GetFieldID(env, thisClass, "isMountPoint", "Z");
    if (fid == 0) {
      fprintf(stderr, 
            "File::setAttributes(): GetFieldID (isMountPoint) failed\n");
      setError(env, &obj, -1);
      return JNI_FALSE;
    }

    if (mtpoint) {
      (*env)->SetBooleanField(env, obj, fid, JNI_TRUE);
    } else {
      (*env)->SetBooleanField(env, obj, fid, JNI_FALSE);
    }

    fid = (*env)->GetFieldID(env, thisClass, "lastModified", "J");
    if (fid == 0) {
      fprintf(stderr, 
            "File::setAttributes(): GetFieldID (lastModified) failed\n");
      setError(env, &obj, -1);
      return JNI_FALSE;
    }
    (*env)->SetLongField(env, obj, fid, ((jlong) st.st_mtime)*1000);

    fid = (*env)->GetFieldID(env, thisClass, "length", "J");
    if (fid == 0) {
      fprintf(stderr, 
            "File::setAttributes(): GetFieldID (length) failed\n");
      setError(env, &obj, -1);
      return JNI_FALSE;
    }
    (*env)->SetLongField(env, obj, fid, st.st_size);

    setError(env, &obj, 0);

    return JNI_TRUE;
}

/**
 * Returns the permission/ACL mask for the represented directory
 *
 * env      the Java environment
 * obj      the current Java object
 *
 * return  permission/ACL mask
 */
JNIEXPORT jint JNICALL Java_org_openafs_jafs_File_getRights
  (JNIEnv *env, jobject obj)
{
    char dirName[FILENAME_MAX];
    jclass thisClass;
    jfieldID fid;
    int rc;
    int afs_rights;
    int rights;

    *dirName = '\0';
    getAbsolutePath(env, &obj, dirName);

    if(*dirName == '\0') {
      fprintf(stderr, "File::getRights(): failed to get dirName\n");
      setError(env, &obj, -1);
      return JNI_FALSE;
    }

    /*-Access Rights-
     * UAFS_READ 1
     * UAFS_WRITE 2
     * UAFS_INSERT 4
     * UAFS_LOOKUP 8
     * UAFS_DELETE 16
     * UAFS_LOCK 32
     * UAFS_ADMIN 64
     */

    rights = 0;
    afs_rights = uafs_getRights(dirName);
    if (afs_rights < 0) {
      setError(env, &obj, errno);
      return -1;
    }
    
    if (afs_rights & PRSFS_READ)
      rights |= UAFS_READ;
    if (afs_rights & PRSFS_WRITE)
      rights |= UAFS_WRITE;
    if (afs_rights & PRSFS_INSERT)
      rights |= UAFS_INSERT;
    if (afs_rights & PRSFS_LOOKUP)
      rights |= UAFS_LOOKUP;
    if (afs_rights & PRSFS_DELETE)
      rights |= UAFS_DELETE;
    if (afs_rights & PRSFS_LOCK)
      rights |= UAFS_LOCK;
    if (afs_rights & PRSFS_ADMINISTER)
      rights |= UAFS_ADMIN;
    
    return rights;
}


/**
 * List the contents of the represented directory.
 *
 * env      the Java environment
 * obj      the current Java object
 *
 * return   the directory handle
 */
JNIEXPORT jlong JNICALL Java_org_openafs_jafs_File_listNative
  (JNIEnv *env, jobject obj, jobject buffer)
{
  return 0;
#if 0
    char dirName[FILENAME_MAX];
    jclass arrayListClass;
    jmethodID addID;
    jstring entryJString;
    usr_DIR *dirp;
    struct usr_dirent *enp;
    int i, dirSize;

    *dirName='\0';
    getAbsolutePath(env, &obj, dirName);
    if(*dirName == '\0') {
      fprintf(stderr, "File::listNative(): failed to get dirName\n");
      setError(env, &obj, -1);
      return 0;
    }
    arrayListClass = (*env)->GetObjectClass(env, buffer);
    if(arrayListClass == NULL) {
      fprintf(stderr, "File::listNative(): GetObjectClass failed\n");
      setError(env, &obj, -1);
      return 0;
    }
    addID = (*env) -> GetMethodID(env, arrayListClass, "add", 
                                  "(Ljava/lang/Object;)Z");
    if(addID == 0) {
      fprintf(stderr, 
            "File::listNative(): failed to get addID\n");
      setError(env, &obj, -1);
      return 0;
    }
    dirp = uafs_opendir(dirName);
    if(dirp == NULL) {
      fprintf(stderr, "File::listNative(): uafs_opendir(%s) failed(%s)\n",
                       dirName, error_message(errno));
      setError(env, &obj, errno);
      //throwAFSSecurityException( env, errno );
      return 0;
    }
    while((enp = uafs_readdir(dirp)) != NULL) {
	if(strcmp(enp->d_name, ".") == 0 || strcmp(enp->d_name, "..") == 0) {
        continue;
	}
      entryJString = (*env) -> NewStringUTF(env, enp->d_name);
      if(entryJString == NULL) {
        fprintf(stderr, "File::listNative(): NewStringUTF failed\n");
        setError(env, &obj, -1);
        return 0;
      }
      (*env) -> CallBooleanMethod(env, buffer, addID, entryJString);
    }
    /*uafs_closedir(dirp);*/

    setError(env, &obj, 0);

    return (jlong) dirp;
#endif
}

/**
 * Close the currently open directory using a provided directory handle.
 *
 * env      the Java environment
 * obj      the current Java object
 *
 * return  true if the directory closes without error
 */
JNIEXPORT jboolean JNICALL Java_org_openafs_jafs_File_closeDir
  (JNIEnv *env, jobject obj, jlong dp)
{

  return JNI_TRUE;

#if 0
  usr_DIR *dirp = (usr_DIR *) dp;
  int rc;
  
  rc = uafs_closedir(dirp);
  if (rc < 0) {
    setError(env, &obj, errno);
    return JNI_FALSE;
  }
  else return JNI_TRUE;
#endif

}


/**
 * Removes/deletes the represented file.
 *
 * env      the Java environment
 * obj      the current Java object
 *
 * return  true if the file is removed without error
 */
JNIEXPORT jboolean JNICALL Java_org_openafs_jafs_File_rmfile
  (JNIEnv *env, jobject obj)
{
    char dirName[FILENAME_MAX];
    int rc;

    *dirName='\0';
    getAbsolutePath(env, &obj, dirName);
    if(*dirName == '\0') {
      setError(env, &obj, EINVAL);
      fprintf(stderr, "File::rmfile(): failed to get dirName\n");
      return JNI_FALSE;
    }
    rc = uafs_unlink(dirName);
    if(rc < 0) {
      setError(env, &obj, errno);
      fprintf(stderr, "File::rmfile(): uafs_unlink failed\n");
      return JNI_FALSE;
    }
    setError(env, &obj, 0);
    return JNI_TRUE;
}

/**
 * Removes/deletes the represented directory.
 *
 * env      the Java environment
 * obj      the current Java object
 *
 * return  true if the directory is removed without error
 */
JNIEXPORT jboolean JNICALL Java_org_openafs_jafs_File_rmdir
  (JNIEnv *env, jobject obj)
{
    char dirName[FILENAME_MAX];
    int rc;

    *dirName='\0';
    getAbsolutePath(env, &obj, dirName);
    if(*dirName == '\0') {
      setError(env, &obj, -1);
      fprintf(stderr, "File::rmdir(): failed to get dirName\n");
      return JNI_FALSE;
    }
    rc = uafs_rmdir(dirName);
    if(rc < 0) {
      setError(env, &obj, errno);
      fprintf(stderr, "File::rmdir(): uafs_unlink failed\n");
      return JNI_FALSE;
    }
    setError(env, &obj, 0);
    return JNI_TRUE;
}

/**
 * Creates the directory named by this abstract pathname.
 *
 * env      the Java environment
 * obj      the current Java object
 *
 * return  true if and only if the directory was
 *          created; false otherwise
 */
JNIEXPORT jboolean JNICALL Java_org_openafs_jafs_File_mkdir
  (JNIEnv *env, jobject obj)
{
    char dirName[FILENAME_MAX];
    int rc;

    *dirName='\0';
    getAbsolutePath(env, &obj, dirName);
    if(*dirName == '\0') {
      setError(env, &obj, EINVAL);
      fprintf(stderr, "File::mkdir(): failed to get dirName\n");
      return JNI_FALSE;
    }
    rc = uafs_mkdir(dirName, 0755);
    if(rc < 0) {
      setError(env, &obj, errno);
      fprintf(stderr, "File::mkdir(): uafs_mkdir failed\n");
      return JNI_FALSE;
    }
    setError(env, &obj, 0);
    return JNI_TRUE;
}

/**
 * Renames the file denoted by this abstract pathname.
 *
 * env      the Java environment
 * obj      the current Java object
 * dest     the new abstract pathname for the named file
 * 
 * return   true if and only if the renaming succeeded;
 *          false otherwise
 *
 * throws   NullPointerException  
 *          If parameter dest is null
 */
JNIEXPORT jboolean JNICALL Java_org_openafs_jafs_File_renameTo
  (JNIEnv *env, jobject obj, jobject newFile)
{
    char thisDirName[FILENAME_MAX], newDirName[FILENAME_MAX];
    int rc;

    *thisDirName='\0';
    getAbsolutePath(env, &obj, thisDirName);
    if(*thisDirName == '\0') {
      setError(env, &obj, -1);
      fprintf(stderr, "File::renameTo(): failed to get dirName for this \n");
      return JNI_FALSE;
    }
    *newDirName='\0';
    getAbsolutePath(env, &newFile, newDirName);
    if(*newDirName == '\0') {
      setError(env, &obj, -1);
      fprintf(stderr, "File::renameTo(): failed to get dirName for new \n");
      return JNI_FALSE;
    }
    rc = uafs_rename(thisDirName, newDirName);
    if(rc < 0) {
      setError(env, &obj, errno);
      fprintf(stderr, "File::renameTo(): uafs_rename failed\n");
      return JNI_FALSE;
    }
    return JNI_TRUE;
}

/**
 * Set all the necessary fields within the Java File class to indicate the
 * does not exist.
 *
 * env      the Java environment
 * obj      the current Java object
 */
void setFileNotExistsAttributes
    (JNIEnv *env, jobject *obj)
{
    jclass thisClass;
    jfieldID fid;

    thisClass = (*env) -> GetObjectClass(env, *obj);
    if(thisClass == NULL) {
      fprintf(stderr, 
            "File::setFileNotExistsAttributes(): GetObjectClass failed\n");
      return;
    }

    fid = (*env)->GetFieldID(env, thisClass, "exists", "Z");
    if (fid == 0) {
      fprintf(stderr, 
            "File::setFileNotExistsAttributes(): GetFieldID (exists) failed\n");
      return;
    }
    (*env)->SetBooleanField(env, *obj, fid, JNI_FALSE);

    fid = (*env)->GetFieldID(env, thisClass, "isDirectory", "Z");
    if (fid == 0) {
      fprintf(stderr, 
            "File::setFileNotExistsAttributes(): GetFieldID (isDirectory) failed\n");
      return;
    }
    (*env)->SetBooleanField(env, *obj, fid, JNI_FALSE);

    fid = (*env)->GetFieldID(env, thisClass, "isFile", "Z");
    if (fid == 0) {
      fprintf(stderr, 
            "File::setFileNotExistsAttributes(): GetFieldID (isDirectory) failed\n");
      return;
    }
    (*env)->SetBooleanField(env, *obj, fid, JNI_FALSE);

    fid = (*env)->GetFieldID(env, thisClass, "lastModified", "J");
    if (fid == 0) {
      fprintf(stderr, 
            "File::setFileNotExistsAttributes(): GetFieldID (lastModified) failed\n");
      return;
    }
    (*env)->SetLongField(env, *obj, fid, 0);

    
    fid = (*env)->GetFieldID(env, thisClass, "length", "J");
    if (fid == 0) {
      fprintf(stderr, 
            "File::setFileNotExistsAttributes(): GetFieldID (length) failed\n");
      return;
    }
    (*env)->SetLongField(env, *obj, fid, 0);

    return;
}


