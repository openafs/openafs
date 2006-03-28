/*
 * @(#)ACL.java	2.0 04/18/2001
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

import java.io.Serializable;
import java.util.ArrayList;
import java.util.StringTokenizer;

/**
 * An abstract representation of AFS file and directory pathnames.
 *
 * This class is an extension of the standard Java File class with file-based 
 * manipulation methods overridden by integrated AFS native methods.
 *
 * @version 2.0, 04/18/2001 - Completely revised class for efficiency.
 */

public class ACL implements Serializable, Comparable
{
   protected ACL.Entry[] positiveEntries;
   protected ACL.Entry[] negativeEntries;
   protected String path;
 
  public ACL(String path) throws AFSException
  {
  	this(path,true);
 }
  
public ACL(String path, boolean init) throws AFSException {
	int numberPositiveEntries = 0;
	int numberNegativeEntries = 0;
	ACL.Entry aclEntry;
	String buffer;
	this.path = path;

	if (init) {

		StringTokenizer st = new StringTokenizer(getACLString(path), "\n\t");

		buffer = st.nextToken();
		numberPositiveEntries = new Integer(buffer).intValue();
		positiveEntries = new ACL.Entry[numberPositiveEntries];

		buffer = st.nextToken();
		numberNegativeEntries = new Integer(buffer).intValue();
		negativeEntries = new ACL.Entry[numberNegativeEntries];

		for (int i = 0; i < numberPositiveEntries; i++) {
			aclEntry = new ACL.Entry();
			aclEntry.setUser(st.nextToken());
			aclEntry.setPermissions(new Integer(st.nextToken()).intValue());
			positiveEntries[i] = aclEntry;
		}

		for (int i = 0; i < numberNegativeEntries; i++) {
			aclEntry = new ACL.Entry();
			aclEntry.setUser(st.nextToken());
			aclEntry.setPermissions(new Integer(st.nextToken()).intValue());
			negativeEntries[i] = aclEntry;
		}
	}else{
		positiveEntries = new ACL.Entry[numberPositiveEntries];
		negativeEntries = new ACL.Entry[numberNegativeEntries];
	}
}
  public int getEntryCount()
  {
    return positiveEntries.length + positiveEntries.length;
  }
  public String getPath()
  {
    return path;
  }
  public void setPath(String path)
  {
	this.path= path;
  }
  public ACL.Entry[] getPositiveEntries()
  {
    return positiveEntries;
  }
  public void addPositiveEntry(ACL.Entry entry) throws AFSException
  {
    int n = positiveEntries.length;
    ACL.Entry[] e = new ACL.Entry[n + 1];
    System.arraycopy(positiveEntries, 0, e, 0, n);
    e[n] = entry;
    positiveEntries = e;
    setACLString(path, getFormattedString());
  }
  public void setPositiveEntries(ACL.Entry[] entries) throws AFSException
  {
    this.positiveEntries = entries;
    setACLString(path, getFormattedString());
  }
  public ACL.Entry[] getNegativeEntries()
  {
    return negativeEntries;
  }
  public void addNegativeEntry(ACL.Entry entry) throws AFSException
  {
    int n = negativeEntries.length;
    ACL.Entry[] e = new ACL.Entry[n + 1];
    System.arraycopy(negativeEntries, 0, e, 0, n);
    e[n] = entry;
    negativeEntries = e;
    setACLString(path, getFormattedString());
  }
  
  public void addAllNegativeEntrys(ACL.Entry[] entry) throws AFSException
  {
	int n = negativeEntries.length;
	ACL.Entry[] e = new ACL.Entry[n + entry.length];
	System.arraycopy(negativeEntries, 0, e, 0, n);
	System.arraycopy(entry,0,e,n,entry.length);
	
	negativeEntries = e;
	setACLString(path, getFormattedString());
 
  }
  
  public void addAllPositiveEntrys(ACL.Entry[] entry) throws AFSException
  {
	int n = positiveEntries.length;
	ACL.Entry[] e = new ACL.Entry[n + entry.length];
	System.arraycopy(positiveEntries, 0, e, 0, n);
	System.arraycopy(entry,0,e,n,entry.length);
	
	positiveEntries = e;
	setACLString(path, getFormattedString());
  
  }
  public void setNegativeEntries(ACL.Entry[] entries) throws AFSException
  {
    this.negativeEntries = entries;
    setACLString(path, getFormattedString());
  }

 /**
 * Needs a AFS Connection
 */
  public void flush() throws AFSException
  {
    setACLString(path, getFormattedString());
  }
  
  /**
   * removes a ACL Entry from acl
   */
  public void removeNegativeEntry(Entry entry) throws AFSException {
			
	  ArrayList arr = new ArrayList();
		
	  for (int i = 0; i < negativeEntries.length; i++) {
					
		  if(!negativeEntries[i].equals(entry)){
			  arr.add(negativeEntries[i]);
		  }
	  }
	  negativeEntries = (ACL.Entry[]) arr.toArray(new ACL.Entry[arr.size()]);
	
	setACLString(path, getFormattedString());
  }
  
  /**
   * removes all ACL Entrys from acl
   */
  public void removeAllNegativeEntrys() throws AFSException {
	negativeEntries = new Entry[0];
	setACLString(path, getFormattedString());
  }
  
  /**
   * removes all ACL Entrys from acl
   */
  public void removeAllPositiveEntrys() throws AFSException {
	positiveEntries = new Entry[0];
	setACLString(path, getFormattedString());
  }
  
  
  public boolean containsNegative(Entry entry) {
  	
	for (int i = 0; i < negativeEntries.length; i++) {
					
			 if(negativeEntries[i].equals(entry)){
				 return true;
			 }
		 }
	return false;
  }
  
  
	public Entry getNegative(String entryname) {

	  for (int i = 0; i < negativeEntries.length; i++) {

			   if(negativeEntries[i].getUser().equalsIgnoreCase(entryname)){
				   return negativeEntries[i];
			   }
		   }
	  return null;
	}
	
	public Entry getPositive(String entryname) {

	  for (int i = 0; i < positiveEntries.length; i++) {

			   if(positiveEntries[i].getUser().equalsIgnoreCase(entryname)){
				   return positiveEntries[i];
			   }
		   }
	  return null;
	}
  public boolean containsPositive(Entry entry) {
  	
	for (int i = 0; i < positiveEntries.length; i++) {
					
			 if(positiveEntries[i].equals(entry)){
				 return true;
			 }
		 }
	return false;
  }
  
  /**
   * removes a ACL Entry from acl
   */
  public void removePositiveEntry(Entry entry) throws AFSException {
			
	  ArrayList arr = new ArrayList();
		
	  for (int i = 0; i < positiveEntries.length; i++) {
					
		  if(!positiveEntries[i].equals(entry)){
			  arr.add(positiveEntries[i]);
		  }
	  }
	  positiveEntries = (ACL.Entry[]) arr.toArray(new ACL.Entry[arr.size()]);
	setACLString(path, getFormattedString());
  }

  private ACL.Entry[] getNonEmptyEntries(ACL.Entry[] entries)
  {
    ArrayList response = new ArrayList(entries.length);
    for (int i = 0; i < entries.length; i++)
    {
      boolean isNonEmpty = entries[i].canRead()   ||
                           entries[i].canLookup() ||
                           entries[i].canWrite()  ||
                           entries[i].canInsert() ||
                           entries[i].canDelete() ||
                           entries[i].canLock()   ||
                           entries[i].canAdmin();
      if (isNonEmpty) response.add(entries[i]);
    }
    if (response.size() == entries.length) return entries;
    return (ACL.Entry[])response.toArray(new ACL.Entry[response.size()]);
  }

  private void entriesToString(ACL.Entry[] entries, StringBuffer response)
  {
    for (int i = 0; i < entries.length; i++)
    {
      this.entryToString((ACL.Entry)entries[i], response);
    }
  }

  private void entryToString(ACL.Entry entry, StringBuffer response)
  {
    response.append(entry.getUser() + '\t' + entry.getPermissionsMask() + '\n');
  }

  /**
   * Returns a ViceIoctl formatted String representation of this 
   * <CODE>ACL</CODE>.
   *
   * @return a ViceIoctl formatted String representation of this 
   *         <CODE>ACL</CODE>.
   */
  private String getFormattedString()
  {
    StringBuffer out = null;
    ACL.Entry[] nonEmptyPos = this.getNonEmptyEntries(this.getPositiveEntries());
    ACL.Entry[] nonEmptyNeg = this.getNonEmptyEntries(this.getNegativeEntries());

    out = new StringBuffer(nonEmptyPos.length + "\n" + nonEmptyNeg.length + "\n");
    this.entriesToString(nonEmptyPos, out);
    this.entriesToString(nonEmptyNeg, out);

    return out.toString();
  }

  /////////////// custom override methods ////////////////////

  /**
   * Compares two ACL objects respective to their paths and does not
   * factor any other attribute.    Alphabetic case is significant in 
   * comparing names.
   *
   * @param     acl    The ACL object to be compared to this ACL
   *                   instance
   * 
   * @return    Zero if the argument is equal to this ACL's path, a
   *		value less than zero if this ACL's path is
   *		lexicographically less than the argument, or a value greater
   *		than zero if this ACL's path is lexicographically
   *		greater than the argument
   */
  public int compareTo(ACL acl)
  {
    return this.getPath().compareTo(acl.getPath());
  }

  /**
   * Comparable interface method.
   *
   * @see #compareTo(ACL)
   */
  public int compareTo(Object obj)
  {
    return compareTo((ACL)obj);
  }

  /**
   * Tests whether two <code>ACL</code> objects are equal, based on their 
   * paths and permission bits.
   *
   * @param acl the ACL to test
   * @return whether the specifed ACL is the same as this ACL
   */
  public boolean equals( ACL acl )
  {
    return ( (this.getPath().equals(acl.getPath())) &&
             (positiveEntries.equals(acl.getPositiveEntries())) &&
             (negativeEntries.equals(acl.getNegativeEntries())) );
  }

  /**
   * Returns a String representation of this <CODE>ACL</CODE>
   *
   * @return a String representation of this <CODE>ACL</CODE>
   */
  public String toString()
  {
    ACL.Entry[] nonEmptyPos = this.getNonEmptyEntries(this.getPositiveEntries());
    ACL.Entry[] nonEmptyNeg = this.getNonEmptyEntries(this.getNegativeEntries());

    StringBuffer out = new StringBuffer("ACL for ");
    out.append(path);
    out.append("\n");
    out.append("Positive Entries:\n");
    for (int i = 0; i < nonEmptyPos.length; i++) {
      out.append("  ");
      out.append(nonEmptyPos[i].toString());
    }
    if (nonEmptyNeg.length > 0) {
      out.append("Negative Entries:\n");
      for (int i = 0; i < nonEmptyNeg.length; i++) {
        out.append("  ");
        out.append(nonEmptyNeg[i].toString());
      }
    }

    return out.toString();
  }

  /////////////// native methods ////////////////////

  /**
   * Returns a formatted String representing the ACL for the specified path.
   *
   * The string format is in the form of a ViceIoctl and is as follows:
   * printf("%d\n%d\n", positiveEntriesCount, negativeEntriesCount);
   * printf("%s\t%d\n", userOrGroupName, rightsMask);
   *
   * @param   path     the directory path
   * @returns a formatted String representing the ACL for the specified path.
   * @throws an AFSException if an AFS or JNI exception is encountered.
   */
  private native String getACLString(String path) throws AFSException;

  /**
   * Sets the ACL in the file system according to this abstract representation.
   *
   * @param path       the directory path
   * @param aclString  string representation of ACL to be set
   * @throws an AFSException if an AFS or JNI exception is encountered.
   */
  private native void setACLString(String path, String aclString) throws AFSException;

  /*====================================================================*/
  /* INNER CLASSES  */
  /*====================================================================*/

  /**
   * AFS ACL Entry Class.
   *
   * <p> Documentation reference: 
   * <A HREF="http://www.transarc.com/Library/documentation/afs/3.6/unix/en_US/HTML/AdminGd/auagd020.htm#HDRWQ772">Managing Access Control Lists</A>
   *
   * @version 2.0, 04/18/2001 - Completely revised class for efficiency.
   * @version 3.0, 05/01/2002 - Converted class to an inner class.
   */
  public static final class Entry implements Serializable
  {
    /** ACL Mask read constant */
    public static final int READ   = 1;
    /** ACL Mask write constant */
    public static final int WRITE  = 2;
    /** ACL Mask insert constant */
    public static final int INSERT = 4;
    /** ACL Mask lookup constant */
    public static final int LOOKUP = 8;
    /** ACL Mask delete constant */
    public static final int DELETE = 16;
    /** ACL Mask lock constant */
    public static final int LOCK   = 32;
    /** ACL Mask administer constant */
    public static final int ADMIN  = 64;
  
    private String username;
  
    private boolean r = false;
    private boolean l = false;
    private boolean i = false;
    private boolean d = false;
    private boolean w = false;
    private boolean k = false;
    private boolean a = false;
  
    /** 
     * Constructs a new ACL entry with all permission bits set to <code>false</code>.
     */
    public Entry()
    {
    }
    /** 
     * Constructs a new ACL entry with all permission bits set to <code>false</code>
     * and sets the associated user or group name.
     *
     * @param		user	The user or group name associated with this entry
     */
    public Entry(String user)
    {
      this.setUser(user);
    }
    /** 
     * Constructs a new ACL entry setting each permission bit to its appropriate 
     * value according to the <code>permissionsMask</code> specified.
     *
     * @see #canRead
     * @see #canWrite
     * @see #canInsert
     * @see #canLookup
     * @see #canDelete
     * @see #canLock
     * @see #canAdmin
     * @param		permissionsMask	An integer representation of the permissoin 
     *                                rights of this entry
     */
    public Entry(int permissionsMask)
    {
      this.setPermissions(permissionsMask);
    }
    /** 
     * Constructs a new ACL entry setting each permission bit to its appropriate 
     * value according to the <code>permissionsMask</code> specified
     * and sets the associated user or group name.
     *
     * @see #canRead
     * @see #canWrite
     * @see #canInsert
     * @see #canLookup
     * @see #canDelete
     * @see #canLock
     * @see #canAdmin
     * @see #setUser
     * @param		permissionsMask	An integer representation of the permissoin 
     *                                rights of this entry
     * @param		user			The username or group associated with this entry
     */
    public Entry(String user, int permissionsMask)
    {
      this.setUser(user);
      this.setPermissions(permissionsMask);
    }
    /*-------------------------------------------------------------------------*/
    /** 
     * Set this entry's permission bits according to the value of the 
     * <code>permissionsMask</code> specified.
     *
     * @see	#getPermissionsMask
     * @param		permissionsMask	An integer representation of the permissoin 
     *                                rights of this entry
     */
    public void setPermissions(int permissionsMask)
    {
      if ((permissionsMask & READ) != 0) {
          this.setRead(true);
      }
      if ((permissionsMask & LOOKUP) != 0) {
          this.setLookup(true);
      }
      if ((permissionsMask & INSERT) != 0) {
          this.setInsert(true);
      }
      if ((permissionsMask & DELETE) != 0) {
          this.setDelete(true);
      }
      if ((permissionsMask & WRITE) != 0) {
          this.setWrite(true);
      }
      if ((permissionsMask & LOCK) != 0) {
          this.setLock(true);
      }
      if ((permissionsMask & ADMIN) != 0) {
          this.setAdmin(true);
      }
    }
    /** 
     * Returns this entry's permission mask.
     *
     * <p> <B>Permission Mask</B><BR>
     * 01 - READ  <BR>
     * 02 - WRITE <BR>
     * 04 - INSERT<BR>
     * 08 - LOOKUP<BR>
     * 16 - DELETE<BR>
     * 32 - LOCK  <BR>
     * 64 - ADMIN <BR>
     *
     * <p> Any combination of the above mask values would equate to a valid combination of 
     * permission settings.  For example, if the permission mask was <B>11</B>, the ACL permissions
     * would be as follows: <code>read</code> (1), <code>write</code> (2), and <code>lookup</code> (8).<BR>
     * [1 + 2 + 8 = 11]
     *
     * @return	An integer representation (mask) of the permissoin rights of this entry
     */
    public int getPermissionsMask()
    {
      int permissionsMask = 0;
      if (canRead())   permissionsMask |= READ;
      if (canWrite())  permissionsMask |= WRITE;
      if (canInsert()) permissionsMask |= INSERT;
      if (canLookup()) permissionsMask |= LOOKUP;
      if (canDelete()) permissionsMask |= DELETE;
      if (canLock())   permissionsMask |= LOCK;
      if (canAdmin())  permissionsMask |= ADMIN;
      return permissionsMask;
    }
    /** 
     * Returns the user <B>or</B> group name associated with this ACL entry.
     *
     * @return	String representation of the user or group name associated with this entry.
     */
    public String getUser()
    {
      return username;
    }
    /** 
     * Sets the user <B>or</B> group name associated with this ACL entry.
     *
     * @param	user	representation of the user or group name associated with this entry.
     */
    public void setUser(String user)
    {
      username = user;
    }
    /** 
     * <IMG SRC="file.gif" ALT="File Permission" WIDTH="15" HEIGHT="15" BORDER="0"> Tests whether the ACL permits <code>read</code> access.
     *
     * <p> This permission enables a user to read the contents of files in the directory 
     * and to obtain complete status information for the files (read/retrieve the file 
     * attributes).
     *
     * <p><FONT COLOR="666699"><IMG SRC="file.gif" ALT="File Permission" WIDTH="15" HEIGHT="15" BORDER="0"> <U><B>File Permission</B></U></FONT><BR>
     * This permission is meaningful with respect to files in 
     * a directory, rather than the directory itself or its subdirectories. 
     *
     * <p> Documentation reference: 
     * <A HREF="http://www.transarc.com/Library/documentation/afs/3.6/unix/en_US/HTML/AdminGd/auagd020.htm#HDRWQ782">The AFS ACL Permissions</A>
     *
     * @return  <code>true</code> if and only if the ACL permits <code>read</code> access of
     *          files; <code>false</code> otherwise
     */
    public boolean canRead()
    {
      return r;
    }
    /** 
     * Sets the ACL permission to accomodate <code>read</code> access for files.
     *
     * @see #canRead
     * @param	flag	boolean flag that denotes the permission bit for <code>read</code> access.
     */
    public void setRead(boolean flag)
    {
      r = flag;
    }
    /** 
     * <IMG SRC="folder.gif" ALT="Directory Permission" WIDTH="15" HEIGHT="15" BORDER="0"> Tests whether the ACL permits lookup access.
     *
     * <p> This permission functions as something of a gate keeper for access to the directory 
     * and its files, because a user must have it in order to exercise any other permissions. 
     * In particular, a user must have this permission to access anything in the directory's 
     * subdirectories, even if the ACL on a subdirectory grants extensive permissions.
     *
     * <p> This permission enables a user to list the names of the files and subdirectories in 
     * the directory (this does not permit read access to its respective entries), obtain 
     * complete status information for the directory element itself, and examine the directory's 
     * ACL.
     *
     * <p> This permission does not enable a user to read the contents of a file in the 
     * directory.
     *
     * <p> Similarly, this permission does not enable a user to lookup the contents of, 
     * obtain complete status information for, or examine the ACL of the subdirectory of 
     * the directory. Those operations require the <code>lookup</code> permission on the ACL
     * of the subdirectory itself.
     *
     * <p><FONT COLOR="666699"><IMG SRC="folder.gif" ALT="Directory Permission" WIDTH="15" HEIGHT="15" BORDER="0"> <U><B>Directory Permission</B></U></FONT><BR>
     * This permission is meaningful with respect to the 
     * directory itself. For example, the <code>insert</code> permission (see: {@link #canInsert})
     * does not control addition of data to a file, but rather creation of a new file or 
     * subdirectory. 
     *
     * <p> Documentation reference: 
     * <A HREF="http://www.transarc.com/Library/documentation/afs/3.6/unix/en_US/HTML/AdminGd/auagd020.htm#HDRWQ782">The AFS ACL Permissions</A>
     *
     * @return  <code>true</code> if and only if the ACL permits <code>lookup</code> access for
     *          directories; <code>false</code> otherwise
     */
    public boolean canLookup()
    {
      return l;
    }
    /** 
     * Sets the ACL permission to accomodate <code>lookup</code> access for directories.
     *
     * @see #canLookup
     * @param	flag	boolean flag that denotes the permission bit for <code>lookup</code> access.
     */
    public void setLookup(boolean flag)
    {
      l = flag;
    }
    /** 
     * <IMG SRC="folder.gif" ALT="Directory Permission" WIDTH="15" HEIGHT="15" BORDER="0"> Tests whether the ACL permits <code>insert</code> access.
     *
     * <p> This permission enables a user to add new files to the directory, either by creating 
     * or copying, and to create new subdirectories. It does not extend into any subdirectories,
     * which are protected by their own ACLs.
     *
     * <p><FONT COLOR="666699"><IMG SRC="folder.gif" ALT="Directory Permission" WIDTH="15" HEIGHT="15" BORDER="0"> <U><B>Directory Permission</B></U></FONT><BR>
     * This permission is meaningful with respect to the 
     * directory itself. For example, the <code>insert</code> permission (see: {@link #canInsert})
     * does not control addition of data to a file, but rather creation of a new file or 
     * subdirectory. 
     *
     * <p> Documentation reference: 
     * <A HREF="http://www.transarc.com/Library/documentation/afs/3.6/unix/en_US/HTML/AdminGd/auagd020.htm#HDRWQ782">The AFS ACL Permissions</A>
     *
     * @return  <code>true</code> if and only if the ACL permits <code>insert</code> access for
     *          directories; <code>false</code> otherwise
     */
    public boolean canInsert()
    {
      return i;
    }
    /** 
     * Sets the ACL permission to accomodate <code>insert</code> access for directories.
     *
     * @see #canInsert
     * @param	flag	boolean flag that denotes the permission bit for <code>insert</code> access.
     */
    public void setInsert(boolean flag)
    {
      i = flag;
    }
    /** 
     * <IMG SRC="folder.gif" ALT="Directory Permission" WIDTH="15" HEIGHT="15" BORDER="0"> Tests whether the ACL permits <code>delete</code> access.
     *
     * <p> This permission enables a user to remove files and subdirectories from the directory 
     * or move them into other directories (assuming that the user has the <code>insert</code>
     * (see: {@link #canInsert}) permission on the ACL of the other directories).
     *
     * <p><FONT COLOR="666699"><IMG SRC="folder.gif" ALT="Directory Permission" WIDTH="15" HEIGHT="15" BORDER="0"> <U><B>Directory Permission</B></U></FONT><BR>
     * This permission is meaningful with respect to the 
     * directory itself. For example, the <code>insert</code> permission (see: {@link #canInsert})
     * does not control addition of data to a file, but rather creation of a new file or 
     * subdirectory. 
     *
     * <p> Documentation reference: 
     * <A HREF="http://www.transarc.com/Library/documentation/afs/3.6/unix/en_US/HTML/AdminGd/auagd020.htm#HDRWQ782">The AFS ACL Permissions</A>
     *
     * @return  <code>true</code> if and only if the ACL permits <code>delete</code> access for
     *          directories; <code>false</code> otherwise
     */
    public boolean canDelete()
    {
      return d;
    }
    /** 
     * Sets the ACL permission to accomodate <code>delete</code> access for directories.
     *
     * @see #canDelete
     * @param	flag	boolean flag that denotes the permission bit for <code>delete</code> rights.
     */
    public void setDelete(boolean flag)
    {
      d = flag;
    }
    /** 
     * <IMG SRC="file.gif" ALT="File Permission" WIDTH="15" HEIGHT="15" BORDER="0"> Tests whether the ACL permits <code>write</code> access.
     *
     * <p> This permission enables a user to modify the contents of files in the directory 
     * and to change their operating system specific mode bits. 
     *
     * <p><FONT COLOR="666699"><IMG SRC="file.gif" ALT="File Permission" WIDTH="15" HEIGHT="15" BORDER="0"> <U><B>File Permission</B></U></FONT><BR>
     * This permission is meaningful with respect to files in 
     * a directory, rather than the directory itself or its subdirectories. 
     *
     * <p> Documentation reference: 
     * <A HREF="http://www.transarc.com/Library/documentation/afs/3.6/unix/en_US/HTML/AdminGd/auagd020.htm#HDRWQ782">The AFS ACL Permissions</A>
     *
     * @return  <code>true</code> if and only if the ACL permits <code>write</code> access for
     *          files; <code>false</code> otherwise
     */
    public boolean canWrite()
    {
      return w;
    }
    /** 
     * Sets the ACL permission to accomodate <code>write</code> access for files.
     *
     * @see #canWrite
     * @param	flag	boolean flag that denotes the permission bit for <code>write</code> access.
     */
    public void setWrite(boolean flag)
    {
      w = flag;
    }
    /** 
     * <IMG SRC="file.gif" ALT="File Permission" WIDTH="15" HEIGHT="15" BORDER="0"> Tests whether the ACL permits the <code>lock</code> authority.
     *
     * <p> This permission enables the user to run programs that issue system calls to 
     * lock files in the directory. 
     *
     * <p><FONT COLOR="666699"><IMG SRC="file.gif" ALT="File Permission" WIDTH="15" HEIGHT="15" BORDER="0"> <U><B>File Permission</B></U></FONT><BR>
     * This permission is meaningful with respect to files in 
     * a directory, rather than the directory itself or its subdirectories. 
     *
     * <p> Documentation reference: 
     * <A HREF="http://www.transarc.com/Library/documentation/afs/3.6/unix/en_US/HTML/AdminGd/auagd020.htm#HDRWQ782">The AFS ACL Permissions</A>
     *
     * @return  <code>true</code> if and only if the ACL permits <code>lock</code> authority for
     *          files; <code>false</code> otherwise
     */
    public boolean canLock()
    {
      return k;
    }
    /** 
     * Sets the ACL permission to accomodate <code>lock</code> access for files.
     *
     * @see #canLock
     * @param	flag	boolean flag that denotes the permission bit for <code>lock</code> rights.
     */
    public void setLock(boolean flag)
    {
      k = flag;
    }
    /** 
     * <IMG SRC="folder.gif" ALT="Directory Permission" WIDTH="15" HEIGHT="15" BORDER="0"> Tests whether the ACL permits <code>administer</code> access.
     *
     * <p> This permission enables a user to change the directory's ACL. Members of the 
     * <code>system:administrators</code> group implicitly have this permission on every 
     * directory (that is, even if that group does not appear on the ACL). Similarly, the 
     * owner of a directory implicitly has this permission on its ACL and those of all 
     * directories below it that he or she owns. 
     *
     * <p><FONT COLOR="666699"><IMG SRC="folder.gif" ALT="Directory Permission" WIDTH="15" HEIGHT="15" BORDER="0"> <U><B>Directory Permission</B></U></FONT><BR>
     * This permission is meaningful with respect to the 
     * directory itself. For example, the <code>insert</code> permission (see: {@link #canInsert})
     * does not control addition of data to a file, but rather creation of a new file or 
     * subdirectory. 
     *
     * <p> Documentation reference: 
     * <A HREF="http://www.transarc.com/Library/documentation/afs/3.6/unix/en_US/HTML/AdminGd/auagd020.htm#HDRWQ782">The AFS ACL Permissions</A>
     *
     * @return  <code>true</code> if and only if the ACL permits <code>administer</code> access for
     *          directories; <code>false</code> otherwise
     */
    public boolean canAdmin()
    {
      return a;
    }
    /** 
     * Sets the ACL permission to accomodate <code>administer</code> rights for directories.
     *
     * @see #canAdmin
     * @param	flag	boolean flag that denotes the permission bit for <code>administer</code> rights.
     */
    public void setAdmin(boolean flag)
    {
      a = flag;
    }

    /////////////// custom override methods ////////////////////

    /**
     * Tests whether two <code>ACL.Entry</code> objects are equal, based on associated
     * username and permission bits.
     *
     * @param entry the ACL.Entry to test
     * @return whether the specifed ACL.Entry is the same as this ACL.Entry
     */
    public boolean equals( ACL.Entry entry )
    {
      return ( (this.getUser().equals( entry.getUser() )) &&
               (this.getPermissionsMask() == entry.getPermissionsMask()) );
    }
  
    /**
     * Returns a String representation of this <CODE>ACL.Entry</CODE>
     *
     * @return a String representation of this <CODE>ACL.Entry</CODE>
     */
    public String toString()
    {
      StringBuffer out = new StringBuffer(username);
      out.append("\t");
      if (r) out.append("r");
      if (l) out.append("l");
      if (i) out.append("i");
      if (d) out.append("d");
      if (w) out.append("w");
      if (k) out.append("k");
      if (a) out.append("a");
      out.append("\n");
      return out.toString();
    }

  }
}









