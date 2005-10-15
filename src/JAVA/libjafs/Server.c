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
#include "org_openafs_jafs_Server.h"

#include <afs_clientAdmin.h>
#include <afs_vosAdmin.h>
#include <afs_bosAdmin.h>
#include <afs_AdminCommonErrors.h>
#include <rx/rxkad.h>
#include <bnode.h>

//// definitions in Internal.c  //////////////////

extern jclass serverCls;
extern jfieldID server_nameField;
extern jfieldID server_nameField;
extern jfieldID server_databaseField;
extern jfieldID server_fileServerField;
extern jfieldID server_badDatabaseField;
extern jfieldID server_badFileServerField;
extern jfieldID server_IPAddressField;

extern jclass exectimeCls;
extern jfieldID exectime_HourField;
extern jfieldID exectime_MinField;
extern jfieldID exectime_SecField;
extern jfieldID exectime_DayField;
extern jfieldID exectime_NowField;
extern jfieldID exectime_NeverField;

extern jclass partitionCls;
extern jfieldID partition_cachedInfoField;

extern jclass keyCls;
extern jfieldID key_cachedInfoField;

extern jclass processCls;
extern jfieldID process_cachedInfoField;
extern jfieldID process_nameField;
//extern jfieldID process_serverHandleField;

extern jclass userCls;
extern jfieldID user_nameField;
extern jfieldID user_cachedInfoField;
//////////////////////////////////////////////////////////

///// definition in jafs_Partition.c /////////////////

extern void fillPartitionInfo( JNIEnv *env, jobject partition, 
			       vos_partitionEntry_t partEntry );

///////////////////////////////////////////////////

///// definition in jafs_Key.c /////////////////

extern void fillKeyInfo( JNIEnv *env, jobject key, bos_KeyInfo_t keyEntry );

///////////////////////////////////////////////////

///// definition in jafs_Process.c /////////////////

extern void getProcessInfoChar( JNIEnv *env, void *serverHandle, 
				const char *processName, jobject process );

///////////////////////////////////////////////////


void IntIPAddressToString(int iIPAddress, char *strIPAddress)
{
    sprintf(strIPAddress, "%d.%d.%d.%d",
	    (int)((iIPAddress >> 24) & 0xFF),
	    (int)((iIPAddress >> 16) & 0xFF),
	    (int)((iIPAddress >>  8) & 0xFF),
	    (int)((iIPAddress	  ) & 0xFF)
    );
} //IntIPAddressToString

/**
 * Extract the information from the given server entry and populate the
 * given object
 *
 * env      the Java environment
 * cellHandle    the handle of the cell to which the server belongs
 * server      the Server object to populate with the info
 * servEntry     the container of the server's information
 */
void fillServerInfo
  ( JNIEnv *env, void *cellHandle, jobject server, afs_serverEntry_t servEntry )
{
  jstring jip;
  jobjectArray jaddresses;
  jstring jserver;
  int i = 0;
  char szServerAddr[AFS_MAX_SERVER_NAME_LEN];

  // get the class fields if need be
  if( serverCls == 0 ) {
    internal_getServerClass( env, server );
  }

  // in case it's blank
  jserver = (*env)->NewStringUTF(env, servEntry.serverName);
  (*env)->SetObjectField(env, server, server_nameField, jserver);

  // let's convert just the addresses in the address array into an IP
  jaddresses = (jobjectArray) (*env)->GetObjectField( env, server, 
						      server_IPAddressField );

  for (i = 0; i < AFS_MAX_SERVER_ADDRESS; i++) {
	if (servEntry.serverAddress[i] != 0) {
	  IntIPAddressToString(servEntry.serverAddress[i], szServerAddr);
	  jip = (*env)->NewStringUTF(env, szServerAddr);
	  (*env)->SetObjectArrayElement(env, jaddresses, i, jip);
	} else {
	  break;
	}
  }

  // let's check if this is really a database server
  (*env)->SetBooleanField(env, server, server_databaseField, 
			  servEntry.serverType & DATABASE_SERVER);

  if( servEntry.serverType & DATABASE_SERVER ) {
    // for now, if it thinks it's a database server than it is
    // later, add checks for database configuration, and actual 
    // on-ness of the machine
    (*env)->SetBooleanField(env, server, server_badDatabaseField, FALSE);
  } else {
    (*env)->SetBooleanField(env, server, server_badDatabaseField, FALSE);
  }

  // we should check to see if this is truly a file server or not
  // it could just be an old remnant, left over inside the vldb that 
  // should be removed.
  // if it is a file server, mark it as such.  If not, mark it as faulty.
  (*env)->SetBooleanField(env, server, server_fileServerField,  
			  servEntry.serverType & FILE_SERVER);

  if( servEntry.serverType & FILE_SERVER ) {
    
    // to see if it's really a file server, make sure the 
    // "fs" process is running
    void *bosHandle;
    afs_status_t ast, ast2;
    bos_ProcessType_t processTypeT;
    bos_ProcessInfo_t processInfoT;
    char *fileServerProcessName = "fs";

    // set the file server to true (it thinks it's a file server)
    (*env)->SetBooleanField(env, server, server_fileServerField, TRUE);

    if( !bos_ServerOpen( cellHandle, servEntry.serverName, 
			 &bosHandle, &ast ) ) {
      throwAFSException( env, ast );
      return;
    }

    if( !bos_ProcessInfoGet( bosHandle, fileServerProcessName, &processTypeT, 
			     &processInfoT, &ast ) ) {
      // if the machine does not have a fs process or is not responding 
      // or is part of another cell
      if( ast == BZNOENT || ast == -1 || ast == RXKADBADTICKET ) {
        (*env)->SetBooleanField(env, server, server_badFileServerField, TRUE);
      // otherwise
      } else {
        bos_ServerClose( bosHandle, &ast2 );
        throwAFSException( env, ast );
        return;
      }
    } else {
      // it's good
      (*env)->SetBooleanField(env, server, server_badFileServerField, FALSE);
    }
    if (!bos_ServerClose( bosHandle, &ast )) {
      throwAFSException( env, ast );
      return;
    }
  } else {
    (*env)->SetBooleanField(env, server, server_badFileServerField, FALSE);
  }
}

/**
 * Fills in the information fields of the provided Server. 
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the server belongs
 * jname     the name of the server for which to get the information
 * server     the Server object in which to fill in 
 *                   the information
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Server_getServerInfo (JNIEnv *env, jclass cls, 
					       jlong cellHandle, jstring jname, 
					       jobject server) {

  const char *name;
  afs_status_t ast;
  afs_serverEntry_t servEntry;

  if( jname != NULL ) {
    name = (*env)->GetStringUTFChars(env, jname, 0);
    if( !name ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    name = NULL;
  }

  // get the server entry
  if ( !afsclient_AFSServerGet( (void *) cellHandle, name, 
				&servEntry, &ast ) ) {
    if( name != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jname, name);
    }
    throwAFSException( env, ast );
    return;
  }

  fillServerInfo( env, cellHandle, server, servEntry );

  if( name != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jname, name);
  }

}

/**
 * Returns the total number of partitions hosted by the server denoted by
 * serverHandle, if the server is a fileserver.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the server belongs
 * serverHandle  the vos handle of the server to which the 
 *                      partitions belong
 * returns total number of partitions
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Server_getPartitionCount (JNIEnv *env, jclass cls, 
						   jlong cellHandle, 
						   jlong serverHandle) {

  afs_status_t ast;
  void *iterationId;
  vos_partitionEntry_t partEntry;
  int i = 0;

  if( !vos_PartitionGetBegin( (void *) cellHandle, (void *) serverHandle, 
			      NULL, &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return -1;
  }

  while ( vos_PartitionGetNext( (void *) iterationId, &partEntry, &ast ) ) i++;

  if( ast != ADMITERATORDONE ) {
    throwAFSException( env, ast );
    return -1;
  }

  return i;
}

/**
 * Begin the process of getting the partitions on a server.  Returns 
 * an iteration ID to be used by subsequent calls to 
 * getPartitionsNext and getPartitionsDone.  
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the server belongs
 * serverHandle  the vos handle of the server to which the 
 *                      partitions belong
 * returns an iteration ID
 */
JNIEXPORT jlong JNICALL 
Java_org_openafs_jafs_Server_getPartitionsBegin (JNIEnv *env, jclass cls, 
						    jlong cellHandle, 
						    jlong serverHandle) {

  afs_status_t ast;
  void *iterationId;

  if( !vos_PartitionGetBegin( (void *) cellHandle, (void *) serverHandle, 
			      NULL, &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

  return (jlong) iterationId;

}

/**
 * Returns the next partition of the server.  Returns null 
 * if there are no more partitions.
 *
 * env      the Java environment
 * cls      the current Java class
 * iterationId   the iteration ID of this iteration
 * returns the name of the next partition of the server
 */
JNIEXPORT jstring JNICALL 
Java_org_openafs_jafs_Server_getPartitionsNextString (JNIEnv *env, 
							 jclass cls, 
							 jlong iterationId) {

  afs_status_t ast;
  jstring jpartition;
  vos_partitionEntry_t partEntry;

  if( !vos_PartitionGetNext( (void *) iterationId, &partEntry, &ast ) ) {
    if( ast == ADMITERATORDONE ) {
      return NULL;
    } else {
      throwAFSException( env, ast );
      return;
    }
  }

  jpartition = (*env)->NewStringUTF(env, partEntry.name);
  return jpartition;

}

/**
 * Fills the next partition object of the server.  Returns 0 if there
 * are no more partitions, != 0 otherwise
 *
 * env      the Java environment
 * cls      the current Java class
 * iterationId   the iteration ID of this iteration
 * thePartition   the Partition object in which to fill the 
 *                       values of the next partition
 * returns 0 if there are no more servers, != 0 otherwise
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Server_getPartitionsNext (JNIEnv *env, jclass cls, 
						   jlong iterationId, 
						   jobject jpartitionObject) {
    
  afs_status_t ast;
  vos_partitionEntry_t partEntry;

  if( !vos_PartitionGetNext( (void *) iterationId, &partEntry, &ast ) ) {
    if( ast == ADMITERATORDONE ) {
      return 0;
    } else {
      throwAFSException( env, ast );
      return 0;
    }
  }

  fillPartitionInfo( env, jpartitionObject, partEntry );

  // get the class fields if need be
  if( partitionCls == 0 ) {
    internal_getPartitionClass( env, jpartitionObject );
  }
  (*env)->SetBooleanField( env, jpartitionObject, partition_cachedInfoField, 
			   TRUE );

    
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
Java_org_openafs_jafs_Server_getPartitionsDone (JNIEnv *env, jclass cls, 
						   jlong iterationId) {

  afs_status_t ast;

  if( !vos_PartitionGetDone( (void *) iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Adds the given to name to the list of bos administrators on that server.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the bos handle of the server to which the 
 *                      partitions belong
 * jnewAdmin   the name of the admin to add to the list
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Server_addBosAdmin (JNIEnv *env, jclass cls, 
					     jlong serverHandle, 
					     jstring jnewAdmin) {

  afs_status_t ast;
  const char *newAdmin;

  if( jnewAdmin != NULL ) {
    newAdmin = (*env)->GetStringUTFChars(env, jnewAdmin, 0);
    if( !newAdmin ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    newAdmin = NULL;
  }

  if( !bos_AdminCreate( (void *) serverHandle, newAdmin, &ast ) ) {
    if( newAdmin != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jnewAdmin, newAdmin);
    }
    throwAFSException( env, ast );
    return;
  }

  if( newAdmin != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jnewAdmin, newAdmin);
  }

}

/**
 * Removes the given to name from the list of bos administrators on 
 * that server.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the bos handle of the server to which the 
 *                      partitions belong
 * joldAdmin   the name of the admin to remove from the list
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Server_removeBosAdmin (JNIEnv *env, jclass cls, 
						jlong serverHandle, 
						jstring joldAdmin) {

  afs_status_t ast;
  const char *oldAdmin;

  if( joldAdmin != NULL ) {
    oldAdmin = (*env)->GetStringUTFChars(env, joldAdmin, 0);
    if( !oldAdmin ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    oldAdmin = NULL;
  }

  if( !bos_AdminDelete( (void *) serverHandle, oldAdmin, &ast ) ) {
    if( oldAdmin != NULL ) {
      (*env)->ReleaseStringUTFChars(env, joldAdmin, oldAdmin);
    }
    throwAFSException( env, ast );
    return;
  }

  if( oldAdmin != NULL ) {
    (*env)->ReleaseStringUTFChars(env, joldAdmin, oldAdmin);
  }

}

/**
 * Returns the total number of BOS administrators associated with the server 
 * denoted by serverHandle.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the vos handle of the server to which the 
 *                      BOS admins belong
 * returns total number of BOS administrators
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Server_getBosAdminCount (JNIEnv *env, jclass cls, 
						  jlong serverHandle) {

  afs_status_t ast;
  void *iterationId;
  char *admin;
  jstring jadmin;
  int i = 0;

  if( !bos_AdminGetBegin( (void *) serverHandle, &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return -1;
  }

  admin = (char *) malloc( sizeof(char)*BOS_MAX_NAME_LEN);

  if( !admin ) {
    throwAFSException( env, JAFSADMNOMEM );
    return -1;
  }

  while ( bos_AdminGetNext( (void *) iterationId, admin, &ast ) ) i++;

  free(admin);

  if( ast != ADMITERATORDONE ) {
    throwAFSException( env, ast );
    return -1;
  }

  return i;
}

/**
 * Begin the process of getting the bos amdinistrators on a server.  Returns 
 * an iteration ID to be used by subsequent calls to 
 * getBosAdminsNext and getBosAdminsDone.  
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the bos handle of the server to which the 
 *                      partitions belong
 * returns an iteration ID
 */
JNIEXPORT jlong JNICALL 
Java_org_openafs_jafs_Server_getBosAdminsBegin (JNIEnv *env, jclass cls, 
						   jlong serverHandle) {

  afs_status_t ast;
  void *iterationId;

  if( !bos_AdminGetBegin( (void *) serverHandle, &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

  return (jlong) iterationId;

}

/**
 * Returns the next bos admin of the server.  Returns null 
 * if there are no more admins.
 *
 * env      the Java environment
 * cls      the current Java class
 * iterationId   the iteration ID of this iteration
 * returns the name of the next admin of the server
 */
JNIEXPORT jstring JNICALL 
Java_org_openafs_jafs_Server_getBosAdminsNextString (JNIEnv *env, 
							jclass cls, 
							jlong iterationId) {

  afs_status_t ast;
  jstring jadmin;
  char *admin = (char *) malloc( sizeof(char)*BOS_MAX_NAME_LEN );

  if( !admin ) {
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }

  if( !bos_AdminGetNext( (void *) iterationId, admin, &ast ) ) {
    free(admin);
    if( ast == ADMITERATORDONE ) {
      return NULL;
    } else {
      throwAFSException( env, ast );
      return;
    }
  }

  jadmin = (*env)->NewStringUTF(env, admin);
  free(admin);
  return jadmin;

}

/**
 * Returns the next bos admin of the server.  Returns 0 if there
 * are no more admins, != 0 otherwise.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which these admins belong
 * iterationId   the iteration ID of this iteration
 * juserObject   the user object in which to fill the values of this admin
 * returns 0 if no more admins, != 0 otherwise
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Server_getBosAdminsNext (JNIEnv *env, jclass cls, 
						  jlong cellHandle, 
						  jlong iterationId, 
						  jobject juserObject ) {
    
  afs_status_t ast;
  char *admin;
  jstring jadmin;

  admin = (char *) malloc( sizeof(char)*BOS_MAX_NAME_LEN);

  if( !admin ) {
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }

  if( !bos_AdminGetNext( (void *) iterationId, admin, &ast ) ) {
    free( admin );
    if( ast == ADMITERATORDONE ) {
      return 0;
    } else {
      throwAFSException( env, ast );
      return 0;
    }
  }

  jadmin = (*env)->NewStringUTF(env, admin);

  if( userCls == 0 ) {
    internal_getUserClass( env, juserObject );
  }

  (*env)->SetObjectField(env, juserObject, user_nameField, jadmin);

  getUserInfoChar( env, cellHandle, admin, juserObject );
  (*env)->SetBooleanField( env, juserObject, user_cachedInfoField, TRUE );

  free( admin );
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
Java_org_openafs_jafs_Server_getBosAdminsDone (JNIEnv *env, jclass cls, 
						  jlong iterationId) {

  afs_status_t ast;

  if( !bos_AdminGetDone( (void *) iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Returns the total number of keys hosted by the server denoted by
 * serverHandle.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the vos handle of the server to which the 
 *                      keys belong
 * returns total number of keys
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Server_getKeyCount (JNIEnv *env, jclass cls, 
					     jlong serverHandle) {

  afs_status_t ast;
  void *iterationId;
  bos_KeyInfo_t keyEntry;
  int i = 0;

  if( !bos_KeyGetBegin( (void *) serverHandle, &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return -1;
  }

  while ( bos_KeyGetNext( (void *) iterationId, &keyEntry, &ast ) ) i++;

  if( ast != ADMITERATORDONE ) {
    throwAFSException( env, ast );
    return -1;
  }

  return i;
}

/**
 * Begin the process of getting the keys of a server.  Returns 
 * an iteration ID to be used by subsequent calls to 
 * getKeysNext and getKeysDone.  
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the bos handle of the server to which the keys belong
 * returns an iteration ID
 */
JNIEXPORT jlong JNICALL 
Java_org_openafs_jafs_Server_getKeysBegin (JNIEnv *env, jclass cls, 
					      jlong serverHandle) {

  afs_status_t ast;
  void *iterationId;

  if( !bos_KeyGetBegin( (void *) serverHandle, &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

  return (jlong) iterationId;

}

/**
 * Returns the next key of the server.  Returns 0 if there
 * are no more keys, != 0 otherwise.
 *
 * env      the Java environment
 * cls      the current Java class
 * iterationId   the iteration ID of this iteration
 * jkeyObject   a Key object, in which to fill in the
 *                 properties of the next key.
 * returns 0 if there are no more keys, != 0 otherwise
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Server_getKeysNext (JNIEnv *env, jclass cls, 
					     jlong iterationId, 
					     jobject jkeyObject) {
    
  afs_status_t ast;
  bos_KeyInfo_t keyEntry;

  if( !bos_KeyGetNext( (void *) iterationId, &keyEntry, &ast ) ) {
    if( ast == ADMITERATORDONE ) {
      return 0;
    } else {
      throwAFSException( env, ast );
      return 0;
    }
  }

  fillKeyInfo( env, jkeyObject, keyEntry );

  // get the class fields if need be
  if( keyCls == 0 ) {
    internal_getKeyClass( env, jkeyObject );
  }

  (*env)->SetBooleanField( env, jkeyObject, key_cachedInfoField, TRUE );

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
Java_org_openafs_jafs_Server_getKeysDone (JNIEnv *env, jclass cls, 
					     jlong iterationId) {

  afs_status_t ast;

  if( !bos_KeyGetDone( (void *) iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Returns the total number of processes hosted by the server denoted by
 * serverHandle.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the vos handle of the server to which the 
 *                      processes belong
 * returns total number of processes
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Server_getProcessCount (JNIEnv *env, jclass cls, 
						 jlong serverHandle) {

  afs_status_t ast;
  void *iterationId;
  char *process;
  jstring jprocess;
  int i = 0;

  if( !bos_ProcessNameGetBegin( (void *) serverHandle, &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return -1;
  }

  process = (char *) malloc( sizeof(char)*BOS_MAX_NAME_LEN );

  if( !process ) {
    throwAFSException( env, JAFSADMNOMEM );
    return -1;
  }

  while ( bos_ProcessNameGetNext( (void *) iterationId, process, &ast ) ) i++;

  free( process );

  if( ast != ADMITERATORDONE ) {
    throwAFSException( env, ast );
    return -1;
  }

  return i;
}

/**
 * Begin the process of getting the processes on a server.  Returns 
 * an iteration ID to be used by subsequent calls to 
 * getProcessesNext and getProcessesDone.  
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the bos handle of the server to which the 
 *                      processes belong
 * returns an iteration ID
 */
JNIEXPORT jlong JNICALL 
Java_org_openafs_jafs_Server_getProcessesBegin (JNIEnv *env, jclass cls, 
						   jlong serverHandle) {

  afs_status_t ast;
  void *iterationId;

  if( !bos_ProcessNameGetBegin( (void *) serverHandle, &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

  return (jlong) iterationId;

}

/**
 * Returns the next process of the server.  Returns null 
 * if there are no more processes.
 *
 * env      the Java environment
 * cls      the current Java class
 * iterationId   the iteration ID of this iteration
 * returns the name of the next process of the cell
 */
JNIEXPORT jstring JNICALL 
Java_org_openafs_jafs_Server_getProcessesNextString (JNIEnv *env, 
							jclass cls, 
							jlong iterationId) {

  afs_status_t ast;
  jstring jprocess;
  char *process = (char *) malloc( sizeof(char)*BOS_MAX_NAME_LEN );

  if( !process ) {
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }

  if( !bos_ProcessNameGetNext( (void *) iterationId, process, &ast ) ) {
    free( process );
    if( ast == ADMITERATORDONE ) {
      return NULL;
    } else {
      throwAFSException( env, ast );
      return;
    }
  }

  jprocess = (*env)->NewStringUTF(env, process);
  free( process );
  return jprocess;

}

/**
 * Fills the next process object of the server.  Returns 0 if there
 * are no more processes, != 0 otherwise.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle    the handle of the BOS server that hosts the process
 * iterationId   the iteration ID of this iteration
 * jprocessObject    the Process object in which to fill the 
 *                          values of the next process
 * returns 0 if there are no more processes, != otherwise
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Server_getProcessesNext (JNIEnv *env, jclass cls, 
						  jlong serverHandle, 
						  jlong iterationId, 
						  jobject jprocessObject) {
    
  afs_status_t ast;
  char *process = (char *) malloc( sizeof(char)*BOS_MAX_NAME_LEN );
  jstring jprocess;

  if( !process ) {
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }

  if( !bos_ProcessNameGetNext( (void *) iterationId, process, &ast ) ) {
    if( ast == ADMITERATORDONE ) {
      return 0;
    } else {
      free( process );
      throwAFSException( env, ast );
      return 0;
    }
  }

  // get the class fields if need be
  if( processCls == 0 ) {
    internal_getProcessClass( env, jprocessObject );
  }

  jprocess = (*env)->NewStringUTF(env, process);
  (*env)->SetObjectField(env, jprocessObject, process_nameField, jprocess);

  getProcessInfoChar( env, (void *) serverHandle, process, jprocessObject );

  (*env)->SetBooleanField( env, jprocessObject, 
			   process_cachedInfoField, TRUE );

  free( process );
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
Java_org_openafs_jafs_Server_getProcessesDone (JNIEnv *env, jclass cls, 
						  jlong iterationId) {

  afs_status_t ast;

  if( !bos_ProcessNameGetDone( (void *) iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Salvages (restores consistency to) a volume, partition, or server
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the volume belongs
 * serverHandle  the bos handle of the server on which the 
 *                      volume resides
 * jpartName  the name of the partition to salvage, 
 *                   can be null only if volName is 
 *                   null
 * jvolName  the name of the volume to salvage, 
 *                  can be null
 * numSalvagers   the number of salvager processes to run in parallel
 * jtempDir   directory to place temporary files, can be 
 *                  null
 * jlogFile    where salvager log will be written, can be 
 *                   null
 * inspectAllVolumes   whether or not to inspect all volumes, 
 *                            not just those marked as active at crash
 * removeBadlyDamaged   whether or not to remove a volume if it's 
 *                             badly damaged
 * writeInodes   whether or not to record a list of inodes modified
 * writeRootInodes   whether or not to record a list of AFS 
 *                          inodes owned by root
 * forceDirectory   whether or not to salvage an entire directory 
 *                         structure
 * forceBlockReads   whether or not to force the salvager to read 
 *                          the partition
 *                          one block at a time and skip badly damaged 
 *                          blocks.  Use if partition has disk errors
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Server_salvage (JNIEnv *env, jclass cls, 
					 jlong cellHandle, jlong serverHandle, 
					 jstring jpartName, jstring jvolName, 
					 jint numSalvagers, jstring jtempDir, 
					 jstring jlogFile, 
					 jboolean inspectAllVolumes, 
					 jboolean removeBadlyDamaged, 
					 jboolean writeInodes, 
					 jboolean writeRootInodes, 
					 jboolean forceDirectory, 
					 jboolean forceBlockReads) {

  afs_status_t ast;
  const char *partName;
  const char *volName;
  const char *tempDir;
  const char *logFile;
  vos_force_t force;
  bos_SalvageDamagedVolumes_t sdv;
  bos_WriteInodes_t wi;
  bos_WriteRootInodes_t wri;
  bos_ForceDirectory_t forceD;
  bos_ForceBlockRead_t forceBR;
  
  // convert strings
  if( jpartName != NULL ) {
    partName = (*env)->GetStringUTFChars(env, jpartName, 0);    
    if( !partName ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    partName = NULL;
  }
  if( jvolName != NULL ) {
    volName = (*env)->GetStringUTFChars(env, jvolName, 0);    
    if( !volName ) {
      if( partName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jpartName, partName);    
      }
      throwAFSException( env, JAFSADMNOMEM );
      return;    
    }
  } else {
    volName = NULL;
  }
  if( jtempDir != NULL ) {
    tempDir = (*env)->GetStringUTFChars(env, jtempDir, 0);    
    if( !tempDir ) {
      if( partName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jpartName, partName);    
      }
      if( volName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jvolName, volName);    
      }
      throwAFSException( env, JAFSADMNOMEM );
      return;    
    }
  } else {
    tempDir = NULL;
  }
  if( jlogFile != NULL ) {
    logFile = (*env)->GetStringUTFChars(env, jlogFile, 0);    
    if( !logFile ) {
      if( partName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jpartName, partName);    
      }
      if( volName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jvolName, volName);    
      }
      if( tempDir != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jtempDir, tempDir);    
      }
      throwAFSException( env, JAFSADMNOMEM );
      return;    
    }
  } else {
    logFile = NULL;
  }

  // deal with booleans
  if( inspectAllVolumes ) {
    force = VOS_FORCE;
  } else {
    force = VOS_NORMAL;
  }
  if( removeBadlyDamaged ) {
    sdv = BOS_DONT_SALVAGE_DAMAGED_VOLUMES;
  } else {
    sdv = BOS_SALVAGE_DAMAGED_VOLUMES;
  }
  if( writeInodes ) {
    wi = BOS_SALVAGE_WRITE_INODES;
  } else {
    wi = BOS_SALVAGE_DONT_WRITE_INODES;
  }
  if( writeRootInodes ) {
    wri = BOS_SALVAGE_WRITE_ROOT_INODES;
  } else {
    wri = BOS_SALVAGE_DONT_WRITE_ROOT_INODES;
  }
  if( forceDirectory ) {
    forceD = BOS_SALVAGE_FORCE_DIRECTORIES;
  } else {
    forceD = BOS_SALVAGE_DONT_FORCE_DIRECTORIES;
  }
  if( forceBlockReads ) {
    forceBR = BOS_SALVAGE_FORCE_BLOCK_READS;
  } else {
    forceBR = BOS_SALVAGE_DONT_FORCE_BLOCK_READS;
  }

  //salvage!
  if( !bos_Salvage( (void *) cellHandle, (void *) serverHandle, partName, 
		    volName, (int) numSalvagers, tempDir, logFile, force, sdv, 
		    wi, wri, forceD, forceBR, &ast ) ) {
    if( partName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jpartName, partName);    
    }
    if( volName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jvolName, volName);    
    }
    if( tempDir != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jtempDir, tempDir);    
    }
    if( logFile != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jlogFile, logFile);    
    }
    throwAFSException( env, ast );
    return;
  }

  // release strings
  if( partName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jpartName, partName);    
  }
  if( volName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jvolName, volName);    
  }
  if( tempDir != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jtempDir, tempDir);    
  }
  if( logFile != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jlogFile, logFile);    
  }

}

/**
 *  Fills in the restart time fields of the given Server
 *  object. 
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the bos handle of the server to which the key belongs
 * jtype  whether to get the general or binary restart. 
 *               Acceptable values are:
 *               org_opemafs_jafs_Server_RESTART_BINARY
 *               org_opemafs_jafs_Server_RESTART_GENERAL    
 * execTime   the ExecutableTime object, in which 
 *                   to fill the restart time fields
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Server_getRestartTime
  (JNIEnv *env, jclass cls, jlong serverHandle, jint jtype, jobject exectime)
{
  afs_status_t ast;
  bos_Restart_t type;
  bos_RestartTime_t time;
  jfieldID hourField;
  jfieldID minField;
  jfieldID secField;
  jfieldID dayField;
  jfieldID neverField;
  jfieldID nowField;

  // get the class fields if need be
  if( exectimeCls == 0 ) {
    internal_getExecTimeClass( env, exectime );
  }

  if( jtype == org_openafs_jafs_Server_RESTART_BINARY ) {
    type = BOS_RESTART_DAILY;
  } else {
    type = BOS_RESTART_WEEKLY;
  }

  hourField  = exectime_HourField;
  minField   = exectime_MinField;
  secField   = exectime_SecField;
  dayField   = exectime_DayField;
  neverField = exectime_NeverField;
  nowField   = exectime_NowField;

  if( !bos_ExecutableRestartTimeGet( (void *) serverHandle, type, 
				     &time, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }
  
  // set now
  (*env)->SetBooleanField(env, exectime, nowField, 
			  (time.mask & BOS_RESTART_TIME_NOW) );
  
  // set never
  (*env)->SetBooleanField(env, exectime, neverField, 
			  (time.mask & BOS_RESTART_TIME_NEVER) );

  // set hour
  (*env)->SetShortField(env, exectime, hourField, time.hour );

  // set minute
  (*env)->SetShortField(env, exectime, minField, time.min );

  // set second
  (*env)->SetShortField(env, exectime, secField, time.sec );

  // set day
  if( time.mask & BOS_RESTART_TIME_DAY ) {
    (*env)->SetShortField(env, exectime, dayField, time.day );
  } else {
    (*env)->SetShortField(env, exectime, dayField, (jshort) -1 );
  }

}

/**
 *  Sets the restart time of the bos server.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the bos handle of the server to which the key belongs
 * jtype  whether this is to be a general or binary restart. 
 *               Acceptable values are:
 *               org_opemafs_jafs_Server_RESTART_BINARY
 *               org_opemafs_jafs_Server_RESTART_GENERAL
 * executableTime   the ExecutableTime object containing the 
 *                         desired information
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Server_setRestartTime (JNIEnv *env, jclass cls, 
						jlong serverHandle, jint jtype, 
						jobject exectime ) {

  afs_status_t ast;
  bos_Restart_t type;
  bos_RestartTime_t time;
  jboolean doHour;
  jboolean doMinute;
  jboolean doSecond;
  jboolean doDay;
  jboolean doNever;
  jboolean doNow;  
  jshort hour;
  jshort minute;
  jshort second;
  jshort day;
  jfieldID hourField;
  jfieldID minField;
  jfieldID secField;
  jfieldID dayField;
  jfieldID neverField;
  jfieldID nowField;

  // get the class fields if need be
  if( exectimeCls == 0 ) {
    internal_getExecTimeClass( env, exectime );
  }

  if( jtype == org_openafs_jafs_Server_RESTART_BINARY ) {
    type = BOS_RESTART_DAILY;
  } else {
    type = BOS_RESTART_WEEKLY;
  }

  hourField  = exectime_HourField;
  minField   = exectime_MinField;
  secField   = exectime_SecField;
  dayField   = exectime_DayField;
  neverField = exectime_NeverField;
  nowField   = exectime_NowField;

  hour = (*env)->GetShortField(env, exectime, hourField );
  if( hour != 0 ) {
    doHour = TRUE;
  } else {
    doHour = FALSE;
  }
  minute = (*env)->GetShortField(env, exectime, minField );
  if( minute != 0 ) {
    doMinute = TRUE;
  } else {
    doMinute = FALSE;
  }
  second = (*env)->GetShortField(env, exectime, secField );
  if( second != 0 ) {
    doSecond = TRUE;
  } else {
    doSecond = FALSE;
  }
  day = (*env)->GetShortField(env, exectime, dayField );
  if( day != -1 ) {
    doDay = TRUE;
  } else {
    doDay = FALSE;
  }
  doNever = (*env)->GetBooleanField(env, exectime, neverField );
  doNow = (*env)->GetBooleanField(env, exectime, nowField );

  bzero(&time, sizeof(time));

  if( jtype == org_openafs_jafs_Server_RESTART_BINARY ) {
    type = BOS_RESTART_DAILY;
  } else {
    type = BOS_RESTART_WEEKLY;
  }  

  if( doHour ) {
    time.mask |= BOS_RESTART_TIME_HOUR;
  }
  if( doMinute ) {
    time.mask |= BOS_RESTART_TIME_MINUTE;
  }
  if( doSecond ) {
    time.mask |= BOS_RESTART_TIME_SECOND;
  }
  if( doDay ) {
    time.mask |= BOS_RESTART_TIME_DAY;
  }
  if( doNever ) {
    time.mask |= BOS_RESTART_TIME_NEVER;
  }
  if( doNow ) {
    time.mask |= BOS_RESTART_TIME_NOW;
  }

  time.hour = hour;
  time.min = minute;
  time.sec = second;
  time.day = day;

  if( !bos_ExecutableRestartTimeSet( (void *) serverHandle, type, 
				     time, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 *  Synchronizes a particular server with the volume location database.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the server belongs
 * serverHandle  the vos handle of the server     
 * partition   the id of the partition to sync, can be -1 to ignore
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Server_syncServerWithVLDB (JNIEnv *env, jclass cls, 
						    jlong cellHandle, 
						    jlong serverHandle, 
						    jint partition) {

  afs_status_t ast;
  int *part;

  if( partition == -1 ) {
    part = NULL;
  } else {
    part = (int *) &partition;
  }

  if( !vos_ServerSync( (void *) cellHandle, (void *) serverHandle, 
		       NULL, part, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 *  Synchronizes the volume location database with a particular server.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the server belongs
 * serverHandle  the vos handle of the server     
 * partition   the id of the partition to sync, can be -1 to ignore
 * forceDeletion   whether or not to force the deletion of bad volumes
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Server_syncVLDBWithServer (JNIEnv *env, jclass cls, 
						    jlong cellHandle, 
						    jlong serverHandle, 
						    jint partition, 
						    jboolean forceDeletion) {

  afs_status_t ast;
  int *part;
  vos_force_t force;

  if( partition == -1 ) {
    part = NULL;
  } else {
    part = (int *) &partition;
  }
  
  if( forceDeletion ) {
    force = VOS_FORCE;
  } else {
    force = VOS_NORMAL;
  }

  if( !vos_VLDBSync( (void *) cellHandle, (void *) serverHandle, NULL, part, 
		     force, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Start all server processes.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the bos handle of the server to which the 
 *                      processes belong
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Server_startAllProcesses (JNIEnv *env, jclass cls, 
						   jlong serverHandle) {

  afs_status_t ast;

  if( !bos_ProcessAllStart( (void *) serverHandle, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Stop all server processes.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the bos handle of the server to which the 
 *                      processes belong
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Server_stopAllProcesses (JNIEnv *env, jclass cls, 
						  jlong serverHandle) {

  afs_status_t ast;

  if( !bos_ProcessAllStop( (void *) serverHandle, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Restart all server processes.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the bos handle of the server to which the 
 *                      processes belong
 * restartBosServer   whether or not to restart the bos server as well
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Server_restartAllProcesses (JNIEnv *env, jclass cls, 
						     jlong serverHandle, 
						   jboolean restartBosServer) {

  afs_status_t ast;
  bos_RestartBosServer_t rbs;
  
  if( restartBosServer ) {
    rbs = BOS_RESTART_BOS_SERVER;
  } else {
    rbs = BOS_DONT_RESTART_BOS_SERVER;
  }

  if( !bos_ProcessAllStopAndRestart( (void *) serverHandle, rbs, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Retrieves a specified bos log from a server.  Right now this 
 * method will simply return a huge String containing the log, but 
 * hopefully we can devise a better way to make this work more efficiently.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the bos handle of the server to which the key belongs
 * jlogFile   the full path and name of the desired bos log
 */
JNIEXPORT jstring JNICALL 
Java_org_openafs_jafs_Server_getLog(JNIEnv *env, jclass cls, 
				       jlong serverHandle, jstring jlogFile) {

    afs_status_t ast;
    const char *logFile;
    char *logData;
    unsigned long currInLogSize = 1;
    unsigned long currOutLogSize = 0;
    jstring logOut;

    if( jlogFile != NULL ) {
      logFile = (*env)->GetStringUTFChars(env, jlogFile, 0);
      if( !logFile ) {
	  throwAFSException( env, JAFSADMNOMEM );
	  return;    
      }
    } else {
      logFile = NULL;
    }

    logData = (char *) malloc( sizeof(char)*currInLogSize );
    if( !logData ) {
      throwAFSException( env, JAFSADMNOMEM );
      return;    
    }

    // check how big the log is . . .
    if( !bos_LogGet( (void *) serverHandle, logFile, 
		     &currOutLogSize, logData, &ast ) ) {
      // anything but not enough room in buffer
      if( ast != ADMMOREDATA ) {
	free( logData );
	if( logFile != NULL ) { 
	  (*env)->ReleaseStringUTFChars(env, jlogFile, logFile);
	}
	throwAFSException( env, ast );
	return NULL;
      }
    }

    free( logData );

    // increase log size (plus one for terminator)
    currInLogSize = currOutLogSize + 1;
    // allocate buffer
    logData = (char *) malloc( sizeof(char)*currInLogSize );
    if( !logData ) {
      throwAFSException( env, JAFSADMNOMEM );
      return;    
    }

    if( !logData ) {
      // memory exception
      if( logFile != NULL ) { 
	  (*env)->ReleaseStringUTFChars(env, jlogFile, logFile);
      }
      throwAFSException( env, ast );
      return NULL;
    }

    // get the log for real
    if( !bos_LogGet( (void *) serverHandle, logFile, &currOutLogSize, 
		     logData, &ast ) ) {
	free( logData );
	if( logFile != NULL ) { 
	  (*env)->ReleaseStringUTFChars(env, jlogFile, logFile);
	}
	(*env)->ReleaseStringUTFChars(env, jlogFile, logFile);
	throwAFSException( env, ast );
	return NULL;
    }
    
    logData[currOutLogSize] == '\0';

    logOut = (*env)->NewStringUTF(env, logData);
    
    free( logData );
    if( logFile != NULL ) { 
      (*env)->ReleaseStringUTFChars(env, jlogFile, logFile);
    }
    return logOut;

}


/**
 * Executes any command on the specified server.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the bos handle of the server to which the key belongs
 * jcommand     the text of the commmand to execute
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Server_executeCommand (JNIEnv *env, jclass cls, 
						jlong serverHandle, 
						jstring jcommand) {

  afs_status_t ast;
  const char *command;

  if( jcommand != NULL ) {
    command = (*env)->GetStringUTFChars(env, jcommand, 0);
    if( !command ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    command = NULL;
  }

  if( !bos_CommandExecute( (void *) serverHandle, command, &ast ) ) {
    if( command != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jcommand, command);
    }
    throwAFSException( env, ast );
    return;
  }

  if( command != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jcommand, command);
  }

}

// reclaim global memory being used by this portion
JNIEXPORT void JNICALL
Java_org_openafs_jafs_Server_reclaimServerMemory (JNIEnv *env, jclass cls) {

  if( serverCls ) {
      (*env)->DeleteGlobalRef(env, serverCls);
      serverCls = 0;
  }

}




















