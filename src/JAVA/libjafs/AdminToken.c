/*
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

#include "Internal.h"
#include "org_openafs_jafs_Cell.h"
#include "org_openafs_jafs_Token.h"

#include <stdio.h>
#include <afs_kasAdmin.h>
#include <afs_ptsAdmin.h>
#include <afs_clientAdmin.h>
#include <kautils.h>
#include <cellconfig.h>
#include <afs_AdminClientErrors.h>

/**
 * Static function used to initialize the client library and the 
 * jafs library.
 *
 * env      the Java environment
 * obj      the current Java object
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Token_initializeAdminClient(JNIEnv *env, jclass cls)
{
  afs_status_t ast;
  if ( !afsclient_Init( &ast ) ) {
    throwAFSException( env, ast );
    return;
  }
}


/**
 * Authenticates the given user and password in the cell.  Returns
 * a token that can be used to prove this authentication later.
 *
 * env      the Java environment
 * obj      the current Java object
 * jcellName    the name of the cell in which to authenticate this user
 * juserName    the name of the user to authenticate
 * jpassword    the password of the user
 * returns a token representing the authentication
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Token_getToken
  (JNIEnv *env, jobject obj, jstring jcellName, jstring juserName,
   jstring jpassword)
{
  afs_status_t ast;
  char *cellName;
  char *userName;
  char *password;
  void *tokenHandle;
  int rc;

  // convert java strings
  if ( jcellName != NULL ) { 
    cellName = getNativeString(env, jcellName);
    if ( !cellName ) {
      throwAFSException( env, JAFSADMNOMEM );
      return 0;    
    }
  } else {
    cellName = NULL;
  }

  if ( juserName != NULL ) {
    userName = getNativeString(env, juserName);
    if ( !userName ) {
      if ( cellName != NULL ) free( cellName );
      throwAFSException( env, JAFSADMNOMEM );
      return 0;    
    }
  } else {
    if ( cellName != NULL ) free( cellName );
    throwAFSException( env, JAFSNULLUSER );
    return 0;
  }

  if ( jpassword != NULL ) {
    password = getNativeString(env, jpassword);
    if ( !password ) {
      if ( cellName != NULL ) free( cellName );
      free( userName );
      throwAFSException( env, JAFSADMNOMEM );
      return 0;    
    }
  } else {
    if ( cellName != NULL ) free( cellName );
    free( userName );
    throwAFSException( env, JAFSNULLPASS );
    return 0;
  }

  if ( !(afsclient_TokenGetNew( cellName, userName, password, &tokenHandle, 
                                &ast) ) ) {
    throwAFSException( env, ast );
  }

  if ( cellName != NULL ) free( cellName );
  free( userName );
  free( password );

  return (jint) tokenHandle;
}

/**
 * Closes the given currently open token.
 *
 * env      the Java environment
 * obj      the current Java object
 * tokenHandle   the token to close
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Token_close
  (JNIEnv *env, jobject obj, jint tokenHandle)
{
  afs_status_t ast;

  if ( !afsclient_TokenClose( (void *) tokenHandle, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }
}

/**
 * Opens a cell for administrative use, based on the token provided.  
 * Returns a cell handle to be used by other methods as a means of 
 * authentication.
 *
 * env      the Java environment
 * obj      the current Java object
 * jcellName    the name of the cell for which to get the handle
 * tokenHandle    a token handle previously returned by a call to getToken
 * returns a handle to the open cell
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Cell_getCellHandle
  (JNIEnv *env, jobject obj, jstring jcellName, jint tokenHandle)
{
  afs_status_t ast;
  char *cellName;
  void *cellHandle;

  if ( jcellName != NULL ) {
    cellName = getNativeString(env, jcellName);
    if ( !cellName ) {
	throwAFSException( env, JAFSADMNOMEM );
	return -1;
    }
  } else {
    throwAFSException( env, JAFSNULLCELL );
    return -1;
  }

  if ( !afsclient_CellOpen( cellName, (void *) tokenHandle, 
			   &cellHandle, &ast ) ) {
    throwAFSException( env, ast );
  }

  free( cellName );

  return (jint) cellHandle;
}

/**
 * Closes the given currently open cell handle.
 *
 * env      the Java environment
 * obj      the current Java object
 * cellHandle   the cell handle to close
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Cell_closeCell 
  (JNIEnv *env, jobject obj, jint cellHandle)
{
  afs_status_t ast;

  if ( !afsclient_CellClose( (void *) cellHandle, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }
}

/**
 * Opens a server for administrative vos use, based on the cell handle 
 * provided.  Returns a vos server handle to be used by other 
 * methods as a means of identification.
 *
 * env      the Java environment
 * obj      the current Java object
 * cellHandle    a cell handle previously returned by 
 *                      a call to getCellHandle
 * jserverName    the name of the server for which to retrieve 
 *                      a vos handle
 * returns a vos handle to the server
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Server_getVosServerHandle
  (JNIEnv *env, jobject obj, jint cellHandle, jstring jserverName)
{
  afs_status_t ast;
  void *serverHandle;
  char *serverName;

  if ( jserverName != NULL ) {
    serverName = getNativeString(env, jserverName);
    if ( !serverName ) {
      throwAFSException( env, JAFSADMNOMEM );
      return -1;
    }
  } else {
    throwAFSException( env, JAFSNULLSERVER );
    return -1;
  }

  if ( !vos_ServerOpen( (void *) cellHandle, serverName, 
                       (void **) &serverHandle, &ast ) ) {
    throwAFSException( env, ast );
  }

  // release converted string
  free( serverName );

  return (jint) serverHandle;
}

/**
 * Closes the given currently open vos server handle.
 *
 * env      the Java environment
 * obj      the current Java object
 * vosServerHandle   the vos server handle to close
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Server_closeVosServerHandle
  (JNIEnv *env, jobject obj, jint vosServerHandle)
{
  afs_status_t ast;

  if ( !vos_ServerClose( (void *) vosServerHandle, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }
}

/**
 * Opens a server for administrative bos use, based on the cell handle 
 * provided.  Returns a bos server handle to be used by other methods 
 * as a means of identification.
 *
 * env      the Java environment
 * obj      the current Java object
 * cellHandle    a cell handle previously returned by a call 
 *                      to getCellHandle
 * jserverName    the name of the server for which to retrieve 
 *                      a bos handle
 * returns a bos handle to the server
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Server_getBosServerHandle
  (JNIEnv *env, jobject obj, jint cellHandle, jstring jserverName)
{
  afs_status_t ast;
  void *serverHandle;
  char *serverName;

  if ( jserverName != NULL ) {
    serverName = getNativeString(env, jserverName);
    if ( !serverName ) {
      throwAFSException( env, JAFSADMNOMEM );
      return;
    }
  } else {
    throwAFSException( env, JAFSNULLSERVER );
    return;
  }

  if ( !bos_ServerOpen( (void *) cellHandle, serverName, 
                       (void **) &serverHandle, &ast ) ) {
    throwAFSException( env, ast );
  }

  // release converted string
  free( serverName );

  return (jint) serverHandle;
}

/**
 * Closes the given currently open bos server handle.
 *
 * env      the Java environment
 * obj      the current Java object
 * bosServerHandle   the bos server handle to close
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Server_closeBosServerHandle
  (JNIEnv *env, jobject obj, jint bosServerHandle)
{
  afs_status_t ast;

  if ( !bos_ServerClose( (void *) bosServerHandle, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }
}

/**
 * Gets the expiration time for a given token.
 *
 * env      the Java environment
 * obj      the current Java object
 *
 * tokenHandle    a token handle previously returned by a call 
 *                       to getToken
 * returns a long representing the UTC time for the token expiration
 */
JNIEXPORT jlong JNICALL 
Java_org_openafs_jafs_Token_getExpiration
  (JNIEnv *env, jobject obj, jint tokenHandle)
{
  afs_status_t ast;
  unsigned long expTime;
  char *prince = malloc( sizeof(char)*KAS_MAX_NAME_LEN ); 
  char *inst   = malloc( sizeof(char)*KAS_MAX_NAME_LEN );    
  char *cell   = malloc( sizeof(char)*AFS_MAX_SERVER_NAME_LEN );    
  int hkt;

  if ( !prince || !inst || !cell ) {
    if ( prince ) {
      free( prince );
    }
    if ( inst ) {
      free( inst );
    }
    if ( cell ) {
      free( cell );
    }
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }

  if ( !afsclient_TokenQuery( (void *) tokenHandle, &expTime, prince, inst, 
                             cell, &hkt, &ast ) ) {
    throwAFSException( env, ast );
  }

  free( prince );
  free( inst );
  free( cell );

  return (jlong) expTime;
}

// reclaim global memory used by this portion
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Token_reclaimAuthMemory (JNIEnv *env, jclass cls)
{
}
