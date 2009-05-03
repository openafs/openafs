/*
 * @(#)Cell.java	1.0 6/29/2001
 *
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

package org.openafs.jafs;

import java.util.ArrayList;
import java.util.GregorianCalendar;
import java.util.Date;

/**
 * An abstract representation of an AFS cell.  It holds information about 
 * the cell, such as what users, groups, and servers exist in the cell.
 * <BR><BR>
 *
 * Constructing a <code>Cell</code> object does not mean a new cell is
 * created in the AFS file system -- on the contrary, a <code>Cell</code>
 * object must be a representation of an already existing AFS cell.  There
 * is no way to create a new AFS cell through this API.  See 
 * <a href="http://www.openafs.org">OpenAFS.org</a> for information on how
 * to create a new cell.<BR><BR>
 * 
 * The construction of a <code>Cell</code> object acts as an entry point
 * for authentication into the AFS system.  Thus, when you construct a 
 * <code>Cell</code>, you must pass in an authenticated <code>Token</code>
 * of a user in the AFS cell that the <code>Cell</code> represents.  You
 * will be authenticated as the user represented by <code>token</code> and 
 * you will only be allowed to perform actions that the user is
 * authorized to perform.  You must construct a <code>Cell</code> before 
 * attempting to construct any other object in this package, since the
 * other objects all require a <code>Cell</code> object on construction,
 * either directly or indirectly.<BR><BR>
 *
 * Note that to successfully construct a <code>Cell</code> object, the 
 * code must be running on a machine with a running AFS client, and the
 * cell this object is to represent must have an entry in the client's
 * CellServDB file.<BR><BR>
 * 
 * Each <code>Cell</code> object has its own individual set of
 * <code>Server</code>s, <code>User</code>s, and <code>Group</code>s.
 * This represents the properties and attributes of an actual AFS cell.
 *
 * If an error occurs during a method call, an 
 * <code>AFSException</code> will be thrown.  This class is the Java
 * equivalent of errors thrown by AFS; see {@link AFSException}
 * for a complete description.<BR><BR>
 *
 * <!--Example of how to use class-->
 * The following is a simple example of how to construct and use a 
 * <code>Cell</code> object. It shows how a <code>Cell</code> can be used to 
 * get an abstract representation of an AFS server, and how it can obtain an 
 * array of <code>User</code> objects, each of which is an abstract 
 * representation of an AFS user.<BR><BR>
 * 
 * <PRE>
 * import org.openafs.jafs.AFSException;
 * import org.openafs.jafs.Cell;
 * import org.openafs.jafs.Partition;
 * import org.openafs.jafs.Server;
 * import org.openafs.jafs.Token;
 * import org.openafs.jafs.User;
 * ...
 * public class ...
 * {
 *   ...
 *   private Cell cell;
 *   private Server server;
 *   private Token token;
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
 *     server = cell.getServer(serverName);
 *
 *     User[] users = cell.getUsers();
 *     ...
 *   }
 *   ...
 * }
 * </PRE>
 *
 */
public class Cell implements java.io.Serializable
{
  protected ArrayList users;
  protected ArrayList userNames;
  protected ArrayList groups;
  protected ArrayList groupNames;
  protected ArrayList servers;
  protected ArrayList serverNames;

  protected String name;
  protected long cellHandle;
  protected Token token;

  protected int maxGroupID;
  protected int maxUserID;

  protected GregorianCalendar tokenExpiration;

  protected boolean cachedInfo;

  /**
   * Constructs a new <CODE>Cell</CODE> object instance given 
   * the <code>Token</code> that should represents an authenticated user
   * with administrative access.  In order to get full access to the cell, 
   * it is best that the <code>Token</code> provided have administrative 
   * privileges.
   *
   * @param token      the user's authenticated token
   * @exception AFSException  If an error occurs in the native code
   */
  public Cell( Token token ) 
      throws AFSException
  {
    this.token = token;
    this.name  = token.getCellName();

    cellHandle = getCellHandle( name, token.getHandle() );
//System.out.println("cellHandle: " + cellHandle);
    users = null;
    userNames = null;
    groups = null;
    groupNames = null;
    servers = null;
    serverNames = null;
    cachedInfo = false;
    tokenExpiration = null;
  }

  /**
   * Constructs a new <CODE>Cell</CODE> object instance given 
   * the <code>Token</code> that should represents an authenticated user
   * with administrative access.  In order to get full access to the cell, 
   * it is best that the <code>Token</code> provided have administrative 
   * privileges.
   *
   * <P> This constructor is ideal for point-in-time representation and 
   * transient applications.  It ensures all data member values are set 
   * and available without calling back to the filesystem at the first 
   * request for them.  Use the {@link #refresh()} method to address any 
   * coherency concerns.
   *
   * @param token              the user's authenticated token
   * @param preloadAllMembers  true will ensure all object members are 
   *                           set upon construction; otherwise members 
   *                           will be set upon access, which is the default 
   *                           behavior.
   * @exception AFSException      If an error occurs in the native code
   * @see #refresh
   */
  public Cell( Token token, boolean preloadAllMembers ) 
      throws AFSException
  {
    this(token);
    if (preloadAllMembers) refresh(true);
  }

  /**
   * Refreshes the properties of this Cell object instance with values 
   * from the AFS cell it represents.  All properties that have been 
   * initialized and/or accessed will be renewed according to the values 
   * of the AFS cell this Cell object instance represents.
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
    this.refresh(false);
  }

  /**
   * Refreshes the properties of this Cell object instance with values 
   * from the AFS cell it represents.  If <CODE>all</CODE> is <CODE>true</CODE>
   * then <U>all</U> of the properties of this Cell object instance will be 
   * set, or renewed, according to the values of the AFS cell it represents, 
   * disregarding any previously set properties.
   *
   * <P> Thus, if <CODE>all</CODE> is <CODE>false</CODE> then properties that 
   * are currently set will be refreshed and properties that are not set will 
   * remain uninitialized. See {@link #refresh()} for more information.
   *
   * @param all   if true set or renew all object properties; otherwise 
   *              renew all set properties
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  protected void refresh(boolean all) throws AFSException
  {
// System.err.print("<r");
    if( all || (users != null) ) {
// System.err.print("u");
        refreshUsers();
    }
    if( all || (userNames != null) ) {
// System.err.print("U");
        refreshUserNames();
    }
    if( all || (groups != null) ) {
// System.err.print("g");
        refreshGroups();
    }
    if( all || (groupNames != null) ) {
// System.err.print("G");
        refreshGroupNames();
    }
    if( all || (servers != null) ) {
// System.err.print("s");
        refreshServers();
    }
    if( all || (serverNames != null) ) {
// System.err.print("S");
        refreshServerNames();
    }
    if( all || cachedInfo ) {
// System.err.print("i");
        refreshInfo();
    }
// System.err.println(">");
  }

  /**
   * Obtains the expiration time of the token being used by this 
   * <code>Cell</code> object.  Does not actually refresh the token; that is,
   * once a token is obtained, its expiration time will not change.  This
   * method is mostly for consistency with the other methods.  It is mainly 
   * used for getting the token information once; after that, it need not
   * be called again.
   *
   * @exception AFSException  If an error occurs in the native code     
   */
  private void refreshTokenExpiration() throws AFSException
  {
    long expTime;

    expTime = token.getExpiration();

    tokenExpiration = new GregorianCalendar();
    long longTime = expTime*1000;
    Date d = new Date( longTime );
    tokenExpiration.setTime( d );
  }

  /**
   * Sets all the information fields of this <code>Cell</code> object, 
   * such as max group and user ids, to trheir most current values.
   *
   * @exception AFSException  If an error occurs in the native code     
   */
  protected void refreshInfo() throws AFSException
  {
    maxGroupID = getMaxGroupID( cellHandle );
    maxUserID = getMaxUserID( cellHandle );
    cachedInfo = true;
  }

  /**
   * Obtains the most current list of <code>User</code> objects of this cell.  
   * Finds all users that currently have a kas and/or pts entry for this cell.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshUsers() throws AFSException
  {
    User currUser;
    users = new ArrayList();
    int hammer = 0;
    long iterationId = 0;
    int r = 1;
    boolean authorized = false;
    currUser = new User( this );

    // get kas entries
    try {
      iterationId = getKasUsersBegin( cellHandle );

      while( r != 0 ) {
	try {
	  if (authorized) {
	    users.add( currUser );
	    currUser = new User( this );
	  }
	  r = getKasUsersNext( cellHandle, iterationId, currUser );
	  authorized = true;
	} catch (AFSException e) {
	  System.err.println("ERROR Cell::refreshUsers():kas (User: " 
			     + currUser.getName() + ") -> " + e.getMessage());
	  authorized = false;
	  //if (org.openafs.jafs.ErrorCodes.isPermissionDenied(e.getErrorCode())) 
	  //r = 0;
	  if (++hammer > 5) r = 0;
	}
      } 
      getKasUsersDone( iterationId );
    } catch (AFSException e) {
      r = 0;	/* XXX should only do this on ADMCLIENTCELLKASINVALID ??? */
    }

    //take the union with the pts entries
    iterationId = getPtsUsersBegin( cellHandle );
    authorized = false;
    r = 1;
    while( r != 0 ) {
      try {
        if (authorized) {
          if( !users.contains( currUser ) ) {
            users.add( currUser );
          }
          currUser = new User( this );
        }
        r = getPtsOnlyUsersNext( cellHandle, iterationId, currUser );
        authorized = true;
      } catch (AFSException e) {
        System.err.println("ERROR Cell::refreshUsers():pts (User: " 
			   + currUser.getName() + ") -> " + e.getMessage());
        authorized = false;
        //if (org.openafs.jafs.ErrorCodes.isPermissionDenied(e.getErrorCode())) 
	// r = 0;
	if (++hammer > 5) r = 0;
      }
    } 
    getPtsUsersDone( iterationId );

  }

  /**
   * Obtains the most current list of user names of this cell.  Finds
   * all users that currently have a kas and/or pts entry for this cell.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshUserNames() throws AFSException
  {
    String currName;
    long iterationId ;
    userNames = new ArrayList();

// System.err.print("<q");
    // get kas entries
	try {
    iterationId = getKasUsersBegin( cellHandle );
    while( ( currName = getKasUsersNextString( iterationId )) != null ) {
      userNames.add( currName );
    } 
    getKasUsersDone( iterationId );
	} catch (AFSException e) {
// System.err.print("getKasUsers(x) failed");
//	e.printStackTrace();
	}
    
    //take the union with the pts entries
    iterationId = Cell.getPtsUsersBegin( cellHandle );
    while( ( currName = getPtsOnlyUsersNextString( iterationId, cellHandle ) ) 
	   != null ) {
      if( !userNames.contains( currName ) ) {
        userNames.add( currName );
      }
    } 
    getPtsUsersDone( iterationId );
// System.err.println(">");
  }


  /**
   * Obtains the most current list of <code>Group</code> objects of this cell.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshGroups() throws AFSException
  {
    Group currGroup;
    int hammer = 0;

    long iterationId = getGroupsBegin( cellHandle );
    
    groups = new ArrayList();
    
    currGroup = new Group( this );
    boolean authorized = false;
    int r = 1;
    while( r != 0 ) {
      try {
        if (authorized) {
          groups.add( currGroup );
          currGroup = new Group( this );
        }
        r = getGroupsNext( cellHandle, iterationId, currGroup );
        authorized = true;
      } catch (AFSException e) {
	e.printStackTrace();

//        System.err.println("ERROR Cell::refreshGroups() (Group: " 
//			   + currGroup.getName() + ") -> " + e.getMessage());
        authorized = false;
        //if (org.openafs.jafs.ErrorCodes.isPermissionDenied(e.getErrorCode())) 
	// r = 0;
	if (++hammer > 5) r = 0;
      }
    } 
    Cell.getGroupsDone( iterationId );
  }

  /**
   * Obtains the most current list of group names of this cell.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshGroupNames() throws AFSException
  {
    String currName;

    long iterationId = getGroupsBegin( cellHandle );
    
    groupNames = new ArrayList();
    while( ( currName = getGroupsNextString( iterationId ) ) != null ) {
        groupNames.add( currName );
    } 
    getGroupsDone( iterationId );
  }

  /**
   * Obtains the most current list of <code>Server</code> objects of this cell.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshServers() throws AFSException
  {
    Server currServer;
    int hammer = 0;

    long iterationId = getServersBegin( cellHandle );
   
    servers = new ArrayList();
    
    currServer = new Server( this );
    boolean authorized = false;
    int r = 1;
    while( r != 0 ) {
      try {
        if (authorized) {
          //System.out.println("[Java] Cell::refreshServers() -> adding server: " 
		//	     + currServer.getName());
          servers.add( currServer );
          currServer = new Server( this );
        }
        r = getServersNext( cellHandle, iterationId, currServer );
//System.out.println("[Java] Cell::refreshServers() -> r: " + r);
        authorized = true;
      } catch (AFSException e) {
        System.err.println("ERROR Cell::refreshServers() (Server: " 
			   + currServer.getName() + ") -> " + e.getMessage());
        authorized = false;
        //if (e.getErrorCode() == org.openafs.jafs.ErrorCodes.PERMISSION_DENIED) 
        // r = 0;
	if (++hammer > 5) r = 0;
      }
    } 
    getServersDone( iterationId );
  }

  /**
   * Obtains the most current list of server names of this cell.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshServerNames() 
      throws AFSException
  {
    String currName;

    long iterationId = getServersBegin( cellHandle );

    serverNames = new ArrayList();
    while( ( currName = getServersNextString( iterationId ) ) != null ) {
        serverNames.add( currName );
    } 
    getServersDone( iterationId );
  }

  /**
   * Unauthenticates this </code>Token</code> object associated with this 
   * <code>Cell</code> and deletes all of its stored information.  This 
   * method should only be called when this <code>Cell</code> or any of the
   * objects constructed using this <code>Cell</code> will not be used 
   * anymore.  Note that this does not delete the actual AFS cell that this 
   * <code>Cell</code> object represents; it merely closes the 
   * representation.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  public void close() throws AFSException
  {
    Cell.closeCell( cellHandle );
    token.close();
    users = null;
    userNames = null;
    groups = null;
    groupNames = null;
    servers = null;
    serverNames = null;
    cachedInfo = false;
    tokenExpiration = null;
  }

  //////////////// ACCESSORS ////////////////////////

  /**
   * Retrieves the <CODE>User</CODE> object (which is an abstract 
   * representation of an actual AFS user) designated by <code>name</code>.
   * If a user by that name does not actually exist in AFS in the cell
   * represented by this object, an {@link AFSException} will be
   * thrown.
   *
   * @exception AFSException  If an error occurs in the native code
   * @exception NullPointerException  If <CODE>name</CODE> is 
   *                                  <CODE>null</CODE>.
   * @param name    the name of the user to retrieve
   * @return <CODE>User</CODE> designated by <code>name</code>.
   */
  public User getUser(String name) throws AFSException
  {
    if (name == null) throw new NullPointerException();
    User user = new User(name, this);
    return user;
  }

  /**
   * Returns the total number of users who are registered with KAS and PTS,
   * without duplicates.  If a user has a KAS entry and not a PTS entry,
   * it will still be counted.  Conversely, if a user has a PTS entry and
   * not KAS, it too will be counted.  Effectively it is a non-duplicate
   * union of KAS and PTS user entries.
   * 
   * <P>If the total list of users or user names have already been 
   * collected (see {@link #getUsers()}), then the returning value will be 
   * calculated based upon the current list.  Otherwise, KAS and PTS will be
   * explicitly queried for the information.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return a <code>User</code> array of the users of the cell.
   * @see #getUsers()
   * @see #getUserNames()
   */
  public int getUserCount() throws AFSException
  {
    if( users != null ) {
      return users.size();
    } else if( userNames != null ) {
      return userNames.size();
    } else {
      int k = getKasUserCount(cellHandle);
      int p = getPtsOnlyUserCount(cellHandle);
      return k + p;
    }
  }

  /**
   * Retrieves an array containing all of the <code>User</code> objects 
   * associated with this <code>Cell</code>, each of which are an abstract 
   * representation of an actual user of the AFS cell.  After this method
   * is called once, it saves the array of <code>User</code>s and returns
   * that saved array on subsequent calls, until the {@link #refresh()} method
   * is called and a more current list is obtained.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return a <code>User</code> array of the users of the cell.
   */
  public User[] getUsers() throws AFSException
  {
    if( users == null ) refreshUsers();
    return (User[]) users.toArray( new User[users.size()] );
  }

  /**
   * Returns an array containing a subset of the <code>User</code> objects
   * associated with this <code>Cell</code>, each of which is an abstract
   * representation of an actual AFS user of the AFS cell.  The subset
   * is a point-in-time list of users (<code>User</code> objects
   * representing AFS users) starting at the complete array's index of
   * <code>startIndex</code> and containing up to <code>length</code>
   * elements.
   *
   * If <code>length</code> is larger than the number of remaining elements, 
   * respective to <code>startIndex</code>, then this method will
   * ignore the remaining positions requested by <code>length</code> and 
   * return an array that contains the remaining number of elements found in 
   * this cell's complete array of users.
   *
   * <P>This method is especially useful when managing iterations of very
   * large lists.  {@link #getUserCount()} can be used to determine if
   * iteration management is practical.
   *
   * <P>This method does not save the resulting data and therefore 
   * queries AFS for each call.
   *
   * <P><B>Note:</B> PTS-only users are collected before KAS users
   * and therefore will always, if PTS-only users exist, be within the
   * lowest range of this cell's complete list of users.  PTS and KAS
   * users are joined in a non-duplicating union and are consequently
   * treated as a single list of users, thus <code>startIndex</code>
   * does not necessarily indicate the first KAS user.
   *
   * <P><B>Example:</B> If there are more than 50,000 users within this cell
   * then only render them in increments of 10,000.
   * <PRE>
   * ...
   *   User[] users;
   *   if (cell.getUserCount() > 50000) {
   *     int index = 0;
   *     int length = 10000;
   *     while (index < cell.getUserCount()) {
   *       users = cell.<B>getUsers</B>(index, length);
   *       for (int i = 0; i < users.length; i++) {
   *         ...
   *       }
   *       index += length;
   *       ...
   *     }
   *   } else {
   *     users = cell.getUsers();
   *     for (int i = 0; i < users.length; i++) {
   *       ...
   *     }
   *   }
   * ...
   * </PRE>
   *
   * @param startIndex  the base zero index position at which the subset array 
   *                    should start from, relative to the complete list of 
   *                    elements present in AFS.
   * @param length      the number of elements that the subset should contain
   * @return a subset array of users in this cell
   * @exception AFSException  If an error occurs in the native code
   * @see #getUserCount()
   * @see #getUserNames(int, int)
   * @see #getUsers()
   */
  public User[] getUsers(int startIndex, int length) throws AFSException
  {
    User[] users  = new User[length];
    User currUser = new User( this );
    int ptsOnlyCount = getPtsOnlyUserCount(cellHandle);
    long iterationID = 0;
    int indexPTS = 0;
    int indexKAS = 0;

    if (startIndex < ptsOnlyCount) {
      int i = 0;
      iterationID = getPtsUsersBegin(cellHandle);
      while( getPtsOnlyUsersNext( cellHandle, iterationID, currUser ) != 0 &&
             indexPTS < length )
      {
        if (i >= startIndex) {
          users[indexPTS] = currUser;
          currUser = new User( this );
          indexPTS++;
        }
      } 
      getPtsUsersDone( iterationID );

      if (indexPTS < length) {
        startIndex = 0;
        length -= indexPTS;
      } else {
        return users;
      }
    } else {
      startIndex -= (ptsOnlyCount - 1);
    }

    iterationID = getKasUsersBeginAt( cellHandle, startIndex );
    while( getKasUsersNext(cellHandle, iterationID, currUser ) != 0 && 
           indexKAS < length )
    {
      users[indexKAS] = currUser;
      currUser = new User( this );
      indexKAS++;
    } 
    getKasUsersDone( iterationID );

    if (indexKAS < length) {
      User[] u = new User[indexKAS + indexPTS];
      System.arraycopy(users, 0, u, 0, u.length);
      return u;
    } else {
      return users;
    }
  }

  /**
   * Retrieves an array containing all of the names of users 
   * associated with this <code>Cell</code>. After this method
   * is called once, it saves the array of <code>String</code>s and returns
   * that saved array on subsequent calls, until the {@link #refresh()} method
   * is called and a more current list is obtained.
   *
   * <P>This method is especially useful when managing iterations of
   * large lists.  {@link #getUserCount()} can be used to determine if
   * iteration management is practical.  In comparison to {@link #getUsers()},
   * this method has yielded an average performance advantage of approximately
   * 82% at 10K users; this statistic, however, strictly compares the response
   * time of each method and understands that the {@link #getUsers()} method
   * will return an array of populated <code>User</code> objects, whereas this
   * method will return an array of <code>String</code> names.
   * <BR><BR>
   *
   * @return an <code>String</code> array of the user names of the cell.
   * @exception AFSException  If an error occurs in the native code
   */
  public String[] getUserNames() throws AFSException
  {
// System.err.print("<u");
    if( userNames == null ) refreshUserNames();
// System.err.println(">");
    return (String[]) userNames.toArray( new String[userNames.size()] );
  }

  /**
   * Returns an array containing a subset of the names of users
   * associated with this <code>Cell</code>.  The subset
   * is a point-in-time list of users (<code>String</code> names
   * of AFS users) starting at the complete array's index of
   * <code>startIndex</code> and containing up to <code>length</code>
   * elements.
   *
   * If <code>length</code> is larger than the number of remaining elements, 
   * respective to <code>startIndex</code>, then this method will
   * ignore the remaining positions requested by <code>length</code> and 
   * return an array that contains the remaining number of elements found in 
   * this cell's complete array of users.
   *
   * <P>This method is especially useful when managing iterations of very
   * large lists.  {@link #getUserCount()} can be used to determine if
   * iteration management is practical.
   *
   * <P>This method does not save the resulting data and therefore 
   * queries AFS for each call.
   *
   * <P><B>Note:</B> PTS-only users are collected before KAS users
   * and therefore will always, if PTS-only users exist, be within the
   * lowest range of this cell's complete list of users.  PTS and KAS
   * users are joined in a non-duplicating union and are consequently
   * treated as a single list of users, thus <code>startIndex</code>
   * does not necessarily indicate the first KAS user.
   *
   * <P><B>Example:</B> If there are more than 50,000 users within this cell
   * then only render them in increments of 10,000.
   * <PRE>
   * ...
   *   String[] users;
   *   if (cell.getUserCount() > 50000) {
   *     int index = 0;
   *     int length = 10000;
   *     while (index < cell.getUserCount()) {
   *       users = cell.<B>getUserNames</B>(index, length);
   *       for (int i = 0; i < users.length; i++) {
   *         ...
   *       }
   *       index += length;
   *       ...
   *     }
   *   } else {
   *     users = cell.getUserNames();
   *     for (int i = 0; i < users.length; i++) {
   *       ...
   *     }
   *   }
   * ...
   * </PRE>
   *
   * @param startIndex  the base zero index position at which the subset array
   *                    should start from, relative to the complete list of 
   *                    elements present in AFS.
   * @param length      the number of elements that the subset should contain
   * @return a subset array of user names in this cell
   * @exception AFSException  If an error occurs in the native code
   * @see #getUserCount()
   * @see #getUserNames()
   * @see #getUsers(int, int)
   */
  public String[] getUserNames(int startIndex, int length) 
    throws AFSException
  {
    String[] users  = new String[length];
    String currUser;
    int ptsOnlyCount = getPtsOnlyUserCount(cellHandle);
    long iterationID = 0;
    int indexPTS = 0;
    int indexKAS = 0;

// System.err.print("<U");
    if (startIndex < ptsOnlyCount) {
      int i = 0;
      iterationID = getPtsUsersBegin(cellHandle);
      while( (currUser = getPtsOnlyUsersNextString( iterationID, cellHandle ))
             != null && indexPTS < length ) {
        if (i >= startIndex) {
          users[indexPTS] = currUser;
          indexPTS++;
        }
      } 
      getPtsUsersDone( iterationID );

      if (indexPTS < length) {
        startIndex = 0;
        length -= indexPTS;
      } else {
        return users;
      }
    } else {
      startIndex -= (ptsOnlyCount - 1);
    }

    iterationID = getKasUsersBeginAt( cellHandle, startIndex );
    while( (currUser = getKasUsersNextString( iterationID )) != null &&
            indexKAS < length ) {
      users[indexKAS] = currUser;
      indexKAS++;
    } 
    getKasUsersDone( iterationID );

// System.err.println(">");
    if (indexKAS < length) {
      String[] u = new String[indexKAS + indexPTS];
      System.arraycopy(users, 0, u, 0, u.length);
      return u;
    } else {
      return users;
    }
  }

  /**
   * Retrieves the <CODE>Group</CODE> object (which is an abstract 
   * representation of an actual AFS group) designated by <code>name</code>.
   * If a group by that name does not actually exist in AFS in the cell
   * represented by this object, an {@link AFSException} will be
   * thrown.
   *
   * @exception AFSException  If an error occurs in the native code
   * @exception NullPointerException  If <CODE>name</CODE> is 
   *                                  <CODE>null</CODE>.
   * @param name the name of the group to retrieve
   * @return <CODE>Group</CODE> designated by <code>name</code>.
   */
  public Group getGroup(String name) throws AFSException
  {
    if (name == null) throw new NullPointerException();
    Group group = new Group(name, this);
    group.refresh(true);
    return group;
  }

  /**
   * Returns the total number of groups associated with this Cell.
   * 
   * <P>If the total list of groups or group names have already been 
   * collected (see {@link #getGroups()}), then the returning value will be 
   * calculated based upon the current list.  Otherwise, PTS will be
   * explicitly queried for the information.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return a <code>User</code> array of the users of the cell.
   * @see #getGroups()
   * @see #getGroupNames()
   */
  public int getGroupCount() throws AFSException
  {
    if( groups != null ) {
      return groups.size();
    } else if( groupNames != null ) {
      return groupNames.size();
    } else {
      return getGroupCount(cellHandle);
    }
  }

  /**
   * Retrieves an array containing all of the <code>Group</code> objects 
   * associated with this <code>Cell</code>, each of which are an abstract 
   * representation of an actual group of the AFS cell.  After this method
   * is called once, it saves the array of <code>Group</code>s and returns
   * that saved array on subsequent calls, until the {@link #refresh()} method
   * is called and a more current list is obtained.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return a <code>Group</code> array of the groups of the cell.
   */
  public Group[] getGroups() throws AFSException
  {
    if( groups == null ) refreshGroups();
    return (Group[]) groups.toArray( new Group[groups.size()] );
  }

  /**
   * Returns an array containing a subset of the <code>Group</code> objects
   * associated with this <code>Cell</code>, each of which is an abstract
   * representation of an actual AFS group of the AFS cell.  The subset
   * is a point-in-time list of groups (<code>Group</code> objects
   * representing AFS groups) starting at the complete array's index of
   * <code>startIndex</code> and containing up to <code>length</code>
   * elements.
   *
   * If <code>length</code> is larger than the number of remaining elements, 
   * respective to <code>startIndex</code>, then this method will
   * ignore the remaining positions requested by <code>length</code> and 
   * return an array that contains the remaining number of elements found in 
   * this cell's complete array of groups.
   *
   * <P>This method is especially useful when managing iterations of very
   * large lists.  {@link #getGroupCount()} can be used to determine if
   * iteration management is practical.
   *
   * <P>This method does not save the resulting data and therefore 
   * queries AFS for each call.
   *
   * <P><B>Example:</B> If there are more than 50,000 groups within this cell
   * then only render them in increments of 10,000.
   * <PRE>
   * ...
   *   Group[] groups;
   *   if (cell.getGroupCount() > 50000) {
   *     int index = 0;
   *     int length = 10000;
   *     while (index < cell.getGroupCount()) {
   *       groups = cell.<B>getGroups</B>(index, length);
   *       for (int i = 0; i < groups.length; i++) {
   *         ...
   *       }
   *       index += length;
   *       ...
   *     }
   *   } else {
   *     groups = cell.getGroups();
   *     for (int i = 0; i < groups.length; i++) {
   *       ...
   *     }
   *   }
   * ...
   * </PRE>
   *
   * @param startIndex  the base zero index position at which the subset array 
   *                    should start from, relative to the complete list of 
   *                    elements present in AFS.
   * @param length      the number of elements that the subset should contain
   * @return a subset array of groups in this cell
   * @exception AFSException  If an error occurs in the native code
   * @see #getGroupCount()
   * @see #getGroupNames(int, int)
   * @see #getGroups()
   */
  public Group[] getGroups(int startIndex, int length) throws AFSException
  {
    Group[] groups  = new Group[length];
    Group currGroup = new Group( this );
    int i = 0;

    long iterationID = getGroupsBeginAt( cellHandle, startIndex );

    while( getGroupsNext( cellHandle, iterationID, currGroup ) != 0 
	   && i < length ) {
      groups[i] = currGroup;
      currGroup = new Group( this );
      i++;
    } 
    getGroupsDone( iterationID );
    if (i < length) {
      Group[] v = new Group[i];
      System.arraycopy(groups, 0, v, 0, i);
      return v;
    } else {
      return groups;
    }
  }

  /**
   * Retrieves an array containing all of the names of groups
   * associated with this <code>Cell</code>. After this method
   * is called once, it saves the array of <code>String</code>s and returns
   * that saved array on subsequent calls, until the {@link #refresh()} method
   * is called and a more current list is obtained.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return a <code>String</code> array of the group names of the cell.
   */
  public String[] getGroupNames() throws AFSException
  {
    if( groupNames == null ) refreshGroupNames();
    return (String[]) groupNames.toArray( new String[groupNames.size()] );
  }

  /**
   * Returns an array containing a subset of the names of groups
   * associated with this <code>Cell</code>.  The subset
   * is a point-in-time list of groups (<code>String</code> names
   * of AFS groups) starting at the complete array's index of
   * <code>startIndex</code> and containing up to <code>length</code>
   * elements.
   *
   * If <code>length</code> is larger than the number of remaining elements, 
   * respective to <code>startIndex</code>, then this method will
   * ignore the remaining positions requested by <code>length</code> and 
   * return an array that contains the remaining number of elements found in 
   * this cell's complete array of groups.
   *
   * <P>This method is especially useful when managing iterations of very
   * large lists.  {@link #getGroupCount()} can be used to determine if
   * iteration management is practical.
   *
   * <P>This method does not save the resulting data and therefore 
   * queries AFS for each call.
   *
   * <P><B>Example:</B> If there are more than 50,000 groups within this cell
   * then only render them in increments of 10,000.
   * <PRE>
   * ...
   *   String[] groups;
   *   if (cell.getGroupCount() > 50000) {
   *     int index = 0;
   *     int length = 10000;
   *     while (index < cell.getGroupCount()) {
   *       groups = cell.<B>getGroupNames</B>(index, length);
   *       for (int i = 0; i < groups.length; i++) {
   *         ...
   *       }
   *       index += length;
   *       ...
   *     }
   *   } else {
   *     groups = cell.getGroupNames();
   *     for (int i = 0; i < groups.length; i++) {
   *       ...
   *     }
   *   }
   * ...
   * </PRE>
   *
   * @param startIndex  the base zero index position at which the subset array 
   *                    should start from, relative to the complete list of 
   *                    elements present in AFS.
   * @param length      the number of elements that the subset should contain
   * @return a subset array of group names in this cell
   * @exception AFSException  If an error occurs in the native code
   * @see #getGroupCount()
   * @see #getGroups(int, int)
   * @see #getGroupNames()
   */
  public String[] getGroupNames(int startIndex, int length) 
    throws AFSException
  {
    String[] groups  = new String[length];
    String currGroup;
    int i = 0;

    long iterationID = getGroupsBeginAt( cellHandle, startIndex );

    while( (currGroup = getGroupsNextString( iterationID )) != null &&
            i < length )
    {
      groups[i] = currGroup;
      i++;
    } 
    getGroupsDone( iterationID );
    if (i < length) {
      String[] v = new String[i];
      System.arraycopy(groups, 0, v, 0, i);
      return v;
    } else {
      return groups;
    }
  }

  /**
   * Retrieves the <CODE>Server</CODE> object (which is an abstract 
   * representation of an actual AFS server) designated by <code>name</code>.
   * If a group by that name does not actually exist in AFS in the cell
   * represented by this object, an {@link AFSException} will be
   * thrown.
   *
   * @exception AFSException  If an error occurs in the native code
   * @exception NullPointerException  If <CODE>name</CODE> is 
   *                                  <CODE>null</CODE>.
   * @param name the name of the server to retrieve
   * @return <CODE>Server</CODE> designated by <code>name</code>.
   */
  public Server getServer(String name) 
      throws AFSException
  {
    if (name == null) throw new NullPointerException();
    Server server = new Server(name, this);
    server.refresh(true);
    return server;
  }

  /**
   * Returns the total number of servers associated with this Cell.
   * 
   * <P>If the total list of servers or server names have already been 
   * collected (see {@link #getServers()}), then the returning value will be 
   * calculated based upon the current list.  Otherwise, AFS will be
   * explicitly queried for the information.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return a <code>User</code> array of the users of the cell.
   * @see #getServers()
   * @see #getServerNames()
   */
  public int getServerCount() throws AFSException
  {
    if( servers != null ) {
      return servers.size();
    } else if( serverNames != null ) {
      return serverNames.size();
    } else {
      return getServerCount(cellHandle);
    }
  }

  /**
   * Retrieves an array containing all of the <code>Server</code> objects 
   * associated with this <code>Cell</code>, each of which are an abstract 
   * representation of an actual server of the AFS cell.  After this method
   * is called once, it saves the array of <code>Server</code>s and returns
   * that saved array on subsequent calls, until the {@link #refresh()} method
   * is called and a more current list is obtained.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return an <code>Server</code> array of the servers of the cell.
   */
  public Server[] getServers() throws AFSException
  {
    if ( servers == null ) refreshServers();
    return (Server[]) servers.toArray( new Server[servers.size()] );
  }

  /**
   * Retrieves an array containing all of the names of servers
   * associated with this <code>Cell</code>. After this method
   * is called once, it saves the array of <code>String</code>s and returns
   * that saved array on subsequent calls, until the {@link #refresh()} method
   * is called and a more current list is obtained.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return a <code>String</code> array of the servers of the cell.
   */
  public String[] getServerNames() throws AFSException
  {
    if ( serverNames == null ) refreshServerNames();
    return (String[]) serverNames.toArray( new String[serverNames.size()] );
  }

  /**
   * Returns the maximum group ID that's been used within the cell.  
   * The next auto-assigned group ID will be one less (more negative) 
   * than this amount.   After this method is called once, it saves the 
   * max group id and returns that id on subsequent calls, until the 
   * {@link #refresh()} method is called and a more current id is obtained.
   *
   * @return an integer representing the maximum group ID
   * @exception AFSException  If an error occurs in the native code
   */
  public int getMaxGroupID() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return maxGroupID;

  }

  /**
   * Returns the maximum user ID that's been used within the cell.  
   * The next auto-assigned user ID will be one greater (more positive) 
   * than this amount.   After this method is called once, it saves the 
   * max user id and returns that id on subsequent calls, until the 
   * {@link #refresh()} method is called and a more current id is obtained.
   *
   * @return an integer representing the maximum user ID
   * @exception AFSException  If an error occurs in the native code
   */
  public int getMaxUserID() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return maxUserID;
  }

  /**
   * Returns the expiration time of the authentication token being used 
   * by this <code>Cell</code> object.  After this time, this 
   * <code>Cell</code> object will no longer be authorized to perform
   * actions requiring administrative authority.
   *
   * @return expiration time of the token
   * @exception AFSException  If an error occurs in the native code
   */
  public GregorianCalendar getTokenExpiration() throws AFSException
  {
    if( tokenExpiration == null ) refreshTokenExpiration();
    return tokenExpiration;
  }

  /**
   * Returns the cell handle of this cell.
   *
   * @return the cell handle
   * @exception AFSException  If an error occurs in the native code
   */
  public long getCellHandle() throws AFSException
  {
    return cellHandle;
  }

  /**
   * Returns the name of this cell.
   *
   * @return the cell name
   */
  public String getName()
  {
    return name;
  }

  /**
   * Sets the maximum group ID that's been used within the cell.  The next 
   * auto-assigned group ID will be one less (more negative) than this amount.
   *
   * @param maxID an integer representing the maximum group ID
   * @exception AFSException  If an error occurs in the native code
   */
  public void setMaxGroupID( int maxID ) throws AFSException
  {
    setMaxGroupID( cellHandle, maxID );
    maxGroupID = maxID;
  }

  /**
   * Sets the maximum user ID that's been used within the cell.  The next 
   * auto-assigned user ID will be one greater (more positive) than this 
   * amount.
   *
   * @param maxID an integer representing the maximum user ID
   * @exception AFSException  If an error occurs in the native code
   */
  public void setMaxUserID( int maxID ) throws AFSException
  {
    setMaxUserID( cellHandle, maxID );
    maxUserID = maxID;
  }

  /////////////// information methods ////////////////////

  /**
   * Returns a <code>String</code> representation of this <code>Cell</code>.  
   * Contains the cell name followed by the names of its users and groups.
   *
   * @return    a <code>String</code> representation of this <code>Cell</code>
   */
  public String getInfo()
  {
    String r = "Cell: " + name + "\n\n";
String x = null;
    try {
        r += "\tMax group ID: " + getMaxGroupID() + "\n";
        r += "\tMax user ID: " + getMaxUserID() + "\n";
        r += "\tToken expiration: " + getTokenExpiration().getTime() + "\n";
    } catch( AFSException e ) {
        return e.toString();
    }

    String[] servs;
    String[] usrs;
    String[] grps;
    try {
x = "getUserNames";
        usrs = getUserNames();
x = "getGroupNames";
        grps = getGroupNames();
x = "getServerNames";
        servs = getServerNames();

    } catch( Exception e ) {
System.err.println("getInfo: exception in " + x + ": " + e.toString());
e.printStackTrace();
        return e.toString();
    }

    r += "--Users--\n";

    for( int i = 0; i < usrs.length; i++ ) {

        r += usrs[i] + "\n"; 
    }

    r += "\n--Groups--\n";

    for( int i = 0; i < grps.length; i++ ) {

        r += grps[i] + "\n"; 
    }

    r += "\n--Servers--\n";

    for( int i = 0; i < servs.length; i++ ) {

        r += servs[i] + "\n"; 
    }

    return r;
  }

  /**
   * Returns a <code>String</code> containing the <code>String</code> 
   * representations of all the users of this <code>Cell</code>.
   *
   * @return    a <code>String</code> representation of the users
   * @see       User#getInfo
   */
  public String getInfoUsers() throws AFSException
  {
    String r;

    r = "Cell: " + name + "\n\n";
    r += "--Users--\n";

    User usrs[] = getUsers();
    for( int i = 0; i < usrs.length; i++ ) {
        r += usrs[i].getInfo() + "\n";
    }

    return r;
  }

  /**
   * Returns a <code>String</code> containing the <code>String</code> 
   * representations of all the groups of this <code>Cell</code>.
   *
   * @return    a <code>String</code> representation of the groups
   * @see       Group#getInfo
   */
  public String getInfoGroups() throws AFSException
  {
    String r;

    r = "Cell: " + name + "\n\n";
    r += "--Groups--\n";

    Group grps[] = getGroups();
    for( int i = 0; i < grps.length; i++ ) {
        r += grps[i].getInfo() + "\n";
    }
    return r;
  }

  /**
   * Returns a <code>String</code> containing the <code>String</code> 
   * representations of all the servers of this <code>Cell</code>.
   *
   * @return    a <code>String</code> representation of the servers
   * @see       Server#getInfo
   */
  public String getInfoServers() 
      throws AFSException
  {
    String r;
    r = "Cell: " + name + "\n\n";
    r += "--Servers--\n";
    Server[] servs = getServers();
    for( int i = 0; i < servs.length; i++ ) {
        r += servs[i].getInfo() + "\n";
    }
    return r;
  }

  /////////////// override methods ////////////////////

  /**
   * Tests whether two <code>Cell</code> objects are equal, based on their 
   * names.  Does not test whether the objects are actually the same
   * representational instance of the AFS cell.
   *
   * @param otherCell   the <code>Cell</code> to test
   * @return whether the specifed user is the same as this user
   */
  public boolean equals( Cell otherCell )
  {
    return name.equals( otherCell.getName() );
  }

  /**
   * Returns the name of this <CODE>Cell</CODE>
   *
   * @return the name of this <CODE>Cell</CODE>
   */
  public String toString()
  {
    return name;
  }

  /////////////// native methods Cell ////////////////////

  /**
   * Returns the total number of KAS users belonging to the cell denoted
   * by <CODE>cellHandle</CODE>.
   *
   * @param cellHandle    the handle of the cell to which the users belong
   * @return total count of KAS users
   * @exception AFSException  If an error occurs in the native code
   * @see Cell#getCellHandle
   */
  protected static native int getKasUserCount( long cellHandle )
    throws AFSException;

  /**
   * Begin the process of getting the kas users that belong to the cell.  
   * Returns an iteration ID to be used by subsequent calls to 
   * <code>getKasUsersNextString</code> (or <code>getKasUsersNext</code>) 
   * and <code>getKasUsersDone</code>.
   *
   * @param cellHandle    the handle of the cell to which the users belong
   * @see Cell#getCellHandle
   * @return an iteration ID
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native long getKasUsersBegin( long cellHandle )
    throws AFSException;

  /**
   * Begin the process of getting the KAS users, starting at
   * <code>startIndex</code>, that belong to the cell.  
   * Returns an iteration ID to be used by subsequent calls to 
   * <code>getKasUsersNextString</code> (or <code>getKasUsersNext</code>) 
   * and <code>getKasUsersDone</code>.
   *
   * @param cellHandle    the handle of the cell to which the users belong
   * @param startIndex    the starting base-zero index
   * @return an iteration ID
   * @exception AFSException  If an error occurs in the native code
   * @see Cell#getCellHandle
   */
  protected static native long getKasUsersBeginAt( long cellHandle,
                                                  int startIndex )
    throws AFSException;

  /**
   * Returns the next kas user of the cell.  Returns <code>null</code> if there
   * are no more users.  Appends instance names to principal names as follows:
   * <i>principal</i>.<i>instance</i>
   *
   * @param iterationId   the iteration ID of this iteration
   * @see Cell#getKasUsersBegin
   * @return the name of the next user of the cell
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native String getKasUsersNextString( long iterationId )
    throws AFSException;

  /**
   * Fills the next kas user object of the cell.  Returns 0 if there
   * are no more users, != 0 otherwise.
   *
   * @param cellHandle    the handle of the cell to which the users belong
   * @see Cell#getCellHandle
   * @param iterationId   the iteration ID of this iteration
   * @see Cell#getKasUsersBegin
   * @param theUser   a User object to be populated with the values of 
   *                  the next kas user
   * @return 0 if there are no more users, != 0 otherwise
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native int getKasUsersNext( long cellHandle,  
					       long iterationId, 
					       User theUser )
    throws AFSException;

  /**
   * Signals that the iteration is complete and will not be accessed anymore.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see Cell#getKasUsersBegin
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void getKasUsersDone( long iterationId )
    throws AFSException;

  /**
   * Returns the total number of PTS users belonging to the cell denoted
   * by <CODE>cellHandle</CODE>.
   *
   * @param cellHandle    the handle of the cell to which the users belong
   * @return total number of PTS users
   * @exception AFSException  If an error occurs in the native code
   * @see Cell#getCellHandle
   */
  protected static native int getPtsUserCount( long cellHandle )
    throws AFSException;

  /**
   * Returns the total number of PTS users, belonging to the cell denoted
   * by <CODE>cellHandle</CODE>, that are not in KAS.
   *
   * @param cellHandle    the handle of the cell to which the users belong
   * @return total number of users that are in PTS and not KAS
   * @exception AFSException  If an error occurs in the native code
   * @see Cell#getCellHandle
   */
  protected static native int getPtsOnlyUserCount( long cellHandle )
    throws AFSException;

  /**
   * Begin the process of getting the pts users that belong to the cell.  
   * Returns an iteration ID to be used by subsequent calls to 
   * <code>getPtsUsersNextString</code> (or <code>getPtsUsersNext</code>) 
   * and <code>getPtsUsersDone</code>.  
   *
   * @param cellHandle    the handle of the cell to which the users belong
   * @see Cell#getCellHandle
   * @return an iteration ID
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native long getPtsUsersBegin( long cellHandle )
    throws AFSException;

  /**
   * Returns the next pts user of the cell.  Returns <code>null</code> if 
   * there are no more users.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see Cell#getPtsUsersBegin
   * @return the name of the next user of the cell
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native String getPtsUsersNextString( long iterationId )
    throws AFSException;

  /**
   * Returns the next pts user (who is not a kas user) of the cell.  
   * Returns <code>null</code> if there are no more users.
   *
   * @param iterationId   the iteration ID of this iteration
   * @param cellHandle   the cell handle to which these users will belong
   * @see Cell#getPtsUsersBegin
   * @return the name of the next pts user (not kas user) of the cell
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native String getPtsOnlyUsersNextString( long iterationId, 
							    long cellHandle )
    throws AFSException;

  /**
   * Fills the next pts user object of the cell.  Returns 0 if there
   * are no more users, != 0 otherwise.
   *
   * @param cellHandle    the handle of the cell to which the users belong
   * @see Cell#getCellHandle
   * @param iterationId   the iteration ID of this iteration
   * @see Cell#getPtsUsersBegin
   * @param theUser   a User object to be populated with the values of 
   *                  the next pts user
   * @return 0 if there are no more users, != 0 otherwise
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native int getPtsUsersNext( long cellHandle, long iterationId,
					       User theUser )
    throws AFSException;

  /**
   * Fills the next pts user (who does not have a kas entry) object of 
   * the cell.  Returns 0 if there are no more users, != 0 otherwise.
   *
   * @param cellHandle    the handle of the cell to which the users belong
   * @see Cell#getCellHandle
   * @param iterationId   the iteration ID of this iteration
   * @see Cell#getPtsUsersBegin
   * @param theUser   a User object to be populated with the values of 
   *                  the next pts (with no kas) user
   * @return 0 if there are no more users, != 0 otherwise
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native int getPtsOnlyUsersNext( long cellHandle, 
						   long iterationId, 
						   User theUser )
    throws AFSException;

  /**
   * Signals that the iteration is complete and will not be accessed anymore.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see Cell#getPtsUsersBegin
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void getPtsUsersDone( long iterationId )
    throws AFSException;

  /**
   * Returns the total number of groups belonging to the cell denoted
   * by <CODE>cellHandle</CODE>.
   *
   * @param cellHandle    the handle of the cell to which the groups belong
   * @return total number of groups
   * @exception AFSException  If an error occurs in the native code
   * @see Cell#getCellHandle
   */
  protected static native int getGroupCount( long cellHandle )
    throws AFSException;

  /**
   * Begin the process of getting the groups that belong to the cell.  Returns 
   * an iteration ID to be used by subsequent calls to 
   * <code>getGroupsNextString</code> (or <code>getGroupsNext</code>) and 
   * <code>getGroupsDone</code>.  
   *
   * @param cellHandle    the handle of the cell to which the groups belong
   * @see Cell#getCellHandle
   * @return an iteration ID
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native long getGroupsBegin( long cellHandle )
    throws AFSException;

  /**
   * Begin the process of getting the groups that belong to the cell, starting
   * with element index <code>startIndex</code>.  Returns an iteration ID to 
   * be used by subsequent calls to <code>getGroupsNextString</code> 
   * (or <code>getGroupsNext</code>) and <code>getGroupsDone</code>.  
   *
   * @param cellHandle    the handle of the cell to which the groups belong
   * @param startIndex    the starting base-zero index
   * @return an iteration ID
   * @exception AFSException  If an error occurs in the native code
   * @see Cell#getCellHandle
   */
  protected static native long getGroupsBeginAt( long cellHandle, 
                                                int startIndex )
    throws AFSException;

  /**
   * Returns the next group of the cell.  Returns <code>null</code> if there
   * are no more groups.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see Cell#getGroupsBegin
   * @return the name of the next user of the cell
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native String getGroupsNextString( long iterationId )
    throws AFSException;

  /**
   * Fills the next group object of the cell.  Returns 0 if there
   * are no more groups, != 0 otherwise.
   *
   * @param cellHandle    the handle of the cell to which the users belong
   * @see Cell#getCellHandle
   * @param iterationId   the iteration ID of this iteration
   * @see Cell#getGroupsBegin
   * @param theGroup   a Group object to be populated with the values of 
   *                   the next group
   * @return 0 if there are no more users, != 0 otherwise
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native int getGroupsNext( long cellHandle, long iterationId, 
					     Group theGroup )
    throws AFSException;

  /**
   * Signals that the iteration is complete and will not be accessed anymore.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see Cell#getGroupsBegin
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void getGroupsDone( long iterationId )
    throws AFSException;

  /**
   * Returns the total number of servers belonging to the cell denoted
   * by <CODE>cellHandle</CODE>.
   *
   * @param cellHandle    the handle of the cell to which the servers belong
   * @return total number of servers
   * @exception AFSException  If an error occurs in the native code
   * @see Cell#getCellHandle
   */
  protected static native int getServerCount( long cellHandle )
    throws AFSException;

  /**
   * Begin the process of getting the servers in the cell.  Returns 
   * an iteration ID to be used by subsequent calls to 
   * <code>getServersNextString</code> and <code>getServersDone</code>.  
   *
   * @param cellHandle    the handle of the cell to which the servers belong
   * @see Cell#getCellHandle
   * @return an iteration ID
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native long getServersBegin( long cellHandle )
    throws AFSException;

  /**
   * Returns the next server of the cell.  Returns <code>null</code> if there
   * are no more servers.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see Cell#getServersBegin
   * @return the name of the next server of the cell
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native String getServersNextString( long iterationId )
    throws AFSException;

  /**
   * Fills the next server object of the cell.  Returns 0 if there are no 
   * more servers, != 0 otherwise.
   *
   * @param cellHandle    the handle of the cell to which the users belong
   * @see Cell#getCellHandle
   * @param iterationId   the iteration ID of this iteration
   * @param theServer   a Server object to be populated with the values 
   *                    of the next server 
   * @see Cell#getServersBegin
   * @return 0 if there are no more servers, != 0 otherwise
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native int getServersNext( long cellHandle, long iterationId, 
					      Server theServer )
    throws AFSException;

  /**
   * Signals that the iteration is complete and will not be accessed anymore.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see Cell#getServersBegin
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void getServersDone( long iterationId )
    throws AFSException;

  /**
   * Returns the name of the cell.
   *
   * @param cellHandle    the handle of the cell to which the user belongs
   * @see Cell#getCellHandle
   * @return the name of the cell
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native String getCellName( long cellHandle )
    throws AFSException;

  /**
   * Creates a mount point for a volume within the file system.
   *
   * @param cellHandle    the handle of the cell to which the user belongs
   * @see Cell#getCellHandle
   * @param directory    the full path of the place in the AFS file system 
   *                     at which to mount the volume
   * @param volumeName   the name of the volume to mount
   * @param readWrite   whether or not this is to be a readwrite mount point
   * @param forceCheck  whether or not to check if this volume name exists
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void createMountPoint( long cellHandle, 
						 String directory, 
						 String volumeName, 
						 boolean readWrite, 
						 boolean forceCheck )
    throws AFSException;

  /*
   * Sets an ACL for a given place in the AFS file system.
   *
   * @param directory    the full path of the place in the AFS file system 
   *                     for which to add an entry
   * @param username   the name of the user or group for which to add an entry
   * @param read    whether or not to allow read access to this user
   * @param write    whether or not to allow write access to this user
   * @param lookup    whether or not to allow lookup access to this user
   * @param delete    whether or not to allow deletion access to this user
   * @param insert    whether or not to allow insertion access to this user
   * @param lock    whether or not to allow lock access to this user
   * @param admin    whether or not to allow admin access to this user
   * @exception AFSException  If an error occurs in the native code
   */
  public static native void setACL( String directory, String username, 
                                       boolean read, boolean write, 
                                       boolean lookup, boolean delete, 
                                       boolean insert, boolean lock,
				       boolean admin )
    throws AFSException;

  /**
   * Gets the maximum group pts ID that's been used within a cell.   
   * The next auto-assigned group ID will be one less (more negative) 
   * than this value.
   *
   * @param cellHandle    the handle of the cell to which the group belongs
   * @see Cell#getCellHandle
   * @return an integer reresenting the max group id in a cell
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native int getMaxGroupID( long cellHandle )
    throws AFSException;

  /**
   * Sets the maximum group pts ID that's been used within a cell.  The next 
   * auto-assigned group ID will be one less (more negative) than this value.
   *
   * @param cellHandle    the handle of the cell to which the group belongs
   * @see Cell#getCellHandle
   * @param maxID an integer reresenting the new max group id in a cell
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void setMaxGroupID( long cellHandle, int maxID )
    throws AFSException;

  /**
   * Gets the maximum user pts ID that's been used within a cell.   
   * The next auto-assigned user ID will be one greater (more positive) 
   * than this value.
   *
   * @param cellHandle    the handle of the cell to which the user belongs
   * @see Cell#getCellHandle
   * @return an integer reresenting the max user id in a cell
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native int getMaxUserID( long cellHandle )
    throws AFSException;

  /**
   * Sets the maximum user pts ID that's been used within a cell.  The next 
   * auto-assigned user ID will be one greater (more positive) than this value.
   *
   * @param cellHandle    the handle of the cell to which the user belongs
   * @see Cell#getCellHandle
   * @param maxID an integer reresenting the new max user id in a cell
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void setMaxUserID( long cellHandle, int maxID )
    throws AFSException;

  /**
   * Reclaims all memory being saved by the cell portion of the native library.
   * This method should be called when no more <code>Cell</code> objects 
   * are expected to be used.
   */
  protected static native void reclaimCellMemory();


  /////////////// native methods jafs_Cell ////////////////////

  /**
   * Opens a cell for administrative use, based on the token provided.  
   * Returns a cell handle to be used by other methods as a means of 
   * authentication.
   *
   * @param cellName    the name of the cell for which to get the handle
   * @param tokenHandle    a token handle previously returned by a call to 
   * {@link Token#getHandle}
   * @return a handle to the open cell
   * @exception AFSException  If an error occurs in the native code
   * @see Token#getHandle
   */
  protected static native long getCellHandle( String cellName, long tokenHandle )
	throws AFSException;
 
  /**
   * Closes the given currently open cell handle.
   *
   * @param cellHandle   the cell handle to close
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void closeCell( long cellHandle ) 
	throws AFSException;
}








