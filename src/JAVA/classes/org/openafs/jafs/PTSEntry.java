/*
 * @(#)PTSEntry.java	1.2 10/23/2001
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

/**
 * An interface representation of a PTS entry as it applies to
 * AFS users and groups.  This interface is implemented in both
 * {@link User} and {@link Group} object abstractions.
 * <BR><BR>
 *
 *
 * @version   1.0, 3/31/02
 * @see User
 * @see Group
 */
public interface PTSEntry
{
  /**
   * Constant for {@link User} object implementers, 
   * used with {@link #getType()}
   */
  public static final short PTS_USER  = 0;
  /** 
   * Constant for {@link Group} object implementers,
   * used with {@link #getType()}
   */
  public static final short PTS_GROUP = 1;
  /**
   * Returns the Cell this PTS user or group belongs to.
   *
   * @return the Cell this PTS user or group belongs to
   */
  public Cell getCell();
  /**
   * Returns the creator of this PTS user or group.
   *
   * @return the creator of this PTS user or group
   * @exception AFSException  If an error occurs in the native code
   */
  public PTSEntry getCreator() throws AFSException;
  /**
   * Returns the name of this PTS user or group.
   *
   * @return the name of this PTS user or group
   */
  public String getName();
  /**
   * Returns the owner of this PTS user or group.
   *
   * @return the owner of this PTS user or group
   * @exception AFSException  If an error occurs in the native code
   */
  public PTSEntry getOwner() throws AFSException;
  /**
   * Returns the type of PTS entry the implementing object represents.
   * 
   * <P>Possible values are:<BR>
   * <ul>
   * <li><code>{@link #PTS_USER}</code> 
   *           -- a {@link User} object</li>
   * <li><code>{@link #PTS_GROUP}</code> 
   *           -- a {@link Group} object</li>
   * </ul>
   *
   * @return the name of this PTS user or group
   */
  public short getType();
  /**
   * Returns the numeric AFS id of this user or group.
   *
   * @return the AFS id of this user/group
   * @exception AFSException  If an error occurs in the native code
   */
  public int getUID() throws AFSException;
}



