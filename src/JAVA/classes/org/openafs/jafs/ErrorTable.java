/*
 * @(#)ErrorTable.java	2.0 11/06/2000
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

import java.util.ResourceBundle;
import java.util.Locale;

/**
 * Static class for error code message management.
 *
 * <P>Simply translates all error codes returned by the AFS native library
 * to literal string messages according to the defined locale.
 *                                                                             
 * @version 2.0, 11/06/2000
 */
public final class ErrorTable
{
  /* Undefined Error Constants */
  public static final int UNKNOWN = -3;
  public static final int SPECIAL_CASE = -2;
  public static final int GENERAL_FAILURE = -1;

  /* Java Application Error Constants */
  public static final int NULL = 1000;
  public static final int INVALID_SESSION = 1001;
  public static final int EXPIRED_SESSION = 1002;
  public static final int OPERATION_ABORTED = 1003;
  public static final int FORCED_ABORT = 1004;

  /* General UNIX Error Constants */
  public static final int NOT_PERMITTED = 1;
  public static final int NOT_FOUND = 2;
  public static final int IO_ERROR = 5;
  public static final int NO_DEVICE_ADDRESS = 6;
  public static final int BAD_FILE = 9;
  public static final int TRY_AGAIN = 11;
  public static final int OUT_OF_MEMORY = 12;
  public static final int PERMISSION_DENIED = 13;
  public static final int BAD_ADDRESS = 14;
  public static final int DEVICE_BUSY = 16;
  public static final int FILE_EXISTS = 17;
  public static final int NO_DEVICE = 19;
  public static final int NOT_DIRECTORY = 20;
  public static final int IS_DIRECTORY = 21;
  public static final int INVALID_ARG = 22;
  public static final int FILE_OVERFLOW = 23;
  public static final int FILE_BUSY = 26;
  public static final int NAME_TOO_LONG = 36;
  public static final int DIRECTORY_NOT_EMPTY = 39;
  public static final int CONNECTION_TIMED_OUT = 110;
  public static final int QUOTA_EXCEEDED = 122;

  /* AFS Error Constants */
  public static final int BAD_USERNAME = 180486;
  public static final int BAD_PASSWORD = 180490;
  public static final int EXPIRED_PASSWORD = 180519;
  public static final int SKEWED_CLOCK = 180514;
  public static final int ID_LOCKED = 180522;
  public static final int CELL_NOT_FOUND = 180501;
  public static final int USERNAME_EXISTS = 180481;
  public static final int USER_DOES_NOT_EXIST = 180484;

  /* AFS Authentication Error Constants */
  public static final int PRPERM      = 267269;
  public static final int UNOACCESS   = 5407;
  public static final int BZACCESS    = 39430;
  public static final int KANOAUTH    = 180488;
  public static final int RXKADNOAUTH = 19270405;

  private static java.util.Hashtable bundles;

  static
  {
    bundles = new java.util.Hashtable(2);
    try {
      bundles.put(Locale.US, 
        ResourceBundle.getBundle("ErrorMessages", Locale.US));
      bundles.put(Locale.SIMPLIFIED_CHINESE, 
        ResourceBundle.getBundle("ErrorMessages", Locale.SIMPLIFIED_CHINESE));
    } catch (Exception e) {
      bundles.put(Locale.getDefault(),
        ResourceBundle.getBundle("ErrorMessages"));
    }
  }

  /*-----------------------------------------------------------------------*/
  /**
   * Tests to identify if the return code is a "Permission Denied" error.
   *
   * <P> This method will qualify <CODE>errno</CODE> against:
   * <LI><CODE>ErrorTable.PERMISSION_DENIED</CODE>
   * <LI><CODE>ErrorTable.PRPERM</CODE>
   * <LI><CODE>ErrorTable.UNOACCESS</CODE>
   * <LI><CODE>ErrorTable.BZACCESS</CODE>
   * <LI><CODE>ErrorTable.KANOAUTH</CODE>
   * <LI><CODE>ErrorTable.RXKADNOAUTH</CODE>
   *
   * @param		errno		Error Code/Number
   * @return	boolean	If <CODE>errno</CODE> is a "Permission Denied"
   *                          error.
   */
  public static boolean isPermissionDenied(int errno)
  {
    return (errno == PERMISSION_DENIED || errno == PRPERM ||
            errno == UNOACCESS || errno == BZACCESS || errno == KANOAUTH
            || errno == RXKADNOAUTH);
  }
  /*-----------------------------------------------------------------------*/
  /**
   * Returns a String message representing the error code (number) provided.
   *
   * <P> If the error code provided is out of range of the library of defined
   * error codes, this method will return <CODE>Error number [###] unknown
   * </CODE>. If an exception is thrown, this method will return either: 
   * <CODE>Unknown error</CODE>, <CODE>Special case error</CODE>, or 
   * <CODE>Invalid error code: ###</CODE>.
   *
   * @param		errno		Error Code/Number
   * @return	String	Interpreted error message derived from 
   *                          <CODE>errno</CODE>.
   */
  public static String getMessage(int errno)
  {
    return getMessage(errno, Locale.getDefault());
  }
  /*-----------------------------------------------------------------------*/
  /**
   * Returns a String message, respective to the provided locale, representing
   * the error code (number) provided.
   *
   * <P> If the error code provided is out of range of the library of defined
   * error codes, this method will return <CODE>Error number [###] unknown
   * </CODE>. If an exception is thrown, this method will return either: 
   * <CODE>Unknown error</CODE>, <CODE>Special case error</CODE>, or 
   * <CODE>Invalid error code: ###</CODE>.
   *
   * @param   errno   Error Code/Number
   * @param   locale  Locale of to be used for translating the message.
   * @return  String  Interpreted error message derived from 
   *                  <CODE>errno</CODE>.
   */
  public static String getMessage(int errno, Locale locale)
  {
    String msg = "Error number [" + errno + "] unknown.";
    try {
	msg = getBundle(locale).getString("E" + errno);
    } catch (Exception e) {
      try {
	  if (errno == 0) {
	    msg = "";
	  } else if (errno == GENERAL_FAILURE) {
	    msg = getBundle(locale).getString("GENERAL_FAILURE");
	  } else if (errno == UNKNOWN) {
	    msg = getBundle(locale).getString("UNKNOWN");
	  } else if (errno == SPECIAL_CASE) {
	    msg = getBundle(locale).getString("SPECIAL_CASE");
	  } else {
	    System.err.println("ERROR in ErrorCode getMessage(): " + e);
	    msg = "Invaid error code: " + errno;
	  }
      } catch (Exception e2) {
        //INGORE
      }
    } finally {
      return msg;
    }
  }
  /*-----------------------------------------------------------------------*/
  private static ResourceBundle getBundle(Locale locale) throws Exception
  {
    if (locale == null) return getBundle(Locale.getDefault());
    ResourceBundle rb = (ResourceBundle)bundles.get(locale);
    if (rb == null) {
      rb = ResourceBundle.getBundle("ErrorMessages", locale);
      bundles.put(locale, rb);
    }
    return rb;
  }
}



