/*
 * @(#)Partition.java	1.0 6/29/2001
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

/**
 * An abstract representation of an AFS partition.  It holds information about 
 * the partition, such as what its total space is.
 * <BR><BR>
 *
 * Constructing an instance of a <code>Partition</code> does not mean 
 * an actual AFS partition is created on a server -- on the contrary, 
 * a <code>Partition</code> object must be a representation of an already 
 * existing AFS partition.  There is no way to create a new AFS partition 
 * through this API.<BR><BR>
 *
 * Each <code>Partition</code> object has its own individual set of
 * <code>Volume</code>s. This represents the properties and attributes 
 * of an actual AFS cell.<BR><BR>
 *
 * <!--Example of how to use class-->
 * The following is a simple example of how to obtain and use a 
 * <code>Partition</code> object.  In this example, a list of the 
 * <code>Partition</code> objects of a server are obtained, and the name
 * and number of volumes is printed out for each one.<BR><BR>
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
 *     Partition[] partitions = server.getPartitions();
 *     for (int i = 0; i < partitions.length; i++) {
 *       System.out.print("Partition " + partitions[i].getName());
 *       System.out.print("hosts " + partitions[i].getVolumeCount());
 *       System.out.print("volumes.\n");
 *     }
 *   }
 *   ...
 * }
 * </PRE>
 *
 */
public class Partition implements Serializable, Comparable
{
  protected Cell cell;
  protected Server server;

  /* Populated by native method */
  protected String name;

  /* Populated by native method */
  protected int id;

  /* Populated by native method */
  protected String deviceName;

  /* Populated by native method */
  protected int lockFileDescriptor;

  /* Populated by native method */
  protected int totalSpace;

  /* Populated by native method */
  protected int totalFreeSpace;

  protected int totalQuota;

  protected ArrayList volumes;
  protected ArrayList volumeNames;

  protected boolean cachedInfo;

  /**
   * Constructs a new <code>Partition</code> object instance given the 
   * name of the AFS partition and the AFS server, represented by 
   * <CODE>server</CODE>, to which it belongs.   This does not actually
   * create a new AFS partition, it just represents an existing one.
   * If <code>name</code> is not an actual AFS partition, exceptions
   * will be thrown during subsequent method invocations on this 
   * object.
   *
   * @param name      the name of the partition to represent 
   * @param server    the server on which the partition resides
   * @exception AFSException      If an error occurs in the native code
   */
  public Partition( String name, Server server ) throws AFSException
  {
    this.name = name;
    this.server = server;
    this.cell = server.getCell();
    
    id = -1;
    
    volumes = null;
    volumeNames = null;
    
    cachedInfo = false;
  }
  
  /**
   * Constructs a new <CODE>Partition</CODE> object instance given the name 
   * of the AFS partition and the AFS server, represented by 
   * <CODE>server</CODE>, to which it belongs.   This does not actually
   * create a new AFS partition, it just represents an existing one.
   * If <code>name</code> is not an actual AFS partition, exceptions
   * will be thrown during subsequent method invocations on this 
   * object.
   *
   * <P> This constructor is ideal for point-in-time representation and 
   * transient applications.  It ensures all data member values are set and 
   * available without calling back to the filesystem at the first request 
   * for them.  Use the {@link #refresh()} method to address any coherency 
   * concerns.
   *
   * @param name               the name of the partition to represent 
   * @param server             the server to which the partition belongs.
   * @param preloadAllMembers  true will ensure all object members are 
   *                           set upon construction;
   *                           otherwise members will be set upon access, 
   *                           which is the default behavior.
   * @exception AFSException      If an error occurs in the native code
   * @see #refresh
   */
  public Partition( String name, Server server, boolean preloadAllMembers ) 
      throws AFSException
  {
    this(name, server);
    if (preloadAllMembers) refresh(true);
  }
  
  /**
   * Creates a blank <code>Server</code> given the cell to which the partition
   * belongs and the server on which the partition resides.  This blank 
   * object can then be passed into other methods to fill out its properties.
   *
   * @exception AFSException      If an error occurs in the native code
   * @param cell       the cell to which the partition belongs.
   * @param server     the server on which the partition resides
   */
  Partition( Server server ) throws AFSException
  {
    this( null, server );
  }

  /*-------------------------------------------------------------------------*/

  /**
   * Refreshes the properties of this Partition object instance with values 
   * from the AFS partition
   * it represents.  All properties that have been initialized and/or 
   * accessed will be renewed according to the values of the AFS partition 
   * this Partition object instance represents.
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
   * Refreshes the properties of this Partition object instance with values 
   * from the AFS partition it represents.  If <CODE>all</CODE> is 
   * <CODE>true</CODE> then <U>all</U> of the properties of this Partition 
   * object instance will be set, or renewed, according to the values of the 
   * AFS partition it represents, disregarding any previously set properties.
   *
   * <P> Thus, if <CODE>all</CODE> is <CODE>false</CODE> then properties 
   * that are currently set will be refreshed and properties that are not 
   * set will remain uninitialized. See {@link #refresh()} for more 
   * information.
   *
   * @param all   if true set or renew all object properties; otherwise 
   * renew all set properties
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  protected void refresh(boolean all) throws AFSException
  {
    if (all || volumes != null) {
      refreshVolumes();
    }
    if (all || volumeNames != null) {
      refreshVolumeNames();
    }
    if (all || cachedInfo) {
      refreshInfo();
    }
  }

  /**
   * Refreshes the information fields of this <code>Partition</code> to 
   * reflect the current state of the AFS partition. These include total
   * free space, id, etc.
   *
   * @exception AFSException      If an error occurs in the native code
   */
  protected void refreshInfo() throws AFSException
  {
    getPartitionInfo( cell.getCellHandle(), server.getVosHandle(), getID(), 
		      this );
    cachedInfo = true;
  }

  /**
   * Obtains the most current list of <code>Volume</code> objects of this 
   * partition.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshVolumes() throws AFSException
  {
    Volume currVolume;

    long iterationID = getVolumesBegin( cell.getCellHandle(), 
				       server.getVosHandle(), getID() );

    volumes = new ArrayList();
	
    currVolume = new Volume( this );
    while( getVolumesNext( iterationID, currVolume ) != 0 ) {
      volumes.add( currVolume );
      currVolume = new Volume( this );
    } 
    getVolumesDone( iterationID );
  }

  /**
   * Obtains the most current list of volume names of this partition.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected void refreshVolumeNames() throws AFSException
  {
    String currName;

    long iterationID = getVolumesBegin( cell.getCellHandle(), 
				       server.getVosHandle(), getID() );
	
    volumeNames = new ArrayList();
	
    while( ( currName = getVolumesNextString( iterationID ) ) != null ) {
      volumeNames.add( currName );
    } 
    getVolumesDone( iterationID );
  }

  /**
   * Syncs this partition to the VLDB.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  public void syncPartition() throws AFSException
  {
    server.syncServerWithVLDB( cell.getCellHandle(), server.getVosHandle(), 
			       getID() );
  }

  /**
   * Syncs the VLDB to this partition.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  public void syncVLDB() throws AFSException
  {
    server.syncVLDBWithServer( cell.getCellHandle(), server.getVosHandle(), 
			       getID(), false );
  }

  /**
   * Salvages (restores consistency to) this partition. Uses default values for
   * most salvager options in order to simplify the API.
   *
   * @exception AFSException  If an error occurs in the native code
   */ 
  public void salvage() throws AFSException
  {
    server.salvage( cell.getCellHandle(), server.getBosHandle(), name, null, 
		    4, null, null, false, false, false, false, false, false );
  }

  //////////////// accessors:  ////////////////////////

  /**
   * Returns the name of this partition.
   *
   * @return the name of this partition
   */
  public String getName()
  {
    return name;
  }

  /**
   * Returns this partition's hosting server.
   *
   * @return this partition's server
   */
  public Server getServer()
  {
    return server;
  }

  /**
   * Returns the number of volumes contained in this partition.
   *
   * <P>If the total list of volumes or volume names have already been
   * collected (see {@link #getVolumes()}), then the returning value will
   * be calculated based upon the current list.  Otherwise, AFS will be
   * explicitly queried for the information.
   *
   * <P> The product of this method is not saved, and is recalculated
   * with every call.
   *
   * @return the number of volumes contained in this partition.
   * @exception AFSException  If an error occurs in any 
   *                               of the associated native methods
   */
  public int getVolumeCount() throws AFSException
  {
    if (volumes != null) {
      return volumes.size();
    } else if (volumeNames != null) {
      return volumeNames.size();
    } else {
      return getVolumeCount( cell.getCellHandle(), 
				       server.getVosHandle(), getID() );
    }
  }

  /**
   * Retrieves the <CODE>Volume</CODE> object (which is an abstract 
   * representation of an actual AFS volume of this partition) designated 
   * by <code>name</code> (i.e. "root.afs", etc.).  If a volume by 
   * that name does not actually exist in AFS on the partition
   * represented by this object, an {@link AFSException} will be
   * thrown.
   *
   * @exception AFSException  If an error occurs in the native code
   * @exception NullPointerException  If <CODE>name</CODE> is 
   *                                  <CODE>null</CODE>.
   * @param name the name of the volume to retrieve
   * @return <CODE>Volume</CODE> designated by <code>name</code>.
   */
  public Volume getVolume(String name) throws AFSException
  {
    if (name == null) throw new NullPointerException();
    Volume volume = new Volume(name, this);
    return volume;
  }

  /**
   * Retrieves an array containing all of the <code>Volume</code> objects 
   * associated with this <code>Partition</code>, each of which is an 
   * abstract representation of an actual AFS volume of the AFS partition.
   * After this method is called once, it saves the array of 
   * <code>Volume</code>s and returns that saved array on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current list 
   * is obtained.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return a <code>Volume</code> array of the <code>Volume</code> 
   *         objects of the partition.
   * @see #refresh()
   */
  public Volume[] getVolumes() throws AFSException
  {
    if (volumes == null) refreshVolumes();
    return (Volume[]) volumes.toArray( new Volume[volumes.size()] );
  }

  /**
   * Returns an array containing a subset of the <code>Volume</code> objects
   * associated with this <code>Partition</code>, each of which is an abstract
   * representation of an actual AFS volume of the AFS partition.  The subset
   * is a point-in-time list of volumes (<code>Volume</code> objects
   * representing AFS volumes) starting at the complete array's index of
   * <code>startIndex</code> and containing up to <code>length</code>
   * elements.
   *
   * If <code>length</code> is larger than the number of remaining elements, 
   * respective to <code>startIndex</code>, then this method will
   * ignore the remaining positions requested by <code>length</code> and 
   * return an array that contains the remaining number of elements found in 
   * this partition's complete array of volumes.
   *
   * <P>This method is especially useful when managing iterations of very
   * large lists.  {@link #getVolumeCount()} can be used to determine if
   * iteration management is practical.
   *
   * <P>This method does not save the resulting data and therefore 
   * queries AFS for each call.
   *
   * <P><B>Example:</B> If there are more than 50,000 volumes within this partition
   * then only render them in increments of 10,000.
   * <PRE>
   * ...
   *   Volume[] volumes;
   *   if (partition.getVolumeCount() > 50000) {
   *     int index = 0;
   *     int length = 10000;
   *     while (index < partition.getVolumeCount()) {
   *       volumes = partition.<B>getVolumes</B>(index, length);
   *       for (int i = 0; i < volumes.length; i++) {
   *         ...
   *       }
   *       index += length;
   *       ...
   *     }
   *   } else {
   *     volumes = partition.getVolumes();
   *     for (int i = 0; i < volumes.length; i++) {
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
   * @return a subset array of volumes hosted by this partition
   * @exception AFSException  If an error occurs in the native code
   * @see #getVolumeCount()
   * @see #getVolumeNames(int, int)
   * @see #getVolumes()
   */
  public Volume[] getVolumes(int startIndex, int length) throws AFSException
  {
    Volume[] volumes  = new Volume[length];
    Volume currVolume = new Volume( this );
    int i = 0;

    long iterationID = getVolumesBeginAt( cell.getCellHandle(), 
				       server.getVosHandle(), getID(), startIndex );

    while( getVolumesNext( iterationID, currVolume ) != 0 && i < length ) {
      volumes[i] = currVolume;
      currVolume = new Volume( this );
      i++;
    } 
    getVolumesDone( iterationID );
    if (i < length) {
      Volume[] v = new Volume[i];
      System.arraycopy(volumes, 0, v, 0, i);
      return v;
    } else {
      return volumes;
    }
  }

  /**
   * Retrieves an array containing all of the names of volumes
   * associated with this <code>Partition</code>. 
   * After this method is called once, it saves the array of 
   * <code>String</code>s and returns that saved array on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * list is obtained.
   *
   * @return a <code>String</code> array of the volumes of the partition.
   * @exception AFSException  If an error occurs in the native code
   * @see #refresh()
   */
  public String[] getVolumeNames() throws AFSException
  {
    if (volumeNames == null) refreshVolumeNames();
    return (String []) volumeNames.toArray( new String[volumeNames.size() ] );
  }

  /**
   * Returns an array containing a subset of the names of volumes
   * associated with this <code>Partition</code>.  The subset is a 
   * point-in-time list of volume names starting at the complete array's 
   * index of <code>startIndex</code> and containing up to <code>length</code>
   * elements.
   *
   * If <code>length</code> is larger than the number of remaining elements, 
   * respective to <code>startIndex</code>, then this method will
   * ignore the remaining positions requested by <code>length</code> and 
   * return an array that contains the remaining number of elements found in 
   * this partition's complete array of volume names.
   *
   * <P>This method is especially useful when managing iterations of very
   * large lists.  {@link #getVolumeCount()} can be used to determine if
   * iteration management is practical.
   *
   * <P>This method does not save the resulting data and therefore 
   * queries AFS for each call.
   *
   * <P><B>Example:</B> If there are more than 50,000 volumes within this partition
   * then only render them in increments of 10,000.
   * <PRE>
   * ...
   *   String[] volumes;
   *   if (partition.getVolumeCount() > 50000) {
   *     int index = 0;
   *     int length = 10000;
   *     while (index < partition.getVolumeCount()) {
   *       volumes = partition.<B>getVolumeNames</B>(index, length);
   *       for (int i = 0; i < volumes.length; i++) {
   *         ...
   *       }
   *       index += length;
   *       ...
   *     }
   *   } else {
   *     volumes = partition.getVolumeNames();
   *     for (int i = 0; i < volumes.length; i++) {
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
   * @return a subset array of volume names hosted by this partition
   * @exception AFSException  If an error occurs in the native code
   * @see #getVolumeCount()
   * @see #getVolumes(int, int)
   * @see #getVolumes()
   */
  public String[] getVolumeNames(int startIndex, int length) throws AFSException
  {
    String[] volumes  = new String[length];
    String currName;
    int i = 0;

    long iterationID = getVolumesBeginAt( cell.getCellHandle(), 
				       server.getVosHandle(), getID(), startIndex );

    while( ( currName = getVolumesNextString( iterationID ) ) != null && i < length ) {
      volumes[i] = currName;
      i++;
    } 
    getVolumesDone( iterationID );
    if (i < length) {
      String[] v = new String[i];
      System.arraycopy(volumes, 0, v, 0, i);
      return v;
    } else {
      return volumes;
    }
  }

  /**
   * Returns the id of this partition (i.e. "vicepa" = 0, etc.)
   *
   * @exception AFSException  If an error occurs in the native code
   * @return the id of this partition
   */
  public int getID() throws AFSException
  {
    if (id == -1) id = translateNameToID( name );
    return id;
  }

  /**
   * Returns the device name of this partition (i.e. "hda5", etc.)
   *
   * @exception AFSException  If an error occurs in the native code
   * @return the device name of this partition
   * @see #refresh()
   */
  public String getDeviceName() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return deviceName;
  }

  /**
   * Returns the lock file descriptor of this partition
   *
   * @exception AFSException  If an error occurs in the native code
   * @return the lock file descriptor of this partition
   * @see #refresh()
   */
  public int getLockFileDescriptor() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return lockFileDescriptor;
  }

  /**
   * Returns the total space on this partition.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return the total space on this partition
   * @see #refresh()
   */
  public int getTotalSpace() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return totalSpace;
  }

  /**
   * Returns the total free space on this partition.
   * After this method is called once, it saves the total free space 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return the total free space on this partition
   * @see #refresh()
   */
  public int getTotalFreeSpace() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return totalFreeSpace;
  }

  /**
   * Returns the total used space on this partition.
   * After this method is called once, it saves the total used space 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return the total used space on this partition
   * @see #refresh()
   */
  public int getUsedSpace() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return (totalSpace - totalFreeSpace);
  }

  /**
   * Returns the total combined quota of all volumes on this partition,
   * unless a volume has configured an unlimited quota at which case an
   * {@link AFSException} is thrown.
   *
   * <P>After this method is called once, it saves the value and returns 
   * that value on subsequent calls, until the {@link #refresh()} 
   * method is called and a more current value is obtained.
   *
   * @exception AFSException  If an error occurs while retrieving and 
   *                               calculating, or a volume has an 
   *                               unlimited quota.
   * @return the total combined quota of all volumes on this partition
   * @see #getTotalQuota(boolean)
   * @see #hasVolumeWithUnlimitedQuota()
   * @see Volume#getQuota()
   */
  public int getTotalQuota() throws AFSException
  {
    return getTotalQuota(false);
  }

  /**
   * Returns the total combined quota of all volumes on this partition,
   * ignoring volumes with unlimited quotas, if <CODE>
   * ignoreUnlimitedQuotas</CODE> is <CODE>true</CODE>; otherwise an
   * {@link AFSException} is thrown if a volume has an unlimited quota.
   *
   * <P>After this method is called once, it saves the value and returns 
   * that value on subsequent calls, until the {@link #refresh()} 
   * method is called and a more current value is obtained.
   *
   * @exception AFSException  If an error occurs while retrieving and 
   *                               calculating, or a volume has an 
   *                               unlimited quota.
   * @return the total combined quota of all volumes on this partition
   * @see #getTotalQuota()
   * @see #hasVolumeWithUnlimitedQuota()
   * @see Volume#getQuota()
   * @see #refresh()
   */
  public int getTotalQuota(boolean ignoreUnlimitedQuotas) 
    throws AFSException
  {
    if (volumes == null) refreshVolumes();
    if (totalQuota == 0 || !ignoreUnlimitedQuotas) {
      Volume[] volumes = getVolumes();
      for (int i = 0; i < volumes.length; i++) {
        try {
          totalQuota += volumes[i].getQuota();
        } catch (AFSException e) {
          if (!ignoreUnlimitedQuotas) {
            totalQuota = 0;
            throw e;
          }
        }
      }
    }
    return totalQuota;
  }

  /**
   * Tests whether this partition contains a volume that has an unlimited
   * quota configured.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return <CODE>true</CODE> if a contained volume's quota is configured
   *         as unlimited; otherwise <CODE>false</CODE>.
   * @see #getTotalQuota()
   * @see #getTotalQuota(boolean)
   * @see Volume#isQuotaUnlimited()
   * @see Volume#getQuota()
   * @see #refresh()
   */
  public boolean hasVolumeWithUnlimitedQuota() throws AFSException
  {
    if (volumes == null) refreshVolumes();
    Volume[] volumes = getVolumes();
    for (int i = 0; i < volumes.length; i++) {
      if (volumes[i].isQuotaUnlimited()) return true;
    }
    return false;
  }

  /////////////// custom information methods ////////////////////

  /**
   * Returns a <code>String</code> representation of this 
   * <code>Partition</code>.  Contains the information fields and a list of 
   * volumes.
   *
   * @return a <code>String</code> representation of the <code>Partition</code>
   */
  public String getInfo()
  {
    String r;
    
    try {
	
	r = "Partition: " + name + "\tid: " + getID() + "\n";
	
	r += "\tDevice name: " + getDeviceName() + "\n";
	r += "\tLock file descriptor: " + getLockFileDescriptor() + "\n";
	r += "\tTotal free space: " + getTotalFreeSpace() + " K\n";
	r += "\tTotal space: " + getTotalSpace() + " K\n";

	r += "\tVolumes:\n";

	String vols[] = getVolumeNames();

	for( int i = 0; i < vols.length; i++ ) {
	    r += "\t\t" + vols[i] + "\n";
	}

    } catch( Exception e ) {
	return e.toString();
    }
    return r;
  }

  /**
   * Returns a <code>String</code> containing the <code>String</code> 
   * representations of all the volumes of this <code>Partition</code>.
   *
   * @return    a <code>String</code> representation of the volumes
   * @see       Volume#getInfo
   */
  public String getInfoVolumes() throws AFSException
  {
	String r;

	r = "Partition: " + name + "\n\n";
	r += "--Volumes--\n";

	Volume vols[] = getVolumes();
	for( int i = 0; i < vols.length; i++ ) {
	    r += vols[i].getInfo() + "\n";
	}
	return r;
  }

  /////////////// custom override methods ////////////////////

  /**
   * Compares two Partition objects respective to their names and does not
   * factor any other attribute.    Alphabetic case is significant in 
   * comparing names.
   *
   * @param     partition    The Partition object to be compared to 
   *                         this Partition instance
   * 
   * @return    Zero if the argument is equal to this Partition's name, a
   *		value less than zero if this Partition's name is
   *		lexicographically less than the argument, or a value greater
   *		than zero if this Partition's name is lexicographically
   *		greater than the argument
   */
  public int compareTo(Partition partition)
  {
    return this.getName().compareTo(partition.getName());
  }

  /**
   * Comparable interface method.
   *
   * @see #compareTo(Partition)
   */
  public int compareTo(Object obj)
  {
    return compareTo((Partition)obj);
  }

  /**
   * Tests whether two <code>Partition</code> objects are equal, 
   * based on their names and hosting server.
   *
   * @param otherPartition   the Partition to test
   * @return whether the specifed Partition is the same as this Partition
   */
  public boolean equals( Partition otherPartition )
  {
    return ( name.equals(otherPartition.getName()) ) &&
           ( getServer().equals(otherPartition.getServer()) );
  }

  /**
   * Returns the name of this <CODE>Partition</CODE>
   *
   * @return the name of this <CODE>Partition</CODE>
   */
  public String toString()
  {
    return getName();
  }

  /////////////// native methods ////////////////////

  /**
   * Fills in the information fields of the provided <code>Partition</code>.
   *
   * @param cellHandle    the handle of the cell to which the partition belongs
   * @see Cell#getCellHandle
   * @param serverHandle  the vos handle of the server on which the 
   *                      partition resides
   * @see Server#getVosServerHandle
   * @param partition   the numeric id of the partition for which to get the 
   *                    info
   * @param thePartition   the {@link Partition Partition} object in which to 
   *                       fill in the information
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void getPartitionInfo( long cellHandle, 
						 long serverHandle, 
						 int partition, 
						 Partition thePartition ) 
    throws AFSException;

  /**
   * Returns the total number of volumes hosted by this partition.
   *
   * @param cellHandle    the handle of the cell to which the partition belongs
   * @param serverHandle  the vos handle of the server to which the partition 
   *                      belongs
   * @param partition   the numeric id of the partition on which the volumes 
   *                    reside
   * @return total number of volumes hosted by this partition
   * @exception AFSException  If an error occurs in the native code
   * @see Cell#getCellHandle
   * @see Server#getVosServerHandle
   */
  protected static native int getVolumeCount( long cellHandle, 
					       long serverHandle, 
					       int partition )
    throws AFSException;

  /**
   * Begin the process of getting the volumes on a partition.  Returns 
   * an iteration ID to be used by subsequent calls to 
   * <code>getVolumesNext</code> and <code>getVolumesDone</code>.  
   *
   * @param cellHandle    the handle of the cell to which the partition belongs
   * @see Cell#getCellHandle
   * @param serverHandle  the vos handle of the server to which the partition 
   *                      belongs
   * @see Server#getVosServerHandle
   * @param partition   the numeric id of the partition on which the volumes 
   *                    reside
   * @return an iteration ID
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native long getVolumesBegin( long cellHandle, 
					       long serverHandle, 
					       int partition )
    throws AFSException;

  /**
   * Begin the process of getting the volumes on a partition.  Returns 
   * an iteration ID to be used by subsequent calls to 
   * <code>getVolumesNext</code> and <code>getVolumesDone</code>.  
   *
   * @param cellHandle    the handle of the cell to which the partition belongs
   * @see Cell#getCellHandle
   * @param serverHandle  the vos handle of the server to which the partition 
   *                      belongs
   * @see Server#getVosServerHandle
   * @param partition   the numeric id of the partition on which the volumes 
   *                    reside
   * @return an iteration ID
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native long getVolumesBeginAt( long cellHandle, 
						 long serverHandle, 
						 int partition, int index )
    throws AFSException;

  /**
   * Returns the next volume of the partition.  Returns <code>null</code> 
   * if there are no more volumes.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see #getVolumesBegin
   * @return the name of the next volume of the server
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native String getVolumesNextString( long iterationId )
    throws AFSException;

  /**
   * Fills the next volume object of the partition.  Returns 0 if there
   * are no more volumes, != 0 otherwise.
   *
   * @param iterationId   the iteration ID of this iteration
   * @param theVolume    the Volume object in which to fill the values 
   *                     of the next volume
   * @see #getVolumesBegin
   * @return 0 if there are no more volumes, != 0 otherwise
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native int getVolumesNext( long iterationId, 
					      Volume theVolume )
    throws AFSException;

  /**
   * Fills the next volume object of the partition.  Returns 0 if there
   * are no more volumes, != 0 otherwise.
   *
   * @param iterationId   the iteration ID of this iteration
   * @param theVolume    the Volume object in which to fill the values of the 
   *                     next volume
   * @see #getVolumesBegin
   * @return 0 if there are no more volumes, != 0 otherwise
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native int getVolumesAdvanceTo( long iterationId, 
						   Volume theVolume, 
						   int advanceCount )
    throws AFSException;

  /**
   * Signals that the iteration is complete and will not be accessed anymore.
   *
   * @param iterationId   the iteration ID of this iteration
   * @see #getVolumesBegin
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void getVolumesDone( long iterationId )
    throws AFSException;

  /**
   * Translates a partition name into a partition id
   *
   * @param name  the name of the partition in question
   * @return   the id of the partition in question
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native int translateNameToID( String name )
    throws AFSException;

  /**
   * Translates a partition id into a partition name
   *
   * @param id  the id of the partition in question
   * @return   the name of the partition in question
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native String translateIDToName( int id )
    throws AFSException;

  /**
   * Reclaims all memory being saved by the partition portion of the native 
   * library. This method should be called when no more <code>Partition</code> 
   * objects are expected to be
   * used.
   */
  protected static native void reclaimPartitionMemory();
}









