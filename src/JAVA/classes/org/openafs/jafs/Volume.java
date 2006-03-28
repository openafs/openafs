/*
 * @(#)Volume.java	1.0 6/29/2001
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
 * An abstract representation of an AFS volume.  It holds information about 
 * the server, such as what its quota is.
 * <BR><BR>
 *
 * Constructing an instance of a <code>Volume</code> does not mean an actual 
 * AFS partition is created on a partition -- usually a <code>Volume</code>
 * object is a representation of an already existing AFS volume.  If, 
 * however, the <code>Volume</code> is constructed with the name of a 
 * volume that does not exist in the cell to which the provided 
 * <code>Partition</code> belongs, a new AFS volume with that name can be
 * created on that partition by calling the {@link #create(int)} method.  If
 * such a volume does already exist when this method is called, an exception 
 * will be thrown.<BR><BR>
 *
 * <!--Example of how to use class-->
 * The following is a simple example of how to construct and use a 
 * <code>Volume</code> object.  This example obtains the list of 
 * <code>Volume</code> objects residing on a particular partition, and prints
 * out the name and id of each one.<BR><BR>
 *
 * <PRE>
 * import org.openafs.jafs.Cell;
 * import org.openafs.jafs.AFSException;
 * import org.openafs.jafs.Partition;
 * import org.openafs.jafs.Server;
 * import org.openafs.jafs.Volume;
 * ...
 * public class ...
 * {
 *   ...
 *   private Cell cell;
 *   private Server server;
 *   private Partition partition;
 *   ...
 *   public static void main(String[] args) throws Exception
 *   {
 *     String username      = arg[0];
 *     String password      = arg[1];
 *     String cellName      = arg[2];
 *     String serverName    = arg[3];
 *     String partitionName = arg[4];
 * 
 *     token  = new Token(username, password, cellName);
 *     cell   = new Cell(token);
 *     server = cell.getServer(serverName);
 *     partition = cell.getPartition(partitionName);
 * 
 *     System.out.println("Volumes in Partition " + partition.getName() + ":");
 *     Volume[] volumes = partition.getVolumes();
 *     for (int i = 0; i < volumes.length; i++) {
 *       System.out.println(" -> " + volumes[i] + ": " + volumes[i].getID());
 *     }
 *   }
 *   ...
 * }
 * </PRE>
 *
 */
public class Volume implements Serializable, Comparable
{
  /**
   * Read-write volume type
   */
  public static final int VOLUME_TYPE_READ_WRITE = 0;
  /**
   * Read-only volume type
   */
  public static final int VOLUME_TYPE_READ_ONLY = 1;
  /**
   * Backup volume type
   */
  public static final int VOLUME_TYPE_BACKUP = 2;

  /**
   * Status/disposition ok
   */
  public static final int VOLUME_OK = 0;
  /**
   * Status/disposition salvage
   */
  public static final int VOLUME_SALVAGE = 1;
  /**
   * Status/disposition no vnode
   */
  public static final int VOLUME_NO_VNODE = 2;
  /**
   * Status/disposition no volume
   */
  public static final int VOLUME_NO_VOL = 3;
  /**
   * Status/disposition volume exists
   */
  public static final int VOLUME_VOL_EXISTS = 4;
  /**
   * Status/disposition no service 
   */
  public static final int VOLUME_NO_SERVICE = 5;
  /**
   * Status/disposition offline
   */
  public static final int VOLUME_OFFLINE = 6;
  /**
   * Status/disposition online
   */
  public static final int VOLUME_ONLINE = 7;
  /**
   * Status/disposition disk full
   */
  public static final int VOLUME_DISK_FULL = 8;
  /**
   * Status/disposition over quota
   */
  public static final int VOLUME_OVER_QUOTA = 9;
  /**
   * Status/disposition busy
   */
  public static final int VOLUME_BUSY = 10;
  /**
   * Status/disposition moved
   */
  public static final int VOLUME_MOVED = 11;

  protected Cell cell;
  protected Server server;
  protected Partition partition;

  protected String name;

  protected int id;
  protected int readWriteID;
  protected int readOnlyID;
  protected int backupID;

  protected long creationDate;
  protected long lastAccessDate;
  protected long lastUpdateDate;
  protected long lastBackupDate;
  protected long copyCreationDate;

  protected int accessesSinceMidnight;
  protected int fileCount;
  protected int maxQuota;
  protected int currentSize;
  protected int status;
  protected int disposition;
  protected int type;

  protected GregorianCalendar creationDateCal;
  protected GregorianCalendar lastUpdateDateCal;
  protected GregorianCalendar copyCreationDateCal;

  protected boolean cachedInfo;

  /**
   * Constructs a new <CODE>Volume</CODE> object instance given the name of 
   * the AFS volume and the AFS cell, represented by <CODE>partition</CODE>, 
   * to which it belongs.   This does not actually
   * create a new AFS volume, it just represents one.
   * If <code>name</code> is not an actual AFS volume, exceptions
   * will be thrown during subsequent method invocations on this 
   * object, unless the {@link #create(int)} method is explicitly called
   * to create it.
   *
   * @param name       the name of the volume to represent 
   * @param partition  the partition on which the volume resides
   * @exception AFSException      If an error occurs in the native code
   */
  public Volume( String name, Partition partition ) throws AFSException
  {
    this.partition = partition;
    this.server = partition.getServer();
    this.cell = server.getCell();
    this.name = name;
    
    creationDateCal = null;
    lastUpdateDateCal = null;
    copyCreationDateCal = null;
    
    id = -1;
    cachedInfo = false;
  }

  /**
   * Constructs a new <CODE>Volume</CODE> object instance given the name of 
   * the AFS volume and the AFS partition, represented by 
   * <CODE>partition</CODE>, to which it belongs.    This does not actually
   * create a new AFS volume, it just represents one.
   * If <code>name</code> is not an actual AFS volume, exceptions
   * will be thrown during subsequent method invocations on this 
   * object, unless the {@link #create(int)} method is explicitly called
   * to create it.  Note that if the volume doesn't exist and 
   * <code>preloadAllMembers</code> is true, an exception will be thrown.
   *
   * <P> This constructor is ideal for point-in-time representation and 
   * transient applications. It ensures all data member values are set 
   * and available without calling back to the filesystem at the first request
   * for them.  Use the {@link #refresh()} method to address any coherency 
   * concerns.
   *
   * @param name               the name of the volume to represent 
   * @param partition          the partition on which the volume resides.
   * @param preloadAllMembers  true will ensure all object members are set 
   *                           upon construction; otherwise members will be 
   *                           set upon access, which is the default behavior.
   * @exception AFSException      If an error occurs in the native code
   * @see #refresh
   */
  public Volume( String name, Partition partition, boolean preloadAllMembers ) 
      throws AFSException
  {
    this(name, partition);
    if (preloadAllMembers) refresh(true);
  }
  
  /**
   * Creates a blank <code>Volume</code> given the cell to which the volume
   * belongs, the server on which the partition resides, and 
   * the partition on which the volume resides. This blank 
   * object can then be passed into other methods to fill out its properties. 
   *
   * @exception AFSException      If an error occurs in the native code
   * @param cell       the cell to which the server belongs.
   * @param server     the server on which the partition resides
   * @param partition  the partition on which the volume resides
   */
  Volume( Partition partition ) throws AFSException
  {
    this( null, partition );
  }

  /*-------------------------------------------------------------------------*/

  /**
   * Refreshes the properties of this Volume object instance with values from 
   * the AFS volume it represents.  All properties that have been initialized 
   * and/or accessed will be renewed according to the values of the AFS volume
   * this Volume object instance represents.
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
   * Refreshes the properties of this Volume object instance with values from 
   * the AFS volume it represents.  If <CODE>all</CODE> is <CODE>true</CODE> 
   * then <U>all</U> of the properties of this Volume object instance will be 
   * set, or renewed, according to the values of the AFS volume it represents,
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
    if (all || cachedInfo) refreshInfo();
  }

  /**
   * Refreshes the information fields of this <code>Volume</code> to reflect 
   * the current state of the AFS volume. These include the last update time,
   * file count, etc.
   *
   * @exception AFSException      If an error occurs in the native code
   */
  protected void refreshInfo() throws AFSException
  {
    getVolumeInfo( cell.getCellHandle(), server.getVosHandle(), 
		   partition.getID(), getID(), this );
    cachedInfo = true;
    creationDateCal = null;
    lastUpdateDateCal = null;
    copyCreationDateCal = null;
  }

  /**
   * Creates a new volume on the server and partition given upon construction.
   *
   * @param quota    the quota for the volume in K, 0 indicates an unlimited 
   *                 quota
   *
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void create( int quota ) throws AFSException
  {
    id = create( cell.getCellHandle(), server.getVosHandle(), 
		 partition.getID(), name, quota );
    maxQuota = quota;
  }

  /**
   * Creates a backup volume for this volume.
   *
   * @return  the <code>Volume</code> object representation for the
   *          backup volume that was created
   * @exception AFSException  If an error occurs in the native code
   */ 
  public Volume createBackup( ) throws AFSException
  {
    createBackupVolume( cell.getCellHandle(), getID() );
    return new Volume( name + ".backup", partition );
  }

  /**
   * Creates a readonly site for this volume on the specified server and 
   * partition.  Automatically releases the volume.
   *
   * @param sitePartition   the partition on which the readonly volume is 
   *                        to reside
   *
   * @return the <code>Volume</code> representation for the
   *         read-only volume that was created
   * @exception AFSException  If an error occurs in the native code
   */ 
  public Volume createReadOnly( Partition sitePartition ) 
      throws AFSException
  {
    Server siteServer = sitePartition.getServer();
    createReadOnlyVolume( cell.getCellHandle(), siteServer.getVosHandle(), 
			  sitePartition.getID(), getID() );
    release( false );
    return new Volume( name + ".readonly", sitePartition );
  }

  /**
   * Deletes the volume from the cell.
   *
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void delete() throws AFSException
  {
    delete( cell.getCellHandle(), server.getVosHandle(), partition.getID(), 
	    getID() );
    name = null;
    creationDateCal = null;
    lastUpdateDateCal = null;
    copyCreationDateCal = null;
    cell = null;
    server = null;
    partition = null;
  }

  /**
   * Releases this volume, which updates the read-only copies of it.
   *
   * <P> This method will force a complete release; a complete release updates
   * all read-only sites even if the VLDB entry has a flag.
   *
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void release() throws AFSException
  {
    this.release( true );
  }

  /**
   * Releases this volume, which updates the read-only copies of it.
   *
   * @param forceComplete   whether or not to force a complete release; 
   *                        a complete release updates all read-only sites 
   *                        even if the VLDB entry has a flag
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void release( boolean forceComplete ) throws AFSException
  {
    release( cell.getCellHandle(), getID(), forceComplete );
  }

  /**
   * Dumps this volume to a file. If you use the dumpSince argument you will 
   * create an incremental dump, but you can leave it <code>null</code> 
   * for a full dump.
   *
   * @param fileName    the path name of the file on the client machine to 
   *                    which to dump this volume
   * @param dumpSince   dump only files that have been modified more recently 
   *                    than this date
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void dump( String fileName, GregorianCalendar dumpSince ) 
      throws AFSException
  {
    int startTime = 0;
    if ( dumpSince != null ) {
      startTime = (int) ((dumpSince.getTime().getTime())/((long) 1000));
    }
    dump( cell.getCellHandle(), server.getVosHandle(), partition.getID(), 
	  getID(), startTime, fileName );
  }

  /**
   * Dumps this volume to a file. Creates a full dump.
   *
   * @param fileName    the path name of the file to which to dump this volume
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void dump( String fileName ) throws AFSException
  {
    this.dump( fileName, null );
  }

  /**
   * Restores a file to this volume.  Note that this does not have to be an 
   * existing volume in order to be restored - you may create a 
   * <code>Volume</code> as a volume that doesn't yet exist and then restore 
   * a file to it.  Or you can restore over an existing volume.  If a new 
   * volume is being created with this method, the id will be automatically 
   * assigned.
   *
   * @param fileName    the path name of the file on the client machine from 
   * which to restore this volume
   * @param incremental   if true, restores an incremental dump over an 
   *                      existing volume
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void restore( String fileName, boolean incremental ) 
      throws AFSException
  {
      restore( fileName, incremental, 0 );
  }

  /**
   * Restores a file to this volume.  Note that this does not have to be an 
   * existing volume in order to be restored - you may create a 
   * <code>Volume</code> as a volume that doesn't yet exist and then restore 
   * a file to it.  Or you can restore over an existing volume.
   *
   * @param fileName    the path name of the file on the client machine from 
   * which to restore this volume
   * @param incremental   if true, restores an incremental dump over an 
   *                      existing volume
   * @param id     the id to assign this volume
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void restore( String fileName, boolean incremental, int id ) 
      throws AFSException
  {
    restore( cell.getCellHandle(), server.getVosHandle(), partition.getID(), 
	     id, name, fileName, incremental );
  }

  /**
   * Mounts this volume, bringing it online and making it accessible.
   *
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void mount( ) throws AFSException
  {
    mount( server.getVosHandle(), partition.getID(), getID(), 0, true );
  }

  /**
   * Unmounts this volume, bringing it offline and making it inaccessible.
   *
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void unmount( ) throws AFSException
  {
    unmount( server.getVosHandle(), partition.getID(), getID() );
  }

  /**
   * Locks the VLDB enrty for this volume
   *
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void lock( ) throws AFSException
  {
    lock( cell.getCellHandle(), getID() );
  }

  /**
   * Unlocks the VLDB entry for this volume
   *
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void unlock( ) throws AFSException
  {
    unlock( cell.getCellHandle(), getID() );
  }

  /**
   * Moves this volume to the specified partition (which indirectly 
   * specifies a new server, as well).  Caution: This will remove any backup 
   * volumes at the original site.
   *
   * @param newPartition   the partition to which to move the volume
   *
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void moveTo( Partition newPartition ) throws AFSException
  {
    Server newServer = newPartition.getServer();
    move( cell.getCellHandle(), server.getVosHandle(), partition.getID(), 
	  newServer.getVosHandle(), newPartition.getID(), getID() );

    server = newServer;
    partition = newPartition;
  }

  /**
   * Renames this volume.
   *
   * @param newName   the new name for this volume
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void rename( String newName ) throws AFSException
  {
    rename( cell.getCellHandle(), getID(), newName );
    name = newName;
  }

  /**
   * Salvages (restores consistency to) this volume.  Uses default values for
   * most salvager options in order to simplify the API.
   *
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void salvage() throws AFSException
  {
    Server.salvage( cell.getCellHandle(), server.getBosHandle(), 
		    partition.getName(), name, 4, null, null, false, false, 
		    false, false, false, false );
  }

  /**
   * Creates a read-write mount point for this volume.  Does not ensure the 
   * volume already exists.
   *
   * @param directory   the name of the directory where this volume 
   * should be mounted
   * @exception AFSException  If an error occurs in the native code
   */
  public void createMountPoint( String directory ) throws AFSException
  {
    createMountPoint(directory, true); 
  }

  /**
   * Creates a mount point for this volume.  Does not ensure the volume 
   * already exists.
   *
   * @param directory   the name of the directory where this volume should be 
   *                    mounted
   * @param readWrite   whether or not this mount point should be read-write
   * @exception AFSException  If an error occurs in the native code
   */
  public void createMountPoint( String directory, boolean readWrite ) 
    throws AFSException
  {
    Cell.createMountPoint( cell.getCellHandle(), directory, getName(), 
			   readWrite, false ); 
  }

  //////////////// accessors:  ////////////////////////

  /**
   * Returns the name of this volume.
   *
   * @return the name of this volume
   */
  public String getName()
  {
    return name;
  }

  /**
   * Returns this volume's hosting partition.
   *
   * @return this volume's partition
   */
  public Partition getPartition()
  {
    return partition;
  }

  /**
   * Returns the id of this volume.   
   *
   * @exception AFSException  If an error occurs in the native code
   * @return the id of this volume
   */
  public int getID() throws AFSException
  {
    if( id == -1 && name != null ) {
	String nameNoSuffix;
	if( name.endsWith( "backup" ) ) {
	    type = VOLUME_TYPE_BACKUP;
	    nameNoSuffix = name.substring( 0, name.lastIndexOf( '.' ) );
	} else if( name.endsWith( "readonly" ) ) {
	    type = VOLUME_TYPE_READ_ONLY;
	    nameNoSuffix = name.substring( 0, name.lastIndexOf( '.' ) );
	} else {
	    type = VOLUME_TYPE_READ_WRITE;
	    nameNoSuffix = name;
	}
	id = translateNameToID( cell.getCellHandle(), 
				     nameNoSuffix, type );
    }
    return id;
  }

  /**
   * Returns the read-write ID of this volume
   *
   * @exception AFSException  If an error occurs in the native code
   * @return the read-write id
   */
  public int getReadWriteID() throws AFSException
  {
    if( !cachedInfo ) {
      refreshInfo();
    }
    return readWriteID;
  }

  /**
   * Returns the read-only ID of this volume
   *
   * @exception AFSException  If an error occurs in the native code
   * @return the read-only id
   */
  public int getReadOnlyID() throws AFSException
  {
    if( !cachedInfo ) {
      refreshInfo();
    }
    return readOnlyID;
  }

  /**
   * Returns the backup ID of this volume
   *
   * @exception AFSException  If an error occurs in the native code
   * @return the backup id
   */
  public int getBackupID() throws AFSException
  {
    if( !cachedInfo ) {
      refreshInfo();
    }
    return backupID;
  }

  /**
   * Returns the date the volume was created
   *
   * @return the date the volume was created
   * @exception AFSException  If an error occurs in the native code
   */
  public GregorianCalendar getCreationDate() throws AFSException
  {
    if( !cachedInfo ) {
      refreshInfo();
    }
    if( creationDateCal == null ) {
      // make it into a date . . .
      creationDateCal = new GregorianCalendar();
      Date d = new Date( creationDate*1000 );
      creationDateCal.setTime( d );
    }
    return creationDateCal;
  }

  /**
   * Returns the date the volume was last updated.
   * After this method is called once, it saves the date 
   * and returns that date on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @return the date the volume was last updated
   * @exception AFSException  If an error occurs in the native code
   */
  public GregorianCalendar getLastUpdateDate() throws AFSException
  {
    if( !cachedInfo ) {
      refreshInfo();
    }
    if( lastUpdateDateCal == null ) {
      // make it into a date . . .
      lastUpdateDateCal = new GregorianCalendar();
      Date d = new Date( lastUpdateDate*1000 );
      lastUpdateDateCal.setTime( d );
    }
    return lastUpdateDateCal;
  }

  /**
   * Returns the date the volume was copied.
   * After this method is called once, it saves the date 
   * and returns that date on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @return the date the volume was copied
   * @exception AFSException  If an error occurs in the native code
   */
  public GregorianCalendar getCopyCreationDate() throws AFSException
  {
    if( !cachedInfo ) {
      refreshInfo();
    }
    if( copyCreationDateCal == null ) {
      // make it into a date . . .
      copyCreationDateCal = new GregorianCalendar();
      Date d = new Date( copyCreationDate*1000 );
      copyCreationDateCal.setTime( d );
    }
    return copyCreationDateCal;
  }

  /**
   * Returns the number of accesses since midnight.
   * After this method is called once, it saves the value 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return the number of accesses since midnight
   */
  public int getAccessesSinceMidnight() throws AFSException
  {
    if( !cachedInfo ) {
	  refreshInfo();
    }
    return accessesSinceMidnight;
  }

  /**
   * Returns file count.
   * After this method is called once, it saves the value 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return the file count
   */
  public int getFileCount() throws AFSException
  {
    if( !cachedInfo ) {
	  refreshInfo();
    }
    return fileCount;
  }

  /**
   * Returns current volume size in K.
   * After this method is called once, it saves the value 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return the current volume size in K
   */
  public int getCurrentSize() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return currentSize;
  }

  /**
   * Returns the difference between quota and current volume size (in K).
   *
   * <P> Please note: the product of this method is <U>not</U> saved.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return the current free space in K
   */
  public int getTotalFreeSpace() throws AFSException
  {
    if ( !cachedInfo ) refreshInfo();
    return (maxQuota - currentSize);
  }

  /**
   * Returns this volume's quota, expressed in kilobyte blocks (1024 
   * kilobyte blocks equal one megabyte). After this method is called once, 
   * it saves the value and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * <P><B>Note:</B> A quota value of zero, "0", grants an unlimited quota
   * in AFS.  Consequently, to avoid delusion this method will throw an 
   * {@link AFSException} if the returning value is zero.
   *
   * @exception AFSException  If an error occurs in the native code or
   *                               this volume's quota is configured as
   *                               unlimited.
   * @return the volume quota in K
   * @see #isQuotaUnlimited()
   */
  public int getQuota() throws AFSException
  {
    if ( !cachedInfo ) refreshInfo();
    if (maxQuota == 0) {
      throw new AFSException("Volume with id " + id + 
                                  " has an unlimited quota configured.", 0);
    }
    return maxQuota;
  }

  /**
   * Tests whether this volume's quota is configured as unlimited.
   *
   * <P>After this method is called once, it saves the value and returns 
   * that value on subsequent calls, until the {@link #refresh()} 
   * method is called and a more current value is obtained.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return <CODE>true</CODE> if this volume's quota is configured as
   *         unlimited; otherwise <CODE>false</CODE>.
   * @see #getQuota()
   */
  public boolean isQuotaUnlimited() throws AFSException
  {
    if ( !cachedInfo ) refreshInfo();
    return (maxQuota == 0);
  }

  /**
   * Returns volume status. Possible values are:<ul>
   *       <li>{@link #VOLUME_OK}</li>
   *       <li>{@link #VOLUME_SALVAGE}</li>
   *       <li>{@link #VOLUME_NO_VNODE}</li>
   *       <li>{@link #VOLUME_NO_VOL}</li>
   *       <li>{@link #VOLUME_VOL_EXISTS}</li>
   *       <li>{@link #VOLUME_NO_SERVICE}</li>
   *       <li>{@link #VOLUME_OFFLINE}</li>
   *       <li>{@link #VOLUME_ONLINE}</li>
   *       <li>{@link #VOLUME_DISK_FULL}</li>
   *       <li>{@link #VOLUME_OVER_QUOTA}</li>
   *       <li>{@link #VOLUME_BUSY}</li>
   *       <li>{@link #VOLUME_MOVED}</li></ul>
   * Typical value is VOLUME_OK.
   * After this method is called once, it saves the value 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return volume status
   */
  public int getStatus() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return status;
  }

  /**
   * Returns volume disposition. Possible values are:<ul>
   *       <li>{@link #VOLUME_OK}</li>
   *       <li>{@link #VOLUME_SALVAGE}</li>
   *       <li>{@link #VOLUME_NO_VNODE}</li>
   *       <li>{@link #VOLUME_NO_VOL}</li>
   *       <li>{@link #VOLUME_VOL_EXISTS}</li>
   *       <li>{@link #VOLUME_NO_SERVICE}</li>
   *       <li>{@link #VOLUME_OFFLINE}</li>
   *       <li>{@link #VOLUME_ONLINE}</li>
   *       <li>{@link #VOLUME_DISK_FULL}</li>
   *       <li>{@link #VOLUME_OVER_QUOTA}</li>
   *       <li>{@link #VOLUME_BUSY}</li>
   *       <li>{@link #VOLUME_MOVED}</li></ul>
   * Typical value is VOLUME_ONLINE.
   * After this method is called once, it saves the value 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return volume disposition
   */
  public int getDisposition() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return disposition;
  }

  /**
   * Returns volume type. Possible values are:<ul>
   *       <li>{@link #VOLUME_TYPE_READ_WRITE}</li>
   *       <li>{@link #VOLUME_TYPE_READ_ONLY}</li>
   *       <li>{@link #VOLUME_TYPE_BACKUP}</li></ul>
   *
   * @exception AFSException  If an error occurs in the native code
   * @return volume type
   */
  public int getType() throws AFSException
  {
    if( !cachedInfo ) refreshInfo();
    return type;
  }

  //////////////// mutators:  ////////////////////////

  /**
   * Sets quota of volume, 0 denotes an unlimited quota.
   *
   * @exception AFSException  If an error occurs in the native code
   * @param quota  the new volume quota in K (0 for unlimited)
   */
  public void setQuota( int quota ) throws AFSException
  {
    this.changeQuota( cell.getCellHandle(), server.getVosHandle(), 
		      partition.getID(), getID(), quota );
    maxQuota = quota;
  }

  /////////////// custom information methods ////////////////////

  /**
   * Returns a <code>String</code> representation of this <code>Volume</code>.
   * Contains the information fields.
   *
   * @return a <code>String</code> representation of the <code>Volume</code>
   */
  public String getInfo()
  {
      String r;
      try {
	r = "Volume: " + name + "\tid: " + getID() + "\n";
	
	r += "\tread-write id: " + getReadWriteID() + "\tread-only id: " 
	  + getReadOnlyID() + "\n";
	r += "\tbackup id: " + getBackupID() + "\n";

	r += "\tcreation date: " + getCreationDate().getTime() + "\n";
	r += "\tlast update date: " + getLastUpdateDate().getTime() + "\n";
	r += "\tcopy creation date: " + getCopyCreationDate().getTime() + "\n";
	r += "\taccesses since midnight: " + getAccessesSinceMidnight() + "\n";
	r += "\tfile count: " + getFileCount() + "\n";
	r += "\tcurrent size: " + getCurrentSize() + " K\tquota: " + 
	  getQuota() + " K\n";
	r += "\tstatus: ";
	switch( getStatus() ) {
	case VOLUME_OK: 
	    r += "OK";
	    break;
	default:
	    r += "OTHER";
	}

	r += "\tdisposition: ";
	switch( getDisposition() ) {
	case VOLUME_ONLINE: 
	    r += "ONLINE";
	    break;
	default:
	    r += "OTHER - " + getDisposition();
	}
	r += "\n";

	r += "\ttype: ";
	switch( getType() ) {
	case VOLUME_TYPE_READ_WRITE: 
	    r += "read-write";
	    break;
	case VOLUME_TYPE_READ_ONLY: 
	    r += "read-only";
	    break;
	case VOLUME_TYPE_BACKUP: 
	    r += "backup";
	    break;
	default:
	    r += "OTHER";
	}
	r += "\n";

      } catch( Exception e ) {
	return e.toString();
      }
      return r;
  }

  /////////////// custom override methods ////////////////////

  /**
   * Compares two Volume objects respective to their names and does not
   * factor any other attribute.    Alphabetic case is significant in 
   * comparing names.
   *
   * @param     volume    The Volume object to be compared to this Volume 
   *                      instance
   * 
   * @return    Zero if the argument is equal to this Volume's name, a
   *		value less than zero if this Volume's name is
   *		lexicographically less than the argument, or a value greater
   *		than zero if this Volume's name is lexicographically
   *		greater than the argument
   */
  public int compareTo(Volume volume)
  {
    return this.getName().compareTo(volume.getName());
  }

  /**
   * Comparable interface method.
   *
   * @see #compareTo(Volume)
   */
  public int compareTo(Object obj)
  {
    return compareTo((Volume)obj);
  }

  /**
   * Tests whether two <code>Volume</code> objects are equal, based on their 
   * names and hosting partition.
   *
   * @param otherVolume   the Volume to test
   * @return whether the specifed Volume is the same as this Volume
   */
  public boolean equals( Volume otherVolume )
  {
    return ( name.equals(otherVolume.getName()) ) &&
           ( this.getPartition().equals(otherVolume.getPartition()) );
  }

  /**
   * Returns the name of this <CODE>Volume</CODE>
   *
   * @return the name of this <CODE>Volume</CODE>
   */
  public String toString()
  {
    return getName();
  }


  /////////////// native methods ////////////////////

  /**
   * Fills in the information fields of the provided <code>Volume</code>.
   *
   * @param cellHandle    the handle of the cell to which the volume belongs
   * @see Cell#getCellHandle
   * @param serverHandle  the vos handle of the server on which the volume 
   *                      resides
   * @see Server#getVosServerHandle
   * @param partition   the numeric id of the partition on which the volume 
   *                    resides
   * @param volId  the numeric id of the volume for which to get the info
   * @param theVolume   the {@link Volume Volume} object in which to fill in 
   *                    the information
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void getVolumeInfo( long cellHandle, long serverHandle,
					      int partition, int volId, 
					      Volume theVolume ) 
	throws AFSException;

  /**
   * Creates a volume on a particular partition.
   *
   * @param cellHandle    the handle of the cell in which to create the volume
   * @see Cell#getCellHandle
   * @param serverHandle  the vos handle of the server on which to create 
   *                      the volume
   * @see Server#getVosServerHandle
   * @param partition   the numeric id of the partition on which to create 
   *                    the volume
   * @param volumeName   the name of the volume to create
   * @param quota    the amount of space (in KB) to set as this volume's quota
   * @return the numeric ID assigned to the volume
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native int create( long cellHandle, long serverHandle, 
				      int partition, String volumeName, 
				      int quota ) 
	throws AFSException;

  /**
   * Deletes a volume from a particular partition.
   *
   * @param cellHandle    the handle of the cell in which to delete the volume
   * @see Cell#getCellHandle
   * @param serverHandle  the vos handle of the server from which to delete 
   *                      the volume
   * @see Server#getVosServerHandle
   * @param partition   the numeric id of the partition from which to delete 
   *                    the volume
   * @param volId   the numeric id of the volume to delete
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void delete( long cellHandle, long serverHandle, 
				       int partition, int volId ) 
	throws AFSException;

  /**
   * Creates a backup volume for the specified regular volume.
   *
   * @param cellHandle    the handle of the cell to which the volume belongs
   * @param volId  the numeric id of the volume for which to create a backup 
   *               volume
   * @see Cell#getCellHandle
   */
  protected static native void createBackupVolume( long cellHandle, int volId )
	throws AFSException;

  /**
   * Creates a read-only volume for the specified regular volume.
   *
   * @param cellHandle    the handle of the cell to which the volume belongs
   * @param serverHandle  the vos handle of the server on which the read-only 
   *                      volume is to reside 
   * @see Server#getVosServerHandle
   * @param partition   the numeric id of the partition on which the read-only 
                        volume is to reside 
   * @param volId  the numeric id of the volume for which to create a read-only volume
   * @see Cell#getCellHandle
   */
  protected static native void createReadOnlyVolume( long cellHandle, 
						     long serverHandle, 
						     int partition, int volId )
	throws AFSException;

  /**
   * Deletes a read-only volume for the specified regular volume.
   *
   * @param cellHandle    the handle of the cell to which the volume belongs
   * @param serverHandle  the vos handle of the server on which the read-only 
   *                      volume residea 
   * @see Server#getVosServerHandle
   * @param partition   the numeric id of the partition on which the read-only
   *                     volume resides 
   * @param volId  the numeric read-write id of the volume for which to 
   *               delete the read-only volume
   * @see Cell#getCellHandle
   */
  protected static native void deleteReadOnlyVolume( long cellHandle, 
						     long serverHandle, 
						     int partition, int volId )
	throws AFSException;

  /**
   * Changes the quota of the specified volume.
   *
   * @param cellHandle    the handle of the cell to which the volume belongs
   * @see Cell#getCellHandle
   * @param serverHandle  the vos handle of the server on which the volume 
   *                      resides
   * @see Server#getVosServerHandle
   * @param partition   the numeric id of the partition on which the volume 
   *                    resides
   * @param volId  the numeric id of the volume for which to change the quota
   * @param newQuota    the new quota (in KB) to assign the volume
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void changeQuota( long cellHandle, long serverHandle, 
					    int partition, int volId, 
					    int newQuota ) 
	throws AFSException;

  /**
   * Move the specified volume to a different site.
   *
   * @param cellHandle    the handle of the cell to which the volume belongs
   * @see Cell#getCellHandle
   * @param fromServerHandle  the vos handle of the server on which the volume
   *                          currently resides
   * @see Server#getVosServerHandle
   * @param fromPartition   the numeric id of the partition on which the volume
   *                        currently resides
   * @param toServerHandle  the vos handle of the server to which the volume 
   *                        should be moved
   * @param toPartition   the numeric id of the partition to which the volume 
   *                      should be moved
   * @param volId  the numeric id of the volume to move
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void move( long cellHandle, long fromServerHandle, 
				     int fromPartition, long toServerHandle, 
				     int toPartition, int volId ) 
	throws AFSException;

  /**
   * Releases the specified volume that has readonly volume sites.
   *
   * @param cellHandle    the handle of the cell to which the volume belongs
   * @param volId  the numeric id of the volume to release
   * @param forceComplete  whether or not to force a complete release
   * @see Cell#getCellHandle
   */
  protected static native void release( long cellHandle, int volId, 
					boolean forceComplete )
	throws AFSException;

  /**
   * Dumps the specified volume to a file.
   *
   * @param cellHandle    the handle of the cell to which the volume belongs
   * @see Cell#getCellHandle
   * @param serverHandle  the vos handle of the server on which the volume 
   *                      resides
   * @see Server#getVosServerHandle
   * @param partition   the numeric id of the partition on which the 
   *                    volume resides
   * @param volId  the numeric id of the volume to dump
   * @param startTime   files with a modification time >= to this time will 
   *                    be dumped
   * @param dumpFile   the full path of the file to which to dump
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void dump( long cellHandle, long serverHandle, 
				     int partition, int volId, int startTime, 
				     String dumpFile ) 
	throws AFSException;

  /**
   * Restores the specified volume from a dump file.
   *
   * @param cellHandle    the handle of the cell to which the volume belongs
   * @see Cell#getCellHandle
   * @param serverHandle  the vos handle of the server on which the volume is 
   *                      to reside
   * @see Server#getVosServerHandle
   * @param partition   the numeric id of the partition on which the volume is
   *                    to reside
   * @param volId  the numeric id to assign the restored volume (can be 0)
   * @param volumeName  the name of the volume to restore as
   * @param dumpFile   the full path of the dump file from which to restore
   * @param incremental  if true, restores an incremental dump over an existing
   *                     volume (server and partition values must correctly 
   *                     indicate the current position of the existing volume),
   *                     otherwise restores a full dump
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void restore( long cellHandle, long serverHandle, 
					int partition, int volId, 
					String volumeName, String dumpFile, 
					boolean incremental ) 
	throws AFSException;

  /**
   * Renames the specified read-write volume.
   *
   * @param cellHandle    the handle of the cell to which the volume belongs
   * @see Cell#getCellHandle
   * @param volId  the numeric id of the read-write volume to rename
   * @param newVolumeName  the new name for the volume
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void rename( long cellHandle, int volId, 
				       String newVolumeName ) 
	throws AFSException;

  /**
   * "Mounts" the specified volume, bringing it online.
   *
   * @param serverHandle  the vos handle of the server on which the volume 
   *                      resides
   * @see Server#getVosServerHandle
   * @param partition   the numeric id of the partition on which the volume 
   *                    resides
   * @param volId  the numeric id of the volume to bring online
   * @param sleepTime  ?  (not sure what this is yet, possibly a time to wait 
   *                      before brining it online)
   * @param offline   ?  (not sure what this is either, probably the current 
   *                     status of the volume -- busy or offline)
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void mount( long serverHandle, int partition, 
				      int volId, int sleepTime, 
				      boolean offline ) 
	throws AFSException;

  /**
   * "Unmounts" the specified volume, bringing it offline.
   *
   * @param serverHandle  the vos handle of the server on which the volume 
   *                      resides
   * @see Server#getVosServerHandle
   * @param partition   the numeric id of the partition on which the volume 
   *                    resides
   * @param volId  the numeric id of the volume to bring offline
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void unmount( long serverHandle, int partition, 
					int volId ) 
	throws AFSException;

  /**
   * Locks the VLDB entry specified volume
   *
   * @param cellHandle  the handle of the cell on which the volume resides
   * @see Cell#getCellHandle
   * @param volId  the numeric id of the volume to lock
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void lock( long cellHandle, int volId ) 
	throws AFSException;

  /**
   * Unlocks the VLDB entry of the specified volume
   *
   * @param cellHandle  the handle of the cell on which the volume resides
   * @see Cell#getCellHandle
   * @param volId  the numeric id of the volume to unlock
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void unlock( long cellHandle, int volId ) 
	throws AFSException;

  /**
   * Translates a volume name into a volume id
   *
   * @param cellHandle    the handle of the cell to which the volume belongs
   * @see Cell#getCellHandle
   * @param name  the name of the volume in question, cannot end in backup or
   *              readonly
   * @param type  the type of volume: read-write, read-only, or backup.  
   *              Acceptable values are:<ul>
   *              <li>{@link #VOLUME_TYPE_READ_WRITE}</li>
   *              <li>{@link #VOLUME_TYPE_READ_ONLY}</li>
   *              <li>{@link #VOLUME_TYPE_BACKUP}</li></ul> 
   * @return   the id of the volume in question
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native int translateNameToID( long cellHandle, String name, 
						 int volumeType )
    throws AFSException;

  /**
   * Reclaims all memory being saved by the volume portion of the native 
   * library. This method should be called when no more <code>Volume</code> 
   * objects are expected to be used.
   */
  protected static native void reclaimVolumeMemory();
}













