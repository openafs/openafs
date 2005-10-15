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
#include "org_openafs_jafs_Volume.h"

#include <afs_vosAdmin.h>
#include <afs_AdminCommonErrors.h>

//// definitions in Internal.c  //////////////////

extern jclass volumeCls;
extern jfieldID volume_nameField;
extern jfieldID volume_idField;
extern jfieldID volume_readWriteIdField;
extern jfieldID volume_readOnlyIdField;
extern jfieldID volume_backupIdField;
extern jfieldID volume_creationDateField;
extern jfieldID volume_lastAccessDateField;
extern jfieldID volume_lastUpdateDateField;
extern jfieldID volume_lastBackupDateField;
extern jfieldID volume_copyCreationDateField;
extern jfieldID volume_accessesSinceMidnightField;
extern jfieldID volume_fileCountField;
extern jfieldID volume_maxQuotaField;
extern jfieldID volume_currentSizeField;
extern jfieldID volume_statusField;
extern jfieldID volume_dispositionField;
extern jfieldID volume_typeField;

//////////////////////////////////////////////////////////

/**
 * Extract the information from the given volume entry and populate the
 * given object
 *
 * env      the Java environment
 * volume      the Volume object to populate with the info
 * volEntry     the container of the volume's information
 */
extern void fillVolumeInfo( JNIEnv *env, jobject volume, 
			    vos_volumeEntry_t volEntry ) {

  jstring jvolume;

  // get the class fields if need be
  if( volumeCls == 0 ) {
    internal_getVolumeClass( env, volume );
  }

  // set name, just in case it was a completely blank object
  jvolume = (*env)->NewStringUTF(env, volEntry.name);
  (*env)->SetObjectField(env, volume, volume_nameField, jvolume);

  (*env)->SetIntField(env, volume, volume_idField, volEntry.id);
  (*env)->SetIntField(env, volume, volume_readWriteIdField, 
		      volEntry.readWriteId);
  (*env)->SetIntField(env, volume, volume_readOnlyIdField, 
		      volEntry.readOnlyId);
  (*env)->SetIntField(env, volume, volume_backupIdField, volEntry.backupId);
  (*env)->SetLongField(env, volume, volume_creationDateField, 
		       volEntry.creationDate);
  (*env)->SetLongField(env, volume, volume_lastAccessDateField, 
		       volEntry.lastAccessDate);
  (*env)->SetLongField(env, volume, volume_lastUpdateDateField, 
		       volEntry.lastUpdateDate);
  (*env)->SetLongField(env, volume, volume_lastBackupDateField, 
		       volEntry.lastBackupDate);
  (*env)->SetLongField(env, volume, volume_copyCreationDateField, 
		       volEntry.copyCreationDate);
  (*env)->SetIntField(env, volume, volume_accessesSinceMidnightField, 
		      volEntry.accessesSinceMidnight);
  (*env)->SetIntField(env, volume, volume_fileCountField, volEntry.fileCount);
  (*env)->SetIntField(env, volume, volume_maxQuotaField, volEntry.maxQuota);
  (*env)->SetIntField(env, volume, volume_currentSizeField, 
		      volEntry.currentSize);

  // set status variable
  switch( volEntry.status ) {
  case VOS_OK :
      (*env)->SetIntField(env, volume, volume_statusField, 
			  org_openafs_jafs_Volume_VOLUME_OK);
      break;
  case VOS_SALVAGE :
      (*env)->SetIntField(env, volume, volume_statusField, 
			  org_openafs_jafs_Volume_VOLUME_SALVAGE);
      break;
  case VOS_NO_VNODE:
      (*env)->SetIntField(env, volume, volume_statusField, 
			  org_openafs_jafs_Volume_VOLUME_NO_VNODE);
      break;
  case VOS_NO_VOL:
      (*env)->SetIntField(env, volume, volume_statusField, 
			  org_openafs_jafs_Volume_VOLUME_NO_VOL);
      break;
  case VOS_VOL_EXISTS:
      (*env)->SetIntField(env, volume, volume_statusField, 
			  org_openafs_jafs_Volume_VOLUME_VOL_EXISTS);
      break;
  case VOS_NO_SERVICE:
      (*env)->SetIntField(env, volume, volume_statusField, 
			  org_openafs_jafs_Volume_VOLUME_NO_SERVICE);
      break;
  case VOS_OFFLINE:
      (*env)->SetIntField(env, volume, volume_statusField, 
			  org_openafs_jafs_Volume_VOLUME_OFFLINE);
      break;
  case VOS_ONLINE:
      (*env)->SetIntField(env, volume, volume_statusField, 
			  org_openafs_jafs_Volume_VOLUME_ONLINE);
      break;
  case VOS_DISK_FULL:
      (*env)->SetIntField(env, volume, volume_statusField, 
			  org_openafs_jafs_Volume_VOLUME_DISK_FULL);
      break;
  case VOS_OVER_QUOTA:
      (*env)->SetIntField(env, volume, volume_statusField, 
			  org_openafs_jafs_Volume_VOLUME_OVER_QUOTA);
      break;
  case VOS_BUSY:
      (*env)->SetIntField(env, volume, volume_statusField, 
			  org_openafs_jafs_Volume_VOLUME_BUSY);
      break;
  case VOS_MOVED:
      (*env)->SetIntField(env, volume, volume_statusField, 
			  org_openafs_jafs_Volume_VOLUME_MOVED);
      break;
  default:
      throwAFSException( env, volEntry.status );
  }

  // set disposition variable
  switch( volEntry.volumeDisposition ) {
  case VOS_OK :
      (*env)->SetIntField(env, volume, volume_dispositionField, 
			  org_openafs_jafs_Volume_VOLUME_OK);
      break;
  case VOS_SALVAGE :
      (*env)->SetIntField(env, volume, volume_dispositionField, 
			  org_openafs_jafs_Volume_VOLUME_SALVAGE);
      break;
  case VOS_NO_VNODE:
      (*env)->SetIntField(env, volume, volume_dispositionField, 
			  org_openafs_jafs_Volume_VOLUME_NO_VNODE);
      break;
  case VOS_NO_VOL:
      (*env)->SetIntField(env, volume, volume_dispositionField, 
			  org_openafs_jafs_Volume_VOLUME_NO_VOL);
      break;
  case VOS_VOL_EXISTS:
      (*env)->SetIntField(env, volume, volume_dispositionField, 
			  org_openafs_jafs_Volume_VOLUME_VOL_EXISTS);
      break;
  case VOS_NO_SERVICE:
      (*env)->SetIntField(env, volume, volume_dispositionField, 
			  org_openafs_jafs_Volume_VOLUME_NO_SERVICE);
      break;
  case VOS_OFFLINE:
      (*env)->SetIntField(env, volume, volume_dispositionField, 
			  org_openafs_jafs_Volume_VOLUME_OFFLINE);
      break;
  case VOS_ONLINE:
      (*env)->SetIntField(env, volume, volume_dispositionField, 
			  org_openafs_jafs_Volume_VOLUME_ONLINE);
      break;
  case VOS_DISK_FULL:
      (*env)->SetIntField(env, volume, volume_dispositionField, 
			  org_openafs_jafs_Volume_VOLUME_DISK_FULL);
      break;
  case VOS_OVER_QUOTA:
      (*env)->SetIntField(env, volume, volume_dispositionField, 
			  org_openafs_jafs_Volume_VOLUME_OVER_QUOTA);
      break;
  case VOS_BUSY:
      (*env)->SetIntField(env, volume, volume_dispositionField, 
			  org_openafs_jafs_Volume_VOLUME_BUSY);
      break;
  case VOS_MOVED:
      (*env)->SetIntField(env, volume, volume_dispositionField, 
			  org_openafs_jafs_Volume_VOLUME_MOVED);
      break;
  default:
      throwAFSException( env, volEntry.volumeDisposition );
  }

  // set type variable
  switch( volEntry.type ) {
  case VOS_READ_WRITE_VOLUME:
      (*env)->SetIntField(env, volume, volume_typeField, 
			  org_openafs_jafs_Volume_VOLUME_TYPE_READ_WRITE);
      break;
  case VOS_READ_ONLY_VOLUME:
      (*env)->SetIntField(env, volume, volume_typeField, 
			  org_openafs_jafs_Volume_VOLUME_TYPE_READ_ONLY);
      break;
  case VOS_BACKUP_VOLUME:
      (*env)->SetIntField(env, volume, volume_typeField, 
			  org_openafs_jafs_Volume_VOLUME_TYPE_BACKUP);
      break;
  default:
      throwAFSException( env, volEntry.type );
  }

}

/**
 * Fills in the information fields of the provided Volume.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the volume belongs
 * serverHandle  the vos handle of the server on which the volume 
 *                      resides
 * partition   the numeric id of the partition on which the volume 
 *                    resides
 * volId  the numeric id of the volume for which to get the info
 * jvolumeObject   the Volume object in which to fill in 
 *                        the information
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Volume_getVolumeInfo (JNIEnv *env, jclass cls, 
					       jlong cellHandle, 
					       jlong serverHandle, 
					       jint partition, jint volID, 
					       jobject jvolumeObject) {

  afs_status_t ast;
  vos_volumeEntry_t volEntry;

  // get the volume entry
  if ( !vos_VolumeGet( (void *) cellHandle, (void *) serverHandle, NULL, 
		       (unsigned int) partition, (unsigned int) volID, 
		       &volEntry, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

  fillVolumeInfo( env, jvolumeObject, volEntry );

}

/**
 * Creates a volume on a particular partition.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell in which to create the volume
 * serverHandle  the vos handle of the server on which to create 
 *                      the volume
 * partition   the numeric id of the partition on which to create 
 *                    the volume
 * jvolName   the name of the volume to create
 * quota    the amount of space (in KB) to set as this volume's quota
 * returns the numeric ID assigned to the volume
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Volume_create (JNIEnv *env, jclass cls, 
					jlong cellHandle, jlong serverHandle, 
					jint partition, jstring jvolName, 
					jint quota) {

  afs_status_t ast;
  const char *volName;
  int id;

  if( jvolName != NULL ) {
    volName = (*env)->GetStringUTFChars(env, jvolName, 0);
    if( !volName ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    volName = NULL;
  }

  if( !vos_VolumeCreate( (void *) cellHandle, (void *) serverHandle, NULL, 
			 (unsigned int) partition, volName, 
			 (unsigned int) quota, &id, &ast ) ) {
    if( volName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jvolName, volName);
    }
    throwAFSException( env, ast );
    return;
  }

  if( volName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jvolName, volName);
  }
  return (jint) id;

}

/**
 * Deletes a volume from a particular partition.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell in which to delete the volume
 * serverHandle  the vos handle of the server from which to delete 
 *                      the volume
 * partition   the numeric id of the partition from which to delete 
 *                    the volume
 * volId   the numeric id of the volume to delete
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Volume_delete (JNIEnv *env, jclass cls, 
					jlong cellHandle, jlong serverHandle, 
					jint partition, jint volID) {

  afs_status_t ast;

  if( !vos_VolumeDelete( (void *) cellHandle, (void *) serverHandle, NULL, 
			 (unsigned int) partition, 
			 (unsigned int) volID, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Creates a backup volume for the specified regular volume.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the volume belongs
 * volId  the numeric id of the volume for which to create a backup 
 *               volume
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Volume_createBackupVolume (JNIEnv *env, jclass cls, 
						    jlong cellHandle, 
						    jint volID) {

  afs_status_t ast;

  if( !vos_BackupVolumeCreate( (void *) cellHandle, NULL, 
			       (unsigned int) volID, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Creates a read-only volume for the specified regular volume.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the volume belongs
 * serverHandle  the vos handle of the server on which the read-only 
 *                      volume is to reside 
 * partition   the numeric id of the partition on which the read-only 
 *             volume is to reside 
 * volId  the numeric id of the volume for which to 
 *               create a read-only volume
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Volume_createReadOnlyVolume (JNIEnv *env, jclass cls, 
						      jlong cellHandle, 
						      jlong serverHandle, 
						      jint partition, 
						      jint volID) {

  afs_status_t ast;

  if( !vos_VLDBReadOnlySiteCreate( (void *) cellHandle, (void *) serverHandle, 
				   NULL, (unsigned int) partition, 
				   (unsigned int) volID, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Deletes a read-only volume for the specified regular volume.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the volume belongs
 * serverHandle  the vos handle of the server on which the read-only 
 *                      volume residea 
 * partition   the numeric id of the partition on which the read-only
 *                     volume resides 
 * volId  the numeric read-write id of the volume for which to 
 *               delete the read-only volume
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Volume_deleteReadOnlyVolume (JNIEnv *env, jclass cls, 
						      jlong cellHandle, 
						      jlong serverHandle, 
						      jint partition, 
						      jint volID) {

  afs_status_t ast;

  if( !vos_VLDBReadOnlySiteDelete( (void *) cellHandle, (void *) serverHandle, 
				   NULL, (unsigned int) partition, 
				   (unsigned int) volID, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Changes the quota of the specified volume.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the volume belongs
 * serverHandle  the vos handle of the server on which the volume 
 *                      resides
 * partition   the numeric id of the partition on which the volume 
 *                    resides
 * volId  the numeric id of the volume for which to change the quota
 * newQuota    the new quota (in KB) to assign the volume
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Volume_changeQuota (JNIEnv *env, jclass cls, 
					     jlong cellHandle, 
					     jlong serverHandle, 
					     jint partition, jint volID, 
					     jint newQuota) {

  afs_status_t ast;

  if( !vos_VolumeQuotaChange( (void *) cellHandle, (void *) serverHandle, 
			      NULL, (unsigned int) partition, 
			      (unsigned int) volID, (unsigned int) newQuota, 
			      &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Move the specified volume to a different site.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the volume belongs
 * fromServerHandle  the vos handle of the server on which the volume
 *                          currently resides
 * fromPartition   the numeric id of the partition on which the volume
 *                        currently resides
 * toServerHandle  the vos handle of the server to which the volume 
 *                        should be moved
 * toPartition   the numeric id of the partition to which the volume 
 *                      should be moved
 * volId  the numeric id of the volume to move
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Volume_move
  (JNIEnv *env, jclass cls, jlong cellHandle, jlong fromServerHandle, 
   jint fromPartition, jlong toServerHandle, jint toPartition, jint volID)
{
  afs_status_t ast;

  if( !vos_VolumeMove( (void *) cellHandle, NULL, (unsigned int) volID, 
		       (void *) fromServerHandle, (unsigned int) fromPartition,
		       (void *) toServerHandle, (unsigned int) toPartition, 
		       &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Releases the specified volume that has readonly volume sites.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the volume belongs
 * volId  the numeric id of the volume to release
 * forceComplete  whether or not to force a complete release
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Volume_release
  (JNIEnv *env, jclass cls, jlong cellHandle, jint volID, jboolean forceComplete)
{
  afs_status_t ast;
  vos_force_t force;

  if( forceComplete ) {
    force = VOS_FORCE;
  } else {
    force = VOS_NORMAL;
  }

  if( !vos_VolumeRelease( (void *) cellHandle, NULL, (unsigned int) volID,
                          force, &ast )) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Dumps the specified volume to a file.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the volume belongs
 * serverHandle  the vos handle of the server on which the volume 
 *                      resides
 * partition   the numeric id of the partition on which the 
 *                    volume resides
 * volId  the numeric id of the volume to dump
 * startTime   files with a modification time >= to this time will 
 *                    be dumped
 * jdumpFile   the full path of the file to which to dump
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Volume_dump (JNIEnv *env, jclass cls, 
				      jlong cellHandle, jlong serverHandle, 
				      jint partition, jint volID, 
				      jint startTime, jstring jdumpFile) {

  afs_status_t ast;
  const char *dumpFile;

  if( jdumpFile != NULL ) {
    dumpFile = (*env)->GetStringUTFChars(env, jdumpFile, 0);
    if( !dumpFile ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    dumpFile = NULL;
  }

  if( !vos_VolumeDump( (void *) cellHandle, (void *) serverHandle, NULL, 
		       (unsigned int *) &partition, (unsigned int) volID, 
		       (unsigned int) startTime, dumpFile, &ast ) ) {
    if( dumpFile != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jdumpFile, dumpFile);
    }
    throwAFSException( env, ast );
    return;
  }

  if( dumpFile != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jdumpFile, dumpFile);
  }

}

/**
 * Restores the specified volume from a dump file.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the volume belongs
 * serverHandle  the vos handle of the server on which the volume is 
 *                      to reside
 * partition   the numeric id of the partition on which the volume is
 *                    to reside
 * volId  the numeric id to assign the restored volume (can be 0)
 * jvolName  the name of the volume to restore as
 * jdumpFile   the full path of the dump file from which to restore
 * incremental  if true, restores an incremental dump over an existing
 *                     volume (server and partition values must correctly 
 *                     indicate the current position of the existing volume),
 *                     otherwise restores a full dump
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Volume_restore (JNIEnv *env, jclass cls, 
					 jlong cellHandle, jlong serverHandle, 
					 jint partition, jint volID, 
					 jstring jvolName, jstring jdumpFile, 
					 jboolean incremental) {

  afs_status_t ast;
  const char *volName;
  const char *dumpFile;
  int *volumeIDp;
  vos_volumeRestoreType_t vrt;

  if( jvolName != NULL ) {
    volName = (*env)->GetStringUTFChars(env, jvolName, 0);
    if( !volName ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    volName = NULL;
  }

  if( jdumpFile != NULL ) {
    dumpFile = (*env)->GetStringUTFChars(env, jdumpFile, 0);
    if( !dumpFile ) {
      if( volName != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jvolName, volName);
      }
      throwAFSException( env, JAFSADMNOMEM );
      return;    
    }
  } else {
    dumpFile = NULL;
  }

  if( volID == 0 ) {
    volumeIDp = NULL;
  } else {
    volumeIDp = (int *) &volID;
  }
  
  if( incremental ) {
    vrt = VOS_RESTORE_INCREMENTAL;
  } else {
    vrt = VOS_RESTORE_FULL;
  }

  if( !vos_VolumeRestore( (void *) cellHandle, (void *) serverHandle, NULL, 
			  (unsigned int) partition, (unsigned int *) volumeIDp,
			  volName, dumpFile, vrt, &ast ) ) {
    if( volName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jvolName, volName);
    }
    if( dumpFile != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jdumpFile, dumpFile);
    }
    throwAFSException( env, ast );
    return;
  }

  if( dumpFile != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jdumpFile, dumpFile);
  }
  if( volName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jvolName, volName);
  }

}

/**
 * Renames the specified read-write volume.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the volume belongs
 * volId  the numeric id of the read-write volume to rename
 * jnewName  the new name for the volume
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Volume_rename (JNIEnv *env, jclass cls, 
					jlong cellHandle, jint volID, 
					jstring jnewName) {

  afs_status_t ast;
  const char *newName;

  if( jnewName != NULL ) {
    newName = (*env)->GetStringUTFChars(env, jnewName, 0);
    if( !newName ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    newName = NULL;
  }

  if( !vos_VolumeRename( (void *) cellHandle, NULL, (unsigned int) volID, 
			 newName, &ast ) ) {
    if( newName != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jnewName, newName);
    }
    throwAFSException( env, ast );
    return;
  }

  if( newName != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jnewName, newName);
  }

}

/**
 * "Mounts" the specified volume, bringing it online.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the vos handle of the server on which the volume 
 *                      resides
 * partition   the numeric id of the partition on which the volume 
 *                    resides
 * volId  the numeric id of the volume to bring online
 * sleepTime  ?  (not sure what this is yet, possibly a time to wait 
 *                      before brining it online)
 * offline   ?  (not sure what this is either, probably the current 
 *                     status of the volume -- busy or offline)
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Volume_mount (JNIEnv *env, jclass cls, 
				       jlong serverHandle, jint partition, 
				       jint volID, jint sleepTime, 
				       jboolean offline) {

  afs_status_t ast;
  vos_volumeOnlineType_t volumeStatus;

  if( offline ) {
    volumeStatus = VOS_ONLINE_OFFLINE;
  } else {
    volumeStatus = VOS_ONLINE_BUSY;
  }

  if( !vos_VolumeOnline( (void *) serverHandle, NULL, (unsigned int) partition,
			 (unsigned int) volID, (unsigned int) sleepTime, 
			 volumeStatus, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * "Unmounts" the specified volume, bringing it offline.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the vos handle of the server on which the volume 
 *                      resides
 * partition   the numeric id of the partition on which the volume 
 *                    resides
 * volId  the numeric id of the volume to bring offline
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Volume_unmount (JNIEnv *env, jclass cls, 
					 jlong serverHandle, jint partition, 
					 jint volID) {

  afs_status_t ast;

  if( !vos_VolumeOffline( (void *) serverHandle, NULL, 
			  (unsigned int) partition, (unsigned int) volID, 
			  &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Locks the VLDB entry specified volume
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle  the handle of the cell on which the volume resides
 * volId  the numeric id of the volume to lock
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Volume_lock (JNIEnv *env, jclass cls, 
				      jlong cellHandle, jint volID ) {

  afs_status_t ast;

  if( !vos_VLDBEntryLock( (void *) cellHandle, NULL, (unsigned int) volID, 
			  &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Unlocks the VLDB entry of the specified volume
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle  the handle of the cell on which the volume resides
 * volId  the numeric id of the volume to unlock
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Volume_unlock (JNIEnv *env, jclass cls, 
					jlong cellHandle, jint volID) {

  afs_status_t ast;

  if( !vos_VLDBEntryUnlock( (void *) cellHandle, NULL, (unsigned int) volID, 
			    &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Translates a volume name into a volume id
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the volume belongs
 * jname  the name of the volume in question, cannot end in backup or
 *              readonly
 * type  the type of volume: read-write, read-only, or backup.  
 *              Acceptable values are:
 *              org_openafs_jafs_Volume_VOLUME_TYPE_READ_WRITE
 *              org_openafs_jafs_Volume_VOLUME_TYPE_READ_ONLY
 *              org_openafs_jafs_Volume_VOLUME_TYPE_BACKUP
 * returns   the id of the volume in question
 */
JNIEXPORT jint JNICALL 
Java_org_openafs_jafs_Volume_translateNameToID (JNIEnv *env, jclass cls, 
						   jlong cellHandle, 
						   jstring jname, jint type) {

  afs_status_t ast;
  const char *name;
  vos_vldbEntry_t vldbEntry;

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
  if( !vos_VLDBGet( (void *) cellHandle, NULL, NULL, name, 
		    &vldbEntry, &ast ) ) {
    if( name != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jname, name);
    }
    throwAFSException( env, ast );
    return -1;
  } 

  if( name != NULL ) {
    (*env)->ReleaseStringUTFChars(env, jname, name);
  }

  if( type == org_openafs_jafs_Volume_VOLUME_TYPE_READ_WRITE ) {
    return vldbEntry.volumeId[VOS_READ_WRITE_VOLUME];
  } else if( type == org_openafs_jafs_Volume_VOLUME_TYPE_READ_ONLY ) {
    return vldbEntry.volumeId[VOS_READ_ONLY_VOLUME];
  } else {
    return vldbEntry.volumeId[VOS_BACKUP_VOLUME];
  }

}


// reclaim global memory being used by this portion
JNIEXPORT void JNICALL
Java_org_openafs_jafs_Volume_reclaimVolumeMemory (JNIEnv *env, jclass cls) {

  if( volumeCls ) {
      (*env)->DeleteGlobalRef(env, volumeCls);
      volumeCls = 0;
  }

}























