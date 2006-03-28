/*
 * @(#)AFSException.java	2.0 01/04/16
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

import java.util.Locale;

/**
 * An exception indicating that an error has occurred in the Java AFS 
 * API, in the Java AFS JNI, or in the AFS file system.
 *
 * @version 1.0, 04/16/2001
 * @see     java.lang.Exception
 */
public class AFSException extends Exception
{
  /**
   * The AFS specific error number (code). 
   * @see     #getErrorCode()
   */
  protected int errno;

  /**
   * Constructs an <code>AFSException</code> with the specified detail
   * message. 
   *
   * @param   reason   the detail message.
   */
  public AFSException(String reason)
  {
    super(reason);
  }
  /**
   * Constructs an <code>AFSException</code> with the specified error code. 
   * This constructor will also generate the appropriate error message
   * respective to the specified error code. 
   *
   * @param   errno   the AFS error number (error code).
   */
  public AFSException(int errno)
  {
    super(ErrorTable.getMessage( (errno == 0) ? ErrorTable.UNKNOWN : errno ));
    this.errno = (errno == 0) ? ErrorTable.UNKNOWN : errno;
  }
  /**
   * Constructs an <code>AFSException</code> with the specified detail message
   * and specified error code.  In this constructor the specified detail message
   * overrides the default AFS error message defined by the
   * <code>ErrorTable</code> class.  Therefore, to retrieve the AFS specific 
   * error message, you must use the <code>{@link #getAFSMessage}</code> method.
   * The <code>{@link #getMessage}</code> method will return the message specified 
   * in this constructor.
   *
   * @param   reason   the detail message.
   * @param   errno    the AFS error number (error code).
   */
  public AFSException(String reason, int errno)
  {
    super(reason);
    this.errno = errno;
  }
  /*-----------------------------------------------------------------------*/
  /**
   * Returns the AFS specific error number (code).  This code can be interpreted 
   * by use of the <code>{@link ErrorTable}</code> class method 
   * <code>{@link ErrorTable#getMessage(int)}</code>.
   *
   * @return  the AFS error code of this <code>AFSException</code> 
   *          object. 
   * @see     ErrorTable#getMessage(int)
   */
  public int getErrorCode() 
  {
    return errno;
  }
  /*-----------------------------------------------------------------------*/
  /**
   * Returns the error message string of this exception.
   *
   * @return  the error message string of this exception object. 
   *            
   * @see     #getAFSMessage()
   */
  public String getMessage()
  {
    String msg = super.getMessage();
    return (msg == null) ? ErrorTable.getMessage(errno) : msg;
  }
  /*-----------------------------------------------------------------------*/
  /**
   * Returns the locale specific error message string of this exception.
   *
   * @return  the error message string of this exception object. 
   * @param   locale the locale for which this message will be displayed
   *            
   * @see     #getAFSMessage()
   */
  public String getMessage(Locale locale)
  {
    String msg = super.getMessage();
    return (msg == null) ? ErrorTable.getMessage(errno, locale) : msg;
  }
  /*-----------------------------------------------------------------------*/
  /**
   * Returns the AFS error message string defined by the <code>ErrorTable
   * </code> class.  The message will be formatted according to the default
   * Locale.
   *
   * <P> This message is also available from this object's super class
   * method <code>getMessage</code>. However, this method will always return
   * the string message associated with the actual AFS error code/number
   * specified, whereas the {@link #getMessage()} method will return the
   * string message intended for this Exception object, which may be
   * an overridden message defined in the constructor of this Exception.
   *
   * @return  the AFS error message string of this exception object. 
   *            
   * @see     #getAFSMessage(Locale)
   * @see	  AFSException#AFSException(String, int)
   * @see     ErrorTable#getMessage(int)
   * @see     java.lang.Exception#getMessage()
   */
  public String getAFSMessage() 
  {
    return ErrorTable.getMessage(errno);
  }
  /*-----------------------------------------------------------------------*/
  /**
   * Returns the AFS error message defined by the <code>ErrorTable</code>
   * class.  The message will be formatted according to the specified Locale.
   *
   * <P> This message is also available from this object's super class
   * method <code>getMessage</code>. However, this method will always return
   * the string message associated with the actual AFS error code/number
   * specified, whereas the {@link #getMessage()} method will return the
   * string message intended for this Exception object, which may be
   * an overridden message defined in the constructor of this Exception.
   *
   * @return  the AFS error message string of this exception object. 
   * @param   locale the locale for which this message will be displayed
   *            
   * @see     #getAFSMessage()
   * @see	  AFSException#AFSException(String, int)
   * @see     ErrorTable#getMessage(int, Locale)
   * @see     java.lang.Exception#getMessage()
   */
  public String getAFSMessage(Locale locale)
  {
    return ErrorTable.getMessage(errno, locale);
  }
  /*-----------------------------------------------------------------------*/
  /**
   * Returns a string representation of this AFS Exception.
   *
   * <P> The message will be formatted according to the specified Locale.
   *
   * @return  the AFS error message string of this exception object. 
   *            
   * @see     #getAFSMessage()
   * @see     ErrorTable#getMessage(int)
   * @see     java.lang.Exception#getMessage()
   */
  public String toString()
  {
    return "AFSException: Error Code: " + errno + "; Message: " +
            getMessage();
  }
  /*-----------------------------------------------------------------------*/
}















