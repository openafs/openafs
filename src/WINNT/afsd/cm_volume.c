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
#include <winsock2.h>
#include <nb30.h>
#include <string.h>
#include <malloc.h>
#include "afsd.h"
#include <osi.h>
#include <rx/rx.h>

osi_rwlock_t cm_volumeLock;

long 
cm_ValidateVolume(void)
{
    cm_volume_t * volp;
    afs_uint32 count;

    for (volp = cm_data.allVolumesp, count = 0; volp; volp=volp->allNextp, count++) {
        if ( volp->magic != CM_VOLUME_MAGIC ) {
            afsi_log("cm_ValidateVolume failure: volp->magic != CM_VOLUME_MAGIC");
            fprintf(stderr, "cm_ValidateVolume failure: volp->magic != CM_VOLUME_MAGIC\n");
            return -1;
        }
        if ( volp->cellp && volp->cellp->magic != CM_CELL_MAGIC ) {
            afsi_log("cm_ValidateVolume failure: volp->cellp->magic != CM_CELL_MAGIC");
            fprintf(stderr, "cm_ValidateVolume failure: volp->cellp->magic != CM_CELL_MAGIC\n");
            return -2;
        }
        if ( volp->allNextp && volp->allNextp->magic != CM_VOLUME_MAGIC ) {
            afsi_log("cm_ValidateVolume failure: volp->allNextp->magic != CM_VOLUME_MAGIC");
            fprintf(stderr, "cm_ValidateVolume failure: volp->allNextp->magic != CM_VOLUME_MAGIC\n");
            return -3;
        }
        if ( count != 0 && volp == cm_data.allVolumesp || 
             count > cm_data.maxVolumes ) {
            afsi_log("cm_ValidateVolume failure: cm_data.allVolumep loop detected");
            fprintf(stderr, "cm_ValidateVolume failure: cm_data.allVolumep loop detected\n");
            return -4;
        }
    }

    if ( count != cm_data.currentVolumes ) {
        afsi_log("cm_ValidateVolume failure: count != cm_data.currentVolumes");
        fprintf(stderr, "cm_ValidateVolume failure: count != cm_data.currentVolumes\n");
        return -5;
    }
    
    return 0;
}

long
cm_ShutdownVolume(void)
{
    cm_volume_t * volp;

    for (volp = cm_data.allVolumesp; volp; volp=volp->allNextp) {

        if (volp->rw.ID)
            cm_VolumeStatusNotification(volp, volp->rw.ID, volp->rw.state, vl_alldown);
        if (volp->ro.ID)
            cm_VolumeStatusNotification(volp, volp->ro.ID, volp->ro.state, vl_alldown);
        if (volp->bk.ID)
            cm_VolumeStatusNotification(volp, volp->bk.ID, volp->bk.state, vl_alldown);
        volp->cbExpiresRO = 0;
        lock_FinalizeMutex(&volp->mx);
    }

    return 0;
}

void cm_InitVolume(int newFile, long maxVols)
{
    static osi_once_t once;

    if (osi_Once(&once)) {
        lock_InitializeRWLock(&cm_volumeLock, "cm global volume lock");

        if ( newFile ) {
            cm_data.allVolumesp = NULL;
            cm_data.currentVolumes = 0;
            cm_data.maxVolumes = maxVols;
            memset(cm_data.volumeNameHashTablep, 0, sizeof(cm_volume_t *) * cm_data.volumeHashTableSize);
            memset(cm_data.volumeRWIDHashTablep, 0, sizeof(cm_volume_t *) * cm_data.volumeHashTableSize);
            memset(cm_data.volumeROIDHashTablep, 0, sizeof(cm_volume_t *) * cm_data.volumeHashTableSize);
            memset(cm_data.volumeBKIDHashTablep, 0, sizeof(cm_volume_t *) * cm_data.volumeHashTableSize);
            cm_data.volumeLRUFirstp = cm_data.volumeLRULastp = NULL;
        } else {
            cm_volume_t * volp;

            for (volp = cm_data.allVolumesp; volp; volp=volp->allNextp) {
                lock_InitializeMutex(&volp->mx, "cm_volume_t mutex");
                volp->flags |= CM_VOLUMEFLAG_RESET;
                volp->rw.state = vl_unknown;
                volp->rw.serversp = NULL;
                volp->ro.state = vl_unknown;
                volp->ro.serversp = NULL;
                volp->bk.state = vl_unknown;
                volp->bk.serversp = NULL;
                if (volp->rw.ID)
                    cm_VolumeStatusNotification(volp, volp->rw.ID, vl_alldown, volp->rw.state);
                if (volp->ro.ID)
                    cm_VolumeStatusNotification(volp, volp->ro.ID, vl_alldown, volp->ro.state);
                if (volp->bk.ID)
                    cm_VolumeStatusNotification(volp, volp->bk.ID, vl_alldown, volp->bk.state);
                volp->cbExpiresRO = 0;
            }
        }
        osi_EndOnce(&once);
    }
}


/* returns true if the id is a decimal integer, in which case we interpret it
 * as an id.  make the cache manager much simpler.  
 * Stolen from src/volser/vlprocs.c */
int
cm_VolNameIsID(char *aname)
{
    int tc;
    while (tc = *aname++) {
        if (tc > '9' || tc < '0')
            return 0;
    }
    return 1;
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
#define MULTIHOMED 1
long cm_UpdateVolume(struct cm_cell *cellp, cm_user_t *userp, cm_req_t *reqp,
		     cm_volume_t *volp)
{
    cm_conn_t *connp;
    int i, j, k;
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
    int method = -1;
    int ROcount = 0;
    long code;
    enum volstatus rwNewstate = vl_online;
    enum volstatus roNewstate = vl_online;
    enum volstatus bkNewstate = vl_online;
#ifdef AFS_FREELANCE_CLIENT
    int freelance = 0;
#endif

    /* clear out old bindings */
    if (volp->rw.serversp)
        cm_FreeServerList(&volp->rw.serversp, CM_FREESERVERLIST_DELETE);
    if (volp->ro.serversp)
        cm_FreeServerList(&volp->ro.serversp, CM_FREESERVERLIST_DELETE);
    if (volp->bk.serversp)
        cm_FreeServerList(&volp->bk.serversp, CM_FREESERVERLIST_DELETE);

#ifdef AFS_FREELANCE_CLIENT
    if ( cellp->cellID == AFS_FAKE_ROOT_CELL_ID && volp->rw.ID == AFS_FAKE_ROOT_VOL_ID ) 
    {
	freelance = 1;
        memset(&vldbEntry, 0, sizeof(vldbEntry));
        vldbEntry.flags |= VLF_RWEXISTS;
        vldbEntry.volumeId[0] = AFS_FAKE_ROOT_VOL_ID;
        code = 0;
        method = 0;
    } else
#endif
    {
        /* now we have volume structure locked and held; make RPC to fill it */
	osi_Log2(afsd_logp, "CALL VL_GetEntryByName{UNO} name %s:%s", volp->cellp->name, volp->namep);
        do {
            code = cm_ConnByMServers(cellp->vlServersp, userp, reqp, &connp);
            if (code) 
                continue;
#ifdef MULTIHOMED
            code = VL_GetEntryByNameU(connp->callp, volp->namep, &uvldbEntry);
            method = 2;
            if ( code == RXGEN_OPCODE ) 
#endif
            {
                code = VL_GetEntryByNameN(connp->callp, volp->namep, &nvldbEntry);
                method = 1;
            }
            if ( code == RXGEN_OPCODE ) {
                code = VL_GetEntryByNameO(connp->callp, volp->namep, &vldbEntry);
                method = 0;
            }
        } while (cm_Analyze(connp, userp, reqp, NULL, NULL, cellp->vlServersp, NULL, code));
        code = cm_MapVLRPCError(code, reqp);
	if ( code )
	    osi_Log3(afsd_logp, "CALL VL_GetEntryByName{UNO} name %s:%s FAILURE, code 0x%x", 
		      volp->cellp->name, volp->namep, code);
	else
	    osi_Log2(afsd_logp, "CALL VL_GetEntryByName{UNO} name %s:%s SUCCESS", 
		      volp->cellp->name, volp->namep);
    }

    /* We can end up here with code == CM_ERROR_NOSUCHVOLUME if the base volume name
     * does not exist but there might exist a .readonly volume.  If the base name 
     * doesn't exist we will not care about the .backup that might be left behind
     * since there should be no method to access it.  
     */
    if (code == CM_ERROR_NOSUCHVOLUME && volp->rw.ID == 0 && strlen(volp->namep) < (VL_MAXNAMELEN - 9)) {
        char name[VL_MAXNAMELEN];

        snprintf(name, VL_MAXNAMELEN, "%s.readonly", volp->namep);
                
        /* now we have volume structure locked and held; make RPC to fill it */
	osi_Log2(afsd_logp, "CALL VL_GetEntryByName{UNO} name %s:%s", volp->cellp->name, name);
        do {
            code = cm_ConnByMServers(cellp->vlServersp, userp, reqp, &connp);
            if (code) 
                continue;
#ifdef MULTIHOMED
            code = VL_GetEntryByNameU(connp->callp, name, &uvldbEntry);
            method = 2;
            if ( code == RXGEN_OPCODE ) 
#endif
            {
                code = VL_GetEntryByNameN(connp->callp, name, &nvldbEntry);
                method = 1;
            }
            if ( code == RXGEN_OPCODE ) {
                code = VL_GetEntryByNameO(connp->callp, name, &vldbEntry);
                method = 0;
            }
        } while (cm_Analyze(connp, userp, reqp, NULL, NULL, cellp->vlServersp, NULL, code));
        code = cm_MapVLRPCError(code, reqp);
	if ( code )
	    osi_Log3(afsd_logp, "CALL VL_GetEntryByName{UNO} name %s:%s FAILURE, code 0x%x", 
		      volp->cellp->name, name, code);
	else
	    osi_Log2(afsd_logp, "CALL VL_GetEntryByName{UNO} name %s:%s SUCCESS", 
		      volp->cellp->name, name);
    }

    if (code == 0) {
        afs_int32 flags;
        afs_int32 nServers;
        afs_int32 rwID;
        afs_int32 roID;
        afs_int32 bkID;
        afs_int32 serverNumber[NMAXNSERVERS];
        afs_int32 serverFlags[NMAXNSERVERS];
        afs_int32 rwServers_alldown = 1;
        afs_int32 roServers_alldown = 1;
        afs_int32 bkServers_alldown = 1;
        char      name[VL_MAXNAMELEN];

#ifdef AFS_FREELANCE_CLIENT
	if (freelance)
	    rwServers_alldown = 0;
#endif

        switch ( method ) {
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
            strncpy(name, vldbEntry.name, VL_MAXNAMELEN);
            name[VL_MAXNAMELEN - 1] = '\0';
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
            strncpy(name, nvldbEntry.name, VL_MAXNAMELEN);
            name[VL_MAXNAMELEN - 1] = '\0';
            break;
#ifdef MULTIHOMED
        case 2:
            flags = uvldbEntry.flags;
            nServers = uvldbEntry.nServers;
            rwID = uvldbEntry.volumeId[0];
            roID = uvldbEntry.volumeId[1];
            bkID = uvldbEntry.volumeId[2];
            for ( i=0, j=0; i<nServers && j<NMAXNSERVERS; i++ ) {
                if ( !(uvldbEntry.serverFlags[i] & VLSERVER_FLAG_UUID) ) {
                    serverFlags[j] = uvldbEntry.serverFlags[i];
                    serverNumber[j] = uvldbEntry.serverNumber[i].time_low;
                    j++;
                } else {
                    afs_uint32 * addrp, nentries, code, unique;
                    bulkaddrs  addrs;
                    ListAddrByAttributes attrs;
                    afsUUID uuid;

                    memset((char *)&attrs, 0, sizeof(attrs));
                    attrs.Mask = VLADDR_UUID;
                    attrs.uuid = uvldbEntry.serverNumber[i];
                    memset((char *)&uuid, 0, sizeof(uuid));
                    memset((char *)&addrs, 0, sizeof(addrs));

                    do {
                        code = cm_ConnByMServers(cellp->vlServersp, userp, reqp, &connp);
                        if (code) 
                            continue;
                   
                        code = VL_GetAddrsU(connp->callp, &attrs, &uuid, &unique, &nentries, &addrs);

                        if (code == 0 && nentries == 0)
                            code = VL_NOENT;
                    } while (cm_Analyze(connp, userp, reqp, NULL, NULL, cellp->vlServersp, NULL, code));
                    code = cm_MapVLRPCError(code, reqp);
                    if (code)
                        return code;

                    addrp = addrs.bulkaddrs_val;
                    for (k = 0; k < nentries && j < NMAXNSERVERS; j++, k++) {
                        serverFlags[j] = uvldbEntry.serverFlags[i];
                        serverNumber[j] = addrp[k];
                    }

                    free(addrs.bulkaddrs_val);  /* This is wrong */
                }
            }
            nServers = j;					/* update the server count */
            strncpy(name, uvldbEntry.name, VL_MAXNAMELEN);
            name[VL_MAXNAMELEN - 1] = '\0';
            break;
#endif
        }

        /* decode the response */
        lock_ObtainWrite(&cm_volumeLock);
        if (cm_VolNameIsID(volp->namep)) {
            size_t    len;

            len = strlen(name);

            if (len >= 8 && strcmp(name + len - 7, ".backup") == 0) {
                name[len - 7] = '\0';
            } else if (len >= 10 && strcmp(name + len - 9, ".readonly") == 0) {
                name[len - 9] = '\0';
            }
            
            osi_Log2(afsd_logp, "cm_UpdateVolume name %s -> %s", volp->namep, name);

            if (volp->flags & CM_VOLUMEFLAG_IN_HASH)
                cm_RemoveVolumeFromNameHashTable(volp);

            strcpy(volp->namep, name);

            cm_AddVolumeToNameHashTable(volp);
        }

        if (flags & VLF_RWEXISTS) {
            if (volp->rw.ID != rwID) {
                if (volp->rw.flags & CM_VOLUMEFLAG_IN_HASH)
                    cm_RemoveVolumeFromIDHashTable(volp, RWVOL);
                volp->rw.ID = rwID;
                cm_AddVolumeToIDHashTable(volp, RWVOL);
            }
        } else {
            if (volp->rw.flags & CM_VOLUMEFLAG_IN_HASH)
                cm_RemoveVolumeFromIDHashTable(volp, RWVOL);
            volp->rw.ID = 0;
        }
        if (flags & VLF_ROEXISTS) {
            if (volp->ro.ID != roID) {
                if (volp->ro.flags & CM_VOLUMEFLAG_IN_HASH)
                    cm_RemoveVolumeFromIDHashTable(volp, ROVOL);
                volp->ro.ID = roID;
                cm_AddVolumeToIDHashTable(volp, ROVOL);
            }
        } else {
            if (volp->ro.flags & CM_VOLUMEFLAG_IN_HASH)
                cm_RemoveVolumeFromIDHashTable(volp, ROVOL);
            volp->ro.ID = 0;
        }
        if (flags & VLF_BACKEXISTS) {
            if (volp->bk.ID != bkID) {
                if (volp->bk.flags & CM_VOLUMEFLAG_IN_HASH)
                    cm_RemoveVolumeFromIDHashTable(volp, BACKVOL);
                volp->bk.ID = bkID;
                cm_AddVolumeToIDHashTable(volp, BACKVOL);
            }
        } else {
            if (volp->bk.flags & CM_VOLUMEFLAG_IN_HASH)
                cm_RemoveVolumeFromIDHashTable(volp, BACKVOL);
            volp->bk.ID = 0;
        }
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

            osi_assertx(tsp != NULL, "null cm_server_t");
                        
            /* and add it to the list(s). */
            /*
             * Each call to cm_NewServerRef() increments the
             * ref count of tsp.  These reference will be dropped,
             * if and when the volume is reset; see reset code
             * earlier in this function.
             */
            if ((tflags & VLSF_RWVOL) && (flags & VLF_RWEXISTS)) {
                tsrp = cm_NewServerRef(tsp, rwID);
                cm_InsertServerList(&volp->rw.serversp, tsrp);

                lock_ObtainWrite(&cm_serverLock);
                tsrp->refCount--;       /* drop allocation reference */
                lock_ReleaseWrite(&cm_serverLock);

                if (!(tsp->flags & CM_SERVERFLAG_DOWN))
                    rwServers_alldown = 0;
            }
            if ((tflags & VLSF_ROVOL) && (flags & VLF_ROEXISTS)) {
                tsrp = cm_NewServerRef(tsp, roID);
                cm_InsertServerList(&volp->ro.serversp, tsrp);
                lock_ObtainWrite(&cm_serverLock);
                tsrp->refCount--;       /* drop allocation reference */
                lock_ReleaseWrite(&cm_serverLock);
                ROcount++;

                if (!(tsp->flags & CM_SERVERFLAG_DOWN))
                    roServers_alldown = 0;
            }
            /* We don't use VLSF_BACKVOL !?! */
            /* Because only the backup on the server holding the RW 
             * volume can be valid.  This check prevents errors if a
             * RW is moved but the old backup is not removed.
             */
            if ((tflags & VLSF_RWVOL) && (flags & VLF_BACKEXISTS)) {
                tsrp = cm_NewServerRef(tsp, bkID);
                cm_InsertServerList(&volp->bk.serversp, tsrp);
                lock_ObtainWrite(&cm_serverLock);
                tsrp->refCount--;       /* drop allocation reference */
                lock_ReleaseWrite(&cm_serverLock);

                if (!(tsp->flags & CM_SERVERFLAG_DOWN))
                    bkServers_alldown = 0;
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
            cm_RandomizeServer(&volp->ro.serversp);
        }

        rwNewstate = rwServers_alldown ? vl_alldown : vl_online;
        roNewstate = roServers_alldown ? vl_alldown : vl_online;
        bkNewstate = bkServers_alldown ? vl_alldown : vl_online;
    } else {
        rwNewstate = roNewstate = bkNewstate = vl_alldown;
    }

    if (volp->rw.state != rwNewstate) {
        if (volp->rw.ID)
            cm_VolumeStatusNotification(volp, volp->rw.ID, volp->rw.state, rwNewstate);
        volp->rw.state = rwNewstate;
    }
    if (volp->ro.state != roNewstate) {
        if (volp->ro.ID)
            cm_VolumeStatusNotification(volp, volp->ro.ID, volp->ro.state, roNewstate);
        volp->ro.state = roNewstate;
    }
    if (volp->bk.state != bkNewstate) {
        if (volp->bk.ID)
            cm_VolumeStatusNotification(volp, volp->bk.ID, volp->bk.state, bkNewstate);
        volp->bk.state = bkNewstate;
    }

    return code;
}

void cm_GetVolume(cm_volume_t *volp)
{
    if (volp) {
	lock_ObtainWrite(&cm_volumeLock);
	volp->refCount++;
	lock_ReleaseWrite(&cm_volumeLock);
    }
}


long cm_GetVolumeByID(cm_cell_t *cellp, afs_uint32 volumeID, cm_user_t *userp,
                      cm_req_t *reqp, afs_uint32 flags, cm_volume_t **outVolpp)
{
    cm_volume_t *volp;
#ifdef SEARCH_ALL_VOLUMES
    cm_volume_t *volp2;
#endif
    char volNameString[VL_MAXNAMELEN];
    afs_uint32 hash;
    long code = 0;

    lock_ObtainRead(&cm_volumeLock);
#ifdef SEARCH_ALL_VOLUMES
    for(volp = cm_data.allVolumesp; volp; volp=volp->allNextp) {
	if (cellp == volp->cellp &&
	     ((unsigned) volumeID == volp->rw.ID ||
	       (unsigned) volumeID == volp->ro.ID ||
	       (unsigned) volumeID == volp->bk.ID))
	    break;
    }	

    volp2 = volp;
#endif /* SEARCH_ALL_VOLUMES */

    hash = CM_VOLUME_ID_HASH(volumeID);
    /* The volumeID can be any one of the three types.  So we must
     * search the hash table for all three types until we find it.
     * We will search in the order of RO, RW, BK.
     */
    for ( volp = cm_data.volumeROIDHashTablep[hash]; volp; volp = volp->ro.nextp) {
        if ( cellp == volp->cellp && volumeID == volp->ro.ID )
            break;
    }
    if (!volp) {
        /* try RW volumes */
        for ( volp = cm_data.volumeRWIDHashTablep[hash]; volp; volp = volp->rw.nextp) {
            if ( cellp == volp->cellp && volumeID == volp->rw.ID )
                break;
        }
    }
    if (!volp) {
        /* try BK volumes */
        for ( volp = cm_data.volumeBKIDHashTablep[hash]; volp; volp = volp->bk.nextp) {
            if ( cellp == volp->cellp && volumeID == volp->bk.ID )
                break;
        }
    }

#ifdef SEARCH_ALL_VOLUMES
    osi_assertx(volp == volp2, "unexpected cm_vol_t");
#endif

    lock_ReleaseRead(&cm_volumeLock);

    /* hold the volume if we found it */
    if (volp) 
        cm_GetVolume(volp);
        
    /* return it held */
    if (volp) {
	lock_ObtainMutex(&volp->mx);
        
	code = 0;
	if (volp->flags & CM_VOLUMEFLAG_RESET) {
	    code = cm_UpdateVolume(cellp, userp, reqp, volp);
	    if (code == 0)
		volp->flags &= ~CM_VOLUMEFLAG_RESET;
	}
	lock_ReleaseMutex(&volp->mx);
	if (code == 0) {
	    *outVolpp = volp;

            lock_ObtainWrite(&cm_volumeLock);
            cm_AdjustVolumeLRU(volp);
            lock_ReleaseWrite(&cm_volumeLock);
        } else
	    cm_PutVolume(volp);

	return code;
    }
        
    /* otherwise, we didn't find it so consult the VLDB */
    sprintf(volNameString, "%u", volumeID);
    code = cm_GetVolumeByName(cellp, volNameString, userp, reqp,
			      flags, outVolpp);
    return code;
}


long cm_GetVolumeByName(struct cm_cell *cellp, char *volumeNamep,
			struct cm_user *userp, struct cm_req *reqp,
			afs_uint32 flags, cm_volume_t **outVolpp)
{
    cm_volume_t *volp;
#ifdef SEARCH_ALL_VOLUMES
    cm_volume_t *volp2;
#endif
    long        code = 0;
    char        name[VL_MAXNAMELEN];
    size_t      len;
    int         type;
    afs_uint32  hash;

    strncpy(name, volumeNamep, VL_MAXNAMELEN);
    name[VL_MAXNAMELEN-1] = '\0';
    len = strlen(name);

    if (len >= 8 && strcmp(name + len - 7, ".backup") == 0) {
        type = BACKVOL;
        name[len - 7] = '\0';
    } else if (len >= 10 && strcmp(name + len - 9, ".readonly") == 0) {
        type = ROVOL;
        name[len - 9] = '\0';
    } else {
        type = RWVOL;
    }

    lock_ObtainRead(&cm_volumeLock);
#ifdef SEARCH_ALL_VOLUMES
    for (volp = cm_data.allVolumesp; volp; volp=volp->allNextp) {
	if (cellp == volp->cellp && strcmp(name, volp->namep) == 0) {
	    break;
	}
    }	
    volp2 = volp;
#endif /* SEARCH_ALL_VOLUMES */

    hash = CM_VOLUME_NAME_HASH(name);
    for (volp = cm_data.volumeNameHashTablep[hash]; volp; volp = volp->nameNextp) {
        if (cellp == volp->cellp && strcmp(name, volp->namep) == 0)
            break;
    }

#ifdef SEARCH_ALL_VOLUMES
    osi_assertx(volp2 == volp, "unexpected cm_vol_t");
#endif

    if (!volp && (flags & CM_GETVOL_FLAG_CREATE)) {
        /* otherwise, get from VLDB */

	if ( cm_data.currentVolumes >= cm_data.maxVolumes ) {

#ifdef RECYCLE_FROM_ALL_VOLUMES_LIST
	    for (volp = cm_data.allVolumesp; volp; volp=volp->allNextp) {
		if ( volp->refCount == 0 ) {
		    /* There is one we can re-use */
		    break;
		}
	    }
#else
            for ( volp = cm_data.volumeLRULastp;
                  volp;
                  volp = (cm_volume_t *) osi_QPrev(&volp->q)) 
            {
		if ( volp->refCount == 0 ) {
		    /* There is one we can re-use */
		    break;
		}
            }
#endif
	    if (!volp)
		osi_panic("Exceeded Max Volumes", __FILE__, __LINE__);

            lock_ReleaseRead(&cm_volumeLock);
            lock_ObtainMutex(&volp->mx);
            lock_ObtainWrite(&cm_volumeLock);

            osi_Log2(afsd_logp, "Recycling Volume %s:%s",
                     volp->cellp->name, volp->namep);

            if (volp->flags & CM_VOLUMEFLAG_IN_LRU_QUEUE)
                cm_RemoveVolumeFromLRU(volp);
            if (volp->flags & CM_VOLUMEFLAG_IN_HASH)
                cm_RemoveVolumeFromNameHashTable(volp);
            if (volp->rw.flags & CM_VOLUMEFLAG_IN_HASH)
                cm_RemoveVolumeFromIDHashTable(volp, RWVOL);
            if (volp->ro.flags & CM_VOLUMEFLAG_IN_HASH)
                cm_RemoveVolumeFromIDHashTable(volp, ROVOL);
            if (volp->bk.flags & CM_VOLUMEFLAG_IN_HASH)
                cm_RemoveVolumeFromIDHashTable(volp, BACKVOL);

            if (volp->rw.ID)
                cm_VolumeStatusNotification(volp, volp->rw.ID, volp->rw.state, vl_unknown);
            if (volp->ro.ID)
                cm_VolumeStatusNotification(volp, volp->ro.ID, volp->ro.state, vl_unknown);
            if (volp->bk.ID)
                cm_VolumeStatusNotification(volp, volp->bk.ID, volp->bk.state, vl_unknown);

            volp->rw.ID = volp->ro.ID = volp->bk.ID = 0;
	    volp->dotdotFid.cell = 0;
	    volp->dotdotFid.volume = 0;
	    volp->dotdotFid.unique = 0;
	    volp->dotdotFid.vnode = 0;
	} else {
	    volp = &cm_data.volumeBaseAddress[cm_data.currentVolumes++];
	    memset(volp, 0, sizeof(cm_volume_t));
	    volp->magic = CM_VOLUME_MAGIC;
	    volp->allNextp = cm_data.allVolumesp;
	    cm_data.allVolumesp = volp;
	    lock_InitializeMutex(&volp->mx, "cm_volume_t mutex");
            lock_ReleaseRead(&cm_volumeLock);
            lock_ObtainMutex(&volp->mx);
            lock_ObtainWrite(&cm_volumeLock);
        }
	volp->cellp = cellp;
	strncpy(volp->namep, name, VL_MAXNAMELEN);
	volp->namep[VL_MAXNAMELEN-1] = '\0';
        volp->refCount = 1;	/* starts off held */
	volp->flags = CM_VOLUMEFLAG_RESET;
        volp->rw.state = volp->ro.state = volp->bk.state = vl_unknown;
        volp->rw.nextp = volp->ro.nextp = volp->bk.nextp = NULL;
        volp->rw.flags = volp->ro.flags = volp->bk.flags = 0;
        volp->cbExpiresRO = 0;
        cm_AddVolumeToNameHashTable(volp);
        lock_ReleaseWrite(&cm_volumeLock);
    }
    else {
        lock_ReleaseRead(&cm_volumeLock);
        if (volp) {
            cm_GetVolume(volp);
            lock_ObtainMutex(&volp->mx);
        } else {
            return CM_ERROR_NOSUCHVOLUME;
        }
    }

    /* if we get here we are holding the mutex */
    if (volp->flags & CM_VOLUMEFLAG_RESET) {
	code = cm_UpdateVolume(cellp, userp, reqp, volp);
	if (code == 0)
	    volp->flags &= ~CM_VOLUMEFLAG_RESET;
    }	
    lock_ReleaseMutex(&volp->mx);

    if (code == 0 && (type == BACKVOL && volp->bk.ID == 0 ||
                      type == ROVOL && volp->ro.ID == 0))
        code = CM_ERROR_NOSUCHVOLUME;

    if (code == 0) {
	*outVolpp = volp;

        lock_ObtainWrite(&cm_volumeLock);
        cm_AdjustVolumeLRU(volp);
        lock_ReleaseWrite(&cm_volumeLock);
    } else
	cm_PutVolume(volp);

    return code;
}	

void cm_ForceUpdateVolume(cm_fid_t *fidp, cm_user_t *userp, cm_req_t *reqp)
{
    cm_cell_t *cellp;
    cm_volume_t *volp;
#ifdef SEARCH_ALL_VOLUMES
    cm_volume_t *volp2;
#endif
    afs_uint32  hash;

    if (!fidp) return;

    cellp = cm_FindCellByID(fidp->cell);
    if (!cellp) return;

    /* search for the volume */
    lock_ObtainRead(&cm_volumeLock);
#ifdef SEARCH_ALL_VOLUMES
    for(volp = cm_data.allVolumesp; volp; volp=volp->allNextp) {
	if (cellp == volp->cellp &&
	     (fidp->volume == volp->rw.ID ||
	       fidp->volume == volp->ro.ID ||
	       fidp->volume == volp->bk.ID))
	    break;
    }	
#endif /* SEARCH_ALL_VOLUMES */

    hash = CM_VOLUME_ID_HASH(fidp->volume);
    /* The volumeID can be any one of the three types.  So we must
     * search the hash table for all three types until we find it.
     * We will search in the order of RO, RW, BK.
     */
    for ( volp = cm_data.volumeROIDHashTablep[hash]; volp; volp = volp->ro.nextp) {
        if ( cellp == volp->cellp && fidp->volume == volp->ro.ID )
            break;
    }
    if (!volp) {
        /* try RW volumes */
        for ( volp = cm_data.volumeRWIDHashTablep[hash]; volp; volp = volp->rw.nextp) {
            if ( cellp == volp->cellp && fidp->volume == volp->rw.ID )
                break;
        }
    }
    if (!volp) {
        /* try BK volumes */
        for ( volp = cm_data.volumeBKIDHashTablep[hash]; volp; volp = volp->bk.nextp) {
            if ( cellp == volp->cellp && fidp->volume == volp->bk.ID )
                break;
        }
    }

#ifdef SEARCH_ALL_VOLUMES
    osi_assertx(volp == volp2, "unexpected cm_vol_t");
#endif

    lock_ReleaseRead(&cm_volumeLock);

    /* hold the volume if we found it */
    if (volp) 
	cm_GetVolume(volp);

    /* update it */
    cm_data.mountRootGen = time(NULL);
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
cm_serverRef_t **cm_GetVolServers(cm_volume_t *volp, afs_uint32 volume)
{
    cm_serverRef_t **serverspp;
    cm_serverRef_t *current;;

    lock_ObtainWrite(&cm_serverLock);

    if (volume == volp->rw.ID)
        serverspp = &volp->rw.serversp;
    else if (volume == volp->ro.ID)
        serverspp = &volp->ro.serversp;
    else if (volume == volp->bk.ID)
        serverspp = &volp->bk.serversp;
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
    osi_assertx(volp->refCount-- > 0, "cm_volume_t refCount 0");
    lock_ReleaseWrite(&cm_volumeLock);
}

/* return the read-only volume, if there is one, or the read-write volume if
 * not.
 */
long cm_GetROVolumeID(cm_volume_t *volp)
{
    long id;

    lock_ObtainMutex(&volp->mx);
    if (volp->ro.ID && volp->ro.serversp)
	id = volp->ro.ID;
    else
	id = volp->rw.ID;
    lock_ReleaseMutex(&volp->mx);

    return id;
}

void cm_RefreshVolumes(void)
{
    cm_volume_t *volp;
    cm_scache_t *scp;

    cm_data.mountRootGen = time(NULL);

    /* force a re-loading of volume data from the vldb */
    lock_ObtainWrite(&cm_volumeLock);
    for (volp = cm_data.allVolumesp; volp; volp=volp->allNextp) {
	volp->refCount++;
	lock_ReleaseWrite(&cm_volumeLock);

	lock_ObtainMutex(&volp->mx);
	volp->flags |= CM_VOLUMEFLAG_RESET;
	lock_ReleaseMutex(&volp->mx);
	
        lock_ObtainWrite(&cm_volumeLock);
	osi_assertx(volp->refCount-- > 0, "cm_volume_t refCount 0");
    }
    lock_ReleaseWrite(&cm_volumeLock);

    /* force mount points to be re-evaluated so that 
     * if the volume location has changed we will pick 
     * that up
     */
    for ( scp = cm_data.scacheLRUFirstp; 
          scp;
          scp = (cm_scache_t *) osi_QNext(&scp->q)) {
        if ( scp->fileType == CM_SCACHETYPE_MOUNTPOINT 
#ifdef AFS_FREELANCE_CLIENT
             && !(scp->fid.cell == AFS_FAKE_ROOT_CELL_ID && scp->fid.volume == AFS_FAKE_ROOT_VOL_ID)
#endif
             ) {
            lock_ObtainMutex(&scp->mx);
            scp->mountPointStringp[0] = '\0';
            lock_ReleaseMutex(&scp->mx);
        }
    }

}


/* The return code is 0 if the volume is not online and 
 * 1 if the volume is online
 */
long
cm_CheckOfflineVolume(cm_volume_t *volp, afs_uint32 volID)
{
    cm_conn_t *connp;
    long code;
    AFSFetchVolumeStatus volStat;
    char *Name;
    char *OfflineMsg;
    char *MOTD;
    cm_req_t req;
    struct rx_connection * callp;
    char volName[32];
    char offLineMsg[256];
    char motd[256];
    long online = 0;
    cm_serverRef_t *serversp;

    Name = volName;
    OfflineMsg = offLineMsg;
    MOTD = motd;

    lock_ObtainMutex(&volp->mx);

    if (volp->rw.ID != 0 && (!volID || volID == volp->rw.ID) &&
         (volp->rw.state == vl_busy || volp->rw.state == vl_offline)) {
        cm_InitReq(&req);

        for (serversp = volp->rw.serversp; serversp; serversp = serversp->next) {
            if (serversp->status == srv_busy || serversp->status == srv_offline)
                serversp->status = srv_not_busy;
        }

        do {
            code = cm_ConnFromVolume(volp, volp->rw.ID, cm_rootUserp, &req, &connp);
            if (code) 
                continue;

            callp = cm_GetRxConn(connp);
            code = RXAFS_GetVolumeStatus(callp, volp->rw.ID,
                                          &volStat, &Name, &OfflineMsg, &MOTD);
            rx_PutConnection(callp);        

        } while (cm_Analyze(connp, cm_rootUserp, &req, NULL, NULL, NULL, NULL, code));
        code = cm_MapRPCError(code, &req);

        if (code == 0 && volStat.Online) {
            cm_VolumeStatusNotification(volp, volp->rw.ID, volp->rw.state, vl_online);
            volp->rw.state = vl_online;
            online = 1;
        }
    }

    if (volp->ro.ID != 0 && (!volID || volID == volp->ro.ID) &&
         (volp->ro.state == vl_busy || volp->ro.state == vl_offline)) {
        cm_InitReq(&req);

        for (serversp = volp->ro.serversp; serversp; serversp = serversp->next) {
            if (serversp->status == srv_busy || serversp->status == srv_offline)
                serversp->status = srv_not_busy;
        }

        do {
            code = cm_ConnFromVolume(volp, volp->ro.ID, cm_rootUserp, &req, &connp);
            if (code) 
                continue;

            callp = cm_GetRxConn(connp);
            code = RXAFS_GetVolumeStatus(callp, volp->ro.ID,
                                          &volStat, &Name, &OfflineMsg, &MOTD);
            rx_PutConnection(callp);        

        } while (cm_Analyze(connp, cm_rootUserp, &req, NULL, NULL, NULL, NULL, code));
        code = cm_MapRPCError(code, &req);

        if (code == 0 && volStat.Online) {
            cm_VolumeStatusNotification(volp, volp->ro.ID, volp->ro.state, vl_online);
            volp->ro.state = vl_online;
            online = 1;
        }
    }

    if (volp->bk.ID != 0 && (!volID || volID == volp->bk.ID) &&
         (volp->bk.state == vl_busy || volp->bk.state == vl_offline)) {
        cm_InitReq(&req);

        for (serversp = volp->bk.serversp; serversp; serversp = serversp->next) {
            if (serversp->status == srv_busy || serversp->status == srv_offline)
                serversp->status = srv_not_busy;
        }

        do {
            code = cm_ConnFromVolume(volp, volp->bk.ID, cm_rootUserp, &req, &connp);
            if (code) 
                continue;

            callp = cm_GetRxConn(connp);
            code = RXAFS_GetVolumeStatus(callp, volp->bk.ID,
                                          &volStat, &Name, &OfflineMsg, &MOTD);
            rx_PutConnection(callp);        

        } while (cm_Analyze(connp, cm_rootUserp, &req, NULL, NULL, NULL, NULL, code));
        code = cm_MapRPCError(code, &req);

        if (code == 0 && volStat.Online) {
            cm_VolumeStatusNotification(volp, volp->bk.ID, volp->bk.state, vl_online);
            volp->bk.state = vl_online;
            online = 1;
        }
    }

    lock_ReleaseMutex(&volp->mx);
    return online;
}


/* called from the Daemon thread */
void cm_CheckOfflineVolumes(void)
{
    cm_volume_t *volp;

    lock_ObtainWrite(&cm_volumeLock);
    for (volp = cm_data.allVolumesp; volp; volp=volp->allNextp) {
	volp->refCount++;
	lock_ReleaseWrite(&cm_volumeLock);

        cm_CheckOfflineVolume(volp, 0);

	lock_ObtainWrite(&cm_volumeLock);
	osi_assertx(volp->refCount-- > 0, "cm_volume_t refCount 0");
    }
    lock_ReleaseWrite(&cm_volumeLock);
}

void
cm_UpdateVolumeStatus(cm_volume_t *volp, afs_uint32 volID)
{
    struct cm_vol_state * statep = NULL;
    enum volstatus newStatus;
    cm_serverRef_t *tsrp;
    cm_server_t *tsp;
    int someBusy = 0, someOffline = 0, allOffline = 1, allBusy = 1, allDown = 1;

    if (volp->rw.ID == volID) {
        statep = &volp->rw;
    } else if (volp->ro.ID == volID) {
        statep = &volp->ro;
    } else if (volp->bk.ID == volID) {
        statep = &volp->bk;
    }

    if (!statep) {
#ifdef DEBUG
        DebugBreak();
#endif
        return;
    }

    lock_ObtainWrite(&cm_serverLock);
    for (tsrp = statep->serversp; tsrp; tsrp=tsrp->next) {
        tsp = tsrp->server;
        cm_GetServerNoLock(tsp);
        if (!(tsp->flags & CM_SERVERFLAG_DOWN)) {
	    allDown = 0;
            if (tsrp->status == srv_busy) {
		allOffline = 0;
                someBusy = 1;
            } else if (tsrp->status == srv_offline) {
		allBusy = 0;
		someOffline = 1;
            } else {
		allOffline = 0;
                allBusy = 0;
            }
        }
        cm_PutServerNoLock(tsp);
    }   
    lock_ReleaseWrite(&cm_serverLock);

    if (allDown)
	newStatus = vl_alldown;
    else if (allBusy || (someBusy && someOffline)) 
	newStatus = vl_busy;
    else if (allOffline)
	newStatus = vl_offline;
    else
	newStatus = vl_online;


    if (statep->ID && statep->state != newStatus)
        cm_VolumeStatusNotification(volp, statep->ID, statep->state, newStatus);

    statep->state = newStatus;
}

/*
** Finds all volumes that reside on this server and reorders their
** RO list according to the changed rank of server.
*/
void cm_ChangeRankVolume(cm_server_t *tsp)
{	
    int 		code;
    cm_volume_t*	volp;

    /* find volumes which might have RO copy on server*/
    lock_ObtainWrite(&cm_volumeLock);
    for(volp = cm_data.allVolumesp; volp; volp=volp->allNextp)
    {
	code = 1 ;	/* assume that list is unchanged */
	volp->refCount++;
	lock_ReleaseWrite(&cm_volumeLock);
	lock_ObtainMutex(&volp->mx);

	if ((tsp->cellp==volp->cellp) && (volp->ro.serversp))
	    code =cm_ChangeRankServer(&volp->ro.serversp, tsp);

	/* this volume list was changed */
	if ( !code )
	    cm_RandomizeServer(&volp->ro.serversp);

	lock_ReleaseMutex(&volp->mx);
	lock_ObtainWrite(&cm_volumeLock);
	osi_assertx(volp->refCount-- > 0, "cm_volume_t refCount 0");
    }
    lock_ReleaseWrite(&cm_volumeLock);
}	

/* dump all volumes that have reference count > 0 to a file. 
 * cookie is used to identify this batch for easy parsing, 
 * and it a string provided by a caller 
 */
int cm_DumpVolumes(FILE *outputFile, char *cookie, int lock)
{
    int zilch;
    cm_volume_t *volp;
    char output[1024];
  
    if (lock) {
	lock_ObtainRead(&cm_scacheLock);
        lock_ObtainRead(&cm_volumeLock);
    }
  
    sprintf(output, "%s - dumping volumes - cm_data.currentVolumes=%d, cm_data.maxVolumes=%d\r\n", cookie, cm_data.currentVolumes, cm_data.maxVolumes);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
  
    for (volp = cm_data.allVolumesp; volp; volp=volp->allNextp)
    {
        cm_scache_t *scp;
        int scprefs = 0;

        for (scp = cm_data.allSCachesp; scp; scp = scp->allNextp) 
        {
            if (scp->volp == volp)
                scprefs++;
        }   

        sprintf(output, "%s - volp=0x%p cell=%s name=%s rwID=%u roID=%u bkID=%u flags=0x%x fid (cell=%d, volume=%d, vnode=%d, unique=%d) refCount=%u scpRefs=%u\r\n", 
                 cookie, volp, volp->cellp->name, volp->namep, volp->rw.ID, volp->ro.ID, volp->bk.ID, volp->flags, 
                 volp->dotdotFid.cell, volp->dotdotFid.volume, volp->dotdotFid.vnode, volp->dotdotFid.unique,
                 volp->refCount, scprefs);
        WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
    }
    sprintf(output, "%s - Done dumping volumes.\r\n", cookie);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
  
    if (lock) {
        lock_ReleaseRead(&cm_volumeLock);
	lock_ReleaseRead(&cm_scacheLock);
    }
    return (0);     
}


/* 
 * String hash function used by SDBM project.
 * It was chosen because it is fast and provides
 * decent coverage.
 */
afs_uint32 SDBMHash(const char * str)
{
    afs_uint32 hash = 0;
    size_t i, len;

    if (str == NULL)
        return 0;

    for(i = 0, len = strlen(str); i < len; i++)
    {
        hash = str[i] + (hash << 6) + (hash << 16) - hash;
    }

    return (hash & 0x7FFFFFFF);
}

/* call with volume write-locked and mutex held */
void cm_AddVolumeToNameHashTable(cm_volume_t *volp)
{
    int i;
    
    if (volp->flags & CM_VOLUMEFLAG_IN_HASH)
        return;

    i = CM_VOLUME_NAME_HASH(volp->namep);

    volp->nameNextp = cm_data.volumeNameHashTablep[i];
    cm_data.volumeNameHashTablep[i] = volp;
    volp->flags |= CM_VOLUMEFLAG_IN_HASH;
}

/* call with volume write-locked and mutex held */
void cm_RemoveVolumeFromNameHashTable(cm_volume_t *volp)
{
    cm_volume_t **lvolpp;
    cm_volume_t *tvolp;
    int i;
	
    if (volp->flags & CM_VOLUMEFLAG_IN_HASH) {
	/* hash it out first */
	i = CM_VOLUME_NAME_HASH(volp->namep);
	for (lvolpp = &cm_data.volumeNameHashTablep[i], tvolp = cm_data.volumeNameHashTablep[i];
	     tvolp;
	     lvolpp = &tvolp->nameNextp, tvolp = tvolp->nameNextp) {
	    if (tvolp == volp) {
		*lvolpp = volp->nameNextp;
		volp->flags &= ~CM_VOLUMEFLAG_IN_HASH;
                volp->nameNextp = NULL;
		break;
	    }
	}
    }
}

/* call with volume write-locked and mutex held */
void cm_AddVolumeToIDHashTable(cm_volume_t *volp, afs_uint32 volType)
{
    int i;
    struct cm_vol_state * statep;

    switch (volType) {
    case RWVOL:
        statep = &volp->rw;
        break;
    case ROVOL:                                
        statep = &volp->ro;
        break;
    case BACKVOL:
        statep = &volp->bk;
        break;
    default:
        return;
    }

    if (statep->flags & CM_VOLUMEFLAG_IN_HASH)
        return;

    i = CM_VOLUME_ID_HASH(statep->ID);

    switch (volType) {
    case RWVOL:
        statep->nextp = cm_data.volumeRWIDHashTablep[i];
        cm_data.volumeRWIDHashTablep[i] = volp;
        break;
    case ROVOL:                                
        statep->nextp = cm_data.volumeROIDHashTablep[i];
        cm_data.volumeROIDHashTablep[i] = volp;
        break;
    case BACKVOL:
        statep->nextp = cm_data.volumeBKIDHashTablep[i];
        cm_data.volumeBKIDHashTablep[i] = volp;
        break;
    }
    statep->flags |= CM_VOLUMEFLAG_IN_HASH;
}


/* call with volume write-locked and mutex held */
void cm_RemoveVolumeFromIDHashTable(cm_volume_t *volp, afs_uint32 volType)
{
    cm_volume_t **lvolpp;
    cm_volume_t *tvolp;
    struct cm_vol_state * statep;
    int i;
	
    switch (volType) {
    case RWVOL:
        statep = &volp->rw;
        break;
    case ROVOL:                                
        statep = &volp->ro;
        break;
    case BACKVOL:
        statep = &volp->bk;
        break;
    default:
        return;
    }

    if (statep->flags & CM_VOLUMEFLAG_IN_HASH) {
	/* hash it out first */
        i = CM_VOLUME_ID_HASH(statep->ID);

        switch (volType) {
        case RWVOL:
            lvolpp = &cm_data.volumeRWIDHashTablep[i];
            tvolp = cm_data.volumeRWIDHashTablep[i];
            break;
        case ROVOL:
            lvolpp = &cm_data.volumeROIDHashTablep[i];
            tvolp = cm_data.volumeROIDHashTablep[i];
            break;
        case BACKVOL:
            lvolpp = &cm_data.volumeBKIDHashTablep[i];
            tvolp = cm_data.volumeBKIDHashTablep[i];
            break;
        }
	do {
	    if (tvolp == volp) {
		*lvolpp = statep->nextp;
                statep->flags &= ~CM_VOLUMEFLAG_IN_HASH;
                statep->nextp = NULL;
		break;
	    }

            switch (volType) {
            case RWVOL:
                lvolpp = &tvolp->rw.nextp;
                tvolp = tvolp->rw.nextp;
                break;
            case ROVOL:                                
                lvolpp = &tvolp->ro.nextp;
                tvolp = tvolp->ro.nextp;
                break;
            case BACKVOL:
                lvolpp = &tvolp->bk.nextp;
                tvolp = tvolp->bk.nextp;
                break;
            }
	} while(tvolp);
    }
}

/* must be called with cm_volumeLock write-locked! */
void cm_AdjustVolumeLRU(cm_volume_t *volp)
{
    if (volp == cm_data.volumeLRULastp)
        cm_data.volumeLRULastp = (cm_volume_t *) osi_QPrev(&volp->q);
    if (volp->flags & CM_VOLUMEFLAG_IN_LRU_QUEUE)
        osi_QRemoveHT((osi_queue_t **) &cm_data.volumeLRUFirstp, (osi_queue_t **) &cm_data.volumeLRULastp, &volp->q);
    osi_QAdd((osi_queue_t **) &cm_data.volumeLRUFirstp, &volp->q);
    volp->flags |= CM_VOLUMEFLAG_IN_LRU_QUEUE;
    if (!cm_data.volumeLRULastp) 
        cm_data.volumeLRULastp = volp;
}

/* must be called with cm_volumeLock write-locked! */
void cm_RemoveVolumeFromLRU(cm_volume_t *volp)
{
    if (volp->flags & CM_VOLUMEFLAG_IN_LRU_QUEUE) {
        if (volp == cm_data.volumeLRULastp)
            cm_data.volumeLRULastp = (cm_volume_t *) osi_QPrev(&volp->q);
        osi_QRemoveHT((osi_queue_t **) &cm_data.volumeLRUFirstp, (osi_queue_t **) &cm_data.volumeLRULastp, &volp->q);
        volp->flags &= ~CM_VOLUMEFLAG_IN_LRU_QUEUE;
    }
}

static char * volstatus_str(enum volstatus vs)
{
    switch (vs) {
    case vl_online:
        return "online";
    case vl_busy:
        return "busy";
    case vl_offline:
        return "offline";
    case vl_alldown:
        return "alldown";
    default:
        return "unknown";
    }
}

void cm_VolumeStatusNotification(cm_volume_t * volp, afs_uint32 volID, enum volstatus old, enum volstatus new)
{
    char volstr[CELL_MAXNAMELEN + VL_MAXNAMELEN]="";
    char *ext = "";

    if (volID == volp->rw.ID)
        ext = "";
    else if (volID == volp->ro.ID)
        ext = ".readonly";
    else if (volID == volp->bk.ID)
        ext = ".backup";
    else
        ext = ".nomatch";
    snprintf(volstr, sizeof(volstr), "%s:%s%s", volp->cellp->name, volp->namep, ext);

    osi_Log4(afsd_logp, "VolumeStatusNotification: %-48s [%10u] (%s -> %s)",
             osi_LogSaveString(afsd_logp, volstr), volID, volstatus_str(old), volstatus_str(new));

    cm_VolStatus_Change_Notification(volp->cellp->cellID, volID, new);
}       

enum volstatus cm_GetVolumeStatus(cm_volume_t *volp, afs_uint32 volID)
{
    if (volp->rw.ID == volID) {
        return volp->rw.state;
    } else if (volp->ro.ID == volID) {
        return volp->ro.state;
    } else if (volp->bk.ID == volID) {
        return volp->bk.state;
    } else {
        return vl_unknown;
    }
}


void 
cm_VolumeRenewROCallbacks(void)
{
    cm_volume_t * volp;


    lock_ObtainRead(&cm_volumeLock);
    for (volp = cm_data.allVolumesp; volp; volp=volp->allNextp) {
        if ( volp->cbExpiresRO > 0) {
            cm_req_t      req;
            cm_fid_t      fid;
            cm_scache_t * scp;

            fid.cell = volp->cellp->cellID;
            fid.volume = volp->ro.ID;
            fid.vnode = 1;
            fid.unique = 1;

            cm_InitReq(&req);

            lock_ReleaseRead(&cm_volumeLock);
            if (cm_GetSCache(&fid, &scp, cm_rootUserp, &req) == 0) {
                lock_ObtainMutex(&scp->mx);
                cm_GetCallback(scp, cm_rootUserp, &req, 1);
                lock_ReleaseMutex(&scp->mx);
                cm_ReleaseSCache(scp);
            }
            lock_ObtainRead(&cm_volumeLock);
        }
    }
    lock_ReleaseRead(&cm_volumeLock);
}
