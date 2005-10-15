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

#ifdef DMALLOC
#include "dmalloc.h"
#endif

extern int errno;

#ifndef LIBJUAFS
// user class and fields //
jclass userCls = 0;
jfieldID user_ptsField = 0;
jfieldID user_kasField = 0;
jfieldID user_nameField = 0;
//jfieldID user_cellHandleField = 0;
jfieldID user_cachedInfoField = 0;
//pts fields
jfieldID user_nameUidField = 0;
jfieldID user_ownerUidField = 0;
jfieldID user_creatorUidField = 0;
jfieldID user_listStatusField = 0;
jfieldID user_listGroupsOwnedField = 0;
jfieldID user_listMembershipField = 0;
jfieldID user_groupCreationQuotaField = 0;
jfieldID user_groupMembershipCountField = 0;
jfieldID user_ownerField = 0;
jfieldID user_creatorField = 0;
// kas fields
jfieldID user_adminSettingField = 0;
jfieldID user_tgsSettingField = 0;
jfieldID user_encSettingField = 0;
jfieldID user_cpwSettingField = 0;
jfieldID user_rpwSettingField = 0;
jfieldID user_userExpirationField = 0;
jfieldID user_lastModTimeField = 0;
jfieldID user_lastModNameField = 0;
jfieldID user_lastChangePasswordTimeField = 0;
jfieldID user_maxTicketLifetimeField = 0;
jfieldID user_keyVersionField = 0;
jfieldID user_encryptionKeyField = 0;
jfieldID user_keyCheckSumField = 0;
jfieldID user_daysToPasswordExpireField = 0;
jfieldID user_failLoginCountField = 0;
jfieldID user_lockTimeField = 0;
jfieldID user_lockedUntilField = 0;

// group class and fields //
jclass groupCls = 0;
jfieldID group_nameField = 0;
//jfieldID group_cellHandleField = 0;
jfieldID group_cachedInfoField = 0;
jfieldID group_nameUidField = 0;
jfieldID group_ownerUidField = 0;
jfieldID group_creatorUidField = 0;
jfieldID group_listStatusField = 0;
jfieldID group_listGroupsOwnedField = 0;
jfieldID group_listMembershipField = 0;
jfieldID group_listAddField = 0;
jfieldID group_listDeleteField = 0;
jfieldID group_membershipCountField = 0;
jfieldID group_ownerField = 0;
jfieldID group_creatorField = 0;

// server class and fields //
jclass serverCls = 0;
jfieldID server_nameField = 0;
//jfieldID server_cellHandleField = 0;
jfieldID server_cachedInfoField = 0;
jfieldID server_databaseField = 0;
jfieldID server_fileServerField = 0;
jfieldID server_badDatabaseField = 0;
jfieldID server_badFileServerField = 0;
jfieldID server_IPAddressField = 0;

// executable time class and fields //
jclass exectimeCls = 0;
jfieldID exectime_HourField = 0;
jfieldID exectime_MinField = 0;
jfieldID exectime_SecField = 0;
jfieldID exectime_DayField = 0;
jfieldID exectime_NowField = 0;
jfieldID exectime_NeverField = 0;

// partition class and fields //
jclass partitionCls = 0;
jfieldID partition_nameField = 0;
jfieldID partition_cachedInfoField = 0;
jfieldID partition_idField = 0;
jfieldID partition_deviceNameField = 0;
jfieldID partition_lockFileDescriptorField = 0;
jfieldID partition_totalSpaceField = 0;
jfieldID partition_totalFreeSpaceField = 0;

// volume class and fields //
jclass volumeCls = 0;
jfieldID volume_nameField = 0;
jfieldID volume_cachedInfoField = 0;
jfieldID volume_idField = 0;
jfieldID volume_readWriteIdField = 0;
jfieldID volume_readOnlyIdField = 0;
jfieldID volume_backupIdField = 0;
jfieldID volume_creationDateField = 0;
jfieldID volume_lastAccessDateField = 0;
jfieldID volume_lastUpdateDateField = 0;
jfieldID volume_lastBackupDateField = 0;
jfieldID volume_copyCreationDateField = 0;
jfieldID volume_accessesSinceMidnightField = 0;
jfieldID volume_fileCountField = 0;
jfieldID volume_maxQuotaField = 0;
jfieldID volume_currentSizeField = 0;
jfieldID volume_statusField = 0;
jfieldID volume_dispositionField = 0;
jfieldID volume_typeField = 0;

// key class and fields //
jclass keyCls = 0;
jfieldID key_cachedInfoField = 0;
jfieldID key_versionField = 0;
jfieldID key_encryptionKeyField = 0;
jfieldID key_lastModDateField = 0;
jfieldID key_lastModMsField = 0;
jfieldID key_checkSumField = 0;

// process class and fields //
jclass processCls = 0;
jfieldID process_cachedInfoField = 0;
jfieldID process_nameField = 0;
//jfieldID process_serverHandleField = 0;
jfieldID process_typeField = 0;
jfieldID process_stateField = 0;
jfieldID process_goalField = 0;
jfieldID process_startTimeField = 0;
jfieldID process_numberStartsField = 0;
jfieldID process_exitTimeField = 0;
jfieldID process_exitErrorTimeField = 0;
jfieldID process_errorCodeField = 0;
jfieldID process_errorSignalField = 0;
jfieldID process_stateOkField = 0;
jfieldID process_stateTooManyErrorsField = 0;
jfieldID process_stateBadFileAccessField = 0;
#endif /* !LIBJUAFS */

/**
 * Throws an exception up to the Java layer, using ast as the error code
 * for the exception.  See Exceptions.h for the available
 * exceptions.
 */
void throwException
  (JNIEnv *env, jclass *excCls, char *excClsName, jmethodID *initID, int code)
{
  jobject exc;
  if( *excCls == 0 ) {
    *excCls = (*env)->NewGlobalRef(env, (*env)->FindClass(env, excClsName ));
    if( !*excCls ) {
      fprintf(stderr, "ERROR: Internal::throwException()\n Cannot find class: %s\n", excClsName);
	return;
    }
    *initID = (*env)->GetMethodID( env, *excCls, "<init>", "(I)V" );
    if( !*initID ) {
      fprintf(stderr, "ERROR: Internal::throwException()\n Cannot find construction method: %s\n",
	      excClsName);
	return;
    }
  }
  
  exc = (*env)->NewObject( env, *excCls, *initID, code );
  if( !exc ) {
    fprintf(stderr, "ERROR: Internal::throwException()\n Cannot construct new exception object: %s\n",
                     excClsName);
    return;
  }
  (*env)->Throw(env, exc);
}

/**
 * Throws an exception up to the Java layer, constructing it with msg.
 * This function should only be used when a valid AFS error number/code
 * is unavailable and it is necessary to interrupt the Java call with an
 * exception. See Exceptions.h for the available exceptions.
 */
void throwMessageException( JNIEnv *env, char *msg )
{
  jclass excCls = (*env)->FindClass(env, afsExceptionName);
  if(excCls == 0) {
    fprintf(stderr, "ERROR: Internal::throwMessageException()\n Cannot find class: %s\n", afsExceptionName);
    return;
  }
  (*env)->ThrowNew(env, excCls, msg);
}

/**
 * Throws an exception up to the Java layer, using ast as the error code
 * for the exception.  See Exceptions.h for the available
 * exceptions.
 */
void throwAFSException( JNIEnv *env, int code )
{
  jclass afsExceptionCls;
  jmethodID afsExceptionInit;
  jthrowable exc;

  afsExceptionCls = (*env)->FindClass(env, afsExceptionName);
  if( !afsExceptionCls ) {
    fprintf(stderr, "ERROR: Internal::throwAFSException()\n Cannot find class: %s\n", afsExceptionName);
    return;
  }

  afsExceptionInit = (*env)->GetMethodID( env, afsExceptionCls, 
                             "<init>", "(I)V" );
  if( !afsExceptionInit ) {
    fprintf(stderr, "ERROR: Internal::throwAFSException()\n Cannot find construction method: %s\n",
                     afsExceptionName);
    return;
  }
  
  exc = (*env)->NewObject( env, afsExceptionCls, afsExceptionInit, code );

  if( !exc ) {
    fprintf(stderr, "ERROR: Internal::throwAFSException()\n Cannot construct new exception object: %s\n",
                     afsExceptionName);
    return;
  }
  (*env)->Throw(env, exc);
}

/**
 * Throws an exception up to the Java layer, using ast as the error code
 * for the exception.  See Exceptions.h for the available
 * exceptions.
 */
void throwAFSFileException( JNIEnv *env, int code, char *msg )
{
  jclass afsFileExceptionCls;
  jmethodID afsFileExceptionInit;
  jthrowable exc;

  afsFileExceptionCls = (*env)->FindClass(env, afsFileExceptionName);
  if( !afsFileExceptionCls ) {
    fprintf(stderr, "ERROR: Internal::throwAFSFileException()\n Cannot find class: %s\n", afsFileExceptionName);
    return;
  }

  afsFileExceptionInit = (*env)->GetMethodID( env, afsFileExceptionCls, 
                         "<init>", "(Ljava/lang/String;I)V" );
  if( !afsFileExceptionInit ) {
    fprintf(stderr, "ERROR: Internal::throwAFSFileException()\n Cannot find construction method: %s\n",
                     afsFileExceptionName);
    return;
  }
  
  exc = (*env)->NewObject( env, afsFileExceptionCls,
                           afsFileExceptionInit, msg, code );
  if( !exc ) {
    fprintf(stderr, "ERROR: Internal::throwAFSFileException()\n Cannot construct new exception object: %s\n",
                     afsFileExceptionName);
    return;
  }
  (*env)->Throw(env, exc);
}

/**
 * Throws an exception up to the Java layer, using ast as the error code
 * for the exception.  See Exceptions.h for the available
 * exceptions.
 */
void throwAFSSecurityException( JNIEnv *env, int code )
{
  jclass afsSecurityExceptionCls;
  jmethodID afsSecurityExceptionInit;
  jthrowable exc;

  afsSecurityExceptionCls = (*env)->FindClass(env, afsSecurityExceptionName);
  if( !afsSecurityExceptionCls ) {
    fprintf(stderr, "ERROR: Internal::throwAFSSecurityException()\n Cannot find class: %s\n", afsSecurityExceptionName);
    return;
  }

  afsSecurityExceptionInit = (*env)->GetMethodID( env, afsSecurityExceptionCls, 
                             "<init>", "(I)V" );
  if( !afsSecurityExceptionInit ) {
    fprintf(stderr, "ERROR: Internal::throwAFSSecurityException()\n Cannot find construction method: %s\n",
                     afsSecurityExceptionName);
    return;
  }
  
  exc = (*env)->NewObject( env, afsSecurityExceptionCls,
                           afsSecurityExceptionInit, code );
  if( !exc ) {
    fprintf(stderr, "ERROR: Internal::throwAFSSecurityException()\n Cannot construct new exception object: %s\n",
                     afsSecurityExceptionName);
    return;
  }
  (*env)->Throw(env, exc);
}

int setError(JNIEnv *env, jobject *obj, int code)
{
  jfieldID fid;
  jclass cls = (*env)->GetObjectClass(env, *obj);
  if (cls != NULL) {
    fid = (*env)->GetFieldID(env, cls, "errno", "I");
    if (fid)
    {
      (*env)->SetIntField(env, *obj, fid, code);
      return 0;
    }
  }
  return -1;
}

#ifdef LIBJUAFS

/**
 * Opens an AFS file, with the specified name, using the specified flags
 * with in the specified mode (permission mode).
 * 
 *  env		the Java environment
 *  fileNameUTF	name of file to be opened
 *  flags		open mode: O_CREAT, O_APPEND
 *  mode		UNIX permission mode mask
 *  err		error variable
 *
 * @returns		file descriptor
 */
int openAFSFile
  (JNIEnv *env, jstring fileNameUTF, int flags, int mode, int *err)
{
    char *fileName;
    int fd = -1;

    *err = 0;
    errno = 0;
    fileName=(char*) (*env)->GetStringUTFChars(env, fileNameUTF, 0);
    if(fileName == NULL) {
      fprintf(stderr, "Internal::openAFSFile(): failed to get fileName\n");
      *err = -1;
      return fd;
    }
    fd  = uafs_open(fileName, flags, mode);
    *err = errno;
    if (errno != 0) {
      fprintf(stderr, "Internal::openAFSFile(): errno=%d\n", errno);
      fprintf(stderr, "Internal::openAFSFile(): fd=%d\n", fd);
    }
    (*env)->ReleaseStringUTFChars(env, fileNameUTF, fileName);
    if (fd < 0) {
      fprintf(stderr, "Internal::openAFSFile(): failed to open fileName\n");
      fprintf(stderr, "Internal::openAFSFile(): fd=%d\n", fd);
      return -1;
    }
    return fd;
}

int readCacheParms(char *afsMountPoint, char *afsConfDir, char *afsCacheDir,
                   int *cacheBlocks, int *cacheFiles, int *cacheStatEntries,
                   int *dCacheSize, int *vCacheSize, int *chunkSize,
                   int *closeSynch, int *debug, int *nDaemons, int *cacheFlags,
                   char *logFile)
{
  FILE *f;
  char line[100];
  char *p;
  int len1, len2, n;
  char cacheConfigFile[100];
  
  p = (char *)getenv("LIBJAFS_CACHE_CONFIG");
  if (p) {
    strcpy(cacheConfigFile, p);
  } else {
    strcpy(cacheConfigFile, "/usr/afswsp/etc/CacheConfig");
  }

  f = fopen(cacheConfigFile, "r");
  if (!f) {
    fprintf(stderr, "Could not open cache config file: %s\n",
            cacheConfigFile);
    return -1;
  }

  while (1) {
    fgets(line, 100, f);
    if (feof(f)) break;
    p = (char *)strchr(line, '\n');
    if (p) *p = '\0';
    if (strncmp(line, "#", 1) == 0) continue;  /* comment */

    p = (char *)strchr(line, ' ');
    if (!p) continue;
    len1 = p - line;
    p++; len2 = strlen(p);

    if (strncmp(line, "MountPoint", len1) == 0)
       strcpy(afsMountPoint, p);
    else if (strncmp(line, "ConfDir", len1) == 0)
       strcpy(afsConfDir, p);
    else if (strncmp(line, "CacheDir", len1) == 0)
       strcpy(afsCacheDir, p);
    else if (strncmp(line, "CacheBlocks", len1) == 0)
       *cacheBlocks = atoi(p);
    else if (strncmp(line, "CacheFiles", len1) == 0)
       *cacheFiles = atoi(p);
    else if (strncmp(line, "CacheStatEntries", len1) == 0)
       *cacheStatEntries = atoi(p);
    else if (strncmp(line, "DCacheSize", len1) == 0)
       *dCacheSize = atoi(p);
    else if (strncmp(line, "VCacheSize", len1) == 0)
       *vCacheSize = atoi(p);
    else if (strncmp(line, "ChunkSize", len1) == 0)
       *chunkSize = atoi(p);
    else if (strncmp(line, "CloseSynch", len1) == 0)
       *closeSynch = atoi(p);
    else if (strncmp(line, "Debug", len1) == 0)
       *debug = atoi(p);
    else if (strncmp(line, "NDaemons", len1) == 0)
       *nDaemons = atoi(p);
    else if (strncmp(line, "CacheFlags", len1) == 0)
       *cacheFlags = atoi(p);
    else if (strncmp(line, "LogFile", len1) == 0)
       strcpy(logFile, p);
  }
  return 0;
}

int setString(JNIEnv *env, jobject *obj, char *field, char *string)
{
  jclass cls;
  jstring jstr;
  jfieldID fid;

  cls = (*env)->GetObjectClass(env, *obj);
  /*fprintf(stderr, "setString: env=0x%x, obj=0x%x, cls=0x%x\n", env, obj, cls);*/
  if (cls != NULL) {
    fid = (*env)->GetFieldID(env, cls, field, "Ljava/lang/String;");
    /*fprintf(stderr, "setString: field=%s, fid=0x%x\n", field, fid);*/
    if (fid) {
      jstr = (*env)->NewStringUTF(env, (string));
      /*fprintf(stderr, "jstr = 0x%x\n", jstr);*/
      (*env)->SetObjectField(env, *obj, fid, jstr);
      return 0;
    }
  }
  return -1;
}

#else

/**
 * Makes a kas identity given the full name of a kas user.  If the
 * name contains a period, everything after the first period is
 * considered to be the instance of that name, otherwise
 * the instance is the empty string.  The memory for who 
 * that's passed in should be fully allocated in advance.
 */
void internal_makeKasIdentity( const char *fullName, 
				       kas_identity_p who ) {

  char *period;

  if( (period = (char *) strchr( fullName, '.' )) != NULL ) {
    strncpy( who->principal, fullName, period - fullName );
    who->principal[period - fullName] = '\0';
    strncpy( who->instance, period + 1, 
	     strlen(fullName) - (period - fullName) );
  } else {
    strcpy( who->principal, fullName);
    strcpy( who->instance, "" );
  }

}

/**
 * Given a Java environment and an instance of a user, gets the object and
 * field information for the user object from the Java environment.
 */
void internal_getUserClass( JNIEnv *env, jobject user ) {
  if( userCls == 0 ) {
    userCls = (*env)->NewGlobalRef( env, (*env)->GetObjectClass(env, user) );
    if( !userCls ) {
	throwAFSException( env, JAFSADMCLASSNOTFOUND );
	return;
    }
    user_ptsField = (*env)->GetFieldID( env, userCls, "pts", "Z" );
    user_kasField = (*env)->GetFieldID( env, userCls, "kas", "Z" );
    user_nameField = (*env)->GetFieldID( env, userCls, "name", 
					 "Ljava/lang/String;" );
    user_cachedInfoField = (*env)->GetFieldID( env, userCls, "cachedInfo", 
					       "Z" );
    // pts fields
    user_nameUidField = (*env)->GetFieldID( env, userCls, "nameUID", "I" );
    user_ownerUidField = (*env)->GetFieldID( env, userCls, "ownerUID", "I" );
    user_creatorUidField = (*env)->GetFieldID( env, userCls, "creatorUID", 
					       "I" );
    user_listStatusField = (*env)->GetFieldID( env, userCls, "listStatus", 
					       "I" );
    user_listGroupsOwnedField = (*env)->GetFieldID( env, userCls, 
						    "listGroupsOwned", "I" );
    user_listMembershipField = (*env)->GetFieldID( env, userCls, 
						   "listMembership", "I" );
    user_groupCreationQuotaField = (*env)->GetFieldID( env, userCls, 
						       "groupCreationQuota", 
						       "I" );
    user_groupMembershipCountField = (*env)->GetFieldID( env, userCls, 
							"groupMembershipCount",
							 "I" );
    user_ownerField = (*env)->GetFieldID( env, userCls, "owner", 
					  "Ljava/lang/String;" );
    user_creatorField = (*env)->GetFieldID( env, userCls, "creator", 
					    "Ljava/lang/String;" );
    // kas fields
    user_adminSettingField = (*env)->GetFieldID( env, userCls, "adminSetting",
						 "I" );
    user_tgsSettingField = (*env)->GetFieldID( env, userCls, "tgsSetting", 
					       "I" );
    user_encSettingField = (*env)->GetFieldID( env, userCls, "encSetting", 
					       "I" );
    user_cpwSettingField = (*env)->GetFieldID( env, userCls, "cpwSetting", 
					       "I" );
    user_rpwSettingField = (*env)->GetFieldID( env, userCls, "rpwSetting", 
					       "I" );
    user_userExpirationField = (*env)->GetFieldID( env, userCls, 
						   "userExpiration", "I" );
    user_lastModTimeField = (*env)->GetFieldID( env, userCls, "lastModTime", 
						"I" );
    user_lastModNameField = (*env)->GetFieldID( env, userCls, "lastModName", 
						"Ljava/lang/String;" );
    user_lastChangePasswordTimeField = (*env)->GetFieldID( env, userCls, 
						      "lastChangePasswordTime",
							   "I" );
    user_maxTicketLifetimeField = (*env)->GetFieldID( env, userCls, 
						      "maxTicketLifetime", 
						      "I" );
    user_keyVersionField = (*env)->GetFieldID( env, userCls, "keyVersion", 
					       "I" );
    user_encryptionKeyField = (*env)->GetFieldID( env, userCls, 
						  "encryptionKey", 
						  "Ljava/lang/String;" );
    user_keyCheckSumField = (*env)->GetFieldID( env, userCls, "keyCheckSum", 
						"J" );
    user_daysToPasswordExpireField = (*env)->GetFieldID( env, userCls, 
							"daysToPasswordExpire",
							 "I" );
    user_failLoginCountField = (*env)->GetFieldID( env, userCls, 
						   "failLoginCount", "I" );
    user_lockTimeField = (*env)->GetFieldID( env, userCls, "lockTime", "I" );
    user_lockedUntilField = (*env)->GetFieldID( env, userCls, "lockedUntil", 
						"I" );
    if( !user_ptsField || !user_kasField || !user_nameField || 
	!user_cachedInfoField || !user_nameUidField || !user_ownerUidField || 
	!user_creatorUidField || !user_listStatusField || 
	!user_listGroupsOwnedField || !user_listMembershipField || 
	!user_groupCreationQuotaField || !user_groupMembershipCountField || 
	!user_ownerField || !user_creatorField || !user_adminSettingField || 
	!user_tgsSettingField || !user_encSettingField || 
	!user_cpwSettingField || !user_rpwSettingField || 
	!user_userExpirationField || !user_lastModTimeField || 
	!user_lastModNameField || !user_lastChangePasswordTimeField || 
	!user_maxTicketLifetimeField || !user_keyVersionField || 
	!user_encryptionKeyField || !user_keyCheckSumField || 
	!user_daysToPasswordExpireField || !user_failLoginCountField || 
	!user_lockTimeField || !user_lockedUntilField ) {

	throwAFSException( env, JAFSADMFIELDNOTFOUND );
	return;

    }
  } 
}

/**
 * Given a Java environment and an instance of a group, gets the object and
 * field information for the group object from the Java environment.
 */
void internal_getGroupClass( JNIEnv *env, jobject group ) {
  if( groupCls == 0 ) {
    groupCls = (*env)->NewGlobalRef( env, (*env)->GetObjectClass(env, group) );
    if( !groupCls ) {
	throwAFSException( env, JAFSADMCLASSNOTFOUND );
	return;
    }
    group_nameField = (*env)->GetFieldID( env, groupCls, "name", 
					  "Ljava/lang/String;" );
    group_cachedInfoField = (*env)->GetFieldID( env, groupCls, "cachedInfo", 
						"Z" );
    group_nameUidField = (*env)->GetFieldID( env, groupCls, "nameUID", "I" );
    group_ownerUidField = (*env)->GetFieldID( env, groupCls, "ownerUID", "I" );
    group_creatorUidField = (*env)->GetFieldID( env, groupCls, "creatorUID", 
						"I" );
    group_listStatusField = (*env)->GetFieldID( env, groupCls, "listStatus", 
						"I" );
    group_listGroupsOwnedField = (*env)->GetFieldID( env, groupCls, 
						     "listGroupsOwned", "I" );
    group_listMembershipField = (*env)->GetFieldID( env, groupCls, 
						    "listMembership", "I" );
    group_listAddField = (*env)->GetFieldID( env, groupCls, "listAdd", "I" );
    group_listDeleteField = (*env)->GetFieldID( env, groupCls, "listDelete", 
						"I" );
    group_membershipCountField = (*env)->GetFieldID( env, groupCls, 
						     "membershipCount", "I" );
    group_ownerField = (*env)->GetFieldID( env, groupCls, "owner", 
					   "Ljava/lang/String;" );
    group_creatorField = (*env)->GetFieldID( env, groupCls, "creator", 
					     "Ljava/lang/String;" );
    if( !group_nameField || !group_cachedInfoField || !group_nameUidField || 
	!group_ownerUidField || !group_creatorUidField || 
	!group_listStatusField || !group_listGroupsOwnedField || 
	!group_listMembershipField || !group_listAddField || 
	!group_listDeleteField || !group_membershipCountField || 
	!group_ownerField || !group_creatorField ) {

	throwAFSException( env, JAFSADMFIELDNOTFOUND );
	return;

    }
  }
}

/**
 * Given a Java environment and an instance of a server, gets the object and
 * field information for the server object from the Java environment.
 */
void internal_getServerClass( JNIEnv *env, jobject server ) {
  if( serverCls == 0 ) {
    serverCls = (*env)->NewGlobalRef( env,
                                      (*env)->GetObjectClass(env, server) );
    if( !serverCls ) {
	throwAFSException( env, JAFSADMCLASSNOTFOUND );
	return;
    }
    server_nameField = (*env)->GetFieldID( env, serverCls, "name", 
					   "Ljava/lang/String;" );
    server_cachedInfoField = (*env)->GetFieldID( env, serverCls, "cachedInfo",
						 "Z" );
    server_databaseField = (*env)->GetFieldID( env, serverCls, "database", 
					       "Z" );
    server_fileServerField = (*env)->GetFieldID( env, serverCls, "fileServer", 
						 "Z" );
    server_badDatabaseField = (*env)->GetFieldID( env, serverCls, 
						  "badDatabase", "Z" );
    server_badFileServerField = (*env)->GetFieldID( env, serverCls, 
						    "badFileServer", "Z" );
    server_IPAddressField = (*env)->GetFieldID( env, serverCls, "ipAddresses",
						"[Ljava/lang/String;" );
    if( !server_nameField || !server_cachedInfoField || !server_databaseField 
	|| !server_fileServerField || !server_badDatabaseField || 
	!server_badFileServerField || !server_IPAddressField ) {

	throwAFSException( env, JAFSADMFIELDNOTFOUND );
	return;

    }
  }
}

/**
 * Given a Java environment and an instance of an executableTime, gets the 
 * object and field information for the executableTime object from the 
 * Java environment.
 */
void internal_getExecTimeClass( JNIEnv *env, jobject exectime ) {
  if( exectimeCls == 0 ) {
    exectimeCls = (*env)->NewGlobalRef( env, 
				       (*env)->GetObjectClass(env, exectime) );
    if( !exectimeCls ) {
	throwAFSException( env, JAFSADMCLASSNOTFOUND );
	return;
    }
    exectime_HourField  = (*env)->GetFieldID( env, exectimeCls, "hour", "S" );
    exectime_MinField   = (*env)->GetFieldID( env, exectimeCls, "minute", 
					      "S" );
    exectime_SecField   = (*env)->GetFieldID( env, exectimeCls, "second", 
					      "S" );
    exectime_DayField   = (*env)->GetFieldID( env, exectimeCls, "day", "S" );
    exectime_NowField   = (*env)->GetFieldID( env, exectimeCls, "now", "Z" );
    exectime_NeverField = (*env)->GetFieldID( env, exectimeCls, "never", "Z" );
    if( !exectime_HourField || !exectime_MinField || !exectime_SecField || 
	!exectime_DayField || !exectime_NowField || !exectime_NeverField ) {

	throwAFSException( env, JAFSADMFIELDNOTFOUND );
	return;

    }
  }
}

/**
 * Given a Java environment and an instance of a partition, gets the object and
 * field information for the partition object from the Java environment.
 */
void internal_getPartitionClass( JNIEnv *env, jobject partition ) {
  if( partitionCls == 0 ) {
    partitionCls = (*env)->NewGlobalRef( env, 
				      (*env)->GetObjectClass(env, partition) );
    if( !partitionCls ) {
	throwAFSException( env, JAFSADMCLASSNOTFOUND );
	return;
    }
    partition_nameField = (*env)->GetFieldID( env, partitionCls, "name", 
					      "Ljava/lang/String;" );
    partition_deviceNameField = (*env)->GetFieldID( env, partitionCls, 
						    "deviceName", 
						    "Ljava/lang/String;" );
    partition_idField = (*env)->GetFieldID( env, partitionCls, "id", "I" );
    partition_cachedInfoField = (*env)->GetFieldID( env, partitionCls, 
						    "cachedInfo", "Z" );
    partition_lockFileDescriptorField = (*env)->GetFieldID( env, partitionCls,
							  "lockFileDescriptor",
							    "I" );
    partition_totalSpaceField = (*env)->GetFieldID( env, partitionCls, 
						    "totalSpace", "I" );
    partition_totalFreeSpaceField = (*env)->GetFieldID( env, partitionCls, 
							"totalFreeSpace", "I");
    if( !partition_nameField || !partition_cachedInfoField || 
	!partition_idField || !partition_deviceNameField || 
	!partition_lockFileDescriptorField || !partition_totalSpaceField || 
	!partition_totalFreeSpaceField ) {

	throwAFSException( env, JAFSADMFIELDNOTFOUND );
	return;

    }
  }
}

/**
 * Given a Java environment and an instance of a volume, gets the object and
 * field information for the volume object from the Java environment.
 */
void internal_getVolumeClass( JNIEnv *env, jobject volume ) {
  if( volumeCls == 0 ) {
    volumeCls = (*env)->NewGlobalRef( env, 
				      (*env)->GetObjectClass(env, volume) );
    if( !volumeCls ) {
	throwAFSException( env, JAFSADMCLASSNOTFOUND );
	return;
    }
    volume_nameField = (*env)->GetFieldID( env, volumeCls, "name", 
					   "Ljava/lang/String;" );
    volume_cachedInfoField = (*env)->GetFieldID( env, volumeCls, "cachedInfo", 
						 "Z" );
    volume_idField = (*env)->GetFieldID( env, volumeCls, "id", "I" );
    volume_readWriteIdField = (*env)->GetFieldID( env, volumeCls, 
						  "readWriteID", "I" );
    volume_readOnlyIdField = (*env)->GetFieldID( env, volumeCls, "readOnlyID", 
						 "I" );
    volume_backupIdField = (*env)->GetFieldID( env, volumeCls, "backupID", 
					       "I" );
    volume_creationDateField = (*env)->GetFieldID( env, volumeCls, 
						   "creationDate", "J" );
    volume_lastAccessDateField = (*env)->GetFieldID( env, volumeCls, 
						     "lastAccessDate", "J" );
    volume_lastUpdateDateField = (*env)->GetFieldID( env, volumeCls, 
						     "lastUpdateDate", "J" );
    volume_lastBackupDateField = (*env)->GetFieldID( env, volumeCls, 
						     "lastBackupDate", "J" );
    volume_copyCreationDateField = (*env)->GetFieldID( env, volumeCls, 
						       "copyCreationDate", 
						       "J" );
    volume_accessesSinceMidnightField = (*env)->GetFieldID( env, volumeCls, 
						       "accessesSinceMidnight",
							    "I" );
    volume_fileCountField = (*env)->GetFieldID( env, volumeCls, "fileCount", 
						"I" );
    volume_maxQuotaField = (*env)->GetFieldID( env, volumeCls, "maxQuota", 
					       "I" );
    volume_currentSizeField = (*env)->GetFieldID( env, volumeCls, 
						  "currentSize", "I" );
    volume_statusField = (*env)->GetFieldID( env, volumeCls, "status", "I" );
    volume_dispositionField = (*env)->GetFieldID( env, volumeCls, 
						  "disposition", "I" );
    volume_typeField = (*env)->GetFieldID( env, volumeCls, "type", "I" );
    if( !volume_nameField || !volume_cachedInfoField || !volume_idField || 
	!volume_readWriteIdField || !volume_readOnlyIdField || 
	!volume_backupIdField || !volume_creationDateField || 
	!volume_lastAccessDateField || !volume_lastUpdateDateField || 
	!volume_lastBackupDateField || !volume_copyCreationDateField || 
	!volume_accessesSinceMidnightField || !volume_fileCountField || 
	!volume_maxQuotaField || !volume_currentSizeField || 
	!volume_statusField || !volume_dispositionField || 
	!volume_typeField ) {

	throwAFSException( env, JAFSADMFIELDNOTFOUND );
	return;

    }
  }
}

/**
 * Given a Java environment and an instance of a key, gets the object and
 * field information for the key object from the Java environment.
 */
void internal_getKeyClass( JNIEnv *env, jobject key ) {
  if( keyCls == 0 ) {
    keyCls = (*env)->NewGlobalRef( env, (*env)->GetObjectClass(env, key) );
    if( !keyCls ) {
	throwAFSException( env, JAFSADMCLASSNOTFOUND );
	return;
    }
    key_encryptionKeyField = (*env)->GetFieldID( env, keyCls, 
						 "encryptionKey", 
						 "Ljava/lang/String;" );
    key_cachedInfoField = (*env)->GetFieldID( env, keyCls, "cachedInfo", "Z" );
    key_versionField = (*env)->GetFieldID( env, keyCls, "version", "I" );
    key_lastModDateField = (*env)->GetFieldID( env, keyCls, "lastModDate", 
					       "I" );
    key_lastModMsField = (*env)->GetFieldID( env, keyCls, "lastModMs", "I" );
    key_checkSumField = (*env)->GetFieldID( env, keyCls, "checkSum", "J" );
    if( !key_cachedInfoField || !key_versionField || !key_encryptionKeyField 
	|| !key_lastModDateField || !key_lastModMsField || 
	!key_checkSumField ) {

	throwAFSException( env, JAFSADMFIELDNOTFOUND );
	return;

    }
  }
}

/**
 * Given a Java environment and an instance of a process, gets the object and
 * field information for the process object from the Java environment.
 */
void internal_getProcessClass( JNIEnv *env, jobject process ) {
  if( processCls == 0 ) {
    processCls = (*env)->NewGlobalRef( env, 
				       (*env)->GetObjectClass(env, process) );
    if( !processCls ) {
	throwAFSException( env, JAFSADMCLASSNOTFOUND );
	return;
    }
    process_cachedInfoField = (*env)->GetFieldID( env, processCls, 
						  "cachedInfo", "Z" );
    process_nameField = (*env)->GetFieldID( env, processCls, "name", 
					    "Ljava/lang/String;" );
    process_typeField = (*env)->GetFieldID( env, processCls, "type", "I" );
    process_stateField = (*env)->GetFieldID( env, processCls, "state", "I" );
    process_goalField = (*env)->GetFieldID( env, processCls, "goal", "I" );
    process_startTimeField = (*env)->GetFieldID( env, processCls, "startTime", 
						 "J" ); 
    process_numberStartsField = (*env)->GetFieldID( env, processCls, 
						    "numberStarts", "J" ); 
    process_exitTimeField = (*env)->GetFieldID( env, processCls, "exitTime", 
						"J" ); 
    process_exitErrorTimeField = (*env)->GetFieldID( env, processCls, 
						     "exitErrorTime", "J" ); 
    process_errorCodeField = (*env)->GetFieldID( env, processCls, "errorCode", 
						 "J" ); 
    process_errorSignalField = (*env)->GetFieldID( env, processCls, 
						   "errorSignal", "J" ); 
    process_stateOkField = (*env)->GetFieldID( env, processCls, "stateOk", 
					       "Z" ); 
    process_stateTooManyErrorsField = (*env)->GetFieldID( env, processCls, 
							  "stateTooManyErrors",
							  "Z" ); 
    process_stateBadFileAccessField = (*env)->GetFieldID( env, processCls, 
							  "stateBadFileAccess",
							  "Z" ); 
    if( !process_cachedInfoField || !process_nameField || !process_typeField 
	|| !process_stateField || !process_goalField || 
	!process_startTimeField || !process_numberStartsField || 
	!process_exitTimeField || !process_exitErrorTimeField || 
	!process_errorCodeField || !process_errorSignalField || 
	!process_stateOkField || !process_stateTooManyErrorsField || 
	!process_stateBadFileAccessField ) {

	throwAFSException( env, JAFSADMFIELDNOTFOUND );
	return;

    }
  }
}

#endif /* LIBJUAFS */


