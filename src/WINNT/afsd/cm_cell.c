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

/* if it's from DNS, see if it has expired 
 * and check to make sure we have a valid set of volume servers
 * this function must be called with a Write Lock on cm_cellLock
 */
cm_cell_t *cm_UpdateCell(cm_cell_t * cp)
{
    long code = 0;

    if (cp == NULL)
        return NULL;

    lock_ObtainMutex(&cp->mx);
    if ((cp->vlServersp == NULL 
#ifdef AFS_FREELANCE_CLIENT
          && !(cp->flags & CM_CELLFLAG_FREELANCE)
#endif
          ) || (time(0) > cp->timeout)
#ifdef AFS_AFSDB_ENV
        || (cm_dnsEnabled && (cp->flags & CM_CELLFLAG_DNS) &&
         ((cp->flags & CM_CELLFLAG_VLSERVER_INVALID)))
#endif
            ) {
        /* must empty cp->vlServersp */
        if (cp->vlServersp) {
            cm_FreeServerList(&cp->vlServersp);
            cp->vlServersp = NULL;
        }

        code = cm_SearchCellFile(cp->name, NULL, cm_AddCellProc, cp);
#ifdef AFS_AFSDB_ENV
        if (code) {
            if (cm_dnsEnabled) {
                int ttl;

                code = cm_SearchCellByDNS(cp->name, NULL, &ttl, cm_AddCellProc, cp);
                if (code == 0) {   /* got cell from DNS */
                    cp->flags |= CM_CELLFLAG_DNS;
                    cp->flags &= ~CM_CELLFLAG_VLSERVER_INVALID;
		    cp->timeout = time(0) + ttl;
#ifdef DEBUG
                    fprintf(stderr, "cell %s: ttl=%d\n", cp->name, ttl);
#endif
		} else {
                    /* if we fail to find it this time, we'll just do nothing and leave the
                     * current entry alone 
		     */
                    cp->flags |= CM_CELLFLAG_VLSERVER_INVALID;
                }
	    }
	} else 
#endif /* AFS_AFSDB_ENV */
	{
	    cp->timeout = time(0) + 7200;
	}	
    }
    lock_ReleaseMutex(&cp->mx);
    return code ? NULL : cp;
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
    char fullname[200]="";

    if (!strcmp(namep,SMB_IOCTL_FILENAME_NOSLASH))
        return NULL;

    lock_ObtainWrite(&cm_cellLock);
    for (cp = cm_data.allCellsp; cp; cp=cp->nextp) {
        if (stricmp(namep, cp->name) == 0) {
            strcpy(fullname, cp->name);
            break;
        }
    }   

    if (cp) {
        cp = cm_UpdateCell(cp);
    } else if (flags & CM_FLAG_CREATE) {
        if ( cm_data.currentCells >= cm_data.maxCells )
            osi_panic("Exceeded Max Cells", __FILE__, __LINE__);

        /* don't increment currentCells until we know that we 
         * are going to keep this entry 
         */
        cp = &cm_data.cellBaseAddress[cm_data.currentCells];
        memset(cp, 0, sizeof(cm_cell_t));
        cp->magic = CM_CELL_MAGIC;
        
        code = cm_SearchCellFile(namep, fullname, cm_AddCellProc, cp);
        if (code) {
            osi_Log3(afsd_logp,"in cm_GetCell_gen cm_SearchCellFile(%s) returns code= %d fullname= %s", 
                      osi_LogSaveString(afsd_logp,namep), code, osi_LogSaveString(afsd_logp,fullname));

#ifdef AFS_AFSDB_ENV
            if (cm_dnsEnabled) {
                int ttl;

                code = cm_SearchCellByDNS(namep, fullname, &ttl, cm_AddCellProc, cp);
                if ( code ) {
                    osi_Log3(afsd_logp,"in cm_GetCell_gen cm_SearchCellByDNS(%s) returns code= %d fullname= %s", 
                             osi_LogSaveString(afsd_logp,namep), code, osi_LogSaveString(afsd_logp,fullname));
                    cp = NULL;
                    goto done;
                } else {   /* got cell from DNS */
                    cp->flags |= CM_CELLFLAG_DNS;
                    cp->flags &= ~CM_CELLFLAG_VLSERVER_INVALID;
                    cp->timeout = time(0) + ttl;
                }
            } else {
#endif
                cp = NULL;
                goto done;
#ifdef AFS_AFSDB_ENV
	    }
#endif
        } else {
	    cp->timeout = time(0) + 7200;	/* two hour timeout */
	}

        /* randomise among those vlservers having the same rank*/ 
        cm_RandomizeServer(&cp->vlServersp);

        /* otherwise we found the cell, and so we're nearly done */
        lock_InitializeMutex(&cp->mx, "cm_cell_t mutex");

        /* copy in name */
        strncpy(cp->name, fullname, CELL_MAXNAMELEN);
        cp->name[CELL_MAXNAMELEN-1] = '\0';

        /* thread on global list */
        cp->nextp = cm_data.allCellsp;
        cm_data.allCellsp = cp;
           
        /* the cellID cannot be 0 */
        cp->cellID = ++cm_data.currentCells;
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

    lock_ObtainWrite(&cm_cellLock);
    for (cp = cm_data.allCellsp; cp; cp=cp->nextp) {
        if (cellID == cp->cellID) 
            break;
    }

    if (cp)
        cp = cm_UpdateCell(cp);

    lock_ReleaseWrite(&cm_cellLock);	
    return cp;
}

long 
cm_ValidateCell(void)
{
    cm_cell_t * cellp;
    afs_uint32 count;

    for (cellp = cm_data.allCellsp, count = 0; cellp; cellp=cellp->nextp, count++) {
        if ( cellp->magic != CM_CELL_MAGIC ) {
            afsi_log("cm_ValidateCell failure: cellp->magic != CM_CELL_MAGIC");
            fprintf(stderr, "cm_ValidateCell failure: cellp->magic != CM_CELL_MAGIC\n");
            return -1;
        }
        if ( count != 0 && cellp == cm_data.allCellsp ||
             count > cm_data.maxCells ) {
            afsi_log("cm_ValidateCell failure: cm_data.allCellsp infinite loop");
            fprintf(stderr, "cm_ValidateCell failure: cm_data.allCellsp infinite loop\n");
            return -2;
        }
    }

    if ( count != cm_data.currentCells ) {
        afsi_log("cm_ValidateCell failure: count != cm_data.currentCells");
        fprintf(stderr, "cm_ValidateCell failure: count != cm_data.currentCells\n");
        return -3;
    }
    
    return 0;
}


long 
cm_ShutdownCell(void)
{
    cm_cell_t * cellp;

    for (cellp = cm_data.allCellsp; cellp; cellp=cellp->nextp)
        lock_FinalizeMutex(&cellp->mx);

    return 0;
}


void cm_InitCell(int newFile, long maxCells)
{
    static osi_once_t once;
        
    if (osi_Once(&once)) {
        cm_cell_t * cellp;

        lock_InitializeRWLock(&cm_cellLock, "cell global lock");

        if ( newFile ) {
            cm_data.allCellsp = NULL;
            cm_data.currentCells = 0;
            cm_data.maxCells = maxCells;
        
#ifdef AFS_FREELANCE_CLIENT
            /* Generate a dummy entry for the Freelance cell whether or not 
             * freelance mode is being used in this session 
             */

            cellp = &cm_data.cellBaseAddress[cm_data.currentCells++];
            memset(cellp, 0, sizeof(cm_cell_t));
            cellp->magic = CM_CELL_MAGIC;

            lock_InitializeMutex(&cellp->mx, "cm_cell_t mutex");

            /* copy in name */
            strncpy(cellp->name, "Freelance.Local.Cell", CELL_MAXNAMELEN); /*safe*/

            /* thread on global list */
            cellp->nextp = cm_data.allCellsp;
            cm_data.allCellsp = cellp;
                
            cellp->cellID = AFS_FAKE_ROOT_CELL_ID;
            cellp->vlServersp = NULL;
            cellp->flags = CM_CELLFLAG_FREELANCE;
#endif  
        } else {
            for (cellp = cm_data.allCellsp; cellp; cellp=cellp->nextp) {
                lock_InitializeMutex(&cellp->mx, "cm_cell_t mutex");
                cellp->vlServersp = NULL;
            }
        }

        osi_EndOnce(&once);
    }
}

void cm_ChangeRankCellVLServer(cm_server_t *tsp)
{
    cm_cell_t *cp;
    int code;

    cp = tsp->cellp;	/* cell that this vlserver belongs to */
    if (cp) {
	lock_ObtainMutex(&cp->mx);
	code = cm_ChangeRankServer(&cp->vlServersp, tsp);

	if ( !code ) 		/* if the server list was rearranged */
	    cm_RandomizeServer(&cp->vlServersp);

	lock_ReleaseMutex(&cp->mx);
    }
}       

