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
#include "org_openafs_jafs_User.h"

#include <stdio.h>
#include <string.h>
#include <afs_ptsAdmin.h>
#include <afs_kasAdmin.h>
#include <kautils.h>
#include <afs_AdminPtsErrors.h>
#include <afs_AdminClientErrors.h>
#include <afs_AdminCommonErrors.h>
#include <prerror.h>

///// definitions in Internal.c ////////////////////

extern jclass userCls;
extern jfieldID user_nameField;
extern jfieldID user_ptsField;
extern jfieldID user_kasField;
//pts fields
extern jfieldID user_nameUidField;
extern jfieldID user_ownerUidField;
extern jfieldID user_creatorUidField;
extern jfieldID user_listStatusField;
extern jfieldID user_listGroupsOwnedField;
extern jfieldID user_listMembershipField;
extern jfieldID user_groupCreationQuotaField;
extern jfieldID user_groupMembershipCountField;
extern jfieldID user_ownerField;
extern jfieldID user_creatorField;
// kas fields
extern jfieldID user_adminSettingField;
extern jfieldID user_tgsSettingField;
extern jfieldID user_encSettingField;
extern jfieldID user_cpwSettingField;
extern jfieldID user_rpwSettingField;
extern jfieldID user_userExpirationField;
extern jfieldID user_lastModTimeField;
extern jfieldID user_lastModNameField;
extern jfieldID user_lastChangePasswordTimeField;
extern jfieldID user_maxTicketLifetimeField;
extern jfieldID user_keyVersionField;
extern jfieldID user_encryptionKeyField;
extern jfieldID user_keyCheckSumField;
extern jfieldID user_daysToPasswordExpireField;
extern jfieldID user_failLoginCountField;
extern jfieldID user_lockTimeField;
extern jfieldID user_lockedUntilField;

extern jclass groupCls;
//extern jfieldID group_cellHandleField;
extern jfieldID group_nameField;
extern jfieldID group_cachedInfoField;

//////////////////////////////////////////////////////////////////

/**
 * Creates the kas and pts entries for a new user.  Pass in 0 for the uid 
 * if pts is to automatically assign the user id.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the user belongs
 * juserName      the name of the user to create
 * jpassword      the password for the new user
 * uid     the user id to assign to the user (0 to have one 
 *                automatically assigned)
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_User_create
  (JNIEnv *env, jclass cls, jlong cellHandle, jstring juserName, 
   jstring jpassword, jint uid )
{
  afs_status_t ast;
  const char *userName;
  const char *password;
  kas_identity_p who = (kas_identity_p) malloc( sizeof(kas_identity_t) );
  
  if( !who ) {
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }

  // convert java strings
  if( juserName != NULL ) {
    userName = (*env)->GetStringUTFChars(env, juserName, 0);
    if( !userName ) {
      free( who );
      throwAFSException( env, JAFSADMNOMEM );
      return;    
    }
  } else {
    userName = NULL;
  }
  if( jpassword != NULL ) {
    password = (*env)->GetStringUTFChars(env, jpassword, 0);
    if( !password ) {
      free( who );
      if( userName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, juserName, userName);
      }
      throwAFSException( env, JAFSADMNOMEM );
      return;    
    }
  } else {
    password = NULL;
  }

  // make sure the name is within the allowed bounds
  if( userName != NULL && strlen( userName ) > KAS_MAX_NAME_LEN ) {
    free( who );
    // release converted java strings
    if( userName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, juserName, userName);
    }
    if( password != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jpassword, password);
    }
    throwAFSException( env, ADMPTSUSERNAMETOOLONG  );
    return;
  }

  // make sure name doesn't have ":" in it
  if( userName != NULL && strchr( userName, ':' ) != (int) NULL ) {
      free(who);
      // release converted java strings
      if( userName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, juserName, userName);
      }
      if( password != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jpassword, password);
      }
      throwAFSException( env, PRBADNAM );
      return;
  }

  // make sure the id isn't negative
  if( uid < 0 ) {
      free(who);
      // release converted java strings
      if( userName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, juserName, userName);
      }
      if( password != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jpassword, password);
      }
      // use the "bad arg" error code even though it's an ID exception.  
      // There isn't a bad user ID error code
      throwAFSException( env, PRBADARG );
      return;
  }

  if( userName != NULL ) {
    internal_makeKasIdentity( userName, who );
  }

  // create the kas entry
  if (!kas_PrincipalCreate( (void *) cellHandle, NULL, who, 
			    password, &ast ) && ast != ADMCLIENTCELLKASINVALID ) {
    free(who);
    // release converted java strings
    if( userName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, juserName, userName);
    }
    if( password != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jpassword, password);
    }
    throwAFSException( env, ast );
    return;
  } 

  // create the pts entry - if there's an error, make sure to delete 
  // the kas entry 
  if( !pts_UserCreate( (void *) cellHandle, userName, (int *) &uid, &ast ) ) {
    afs_status_t ast_kd;
    kas_PrincipalDelete( (void *) cellHandle, NULL, who, &ast_kd );
    free( who );
    // release converted java strings
    if( userName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, juserName, userName);
    }
    if( password != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jpassword, password);
    }
    throwAFSException( env, ast );
    return;
  }

  free( who );
  // release converted java strings
  if( userName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, juserName, userName);
  }
  if( password != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jpassword, password);
  }

}

/**
 * Deletes the pts and kas entry for a user.  Deletes this user from the 
 * membership list of the groups to which it belonged, but does not delete 
 * the groups owned by this user.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the user belongs
 * juserName      the name of the user to delete
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_User_delete
  (JNIEnv *env, jclass cls, jlong cellHandle, jstring juserName )
{
  afs_status_t ast;
  const char *userName;
  kas_identity_p who = (kas_identity_p) malloc( sizeof(kas_identity_t) );  
  int kas;
  
  if( !who ) {
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }

  if( juserName != NULL ) {
      userName = (*env)->GetStringUTFChars(env, juserName, 0);
      if( !userName ) {
	free( who );
	throwAFSException( env, JAFSADMNOMEM );
	return;    
      }
  } else {
      userName = NULL;
  }

  // make sure the name is within the allowed bounds
  if( userName != NULL && strlen( userName ) > KAS_MAX_NAME_LEN ) {
    free( who );
    if( userName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, juserName, userName);
    }
    throwAFSException( env, ADMPTSUSERNAMETOOLONG  );
    return;
  }

  if( userName != NULL ) {
      internal_makeKasIdentity( userName, who );
  }

  // delete the kas entry
  if( !kas_PrincipalDelete( (void *) cellHandle, NULL, who, &ast ) ) {
      if( ast != KANOENT  && ast != ADMCLIENTCELLKASINVALID) {
	  free(who);
	  if( userName != NULL ) {
	      (*env)->ReleaseStringUTFChars(env, juserName, userName);
	  }
	  throwAFSException( env, ast );
	  return;
      } else {
	  kas = FALSE;
      }
  }

  //delete the pts entry
  if( !pts_UserDelete( (void *) cellHandle, userName, &ast ) ) {
      // throw exception if there was no such pts user only if there was 
      // also no such kas user
      if( (ast == ADMPTSFAILEDNAMETRANSLATE && !kas ) || 
	  ast != ADMPTSFAILEDNAMETRANSLATE ) {
	  free( who );
	  if( userName != NULL ) {
	      (*env)->ReleaseStringUTFChars(env, juserName, userName);
	  }
	  throwAFSException( env, ast );
	  return;
      }
  }

  free( who );
  // release converted java strings
  if( userName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, juserName, userName);
  }
}

/**
 * Unlocks a user.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the user belongs
 * juserName      the name of the user to unlock
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_User_unlock
  (JNIEnv *env, jclass cls, jlong cellHandle, jstring juserName )
{
  afs_status_t ast;
  const char *userName;
  kas_identity_p who = (kas_identity_p) malloc( sizeof(kas_identity_t) );  
  
  if( !who ) {
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }

  // convert java strings
  if( juserName != NULL ) {
      userName = (*env)->GetStringUTFChars(env, juserName, 0);
    if( !userName ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
      userName = NULL;
  }

  // make sure the name is within the allowed bounds
  if( userName != NULL && strlen( userName ) > KAS_MAX_NAME_LEN ) {
    free( who );
    if( userName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, juserName, userName);
    }
    throwAFSException( env, ADMPTSUSERNAMETOOLONG  );
    return;
  }  

  if( userName != NULL ) {
    internal_makeKasIdentity( userName, who );
  }

  if( !kas_PrincipalUnlock( (void *) cellHandle, NULL, who, &ast ) ) {
    free( who );
    if( userName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, juserName, userName);
    }
    throwAFSException( env, ast );
    return;
  }

  free( who );
  // release converted java strings
  if( userName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, juserName, userName);
  }
}

/**
 * Retrieve the information for the specified user and populate the
 * given object
 *
 * env      the Java environment
 * cellHandle    the handle of the cell to which the user belongs
 * name      the name of the user for which to get the info
 * user      the User object to populate with the info
 */
void getUserInfoChar
  (JNIEnv *env, void *cellHandle, const char *name, jobject user)
{
  jstring jowner;
  jstring jcreator;
  jstring jlastModName;
  jstring jencryptionKey;
  jboolean pts;
  jboolean kas;
  pts_UserEntry_t ptsEntry;
  afs_status_t ast;
  kas_identity_p who = (kas_identity_p) malloc( sizeof(kas_identity_t) );
  kas_principalEntry_t kasEntry;
  unsigned int lockedUntil;

  if( !who ) {
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }

  // make sure the name is within the allowed bounds
  if( name != NULL && strlen( name ) > KAS_MAX_NAME_LEN ) {
    free( who );
    throwAFSException( env, ADMPTSUSERNAMETOOLONG );
    return;
  }
  
  if( name != NULL ) {
      internal_makeKasIdentity( name, who );
  }

  // get all the field ids, if you haven't done so already
  if( userCls == 0 ) {
    internal_getUserClass( env, user );
  }

  // get the pts entry
  if ( !pts_UserGet( cellHandle, name, &ptsEntry, &ast ) ) {
    // if the user has no pts ptsEntry
    if( ast == ADMPTSFAILEDNAMETRANSLATE ) {
	pts = FALSE;	
    } else {
	free( who );
	throwAFSException( env, ast );
	return;
    }
  } else {
      pts = TRUE;
  }
  

  // get the kas entry
  if( !kas_PrincipalGet( cellHandle, NULL, who, &kasEntry, &ast ) ) {
    // no kas entry
    if( ast == KANOENT || ast == ADMCLIENTCELLKASINVALID ) { 
	if( !pts ) {
	    free( who );
	    throwAFSException( env, ast );
	    return;
	} else {
	    kas = FALSE;
	}
    // other
    } else {
	free( who );
	throwAFSException( env, ast );
	return;
    }
  } else {
      kas = TRUE;
  }

  // get the lock status
  if( kas && !kas_PrincipalLockStatusGet( cellHandle, NULL, who, 
					  &lockedUntil, &ast ) ) {
    free( who );
    throwAFSException( env, ast );
    return;
  }

  (*env)->SetBooleanField(env, user, user_ptsField, pts);
  (*env)->SetBooleanField(env, user, user_kasField, kas);

  // set the pts fields
  if( pts ) {
      (*env)->SetIntField(env, user, user_nameUidField, ptsEntry.nameUid);
      (*env)->SetIntField(env, user, user_ownerUidField, ptsEntry.ownerUid);
      (*env)->SetIntField(env, user, user_creatorUidField, 
			  ptsEntry.creatorUid);
      (*env)->SetIntField(env, user, user_groupCreationQuotaField, 
			  ptsEntry.groupCreationQuota);
      (*env)->SetIntField(env, user, user_groupMembershipCountField, 
			  ptsEntry.groupMembershipCount);
      
      if( ptsEntry.listStatus == PTS_USER_OWNER_ACCESS ) {
	  (*env)->SetIntField(env, user, user_listStatusField, 
			      org_openafs_jafs_User_USER_OWNER_ACCESS);
      } else {
	  (*env)->SetIntField(env, user, user_listStatusField, 
			      org_openafs_jafs_User_USER_ANYUSER_ACCESS);
      }
      if( ptsEntry.listGroupsOwned == PTS_USER_OWNER_ACCESS ) {
	  (*env)->SetIntField(env, user, user_listGroupsOwnedField, 
			      org_openafs_jafs_User_USER_OWNER_ACCESS);
      } else {
	  (*env)->SetIntField(env, user, user_listGroupsOwnedField, 
			      org_openafs_jafs_User_USER_ANYUSER_ACCESS);
      }
      if( ptsEntry.listMembership == PTS_USER_OWNER_ACCESS ) {
	  (*env)->SetIntField(env, user, user_listMembershipField, 
			      org_openafs_jafs_User_USER_OWNER_ACCESS);
      } else {
	  (*env)->SetIntField(env, user, user_listMembershipField, 
			      org_openafs_jafs_User_USER_ANYUSER_ACCESS);
      }
      
      jowner = (*env)->NewStringUTF(env, ptsEntry.owner);
      jcreator =  (*env)->NewStringUTF(env, ptsEntry.creator);
      
      (*env)->SetObjectField(env, user, user_ownerField, jowner);
      (*env)->SetObjectField(env, user, user_creatorField, jcreator);

  }

  // set the kas fields
  if( kas ) {
      char *convertedKey;
      int i;
      if( kasEntry.adminSetting == KAS_ADMIN ) {
	  (*env)->SetIntField(env, user, user_adminSettingField, 
			      org_openafs_jafs_User_ADMIN);
      } else {
	  (*env)->SetIntField(env, user, user_adminSettingField, 
			      org_openafs_jafs_User_NO_ADMIN);
      }
      if( kasEntry.tgsSetting == TGS ) {
	  (*env)->SetIntField(env, user, user_tgsSettingField, 
			      org_openafs_jafs_User_GRANT_TICKETS);
      } else {
	  (*env)->SetIntField(env, user, user_tgsSettingField, 
			      org_openafs_jafs_User_NO_GRANT_TICKETS);
      }
      if( kasEntry.encSetting != NO_ENCRYPT ) {
	  (*env)->SetIntField(env, user, user_encSettingField, 
			      org_openafs_jafs_User_ENCRYPT);
      } else {
	  (*env)->SetIntField(env, user, user_encSettingField, 
			      org_openafs_jafs_User_NO_ENCRYPT);
      }
      if( kasEntry.cpwSetting == CHANGE_PASSWORD ) {
	  (*env)->SetIntField(env, user, user_cpwSettingField, 
			      org_openafs_jafs_User_CHANGE_PASSWORD);
      } else {
	  (*env)->SetIntField(env, user, user_cpwSettingField, 
			      org_openafs_jafs_User_NO_CHANGE_PASSWORD);
      }
      if( kasEntry.rpwSetting == REUSE_PASSWORD ) {
	  (*env)->SetIntField(env, user, user_rpwSettingField, 
			      org_openafs_jafs_User_REUSE_PASSWORD);
      } else {
	  (*env)->SetIntField(env, user, user_rpwSettingField, 
			      org_openafs_jafs_User_NO_REUSE_PASSWORD);
      }
      (*env)->SetIntField(env, user, user_userExpirationField, 
			  kasEntry.userExpiration);
      (*env)->SetIntField(env, user, user_lastModTimeField, 
			  kasEntry.lastModTime);
      (*env)->SetIntField(env, user, user_lastChangePasswordTimeField, 
			  kasEntry.lastChangePasswordTime);
      (*env)->SetIntField(env, user, user_maxTicketLifetimeField, 
			  kasEntry.maxTicketLifetime);
      (*env)->SetIntField(env, user, user_keyVersionField, 
			  kasEntry.keyVersion);
      (*env)->SetLongField(env, user, user_keyCheckSumField, 
			   (unsigned int) kasEntry.keyCheckSum);
      (*env)->SetIntField(env, user, user_daysToPasswordExpireField, 
			  kasEntry.daysToPasswordExpire);
      (*env)->SetIntField(env, user, user_failLoginCountField, 
			  kasEntry.failLoginCount);
      (*env)->SetIntField(env, user, user_lockTimeField, kasEntry.lockTime);
      (*env)->SetIntField(env, user, user_lockedUntilField, lockedUntil);
      
      jlastModName = (*env)->NewStringUTF(env, 
					  kasEntry.lastModPrincipal.principal);
      (*env)->SetObjectField(env, user, user_lastModNameField, jlastModName);

      convertedKey = (char *) malloc( sizeof(char *)*
				      (sizeof(kasEntry.key.key)*4+1) );
      if( !convertedKey ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
      }
      for( i = 0; i < sizeof(kasEntry.key.key); i++ ) {
	sprintf( &(convertedKey[i*4]), "\\%0.3o", kasEntry.key.key[i] );
      }
      jencryptionKey =  (*env)->NewStringUTF(env, convertedKey);
      (*env)->SetObjectField(env, user, user_encryptionKeyField, 
			     jencryptionKey);
      free( convertedKey );
  }
  free(who);
}

/**
 * Fills in the information fields of the provided User.  
 * Fills in values based on the current pts and kas information of the user.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the user belongs
 * jname     the name of the user for which to get the information
 * user     the User object in which to fill in the 
 *                 information
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_User_getUserInfo
  (JNIEnv *env, jclass cls, jlong cellHandle, jstring jname, jobject user)
{
  const char *name;

  if( jname != NULL ) {
      name = (*env)->GetStringUTFChars(env, jname, 0);
    if( !name ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
      name = NULL;
  }

  getUserInfoChar( env, (void *) cellHandle, name, user );
 
  // get class fields if need be
  if( userCls == 0 ) {
    internal_getUserClass( env, user );
  }
  
  // set name in case blank object
  (*env)->SetObjectField(env, user, user_nameField, jname);
  
  if( name != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jname, name);
  }      
}

/**
 * Sets the information values of this AFS user to be the parameter values.  
 * Sets both kas and pts fields.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the user belongs
 * jname     the name of the user for which to set the information
 * user  the User object containing the desired 
 *                 information
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_User_setUserInfo
  (JNIEnv *env, jclass cls, jlong cellHandle, jstring jname, jobject user )
{
  const char *name;
  kas_identity_p who = (kas_identity_p) malloc( sizeof(kas_identity_t) );
  pts_UserUpdateEntry_t ptsEntry;
  afs_status_t ast;
  kas_admin_t isAdmin;
  kas_tgs_t grantTickets;
  kas_enc_t canEncrypt;
  kas_cpw_t canChangePassword;
  kas_rpw_t passwordReuse;
  unsigned int expirationDate;
  unsigned int maxTicketLifetime;
  unsigned int passwordExpires;
  unsigned int failedPasswordAttempts;
  unsigned int failedPasswordLockTime;
  int kas;
  int pts;

  if( !who ) {
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }

  if( jname != NULL ) {
      name = (*env)->GetStringUTFChars(env, jname, 0);
    if( !name ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
      name = NULL;
  }

  // make sure the name is within the allowed bounds
  if( name != NULL && strlen( name ) > KAS_MAX_NAME_LEN ) {
    free( who );
    (*env)->ReleaseStringUTFChars(env, jname, name);
    throwAFSException( env, ADMPTSUSERNAMETOOLONG );
    return;
  }

  if( name != NULL ) {
      internal_makeKasIdentity( name, who );
  }

  // get class fields if need be
  if( userCls == 0 ) {
    internal_getUserClass( env, user );
  }

  kas = (*env)->GetBooleanField(env, user, user_kasField);
  pts = (*env)->GetBooleanField(env, user, user_ptsField);

  if( pts ) {
      // set the pts fields: 
      ptsEntry.flag = PTS_USER_UPDATE_GROUP_CREATE_QUOTA | 
	PTS_USER_UPDATE_PERMISSIONS;
      ptsEntry.groupCreationQuota = 
	(*env)->GetIntField(env, user, user_groupCreationQuotaField);
      if( (*env)->GetIntField(env, user, user_listStatusField) == 
	  org_openafs_jafs_User_USER_OWNER_ACCESS ) {
	  ptsEntry.listStatus = PTS_USER_OWNER_ACCESS;
      } else {
	  ptsEntry.listStatus = PTS_USER_ANYUSER_ACCESS;
      }
      if( (*env)->GetIntField(env, user, user_listGroupsOwnedField) == 
	  org_openafs_jafs_User_USER_OWNER_ACCESS ) {
	  ptsEntry.listGroupsOwned = PTS_USER_OWNER_ACCESS;
      } else {
	  ptsEntry.listGroupsOwned = PTS_USER_ANYUSER_ACCESS;
      }
      if( (*env)->GetIntField(env, user, user_listMembershipField) == 
	  org_openafs_jafs_User_USER_OWNER_ACCESS ) {
	  ptsEntry.listMembership = PTS_USER_OWNER_ACCESS;
      } else {
	  ptsEntry.listMembership = PTS_USER_ANYUSER_ACCESS;
      }
      if( !pts_UserModify( (void *) cellHandle, name, &ptsEntry, &ast ) ) {
	  free( who );
	  if( name != NULL ) {
	      (*env)->ReleaseStringUTFChars(env, jname, name);
	  }
	  throwAFSException( env, ast );
	  return;    
      }
  }

  if( kas ) {
      // set the kas fields:
      if( (*env)->GetIntField(env, user, user_adminSettingField) == 
	  org_openafs_jafs_User_ADMIN ) {
	  isAdmin = KAS_ADMIN;
      } else {
	  isAdmin = NO_KAS_ADMIN;
      }
      if( (*env)->GetIntField(env, user, user_tgsSettingField) == 
	  org_openafs_jafs_User_GRANT_TICKETS ) {
	  grantTickets = TGS;
      } else {
	  grantTickets = NO_TGS;
      }
      if( (*env)->GetIntField(env, user, user_encSettingField) == 
	  org_openafs_jafs_User_ENCRYPT ) {
	  canEncrypt = 0;
      } else {
	  canEncrypt = NO_ENCRYPT;
      }
      if( (*env)->GetIntField(env, user, user_cpwSettingField) == 
	  org_openafs_jafs_User_CHANGE_PASSWORD ) {
	  canChangePassword = CHANGE_PASSWORD;
      } else {
	  canChangePassword = NO_CHANGE_PASSWORD;
      }
      if( (*env)->GetIntField(env, user, user_rpwSettingField) == 
	  org_openafs_jafs_User_REUSE_PASSWORD ) {
	  passwordReuse = REUSE_PASSWORD;
      } else {
	  passwordReuse = NO_REUSE_PASSWORD;
      }
      expirationDate = (*env)->GetIntField(env, user, 
					   user_userExpirationField);
      maxTicketLifetime = (*env)->GetIntField(env, user, 
					      user_maxTicketLifetimeField);
      passwordExpires = (*env)->GetIntField(env, user, 
					    user_daysToPasswordExpireField);
      failedPasswordAttempts = (*env)->GetIntField(env, user, 
						   user_failLoginCountField);
      failedPasswordLockTime =  (*env)->GetIntField(env, user, 
						    user_lockTimeField);
      
      if( !kas_PrincipalFieldsSet( (void *) cellHandle, NULL, who, &isAdmin, 
				   &grantTickets, &canEncrypt, 
				   &canChangePassword, &expirationDate, 
				   &maxTicketLifetime, &passwordExpires, 
				   &passwordReuse, &failedPasswordAttempts, 
				   &failedPasswordLockTime, &ast ) ) {
	  free( who );
	  if( name != NULL ) {
	      (*env)->ReleaseStringUTFChars(env, jname, name);
	  }
	  throwAFSException( env, ast );
	  return;    
      }
  }      

  free( who );
  if( name != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jname, name);
  }
}

/**
 * Renames the given user.  Does not update the info fields of the kas entry
 *  -- the calling code is responsible for that.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the user belongs
 * joldName     the name of the user to rename
 * jnewName     the new name for the user
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_User_rename
  (JNIEnv *env, jclass cls, jlong cellHandle, jstring joldName, jstring jnewName)
{

    const char *oldName;
    const char *newName;
    kas_identity_p whoOld = (kas_identity_p) malloc( sizeof(kas_identity_t) );
    kas_identity_p whoNew = (kas_identity_p) malloc( sizeof(kas_identity_t) );
    kas_principalEntry_t kasEntry;
    pts_UserEntry_t ptsEntry;
    afs_status_t ast;
    int kas;

    if( !whoOld || !whoNew ) {
      if( whoOld ) {
	free( whoOld );
      }
      if( whoNew ) {
	free( whoNew );
      } 
      throwAFSException( env, JAFSADMNOMEM );
      return;    
    }

    if( joldName != NULL ) {
	oldName = (*env)->GetStringUTFChars(env, joldName, 0);
	if( !oldName ) {
	    throwAFSException( env, JAFSADMNOMEM );
	    return;    
	}
    } else {
	oldName = NULL;
    }
    if( jnewName != NULL ) {
	newName = (*env)->GetStringUTFChars(env, jnewName, 0);
	if( !newName ) {
	  if( oldName != NULL ) {
	    (*env)->ReleaseStringUTFChars(env, joldName, oldName);
	  }
	  throwAFSException( env, JAFSADMNOMEM );
	  return;    
	}
    } else {
	newName = NULL;
    }

    // make sure the names are within the allowed bounds
    if( oldName != NULL && strlen( oldName ) > KAS_MAX_NAME_LEN ) {
	free( whoOld );
	free( whoNew );
	if( oldName != NULL ) {
	    (*env)->ReleaseStringUTFChars(env, joldName, oldName);
	}
	if( newName != NULL ) {
	    (*env)->ReleaseStringUTFChars(env, jnewName, newName);
	}
	throwAFSException( env, ADMPTSUSERNAMETOOLONG );
	return;
    }
    if( newName != NULL && strlen( newName ) > KAS_MAX_NAME_LEN ) {
	free( whoOld );
	free( whoNew );
	if( oldName != NULL ) {
	    (*env)->ReleaseStringUTFChars(env, joldName, oldName);
	}
	if( newName != NULL ) {
	    (*env)->ReleaseStringUTFChars(env, jnewName, newName);
	}
	throwAFSException( env, ADMPTSUSERNAMETOOLONG );
	return;
    }
    
    if( oldName != NULL ) {
	internal_makeKasIdentity( oldName, whoOld );
    }
    if( newName != NULL ) {
	internal_makeKasIdentity( newName, whoNew );
    }

    // retrieve the old kas info
    if( !kas_PrincipalGet( (void *) cellHandle, NULL, whoOld, 
			   &kasEntry, &ast ) ) {
	if( ast != KANOENT  && ast != ADMCLIENTCELLKASINVALID ) {
	    free( whoOld );
	    free( whoNew );
	    if( oldName != NULL ) {
		(*env)->ReleaseStringUTFChars(env, joldName, oldName);
	    }
	    if( newName != NULL ) {
		(*env)->ReleaseStringUTFChars(env, jnewName, newName);
	    }
	    throwAFSException( env, ast );
	    return;
	} else {
	    kas = FALSE;
	}
    } else {
	kas = TRUE;
    }   
	
    if( kas ) {
	// create a new kas entry
	// temporarily set the password equal to the new name
	if (!kas_PrincipalCreate( (void *) cellHandle, NULL, whoNew, 
				  newName, &ast ) ) {	    
	    free( whoOld );
	    free( whoNew );
	    if( oldName != NULL ) {
		(*env)->ReleaseStringUTFChars(env, joldName, oldName);
	    }
	    if( newName != NULL ) {
		(*env)->ReleaseStringUTFChars(env, jnewName, newName);
	    }
	    throwAFSException( env, ast );
	    return;
	} 

	// set the password 
	ast = 0;
	// For some reason kas_PrincipalKeySet doesn't set the return code 
	// correctly.  It always returns 0.
	// So instead of checking the return code, we see if there's an 
	// error in the status variable.
	kas_PrincipalKeySet( (void *) cellHandle, NULL, whoNew, 0, 
			     &(kasEntry.key), &ast );
	if( ast ) {
	    afs_status_t ast_kd;
	    kas_PrincipalDelete( (void *) cellHandle, NULL, whoNew, &ast_kd );
	    free( whoOld );
	    free( whoNew );
	    if( oldName != NULL ) {
		(*env)->ReleaseStringUTFChars(env, joldName, oldName);
	    }
	    if( newName != NULL ) {
		(*env)->ReleaseStringUTFChars(env, jnewName, newName);
	    }
	    throwAFSException( env, ast );
	    return;
	}
    }

    // rename the pts entry
    if( !pts_UserRename( (void *) cellHandle, oldName, newName, &ast ) ) {
	// throw exception if there was no such pts user only if 
        // there was also no such kas user
	if( (ast == ADMPTSFAILEDNAMETRANSLATE && !kas ) || 
	    ast != ADMPTSFAILEDNAMETRANSLATE ) {
	    afs_status_t ast_kd;
	    if( kas ) {
		kas_PrincipalDelete( (void *) cellHandle, NULL, whoNew, 
				     &ast_kd );
	    }
	    free( whoOld );
	    free( whoNew );
	    if( oldName != NULL ) {
		(*env)->ReleaseStringUTFChars(env, joldName, oldName);
	    }
	    if( newName != NULL ) {
		(*env)->ReleaseStringUTFChars(env, jnewName, newName);
	    }
	    throwAFSException( env, ast );
	    return;
	}
    }

    if( kas ) {
	// delete the old kas entry
	if( !kas_PrincipalDelete( (void *) cellHandle, NULL, whoOld, &ast ) ) {
	    free( whoOld );
	    free( whoNew );
	    if( oldName != NULL ) {
		(*env)->ReleaseStringUTFChars(env, joldName, oldName);
	    }
	    if( newName != NULL ) {
		(*env)->ReleaseStringUTFChars(env, jnewName, newName);
	    }
	    throwAFSException( env, ast );
	    return;
	}
    }    

    free( whoOld );
    free( whoNew );
    if( oldName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, joldName, oldName);
    }
    if( newName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jnewName, newName);
    }
}

/**
 * Sets the password of the given user.  Sets the key version to 0.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the user belongs
 * juserName     the name of the user for which to set the password
 * jnewPassword     the new password for the user
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_User_setPassword
  (JNIEnv *env, jclass cls, jlong cellHandle, jstring juserName,
   jstring jnewPassword)
{
  afs_status_t ast;
  char *cellName;
  const char *userName;
  const char *newPassword;
  kas_encryptionKey_p newKey = 
    (kas_encryptionKey_p) malloc( sizeof(kas_encryptionKey_t) );
  kas_identity_p who = (kas_identity_p) malloc( sizeof(kas_identity_t) );

  if( !who || !newKey ) {
    if( who ) {
      free( who );
    }
    if( newKey ) {
      free( newKey );
    }
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }

  if( juserName != NULL ) {
      userName = (*env)->GetStringUTFChars(env, juserName, 0);
      if( !userName ) {
	  throwAFSException( env, JAFSADMNOMEM );
	  return;    
      }
  } else {
      userName = NULL;
  }
  if( jnewPassword != NULL ) {
      newPassword = (*env)->GetStringUTFChars(env, jnewPassword, 0);
      if( !newPassword ) {
	  throwAFSException( env, JAFSADMNOMEM );
	  return;    
      }
  } else {
      newPassword = NULL;
  }

  // make sure the name is within the allowed bounds
  if( userName != NULL && strlen( userName ) > KAS_MAX_NAME_LEN ) {
    free(who);
    free( newKey );
    if( userName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, juserName, userName);
    }
    if( newPassword != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jnewPassword, newPassword);
    }
    throwAFSException( env, ADMPTSUSERNAMETOOLONG );
    return;
  }

  if( !afsclient_CellNameGet( (void *) cellHandle, &cellName, &ast ) ) {
      free(who);
      free( newKey );
      if( userName != NULL ) {
	  (*env)->ReleaseStringUTFChars(env, juserName, userName);
      }
      if( newPassword != NULL ) {
	  (*env)->ReleaseStringUTFChars(env, jnewPassword, newPassword);
      }
      throwAFSException( env, ast );
      return;
  }
  
  if( !kas_StringToKey( cellName, newPassword, newKey, &ast ) ) {
      free(who);
      free( newKey );
      if( userName != NULL ) {
	  (*env)->ReleaseStringUTFChars(env, juserName, userName);
      }
      if( newPassword != NULL ) {
	  (*env)->ReleaseStringUTFChars(env, jnewPassword, newPassword);
      }
      throwAFSException( env, ast );
      return;
  }

  if( userName != NULL ) {
      internal_makeKasIdentity( userName, who );
  }

  ast = 0;
  // For some reason kas_PrincipalKeySet doesn't set the return code correctly.
  //  It always returns 0.
  // So instead of checking the return code, we see if there's an error 
  // in the status variable.
  kas_PrincipalKeySet( (void *) cellHandle, NULL, who, 0, newKey, &ast );
  if( ast ) {
    free( who );
    free( newKey );
    if( userName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, juserName, userName);
    }
    if( newPassword != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jnewPassword, newPassword);
    }
    throwAFSException( env, ast );
    return;
  }

  free( who );
  free( newKey );
  if( userName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, juserName, userName);
  }
  if( newPassword != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jnewPassword, newPassword);
  }

}

/**
 * Begin the process of getting the groups to which the user belongs.  
 * Returns an iteration ID to be used by subsequent calls to 
 * getUserGroupsNext and getUserGroupsDone.  
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the user belongs
 * jname          the name of the user for which to get the groups
 * returns an iteration ID
 */
JNIEXPORT jlong JNICALL 
Java_org_openafs_jafs_User_getUserGroupsBegin
  (JNIEnv *env, jclass cls, jlong cellHandle, jstring jname)
{
  const char *name;
  afs_status_t ast;
  void *iterationId;

  if( jname != NULL ) {
      name = (*env)->GetStringUTFChars(env, jname, 0);
      if( !name ) {
	  throwAFSException( env, JAFSADMNOMEM );
	  return;    
      }
  } else {
      name = NULL;
  }

  if( !pts_UserMemberListBegin( (void *) cellHandle, name, &iterationId, 
				&ast ) ) {
      if( name != NULL ) {
	  (*env)->ReleaseStringUTFChars(env, jname, name);
      }
      throwAFSException( env, ast );
      return;
  }

  if( name != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jname, name);
  }

  return (jlong) iterationId;

}

/**
 * Returns the next group to which the user belongs.  Returns 
 * null if there are no more groups.
 *
 * env      the Java environment
 * cls      the current Java class
 * iterationId   the iteration ID of this iteration
 * returns the name of the next group
 */
JNIEXPORT jstring JNICALL 
Java_org_openafs_jafs_User_getUserGroupsNextString
  (JNIEnv *env, jclass cls, jlong iterationId)
{
  afs_status_t ast;
  char *groupName = (char *) malloc( sizeof(char)*PTS_MAX_NAME_LEN);
  jstring jgroup;

  if( !groupName ) {
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }

  if( !pts_UserMemberListNext( (void *) iterationId, groupName, &ast ) ) {
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
 * Fills the next group object of which the user belongs.  Returns 0 if there
 * are no more groups, != 0 otherwise.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the users belong
 * iterationId   the iteration ID of this iteration
 * jgroupObject   a Group object to be populated with the values of the 
 *                   next group
 * returns 0 if there are no more users, != 0 otherwise
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_User_getUserGroupsNext
  (JNIEnv *env, jclass cls, jlong cellHandle, jlong iterationId,
   jobject jgroupObject)
{
  afs_status_t ast;
  char *groupName;
  jstring jgroup;

  groupName = (char *) malloc( sizeof(char)*PTS_MAX_NAME_LEN);

  if( !groupName ) {
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }

  if( !pts_UserMemberListNext( (void *) iterationId, groupName, &ast ) ) {
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

  getGroupInfoChar( env, (void *) cellHandle, groupName, jgroupObject );
  (*env)->SetBooleanField( env, jgroupObject, group_cachedInfoField, TRUE );

  free( groupName );
  return 1;

}

/**
 * Signals that the iteration is complete and will not be accessed anymore.
 *
 * env      the Java environment
 * cls      the current Java class
 * iterationId   the iteration ID of this iteration
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_User_getUserGroupsDone
  (JNIEnv *env, jclass cls, jlong iterationId)
{
  afs_status_t ast;

  if( !pts_UserMemberListDone( (void *) iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }
}

/**
 * Returns the total number of groups owned by the user.  
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the user belongs
 * jname          the name of the user for which to get the groups
 * returns total number of groups owned by the user
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_User_getGroupsOwnedCount
  (JNIEnv *env, jclass cls, jlong cellHandle, jstring jname)
{
  afs_status_t ast;
  void *iterationId;
  char *groupName;
  int i = 0;

  iterationId = 
    (void *) Java_org_openafs_jafs_User_getGroupsOwnedBegin( env, cls, 
								cellHandle, 
								jname );

  groupName = (char *) malloc( sizeof(char)*PTS_MAX_NAME_LEN);

  if( !groupName ) {
    throwAFSException( env, JAFSADMNOMEM );
    return -1;    
  }

  while ( pts_OwnedGroupListNext( (void *) iterationId, groupName, &ast ) ) 
    i++;

  free( groupName );  

  if( ast != ADMITERATORDONE ) {
    throwAFSException( env, ast );
    return -1;
  }

  return i;
}

/**
 * Begin the process of getting the groups that a user or group owns.  
 * Returns an iteration ID to be used by subsequent calls to 
 * getGroupsOwnedNext and getGroupsOwnedDone.  
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the user belongs
 * jname  the name of the user or group for which to get the groups
 * returns an iteration ID
 */
JNIEXPORT jlong JNICALL 
Java_org_openafs_jafs_User_getGroupsOwnedBegin
  (JNIEnv *env, jclass cls, jlong cellHandle, jstring jname)
{
  const char *name;
  afs_status_t ast;
  void *iterationId;

  if( jname != NULL ) {
      name = (*env)->GetStringUTFChars(env, jname, 0);
      if( !name ) {
	  throwAFSException( env, JAFSADMNOMEM );
	  return;    
      }
  } else {
      name = NULL;
  }

  if( !pts_OwnedGroupListBegin( (void *) cellHandle, name, 
				&iterationId, &ast ) ) {
      if( jname != NULL ) {
	  (*env)->ReleaseStringUTFChars(env, jname, name);
      }
      throwAFSException( env, ast );
      return;
  }

  if( jname != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jname, name);
  }

  return (jlong) iterationId;

}

/**
 * Returns the next group the user or group owns.  Returns null 
 * if there are no more groups.
 *
 * env      the Java environment
 * cls      the current Java class
 * iterationId   the iteration ID of this iteration
 * returns the name of the next group
 */
JNIEXPORT jstring JNICALL 
Java_org_openafs_jafs_User_getGroupsOwnedNextString
  (JNIEnv *env, jclass cls, jlong iterationId)
{
  afs_status_t ast;
  char *groupName = (char *) malloc( sizeof(char)*PTS_MAX_NAME_LEN);
  jstring jgroup;

  if( !groupName ) {
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }

  if( !pts_OwnedGroupListNext( (void *) iterationId, groupName, &ast ) ) {
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
 * Fills the next group object that the user or group owns.  Returns 0 if 
 * there are no more groups, != 0 otherwise.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the users belong
 * iterationId   the iteration ID of this iteration
 * jgroupObject   a Group object to be populated with the values of the 
 *                   next group
 * returns 0 if there are no more users, != 0 otherwise
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_User_getGroupsOwnedNext
  (JNIEnv *env, jclass cls, jlong cellHandle, jlong iterationId, 
   jobject jgroupObject)
{
  afs_status_t ast;
  char *groupName;
  jstring jgroup;

  groupName = (char *) malloc( sizeof(char)*PTS_MAX_NAME_LEN);

  if( !groupName ) {
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }

  if( !pts_OwnedGroupListNext( (void *) iterationId, groupName, &ast ) ) {
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

  getGroupInfoChar( env, (void *) cellHandle, groupName, jgroupObject );
  (*env)->SetBooleanField( env, jgroupObject, group_cachedInfoField, TRUE );

  free( groupName );
  return 1;

}

/**
 * Signals that the iteration is complete and will not be accessed anymore.
 *
 * env      the Java environment
 * cls      the current Java class
 * iterationId   the iteration ID of this iteration
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_User_getGroupsOwnedDone
  (JNIEnv *env, jclass cls, jlong iterationId)
{
  afs_status_t ast;

  if( !pts_OwnedGroupListDone( (void *) iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }
  
}

// reclaim global memory being used by this portion
JNIEXPORT void JNICALL
Java_org_openafs_jafs_User_reclaimUserMemory
  (JNIEnv *env, jclass cls)
{
  if( userCls ) {
      (*env)->DeleteGlobalRef(env, userCls);
      userCls = 0;
  }

}



