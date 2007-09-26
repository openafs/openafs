/*
 * @(#)Server.java	1.0 6/29/2001
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
import java.text.DecimalFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.GregorianCalendar;

/**
 * An abstract representation of an AFS server.  It holds information about 
 * the server, such as what its processes are.
 * <BR><BR>
 *
 * Constructing an instance of a <code>Server</code> does not mean an actual 
 * AFS server is created and added to a cell -- on the contrary, a 
 * <code>Server</code> object must be a representation of an already existing 
 * AFS server.  There is no way to create a new AFS server through this API.  
 * See <a href="http://www.openafs.org">OpenAFS.org</a> for information on how
 * to create a new AFS server.<BR><BR>
 *
 * A <code>Server</code> object may represent either an AFS file server,
 * an AFS database server, or both if the same machine serves both
 * purposes.<BR><BR>
 *
 * Each <code>Server</code> object has its own individual set of
 * <code>Partition</code>s, <code>Process</code>es, and <code>Key</code>s.
 * This represents the properties and attributes of an actual AFS server.
 * <BR><BR>
 *    
 * <!--Example of how to use class-->
 * The following is a simple example of how to construct and use a Server 
 * object.  This example constructs a <code>Server</code> using the 
 * <code>Cell</code> representing teh AFS cell to which the server belongs, 
 * and prints out the names of all the partitions residing on the server.
 * <BR><BR>
 *
 * <PRE>
 * import org.openafs.jafs.Cell;
 * import org.openafs.jafs.AFSException;
 * import org.openafs.jafs.Partition;
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
 *     server = new Server(serverName, cell);
 * 
 *     System.out.println("Partitions in Server " + server.getName() + ":");
 *     if( server.isFileServer() ) {
 *       Partition[] partitions = server.getPartitions();
 *       for (int i = 0; i < partitions.length; i++) {
 *         System.out.println(" -> " + partitions[i]);
 *       }
 *     }
 *   }
 *   ...
 * }
 * </PRE>
 *
 */
public class Server implements Serializable, Comparable
{
  /**
   * Used for binary restart time types.
   */
  private static final int RESTART_BINARY = 0;

  /**
   * Used for general restart time types.
   */
  private static final int RESTART_GENERAL = 1;

  protected String name;
  protected Cell cell;

  protected long vosHandle;
  protected long bosHandle;

  protected boolean database;
  protected boolean fileServer;

  // these will be true if the machine is supposedly listed as a server 
  // but that's wrong, or the machine is down 
  protected boolean badFileServer;  
  protected boolean badDatabase;

  // String IP Address of address[0]
  protected String[] ipAddresses;

  protected ArrayList partitionNames;
  protected ArrayList partitions;
  protected ArrayList adminNames;
  protected ArrayList admins;
  protected ArrayList keys;
  protected ArrayList processNames;
  protected ArrayList processes;

  // Storage information
  protected int totalSpace;
  protected int totalQuota;
  protected int totalFreeSpace;
  protected int totalUsedSpace;

  protected ExecutableTime genRestartTime;
  protected ExecutableTime binRestartTime;

  protected boolean cachedInfo;

  /**
   * Constructs a new <CODE>Server</CODE> object instance given the 
   * name of the AFS server and the AFS cell, represented by 
   * <CODE>cell</CODE>, to which it belongs.  This does not actually
   * create a new AFS server, it just represents an existing one.
   * If <code>name</code> is not an actual AFS server, exceptions
   * will be thrown during subsequent method invocations on this 
   * object.
   *
   * @param name  the name of the server to represent
   * @param cell  the cell to which the server belongs.
   * @exception AFSException      If an error occurs in the native code
   */
  public Server( String name, Cell cell ) throws AFSException
  {
    this.name = name;
    this.cell = cell;
    
    cachedInfo = false;

    vosHandle = 0;
    bosHandle = 0;

    ipAddresses = new String[16];

    partitionNames = null;
    partitions = null;
    adminNames = null;
    admins = null;
    keys = null;
    processNames = null;
    processes = null;
  }

  /**
   * Constructs a new <CODE>Server</CODE> object instance given the name 
   * of the AFS server and the AFS cell, represented by <CODE>cell</CODE>, 
   * to which it belongs.   This does not actually
   * create a new AFS server, it just represents an existing one.
   * If <code>name</code> is not an actual AFS server, exceptions
   * will be thrown during subsequent method invocations on this 
   * object.
   *
   * <P> This constructor is ideal for point-in-time representation and 
   * transient applications.  It ensures all data member values are set 
   * and available without calling back to the filesystem at the first 
   * request for them.  Use the {@link #refresh()} method to address any 
   * coherency concerns.
   *
   * @param name               the name of the server to represent 
   * @param cell               the cell to which the server belongs.
   * @param preloadAllMembers  true will ensure all object members are 
   *                           set upon construction;
   *                           otherwise members will be set upon access, 
   *                           which is the default behavior.
   * @exception AFSException      If an error occurs in the native code
   * @see #refresh
   */
  public Server( String name, Cell cell, boolean preloadAllMembers ) 
      throws AFSException
  {
    this(name, cell);
    if (preloadAllMembers) refresh(true);
  }
  
  /**
   * Constructs a blank <code>Server</code> object instance given the cell to 
   * which the server belongs.  This blank object can then be passed into 
   * other methods to fill out its properties.
   *
   * @param cell       the cell to which the server belongs.
   * @exception AFSException      If an error occurs in the native code
   */
  Server( Cell cell ) throws AFSException
  {
    this( null, cell );
  }

  /*-------------------------------------------------------------------------*/

  /**
   * Refreshes the properties of this Server object instance with values 
   * from the AFS server it represents.  All properties that have been 
   * initialized and/or accessed will be renewed according to the values 
   * of the AFS server this Server object instance represents.
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
   * Refreshes the properties of this Server object instance with values 
   * from the AFS server it represents.  If <CODE>all</CODE> is 
   * <CODE>true</CODE> then <U>all</U> of the properties of this Server 
   * object instance will be set, or renewed, according to the values of the 
   * AFS server it represents, disregarding any previously set properties.
   *
   * <P> Thus, if <CODE>all</CODE> is <CODE>false</CODE> then properties that 
   * are currently set will be refreshed and properties that are not set 
   * will remain uninitialized.
   * See {@link #refresh()} for more information.
   *
   * @param all   if true set or renew all object properties; 
   *              otherwise renew all set properties
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  protected void refresh(boolean all) throws AFSException
  {
    if ( all ) {
      refreshProcesses();
      refreshProcessNames();
      refreshKeys();
      refreshAdminNames();
      refreshAdmins();
      refreshPartitionNames();
      refreshPartitions(all);
      refreshInfo();
      refreshGeneralRestart();
      refreshBinaryRestart();
    } else {
      if ( processes      != null ) refreshProcesses();
      if ( processNames   != null ) refreshProcessNames();
      if ( keys           != null ) refreshKeys();
      if ( adminNames     != null ) refreshAdminNames();
      if ( admins         != null ) refreshAdmins();
      if ( partitionNames != null ) refreshPartitionNames();
      if ( partitions     != null ) refreshPartitions(all);
      if ( genRestartTime != null ) refreshGeneralRestart();
      if ( binRestartTime != null ) refreshBinaryRestart();
      if ( cachedInfo )             refreshInfo();
    }
  }

  /**
   * Refreshes the information fields of this <code>Server</code> to 
   * reflect the current state of the AFS server.  These fields include
   * the IP addresses and the fileserver types.
   *
   * @exception AFSException      If an error occurs in the native code
   */
  protected void refreshInfo() throws AFSException
  {
    getServerInfo( cell.getCellHandle(), name, this );
    cachedInfo = true;
  }

  /**
   * Refreshes the general restart time fields of this <code>Server</code> 
   * to reflect the current state of the AFS server.
   *
   * @exception AFSException      If an error occurs in the native code
   */
  protected void refreshGeneralRestart() throws AFSException
  {
    if (genRestartTime == null) genRestartTime = new ExecutableTime();
    getRestartTime( getBosHandle(), RESTART_GENERAL, genRestartTime );
  }

  /**
   * Refreshes the binary restart time fields of this <code>Server</code> 
   * to reflect the current state of the AFS server.
   *
   * @exception AFSException      If an error occurs in the native code
   */
  protected void refreshBinaryRestart() throws AFSException
  {
    if (binRestartTime == null) binRestartTime = new ExecutableTime();
    getRestartTime( getBosHandle(), RESTART_BINARY, binRestartTime );
  }

  /**
   * Obtains the most current list of <code>Partition</code> objects 
   * of this server.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshPartitions() throws AFSException
  {
    this.refreshPartitions(false);
  }

  /**
   * Obtains the most current list of <code>Partition</code> objects of 
   * this server.
   *
   * @param refreshVolumes force all volumes contained in each 
   *                       partition to be refreshed.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshPartitions(boolean refreshVolumes) 
      throws AFSException
  {
    if (!isFileServer() || isBadFileServer()) return;

    Partition currPartition;

    long iterationID = getPartitionsBegin( cell.getCellHandle(), 
					  getVosHandle() );
    
    partitions = new ArrayList();
    
    currPartition = new Partition( this );
    while( getPartitionsNext( iterationID, currPartition ) != 0 ) {
      //Only volumes are necessary since volume information 
      //is populated at time of construction
      if (refreshVolumes) currPartition.refreshVolumes();
      partitions.add( currPartition );
      currPartition = new Partition( this );
    } 
    getPartitionsDone( iterationID );
    totalSpace = 0;
    totalQuota = 0;
    totalUsedSpace = 0;
    totalFreeSpace = 0;
  }

  /**
   * Obtains the most current list of partition names of this server.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshPartitionNames() throws AFSException
  {
    if (!isFileServer() || isBadFileServer()) return;

    String currName;

    long iterationID = getPartitionsBegin( cell.getCellHandle(), 
					  getVosHandle() );
    
    partitionNames = new ArrayList();
    
    while( ( currName = getPartitionsNextString( iterationID ) ) != null ) {
        partitionNames.add( currName );
    } 
    getPartitionsDone( iterationID );
  }

  /**
   * Obtains the most current list of bos admin names of this server.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshAdminNames() throws AFSException
  {
    String currName;

    long iterationID = getBosAdminsBegin( getBosHandle() );
    
    adminNames = new ArrayList();
    
    while( ( currName = getBosAdminsNextString( iterationID ) ) != null ) {
        adminNames.add( currName );
    } 
    getBosAdminsDone( iterationID );
  }

  /**
   * Obtains the most current list of admin <code>User</code> objects of 
   * this server.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshAdmins() throws AFSException
  {
    User currUser;

    long iterationID = getBosAdminsBegin( getBosHandle() );
	
    admins = new ArrayList();
	
    currUser = new User( cell );
    while( getBosAdminsNext( cell.getCellHandle(), iterationID, currUser ) 
	   != 0 ) {
      admins.add( currUser );
      currUser = new User( cell );
    } 
    getBosAdminsDone( iterationID );
  }

  /**
   * Obtains the most current list of <code>Key</code> objects of this server.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshKeys() throws AFSException
  {
    Key currKey;

    long iterationID = getKeysBegin( getBosHandle() );
    
    keys = new ArrayList();
    
    currKey = new Key( this );
    while( getKeysNext( iterationID, currKey ) != 0 ) {
        keys.add( currKey );
        currKey = new Key( this );
    } 
    getKeysDone( iterationID );
  }

  /**
   * Obtains the most current list of process names of this server.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshProcessNames() throws AFSException
  {
    String currName;

    long iterationID = getProcessesBegin( getBosHandle() );
    
    processNames = new ArrayList();
    
    while( ( currName = getProcessesNextString( iterationID ) ) != null ) {
        processNames.add( currName );
    } 
    getProcessesDone( iterationID );
  }

  /**
   * Obtains the most current list of <code>Process</code> objects of 
   * this server.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshProcesses() throws AFSException
  {
    Process currProcess;

    long iterationID = getProcessesBegin( getBosHandle() );
    
    processes = new ArrayList();
    
    currProcess = new Process( this );
    while( getProcessesNext( getBosHandle(), iterationID, currProcess ) 
	   != 0 ) {
        processes.add( currProcess );
        currProcess = new Process( this );
    } 
    getProcessesDone( iterationID );
  }

  /**
   * Add a bos admin to the UserList file of this server, in order to
   * given the AFS user represented by <code>admin</code> full bos
   * administrative privileges on this server.
   *
   * @param admin   the admin to add
   * @exception AFSException  If an error occurs in the native code
   */
  public void addAdmin( User admin ) throws AFSException
  {
    String adminName = admin.getName();
    
    addBosAdmin( getBosHandle(), adminName );
    if ( adminNames != null ) {
        adminNames.add( adminName );
    }
  }

  /**
   * Remove a bos admin from the UserList file of this server, in order to
   * take away from the AFS user represented by <code>admin</code> bos
   * administrative privileges on this machine.
   *
   * @param admin   the admin to remove
   * @exception AFSException  If an error occurs in the native code
   */
  public void removeAdmin( User admin ) throws AFSException
  {
    String adminName = admin.getName();
    
    removeBosAdmin( getBosHandle(), adminName );
    if ( adminNames != null ) {
        adminNames.remove( adminNames.indexOf( adminName ) );
        adminNames.trimToSize();
    }
  }

  /**
   * Syncs this server to the VLDB.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  public void syncServer() throws AFSException
  {
    syncServerWithVLDB( cell.getCellHandle(), getVosHandle(), -1 );
  }

  /**
   * Syncs the VLDB to this server.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  public void syncVLDB() throws AFSException
  {
    syncVLDBWithServer( cell.getCellHandle(), getVosHandle(), -1, false );
  }

  /**
   * Salvages (restores consistency to) this server. Uses default values for
   * most salvager options in order to simplify the API.
   *
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void salvage() throws AFSException
  {
    salvage( cell.getCellHandle(), getBosHandle(), null, null, 4, null, null, 
	     false, false, false, false, false, false );
  }

  /**
   * Starts up all bos processes on this server.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  public void startAllProcesses() throws AFSException
  {
    startAllProcesses( getBosHandle() );
  }

  /**
   * Stops all bos processes on this server.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  public void stopAllProcesses() throws AFSException
  {
    stopAllProcesses( getBosHandle() );
  }

  /**
   * Restarts all bos processes on this server.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  public void restartAllProcesses() throws AFSException
  {
    restartAllProcesses( getBosHandle(), false );
  }

  /**
   * Restarts bos server and all bos processes on this server.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  public void restartBosServer() throws AFSException
  {
    restartAllProcesses( getBosHandle(), true );
  }

  /**
   * Gets the contents of a log file, in one large <code>String</code>.  
   * The log cannot be in AFS file space.
   *
   * @return a <code>String</code> containing the contents of the log file
   * @exception AFSException  If an error occurs in the native code
   */
  public String getLog( String logLocation ) throws AFSException
  {
    return getLog( getBosHandle(), logLocation );
  }

  /**
   * Unauthenticates all server-related tokens that have been obtained by 
   * this <code>Server</code> object, and shuts this server object down.
   * This method should only be called when this <code>Server</code> or any 
   * of the objects constructed using this <code>Server</code> will not be 
   * used anymore.  Note that this does not effect the actual AFS server;
   * it merely closes the representation.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  public void close() throws AFSException
  {
    if ( vosHandle != 0 ) {
        closeVosServerHandle( vosHandle );
    }
    if ( bosHandle != 0 ) {
        closeBosServerHandle( bosHandle );
    }

    cachedInfo = false;

    vosHandle = 0;
    bosHandle = 0;

    partitionNames = null;
    partitions = null;
    adminNames = null;
    admins = null;
    keys = null;
    processNames = null;
    processes = null;
  }

  //////////////// accessors:  ////////////////////////

  /**
   * Returns the name of this server.
   *
   * @return the name of this server
   */
  public String getName()
  {
    return name;
  }

  /**
   * Returns the <code>Cell</code> object with which this <code>Server</code>
   * was constructed.  It represents the actual AFS cell to which this
   * server belongs.
   *
   * @return this server's cell
   */
  public Cell getCell()
  {
    return cell;
  }

  /**
   * Returns the number of BOS administrators assigned to this server.
   *
   * <P>If the total list of admins or admin names have already been 
   * collected (see {@link #getAdmins()}), then the returning value will
   * be calculated based upon the current list.  Otherwise, AFS will be
   * explicitly queried for the information.
   *
   * <P> The product of this method is not saved, and is recalculated
   * with every call.
   *
   * @return the number of admins on this server.
   * @exception AFSException  If an error occurs 
   *                               in any of the associated native methods
   * @see #getAdmins()
   * @see #getAdminNames()
   */
  public int getAdminCount() throws AFSException
  {
    if (adminNames != null) {
      return adminNames.size();
    } else if (admins != null) {
      return admins.size();
    } else {
      return getBosAdminCount(getBosHandle());
    }
  }

  /**
   * Retrieves an array containing all of the admin <code>User</code> objects 
   * associated with this <code>Server</code>, each of which are an abstract 
   * representation of an actual bos administrator of the AFS server.  
   * After this method is called once, it saves the array of 
   * <code>User</code>s and returns that saved array on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current list 
   * is obtained.
   *
   * @return a <code>User</code> array of the admins of the server.
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public User[] getAdmins() throws AFSException
  {
    if ( admins == null ) refreshAdmins();
    return (User[]) admins.toArray( new User[admins.size()] );
  }

  /**
   * Retrieves an array containing all of the names of bos admins 
   * associated with this <code>Server</code>. After this method
   * is called once, it saves the array of <code>String</code>s and returns
   * that saved array on subsequent calls, until the {@link #refresh()} method
   * is called and a more current list is obtained.
   *
   * @return a <code>String</code> array of the bos admin of the server.
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public String[] getAdminNames() throws AFSException
  {
    if ( adminNames == null ) refreshAdminNames();
    return (String []) adminNames.toArray( new String[adminNames.size()] );
  }

  /**
   * Returns the number of partitions on this server.
   *
   * <P>If the total list of partitions or partition names have already been 
   * collected (see {@link #getPartitions()}), then the returning value will
   * be calculated based upon the current list.  Otherwise, AFS will be
   * explicitly queried for the information.
   *
   * <P> The product of this method is not saved, and is recalculated
   * with every call.
   *
   * @return the number of partitions on this server.
   * @exception AFSException  If an error occurs 
   *                               in any of the associated native methods
   * @see #getPartitions()
   * @see #getPartitionNames()
   */
  public int getPartitionCount() throws AFSException
  {
    if (partitionNames != null) {
      return partitionNames.size();
    } else if (partitions != null) {
      return partitions.size();
    } else {
      return getPartitionCount(cell.getCellHandle(), getVosHandle());
    }
  }

  /**
   * Retrieves the <CODE>Partition</CODE> object (which is an abstract 
   * representation of an actual AFS partition of this server) designated 
   * by <code>name</code> (i.e. "/vicepa", etc.).  If a partition by 
   * that name does not actually exist in AFS on the server
   * represented by this object, an {@link AFSException} will be
   * thrown.
   *
   * @param name the name of the partition to retrieve
   * @return <CODE>Partition</CODE> designated by <code>name</code>.
   * @exception AFSException  If an error occurs in the native code
   * @exception NullPointerException  If <CODE>name</CODE> is 
   *                                  <CODE>null</CODE>.
   */
  public Partition getPartition(String name) throws AFSException
  {
    if (name == null) throw new NullPointerException();
    if (isFileServer() && !isBadFileServer()) {
      Partition partition = new Partition(name, this);
      partition.refresh(true);
      return partition;
    } else {
      //Throw "No such entry" error
      throw new AFSException("Server is not a file server.", 363524);
    }
  }

  /**
   * Retrieves an array containing all of the <code>Partition</code> objects 
   * associated with this <code>Server</code>, each of which are an abstract 
   * representation of an actual AFS partition of the AFS server.  
   * After this method is called once, it saves the array of 
   * <code>Partition</code>s and returns that saved array on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current list 
   * is obtained.
   *
   * @return a <code>Partition</code> array of the <code>Partition</code> 
   *         objects of the server.
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public Partition[] getPartitions() throws AFSException
  {
    if ( partitions == null ) refreshPartitions();
    if ( partitions != null) {
    	return (Partition []) partitions.toArray( new Partition[partitions.size()] );
    } else {
    	return null;
    }
  }

  /**
   * Retrieves an array containing all of the names of partitions
   * associated with this <code>Server</code> (i.e. "vicepa", etc.). 
   * After this method is called once, it saves the array of 
   * <code>String</code>s and returns that saved array on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * list is obtained.
   *
   * @return a <code>String</code> array of the partitions of the server.
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public String[] getPartitionNames() throws AFSException
  {
    if ( partitionNames == null ) refreshPartitionNames();
    return (String []) 
	partitionNames.toArray( new String[partitionNames.size()] );
  }

  /**
   * Retrieves the <CODE>Key</CODE> object (which is an abstract 
   * representation of an actual AFS partition of this server) designated 
   * by <code>nkeyVersion</code>.  If a key with 
   * that version does not actually exist in AFS on the server
   * represented by this object, <code>null</code> is returned.
   *
   * @param keyVersion the version of the key to retrieve
   * @return <CODE>Key</CODE> designated by <code>keyVersion</code>.
   * @exception AFSException  If an error occurs in the native code
   */
  public Key getKey(int keyVersion) throws AFSException
  {
    try {
      Key[] keys = this.getKeys();
      for (int i = 0; i < keys.length; i++) {
        if (keys[i].getVersion() == keyVersion) {
          return keys[i];
        }
      }
    } catch (Exception e) {
      e.printStackTrace();
    }
    return null;
  }

  /**
   * Returns the number of keys on this server.
   *
   * <P>If the total list of keys has already been 
   * collected (see {@link #getKeys()}), then the returning value will
   * be calculated based upon the current list.  Otherwise, AFS will be
   * explicitly queried for the information.
   *
   * <P> The product of this method is not saved, and is recalculated
   * with every call.
   *
   * @return the number of keys on this server.
   * @exception AFSException  If an error occurs 
   *                               in any of the associated native methods
   * @see #getKeys()
   */
  public int getKeyCount() throws AFSException
  {
    if (keys != null) {
      return keys.size();
    } else {
      return getKeyCount(getBosHandle());
    }
  }

  /**
   * Retrieves an array containing all of the <code>Key</code> objects 
   * associated with this <code>Server</code>, each of which are an abstract 
   * representation of an actual AFS key of the AFS server.  
   * After this method is called once, it saves the array of 
   * <code>Key</code>s and returns that saved array on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current list 
   * is obtained.
   *
   * @return a <code>Key</code> array of the <code>Key</code> objects 
   *         of the server.
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public Key[] getKeys() throws AFSException
  {
    if ( keys == null ) refreshKeys();
    return (Key[]) keys.toArray( new Key[keys.size()] );
  }

  /**
   * Retrieves the <CODE>Process</CODE> object (which is an abstract 
   * representation of an actual AFS process of this server) designated 
   * by <code>name</code> (i.e. "kaserver", etc.).  If a process by 
   * that name does not actually exist in AFS on the server
   * represented by this object, an {@link AFSException} will be
   * thrown.
   *
   * @param name the name of the process to retrieve
   * @return <CODE>Process</CODE> designated by <code>name</code>.
   * @exception AFSException  If an error occurs in the native code
   * @exception NullPointerException  If <CODE>name</CODE> is 
   *                                  <CODE>null</CODE>.
   */
  public Process getProcess(String name) throws AFSException
  {
    if (name == null) throw new NullPointerException();
    //if (isFileServer() && !isBadFileServer()) {
      Process process = new Process(name, this);
      process.refresh(true);
      return process;
    //}
  }

  /**
   * Returns the number of processes hosted by this server.
   *
   * <P>If the total list of processes or process names have already been 
   * collected (see {@link #getProcesses()}), then the returning value will
   * be calculated based upon the current list.  Otherwise, AFS will be
   * explicitly queried for the information.
   *
   * <P> The product of this method is not saved, and is recalculated
   * with every call.
   *
   * @return the number of processes on this server.
   * @exception AFSException  If an error occurs 
   *                               in any of the associated native methods
   * @see #getProcesses()
   * @see #getProcessNames()
   */
  public int getProcessCount() throws AFSException
  {
    if (processNames != null) {
      return processNames.size();
    } else if (processes != null) {
      return processes.size();
    } else {
      return getProcessCount(getBosHandle());
    }
  }

  /**
   * Retrieves an array containing all of the <code>Process</code> objects 
   * associated with this <code>Server</code>, each of which are an abstract 
   * representation of an actual AFS process of the AFS server.  
   * After this method is called once, it saves the array of 
   * <code>Process</code>es and returns that saved array on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current list 
   * is obtained.
   *
   * @return a <code>Process</code> array of the <code>Process</code> 
   *         objects of the server.
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public Process[] getProcesses() throws AFSException
  {
    if ( processes == null ) refreshProcesses();
    if ( processes != null) {
    	return (Process[]) processes.toArray( new Process[processes.size()] );
    }
    return null;
  }

  /**
   * Retrieves an array containing all of the names of processes
   * associated with this <code>Server</code> (i.e. "kaserver", etc.). 
   * After this method is called once, it saves the array of 
   * <code>String</code>s and returns that saved array on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * list is obtained.
   *
   * @return a <code>String</code> array of the processes of the server.
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public String[] getProcessNames() throws AFSException
  {
    if ( processNames == null ) refreshProcessNames();
    return (String[]) processNames.toArray( new String[processNames.size()] );
  }

  /**
   * Returns whether or not this server is a database machine, meaning it runs
   * processes such as the "kaserver" and "vlserver", and participates in 
   * elections.
   *
   * @return whether or not this user this server is a database machine.
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public boolean isDatabase() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return database;
  }

  /**
   * Returns whether or not this server is a file server machine, meaning it
   * runs the "fs" process and stores AFS volumes.
   *
   * @return whether or not this user this server is a file server machine.
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public boolean isFileServer() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return fileServer;
  }

  /**
   * Returns whether or not this server is a database machine AND 
   * either it isn't in reality (e.g. it's incorrectly configured) 
   * or it's currently down.
   *
   * @return whether or not this server is a database machine 
   *         AND either it isn't in reality or it's currently down
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public boolean isBadDatabase() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return badDatabase;
  }

  /**
   * Returns whether this machine thinks it's a file server AND 
   * either it isn't in reality (e.g. it's incorrectly configured) 
   * or it's currently down.
   *
   * @return whether or not this server is a file server machine AND 
   *         either it isn't in reality or it's currently down
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public boolean isBadFileServer() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return badFileServer;
  }

  /**
   * Returns this server's IP address as a String.  It returns it in 
   * dotted quad notation (i.e. 123.123.123.123).  
   *
   * @return this server's IP address as a String
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public String[] getIPAddresses() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    int n = 16;
    for (int i = 0; i < n; i++) {
      if (ipAddresses[i] == null) {
        n = i;
        break;
      }
    }
    String[] addresses = new String[n];
    System.arraycopy(ipAddresses, 0, addresses, 0, n);
    return addresses;
  }

  /**
   * Returns the BOS Server's general restart time in the form of an
   * ExecutableTime object.  This is the time at which the bos server
   * restarts itself and all running processes.  After this method
   * is called once, it saves the time and returns
   * that value on subsequent calls, until the {@link #refresh()} method
   * is called and a more current value is obtained.
   *
   * @return the general restart time
   * @exception AFSException  If an error occurs in the native code
   * @see Server.ExecutableTime
   * @see #refresh()
   */
  public ExecutableTime getGeneralRestartTime() throws AFSException
  {
    if (genRestartTime == null) refreshGeneralRestart();
    return genRestartTime;
  }

  /**
   * Returns the BOS Server's binary restart time in the form of an
   * ExecutableTime object.  This is the time at which all new or newly
   * modified AFS binaries are restarted.  After this method
   * is called once, it saves the time and returns
   * that value on subsequent calls, until the {@link #refresh()} method
   * is called and a more current value is obtained.
   *
   * @return the binary restart time
   * @exception AFSException  If an error occurs in the native code
   * @see Server.ExecutableTime
   * @see #refresh()
   */
  public ExecutableTime getBinaryRestartTime() throws AFSException
  {
    if (binRestartTime == null) refreshBinaryRestart();
    return binRestartTime;
  }

  /**
   * Returns the total space on this server (a sum of the space of all the 
   * partitions associated with this server).  If this server is not a 
   * file server, zero will be returned. After this method
   * is called once, it saves the total space and returns
   * that value on subsequent calls, until the {@link #refresh()} method
   * is called and a more current value is obtained.
   *
   * @return the total space on this server
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public int getTotalSpace() throws AFSException
  {
    if (partitions == null) refreshPartitions(true);
    if (!isFileServer() || isBadFileServer()) return 0;
    if (totalSpace == 0) {
      Partition[] partitions = getPartitions();
      for (int i = 0; i < partitions.length; i++) {
        totalSpace += partitions[i].getTotalSpace();
      }
    }
    return totalSpace;
  }

  /**
   * Returns the total free space on this server (a sum of the free space of 
   * all the partitions associated with this server).  If this server is not a 
   * file server, zero will be returned. After this method
   * is called once, it saves the total free space and returns
   * that value on subsequent calls, until the {@link #refresh()} method
   * is called and a more current value is obtained.
   *
   * @return the total free space on this server
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public int getTotalFreeSpace() throws AFSException
  {
    if (partitions == null) refreshPartitions(true);
    if (!isFileServer() || isBadFileServer()) return 0;
    if (totalFreeSpace == 0) {
      Partition[] partitions = getPartitions();
      for (int i = 0; i < partitions.length; i++) {
        totalFreeSpace += partitions[i].getTotalFreeSpace();
      }
    }
    return totalFreeSpace;
  }

  /**
   * Returns the total used space on this server (a sum of the used space of 
   * all the partitions associated with this server).  If this server is not a 
   * file server, zero will be returned. After this method
   * is called once, it saves the total used space and returns
   * that value on subsequent calls, until the {@link #refresh()} method
   * is called and a more current value is obtained.
   *
   * @return the total space on this partition
   * @exception AFSException  If an error occurs in the native code
   * @see #getTotalSpace()
   * @see #getTotalFreeSpace()
   */
  public int getTotalUsedSpace() throws AFSException
  {
    if (totalUsedSpace == 0) {
      totalUsedSpace = getTotalSpace() - getTotalFreeSpace();
    }
    return totalUsedSpace;
  }

  /**
   * Returns this server's vos handle.
   *
   * @return this server's vos handle
   * @exception AFSException  If an error occurs in the native code
   */
  protected long getVosHandle() throws AFSException
  {
    if ( vosHandle == 0 ) {
      vosHandle = getVosServerHandle( cell.getCellHandle(), name );
    }
    return vosHandle;
  }

  /**
   * Returns this server's bos handle.
   *
   * @return this server's bos handle
   * @exception AFSException  If an error occurs in the native code
   */
  protected long getBosHandle() throws AFSException
  {
    if ( bosHandle == 0 ) {
      bosHandle = getBosServerHandle( cell.getCellHandle(), name );
    }
    return bosHandle;
  }

  //////////////// mutators:  ////////////////////////

  /**
   * Sets the BOS general restart time.   This is the time at which the bos 
   * server restarts itself and all running processes.
   *
   * @param executableTime  Executable time object that represents what 
   * the BOS Server's general restart time should be. 
   * @exception AFSException  If an error occurs in the native code
   * @see Server.ExecutableTime
   */
  public void setGeneralRestartTime( ExecutableTime executableTime ) 
      throws AFSException
  {
    this.setRestartTime( getBosHandle(), RESTART_GENERAL, executableTime );
  }

  /**
   * Sets the BOS binary restart time.   This is the time at which all new 
   * or newly modified AFS binaries are restarted.
   *
   * @param executableTime  Executable time object that represents what 
   *                        the BOS Server's binary restart time should be.
   * @exception AFSException  If an error occurs in the native code
   * @see Server.ExecutableTime
   */
  public void setBinaryRestartTime( ExecutableTime executableTime ) 
      throws AFSException
  {
    this.setRestartTime( getBosHandle(), RESTART_BINARY, executableTime );
  }

  /////////////// custom information methods ////////////////////

  /**
   * Returns a <code>String</code> representation of this <code>Server</code>.
   * Contains the information fields and a list of partitions, admin, and 
   * processes.
   *
   * @return a <code>String</code> representation of the <code>Server</code>
   */
  public String getInfo()
  {
    String r;
    try {
    
    r = "Server: " + name + "\n";

    r += "\tdatabase: " + isDatabase() + "\t\tfileServer: " + 
	isFileServer() + "\n";
    r += "\tbad database: " + isBadDatabase() + "\tbad fileServer: " + 
	isBadFileServer() + "\n";
    //r += "\tAddress: " + getIPAddress()[0] + "\n";

    // restart times:
System.err.println("org.openafs.jafs.Server.getInfo: get general restart time for " + name);
    r += "\tGeneral restart date: " + getGeneralRestartTime() + "\n";
System.err.println("org.openafs.jafs.Server.getInfo: get binary restart time for " + name);
    r += "\tBinary restart date: " + getBinaryRestartTime() + "\n";
    
    if ( isFileServer() && !isBadFileServer() ) {
      r += "\tPartitions:\n";
      
      String parts[] = getPartitionNames();
      
      for( int i = 0; i < parts.length; i++ ) {
        r += "\t\t" + parts[i] + "\n";
      }
    }

    if ( (isDatabase() && !isBadDatabase()) || 
	 (isFileServer() && !isBadFileServer()) ) {
        r += "\tAdmins:\n";
        
        String ads[] = getAdminNames();
    
        for( int i = 0; i < ads.length; i++ ) {
    	r += "\t\t" + ads[i] + "\n";
        }
    }

    if ( (isDatabase() && !isBadDatabase()) || 
	 (isFileServer() && !isBadFileServer()) ) {
        r += "\tProcesses:\n";
        
        String pros[] = getProcessNames();
    
        for( int i = 0; i < pros.length; i++ ) {
    	r += "\t\t" + pros[i] + "\n";
        }
    }

    } catch( Exception e ) {
    return e.toString();
    }
    return r;
  }

  /**
   * Returns a <code>String</code> containing the <code>String</code> 
   * representations of all the partitions of this <code>Server</code>.
   *
   * @return    a <code>String</code> representation of the partitions
   * @see       Partition#getInfo
   */
  public String getInfoPartitions() throws AFSException
  {
    String r;
    r = "Server: " + name + "\n\n";
    r += "--Partitions--\n";

    Partition parts[] = getPartitions();

    for( int i = 0; i < parts.length; i++ ) {
        r += parts[i].getInfo() + "\n";
    }
    return r;
  }

  /**
   * Returns a <code>String</code> containing the <code>String</code> 
   * representations of all the keys of this <code>Server</code>.
   *
   * @return    a <code>String</code> representation of the keys
   * @see       Key#getInfo
   */
  public String getInfoKeys() throws AFSException
  {
    String r;

    r = "Server: " + name + "\n\n";
    r += "--Keys--\n";

    Key kys[] = getKeys();

    for( int i = 0; i < kys.length; i++ ) {
        r += kys[i].getInfo() + "\n";
    }

    return r;
  }

  /**
   * Returns a <code>String</code> containing the <code>String</code> 
   * representations of all the processes of this <code>Server</code>.
   *
   * @return    a <code>String</code> representation of the processes
   * @see       Process#getInfo
   */
  public String getInfoProcesses() throws AFSException
  {
    String r;

    r = "Server: " + name + "\n\n";
    r += "--Processes--\n";

    Process pros[] = getProcesses();

    for( int i = 0; i < pros.length; i++ ) {
        r += pros[i].getInfo() + "\n";
    }
    return r;
  }

  /////////////// custom override methods ////////////////////

  /**
   * Compares two Server objects respective to their names and does not
   * factor any other attribute.  Alphabetic case is significant in 
   * comparing names.
   *
   * @param     server    The Server object to be compared to this 
   *                      Server instance
   * 
   * @return    Zero if the argument is equal to this Server's name, a
   *		value less than zero if this Server's name is
   *		lexicographically less than the argument, or a value greater
   *		than zero if this Server's name is lexicographically
   *		greater than the argument
   */
  public int compareTo(Server server)
  {
    return this.getName().compareTo(server.getName());
  }

  /**
   * Comparable interface method.
   *
   * @see #compareTo(Server)
   */
  public int compareTo(Object obj)
  {
    return compareTo((Server)obj);
  }

  /**
   * Tests whether two <code>Server</code> objects are equal, based on their 
   * names and hosting Cell.
   *
   * @param otherServer   the Server to test
   * @return whether the specifed Server is the same as this Server
   */
  public boolean equals( Server otherServer )
  {
    return ( name.equals(otherServer.getName()) ) &&
           ( this.getCell().equals(otherServer.getCell()) );
  }

  /**
   * Returns the name of this <CODE>Server</CODE>
   *
   * @return the name of this <CODE>Server</CODE>
   */
  public String toString()
  {
    return getName();
  }

  /////////////// native methods ////////////////////

  /**
   * Opens a server for administrative vos use, based on the cell handle 
   * provided.  Returns a vos server handle to be used by other 
   * methods as a means of identification.
   *
   * @param cellHandle    a cell handle previously returned by 
   *                      a call to {@link #getCellHandle}
   * @param serverName    the name of the server for which to retrieve 
   *                      a vos handle
   * @return a vos handle to the server
   * @exception AFSException  If an error occurs in the native code
   * @see #getCellHandle
   */
  protected static native long getVosServerHandle( long cellHandle, 
						  String serverName )
	throws AFSException;

  /**
   * Closes the given currently open vos server handle.
   *
   * @param vosHandle   the vos server handle to close
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void closeVosServerHandle( long vosHandle ) 
	throws AFSException; 

  /**
   * Opens a server for administrative bos use, based on the cell handle 
   * provided.  Returns a bos server handle to be used by other methods 
   * as a means of identification.
   *
   * @param cellHandle    a cell handle previously returned by a call 
   *                      to {@link #getCellHandle}
   * @param serverName    the name of the server for which to retrieve 
   *                      a bos handle
   * @return a bos handle to the server
   * @exception AFSException  If an error occurs in the native code
   * @see #getCellHandle
   */
  protected static native long getBosServerHandle( long cellHandle, 
						  String serverName )
	throws AFSException;

  /**
   * Closes the given currently open bos server handle.
   *
   * @param bosHandle   the bos server handle to close
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void closeBosServerHandle( long bosHandle ) 
	throws AFSException; 

  /**
   * Fills in the information fields of the provided <code>Server</code>. 
   *
   * @param cellHandle    the handle of the cell to which the server belongs
   * @see Cell#getCellHandle
   * @param name     the name of the server for which to get the information
   * @param server     the <code>Server</code> object in which to fill in 
   *                   the information
   * @see Server
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void getServerInfo( long cellHandle, String name, 
					      Server server ) 
	throws AFSException;

  /**
   * Returns the total number of partitions hosted by the server denoted by
   * <CODE>serverHandle</CODE>, if the server is a fileserver.
   *
   * @param cellHandle    the handle of the cell to which the server belongs
   * @param serverHandle  the vos handle of the server to which the 
   *                      partitions belong
   * @return total number of partitions
   * @exception AFSException  If an error occurs in the native code
   * @see Cell#getCellHandle
   * @see #getVosServerHandle
   */
  protected static native int getPartitionCount( long cellHandle, 
						  long serverHandle )
    throws AFSException;

  /**
   * Begin the process of getting the partitions on a server.  Returns 
   * an iteration ID to be used by subsequent calls to 
   * <code>getPartitionsNext</code> and <code>getPartitionsDone</code>.  
   *
   * @param cellHandle    the handle of the cell to which the server belongs
   * @see Cell#getCellHandle
   * @param serverHandle  the vos handle of the server to which the 
   *                      partitions belong
   * @see #getVosServerHandle
   * @return an iteration ID
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native long getPartitionsBegin( long cellHandle, 
						  long serverHandle )
    throws AFSException;

  /**
   * Returns the next partition of the server.  Returns <code>null</code> 
   * if there are no more partitions.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see #getPartitionsBegin
   * @return the name of the next partition of the server
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native String getPartitionsNextString( long iterationId )
    throws AFSException;

  /**
   * Fills the next partition object of the server.  Returns 0 if there
   * are no more partitions, != 0 otherwise
   *
   * @param iterationId   the iteration ID of this iteration
   * @param thePartition   the Partition object in which to fill the 
   *                       values of the next partition
   * @see #getPartitionsBegin
   * @return 0 if there are no more servers, != 0 otherwise
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native int getPartitionsNext( long iterationId, 
						 Partition thePartition )
    throws AFSException;

  /**
   * Signals that the iteration is complete and will not be accessed anymore.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see #getPartitionsBegin
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void getPartitionsDone( long iterationId )
    throws AFSException;
  
  /**
   * Returns the total number of processes hosted by the server denoted by
   * <CODE>serverHandle</CODE>.
   *
   * @param serverHandle  the vos handle of the server to which the 
   *                      processes belong
   * @return total number of processes
   * @exception AFSException  If an error occurs in the native code
   * @see #getVosServerHandle
   */
  protected static native int getProcessCount( long serverHandle )
    throws AFSException;

  /**
   * Begin the process of getting the processes on a server.  Returns 
   * an iteration ID to be used by subsequent calls to 
   * <code>getProcessesNext</code> and <code>getProcessesDone</code>.  
   *
   * @param serverHandle  the bos handle of the server to which the 
   *                      processes belong
   * @see #getBosServerHandle
   * @return an iteration ID
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native long getProcessesBegin( long serverHandle )
    throws AFSException;

  /**
   * Returns the next process of the server.  Returns <code>null</code> 
   * if there are no more processes.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see #getProcessesBegin
   * @return the name of the next process of the cell
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native String getProcessesNextString( long iterationId )
    throws AFSException;

  /**
   * Fills the next process object of the server.  Returns 0 if there
   * are no more processes, != 0 otherwise.
   *
   * @param serverHandle    the handle of the BOS server that hosts the process
   * @see #getBosHandle
   * @param iterationId   the iteration ID of this iteration
   * @param theProcess    the Process object in which to fill the 
   *                      values of the next process
   * @see #getProcessesBegin
   * @return 0 if there are no more processes, != otherwise
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native int getProcessesNext( long serverHandle, 
						long iterationId, 
						Process theProcess )
    throws AFSException;

  /**
   * Signals that the iteration is complete and will not be accessed anymore.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see #getProcessesBegin
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void getProcessesDone( long iterationId )
    throws AFSException;

  /**
   * Returns the total number of keys hosted by the server denoted by
   * <CODE>serverHandle</CODE>.
   *
   * @param serverHandle  the vos handle of the server to which the 
   *                      keys belong
   * @return total number of keys
   * @exception AFSException  If an error occurs in the native code
   * @see #getVosServerHandle
   */
  protected static native int getKeyCount( long serverHandle )
    throws AFSException;

  /**
   * Begin the process of getting the keys of a server.  Returns 
   * an iteration ID to be used by subsequent calls to 
   * <code>getKeysNext</code> and <code>getKeysDone</code>.  
   *
   * @param serverHandle  the bos handle of the server to which the keys belong
   * @see #getBosServerHandle
   * @return an iteration ID
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native long getKeysBegin( long serverHandle )
    throws AFSException;

  /**
   * Returns the next key of the server.  Returns 0 if there
   * are no more keys, != 0 otherwise.
   *
   * @param iterationId   the iteration ID of this iteration
   * @param theKey   a {@link Key Key} object, in which to fill in the
   *                 properties of the next key.
   * @see #getKeysBegin
   * @return 0 if there are no more keys, != 0 otherwise
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native int getKeysNext( long iterationId, Key theKey )
    throws AFSException;

  /**
   * Signals that the iteration is complete and will not be accessed anymore.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see #getKeysBegin
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void getKeysDone( long iterationId )
    throws AFSException;

  /**
   * Returns the total number of BOS administrators associated with the server 
   * denoted by <CODE>serverHandle</CODE>.
   *
   * @param serverHandle  the vos handle of the server to which the 
   *                      BOS admins belong
   * @return total number of BOS administrators
   * @exception AFSException  If an error occurs in the native code
   * @see #getVosServerHandle
   */
  protected static native int getBosAdminCount( long serverHandle )
    throws AFSException;

  /**
   * Begin the process of getting the bos amdinistrators on a server.  Returns 
   * an iteration ID to be used by subsequent calls to 
   * <code>getBosAdminsNext</code> and <code>getBosAdminsDone</code>.  
   *
   * @param serverHandle  the bos handle of the server to which the 
   *                      partitions belong
   * @see #getBosServerHandle
   * @return an iteration ID
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native long getBosAdminsBegin( long serverHandle )
    throws AFSException;

  /**
   * Returns the next bos admin of the server.  Returns <code>null</code> 
   * if there are no more admins.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see #getBosAdminsBegin
   * @return the name of the next admin of the server
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native String getBosAdminsNextString( long iterationId )
    throws AFSException;

  /**
   * Returns the next bos admin of the server.  Returns 0 if there
   * are no more admins, != 0 otherwise.
   *
   * @param cellHandle    the handle of the cell to which these admins belong
   * @see #getCellHandle
   * @param iterationId   the iteration ID of this iteration
   * @see #getBosAdminsBegin
   * @param theUser   the user object in which to fill the values of this admin
   * @return 0 if no more admins, != 0 otherwise
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native int getBosAdminsNext( long cellHandle, 
						long iterationId, User theUser )
    throws AFSException;

  /**
   * Signals that the iteration is complete and will not be accessed anymore.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see #getBosAdminsBegin
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void getBosAdminsDone( long iterationId )
    throws AFSException;

  /**
   * Adds the given to name to the list of bos administrators on that server.
   *
   * @param serverHandle  the bos handle of the server to which the 
   *                      partitions belong
   * @see #getBosServerHandle
   * @param adminName   the name of the admin to add to the list
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void addBosAdmin( long serverHandle, 
					    String adminName )
    throws AFSException;

  /**
   * Removes the given to name from the list of bos administrators on 
   * that server.
   *
   * @param serverHandle  the bos handle of the server to which the 
   *                      partitions belong
   * @see #getBosServerHandle
   * @param adminName   the name of the admin to remove from the list
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void removeBosAdmin( long serverHandle, 
					       String adminName )
    throws AFSException;

  /**
   * Salvages (restores consistency to) a volume, partition, or server
   *
   * @param cellHandle    the handle of the cell to which the volume belongs
   * @see #getCellHandle
   * @param serverHandle  the bos handle of the server on which the 
   *                      volume resides
   * @see #getBosServerHandle
   * @param partitionName  the name of the partition to salvage, 
   *                       can be <code>null</code> only if volName is 
   *                       <code>null</code>
   * @param volName  the name of the volume to salvage, 
   *                 can be <code>null</code>
   * @param numSalvagers   the number of salvager processes to run in parallel
   * @param tempDir   directory to place temporary files, can be 
   *                  <code>null</code>
   * @param logFile    where salvager log will be written, can be 
   *                   <code>null</code>
   * @param inspectAllVolumes   whether or not to inspect all volumes, 
   *                            not just those marked as active at crash
   * @param removeBadlyDamaged   whether or not to remove a volume if it's 
   *                             badly damaged
   * @param writeInodes   whether or not to record a list of inodes modified
   * @param writeRootInodes   whether or not to record a list of AFS 
   *                          inodes owned by root
   * @param forceDirectory   whether or not to salvage an entire directory 
   *                         structure
   * @param forceBlockReads   whether or not to force the salvager to read 
   *                          the partition
   *                          one block at a time and skip badly damaged 
   *                          blocks.  Use if partition has disk errors
   */
  protected static native void salvage( long cellHandle, long serverHandle, 
					String partitionName, String volName,
					int numSalvagers, String tempDir, 
					String logFile, 
					boolean inspectAllVolumes,
					boolean removeBadlyDamaged, 
					boolean writeInodes, 
					boolean writeRootInodes, 
					boolean forceDirectory, 
					boolean forceBlockReads)
	throws AFSException;

  /**
   *  Synchronizes a particular server with the volume location database.
   *
   * @param cellHandle    the handle of the cell to which the server belongs
   * @see #getCellHandle
   * @param serverHandle  the vos handle of the server     
   * @see #getVosServerHandle
   * @param partition   the id of the partition to sync, can be -1 to ignore
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void syncServerWithVLDB( long cellHandle, 
						   long serverHandle, 
						   int partition )
    throws AFSException;

  /**
   *  Synchronizes the volume location database with a particular server.
   *
   * @param cellHandle    the handle of the cell to which the server belongs
   * @see #getCellHandle
   * @param serverHandle  the vos handle of the server     
   * @see #getVosServerHandle
   * @param partition   the id of the partition to sync, can be -1 to ignore
   * @param forceDeletion   whether or not to force the deletion of bad volumes
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void syncVLDBWithServer( long cellHandle, 
						   long serverHandle, 
						   int partition, 
						   boolean forceDeletion )
    throws AFSException;

  /**
   * Retrieves a specified bos log from a server.  Right now this 
   * method will simply return a huge String containing the log, but 
   * hopefully we can devise a better way to make this work more efficiently.
   *
   * @param serverHandle  the bos handle of the server to which the key belongs
   * @see #getBosServerHandle
   * @param logLocation   the full path and name of the desired bos log
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native String getLog( long serverHandle, String logLocation )
    throws AFSException;

  /**
   *  Fills in the restart time fields of the given {@link Server Server} 
   *  object. 
   *
   * @param serverHandle  the bos handle of the server to which the key belongs
   * @see #getBosServerHandle
   * @param restartType  whether to get the general or binary restart. 
   *                     Acceptable values are:<ul>
   *                     <li>{@link RESTART_BINARY}</li>
   *                     <li>{@link RESTART_GENERAL}</li></ul>     
   * @param theServer   the <code>Server</code> object, in which to fill 
   *                    the restart time fields
   * @exception AFSException  If an error occurs in the native code
   */
  private static native void getRestartTime( long serverHandle, 
					       int restartType, 
					       ExecutableTime executableTime )
    throws AFSException;

  /**
   *  Sets the restart time of the bos server.
   *
   * @param serverHandle  the bos handle of the server to which the key belongs
   * @see #getBosServerHandle
   * @param restartType  whether this is to be a general or binary restart. 
   *                     Acceptable values are:<ul>
   *                     <li>{@link RESTART_BINARY}</li>
   *                     <li>{@link RESTART_GENERAL}</li></ul>
   * @param theServer   the server object containing the desired information
   * @exception AFSException  If an error occurs in the native code
   */
  private static native void setRestartTime( long serverHandle, 
					       int restartType, 
					       ExecutableTime executableTime )
    throws AFSException;

  /**
   * Start all server processes.
   *
   * @param serverHandle  the bos handle of the server to which the 
   *                      processes belong
   * @see #getBosServerHandle
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void startAllProcesses( long serverHandle )
    throws AFSException;

  /**
   * Restart all server processes.
   *
   * @param serverHandle  the bos handle of the server to which the 
   *                      processes belong
   * @see #getBosServerHandle
   * @param restartBosServer   whether or not to restart the bos server as well
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void restartAllProcesses( long serverHandle, 
						    boolean restartBosServer )
    throws AFSException;

  /**
   * Stop all server processes.
   *
   * @param serverHandle  the bos handle of the server to which the 
   *                      processes belong
   * @see #getBosServerHandle
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void stopAllProcesses( long serverHandle )
    throws AFSException;

  /**
   * Reclaims all memory being saved by the server portion of the native 
   * library. This method should be called when no more <code>Server</code> 
   * objects are expected to be used.
   */
  protected static native void reclaimServerMemory();

  /*====================================================================*/
  /* INNER CLASSES  */
  /*====================================================================*/
  public static final class ExecutableTime implements Serializable
  {
    public static final short NEVER = 0;
    public static final short NOW = 1;

    public static final short EVERYDAY  = -1;
    public static final short SUNDAY    = 0;
    public static final short MONDAY    = 1;
    public static final short TUESDAY   = 2;
    public static final short WEDNESDAY = 3;
    public static final short THURSDAY  = 4;
    public static final short FRIDAY    = 5;
    public static final short SATURDAY  = 6;

    static final DecimalFormat formatter = 
	(DecimalFormat)DecimalFormat.getInstance();

    private short second;
    private short minute;
    private short hour;
    private short day;
    private boolean now;
    private boolean never;

    static
    {
      formatter.applyPattern("00");
    }

    /**
     * Internal constructor used to construct an empty object that will 
     * be passed to JNI for member synchronization of the BOS Server 
     * executable time this object represents.
     */
    ExecutableTime()
    {
      this.second = (short) 0;
      this.minute = (short) 0;
      this.hour = (short) 0;
      this.day = (short) -1;
      this.now = false;
      this.never = false;
    }

    /**
     * Constructs an <code>ExecutableTime</code> object that represents either 
     * a "<CODE>now</CODE>" <B>or</B> "<CODE>never</CODE>" BOS Executable 
     * Restart Time. 
     *
     * <P>Valid values for the <CODE>type</CODE> parameter are ExecutableTime.NOW
     * or ExecutableTime.NEVER.  If a value other than these two is used an
     * IllegalArgumentException will be thrown.
     *
     * @param type   either ExecutableTime.NOW or ExecutableTime.NEVER
     * @exception    IllegalArgumentException
     *               If a value other than ExecutableTime.NOW or 
     *               ExecutableTime.NEVER is used for the <CODE>type</CODE> 
     *               parameter.
     * @see #isNow()
     * @see #isNever()
     * @see #Server.ExecutableTime(short, short, short)
     */
    public ExecutableTime(short type) throws IllegalArgumentException
    {
      if (type == NOW) {
        this.now = true;
        this.never = false;
      } else if (type == NEVER) {
        this.now = false;
        this.never = true;
      } else {
        throw new IllegalArgumentException("You must specify either " + 
                                           "ExecutableTime.NOW or " + 
                                           "ExecutableTime.NEVER when " +
                                           "using this constructor.");
      }
      this.second = (short) 0;
      this.minute = (short) 0;
      this.hour = (short) 0;
      this.day = (short) -1;
    }

    /**
     * Constructs an <code>ExecutableTime</code> object that may be used to 
     * represent a <U>daily</U> BOS Executable Restart Time of a process. 
     *
     * @param second   the second field for this representation of a 
     *                 BOS Server restart time value (range: 0-59)
     * @param minute   the minute field for this representation of a 
     *                 BOS Server restart time value (range: 0-59)
     * @param hour     the hour field for this representation of a BOS 
     *                 Server restart time value (range: 0-23)
     * @exception      IllegalArgumentException
     *                 If any of the parameters values are out of range 
     *                 of their respective fields.
     * @see #Server.ExecutableTime(short, short, short, short)
     * @see #getSecond()
     * @see #getMinute()
     * @see #getHour()
     */
    public ExecutableTime(short second, short minute, short hour)
      throws IllegalArgumentException
    {
      this(second, minute, hour, ExecutableTime.EVERYDAY);
    }

    /**
     * Constructs an <code>ExecutableTime</code> object that may be used to 
     * represent the BOS Executable Restart Time of a process. 
     *
     * @param second   the second field for this representation of a 
     *                 BOS Server restart time value (range: 0-59)
     * @param minute   the minute field for this representation of a 
     *                 BOS Server restart time value (range: 0-59)
     * @param hour     the hour field for this representation of a BOS 
     *                 Server restart time value (range: 0-23)
     * @param day      the day field for this representation of a BOS 
     *                 Server restart time value.<BR><UL>Valid values include:
     *                 <CODE>ExecutableTime.EVERYDAY</CODE> (see also {@link 
     *                 #Server.ExecutableTime(short, short, short)})<BR>
     *                 <CODE>
     *                 ExecutableTime.SUNDAY<BR>
     *                 ExecutableTime.MONDAY<BR>
     *                 ExecutableTime.TUESDAY<BR>
     *                 ExecutableTime.WEDNESDAY<BR>
     *                 ExecutableTime.THURSDAY<BR>
     *                 ExecutableTime.FRIDAY<BR>
     *                 ExecutableTime.SATURDAY<BR>
     *                 </CODE></UL>
     * @exception      IllegalArgumentException
     *                 If any of the parameters values are out of range 
     *                 of their respective fields.
     * @see #Server.ExecutableTime(short, short, short)
     * @see #getSecond()
     * @see #getMinute()
     * @see #getHour()
     * @see #getDay()
     */
    public ExecutableTime(short second, short minute, short hour, short day) 
    {
      if ( (0 > second || second > 59) ||
           (0 > minute || minute > 59) ||
           (0 > hour || hour > 24) ||
           (-1 > day || day > 6) ) {
        throw new IllegalArgumentException("One of the specified values " + 
                                        "are invalid.");
      }
      this.second = second;
      this.minute = minute;
      this.hour = hour;
      this.day = day;
      this.now = false;
      this.never = false;
    }

    /**
     * Returns the second of this ExecutableTime object.
     *
     * @return the second of this ExecutableTime object.
     * @exception IllegalStateException 
     *            If the executable time this object represents has a value of
     *            "<CODE>now</CODE>" or "<CODE>never</CODE>".
     */
    public short getSecond() throws IllegalStateException 
    {
      if (now || never) {
        throw new IllegalStateException("Executable time is set to 'now' or" +
							" 'never'.");
      }
      return second;
    }

    /**
     * Returns the minute of this ExecutableTime object.
     *
     * @return the minute of this ExecutableTime object.
     * @exception IllegalStateException 
     *            If the executable time this object represents has a value of
     *            "<CODE>now</CODE>" or "<CODE>never</CODE>".
     */
    public short getMinute() throws IllegalStateException 
    {
      if (now || never) {
        throw new IllegalStateException("Executable time is set to 'now' or" + 
                                        " 'never'.");
      }
      return minute;
    }

    /**
     * Returns the hour of this ExecutableTime object, in 24 hour time.
     *
     * @return the hour of this ExecutableTime object.
     * @exception IllegalStateException
     *            If the executable time this object represents has a value of
     *            "<CODE>now</CODE>" or "<CODE>never</CODE>".
     */
    public short getHour() throws IllegalStateException
    {
      if (now || never) {
        throw new IllegalStateException("Executable time is set to 'now' or" + 
							" 'never'.");
      }
      return hour;
    }

    /**
     * Returns a numeric representation of the day of this ExecutableTime 
     * object. If it is daily, the value of ExecutableTime.EVERYDAY is returned.
     * 
     * <P>Possible return values are:<BR>
     * <CODE>
     * ExecutableTime.EVERYDAY<BR>
     * <BR>
     * ExecutableTime.SUNDAY<BR>
     * ExecutableTime.MONDAY<BR>
     * ExecutableTime.TUESDAY<BR>
     * ExecutableTime.WEDNESDAY<BR>
     * ExecutableTime.THURSDAY<BR>
     * ExecutableTime.FRIDAY<BR>
     * ExecutableTime.SATURDAY<BR>
     * </CODE>
     *
     * @return    a numeric representation of the day of this ExecutableTime 
     *            object.
     * @exception IllegalStateException
     *            If the executable time this object represents has a value of
     *            "<CODE>now</CODE>" or "<CODE>never</CODE>".
     */
    public short getDay() throws IllegalStateException
    {
      if (now || never) {
        throw new IllegalStateException("Executable time is set to 'now' or" + 
							" 'never'.");
      }
      return day;
    }

    /**
     * Returns a String representation, name for the day of the week or 
     * "Everyday", of this object's day property.
     * 
     * <P>Possible return values are:
     * <PRE>
     * Sunday
     * Monday
     * Tuesday
     * Wednesday
     * Thursday
     * Friday
     * Saturday
     * 
     * Everyday
     * </PRE>
     *
     * @return the day of this ExecutableTime object.
     * @exception IllegalStateException
     *            If the executable time this object represents has a value of
     *            "<CODE>now</CODE>" or "<CODE>never</CODE>".
     * @see #getDay()
     */
    public String getDayString() throws IllegalStateException
    {
      switch (getDay())
      {
        case 0:
            return "Sunday";
        case 1:
            return "Monday";
        case 2:
            return "Tuesday";
        case 3:
            return "Wednesday";
        case 4:
            return "Thursday";
        case 5:
            return "Friday";
        case 6:
            return "Saturday";
        default:
            return "Everyday";
      }
    }

    /**
     * Returns whether or not the BOS restart time, represented by this 
     * ExecutableTime object, is set to "<CODE>now</CODE>" or not.  
     * This means that at some point in the past, when someone set it to 
     * "<CODE>now</CODE>", the bosserver restarted all its processes,
     * and never again.
     *
     * @return whether or not the restart time is "<CODE>now</CODE>"
     */
    public boolean isNow()
    {
      return now;
    }

    /**
     * Returns the second of this ExecutableTime object.
     *
     * @return the second of this ExecutableTime object.
     */
    /**
     * Returns whether or not the BOS restart time, represented by this 
     * ExecutableTime object, is set to "<CODE>never</CODE>" or not.  
     * This means that the bosserver will never restart its processes.
     *
     * @return whether or not the restart time is "<CODE>never</CODE>"
     */
    public boolean isNever()
    {
      return never;
    }

    /**
     * Tests whether two <code>ExecutableTime</code> objects are equal, 
     * based on a
     * comparison of each of their respective properties. If 
     * "<CODE>now</CODE>" or "<CODE>never</CODE>" is set in either object, 
     * only those properties are analyzed.
     *
     * @param time   the ExecutableTime to test against
     * @return whether the specifed ExecutableTime is the same as this 
     * ExecutableTime as defined above
     */
    public boolean equals( ExecutableTime time )
    {
      boolean same = false;
      try {
        same = ( (second == time.getSecond()) &&
                 (minute == time.getMinute()) &&
                 (hour   == time.getHour()  ) &&
                 (day    == time.getDay()   ) );
      } catch (Exception e) {
        same = ( (now    == time.isNow()    ) &&
                 (never  == time.isNever()  ) );

      }
      return same;
    }

    /**
     * Returns the String representation of time value of this 
     * <CODE>ExecutableTime</CODE> object.
     *
     * <P> Possible return values:<BR>
     * <LI> "Now"<BR>
     * <LI> "Never"<BR>
     * <LI> Day and time string in the form:<BR>&nbsp;&nbsp;&nbsp;&nbsp; 
     * <CODE>&#60;day&#62; at &#60;hh&#62;:&#60;MM&#62;[:&#60;ss&#62;]</CODE>
     * <BR><BR>
     *
     * <B>Example Return Values:</B><PRE>
     * Sunday at 04:00
     * Sunday at 05:10:30
     * Everyday at 20:00</PRE>
     *
     * @return the String representation of this <CODE>ExecutableTime</CODE> 
     * object
     */
    public String toString()
    {
      if (now) {
        return "Now";
      } else if (never) {
        return "Never";
      } else {
        try {
          if (second != 0) {
            return getDayString() + " at " + 
		ExecutableTime.formatter.format(hour) + ":" + 
		ExecutableTime.formatter.format(minute) + ":" + 
		ExecutableTime.formatter.format(second);
          } else {
            return getDayString() + " at " + 
		ExecutableTime.formatter.format(hour) + ":" + 
		ExecutableTime.formatter.format(minute);
          }
        } catch (Exception e) {
          return "(unknown)";
        }
      }
    }

  }
  /*====================================================================*/

}







