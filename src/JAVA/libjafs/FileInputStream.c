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

#include "Internal.h"
#include "org_openafs_jafs_FileInputStream.h"

#include <fcntl.h>
#include <errno.h>

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
 * @return		file descriptor
 * @exception	AFSFileException  if an I/O or other file related error occurs.
 */
JNIEXPORT jint JNICALL Java_org_openafs_jafs_FileInputStream_openReadOnly
  (JNIEnv *env, jobject obj, jstring fileNameUTF)
{
  int err;
  int fd = -1;		//file descriptor

  fd = openAFSFile(env, fileNameUTF, O_RDONLY, 0, &err);
  if (fd < 0) {
    fprintf(stderr, "FileInputStream::openReadOnly(): err=%d\n", err);
    throwAFSFileException( env, err, NULL );
  }
  return fd;
}

/**
 * Reads up to 'length' bytes of data from this input stream
 * into an array of bytes. This method blocks until some input is
 * available.
 * 
 *  env		the Java environment
 *  obj		the current Java object
 *  jbytes 		the data to be written
 *  offset 		the start offset in the data
 *  length 		the number of bytes that are written
 *
 * @return		the total number of bytes read into the buffer, or
 *			<code>-1</code> if there is no more data because the end of
 *			the file has been reached.
 * @exception	AFSFileException  if an I/O or other file related error occurs.
 */
JNIEXPORT jint JNICALL Java_org_openafs_jafs_FileInputStream_read
  (JNIEnv *env, jobject obj, jbyteArray jbytes, jint offset, jint length)
{
  int fd, bytesLen, bytesRead;
  jclass thisClass;
  jmethodID getFileDescriptorID;
  jbyte *bytes;
  jfieldID fid;

  /* If we have to read 0 bytes just return */
  if(length == 0) return 0;

  thisClass = (*env)->GetObjectClass(env, obj);
  fid = (*env)->GetFieldID(env, thisClass, "fileDescriptor", "I");
  fd = (*env)->GetIntField(env, obj, fid);

  if(fd < 0) {
    fprintf(stderr, "FileInputStream::read(): invalid file state\n");
    throwAFSFileException(env, 0, "Invalid file state");
    return -1;
  }

  bytes = (*env) -> GetByteArrayElements(env, jbytes, 0);
  bytesLen = (*env) -> GetArrayLength(env, jbytes);
  bytesRead = uafs_read(fd, bytes, bytesLen);

  if (errno != 0) throwAFSFileException(env, errno, NULL);

  (*env) -> ReleaseByteArrayElements(env, jbytes, bytes, 0);
  return (bytesRead > 0) ? bytesRead : -1;
}

/**
 * Closes this file input stream and releases any system resources 
 * associated with this stream. This file input stream may no longer 
 * be used for writing bytes. 
 * 
 *  env		the Java environment
 *  obj		the current Java object
 *
 * @exception	AFSFileException  if an I/O or other file related error occurs.
 */
JNIEXPORT void JNICALL Java_org_openafs_jafs_FileInputStream_close
  (JNIEnv *env, jobject obj)
{
  int fd, rc;
  jclass thisClass;
  jmethodID getFileDescriptorID;
  jfieldID fid;
  char *bytes;

  thisClass = (*env)->GetObjectClass(env, obj);
  fid = (*env)->GetFieldID(env, thisClass, "fileDescriptor", "I");
  fd = (*env)->GetIntField(env, obj, fid);

  if(fd < 0) {
    fprintf(stderr, "FileInputStream::close(): invalid file state\n");
    throwAFSFileException(env, 0, "Invalid file state");
    return;
  }

  rc = uafs_close(fd);

  if (rc != 0) {
    throwAFSFileException(env, errno, NULL);
  }
}



