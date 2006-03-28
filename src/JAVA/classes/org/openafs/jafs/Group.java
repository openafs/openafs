/*
 * @(#)Group.java	1.0 6/29/2001
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

import java.util.Vector;
import java.util.Enumeration;
import java.util.ArrayList;
import java.io.Serializable;

/**
 * An abstract representation of an AFS group.  It holds information about 
 * the group, such as what groups it owns.<BR><BR>
 *
 * Constructing an instance of a <code>Group</code> does not mean an actual 
 * AFS group is created in a cell -- usually a <code>Group</code>
 * object is a representation of an already existing AFS group.  If, 
 * however, the <code>Group</code> is constructed with the name of a 
 * group that does not exist in the cell represented by the provided 
 * <code>Cell</code>, a new group with that name can be
 * created in that cell by calling the {@link #create(String, int)} or
 * {@link #create(String)} method. If such a group does already exist when 
 * one of these methods is called, an exception will be thrown.<BR><BR>
 *
 * Each <code>Group</code> object has its own individual set of
 * <code>Group</code>s that it owns and <code>User</code>s that belong
 * to it.  These represents the properties and attributes 
 * of an actual AFS group.
 * <BR><BR>
 *
 * <!--Information on how member values are set-->
 *
 * Associated with an AFS group are many attributes, such as whether or not
 * who is allowed to list the members of this group. The <code>Group</code> 
 * class has many "set" methods to indicate values for these attributes (i.e. 
 * {@link #setListMembership(int)}.  However, in order for these values to be 
 * written to the actual AFS group, the {@link #flushInfo()} method needs to 
 * be called.  This writes all user attributes set through this API to AFS.  
 * This is done to minimize calls through JNI.<BR><BR>
 *
 * <!--Example of how to use class-->
 * The following is a simple example of how to construct and use a 
 * <code>Group</code> object.  It lists the name and owner of a specified 
 * group.
 *
 * <PRE>
 * import org.openafs.jafs.Cell;
 * import org.openafs.jafs.AFSException;
 * import org.openafs.jafs.Partition;
 * import org.openafs.jafs.Group;
 * ...
 * public class ...
 * {
 *   ...
 *   private Cell cell;
 *   private Group group;
 *   ...
 *   public static void main(String[] args) throws Exception
 *   {
 *     String username   = arg[0];
 *     String password   = arg[1];
 *     String cellName   = arg[2];
 *     String groupName  = arg[3];
 * 
 *     token = new Token(username, password, cellName);
 *     cell   = new Cell(token);
 *     group = new Group(groupName, cell);
 *     
 *     System.out.println("Owner of group " + group.getName() + " is " 
 *                        + group.getOwnerName());
 *     ...
 *   }
 *   ...
 * }
 * </PRE>
 *
 */
public class Group implements PTSEntry, Serializable, Comparable
{
  /**
   * Only the owner of the group has access
   */
  public static final int GROUP_OWNER_ACCESS = 0;
  /**
   * Members of the group have access
   */
  public static final int GROUP_GROUP_ACCESS = 1;
  /**
   * Any user has access
   */
  public static final int GROUP_ANYUSER_ACCESS = 2;

  protected Cell cell;
  protected long cellHandle;
  protected String name;
  
  protected int membershipCount;
  protected int nameUID;
  protected int ownerUID;
  protected int creatorUID;

  protected String owner;
  protected String creator;

  /**
   * who is allowed to execute PTS examine for this group.  Valid values are:
   * <ul>
   * <li>{@link #GROUP_OWNER_ACCESS} -- only the owner has permission</li>
   * <li>{@link #GROUP_GROUP_ACCESS} 
   *     -- only members of the group have permission</li>
   * <li>{@link #GROUP_ANYUSER_ACCESS} -- any user has permission</li></ul>
   */
  protected int listStatus;
  /**
   * who is allowed to execute PTS examine for this group.  Valid values are:
   * <ul>
   * <li>{@link #GROUP_OWNER_ACCESS} -- only the owner has permission</li>
   * <li>{@link #GROUP_GROUP_ACCESS} 
   *     -- only members of the group have permission</li>
   * <li>{@link #GROUP_ANYUSER_ACCESS} -- any user has permission</li></ul>
   */
  protected int listGroupsOwned;
  /**
   * who is allowed to execute PTS listowned for this group.  Valid values are:
   * <ul>
   * <li>{@link #GROUP_OWNER_ACCESS} -- only the owner has permission</li>
   * <li>{@link #GROUP_GROUP_ACCESS} 
   *     -- only members of the group have permission</li>
   * <li>{@link #GROUP_ANYUSER_ACCESS} -- any user has permission</li></ul>
   */
  protected int listMembership;
  /**
   * who is allowed to execute PTS adduser for this group.   Valid values are:
   * <ul>
   * <li>{@link #GROUP_OWNER_ACCESS} -- only the owner has permission</li>
   * <li>{@link #GROUP_GROUP_ACCESS} 
   *     -- only members of the group have permission</li>
   * <li>{@link #GROUP_ANYUSER_ACCESS} -- any user has permission</li></ul>
   */
  protected int listAdd;
  /**
   * who is allowed to execute PTS removeuser for this group.   Valid 
   * values are:
   * <ul>
   * <li>{@link #GROUP_OWNER_ACCESS} -- only the owner has permission</li>
   * <li>{@link #GROUP_GROUP_ACCESS} 
   *     -- only members of the group have permission</li>
   * <li>{@link #GROUP_ANYUSER_ACCESS} -- any user has permission</li></ul>
   */
  protected int listDelete;

  protected ArrayList members;
  protected ArrayList memberNames;
  protected ArrayList groupsOwned;
  protected ArrayList groupsOwnedNames;

  /**
   * Whether or not the information fields of this group have been filled.
   */
  protected boolean cachedInfo;

  /**
   * Constructs a new <code>Group</code> object instance given the name 
   * of the AFS group and the AFS cell, represented by 
   * <CODE>cell</CODE>, to which it belongs.   This does not actually
   * create a new AFS group, it just represents one.
   * If <code>name</code> is not an actual AFS group, exceptions
   * will be thrown during subsequent method invocations on this 
   * object, unless the {@link #create(String, int)} or {@link #create(String)}
   * method is explicitly called to create it.  
   *
   * @param name      the name of the group to represent
   * @param cell       the cell to which the group belongs.
   * @exception AFSException      If an error occurs in the native code
   */
  public Group( String name, Cell cell ) throws AFSException
  {
    this.name = name;
    this.cell = cell;
    cellHandle = cell.getCellHandle();

    members = null;
    memberNames = null;
    groupsOwned = null;
    groupsOwnedNames = null;
    cachedInfo = false;
  }

  /**
   * Constructs a new <code>Group</code> object instance given the name 
   * of the AFS group and the AFS cell, represented by 
   * <CODE>cell</CODE>, to which it belongs.   This does not actually
   * create a new AFS group, it just represents one.
   * If <code>name</code> is not an actual AFS group, exceptions
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
   * @param name               the name of the group to represent
   * @param cell               the cell to which the group belongs.
   * @param preloadAllMembers  true will ensure all object members are 
   *                           set upon construction;
   *                           otherwise members will be set upon access, 
   *                           which is the default behavior.
   * @exception AFSException      If an error occurs in the native code
   * @see #refresh
   */
  public Group( String name, Cell cell, boolean preloadAllMembers ) 
      throws AFSException
  {
    this(name, cell);
    if (preloadAllMembers) refresh(true);
  }
  
  /**
   * Creates a blank <code>Group</code> given the cell to which the group
   * belongs. Other methods cvan then be used to fill the fields of this 
   * blank object.
   *
   * @exception AFSException      If an error occurs in the native code
   * @param cell       the cell to which the group belongs.
   */
  Group( Cell cell ) throws AFSException
  {
    this( null, cell );
  }

  /*-------------------------------------------------------------------------*/

  /**
   * Creates the PTS entry for a new group in this cell.  Automatically assigns
   * a group id.
   *
   * @param ownerName      the owner of this group
   */    
  public void create( String ownerName ) throws AFSException
  {
    this.create( ownerName, 0 );
  }

  /**
   * Creates the PTS entry for a new group in this cell.
   *
   * @param ownerName      the owner of this group
   * @param gid       the group id to assign to the new group
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void create( String ownerName, int gid ) throws AFSException
  {
    Group.create( cell.getCellHandle(), name, ownerName, gid );
  }

  /**
   * Deletes the PTS entry for a group in this cell. Deletes this group 
   * from the membership list of the user that belonged to it, but does not 
   * delete the groups owned by this group.  Also nullifies the Java object.
   *
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void delete() throws AFSException
  {
    Group.delete( cell.getCellHandle(), name );

    cell = null;
    name = null;
    owner = null;
    creator = null;
    members = null;
    memberNames = null;
    groupsOwned = null;
    groupsOwnedNames = null;
    try {
      finalize();
    } catch( Throwable t ) {
      throw new AFSException( t.getMessage() );
    }
  }

  /**
   * Flushes the current information of this <code>Group</code> object to disk.
   * This will update the information of the actual AFS group to match the 
   * settings that have been modified in this <code>Group</code> object.  
   * This function must be called before any changes made to the information 
   * fields of this group will be seen by the AFS system.
   *
   * @exception AFSException      If an error occurs in the native code
   */
  public void flushInfo() throws AFSException
  {
    Group.setGroupInfo( cell.getCellHandle(), name, this );
  }

  /**
   * Add the specified member to this group. 
   *
   * @param userName    the <code>User</code> object to add
   * @exception AFSException  If an error occurs in the native code
   */
  public void addMember( User theUser ) throws AFSException
  {
    String userName = theUser.getName(); 

    Group.addMember( cell.getCellHandle(), name, userName );

    // add to cache
    if( memberNames != null ) {
      memberNames.add( userName );
    }
    if( members != null ) {
      members.add( new User( userName, cell ) );
    }
  }

  /**
   * Remove the specified member from this group.
   * @param userName    the <code>User</code> object to remove
   * @exception AFSException  If an error occurs in the native code
   */
  public void removeMember( User theUser ) throws AFSException
  {
    String userName = theUser.getName();
    Group.removeMember( cell.getCellHandle(), name, userName );

    // remove from cache
    if( memberNames != null ) {
      memberNames.remove( memberNames.indexOf(userName) );
      memberNames.trimToSize();
    }
    if( members != null && members.indexOf(theUser) > -1) {
      members.remove( members.indexOf(theUser) );
      members.trimToSize();
    }
  }

  /**
   * Change the owner of this group.
   *
   * @param ownerName    the new owner <code>User</code> object
   * @exception AFSException  If an error occurs in the native code
   */
  public void changeOwner( User theOwner ) throws AFSException
  {
    String ownerName = theOwner.getName();

    Group.changeOwner( cell.getCellHandle(), name, ownerName );

    if( cachedInfo ) {
      owner = ownerName;
    }
  }

  /**
   * Change the owner of this group.
   *
   * @param ownerName    the new owner <code>Group</code> object
   * @exception AFSException  If an error occurs in the native code
   */
  public void changeOwner( Group theOwner ) throws AFSException
  {
    String ownerName = theOwner.getName();
    Group.changeOwner( cell.getCellHandle(), name, ownerName );
    if( cachedInfo ) {
      owner = ownerName;
    }
  }

  /**
   * Change the name of this group.
   *
   * @param newName    the new name for this group
   * @exception AFSException  If an error occurs in the native code
   */
  public void rename( String newName ) throws AFSException
  {
    Group.rename( cell.getCellHandle(), name, newName );
    name = newName;
  }

  /**
   * Refreshes the properties of this Group object instance with values from 
   * the AFS group it represents.  All properties that have been initialized 
   * and/or accessed will be renewed according to the values of the AFS group 
   * this <code>Group</code> object instance represents.
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
   * Refreshes the properties of this Group object instance with values from 
   * the AFS group it represents.  If <CODE>all</CODE> is <CODE>true</CODE> 
   * then <U>all</U> of the properties of this Group object instance will be 
   * set, or renewed, according to the values of the AFS group it represents, 
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
    if( all || cachedInfo ) {
      refreshInfo();
    }
    if( all || groupsOwned != null ) {
      refreshGroupsOwned();
    }
    if( all || groupsOwnedNames != null ) {
      refreshGroupsOwnedNames();
    }
    if( all || members != null ) {
      refreshMembers();
    }
    if( all || memberNames != null ) {
      refreshMemberNames();
    }
  }

  /**
   * Refreshes the information fields of this <code>Group</code> to reflect 
   * the current state of the AFS group.  Does not refresh the members that 
   * belong to the group, nor the groups the group owns.
   *
   * @exception AFSException      If an error occurs in the native code
   */
  protected void refreshInfo() throws AFSException
  {
    cachedInfo = true;
    Group.getGroupInfo( cell.getCellHandle(), name, this );
  }

  /**
   * Refreshes the current information about the <code>User</code> objects 
   * belonging to this group.  Does not refresh the information fields of 
   * the group or groups owned.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshMembers() throws AFSException
  {
    User currUser;

    long iterationID = Group.getGroupMembersBegin( cell.getCellHandle(), name );

    members = new ArrayList();

    currUser = new User( cell );
    while( Group.getGroupMembersNext( cellHandle, iterationID, currUser ) 
	   != 0 ) {
      members.add( currUser );
      currUser = new User( cell );
    } 
    
    Group.getGroupMembersDone( iterationID );
  }
  
  /**
   * Refreshes the current information about the names of members belonging 
   * to this group.  Does not refresh the information fields of the group 
   * or groups owned.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshMemberNames() throws AFSException
  {
    String currName;
    long iterationID = Group.getGroupMembersBegin( cell.getCellHandle(), name );

    memberNames = new ArrayList();

    while( ( currName = Group.getGroupMembersNextString( iterationID ) ) 
	   != null ) {
      memberNames.add( currName );
    } 
    Group.getGroupMembersDone( iterationID );
  }
  
  /**
   * Refreshes the current information about the <code>Group</code> objects the
   * group owns.  Does not refresh the information fields of the group or 
   * members.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshGroupsOwned() throws AFSException
  {
    Group currGroup;

    long iterationID = User.getGroupsOwnedBegin( cell.getCellHandle(), name );

    groupsOwned = new ArrayList();

    currGroup = new Group( cell );
    while( User.getGroupsOwnedNext( cellHandle, iterationID, currGroup ) 
	   != 0 ) {
      groupsOwned.add( currGroup );
      currGroup = new Group( cell );
    } 
    
    User.getGroupsOwnedDone( iterationID );
  }

  /**
   * Refreshes the current information about the names of groups the group 
   * owns.  Does not refresh the information fields of the group or members.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshGroupsOwnedNames() throws AFSException
  {
    String currName;

    long iterationID = User.getGroupsOwnedBegin( cell.getCellHandle(), name );

    groupsOwnedNames = new ArrayList();
    while( ( currName = User.getGroupsOwnedNextString( iterationID ) ) 
	   != null ) {
      groupsOwnedNames.add( currName );
    } 
    User.getGroupsOwnedDone( iterationID );
  }

  /**
   * Adds an access control list entry for some AFS directory for this group.
   *
   * @param directory  the full path of the place in the AFS file system 
   *                   for which to add an entry
   * @param read  whether or not to allow read access to this user
   * @param write  whether or not to allow write access to this user
   * @param lookup  whether or not to allow lookup access to this user
   * @param delete  whether or not to allow deletion access to this user
   * @param insert  whether or not to allow insertion access to this user
   * @param lock  whether or not to allow lock access to this user
   * @param admin  whether or not to allow admin access to this user
   * @exception AFSException  If an error occurs in the native code
  public void setACL( String directory, boolean read, boolean write, boolean lookup, boolean delete, boolean insert, boolean lock, boolean admin )
    throws AFSException
  {
    Cell.setACL( directory, name, read, write, lookup, delete, insert, lock, admin );
  }
   */

  //////////////////////  accessors: ///////////////

  /**
   * Returns the name of this group.
   *
   * @return the name of this group
   */
  public String getName()
  {
    return name;
  }

  /**
   * Returns the numeric AFS id of this group.
   *
   * @return the AFS id of this group
   * @exception AFSException  If an error occurs in the native code
   */
  public int getUID() throws AFSException
  {
    if( !cachedInfo ) {
	refreshInfo();
    }
    return nameUID;
  }

  /**
   * Returns the Cell this group belongs to.
   *
   * @return the Cell this group belongs to
   */
  public Cell getCell()
  {
    return cell;
  }

  /**
   * Returns an array of the <code>User</code> object members of this group.
   *
   * @return      an array of the members of this group
   * @exception AFSException  If an error occurs in the native code
   */
  public User[] getMembers() throws AFSException
  {
    if( members == null ) {
      refreshMembers();
    }
    return (User[]) members.toArray( new User[members.size()] );
  }

  /**
   * Returns an array of the member names of this group.
   *
   * @return      an array of the member names of this group
   * @exception AFSException  If an error occurs in the native code
   */
  public String[] getMemberNames() throws AFSException
  {
    if( memberNames == null ) {
      refreshMemberNames();
    }
    return (String[]) memberNames.toArray( new String[memberNames.size()] );
  }

  /**
   * Returns an array of the <code>Group</code> objects this group owns.
   *
   * @return     an array of the <code>Groups</code> this group owns
   * @exception AFSException  If an error occurs in the native code
   */
  public Group[] getGroupsOwned() throws AFSException
  {
    if( groupsOwned == null ) {
      refreshGroupsOwned();
    }
    return (Group[]) groupsOwned.toArray( new Group[groupsOwned.size()] );
  }

  /**
   * Returns an array of the group names this group owns.
   * Contains <code>String</code> objects.
   *
   * @return     an array of the group names this group owns
   * @exception AFSException  If an error occurs in the native code
   */
  public String[] getGroupsOwnedNames() throws AFSException
  {
    if( groupsOwnedNames == null ) {
      refreshGroupsOwnedNames();
    }
    return (String[]) 
	groupsOwnedNames.toArray(new String[groupsOwnedNames.size()] );
  }

  /**
   * Returns the number of members of this group.
   *
   * @return the membership count
   * @exception AFSException  If an error occurs in the native code
   */
  public int getMembershipCount() throws AFSException
  {
    if( !cachedInfo ) {
	refreshInfo();
    }
    return membershipCount;
  }

  /**
   * PTS: Returns the owner of this group in the form of a {@link PTSEntry}.
   *
   * <P>The returning object could be either a {@link User} or {@link Group};
   * to determine what type of object the {@link PTSEntry} represents,
   * call the {@link PTSEntry#getType()} method.
   *
   * @return the owner of this group
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
   * PTS: Returns the creator of this group in the form of a {@link PTSEntry}.
   *
   * <P>The returning object could be either a {@link User} or {@link Group};
   * to determine what type of object the {@link PTSEntry} represents,
   * call the {@link PTSEntry#getType()} method.
   *
   * @return the creator of this group
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
   * <P>This method will always return {@link PTSEntry#PTS_GROUP}.
   *
   * @return  the type of PTSEntry this object represents 
              (will always return {@link PTSEntry#PTS_GROUP})
   * @see PTSEntry
   * @see PTSEntry#getType()
   */
  public short getType()
  {
    return PTSEntry.PTS_GROUP;
  }

  /**
   * Returns who can list the status (pts examine) of this group.  
   * Valid values are:
   * <ul>
   * <li><code>{@link #GROUP_OWNER_ACCESS}</code> 
   *           -- only the owner has permission</li>
   * <li><code>{@link #GROUP_GROUP_ACCESS}</code> 
   *           -- only members of the group have permission</li>
   * <li><code>{@link #GROUP_ANYUSER_ACCESS}</code> 
   *           -- any user has permission</li>
   * </ul>
   *
   * @return the status listing permission
   * @exception AFSException  If an error occurs in the native code
   */
  public int getListStatus() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return listStatus;
  }

  /**
   * Returns who can list the groups owned (pts listowned) by this group.  
   * Valid values are:
   * <ul>
   * <li><code>{@link #GROUP_OWNER_ACCESS}</code>
   *           -- only the owner has permission</li>
   * <li><code>{@link #GROUP_GROUP_ACCESS}</code> 
   *           -- only members of the group have permission</li>
   * <li><code>{@link #GROUP_ANYUSER_ACCESS}</code> 
   *           -- any user has permission</li>
   * </ul>
   *
   * @return the groups owned listing permission
   * @exception AFSException  If an error occurs in the native code
   */
  public int getListGroupsOwned() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return listGroupsOwned;
  }

  /**
   * Returns who can list the users (pts membership) that belong to this group.  
   * Valid values are:
   * <ul>
   * <li><code>{@link #GROUP_OWNER_ACCESS}</code> 
   *           -- only the owner has permission</li>
   * <li><code>{@link #GROUP_GROUP_ACCESS}</code> 
   *           -- only members of the group have permission</li>
   * <li><code>{@link #GROUP_ANYUSER_ACCESS}</code> 
   *           -- any user has permission</li>
   * </ul>
   *
   * @return the membership listing permission
   * @exception AFSException  If an error occurs in the native code
   */
  public int getListMembership() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return listMembership;
  }

  /**
   * Returns who can add members (pts adduser) to this group.
   * Valid values are:
   * <ul>
   * <li><code>{@link #GROUP_OWNER_ACCESS}</code> 
   *           -- only the owner has permission</li>
   * <li><code>{@link #GROUP_GROUP_ACCESS}</code> 
   *           -- only members of the group have permission</li>
   * <li><code>{@link #GROUP_ANYUSER_ACCESS}</code> 
   *           -- any user has permission</li>
   * </ul>
   *
   * @return the member adding permission
   * @exception AFSException  If an error occurs in the native code
   */
  public int getListAdd() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return listAdd;
  }

  /**
   * Returns who can delete members (pts removemember) from this group.
   * Valid values are:
   * <ul>
   * <li><code>{@link #GROUP_OWNER_ACCESS}</code> 
   *           -- only the owner has permission</li>
   * <li><code>{@link #GROUP_GROUP_ACCESS}</code> 
   *           -- only members of the group have permission</li>
   * <li><code>{@link #GROUP_ANYUSER_ACCESS}</code> 
   *           -- any user has permission</li>
   * </ul>
   *
   * @return the member deleting permission
   * @exception AFSException  If an error occurs in the native code
   */
  public int getListDelete() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return listDelete;
  }

    ///////////////////  mutators: //////////////////////

  /**
   * Sets who can list the status (pts examine) of this group.  
   * Valid values are:
   * <ul>
   * <li><code>{@link #GROUP_OWNER_ACCESS}</code>
   *           -- only the owner has permission</li>
   * <li><code>{@link #GROUP_GROUP_ACCESS}</code> 
   *           -- only members of the group have permission</li>
   * <li><code>{@link #GROUP_ANYUSER_ACCESS}</code> 
   *           -- any user has permission</li>
   * </ul>
   *
   * @param value    the value of the new list membership permission
   * @exception AFSException      if an error occurs in the native code
   * @exception IllegalArgumentException   if an invalud argument is provided
   */
  public void setListStatus( int value ) throws AFSException
  {
    if( (value != Group.GROUP_OWNER_ACCESS) && 
	(value != Group.GROUP_GROUP_ACCESS) && 
	(value != Group.GROUP_ANYUSER_ACCESS) ) {
	throw new IllegalArgumentException( "Cannot set listStatus to " 
					    + value );
    } else {
	listStatus = value;
    }
  }

  /**
   * Sets who can list the groups owned (pts listowned) by this group.  
   * Valid values are:
   * <ul>
   * <li><code>{@link #GROUP_OWNER_ACCESS}</code> 
   *           -- only the owner has permission</li>
   * <li><code>{@link #GROUP_GROUP_ACCESS}</code> 
   *           -- only members of the group have permission</li>
   * <li><code>{@link #GROUP_ANYUSER_ACCESS}</code> 
   *           -- any user has permission</li>
   * </ul>
   *
   * @param value    the value of the new list membership permission
   * @exception AFSException      if an error occurs in the native code
   * @exception IllegalArgumentException   if an invalud argument is provided
   */
  public void setListGroupsOwned( int value ) throws AFSException
  {
    if( (value != Group.GROUP_OWNER_ACCESS) && 
	(value != Group.GROUP_GROUP_ACCESS) && 
	(value != Group.GROUP_ANYUSER_ACCESS) ) {
	throw new IllegalArgumentException( "Cannot set listGroupsOwned to " 
					    + value );
    } else {
	listGroupsOwned = value;
    }
  }

  /**
   * Sets who can list the users (pts membership) that belong to this group.  
   * Valid values are:
   * <ul>
   * <li><code>{@link #GROUP_OWNER_ACCESS}</code> 
   *           -- only the owner has permission</li>
   * <li><code>{@link #GROUP_GROUP_ACCESS}</code> 
   *           -- only members of the group have permission</li>
   * <li><code>{@link #GROUP_ANYUSER_ACCESS}</code> 
   *           -- any user has permission</li>
   * </ul>
   *
   * @param value    the value of the new list membership permission
   * @exception AFSException      if an error occurs in the native code
   * @exception IllegalArgumentException   if an invalud argument is provided
   */
  public void setListMembership( int value ) throws AFSException
  {
    if( (value != Group.GROUP_OWNER_ACCESS) && 
	(value != Group.GROUP_GROUP_ACCESS) && 
	(value != Group.GROUP_ANYUSER_ACCESS) ) {
	throw new IllegalArgumentException( "Cannot set listMembership to " 
					    + value );
    } else {
	listMembership = value;
    }
  }

  /**
   * Sets who can add members (pts adduser) to this group.
   * Valid values are:
   * <ul>
   * <li><code>{@link #GROUP_OWNER_ACCESS}</code> 
   *           -- only the owner has permission</li>
   * <li><code>{@link #GROUP_GROUP_ACCESS}</code> 
   *           -- only members of the group have permission</li>
   * <li><code>{@link #GROUP_ANYUSER_ACCESS}</code> 
   *           -- any user has permission</li>
   * </ul>
   *
   * @param value    the value of the new list membership permission
   * @exception AFSException      if an invalid value is provided
   */
  public void setListAdd( int value ) throws AFSException
  {
    if( (value != Group.GROUP_OWNER_ACCESS) && 
	(value != Group.GROUP_GROUP_ACCESS) && 
	(value != Group.GROUP_ANYUSER_ACCESS) ) {
	throw new IllegalArgumentException( "Cannot set listAdd to " + value );
    } else {
	listAdd = value;
    }
  }

  /**
   * Sets who can delete members (pts removemember) from this group.
   * Valid values are:
   * <ul>
   * <li><code>{@link #GROUP_OWNER_ACCESS}</code> 
   *           -- only the owner has permission</li>
   * <li><code>{@link #GROUP_GROUP_ACCESS}</code> 
   *           -- only members of the group have permission</li>
   * <li><code>{@link #GROUP_ANYUSER_ACCESS}</code> 
   *           -- any user has permission</li>
   * </ul>
   *
   * @param value    the value of the new list membership permission
   * @exception AFSException      if an invalid value is provided
   */
  public void setListDelete( int value ) throws AFSException
  {
    if( (value != Group.GROUP_OWNER_ACCESS) && 
	(value != Group.GROUP_GROUP_ACCESS) && 
	(value != Group.GROUP_ANYUSER_ACCESS) ) {
	throw new IllegalArgumentException( "Cannot set listDelete to " 
					    + value );
    } else {
	listDelete = value;
    }
  }

  /////////////// information methods ////////////////////

  /**
   * Returns a <code>String</code> representation of this <code>Group</code>. 
   * Contains the information fields and members.
   *
   * @return a <code>String</code> representation of the <code>Group</code>
   */
  public String getInfo()
  {
    String r;
    try {
	r = "Group: " + getName() + ", uid: " + getUID() + "\n";
	r += "\towner: " + getOwner().getName() + ", uid: " + getOwner().getUID() + "\n";
	r += "\tcreator: " + getCreator().getName() + ", uid: " 
	    + getCreator().getUID() + "\n";
	r += "\tMembership count: " + getMembershipCount() + "\n";
	r += "\tList status: " + getListStatus() + "\n";
	r += "\tList groups owned: " + getListGroupsOwned() + "\n";
	r += "\tList membership: " + getListMembership() + "\n";
	r += "\tAdd members: " + getListAdd() + "\n";
	r += "\tDelete members: " + getListDelete() + "\n";

	r += "\tGroup members: \n";
	String names[] = getMemberNames();
	for( int i = 0; i < names.length; i++ ) {
	    r += "\t\t" + names[i] + "\n";
	}

	r += "\tOwns groups: \n";
	names = getGroupsOwnedNames();
	for( int i = 0; i < names.length; i++ ) {
	    r += "\t\t" + names[i] + "\n";
	}
	return r;
    } catch ( AFSException e ) {
	return e.toString();
    }
  }

  /////////////// custom override methods ////////////////////

  /**
   * Compares two Group objects respective to their names and does not
   * factor any other attribute.    Alphabetic case is significant in 
   * comparing names.
   *
   * @param     group    The Group object to be compared to this Group instance
   * 
   * @return    Zero if the argument is equal to this Group's name, a
   *		value less than zero if this Group's name is
   *		lexicographically less than the argument, or a value greater
   *		than zero if this Group's name is lexicographically
   *		greater than the argument
   */
  public int compareTo(Group group)
  {
    return this.getName().compareTo(group.getName());
  }

  /**
   * Comparable interface method.
   *
   * @see #compareTo(Group)
   */
  public int compareTo(Object obj)
  {
    return compareTo((Group)obj);
  }

  /**
   * Tests whether two <code>Group</code> objects are equal, based on their 
   * names.
   *
   * @param otherGroup   the Group to test
   * @return whether the specifed Group is the same as this Group
   */
  public boolean equals( Group otherGroup )
  {
    return name.equals( otherGroup.getName() );
  }

  /**
   * Returns the name of this <CODE>Group</CODE>
   *
   * @return the name of this <CODE>Group</CODE>
   */
  public String toString()
  {
    return getName();
  }

  /////////////// native methods ////////////////////

  /**
   * Creates the PTS entry for a new group.  Pass in 0 for the uid if PTS is to
   * automatically assign the group id.
   *
   * @param cellHandle    the handle of the cell to which the group belongs
   * @see Cell#getCellHandle
   * @param groupName      the name of the group to create
   * @param ownerName      the owner of this group
   * @param gid     the group id to assign to the group (0 to have one 
   *                automatically assigned)
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void create( long cellHandle, String groupName, 
				       String ownerName, int gid )
	throws AFSException;

  /**
   * Deletes the PTS entry for a group.  Deletes this group from the 
   * membership list of the users that belonged to it, but does not delete 
   * the groups owned by this group.
   *
   * @param cellHandle    the handle of the cell to which the group belongs
   * @see Cell#getCellHandle
   * @param groupName      the name of the group to delete
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void delete( long cellHandle, String groupName )
	throws AFSException;

  /**
   * Fills in the information fields of the provided <code>Group</code>.  
   * Fills in values based on the current PTS information of the group.
   *
   * @param cellHandle    the handle of the cell to which the group belongs
   * @see Cell#getCellHandle
   * @param name     the name of the group for which to get the information
   * @param group     the <code>Group</code> object in which to fill in the 
   *                  information
   * @see Group
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void getGroupInfo( long cellHandle, String name, 
					     Group group ) 
	throws AFSException;

  /**
   * Sets the information values of this AFS group to be the parameter values.
   *
   * @param cellHandle    the handle of the cell to which the user belongs
   * @see Cell#getCellHandle
   * @param name     the name of the user for which to set the information
   * @param theGroup   the group object containing the desired information
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void setGroupInfo( long cellHandle, String name, 
					     Group theGroup ) 
	throws AFSException;

  /**
   * Begin the process of getting the users that belong to the group.  Returns 
   * an iteration ID to be used by subsequent calls to 
   * <code>getGroupMembersNext</code> and <code>getGroupMembersDone</code>.  
   *
   * @param cellHandle    the handle of the cell to which the group belongs
   * @see Cell#getCellHandle
   * @param name          the name of the group for which to get the members
   * @return an iteration ID
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native long getGroupMembersBegin( long cellHandle, 
						    String name )
	throws AFSException;

  /**
   * Returns the next members that belongs to the group.  Returns 
   * <code>null</code> if there are no more members.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see getGroupMembersBegin
   * @return the name of the next member
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native String getGroupMembersNextString( long iterationId ) 
	throws AFSException;

  /**
   * Fills the next user object belonging to that group.  Returns 0 if there
   * are no more users, != 0 otherwise.
   *
   * @param cellHandle    the handle of the cell to which the users belong
   * @see Cell#getCellHandle
   * @param iterationId   the iteration ID of this iteration
   * @see getGroupMembersBegin
   * @param theUser   a User object to be populated with the values of the 
   *                  next user
   * @return 0 if there are no more users, != 0 otherwise
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native int getGroupMembersNext( long cellHandle, 
						   long iterationId, 
						   User theUser )
    throws AFSException;

  /**
   * Signals that the iteration is complete and will not be accessed anymore.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see getGroupMembersBegin
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void getGroupMembersDone( long iterationId )
	throws AFSException;

  /**
   * Adds a user to the specified group. 
   *
   * @param cellHandle    the handle of the cell to which the group belongs
   * @see Cell#getCellHandle
   * @param groupName          the name of the group to which to add a member
   * @param userName      the name of the user to add
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void addMember( long cellHandle, String groupName, 
					  String userName )
	throws AFSException;

  /**
   * Removes a user from the specified group. 
   *
   * @param cellHandle    the handle of the cell to which the group belongs
   * @see Cell#getCellHandle
   * @param groupName          the name of the group from which to remove a 
   *                           member
   * @param userName      the name of the user to remove
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void removeMember( long cellHandle, String groupName, 
					     String userName )
	throws AFSException;

  /**
   * Change the owner of the specified group. 
   *
   * @param cellHandle    the handle of the cell to which the group belongs
   * @see Cell#getCellHandle
   * @param groupName          the name of the group of which to change the 
   *                           owner
   * @param ownerName      the name of the new owner
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void changeOwner( long cellHandle, String groupName, 
					    String ownerName )
	throws AFSException;

  /**
   * Change the name of the specified group. 
   *
   * @param cellHandle    the handle of the cell to which the group belongs
   * @see Cell#getCellHandle
   * @param oldGroupName          the old name of the group
   * @param newGroupName      the new name for the group
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void rename( long cellHandle, String oldGroupName, 
				       String newGroupName )
	throws AFSException;

  /**
   * Reclaims all memory being saved by the group portion of the native 
   * library.
   * This method should be called when no more <code>Groups</code> are expected
   * to be used.
   */
  protected static native void reclaimGroupMemory();
}





