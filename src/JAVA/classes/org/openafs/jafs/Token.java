/*
 * @(#)Token.java	1.2 05/06/2002
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

/**
 * An abstract representation of an AFS authentication token.  It conveniently
 * maintains the handle associated with token and the cell to which the token
 * is authenticated.
 * <BR><BR>
 *
 * Constructing a <code>Token</code> object results in an immediate attempt to
 * authenticate the user within the specified cell.  If this attempt fails, an
 * <code>{@link AFSException}</code> will be thrown.  Therefore, if the
 * construction of the object succeeds without an exception, then the 
 * <code>Token</code> is considered authenticated.
 *
 * The construction of a <code>Token</code> object acts as an entry point
 * for authentication into the AFS system.  Thus, when you construct a 
 * <code>{@link Cell}</code> object, you must pass in an instance of a
 * <code>Token</code> that has been authenticated within the AFS cell that
 * <code><I>Cell</I></code> is intended to represent.  You will only be 
 * allowed to perform actions that the user, used to authenticate 
 * <code>Token</code>, is authorized to perform.  You must construct a 
 * <code>Token</code> object before constructing a <code>Cell</code> object,
 * which is required by all other objects within this package either directly 
 * or indirectly.<BR><BR>
 *
 * If an error occurs during a method call, an 
 * <code>AFSException</code> will be thrown.  This class is the Java
 * equivalent of errors thrown by AFS; see {@link AFSException}
 * for a complete description.<BR><BR>
 *
 * <!--Example of how to use class-->
 * The following is a simple example of how to construct and use a 
 * <code>Token</code> object. It shows how to construct a <code>Cell</code> 
 * using a <code>Token</code>.  See {@link Cell} for a more detailed example 
 * of constructing and using a <code>Cell</code> object.<BR><BR>
 *
 * <PRE>
 * import org.openafs.jafs.AFSException;
 * import org.openafs.jafs.Cell;
 * import org.openafs.jafs.Token;
 * ...
 * public class ...
 * {
 *   ...
 *   private Cell cell;
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
 *     ...
 *   }
 *   ...
 * }
 * </PRE>
 *
 */

public class Token implements Serializable, Comparable
{
  public static int ANYUSER_PAG_ID;

  protected long tokenHandle;
  protected int pagID = -1;
  protected int errno;

  protected String cellName;
  protected String username;
  private   String password;

  private boolean hasInitialized = false;

  /**
   * Load the native libraries <code>libjafs</code> and 
   * <code>libjafs</code>.
   */
  static
  {
    try {
      Class.forName("org.openafs.jafs.AFSLibraryLoader");
      try {
        initializeAdminClient();
      } catch (Exception e) {
        System.err.println(e);
      }
    } catch (ClassNotFoundException e) {
      /* Most likely running on a client, do nothing */
    }
  }

  /**
   * Constructs a new <CODE>Token</CODE> object instance given 
   * the name of the AFS cell it represents and the username and password 
   * of the user to be Tokend for 
   * administrative access.
   *
   * @param username    the name of the user to Token with
   * @param password    the password of that user
   * @param cellName    the name of the cell to Token into
   * @param login       if true, automatically login upon construction
   * @exception AFSException  If an error occurs in the native code
   */
  protected Token( String username, String password, String cellName, 
                   boolean automaticallyLogin ) 
      throws AFSException
  {
    this.username = username;
    this.password = password;
    this.cellName = cellName;

    /* By default lets authenticate the user using libafsauthent.a */
    if (automaticallyLogin) login();
  }

  /**
   * Constructs a new <CODE>Token</CODE> object instance given 
   * the name of the AFS cell it represents and the username and password 
   * of the user to be Tokend for 
   * administrative access.
   *
   * @param username    the name of the user to Token with
   * @param password    the password of that user
   * @param cellName    the name of the cell to Token into
   * @exception AFSException  If an error occurs in the native code
   */
  public Token( String username, String password, String cellName ) 
      throws AFSException
  {
    this.username = username;
    this.password = password;
    this.cellName = cellName;

//System.out.println(username + ", " + cellName);
    /* By default lets authenticate the user using libafsauthent.a */
    login();
  }

  /**
   * Returns the name of the AFS cell that this <code>Token</code> was
   * authenticated against.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return the name of the AFS cell associated with this <code>Token</code>.
   */
  public String getCellName()
  {
    return cellName;
  }

  /**
   * Returns the username of user to whom this token belongs.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return the username of the user represented by this Token
   */
  public String getUsername()
  {
    return username;
  }

  /**
   * Returns a token handle that can be used to prove this authentication 
   * later.
   *
   * @exception AFSException  If an error occurs in the native code
   * @return a token representing the authentication
   */
  protected long getHandle()
  {
    return tokenHandle;
  }

  /**
   * Closes the given currently open token.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  public void close() throws AFSException
  {
    close(tokenHandle);
  }

  /**
   *  Gets the expiration time for a given token.
   *
   * @return a long representing the UTC time for the token expiration
   * @exception AFSException  If an error occurs in the native code
   */
  public long getExpiration() throws AFSException
  {
    return getExpiration(tokenHandle);
  }

  /**
   * Authenticates a user in kas, and binds that authentication
   * to the current process.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  public void klog() throws AFSException
  {
    if (!hasInitialized) {
      initializeUserSpace();
      hasInitialized = true;
    }
    if (pagID > -1) {
      relog(pagID);
    } else {
      pagID = klog(username, password, cellName, pagID);
    }
  }

  /**
   * Authenticates a user in KAS, and binds that authentication
   * to the current process.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  public void login() throws AFSException
  {
    this.tokenHandle = this.getToken(cellName, username, password);
//System.out.println("Token handle -> " + tokenHandle);
  }

  /**
   * Initialize the user space AFS client (libjafs).
   *
   * <P> The user space client must be initialized prior to any
   * user space related methods, including: klog, unlog, relog,
   * and shutdown.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected static void initializeUserSpace() throws AFSException
  {
    try {
      Token.initUserSpace();
    } catch (AFSException e) {
      System.err.println(e.getMessage());
    }
    try {
	Runtime.getRuntime().addShutdownHook(new AFSShutdownHandler());
    } catch (Exception e) {
	System.err.println("Could not register shutdown hook: " + e.toString());
    }
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
  public int compareTo(Token token)
  {
    return this.toString().compareTo(token.toString());
  }

  /**
   * Comparable interface method.
   *
   * @see #compareTo(Token)
   */
  public int compareTo(Object obj)
  {
    return compareTo((Token)obj);
  }

  /**
   * Tests whether two <code>Cell</code> objects are equal, based on their 
   * names.  Does not test whether the objects are actually the same
   * representational instance of the AFS cell.
   *
   * @param otherCell   the <code>Cell</code> to test
   * @return whether the specifed user is the same as this user
   */
  public boolean equals( Token token )
  {
    return this.toString().equals( token.toString() );
  }

  /**
   * Returns the name of this <CODE>Cell</CODE>
   *
   * @return the name of this <CODE>Cell</CODE>
   */
  public String toString()
  {
    return username + "@" + cellName + ":" + tokenHandle;
  }

  /////////////// native methods found in *Token.c ////////////////////

  /**
   * Initialize the user space library.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  private static native void initUserSpace() throws AFSException;

  /**
   * Initialize the administrative library.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  protected static native void initializeAdminClient() throws AFSException;

  /**
   * Returns a token handle that can be used to prove this authentication 
   * later.
   *
   * @param cellName    the name of the cell in which to Token this user
   * @param userName    the name of the user to Token
   * @param password    the password of the user
   * @exception AFSException  If an error occurs in the native code
   * @return a token representing the authentication
   */
  protected native long getToken( String cellName, String username, 
                                 String password ) 
	throws AFSException;

  /**
   * Closes the given currently open token.
   *
   * @param tokenHandle   the token to close
   * @exception AFSException  If an error occurs in the native code
   */
  protected native void close( long tokenHandle ) throws AFSException;

  /**
   *  Gets the expiration time for a given token.
   *
   * @param tokenHandle    a token handle previously returned by a call 
   *                       to {@link #getToken}
   * @see #getToken
   * @return a long representing the UTC time for the token expiration
   * @exception AFSException  If an error occurs in the native code
   */
  protected native long getExpiration( long tokenHandle )
      throws AFSException;

  /**
   * Authenticates a user in KAS, and binds that authentication
   * to the current thread or native process.
   *
   * @param username         the login to authenticate 
   *                         (expected as username@cellname)
   * @param password         the password of the login
   * @param cellName         the name of the cell to authenticate into
   * @param id               the existing pag (or 0)
   *
   * @return the assigned pag
   * @exception AFSException  If an error occurs in the native code
   */
  protected native int klog(String username, String password, 
                            String cellName, int id) 
	throws AFSException;

  /**
   * Authenticates a user in KAS by a previously acquired PAG ID, and binds 
   * that authentication to the current thread or native process.
   *
   * <P> This method does not require the user's username and password to
   * fully authenticate their request.  Rather it utilizes the user's PAG ID
   * to recapture the user's existing credentials.
   *
   * <P> This method is called by the public <code>klog</code> method, which
   * internally manages the PAG ID. Additionally, an application needs only
   * call <code>klog</code>, this reduces the amount of complexity and ensures
   * that <code>relog</code> is never called before a <code>klog</code>.
   *
   * @param int User's current PAG (process authentication group) ID
   * @exception AFSException  If an error occurs in the native code
   */
  protected native void relog(int id) throws AFSException;

  /**
   * Manually discards all AFS credentials associated with the bound user.
   *
   * @exception AFSException  If an error occurs in the native code
   */
  public native void unlog() throws AFSException;

  /**
   * Inform the native library that the application is 
   * shutting down and will be unloading.
   *
   * <p> The library will make a call informing the file server that it will 
   * no longer be available for callbacks.
   */
  protected static native void shutdown();

  /**
   * Reclaims all memory being saved by the authentication portion of 
   * the native library.
   * This method should be called when no more authentications are expected.
   */
  protected static native void reclaimAuthMemory();
}

/*=======================================================================*/
/**
 * Class that loads the native libraries required for direct communication with
 * AFS. Since the Token class is serializable the function of loading the 
 * native libraries must be performed in a non-serialized class, one that will
 * not be included in any client side application packages.
 *                                                                             
 * @version 1.0, 06/13/2001
 */
class AFSLibraryLoader
{
  static
  {
    System.loadLibrary("jafs");
    System.loadLibrary("jafsadm");
  }
}
/*=======================================================================*/
/**
 * Class that handles graceful AFS application shutdown procedures by
 * instructing the native library to inform the file system server that
 * it is shutting down.
 *                                                                             
 * @version 1.0, 06/13/2001
 */
class AFSShutdownHandler extends Thread
{
  public AFSShutdownHandler() {}

  /**
   * This is the execution method satisfying the interface requirement as a 
   * stand alone runnable thread.
   *
   * <p> This method will automatically be invoked by the Thread instantiator.
   *
   * @see Token#shutdown()
   */
  public void run() 
  {
    System.out.println("Shutting down Java AFS library...");
    org.openafs.jafs.Token.shutdown();
  }
}
/*=======================================================================*/








