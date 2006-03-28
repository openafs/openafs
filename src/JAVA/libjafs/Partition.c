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
#include "org_openafs_jafs_Partition.h"

#include <afs_vosAdmin.h>
#include <afs_AdminCommonErrors.h>

//// definitions in Internal.c  //////////////////

extern jclass partitionCls;
extern jfieldID partition_nameField;
extern jfieldID partition_idField;
extern jfieldID partition_deviceNameField;
extern jfieldID partition_lockFileDescriptorField;
extern jfieldID partition_totalSpaceField;
extern jfieldID partition_totalFreeSpaceField;

extern jclass volumeCls;
extern jfieldID volume_cachedInfoField;

//////////////////////////////////////////////////////////

///// definition in jafs_Volume.c /////////////////

extern void fillVolumeInfo
            ( JNIEnv *env, jobject volume, vos_volumeEntry_t volEntry );

///////////////////////////////////////////////////


/**
 * Extract the information from the given partition entry and populate the
 * given object
 *
 * env      the Java environment
 * partition      the Partition object to populate with the info
 * partEntry     the container of the partition's information
 */
void fillPartitionInfo
  (JNIEnv *env, jobject partition, vos_partitionEntry_t partEntry)
{
  jstring jdeviceName;
  jstring jpartition;
  jint id;
  afs_status_t ast;

  // get the class fields if need be
  if( partitionCls == 0 ) {
    internal_getPartitionClass( env, partition );
  }

  // fill name and id in case it's a blank object
  jpartition = (*env)->NewStringUTF(env, partEntry.name);
  // get the id
  if( !vos_PartitionNameToId( partEntry.name, (int *) &id, &ast ) ) {
      throwAFSException( env, ast );
      return;
  } 
  (*env)->SetObjectField(env, partition, partition_nameField, jpartition);
  (*env)->SetIntField(env, partition, partition_idField, id);

  jdeviceName = (*env)->NewStringUTF(env, partEntry.deviceName);
  (*env)->SetObjectField(env, partition, partition_deviceNameField, 
			 jdeviceName);

  (*env)->SetIntField(env, partition, partition_lockFileDescriptorField, 
		      partEntry.lockFileDescriptor);
  (*env)->SetIntField(env, partition, partition_totalSpaceField, 
		      partEntry.totalSpace);
  (*env)->SetIntField(env, partition, partition_totalFreeSpaceField, 
		      partEntry.totalFreeSpace);
  
}

/**
 * Fills in the information fields of the provided Partition.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the partition belongs
 * serverHandle  the vos handle of the server on which the 
 *                      partition resides
 * partition   the numeric id of the partition for which to get the 
 *                    info
 * jpartitionObject   the Partition object in which to 
 *                    fill in the information
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Partition_getPartitionInfo
  (JNIEnv *env, jclass cls, jlong cellHandle, jlong serverHandle, 
   jint partition, jobject jpartitionObject)
{
  afs_status_t ast;
  vos_partitionEntry_t partEntry;

  // get the partition entry
  if ( !vos_PartitionGet( (void *) cellHandle, (void *) serverHandle, NULL, 
			  (unsigned int) partition, &partEntry, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

  fillPartitionInfo( env, jpartitionObject, partEntry );

}

/**
 * Translates a partition name into a partition id
 *
 * env      the Java environment
 * cls      the current Java class
 * jname  the name of the partition in question
 * returns   the id of the partition in question
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Partition_translateNameToID
  (JNIEnv *env, jclass cls, jstring jname)
{
  afs_status_t ast;
  jint id;
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

  // get the id
  if( !vos_PartitionNameToId( name, (unsigned int *) &id, &ast ) ) {
    if( name != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jname, name);
    }
    throwAFSException( env, ast );
    return -1;
  } 

  if( name != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jname, name);
  }

  return id;

}

/**
 * Translates a partition id into a partition name
 *
 * env      the Java environment
 * cls      the current Java class
 * id  the id of the partition in question
 * returns   the name of the partition in question
 */
JNIEXPORT jstring JNICALL 
Java_org_openafs_jafs_Partition_translateIDToName
 (JNIEnv *env, jclass cls, jint id)
{
  afs_status_t ast;
  char *name = (char *) malloc( sizeof(char)*VOS_MAX_PARTITION_NAME_LEN);
  jstring jname;

  if( !name ) {
    throwAFSException( env, JAFSADMNOMEM );
    return NULL;    
  }

  // get the name
  if( !vos_PartitionIdToName( (unsigned int) id, name, &ast ) ) {
    free(name);
    throwAFSException( env, ast );
    return NULL;
  } 

  jname = (*env)->NewStringUTF(env, name);
  free(name);
  return jname;

}

/**
 * Returns the total number of volumes hosted by this partition.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the partition belongs
 * serverHandle  the vos handle of the server to which the partition 
 *                      belongs
 * partition   the numeric id of the partition on which the volumes 
 *                    reside
 * returns total number of volumes hosted by this partition
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Partition_getVolumeCount
 (JNIEnv *env, jclass cls, jlong cellHandle, jlong serverHandle, jint partition)
{
  afs_status_t ast;
  void *iterationId;
  vos_volumeEntry_t volEntry;
  int i = 0;

  if( !vos_VolumeGetBegin( (void *) cellHandle, (void *) serverHandle, NULL, 
			   (unsigned int) partition, &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return -1;
  }

  while ( vos_VolumeGetNext( (void *) iterationId, &volEntry, &ast ) ) i++;

  if( ast != ADMITERATORDONE ) {
    throwAFSException( env, ast );
    return -1;
  }

  return i;
}

/**
 * Begin the process of getting the volumes on a partition.  Returns 
 * an iteration ID to be used by subsequent calls to 
 * getVolumesNext and getVolumesDone.  
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the partition belongs
 * serverHandle  the vos handle of the server to which the partition 
 *                      belongs
 * partition   the numeric id of the partition on which the volumes 
 *                    reside
 * returns an iteration ID
 */
JNIEXPORT jlong JNICALL 
Java_org_openafs_jafs_Partition_getVolumesBegin
 (JNIEnv *env, jclass cls, jlong cellHandle, jlong serverHandle, jint partition)
{

  afs_status_t ast;
  void *iterationId;

  if( !vos_VolumeGetBegin( (void *) cellHandle, (void *) serverHandle, NULL, 
			   (unsigned int) partition, &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

  return (jlong) iterationId;
}

/**
 * Begin the process of getting the volumes on a partition.  Returns 
 * an iteration ID to be used by subsequent calls to 
 * getVolumesNext and getVolumesDone.  
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the partition belongs
 * serverHandle  the vos handle of the server to which the partition 
 *                      belongs
 * partition   the numeric id of the partition on which the volumes 
 *                    reside
 * returns an iteration ID
 */
JNIEXPORT jlong JNICALL 
Java_org_openafs_jafs_Partition_getVolumesBeginAt
  (JNIEnv *env, jclass cls, jlong cellHandle, jlong serverHandle,
   jint partition, jint index)
{

  afs_status_t ast;
  void *iterationId;
  vos_volumeEntry_t volEntry;
  int i;

  if( !vos_VolumeGetBegin( (void *) cellHandle, (void *) serverHandle, NULL, 
			   (unsigned int) partition, &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

  for ( i = 1; i < index; i++) {
    if( !vos_VolumeGetNext( (void *) iterationId, &volEntry, &ast ) ) {
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
 * Returns the next volume of the partition.  Returns null 
 * if there are no more volumes.
 *
 * env      the Java environment
 * cls      the current Java class
 * iterationId   the iteration ID of this iteration
 * returns the name of the next volume of the server
 */
JNIEXPORT jstring JNICALL 
Java_org_openafs_jafs_Partition_getVolumesNextString
  (JNIEnv *env, jclass cls, jlong iterationId)
{
  afs_status_t ast;
  jstring jvolume;
  vos_volumeEntry_t volEntry;

  if( !vos_VolumeGetNext( (void *) iterationId, &volEntry, &ast ) ) {
    if( ast == ADMITERATORDONE ) {
      return NULL;
    } else {
      throwAFSException( env, ast );
      return;
    }
  }

  jvolume = (*env)->NewStringUTF(env, volEntry.name);
  return jvolume;

}

/**
 * Fills the next volume object of the partition.  Returns 0 if there
 * are no more volumes, != 0 otherwise.
 *
 * env      the Java environment
 * cls      the current Java class
 * iterationId   the iteration ID of this iteration
 * jvolumeObject    the Volume object in which to fill the values 
 *                         of the next volume
 * returns 0 if there are no more volumes, != 0 otherwise
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Partition_getVolumesNext
  (JNIEnv *env, jclass cls, jlong iterationId, jobject jvolumeObject)
{
  afs_status_t ast;
  jstring jvolume;
  vos_volumeEntry_t volEntry;

  if( !vos_VolumeGetNext( (void *) iterationId, &volEntry, &ast ) ) {
    if( ast == ADMITERATORDONE ) {
      return 0;
    } else {
      throwAFSException( env, ast );
      return 0;
    }
  }


  fillVolumeInfo( env, jvolumeObject, volEntry );

  // get the class fields if need be
  if( volumeCls == 0 ) {
    internal_getVolumeClass( env, jvolumeObject );
  }
  (*env)->SetBooleanField( env, jvolumeObject, volume_cachedInfoField, TRUE );
    
  return 1;

}

/**
 * Fills the next volume object of the partition.  Returns 0 if there
 * are no more volumes, != 0 otherwise.
 *
 * env      the Java environment
 * cls      the current Java class
 * iterationId   the iteration ID of this iteration
 * jvolumeObject    the Volume object in which to fill the values of the 
 *                  next volume
 * advanceCount     the number of volumes to advance past
 * returns 0 if there are no more volumes, != 0 otherwise
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Partition_getVolumesAdvanceTo
  (JNIEnv *env, jclass cls, jlong iterationId, jobject jvolumeObject,
   jint advanceCount)
{
  afs_status_t ast;
  jstring jvolume;
  vos_volumeEntry_t volEntry;
  int i;

  for ( i = 0; i < advanceCount; i++) {
    if( !vos_VolumeGetNext( (void *) iterationId, &volEntry, &ast ) ) {
      if( ast == ADMITERATORDONE ) {
        return 0;
      } else {
        throwAFSException( env, ast );
        return 0;
      }
    }
  }


  fillVolumeInfo( env, jvolumeObject, volEntry );

  // get the class fields if need be
  if( volumeCls == 0 ) {
    internal_getVolumeClass( env, jvolumeObject );
  }
  (*env)->SetBooleanField( env, jvolumeObject, volume_cachedInfoField, TRUE );
    
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
Java_org_openafs_jafs_Partition_getVolumesDone
  (JNIEnv *env, jclass cls, jlong iterationId)
{
  afs_status_t ast;

  if( !vos_VolumeGetDone( (void *) iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }
}

// reclaim global memory being used by this portion
JNIEXPORT void JNICALL
Java_org_openafs_jafs_Partition_reclaimPartitionMemory
 (JNIEnv *env, jclass cls)
{
  if( partitionCls ) {
      (*env)->DeleteGlobalRef(env, partitionCls);
      partitionCls = 0;
  }
}



















