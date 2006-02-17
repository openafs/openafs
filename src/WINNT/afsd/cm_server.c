/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#ifndef DJGPP
#include <windows.h>
#include <winsock2.h>
#include <nb30.h>
#else
#include <sys/socket.h>
#endif /* !DJGPP */
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include <osi.h>
#include <rx/rx.h>
#include "afsd.h"

osi_rwlock_t cm_serverLock;

cm_server_t *cm_allServersp;

void cm_CheckServers(long flags, cm_cell_t *cellp)
{
    /* ping all file servers, up or down, with unauthenticated connection,
     * to find out whether we have all our callbacks from the server still.
     * Also, ping down VLDBs.
     */
    cm_server_t *tsp;
    long code;
    long secs;
    long usecs;
    int doPing;
    int serverType;
    unsigned long now;
    int wasDown;
    cm_conn_t *connp;
    struct rx_connection * callp;

    lock_ObtainWrite(&cm_serverLock);
    for (tsp = cm_allServersp; tsp; tsp = tsp->allNextp) {
        cm_GetServerNoLock(tsp);
        lock_ReleaseWrite(&cm_serverLock);

        /* now process the server */
        lock_ObtainMutex(&tsp->mx);

        /* what time is it? */
        now = osi_Time();

        serverType = tsp->type;
        doPing = 0;
        wasDown = tsp->flags & CM_SERVERFLAG_DOWN;

        /* only do the ping if the cell matches the requested cell, or we're
         * matching all cells (cellp == NULL), and if we've requested to ping
         * this type of {up, down} servers.
         */
        if ((cellp == NULL || cellp == tsp->cellp) &&
             ((wasDown && (flags & CM_FLAG_CHECKDOWNSERVERS)) ||
               (!wasDown && (flags & CM_FLAG_CHECKUPSERVERS)))) {

            doPing = 1;
        }	/* we're supposed to check this up/down server */
        lock_ReleaseMutex(&tsp->mx);

        /* at this point, we've adjusted the server state, so do the ping and
         * adjust things.
         */
        if (doPing) {
            code = cm_ConnByServer(tsp, cm_rootUserp, &connp);
            if (code == 0) {
                /* now call the appropriate ping call.  Drop the timeout if
                 * the server is known to be down, so that we don't waste a
                 * lot of time retiming out down servers.
                 */
                if (wasDown)
                    rx_SetConnDeadTime(connp->callp, 10);
                if (serverType == CM_SERVER_VLDB) {
                    code = VL_ProbeServer(connp->callp);
                }
                else {
                    /* file server */
                    callp = cm_GetRxConn(connp);
                    code = RXAFS_GetTime(callp, &secs, &usecs);
                    rx_PutConnection(callp);
                }
                if (wasDown)
                    rx_SetConnDeadTime(connp->callp, ConnDeadtimeout);
                cm_PutConn(connp);
            }	/* got an unauthenticated connection to this server */

            lock_ObtainMutex(&tsp->mx);
            if (code >= 0) {
                /* mark server as up */
                tsp->flags &= ~CM_SERVERFLAG_DOWN;
            }
            else {
                /* mark server as down */
                tsp->flags |= CM_SERVERFLAG_DOWN;
		if (code != VRESTARTING)
		    cm_ForceNewConnections(tsp);
            }
            lock_ReleaseMutex(&tsp->mx);
        }

        /* also, run the GC function for connections on all of the
         * server's connections.
         */
        cm_GCConnections(tsp);

        lock_ObtainWrite(&cm_serverLock);
        cm_PutServerNoLock(tsp);
    }
    lock_ReleaseWrite(&cm_serverLock);
}       

void cm_InitServer(void)
{
    static osi_once_t once;
        
    if (osi_Once(&once)) {
        lock_InitializeRWLock(&cm_serverLock, "cm_serverLock");
        osi_EndOnce(&once);
    }
}

void cm_GetServer(cm_server_t *serverp)
{
    lock_ObtainWrite(&cm_serverLock);
    serverp->refCount++;
    lock_ReleaseWrite(&cm_serverLock);
}

void cm_GetServerNoLock(cm_server_t *serverp)
{
    serverp->refCount++;
}

void cm_PutServer(cm_server_t *serverp)
{
    lock_ObtainWrite(&cm_serverLock);
    osi_assert(serverp->refCount-- > 0);
    lock_ReleaseWrite(&cm_serverLock);
}

void cm_PutServerNoLock(cm_server_t *serverp)
{
    osi_assert(serverp->refCount-- > 0);
}

void cm_SetServerPrefs(cm_server_t * serverp)
{
    unsigned long	serverAddr; 	/* in host byte order */
    unsigned long	myAddr, myNet, mySubnet;/* in host byte order */
    unsigned long	netMask;
    int 		i;

    /* implement server prefs for fileservers only */
    if ( serverp->type == CM_SERVER_FILE )
    {
        int cm_noIPAddr;         /* number of client network interfaces */
        int cm_IPAddr[CM_MAXINTERFACE_ADDR];    /* client's IP address in host order */
        int cm_SubnetMask[CM_MAXINTERFACE_ADDR];/* client's subnet mask in host order*/
        int cm_NetMtu[CM_MAXINTERFACE_ADDR];    /* client's MTU sizes */
        int cm_NetFlags[CM_MAXINTERFACE_ADDR];  /* network flags */
        long code;

        /* get network related info */
        cm_noIPAddr = CM_MAXINTERFACE_ADDR;
        code = syscfg_GetIFInfo(&cm_noIPAddr,
                                 cm_IPAddr, cm_SubnetMask,
                                 cm_NetMtu, cm_NetFlags);

        serverAddr = ntohl(serverp->addr.sin_addr.s_addr);
        serverp->ipRank  = CM_IPRANK_LOW;	/* default setings */

        for ( i=0; i < cm_noIPAddr; i++)
        {
            /* loop through all the client's IP address and compare
            ** each of them against the server's IP address */

            myAddr = cm_IPAddr[i];
            if ( IN_CLASSA(myAddr) )
                netMask = IN_CLASSA_NET;
            else if ( IN_CLASSB(myAddr) )
                netMask = IN_CLASSB_NET;
            else if ( IN_CLASSC(myAddr) )
                netMask = IN_CLASSC_NET;
            else
                netMask = 0;

            myNet    =  myAddr & netMask;
            mySubnet =  myAddr & cm_SubnetMask[i];

            if ( (serverAddr & netMask) == myNet ) 
            {
                if ( (serverAddr & cm_SubnetMask[i]) == mySubnet)
                {
                    if ( serverAddr == myAddr ) 
                        serverp->ipRank = min(serverp->ipRank,
                                               CM_IPRANK_TOP);/* same machine */
                    else serverp->ipRank = min(serverp->ipRank,
                                                CM_IPRANK_HI); /* same subnet */
                }
                else serverp->ipRank = min(serverp->ipRank,CM_IPRANK_MED);
                /* same net */
            }	
            /* random between 0..15*/
            serverp->ipRank += min(serverp->ipRank, rand() % 0x000f);
        } /* and of for loop */
    }
    else 
        serverp->ipRank = 10000 + (rand() % 0x00ff); /* VL server */
}

cm_server_t *cm_NewServer(struct sockaddr_in *socketp, int type, cm_cell_t *cellp) {
    cm_server_t *tsp;

    osi_assert(socketp->sin_family == AF_INET);

    tsp = malloc(sizeof(*tsp));
    memset(tsp, 0, sizeof(*tsp));
    tsp->type = type;
    tsp->cellp = cellp;
    tsp->refCount = 1;
    lock_InitializeMutex(&tsp->mx, "cm_server_t mutex");
    tsp->addr = *socketp;
    tsp->flags = 0;	/* assume up until we attempt to contact it */

    cm_SetServerPrefs(tsp); 

    lock_ObtainWrite(&cm_serverLock); /* get server lock */
    tsp->allNextp = cm_allServersp;
    cm_allServersp = tsp;
    lock_ReleaseWrite(&cm_serverLock); /* release server lock */

    return tsp;
}

/* find a server based on its properties */
cm_server_t *cm_FindServer(struct sockaddr_in *addrp, int type)
{
    cm_server_t *tsp;

    osi_assert(addrp->sin_family == AF_INET);
        
    lock_ObtainWrite(&cm_serverLock);
    for (tsp = cm_allServersp; tsp; tsp=tsp->allNextp) {
        if (tsp->type == type &&
            tsp->addr.sin_addr.s_addr == addrp->sin_addr.s_addr) 
            break;
    }       

    /* bump ref count if we found the server */
    if (tsp) 
        cm_GetServerNoLock(tsp);

    /* drop big table lock */
    lock_ReleaseWrite(&cm_serverLock);
	
    /* return what we found */
    return tsp;
}       

cm_serverRef_t *cm_NewServerRef(cm_server_t *serverp)
{
    cm_serverRef_t *tsrp;

    cm_GetServer(serverp);
    tsrp = malloc(sizeof(*tsrp));
    tsrp->server = serverp;
    tsrp->status = not_busy;
    tsrp->next = NULL;
    tsrp->refCount = 1;

    return tsrp;
}

long cm_ChecksumServerList(cm_serverRef_t *serversp)
{
    long sum = 0;
    int first = 1;
    cm_serverRef_t *tsrp;

    lock_ObtainWrite(&cm_serverLock);
    for (tsrp = serversp; tsrp; tsrp=tsrp->next) {
        if (first)
            first = 0;
        else
            sum <<= 1;
        sum ^= (long) tsrp->server;
    }

    lock_ReleaseWrite(&cm_serverLock);
    return sum;
}

/*
** Insert a server into the server list keeping the list sorted in 
** asending order of ipRank. 
** 
** The refCount of the cm_serverRef_t is increased
*/
void cm_InsertServerList(cm_serverRef_t** list, cm_serverRef_t* element)
{
    cm_serverRef_t	*current=*list;
    unsigned short ipRank = element->server->ipRank;

    lock_ObtainWrite(&cm_serverLock);
    element->refCount++;                /* increase refCount */

    /* insertion into empty list  or at the beginning of the list */
    if ( !current || (current->server->ipRank > ipRank) )
    {
        element->next = *list;
        *list = element;
        lock_ReleaseWrite(&cm_serverLock);
        return ;	
    }
	
    while ( current->next ) /* find appropriate place to insert */
    {
        if ( current->next->server->ipRank > ipRank )
            break;
        else current = current->next;
    }
    element->next = current->next;
    current->next = element;
    lock_ReleaseWrite(&cm_serverLock);
}       
/*
** Re-sort the server list with the modified rank
** returns 0 if element was changed successfully. 
** returns 1 if  list remained unchanged.
*/
long cm_ChangeRankServer(cm_serverRef_t** list, cm_server_t*	server)
{
    cm_serverRef_t  **current=list;
    cm_serverRef_t	*element=0;

    /* if there is max of one element in the list, nothing to sort */
    if ( (!*current) || !((*current)->next)  )
        return 1;		/* list unchanged: return success */

    lock_ObtainWrite(&cm_serverLock);
    /* if the server is on the list, delete it from list */
    while ( *current )
    {
        if ( (*current)->server == server)
        {
            element = (*current);
            *current = (*current)->next; /* delete it */
            break;
        }
        current = & ( (*current)->next);	
    }
    lock_ReleaseWrite(&cm_serverLock);

    /* if this volume is not replicated on this server  */
    if (!element)
        return 1;	/* server is not on list */

    /* re-insert deleted element into the list with modified rank*/
    cm_InsertServerList(list, element);

    /* reduce refCount which was increased by cm_InsertServerList */
    lock_ObtainWrite(&cm_serverLock);
    element->refCount--;
    lock_ReleaseWrite(&cm_serverLock);
    return 0;
}
/*
** If there are more than one server on the list and the first n servers on 
** the list have the same rank( n>1), then randomise among the first n servers.
*/
void cm_RandomizeServer(cm_serverRef_t** list)
{
    int 		count, picked;
    cm_serverRef_t*	tsrp = *list, *lastTsrp;
    unsigned short	lowestRank;

    /* an empty list or a list with only one element */
    if ( !tsrp || ! tsrp->next )
        return ; 

    lock_ObtainWrite(&cm_serverLock);

    /* count the number of servers with the lowest rank */
    lowestRank = tsrp->server->ipRank;
    for ( count=1, tsrp=tsrp->next; tsrp; tsrp=tsrp->next)
    {
        if ( tsrp->server->ipRank != lowestRank)
            break;
        else
            count++;
    }       	

    /* if there is only one server with the lowest rank, we are done */
    if ( count <= 1 ) {
        lock_ReleaseWrite(&cm_serverLock);
        return ;
    }   

    picked = rand() % count;
    if ( !picked ) {
        lock_ReleaseWrite(&cm_serverLock);
        return ;
    }   

    tsrp = *list;
    while (--picked >= 0)
    {
        lastTsrp = tsrp;
        tsrp = tsrp->next;
    }
    lastTsrp->next = tsrp->next;  /* delete random element from list*/
    tsrp->next     = *list; /* insert element at the beginning of list */
    *list          = tsrp;
    lock_ReleaseWrite(&cm_serverLock);
}       

/* call cm_FreeServer while holding a write lock on cm_serverLock */
void cm_FreeServer(cm_server_t* serverp)
{
    cm_PutServerNoLock(serverp);
    if (serverp->refCount == 0)
    {
        /* we need to check to ensure that all of the connections
         * for this server have a 0 refCount; otherwise, they will
         * not be garbage collected 
         */
        cm_GCConnections(serverp);  /* connsp */

	if (!(serverp->flags & CM_SERVERFLAG_PREF_SET)) {
	    lock_FinalizeMutex(&serverp->mx);
	    if ( cm_allServersp == serverp )
		cm_allServersp = serverp->allNextp;
	    else {
		cm_server_t *tsp;

		for(tsp = cm_allServersp; tsp->allNextp; tsp=tsp->allNextp) {
		    if ( tsp->allNextp == serverp ) {
			tsp->allNextp = serverp->allNextp;
			break;
		    }
		}
            }
	    free(serverp);
        }
    }
}

void cm_FreeServerList(cm_serverRef_t** list)
{
    cm_serverRef_t  **current = list;
    cm_serverRef_t  **nextp = 0;
    cm_serverRef_t  * next = 0;

    lock_ObtainWrite(&cm_serverLock);

    while (*current)
    {
        nextp = &(*current)->next;
        if (--((*current)->refCount) == 0) {
            next = *nextp;
            cm_FreeServer((*current)->server);
            free(*current);
            *current = next;
        } else {
           current = nextp;
        }
    }
  
    lock_ReleaseWrite(&cm_serverLock);
}

