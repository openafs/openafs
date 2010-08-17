/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_KAS_ADMIN_H
#define OPENAFS_KAS_ADMIN_H

#include <afs/param.h>
#include <afs/afs_Admin.h>
#include <time.h>
#ifdef AFS_NT40_ENV
#ifndef _MFC_VER
#include <winsock2.h>
#endif /* _MFC_VER */
#endif

#define KAS_MAX_NAME_LEN 64
#define KAS_ENCRYPTION_KEY_LEN 8
extern const int KAS_PRINCIPAL_FLAG_NORMAL;
extern const int KAS_PRINCIPAL_FLAG_FREE;
extern const int KAS_PRINCIPAL_FLAG_OLDKEYS;
extern const int KAS_PRINCIPAL_FLAG_SPECIAL;
extern const int KAS_PRINCIPAL_FLAG_ASSOC_ROOT;
extern const int KAS_PRINCIPAL_FLAG_ASSOC;
extern const int KAS_PRINCIPAL_FLAG_ADMIN;
extern const int KAS_PRINCIPAL_FLAG_NO_TGS;
extern const int KAS_PRINCIPAL_FLAG_NO_SEAL;
extern const int KAS_PRINCIPAL_FLAG_NO_CPW;
extern const int KAS_PRINCIPAL_FLAG_NEW_ASSOC;
#define KAS_MAX_SERVER_OPERATION_LEN 16
#define KAS_MAX_PRINCIPAL_LEN 256
#define KAS_KEYCACHE_DEBUG_INFO_SIZE 25

typedef struct kas_identity {
    char principal[KAS_MAX_NAME_LEN];
    char instance[KAS_MAX_NAME_LEN];
} kas_identity_t, *kas_identity_p;

typedef struct kas_encryptionKey {
    unsigned char key[KAS_ENCRYPTION_KEY_LEN];
} kas_encryptionKey_t, *kas_encryptionKey_p;

typedef enum { KAS_ADMIN, NO_KAS_ADMIN } kas_admin_t, *kas_admin_p;
typedef enum { TGS, NO_TGS } kas_tgs_t, *kas_tgs_p;
typedef enum { ENCRYPT, NO_ENCRYPT } kas_enc_t, *kas_enc_p;
typedef enum { CHANGE_PASSWORD, NO_CHANGE_PASSWORD } kas_cpw_t, *kas_cpw_p;
typedef enum { REUSE_PASSWORD, NO_REUSE_PASSWORD } kas_rpw_t, *kas_rpw_p;


typedef struct kas_principalEntry {
    kas_admin_t adminSetting;
    kas_tgs_t tgsSetting;
    kas_enc_t encSetting;
    kas_cpw_t cpwSetting;
    kas_rpw_t rpwSetting;
    unsigned int userExpiration;
    unsigned int lastModTime;
    kas_identity_t lastModPrincipal;
    unsigned int lastChangePasswordTime;
    int maxTicketLifetime;
    int keyVersion;
    kas_encryptionKey_t key;
    unsigned int keyCheckSum;
    int daysToPasswordExpire;
    int failLoginCount;
    int lockTime;
} kas_principalEntry_t, *kas_principalEntry_p;

typedef struct kas_serverProcStats {
    int requests;
    int aborts;
} kas_serverProcStats_t, *kas_serverProcStats_p;

typedef struct kas_serverStats {
    int allocations;
    int frees;
    int changePasswordRequests;
    int adminAccounts;
    int host;
    unsigned int serverStartTime;
    struct timeval userTime;
    struct timeval systemTime;
    int dataSize;
    int stackSize;
    int pageFaults;
    int hashTableUtilization;
    kas_serverProcStats_t authenticate;
    kas_serverProcStats_t changePassword;
    kas_serverProcStats_t getTicket;
    kas_serverProcStats_t createUser;
    kas_serverProcStats_t setPassword;
    kas_serverProcStats_t setFields;
    kas_serverProcStats_t deleteUser;
    kas_serverProcStats_t getEntry;
    kas_serverProcStats_t listEntry;
    kas_serverProcStats_t getStats;
    kas_serverProcStats_t getPassword;
    kas_serverProcStats_t getRandomKey;
    kas_serverProcStats_t debug;
    kas_serverProcStats_t udpAuthenticate;
    kas_serverProcStats_t udpGetTicket;
    kas_serverProcStats_t unlock;
    kas_serverProcStats_t lockStatus;
    int stringChecks;
} kas_serverStats_t, *kas_serverStats_p;

typedef struct key_keyCacheItem {
    unsigned int lastUsed;
    int keyVersionNumber;
    char primary;
    char keyCheckSum;
    char principal[KAS_MAX_NAME_LEN];
} key_keyCacheItem_t, *key_keyCacheItem_p;

typedef struct kas_serverDebugInfo {
    int host;
    unsigned int serverStartTime;
    unsigned int currentTime;
    int noAuth;
    unsigned int lastTransaction;
    char lastOperation[KAS_MAX_SERVER_OPERATION_LEN];
    char lastPrincipalAuth[KAS_MAX_PRINCIPAL_LEN];
    char lastPrincipalUDPAuth[KAS_MAX_PRINCIPAL_LEN];
    char lastPrincipalTGS[KAS_MAX_PRINCIPAL_LEN];
    char lastPrincipalUDPTGS[KAS_MAX_PRINCIPAL_LEN];
    char lastPrincipalAdmin[KAS_MAX_PRINCIPAL_LEN];
    char lastServerTGS[KAS_MAX_PRINCIPAL_LEN];
    char lastServerUDPTGS[KAS_MAX_PRINCIPAL_LEN];
    unsigned int nextAutoCheckPointWrite;
    int updatesRemainingBeforeAutoCheckPointWrite;
    unsigned int dbHeaderRead;
    int dbVersion;
    int dbFreePtr;
    int dbEOFPtr;
    int dbKvnoPtr;
    int dbSpecialKeysVersion;
    int dbHeaderLock;
    int keyCacheLock;
    int keyCacheVersion;
    int keyCacheSize;
    int keyCacheUsed;
    key_keyCacheItem_t keyCache[KAS_KEYCACHE_DEBUG_INFO_SIZE];
} kas_serverDebugInfo_t, *kas_serverDebugInfo_p;

extern int ADMINAPI kas_ServerOpen(const void *cellHandle,
				   const char **serverList,
				   void **serverHandleP, afs_status_p st);

extern int ADMINAPI kas_ServerClose(const void *serverHandle,
				    afs_status_p st);

extern int ADMINAPI kas_PrincipalCreate(const void *cellHandle,
					const void *serverHandle,
					const kas_identity_p who,
					const char *password,
					afs_status_p st);

extern int ADMINAPI kas_PrincipalDelete(const void *cellHandle,
					const void *serverHandle,
					const kas_identity_p who,
					afs_status_p st);

extern int ADMINAPI kas_PrincipalGet(const void *cellHandle,
				     const void *serverHandle,
				     const kas_identity_p who,
				     kas_principalEntry_p principal,
				     afs_status_p st);

extern int ADMINAPI kas_PrincipalGetBegin(const void *cellHandle,
					  const void *serverHandle,
					  void **iterationIdP,
					  afs_status_p st);

extern int ADMINAPI kas_PrincipalGetNext(const void *iterationId,
					 kas_identity_p who, afs_status_p st);

extern int ADMINAPI kas_PrincipalGetDone(const void *iterationIdP,
					 afs_status_p st);

extern int ADMINAPI kas_PrincipalKeySet(const void *cellHandle,
					const void *serverHandle,
					const kas_identity_p who,
					int keyVersion,
					const kas_encryptionKey_p key,
					afs_status_p st);

extern int ADMINAPI kas_PrincipalLockStatusGet(const void *cellHandle,
					       const void *serverHandle,
					       const kas_identity_p who,
					       unsigned int *lock_end_timeP,
					       afs_status_p st);

extern int ADMINAPI kas_PrincipalUnlock(const void *cellHandle,
					const void *serverHandle,
					const kas_identity_p who,
					afs_status_p st);

extern int ADMINAPI kas_PrincipalFieldsSet(const void *cellHandle,
					   const void *serverHandle,
					   const kas_identity_p who,
					   const kas_admin_p isAdmin,
					   const kas_tgs_p grantTickets,
					   const kas_enc_p canEncrypt,
					   const kas_cpw_p canChangePassword,
					   const unsigned int *expirationDate,
					   const unsigned int
					   *maxTicketLifetime, const unsigned int
					   *passwordExpires,
					   const kas_rpw_p passwordReuse,
					   const unsigned int
					   *failedPasswordAttempts, const unsigned int
					   *failedPasswordLockTime,
					   afs_status_p st);

extern int ADMINAPI kas_ServerStatsGet(const void *cellHandle,
				       const void *serverHandle,
				       kas_serverStats_p stats,
				       afs_status_p st);

extern int ADMINAPI kas_ServerDebugGet(const void *cellHandle,
				       const void *serverHandle,
				       kas_serverDebugInfo_p debug,
				       afs_status_p st);

extern int ADMINAPI kas_ServerRandomKeyGet(const void *cellHandle,
					   const void *serverHandle,
					   kas_encryptionKey_p key,
					   afs_status_p st);

extern int ADMINAPI kas_StringToKey(const char *cellName, const char *string,
				    kas_encryptionKey_p key, afs_status_p st);

extern int ADMINAPI kas_KeyCheckSum(const kas_encryptionKey_p key,
				    unsigned int *cksumP, afs_status_p st);

#endif /* OPENAFS_KAS_ADMIN_H */
