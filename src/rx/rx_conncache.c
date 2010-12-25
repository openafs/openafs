/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implement caching of rx connections.
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <sys/types.h>
#include <errno.h>
#include "rx.h"

/*
 * We initialize rxi_connectionCache at compile time, so there is no
 * need to call queue_Init(&rxi_connectionCache).
 */
static struct rx_queue rxi_connectionCache = { &rxi_connectionCache,
    &rxi_connectionCache
};

#ifdef AFS_PTHREAD_ENV
#include <pthread.h>
/*
 * This mutex protects the following global variables:
 * rxi_connectionCache
 */

afs_kmutex_t rxi_connCacheMutex;
#define LOCK_CONN_CACHE MUTEX_ENTER(&rxi_connCacheMutex)
#define UNLOCK_CONN_CACHE MUTEX_EXIT(&rxi_connCacheMutex)
#else
#define LOCK_CONN_CACHE
#define UNLOCK_CONN_CACHE
#endif /* AFS_PTHREAD_ENV */

/*
 * convenience typedef - all the stuff that makes up an rx
 * connection
 */

typedef struct rx_connParts {
    unsigned int hostAddr;
    unsigned short port;
    unsigned short service;
    struct rx_securityClass *securityObject;
    int securityIndex;
} rx_connParts_t, *rx_connParts_p;

/*
 * Each element in the cache is represented by the following
 * structure.  I use an rx_queue to manipulate the cache entries.
 * inUse tracks the number of calls within this connection that
 * we know are in use.
 */

typedef struct cache_entry {
    struct rx_queue queue_header;
    struct rx_connection *conn;
    rx_connParts_t parts;
    int inUse;
    int hasError;
} cache_entry_t, *cache_entry_p;

/*
 * Locking hierarchy
 *
 *    rxi_connCacheMutex is the only mutex used by these functions.  It should
 *    be locked when manipulating the connection cache.
 */

/*
 * Internal functions
 */

/*
 * Compare two connections for equality
 */

static int
rxi_CachedConnectionsEqual(rx_connParts_p a, rx_connParts_p b)
{
    return ((a->hostAddr == b->hostAddr) && (a->port == b->port)
	    && (a->service == b->service)
	    && (a->securityObject == b->securityObject)
	    && (a->securityIndex == b->securityIndex));
}

/*
 * Check the cache for a connection
 */

static int
rxi_FindCachedConnection(rx_connParts_p parts, struct rx_connection **conn)
{
    int error = 0;
    cache_entry_p cacheConn, nCacheConn;

    for (queue_Scan(&rxi_connectionCache, cacheConn, nCacheConn, cache_entry)) {
	if ((rxi_CachedConnectionsEqual(parts, &cacheConn->parts))
	    && (cacheConn->inUse < RX_MAXCALLS)
	    && (cacheConn->hasError == 0)) {
	    cacheConn->inUse++;
	    *conn = cacheConn->conn;
	    error = 1;
	    break;
	}
    }
    return error;
}

/*
 * Create an rx connection and return it to the caller
 */

/*
 * Add a connection to the cache keeping track of the input
 * arguments that were used to create it
 */

static void
rxi_AddCachedConnection(rx_connParts_p parts, struct rx_connection **conn)
{
    cache_entry_p new_entry;

    if ((new_entry = (cache_entry_p) malloc(sizeof(cache_entry_t)))) {
	new_entry->conn = *conn;
	new_entry->parts = *parts;
	new_entry->inUse = 1;
	new_entry->hasError = 0;
	queue_Prepend(&rxi_connectionCache, new_entry);
    }

    /*
     * if malloc fails, we fail silently
     */

    return;
}

/*
 * Get a connection by first checking to see if any matching
 * available connections are stored in the cache.
 * Create a new connection if none are currently available.
 */

static int
rxi_GetCachedConnection(rx_connParts_p parts, struct rx_connection **conn)
{
    int error = 0;

    /*
     * Look to see if we have a cached connection
     *
     * Note - we hold the connection cache mutex for the entire
     * search/create/enter operation.  We want this entire block to
     * be atomic so that in the event four threads all pass through
     * this code at the same time only one actually allocates the
     * new connection and the other three experience cache hits.
     *
     * We intentionally slow down throughput in order to
     * increase the frequency of cache hits.
     */

    LOCK_CONN_CACHE;
    if (!rxi_FindCachedConnection(parts, conn)) {
	/*
	 * Create a new connection and enter it in the cache
	 */
	if ((*conn =
	     rx_NewConnection(parts->hostAddr, parts->port, parts->service,
			      parts->securityObject, parts->securityIndex))) {
	    rxi_AddCachedConnection(parts, conn);
	} else {
	    error = 1;
	}
    }
    UNLOCK_CONN_CACHE;
    return error;
}

/*
 * Delete remaining entries in the cache.
 * Note - only call this routine from rx_Finalize.
 */

void
rxi_DeleteCachedConnections(void)
{
    cache_entry_p cacheConn, nCacheConn;

    LOCK_CONN_CACHE;
    for (queue_Scan(&rxi_connectionCache, cacheConn, nCacheConn, cache_entry)) {
	if (!cacheConn)
	    break;
	queue_Remove(cacheConn);
	rxi_DestroyConnection(cacheConn->conn);
	free(cacheConn);
    }
    UNLOCK_CONN_CACHE;
}

/*
 * External functions
 */

/*
 * Hand back the caller a connection
 * The function has the same foot print and return values
 * as rx_NewConnection.
 */

struct rx_connection *
rx_GetCachedConnection(unsigned int remoteAddr, unsigned short port,
		       unsigned short service,
		       struct rx_securityClass *securityObject,
		       int securityIndex)
{
    struct rx_connection *conn = NULL;
    rx_connParts_t parts;

    parts.hostAddr = remoteAddr;
    parts.port = port;
    parts.service = service;
    parts.securityObject = securityObject;
    parts.securityIndex = securityIndex;
    /*
     * Get a connection matching the user's request
     * note we don't propagate the error returned by rxi_GetCachedConnection
     * since rx_NewConnection doesn't return errors either.
     */
    rxi_GetCachedConnection(&parts, &conn);

    return conn;
}

/*
 * Release a connection we previously handed out
 */

void
rx_ReleaseCachedConnection(struct rx_connection *conn)
{
    cache_entry_p cacheConn, nCacheConn;

    LOCK_CONN_CACHE;
    for (queue_Scan(&rxi_connectionCache, cacheConn, nCacheConn, cache_entry)) {
	if (conn == cacheConn->conn) {
	    cacheConn->inUse--;
	    /*
	     * check to see if the connection is in error.
	     * If it is, mark its cache entry so it won't be
	     * given out subsequently.  If nobody is using it, delete
	     * it from the cache
	     */
	    if (rx_ConnError(conn)) {
		cacheConn->hasError = 1;
		if (cacheConn->inUse == 0) {
		    queue_Remove(cacheConn);
		    rxi_DestroyConnection(cacheConn->conn);
		    free(cacheConn);
		}
	    }
	    break;
	}
    }
    UNLOCK_CONN_CACHE;
}
