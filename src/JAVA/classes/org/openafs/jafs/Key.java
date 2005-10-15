/*
 * @(#)Key.java	1.0 6/29/2001
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

import java.util.GregorianCalendar;
import java.util.Date;
import java.io.Serializable;

/**
 * An abstract representation of an AFS key.  It holds information about 
 * the key, such as what its version is.
 * <BR><BR>
 *
 * Constructing an instance of a <code>Key</code> does not mean an actual 
 * AFS key is created on a server -- usually a <code>Key</code>
 * object is a representation of an already existing AFS key.  If, 
 * however, the <code>Key</code> is constructed with the version number of a 
 * key that does not exist on the server represented by the provided 
 * <code>Server</code>, a new key with that version number can be
 * created on that server by calling the {@link #create(String)} methods If
 * such a key does already exist when this method is called, 
 * an exception will be thrown.<BR><BR>
 *
 * <!--Example of how to use class-->
 * The following is a simple example of how to construct and use a 
 * <code>Key</code> object. It obtains the list of <code>Key</code>s from
 * a specified server, and prints the string representation of each key.
 * <PRE>
 * import org.openafs.jafs.Cell;
 * import org.openafs.jafs.AFSException;
 * import org.openafs.jafs.Key;
 * import org.openafs.jafs.Server;
 * ...
 * public class ...
 * {
 *   ...
 *   private Cell cell;
 *   private Server server;
 *   ...
 *   public static void main(String[] args) throws Exception
 *   {
 *     String username   = arg[0];
 *     String password   = arg[1];
 *     String cellName   = arg[2];
 *     String serverName = arg[3];
 * 
 *     token = new Token(username, password, cellName);
 *     cell   = new Cell(token);
 *     server  = new Server(serverName, cell);
 * 
 *     System.out.println("Keys in Server " + server.getName() + ":");
 *     Key[] keys = server.getKeys();
 *     for (int i = 0; i < keys.length; i++) {
 *       System.out.println(" -> " + keys[i] );
 *     }
 *   }
 *   ...
 * }
 * </PRE>
 *
 */
public class Key implements Serializable, Comparable
{
  protected Server server;

  protected int version;
  protected long checkSum;
  protected String encryptionKey;
  protected int lastModDate;
  protected int lastModMs;
  
  protected GregorianCalendar lastModDateDate;

  protected boolean cachedInfo;

  /**
   * Constructs a new <CODE>Key</CODE> object instance given the version of 
   * the AFS key and the AFS server, represented by <CODE>server</CODE>, 
   * to which it belongs.   This does not actually
   * create a new AFS key, it just represents one.
   * If <code>version</code> is not an actual AFS key, exceptions
   * will be thrown during subsequent method invocations on this 
   * object, unless the {@link #create(String)}
   * method is explicitly called to create it.
   *
   * @exception AFSException      If an error occurs in the native code
   * @param version      the version of the key to represent
   * @param server       the server to which the key belongs.
   */
  public Key( int version, Server server ) throws AFSException
  {
    this.server = server;
    this.version = version;

    lastModDateDate = null;

    cachedInfo = false;
  }

  /**
   * Constructs a new <CODE>Key</CODE> object instance given the version of 
   * the AFS key and the AFS server, represented by <CODE>server</CODE>, 
   * to which it belongs.    This does not actually
   * create a new AFS key, it just represents one.
   * If <code>version</code> is not an actual AFS key, exceptions
   * will be thrown during subsequent method invocations on this 
   * object, unless the {@link #create(String)}
   * method is explicitly called to create it. Note that if the key does not
   * exist and <code>preloadAllMembers</code> is true, an exception will
   * be thrown. 
   *
   * <P> This constructor is ideal for point-in-time representation and 
   * transient applications. It ensures all data member values are set and 
   * available without calling back to the filesystem at the first request 
   * for them.  Use the {@link #refresh()} method to address any coherency 
   * concerns.
   *
   * @param version            the version of the key to represent 
   * @param server             the server to which the key belongs.
   * @param preloadAllMembers  true will ensure all object members are set 
   *                           upon construction; otherwise members will be 
   *                           set upon access, which is the default behavior.
   * @exception AFSException      If an error occurs in the native code
   * @see #refresh
   */
  public Key( int version, Server server, boolean preloadAllMembers ) 
      throws AFSException
  {
    this(version, server);
    if (preloadAllMembers) refresh(true);
  }
  
  /**
   * Creates a blank <code>Key</code> given the server to which the key
   * belongs.  This blank object can then be passed into other methods to 
   * fill out its properties.
   *
   * @exception AFSException      If an error occurs in the native code
   * @param server       the server to which the key belongs.
   */
  Key( Server server ) throws AFSException
  {
    this( -1, server );
  }

  /*-------------------------------------------------------------------------*/

  /**
   * Refreshes the properties of this Key object instance with values from 
   * the AFS key it represents.  All properties that have been initialized 
   * and/or accessed will be renewed according to the values of the AFS key 
   * this Key object instance represents.
   *
   * <P>Since in most environments administrative changes can be administered
   * from an AFS command-line program or an alternate GUI application, this
   * method provides a means to refresh the Java object representation and
   * thereby ascertain any possible modifications that may have been made
   * from such alternate administrative programs.  Using this method before
   * an associated instance accessor will ensure the highest level of 
   * representative accuracy, accommodating changes made external to the
   * Java application space.  If administrative changes to the underlying AFS 
   * system are only allowed via this API, then the use of this method is 
   * unnecessary.
   * 
   * @exception AFSException  If an error occurs in the native code
   */
  public void refresh() throws AFSException
  {
    refresh(false);
  }

  /**
   * Refreshes the properties of this Key object instance with values from 
   * the AFS key it represents.  If <CODE>all</CODE> is <CODE>true</CODE> 
   * then <U>all</U> of the properties of this Key object instance will be 
   * set, or renewed, according to the values of the AFS key it represents, 
   * disregarding any previously set properties.
   *
   * <P> Thus, if <CODE>all</CODE> is <CODE>false</CODE> then properties that 
   * are currently set will be refreshed and properties that are not set will 
   * remain uninitialized. See {@link #refresh()} for more information.
   *
   * @param all   if true set or renew all object properties; otherwise renew 
   * all set properties
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  protected void refresh(boolean all) throws AFSException
  {
    if( all || cachedInfo ) {
      refreshInfo();
    }
  }

  /**
   * Refreshes the information fields of this <code>Key</code> to reflect the 
   * current state of the AFS server key.  These inlclude the last 
   * modification time, etc.
   *
   * @exception AFSException      If an error occurs in the native code
   */
  protected void refreshInfo() throws AFSException
  {
    getKeyInfo( server.getBosHandle(), version, this );
    cachedInfo = true;
    lastModDateDate = null;
  }

  /**
   * Creates a key with this <code>Key's</code> version number at the server,
   * using the specified <code>String</code> for the key.
   *
   * @param keyString   the string to use for the encryption key
   * @exception AFSException      If an error occurs in the native code
   */
  public void create( String keyString ) throws AFSException
  {
    create( server.getCell().getCellHandle(), server.getBosHandle(), version, 
	    keyString );
  }

  /**
   * Removes the key with this <code>Key's</code> version number from 
   * the server.
   *
   * @exception AFSException      If an error occurs in the native code
   */
  public void delete( ) throws AFSException
  {
    delete( server.getBosHandle(), version );

    encryptionKey = null;
    cachedInfo = false;
  }

  //////////////// accessors:  ////////////////////////

  /**
   * Returns the version of this key in primitive form.
   *
   * @return the version number of this key
   */
  public int getVersion()
  {
    return version;
  }

  /**
   * Returns the server this key is associated with.
   *
   * @return this key's server
   */
  public Server getServer()
  {
    return server;
  }

  /**
   * Returns the encrypted key as a string in octal form. This is how AFS
   * prints it out on the command line.  An example would be:
   * '\040\205\211\241\345\002\023\211'.
   *
   * @return the encrypted key
   * @exception AFSException      If an error occurs in the native code
   */
  public String getEncryptionKey() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return encryptionKey;
  }

  /**
   * Returns the check sum of this key.
   *
   * @return the check sum of this key
   * @exception AFSException      If an error occurs in the native code
   */
  public long getCheckSum() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return checkSum;
  }

  /**
   * Returns the last modification date of this key.
   *
   * @return the last modification date of this key
   * @exception AFSException      If an error occurs in the native code
   */
  public GregorianCalendar getLastModDate() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    if ( lastModDateDate == null && cachedInfo ) {
	  // make it into a date . . .
	  lastModDateDate = new GregorianCalendar();
	  long longTime = ((long) lastModDate)*1000;
	  Date d = new Date( longTime );
	  lastModDateDate.setTime( d );
    }
    return lastModDateDate;
  }

  /////////////// information methods ////////////////////

  /**
   * Returns a <code>String</code> representation of this <code>Key</code>.  
   * Contains the information fields.
   *
   * @return a <code>String</code> representation of the <code>Key</code>
   */
  public String getInfo()
  {
    String r;
    try {
	r = "Key version number: " + getVersion() + "\n";
	r += "\tencrypted key: " + getEncryptionKey() + "\n";
	r += "\tcheck sum: " + getCheckSum() + "\n";
	r += "\tlast mod time: " + getLastModDate().getTime() + "\n";
    } catch( Exception e ) {
	return e.toString();
    }
    return r;
  }

  /////////////// override methods ////////////////////

  /**
   * Compares two Key objects respective to their key version and does not
   * factor any other attribute.
   *
   * @param     key    The Key object to be compared to this Key instance
   * 
   * @return    Zero if the argument is equal to this Key's version, a
   *            value less than zero if this Key's version is less than 
   *            the argument, or a value greater than zero if this Key's 
   *            version is greater than the argument
   */
  public int compareTo(Key key)
  {
    return (this.getVersion() - key.getVersion());
  }

  /**
   * Comparable interface method.
   *
   * @see #compareTo(Key)
   */
  public int compareTo(Object obj)
  {
      return compareTo((Key)obj);
  }

  /**
   * Tests whether two <code>Key</code> objects are equal, based on their 
   * encryption key, version, and associated Server.
   *
   * @param otherKey   the Key to test
   * @return whether the specifed Key is the same as this Key
   */
  public boolean equals( Key otherKey )
  {
    try {
      return ( this.getEncryptionKey().equals(otherKey.getEncryptionKey()) ) &&
             ( this.getVersion() == otherKey.getVersion() ) &&
             ( this.getServer().equals(otherKey.getServer()) );
    } catch (Exception e) {
      return false;
    }
  }

  /**
   * Returns the name of this <CODE>Key</CODE>
   *
   * @return the name of this <CODE>Key</CODE>
   */
  public String toString()
  {
    try {
      return getVersion() + " - " + getEncryptionKey() + " - " + getCheckSum();
    } catch (Exception e) {
      return e.toString();
    }
  }

  /////////////// native methods ////////////////////

  /**
   * Fills in the information fields of the provided <code>Key</code>. 
   *
   * @param serverHandle    the bos handle of the server to which the key 
   *                        belongs
   * @see Server#getBosServerHandle
   * @param version     the version of the key for which to get the information
   * @param key     the <code>Key</code> object in which to fill in the 
   *                information
   * @see Server
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void getKeyInfo( long serverHandle, int version, 
					   Key key ) 
	throws AFSException;

  /**
   * Create a server key.
   *
   * @param cellHandle   the handle of the cell to which the server belongs
   * @see Cell#getCellHandle
   * @param serverHandle  the bos handle of the server to which the key will 
   *                      belong
   * @see Server#getBosServerHandle
   * @param versionNumber   the version number of the key to create (0 to 255)
   * @param keyString     the <code>String</code> version of the key that will
   *                      be encrypted
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void create( long cellHandle, long serverHandle, int versionNumber, String keyString )
    throws AFSException;

  /**
   * Delete a server key.
   *
   * @param serverHandle  the bos handle of the server to which the key belongs
   * @see Server#getBosServerHandle
   * @param versionNumber   the version number of the key to remove (0 to 255)
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void delete( long serverHandle, int versionNumber )
    throws AFSException;

  /**
   * Reclaims all memory being saved by the key portion of the native library.
   * This method should be called when no more <code>Key</code> objects are 
   * expected to be
   * used.
   */
  protected static native void reclaimKeyMemory();

}








