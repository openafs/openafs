/*
 * @(#)User.java	1.0 6/29/2001
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
import java.util.ArrayList;
import java.io.Serializable;

/**
 * An abstract representation of an AFS user.  It holds information about 
 * the user, such as what groups it belongs to.
 * <BR><BR>
 *
 * Constructing an instance of a <code>User</code> does not mean an actual 
 * AFS user is created in a cell -- usually a <code>User</code>
 * object is a representation of an already existing AFS user.  If, 
 * however, the <code>User</code> is constructed with the name of a 
 * user that does not exist in the cell represented by the provided 
 * <code>Cell</code>, a new user with that name can be
 * created in that server by calling the {@link #create(String, int)} or
 * {@link #create(String)} method. If such a user does already exist when 
 * one of these methods is called, an exception will be thrown.<BR><BR>
 *
 * Each <code>User</code> object has its own individual set of
 * <code>Group</code>s that it owns and <code>Group</code>s for which
 * it is a member.  These represents the properties and attributes 
 * of an actual AFS user.
 * <BR><BR>
 *
 * Since this <code>User</code> object is a union of both the PTS and KAS
 * properties of AFS users, some methods meant for users with a PTS entry
 * will throw exceptions if used on a user with only a KAS entry, and vice
 * versa.<BR><BR>  
 *
 * <!--Information on how member values are set-->
 *
 * Associated with an AFS user are many attributes, such as whether or not
 * it can change its own password, or who is allowed to find out the groups
 * to which this user belongs.  The <code>User</code> class has many
 * "set" methods to indicate values for these attributes (i.e. 
 * {@link #setChangePassword(boolean)} and {@link #setListMembership(int)}).  
 * However, in order for these values to be written to the actual AFS user, 
 * the {@link #flushInfo()} method needs to be called.  This writes all user
 * attributes set through this API to AFS.  This is done to minimize calls 
 * through JNI.<BR><BR>
 *
 * <!--Example of how to use class-->
 * The following is a simple example of how to construct and use a 
 * <code>User</code> object.  It iterates through the list of users 
 * (a union of pts and kas users) for a cell, and prints out the name and 
 * id of each.
 *
 * <PRE>
 * import org.openafs.jafs.Cell;
 * import org.openafs.jafs.AFSException;
 * import org.openafs.jafs.User;
 * ...
 * public class ...
 * {
 *   ...
 *   private Cell cell;
 *   ...
 *   public static void main(String[] args) throws Exception
 *   {
 *     String username   = arg[0];
 *     String password   = arg[1];
 *     String cellName   = arg[2];
 * 
 *     token  = new Token(username, password, cellName);
 *     cell   = new Cell(token);
 *     server = cell.getServer(serverName);
 * 
 *     System.out.println("Users in Cell " + cell.getName() + ":");
 *     User[] users = cell.getUsers();
 *     for (int i = 0; i < users.length; i++) {
 *       System.out.println(" -> " + users[i] + ": " users[i].getID());
 *     }
 *   }
 *   ...
 * }
 * </PRE>
 *
 */
public class User implements PTSEntry, Serializable, Comparable
{
  /**
   * Only the owner of the user has access
   */
  public static final int USER_OWNER_ACCESS = 0;
  /**
   * Any user has access
   */
  public static final int USER_ANYUSER_ACCESS = 1;

  /**
   * User has administrative kas privileges
   */
  public static final int ADMIN = 0;
  /**
   * User has no administrative kas privileges
   */
  public static final int NO_ADMIN = 1;

  /**
   * TGS will grant tickets for user
   */
  public static final int GRANT_TICKETS = 0;
  /**
   * TGS will not grant tickets for user
   */
  public static final int NO_GRANT_TICKETS = 1;

  /**
   * TGS can use user's key for an encryption key
   */
  public static final int ENCRYPT = 0;
  /**
   * TGS cannot use user's key for an encryption key
   */
  public static final int NO_ENCRYPT = 1;

  /**
   * User can change their password
   */
  public static final int CHANGE_PASSWORD = 0;
  /**
   * User cannot change their password
   */
  public static final int NO_CHANGE_PASSWORD = 1;

  /**
   * User can reuse their password
   */
  public static final int REUSE_PASSWORD = 0;
  /**
   * User cannot reuse their password
   */
  public static final int NO_REUSE_PASSWORD = 1;

  protected Cell cell;
  protected long cellHandle;
  protected String name;

  /**
   * Does this user have a kas entry?
   */
  protected boolean kas;
  /**
   * Does this user have a pts entry?
   */
  protected boolean pts;

  // pts fields
  protected int groupCreationQuota;
  protected int groupMembershipCount;
  protected int nameUID;
  protected int ownerUID;
  protected int creatorUID;

  /**
   * who is allowed to execute pts examine for this user.  Valid values are:
   *   <ul>
   *   <li>{@link #USER_OWNER_ACCESS}  
   *       -- only the owner has permission</li>
   *   <li>{@link #USER_ANYUSER_ACCESS} 
   *       -- any user has permission</li></ul>
   */
  protected int listStatus;
  /**
   * who is allowed to execute pts listowned for this user.  Valid values are:
   *   <ul>
   *   <li>{@link #USER_OWNER_ACCESS}  
   *       -- only the owner has permission</li>
   *   <li>{@link #USER_ANYUSER_ACCESS} 
   *       -- any user has permission</li></ul>
   */
  protected int listGroupsOwned;
  /**
   * who is allowed to execute pts membership for this user.  Valid values are:
   *   <ul>
   *   <li>{@link #USER_OWNER_ACCESS} 
   *       -- only the owner has permission</li>
   *   <li>{@link #USER_ANYUSER_ACCESS} 
   *       -- any user has permission</li></ul>
   */
  protected int listMembership;
  protected String owner;
  protected String creator;
  
  // lists
  protected ArrayList groups;
  protected ArrayList groupNames;
  protected ArrayList groupsOwned;
  protected ArrayList groupsOwnedNames;

  // kas fields
  /**
   * whether or not this user has kas administrative privileges. 
   *   Valid values are:
   *   <ul>
   *   <li>{@link #ADMIN}</li>
   *   <li>{@link #NO_ADMIN}</li></ul>
   */
  protected int adminSetting;
  /**
   * whether the TGS will grant tickets for this user. Valid values are:
   *   <ul>
   *   <li>{@link #GRANT_TICKETS}</li>
   *   <li>{@link #NO_GRANT_TICKETS}</li></ul>
   */
  protected int tgsSetting;
  /**
   * whether the TGS can use this user's key as an encryption key. Valid values are:
   *   <ul>
   *   <li>{@link #ENCRYPT}</li>
   *   <li>{@link #NO_ENCRYPT}</li></ul>
   */
  protected int encSetting;
  /**
   * whether this user is allowed to change its password. Valid values are:
   *   <ul>
   *   <li>{@link #CHANGE_PASSWORD}</li>
   *   <li>{@link #NO_CHANGE_PASSWORD}</li></ul>
   */
  protected int cpwSetting;
  /**
   * whether this user is allowed to reuse its password. Valid values are:
   *   <ul>
   *   <li>{@link #REUSE_PASSWORD}</li>
   *   <li>{@link #NO_REUSE_PASSWORD}</li></ul>
   */
  protected int rpwSetting;
  protected int userExpiration;
  protected int lastModTime;
  protected String lastModName;
  protected int lastChangePasswordTime;
  protected int maxTicketLifetime;
  protected int keyVersion;
  protected String encryptionKey;
  protected long keyCheckSum;
  protected int daysToPasswordExpire;
  protected int failLoginCount;
  protected int lockTime;
  protected int lockedUntil;

  // Dates and times
  protected GregorianCalendar lockedUntilDate;
  protected GregorianCalendar userExpirationDate;
  protected GregorianCalendar lastModTimeDate;
  protected GregorianCalendar lastChangePasswordTimeDate;

  /**
   * Whether or not the information fields of this user have been filled.
   */
  protected boolean cachedInfo;

  /**
   * Constructs a new <code>User</code> object instance given the name 
   * of the AFS user and the AFS cell, represented by 
   * <CODE>cell</CODE>, to which it belongs.   This does not actually
   * create a new AFS user, it just represents one.
   * If <code>name</code> is not an actual AFS user, exceptions
   * will be thrown during subsequent method invocations on this 
   * object, unless the {@link #create(String, int)} or {@link #create(String)}
   * method is explicitly called to create it.
   *
   * @param name  the name of the user to represent
   * @param cell  the cell to which the user belongs.
   * @exception AFSException      If an error occurs in the native code
   */
  public User( String name, Cell cell ) throws AFSException
  {
    this.name = name;
    this.cell = cell;
    cellHandle = cell.getCellHandle();
    
    groups = null;
    groupNames = null;
    groupsOwned = null;
    groupsOwnedNames = null;
    cachedInfo = false;
    kas = false;
    pts = false;
  }

  /**
   * Constructs a new <code>User</code> object instance given the name 
   * of the AFS user and the AFS cell, represented by 
   * <CODE>cell</CODE>, to which it belongs.   This does not actually
   * create a new AFS user, it just represents one.
   * If <code>name</code> is not an actual AFS user, exceptions
   * will be thrown during subsequent method invocations on this 
   * object, unless the {@link #create(String, int)} or {@link #create(String)}
   * method is explicitly called to create it.   Note that if the process
   * doesn't exist and <code>preloadAllMembers</code> is true, an exception
   * will be thrown.
   *
   * <P> This constructor is ideal for point-in-time representation and 
   * transient applications. It ensures all data member values are set and 
   * available without calling back to the filesystem at the first request 
   * for them.  Use the {@link #refresh()} method to address any coherency 
   * concerns.
   *
   * @param name               the name of the user to represent 
   * @param cell               the cell to which the user belongs.
   * @param preloadAllMembers  true will ensure all object members are 
   *                           set upon construction;
   *                           otherwise members will be set upon access, 
   *                           which is the default behavior.
   * @exception AFSException      If an error occurs in the native code
   * @see #refresh
   */
  public User( String name, Cell cell, boolean preloadAllMembers ) 
      throws AFSException
  {
    this(name, cell);
    if (preloadAllMembers) refresh(true);
  }
  
  /**
   * Constructs a blank <code>User</code> object given the cell to which 
   * the user belongs.  This blank object can then be passed into other 
   * methods to fill out its properties.
   *
   * @exception AFSException      If an error occurs in the native code
   * @param cell       the cell to which the user belongs.
   */
  User( Cell cell ) throws AFSException
  {
    this( null, cell );
  }

  /*-------------------------------------------------------------------------*/

  /**
   * Creates the kas and pts entries for a new user in this cell.  
   * Automatically assigns a user id.
   *   *
   * @param password      the password for the new user
   * @exception AFSException  If an error occurs in the native code
   */    
  public void create( String password ) throws AFSException
  {
    create( password, 0 );
  }

  /**
   * Creates the kas and pts entries for a new user in this cell.
   *
   * @param password      the password for the new user
   * @param uid       the user id to assign to the new user
   *
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void create( String password, int uid ) throws AFSException
  {
    create( cell.getCellHandle(), name, password, uid );
  }

  /**
   * Deletes the pts and kas entries for a user in this cell. Deletes this user
   * from the membership list of the groups to which it belonged, but does not 
   * delete the groups owned by this user.  Also nullifies this corresponding 
   * Java object.
   *
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void delete() throws AFSException
  {
	delete( cell.getCellHandle(), name );

	cell = null;
	name = null;
	kas = false;
	pts = false;
	owner = null;
	creator = null;
	groups = null;
	groupsOwned = null;
	groupNames = null;
	groupsOwnedNames = null;
	lastModName = null;
	encryptionKey = null;
	lockedUntilDate = null;
	userExpirationDate = null;
	lastModTimeDate = null;
	lastChangePasswordTimeDate = null;
	try {
	    finalize();
	} catch( java.lang.Throwable t ) {
	    throw new AFSException( t.getMessage() );
	}
  }

  /**
   * Unlocks the given user if they were locked out of the cell.
   *
   * @param userName      the name of the user to unlock
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void unlock() throws AFSException
  {
    unlock( cell.getCellHandle(), name );
    lockedUntil = 0;
    lockedUntilDate = null;
  }

  /**
   * Flushes the current information of this <code>User</code> object to disk.
   * This will update the information of the actual AFS user to match the 
   * settings that have been modified within this <code>User</code> object.  
   * This function must be called before any changes made to the information 
   * fields of this user will be seen by AFS.
   *
   * @exception AFSException      If an error occurs in the native code
   */
  public void flushInfo() throws AFSException
  {
    setUserInfo( cell.getCellHandle(), name, this );
  }

  /**
   * Change the name of this user.  Automatically flushes the info of this 
   * user in order to update kas entry of the new name.  NOTE:  renaming a 
   * locked user will unlock that user.
   *
   * @param newName    the new name for this user
   * @exception AFSException  If an error occurs in the native code
   */
  public void rename( String newName ) throws AFSException
  {
    rename( cell.getCellHandle(), name, newName );
    name = newName;
    flushInfo();
  }

  /**
   * Refreshes the properties of this User object instance with values from 
   * the AFS user it represents.  All properties that have been initialized 
   * and/or accessed will be renewed according to the values of the AFS user 
   * this User object instance represents.
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
   * Refreshes the properties of this User object instance with values from 
   * the AFS user it represents.  If <CODE>all</CODE> is <CODE>true</CODE> 
   * then <U>all</U> of the properties of this User object instance will be 
   * set, or renewed, according to the values of the AFS user it represents, 
   * disregarding any previously set properties.
   *
   * <P> Thus, if <CODE>all</CODE> is <CODE>false</CODE> then properties that 
   * are currently set will be refreshed and properties that are not set will 
   * remain uninitialized. See {@link #refresh()} for more information.
   *
   * @param all   if true set or renew all object properties; otherwise renew 
   *              all set properties
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  protected void refresh(boolean all) throws AFSException
  {
    if ( all || cachedInfo ) {
      refreshInfo();
    }
    if ( all || groupsOwned != null ) {
      refreshGroupsOwned();
    }
    if ( all || groupsOwnedNames != null ) {
      refreshGroupsOwnedNames();
    }
    if ( all || groups != null ) {
      refreshGroups();
    }
    if ( all || groupNames != null ) {
      refreshGroupNames();
    }
  }

  /**
   * Refreshes the information fields of this <code>User</code> to reflect 
   * the current state of the AFS user.  Does not refresh the groups to which 
   * the user belongs or groups owned by the user.
   *
   * @exception AFSException      If an error occurs in the native code
   */
  protected void refreshInfo() throws AFSException
  {
    getUserInfo( cell.getCellHandle(), name, this );
    cachedInfo = true;
    lastModTimeDate = null;
    lastChangePasswordTimeDate = null;
    lockedUntilDate = null;
    userExpirationDate = null;
  }

  /**
   * Refreshes the current information about the group names to which the 
   * user belongs.  Does not refresh the information fields of the user or 
   * the groups owned.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshGroupNames() throws AFSException
  {
    String currName;
    long iterationID = getUserGroupsBegin( cell.getCellHandle(), name );
    groupNames = new ArrayList();
    while( ( currName = getUserGroupsNextString( iterationID ) ) != null ) {
      groupNames.add( currName );
    } 
    getUserGroupsDone( iterationID );
  }
  
  /**
   * Refreshes the current information about the group objects to which the 
   * user belongs.  Does not refresh the information fields of the user or 
   * the groups owned.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshGroups() throws AFSException
  {
    Group currGroup;
    long iterationID = getUserGroupsBegin( cell.getCellHandle(), name );

    groups = new ArrayList();

    currGroup = new Group( cell );
    while( getUserGroupsNext( cellHandle, iterationID, currGroup ) != 0 ) {
      groups.add( currGroup );
      currGroup = new Group( cell );
    } 
    getUserGroupsDone( iterationID );
  }

  /**
   * Refreshes the current information about the group names that the user 
   * owns.  Does not refresh the information fields of the user or the groups 
   * belonged to.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshGroupsOwnedNames() throws AFSException
  {
    String currName;
    long iterationID = this.getGroupsOwnedBegin( cell.getCellHandle(), name );
    groupsOwnedNames = new ArrayList();
    while( ( currName = this.getGroupsOwnedNextString( iterationID ) ) 
	   != null ) {
      groupsOwnedNames.add( currName );
    } 
    this.getGroupsOwnedDone( iterationID );
  }
  
  /**
   * Refreshes the current information about the group objects that the user \
   * owns.  Does not refresh the information fields of the user or the groups 
   * belonged to.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshGroupsOwned() throws AFSException
  {
    Group currGroup;
    long iterationID = getGroupsOwnedBegin( cell.getCellHandle(), name );
    groupsOwned = new ArrayList();
    currGroup = new Group( cell );
    while( getGroupsOwnedNext( cellHandle, iterationID, currGroup ) != 0 ) {
      groupsOwned.add( currGroup );
      currGroup = new Group( cell );
    } 
    getGroupsOwnedDone( iterationID );
  }

  /**
   * Adds an access control list entry for some AFS directory for this user.
   *
   * @param directory    the full path of the place in the AFS file system 
   *                     for which to add an entry
   * @param read    whether or not to allow read access to this user
   * @param write    whether or not to allow write access to this user
   * @param lookup    whether or not to allow lookup access to this user
   * @param delete    whether or not to allow deletion access to this user
   * @param insert    whether or not to allow insertion access to this user
   * @param lock    whether or not to allow lock access to this user
   * @param admin    whether or not to allow admin access to this user
   * @exception AFSException  If an error occurs in the native code
  public void setACL( String directory, boolean read, boolean write, boolean lookup, boolean delete, boolean insert, boolean lock, boolean admin ) throws AFSException
  {
    cell.setACL( directory, name, read, write, lookup, delete, insert, lock, admin );
  }
   */

  //////////////// ACCESSORS ////////////////////////

  /**
   * Returns the name of this user.
   *
   * @return the name of this user
   */
  public String getName()
  {
    return name;
  }

  /**
   * Returns the Cell this user belongs to.
   *
   * @return the Cell this user belongs to
   */
  public Cell getCell()
  {
    return cell;
  }

  /**
   * Returns whether or not this user has a kas entry.
   *
   * @return whether or not this user has a kas entry
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public boolean isKAS() throws AFSException
  {
    if ( !cachedInfo ) refreshInfo();
    return kas;
  }

  /**
   * Returns whether or not this user has a pts entry.
   *
   * @return whether or not this user has a pts entry
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public boolean isPTS() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return pts;
  }

  /**
   * PTS: Returns an array of the <code>Group</code> objects 
   * to which this user belongs.
   *
   * @return      an array of the groups to which this user belongs
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public Group[] getGroups() throws AFSException
  {
    if( groups == null ) refreshGroups();
    return (Group[]) groups.toArray( new Group[groups.size()] );
  }

  /**
   * PTS: Returns the total count of groups this user owns.
   *
   * <P>If the total list of groups or group names have already been 
   * collected (see {@link #getGroupsOwned()}), then the returning value 
   * will be calculated based upon the current list.  Otherwise, PTS will 
   * be explicitly queried for the information.
   *
   * @return      total count of groups this user owns
   * @exception AFSException  If an error occurs in the native code
   * @see #getGroupsOwned()
   * @see #getGroupsOwnedNames()
   */
  public int getGroupsOwnedCount() throws AFSException
  {
    if( groupsOwned != null ) {
      return groupsOwned.size();
    } else if ( groupsOwnedNames != null ) {
      return groupsOwnedNames.size();
    } else {
      return getGroupsOwnedCount( cell.getCellHandle(), name );
    }
  }

  /**
   * PTS: Returns an array of the <code>Group</code> objects 
   * this user owns.
   *
   * @return     an array of the <code>Groups</code> this user owns
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public Group[] getGroupsOwned() throws AFSException
  {
    if( groupsOwned == null ) refreshGroupsOwned();
    return (Group[]) groupsOwned.toArray( new Group[groupsOwned.size()] );
  }

  /**
   * PTS: Returns a <code>String</code> array of the group names 
   * to which this user belongs.
   *
   * @return      a <code>String</code> array of the groups to which this 
   *              user belongs
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public String[] getGroupNames() throws AFSException
  {
    if( groupNames == null ) refreshGroupNames();
    return (String []) groupNames.toArray( new String[groupNames.size() ] );
  }

  /**
   * PTS: Returns a <code>String</code> array of the group names 
   * this user owns.
   *
   * @return     a <code>String</code> array of the groups this user owns
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public String[] getGroupsOwnedNames() throws AFSException
  {
    if( groupsOwnedNames == null ) refreshGroupsOwnedNames();
    return (String []) groupsOwnedNames.toArray( new String[groupsOwnedNames.size() ] );
  }

  /**
   * PTS: Returns the numeric AFS id of this user.
   *
   * @return the AFS id of this user
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public int getUID() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return nameUID;
  }

  /**
   * PTS: Returns how many more groups this user is allowed to create.  
   * -1 indicates unlimited.
   *
   * @return the group creation quota
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public int getGroupCreationQuota() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return groupCreationQuota;
  }

  /**
   * PTS: Returns the number of groups to which this user belongs.
   *
   * @return the group membership count
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public int getGroupMembershipCount() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return groupMembershipCount;
  }

  /**
   * PTS: Returns the owner of this user in the form of a {@link PTSEntry}.
   *
   * <P>The returning object could be either a {@link User} or {@link Group};
   * to determine what type of object the {@link PTSEntry} represents,
   * call the {@link PTSEntry#getType()} method.
   *
   * @return the owner of this user
   * @exception AFSException  If an error occurs in the native code
   * @see PTSEntry
   * @see PTSEntry#getType()
   * @see #refresh()
   */
  public PTSEntry getOwner() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    if (owner == null) return null;
    if (ownerUID > 0) {
      return new User(owner, cell);
    } else {
      return new Group(owner, cell);
    }
  }

  /**
   * PTS: Returns the creator of this user in the form of a {@link PTSEntry}.
   *
   * <P>The returning object could be either a {@link User} or {@link Group};
   * to determine what type of object the {@link PTSEntry} represents,
   * call the {@link PTSEntry#getType()} method.
   *
   * @return the creator of this user
   * @exception AFSException  If an error occurs in the native code
   * @see PTSEntry
   * @see PTSEntry#getType()
   * @see #refresh()
   */
  public PTSEntry getCreator() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    if (creator == null) return null;
    if (creatorUID > 0) {
      return new User(creator, cell);
    } else {
      return new Group(creator, cell);
    }
  }

  /**
   * Returns the type of {@link PTSEntry} this object represents.
   *
   * <P>This method will always return {@link PTSEntry#PTS_USER}.
   *
   * @return  the type of PTSEntry this object represents 
              (will always return {@link PTSEntry#PTS_USER})
   * @see PTSEntry
   * @see PTSEntry#getType()
   */
  public short getType()
  {
    return PTSEntry.PTS_USER;
  }

  /**
   * PTS: Returns who can list the status (pts examine) of this user.  
   * Valid values are:
   * <ul>
   * <li><code>{@link #USER_OWNER_ACCESS}</code> 
   *           -- only the owner has permission</li>
   * <li><code>{@link #USER_ANYUSER_ACCESS}</code> 
   *           -- any user has permission</li>
   * </ul>
   *
   * @return the status listing permission
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public int getListStatus() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return listStatus;
  }

  /**
   * PTS: Returns who can list the groups owned (pts listowned) by this user.  
   * Valid values are:
   * <ul>
   * <li><code>{@link #USER_OWNER_ACCESS}</code> 
   *           -- only the owner has permission</li>
   * <li><code>{@link #USER_ANYUSER_ACCESS}</code> 
   *           -- any user has permission</li>
   * </ul>
   *
   * @return the groups owned listing permission
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public int getListGroupsOwned() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return listGroupsOwned;
  }

  /**
   * PTS: Returns who can list the groups (pts membership) to which this 
   * user belongs.  
   * Valid values are:
   * <ul>
   * <li><code>{@link #USER_OWNER_ACCESS}</code> 
   *           -- only the owner has permission</li>
   * <li><code>{@link #USER_ANYUSER_ACCESS}</code> 
   *           -- any user has permission</li>
   * </ul>
   *
   * @return the membership listing permission
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public int getListMembership() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return listMembership;
  }

  /**
   * KAS: Returns whether or not this user has kas administrative privileges
   *
   * @return whether or not this user has kas administrative priveleges
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public boolean isAdmin() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return (adminSetting == this.ADMIN);
  }

  /**
   * KAS: Returns whether or not TGS will issue tickets for this user
   *
   * @return whether or not TGS will issue tickets for this user
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public boolean willGrantTickets() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return (tgsSetting == this.GRANT_TICKETS);
  }

  /**
   * KAS: Returns whether or not TGS can use this users ticket for an encryption key
   *
   * @return whether or not TGS can use this users ticket for an encryption key
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public boolean canEncrypt() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return (encSetting == this.ENCRYPT);
  }

  /**
   * KAS: Returns whether or not the user can change their password
   *
   * @return whether or not the user can change their password
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public boolean canChangePassword() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return (cpwSetting == this.CHANGE_PASSWORD);
  }

  /**
   * KAS: Returns whether or not the user can reuse their password
   *
   * @return whether or not the user can reuse their password
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public boolean canReusePassword() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return (rpwSetting == this.REUSE_PASSWORD);
  }

  /**
   * KAS: Returns the date and time the user expires.  
   * A <code>null</code> value indicates the user never exipres (or that
   * there is no kas entry for this user).
   *
   * @return the date and time the user expires
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public int getUserExpiration() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return userExpiration;
  }
  /**
   * KAS: Returns the date and time the user expires.  
   * A <code>null</code> value indicates the user never expires (or that
   * there is no kas entry for this user).
   *
   * @return the date and time the user expires
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public GregorianCalendar getUserExpirationDate() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    if( userExpirationDate == null && userExpiration != 0 ) {
      // make it into a date . . .
      if( userExpiration == 0 ) {
        userExpirationDate = null;
      } else {
        userExpirationDate = new GregorianCalendar();
        long longTime = ((long) userExpiration)*1000;
        Date d = new Date( longTime );
        userExpirationDate.setTime( d );
      }
    }
    return userExpirationDate;
  }

  /**
   * KAS: Returns the date and time (in UTC) the user's KAS entry was 
   * last modified.
   *
   * @return the date and time (in UTC) the user was last modified
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public int getLastModTime() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return lastModTime;
  }
  /**
   * KAS: Returns the date and time the user was last modified.
   *
   * @return the date and time the user was last modified
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public GregorianCalendar getLastModTimeDate() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    if( lastModTimeDate == null ) {
      // make it into a date . . .
      lastModTimeDate = new GregorianCalendar();
      long longTime = ((long) lastModTime)*1000;
      Date d = new Date( longTime );
      lastModTimeDate.setTime( d );
    }
    return lastModTimeDate;
  }

  /**
   * KAS: Returns the name of the user that last modified this user.
   *
   * @return the name of this user that last modified this user.
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public String getLastModName() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return lastModName;
  }

  /**
   * KAS: Returns the last date and time the user changed its password.
   *
   * @return the last date and time the user changed its password.
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public GregorianCalendar getLastChangePasswordTimeDate() 
      throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    if( lastChangePasswordTimeDate == null ) {
      // make it into a date . . .
      lastChangePasswordTimeDate = new GregorianCalendar();
      long longTime = ((long) lastChangePasswordTime)*1000;
      Date d = new Date( longTime );
      lastChangePasswordTimeDate.setTime( d );
    }
    return lastChangePasswordTimeDate;
  }

  /**
   * KAS: Returns the last date and time (in UTC) the user changed 
   * its password.
   *
   * @return the last date and time (in UTC) the user changed its password.
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public int getLastChangePasswordTime() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return lastChangePasswordTime;
  }

  /**
   * KAS: Returns the maximum lifetime of a ticket issued to this user 
   * (in seconds).
   *
   * @return the maximum lifetime of a ticket issued to this user (in seconds).
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public int getMaxTicketLifetime() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return maxTicketLifetime;
  }

  /**
   * KAS: Returns the number of days a password is valid before it expires.  
   * A value of 0 indicates passwords never expire.
   *
   * @return the number of days for which a password is valid
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public int getDaysToPasswordExpire() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return daysToPasswordExpire;
  }

  /**
   * KAS: Returns the number of failed login attempts this user is allowed 
   * before being locked out.  A value of 0 indicates there is no limit.
   *
   * @return the number of failed login attempts a user is allowed
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public int getFailLoginCount() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return failLoginCount;
  }

  /**
   * KAS: Returns the amount of time (in seconds) a user is locked out when 
   * it exceeds the maximum number of allowable failed login attempts.  
   * A value of 0 indicates an infinite lockout time.
   *
   * @return the number of failed login attempts a user is allowed
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public int getLockTime() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return lockTime;
  }

  /**
   * KAS: Returns the encryption key, in octal form, of this user.  An
   * example of a key in octal form is:    
   * '\040\205\211\241\345\002\023\211'.
   *
   * @return the encryption key
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public String getEncryptionKey() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return encryptionKey;
  }

  /**
   * KAS: Returns the check sum of this user's key.
   *
   * @return the check sum
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public long getKeyCheckSum() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return keyCheckSum;
  }

  /**
   * KAS: Returns the version number of the user's key.
   *
   * @return the key version
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public int getKeyVersion() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return keyVersion;
  }

  /**
   * KAS: Returns the date and time (in UTC) at which the user stops 
   * being locked out. A value of 0 indicates the user is not currently 
   * locked out. If the user is locked out forever, the value 
   * will be equal to -1.
   *
   * @return the date and time (in UTC) at which the user stops being 
   *         locked out
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public int getLockedUntil() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return lockedUntil;
  }

  /**
   * KAS: Returns the date and time at which the user stops being locked out.
   * A value of <code>null</code> indicates the user is not currently locked 
   * out. If the user is locked out forever, the value 
   * <code>getLockedUntil().getTime().getTime()</code> will be equal to -1.
   *
   * @return the date and time at which the user stops being locked out
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public GregorianCalendar getLockedUntilDate() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    if( lockedUntilDate == null ) {
      // make it into a date . . .
      Date d;
      if( lockedUntil == 0 ) {
        lockedUntilDate = null;
      } else if( lockedUntil == -1 ) {
        d = new Date( lockedUntil );
        lockedUntilDate = new GregorianCalendar();
        lockedUntilDate.setTime( d );
      } else {
        d = new Date( ((long) lockedUntil)*1000 );
        lockedUntilDate = new GregorianCalendar();
        lockedUntilDate.setTime( d );
      }
    }
    return lockedUntilDate;
  }

  /////////////// mutators ////////////////////

  /**
   * PTS: Sets how many more groups this user is allowed to create.  
   * -1 indicates unlimited.
   *
   * @param quota    the new group creation quota
   */
  public void setGroupCreationQuota( int quota )
  {
    groupCreationQuota = quota;
  }


  /**
   * PTS: Sets who can list the status (pts examine) of this user.  
   * Valid values are:
   * <ul>
   * <li><code>{@link #USER_OWNER_ACCESS}</code> 
   *           -- only the owner has permission</li>
   * <li><code>{@link #USER_ANYUSER_ACCESS}</code> 
   *           -- any user has permission</li>
   * </ul>
   *
   * @param value    the value of the new list status permission
   * @exception AFSException      if an error occurs in the native code
   * @exception IllegalArgumentException   if an invalud argument is provided
   */
  public void setListStatus( int value ) throws AFSException
  {
    if( (value != this.USER_OWNER_ACCESS) && 
	(value != this.USER_ANYUSER_ACCESS) ) {
	throw new IllegalArgumentException( "Cannot set listStatus to " 
					    + value );
    } else {
	listStatus = value;
    }
  }

  /**
   * PTS: Sets who can list the groups owned (pts listowned) by this user.  
   * Valid values are:
   * <ul>
   * <li><code>{@link #USER_OWNER_ACCESS}</code> 
   *           -- only the owner has permission</li>
   * <li><code>{@link #USER_ANYUSER_ACCESS}</code> 
   *           -- any user has permission</li>
   * </ul>
   *
   * @param value    the value of the new list groups owned permission
   * @exception AFSException      if an error occurs in the native code
   * @exception IllegalArgumentException   if an invalud argument is provided
   */
  public void setListGroupsOwned( int value ) throws AFSException
  {
    if( (value != this.USER_OWNER_ACCESS) && 
	(value != this.USER_ANYUSER_ACCESS) ) {
	throw new IllegalArgumentException( "Cannot set listGroupsOwned to " 
					    + value );
    } else {
	listGroupsOwned = value;
    }
  }

  /**
   * PTS: Sets who can list the groups (pts membership) to which this 
   *      user belongs.  
   * Valid values are:
   * <ul>
   * <li><code>{@link #USER_OWNER_ACCESS}</code> 
   *           -- only the owner has permission</li>
   * <li><code>{@link #USER_ANYUSER_ACCESS}</code> 
   *           -- any user has permission</li>
   * </ul>
   *
   * @param value    the value of the new list membership permission
   * @exception AFSException      if an error occurs in the native code
   * @exception IllegalArgumentException   if an invalud argument is provided
   */
  public void setListMembership( int value ) throws AFSException
  {
    if( (value != this.USER_OWNER_ACCESS) && 
	(value != this.USER_ANYUSER_ACCESS) ) {
	throw new IllegalArgumentException( "Cannot set listMembership to " 
					    + value );
    } else {
	listMembership = value;
    }
  }

  /**
   * KAS: Sets whether or not this user has kas administrative privileges
   *
   * @param setting     whether or not this user has kas 
   *                    administrative privileges
   */
  public void setAdmin( boolean setting )
  {
    if ( setting ) {
	adminSetting = this.ADMIN;
    } else {
	adminSetting = this.NO_ADMIN;
    }
  }

  /**
   * KAS: Sets whether or not TGS will issue tickets for this user
   *
   * @param setting    whether or not TGS will issue tickets for this user
   */
  public void setGrantTickets( boolean setting )
  {
    if ( setting ) {
	tgsSetting = this.GRANT_TICKETS;
    } else {
	tgsSetting = this.NO_GRANT_TICKETS;
    }
  }

  /**
   * KAS: Sets whether or not TGS can use this users ticket for an 
   *      encryption key
   *
   * @param setting     whether or not TGS can use this users ticket for an 
   *                    encryption key
   */
  public void setEncrypt( boolean setting )
  {
    if ( setting ) {
	encSetting = this.ENCRYPT;
    } else {
	encSetting = this.NO_ENCRYPT;
    }
  }

  /**
   * KAS: Sets whether or not the user can change their password
   *
   * @param setting     whether or not the user can change their password
   */
  public void setChangePassword( boolean setting )
  {
    if ( setting ) {
	cpwSetting = this.CHANGE_PASSWORD;
    } else {
	cpwSetting = this.NO_CHANGE_PASSWORD;
    }
  }

  /**
   * KAS: Sets whether or not the user can reuse their password
   *
   * @param setting      whether or not the user can reuse their password
   */
  public void setReusePassword( boolean setting )
  {
    if ( setting ) {
	rpwSetting = this.REUSE_PASSWORD;
    } else {
	rpwSetting = this.NO_REUSE_PASSWORD;
    }
  }

  /**
   * KAS: Sets the date and time the user expires.  
   * A <code>null</code> value indicates the user never exipres.
   *
   * @param expirationDate     the date and time the user expires
   */
  public void setUserExpiration( GregorianCalendar expirationDate )
  {
    userExpirationDate = expirationDate;
    if( expirationDate == null ) {
      userExpiration = -1;
    } else {
      Date d = expirationDate.getTime();
      long millis = d.getTime();
      userExpiration = (int) (millis/((long)1000));
    }
  }

  /**
   * KAS: Sets the maximum lifetime of a ticket issued to this user 
   *      (in seconds).
   *
   * @param seconds    the maximum lifetime of a ticket issued to this user (in seconds).
   */
  public void setMaxTicketLifetime( int seconds )
  {
    maxTicketLifetime = seconds;
  }

  /**
   * KAS: Sets the number of days a password is valid before it expires.  
   * A value of 0 indicates passwords never expire.
   *
   * @param days     the number of days for which a password is valid
   */
  public void setDaysToPasswordExpire( int days )
  {
    daysToPasswordExpire = days;
  }

  /**
   * KAS: Sets the number of failed login attempts this user is allowed before 
   * being locked out.  A value of 0 indicates there is no limit.
   *
   * @param logins      the number of failed login attempts a user is allowed
   */
  public void setFailLoginCount( int logins )
  {
    failLoginCount = logins;
  }

  /**
   * KAS: Sets the amount of time (in seconds) a user is locked out when it 
   * exceeds the maximum number of allowable failed login attempts. 
   * A value of 0 indicates an infinite lockout time.  Any nonzero value gets
   * rounded up to the next highest multiple of 8.5 minutes, and any value over
   * 36 hours gets rounded down to 36 hours.
   *
   * @param seconds      the number of failed login attempts a user is allowed
   */
  public void setLockTime( int seconds )
  {
    lockTime = seconds;
  }

  /**
   * Sets the password of this user to something new.  Sets the key version 
   * to 0 automatically.
   *
   * @param newPassword     the new password for this user
   * @exception AFSException  If an error occurs in the native code
   */
  public void setPassword( String newPassword ) throws AFSException
  {
    this.setPassword( cell.getCellHandle(), name, newPassword );
  }
  
  /////////////// custom information methods ////////////////////

  /**
   * Returns a <code>String</code> representation of this <code>User</code>.  
   * Contains the information fields and groups.
   *
   * @return a <code>String</code> representation of the <code>User</code>
   */
  public String getInfo()
  {
    String r;
    try {
    
	r = "User: " + name;

	if( pts ) {
	    r += ", uid: " + getUID();
	}
	r += "\n";
	r += "\tKAS: " + isKAS() + "\tPTS: " + isPTS() + "\n";

	if( pts ) { 
	    r += "\towner: " + getOwner().getName() + ", uid: " 
		+ getOwner().getUID() + "\n";
	    r += "\tcreator: " + getCreator().getName() + ", uid: " 
		+ getCreator().getUID() + "\n";
	    r += "\tGroup creation quota: " + getGroupCreationQuota() + "\n";
	    r += "\tGroup membership count: " + getGroupMembershipCount() 
		+ "\n";
	    r += "\tList status: " + getListStatus() + "\n";
	    r += "\tList groups owned: " + getListGroupsOwned() + "\n";
	    r += "\tList membership: " + getListMembership() + "\n";
	}
	
	if( kas ) {
	    r += "\tKAS admin status: ";

	    if( isAdmin() ) {
		r += "Yes\n";
	    } else {
		r += "No\n";
	    }

	    r += "\tTGS grant tickets: ";
	    if( willGrantTickets() ) {
	    r += "Yes\n";
	    } else {
		r += "No\n";
	    }
	    
	    r += "\tUse key as encryption key: ";
	    if( canEncrypt() ) {
		r += "Yes\n";
	    } else {
		r += "No\n";
	    }
	    
	
	    r += "\tCan change password: ";
	    
	    if( canChangePassword() ) {
		r += "Yes\n";
	    } else {
		r += "No\n";
	    }
	    
	    r += "\tCan reuse password: ";
	    if( canReusePassword() ) {
		r += "Yes\n";
	    } else {
		r += "No\n";
	    }

	    if( userExpiration != 0 ) {
		r += "\tExpiration date: " 
		    + getUserExpirationDate().getTime() + "\n";
	    } else {
		r += "\tUser never expires\n";
	    }
	    r += "\tLast modified " + getLastModTimeDate().getTime() 
		+ " by " + getLastModName() + "\n";
	    r += "\tLast changed password " 
		+ getLastChangePasswordTimeDate().getTime() + "\n";
	    r += "\tMax ticket lifetime: " + getMaxTicketLifetime() + "\n";
	    r += "\tKey: " + getEncryptionKey() + ", version: " 
		+ getKeyVersion() + ", checksum: " + getKeyCheckSum() + "\n";
	    r += "\tDays till password expires: " + getDaysToPasswordExpire() 
		+ "\n";
	    r += "\tAllowed failed logins: " + getFailLoginCount() + "\n";
	    r += "\tLock time after failed logins: " + getLockTime() + "\n";
	    if( lockedUntil == 0 ) { 
		r += "\tNot locked\n";
	    } else if( getLockedUntilDate().getTime().getTime() == -1 ) {
		r += "\tLocked forever\n";
	    } else {
		r += "\tLocked until: " + getLockedUntilDate().getTime() 
		    + "\n";
	    }      
	    
	}
	if( pts ) {
  
	    r += "\tBelongs to groups: \n";
	    
	    String grps[] = getGroupNames();
	    for( int i = 0; i < grps.length; i++ ) {
		r += "\t\t" + grps[i] + "\n";
	    }
	    
	    r += "\tOwns groups: \n";
	    grps = getGroupsOwnedNames();
	    for( int i = 0; i < grps.length; i++ ) {
		r += "\t\t" + grps[i] + "\n";
	    }
	    
	}
    } catch( AFSException e ) {
	return e.toString();
    }
    return r;
  }

  /**
   * Returns a <code>String</code> containing the <code>String</code> 
   * representations of all the groups to which this user belongs.
   *
   * @return    a <code>String</code> representation of the groups belonged to
   * @see       Group#toString
   */
  public String getInfoGroups() throws AFSException
  {
	String r;
	r = "User: " + name + "\n\n";
	r += "--Member of Groups:--\n";

	Group grps[] = getGroups();
	for( int i = 0; i < grps.length; i++ ) {
	    r += grps[i].getInfo() + "\n";
	}
	return r;
  }

  /**
   * Returns a <code>String</code> containing the <code>String</code> 
   * representations of all the groups that this user owns.
   *
   * @return    a <code>String</code> representation of the groups owned
   * @see       Group#toString
   */
  public String getInfoGroupsOwned() throws AFSException
  {
	String r;
	r = "User: " + name + "\n\n";
	r += "--Owns Groups:--\n";
	Group grps[] = getGroupsOwned();
	for( int i = 0; i < grps.length; i++ ) {
	    r += grps[i].getInfo() + "\n";
	}
	return r;
  }

  /////////////// custom override methods ////////////////////

  /**
   * Compares two User objects respective to their names and does not
   * factor any other attribute.    Alphabetic case is significant in 
   * comparing names.
   *
   * @param     user    The User object to be compared to this User instance
   * 
   * @return    Zero if the argument is equal to this User's name, a
   *		value less than zero if this User's name is
   *		lexicographically less than the argument, or a value greater
   *		than zero if this User's name is lexicographically
   *		greater than the argument
   */
  public int compareTo(User user)
  {
    return this.getName().compareTo(user.getName());
  }

  /**
   * Comparable interface method.
   *
   * @see #compareTo(User)
   */
  public int compareTo(Object obj)
  {
    return compareTo((User)obj);
  }

  /**
   * Tests whether two <code>User</code> objects are equal, based on their 
   * names.
   *
   * @param otherUser   the user to test
   * @return whether the specifed user is the same as this user
   */
  public boolean equals( User otherUser )
  {
    return name.equals(otherUser.getName());
  }

  /**
   * Returns the name of this <CODE>User</CODE>
   *
   * @return the name of this <CODE>User</CODE>
   */
  public String toString()
  {
    return getName();
  }


  /////////////// native methods ////////////////////

  /**
   * Creates the kas and pts entries for a new user.  Pass in 0 for the uid 
   * if pts is to automatically assign the user id.
   *
   * @param cellHandle    the handle of the cell to which the user belongs
   * @see Cell#getCellHandle
   * @param userName      the name of the user to create
   * @param password      the password for the new user
   * @param uid     the user id to assign to the user (0 to have one 
   *                automatically assigned)
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void create( long cellHandle, String userName, 
				       String password, int uid )
	throws AFSException;

  /**
   * Deletes the pts and kas entry for a user.  Deletes this user from the 
   * membership list of the groups to which it belonged, but does not delete 
   * the groups owned by this user.
   *
   * @param cellHandle    the handle of the cell to which the user belongs
   * @see Cell#getCellHandle
   * @param groupName      the name of the user to delete
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void delete( long cellHandle, String userName )
	throws AFSException;

  /**
   * Unlocks a user.
   *
   * @param cellHandle    the handle of the cell to which the user belongs
   * @see Cell#getCellHandle
   * @param groupName      the name of the user to unlock
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void unlock( long cellHandle, String userName )
	throws AFSException;

  /**
   * Fills in the information fields of the provided <code>User</code>.  
   * Fills in values based on the current pts and kas information of the user.
   *
   * @param cellHandle    the handle of the cell to which the user belongs
   * @see Cell#getCellHandle
   * @param name     the name of the user for which to get the information
   * @param user     the <code>User</code> object in which to fill in the 
   *                 information
   * @see User
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void getUserInfo( long cellHandle, String name, 
					    User user ) 
	throws AFSException;

  /**
   * Sets the information values of this AFS user to be the parameter values.  
   * Sets both kas and pts fields.
   *
   * @param cellHandle    the handle of the cell to which the user belongs
   * @see Cell#getCellHandle
   * @param name     the name of the user for which to set the information
   * @param theUser  the <code>User</code> object containing the desired 
   *                 information
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void setUserInfo( long cellHandle, String name, 
					    User theUser ) 
	throws AFSException;

  /**
   * Renames the given user.  Does not update the info fields of the kas entry
   *  -- the calling code is responsible for that.
   *
   * @param cellHandle    the handle of the cell to which the user belongs
   * @see Cell#getCellHandle
   * @param oldName     the name of the user to rename
   * @param newName     the new name for the user
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void rename( long cellHandle, String oldName, 
				       String newName )
	throws AFSException;

  /**
   * Sets the password of the given user.  Sets the key version to 0.
   *
   * @param cellHandle    the handle of the cell to which the user belongs
   * @see Cell#getCellHandle
   * @param userName     the name of the user for which to set the password
   * @param newPassword     the new password for the user
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void setPassword( long cellHandle, String userName, 
					    String newPassword )
	throws AFSException;

  /**
   * Begin the process of getting the groups to which the user belongs.  
   * Returns an iteration ID to be used by subsequent calls to 
   * <code>getUserGroupsNext</code> and <code>getUserGroupsDone</code>.  
   *
   * @param cellHandle    the handle of the cell to which the user belongs
   * @see Cell#getCellHandle
   * @param name          the name of the user for which to get the groups
   * @return an iteration ID
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native long getUserGroupsBegin( long cellHandle, String name )
	throws AFSException;

  /**
   * Returns the next group to which the user belongs.  Returns 
   * <code>null</code> if there are no more groups.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see getUserGroupsBegin
   * @return the name of the next group
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native String getUserGroupsNextString( long iterationId )
	throws AFSException;

  /**
   * Fills the next group object of which the user belongs.  Returns 0 if there
   * are no more groups, != 0 otherwise.
   *
   * @param cellHandle    the handle of the cell to which the users belong
   * @see Cell#getCellHandle
   * @param iterationId   the iteration ID of this iteration
   * @see getUserGroupsBegin
   * @param theGroup   a Group object to be populated with the values of the 
   *                   next group
   * @return 0 if there are no more users, != 0 otherwise
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native int getUserGroupsNext( long cellHandle, 
						 long iterationId, 
						 Group theGroup )
    throws AFSException;

  /**
   * Signals that the iteration is complete and will not be accessed anymore.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see getUserGroupsBegin
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void getUserGroupsDone( long iterationId )
	throws AFSException;

  /**
   * Returns the total number of groups owned by the user.  
   *
   * @param cellHandle    the handle of the cell to which the user belongs
   * @param name          the name of the user for which to get the groups
   * @return total number of groups owned by the user
   * @exception AFSException   If an error occurs in the native code
   * @see Cell#getCellHandle
   */
  protected static native int getGroupsOwnedCount( long cellHandle, String name )
	throws AFSException;

  /**
   * Begin the process of getting the groups that a user or group owns.  
   * Returns an iteration ID to be used by subsequent calls to 
   * <code>getGroupsOwnedNext</code> and <code>getGroupsOwnedDone</code>.  
   *
   * @param cellHandle    the handle of the cell to which the user belongs
   * @see Cell#getCellHandle
   * @param name  the name of the user or group for which to get the groups
   * @return an iteration ID
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native long getGroupsOwnedBegin( long cellHandle, 
						   String name )
	throws AFSException;

  /**
   * Returns the next group the user or group owns.  Returns <code>null</code> 
   * if there are no more groups.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see getGroupsOwnedBegin
   * @return the name of the next group
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native String getGroupsOwnedNextString( long iterationId )
	throws AFSException;

  /**
   * Fills the next group object that the user or group owns.  Returns 0 if 
   * there are no more groups, != 0 otherwise.
   *
   * @param cellHandle    the handle of the cell to which the users belong
   * @see Cell#getCellHandle
   * @param iterationId   the iteration ID of this iteration
   * @see getGroupsOwnedBegin
   * @param theGroup   a Group object to be populated with the values of the 
   *                   next group
   * @return 0 if there are no more users, != 0 otherwise
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native int getGroupsOwnedNext( long cellHandle, 
						  long iterationId, 
						  Group theGroup )
    throws AFSException;

  /**
   * Signals that the iteration is complete and will not be accessed anymore.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see getGroupsOwnedBegin
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void getGroupsOwnedDone( long iterationId )
	throws AFSException;

  /**
   * Reclaims all memory being saved by the user portion of the native library.
   * This method should be called when no more <code>Users</code> are expected
   * to be used.
   */
  protected static native void reclaimUserMemory();
}









