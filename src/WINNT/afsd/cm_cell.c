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
#include <nb30.h>
#include <winsock2.h>
#endif /* !DJGPP */
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <osi.h>
#include <string.h>

#include "afsd.h"

osi_rwlock_t cm_cellLock;

cm_cell_t *cm_allCellsp;

/* function called as callback proc from cm_SearchCellFile.  Return 0 to
 * continue processing.
 */
long cm_AddCellProc(void *rockp, struct sockaddr_in *addrp, char *namep)
{
	cm_server_t *tsp;
	cm_serverRef_t *tsrp;
    cm_cell_t *cellp;
        
	cellp = rockp;

	/* if this server was previously created by fs setserverprefs */
	if ( tsp = cm_FindServer(addrp, CM_SERVER_VLDB))
	{
		if ( !tsp->cellp )
			tsp->cellp = cellp;
	}
	else
        tsp = cm_NewServer(addrp, CM_SERVER_VLDB, cellp);

	/* Insert the vlserver into a sorted list, sorted by server rank */
	tsrp = cm_NewServerRef(tsp);
	cm_InsertServerList(&cellp->vlServersp, tsrp);
    /* drop the allocation reference */
    lock_ObtainWrite(&cm_serverLock);
    tsrp->refCount--;
    lock_ReleaseWrite(&cm_serverLock);
	return 0;
}

/* load up a cell structure from the cell database, afsdcell.ini */
cm_cell_t *cm_GetCell(char *namep, long flags)
{
  return cm_GetCell_Gen(namep, NULL, flags);
}

cm_cell_t *cm_GetCell_Gen(char *namep, char *newnamep, long flags)
{
	cm_cell_t *cp;
    long code;
    static cellCounter = 1;		/* locked by cm_cellLock */
	int ttl;
	char fullname[200]="";

	if (!strcmp(namep,SMB_IOCTL_FILENAME_NOSLASH))
		return NULL;

	lock_ObtainWrite(&cm_cellLock);
	for (cp = cm_allCellsp; cp; cp=cp->nextp) {
		if (strcmp(namep, cp->namep) == 0) {
            strcpy(fullname, cp->namep);
            break;
		}
    }

	if ((!cp && (flags & CM_FLAG_CREATE))
#ifdef AFS_AFSDB_ENV
         /* if it's from DNS, see if it has expired */
         || (cp && (cp->flags & CM_CELLFLAG_DNS) 
         && ((cp->flags & CM_CELLFLAG_VLSERVER_INVALID) || (time(0) > cp->timeout)))
#endif
	  ) {
        int dns_expired = 0;
		if (!cp) {
            cp = malloc(sizeof(cm_cell_t));
            memset(cp, 0, sizeof(cm_cell_t));
        } 
        else {
            dns_expired = 1;
            /* must empty cp->vlServersp */
            lock_ObtainWrite(&cp->mx);
            cm_FreeServerList(&cp->vlServersp);
            cp->vlServersp = NULL;
            lock_ReleaseWrite(&cp->mx);
        }

        code = cm_SearchCellFile(namep, fullname, cm_AddCellProc, cp);
		if (code) {
            afsi_log("in cm_GetCell_gen cm_SearchCellFile(%s) returns code= %d fullname= %s", 
                      namep, code, fullname);

#ifdef AFS_AFSDB_ENV
            if (cm_dnsEnabled /*&& cm_DomainValid(namep)*/) {
                code = cm_SearchCellByDNS(namep, fullname, &ttl, cm_AddCellProc, cp);
                if ( code ) {
                    afsi_log("in cm_GetCell_gen cm_SearchCellByDNS(%s) returns code= %d fullname= %s", 
                             namep, code, fullname);
                    if (dns_expired) {
                        cp->flags |= CM_CELLFLAG_VLSERVER_INVALID;
                        cp = NULL;  /* set cp to NULL to indicate error */
                        goto done;
                    } 
                }
                else {   /* got cell from DNS */
                    cp->flags |= CM_CELLFLAG_DNS;
                    cp->flags &= ~CM_CELLFLAG_VLSERVER_INVALID;
                    cp->timeout = time(0) + ttl;
                }
            }
#endif
            if (cp && code) {     /* free newly allocated memory */
                free(cp);
                cp = NULL;
                goto done;
            }
		}

		/* randomise among those vlservers having the same rank*/ 
        cm_RandomizeServer(&cp->vlServersp);

#ifdef AFS_AFSDB_ENV
        if (dns_expired) {
            /* we want to preserve the full name and mutex.
             * also, cp is already in the cm_allCellsp list
             */
            goto done;
        }
#endif /* AFS_AFSDB_ENV */

        /* otherwise we found the cell, and so we're nearly done */
        lock_InitializeMutex(&cp->mx, "cm_cell_t mutex");

		/* copy in name */
        cp->namep = malloc(strlen(fullname)+1);
        strcpy(cp->namep, fullname);

		/* thread on global list */
        cp->nextp = cm_allCellsp;
        cm_allCellsp = cp;
                
        cp->cellID = cellCounter++;
    }

  done:
    /* fullname is not valid if cp == NULL */
	if (cp && newnamep)
        strcpy(newnamep, fullname);
	lock_ReleaseWrite(&cm_cellLock);
    return cp;
}

cm_cell_t *cm_FindCellByID(long cellID)
{
	cm_cell_t *cp;
	int ttl;
    int code;

	lock_ObtainWrite(&cm_cellLock);
	for(cp = cm_allCellsp; cp; cp=cp->nextp) {
		if (cellID == cp->cellID) 
            break;
    }

#ifdef AFS_AFSDB_ENV
	/* if it's from DNS, see if it has expired */
	if (cp && cm_dnsEnabled && (cp->flags & CM_CELLFLAG_DNS) && 
        ((cp->flags & CM_CELLFLAG_VLSERVER_INVALID) || (time(0) > cp->timeout))) {
        /* must empty cp->vlServersp */
        cm_FreeServerList(&cp->vlServersp);
        cp->vlServersp = NULL;

        code = cm_SearchCellByDNS(cp->namep, NULL, &ttl, cm_AddCellProc, cp);
        if (code == 0) {   /* got cell from DNS */
            cp->flags |= CM_CELLFLAG_DNS;
            cp->flags &= ~CM_CELLFLAG_VLSERVER_INVALID;
#ifdef DEBUG
            fprintf(stderr, "cell %s: ttl=%d\n", cp->namep, ttl);
#endif
            cp->timeout = time(0) + ttl;
        } else {
            cp->flags |= CM_CELLFLAG_VLSERVER_INVALID;
            cp = NULL;      /* return NULL to indicate failure */
        }
        /* if we fail to find it this time, we'll just do nothing and leave the
         * current entry alone 
         */
	}
#endif /* AFS_AFSDB_ENV */

	lock_ReleaseWrite(&cm_cellLock);	
    return cp;
}

void cm_InitCell(void)
{
	static osi_once_t once;
        
    if (osi_Once(&once)) {
		lock_InitializeRWLock(&cm_cellLock, "cell global lock");
        cm_allCellsp = NULL;
		osi_EndOnce(&once);
    }
}
void cm_ChangeRankCellVLServer(cm_server_t *tsp)
{
	cm_cell_t *cp;
	int code;

	cp = tsp->cellp;	/* cell that this vlserver belongs to */
	osi_assert(cp);

	lock_ObtainMutex(&cp->mx);
	code = cm_ChangeRankServer(&cp->vlServersp, tsp);

	if ( !code ) 		/* if the server list was rearranged */
	    cm_RandomizeServer(&cp->vlServersp);

	lock_ReleaseMutex(&cp->mx);
}

