/*
 * @(#)FilterOutputStream.java	1.0 00/10/10
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

import java.io.OutputStream;

/**
 * This class is a file output stream for files within AFS.  
 * It is an output stream for writing data to a 
 * <code>{@link org.openafs.jafs.File}</code>.
 *
 * @version 2.1, 08/03/2001
 * @see org.openafs.jafs.File
 * @see org.openafs.jafs.FileInputStream
 * @see java.io.FileOutputStream
 */
public class FileOutputStream extends OutputStream 
{
  /** Status indicator for the current state of this file output stream */
  private int fileDescriptor;

  /**
   * Creates an output file stream to write to the AFS file with the 
   * specified name.
   * <p>
   * If the file exists but is a directory rather than a regular file, does
   * not exist but cannot be created, or cannot be opened for any other
   * reason then a <code>AFSFileException</code> is thrown.
   *
   * @param name the name of the file to write to
   * @exception	AFSFileException  If an AFS specific error occurs, 
   *			if the file exists but is a directory
   *			rather than a regular file, does not exist but cannot
   *			be created, or cannot be opened for any other reason, including
   *			authorization.
   */
  public FileOutputStream(String name) throws AFSFileException
  {
    this(name, false);
  }
  /**
   * Creates an output file stream to write to the AFS file with the specified
   * <code>name</code>.  If the second argument is <code>true</code>, then
   * bytes will be written to the end of the file rather than the beginning.
   * <p>
   * If the file exists but is a directory rather than a regular file, does
   * not exist but cannot be created, or cannot be opened for any other
   * reason then a <code>AFSFileException</code> is thrown.
   * 
   * @param	name		the name of the file to write to
   * @param	append	if <code>true</code>, then bytes will be written
   *				to the end of the file rather than the beginning
   * @exception	AFSFileException  If an AFS specific error occurs, 
   *			if the file exists but is a directory
   *			rather than a regular file, does not exist but cannot
   *			be created, or cannot be opened for any other reason, including
   *			authorization.
   */
  public FileOutputStream(String name, boolean append) throws AFSFileException
  {
    if (append) {
      fileDescriptor = this.openAppend(name);
    } else {
      fileDescriptor = this.openWrite(name);
    }
  }
  /**
   * Creates a file output stream to write to the AFS file represented by 
   * the specified <code>File</code> object.
   * <p>
   * If the file exists but is a directory rather than a regular file, does
   * not exist but cannot be created, or cannot be opened for any other
   * reason then a <code>AFSFileException</code> is thrown.
   *
   * @param    file         the AFS file to be opened for writing.
   * @exception	AFSFileException  If an AFS specific error occurs, 
   *			if the file exists but is a directory
   *			rather than a regular file, does not exist but cannot
   *			be created, or cannot be opened for any other reason, including
   *			authorization.
   * @see    org.openafs.jafs.File#getPath()
   */
  public FileOutputStream(File file) throws AFSFileException
  {
    this(file.getPath(), false);
  }
  /**
   * Creates a file output stream to write to the AFS file represented by 
   * the specified <code>File</code> object.
   * <p>
   * If the file exists but is a directory rather than a regular file, does
   * not exist but cannot be created, or cannot be opened for any other
   * reason then a <code>AFSFileException</code> is thrown.
   *
   * @param file		the AFS file to be opened for writing.
   * @param	append	if <code>true</code>, then bytes will be written
   *				to the end of the file rather than the beginning
   * @exception	AFSFileException  If an AFS specific error occurs, 
   *			if the file exists but is a directory
   *			rather than a regular file, does not exist but cannot
   *			be created, or cannot be opened for any other reason, including
   *			authorization.
   * @see    org.openafs.jafs.File#getPath()
   */
  public FileOutputStream(File file, boolean append) throws AFSFileException
  {
    this(file.getPath(), append);
  }

  /*-------------------------------------------------------------------------*/

  /**
   * Writes the specified <code>byte</code> to this file output stream. 
   * <p>
   * Implements the abstract <tt>write</tt> method of <tt>OutputStream</tt>. 
   *
   * @param      b   the byte to be written.
   * @exception  AFSFileException  if an error occurs.
   */
  public void write(int b) throws AFSFileException 
  {
    byte[] bytes = new byte[1];
    bytes[0] = (byte) b;
    this.write(bytes, 0, 1);
  }
  /**
   * Writes <code>b.length</code> bytes from the specified byte array 
   * to this file output stream. 
   * <p>
   * Implements the <code>write</code> method of three arguments with the 
   * arguments <code>b</code>, <code>0</code>, and 
   * <code>b.length</code>. 
   * <p>
   * Note that this method does not call the one-argument 
   * <code>write</code> method of its underlying stream with the single 
   * argument <code>b</code>. 
   *
   * @param    b   the data to be written.
   * @exception  AFSFileException  if an error occurs.
   * @see    #write(byte[], int, int)
   * @see    java.io.FilterOutputStream#write(byte[], int, int)
   */
  public void write(byte[] b) throws AFSFileException 
  {
    this.write(b, 0, b.length);
  }

  /////////////// public native methods ////////////////////

  /**
   * Writes <code>len</code> bytes from the specified 
   * <code>byte</code> array starting at offset <code>off</code> to 
   * this file output stream. 
   *
   * @param b the data to be written
   * @param off the start offset in the data
   * @param len the number of bytes that are written
   * @exception  AFSFileException  if an I/O or other file related error occurs.
   * @see    java.io.FilterOutputStream#write(int)
   */
  public native void write(byte[] b, int off, int len) throws AFSFileException;
  /**
   * Closes this file output stream and releases any system resources 
   * associated with this stream. This file output stream may no longer 
   * be used for writing bytes. 
   *
   * @exception  AFSFileException  if an I/O or other file related error occurs.
   */
  public native void close()  throws AFSFileException;

  /////////////// private native methods ////////////////////

  /**
   * Opens an AFS file, with the specified name, for writing.
   *
   * @param		filename name of file to be opened
   * @return	file descriptor
   * @exception  AFSFileException  if an I/O or other file related error occurs.
   */
  private native int openWrite(String filename) throws AFSFileException;
  /**
   * Opens an AFS file, with the specified name, for appending.
   *
   * @param		filename name of file to be opened
   * @return	file descriptor
   * @exception  AFSFileException  if an I/O or other file related error occurs.
   */
  private native int openAppend(String filename) throws AFSFileException;

  /*-------------------------------------------------------------------------*/
}





