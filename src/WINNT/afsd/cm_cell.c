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

#include <windows.h>
#include <nb30.h>
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <osi.h>
#include <string.h>
#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>

#include "afsd.h"

osi_rwlock_t cm_cellLock;

/* function called as callback proc from cm_SearchCellFile.  Return 0 to
 * continue processing.
 *
 * At the present time the return value is ignored by the caller.
 */
long cm_AddCellProc(void *rockp, struct sockaddr_in *addrp, char *hostnamep, unsigned short ipRank)
{
    cm_server_t *tsp;
    cm_serverRef_t *tsrp;
    cm_cell_t *cellp;
    cm_cell_rock_t *cellrockp = (cm_cell_rock_t *)rockp;
    afs_uint32 probe;

    cellp = cellrockp->cellp;
    probe = !(cellrockp->flags & CM_FLAG_NOPROBE);

    /* if this server was previously created by fs setserverprefs */
    if ( tsp = cm_FindServer(addrp, CM_SERVER_VLDB, FALSE))
    {
        if ( !tsp->cellp )
            tsp->cellp = cellp;
        else if (tsp->cellp != cellp) {
            osi_Log3(afsd_logp, "found a vlserver %s associated with two cells named %s and %s",
                     osi_LogSaveString(afsd_logp,hostnamep),
                     osi_LogSaveString(afsd_logp,tsp->cellp->name),
                     osi_LogSaveString(afsd_logp,cellp->name));
        }
    }
    else
        tsp = cm_NewServer(addrp, CM_SERVER_VLDB, cellp, NULL, probe ? 0 : CM_FLAG_NOPROBE);

    if (ipRank)
        tsp->ipRank = ipRank;

    /* Insert the vlserver into a sorted list, sorted by server rank */
    tsrp = cm_NewServerRef(tsp, 0);
    cm_InsertServerList(&cellp->vlServersp, tsrp);

    return 0;
}

/* if it's from DNS, see if it has expired
 * and check to make sure we have a valid set of volume servers
 * this function must not be called with a lock on cm_cellLock
 */
cm_cell_t *cm_UpdateCell(cm_cell_t * cp, afs_uint32 flags)
{
    long code = 0;
    cm_cell_rock_t rock;
    afs_uint32 mxheld = 0;

    if (cp == NULL)
        return NULL;

    lock_ObtainMutex(&cp->mx);
    mxheld = 1;

#ifdef AFS_FREELANCE_CLIENT
    if (cp->flags & CM_CELLFLAG_FREELANCE) {
        lock_ReleaseMutex(&cp->mx);
        return cp;
    }
#endif

    if ((cp->vlServersp == NULL) ||
        (time(0) > cp->timeout) ||
        (cm_dnsEnabled &&
         (cp->flags & CM_CELLFLAG_DNS) &&
         ((cp->flags & CM_CELLFLAG_VLSERVER_INVALID))))
    {
        lock_ReleaseMutex(&cp->mx);
        mxheld = 0;

        /* must empty cp->vlServersp */
        if (cp->vlServersp)
            cm_FreeServerList(&cp->vlServersp, CM_FREESERVERLIST_DELETE);

        rock.cellp = cp;
        rock.flags = flags;
        code = cm_SearchCellRegistry(1, cp->name, NULL, cp->linkedName, cm_AddCellProc, &rock);
        if (code && code != CM_ERROR_FORCE_DNS_LOOKUP)
            code = cm_SearchCellFileEx(cp->name, NULL, cp->linkedName, cm_AddCellProc, &rock);
        if (code == 0) {
            lock_ObtainMutex(&cp->mx);
            mxheld = 1;
	    cp->timeout = time(0) + 7200;
        }
        else {
            if (cm_dnsEnabled) {
                int ttl;

                code = cm_SearchCellByDNS(cp->name, NULL, &ttl, cm_AddCellProc, &rock);
                if (code == 0) {   /* got cell from DNS */
                    lock_ObtainMutex(&cp->mx);
                    mxheld = 1;
                    _InterlockedOr(&cp->flags, CM_CELLFLAG_DNS);
                    _InterlockedAnd(&cp->flags, ~CM_CELLFLAG_VLSERVER_INVALID);
		    cp->timeout = time(0) + ttl;
#ifdef DEBUG
                    fprintf(stderr, "cell %s: ttl=%d\n", cp->name, ttl);
#endif
		} else {
                    /* if we fail to find it this time, we'll just do nothing and leave the
                     * current entry alone
		     */
                    lock_ObtainMutex(&cp->mx);
                    mxheld = 1;
                    _InterlockedOr(&cp->flags, CM_CELLFLAG_VLSERVER_INVALID);
                }
	    }
	}
    }

    if (code == 0)
        cm_RandomizeServer(&cp->vlServersp);

    if (mxheld)
        lock_ReleaseMutex(&cp->mx);

    return code ? NULL : cp;
}

/* load up a cell structure from the cell database, AFS_CELLSERVDB */
cm_cell_t *cm_GetCell(char *namep, afs_uint32 flags)
{
    return cm_GetCell_Gen(namep, NULL, flags);
}

void cm_FreeCell(cm_cell_t *cellp)
{
    lock_AssertWrite(&cm_cellLock);

    if (cellp->vlServersp)
        cm_FreeServerList(&cellp->vlServersp, CM_FREESERVERLIST_DELETE);
    cellp->name[0] = '\0';

    cellp->freeNextp = cm_data.freeCellsp;
    cm_data.freeCellsp = cellp;
}

cm_cell_t *cm_GetCell_Gen(char *namep, char *newnamep, afs_uint32 flags)
{
    cm_cell_t *cp, *cp2;
    long code;
    char fullname[CELL_MAXNAMELEN]="";
    char linkedName[CELL_MAXNAMELEN]="";
    char name[CELL_MAXNAMELEN]="";
    int  hasWriteLock = 0;
    int  hasMutex = 0;
    afs_uint32 hash;
    cm_cell_rock_t rock;
    size_t len;

    if (namep == NULL || !namep[0] || !strcmp(namep,CM_IOCTL_FILENAME_NOSLASH))
        return NULL;

    /*
     * Strip off any trailing dots at the end of the cell name.
     * Failure to do so results in an undesireable alias as the
     * result of DNS AFSDB record lookups where a trailing dot
     * has special meaning.
     */
    strncpy(name, namep, CELL_MAXNAMELEN);
    for (len = strlen(namep); len > 0 && namep[len-1] == '.'; len--) {
        name[len-1] = '\0';
    }
    if (len == 0)
        return NULL;
    namep = name;

    hash = CM_CELL_NAME_HASH(namep);

    lock_ObtainRead(&cm_cellLock);
    for (cp = cm_data.cellNameHashTablep[hash]; cp; cp=cp->nameNextp) {
        if (cm_stricmp_utf8(namep, cp->name) == 0) {
            strncpy(fullname, cp->name, CELL_MAXNAMELEN);
            fullname[CELL_MAXNAMELEN-1] = '\0';
            break;
        }
    }

    if (!cp) {
        for (cp = cm_data.allCellsp; cp; cp=cp->allNextp) {
            if (strnicmp(namep, cp->name, strlen(namep)) == 0) {
                strncpy(fullname, cp->name, CELL_MAXNAMELEN);
                fullname[CELL_MAXNAMELEN-1] = '\0';
                break;
            }
        }
    }

    if (cp) {
        lock_ReleaseRead(&cm_cellLock);
        cm_UpdateCell(cp, flags);
    } else if (flags & CM_FLAG_CREATE) {
        lock_ConvertRToW(&cm_cellLock);
        hasWriteLock = 1;

        /* when we dropped the lock the cell could have been added
         * to the list so check again while holding the write lock
         */
        for (cp = cm_data.cellNameHashTablep[hash]; cp; cp=cp->nameNextp) {
            if (cm_stricmp_utf8(namep, cp->name) == 0) {
                strncpy(fullname, cp->name, CELL_MAXNAMELEN);
                fullname[CELL_MAXNAMELEN-1] = '\0';
                break;
            }
        }

        if (cp)
            goto done;

        for (cp = cm_data.allCellsp; cp; cp=cp->allNextp) {
            if (strnicmp(namep, cp->name, strlen(namep)) == 0) {
                strncpy(fullname, cp->name, CELL_MAXNAMELEN);
                fullname[CELL_MAXNAMELEN-1] = '\0';
                break;
            }
        }

        if (cp) {
            lock_ReleaseWrite(&cm_cellLock);
            lock_ObtainMutex(&cp->mx);
            lock_ObtainWrite(&cm_cellLock);
            cm_AddCellToNameHashTable(cp);
            cm_AddCellToIDHashTable(cp);
            lock_ReleaseMutex(&cp->mx);
            goto done;
        }

        if ( cm_data.freeCellsp != NULL ) {
            cp = cm_data.freeCellsp;
            cm_data.freeCellsp = cp->freeNextp;

            /*
             * The magic, cellID, and mx fields are already set.
             */
        } else {
            if ( cm_data.currentCells >= cm_data.maxCells )
                osi_panic("Exceeded Max Cells", __FILE__, __LINE__);

            /* don't increment currentCells until we know that we
             * are going to keep this entry
             */
            cp = &cm_data.cellBaseAddress[cm_data.currentCells];
            memset(cp, 0, sizeof(cm_cell_t));
            cp->magic = CM_CELL_MAGIC;

            /* the cellID cannot be 0 */
            cp->cellID = ++cm_data.currentCells;

            /* otherwise we found the cell, and so we're nearly done */
            lock_InitializeMutex(&cp->mx, "cm_cell_t mutex", LOCK_HIERARCHY_CELL);
        }

        lock_ReleaseWrite(&cm_cellLock);
        hasWriteLock = 0;

        rock.cellp = cp;
        rock.flags = flags;
        code = cm_SearchCellRegistry(1, namep, fullname, linkedName, cm_AddCellProc, &rock);
        if (code && code != CM_ERROR_FORCE_DNS_LOOKUP)
            code = cm_SearchCellFileEx(namep, fullname, linkedName, cm_AddCellProc, &rock);
        if (code) {
            osi_Log4(afsd_logp,"in cm_GetCell_gen cm_SearchCellFileEx(%s) returns code= %d fullname= %s linkedName= %s",
                      osi_LogSaveString(afsd_logp,namep), code, osi_LogSaveString(afsd_logp,fullname),
                      osi_LogSaveString(afsd_logp,linkedName));

            if (cm_dnsEnabled) {
                int ttl;

                code = cm_SearchCellByDNS(namep, fullname, &ttl, cm_AddCellProc, &rock);
                if ( code ) {
                    osi_Log3(afsd_logp,"in cm_GetCell_gen cm_SearchCellByDNS(%s) returns code= %d fullname= %s",
                             osi_LogSaveString(afsd_logp,namep), code, osi_LogSaveString(afsd_logp,fullname));
                    lock_ObtainMutex(&cp->mx);
                    lock_ObtainWrite(&cm_cellLock);
                    hasWriteLock = 1;
                    cm_RemoveCellFromIDHashTable(cp);
                    cm_RemoveCellFromNameHashTable(cp);
                    lock_ReleaseMutex(&cp->mx);
                    cm_FreeCell(cp);
                    cp = NULL;
                    goto done;
                } else {   /* got cell from DNS */
                    lock_ObtainMutex(&cp->mx);
                    hasMutex = 1;
                    _InterlockedOr(&cp->flags, CM_CELLFLAG_DNS);
                    _InterlockedAnd(&cp->flags, ~CM_CELLFLAG_VLSERVER_INVALID);
                    cp->timeout = time(0) + ttl;
                }
            }
            else
            {
                lock_ObtainMutex(&cp->mx);
                lock_ObtainWrite(&cm_cellLock);
                hasWriteLock = 1;
                cm_RemoveCellFromIDHashTable(cp);
                cm_RemoveCellFromNameHashTable(cp);
                lock_ReleaseMutex(&cp->mx);
                cm_FreeCell(cp);
                cp = NULL;
                goto done;
	    }
        } else {
            lock_ObtainMutex(&cp->mx);
            hasMutex = 1;
	    cp->timeout = time(0) + 7200;	/* two hour timeout */
	}

        /* we have now been given the fullname of the cell.  It may
         * be that we already have a cell with that name.  If so,
         * we should use it instead of completing the allocation
         * of a new cm_cell_t
         */
        lock_ObtainRead(&cm_cellLock);
        hash = CM_CELL_NAME_HASH(fullname);
        for (cp2 = cm_data.cellNameHashTablep[hash]; cp2; cp2=cp2->nameNextp) {
            if (cm_stricmp_utf8(fullname, cp2->name) == 0) {
                break;
            }
        }

        if (cp2) {
            if (!hasMutex) {
                lock_ObtainMutex(&cp->mx);
                hasMutex = 1;
            }
            lock_ConvertRToW(&cm_cellLock);
            hasWriteLock = 1;
            cm_RemoveCellFromIDHashTable(cp);
            cm_RemoveCellFromNameHashTable(cp);
            lock_ReleaseMutex(&cp->mx);
            hasMutex = 0;
            cm_FreeCell(cp);
            cp = cp2;
            goto done;
        }
        lock_ReleaseRead(&cm_cellLock);

        /* randomise among those vlservers having the same rank*/
        cm_RandomizeServer(&cp->vlServersp);

        if (!hasMutex)
            lock_ObtainMutex(&cp->mx);

        /* copy in name */
        strncpy(cp->name, fullname, CELL_MAXNAMELEN);
        cp->name[CELL_MAXNAMELEN-1] = '\0';

        strncpy(cp->linkedName, linkedName, CELL_MAXNAMELEN);
        cp->linkedName[CELL_MAXNAMELEN-1] = '\0';

        lock_ObtainWrite(&cm_cellLock);
        hasWriteLock = 1;
        cm_AddCellToNameHashTable(cp);
        cm_AddCellToIDHashTable(cp);
        lock_ReleaseMutex(&cp->mx);
        hasMutex = 0;

        /* append cell to global list */
        if (cm_data.allCellsp == NULL) {
            cm_data.allCellsp = cp;
        } else {
            for (cp2 = cm_data.allCellsp; cp2->allNextp; cp2=cp2->allNextp)
                ;
            cp2->allNextp = cp;
        }
        cp->allNextp = NULL;

    } else {
        lock_ReleaseRead(&cm_cellLock);
    }
  done:
    if (hasMutex && cp)
        lock_ReleaseMutex(&cp->mx);
    if (hasWriteLock)
        lock_ReleaseWrite(&cm_cellLock);

    /* fullname is not valid if cp == NULL */
    if (newnamep) {
        if (cp) {
            strncpy(newnamep, fullname, CELL_MAXNAMELEN);
            newnamep[CELL_MAXNAMELEN-1]='\0';
        } else {
            newnamep[0] = '\0';
        }
    }

    if (cp && cp->linkedName[0]) {
        cm_cell_t * linkedCellp = NULL;

        if (!strcmp(cp->name, cp->linkedName)) {
            cp->linkedName[0] = '\0';
        } else if (!(flags & CM_FLAG_NOMOUNTCHASE)) {
            linkedCellp = cm_GetCell(cp->linkedName, CM_FLAG_CREATE|CM_FLAG_NOPROBE|CM_FLAG_NOMOUNTCHASE);

            lock_ObtainWrite(&cm_cellLock);
            if (!linkedCellp ||
                (linkedCellp->linkedName[0] && strcmp(cp->name, linkedCellp->linkedName))) {
                cp->linkedName[0] = '\0';
            } else {
                strncpy(linkedCellp->linkedName, cp->name, CELL_MAXNAMELEN);
                linkedCellp->linkedName[CELL_MAXNAMELEN-1]='\0';
            }
            lock_ReleaseWrite(&cm_cellLock);
        }
    }
    return cp;
}

cm_cell_t *cm_FindCellByID(afs_int32 cellID, afs_uint32 flags)
{
    cm_cell_t *cp;
    afs_uint32 hash;

    lock_ObtainRead(&cm_cellLock);

    hash = CM_CELL_ID_HASH(cellID);

    for (cp = cm_data.cellIDHashTablep[hash]; cp; cp=cp->idNextp) {
        if (cellID == cp->cellID)
            break;
    }
    lock_ReleaseRead(&cm_cellLock);

    if (cp)
        cm_UpdateCell(cp, flags);

    return cp;
}

long
cm_ValidateCell(void)
{
    cm_cell_t * cellp;
    afs_uint32 count1, count2;

    for (cellp = cm_data.allCellsp, count1 = 0; cellp; cellp=cellp->allNextp, count1++) {
        if ( cellp->magic != CM_CELL_MAGIC ) {
            afsi_log("cm_ValidateCell failure: cellp->magic != CM_CELL_MAGIC");
            fprintf(stderr, "cm_ValidateCell failure: cellp->magic != CM_CELL_MAGIC\n");
            return -1;
        }
        if ( count1 != 0 && cellp == cm_data.allCellsp ||
             count1 > cm_data.maxCells ) {
            afsi_log("cm_ValidateCell failure: cm_data.allCellsp infinite loop");
            fprintf(stderr, "cm_ValidateCell failure: cm_data.allCellsp infinite loop\n");
            return -2;
        }
    }

    for (cellp = cm_data.freeCellsp, count2 = 0; cellp; cellp=cellp->freeNextp, count2++) {
        if ( count2 != 0 && cellp == cm_data.freeCellsp ||
             count2 > cm_data.maxCells ) {
            afsi_log("cm_ValidateCell failure: cm_data.freeCellsp infinite loop");
            fprintf(stderr, "cm_ValidateCell failure: cm_data.freeCellsp infinite loop\n");
            return -3;
        }
    }

    if ( (count1 + count2) != cm_data.currentCells ) {
        afsi_log("cm_ValidateCell failure: count != cm_data.currentCells");
        fprintf(stderr, "cm_ValidateCell failure: count != cm_data.currentCells\n");
        return -4;
    }

    return 0;
}


long
cm_ShutdownCell(void)
{
    cm_cell_t * cellp;

    for (cellp = cm_data.allCellsp; cellp; cellp=cellp->allNextp)
        lock_FinalizeMutex(&cellp->mx);

    return 0;
}


void cm_InitCell(int newFile, long maxCells)
{
    static osi_once_t once;

    if (osi_Once(&once)) {
        cm_cell_t * cellp;

        lock_InitializeRWLock(&cm_cellLock, "cell global lock", LOCK_HIERARCHY_CELL_GLOBAL);

        if ( newFile ) {
            cm_data.allCellsp = NULL;
            cm_data.currentCells = 0;
            cm_data.maxCells = maxCells;
            memset(cm_data.cellNameHashTablep, 0, sizeof(cm_cell_t *) * cm_data.cellHashTableSize);
            memset(cm_data.cellIDHashTablep, 0, sizeof(cm_cell_t *) * cm_data.cellHashTableSize);

#ifdef AFS_FREELANCE_CLIENT
            /* Generate a dummy entry for the Freelance cell whether or not
             * freelance mode is being used in this session
             */

            cellp = &cm_data.cellBaseAddress[cm_data.currentCells++];
            memset(cellp, 0, sizeof(cm_cell_t));
            cellp->magic = CM_CELL_MAGIC;

            lock_InitializeMutex(&cellp->mx, "cm_cell_t mutex", LOCK_HIERARCHY_CELL);

            lock_ObtainMutex(&cellp->mx);
            lock_ObtainWrite(&cm_cellLock);

            /* copy in name */
            strncpy(cellp->name, "Freelance.Local.Cell", CELL_MAXNAMELEN); /*safe*/
            cellp->name[CELL_MAXNAMELEN-1] = '\0';

            /* thread on global list */
            cellp->allNextp = cm_data.allCellsp;
            cm_data.allCellsp = cellp;

            cellp->cellID = AFS_FAKE_ROOT_CELL_ID;
            cellp->vlServersp = NULL;
            _InterlockedOr(&cellp->flags, CM_CELLFLAG_FREELANCE);

            cm_AddCellToNameHashTable(cellp);
            cm_AddCellToIDHashTable(cellp);
            lock_ReleaseWrite(&cm_cellLock);
            lock_ReleaseMutex(&cellp->mx);
#endif
        } else {
            lock_ObtainRead(&cm_cellLock);
            for (cellp = cm_data.allCellsp; cellp; cellp=cellp->allNextp) {
                lock_InitializeMutex(&cellp->mx, "cm_cell_t mutex", LOCK_HIERARCHY_CELL);
                cellp->vlServersp = NULL;
                _InterlockedOr(&cellp->flags, CM_CELLFLAG_VLSERVER_INVALID);
            }
            lock_ReleaseRead(&cm_cellLock);
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

int cm_DumpCells(FILE *outputFile, char *cookie, int lock)
{
    cm_cell_t *cellp;
    int zilch;
    char output[1024];

    if (lock)
        lock_ObtainRead(&cm_cellLock);

    sprintf(output, "%s - dumping cells - cm_data.currentCells=%d, cm_data.maxCells=%d\r\n",
            cookie, cm_data.currentCells, cm_data.maxCells);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);

    for (cellp = cm_data.allCellsp; cellp; cellp=cellp->allNextp) {
        sprintf(output, "%s cellp=0x%p,name=%s ID=%d flags=0x%x timeout=%I64u\r\n",
                cookie, cellp, cellp->name, cellp->cellID, cellp->flags, cellp->timeout);
        WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
    }

    sprintf(output, "%s - Done dumping cells.\r\n", cookie);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);

    if (lock)
        lock_ReleaseRead(&cm_cellLock);

    return(0);
}

/* call with volume write-locked and mutex held */
void cm_AddCellToNameHashTable(cm_cell_t *cellp)
{
    int i;

    lock_AssertWrite(&cm_cellLock);
    lock_AssertMutex(&cellp->mx);

    if (cellp->flags & CM_CELLFLAG_IN_NAMEHASH)
        return;

    i = CM_CELL_NAME_HASH(cellp->name);

    cellp->nameNextp = cm_data.cellNameHashTablep[i];
    cm_data.cellNameHashTablep[i] = cellp;
    _InterlockedOr(&cellp->flags, CM_CELLFLAG_IN_NAMEHASH);
}

/* call with cell write-locked and mutex held */
void cm_RemoveCellFromNameHashTable(cm_cell_t *cellp)
{
    cm_cell_t **lcellpp;
    cm_cell_t *tcellp;
    int i;

    lock_AssertWrite(&cm_cellLock);
    lock_AssertMutex(&cellp->mx);

    if (cellp->flags & CM_CELLFLAG_IN_NAMEHASH) {
	/* hash it out first */
	i = CM_CELL_NAME_HASH(cellp->name);
	for (lcellpp = &cm_data.cellNameHashTablep[i], tcellp = cm_data.cellNameHashTablep[i];
	     tcellp;
	     lcellpp = &tcellp->nameNextp, tcellp = tcellp->nameNextp) {
	    if (tcellp == cellp) {
		*lcellpp = cellp->nameNextp;
		_InterlockedAnd(&cellp->flags, ~CM_CELLFLAG_IN_NAMEHASH);
                cellp->nameNextp = NULL;
		break;
	    }
	}
    }
}

/* call with cell write-locked and mutex held */
void cm_AddCellToIDHashTable(cm_cell_t *cellp)
{
    int i;

    lock_AssertWrite(&cm_cellLock);
    lock_AssertMutex(&cellp->mx);

    if (cellp->flags & CM_CELLFLAG_IN_IDHASH)
        return;

    i = CM_CELL_ID_HASH(cellp->cellID);

    cellp->idNextp = cm_data.cellIDHashTablep[i];
    cm_data.cellIDHashTablep[i] = cellp;
    _InterlockedOr(&cellp->flags, CM_CELLFLAG_IN_IDHASH);
}

/* call with cell write-locked and mutex held */
void cm_RemoveCellFromIDHashTable(cm_cell_t *cellp)
{
    cm_cell_t **lcellpp;
    cm_cell_t *tcellp;
    int i;

    lock_AssertWrite(&cm_cellLock);
    lock_AssertMutex(&cellp->mx);

    if (cellp->flags & CM_CELLFLAG_IN_IDHASH) {
	/* hash it out first */
	i = CM_CELL_ID_HASH(cellp->cellID);
	for (lcellpp = &cm_data.cellIDHashTablep[i], tcellp = cm_data.cellIDHashTablep[i];
	     tcellp;
	     lcellpp = &tcellp->idNextp, tcellp = tcellp->idNextp) {
	    if (tcellp == cellp) {
		*lcellpp = cellp->idNextp;
		_InterlockedAnd(&cellp->flags, ~CM_CELLFLAG_IN_IDHASH);
                cellp->idNextp = NULL;
		break;
	    }
	}
    }
}

long
cm_CreateCellWithInfo( char * cellname,
                       char * linked_cellname,
                       unsigned short vlport,
                       afs_uint32 host_count,
                       char *hostname[],
                       afs_uint32 flags)
{
    afs_uint32 code = 0;
    cm_cell_rock_t rock;
    struct hostent *thp;
    struct sockaddr_in vlSockAddr;
    afs_uint32 i, j;

    rock.cellp = cm_GetCell(cellname, CM_FLAG_CREATE | CM_FLAG_NOPROBE);
    rock.flags = 0;

    cm_FreeServerList(&rock.cellp->vlServersp, CM_FREESERVERLIST_DELETE);

    if (!(flags & CM_CELLFLAG_DNS)) {
        for (i = 0; i < host_count; i++) {
            thp = gethostbyname(hostname[i]);
            if (thp) {
                int foundAddr = 0;
                for (j=0 ; thp->h_addr_list[j]; j++) {
                    if (thp->h_addrtype != AF_INET)
                        continue;
                    memcpy(&vlSockAddr.sin_addr.s_addr,
                           thp->h_addr_list[j],
                           sizeof(long));
                    vlSockAddr.sin_port = htons(vlport ? vlport : 7003);
                    vlSockAddr.sin_family = AF_INET;
                    cm_AddCellProc(&rock, &vlSockAddr, hostname[i], CM_FLAG_NOPROBE);
                }
            }
        }
        lock_ObtainMutex(&rock.cellp->mx);
        _InterlockedAnd(&rock.cellp->flags, ~CM_CELLFLAG_DNS);
    } else if (cm_dnsEnabled) {
        int ttl;

        code = cm_SearchCellByDNS(rock.cellp->name, NULL, &ttl, cm_AddCellProc, &rock);
        lock_ObtainMutex(&rock.cellp->mx);
        if (code == 0) {   /* got cell from DNS */
            _InterlockedOr(&rock.cellp->flags, CM_CELLFLAG_DNS);
            rock.cellp->timeout = time(0) + ttl;
#ifdef DEBUG
            fprintf(stderr, "cell %s: ttl=%d\n", rock.cellp->name, ttl);
#endif
        }
    } else {
        lock_ObtainMutex(&rock.cellp->mx);
        rock.cellp->flags &= ~CM_CELLFLAG_DNS;
    }
    _InterlockedOr(&rock.cellp->flags, CM_CELLFLAG_VLSERVER_INVALID);
    StringCbCopy(rock.cellp->linkedName, CELL_MAXNAMELEN, linked_cellname);
    lock_ReleaseMutex(&rock.cellp->mx);

    if (rock.cellp->vlServersp)
        cm_RandomizeServer(&rock.cellp->vlServersp);

    return code;
}
