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

#include <stdio.h>
#include <afs_kasAdmin.h>
#include <afs_ptsAdmin.h>
#include <afs_clientAdmin.h>
#include <pterror.h>
#include <kautils.h>
#include <ptclient.h>
#include <afs_AdminPtsErrors.h>
#include <afs_AdminClientErrors.h>
#include <afs_AdminCommonErrors.h>

//// definitions in Internal.c  //////////////////

extern jclass userCls;
//extern jfieldID user_cellHandleField;
extern jfieldID user_nameField;
extern jfieldID user_cachedInfoField;

extern jclass groupCls;
//extern jfieldID group_cellHandleField;
extern jfieldID group_nameField;
extern jfieldID group_cachedInfoField;

extern jclass serverCls;
//extern jfieldID server_cellHandleField;
extern jfieldID server_cachedInfoField;

//////////////////////////////////////////////////////////

///// definition in jafs_User.c /////////////////

extern void getUserInfoChar (JNIEnv *env, void *cellHandle, const char *name, 
			     jobject user);

///////////////////////////////////////////////////

///// definition in jafs_Group.c /////////////////

extern void getGroupInfoChar (JNIEnv *env, void *cellHandle, const char *name, 
			      jobject group);

///////////////////////////////////////////////////

///// definition in jafs_Server.c /////////////////

extern void fillServerInfo (JNIEnv *env, void *cellHandle, jobject server, 
			    afs_serverEntry_t servEntry);

///////////////////////////////////////////////////

/**
 * Returns the total number of KAS users belonging to the cell denoted
 * by cellHandle.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the users belong
 *  returns total count of KAS users
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Cell_getKasUserCount (JNIEnv *env, jclass cls, 
					       jlong cellHandle) {

  afs_status_t ast;
  void *iterationId;
  kas_identity_t who;
  int i = 0;

  if( !kas_PrincipalGetBegin( (void *) cellHandle, NULL, 
			      &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return -1;
  }

  while ( kas_PrincipalGetNext( iterationId, &who, &ast ) ) i++;

  if( ast != ADMITERATORDONE ) {
    throwAFSException( env, ast );
    return -1;
  }

  return i;
}


/**
 * Begin the process of getting the kas users that belong to the cell.  
 * Returns an iteration ID to be used by subsequent calls to 
 * getKasUsersNextString (or getKasUsersNext) 
 * and getKasUsersDone.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the users belong
 *  returns an iteration ID
 */
JNIEXPORT jlong JNICALL 
Java_org_openafs_jafs_Cell_getKasUsersBegin (JNIEnv *env, jclass cls, 
						jlong cellHandle) {

  afs_status_t ast;
  void *iterationId;

  if( !kas_PrincipalGetBegin( (void *) cellHandle, NULL, &iterationId, 
			      &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

  return (jlong) iterationId;

}

/**
 * Begin the process of getting the KAS users, starting at
 * startIndex, that belong to the cell.  
 * Returns an iteration ID to be used by subsequent calls to 
 * getKasUsersNextString (or getKasUsersNext) 
 * and getKasUsersDone.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the users belong
 *  startIndex    the starting base-zero index
 *  returns an iteration ID
 */
JNIEXPORT jlong JNICALL 
Java_org_openafs_jafs_Cell_getKasUsersBeginAt (JNIEnv *env, jclass cls, 
						  jlong cellHandle, 
						  jint startIndex) {

  afs_status_t ast;
  void *iterationId;
  kas_identity_t who;
  int i;

  if( !kas_PrincipalGetBegin( (void *) cellHandle, NULL, 
			      &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

  for ( i = 1; i < startIndex; i++) {
    if( !kas_PrincipalGetNext( (void *) iterationId, &who, &ast ) ) {
      if( ast == ADMITERATORDONE ) {
        return 0;
      } else {
        throwAFSException( env, ast );
        return 0;
      }
    }
  }

  return (jlong) iterationId;

}

/**
 * Returns the next kas user of the cell.  Returns null if there
 * are no more users.  Appends instance names to principal names as follows:
 * principal.instance
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  iterationId   the iteration ID of this iteration
 *  returns the name of the next user of the cell
 */
JNIEXPORT jstring JNICALL 
Java_org_openafs_jafs_Cell_getKasUsersNextString (JNIEnv *env, jclass cls, 
						     jlong iterationId) {

  afs_status_t ast;
  kas_identity_t who;
  jstring juser;

  if( !kas_PrincipalGetNext( (void *) iterationId, &who, &ast ) ) {
    if( ast == ADMITERATORDONE ) {
	return NULL;
    // other
    } else {
      throwAFSException( env, ast );
      return;
    }    
  }

  if( strcmp( who.instance, "" ) ) {
    char *fullName = (char *) malloc( sizeof(char)*( strlen( who.principal ) 
						     + strlen( who.instance ) 
						     + 2 ) );
    if( !fullName ) {
      throwAFSException( env, JAFSADMNOMEM );
      return;    
    }
    *fullName = '\0';
    strcat( fullName, who.principal );
    strcat( fullName, "." );
    strcat( fullName, who.instance );
    juser = (*env)->NewStringUTF(env, fullName );
    free( fullName );
  } else {
    juser = (*env)->NewStringUTF(env, who.principal);
  }

  return juser;

}

/**
 * Fills the next kas user object of the cell.  Returns 0 if there
 * are no more users, != 0 otherwise.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the users belong
 *  iterationId   the iteration ID of this iteration
 *  juserObject   a User object to be populated with the values of 
 *                  the next kas user
 *  returns 0 if there are no more users, != 0 otherwise
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Cell_getKasUsersNext (JNIEnv *env, jclass cls, 
					       jlong cellHandle, 
					       jlong iterationId, 
					       jobject juserObject) {

  afs_status_t ast;
  kas_identity_t who;
  jstring juser;
  char *fullName = NULL;


  if( !kas_PrincipalGetNext( (void *) iterationId, &who, &ast ) ) {
    if( ast == ADMITERATORDONE ) {
	return 0;
    // other
    } else {
      throwAFSException( env, ast );
      return 0;
    }    
  }

  // take care of the instance stuff(by concatenating with a period in between)
  if( strcmp( who.instance, "" ) ) {
    fullName = (char *) malloc( sizeof(char)*( strlen( who.principal ) + 
					       strlen( who.instance ) + 2 ) );
    if( !fullName ) {
      throwAFSException( env, JAFSADMNOMEM );
      return 0;    
    }
    *fullName = '\0';
    strcat( fullName, who.principal );
    strcat( fullName, "." );
    strcat( fullName, who.instance );
    juser = (*env)->NewStringUTF(env, fullName );
  } else {
    juser = (*env)->NewStringUTF(env, who.principal);
  }

  if( userCls == 0 ) {
    internal_getUserClass( env, juserObject );
  }

  (*env)->SetObjectField(env, juserObject, user_nameField, juser);

  if( fullName != NULL ) { 
      getUserInfoChar( env, (void *) cellHandle, fullName, juserObject );
      free( fullName );
  } else {
      getUserInfoChar( env, (void *) cellHandle, who.principal, juserObject );
  }
  (*env)->SetBooleanField( env, juserObject, user_cachedInfoField, TRUE );

  return 1;

}

/**
 * Signals that the iteration is complete and will not be accessed anymore.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  iterationId   the iteration ID of this iteration
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Cell_getKasUsersDone (JNIEnv *env, jclass cls, 
					       jlong iterationId) {

  afs_status_t ast;

  if( !kas_PrincipalGetDone( (void *) iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Returns the name of the cell.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the user belongs
 *  returns the name of the cell
 */
JNIEXPORT jstring JNICALL 
Java_org_openafs_jafs_Cell_getCellName (JNIEnv *env, jclass cls, 
					   jlong cellHandle) {

  afs_status_t ast;
  char *cellName;
  jstring jcellName;

  if( !afsclient_CellNameGet( (void *) cellHandle, 
			      (const char **) &cellName, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }
  
  jcellName = (*env)->NewStringUTF(env, cellName);

  return jcellName;

}

/**
 * Returns the total number of PTS users belonging to the cell denoted
 * by cellHandle.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the users belong
 *  returns total number of PTS users
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Cell_getPtsUserCount (JNIEnv *env, jclass cls, 
					       jlong cellHandle) {

  afs_status_t ast;
  void *iterationId;
  char *userName;
  int i = 0;

  if( !pts_UserListBegin( (void *) cellHandle, &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return -1;
  }

  userName = (char *) malloc( sizeof(char)*PTS_MAX_NAME_LEN);

  if( !userName ) {
    throwAFSException( env, JAFSADMNOMEM );
    return -1;    
  }

  while ( pts_UserListNext( (void *) iterationId, userName, &ast ) ) i++;

  free( userName );

  if( ast != ADMITERATORDONE ) {
    throwAFSException( env, ast );
    return -1;
  }

  return i;
}

/**
 * Returns the total number of PTS users, belonging to the cell denoted
 * by cellHandle, that are not in KAS.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the users belong
 *  returns total number of users that are in PTS and not KAS
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Cell_getPtsOnlyUserCount (JNIEnv *env, jclass cls, 
						   jlong cellHandle) {

  afs_status_t ast;
  void *iterationId;
  kas_identity_p who = (kas_identity_p) malloc( sizeof(kas_identity_t) );
  kas_principalEntry_t kasEntry;
  char *userName;
  int i = 0;

  if( !who ) {
    throwAFSException( env, JAFSADMNOMEM );
    return -1;
  }

  if( !pts_UserListBegin( (void *) cellHandle, &iterationId, &ast ) ) {
    free( who );
    throwAFSException( env, ast );
    return -1;
  }

  userName = (char *) malloc( sizeof(char)*PTS_MAX_NAME_LEN);
    
  if( !userName ) {
    free( who );
    throwAFSException( env, JAFSADMNOMEM );
    return -1;
  }

  while ( pts_UserListNext( (void *) iterationId, userName, &ast ) ) {
    if( strcmp( userName, "anonymous" ) != 0 ) {
      // make sure the name is within the allowed bounds
      if( strlen( userName ) > KAS_MAX_NAME_LEN ) {
	    free( who );
	    free( userName );
	    throwAFSException( env, ADMPTSUSERNAMETOOLONG );
	    return -1;
	  }
  
      // if there is a kas entry, recurse
      internal_makeKasIdentity( userName, who );
      if( !kas_PrincipalGet( (void *) cellHandle, NULL, who, 
			     &kasEntry, &ast ) ) i++;
	}
  }

  free( userName );
  free( who );

  if( ast != ADMITERATORDONE ) {
    throwAFSException( env, ast );
    return -1;
  }

  return i;
}

/**
 * Begin the process of getting the pts users that belong to the cell.  
 * Returns an iteration ID to be used by subsequent calls to 
 * getPtsUsersNextString (or getPtsUsersNext) 
 * and getPtsUsersDone.  
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the users belong
 *  returns an iteration ID
 */
JNIEXPORT jlong JNICALL 
Java_org_openafs_jafs_Cell_getPtsUsersBegin (JNIEnv *env, jclass cls, 
						jlong cellHandle) {

  afs_status_t ast;
  void *iterationId;

  if( !pts_UserListBegin( (void *) cellHandle, &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

  return (jlong) iterationId;

}

/**
 * Returns the next pts user of the cell.  Returns null if 
 * there are no more users.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  iterationId   the iteration ID of this iteration
 *  returns the name of the next user of the cell
 */
JNIEXPORT jstring JNICALL 
Java_org_openafs_jafs_Cell_getPtsUsersNextString (JNIEnv *env, jclass cls, 
						     jlong iterationId) {

  afs_status_t ast;
  char *userName;
  jstring juser;

  userName = (char *) malloc( sizeof(char)*PTS_MAX_NAME_LEN);

  if( !userName ) {
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }

  if( !pts_UserListNext( (void *) iterationId, userName, &ast ) ) {
    free( userName );
    if( ast == ADMITERATORDONE ) {
      return NULL;
    } else {
      throwAFSException( env, ast );
      return;
    }
  }

  if( strcmp( userName, "anonymous" ) == 0 ) {
    free( userName );
    return Java_org_openafs_jafs_Cell_getPtsUsersNextString( env, cls, 
								iterationId );
  }

  juser = (*env)->NewStringUTF(env, userName);
  free( userName );
  return juser;

}

/**
 * Returns the next pts user (who is not a kas user) of the cell.  
 * Returns null if there are no more users.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  iterationId   the iteration ID of this iteration
 *  cellHandle   the cell handle to which these users will belong
 *  returns the name of the next pts user (not kas user) of the cell
 */
JNIEXPORT jstring JNICALL 
Java_org_openafs_jafs_Cell_getPtsOnlyUsersNextString (JNIEnv *env, 
							 jclass cls, 
							 jlong iterationId, 
							 jlong cellHandle) {

    kas_identity_p who = (kas_identity_p) malloc( sizeof(kas_identity_t) );
    kas_principalEntry_t kasEntry;
    afs_status_t ast;
    char *userName;
    jstring juser;

    if( !who ) {
      throwAFSException( env, JAFSADMNOMEM );
      return;    
    }

    userName = (char *) malloc( sizeof(char)*PTS_MAX_NAME_LEN);
    
    if( !userName ) {
      free( who );
      throwAFSException( env, JAFSADMNOMEM );
      return;    
    }

    while( 1 ) {

	if( !pts_UserListNext( (void *) iterationId, userName, &ast ) ) {
	    free( userName );
	    free( who );
	    if( ast == ADMITERATORDONE ) {
		return NULL;
	    } else {
		throwAFSException( env, ast );
		return NULL;
	    }
	}

	if( strcmp( userName, "anonymous" ) == 0 ) {
	    continue;
	}
	
	// make sure the name is within the allowed bounds
	if( strlen( userName ) > KAS_MAX_NAME_LEN ) {
	    free( who );
	    free( userName );
	    throwAFSException( env, ADMPTSUSERNAMETOOLONG );
	    return NULL;
	}
	
	// if there is a kas entry, recurse
	internal_makeKasIdentity( userName, who );
	if( kas_PrincipalGet( (void *) cellHandle, NULL, who, 
			      &kasEntry, &ast ) ) {
	    continue;
	}
	
	juser = (*env)->NewStringUTF(env, userName);
	free( userName );
	free( who );
	return juser;

    }

}

/**
 * Fills the next pts user object of the cell.  Returns 0 if there
 * are no more users, != 0 otherwise.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the users belong
 *  iterationId   the iteration ID of this iteration
 *  juserObject   a User object to be populated with the values of 
 *                  the next pts user
 *  returns 0 if there are no more users, != 0 otherwise
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Cell_getPtsUsersNext (JNIEnv *env, jclass cls, 
					       jlong cellHandle, 
					       jlong iterationId, 
					       jobject juserObject ) {
    
  afs_status_t ast;
  char *userName;
  jstring juser;

  userName = (char *) malloc( sizeof(char)*PTS_MAX_NAME_LEN);

  if( !userName ) {
    throwAFSException( env, JAFSADMNOMEM );
    return 0;    
  }

  if( !pts_UserListNext( (void *) iterationId, userName, &ast ) ) {
    free( userName );
    if( ast == ADMITERATORDONE ) {
      return 0;
    } else {
      throwAFSException( env, ast );
      return 0;
    }
  }

  if( strcmp( userName, "anonymous" ) == 0 ) {
    free( userName );
    return Java_org_openafs_jafs_Cell_getPtsUsersNext( env, cls, 
							  cellHandle, 
							  iterationId, 
							  juserObject );
  }
  
  juser = (*env)->NewStringUTF(env, userName);

  if( userCls == 0 ) {
    internal_getUserClass( env, juserObject );
  }

  (*env)->SetObjectField(env, juserObject, user_nameField, juser);

  getUserInfoChar( env, (void *) cellHandle, userName, juserObject );
  (*env)->SetBooleanField( env, juserObject, user_cachedInfoField, TRUE );

  free( userName );
  return 1;

}

/**
 * Fills the next pts user (who does not have a kas entry) object of 
 * the cell.  Returns 0 if there are no more users, != 0 otherwise.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the users belong
 *  iterationId   the iteration ID of this iteration
 *  juserObject   a User object to be populated with the values of 
 *                  the next pts (with no kas) user
 *  returns 0 if there are no more users, != 0 otherwise
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Cell_getPtsOnlyUsersNext (JNIEnv *env, jclass cls, 
						   jlong cellHandle, 
						   jlong iterationId, 
						   jobject juserObject ) {

  kas_identity_p who = (kas_identity_p) malloc( sizeof(kas_identity_t) );
  kas_principalEntry_t kasEntry;
  afs_status_t ast;
  char *userName;
  jstring juser;

  if( !who ) {
    throwAFSException( env, JAFSADMNOMEM );
    return 0;    
  }
  
  userName = (char *) malloc( sizeof(char)*PTS_MAX_NAME_LEN);

  if( !userName ) {
    free( who );
    throwAFSException( env, JAFSADMNOMEM );
    return 0;    
  }

  while( 1 ) {

      if( !pts_UserListNext( (void *) iterationId, userName, &ast ) ) {
	  free( userName );
	  free( who );
	  if( ast == ADMITERATORDONE ) {
	      return 0;
	  } else {
	      throwAFSException( env, ast );
	      return 0;
	  }
      }
      
      if( strcmp( userName, "anonymous" ) == 0 ) {
	  continue;
      }
      
      // make sure the name is within the allowed bounds
      if( strlen( userName ) > KAS_MAX_NAME_LEN ) {
	  free( userName );
	  free( who );
	  throwAFSException( env, ADMPTSUSERNAMETOOLONG );
	  return 0;
      }
      
      if( userCls == 0 ) {
	  internal_getUserClass( env, juserObject );
      }
      
      
      // if there is a kas entry, recurse
      internal_makeKasIdentity( userName, who );
      if( kas_PrincipalGet( (void *) cellHandle, NULL, who, 
			    &kasEntry, &ast ) ) {
	  continue;
      } 
      
      juser = (*env)->NewStringUTF(env, userName);
      
      (*env)->SetObjectField(env, juserObject, user_nameField, juser);
      getUserInfoChar( env, (void *) cellHandle, userName, juserObject );
      (*env)->SetBooleanField( env, juserObject, user_cachedInfoField, TRUE );
      
      free( who );
      free( userName );
      return 1;
      
  }

}

/**
 * Signals that the iteration is complete and will not be accessed anymore.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  iterationId   the iteration ID of this iteration
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Cell_getPtsUsersDone (JNIEnv *env, jclass cls, 
					       jlong iterationId) {

  afs_status_t ast;

  if( !pts_UserListDone( (void *) iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }
  
}

/**
 * Returns the total number of groups belonging to the cell denoted
 * by cellHandle.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the groups belong
 *  returns total number of groups
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Cell_getGroupCount (JNIEnv *env, jclass cls, 
					     jlong cellHandle) {

  afs_status_t ast;
  void *iterationId;
  char *groupName;
  int i = 0;

  if( !pts_GroupListBegin( (void *) cellHandle, &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return -1;
  }

  groupName = (char *) malloc( sizeof(char)*PTS_MAX_NAME_LEN);

  if( !groupName ) {
    throwAFSException( env, JAFSADMNOMEM );
    return -1;
  }

  while ( pts_GroupListNext( (void *) iterationId, groupName, &ast ) ) i++;

  free( groupName );

  if( ast != ADMITERATORDONE ) {
    throwAFSException( env, ast );
    return -1;
  }

  return i;
}

/**
 * Begin the process of getting the groups that belong to the cell.  Returns 
 * an iteration ID to be used by subsequent calls to 
 * getGroupsNextString (or getGroupsNext) and 
 * getGroupsDone.  
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the groups belong
 *  returns an iteration ID
 */
JNIEXPORT jlong JNICALL 
Java_org_openafs_jafs_Cell_getGroupsBegin (JNIEnv *env, jclass cls, 
					      jlong cellHandle) {

  afs_status_t ast;
  void *iterationId;

  if( !pts_GroupListBegin( (void *) cellHandle, &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

  return (jlong) iterationId;

}

/**
 * Begin the process of getting the groups that belong to the cell, starting
 * with element index startIndex.  Returns an iteration ID to 
 * be used by subsequent calls to getGroupsNextString 
 * (or getGroupsNext) and getGroupsDone.  
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the groups belong
 *  startIndex    the starting base-zero index
 *  returns an iteration ID
 */
JNIEXPORT jlong JNICALL 
Java_org_openafs_jafs_Cell_getGroupsBeginAt (JNIEnv *env, jclass cls, 
						jlong cellHandle, 
						jint startIndex) {

  afs_status_t ast;
  void *iterationId;
  char *groupName;
  int i;

  groupName = (char *) malloc( sizeof(char)*PTS_MAX_NAME_LEN);

  if( !pts_GroupListBegin( (void *) cellHandle, &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return 0;
  }

  for ( i = 1; i < startIndex; i++) {
    if( !pts_GroupListNext( (void *) iterationId, groupName, &ast ) ) {
      free( groupName );
      if( ast == ADMITERATORDONE ) {
        return 0;
      } else {
        throwAFSException( env, ast );
        return 0;
      }
    }
  }

  free( groupName );
  return (jlong) iterationId;

}

/**
 * Returns the next group of the cell.  Returns null if there
 * are no more groups.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  iterationId   the iteration ID of this iteration
 *  returns the name of the next user of the cell
 */
JNIEXPORT jstring JNICALL 
Java_org_openafs_jafs_Cell_getGroupsNextString (JNIEnv *env, jclass cls, 
						   jlong iterationId) {

  afs_status_t ast;
  char *groupName;
  jstring jgroup;

  groupName = (char *) malloc( sizeof(char)*PTS_MAX_NAME_LEN);

  if( !groupName ) {
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }

  if( !pts_GroupListNext( (void *) iterationId, groupName, &ast ) ) {
    free( groupName );
    if( ast == ADMITERATORDONE ) {
      return NULL;
    } else {
      throwAFSException( env, ast );
      return;
    }
  }

  jgroup = (*env)->NewStringUTF(env, groupName);
  free( groupName );
  return jgroup;

}

/**
 * Fills the next group object of the cell.  Returns 0 if there
 * are no more groups, != 0 otherwise.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the users belong
 *  iterationId   the iteration ID of this iteration
 *  jgroupObject   a Group object to be populated with the values of 
 *                   the next group
 *  returns 0 if there are no more users, != 0 otherwise
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Cell_getGroupsNext (JNIEnv *env, jclass cls, 
					     jlong cellHandle, 
					     jlong iterationId, 
					     jobject jgroupObject) {

  afs_status_t ast;
  char *groupName;
  jstring jgroup;

  groupName = (char *) malloc( sizeof(char)*PTS_MAX_NAME_LEN );

  if( !groupName ) {
    throwAFSException( env, JAFSADMNOMEM );
      return;    
  }
  
  if( !pts_GroupListNext( (void *) iterationId, groupName, &ast ) ) {
    free( groupName );
    if( ast == ADMITERATORDONE ) {
      return 0;
    } else {
      throwAFSException( env, ast );
      return 0;
    }
  }

  jgroup = (*env)->NewStringUTF(env, groupName);

  if( groupCls == 0 ) {
    internal_getGroupClass( env, jgroupObject );
  }

  (*env)->SetObjectField(env, jgroupObject, group_nameField, jgroup);
  getGroupInfoChar( env, (void *)cellHandle, groupName, jgroupObject );
  (*env)->SetBooleanField( env, jgroupObject, group_cachedInfoField, TRUE );

  free( groupName );
  return 1;

}

/**
 * Signals that the iteration is complete and will not be accessed anymore.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  iterationId   the iteration ID of this iteration
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Cell_getGroupsDone (JNIEnv *env, jclass cls, 
					     jlong iterationId) {

  afs_status_t ast;

  if( !pts_GroupListDone( (void *) iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }
  
}

/**
 * Gets the maximum group pts ID that's been used within a cell.   
 * The next auto-assigned group ID will be one less (more negative) 
 * than this value.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the group belongs
 *  returns an integer reresenting the max group id in a cell
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Cell_getMaxGroupID (JNIEnv *env, jclass cls, 
					     jlong cellHandle) {

  afs_status_t ast;
  int maxID;

  if( !pts_GroupMaxGet( (void *) cellHandle, &maxID, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

  return (jint)maxID;

}

/**
 * Sets the maximum group pts ID that's been used within a cell.  The next 
 * auto-assigned group ID will be one less (more negative) than this value.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the group belongs
 *  maxID an integer reresenting the new max group id in a cell
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Cell_setMaxGroupID (JNIEnv *env, jclass cls, 
					     jlong cellHandle, jint maxID) {

  afs_status_t ast;

  if( !pts_GroupMaxSet( (void *) cellHandle, (int) maxID, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Gets the maximum user pts ID that's been used within a cell.   
 * The next auto-assigned user ID will be one greater (more positive) 
 * than this value.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the user belongs
 *  returns an integer reresenting the max user id in a cell
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Cell_getMaxUserID (JNIEnv *env, jclass cls, 
					    jlong cellHandle) {

  afs_status_t ast;
  int maxID;

  if( !pts_UserMaxGet( (void *) cellHandle, &maxID, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

  return (jint)maxID;

}

/**
 * Sets the maximum user pts ID that's been used within a cell.  The next 
 * auto-assigned user ID will be one greater (more positive) than this value.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the user belongs
 *  maxID an integer reresenting the new max user id in a cell
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Cell_setMaxUserID (JNIEnv *env, jclass cls, 
					    jlong cellHandle, jint maxID) {

  afs_status_t ast;

  if( !pts_UserMaxSet( (void *) cellHandle, (int) maxID, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Returns the total number of servers belonging to the cell denoted
 * by cellHandle.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the servers belong
 *  returns total number of servers
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Cell_getServerCount (JNIEnv *env, jclass cls, 
					      jlong cellHandle) {

  afs_status_t ast;
  void *iterationId;
  afs_serverEntry_t servEntry;
  int i = 0;

  if( !afsclient_AFSServerGetBegin( (void *) cellHandle, 
				    &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return -1;
  }

  while ( afsclient_AFSServerGetNext( (void *) iterationId, 
				      &servEntry, &ast ) ) i++;

  if( ast != ADMITERATORDONE ) {
    throwAFSException( env, ast );
    return -1;
  }

  return i;
}

/**
 * Begin the process of getting the servers in the cell.  Returns 
 * an iteration ID to be used by subsequent calls to 
 * getServersNextString and getServersDone.  
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the servers belong
 *  returns an iteration ID
 */
JNIEXPORT jlong JNICALL 
Java_org_openafs_jafs_Cell_getServersBegin (JNIEnv *env, jclass cls, 
					       jlong cellHandle) {

  afs_status_t ast;
  void *iterationId;

  if( !afsclient_AFSServerGetBegin( (void *) cellHandle, 
				    &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

  return (jlong) iterationId;

}

/**
 * Returns the next server of the cell.  Returns null if there
 * are no more servers.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  iterationId   the iteration ID of this iteration
 *  returns the name of the next server of the cell
 */
JNIEXPORT jstring JNICALL 
Java_org_openafs_jafs_Cell_getServersNextString (JNIEnv *env, jclass cls, 
						    jlong iterationId) {

  afs_status_t ast;
  jstring jserver;
  afs_serverEntry_t servEntry;

  if( !afsclient_AFSServerGetNext( (void *) iterationId, &servEntry, &ast ) ) {
      if( ast == ADMITERATORDONE ) {
	  return NULL;
      } else {
	  throwAFSException( env, ast );
	  return NULL;
      }
  }
  
  jserver = (*env)->NewStringUTF(env, "not_implemented"); /* psomogyi 20050514 */

  return jserver;
}

/**
 * Fills the next server object of the cell.  Returns 0 if there are no 
 * more servers, != 0 otherwise.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the users belong
 *  iterationId   the iteration ID of this iteration
 *  jserverObject   a Server object to be populated with the values 
 *                    of the next server 
 *  returns 0 if there are no more servers, != 0 otherwise
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Cell_getServersNext
  (JNIEnv *env, jclass cls, jlong cellHandle, jlong iterationId,
   jobject jserverObject)
{
  afs_status_t ast;
  jstring jserver;
  afs_serverEntry_t servEntry;
  jintArray jaddress;

  if( !afsclient_AFSServerGetNext( (void *) iterationId, &servEntry, &ast ) ) {
    if( ast == ADMITERATORDONE ) {
      return 0;
    } else {
      throwAFSException( env, ast );
      return 0;
    }
  }

  // get the class fields if need be
  if( serverCls == 0 ) {
    internal_getServerClass( env, jserverObject );
  }

  fillServerInfo( env, (void *) cellHandle, jserverObject, servEntry );

  (*env)->SetBooleanField( env, jserverObject, server_cachedInfoField, TRUE );

  return 1;
}

/**
 * Signals that the iteration is complete and will not be accessed anymore.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  iterationId   the iteration ID of this iteration
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Cell_getServersDone
  (JNIEnv *env, jclass cls, jlong iterationId)
{
  afs_status_t ast;

  if( !afsclient_AFSServerGetDone( (void *) iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Creates a mount point for a volume within the file system.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  cellHandle    the handle of the cell to which the user belongs
 *  jdirectory    the full path of the place in the AFS file system 
 *                     at which to mount the volume
 *  jvolumeName   the name of the volume to mount
 *  readWrite   whether or not this is to be a readwrite mount point
 *  forceCheck  whether or not to check if this volume name exists
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Cell_createMountPoint (JNIEnv *env, jclass cls, 
						jlong cellHandle, 
						jstring jdirectory, 
						jstring jvolumeName, 
						jboolean readWrite, 
						jboolean forceCheck) {

  afs_status_t ast;
  const char *directory;
  const char *volumeName;
  vol_type_t type;
  vol_check_t check;

  if( jdirectory != NULL ) {
    directory = (*env)->GetStringUTFChars(env, jdirectory, 0);
    if( !directory ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    directory = NULL;
  }
  if( jvolumeName != NULL ) {
    volumeName = (*env)->GetStringUTFChars(env, jvolumeName, 0);
    if( !volumeName ) {
      if( directory != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jdirectory, directory);
      }
      throwAFSException( env, JAFSADMNOMEM );
      return;    
    }
  } else {
    volumeName = NULL;
  }

  if( readWrite ) {
    type = READ_WRITE;
  } else {
    type = READ_ONLY;
  }

  if( forceCheck ) {
    check = CHECK_VOLUME;
  } else {
    check = DONT_CHECK_VOLUME;
  }

  if( !afsclient_MountPointCreate( (void *) cellHandle, directory, 
				   volumeName, type, check, &ast ) ) {
    if( volumeName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jvolumeName, volumeName);
    }
    if( directory != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jdirectory, directory);
    }
    throwAFSException( env, ast );
    return;
  }

  if( volumeName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jvolumeName, volumeName);
  }
  if( directory != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jdirectory, directory);
  }

}

/*
 * Sets an ACL for a given place in the AFS file system.
 *
 *  env      the Java environment
 *  cls      the current Java class
 *  jdirectory    the full path of the place in the AFS file system 
 *                     for which to add an entry
 *  jusername   the name of the user or group for which to add an entry
 *  read    whether or not to allow read access to this user
 *  write    whether or not to allow write access to this user
 *  lookup    whether or not to allow lookup access to this user
 *  delete    whether or not to allow deletion access to this user
 *  insert    whether or not to allow insertion access to this user
 *  lock    whether or not to allow lock access to this user
 *  admin    whether or not to allow admin access to this user
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Cell_setACL (JNIEnv *env, jclass cls, 
				      jstring jdirectory, jstring juserName, 
				      jboolean read, jboolean write, 
				      jboolean lookup, jboolean delete, 
				      jboolean insert, jboolean lock, 
				      jboolean admin) {

  afs_status_t ast;
  const char *directory;
  const char *userName;
  acl_t acl;

  // Added by MP
  if( !afsclient_Init( &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

  if( jdirectory != NULL ) {
    directory = (*env)->GetStringUTFChars(env, jdirectory, 0);
    if( !directory ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    directory = NULL;
  }
  if( juserName != NULL ) {
    userName = (*env)->GetStringUTFChars(env, juserName, 0);
    if( !userName ) {
      if( directory != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jdirectory, directory);
      }
      throwAFSException( env, JAFSADMNOMEM );
      return;    
    }
  } else {
    userName = NULL;
  }

  if( read ) {
    acl.read = READ;
  } else {
    acl.read = NO_READ;
  }

  if( write ) {
    acl.write = WRITE;
  } else {
    acl.write = NO_WRITE;
  }

  if( lookup ) {
    acl.lookup = LOOKUP;
  } else {
    acl.lookup = NO_LOOKUP;
  }

  if( delete ) {
    acl.del = DELETE;
  } else {
    acl.del = NO_DELETE;
  }

  if( insert ) {
    acl.insert = INSERT;
  } else {
    acl.insert = NO_INSERT;
  }

  if( lock ) {
    acl.lock = LOCK;
  } else {
    acl.lock = NO_LOCK;
  }

  if( admin ) {
    acl.admin = ADMIN;
  } else {
    acl.admin = NO_ADMIN;
  }

  if( !afsclient_ACLEntryAdd( directory, userName, &acl, &ast ) ) {
      if( userName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, juserName, userName);
      }
      if( directory != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jdirectory, directory);
      }
      throwAFSException( env, ast );
      return;
  }

  if( userName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, juserName, userName);
  }
  if( directory != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jdirectory, directory);
  }

}

// reclaim global memory used by this portion
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Cell_reclaimCellMemory (JNIEnv *env, jclass cls) {

}






















