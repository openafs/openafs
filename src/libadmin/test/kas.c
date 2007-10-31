/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * This file implements the kas related funtions for afscp
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "kas.h"
#include <time.h>

/*
 * Utility functions
 */

/*
 * Generic fuction for converting input string to an integer.  Pass
 * the error_msg you want displayed if there is an error converting
 * the string.
 */

static int
GetIntFromString(const char *int_str, const char *error_msg)
{
    int i;
    char *bad_char = NULL;

    i = strtoul(int_str, &bad_char, 10);
    if ((bad_char == NULL) || (*bad_char == 0)) {
	return i;
    }

    ERR_EXT(error_msg);
}

int
DoKasPrincipalCreate(struct cmd_syndesc *as, void *arock)
{
    typedef enum { PRINCIPAL, INSTANCE,
	PASSWORD
    } DoKasPrincipalCreate_parm_t;
    afs_status_t st = 0;
    const char *instance = NULL;
    kas_identity_t user;
    const char *password;

    strcpy(user.principal, as->parms[PRINCIPAL].items->data);

    if (as->parms[INSTANCE].items) {
	strcpy(user.instance, as->parms[INSTANCE].items->data);
    } else {
	user.instance[0] = 0;
    }

    password = as->parms[PASSWORD].items->data;

    if (!kas_PrincipalCreate(cellHandle, 0, &user, password, &st)) {
	ERR_ST_EXT("kas_PrincipalCreate", st);
    }

    return 0;
}

int
DoKasPrincipalDelete(struct cmd_syndesc *as, void *arock)
{
    typedef enum { PRINCIPAL, INSTANCE } DoKasPrincipalGet_parm_t;
    afs_status_t st = 0;
    const char *instance = NULL;
    kas_identity_t user;

    strcpy(user.principal, as->parms[PRINCIPAL].items->data);

    if (as->parms[INSTANCE].items) {
	strcpy(user.instance, as->parms[PRINCIPAL].items->data);
    } else {
	user.instance[0] = 0;
    }

    if (!kas_PrincipalDelete(cellHandle, 0, &user, &st)) {
	ERR_ST_EXT("kas_PrincipalDelete", st);
    }

    return 0;
}

static void
Print_kas_principalEntry_p(kas_principalEntry_p principal, const char *prefix)
{
    int i;

    if (principal->adminSetting == KAS_ADMIN) {
	printf("%sAdmin setting: KAS_ADMIN\n", prefix);
    } else {
	printf("%sAdmin setting: NO_KAS_ADMIN\n", prefix);
    }

    if (principal->tgsSetting == TGS) {
	printf("%sTGS setting: TGS\n", prefix);
    } else {
	printf("%sTGS setting: NO_TGS\n", prefix);
    }

    if (principal->encSetting == ENCRYPT) {
	printf("%sEncrypt setting: ENCRYPT\n", prefix);
    } else {
	printf("%sEncrypt setting: NO_ENCRYPT\n", prefix);
    }

    if (principal->cpwSetting == CHANGE_PASSWORD) {
	printf("%sChange password setting: CHANGE_PASSWORD\n", prefix);
    } else {
	printf("%sChange password setting: NO_CHANGE_PASSWORD\n", prefix);
    }

    if (principal->rpwSetting == REUSE_PASSWORD) {
	printf("%sReuse password setting: REUSE_PASSWORD\n", prefix);
    } else {
	printf("%sReuse password setting: NO_REUSE_PASSWORD\n", prefix);
    }

    printf("%sExpiration: %u\n", prefix, principal->userExpiration);
    printf("%sLast modification time %u\n", prefix, principal->lastModTime);
    printf("%sLast modifying principal %s", prefix,
	   principal->lastModPrincipal.principal);
    if (principal->lastModPrincipal.instance[0] != 0) {
	printf(".%s\n", principal->lastModPrincipal.instance);
    } else {
	printf("\n");
    }

    printf("%sLast change password time %u\n", prefix,
	   principal->lastChangePasswordTime);
    printf("%sMax ticket lifetime %d\n", prefix,
	   principal->maxTicketLifetime);
    printf("%sKey version number %d\n", prefix, principal->keyVersion);

    printf("%sKey contents :", prefix);
    for (i = 0; i < KAS_ENCRYPTION_KEY_LEN; i++) {
	printf("%d ", principal->key.key[i]);
    }
    printf("\n");

    printf("%sKey checksum %u\n", prefix, principal->keyCheckSum);
    printf("%sDays to password expire %d\n", prefix,
	   principal->daysToPasswordExpire);
    printf("%sFailed login count %d\n", prefix, principal->failLoginCount);
    printf("%sLock time %d\n", prefix, principal->lockTime);
}

int
DoKasPrincipalGet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { PRINCIPAL, INSTANCE } DoKasPrincipalGet_parm_t;
    afs_status_t st = 0;
    const char *instance = NULL;
    kas_identity_t user;
    kas_principalEntry_t principal;

    strcpy(user.principal, as->parms[PRINCIPAL].items->data);

    if (as->parms[INSTANCE].items) {
	strcpy(user.instance, as->parms[PRINCIPAL].items->data);
    } else {
	user.instance[0] = 0;
    }

    if (!kas_PrincipalGet(cellHandle, 0, &user, &principal, &st)) {
	ERR_ST_EXT("kas_PrincipalGet", st);
    }

    Print_kas_principalEntry_p(&principal, "");

    return 0;
}

int
DoKasPrincipalList(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    void *iter;
    kas_identity_t prin;

    if (!kas_PrincipalGetBegin(cellHandle, 0, &iter, &st)) {
	ERR_ST_EXT("kas_PrincipalGetBegin", st);
    }

    printf("Listing principals:\n");
    while (kas_PrincipalGetNext(iter, &prin, &st)) {
	printf("%s", prin.principal);
	if (prin.instance[0] != 0) {
	    printf(".%s\n", prin.instance);
	} else {
	    printf("\n");
	}
    }

    if (st != ADMITERATORDONE) {
	ERR_ST_EXT("kas_PrincipalGetNext", st);
    }

    if (!kas_PrincipalGetDone(iter, &st)) {
	ERR_ST_EXT("kas_PrincipalGetDone", st);
    }

    return 0;
}

int
DoKasPrincipalKeySet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { PRINCIPAL, INSTANCE, PASSWORD,
	KEYVERSION
    } DoKasPrincipalKeySet_parm_t;
    afs_status_t st = 0;
    kas_encryptionKey_t key;
    kas_identity_t user;
    int key_version;
    const char *cell;
    const char *password;

    strcpy(user.principal, as->parms[PRINCIPAL].items->data);

    if (as->parms[INSTANCE].items) {
	strcpy(user.instance, as->parms[INSTANCE].items->data);
    } else {
	user.instance[0] = 0;
    }

    if (!afsclient_CellNameGet(cellHandle, &cell, &st)) {
	ERR_ST_EXT("afsclient_CellNameGet", st);
    }

    password = as->parms[PASSWORD].items->data;
    key_version =
	GetIntFromString(as->parms[KEYVERSION].items->data,
			 "invalid key version number");
    if (!kas_StringToKey(cell, password, &key, &st)) {
	ERR_ST_EXT("kas_StringToKey", st);
    }

    if (!kas_PrincipalKeySet(cellHandle, 0, &user, key_version, &key, &st)) {
	ERR_ST_EXT("kas_PrincipalKeySet", st);
    }

    return 0;
}

int
DoKasPrincipalLockStatusGet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { PRINCIPAL, INSTANCE } DoKasPrincipalLockStatusGet_parm_t;
    afs_status_t st = 0;
    kas_identity_t user;
    unsigned int lock_end_time = 0;

    strcpy(user.principal, as->parms[PRINCIPAL].items->data);

    if (as->parms[INSTANCE].items) {
	strcpy(user.instance, as->parms[INSTANCE].items->data);
    } else {
	user.instance[0] = 0;
    }

    if (!kas_PrincipalLockStatusGet
	(cellHandle, 0, &user, &lock_end_time, &st)) {
	ERR_ST_EXT("kas_PrincipalLockStatusGet", st);
    }

    printf("The lock end time is %u\n", lock_end_time);

    return 0;
}

int
DoKasPrincipalUnlock(struct cmd_syndesc *as, void *arock)
{
    typedef enum { PRINCIPAL, INSTANCE } DoKasPrincipalUnlock_parm_t;
    afs_status_t st = 0;
    kas_identity_t user;
    unsigned int lock_end_time = 0;

    strcpy(user.principal, as->parms[PRINCIPAL].items->data);

    if (as->parms[INSTANCE].items) {
	strcpy(user.instance, as->parms[INSTANCE].items->data);
    } else {
	user.instance[0] = 0;
    }

    if (!kas_PrincipalUnlock(cellHandle, 0, &user, &st)) {
	ERR_ST_EXT("kas_PrincipalUnlock", st);
    }

    return 0;
}

int
DoKasPrincipalFieldsSet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { PRINCIPAL, INSTANCE, ADMIN, NOADMIN, GRANTTICKET,
	NOGRANTTICKET, ENCRYPT2, NOENCRYPT, CHANGEPASSWORD,
	NOCHANGEPASSWORD, REUSEPASSWORD, NOREUSEPASSWORD,
	EXPIRES, MAXTICKETLIFETIME, PASSWORDEXPIRES,
	FAILEDPASSWORDATTEMPTS, FAILEDPASSWORDLOCKTIME
    } DoKasPrincipalFieldsSet_parm_t;
    afs_status_t st = 0;
    kas_identity_t user;
    kas_admin_t admin;
    kas_admin_p admin_ptr = NULL;
    int have_admin = 0;
    kas_tgs_t tgs;
    kas_tgs_p tgs_ptr = NULL;
    int have_tgs = 0;
    kas_enc_t enc;
    kas_enc_p enc_ptr = NULL;
    int have_enc = 0;
    kas_cpw_t cpw;
    kas_cpw_p cpw_ptr = NULL;
    int have_cpw = 0;
    kas_rpw_t reuse;
    kas_rpw_p reuse_ptr = NULL;
    int have_reuse = 0;
    unsigned int expire;
    unsigned int *expire_ptr = NULL;
    int have_expire = 0;
    unsigned int max_ticket;
    unsigned int *max_ticket_ptr = NULL;
    int have_max_ticket = 0;
    unsigned int password_expire;
    unsigned int *password_expire_ptr = NULL;
    int have_password_expire = 0;
    unsigned int failed_password_attempts;
    unsigned int *failed_password_attempts_ptr = NULL;
    int have_failed_password_attempts = 0;
    unsigned int failed_password_lock_time;
    unsigned int *failed_password_lock_time_ptr = NULL;
    int have_failed_password_lock_time = 0;

    strcpy(user.principal, as->parms[PRINCIPAL].items->data);

    if (as->parms[INSTANCE].items) {
	strcpy(user.instance, as->parms[INSTANCE].items->data);
    } else {
	user.instance[0] = 0;
    }

    if (as->parms[ADMIN].items) {
	admin = KAS_ADMIN;
	admin_ptr = &admin;
	have_admin = 1;
    }

    if (as->parms[NOADMIN].items) {
	admin = NO_KAS_ADMIN;
	admin_ptr = &admin;
	if (have_admin) {
	    ERR_EXT("specify either admin or noadmin, not both");
	}
	have_admin = 1;
    }

    if (as->parms[GRANTTICKET].items) {
	tgs = TGS;
	tgs_ptr = &tgs;
	have_tgs = 1;
    }

    if (as->parms[NOGRANTTICKET].items) {
	tgs = NO_TGS;
	tgs_ptr = &tgs;
	if (have_tgs) {
	    ERR_EXT("specify either grantticket or nograntticket, not both");
	}
	have_tgs = 1;
    }

    if (as->parms[ENCRYPT2].items) {
	enc = ENCRYPT;
	enc_ptr = &enc;
	have_enc = 1;
    }

    if (as->parms[NOENCRYPT].items) {
	enc = NO_ENCRYPT;
	enc_ptr = &enc;
	if (have_tgs) {
	    ERR_EXT("specify either encrypt or noencrypt, not both");
	}
	have_enc = 1;
    }

    if (as->parms[CHANGEPASSWORD].items) {
	cpw = CHANGE_PASSWORD;
	cpw_ptr = &cpw;
	have_cpw = 1;
    }

    if (as->parms[NOCHANGEPASSWORD].items) {
	cpw = NO_CHANGE_PASSWORD;
	cpw_ptr = &cpw;
	if (have_tgs) {
	    ERR_EXT("specify either changepassword or "
		    "nochangepassword, not both");
	}
	have_cpw = 1;
    }

    if (as->parms[REUSEPASSWORD].items) {
	reuse = REUSE_PASSWORD;
	reuse_ptr = &reuse;
	have_reuse = 1;
    }

    if (as->parms[REUSEPASSWORD].items) {
	reuse = NO_REUSE_PASSWORD;
	reuse_ptr = &reuse;
	if (have_reuse) {
	    ERR_EXT("specify either reusepassword or "
		    "noreusepassword, not both");
	}
	have_reuse = 1;
    }

    if (as->parms[EXPIRES].items) {
	expire =
	    GetIntFromString(as->parms[EXPIRES].items->data,
			     "bad expiration date");
	expire_ptr = &expire;
	have_expire = 1;
    }

    if (as->parms[MAXTICKETLIFETIME].items) {
	max_ticket =
	    GetIntFromString(as->parms[MAXTICKETLIFETIME].items->data,
			     "bad max ticket lifetime");
	max_ticket_ptr = &max_ticket;
	have_max_ticket = 1;
    }

    if (as->parms[PASSWORDEXPIRES].items) {
	password_expire =
	    GetIntFromString(as->parms[PASSWORDEXPIRES].items->data,
			     "bad expiration date");
	password_expire_ptr = &password_expire;
	have_password_expire = 1;
    }

    if (as->parms[FAILEDPASSWORDATTEMPTS].items) {
	failed_password_attempts =
	    GetIntFromString(as->parms[FAILEDPASSWORDATTEMPTS].items->data,
			     "bad expiration date");
	failed_password_attempts_ptr = &failed_password_attempts;
	have_failed_password_attempts = 1;
    }

    if (as->parms[FAILEDPASSWORDLOCKTIME].items) {
	failed_password_lock_time =
	    GetIntFromString(as->parms[FAILEDPASSWORDLOCKTIME].items->data,
			     "bad expiration date");
	failed_password_lock_time_ptr = &failed_password_lock_time;
	have_failed_password_lock_time = 1;
    }

    if ((have_admin + have_tgs + have_enc + have_cpw + have_reuse +
	 have_expire + have_max_ticket + have_password_expire +
	 have_failed_password_attempts + have_failed_password_lock_time) ==
	0) {
	ERR_EXT("You must specify at least one attribute to change");
    }

    if (!kas_PrincipalFieldsSet
	(cellHandle, 0, &user, admin_ptr, tgs_ptr, enc_ptr, cpw_ptr,
	 expire_ptr, max_ticket_ptr, password_expire_ptr, reuse_ptr,
	 failed_password_attempts_ptr, failed_password_lock_time_ptr, &st)) {
	ERR_ST_EXT("kas_PrincipalFieldsSet", st);
    }

    return 0;
}

static void
Print_kas_serverStats_p(kas_serverStats_p stats, const char *prefix)
{
    time_t stime = stats->serverStartTime;

    printf("%sAllocations %d\n", prefix, stats->allocations);
    printf("%sFrees %d\n", prefix, stats->frees);
    printf("%sChange password requests %d\n", prefix,
	   stats->changePasswordRequests);
    printf("%sAdmin accounts %d\n", prefix, stats->adminAccounts);
    printf("%sHost %x\n", prefix, stats->host);
    printf("%sServer start time %s\n", prefix, ctime(&stime));
    printf("%sUser time %ld secs %ld usec\n", prefix, stats->userTime.tv_sec,
	   stats->userTime.tv_usec);
    printf("%sSystem time %ld secs %ld usec\n", prefix,
	   stats->systemTime.tv_sec, stats->systemTime.tv_usec);
    printf("%sData size %d\n", prefix, stats->dataSize);
    printf("%sStack size %d\n", prefix, stats->stackSize);
    printf("%sPage faults %d\n", prefix, stats->pageFaults);
    printf("%sHash table utilization %d\n", prefix,
	   stats->hashTableUtilization);
    printf("%sAuthentication requests %d aborts %d\n", prefix,
	   stats->authenticate.requests, stats->authenticate.aborts);
    printf("%sChange password requests %d aborts %d\n", prefix,
	   stats->changePassword.requests, stats->changePassword.aborts);
    printf("%sGet ticket requests %d aborts %d\n", prefix,
	   stats->getTicket.requests, stats->getTicket.aborts);
    printf("%sCreate user requests %d aborts %d\n", prefix,
	   stats->createUser.requests, stats->createUser.aborts);
    printf("%sSet password requests %d aborts %d\n", prefix,
	   stats->setPassword.requests, stats->setPassword.aborts);
    printf("%sSet fields requests %d aborts %d\n", prefix,
	   stats->setFields.requests, stats->setFields.aborts);
    printf("%sDelete user requests %d aborts %d\n", prefix,
	   stats->deleteUser.requests, stats->deleteUser.aborts);
    printf("%sGet entry requests %d aborts %d\n", prefix,
	   stats->getEntry.requests, stats->getEntry.aborts);
    printf("%sList entry requests %d aborts %d\n", prefix,
	   stats->listEntry.requests, stats->listEntry.aborts);
    printf("%sGet stats requests %d aborts %d\n", prefix,
	   stats->getStats.requests, stats->getStats.aborts);
    printf("%sGet password requests %d aborts %d\n", prefix,
	   stats->getPassword.requests, stats->getPassword.aborts);
    printf("%sGet random key requests %d aborts %d\n", prefix,
	   stats->getRandomKey.requests, stats->getRandomKey.aborts);
    printf("%sDebug requests %d aborts %d\n", prefix, stats->debug.requests,
	   stats->debug.aborts);
    printf("%sUDP authenticate requests %d aborts %d\n", prefix,
	   stats->udpAuthenticate.requests, stats->udpAuthenticate.aborts);
    printf("%sUDP get ticket requests %d aborts %d\n", prefix,
	   stats->udpGetTicket.requests, stats->udpGetTicket.aborts);
    printf("%sUnlock requests %d aborts %d\n", prefix, stats->unlock.requests,
	   stats->unlock.aborts);
    printf("%sLock status requests %d aborts %d\n", prefix,
	   stats->lockStatus.requests, stats->lockStatus.aborts);
    printf("%sString checks %d\n", prefix, stats->stringChecks);
}

int
DoKasServerStatsGet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER } DoKasServerStatsGet_parm_t;
    afs_status_t st = 0;
    const char *server_list[2] = { 0, 0 };
    void *kas_server = NULL;
    kas_serverStats_t stats;

    if (as->parms[SERVER].items) {
	server_list[0] = as->parms[SERVER].items->data;
    }

    if (!kas_ServerOpen(cellHandle, server_list, &kas_server, &st)) {
	ERR_ST_EXT("kas_ServerOpen", st);
    }

    if (!kas_ServerStatsGet(0, kas_server, &stats, &st)) {
	ERR_ST_EXT("kas_ServerStatsGet", st);
    }

    Print_kas_serverStats_p(&stats, "");

    kas_ServerClose(kas_server, 0);

    return 0;
}

static void
Print_kas_serverDebugInfo_p(kas_serverDebugInfo_p debug, const char *prefix)
{
    time_t time;
    int i;

    printf("%sHost %x\n", prefix, debug->host);
    time = debug->serverStartTime;
    printf("%sServer start time %s\n", prefix, ctime(&time));
    time = debug->currentTime;
    printf("%sCurrent time %s\n", prefix, ctime(&time));
    printf("%sNo auth %d\n", prefix, debug->noAuth);
    time = debug->lastTransaction;
    printf("%sLast transaction %s\n", prefix, ctime(&time));
    printf("%sLast operation %s\n", prefix, debug->lastOperation);
    printf("%sLast principal auth %s\n", prefix, debug->lastPrincipalAuth);
    printf("%sLast principal UDP auth %s\n", prefix,
	   debug->lastPrincipalUDPAuth);
    printf("%sLast principal TGS auth %s\n", prefix, debug->lastPrincipalTGS);
    printf("%sLast principal UDP TGS auth %s\n", prefix,
	   debug->lastPrincipalUDPTGS);
    printf("%sLast principal admin %s\n", prefix, debug->lastPrincipalAdmin);
    printf("%sLast server TGS %s\n", prefix, debug->lastServerTGS);
    printf("%sLast server UDP TGS %s\n", prefix, debug->lastServerUDPTGS);
    time = debug->nextAutoCheckPointWrite;
    printf("%sNext auto check point write %s\n", prefix, ctime(&time));
    printf("%sUpdates remaining before ACPW %d\n", prefix,
	   debug->updatesRemainingBeforeAutoCheckPointWrite);
    time = debug->dbHeaderRead;
    printf("%sDatabase header read %s\n", prefix, ctime(&time));
    printf("%sDatabase version %d\n", prefix, debug->dbVersion);
    printf("%sDatabase free ptr %d\n", prefix, debug->dbFreePtr);
    printf("%sDatabase EOF ptr %d\n", prefix, debug->dbEOFPtr);
    printf("%sDatabase kvno ptr %d\n", prefix, debug->dbKvnoPtr);
    printf("%sDatabase special keys version%d\n", prefix,
	   debug->dbSpecialKeysVersion);
    printf("%sDatabase header lock %d\n", prefix, debug->dbHeaderLock);
    printf("%sKey cache lock %d\n", prefix, debug->keyCacheLock);
    printf("%sKey cache version %d\n", prefix, debug->keyCacheVersion);
    printf("%sKey cache size %d\n", prefix, debug->keyCacheSize);
    printf("%sKey cache used %d\n", prefix, debug->keyCacheUsed);

    printf("%sKey cache\n", prefix);

    for (i = 0; i < debug->keyCacheUsed; i++) {
	printf("%s\tPrincipal %s\n", prefix, debug->keyCache[i].principal);
	time = debug->keyCache[i].lastUsed;
	printf("%s\tLast used %s\n", prefix, ctime(&time));
	printf("%s\tVersion number %d\n", prefix,
	       debug->keyCache[i].keyVersionNumber);
	printf("%s\tPrimary %d\n", prefix, debug->keyCache[i].primary);
	printf("%s\tCheck sum %d\n", prefix, debug->keyCache[i].keyCheckSum);
	printf("\n");
    }

}

int
DoKasServerDebugGet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER } DoKasServerDebugGet_parm_t;
    afs_status_t st = 0;
    const char *server_list[2] = { 0, 0 };
    void *kas_server = NULL;
    kas_serverDebugInfo_t debug;

    if (as->parms[SERVER].items) {
	server_list[0] = as->parms[SERVER].items->data;
    }

    if (!kas_ServerOpen(cellHandle, server_list, &kas_server, &st)) {
	ERR_ST_EXT("kas_ServerOpen", st);
    }

    if (!kas_ServerDebugGet(0, kas_server, &debug, &st)) {
	ERR_ST_EXT("kas_ServerDebugGet", st);
    }

    Print_kas_serverDebugInfo_p(&debug, "");

    kas_ServerClose(kas_server, 0);

    return 0;
}

int
DoKasServerRandomKeyGet(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    kas_encryptionKey_t key;
    int i;

    if (!kas_ServerRandomKeyGet(cellHandle, 0, &key, &st)) {
	ERR_ST_EXT("kas_ServerRandomKeyGet", st);
    }

    printf("Key: ");
    for (i = 0; i < KAS_ENCRYPTION_KEY_LEN; i++) {
	printf("%d ", key.key[i]);
    }
    printf("\n");

    return 0;
}

void
SetupKasAdminCmd(void)
{
    struct cmd_syndesc *ts;

    ts = cmd_CreateSyntax("KasPrincipalCreate", DoKasPrincipalCreate, NULL,
			  "create a new principal");
    cmd_AddParm(ts, "-principal", CMD_SINGLE, CMD_REQUIRED,
		"principal to create");
    cmd_AddParm(ts, "-instance", CMD_SINGLE, CMD_OPTIONAL,
		"principal instance");
    cmd_AddParm(ts, "-password", CMD_SINGLE, CMD_REQUIRED,
		"initial principal password");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("KasPrincipalDelete", DoKasPrincipalDelete, NULL,
			  "delete a principal");
    cmd_AddParm(ts, "-principal", CMD_SINGLE, CMD_REQUIRED,
		"principal to delete");
    cmd_AddParm(ts, "-instance", CMD_SINGLE, CMD_OPTIONAL,
		"principal instance");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("KasPrincipalGet", DoKasPrincipalGet, NULL,
			  "get information about a principal");
    cmd_AddParm(ts, "-principal", CMD_SINGLE, CMD_REQUIRED,
		"principal to get");
    cmd_AddParm(ts, "-instance", CMD_SINGLE, CMD_OPTIONAL,
		"principal instance");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("KasPrincipalList", DoKasPrincipalList, NULL,
			  "list all principals");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("KasPrincipalKeySet", DoKasPrincipalKeySet, NULL,
			  "set the password for a principal");
    cmd_AddParm(ts, "-principal", CMD_SINGLE, CMD_REQUIRED,
		"principal to modify");
    cmd_AddParm(ts, "-instance", CMD_SINGLE, CMD_OPTIONAL,
		"principal instance");
    cmd_AddParm(ts, "-password", CMD_SINGLE, CMD_REQUIRED,
		"new principal password");
    cmd_AddParm(ts, "-version", CMD_SINGLE, CMD_REQUIRED,
		"password version number");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("KasPrincipalLockStatusGet",
			  DoKasPrincipalLockStatusGet, NULL,
			  "get the lock status of a principal");
    cmd_AddParm(ts, "-principal", CMD_SINGLE, CMD_REQUIRED,
		"principal to query");
    cmd_AddParm(ts, "-instance", CMD_SINGLE, CMD_OPTIONAL,
		"principal instance");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("KasPrincipalUnlock", DoKasPrincipalUnlock, NULL,
			  "unlock a principal");
    cmd_AddParm(ts, "-principal", CMD_SINGLE, CMD_REQUIRED,
		"principal to unlock");
    cmd_AddParm(ts, "-instance", CMD_SINGLE, CMD_OPTIONAL,
		"principal instance");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("KasPrincipalFieldsSet", DoKasPrincipalFieldsSet, NULL,
			  "modify a principal");
    cmd_AddParm(ts, "-principal", CMD_SINGLE, CMD_REQUIRED,
		"principal to modify");
    cmd_AddParm(ts, "-instance", CMD_SINGLE, CMD_OPTIONAL,
		"principal instance");
    cmd_AddParm(ts, "-admin", CMD_FLAG, CMD_OPTIONAL,
		"make this principal an admin");
    cmd_AddParm(ts, "-noadmin", CMD_FLAG, CMD_OPTIONAL,
		"remove admin from this principal");
    cmd_AddParm(ts, "-grantticket", CMD_FLAG, CMD_OPTIONAL,
		"this principal can grant server tickets");
    cmd_AddParm(ts, "-nograntticket", CMD_FLAG, CMD_OPTIONAL,
		"this principal cannot grant server tickets");
    cmd_AddParm(ts, "-encrypt", CMD_FLAG, CMD_OPTIONAL,
		"this principal can encrypt data");
    cmd_AddParm(ts, "-noencrypt", CMD_FLAG, CMD_OPTIONAL,
		"this principal cannot encrypt data");
    cmd_AddParm(ts, "-changepassword", CMD_FLAG, CMD_OPTIONAL,
		"this principal can change its password");
    cmd_AddParm(ts, "-nochangepassword", CMD_FLAG, CMD_OPTIONAL,
		"this principal cannot change its password");
    cmd_AddParm(ts, "-reusepassword", CMD_FLAG, CMD_OPTIONAL,
		"this principal can reuse its password");
    cmd_AddParm(ts, "-noreusepassword", CMD_FLAG, CMD_OPTIONAL,
		"this principal cannot reuse its password");
    cmd_AddParm(ts, "-expires", CMD_SINGLE, CMD_OPTIONAL,
		"the time at which this principal expires");
    cmd_AddParm(ts, "-maxticketlifetime", CMD_SINGLE, CMD_OPTIONAL,
		"the maximum ticket lifetime this principal can request");
    cmd_AddParm(ts, "-passwordexpires", CMD_SINGLE, CMD_OPTIONAL,
		"the time at which this principal's password expires");
    cmd_AddParm(ts, "-failedpasswordattempts", CMD_SINGLE, CMD_OPTIONAL,
		"the number of failed password attempts this principal "
		"can incur before it is locked");
    cmd_AddParm(ts, "-failedpasswordlocktime", CMD_SINGLE, CMD_OPTIONAL,
		"the amount of time this principal will be locked if the "
		"maximum failed password attempts is exceeded");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("KasServerStatsGet", DoKasServerStatsGet, NULL,
			  "get stats on a kaserver");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to query");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("KasServerDebugGet", DoKasServerDebugGet, NULL,
			  "get debug info from a kaserver");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to query");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("KasServerRandomKeyGet", DoKasServerRandomKeyGet, NULL,
			  "create a random key");
    SetupCommonCmdArgs(ts);

}
