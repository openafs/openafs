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
#include <string.h>
#include <malloc.h>
#include <osi.h>
#include <rx/rx.h>

#include "afsd.h"

osi_rwlock_t cm_volumeLock;
cm_volume_t *cm_allVolumesp;

void cm_InitVolume(void)
{
	static osi_once_t once;
	if (osi_Once(&once)) {
		lock_InitializeRWLock(&cm_volumeLock, "cm global volume lock");
                cm_allVolumesp = NULL;
		osi_EndOnce(&once);
        }
}

/*
 * Update a volume.  Caller holds volume's lock (volp->mx).
 *
 *
 *  shadow / openafs / jhutz@CS.CMU.EDU {ANDREW.CMU.EDU}  01:38    (JHutz)
 *    Yes, we support multihomed fileservers.
 *    Since before we got the code from IBM.
 *    But to find out about multiple addresses on a multihomed server, you need
 *    to use VL_GetEntryByNameU and VL_GetAddrsU.  If you use
 *    VL_GetEntryByNameO or VL_GetEntryByNameN, the vlserver just gives you one
 *    address per server.
 *  shadow / openafs / jhutz@CS.CMU.EDU {ANDREW.CMU.EDU}  01:39    (JHutz)
 *    see src/afs/afs_volume.c, paying particular attention to
 *    afs_NewVolumeByName, afs_SetupVolume, and InstallUVolumeEntry
 *  shadow / openafs / jaltman {ANDREW.CMU.EDU}  01:40    (Jeffrey Altman)
 *    thanks.  The windows client calls the 0 versions.
 *  shadow / openafs / jhutz@CS.CMU.EDU {ANDREW.CMU.EDU}  01:51    (JHutz)
 *    Oh.  Ew.
 *    By not using the N versions, you only get up to 8 sites instead of 13.
 *    By not using the U versions, you don't get to know about multihomed serve
 *  shadow / openafs / jhutz@CS.CMU.EDU {ANDREW.CMU.EDU}  01:52    (JHutz)
 *    Of course, you probably want to support the older versions for backward
 *    compatibility.  If you do that, you need to call the newest interface
 *    first, and fall back to successively older versions if you get
 *    RXGEN_OPCODE.
 */
long cm_UpdateVolume(struct cm_cell *cellp, cm_user_t *userp, cm_req_t *reqp,
	cm_volume_t *volp)
{
    cm_conn_t *connp;
    int i;
    cm_serverRef_t *tsrp;
    cm_server_t *tsp;
    struct sockaddr_in tsockAddr;
    long tflags;
    u_long tempAddr;
    struct vldbentry vldbEntry;
    struct nvldbentry nvldbEntry;
#ifdef MULTIHOMED
    struct uvldbentry uvldbEntry;
#endif
    int type = -1;
    int ROcount = 0;
    long code;

    /* clear out old bindings */
    cm_FreeServerList(&volp->rwServersp);
    cm_FreeServerList(&volp->roServersp);
    cm_FreeServerList(&volp->bkServersp);

    /* now we have volume structure locked and held; make RPC to fill it */
    do {
        code = cm_ConnByMServers(cellp->vlServersp, userp, reqp, &connp);
        if (code) 
            continue;
        osi_Log1(afsd_logp, "CALL VL_GetEntryByName{UNO} name %s", volp->namep);
#ifdef MULTIHOMED
        code = VL_GetEntryByNameU(connp->callp, volp->namep, &uvldbEntry);
        if ( code == RXGEN_OPCODE ) 
#endif
        {
            code = VL_GetEntryByNameN(connp->callp, volp->namep, &nvldbEntry);
            type = 1;
        }
        if ( code == RXGEN_OPCODE ) {
            code = VL_GetEntryByNameO(connp->callp, volp->namep, &vldbEntry);
            type = 0;
        }
    } while (cm_Analyze(connp, userp, reqp, NULL, NULL, cellp->vlServersp, NULL, code));
    code = cm_MapVLRPCError(code, reqp);

    if (code == 0) {
        afs_int32 flags;
        afs_int32 nServers;
        afs_int32 rwID;
        afs_int32 roID;
        afs_int32 bkID;
        afs_int32 serverNumber[NMAXNSERVERS];
        afs_int32 serverFlags[NMAXNSERVERS];

        switch ( type ) {
        case 0:
            flags = vldbEntry.flags;
            nServers = vldbEntry.nServers;
            rwID = vldbEntry.volumeId[0];
            roID = vldbEntry.volumeId[1];
            bkID = vldbEntry.volumeId[2];
            for ( i=0; i<nServers; i++ ) {
                serverFlags[i] = vldbEntry.serverFlags[i];
                serverNumber[i] = vldbEntry.serverNumber[i];
            }
            break;
        case 1:
            flags = nvldbEntry.flags;
            nServers = nvldbEntry.nServers;
            rwID = nvldbEntry.volumeId[0];
            roID = nvldbEntry.volumeId[1];
            bkID = nvldbEntry.volumeId[2];
            for ( i=0; i<nServers; i++ ) {
                serverFlags[i] = nvldbEntry.serverFlags[i];
                serverNumber[i] = nvldbEntry.serverNumber[i];
            }
            break;
#ifdef MULTIHOMED
        case 2:
            flags = uvldbEntry.flags;
            nServers = uvldbEntry.nServers;
            rwID = uvldbEntry.volumeId[0];
            roID = uvldbEntry.volumeId[1];
            bkID = uvldbEntry.volumeId[2];
            for ( i=0; i<nServers; i++ ) {
                serverFlags[i] = uvldbEntry.serverFlags[i];
                if ( !(flags & VLSERVER_FLAG_UUID) )
                    serverNumber[i] = uvldbEntry.serverNumber[i].time_low;
                else {
                    /* see afs/afs_volume.c InstallUVolumeEntry().  We need to 
                     * implement an equivalent to afs_FindServer() and afs_GetServer()
                     * which support multiple addresses.
                     */
                }
            }
            break;
#endif
        }

        /* decode the response */
        lock_ObtainWrite(&cm_volumeLock);
        if (flags & VLF_RWEXISTS)
            volp->rwID = rwID;
        else
            volp->rwID = 0;
        if (flags & VLF_ROEXISTS)
            volp->roID = roID;
        else
            volp->roID = 0;
        if (flags & VLF_BACKEXISTS)
            volp->bkID = bkID;
        else
            volp->bkID = 0;
        lock_ReleaseWrite(&cm_volumeLock);
        for (i=0; i<nServers; i++) {
            /* create a server entry */
            tflags = serverFlags[i];
            if (tflags & VLSF_DONTUSE) 
                continue;
            tsockAddr.sin_family = AF_INET;
            tempAddr = htonl(serverNumber[i]);
            tsockAddr.sin_addr.s_addr = tempAddr;
            tsp = cm_FindServer(&tsockAddr, CM_SERVER_FILE);
            if (!tsp)
                tsp = cm_NewServer(&tsockAddr, CM_SERVER_FILE,
                                    cellp);

            /* if this server was created by fs setserverprefs */
            if ( !tsp->cellp ) 
                tsp->cellp = cellp;

            osi_assert(tsp != NULL);
                        
            /* and add it to the list(s). */
            /*
             * Each call to cm_NewServerRef() increments the
             * ref count of tsp.  These reference will be dropped,
             * if and when the volume is reset; see reset code
             * earlier in this function.
             */
            if ((tflags & VLSF_RWVOL) && (flags & VLF_RWEXISTS)) {
                tsrp = cm_NewServerRef(tsp);
                cm_InsertServerList(&volp->rwServersp, tsrp);
                lock_ObtainWrite(&cm_serverLock);
                tsrp->refCount--;       /* drop allocation reference */
                lock_ReleaseWrite(&cm_serverLock);
            }
            if ((tflags & VLSF_ROVOL) && (flags & VLF_ROEXISTS)) {
                tsrp = cm_NewServerRef(tsp);
                cm_InsertServerList(&volp->roServersp, tsrp);
                lock_ObtainWrite(&cm_serverLock);
                tsrp->refCount--;       /* drop allocation reference */
                lock_ReleaseWrite(&cm_serverLock);
                ROcount++;
            }
            /* We don't use VLSF_BACKVOL !?! */
            if ((tflags & VLSF_RWVOL) && (flags & VLF_BACKEXISTS)) {
                tsrp = cm_NewServerRef(tsp);
                cm_InsertServerList(&volp->bkServersp, tsrp);
                lock_ObtainWrite(&cm_serverLock);
                tsrp->refCount--;       /* drop allocation reference */
                lock_ReleaseWrite(&cm_serverLock);
            }
            /* Drop the reference obtained by cm_FindServer() */
            cm_PutServer(tsp);
        }       

        /*
         * Randomize RO list
         *
         * If the first n servers have the same ipRank, then we 
         * randomly pick one among them and move it to the beginning.
         * We don't bother to re-order the whole list because
         * the rest of the list is used only if the first server is
         * down.  We only do this for the RO list; we assume the other
         * lists are length 1.
         */
        if (ROcount > 1) {
            cm_RandomizeServer(&volp->roServersp);
        }
    }
    return code;
}

long cm_GetVolumeByID(cm_cell_t *cellp, long volumeID, cm_user_t *userp,
	cm_req_t *reqp, cm_volume_t **outVolpp)
{
	cm_volume_t *volp;
        char volNameString[100];
        long code;

        lock_ObtainWrite(&cm_volumeLock);
	for(volp = cm_allVolumesp; volp; volp=volp->nextp) {
		if (cellp == volp->cellp &&
                	((unsigned) volumeID == volp->rwID ||
                	 (unsigned) volumeID == volp->roID ||
                         (unsigned) volumeID == volp->bkID))
                        	break;
        }

	/* hold the volume if we found it */
    if (volp) 
        volp->refCount++;
        lock_ReleaseWrite(&cm_volumeLock);
        
	/* return it held */
        if (volp) {
		lock_ObtainMutex(&volp->mx);
        
		if (volp->flags & CM_VOLUMEFLAG_RESET) {
			code = cm_UpdateVolume(cellp, userp, reqp, volp);
			if (code == 0) {
				volp->flags &= ~CM_VOLUMEFLAG_RESET;
			}
		}
		else
			code = 0;
		lock_ReleaseMutex(&volp->mx);
		if (code == 0)
			*outVolpp = volp;
		return code;
        }
        
        /* otherwise, we didn't find it so consult the VLDB */
        sprintf(volNameString, "%u", volumeID);
        code = cm_GetVolumeByName(cellp, volNameString, userp, reqp,
				  0, outVolpp);
        return code;
}

long cm_GetVolumeByName(struct cm_cell *cellp, char *volumeNamep,
	struct cm_user *userp, struct cm_req *reqp,
	long flags, cm_volume_t **outVolpp)
{
	cm_volume_t *volp;
        long code;
        
	/* initialize this */
	code = 0;

	lock_ObtainWrite(&cm_volumeLock);
        for(volp = cm_allVolumesp; volp; volp=volp->nextp) {
		if (cellp == volp->cellp && strcmp(volumeNamep, volp->namep) == 0) {
			break;
                }
        }
        
        /* otherwise, get from VLDB */
	if (!volp) {
		volp = malloc(sizeof(*volp));
	        memset(volp, 0, sizeof(*volp));
	        volp->cellp = cellp;
	        volp->nextp = cm_allVolumesp;
	        cm_allVolumesp = volp;
	        volp->namep = malloc(strlen(volumeNamep)+1);
	        strcpy(volp->namep, volumeNamep);
	        lock_InitializeMutex(&volp->mx, "cm_volume_t mutex");
	        volp->refCount = 1;	/* starts off held */
                volp->flags |= CM_VOLUMEFLAG_RESET;
	}
        else {
        	volp->refCount++;
	}
        
	/* next should work since no one could have gotten ptr to this structure yet */
        lock_ReleaseWrite(&cm_volumeLock);
	lock_ObtainMutex(&volp->mx);
        
	if (volp->flags & CM_VOLUMEFLAG_RESET) {
		code = cm_UpdateVolume(cellp, userp, reqp, volp);
		if (code == 0)
			volp->flags &= ~CM_VOLUMEFLAG_RESET;
	}

	if (code == 0)
	       	*outVolpp = volp;
        lock_ReleaseMutex(&volp->mx);
        return code;
}

void cm_ForceUpdateVolume(cm_fid_t *fidp, cm_user_t *userp, cm_req_t *reqp)
{
	cm_cell_t *cellp;
	cm_volume_t *volp;
	long code;

	if (!fidp) return;

	cellp = cm_FindCellByID(fidp->cell);
	if (!cellp) return;

	/* search for the volume */
        lock_ObtainWrite(&cm_volumeLock);
	for(volp = cm_allVolumesp; volp; volp=volp->nextp) {
		if (cellp == volp->cellp &&
                	(fidp->volume == volp->rwID ||
                	 fidp->volume == volp->roID ||
                         fidp->volume == volp->bkID))
                        	break;
        }

	/* hold the volume if we found it */
        if (volp) volp->refCount++;
        lock_ReleaseWrite(&cm_volumeLock);

	/* update it */
	cm_mountRootGen++;
	lock_ObtainMutex(&volp->mx);
	volp->flags |= CM_VOLUMEFLAG_RESET;
#ifdef COMMENT
    /* Mark the volume to be updated but don't update it now.
     * This function is called only from within cm_Analyze
     * when cm_ConnByMServers has failed with all servers down
     * The problem is that cm_UpdateVolume is going to call
     * cm_ConnByMServers which may cause a recursive chain
     * of calls each returning a retry on failure.
     * Instead, set the flag so the next time the volume is
     * accessed by Name or ID the UpdateVolume call will
     * occur.
     */
	code = cm_UpdateVolume(cellp, userp, reqp, volp);
	if (code == 0)
		volp->flags &= ~CM_VOLUMEFLAG_RESET;
#endif
	lock_ReleaseMutex(&volp->mx);

	cm_PutVolume(volp);
}

/* find the appropriate servers from a volume */
cm_serverRef_t **cm_GetVolServers(cm_volume_t *volp, unsigned long volume)
{
	cm_serverRef_t **serverspp;
    cm_serverRef_t *current;;

    lock_ObtainWrite(&cm_serverLock);

	if (volume == volp->rwID)
        serverspp = &volp->rwServersp;
	else if (volume == volp->roID)
        serverspp = &volp->roServersp;
	else if (volume == volp->bkID)
        serverspp = &volp->bkServersp;
    else 
        osi_panic("bad volume ID in cm_GetVolServers", __FILE__, __LINE__);
        
    for (current = *serverspp; current; current = current->next)
        current->refCount++;

    lock_ReleaseWrite(&cm_serverLock);

    return serverspp;
}

void cm_PutVolume(cm_volume_t *volp)
{
	lock_ObtainWrite(&cm_volumeLock);
	osi_assert(volp->refCount-- > 0);
	lock_ReleaseWrite(&cm_volumeLock);
}

/* return the read-only volume, if there is one, or the read-write volume if
 * not.
 */
long cm_GetROVolumeID(cm_volume_t *volp)
{
	long id;

	lock_ObtainMutex(&volp->mx);
	if (volp->roID && volp->roServersp)
		id = volp->roID;
	else
        	id = volp->rwID;
	lock_ReleaseMutex(&volp->mx);

        return id;
}

void cm_CheckVolumes(void)
{
	cm_volume_t *volp;

	cm_mountRootGen++;
        lock_ObtainWrite(&cm_volumeLock);
	for(volp = cm_allVolumesp; volp; volp=volp->nextp) {
		volp->refCount++;
                lock_ReleaseWrite(&cm_volumeLock);
                lock_ObtainMutex(&volp->mx);

                volp->flags |= CM_VOLUMEFLAG_RESET;

                lock_ReleaseMutex(&volp->mx);
                lock_ObtainWrite(&cm_volumeLock);
                osi_assert(volp->refCount-- > 0);
	}
        lock_ReleaseWrite(&cm_volumeLock);

	/* We should also refresh cached mount points */
}

/*
** Finds all volumes that reside on this server and reorders their
** RO list according to the changed rank of server.
*/
void cm_ChangeRankVolume(cm_server_t       *tsp)
{
	int 		code;
	cm_volume_t*	volp;

	/* find volumes which might have RO copy on server*/
	lock_ObtainWrite(&cm_volumeLock);
	for(volp = cm_allVolumesp; volp; volp=volp->nextp)
	{
		code = 1 ;	/* assume that list is unchanged */
		volp->refCount++;
		lock_ReleaseWrite(&cm_volumeLock);
		lock_ObtainMutex(&volp->mx);

		if ((tsp->cellp==volp->cellp) && (volp->roServersp))
		    code =cm_ChangeRankServer(&volp->roServersp, tsp);

		/* this volume list was changed */
		if ( !code )
			cm_RandomizeServer(&volp->roServersp);

		lock_ReleaseMutex(&volp->mx);
		lock_ObtainWrite(&cm_volumeLock);
		osi_assert(volp->refCount-- > 0);
	}
	lock_ReleaseWrite(&cm_volumeLock);
}
