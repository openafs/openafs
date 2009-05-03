/*
 * @(#)File.java	1.3 10/12/2000
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

import java.io.IOException;
import java.util.ArrayList;

/*****************************************************************************/
/**
 * 
 * An abstract representation of AFS file and directory pathnames.
 *
 * This class is an extension of the standard Java File class with file-based 
 * manipulation methods overridden by integrated AFS native methods.
 *
 * <p> Extension methods include:
 *
 * <ol>
 * <li> <code>{@link #isMountPoint}</code>
 * <li> <code>{@link #isLink}</code>
 * <li> <code>{@link #isValidated}</code>
 * <li> <code>{@link #validate}</code>
 * <li> <code>{@link #refresh}</code>
 * <li> <code>{@link #getErrorCode}</code>
 * <li> <code>{@link #getErrorMessage}</code>
 * </ol>
 *
 * <p> For performance optimization, all newly constructed <code>File</code> 
 * objects are only validated once.  Furthermore, if an abstract pathname 
 * denotes a symbolic-link, then the {@link #isLink} attribute is set 
 * to true and the {@link #getTarget} field member is populated with 
 * this symbolic-link's target resource. (see {@link #getTarget})
 * 
 * <p> If you are interested in validating the target resource, simply 
 * call {@link #validate} before calling any of the attribute accessors. 
 * This action will <code>stat</code> the target resource, identifying 
 * its associated attributes and populating them in this objects field 
 * members.
 * 
 * <p> Following is an example of how to construct a new AFS File Object:
 * <p><blockquote><pre>
 *     try {
 *         File file = new File("/afs/mycell.com/proj/");
 *         if (file.isDirectory()) {
 *           System.out.println("This is a directory.");
 *         } else if (file.isLink()) {
 *           System.out.println("This is a symbolic-link.");
 *           System.out.println("  Its target is: " + file.getTarget());
 *           file.validate();
 *           if (file.isFile()) {
 *             System.out.println("  This object is now a file!");
 *           } else if (file.isDirectory()) {
 *             System.out.println("  This object is now a directory!");
 *           } else if (file.isMountPoint()) {
 *             System.out.println("  This object is now a volume mount point!");
 *           }
 *         } else if (file.isMountPoint()) {
 *           System.out.println("This is a volume mount point.");
 *         } else if (file.isFile()) {
 *           System.out.println("This is file.");
 *           System.out.println("  its size is: " + file.length());
 *         }
 *     } catch (AFSFileException ae) {
 *         System.out.println("AFS Exception: " + ae.getMessage());
 *         System.out.println("AFS Error Code: " + ae.getErrorCode());
 *     } catch (Exception e) {
 *         System.out.println("Exception: " + e.getMessage());
 *         e.printStackTrace();
 *     }
 * </pre></blockquote>
 *
 * @version 2.0, 04/16/2001 - Completely revised class for efficiency.
 * @version 1.3, 10/12/2000 - Introduced error code capture from native methods.
 * @version 1.2, 05/30/2000
 */
public class File extends java.io.File /* implements Comparable */
{
  private String path;
  private String type;
  private String target;

  /** Each member is mutually exclusive */
  private boolean isMountPoint = false;
  private boolean isDirectory = false;
  private boolean isFile = false;
  private boolean isLink = false;

  private boolean exists;
  private long lastModified;
  private long length;

  private ACL.Entry acl;
  private boolean validated = false;
  private long dirHandle;
  private int permissionsMask;
  private int errno;

  /**
   * Creates a new <code>File</code> instance by converting the given
   * pathname string into an abstract pathname and validating it against
   * the file system.  If the given string is an empty string, then the 
   * result is the empty abstract pathname; otherwise the abstract pathname 
   * is <code>validated</code> to represent a qualified file object.
   *
   * @param   pathname  A pathname string
   * @throws  NullPointerException
   *          If the <code>pathname</code> argument is <code>null</code>
   * @throws  AFSFileException
   *          If the user constructing this AFS file object is denied
   *          access to stat the file or simply a stat cannot be performed
   *          on the file. The reason code and message will be available
   *          from {@link org.openafs.jafs.AFSFileException#getErrorCode} and 
   *          {@link org.openafs.jafs.AFSFileException#getMessage} respectively.
   *          <p> This exception <U>will not</U> be thrown if the file does not
   *          exist.  Rather, the {@link #exists} attribute will be set to
   *          <code>false</code>.
   * @see     #validate()
   */
  public File(String pathname) throws AFSFileException
  {
	super(pathname);
	path = getAbsolutePath();
	validated = setAttributes();
	if (!validated) throw new AFSFileException(errno);
  }
  /**
   * Creates a new <code>File</code> instance by converting the given
   * pathname string into an abstract pathname.  If the given string is
   * an empty string, then the result is the empty abstract pathname.
   *
   * <p> The abstract pathname will remain <B>abstract</B> unless the
   * <code>validate</code> parameter is set to <code>true</code>. This
   * means that the abstract pathname will <U>not</U> be <code>validated</code>
   * and therefore the file object will not represent a qualified, attributed,
   * AFS file resource.  Rather, this constructor provides a method by which
   * you can construct a non-validated <code>File</code> object (one that
   * does not contain the file's complete status information).
   *
   * <p> This constructor is useful for creating file objects of file/path names
   * that you know exist, however are unauthorized to <code>validate</code> (or
   * <code>stat</code> - to obtain complete status information). For example,
   * if you are permitted to <code>lookup</code> (see: {@link #canLookup}) the
   * contents of a given directory yet <U>not</U> permitted to <code>read</code>
   * (see: {@link #canRead}), then this constructor would enable you to render the
   * contents of the directory without validating each entry.
   *
   * <p> <B>Please note:</B> this is the only constructor that does not throw an AFSFileException.
   *
   * @param   pathname  A pathname string
   * @param   validate  A boolean flag to indicate if this abstract path
   *				should be validated.
   * @throws  NullPointerException
   *          If the <code>pathname</code> argument is <code>null</code>
   * @see	  #File(String)
   * @see     #validate()
   */
  public File(String pathname, boolean validate)
  {
	super(pathname);
	path = getAbsolutePath();
	if (validate) validated = setAttributes();
  }
  /**
   * Creates a new <code>File</code> instance from a parent pathname string
   * and a child pathname string and validates it against the file system.
   *
   * <p> If <code>parent</code> is <code>null</code> then the new
   * <code>File</code> instance is created as if by invoking the
   * single-argument <code>File</code> constructor on the given
   * <code>filename</code> string (child pathname).
   *
   * <p> Otherwise the <code>parent</code> pathname string is taken to denote
   * a directory, and the <code>filename</code> string is taken to
   * denote either a directory or a file. The directory or file will then be
   * <code>validated</code> to represent a qualified file object.
   *
   * @param   parent   The parent pathname string
   * @param   filename This file's pathname string (child of specified parent)
   * @throws  NullPointerException
   *          If <code>child</code> is <code>null</code>
   * @throws  AFSFileException
   *          If the user constructing this AFS file object is denied
   *          access to stat the file or simply a stat cannot be performed
   *          on the file. The reason code and message will be available
   *          from {@link org.openafs.jafs.AFSFileException#getErrorCode} and 
   *          {@link org.openafs.jafs.AFSFileException#getMessage} respectively.
   *          <p> This exception <U>will not</U> be thrown if the file does not
   *          exist.  Rather, the {@link #exists} attribute will be set to
   *          <code>false</code>.
   * @see     #validate()
   */
  public File(String parent, String filename) throws AFSFileException
  {
	super(parent, filename);
	path = getAbsolutePath();
	validated = setAttributes();
	if (!validated) throw new AFSFileException(errno);
  }
  /**
   * Creates a new <code>File</code> instance from a parent pathname string
   * and a child pathname string.
   *
   * <p> If <code>parent</code> is <code>null</code> then the new
   * <code>File</code> instance is created as if by invoking the
   * single-argument <code>File</code> constructor on the given
   * <code>filename</code> string (child pathname).
   *
   * <p> Otherwise the <code>parent</code> pathname string is taken to denote
   * a directory, and the <code>filename</code> string is taken to
   * denote either a directory or a file.
   *
   * <p> The abstract pathname will remain <B>abstract</B> unless the
   * <code>validate</code> parameter is set to <code>true</code>. This
   * means that the abstract pathname will <U>not</U> be <code>validated</code>
   * and therefore the file object will not represent a qualified, attributed,
   * AFS file resource.  Rather, this constructor provides a method by which
   * you can construct a non-validated <code>File</code> object (one that
   * does not contain the file's complete status information).
   *
   * <p> This constructor is useful for creating file objects of file/path names
   * that you know exist, however are unauthorized to <code>validate</code> (or
   * <code>stat</code> - to obtain complete status information). For example,
   * if you are permitted to <code>lookup</code> (see: {@link #canLookup}) the
   * contents of a given directory yet <U>not</U> permitted to <code>read</code>
   * (see: {@link #canRead}), then this constructor would enable you to render the
   * contents of the directory without validating each entry.
   *
   * @param   parent	The parent pathname string
   * @param   filename	This file's pathname string (child of specified parent)
   * @param   validate  A boolean flag to indicate if this abstract path
   *				should be validated.
   * @throws  NullPointerException
   *          If <code>child</code> is <code>null</code>
   * @throws  AFSFileException
   *          If the user constructing this AFS file object is denied
   *          access to stat the file or simply a stat cannot be performed
   *          on the file. The reason code and message will be available
   *          from {@link org.openafs.jafs.AFSFileException#getErrorCode} and 
   *          {@link org.openafs.jafs.AFSFileException#getMessage} respectively.
   *          <p> This exception <U>will not</U> be thrown if the file does not
   *          exist.  Rather, the {@link #exists} attribute will be set to
   *          <code>false</code>.
   * @see	  #File(String, String)
   * @see     #validate()
   */
  public File(String parent, String filename, boolean validate) throws AFSFileException
  {
	super(parent, filename);
	path = getAbsolutePath();
	if (validate) {
	  validated = setAttributes();
	  if (!validated) throw new AFSFileException(errno);
	}
  }
  /**
   * Creates a new <code>File</code> instance from a parent abstract
   * pathname and a child pathname string and validates it against the file system.
   *
   * <p> If <code>parent</code> is <code>null</code> then the new
   * <code>File</code> instance is created as if by invoking the
   * single-argument <code>File</code> constructor on the given
   * <code>filename</code> string (child pathname).
   *
   * <p> Otherwise the <code>parent</code> abstract pathname is taken to
   * denote a directory, and the <code>filename</code> string is taken
   * to denote either a directory or a file.  The directory or file will then be
   * <code>validated</code> to represent a qualified file object.
   *
   * @param   parent	The parent abstract pathname
   * @param   filename	This file's pathname string (child of specified parent)
   * @param   validate  A boolean flag to indicate if this abstract path
   *				should be validated.
   * @throws  NullPointerException
   *          If <code>child</code> is <code>null</code>
   * @throws  AFSFileException
   *          If the user constructing this AFS file object is denied
   *          access to stat the file or simply a stat cannot be performed
   *          on the file. The reason code and message will be available
   *          from {@link org.openafs.jafs.AFSFileException#getErrorCode} and 
   *          {@link org.openafs.jafs.AFSFileException#getMessage} respectively.
   *          <p> This exception <U>will not</U> be thrown if the file does not
   *          exist.  Rather, the {@link #exists} attribute will be set to
   *          <code>false</code>.
   * @see     #validate()
   */
  public File(File parent, String filename) throws AFSFileException
  {
	super(parent, filename);
	path = getAbsolutePath();
	validated = setAttributes();
	if (!validated) throw new AFSFileException(errno);
  }
  /**
   * Creates a new <code>File</code> instance from a parent abstract
   * pathname and a child pathname string.
   *
   * <p> If <code>parent</code> is <code>null</code> then the new
   * <code>File</code> instance is created as if by invoking the
   * single-argument <code>File</code> constructor on the given
   * <code>filename</code> string (child pathname).
   *
   * <p> Otherwise the <code>parent</code> abstract pathname is taken to
   * denote a directory, and the <code>filename</code> string is taken
   * to denote either a directory or a file.
   *
   * <p> The abstract pathname will remain <B>abstract</B> unless the
   * <code>validate</code> parameter is set to <code>true</code>. This
   * means that the abstract pathname will <U>not</U> be <code>validated</code>
   * and therefore the file object will not represent a qualified, attributed,
   * AFS file resource.  Rather, this constructor provides a method by which
   * you can construct a non-validated <code>File</code> object (one that
   * does not contain the file's complete status information).
   *
   * <p> This constructor is useful for creating file objects of file/path names
   * that you know exist, however are unauthorized to <code>validate</code> (or
   * <code>stat</code> - to obtain complete status information). For example,
   * if you are permitted to <code>lookup</code> (see: {@link #canLookup}) the
   * contents of a given directory yet <U>not</U> permitted to <code>read</code>
   * (see: {@link #canRead}), then this constructor would enable you to render the
   * contents of the directory without validating each entry.
   *
   * @param   parent	The parent abstract pathname
   * @param   filename	This file's pathname string (child of specified parent)
   * @param   validate  A boolean flag to indicate if this abstract path
   *				should be validated.
   * @throws  NullPointerException
   *          If <code>child</code> is <code>null</code>
   * @throws  AFSFileException
   *          If the user constructing this AFS file object is denied
   *          access to stat the file or simply a stat cannot be performed
   *          on the file. The reason code and message will be available
   *          from {@link org.openafs.jafs.AFSFileException#getErrorCode} and 
   *          {@link org.openafs.jafs.AFSFileException#getMessage} respectively.
   *          <p> This exception <U>will not</U> be thrown if the file does not
   *          exist.  Rather, the {@link #exists} attribute will be set to
   *          <code>false</code>.
   * @see     #validate()
   * @see	  #File(File, String)
   */
  public File(File parent, String filename, boolean validate) throws AFSFileException
  {
	super(parent, filename);
	path = getAbsolutePath();
	if (validate) {
	  validated = setAttributes();
	  if (!validated) throw new AFSFileException(errno);
	}
  }

  /*****************************************************************************/

  /**
   * Validates this abstract pathname as an attributed AFS file object.  
   * This method will, if authorized, perform a <code>stat</code> on the
   * actual AFS file and update its respective field members; defining
   * this file object's attributes.
   *
   * @throws  AFSSecurityException
   *          If an AFS exception occurs while attempting to stat and set this
   *          AFS file object's attributes.
   */
  public void validate() throws AFSSecurityException
  {
	validated = setAttributes();
	if (!validated) throw new AFSSecurityException(errno);
  }
  /**
   * Tests whether the file denoted by this abstract pathname has
   * been validated.
   *
   * <P> Validation is always attempted upon construction of the file object,
   * therefore if this method returns false, then you are not permitted to 
   * <code>validate</code> this file and consequently all attribute accessors
   * will be invalid.
   *
   * <P> This method should return <code>true</code> even if this abstract 
   * pathname does not exist. If this is abstract pathname does not exist then
   * the <code>{@link #exists}</code> method should return false, however this
   * implies that the attribute accessors are valid and accurate; thus implying
   * successful validation.
   *
   * <P> This method is useful before calling any of the attribute accessors
   * to ensure a valid response. 
   *
   * @return <code>true</code> if and only if the file denoted by this
   *          abstract pathname has been validated during or after object construction;
   *          <code>false</code> otherwise
   */
  public boolean isValidated()
  {
	return validated;
  }
  /**
   * Refreshes this AFS file object by updating its attributes.
   * This method currently provides the same functionality as
   * {@link #validate}.
   *
   * @throws  AFSSecurityException
   *          If an AFS exception occurs while attempting to stat and update this
   *          AFS file object's attributes.
   * @see     #validate()
   */
  public void refresh() throws AFSSecurityException
  {
	validate();
  }
  /*-------------------------------------------------------------------------*/
  /**
   * Tests whether the file denoted by this abstract pathname is a
   * directory.
   *
   * @return <code>true</code> if and only if the file denoted by this
   *          abstract pathname exists <em>and</em> is a directory;
   *          <code>false</code> otherwise
   */
  public boolean isDirectory() 
  {
	return (isDirectory || isMountPoint) ? true : false;
  }
  /**
   * Tests whether the file denoted by this abstract pathname is a normal
   * file.  A file is <em>normal</em> if it is not a directory and, in
   * addition, satisfies other system-dependent criteria.  Any non-directory
   * file created by a Java application is guaranteed to be a normal file.
   *
   * @return  <code>true</code> if and only if the file denoted by this
   *          abstract pathname exists <em>and</em> is a normal file;
   *          <code>false</code> otherwise
   */
  public boolean isFile() 
  {
	return isFile;
  }
  /**
   * Tests whether the file denoted by this abstract pathname is an
   * AFS Volume Mount Point.
   *
   * @return <code>true</code> if and only if the file denoted by this
   *          abstract pathname exists <em>and</em> is a mount point;
   *          <code>false</code> otherwise
   */
  public boolean isMountPoint() 
  {
	return isMountPoint;
  }
  /**
   * Tests whether the file denoted by this abstract pathname is a
   * symbolic-link.
   *
   * @return <code>true</code> if and only if the file denoted by this
   *          abstract pathname exists <em>and</em> is a symbolic-link;
   *          <code>false</code> otherwise
   */
  public boolean isLink()
  {
	return isLink;
  }
  /*-------------------------------------------------------------------------*/
  /**
   * Tests whether the file denoted by this abstract pathname exists.
   *
   * @return  <code>true</code> if and only if the file denoted by this
   *          abstract pathname exists; <code>false</code> otherwise
   */
  public boolean exists() 
  {
	return exists;
  }
  /**
   * Returns the time that the file denoted by this abstract pathname was
   * last modified.
   *
   * @return  A <code>long</code> value representing the time the file was
   *          last modified, measured in milliseconds since the epoch
   *          (00:00:00 GMT, January 1, 1970), or <code>0L</code> if the
   *          file does not exist or if an I/O error occurs
   */
  public long lastModified() 
  {
	return lastModified;
  }
  /**
   * Returns the length of the file denoted by this abstract pathname.
   *
   * @return  The length, in bytes, of the file denoted by this abstract
   *          pathname, or <code>0L</code> if the file does not exist
   */
  public long length() 
  {
	return length;
  }
  /**
   * Returns an abstract pathname string that represents the target resource of
   * of this file, if it is a symbolic-link.
   *
   * <p> If this abstract pathname <B>does not</B> denote a symbolic-link, then this
   * method returns <code>null</code>.  Otherwise a string is
   * returned that represents the target resource of this symbolic-link.
   *
   * @return  A string representation of this symbolic-link's target resource.
   * @see     #isLink()
   */
  public String getTarget()
  {
	return target;
  }
  /**
   * Returns an array of strings naming the files and directories in the
   * directory denoted by this abstract pathname.
   *
   * <p> If this abstract pathname does not denote a directory, then this
   * method returns <code>null</code>.  Otherwise an array of strings is
   * returned, one for each file or directory in the directory.  Names
   * denoting the directory itself and the directory's parent directory are
   * not included in the result.  Each string is a file name rather than a
   * complete path.
   *
   * <p> There is no guarantee that the name strings in the resulting array
   * will appear in any specific order; they are not, in particular,
   * guaranteed to appear in alphabetical order.
   *
   * @return  An array of strings naming the files and directories in the
   *          directory denoted by this abstract pathname.  The array will be
   *          empty if the directory is empty.  Returns <code>null</code> if
   *          this abstract pathname does not denote a directory, or if an
   *          I/O error occurs.
   */
  public String[] list()
  {
	try {
	  if (isFile()) {
	    errno = ErrorTable.NOT_DIRECTORY;
	    return null;
	  }
	  ArrayList buffer = new ArrayList();
	  dirHandle = listNative(buffer);
	  if (dirHandle == 0) {
	    return null;
	  } else {
	    return (String[])buffer.toArray(new String[0]);
	  }
	} catch (Exception e) {
	  System.out.println(e);
	  return null;
	}
  }
  /**
   * Returns an ArrayList object containing strings naming the files and 
   * directories in the directory denoted by this abstract pathname.
   *
   * <p> If this abstract pathname does not denote a directory, then this
   * method returns <code>null</code>.  Otherwise an array of strings is
   * returned, one for each file or directory in the directory.  Names
   * denoting the directory itself and the directory's parent directory are
   * not included in the result.  Each string is a file name rather than a
   * complete path.
   *
   * <p> There is no guarantee that the name strings in the resulting array
   * will appear in any specific order; they are not, in particular,
   * guaranteed to appear in alphabetical order.
   *
   * @return  An array of strings naming the files and directories in the
   *          directory denoted by this abstract pathname.  The array will be
   *          empty if the directory is empty.  Returns <code>null</code> if
   *          this abstract pathname does not denote a directory, or if an
   *          I/O error occurs.
   * @throws  AFSSecurityException
   *          If you are not authorized to list the contents of this directory
   * @throws  AFSFileException
   *          If this file object is not a <code>mount point</code>, <code>link
   *          </code>, or <code>directory</code> <B>or</B> an unexpected AFS 
   *          error occurs.
   * @see     #list()
   */
  public ArrayList listArray() throws AFSFileException
  {
	try {
	  if (isFile()) throw new AFSFileException(ErrorTable.NOT_DIRECTORY);
	  ArrayList buffer = new ArrayList();
	  dirHandle = listNative(buffer);
	  if (dirHandle == 0) {
	    if (errno == ErrorTable.PERMISSION_DENIED) {
		throw new AFSSecurityException(errno);
	    } else {
		throw new AFSFileException(errno);
	    }
	  } else {
	    return buffer;
	  }
	} catch (Exception e) {
	  System.out.println(e);
	  throw new AFSFileException(errno);
	}
  }
  /*-------------------------------------------------------------------------*/
  /**
   * Deletes the file or directory denoted by this abstract pathname.  If
   * this pathname denotes a directory, then the directory must be empty in
   * order to be deleted.
   *
   * @return  <code>true</code> if and only if the file or directory is
   *          successfully deleted; <code>false</code> otherwise
   */
  public boolean delete()
  {
	try {
	  if(this.isDirectory()) {
	    return this.rmdir();
	  } else if(this.isFile() || this.isLink()) {
	    return this.rmfile();
	  }
	  return false;
	} catch (Exception e) {
	  System.out.println(e);
	  return false;
	}
  }
  /**
   * Copies the file denoted by this abstract pathname to the destination
   * file provided.  Then checks the newly copied file's size to
   * test for file size consistency.
   *
   * @param  dest  The new abstract pathname for the named file
   * 
   * @return  <code>true</code> if and only if the file that was copied
   *          reports the same file size (length) as that of this file;
   *          <code>false</code> otherwise
   *
   * @throws  AFSFileException
   *          If an I/O or AFS exception is encountered while copying the file.
   */
  public boolean copyTo(File dest) throws AFSFileException
  {
    FileInputStream fis  = new FileInputStream(this);
    FileOutputStream fos = new FileOutputStream(dest);
    byte[] buf = new byte[1024];
    int i = 0;
    while((i=fis.read(buf))!=-1) {
      fos.write(buf, 0, i);
    }
    fis.close();
    fos.close();
    dest.validate();
    return (dest.length() == this.length());
  }
  /*-------------------------------------------------------------------------*/
  /**
   * Returns the permissions mask of the ACL for this object relative to the user accessing it.
   *
   * @return  the permissions mask of this object based upon the current user.
   * @see     org.openafs.jafs.ACL.Entry#getPermissionsMask()
   */
  public int getPermissionsMask() 
  {
	return getRights();
  }
  /**
   * Tests whether the user can administer the ACL (see: {@link org.openafs.jafs.ACL}
   * of the directory denoted by this abstract pathname.
   *
   * @see org.openafs.jafs.ACL.Entry#canAdmin
   * @return  <code>true</code> if and only if the directory specified by this
   *          abstract pathname exists <em>and</em> can be administered by the
   *          current user; <code>false</code> otherwise
   */
  public boolean canAdmin()
  {
	if (acl == null) acl = new ACL.Entry(getRights());
	return acl.canAdmin();
  }
  /**
   * Tests whether the current user can delete the files or subdirectories of
   * the directory denoted by this abstract pathname.
   *
   * @see org.openafs.jafs.ACL.Entry#canDelete
   * @return  <code>true</code> if and only if the directory specified by this
   *          abstract pathname exists <em>and</em> permits deletion of its files
   *          and subdirectories by the current user; <code>false</code> otherwise
   */
  public boolean canDelete()
  {
	if (acl == null) acl = new ACL.Entry(getRights());
	return acl.canDelete();
  }
  /**
   * Tests whether the current user can insert a file into the directory
   * denoted by this abstract pathname.
   *
   * @see org.openafs.jafs.ACL.Entry#canInsert
   * @return  <code>true</code> if and only if the directory specified by this
   *          abstract pathname exists <em>and</em> a file can be inserted by the
   *          current user; <code>false</code> otherwise
   */
  public boolean canInsert()
  {
	if (acl == null) acl = new ACL.Entry(getRights());
	return acl.canInsert();
  }
  /**
   * Tests whether the current user can lock the file denoted by this 
   * abstract pathname.
   *
   * @see org.openafs.jafs.ACL.Entry#canLock
   * @return  <code>true</code> if and only if the file specified by this
   *          abstract pathname exists <em>and</em> can be locked by the
   *          current user; <code>false</code> otherwise
   */
  public boolean canLock()
  {
	if (acl == null) acl = new ACL.Entry(getRights());
	return acl.canLock();
  }
  /**
   * Tests whether the current user can lookup the contents of the directory
   * denoted by this abstract pathname.
   *
   * @see org.openafs.jafs.ACL.Entry#canLookup
   * @return  <code>true</code> if and only if the directory specified by this
   *          abstract pathname exists <em>and</em> its contents can be listed by the
   *          current user; <code>false</code> otherwise
   */
  public boolean canLookup()
  {
	if (acl == null) acl = new ACL.Entry(getRights());
	return acl.canLookup();
  }
  /**
   * Tests whether the current user can read the file denoted by this
   * abstract pathname.
   *
   * @see org.openafs.jafs.ACL.Entry#canRead
   * @return  <code>true</code> if and only if the file specified by this
   *          abstract pathname exists <em>and</em> can be read by the
   *          current user; <code>false</code> otherwise
   */
  public boolean canRead()
  {
	if (acl == null) acl = new ACL.Entry(getRights());
	return acl.canRead();
  }
  /**
   * Tests whether the current user can modify to the file denoted by this
   * abstract pathname.
   *
   * @see org.openafs.jafs.ACL.Entry#canWrite
   * @return  <code>true</code> if and only if the file system actually
   *          contains a file denoted by this abstract pathname <em>and</em>
   *          the current user is allowed to write to the file;
   *          <code>false</code> otherwise.
   */
  public boolean canWrite()
  {
	if (acl == null) acl = new ACL.Entry(getRights());
	return acl.canWrite();
  }
  /*-------------------------------------------------------------------------*/
  /**
   * Closes the directory denoted by this abstract pathname.
   *
   * @return  <code>true</code> if and only if the directory is
   *          successfully closed; <code>false</code> otherwise
   */
  public boolean close()
  {
	if (dirHandle == 0) {
	  return false;
	}
	return closeDir(dirHandle);
  }
  /*-------------------------------------------------------------------------*/
  /**
   * Returns the AFS specific error number (code).  This code can be interpreted 
   * by use of <code>{@link org.openafs.jafs.ErrorTable}</code> static class method 
   * <code>{@link org.openafs.jafs.ErrorTable#getMessage}</code>
   *
   * @return  the AFS error code (number) associated with the last action performed
   *          on this object.
   * @see     org.openafs.jafs.ErrorTable#getMessage(int)
   */
  public int getErrorCode() 
  {
	return errno;
  }
  /**
   * Returns the AFS error message string defined by the <code>{@link org.openafs.jafs.ErrorTable}</code>
   * class.
   *
   * @return  the AFS error message string associated with the last action performed
   *          on this object.
   * @see     org.openafs.jafs.ErrorTable#getMessage(int)
   */
  public String getErrorMessage() 
  {
	return ErrorTable.getMessage(errno);
  }

  /////////////// custom override methods ////////////////////
//X
//X  /**
//X   * Compares two File objects relative to their filenames and <B>does not</B>
//X   * compare their respective absolute paths.  Alphabetic case is significant in 
//X   * comparing filenames.
//X   *
//X   * @param   file  The File object to be compared to this file's filename
//X   * 
//X   * @return  Zero if the argument is equal to this file's filename, a
//X   *		value less than zero if this file's filename is
//X   *		lexicographically less than the argument, or a value greater
//X   *		than zero if this file's filename is lexicographically
//X   *		greater than the argument
//X   *
//X   * @since   JDK1.2
//X   */
//X  public int compareTo(File file) {
//X    return this.getName().compareTo(file.getName());
//X  }
//X  /**
//X   * Compares this file to another File object.  If the other object
//X   * is an abstract pathname, then this function behaves like <code>{@link
//X   * #compareTo(File)}</code>.  Otherwise, it throws a
//X   * <code>ClassCastException</code>, since File objects can only be
//X   * compared to File objects.
//X   *
//X   * @param   o  The <code>Object</code> to be compared to this abstract pathname
//X   *
//X   * @return  If the argument is an File object, returns zero
//X   *          if the argument is equal to this file's filename, a value
//X   *          less than zero if this file's filename is lexicographically
//X   *          less than the argument, or a value greater than zero if this
//X   *          file's filename is lexicographically greater than the
//X   *          argument
//X   *
//X   * @throws  <code>ClassCastException</code> if the argument is not an
//X   *		  File object
//X   *
//X   * @see     java.lang.Comparable
//X   * @since   JDK1.2
//X   */
//X  public int compareTo(Object o) throws ClassCastException 
//X  { 
//X    File file = (File)o;
//X    return compareTo(file);
//X  } 

  /////////////// public native methods ////////////////////

  /**
   * Creates the directory named by this abstract pathname.
   *
   * @return  <code>true</code> if and only if the directory was
   *          created; <code>false</code> otherwise
   */
  public native boolean mkdir();
  /**
   * Renames the file denoted by this abstract pathname.
   *
   * @param  dest  The new abstract pathname for the named file
   * 
   * @return  <code>true</code> if and only if the renaming succeeded;
   *          <code>false</code> otherwise
   *
   * @throws  NullPointerException  
   *          If parameter <code>dest</code> is <code>null</code>
   */
  public native boolean renameTo(File dest);
  /**
   * Performs a file <code>stat</code> on the actual AFS file and populates  
   * this object's respective field members with the appropriate values.
   * method will, if authorized, perform a <code>stat</code> on the
   * actual AFS file and update its respective field members; defining
   * this file object's attributes.
   *
   * <P><B>This method should not be used directly for refreshing or validating
   * this AFS file object.  Please use {@link #validate} instead.</B>
   *
   * @return  <code>true</code> if and only if the current user is allowed to stat the file;
   *          <code>false</code> otherwise.
   * @see     #validate()
   */
  public native boolean setAttributes() throws AFSSecurityException;

  /////////////// private native methods ////////////////////

  /**
   * List the contents of this directory.
   *
   * @return  the directory handle
   */
  private native long listNative(ArrayList buffer) throws AFSSecurityException;
  /**
   * Close the currently open directory using a previously obtained handle.
   *
   * @return  true if the directory closes without error
   */
  private native boolean closeDir(long dp) throws AFSSecurityException;
  /**
   * Removes/deletes the current directory.
   *
   * @return  true if the directory is removed without error
   */
  private native boolean rmdir() throws AFSSecurityException;
  /**
   * Removes/deletes the current file.
   *
   * @return  true if the file is removed without error
   */
  private native boolean rmfile() throws AFSSecurityException;
  /**
   * Returns the permission/ACL mask for this directory
   *
   * @return  permission/ACL mask
   */
  private native int getRights() throws AFSSecurityException;
  /*-------------------------------------------------------------------------*/
}






