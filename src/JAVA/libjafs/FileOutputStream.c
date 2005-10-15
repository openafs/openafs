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
#include "org_openafs_jafs_FileOutputStream.h"

#include <stdio.h>
#include <fcntl.h>
/*#include <afs/afs_usrops.h>*/

#ifdef DMALLOC
#include "dmalloc.h"
#endif

/**
 * Be carefull with the memory management:
 *
 * - For every GetStringUTFChars call the corresponding ReleaseStringUTFChars.
 * - For every Get<type>ArrayElements call the corresponding
 *   Release<type>ArrayElements
 * - For every malloc call the corresponding free.
 */

/*-------------------------------------------------------------------------*/

/**
 * Opens an AFS file, with the specified name, for appending.
 * 
 *  env		the Java environment
 *  obj		the current Java object
 *  fileNameUTF	name of file to be opened
 *
 * @returns		file descriptor
 * @exception	AFSFileException  if an I/O or other file related error occurs.
 */
JNIEXPORT jint JNICALL Java_org_openafs_jafs_FileOutputStream_openWrite
  (JNIEnv *env, jobject obj, jstring fileNameUTF)
{
  int err;
  jint fd = -1;		//file descriptor

  fd = openAFSFile(env, fileNameUTF, O_CREAT | O_TRUNC, 0644, &err);
  if (fd < 0) {
    fprintf(stderr, "FileOutputStream::openWrite(): err=%d\n", err);
    throwAFSFileException(env, err, NULL);
  }
  return fd;
}

/**
 * Opens an AFS file, with the specified name, for writing.
 * 
 *  env		the Java environment
 *  obj		the current Java object
 *  fileNameUTF	name of file to be opened
 *
 * @return		file descriptor
 * @exception	AFSFileException  if an I/O or other file related error occurs.
 */
JNIEXPORT jint JNICALL Java_org_openafs_jafs_FileOutputStream_openAppend
  (JNIEnv *env, jobject obj, jstring fileNameUTF)
{
  int err;
  jint fd = -1;		//file descriptor

  fd = openAFSFile(env, fileNameUTF, O_CREAT | O_APPEND, 0644, &err);
  if (fd < 0) {
    fprintf(stderr, "FileOutputStream::openAppend(): err=%d\n", err);
    throwAFSFileException(env, err, NULL);
  }
  return fd;
}

/**
 * Writes 'lenght' bytes from the specified byte array starting at offset 
 * 'offset' to this file output stream. 
 * 
 *  env		the Java environment
 *  obj		the current Java object
 *  jbytes 		the data to be written
 *  offset 		the start offset in the data
 *  length 		the number of bytes that are written
 *
 * @exception	AFSFileException  if an I/O or other file related error occurs.
 */
JNIEXPORT void JNICALL Java_org_openafs_jafs_FileOutputStream_write
  (JNIEnv *env, jobject obj, jbyteArray jbytes, jint offset, jint length)
{
    int fd, written, toWrite;
    jint twritten;
    jclass thisClass;
    jmethodID getFileDescriptorID;
    char *bytes;
    jfieldID fid;

    thisClass = (*env)->GetObjectClass(env, obj);
    fid = (*env)->GetFieldID(env, thisClass, "fileDescriptor", "I");
    fd = (*env)->GetIntField(env, obj, fid);
    if(fd < 0) {
      fprintf(stderr, "FileOutputStream::write(): failed to get file "
                       "descriptor\n");
      throwAFSFileException(env, 0, "Failed to get file descriptor!");
    }
    bytes = (char*) malloc(length);
    if(bytes == NULL) {
      fprintf(stderr, "FileOutputStream::write(): malloc failed of %d bytes\n",
                       length);
      throwAFSFileException(env, 0, "Failed to allocate memory!");
    }
    (*env) -> GetByteArrayRegion(env, jbytes, offset, length, bytes);
    toWrite = length;
    twritten = 0;
    while(toWrite>0) {
      written = uafs_write(fd, bytes, length);
      twritten += written;
      if(written<0) {
        free(bytes);
        throwAFSFileException(env, errno, NULL);
      }
      toWrite -= written;
    }
    free(bytes);
}

/**
 * Closes this file output stream and releases any system resources 
 * associated with this stream. This file output stream may no longer 
 * be used for writing bytes. 
 * 
 *  env		the Java environment
 *  obj		the current Java object
 *
 * @exception	AFSFileException  if an I/O or other file related error occurs.
 */
JNIEXPORT void JNICALL Java_org_openafs_jafs_FileOutputStream_close
  (JNIEnv *env, jobject obj)
{
    int fd, rc;
    jclass thisClass;
    jmethodID getFileDescriptorID;
    char *bytes;
    jfieldID fid;

    thisClass = (*env)->GetObjectClass(env, obj);
    fid = (*env)->GetFieldID(env, thisClass, "fileDescriptor", "I");
    fd = (*env)->GetIntField(env, obj, fid);
    if(fd < 0) {
      fprintf(stderr, "FileOutputStream::close(): failed to get file descriptor\n");
      throwAFSFileException(env, 0, "Failed to get file descriptor!");
    }
    rc = uafs_close(fd);
    if (rc != 0) {
      throwAFSFileException(env, rc, NULL);
    }
}


