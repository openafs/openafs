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
#include "org_openafs_jafs_Group.h"

#include <stdio.h>
#include <afs_ptsAdmin.h>
#include <afs_AdminPtsErrors.h>
#include <afs_AdminClientErrors.h>
#include <afs_AdminCommonErrors.h>
#include <pterror.h>

/////  definitions in Internal.c ///////////////

extern jclass userCls;
//extern jfieldID user_cellHandleField;
extern jfieldID user_nameField;
extern jfieldID user_cachedInfoField;

extern jclass groupCls;
extern jfieldID group_nameField;
extern jfieldID group_nameUidField;
extern jfieldID group_ownerUidField;
extern jfieldID group_creatorUidField;
extern jfieldID group_listStatusField;
extern jfieldID group_listGroupsOwnedField;
extern jfieldID group_listMembershipField;
extern jfieldID group_listAddField;
extern jfieldID group_listDeleteField;
extern jfieldID group_membershipCountField;
extern jfieldID group_ownerField;
extern jfieldID group_creatorField;

//////////////////////////////////////////////////////


/**
 * Creates the PTS entry for a new group.  Pass in 0 for the uid if PTS is to
 * automatically assign the group id.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the group belongs
 * jgroupName      the name of the group to create
 * jownerName      the owner of this group
 * gid     the group id to assign to the group (0 to have one 
 *                automatically assigned)
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Group_create
  (JNIEnv *env, jclass cls, jlong cellHandle, jstring jgroupName, 
   jstring jownerName, jint gid )
{
  afs_status_t ast;
  // convert java strings
  const char *groupName;
  const char *ownerName;

  if( jgroupName != NULL ) {
    groupName = (*env)->GetStringUTFChars(env, jgroupName, 0);
    if( !groupName ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    groupName = NULL;
  }

  if( jownerName != NULL ) {
    ownerName = (*env)->GetStringUTFChars(env, jownerName, 0);
    if( !ownerName ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    ownerName = NULL;
  }

  // make sure the name is within the allowed bounds
  if( groupName != NULL && strlen( groupName ) > PTS_MAX_NAME_LEN ) {
    // release converted java strings
    if( groupName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jgroupName, groupName);
    }
    if( ownerName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jownerName, ownerName);
    }
    throwAFSException( env, ADMPTSGROUPNAMETOOLONG );
    return;
  }
  
  if( !pts_GroupCreate( (void *) cellHandle, groupName, ownerName, 
			(int *) &gid, &ast ) ) {
    // release converted java strings
    if( groupName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jgroupName, groupName);
    }
    if( ownerName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jownerName, ownerName);
    }
    throwAFSException( env, ast );
    return;
  }

  // release converted java strings
  if( groupName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jgroupName, groupName);
  }
  if( ownerName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jownerName, ownerName);
  }

}

/**
 * Deletes the PTS entry for a group.  Deletes this group from the 
 * membership list of the users that belonged to it, but does not delete 
 * the groups owned by this group.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the group belongs
 * jgroupName      the name of the group to delete
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Group_delete
  (JNIEnv *env, jclass cls, jlong cellHandle, jstring jgroupName )
{
  afs_status_t ast;
  // convert java strings
  const char *groupName;

  if( jgroupName != NULL ) {
    groupName = (*env)->GetStringUTFChars(env, jgroupName, 0);
    if( !groupName ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    groupName = NULL;
  }

  if( !pts_GroupDelete( (void *) cellHandle, groupName, &ast ) ) {
    if( groupName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jgroupName, groupName);
    }
    throwAFSException( env, ast );
    return;
  }
  
  // release converted java strings
  if( groupName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jgroupName, groupName);
  }

}

/**
 * Retrieve the information for the specified group and populate the
 * given object
 *
 * env      the Java environment
 * cellHandle    the handle of the cell to which the user belongs
 * name      the name of the group for which to get the info
 * group      the Group object to populate with the info
 */
void getGroupInfoChar
  ( JNIEnv *env, void *cellHandle, const char *name, jobject group )
{

  jstring jowner;
  jstring jcreator;
  pts_GroupEntry_t entry;
  afs_status_t ast;

  // get the field ids if you haven't already
  if( groupCls == 0 ) {
    internal_getGroupClass( env, group );
  }
  if ( !pts_GroupGet( cellHandle, name, &entry, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

  // set the fields
  (*env)->SetIntField(env, group, group_nameUidField, entry.nameUid);
  (*env)->SetIntField(env, group, group_ownerUidField, entry.ownerUid);
  (*env)->SetIntField(env, group, group_creatorUidField, entry.creatorUid);
  (*env)->SetIntField(env, group, group_membershipCountField, 
		      entry.membershipCount);

  if( entry.listStatus == PTS_GROUP_OWNER_ACCESS ) {
      (*env)->SetIntField(env, group, group_listStatusField, 
			  org_openafs_jafs_Group_GROUP_OWNER_ACCESS);
  } else if( entry.listStatus == PTS_GROUP_ACCESS ) {
      (*env)->SetIntField(env, group, group_listStatusField, 
			  org_openafs_jafs_Group_GROUP_GROUP_ACCESS);
  } else {
      (*env)->SetIntField(env, group, group_listStatusField, 
			  org_openafs_jafs_Group_GROUP_ANYUSER_ACCESS);
  }

  if( entry.listGroupsOwned == PTS_GROUP_OWNER_ACCESS ) {
      (*env)->SetIntField(env, group, group_listGroupsOwnedField, 
			  org_openafs_jafs_Group_GROUP_OWNER_ACCESS);
  } else if( entry.listGroupsOwned == PTS_GROUP_ACCESS ) {
      (*env)->SetIntField(env, group, group_listGroupsOwnedField, 
			  org_openafs_jafs_Group_GROUP_GROUP_ACCESS);
  } else {
      (*env)->SetIntField(env, group, group_listGroupsOwnedField, 
			  org_openafs_jafs_Group_GROUP_ANYUSER_ACCESS);
  }

  if( entry.listMembership == PTS_GROUP_OWNER_ACCESS ) {
      (*env)->SetIntField(env, group, group_listMembershipField, 
			  org_openafs_jafs_Group_GROUP_OWNER_ACCESS);
  } else if( entry.listMembership == PTS_GROUP_ACCESS ) {
      (*env)->SetIntField(env, group, group_listMembershipField, 
			  org_openafs_jafs_Group_GROUP_GROUP_ACCESS);
  } else {
      (*env)->SetIntField(env, group, group_listMembershipField, 
			  org_openafs_jafs_Group_GROUP_ANYUSER_ACCESS);
  }

  if( entry.listAdd == PTS_GROUP_OWNER_ACCESS ) {
      (*env)->SetIntField(env, group, group_listAddField, 
			  org_openafs_jafs_Group_GROUP_OWNER_ACCESS);
  } else if( entry.listAdd == PTS_GROUP_ACCESS ) {
      (*env)->SetIntField(env, group, group_listAddField, 
			  org_openafs_jafs_Group_GROUP_GROUP_ACCESS);
  } else {
      (*env)->SetIntField(env, group, group_listAddField, 
			  org_openafs_jafs_Group_GROUP_ANYUSER_ACCESS);
  }

  if( entry.listDelete == PTS_GROUP_OWNER_ACCESS ) {
      (*env)->SetIntField(env, group, group_listDeleteField, 
			  org_openafs_jafs_Group_GROUP_OWNER_ACCESS);
  } else if( entry.listDelete == PTS_GROUP_ACCESS ) {
      (*env)->SetIntField(env, group, group_listDeleteField, 
			  org_openafs_jafs_Group_GROUP_GROUP_ACCESS);
  } else {
      (*env)->SetIntField(env, group, group_listDeleteField, 
			  org_openafs_jafs_Group_GROUP_ANYUSER_ACCESS);
  }

  jowner = (*env)->NewStringUTF(env, entry.owner);
  jcreator =  (*env)->NewStringUTF(env, entry.creator);

  (*env)->SetObjectField(env, group, group_ownerField, jowner);
  (*env)->SetObjectField(env, group, group_creatorField, jcreator);

}

/**
 * Fills in the information fields of the provided Group.  
 * Fills in values based on the current PTS information of the group.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the group belongs
 * name     the name of the group for which to get the information
 * group     the Group object in which to fill in the 
 *                  information
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Group_getGroupInfo
  (JNIEnv *env, jclass cls, jlong cellHandle, jstring jname, jobject group)
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
  getGroupInfoChar( env, (void *)cellHandle, name, group );

  // get class fields if need be
  if( groupCls == 0 ) {
    internal_getGroupClass( env, group );
  }

  // set name in case blank object
  (*env)->SetObjectField(env, group, group_nameField, jname);

  if( name != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jname, name);
  }

}

/**
 * Sets the information values of this AFS group to be the parameter values.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the user belongs
 * name     the name of the user for which to set the information
 * theGroup   the group object containing the desired information
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Group_setGroupInfo
  (JNIEnv *env, jclass cls, jlong cellHandle, jstring jname, jobject group)
{
  const char *name;
  pts_GroupUpdateEntry_t ptsEntry;
  afs_status_t ast;

  jint jlistStatus;
  jint jlistGroupsOwned;
  jint jlistMembership;
  jint jlistAdd;
  jint jlistDelete;

  // get the field ids if you haven't already
  if( groupCls == 0 ) {
    internal_getGroupClass( env, group );
  }

  jlistStatus = (*env)->GetIntField(env, group, group_listStatusField);
  jlistGroupsOwned = (*env)->GetIntField(env, group, 
					 group_listGroupsOwnedField);
  jlistMembership = (*env)->GetIntField(env, group, group_listMembershipField);
  jlistAdd = (*env)->GetIntField(env, group, group_listAddField);
  jlistDelete = (*env)->GetIntField(env, group, group_listDeleteField);

  if( jname != NULL ) {
    name = (*env)->GetStringUTFChars(env, jname, 0);
    if( !name ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    name = NULL;
  }

  if( jlistStatus == org_openafs_jafs_Group_GROUP_OWNER_ACCESS ) {
    ptsEntry.listStatus = PTS_GROUP_OWNER_ACCESS;
  } else if( jlistStatus == org_openafs_jafs_Group_GROUP_GROUP_ACCESS ) {
    ptsEntry.listStatus = PTS_GROUP_ACCESS;
  } else {
    ptsEntry.listStatus = PTS_GROUP_ANYUSER_ACCESS;
  }
  if( jlistGroupsOwned == org_openafs_jafs_Group_GROUP_OWNER_ACCESS ) {
    ptsEntry.listGroupsOwned = PTS_GROUP_OWNER_ACCESS;
  } else if( jlistGroupsOwned == 
	     org_openafs_jafs_Group_GROUP_GROUP_ACCESS ) {
    ptsEntry.listGroupsOwned = PTS_GROUP_ACCESS;
  } else {
    ptsEntry.listGroupsOwned = PTS_GROUP_ANYUSER_ACCESS;
  }
  if( jlistMembership == org_openafs_jafs_Group_GROUP_OWNER_ACCESS ) {
    ptsEntry.listMembership = PTS_GROUP_OWNER_ACCESS;
  } else if( jlistMembership == 
	     org_openafs_jafs_Group_GROUP_GROUP_ACCESS ) {
    ptsEntry.listMembership = PTS_GROUP_ACCESS;
  } else {
    ptsEntry.listMembership = PTS_GROUP_ANYUSER_ACCESS;
  }
  if( jlistAdd == org_openafs_jafs_Group_GROUP_OWNER_ACCESS ) {
    ptsEntry.listAdd = PTS_GROUP_OWNER_ACCESS;
  } else if( jlistAdd == org_openafs_jafs_Group_GROUP_GROUP_ACCESS ) {
    ptsEntry.listAdd = PTS_GROUP_ACCESS;
  } else {
    ptsEntry.listAdd = PTS_GROUP_ANYUSER_ACCESS;
  }
  if( jlistDelete == org_openafs_jafs_Group_GROUP_OWNER_ACCESS ) {
    ptsEntry.listDelete = PTS_GROUP_OWNER_ACCESS;
  } else if( jlistDelete == org_openafs_jafs_Group_GROUP_GROUP_ACCESS ) {
    ptsEntry.listDelete = PTS_GROUP_ACCESS;
  } else {
    ptsEntry.listDelete = PTS_GROUP_ANYUSER_ACCESS;
  }
  if( !pts_GroupModify( (void *) cellHandle, name, &ptsEntry, &ast ) ) {
    if( name != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jname, name);
    }
    throwAFSException( env, ast );
    return;    
  }

  if( name != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jname, name);
  }

}

/**
 * Begin the process of getting the users that belong to the group.  Returns 
 * an iteration ID to be used by subsequent calls to 
 * getGroupMembersNext and getGroupMembersDone.  
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the group belongs
 * jname          the name of the group for which to get the members
 * returns an iteration ID
 */
JNIEXPORT jlong JNICALL 
Java_org_openafs_jafs_Group_getGroupMembersBegin
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

  if( !pts_GroupMemberListBegin( (void *) cellHandle, name, &iterationId, 
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
 * Returns the next members that belongs to the group.  Returns 
 * null if there are no more members.
 *
 * env      the Java environment
 * cls      the current Java class
 * iterationId   the iteration ID of this iteration
 * returns the name of the next member
 */
JNIEXPORT jstring JNICALL 
Java_org_openafs_jafs_Group_getGroupMembersNextString
  (JNIEnv *env, jclass cls, jlong iterationId)
{
  afs_status_t ast;
  char *userName = (char *) malloc( sizeof(char)*PTS_MAX_NAME_LEN);
  jstring juser;

  if( !userName ) {
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }

  if( !pts_GroupMemberListNext( (void *) iterationId, userName, &ast ) ) {
    free( userName );
    if( ast == ADMITERATORDONE ) {
      return NULL;
    } else {
      throwAFSException( env, ast );
      return;
    }
  }
  
  juser = (*env)->NewStringUTF(env, userName);
  free( userName );
  return juser;
}

/**
 * Fills the next user object belonging to that group.  Returns 0 if there
 * are no more users, != 0 otherwise.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the users belong
 * iterationId   the iteration ID of this iteration
 * juserObject   a User object to be populated with the values of the 
 *                  next user
 * returns 0 if there are no more users, != 0 otherwise
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Group_getGroupMembersNext
  (JNIEnv *env, jclass cls, jlong cellHandle, jlong iterationId,
   jobject juserObject)
{
  afs_status_t ast;
  char *userName;
  jstring juser;

  userName = (char *) malloc( sizeof(char)*PTS_MAX_NAME_LEN);

  if( !userName ) {
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }

  if( !pts_GroupMemberListNext( (void *) iterationId, userName, &ast ) ) {
    free( userName );
    if( ast == ADMITERATORDONE ) {
      return 0;
    } else {
      throwAFSException( env, ast );
      return 0;
    }
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
 * Signals that the iteration is complete and will not be accessed anymore.
 *
 * env      the Java environment
 * cls      the current Java class
 * iterationId   the iteration ID of this iteration
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Group_getGroupMembersDone
  (JNIEnv *env, jclass cls, jlong iterationId)
{
  afs_status_t ast;

  if( !pts_GroupMemberListDone( (void *) iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Adds a user to the specified group. 
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the group belongs
 * jgroupName          the name of the group to which to add a member
 * juserName      the name of the user to add
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Group_addMember
  (JNIEnv *env, jclass cls, jlong cellHandle, jstring jgroupName,
   jstring juserName )
{
  afs_status_t ast;
  const char *groupName;
  const char *userName;

  if( jgroupName != NULL ) {
    groupName = (*env)->GetStringUTFChars(env, jgroupName, 0);
    if( !groupName ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    groupName = NULL;
  }

  if( juserName != NULL ) {
    userName = (*env)->GetStringUTFChars(env, juserName, 0);
    if( !userName ) {
      if( groupName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jgroupName, groupName);
      }
      throwAFSException( env, JAFSADMNOMEM );
      return;    
    }
  } else {
    userName = NULL;
  }

  if( !pts_GroupMemberAdd( (void *) cellHandle, userName, groupName, &ast ) ) {
    if( groupName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jgroupName, groupName);
    }
    if( userName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, juserName, userName);
    }
    throwAFSException( env, ast );
    return;
  }

  if( groupName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jgroupName, groupName);
  }
  if( userName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, juserName, userName);
  }
}

/**
 * Removes a user from the specified group. 
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the group belongs
 * jgroupName          the name of the group from which to remove a 
 *                           member
 * juserName      the name of the user to remove
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Group_removeMember
  (JNIEnv *env, jclass cls, jlong cellHandle, jstring jgroupName,
   jstring juserName)
{
  afs_status_t ast;
  const char *groupName;
  const char *userName;

  if( jgroupName != NULL ) {
    groupName = (*env)->GetStringUTFChars(env, jgroupName, 0);
    if( !groupName ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    groupName = NULL;
  }

  if( juserName != NULL ) {
    userName = (*env)->GetStringUTFChars(env, juserName, 0);
    if( !userName ) {
      if( groupName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jgroupName, groupName);
      }
      throwAFSException( env, JAFSADMNOMEM );
      return;    
    }
  } else {
    userName = NULL;
  }

  if( !pts_GroupMemberRemove( (void *)cellHandle, userName, 
			      groupName, &ast ) ) {
    if( groupName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jgroupName, groupName);
    }
    if( userName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, juserName, userName);
    }
    throwAFSException( env, ast );
    return;
  }
  
  if( groupName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jgroupName, groupName);
  }
  if( userName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, juserName, userName);
  }
}

/**
 * Change the owner of the specified group. 
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the group belongs
 * jgroupName          the name of the group of which to change the 
 *                           owner
 * jownerName      the name of the new owner
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Group_changeOwner
  (JNIEnv *env, jclass cls, jlong cellHandle, jstring jgroupName,
   jstring jownerName )
{
  afs_status_t ast;
  const char *groupName;
  const char *ownerName;

  if( jgroupName != NULL ) {
    groupName = (*env)->GetStringUTFChars(env, jgroupName, 0);
    if( !groupName ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    groupName = NULL;
  }

  if( jownerName != NULL ) {
    ownerName = (*env)->GetStringUTFChars(env, jownerName, 0);
    if( !ownerName ) {
      if( groupName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jgroupName, groupName);
      }
      throwAFSException( env, JAFSADMNOMEM );
      return;    
    }
  } else {
    ownerName = NULL;
  }

  if( !pts_GroupOwnerChange( (void *)cellHandle, groupName, 
			     ownerName, &ast ) ) {
    if( groupName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jgroupName, groupName);
    }
    if( ownerName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jownerName, ownerName);
    }
    throwAFSException( env, ast );
    return; 
  }

  if( groupName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jgroupName, groupName);
  }
  if( ownerName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jownerName, ownerName);
  }
  
}

/**
 * Change the name of the specified group. 
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the group belongs
 * joldGroupName          the old name of the group
 * jnewGroupName      the new name for the group
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Group_rename
  (JNIEnv *env, jclass cls, jlong cellHandle, jstring jgroupOldName, 
   jstring jgroupNewName )
{
  afs_status_t ast;
  const char *groupOldName;
  const char *groupNewName;

  if( jgroupOldName != NULL ) {
    groupOldName = (*env)->GetStringUTFChars(env, jgroupOldName, 0);
    if( !groupOldName ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    groupOldName = NULL;
  }

  if( jgroupNewName != NULL ) {
    groupNewName = (*env)->GetStringUTFChars(env, jgroupNewName, 0);
    if( !groupNewName ) {
      if( groupOldName != NULL ) {
        (*env)->ReleaseStringUTFChars(env, jgroupOldName, groupOldName);
      }
      throwAFSException( env, JAFSADMNOMEM );
      return;    
    }
  } else {
    groupNewName = NULL;
  }

  if( !pts_GroupRename( (void *)cellHandle, groupOldName, 
			groupNewName, &ast ) ) {
    if( groupOldName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jgroupOldName, groupOldName);
    }
    if( groupNewName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jgroupNewName, groupNewName);
    }
    throwAFSException( env, ast );
    return;
  }

  if( groupOldName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jgroupOldName, groupOldName);
  }
  if( groupNewName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jgroupNewName, groupNewName);
  }
}

// reclaim global memory used by this portion
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Group_reclaimGroupMemory (JNIEnv *env, jclass cls)
{
  if( groupCls ) {
      (*env)->DeleteGlobalRef(env, groupCls);
      groupCls = 0;
  }
}



