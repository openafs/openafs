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
    ("$Header: /cvs/openafs/src/libadmin/adminutil/afs_utilAdmin.c,v 1.7.2.2 2005/07/11 19:09:46 shadow Exp $");

#include <afs/stds.h>
#include <afs/afs_Admin.h>
#include <stdio.h>
#include <string.h>
#include <afs/afs_Admin.h>
#include "afs_AdminInternal.h"
#include "afs_utilAdmin.h"
#include <afs/pthread_glock.h>
#include <afs/cellconfig.h>
#include <afs/dirpath.h>
#include <afs/com_err.h>
#include <afs/kautils.h>
#include <afs/cmd.h>
#include <afs/vlserver.h>
#include <afs/pterror.h>
#include <afs/bnode.h>
#include <afs/volser.h>
#include <afs/afsint.h>
#include <rx/rx.h>
#include <rx/rxstat.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

/*
 * AIX 4.2 has PTHREAD_CREATE_UNDETACHED and not PTHREAD_CREATE_JOINABLE
 *
 * This fix should be done more centrally, but there's no time right now.
 */
#if defined(AFS_AIX_ENV)
# if !defined(PTHREAD_CREATE_JOINABLE) && defined(PTHREAD_CREATE_UNDETACHED)
#  define PTHREAD_CREATE_JOINABLE PTHREAD_CREATE_UNDETACHED
# endif
#endif

#define ERRCODE_RANGE 8
static pthread_once_t error_init_once = PTHREAD_ONCE_INIT;
static int error_init_done;

static void
init_once(void)
{

    initialize_KA_error_table();
    initialize_RXK_error_table();
    initialize_KTC_error_table();
    initialize_ACFG_error_table();
    initialize_CMD_error_table();
    initialize_VL_error_table();
    initialize_PT_error_table();
    initialize_BZ_error_table();
    initialize_U_error_table();
    initialize_AB_error_table();
    initialize_AF_error_table();
    initialize_AL_error_table();
    initialize_AC_error_table();
    initialize_AK_error_table();
    initialize_AM_error_table();
    initialize_AP_error_table();
    initialize_AU_error_table();
    initialize_AV_error_table();
    initialize_VOLS_error_table();
    error_init_done = 1;
}

int ADMINAPI
util_AdminErrorCodeTranslate(afs_status_t errorCode, int langId,
			     const char **errorTextP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_int32 code;

    if (errorTextP == NULL) {
	tst = ADMUTILERRORTEXTPNULL;
	goto fail_util_AdminErrorCodeTranslate;
    }

    /*
     * Translate the error
     */

    if (!error_init_done)
	pthread_once(&error_init_once, init_once);
    code = (afs_int32) errorCode;
    *errorTextP = error_message(code);
    rc = 1;

  fail_util_AdminErrorCodeTranslate:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * The iterator functions and data for the database server retrieval functions.
 */

typedef struct database_server_get {
    int total;
    int index;
    struct afsconf_dir *conf;
    struct afsconf_cell cell;
    util_databaseServerEntry_t server[CACHED_ITEMS];
} database_server_get_t, *database_server_get_p;

static int
GetDatabaseServerRPC(void *rpc_specific, int slot, int *last_item,
		     int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    database_server_get_p serv = (database_server_get_p) rpc_specific;

    serv->server[slot].serverAddress =
	ntohl(serv->cell.hostAddr[serv->index].sin_addr.s_addr);
    strcpy(serv->server[slot].serverName, serv->cell.hostName[serv->index]);
    serv->index++;

    /*
     * See if we've processed all the entries
     */

    if (serv->index == serv->total) {
	*last_item = 1;
	*last_item_contains_data = 1;
    }
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
GetDatabaseServerFromCache(void *rpc_specific, int slot, void *dest,
			   afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    database_server_get_p serv = (database_server_get_p) rpc_specific;

    memcpy(dest, (const void *)&serv->server[slot],
	   sizeof(util_databaseServerEntry_t));

    rc = 1;
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
DestroyDatabaseServer(void *rpc_specific, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    database_server_get_p serv = (database_server_get_p) rpc_specific;

    afsconf_Close(serv->conf);
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_DatabaseServerGetBegin - begin iterating over the database
 * server machines in a cell.
 *
 * PARAMETERS
 *
 * IN cellName - the cell where database servers reside.
 *
 * OUT iterationIdP - upon successful completion contains an iterator that
 * can be passed to util_DatabaseServerGetNext.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
util_DatabaseServerGetBegin(const char *cellName, void **iterationIdP,
			    afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter =
	(afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    database_server_get_p serv =
	(database_server_get_p) calloc(1, sizeof(database_server_get_t));
    char copyCell[MAXCELLCHARS];

    /*
     * Validate arguments
     */

    if ((cellName == NULL) || (*cellName == 0)) {
	tst = ADMUTILCELLNAMENULL;
	goto fail_util_DatabaseServerGetBegin;
    }

    if (iterationIdP == NULL) {
	goto fail_util_DatabaseServerGetBegin;
    }

    if ((iter == NULL) || (serv == NULL)) {
	tst = ADMNOMEM;
	goto fail_util_DatabaseServerGetBegin;
    }

    /*
     * Fill in the serv structure
     */

    serv->conf = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);
    if (serv->conf == NULL) {
	tst = ADMUTILCANTOPENCELLSERVDB;
	goto fail_util_DatabaseServerGetBegin;
    }

    /*
     * We must copy the cellname because afsconf_GetCellInfo
     * actually writes over the cell name it is passed.
     */
    strncpy(copyCell, cellName, MAXCELLCHARS - 1);
    tst =
	afsconf_GetCellInfo(serv->conf, copyCell, AFSCONF_KAUTHSERVICE,
			    &serv->cell);
    if (tst != 0) {
	goto fail_util_DatabaseServerGetBegin;
    }

    serv->total = serv->cell.numServers;
    if (IteratorInit
	(iter, (void *)serv, GetDatabaseServerRPC, GetDatabaseServerFromCache,
	 NULL, DestroyDatabaseServer, &tst)) {
	*iterationIdP = (void *)iter;
    } else {
	goto fail_util_DatabaseServerGetBegin;
    }
    rc = 1;

  fail_util_DatabaseServerGetBegin:

    if (rc == 0) {
	if (iter != NULL) {
	    free(iter);
	}
	if (serv != NULL) {
	    free(serv);
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_DatabaseServerGetNext - get the next server address.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator returned by util_DatabaseServerGetBegin
 *
 * OUT serverAddressP - upon successful completion contains the next 
 * server address in the cell.
 *
 * LOCKS
 *
 * This function locks the iterator for the duration of its processing.
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */


int ADMINAPI
util_DatabaseServerGetNext(const void *iterationId,
			   util_databaseServerEntry_p serverP,
			   afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_util_DatabaseServerGetNext;
    }

    if (serverP == NULL) {
	tst = ADMUTILSERVERADDRESSPNULL;
	goto fail_util_DatabaseServerGetNext;
    }

    rc = IteratorNext(iter, (void *)serverP, &tst);

  fail_util_DatabaseServerGetNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_DatabaseServerGetDone - stop using a database iterator.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator returned by util_DatabaseServerGetBegin
 *
 * LOCKS
 *
 * This function locks the iterator for the duration of its processing.
 * And then destroys it before returning.
 *
 * CAUTIONS
 *
 * None.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
util_DatabaseServerGetDone(const void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    /*
     * Validate parameters
     */

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_util_DatabaseServerGetDone;
    }

    rc = IteratorDone(iter, &tst);

  fail_util_DatabaseServerGetDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * GetServerAddressFromName - translate a character string server name
 * to an integer representation of an IP address.
 *
 * PARAMETERS
 *
 * IN serverName - the character string server name in either foo.com
 * format, or 123.12.1.1 format.
 *
 * OUT serverAddress - an integer that is filled with the correct address
 * in host byte order upon successful completion.
 *
 * LOCKS
 * 
 * No locks are obtained or released by this function
 *
 * CAUTIONS
 *
 * On many platforms, gethostbyname is not thread safe.  Since we are
 * only working under NT for now I'll use it directly.  In future UNIX
 * ports, a wrapper function should be written to call the correct function
 * on the particular platform.
 * 
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
util_AdminServerAddressGetFromName(const char *serverName, int *serverAddress,
				   afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    struct hostent *server;
    int part1, part2, part3, part4;
    int num_converted;

    if ((serverName == NULL) || (*serverName == 0)) {
	tst = ADMUTILSERVERNAMENULL;
	goto fail_util_AdminServerAddressGetFromName;
    }

    if (serverAddress == NULL) {
	tst = ADMUTILSERVERADDRESSNULL;
	goto fail_util_AdminServerAddressGetFromName;
    }

    num_converted =
	sscanf(serverName, "%d.%d.%d.%d", &part1, &part2, &part3, &part4);
    if (num_converted == 4) {
	*serverAddress = (part1 << 24) | (part2 << 16) | (part3 << 8) | part4;
    } else {
	LOCK_GLOBAL_MUTEX;
	server = gethostbyname(serverName);
	if (server != NULL) {
	    memcpy((void *)serverAddress, (const void *)server->h_addr,
		   sizeof(int));
	    *serverAddress = ntohl(*serverAddress);
	} else {
	    tst = ADMUTILCANTGETSERVERNAME;
	    UNLOCK_GLOBAL_MUTEX;
	    goto fail_util_AdminServerAddressGetFromName;
	}
	UNLOCK_GLOBAL_MUTEX;
    }
    rc = 1;

  fail_util_AdminServerAddressGetFromName:

    if (st != NULL) {
	*st = tst;
    }

    return rc;
}

/*
 * This file contains functions that can be used to create iterator
 * functions within the api.  I have attempted to make these functions
 * generic so that they can be used across the admin components.
 *
 * The functions in this file are a generalized producer/consumer
 * implementation.  They manage access to a queue of data.  The only
 * assumption these functions make about this data is that it is 
 * stored in a queue which is implemented via an array.  These functions
 * know about the index into the array, but they know nothing about
 * the contents of the array.
 *
 * The data specific functions you implement will have to create some
 * data specific storage structure that will have an array
 * of size CACHED_ITEMS data items.  This structure will also need to
 * store any necessary parameters/state variables you need to issue the
 * rpc to retrieve the next item.
 *
 * In order to use the generic functions, you must implement four functions
 * for each type of data you wish to retrieve.  The functions are:
 *
 * validate_specific_data_func - this function is handed a void pointer
 * that points to the rpc specific data you've allocated.  The function
 * should examine the data for validity and return an appropriate error.
 * This function is called every time the iterator is validated.
 *
 * destroy_specific_data_func - this function is handed a void pointer
 * that points to the rpc specific data you've allocated.  It should
 * destroy any components of the specific data as required and then
 * return.  The void pointer is free'd by the generic functions.
 *
 * get_cached_data_func - this function is handed a void pointer 
 * that points to the rpc specific data you've allocated, an index
 * into the cache of the item to be copied to the caller, and a void
 * pointer where the cache item should be copied.
 *
 * make_rpc_func - this function is handed a void pointer that points
 * to the rpc specific data you've allocated, an index into the cache
 * of the item to be filled with the next retrieved item, and an int
 * pointer that should be set to 1 if there are no more items to be
 * retrieved.  The assumption made by the generic functions is that 
 * the last_item status requires an individual rpc - the data isn't 
 * piggybacked on the last data item.
 */

/*
 * IteratorDelete - delete an iterator.
 *
 * PARAMETERS
 *
 * IN interator - the iterator to delete.
 *
 * LOCKS
 *
 * No locks are held by this function.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

static int
IteratorDelete(afs_admin_iterator_p iter, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    if (pthread_mutex_destroy(&iter->mutex)) {
	tst = ADMMUTEXDESTROY;
	goto fail_IteratorDelete;
    }
    if (pthread_cond_destroy(&iter->add_item)) {
	tst = ADMCONDDESTROY;
	goto fail_IteratorDelete;
    }
    if (pthread_cond_destroy(&iter->remove_item)) {
	tst = ADMCONDDESTROY;
	goto fail_IteratorDelete;
    }
    iter->is_valid = 0;
    if (iter->destroy_specific != NULL) {
	iter->destroy_specific(iter->rpc_specific, &tst);
    }
    free(iter->rpc_specific);
    free(iter);
    rc = 1;

  fail_IteratorDelete:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * DataGet - the background thread that is spawned for every
 * IteratorBegin call that is successful.  This thread tries
 * to fetch the data from the server ahead of the
 * IteratorNext calls.
 *
 * PARAMETERS
 *
 * IN arg - the address of the iterator structure to be used for this
 * iteration.
 *
 * LOCKS
 *
 * The iterator mutex is used by this function to protect elements
 * of the iterator structure.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

static void *
DataGet(void *arg)
{
    afs_admin_iterator_p iter = (afs_admin_iterator_p) arg;
    void *rc = 0;
    afs_status_t tst = 0;
    int mutex_locked = 0;
    int last_item = 0;
    int last_item_contains_data = 0;

    if (pthread_mutex_lock(&iter->mutex)) {
	iter->st = ADMMUTEXLOCK;
	goto fail_DataGet;
    }

    mutex_locked = 1;

    while (1) {

	/*
	 * Check to see if there's room for this datum.  If not, wait
	 * on the consumer to free up another slot.
	 */

	while (iter->cache_slots_used == CACHED_ITEMS) {
	    if (pthread_cond_wait(&iter->remove_item, &iter->mutex)) {
		iter->st = ADMCONDWAIT;
		goto fail_DataGet;
	    }
	}

	/*
	 * Check to see if someone called Done and terminated the request.
	 * We could have gone to sleep above when the buffer was full and
	 * instead of being awoken because another slot is open, we were
	 * awoken because the request was terminated.
	 */

	if (iter->request_terminated) {
	    goto fail_DataGet;
	}

	if (pthread_mutex_unlock(&iter->mutex)) {
	    iter->st = ADMMUTEXUNLOCK;
	    goto fail_DataGet;
	}

	mutex_locked = 0;

	/*
	 * Make an rpc without holding the iter mutex
	 * We reference an item in the principal cache here without
	 * holding the mutex.  This is safe because:
	 * 1. The iter structure is ref counted and won't be deleted
	 *    from underneath us.
	 * 2. cache_queue_tail is always one item ahead of the consumer
	 *    thread so we are the only thread accessing this member.
	 */

	iter->make_rpc(iter->rpc_specific, iter->cache_queue_tail, &last_item,
		       &last_item_contains_data, &tst);

	if (pthread_mutex_lock(&iter->mutex)) {
	    iter->st = ADMMUTEXLOCK;
	    goto fail_DataGet;
	}

	mutex_locked = 1;

	/*
	 * Check to see if someone called Done and terminated the request
	 */

	if (iter->request_terminated) {
	    goto fail_DataGet;
	}

	/*
	 * Check the rc of the rpc, and see if there are no more items
	 * to be retrieved.
	 */

	if (tst != 0) {
	    iter->st = tst;
	    goto fail_DataGet;
	}

	/*
	 * Check to see if this is the last item produced by the rpc.
	 * If it isn't, add the item to the cache and proceed.
	 * If it is, check to see if the last item contains valid data.
	 *     If it contains valid data, we need to add it to our cache.
	 *     If it doesn't, we mark the iterator as complete.
	 */

	if ((!last_item) || ((last_item) && (last_item_contains_data))) {
	    iter->cache_queue_tail =
		(iter->cache_queue_tail + 1) % CACHED_ITEMS;
	    iter->cache_slots_used++;
	}
	if (last_item) {
	    iter->st = ADMITERATORDONE;
	    iter->done_iterating = 1;
	    /*
	     * There's a small chance that the consumer emptied the
	     * cache queue while we were making the last rpc and has
	     * since gone to sleep waiting for more data.  In this case
	     * there will never be more data so we signal him here.
	     */
	    pthread_cond_signal(&iter->add_item);
	    goto fail_DataGet;
	}


	/*
	 * If the cache was empty and we just added another item, signal
	 * the consumer
	 */

	if (iter->cache_slots_used == 1) {
	    if (pthread_cond_signal(&iter->add_item)) {
		iter->st = ADMCONDSIGNAL;
		goto fail_DataGet;
	    }
	}
    }


  fail_DataGet:

    /*
     * If we are exiting with an error, signal the consumer in the event
     * they were waiting for us to produce more data
     */

    if (tst != 0) {
	pthread_cond_signal(&iter->add_item);
    }

    if (mutex_locked) {
	pthread_mutex_unlock(&iter->mutex);
    }

    return rc;
}

/*
 * IsValidIterator - verify the validity of a afs_admin_iterator_t.
 *
 * PARAMETERS
 *
 * IN interator - the interator to be verified.
 *
 * LOCKS
 *
 * We assume the iter->mutex lock is already held.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

static int
IsValidIterator(const afs_admin_iterator_p iterator, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    /*
     * Validate input parameters
     */

    if (iterator == NULL) {
	tst = ADMITERATORNULL;
	goto fail_IsValidIterator;
    }

    if ((iterator->begin_magic != BEGIN_MAGIC)
	|| (iterator->end_magic != END_MAGIC)) {
	tst = ADMITERATORBADMAGICNULL;
	goto fail_IsValidIterator;
    }

    if (iterator->is_valid == 0) {
	tst = ADMITERATORINVALID;
	goto fail_IsValidIterator;
    }

    /*
     * Call the iterator specific validation function
     */

    if (iterator->validate_specific != NULL) {
	if (!iterator->validate_specific(iterator->rpc_specific, &tst)) {
	    goto fail_IsValidIterator;
	}
    }
    rc = 1;

  fail_IsValidIterator:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * IteratorNext - return the next datum in an interator.
 *
 * PARAMETERS
 *
 * IN interator - the iterator containing the data.
 *
 * IN dest - the address where the data should be copied.
 *
 * LOCKS
 *
 * Lock the iterator upon entry, and hold it during the duration of this
 * function.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int
IteratorNext(afs_admin_iterator_p iter, void *dest, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    int locked_iter = 0;

    /*
     * We have to lock the iterator before we validate it
     */

    if (pthread_mutex_lock(&iter->mutex)) {
	tst = ADMMUTEXLOCK;
	goto fail_IteratorNext;
    }

    locked_iter = 1;

    if (!IsValidIterator(iter, &tst)) {
	goto fail_IteratorNext;
    }

    if (iter->request_terminated == 1) {
	tst = ADMITERATORTERMINATED;
	goto fail_IteratorNext;
    }

    if ((iter->st != AFS_STATUS_OK) && (iter->st != ADMITERATORDONE)) {
	tst = iter->st;
	goto fail_IteratorNext;
    }

    /*
     * Check to see if there are any queue'd items.  If not, wait here
     * until signalled by the producer.
     */

    while (iter->cache_slots_used == 0) {

	/*
	 * Maybe the producer experienced an rpc failure.
	 */

	if ((!iter->done_iterating) && (iter->st != 0)) {
	    tst = iter->st;
	    goto fail_IteratorNext;
	}

	/*
	 * Maybe there are no queue'd items because the producer is done
	 */

	if (iter->done_iterating) {
	    tst = iter->st;
	    goto fail_IteratorNext;
	}

	if (pthread_cond_wait(&iter->add_item, &iter->mutex)) {
	    tst = ADMCONDWAIT;
	    goto fail_IteratorNext;
	}
    }

    /*
     * Copy the next cached item and update the cached item count
     * and the index into the cache array
     */

    if (!iter->
	get_cached_data(iter->rpc_specific, iter->cache_queue_head, dest,
			&tst)) {
	goto fail_IteratorNext;
    }

    iter->cache_queue_head = (iter->cache_queue_head + 1) % CACHED_ITEMS;
    iter->cache_slots_used--;

    /*
     * If the cache was full before we removed the item above, the
     * producer may have been waiting for us to remove an item.
     * Signal the producer letting him know that we've opened a slot
     * in the cache
     */

    if (iter->cache_slots_used == (CACHED_ITEMS - 1)) {
	if (pthread_cond_signal(&iter->remove_item)) {
	    tst = ADMCONDSIGNAL;
	    goto fail_IteratorNext;
	}
    }
    rc = 1;


  fail_IteratorNext:

    if (locked_iter == 1) {
	pthread_mutex_unlock(&iter->mutex);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * IteratorDone - mark the iterator done.
 *
 * PARAMETERS
 *
 * IN interator - the iterator to mark done.
 *
 * LOCKS
 *
 * Lock the iterator upon entry, and hold it during the duration of this
 * function.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int
IteratorDone(afs_admin_iterator_p iter, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    int mutex_locked = 1;

    if (pthread_mutex_lock(&iter->mutex)) {
	tst = ADMMUTEXLOCK;
	goto fail_IteratorDone;
    }

    mutex_locked = 1;


    if (!IsValidIterator(iter, &tst)) {
	goto fail_IteratorDone;
    }


    /*
     * Depending upon the status of the background worker thread,
     * we can either join with him immediately (if we know he has
     * terminated), or we need to tell him the request has been
     * terminated and then join with him.
     */

    if (!iter->done_iterating) {
	iter->request_terminated = 1;
	iter->cache_slots_used = 0;
	pthread_cond_signal(&iter->remove_item);
    }

    /*
     * We have to unlock the mutex to allow the background thread to
     * progress
     */

    if (pthread_mutex_unlock(&iter->mutex)) {
	tst = ADMMUTEXUNLOCK;
	goto fail_IteratorDone;
    }

    if (iter->make_rpc != NULL) {
	if (pthread_join(iter->bg_worker, (void **)0)) {
	    tst = ADMTHREADJOIN;
	    goto fail_IteratorDone;
	}
    }

    /*
     * We don't relock the mutex here since we are the only thread
     * that has access to the iter now
     */

    rc = IteratorDelete(iter, &tst);
    mutex_locked = 0;

  fail_IteratorDone:

    if (mutex_locked) {
	pthread_mutex_unlock(&iter->mutex);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * IteratorInit - initialize an iterator.
 *
 * PARAMETERS
 *
 * IN interator - the iterator to initialize.
 *
 * LOCKS
 *
 * No locks are held by this function.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int
IteratorInit(afs_admin_iterator_p iter, void *rpc_specific,
	     make_rpc_func make_rpc, get_cached_data_func get_cached_data,
	     validate_specific_data_func validate_specific_data,
	     destroy_specific_data_func destroy_specific_data,
	     afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    int mutex_inited = 0;
    int add_item_cond_inited = 0;
    int remove_item_cond_inited = 0;

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_IteratorInit;
    }

    if (rpc_specific == NULL) {
	tst = ADMITERATORRPCSPECIFICNULL;
	goto fail_IteratorInit;
    }

    /*
     * Initialize the iterator structure
     */
    iter->begin_magic = BEGIN_MAGIC;
    iter->end_magic = END_MAGIC;
    iter->is_valid = 1;
    iter->cache_slots_used = 0;
    iter->done_iterating = 0;
    iter->request_terminated = 0;
    iter->st = AFS_STATUS_OK;
    iter->cache_queue_head = 0;
    iter->cache_queue_tail = 0;
    iter->cache_slots_used = 0;
    iter->rpc_specific = rpc_specific;
    iter->make_rpc = make_rpc;
    iter->get_cached_data = get_cached_data;
    iter->validate_specific = validate_specific_data;
    iter->destroy_specific = destroy_specific_data;

    if (pthread_mutex_init(&iter->mutex, (const pthread_mutexattr_t *)0)) {
	tst = ADMMUTEXINIT;
	goto fail_IteratorInit;
    } else {
	mutex_inited = 1;
    }

    if (pthread_cond_init(&iter->add_item, (const pthread_condattr_t *)0)) {
	tst = ADMCONDINIT;
	goto fail_IteratorInit;
    } else {
	add_item_cond_inited = 1;
    }

    if (pthread_cond_init(&iter->remove_item, (const pthread_condattr_t *)0)) {
	tst = ADMCONDINIT;
	goto fail_IteratorInit;
    } else {
	remove_item_cond_inited = 1;
    }

    /*
     * Create a worker thread that will begin to query the server
     * and cache responses.
     */

    if (iter->make_rpc != NULL) {
	pthread_attr_t tattr;

	if (pthread_attr_init(&tattr)) {
	    tst = ADMTHREADATTRINIT;
	    goto fail_IteratorInit;
	}

	if (pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_JOINABLE)) {
	    tst = ADMTHREADATTRSETDETACHSTATE;
	    goto fail_IteratorInit;
	}

	if (pthread_create(&iter->bg_worker, &tattr, DataGet, (void *)iter)) {
	    tst = ADMTHREADCREATE;
	    goto fail_IteratorInit;
	}
    }
    rc = 1;

  fail_IteratorInit:

    if (rc == 0) {
	if (mutex_inited) {
	    pthread_mutex_destroy(&iter->mutex);
	}
	if (remove_item_cond_inited) {
	    pthread_cond_destroy(&iter->remove_item);
	}
	if (add_item_cond_inited) {
	    pthread_cond_destroy(&iter->add_item);
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

int ADMINAPI
CellHandleIsValid(const void *cellHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;

    /*
     * Validate input parameters
     */

    if (c_handle == NULL) {
	tst = ADMCLIENTCELLHANDLENULL;
	goto fail_CellHandleIsValid;
    }

    if ((c_handle->begin_magic != BEGIN_MAGIC)
	|| (c_handle->end_magic != END_MAGIC)) {
	tst = ADMCLIENTCELLHANDLEBADMAGIC;
	goto fail_CellHandleIsValid;
    }

    if (c_handle->is_valid == 0) {
	tst = ADMCLIENTCELLINVALID;
	goto fail_CellHandleIsValid;
    }
    rc = 1;

  fail_CellHandleIsValid:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * The iterator functions and data for the rpc statistic retrieval functions
 */

typedef struct rpc_stat_get {
    afs_uint32 clock_sec;
    afs_uint32 clock_usec;
    afs_uint32 index;
    afs_uint32 total;
    afs_uint32 clientVersion;
    afs_uint32 serverVersion;
    struct rpcStats stat_list;
    afs_RPCStats_t stats[CACHED_ITEMS];
    afs_uint32 *pointer;
} rpc_stat_get_t, *rpc_stat_get_p;

static void
UnmarshallRPCStats(afs_uint32 serverVersion, afs_uint32 ** ptrP,
		   afs_RPCUnion_p s)
{
    afs_uint32 *ptr;
    unsigned int hi, lo;

    /*
     * Server must always match lower versions. We are version 1.
     */
    ptr = *ptrP;
    s->stats_v1.remote_peer = *(ptr++);
    s->stats_v1.remote_port = *(ptr++);
    s->stats_v1.remote_is_server = *(ptr++);
    s->stats_v1.interfaceId = *(ptr++);
    s->stats_v1.func_total = *(ptr++);
    s->stats_v1.func_index = *(ptr++);
    hi = *(ptr++);
    lo = *(ptr++);
    hset64(s->stats_v1.invocations, hi, lo);
    hi = *(ptr++);
    lo = *(ptr++);
    hset64(s->stats_v1.bytes_sent, hi, lo);
    hi = *(ptr++);
    lo = *(ptr++);
    hset64(s->stats_v1.bytes_rcvd, hi, lo);
    s->stats_v1.queue_time_sum.sec = *(ptr++);
    s->stats_v1.queue_time_sum.usec = *(ptr++);
    s->stats_v1.queue_time_sum_sqr.sec = *(ptr++);
    s->stats_v1.queue_time_sum_sqr.usec = *(ptr++);
    s->stats_v1.queue_time_min.sec = *(ptr++);
    s->stats_v1.queue_time_min.usec = *(ptr++);
    s->stats_v1.queue_time_max.sec = *(ptr++);
    s->stats_v1.queue_time_max.usec = *(ptr++);
    s->stats_v1.execution_time_sum.sec = *(ptr++);
    s->stats_v1.execution_time_sum.usec = *(ptr++);
    s->stats_v1.execution_time_sum_sqr.sec = *(ptr++);
    s->stats_v1.execution_time_sum_sqr.usec = *(ptr++);
    s->stats_v1.execution_time_min.sec = *(ptr++);
    s->stats_v1.execution_time_min.usec = *(ptr++);
    s->stats_v1.execution_time_max.sec = *(ptr++);
    s->stats_v1.execution_time_max.usec = *(ptr++);
    *ptrP = ptr;
}

static int
GetRPCStatsRPC(void *rpc_specific, int slot, int *last_item,
	       int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    rpc_stat_get_p t = (rpc_stat_get_p) rpc_specific;

    t->stats[slot].clientVersion = t->clientVersion;
    t->stats[slot].serverVersion = t->serverVersion;
    t->stats[slot].statCount = t->total;

    /*
     * If the server stat version is greater than or equal to my version
     * number, it is required to return the values in the client's current
     * format
     */

    UnmarshallRPCStats(t->serverVersion, &t->pointer, &t->stats[slot].s);

    t->index++;

    /*
     * See if we've processed all the entries
     */

    if (t->index == t->total) {
	*last_item = 1;
	*last_item_contains_data = 1;
    }
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
GetRPCStatsFromCache(void *rpc_specific, int slot, void *dest,
		     afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    rpc_stat_get_p stat = (rpc_stat_get_p) rpc_specific;

    memcpy(dest, (const void *)&stat->stats[slot], sizeof(afs_RPCStats_t));

    rc = 1;
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
DestroyRPCStats(void *rpc_specific, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    rpc_stat_get_p stat = (rpc_stat_get_p) rpc_specific;

    if (stat->stat_list.rpcStats_val != NULL) {
	free(stat->stat_list.rpcStats_val);
    }
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_RPCStatsGetBegin - begin retrieving rpc stats for a process
 *
 * PARAMETERS
 *
 * IN conn - an rx connection to the process to be queried
 *
 * IN rpc - the function to call to make the actual rpc
 *
 * OUT iterationIdP - an iteration id that can be passed to
 * util_RPCStatsGetNext to get the next rpc stat
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_RPCStatsGetBegin(struct rx_connection *conn, int (*rpc) (),
		      void **iterationIdP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter =
	(afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    rpc_stat_get_p stat = (rpc_stat_get_p) malloc(sizeof(rpc_stat_get_t));

    if (conn == NULL) {
	tst = ADMRXCONNNULL;
	goto fail_util_RPCStatsGetBegin;
    }

    if (rpc == NULL) {
	tst = ADMRPCPTRNULL;
	goto fail_util_RPCStatsGetBegin;
    }

    if (iterationIdP == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_util_RPCStatsGetBegin;
    }

    if ((iter == NULL) || (stat == NULL)) {
	tst = ADMNOMEM;
	goto fail_util_RPCStatsGetBegin;
    }

    stat->stat_list.rpcStats_len = 0;
    stat->stat_list.rpcStats_val = 0;
    stat->index = 0;
    stat->clientVersion = RX_STATS_RETRIEVAL_VERSION;

    tst =
	(*rpc) (conn, stat->clientVersion, &stat->serverVersion,
		&stat->clock_sec, &stat->clock_usec, &stat->total,
		&stat->stat_list);

    if (tst != 0) {
	goto fail_util_RPCStatsGetBegin;
    }

    /*
     * If there are no statistics, just mark the iterator done and
     * return.
     */

    if (stat->stat_list.rpcStats_len == 0) {
	stat->pointer = NULL;
	if (!IteratorInit(iter, (void *)stat, NULL, NULL, NULL, NULL, &tst)) {
	    goto fail_util_RPCStatsGetBegin;
	}
	iter->done_iterating = 1;
	iter->st = ADMITERATORDONE;
    } else {
	stat->pointer = stat->stat_list.rpcStats_val;
	if (!IteratorInit
	    (iter, (void *)stat, GetRPCStatsRPC, GetRPCStatsFromCache, NULL,
	     DestroyRPCStats, &tst)) {
	    goto fail_util_RPCStatsGetBegin;
	}
    }
    *iterationIdP = (void *)iter;
    rc = 1;

  fail_util_RPCStatsGetBegin:

    if (rc == 0) {
	if (iter != NULL) {
	    free(iter);
	}
	if (stat != NULL) {
	    free(stat);
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_RPCStatsGetNext - retrieve the next rpc stat from the server
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by
 * util_RPCStatsGetBegin.
 *
 * OUT stats - upon successful completion contains the next set of stats
 * from the server
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_RPCStatsGetNext(const void *iterationId, afs_RPCStats_p stats,
		     afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iterationId == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_util_RPCStatsGetNext;
    }

    if (stats == NULL) {
	tst = ADMUTILRPCSTATSNULL;
	goto fail_util_RPCStatsGetNext;
    }

    rc = IteratorNext(iter, (void *)stats, &tst);

  fail_util_RPCStatsGetNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_RPCStatsGetDone - finish using a stats iterator
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by
 * util_RPCStatsGetBegin.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_RPCStatsGetDone(const void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iterationId == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_util_RPCStatsGetDone;
    }

    rc = IteratorDone(iter, &tst);

  fail_util_RPCStatsGetDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_RPCStatsStateGet - get the current state of rpc stat collection
 *
 * PARAMETERS
 *
 * IN conn - an rx connection to the process to be queried
 *
 * IN rpc - the function to call to make the actual rpc
 *
 * OUT state - the rpc stat collection state.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_RPCStatsStateGet(struct rx_connection *conn, int (*rpc) (),
		      afs_RPCStatsState_p state, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    if (conn == NULL) {
	tst = ADMRXCONNNULL;
	goto fail_util_RPCStatsStateGet;
    }

    if (rpc == NULL) {
	tst = ADMRPCPTRNULL;
	goto fail_util_RPCStatsStateGet;
    }

    if (state == NULL) {
	tst = ADMRPCSTATENULL;
	goto fail_util_RPCStatsStateGet;
    }

    tst = (*rpc) (conn, state);

    if (!tst) {
	rc = 1;
    }

  fail_util_RPCStatsStateGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_RPCStatsStateEnable - enable rpc stat collection
 * to enabled
 *
 * PARAMETERS
 *
 * IN conn - an rx connection to the process to be modified
 *
 * IN rpc - the function to call to make the actual rpc
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_RPCStatsStateEnable(struct rx_connection *conn, int (*rpc) (),
			 afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    if (conn == NULL) {
	tst = ADMRXCONNNULL;
	goto fail_util_RPCStatsStateEnable;
    }

    if (rpc == NULL) {
	tst = ADMRPCPTRNULL;
	goto fail_util_RPCStatsStateEnable;
    }

    tst = (*rpc) (conn);

    if (!tst) {
	rc = 1;
    }

  fail_util_RPCStatsStateEnable:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_RPCStatsStateDisable - set the current state of rpc stat collection
 * to disabled
 *
 * PARAMETERS
 *
 * IN conn - an rx connection to the process to be modified
 *
 * IN rpc - the function to call to make the actual rpc
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_RPCStatsStateDisable(struct rx_connection *conn, int (*rpc) (),
			  afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    if (conn == NULL) {
	tst = ADMRXCONNNULL;
	goto fail_util_RPCStatsStateDisable;
    }

    if (rpc == NULL) {
	tst = ADMRPCPTRNULL;
	goto fail_util_RPCStatsStateDisable;
    }

    tst = (*rpc) (conn);

    if (!tst) {
	rc = 1;
    }

  fail_util_RPCStatsStateDisable:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_RPCStatsClear - clear some or all of the fields in the rpc stat
 * collection at a server
 *
 * PARAMETERS
 *
 * IN conn - an rx connection to the process to be modified
 *
 * IN rpc - the function to call to make the actual rpc
 *
 * IN flag - a flag containing the fields to be cleared
 *
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_RPCStatsClear(struct rx_connection *conn, int (*rpc) (),
		   afs_RPCStatsClearFlag_t flag, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    if (conn == NULL) {
	tst = ADMRXCONNNULL;
	goto fail_util_RPCStatsClear;
    }

    if (rpc == NULL) {
	tst = ADMRPCPTRNULL;
	goto fail_util_RPCStatsClear;
    }

    tst = (*rpc) (conn, flag);

    if (!tst) {
	rc = 1;
    }

  fail_util_RPCStatsClear:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_RPCStatsVersionGet - get the current version of rpc stat collection
 *
 * PARAMETERS
 *
 * IN conn - an rx connection to the process to be modified
 *
 * OUT version - the version of rpc stat collection at the remote process
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_RPCStatsVersionGet(struct rx_connection *conn,
			afs_RPCStatsVersion_p version, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    if (conn == NULL) {
	tst = ADMRXCONNNULL;
	goto fail_util_RPCStatsVersionGet;
    }

    if (version == NULL) {
	tst = ADMRPCVERSIONNULL;
	goto fail_util_RPCStatsVersionGet;
    }

    tst = RXSTATS_QueryRPCStatsVersion(conn, version);

    if (!tst) {
	rc = 1;
    }

  fail_util_RPCStatsVersionGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * The iterator for listing CM server preferences
 */

typedef struct cm_srvr_pref_get {
    struct rx_connection *conn;
    afs_int32 index;
    afs_CMServerPref_t srvrPrefs[CACHED_ITEMS];
} cm_srvr_pref_get_t, *cm_srvr_pref_get_p;

static int
GetServerPrefsRPC(void *rpc_specific, int slot, int *last_item,
		  int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    cm_srvr_pref_get_p t = (cm_srvr_pref_get_p) rpc_specific;

    /*
     * Get the next entry in the list of server preferences.
     */
    tst =
	RXAFSCB_GetServerPrefs(t->conn, t->index, &t->srvrPrefs[slot].ipAddr,
			       &t->srvrPrefs[slot].ipRank);
    if (tst) {
	goto fail_GetServerPrefsRPC;
    }

    /*
     * See if we've processed all the entries
     */
    if (t->srvrPrefs[slot].ipAddr == 0xffffffff) {
	*last_item = 1;
	*last_item_contains_data = 0;
    } else {
	t->index += 1;
    }
    rc = 1;

  fail_GetServerPrefsRPC:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
GetServerPrefsFromCache(void *rpc_specific, int slot, void *dest,
			afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    cm_srvr_pref_get_p prefs = (cm_srvr_pref_get_p) rpc_specific;

    memcpy(dest, (const void *)&prefs->srvrPrefs[slot],
	   sizeof(afs_CMServerPref_t));

    rc = 1;
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_CMGetServerPrefsBegin - Begin listing cache manager server preferences
 *
 * PARAMETERS
 *
 * IN conn - an rx connection to the process to be queried
 *
 * OUT iterationIdP - an iteration id that can be passed to
 * util_CMGetServerPrefsNext to get the next server preference
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_CMGetServerPrefsBegin(struct rx_connection *conn, void **iterationIdP,
			   afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter;
    cm_srvr_pref_get_p pref;

    if (conn == NULL) {
	tst = ADMRXCONNNULL;
	goto fail_util_CMGetServerPrefsBegin;
    }

    iter = (afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    if (iter == NULL) {
	tst = ADMNOMEM;
	goto fail_util_CMGetServerPrefsBegin;
    }

    pref = (cm_srvr_pref_get_p) malloc(sizeof(cm_srvr_pref_get_t));
    if (pref == NULL) {
	free(iter);
	tst = ADMNOMEM;
	goto fail_util_CMGetServerPrefsBegin;
    }

    pref->conn = conn;
    pref->index = 0;
    if (!IteratorInit
	(iter, (void *)pref, GetServerPrefsRPC, GetServerPrefsFromCache, NULL,
	 NULL, &tst)) {
	free(iter);
	free(pref);
	goto fail_util_CMGetServerPrefsBegin;
    }
    *iterationIdP = (void *)iter;
    rc = 1;

  fail_util_CMGetServerPrefsBegin:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_CMGetServerPrefsNext - Get next entry in cache manager server
 *                             preferences
 *
 * PARAMETERS
 *
 * IN iterationId - Iteration id created by util_CMGetServerPrefsBegin.
 *
 * OUT prefs - Next entry in cache manager server preferences.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_CMGetServerPrefsNext(const void *iterationId, afs_CMServerPref_p prefs,
			  afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iterationId == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_util_CMGetServerPrefsNext;
    }

    rc = IteratorNext(iter, (void *)prefs, &tst);

  fail_util_CMGetServerPrefsNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_CMGetServerPrefsDone - Finish listing cache manager server
 *                             preferences
 *
 * PARAMETERS
 *
 * IN iterationId - Iteration id created by util_CMGetServerPrefsBegin.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_CMGetServerPrefsDone(const void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iterationId == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_util_CMGetServerPrefsDone;
    }

    rc = IteratorDone(iter, &tst);


  fail_util_CMGetServerPrefsDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * The iterator for listing CM CellServDB
 */

typedef struct cm_list_cell_get {
    struct rx_connection *conn;
    afs_int32 index;
    afs_CMListCell_t cell[CACHED_ITEMS];
} cm_list_cell_get_t, *cm_list_cell_get_p;

static int
ListCellsRPC(void *rpc_specific, int slot, int *last_item,
	     int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    cm_list_cell_get_p t = (cm_list_cell_get_p) rpc_specific;
    char *name;

    /*
     * Get the next entry in the CellServDB.
     */
    name = t->cell[slot].cellname;
    tst =
	RXAFSCB_GetCellServDB(t->conn, t->index, &name,
			      t->cell[slot].serverAddr);
    if (tst) {
	goto fail_ListCellsRPC;
    }
    strcpy(t->cell[slot].cellname, name);

    /*
     * See if we've processed all the entries
     */
    if (strlen(t->cell[slot].cellname) == 0) {
	*last_item = 1;
	*last_item_contains_data = 0;
    } else {
	t->index += 1;
    }
    rc = 1;

  fail_ListCellsRPC:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
ListCellsFromCache(void *rpc_specific, int slot, void *dest, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    cm_list_cell_get_p cell = (cm_list_cell_get_p) rpc_specific;

    memcpy(dest, (const void *)&cell->cell[slot], sizeof(afs_CMListCell_t));

    rc = 1;
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_CMListCellsBegin - Begin listing cache manager CellServDB
 *
 * PARAMETERS
 *
 * IN conn - an rx connection to the process to be queried
 *
 * OUT iterationIdP - an iteration id that can be passed to
 * util_CMListCellsNext to get the next cell
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_CMListCellsBegin(struct rx_connection *conn, void **iterationIdP,
		      afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter;
    cm_list_cell_get_p cell;

    if (conn == NULL) {
	tst = ADMRXCONNNULL;
	goto fail_util_CMListCellsBegin;
    }

    iter = (afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    if (iter == NULL) {
	tst = ADMNOMEM;
	goto fail_util_CMListCellsBegin;
    }

    cell = (cm_list_cell_get_p) malloc(sizeof(cm_list_cell_get_t));
    if (cell == NULL) {
	free(iter);
	tst = ADMNOMEM;
	goto fail_util_CMListCellsBegin;
    }

    cell->conn = conn;
    cell->index = 0;
    if (!IteratorInit
	(iter, (void *)cell, ListCellsRPC, ListCellsFromCache, NULL, NULL,
	 &tst)) {
	free(iter);
	free(cell);
	goto fail_util_CMListCellsBegin;
    }
    *iterationIdP = (void *)iter;
    rc = 1;

  fail_util_CMListCellsBegin:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_CMListCellsNext - Get next entry in cache manager cells
 *
 * PARAMETERS
 *
 * IN iterationId - Iteration id created by util_CMGetServerPrefsBegin.
 *
 * OUT cell - Next entry in cache manager cells.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_CMListCellsNext(const void *iterationId, afs_CMListCell_p cell,
		     afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iterationId == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_util_CMListCellsNext;
    }

    rc = IteratorNext(iter, (void *)cell, &tst);

  fail_util_CMListCellsNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_CMListCellsDone - Finish listing cache manager cells
 *
 * PARAMETERS
 *
 * IN iterationId - Iteration id created by util_CMGetServerPrefsBegin.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_CMListCellsDone(const void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iterationId == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_util_CMListCellsDone;
    }

    rc = IteratorDone(iter, &tst);


  fail_util_CMListCellsDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_CMLocalCell - Get the name of the cache manager's local cell.
 *
 * PARAMETERS
 *
 * IN conn - an rx connection to the process to be queried
 *
 * OUT cellName - the name of the cache manager's local cell.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_CMLocalCell(struct rx_connection *conn, afs_CMCellName_p cellName,
		 afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_CMCellName_p name;

    if (conn == NULL) {
	tst = ADMRXCONNNULL;
	goto fail_util_CMLocalCell;
    }

    if (cellName == NULL) {
	tst = ADMCLIENTCMCELLNAMENULL;
	goto fail_util_CMLocalCell;
    }

    name = cellName;
    tst = RXAFSCB_GetLocalCell(conn, &name);

    if (!tst) {
	rc = 1;
    }

  fail_util_CMLocalCell:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static void
UnmarshallCMClientConfig(afs_uint32 serverVersion, afs_uint32 * ptr,
			 afs_ClientConfigUnion_p config)
{
    /*
     * We currently only support version 1.
     */
    config->config_v1.nChunkFiles = *(ptr++);
    config->config_v1.nStatCaches = *(ptr++);
    config->config_v1.nDataCaches = *(ptr++);
    config->config_v1.nVolumeCaches = *(ptr++);
    config->config_v1.firstChunkSize = *(ptr++);
    config->config_v1.otherChunkSize = *(ptr++);
    config->config_v1.cacheSize = *(ptr++);
    config->config_v1.setTime = *(ptr++);
    config->config_v1.memCache = *(ptr++);
}

/*
 * util_CMClientConfig - Get the cache manager's configuration parameters.
 *
 * PARAMETERS
 *
 * IN conn - an rx connection to the process to be queried
 *
 * OUT config - the cache manager's configuration parameters.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_CMClientConfig(struct rx_connection *conn, afs_ClientConfig_p config,
		    afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_int32 allocbytes;
    struct cacheConfig tconfig;

    if (conn == NULL) {
	tst = ADMRXCONNNULL;
	goto fail_util_CMClientConfig;
    }

    if (config == NULL) {
	tst = ADMCLIENTCMCELLNAMENULL;
	goto fail_util_CMClientConfig;
    }

    config->clientVersion = AFS_CLIENT_RETRIEVAL_VERSION;
    tconfig.cacheConfig_val = NULL;
    tconfig.cacheConfig_len = 0;
    tst =
	RXAFSCB_GetCacheConfig(conn, config->clientVersion,
			       &config->serverVersion, &allocbytes, &tconfig);

    if (tst) {
	goto fail_util_CMClientConfig;
    }

    UnmarshallCMClientConfig(config->serverVersion, tconfig.cacheConfig_val,
			     &config->c);
    rc = 1;
    free(tconfig.cacheConfig_val);

  fail_util_CMClientConfig:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_RXDebugVersion - Get the rxdebug version string.
 *
 * PARAMETERS
 *
 * IN handle - an rxdebug handle for the process to be queried.
 *
 * OUT version - the rxdebug version string.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_RXDebugVersion(rxdebugHandle_p handle, rxdebugVersion_p version,
		    afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    int code;

    if (handle == NULL) {
	tst = ADMRXDEBUGHANDLENULL;
	goto fail_util_RXDebugVersion;
    }

    if (version == NULL) {
	tst = ADMRXDEBUGVERSIONNULL;
	goto fail_util_RXDebugVersion;
    }

    code =
	rx_GetServerVersion(handle->sock, handle->ipAddr, handle->udpPort,
			    UTIL_MAX_RXDEBUG_VERSION_LEN, version);
    if (code < 0) {
	tst = ADMCLIENTRXDEBUGTIMEOUT;
	goto fail_util_RXDebugVersion;
    }

    rc = 1;

  fail_util_RXDebugVersion:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_RXDebugSupportedStats - Get the rxdebug statistics supported by
 *                              the process.
 *
 * PARAMETERS
 *
 * IN handle - an rxdebug handle for the process to be queried.
 *
 * OUT supportedStats - bit mask with supported rxstats.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_RXDebugSupportedStats(rxdebugHandle_p handle,
			   afs_uint32 * supportedStats, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    struct rx_debugStats tstats;

    if (handle == NULL) {
	tst = ADMRXDEBUGHANDLENULL;
	goto fail_util_RXDebugSupportedStats;
    }

    if (supportedStats == NULL) {
	tst = ADMRXDEBUGSTATSNULL;
	goto fail_util_RXDebugSupportedStats;
    }

    if (handle->firstFlag) {
	rc = util_RXDebugBasicStats(handle, &tstats, &tst);
	if (!rc) {
	    goto fail_util_RXDebugSupportedStats;
	}
    }

    *supportedStats = handle->supportedStats;
    rc = 1;

  fail_util_RXDebugSupportedStats:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * util_RXDebugBasicStats - Get the basic rxdebug statistics for the process.
 *
 * PARAMETERS
 *
 * IN handle - an rxdebug handle for the process to be queried.
 *
 * OUT stats - Basic rxdebug statistics for the process.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_RXDebugBasicStats(rxdebugHandle_p handle, struct rx_debugStats *stats,
		       afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    int code;

    if (handle == NULL) {
	tst = ADMRXDEBUGHANDLENULL;
	goto fail_util_RXDebugBasicStats;
    }

    if (stats == NULL) {
	tst = ADMRXDEBUGSTATSNULL;
	goto fail_util_RXDebugBasicStats;
    }

    code =
	rx_GetServerDebug(handle->sock, handle->ipAddr, handle->udpPort,
			  stats, &handle->supportedStats);
    if (code < 0) {
	tst = ADMCLIENTRXDEBUGTIMEOUT;
	goto fail_util_RXDebugBasicStats;
    }

    handle->firstFlag = 0;
    rc = 1;

  fail_util_RXDebugBasicStats:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * util_RXDebugRxStats - Get the detailed rxdebug statistics for the process.
 *
 * PARAMETERS
 *
 * IN handle - an rxdebug handle for the process to be queried.
 *
 * OUT stats - Detailed rxdebug statistics for the process.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_RXDebugRxStats(rxdebugHandle_p handle, struct rx_stats *stats,
		    afs_uint32 * supportedValues, afs_status_p st)
{
    int rc = 0;
    int trc;
    afs_status_t tst = 0;
    int code;
    afs_uint32 tsupported;

    if (handle == NULL) {
	tst = ADMRXDEBUGHANDLENULL;
	goto fail_util_RXDebugRxStats;
    }

    if (supportedValues == NULL) {
	tst = ADMRXDEBUGSTATSNULL;
	goto fail_util_RXDebugRxStats;
    }

    if (stats == NULL) {
	tst = ADMRXDEBUGSTATSNULL;
	goto fail_util_RXDebugRxStats;
    }

    if (handle->firstFlag) {
	trc = util_RXDebugSupportedStats(handle, &tsupported, &tst);
	if (!trc) {
	    rc = trc;
	    goto fail_util_RXDebugRxStats;
	}
    }

    if (!(handle->supportedStats & RX_SERVER_DEBUG_RX_STATS)) {
	tst = ADMCLIENTRXDEBUGNOTSUPPORTED;
	goto fail_util_RXDebugRxStats;
    }

    code =
	rx_GetServerStats(handle->sock, handle->ipAddr, handle->udpPort,
			  stats, &handle->supportedStats);
    if (code < 0) {
	tst = ADMCLIENTRXDEBUGTIMEOUT;
	goto fail_util_RXDebugRxStats;
    }

    rc = 1;

  fail_util_RXDebugRxStats:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * The iterator for listing RXDebug connections
 */

typedef struct rxdebug_conn_item {
    struct rx_debugConn conn;
    afs_uint32 supportedValues;
} rxdebug_conn_item_t, *rxdebug_conn_item_p;

typedef struct rxdebug_conn_get {
    int allconns;
    rxdebugHandle_p handle;
    afs_int32 index;
    rxdebug_conn_item_t items[CACHED_ITEMS];
} rxdebug_conn_get_t, *rxdebug_conn_get_p;

static int
RXDebugConnsFromServer(void *rpc_specific, int slot, int *last_item,
		       int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    int code;
    afs_status_t tst = 0;
    rxdebug_conn_get_p t = (rxdebug_conn_get_p) rpc_specific;

    /*
     * Get the next entry the list of connections
     */
    code =
	rx_GetServerConnections(t->handle->sock, t->handle->ipAddr,
				t->handle->udpPort, &t->index, t->allconns,
				t->handle->supportedStats,
				&t->items[slot].conn,
				&t->items[slot].supportedValues);
    if (code < 0) {
	tst = ADMCLIENTRXDEBUGTIMEOUT;
	goto fail_ListCellsRPC;
    }

    /*
     * See if we've processed all the entries
     */
    if (t->items[slot].conn.cid == 0xffffffff) {
	*last_item = 1;
	*last_item_contains_data = 0;
    }
    rc = 1;

  fail_ListCellsRPC:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
RXDebugConnsFromCache(void *rpc_specific, int slot, void *dest,
		      afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    rxdebug_conn_get_p t = (rxdebug_conn_get_p) rpc_specific;

    memcpy(dest, (const void *)&t->items[slot], sizeof(rxdebug_conn_item_t));

    rc = 1;
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_RXDebugConnectionsBegin - Begin listing rxdebug connection information
 *                                for a process.
 *
 * PARAMETERS
 *
 * IN handle - an rxdebug handle for the process to be queried
 *
 * IN allcons - non-zero to list all connections. If zero, only
 *              "interesting" connections will be listed.
 *
 * OUT iterationIdP - an iteration id that can be passed to
 *                    util_RXDebugConnectionsNext.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_RXDebugConnectionsBegin(rxdebugHandle_p handle, int allconns,
			     void **iterationIdP, afs_status_p st)
{
    int rc = 0;
    int trc;
    afs_uint32 tsupported;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter;
    rxdebug_conn_get_p t;

    if (handle == NULL) {
	tst = ADMRXDEBUGHANDLENULL;
	goto fail_util_RXDebugConnectionsBegin;
    }

    iter = (afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    if (iter == NULL) {
	tst = ADMNOMEM;
	goto fail_util_RXDebugConnectionsBegin;
    }

    if (handle->firstFlag) {
	trc = util_RXDebugSupportedStats(handle, &tsupported, &tst);
	if (!trc) {
	    rc = trc;
	    goto fail_util_RXDebugConnectionsBegin;
	}
    }

    if (allconns && !(handle->supportedStats & RX_SERVER_DEBUG_ALL_CONN)) {
	tst = ADMCLIENTRXDEBUGNOTSUPPORTED;
	goto fail_util_RXDebugConnectionsBegin;
    }

    t = (rxdebug_conn_get_p) malloc(sizeof(rxdebug_conn_get_t));
    if (t == NULL) {
	free(iter);
	tst = ADMNOMEM;
	goto fail_util_RXDebugConnectionsBegin;
    }

    t->allconns = allconns;
    t->handle = handle;
    t->index = 0;
    if (!IteratorInit
	(iter, (void *)t, RXDebugConnsFromServer, RXDebugConnsFromCache, NULL,
	 NULL, &tst)) {
	goto fail_util_RXDebugConnectionsBegin;
    }
    *iterationIdP = (void *)iter;
    rc = 1;

  fail_util_RXDebugConnectionsBegin:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * util_RXDebugConnectionsNext - Get rxdebug information for the next
 *                               connection.
 *
 * PARAMETERS
 *
 * IN iterationId - Iteration id created by util_RXDebugConnectionsNext.
 *
 * OUT conn - Rxdebug information for the next connection.
 *
 * OUT supportedValues - Bit mask of supported rxdebug values.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_RXDebugConnectionsNext(const void *iterationId,
			    struct rx_debugConn *conn,
			    afs_uint32 * supportedValues, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    rxdebug_conn_item_t item;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iterationId == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_util_RXDebugConnectionsNext;
    }

    if (conn == NULL) {
	tst = ADMRXDEBUGHANDLENULL;
	goto fail_util_RXDebugConnectionsNext;
    }

    if (supportedValues == NULL) {
	tst = ADMRXDEBUGHANDLENULL;
	goto fail_util_RXDebugConnectionsNext;
    }

    rc = IteratorNext(iter, (void *)&item, &tst);
    if (!rc) {
	goto fail_util_RXDebugConnectionsNext;
    }

    *conn = item.conn;
    *supportedValues = item.supportedValues;

  fail_util_RXDebugConnectionsNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * util_RXDebugConnectionsDone - Finish listing rxdebug connection information.
 *
 * PARAMETERS
 *
 * IN iterationId - Iteration id created by util_RXDebugConnectionsBegin.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_RXDebugConnectionsDone(const void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    /*
     * Validate parameters
     */

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_util_RXDebugConnectionsDone;
    }

    rc = IteratorDone(iter, &tst);

  fail_util_RXDebugConnectionsDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;

}

/*
 * The iterator for listing RXDebug peer
 */

typedef struct rxdebug_peer_item {
    struct rx_debugPeer peer;
    afs_uint32 supportedValues;
} rxdebug_peer_item_t, *rxdebug_peer_item_p;

typedef struct rxdebug_peer_get {
    rxdebugHandle_p handle;
    afs_int32 index;
    rxdebug_peer_item_t items[CACHED_ITEMS];
} rxdebug_peer_get_t, *rxdebug_peer_get_p;

static int
RXDebugPeersFromServer(void *rpc_specific, int slot, int *last_item,
		       int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    int code;
    afs_status_t tst = 0;
    rxdebug_peer_get_p t = (rxdebug_peer_get_p) rpc_specific;

    /*
     * Get the next entry the list of peers
     */
    code =
	rx_GetServerPeers(t->handle->sock, t->handle->ipAddr,
			  t->handle->udpPort, &t->index,
			  t->handle->supportedStats, &t->items[slot].peer,
			  &t->items[slot].supportedValues);
    if (code < 0) {
	tst = ADMCLIENTRXDEBUGTIMEOUT;
	goto fail_ListCellsRPC;
    }

    /*
     * See if we've processed all the entries
     */
    if (t->items[slot].peer.host == 0xffffffff) {
	*last_item = 1;
	*last_item_contains_data = 0;
    }
    rc = 1;

  fail_ListCellsRPC:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
RXDebugPeersFromCache(void *rpc_specific, int slot, void *dest,
		      afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    rxdebug_peer_get_p t = (rxdebug_peer_get_p) rpc_specific;

    memcpy(dest, (const void *)&t->items[slot], sizeof(rxdebug_peer_item_t));

    rc = 1;
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * util_RXDebugPeersBegin - Begin listing rxdebug peer information for
 *                          a process.
 *
 * PARAMETERS
 *
 * IN handle - an rxdebug handle for the process to be queried
 *
 * OUT iterationIdP - an iteration id that can be passed to
 *                    util_RXDebugPeersNext.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_RXDebugPeersBegin(rxdebugHandle_p handle, void **iterationIdP,
		       afs_status_p st)
{
    int rc = 0;
    int trc;
    afs_uint32 tsupported;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter;
    rxdebug_peer_get_p t;

    if (handle == NULL) {
	tst = ADMRXDEBUGHANDLENULL;
	goto fail_util_RXDebugPeersBegin;
    }

    iter = (afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    if (iter == NULL) {
	tst = ADMNOMEM;
	goto fail_util_RXDebugPeersBegin;
    }

    if (handle->firstFlag) {
	trc = util_RXDebugSupportedStats(handle, &tsupported, &tst);
	if (!trc) {
	    rc = trc;
	    goto fail_util_RXDebugPeersBegin;
	}
    }

    if (!(handle->supportedStats & RX_SERVER_DEBUG_ALL_PEER)) {
	tst = ADMCLIENTRXDEBUGNOTSUPPORTED;
	goto fail_util_RXDebugPeersBegin;
    }

    t = (rxdebug_peer_get_p) malloc(sizeof(rxdebug_peer_get_t));
    if (t == NULL) {
	free(iter);
	tst = ADMNOMEM;
	goto fail_util_RXDebugPeersBegin;
    }

    t->handle = handle;
    t->index = 0;
    if (!IteratorInit
	(iter, (void *)t, RXDebugPeersFromServer, RXDebugPeersFromCache, NULL,
	 NULL, &tst)) {
	goto fail_util_RXDebugPeersBegin;
    }
    *iterationIdP = (void *)iter;
    rc = 1;

  fail_util_RXDebugPeersBegin:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_RXDebugPeersNext - Get rxdebug information for the next peer.
 *
 * PARAMETERS
 *
 * IN iterationId - Iteration id created by util_RXDebugPeersBegin.
 *
 * OUT peer - Rxdebug information for the next peer.
 *
 * OUT supportedValues - Bit mask of supported rxdebug values.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */
int ADMINAPI
util_RXDebugPeersNext(const void *iterationId, struct rx_debugPeer *peer,
		      afs_uint32 * supportedValues, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    rxdebug_peer_item_t item;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iterationId == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_util_RXDebugPeersNext;
    }

    if (peer == NULL) {
	tst = ADMRXDEBUGHANDLENULL;
	goto fail_util_RXDebugPeersNext;
    }

    if (supportedValues == NULL) {
	tst = ADMRXDEBUGHANDLENULL;
	goto fail_util_RXDebugPeersNext;
    }

    rc = IteratorNext(iter, (void *)&item, &tst);
    if (!rc) {
	goto fail_util_RXDebugPeersNext;
    }

    *peer = item.peer;
    *supportedValues = item.supportedValues;

  fail_util_RXDebugPeersNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * util_RXDebugPeersDone - Finish listing rxdebug peer information.
 *
 * PARAMETERS
 *
 * IN iterationId - Iteration id created by util_RXDebugPeersBegin.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
util_RXDebugPeersDone(const void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    /*
     * Validate parameters
     */

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_util_RXDebugPeersDone;
    }

    rc = IteratorDone(iter, &tst);

  fail_util_RXDebugPeersDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;

}
