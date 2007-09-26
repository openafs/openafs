/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <afs/stds.h>
#include "afs_kasAdmin.h"
#include "../adminutil/afs_AdminInternal.h"
#include <afs/afs_AdminErrors.h>
#include <afs/afs_utilAdmin.h>
#include <afs/kauth.h>
#include <afs/kautils.h>
#include <afs/kaport.h>
#include <pthread.h>

#undef ENCRYPT

typedef struct {
    int begin_magic;
    int is_valid;
    struct ubik_client *servers;
    char *cell;
    int end_magic;
} kas_server_t, *kas_server_p;

/*
 * IsValidServerHandle - verify the validity of a kas_server_t handle.
 *
 * PARAMETERS
 *
 * IN serverHandle - the handle to be verified.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

static int
IsValidServerHandle(const kas_server_p serverHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    /*
     * Validate input parameters
     */

    if (serverHandle == NULL) {
	tst = ADMKASSERVERHANDLENULL;
	goto fail_IsValidServerHandle;
    }

    if ((serverHandle->begin_magic != BEGIN_MAGIC)
	|| (serverHandle->end_magic != END_MAGIC)) {
	tst = ADMKASSERVERHANDLEBADMAGIC;
	goto fail_IsValidServerHandle;
    }

    if (!serverHandle->is_valid) {
	tst = ADMKASSERVERHANDLENOTVALID;
	goto fail_IsValidServerHandle;
    }

    if (serverHandle->servers == NULL) {
	tst = ADMKASSERVERHANDLENOSERVERS;
	goto fail_IsValidServerHandle;
    }
    rc = 1;

  fail_IsValidServerHandle:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * IsValidCellHandle - verify the validity of a afs_cell_handle_t handle
 * for doing kas related functions.
 *
 * PARAMETERS
 *
 * IN cellHandle - the handle to be verified.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

static int
IsValidCellHandle(const afs_cell_handle_p cellHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    /*
     * Validate input parameters
     */

    if (!CellHandleIsValid((void *)cellHandle, &tst)) {
	goto fail_IsValidCellHandle;
    }

    if (!cellHandle->kas_valid) {
	tst = ADMCLIENTCELLKASINVALID;
	goto fail_IsValidCellHandle;
    }

    if (cellHandle->kas == NULL) {
	tst = ADMCLIENTCELLKASNULL;
	goto fail_IsValidCellHandle;
    }
    rc = 1;

  fail_IsValidCellHandle:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * For all kas admin functions that take a cellHandle and a serverHandle,
 * the intention is that is the cellHandle is not NULL, we should use
 * it.  Otherwise, we use the serverHandle.  It is an error for both
 * of these parameters to be non-NULL.
 */

/*
 * ChooseValidServer - given a serverHandle and a cellHandle, choose the
 * one that is non-NULL, validate it, and return a ubik_client structure
 * that contains it.
 *
 * PARAMETERS
 *
 * IN cellHandle - the cell where kas calls are to be made
 *
 * IN serverHandle - the group of server(s) that should be used to satisfy
 * kas calls.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

static int
ChooseValidServer(const afs_cell_handle_p cellHandle,
		  const kas_server_p serverHandle, kas_server_p kasHandle,
		  afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    /*
     * Validate input parameters
     */

    if (kasHandle == NULL) {
	tst = ADMKASKASHANDLENULL;
	goto fail_ChooseValidServer;
    }

    /*
     * Only one of the input handle parameters to this function should
     * not be NULL
     */
    if ((cellHandle == NULL) && (serverHandle == NULL)) {
	tst = ADMKASCELLHANDLEANDSERVERHANDLENULL;
	goto fail_ChooseValidServer;
    }

    if ((cellHandle != NULL) && (serverHandle != NULL)) {
	tst = ADMKASCELLHANDLEANDSERVERHANDLENOTNULL;
	goto fail_ChooseValidServer;
    }

    /*
     * Validate the non-NULL handle
     */

    if (cellHandle != NULL) {
	if (IsValidCellHandle(cellHandle, &tst)) {
	    kasHandle->servers = cellHandle->kas;
	    kasHandle->cell = cellHandle->working_cell;
	} else {
	    goto fail_ChooseValidServer;
	}
    } else {
	if (IsValidServerHandle(serverHandle, &tst)) {
	    kasHandle->servers = serverHandle->servers;
	    kasHandle->cell = serverHandle->cell;
	} else {
	    goto fail_ChooseValidServer;
	}
    }

    kasHandle->begin_magic = BEGIN_MAGIC;
    kasHandle->end_magic = END_MAGIC;
    kasHandle->is_valid = 1;
    rc = 1;

  fail_ChooseValidServer:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


static int
kaentryinfo_to_kas_principalEntry_t(struct kaentryinfo *from,
				    kas_principalEntry_p to, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    int short reuse;
    unsigned char misc_stuff[4];

    if (from == NULL) {
	tst = ADMKASFROMNULL;
	goto fail_kaentryinfo_to_kas_principalEntry_t;
    }

    if (to == NULL) {
	tst = ADMKASTONULL;
	goto fail_kaentryinfo_to_kas_principalEntry_t;
    }

    if (from->flags & KAFADMIN) {
	to->adminSetting = KAS_ADMIN;
    } else {
	to->adminSetting = NO_KAS_ADMIN;
    }

    if (from->flags & KAFNOTGS) {
	to->tgsSetting = NO_TGS;
    } else {
	to->tgsSetting = TGS;
    }

    if (from->flags & KAFNOSEAL) {
	to->encSetting = NO_ENCRYPT;
    } else {
	to->encSetting = ENCRYPT;
    }

    if (from->flags & KAFNOCPW) {
	to->cpwSetting = NO_CHANGE_PASSWORD;
    } else {
	to->cpwSetting = CHANGE_PASSWORD;
    }

    reuse = (short)from->reserved3;
    if (!reuse) {
	to->rpwSetting = REUSE_PASSWORD;
    } else {
	to->rpwSetting = NO_REUSE_PASSWORD;
    }


    if (from->user_expiration == NEVERDATE) {
	to->userExpiration = 0;
    } else {
	to->userExpiration = from->user_expiration;
    }

    to->lastModTime = from->modification_time;
    strcpy(to->lastModPrincipal.principal, from->modification_user.name);
    strcpy(to->lastModPrincipal.instance, from->modification_user.instance);
    to->lastChangePasswordTime = from->change_password_time;
    to->maxTicketLifetime = from->max_ticket_lifetime;
    to->keyVersion = from->key_version;
    memcpy(&to->key, &from->key, sizeof(to->key));
    to->keyCheckSum = from->keyCheckSum;

    unpack_long(from->misc_auth_bytes, misc_stuff);
    to->daysToPasswordExpire = misc_stuff[0];
    to->failLoginCount = misc_stuff[2];
    to->lockTime = misc_stuff[3] << 9;
    rc = 1;

  fail_kaentryinfo_to_kas_principalEntry_t:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * kas_ServerOpen - open a handle to a set of kaserver's.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the server(s) live.
 *
 * IN serverList - a NULL terminated list (a la argv) of server's that 
 * should be opened.
 *
 * OUT serverHandleP - a pointer to a void pointer that upon successful
 * completion contains serverHandle that can be used in other kas functions.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 * 
 * ASSUMPTIONS
 * 
 * This function make some assumptions about the afsconf_cell used by 
 * ka_AuthSpecificServersConn (since I just wrote ka_AuthSpecificServersConn).
 * It only fills in the fields that are required.
 *
 * Also we assume that the servers listed are members of the cell in
 * cellHandle without verifying that this is in fact the case.  kas itself
 * always assumes that the -servers parameter lists servers in the current
 * cell without verifying, so I am no worse than the current
 * implementation.  In fact I'm actually a little more flexible since you
 * can actually use my serverList to play with servers in another cell.
 * You can't do that with kas.  For certain functions in kas the same
 * cell assumption can cause things to fail (the ka_StringToKey function in
 * UserCreate).
 */

int ADMINAPI
kas_ServerOpen(const void *cellHandle, const char **serverList,
	       void **serverHandleP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    int server_count = 0, server_addr;
    struct afsconf_cell server_info;
    kas_server_p k_handle = (kas_server_p) malloc(sizeof(kas_server_t));

    /*
     * Validate input parameters
     */

    if (c_handle == NULL) {
	tst = ADMCLIENTCELLHANDLENULL;
	goto fail_kas_ServerOpen;
    }

    if (c_handle->kas_valid == 0) {
	tst = ADMCLIENTCELLKASINVALID;
	goto fail_kas_ServerOpen;
    }

    if (serverList == NULL) {
	tst = ADMKASSERVERLISTNULL;
	goto fail_kas_ServerOpen;
    }

    if (serverHandleP == NULL) {
	tst = ADMKASSERVERHANDLEPNULL;
	goto fail_kas_ServerOpen;
    }

    if (k_handle == NULL) {
	tst = ADMNOMEM;
	goto fail_kas_ServerOpen;
    }

    k_handle->begin_magic = BEGIN_MAGIC;
    k_handle->end_magic = END_MAGIC;
    k_handle->is_valid = 0;
    k_handle->servers = NULL;

    /*
     * Convert serverList to numeric addresses
     */

    for (server_count = 0; serverList[server_count] != NULL; server_count++) {
	if (server_count >= MAXHOSTSPERCELL) {
	    tst = ADMKASSERVERLISTTOOLONG;
	    goto fail_kas_ServerOpen;
	}
	if (util_AdminServerAddressGetFromName
	    (serverList[server_count], &server_addr, &tst)) {
	    server_info.hostAddr[server_count].sin_addr.s_addr =
		htonl(server_addr);
	    server_info.hostAddr[server_count].sin_port =
		htons(AFSCONF_KAUTHPORT);
	} else {
	    goto fail_kas_ServerOpen;
	}
    }

    if (server_count == 0) {
	tst = ADMKASSERVERLISTEMPTY;
	goto fail_kas_ServerOpen;
    }

    /*
     * Get a ubik_client handle for the specified servers
     */
    server_info.numServers = server_count;
    if (!
	(tst =
	 ka_AuthSpecificServersConn(KA_MAINTENANCE_SERVICE,
				    &c_handle->tokens->kas_token,
				    &server_info, &k_handle->servers))) {
	k_handle->is_valid = 1;
	k_handle->cell = c_handle->working_cell;
	*serverHandleP = (void *)k_handle;
    } else {
	goto fail_kas_ServerOpen;
    }
    rc = 1;

  fail_kas_ServerOpen:

    if ((rc == 0) && (k_handle != NULL)) {
	free(k_handle);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * kas_ServerClose - close a serverHandle.
 *
 * PARAMETERS
 *
 * IN serverHandle - a serverHandle previously returned by kas_ServerOpen.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
kas_ServerClose(const void *serverHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    kas_server_p k_handle = (kas_server_p) serverHandle;

    if (!IsValidServerHandle(k_handle, &tst)) {
	goto fail_kas_ServerClose;
    }

    tst = ubik_ClientDestroy(k_handle->servers);
    if (tst) {
	goto fail_kas_ServerClose;
    }

    k_handle->is_valid = 0;
    free(k_handle);
    rc = 1;

  fail_kas_ServerClose:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * kas_PrincipalCreate - create a new principal.
 *
 * PARAMETERS
 *
 * IN cellHandle - a cellHandle previously returned by afsclient_CellOpen.
 *
 * IN serverHandle - a serverHandle previously returned by kas_ServerOpen.
 *
 * IN who - a kas_identity_p containing the identity of the new principal
 * to be created.
 *
 * IN password - the new principal's initial password.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
kas_PrincipalCreate(const void *cellHandle, const void *serverHandle,
		    const kas_identity_p who, const char *password,
		    afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    kas_server_p k_handle = (kas_server_p) serverHandle;
    kas_server_t kaserver;
    EncryptionKey key;
    struct kas_encryptionKey kas_key;

    /*
     * Validate input arguments and make rpc.
     */

    if (who == NULL) {
	tst = ADMKASWHONULL;
	goto fail_kas_PrincipalCreate;
    }

    if (password == NULL) {
	tst = ADMKASPASSWORDNULL;
	goto fail_kas_PrincipalCreate;
    }

    if (!ChooseValidServer(c_handle, k_handle, &kaserver, &tst)) {
	goto fail_kas_PrincipalCreate;
    }

    if (!kas_StringToKey(kaserver.cell, password, &kas_key, &tst)) {
	goto fail_kas_PrincipalCreate;
    }

    memcpy(&key, &kas_key, sizeof(key));

    tst =
	ubik_KAM_CreateUser(kaserver.servers, 0, who->principal,
		  who->instance, key);
    if (tst) {
	goto fail_kas_PrincipalCreate;
    }
    rc = 1;


  fail_kas_PrincipalCreate:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * kas_PrincipalDelete - delete an existing principal.
 *
 * PARAMETERS
 *
 * IN cellHandle - a cellHandle previously returned by afsclient_CellOpen.
 *
 * IN serverHandle - a serverHandle previously returned by kas_ServerOpen.
 *
 * IN who - a kas_identity_p containing the identity of the principal
 * to be deleted.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
kas_PrincipalDelete(const void *cellHandle, const void *serverHandle,
		    const kas_identity_p who, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    kas_server_p k_handle = (kas_server_p) serverHandle;
    kas_server_t kaserver;

    /*
     * Validate input arguments and make rpc.
     */

    if (who == NULL) {
	tst = ADMKASWHONULL;
	goto fail_kas_PrincipalDelete;
    }

    if (!ChooseValidServer(c_handle, k_handle, &kaserver, &tst)) {
	goto fail_kas_PrincipalDelete;
    }
    tst =
	ubik_KAM_DeleteUser(kaserver.servers, 0, who->principal,
		  who->instance);
    if (tst) {
	goto fail_kas_PrincipalDelete;
    }
    rc = 1;

  fail_kas_PrincipalDelete:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * GetPrincipalLockStatus - get the lock status of a principal.
 *
 * PARAMETERS
 *
 * IN kaserver - a valid kaserver handle previously returned by 
 * ChooseValidServer
 *
 * IN who - a kas_identity_p containing the identity of the principal
 * to be queried.
 *
 * OUT lockedUntil - the remaining number of seconds the principal is locked.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

static int
GetPrincipalLockStatus(const kas_server_p kaserver, const kas_identity_p who,
		       unsigned int *lockedUntil, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    unsigned int locked;
    int count = 0;
    int once = 0;

    /*
     * Validate input arguments and make rpc.
     */

    if (kaserver == NULL) {
	tst = ADMKASKASERVERNULL;
	goto fail_GetPrincipalLockStatus;
    }

    if (who == NULL) {
	tst = ADMKASWHONULL;
	goto fail_GetPrincipalLockStatus;
    }

    if (lockedUntil == NULL) {
	tst = ADMKASLOCKEDUNTILNULL;
	goto fail_GetPrincipalLockStatus;
    }

    /*
     * Unlike every other kas rpc we make here, the lock/unlock rpc's
     * aren't ubik based.  So instead of calling ubik_Call, we use
     * ubik_CallIter.  ubik_CallIter steps through the list of hosts
     * in the ubik_client and calls them one at a time.  Since there's
     * no synchronization of this data across the servers we have to 
     * manually keep track of the shortest time to unlock the user ourselves.
     *
     * The original inspiration for this function is ka_islocked
     * in admin_tools.c.  I think that function is totally bogus so I'm
     * rewriting it here.
     *
     * This function should contact all the kaservers and request the lock
     * status of the principal.  If any of the servers say the principal is
     * unlocked, we report it as unlocked.  If all the servers say the
     * principal is locked, we find the server with the shortest lock time
     * remaining on the principal and return that time.
     *
     * This is different than kas, but I think kas is buggy.
     */

    *lockedUntil = 0;
    do {
	locked = 0;
	tst =
	    ubik_CallIter(KAM_LockStatus, kaserver->servers, UPUBIKONLY,
			  &count, (long)who->principal, (long)who->instance, (long)&locked, 0,
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	if (tst == 0) {
	    if (locked) {
		if ((locked < *lockedUntil) || !once) {
		    *lockedUntil = locked;
		    once++;
		}
	    }
	}
    } while ((tst != UNOSERVERS) && (locked != 0));

    /*
     * Check to see if any server reported this principal unlocked.
     */

    if ((tst == 0) && (locked == 0)) {
	*lockedUntil = 0;
    }
    if ((tst == 0) || (tst == UNOSERVERS)) {
	rc = 1;
    }

  fail_GetPrincipalLockStatus:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * kas_PrincipalGet - retrieve information about a single principal.
 *
 * PARAMETERS
 *
 * IN cellHandle - a cellHandle previously returned by afsclient_CellOpen.
 *
 * IN serverHandle - a serverHandle previously returned by kas_ServerOpen.
 *
 * IN who - a kas_identity_p containing the identity of the principal
 * to be retrieved.
 *
 * OUT principal - upon successful completion contains information
 * about who.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
kas_PrincipalGet(const void *cellHandle, const void *serverHandle,
		 const kas_identity_p who, kas_principalEntry_p principal,
		 afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    kas_server_p k_handle = (kas_server_p) serverHandle;
    kas_server_t kaserver;
    struct kaentryinfo entry;

    /*
     * Validate input arguments and make rpc.
     */

    if (who == NULL) {
	tst = ADMKASWHONULL;
	goto fail_kas_PrincipalGet;
    }

    if (principal == NULL) {
	tst = ADMKASPRINCIPALNULL;
	goto fail_kas_PrincipalGet;
    }

    if (!ChooseValidServer(c_handle, k_handle, &kaserver, &tst)) {
	goto fail_kas_PrincipalGet;
    }

    tst =
	ubik_KAM_GetEntry(kaserver.servers, 0, who->principal,
		  who->instance, KAMAJORVERSION, &entry);
    if (tst) {
	goto fail_kas_PrincipalGet;
    }

    /*
     * copy the kaentryinfo structure to our kas_principalEntry_t
     * format
     */
    if (!kaentryinfo_to_kas_principalEntry_t(&entry, principal, &tst)) {
	goto fail_kas_PrincipalGet;
    }
    rc = 1;

  fail_kas_PrincipalGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

typedef struct principal_get {
    int current;
    int next;
    int count;
    kas_server_t kaserver;
    kas_identity_t principal[CACHED_ITEMS];
} principal_get_t, *principal_get_p;

static int
DeletePrincipalSpecificData(void *rpc_specific, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    principal_get_p prin = (principal_get_p) rpc_specific;

    prin->kaserver.is_valid = 0;
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
GetPrincipalRPC(void *rpc_specific, int slot, int *last_item,
		int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    principal_get_p prin = (principal_get_p) rpc_specific;

    tst =
	ubik_KAM_ListEntry(prin->kaserver.servers, 0, prin->current,
                           &prin->next, &prin->count, (kaident *)&prin->principal[slot]);
    if (tst == 0) {
	prin->current = prin->next;
	if (prin->next == 0) {
	    *last_item = 1;
	    *last_item_contains_data = 0;
	}
	rc = 1;
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
GetPrincipalFromCache(void *rpc_specific, int slot, void *dest,
		      afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    principal_get_p prin = (principal_get_p) rpc_specific;

    memcpy(dest, &prin->principal[slot], sizeof(kas_identity_t));
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * kas_PrincipalGetBegin - start the process of iterating over the entire
 * kas database.
 *
 * PARAMETERS
 *
 * IN cellHandle - a cellHandle previously returned by afsclient_CellOpen.
 *
 * IN serverHandle - a serverHandle previously returned by kas_ServerOpen.
 *
 * OUT iterationIdP - upon successful completion contains a iterator that
 * can be passed to kas_PrincipalGetNext.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 * ASSUMPTIONS
 */

int ADMINAPI
kas_PrincipalGetBegin(const void *cellHandle, const void *serverHandle,
		      void **iterationIdP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    kas_server_p k_handle = (kas_server_p) serverHandle;
    afs_admin_iterator_p iter =
	(afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    principal_get_p principal =
	(principal_get_p) malloc(sizeof(principal_get_t));

    /*
     * Validate arguments
     */

    if (iter == NULL) {
	tst = ADMNOMEM;
	goto fail_kas_PrincipalGetBegin;
    }

    if (principal == NULL) {
	tst = ADMNOMEM;
	goto fail_kas_PrincipalGetBegin;
    }

    if (iterationIdP == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_kas_PrincipalGetBegin;
    }

    if (!ChooseValidServer(c_handle, k_handle, &principal->kaserver, &tst)) {
	goto fail_kas_PrincipalGetBegin;
    }

    /*
     * Initialize the iterator structure
     */

    principal->current = 0;
    principal->next = 0;
    principal->count = 0;
    if (IteratorInit
	(iter, (void *)principal, GetPrincipalRPC, GetPrincipalFromCache,
	 NULL, DeletePrincipalSpecificData, &tst)) {
	*iterationIdP = (void *)iter;
	rc = 1;
    }

  fail_kas_PrincipalGetBegin:

    if (rc == 0) {
	if (iter != NULL) {
	    free(iter);
	}
	if (principal != NULL) {
	    free(principal);
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * kas_PrincipalGetNext - retrieve the next principal from the kaserver.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by kas_PrincipalGetBegin
 *
 * OUT who - upon successful completion contains the next principal from the
 * kaserver
 *
 * LOCKS
 *
 * Hold the iterator mutex across the call to the kaserver.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 * Returns 0 and st == ADMITERATORDONE when the last entry is returned.
 */

int ADMINAPI
kas_PrincipalGetNext(const void *iterationId, kas_identity_p who,
		     afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    /*
     * Validate arguments
     */

    if (who == NULL) {
	tst = ADMKASWHONULL;
	goto fail_kas_PrincipalGetNext;
    }

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_kas_PrincipalGetNext;
    }

    rc = IteratorNext(iter, (void *)who, &tst);

  fail_kas_PrincipalGetNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * kas_PrincipalGetDone - finish using a principal iterator
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by kas_PrincipalGetBegin
 *
 * LOCKS
 *
 * No locks are held by this function.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 * ASSUMPTIONS
 *
 * It is the user's responsibility to make sure kas_PrincipalGetDone
 * is called only once for each iterator.
 */

int ADMINAPI
kas_PrincipalGetDone(const void *iterationIdP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationIdP;

    /*
     * Validate argument
     */

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_kas_PrincipalGetDone;
    }

    rc = IteratorDone(iter, &tst);

  fail_kas_PrincipalGetDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * kas_PrincipalKeySet - set a principal's password to a known value.
 *
 * PARAMETERS
 *
 * IN cellHandle - a cellHandle previously returned by afsclient_CellOpen.
 *
 * IN serverHandle - a serverHandle previously returned by kas_ServerOpen.
 *
 * IN who - the principal for whom the password is being set.
 *
 * IN keyVersion - the version number of the new key.
 *
 * IN key - the new password.
 *
 * LOCKS
 *
 * No locks are held by this function.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
kas_PrincipalKeySet(const void *cellHandle, const void *serverHandle,
		    const kas_identity_p who, int keyVersion,
		    const kas_encryptionKey_p kas_keyp, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    kas_server_p k_handle = (kas_server_p) serverHandle;
    kas_server_t kaserver;
    EncryptionKey key; 

    /*
     * Validate input arguments and make rpc.
     */

    if (who == NULL) {
	tst = ADMKASWHONULL;
	goto fail_kas_PrincipalKeySet;
    }

    if (kas_keyp == NULL) {
	tst = ADMKASKEYNULL;
	goto fail_kas_PrincipalKeySet;
    }

    if (!ChooseValidServer(c_handle, k_handle, &kaserver, &tst)) {
	goto fail_kas_PrincipalKeySet;
    }

    memcpy(&key, kas_keyp, sizeof(key));

    tst =
	ubik_KAM_SetPassword(kaserver.servers, 0, who->principal,
		  who->instance, keyVersion, key);
    memset(&key, 0, sizeof(key));
    if (tst) {
	goto fail_kas_PrincipalKeySet;
    }

    /* If we failed to fail we must have succeeded */
    rc = 1;

  fail_kas_PrincipalKeySet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * kas_PrincipalLockStatusGet - determine a principal's lock status.
 *
 * PARAMETERS
 *
 * IN cellHandle - a cellHandle previously returned by afsclient_CellOpen.
 *
 * IN serverHandle - a serverHandle previously returned by kas_ServerOpen.
 *
 * IN who - the principal whose lock status is being checked.
 *
 * OUT lock_end_timeP - the number of seconds until the principal is unlocked.
 * If 0 => user is unlocked.
 *
 * LOCKS
 *
 * No locks are held by this function.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 * ASSUMPTIONS
 *
 * See the comments in GetPrincipalLockStatus regarding how the locking data
 * is kept INconsistently between servers.
 */

int ADMINAPI
kas_PrincipalLockStatusGet(const void *cellHandle, const void *serverHandle,
			   const kas_identity_p who,
			   unsigned int *lock_end_timeP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    kas_server_p k_handle = (kas_server_p) serverHandle;
    kas_server_t kaserver;

    /*
     * Validate input arguments and make rpc.
     */

    if (who == NULL) {
	tst = ADMKASWHONULL;
	goto fail_kas_PrincipalLockStatusGet;
    }

    if (lock_end_timeP == NULL) {
	tst = ADMKASLOCKENDTIMEPNULL;
	goto fail_kas_PrincipalLockStatusGet;
    }

    if (!ChooseValidServer(c_handle, k_handle, &kaserver, &tst)) {
	goto fail_kas_PrincipalLockStatusGet;
    }

    rc = GetPrincipalLockStatus(&kaserver, who, lock_end_timeP, &tst);

  fail_kas_PrincipalLockStatusGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * kas_PrincipalUnlock - unlock a principal.
 *
 * PARAMETERS
 *
 * IN cellHandle - a cellHandle previously returned by afsclient_CellOpen.
 *
 * IN serverHandle - a serverHandle previously returned by kas_ServerOpen.
 *
 * IN who - the principal who is being unlocked.
 *
 * LOCKS
 *
 * No locks are held by this function.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 * ASSUMPTIONS
 *
 * See the comments in GetPrincipalLockStatus regarding how the locking data
 * is kept INconsistently between servers.
 */

int ADMINAPI
kas_PrincipalUnlock(const void *cellHandle, const void *serverHandle,
		    const kas_identity_p who, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    kas_server_p k_handle = (kas_server_p) serverHandle;
    kas_server_t kaserver;
    int count = 0;
    afs_status_t save_tst = 0;

    /*
     * Validate input arguments and make rpc.
     */

    if (who == NULL) {
	tst = ADMKASWHONULL;
	goto fail_kas_PrincipalUnlock;
    }

    if (!ChooseValidServer(c_handle, k_handle, &kaserver, &tst)) {
	goto fail_kas_PrincipalUnlock;
    }

    do {
	tst =
	    ubik_CallIter(KAM_Unlock, kaserver.servers, 0, &count,
			  (long)who->principal, (long)who->instance, 0, 0, 0,
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	if (tst && (tst != UNOSERVERS)) {
	    if (save_tst == 0) {
		save_tst = tst;	/* save the first failure */
	    }
	}
    } while (tst != UNOSERVERS);

    if ((tst == 0) || (tst == UNOSERVERS)) {
	rc = 1;
    }

  fail_kas_PrincipalUnlock:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
getPrincipalFlags(const void *cellHandle, const void *serverHandle,
		  const kas_identity_p who, afs_int32 * cur_flags,
		  afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    kas_server_p k_handle = (kas_server_p) serverHandle;
    kas_server_t kaserver;
    struct kaentryinfo tentry;

    if (!ChooseValidServer(c_handle, k_handle, &kaserver, &tst)) {
	goto fail_getPrincipalFlags;
    }

    tst =
	ubik_KAM_GetEntry(kaserver.servers, 0, who->principal,
		  who->instance, KAMAJORVERSION, &tentry);
    if (tst == 0) {
	*cur_flags = tentry.flags;
	rc = 1;
    }

  fail_getPrincipalFlags:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * kas_PrincipalFieldsSet - modify an existing principal.
 *
 * PARAMETERS
 *
 * IN cellHandle - a cellHandle previously returned by afsclient_CellOpen.
 *
 * IN serverHandle - a serverHandle previously returned by kas_ServerOpen.
 *
 * IN who - the principal who is being modified.
 *
 * IN isAdmin - the admin status of the principal.
 *
 * IN grantTickets - should the TGS issue tickets for the principal.
 *
 * IN canEncrypt - should the TGS allow the use of encryption via the 
 * principal's key.
 *
 * IN canChangePassword - should the principal be allowed to change their
 * own password?
 *
 * IN expirationDate - the date when the principal will expire.
 *
 * IN maxTicketLifetime - the maximum lifetime of a ticket issued for
 * the principal.
 *
 * IN passwordExpires - the maximum number of days a particular
 * password can be used.  The limit is 255, 0 => no expiration.
 *
 * IN passwordReuse - can a password be reused by this principal.
 *
 * IN failedPasswordAttempts - number of failed login attempts before
 * a principal is locked.  The limit is 255, 0 => no limit.
 *
 * IN failedPasswordLockTime - the number of seconds a principal is
 * locked once failedPasswordAttempts is reached.  Some bizarre rounding
 * occurs for this value, see kas for more details.
 *
 * LOCKS
 *
 * No locks are held by this function.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 * ASSUMPTIONS
 *
 * See the comments in GetPrincipalLockStatus regarding how the locking data
 * is kept INconsistently between servers.
 */

int ADMINAPI
kas_PrincipalFieldsSet(const void *cellHandle, const void *serverHandle,
		       const kas_identity_p who, const kas_admin_p isAdmin,
		       const kas_tgs_p grantTickets,
		       const kas_enc_p canEncrypt,
		       const kas_cpw_p canChangePassword,
		       const unsigned int *expirationDate,
		       const unsigned int *maxTicketLifetime,
		       const unsigned int *passwordExpires,
		       const kas_rpw_p passwordReuse,
		       const unsigned int *failedPasswordAttempts,
		       const unsigned int *failedPasswordLockTime,
		       afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    kas_server_p k_handle = (kas_server_p) serverHandle;
    kas_server_t kaserver;
    afs_int32 flags = 0;
    Date expiration = 0;
    afs_int32 lifetime = 0;
    int was_spare;
    char spare_bytes[4] = { 0, 0, 0, 0 };
    int somethings_changing = 0;

    /*
     * Validate input arguments.
     */

    if (who == NULL) {
	tst = ADMKASWHONULL;
	goto fail_kas_PrincipalFieldsSet;
    }

    /*
     * set flags based upon input
     *
     * If we're changing the flags, we need to get the current value of
     * the flags first and then make the changes
     */

    if ((isAdmin != NULL) || (grantTickets != NULL) || (canEncrypt != NULL)
	|| (canChangePassword != NULL)) {
	if (!getPrincipalFlags(cellHandle, serverHandle, who, &flags, &tst)) {
	    goto fail_kas_PrincipalFieldsSet;
	}
    }

    if (isAdmin != NULL) {
	somethings_changing = 1;
	if (*isAdmin == KAS_ADMIN) {
	    flags |= KAFADMIN;
	} else {
	    flags &= ~KAFADMIN;
	}
    }

    if (grantTickets != NULL) {
	somethings_changing = 1;
	if (*grantTickets == NO_TGS) {
	    flags |= KAFNOTGS;
	} else {
	    flags &= ~KAFNOTGS;
	}
    }

    if (canEncrypt != NULL) {
	somethings_changing = 1;
	if (*canEncrypt == NO_ENCRYPT) {
	    flags |= KAFNOSEAL;
	} else {
	    flags &= ~KAFNOSEAL;
	}
    }

    if (canChangePassword != NULL) {
	somethings_changing = 1;
	if (*canChangePassword == NO_CHANGE_PASSWORD) {
	    flags |= KAFNOCPW;
	} else {
	    flags &= ~KAFNOCPW;
	}
    }

    flags = (flags & KAF_SETTABLE_FLAGS) | KAFNORMAL;

    if (expirationDate != NULL) {
	somethings_changing = 1;
	expiration = *expirationDate;
    }

    if (maxTicketLifetime != NULL) {
	somethings_changing = 1;
	lifetime = *maxTicketLifetime;
    }

    if (passwordExpires != NULL) {
	if (*passwordExpires > 255) {
	    tst = ADMKASPASSWORDEXPIRESTOOBIG;
	    goto fail_kas_PrincipalFieldsSet;
	}
	somethings_changing = 1;
	spare_bytes[0] = *passwordExpires + 1;
    }

    if (passwordReuse != NULL) {
	somethings_changing = 1;
	if (*passwordReuse == REUSE_PASSWORD) {
	    spare_bytes[1] = KA_REUSEPW;
	} else {
	    spare_bytes[1] = KA_NOREUSEPW;
	}
    }

    if (failedPasswordAttempts != NULL) {
	if (*failedPasswordAttempts > 255) {
	    tst = ADMKASFAILEDPASSWORDATTEMPTSTOOBIG;
	    goto fail_kas_PrincipalFieldsSet;
	}
	somethings_changing = 1;
	spare_bytes[2] = *failedPasswordAttempts + 1;
    }

    if (failedPasswordLockTime != NULL) {
	if (*failedPasswordLockTime > 36 * 60 * 60) {
	    tst = ADMKASFAILEDPASSWORDLOCKTIME;
	    goto fail_kas_PrincipalFieldsSet;
	}
	somethings_changing = 1;
	spare_bytes[3] = ((*failedPasswordLockTime + 511) >> 9) + 1;
    }

    was_spare = pack_long(spare_bytes);

    if (somethings_changing) {
	if (!ChooseValidServer(c_handle, k_handle, &kaserver, &tst)) {
	    goto fail_kas_PrincipalFieldsSet;
	}
	tst =
	    ubik_KAM_SetFields(kaserver.servers, 0, who->principal,
		      who->instance, flags, expiration, lifetime, -1,
		      was_spare, 0);
	if (tst == 0) {
	    rc = 1;
	}
    } else {
	tst = ADMKASPRINCIPALFIELDSNOCHANGE;
    }

  fail_kas_PrincipalFieldsSet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * kas_ServerStatsGet - get server statistics.
 *
 * PARAMETERS
 *
 * IN cellHandle - a cellHandle previously returned by afsclient_CellOpen.
 *
 * IN serverHandle - a serverHandle previously returned by kas_ServerOpen.
 *
 * OUT stats - the statistics retrieved.
 *
 * LOCKS
 *
 * No locks are held by this function.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
kas_ServerStatsGet(const void *cellHandle, const void *serverHandle,
		   kas_serverStats_p stats, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    kas_server_p k_handle = (kas_server_p) serverHandle;
    kas_server_t kaserver;
    afs_int32 admins;
    kasstats statics;
    kadstats dynamics;
    size_t i;

    /*
     * Validate input arguments and make rpc.
     */

    if (stats == NULL) {
	tst = ADMKASSTATSNULL;
	goto fail_kas_ServerStatsGet;
    }

    if (!ChooseValidServer(c_handle, k_handle, &kaserver, &tst)) {
	goto fail_kas_ServerStatsGet;
    }

    tst =
	ubik_KAM_GetStats(kaserver.servers, 0, KAMAJORVERSION, &admins,
		  &statics, &dynamics);
    if (tst) {
	goto fail_kas_ServerStatsGet;
    }

    stats->allocations = statics.allocs;
    stats->frees = statics.frees;
    stats->changePasswordRequests = statics.cpws;
    stats->adminAccounts = admins;
    stats->host = dynamics.host;
    stats->serverStartTime = dynamics.start_time;
    stats->hashTableUtilization = dynamics.hashTableUtilization;

    i = sizeof(kas_serverProcStats_t);
    memcpy(&stats->authenticate, &dynamics.Authenticate, i);
    memcpy(&stats->changePassword, &dynamics.ChangePassword, i);
    memcpy(&stats->getTicket, &dynamics.GetTicket, i);
    memcpy(&stats->createUser, &dynamics.CreateUser, i);
    memcpy(&stats->setPassword, &dynamics.SetPassword, i);
    memcpy(&stats->setFields, &dynamics.SetFields, i);
    memcpy(&stats->deleteUser, &dynamics.DeleteUser, i);
    memcpy(&stats->getEntry, &dynamics.GetEntry, i);
    memcpy(&stats->listEntry, &dynamics.ListEntry, i);
    memcpy(&stats->getStats, &dynamics.GetStats, i);
    memcpy(&stats->getPassword, &dynamics.GetPassword, i);
    memcpy(&stats->getRandomKey, &dynamics.GetRandomKey, i);
    memcpy(&stats->debug, &dynamics.Debug, i);
    memcpy(&stats->udpAuthenticate, &dynamics.UAuthenticate, i);
    memcpy(&stats->udpGetTicket, &dynamics.UGetTicket, i);
    memcpy(&stats->unlock, &dynamics.Unlock, i);
    memcpy(&stats->lockStatus, &dynamics.LockStatus, i);

    stats->stringChecks = dynamics.string_checks;
    rc = 1;

  fail_kas_ServerStatsGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * kas_ServerDebugGet - get server debug info.
 *
 * PARAMETERS
 *
 * IN cellHandle - a cellHandle previously returned by afsclient_CellOpen.
 *
 * IN serverHandle - a serverHandle previously returned by kas_ServerOpen.
 *
 * OUT stats - the debug info retrieved.
 *
 * LOCKS
 *
 * No locks are held by this function.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
kas_ServerDebugGet(const void *cellHandle, const void *serverHandle,
		   kas_serverDebugInfo_p debug, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    kas_server_p k_handle = (kas_server_p) serverHandle;
    kas_server_t kaserver;
    struct ka_debugInfo info;
    int i;

    /*
     * Validate input arguments and make rpc.
     */

    if (debug == NULL) {
	tst = ADMKASDEBUGNULL;
	goto fail_kas_ServerDebugGet;
    }

    if (!ChooseValidServer(c_handle, k_handle, &kaserver, &tst)) {
	goto fail_kas_ServerDebugGet;
    }
    tst = ubik_KAM_Debug(kaserver.servers, 0, KAMAJORVERSION, 0, &info);
    if (tst) {
	goto fail_kas_ServerDebugGet;
    }
    debug->host = info.host;
    debug->serverStartTime = info.startTime;
    debug->currentTime = info.reserved1;
    debug->noAuth = info.noAuth;
    debug->lastTransaction = info.lastTrans;
    strcpy(debug->lastOperation, info.lastOperation);
    strcpy(debug->lastPrincipalAuth, info.lastAuth);
    strcpy(debug->lastPrincipalUDPAuth, info.lastUAuth);
    strcpy(debug->lastPrincipalTGS, info.lastTGS);
    strcpy(debug->lastPrincipalUDPTGS, info.lastUTGS);
    strcpy(debug->lastPrincipalAdmin, info.lastAdmin);
    strcpy(debug->lastServerTGS, info.lastTGSServer);
    strcpy(debug->lastServerUDPTGS, info.lastUTGSServer);
    debug->nextAutoCheckPointWrite = info.nextAutoCPW;
    debug->updatesRemainingBeforeAutoCheckPointWrite = info.updatesRemaining;
    debug->dbHeaderRead = info.dbHeaderRead;
    debug->dbVersion = info.dbVersion;
    debug->dbFreePtr = info.dbFreePtr;
    debug->dbEOFPtr = info.dbEofPtr;
    debug->dbKvnoPtr = info.dbKvnoPtr;
    debug->dbSpecialKeysVersion = info.dbSpecialKeysVersion;
    debug->dbHeaderLock = info.cheader_lock;
    debug->keyCacheLock = info.keycache_lock;
    debug->keyCacheVersion = info.kcVersion;
    debug->keyCacheSize = info.kcSize;
    debug->keyCacheUsed = info.kcUsed;
    for (i = 0; i < info.kcUsed; i++) {
	debug->keyCache[i].lastUsed = info.kcInfo[i].used;
	debug->keyCache[i].keyVersionNumber = info.kcInfo[i].kvno;
	debug->keyCache[i].primary = info.kcInfo[i].primary;
	debug->keyCache[i].keyCheckSum = info.kcInfo[i].keycksum;
	strcpy(debug->keyCache[i].principal, info.kcInfo[i].principal);
    }
    rc = 1;

  fail_kas_ServerDebugGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * kas_ServerRandomKeyGet - get a random key from a server.
 *
 * PARAMETERS
 *
 * IN cellHandle - a cellHandle previously returned by afsclient_CellOpen.
 *
 * IN serverHandle - a serverHandle previously returned by kas_ServerOpen.
 *
 * OUT key - a random key.
 *
 * LOCKS
 *
 * No locks are held by this function.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
kas_ServerRandomKeyGet(const void *cellHandle, const void *serverHandle,
		       kas_encryptionKey_p kas_keyp, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    kas_server_p k_handle = (kas_server_p) serverHandle;
    kas_server_t kaserver;
    EncryptionKey key;

    /*
     * Validate input arguments and make rpc.
     */

    if (kas_keyp == NULL) {
	tst = ADMKASKEYNULL;
	goto fail_kas_ServerRandomKeyGet;
    }

    if (!ChooseValidServer(c_handle, k_handle, &kaserver, &tst)) {
	goto fail_kas_ServerRandomKeyGet;
    }

    tst = ubik_KAM_GetRandomKey(kaserver.servers, 0, &key);
    if (tst) {
	goto fail_kas_ServerRandomKeyGet;
    }
    memcpy(kas_keyp, &key, sizeof(*kas_keyp));
    rc = 1;

  fail_kas_ServerRandomKeyGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * kas_StringToKey - turn a string key into a key.
 *
 * PARAMETERS
 *
 * IN cellName - the name of the cell where the key will be used.
 *
 * IN string - the string to be converted.
 *
 * OUT key - the encryption key.
 *
 * LOCKS
 *
 * No locks are held by this function.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
kas_StringToKey(const char *cellName, const char *string,
		kas_encryptionKey_p key, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    ka_StringToKey(string, cellName, (struct ktc_encryptionKey *)key);
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * kas_KeyCheckSum - compute the checksum of an encryption key.
 *
 * PARAMETERS
 *
 * IN key - the encryption key.
 *
 * OUT cksumP - key checksum
 *
 * LOCKS
 *
 * No locks are held by this function.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
kas_KeyCheckSum(const kas_encryptionKey_p key, unsigned int *cksumP,
		afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_uint32 cksum32;

    if ((tst = ka_KeyCheckSum((char *)key, &cksum32)) == 0) {
	*cksumP = cksum32;
	rc = 1;
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}
