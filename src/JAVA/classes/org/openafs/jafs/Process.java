/*
 * @(#)Process.java	1.0 6/29/2001
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
 * An abstract representation of an AFS process.  It holds information about 
 * the server, such as what its state is.
 * <BR><BR>
 *
 * Constructing an instance of a <code>Process</code> does not mean an actual 
 * AFS process is created on a server -- usually a <code>Process</code>
 * object is a representation of an already existing AFS process.  If, 
 * however, the <code>Process</code> is constructed with the name of a 
 * process that does not exist in the server represented by the provided 
 * <code>Server</code>, a new process with that name can be
 * created on that server by calling one of the {@link #createSimple(String)},
 * {@link #createFS(String)}, or {@link #createCron(String,String)} methods. If
 * such a process does already exist when one of these methods are called, 
 * an exception will be thrown.<BR><BR>
 *
 * <!--Information on how member values are set-->
 *
 * <!--Example of how to use class-->
 * The following is a simple example of how to construct and use a 
 * <code>Process</code> object.  This example obtains the list of all
 * <code>Process</code> objects on a particular server and prints out the
 * name of each one along with its start time.<BR><BR>
 *
 * <PRE>
 * import org.openafs.jafs.Cell;
 * import org.openafs.jafs.AFSException;
 * import org.openafs.jafs.Process;
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
 *     System.out.println("Processes in Server " + server.getName() + ":");
 *     Process[] processes = server.getProcesss();
 *     for (int i = 0; i < processes.length; i++) {
 *       System.out.print("Process " + processes[i].getName());
 *       System.out.print("was started: " + 
 *                        processes[i].getStartTimeDate().getTime() + "\n");
 *     }
 *   }
 *   ...
 * }
 * </PRE>
 *
 */
public class Process implements Serializable, Comparable
{
  /**
   * Any standard type of process except for fs (such as kaserver, 
   * upclientbin, etc.)
   */
  public static final int SIMPLE_PROCESS = 0;
  
  /**
   * Combination of File Server, Volume Server, and Salvager processes
   */
  public static final int FS_PROCESS = 1;
  
  /**
   * A process that should be restarted at a specific time either daily 
   * or weekly.
   */
  public static final int CRON_PROCESS = 2;

  /**
   * Process execution state stopped
   */
  public static final int STOPPED = 0;

  /**
   * Process execution state running
   */
  public static final int RUNNING = 1;

  /**
   * Process execution state stopping
   */
  public static final int STOPPING = 2;

  /**
   * Process execution state starting
   */
  public static final int STARTING = 3;

  protected String name;
  protected Server server;
  protected long serverHandle;

  protected int type;
  protected int state;
  protected int goal;

  protected long startTime;
  protected long numberStarts;
  protected long exitTime;
  protected long exitErrorTime;
  protected long errorCode;
  protected long errorSignal;

  protected boolean stateOk;
  protected boolean stateTooManyErrors;
  protected boolean stateBadFileAccess;

  protected GregorianCalendar startTimeDate;
  protected GregorianCalendar exitTimeDate;
  protected GregorianCalendar exitErrorTimeDate;

  protected boolean cachedInfo;

  /**
   * Constructs a new <code>Process</code> object instance given the name 
   * of the AFS process and the AFS server, represented by 
   * <CODE>server</CODE>, to which it belongs.   This does not actually
   * create a new AFS process, it just represents one.
   * If <code>name</code> is not an actual AFS process, exceptions
   * will be thrown during subsequent method invocations on this 
   * object, unless one of the {@link #createSimple(String)},
   * {@link #createFS(String)}, or {@link #createCron(String,String)} 
   * methods are explicitly called to create it.
   *
   * @param name      the name of the server to represent 
   * @param server    the server on which the process resides
   * @exception AFSException      If an error occurs in the native code
   */
  public Process( String name, Server server ) throws AFSException
  {
    this.name = name;
    this.server = server;
    serverHandle = server.getBosHandle();
	
    startTimeDate = null;
    exitTimeDate = null;
    exitErrorTimeDate = null;

    cachedInfo = false;
  }

  /**
   * Constructs a new <CODE>Process</CODE> object instance given the name 
   * of the AFS process and the AFS server, represented by 
   * <CODE>server</CODE>, to which it belongs.  This does not actually
   * create a new AFS process, it just represents one.
   * If <code>name</code> is not an actual AFS process, exceptions
   * will be thrown during subsequent method invocations on this 
   * object, unless one of the {@link #createSimple(String)},
   * {@link #createFS(String)}, or {@link #createCron(String,String)} 
   * methods are explicitly called to create it.  Note that if he process
   * doesn't exist and <code>preloadAllMembers</code> is true, an exception
   * will be thrown.
   *
   * <P> This constructor is ideal for point-in-time representation and 
   * transient applications. It ensures all data member values are set and 
   * available without calling back to the filesystem at the first request 
   * for them.  Use the {@link #refresh()} method to address any coherency 
   * concerns.
   *
   * @param name               the name of the process to represent 
   * @param server             the server to which the process belongs.
   * @param preloadAllMembers  true will ensure all object members are 
   *                           set upon construction; otherwise members will 
   *                           be set upon access, which is the default 
   *                           behavior.
   * @exception AFSException      If an error occurs in the native code
   * @see #refresh
   */
  public Process( String name, Server server, boolean preloadAllMembers ) 
      throws AFSException
  {
    this(name, server);
    if (preloadAllMembers) refresh(true);
  }
  
  /**
   * Creates a blank <code>Process</code> given the server to which the process
   * belongs.  This blank object can then be passed into other methods to fill
   * out its properties.
   *
   * @param server       the server to which the process belongs.
   * @exception AFSException      If an error occurs in the native code
   */
  Process( Server server ) throws AFSException
  {
    this( null, server );
  }

  /*-------------------------------------------------------------------------*/

  /**
   * Refreshes the properties of this Process object instance with values 
   * from the AFS process it represents.  All properties that have been 
   * initialized and/or accessed will be renewed according to the values of 
   * the AFS process this Process object instance represents.
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
   * Refreshes the properties of this Process object instance with values from
   * the AFS process it represents.  If <CODE>all</CODE> is <CODE>true</CODE> 
   * then <U>all</U> of the properties of this Process object instance will be
   * set, or renewed, according to the values of the AFS process it represents,
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
   * Refreshes the information fields of this <code>Process</code> to reflect 
   * the current state of the AFS process, such as the start time, the state,
   * etc.
   *
   * @exception AFSException      If an error occurs in the native code
   */
  protected void refreshInfo() throws AFSException
  {
    getProcessInfo( server.getBosHandle(), name, this );
    cachedInfo = true;
    startTimeDate = null;
    exitTimeDate = null;
    exitErrorTimeDate = null;
  }

  /**
   * Creates this process as a simple process on the server.
   *
   * @param executionPath   the path to the process's executable
   * @exception AFSException      If an error occurs in the native code
   */
  public void createSimple( String executionPath ) throws AFSException
  {
    create( server.getBosHandle(), name, SIMPLE_PROCESS, executionPath, null, 
	    null );
  }

  /**
   * Creates this process as a file server process on the server.
   *
   * @param executionPath   the path to the process's executable
   * @exception AFSException      If an error occurs in the native code
   */
  public void createFS( String executionPath ) throws AFSException
  {
    create( server.getBosHandle(), name, FS_PROCESS, executionPath, null, 
	    null );
  }

  /**
   * Creates this process as a cron process on the server.
   *
   * @param executionPath   the path to the process's executable
   * @param cronTime   a String representing the time a cron process is 
   *                   to be run.  Acceptable formats are:<ul>
   *                   <li>for daily restarts: "23:10" or "11:10 pm"</li>
   *                   <li>for weekly restarts: "sunday 11:10pm" or 
   *                       "sun 11:10pm"</li>
   *                   </ul>
   * @exception AFSException      If an error occurs in the native code
   */
  public void createCron( String executionPath, String cronTime ) 
      throws AFSException
  {
    create( server.getBosHandle(), name, CRON_PROCESS, executionPath, 
	    cronTime, null );
  }

  /**
   * Removes this process from the bos server
   *
   * @exception AFSException      If an error occurs in the native code
   */
  public void delete() throws AFSException
  {
    delete( server.getBosHandle(), name );
  }

  /**
   * Stops this process.
   *
   * @exception AFSException      If an error occurs in the native code
   */
  public void stop() throws AFSException
  {
    state = STOPPING;
    goal = STOPPED;
    stop( server.getBosHandle(), name );
  }

  /**
   * Starts this process
   *
   * @exception AFSException      If an error occurs in the native code
   */
  public void start() throws AFSException
  {
    state = STARTING;
    start( server.getBosHandle(), name );
  }

  /**
   * Restarts this process
   *
   * @exception AFSException      If an error occurs in the native code
   */
  public void restart() throws AFSException
  {
    state = STARTING;
    restart( server.getBosHandle(), name );
  }

  //////////////// accessors:  ////////////////////////

  /**
   * Returns the name of this process.
   *
   * @return the name of this process
   */
  public String getName()
  {
    return name;
  }

  /**
   * Returns the server hosting this process.
   *
   * @return this process' server
   */
  public Server getServer()
  {
    return server;
  }

  /**
   * Returns the process type.  Possible values are:<ul>
   *      <li>{@link #SIMPLE_PROCESS}</li>
   *      <li>{@link #FS_PROCESS}</li>
   *      <li>{@link #CRON_PROCESS}</li></ul>
   *
   * @return the process type
   * @exception AFSException      If an error occurs in the native code
   */
  public int getType() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return type;
  }

  /**
   * Returns the process goal.  Possible values are:<ul>
   *      <li>{@link #STOPPED}</li>
   *      <li>{@link #RUNNING}</li>
   *      <li>{@link #STARTING}</li>
   *      <li>{@link #STOPPING}</li></ul>
   * After this method is called once, it saves the value 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @return the process goal
   * @exception AFSException      If an error occurs in the native code
   */
  public int getGoal() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return goal;
  }

  /**
   * Returns the process execution state.  Possible values are:<ul>
   *      <li>{@link #STOPPED}</li>
   *      <li>{@link #RUNNING}</li>
   *      <li>{@link #STARTING}</li>
   *      <li>{@link #STOPPING}</li></ul>
   * After this method is called once, it saves the value 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @return the process execution state
   * @exception AFSException      If an error occurs in the native code
   */
  public int getState() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return state;
  }

  /**
   * Returns the most recent start time of this process.  A 
   * <code>null</code> value 
   * indicates no start time.
   * After this method is called once, it saves the value 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @return the start time
   * @exception AFSException      If an error occurs in the native code
   */
  public long getStartTime() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return startTime;
  }

  /**
   * Returns the most recent start time of this process.  A <code>null</code> 
   * value indicates no start time.
   * After this method is called once, it saves the value 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @return the start time
   * @exception AFSException      If an error occurs in the native code
   */
  public GregorianCalendar getStartTimeDate() throws AFSException
  {
    if (!cachedInfo) {
	refreshInfo();
    }
    if( startTimeDate == null && startTime != 0 ) {
	// make it into a date . . .
	startTimeDate = new GregorianCalendar();
	long longTime = startTime * 1000;
	Date d = new Date( longTime );
	startTimeDate.setTime( d );
    }
    return startTimeDate;
  }

  /**
   * Returns the number of starts of the process.
   * After this method is called once, it saves the value 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @return the number of starts
   * @exception AFSException      If an error occurs in the native code
   */
  public long getNumberOfStarts() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return numberStarts;
  }

  /**
   * Returns the most recent exit time of this process.  A <code>null</code> 
   * value indicates no exit time.
   * After this method is called once, it saves the value 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @return the exit time
   * @exception AFSException      If an error occurs in the native code
   */
  public long getExitTime() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return exitTime;
  }

  /**
   * Returns the most recent exit time of this process.  A <code>null</code> 
   * value indicates no exit time
   * After this method is called once, it saves the value 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @return the exit time
   * @exception AFSException      If an error occurs in the native code
   */
  public GregorianCalendar getExitTimeDate() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    if( exitTimeDate == null && exitTime != 0 ) {
	// make it into a date . . .
	exitTimeDate = new GregorianCalendar();
	long longTime = exitTime*1000;
	Date d = new Date( longTime );
	exitTimeDate.setTime( d );
    }
    return exitTimeDate;
  }

  /**
   * Returns the most recent time this process exited with an error.  A 
   * <code>null</code> value indicates no exit w/ error time.
   * After this method is called once, it saves the value 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @return the exit w/ error time
   * @exception AFSException      If an error occurs in the native code
   */
  public long getExitErrorTime() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return exitErrorTime;
  }

  /**
   * Returns the most recent time this process exited with an error.  A <
   * code>null</code> value indicates no exit w/ error time.
   * After this method is called once, it saves the value 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @return the exit w/ error time
   * @exception AFSException      If an error occurs in the native code
   */
  public GregorianCalendar getExitErrorTimeDate() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    if (exitErrorTimeDate == null && exitErrorTime != 0) {
	// make it into a date . . .
	exitErrorTimeDate = new GregorianCalendar();
	long longTime = exitErrorTime*1000;
	Date d = new Date( longTime );
	exitErrorTimeDate.setTime( d );
    }
    return exitErrorTimeDate;
  }

  /**
   * Returns the error code of the process.  A value of 0 indicates 
   * no error code.
   * After this method is called once, it saves the value 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @return the error code
   * @exception AFSException      If an error occurs in the native code
   */
  public long getErrorCode() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return errorCode;
  }

  /**
   * Returns the error signal of the process.  A value of 0 indicates no 
   * error signal.
   * After this method is called once, it saves the value 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @return the error signal
   * @exception AFSException      If an error occurs in the native code
   */
  public long getErrorSignal() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return errorSignal;
  }

  /**
   * Returns whether or not the state of the process is ok.  A value of 
   * <code>false</code> indicates there has been a core dump.
   * After this method is called once, it saves the value 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @return whether or not the state is ok
   * @exception AFSException      If an error occurs in the native code
   */
  public boolean getStateOk() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return stateOk;
  }

  /**
   * Returns whether or not the state of the process indicates too many errors.
   * After this method is called once, it saves the value 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @return whether or not the state indicates too many errors
   * @exception AFSException      If an error occurs in the native code
   */
  public boolean getStateTooManyErrors() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return stateTooManyErrors;
  }

  /**
   * Returns whether or not the state of the process indicates bad file access.
   * After this method is called once, it saves the value 
   * and returns that value on subsequent calls, 
   * until the {@link #refresh()} method is called and a more current 
   * value is obtained.
   *
   * @return whether or not the state indicates bad file access
   * @exception AFSException      If an error occurs in the native code
   */
  public boolean getStateBadFileAccess() throws AFSException
  {
    if (!cachedInfo) refreshInfo();
    return stateBadFileAccess;
  }

  /////////////// custom information methods ////////////////////

  /**
   * Returns a <code>String</code> representation of this <code>Process</code>.
   * Contains the information fields.
   *
   * @return a <code>String</code> representation of the <code>Process</code>
   */
  public String getInfo()
  {
    String r;
    try {
	
	r = "Process: " + name + "\n";
	
	r += "\ttype: ";
	switch( getType() ) {
	case SIMPLE_PROCESS:
	  r += "simple";
	  break;
	case FS_PROCESS:
	  r += "fs";
	  break;
	case CRON_PROCESS:
	  r += "cron";
	  break;
	default:
	  r += "other - " + getType();
	}
	r += "\n";

	r += "\tstate: ";
	switch( getState() ) {
	case STOPPED:
	  r += "stopped";
	  break;
	case RUNNING:
	  r += "running";
	  break;
	case STOPPING:
	  r += "stopping";
	  break;
	case STARTING:
	  r += "starting";
	  break;
	default:
	  r += "other - " + getState();
	}
	r += "\n";

	r += "\tgoal: ";
	switch( getGoal() ) {
	case STOPPED:
	  r += "stopped";
	  break;
	case RUNNING:
	  r += "running";
	  break;
	case STOPPING:
	  r += "stopping";
	  break;
	case STARTING:
	  r += "starting";
	  break;
	default:
	  r += "other - " + getGoal();
	}
	r += "\n";

	r += "\tstartTime: ";
	if( getStartTime() == 0) {
	  r += "0";
	} else {
	  r += getStartTimeDate().getTime(); 
	}
      r += "\n";

	r += "\tnumberStarts: " + getNumberOfStarts() + "\n";

	r += "\texitTime: ";
	if( getExitTime() == 0 ) {
	  r += "0";
	} else {
	  r += getExitTimeDate().getTime(); 
	}
      r += "\n";

	r += "\texitErrorTime: ";
	if( getExitErrorTimeDate() == null ) {
	  r += "0";
	} else {
	  r += getExitErrorTimeDate().getTime(); 
	}
      r += "\n";

	r += "\terrorCode: " + getErrorCode() + "\n";
	r += "\terrorSignal: " + getErrorSignal() + "\n";
	r += "\tstateOk: " + getStateOk() + "\n";
	r += "\tstateTooManyErrors: " + getStateTooManyErrors() + "\n";
	r += "\tstateBadFileAccess: " + getStateBadFileAccess() + "\n";

    } catch( Exception e ) {
	return e.toString();
    }
    return r;
  }

  /////////////// custom override methods ////////////////////

  /**
   * Compares two Process objects respective to their names and does not
   * factor any other attribute.    Alphabetic case is significant in 
   * comparing names.
   *
   * @param     process    The Process object to be compared to this Process 
   *                       instance
   * 
   * @return    Zero if the argument is equal to this Process' name, a
   *		value less than zero if this Process' name is
   *		lexicographically less than the argument, or a value greater
   *		than zero if this Process' name is lexicographically
   *		greater than the argument
   */
  public int compareTo(Process process)
  {
    return this.getName().compareTo(process.getName());
  }

  /**
   * Comparable interface method.
   *
   * @see #compareTo(Process)
   */
  public int compareTo(Object obj)
  {
    return compareTo((Process)obj);
  }

  /**
   * Tests whether two <code>Process</code> objects are equal, based on their 
   * names and hosting server.
   *
   * @param otherProcess   the Process to test
   * @return whether the specifed Process is the same as this Process
   */
  public boolean equals( Process otherProcess )
  {
    return ( name.equals(otherProcess.getName()) ) &&
           ( this.getServer().equals(otherProcess.getServer()) );
  }

  /**
   * Returns the name of this <CODE>Process</CODE>
   *
   * @return the name of this <CODE>Process</CODE>
   */
  public String toString()
  {
    return getName();
  }

  /////////////// native methods ////////////////////

  /**
   * Fills in the information fields of the provided <code>Process</code>.
   *
   * @param cellHandle    the handle of the cell to which the process belongs
   * @see Cell#getCellHandle
   * @param processName     the instance name of the process  for which to get
   *                        the information
   * @param theProcess     the {@link Process Process} object in which to fill 
   *                       in the information
   * @exception AFSException   If an error occurs in the native code
   */
  protected static native void getProcessInfo( long cellHandle, 
					       String processName, 
					       Process theProcess ) 
	throws AFSException;

  /**
   * Creates a processes on a server.
   *
   * @param serverHandle  the bos handle of the server to which the key will 
   *                      belong
   * @see Server#getBosServerHandle
   * @param processName  the instance name to give the process.  See AFS 
   *                     documentation for a standard list of instance names
   * @param processType  the type of process this will be. 
   *                     Acceptable values are:<ul>
   *                     <li>{@link #SIMPLE_PROCESS}</li>
   *                     <li>{@link #FS_PROCESS}</li>
   *                     <li>{@link #CRON_PROCESS}</li></ul>
   * @param executionPath  the execution path process to create
   * @param cronTime   a String representing the time a cron process is to 
   *                   be run.  Acceptable formats are:<ul>
   *                   <li>for daily restarts: "23:10" or "11:10 pm"</li>
   *                   <li>for weekly restarts: "sunday 11:10pm" or 
   *                       "sun 11:10pm"</li>
   *                   </ul>
   *                   Can be <code>null</code> for non-cron processes.
   * @param notifier   the execution path to a notifier program that should 
   *                   be called when the process terminates.  Can be 
   *                   <code>null</code>
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void create( long serverHandle, String processName, 
				       int processType, String executionPath, 
				       String cronTime, String notifier )
    throws AFSException;

  /**
   * Removes a process from a server.
   *
   * @param serverHandle  the bos handle of the server to which the process 
   *                      belongs
   * @see Server#getBosServerHandle
   * @param processName   the name of the process to remove
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void delete( long serverHandle, String processName )
    throws AFSException;

  /**
   * Start this process.
   *
   * @param serverHandle  the bos handle of the server to which the process 
   *                      belongs
   * @see Server#getBosServerHandle
   * @param processName   the name of the process to start
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void start( long serverHandle, String processName )
    throws AFSException;

  /**
   * Retart this process.
   *
   * @param serverHandle  the bos handle of the server to which the process 
   *                      belongs
   * @see Server#getBosServerHandle
   * @param processName   the name of the process to restart
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void restart( long serverHandle, String processName )
    throws AFSException;

  /**
   * Stop this process.
   *
   * @param serverHandle  the bos handle of the server to which the process 
   *                      belongs
   * @see Server#getBosServerHandle
   * @param processName   the name of the process to stop
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void stop( long serverHandle, String processName )
    throws AFSException;

  /**
   * Reclaims all memory being saved by the process portion of the native 
   * library. This method should be called when no more <code>Process</code> 
   * objects are expected to be used.
   */
  protected static native void reclaimProcessMemory();
}







