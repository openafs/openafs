/*
 * @(#)FilterInputStream.java	1.0 00/10/10
 *
 * Copyright (c) 2001 International Business Machines Corp.
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

package org.openafs.jafs;

import java.io.InputStream;

/**
 * This class is a file input stream for files within AFS.  
 * It is an input stream for reading data from a 
 * <code>{@link org.openafs.jafs.File}</code>.
 *
 * @version 2.1, 08/03/2001
 * @see org.openafs.jafs.File
 * @see org.openafs.jafs.FileOutputStream
 * @see java.io.FileInputStream
 */
public class FileInputStream extends InputStream
{
  /** Status indicator for the current state of this file input stream */
  private int fileDescriptor;

  /**
   * Creates a <code>FileInputStream</code> by
   * opening a connection to an actual AFS file,
   * the file named by the path name <code>name</code>
   * in the AFS file system.
   *
   * @param name the name of the file to read from
   * @exception	AFSFileException  If an AFS specific error occurs, 
   *			if the file does not, or cannot be opened for any 
   *			other reason, including authorization.
   */
  public FileInputStream(String name) throws AFSFileException
  {
    this.fileDescriptor = this.openReadOnly(name);
  }
  /**
   * Creates a <code>FileInputStream</code> by
   * opening a connection to an actual AFS file,
   * the file represented by file <code>file</code>
   * in the AFS file system.
   *
   * @param file	an AFS file object representing a file to read from
   * @exception	AFSFileException  If an AFS specific error occurs, 
   *			if the file does not, or cannot be opened for any 
   *			other reason, including authorization.
   */
  public FileInputStream(File file) throws AFSFileException
  {
    this(file.getPath());
  }

  /*-------------------------------------------------------------------------*/

  /**
   * Reads the next byte of data from this input stream. The value 
   * byte is returned as an <code>int</code> in the range 
   * <code>0</code> to <code>255</code>. If no byte is available 
   * because the end of the stream has been reached, the value 
   * <code>-1</code> is returned. This method blocks until input data 
   * is available, the end of the stream is detected, or an exception 
   * is thrown. 
   *
   * <p>This method simply performs <code>in.read()</code> and returns 
   * the result.
   *
   * @return   the next byte of data, or <code>-1</code> if the end of the
   *           stream is reached.
   * @exception  AFSFileException  if an I/O or other file related error occurs.
   * @see        java.io.FileInputStream#read
   */
  public int read() throws AFSFileException
  {
    byte[] bytes = new byte[1];
    this.read(bytes, 0, 1);
    return bytes[0];
  }
  /**
   * Reads up to <code>b.length</code> bytes of data from this input
   * stream into an array of bytes. This method blocks until some input
   * is available.
   *
   * @param      b   the buffer into which the data is read.
   * @return     the total number of bytes read into the buffer, or
   *             <code>-1</code> if there is no more data because the end of
   *             the file has been reached.
   * @exception  AFSFileException  if an I/O or other file related error occurs.
   */
  public int read(byte[] b) throws AFSFileException
  {
    return this.read(b, 0, b.length);
  }

  /////////////// public native methods ////////////////////

  /**
   * Reads up to <code>len</code> bytes of data from this input stream
   * into an array of bytes. This method blocks until some input is
   * available.
   *
   * @param      b     the buffer into which the data is read.
   * @param      off   the start offset of the data.
   * @param      len   the maximum number of bytes read.
   * @return     the total number of bytes read into the buffer, or
   *             <code>-1</code> if there is no more data because the end of
   *             the file has been reached.
   * @exception  AFSFileException  if an I/O or other file related error occurs.
   */
  public native int read(byte[] b, int off, int len) throws AFSFileException;
  /**
   * Skips over and discards <code>n</code> bytes of data from the
   * input stream. The <code>skip</code> method may, for a variety of
   * reasons, end up skipping over some smaller number of bytes,
   * possibly <code>0</code>. The actual number of bytes skipped is returned.
   *
   * @param      n   the number of bytes to be skipped.
   * @return     the actual number of bytes skipped.
   * @exception  AFSFileException  if an I/O or other file related error occurs.
   */
  public native long skip(long n) throws AFSFileException;
  /**
   * Closes this file input stream and releases any system resources
   * associated with the stream.
   *
   * @exception  AFSFileException  if an I/O or other file related error occurs.
   */
  public native void close() throws AFSFileException;

  /////////////// private native methods ////////////////////

  /**
   * Opens the specified AFS file for reading.
   *
   * @param name fileName of file to be opened
   * @return    file descriptor
   * @exception  AFSFileException  if an I/O or other file related error occurs.
   */
  private native int openReadOnly(String fileName) throws AFSFileException;

  /*-------------------------------------------------------------------------*/
}





