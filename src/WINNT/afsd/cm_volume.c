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
#include <strsafe.h>
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
        afs_uint32 volType;
        for ( volType = RWVOL; volType < NUM_VOL_TYPES; volType++) {
            if (volp->vol[volType].ID)
                cm_VolumeStatusNotification(volp, volp->vol[volType].ID, volp->vol[volType].state, vl_alldown);
        }
        volp->cbExpiresRO = 0;
        volp->cbServerpRO = NULL;
        lock_FinalizeRWLock(&volp->rw);
    }

    return 0;
}

void cm_InitVolume(int newFile, long maxVols)
{
    static osi_once_t once;

    if (osi_Once(&once)) {
        lock_InitializeRWLock(&cm_volumeLock, "cm global volume lock", LOCK_HIERARCHY_VOLUME_GLOBAL);

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
                afs_uint32 volType;

                lock_InitializeRWLock(&volp->rw, "cm_volume_t rwlock", LOCK_HIERARCHY_VOLUME);
                _InterlockedOr(&volp->flags, CM_VOLUMEFLAG_RESET);
                _InterlockedAnd(&volp->flags, ~CM_VOLUMEFLAG_UPDATING_VL);
                volp->lastUpdateTime = 0;
                for (volType = RWVOL; volType < NUM_VOL_TYPES; volType++) {
                    volp->vol[volType].state = vl_unknown;
                    volp->vol[volType].serversp = NULL;
                    if (volp->vol[volType].ID)
                        cm_VolumeStatusNotification(volp, volp->vol[volType].ID, vl_unknown, volp->vol[volType].state);
                }
                volp->cbExpiresRO = 0;
                volp->cbServerpRO = NULL;
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
 * Update a volume.  Caller holds a write lock on the volume (volp->rw).
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
static long
cm_GetEntryByName( struct cm_cell *cellp, const char *name,
                   struct vldbentry *vldbEntryp,
                   struct nvldbentry *nvldbEntryp,
                   struct uvldbentry *uvldbEntryp,
                   int *methodp,
                   cm_user_t *userp,
                   cm_req_t *reqp
                   )
{
    long code;
    cm_conn_t *connp;
    struct rx_connection * rxconnp;

    osi_Log2(afsd_logp, "CALL VL_GetEntryByName{UNO} name %s:%s",
              osi_LogSaveString(afsd_logp,cellp->name),
              osi_LogSaveString(afsd_logp,name));
    do {

        code = cm_ConnByMServers(cellp->vlServersp, FALSE, userp, reqp, &connp);
        if (code)
            continue;

        rxconnp = cm_GetRxConn(connp);
        code = VL_GetEntryByNameU(rxconnp, name, uvldbEntryp);
        *methodp = 2;
        if ( code == RXGEN_OPCODE )
        {
            code = VL_GetEntryByNameN(rxconnp, name, nvldbEntryp);
            *methodp = 1;
        }
        if ( code == RXGEN_OPCODE ) {
            code = VL_GetEntryByNameO(rxconnp, name, vldbEntryp);
            *methodp = 0;
        }
        rx_PutConnection(rxconnp);
    } while (cm_Analyze(connp, userp, reqp, NULL, 0, NULL, cellp->vlServersp, NULL, code));
    code = cm_MapVLRPCError(code, reqp);
    if ( code )
        osi_Log3(afsd_logp, "CALL VL_GetEntryByName{UNO} name %s:%s FAILURE, code 0x%x",
                  osi_LogSaveString(afsd_logp,cellp->name),
                  osi_LogSaveString(afsd_logp,name), code);
    else
        osi_Log2(afsd_logp, "CALL VL_GetEntryByName{UNO} name %s:%s SUCCESS",
                  osi_LogSaveString(afsd_logp,cellp->name),
                  osi_LogSaveString(afsd_logp,name));
    return code;
}

static long
cm_GetEntryByID( struct cm_cell *cellp, afs_uint32 id,
                 struct vldbentry *vldbEntryp,
                 struct nvldbentry *nvldbEntryp,
                 struct uvldbentry *uvldbEntryp,
                 int *methodp,
                 cm_user_t *userp,
                 cm_req_t *reqp
                 )
{
    char name[64];

    StringCbPrintf(name, sizeof(name), "%u", id);

    return cm_GetEntryByName(cellp, name, vldbEntryp, nvldbEntryp, uvldbEntryp, methodp, userp, reqp);
}

long cm_UpdateVolumeLocation(struct cm_cell *cellp, cm_user_t *userp, cm_req_t *reqp,
		     cm_volume_t *volp)
{
    struct rx_connection *rxconnp;
    cm_conn_t *connp;
    int i;
    afs_uint32 j, k;
    cm_serverRef_t *tsrp;
    cm_server_t *tsp;
    struct sockaddr_in tsockAddr;
    long tflags;
    u_long tempAddr;
    struct vldbentry vldbEntry;
    struct nvldbentry nvldbEntry;
    struct uvldbentry uvldbEntry;
    int method = -1;
    int ROcount = 0;
    long code;
    enum volstatus rwNewstate = vl_online;
    enum volstatus roNewstate = vl_online;
    enum volstatus bkNewstate = vl_online;
#ifdef AFS_FREELANCE_CLIENT
    int freelance = 0;
#endif
    afs_uint32 volType;
    time_t now;
    int replicated = 0;

    lock_AssertWrite(&volp->rw);

    /*
     * If the last volume update was in the last five
     * minutes and it did not exist, then avoid the RPC
     * and return No Such Volume immediately.
     */
    now = time(NULL);
    if ((volp->flags & CM_VOLUMEFLAG_NOEXIST) &&
        (now < volp->lastUpdateTime + 600))
    {
        return CM_ERROR_NOSUCHVOLUME;
    }

#ifdef AFS_FREELANCE_CLIENT
    if (cellp->cellID == AFS_FAKE_ROOT_CELL_ID)
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
        while (volp->flags & CM_VOLUMEFLAG_UPDATING_VL) {
            osi_Log3(afsd_logp, "cm_UpdateVolumeLocation sleeping name %s:%s flags 0x%x",
                     volp->cellp->name, volp->namep, volp->flags);
            osi_SleepW((LONG_PTR) &volp->flags, &volp->rw);
            lock_ObtainWrite(&volp->rw);
            osi_Log3(afsd_logp, "cm_UpdateVolumeLocation awake name %s:%s flags 0x%x",
                     volp->cellp->name, volp->namep, volp->flags);
            if (!(volp->flags & CM_VOLUMEFLAG_RESET)) {
                osi_Log3(afsd_logp, "cm_UpdateVolumeLocation nothing to do, waking others name %s:%s flags 0x%x",
                         volp->cellp->name, volp->namep, volp->flags);
                osi_Wakeup((LONG_PTR) &volp->flags);
                return 0;
            }
            now = time(NULL);
        }

        /* Do not query again if the last update attempt failed in the last 60 seconds */
        if ((volp->flags & CM_VOLUMEFLAG_RESET) && (volp->lastUpdateTime > now - 60))
        {
            osi_Log3(afsd_logp, "cm_UpdateVolumeLocation unsuccessful update in last 60 seconds -- name %s:%s flags 0x%x",
                      volp->cellp->name, volp->namep, volp->flags);
            return(CM_ERROR_ALLDOWN);
        }

        _InterlockedOr(&volp->flags, CM_VOLUMEFLAG_UPDATING_VL);

        /* Do not hold the volume lock across the RPC calls */
        lock_ReleaseWrite(&volp->rw);

        if (cellp->flags & CM_CELLFLAG_VLSERVER_INVALID)
            cm_UpdateCell(cellp, 0);

        /* now we have volume structure locked and held; make RPC to fill it */
        code = cm_GetEntryByName(cellp, volp->namep, &vldbEntry, &nvldbEntry,
                                 &uvldbEntry,
                                 &method, userp, reqp);

        /* We can end up here with code == CM_ERROR_NOSUCHVOLUME if the base volume name
         * does not exist and is not a numeric string but there might exist a .readonly volume.
         * If the base name doesn't exist we will not care about the .backup that might be left
         * behind since there should be no method to access it.
         */
        if (code == CM_ERROR_NOSUCHVOLUME &&
             _atoi64(volp->namep) == 0 &&
             volp->vol[RWVOL].ID == 0 &&
             strlen(volp->namep) < (VL_MAXNAMELEN - 9)) {
            char name[VL_MAXNAMELEN];

            snprintf(name, VL_MAXNAMELEN, "%s.readonly", volp->namep);

            /* now we have volume structure locked and held; make RPC to fill it */
            code = cm_GetEntryByName(cellp, name, &vldbEntry, &nvldbEntry,
                                     &uvldbEntry,
                                     &method, userp, reqp);
        }

        /*
         * What if there was a volume rename?  The volume name no longer exists but the
         * volume id might.  Try to refresh the volume location information based one
         * of the readwrite or readonly volume id.
         */
        if (code == CM_ERROR_NOSUCHVOLUME) {
            if (volp->vol[RWVOL].ID != 0) {
                code = cm_GetEntryByID(cellp, volp->vol[RWVOL].ID, &vldbEntry, &nvldbEntry,
                                       &uvldbEntry,
                                       &method, userp, reqp);
            } else if (volp->vol[ROVOL].ID != 0) {
                code = cm_GetEntryByID(cellp, volp->vol[ROVOL].ID, &vldbEntry, &nvldbEntry,
                                       &uvldbEntry,
                                       &method, userp, reqp);
            }
        }
        lock_ObtainWrite(&volp->rw);
    }

    if (code == 0) {
        afs_int32 flags;
        afs_int32 nServers;
        afs_int32 rwID;
        afs_int32 roID;
        afs_int32 bkID;
        afs_int32 serverNumber[NMAXNSERVERS];
        afs_int32 serverFlags[NMAXNSERVERS];
        afsUUID   serverUUID[NMAXNSERVERS];
        afs_int32 rwServers_alldown = 1;
        afs_int32 roServers_alldown = 1;
        afs_int32 bkServers_alldown = 1;
        char      name[VL_MAXNAMELEN];

#ifdef AFS_FREELANCE_CLIENT
	if (freelance)
	    rwServers_alldown = 0;
#endif

        /* clear out old bindings */
        for ( volType = RWVOL; volType < NUM_VOL_TYPES; volType++) {
            if (volp->vol[volType].serversp)
                cm_FreeServerList(&volp->vol[volType].serversp, CM_FREESERVERLIST_DELETE);
        }

        memset(serverUUID, 0, sizeof(serverUUID));

        switch ( method ) {
        case 0:
            flags = vldbEntry.flags;
            nServers = vldbEntry.nServers;
            replicated = (nServers > 0);
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
            replicated = (nServers > 0);
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
        case 2:
            flags = uvldbEntry.flags;
            nServers = uvldbEntry.nServers;
            replicated = (nServers > 0);
            rwID = uvldbEntry.volumeId[0];
            roID = uvldbEntry.volumeId[1];
            bkID = uvldbEntry.volumeId[2];
            for ( i=0, j=0; code == 0 && i<nServers && j<NMAXNSERVERS; i++ ) {
                if ( !(uvldbEntry.serverFlags[i] & VLSERVER_FLAG_UUID) ) {
                    serverFlags[j] = uvldbEntry.serverFlags[i];
                    serverNumber[j] = uvldbEntry.serverNumber[i].time_low;
                    j++;
                } else {
                    afs_uint32 * addrp, nentries, code, unique;
                    bulkaddrs  addrs;
                    ListAddrByAttributes attrs;
                    afsUUID uuid;

                    memset(&attrs, 0, sizeof(attrs));
                    attrs.Mask = VLADDR_UUID;
                    attrs.uuid = uvldbEntry.serverNumber[i];
                    memset(&uuid, 0, sizeof(uuid));
                    memset(&addrs, 0, sizeof(addrs));

                    do {
                        code = cm_ConnByMServers(cellp->vlServersp, FALSE, userp, reqp, &connp);
                        if (code)
                            continue;

                        rxconnp = cm_GetRxConn(connp);
                        code = VL_GetAddrsU(rxconnp, &attrs, &uuid, &unique, &nentries, &addrs);
                        rx_PutConnection(rxconnp);
                    } while (cm_Analyze(connp, userp, reqp, NULL, 0, NULL, cellp->vlServersp, NULL, code));

                    if ( code ) {
                        code = cm_MapVLRPCError(code, reqp);
                        osi_Log2(afsd_logp, "CALL VL_GetAddrsU serverNumber %u FAILURE, code 0x%x",
                                 i, code);
                        continue;
                    }
                    osi_Log1(afsd_logp, "CALL VL_GetAddrsU serverNumber %u SUCCESS", i);

                    addrp = addrs.bulkaddrs_val;
                    for (k = 0; k < nentries && j < NMAXNSERVERS; j++, k++) {
                        serverFlags[j] = uvldbEntry.serverFlags[i];
                        serverNumber[j] = addrp[k];
                        serverUUID[j] = uuid;
                    }

                    xdr_free((xdrproc_t) xdr_bulkaddrs, &addrs);

                    if (nentries == 0)
                        code = CM_ERROR_INVAL;
                }
            }
            nServers = j;					/* update the server count */
            strncpy(name, uvldbEntry.name, VL_MAXNAMELEN);
            name[VL_MAXNAMELEN - 1] = '\0';
            break;
        }

        /* decode the response */
        lock_ObtainWrite(&cm_volumeLock);
        if (!cm_VolNameIsID(volp->namep)) {
            size_t    len;

            len = strlen(name);

            if (len >= 8 && strcmp(name + len - 7, ".backup") == 0) {
                name[len - 7] = '\0';
            } else if (len >= 10 && strcmp(name + len - 9, ".readonly") == 0) {
                name[len - 9] = '\0';
            }

            osi_Log2(afsd_logp, "cm_UpdateVolume name %s -> %s",
                     osi_LogSaveString(afsd_logp,volp->namep), osi_LogSaveString(afsd_logp,name));

            if (volp->qflags & CM_VOLUME_QFLAG_IN_HASH)
                cm_RemoveVolumeFromNameHashTable(volp);

            strcpy(volp->namep, name);

            cm_AddVolumeToNameHashTable(volp);
        }

        if (flags & VLF_DFSFILESET) {
            _InterlockedOr(&volp->flags, CM_VOLUMEFLAG_DFS_VOLUME);
            osi_Log1(afsd_logp, "cm_UpdateVolume Volume Group '%s' is a DFS File Set.  Correct behavior is not implemented.",
                     osi_LogSaveString(afsd_logp, volp->namep));
        }

        if (flags & VLF_RWEXISTS) {
            if (volp->vol[RWVOL].ID != rwID) {
                if (volp->vol[RWVOL].qflags & CM_VOLUME_QFLAG_IN_HASH)
                    cm_RemoveVolumeFromIDHashTable(volp, RWVOL);
                volp->vol[RWVOL].ID = rwID;
                cm_AddVolumeToIDHashTable(volp, RWVOL);
            }
        } else {
            if (volp->vol[RWVOL].qflags & CM_VOLUME_QFLAG_IN_HASH)
                cm_RemoveVolumeFromIDHashTable(volp, RWVOL);
            volp->vol[RWVOL].ID = 0;
        }
        if (flags & VLF_ROEXISTS) {
            if (volp->vol[ROVOL].ID != roID) {
                if (volp->vol[ROVOL].qflags & CM_VOLUME_QFLAG_IN_HASH)
                    cm_RemoveVolumeFromIDHashTable(volp, ROVOL);
                volp->vol[ROVOL].ID = roID;
                cm_AddVolumeToIDHashTable(volp, ROVOL);
            }
            if (replicated)
                _InterlockedOr(&volp->vol[ROVOL].flags, CM_VOL_STATE_FLAG_REPLICATED);
            else
                _InterlockedAnd(&volp->vol[ROVOL].flags, ~CM_VOL_STATE_FLAG_REPLICATED);
        } else {
            if (volp->vol[ROVOL].qflags & CM_VOLUME_QFLAG_IN_HASH)
                cm_RemoveVolumeFromIDHashTable(volp, ROVOL);
            volp->vol[ROVOL].ID = 0;
        }
        if (flags & VLF_BACKEXISTS) {
            if (volp->vol[BACKVOL].ID != bkID) {
                if (volp->vol[BACKVOL].qflags & CM_VOLUME_QFLAG_IN_HASH)
                    cm_RemoveVolumeFromIDHashTable(volp, BACKVOL);
                volp->vol[BACKVOL].ID = bkID;
                cm_AddVolumeToIDHashTable(volp, BACKVOL);
            }
        } else {
            if (volp->vol[BACKVOL].qflags & CM_VOLUME_QFLAG_IN_HASH)
                cm_RemoveVolumeFromIDHashTable(volp, BACKVOL);
            volp->vol[BACKVOL].ID = 0;
        }
        lock_ReleaseWrite(&cm_volumeLock);
        for (i=0; i<nServers; i++) {
            /* create a server entry */
            tflags = serverFlags[i];
            if (tflags & VLSF_DONTUSE)
                continue;
            tsockAddr.sin_port = htons(7000);
            tsockAddr.sin_family = AF_INET;
            tempAddr = htonl(serverNumber[i]);
            tsockAddr.sin_addr.s_addr = tempAddr;
            tsp = cm_FindServer(&tsockAddr, CM_SERVER_FILE, FALSE);
            if (tsp && (method == 2) && (tsp->flags & CM_SERVERFLAG_UUID)) {
                /*
                 * Check to see if the uuid of the server we know at this address
                 * matches the uuid of the server we are being told about by the
                 * vlserver.  If not, ...?
                 */
                if (!afs_uuid_equal(&serverUUID[i], &tsp->uuid)) {
                    char uuid1[128], uuid2[128];
                    char hoststr[16];

                    afsUUID_to_string(&serverUUID[i], uuid1, sizeof(uuid1));
                    afsUUID_to_string(&tsp->uuid, uuid2, sizeof(uuid2));
                    afs_inet_ntoa_r(serverNumber[i], hoststr);

                    osi_Log3(afsd_logp, "cm_UpdateVolumeLocation UUIDs do not match! %s != %s (%s)",
                              osi_LogSaveString(afsd_logp, uuid1),
                              osi_LogSaveString(afsd_logp, uuid2),
                              osi_LogSaveString(afsd_logp, hoststr));
                }
            }
            if (!tsp) {
                /*
                 * cm_NewServer will probe the file server which in turn will
                 * update the state on the volume group object.  Do not probe
                 * in this thread.  It will block the thread and can result in
                 * a recursive call to cm_UpdateVolumeLocation().
                 */
                lock_ReleaseWrite(&volp->rw);
                tsp = cm_NewServer(&tsockAddr, CM_SERVER_FILE, cellp, &serverUUID[i], CM_FLAG_NOPROBE);
                lock_ObtainWrite(&volp->rw);
            }
            osi_assertx(tsp != NULL, "null cm_server_t");

            /*
             * if this server was created by fs setserverprefs
             * then it won't have either a cell assignment or
             * a server uuid.
             */
            if ( !tsp->cellp )
                tsp->cellp = cellp;
            if ( (method == 2) && !(tsp->flags & CM_SERVERFLAG_UUID) &&
                 !afs_uuid_is_nil(&serverUUID[i])) {
                tsp->uuid = serverUUID[i];
                _InterlockedOr(&tsp->flags, CM_SERVERFLAG_UUID);
            }

            /* and add it to the list(s). */
            /*
             * Each call to cm_NewServerRef() increments the
             * ref count of tsp.  These reference will be dropped,
             * if and when the volume is reset; see reset code
             * earlier in this function.
             */
            if ((tflags & VLSF_RWVOL) && (flags & VLF_RWEXISTS)) {
                tsrp = cm_NewServerRef(tsp, rwID);
                cm_InsertServerList(&volp->vol[RWVOL].serversp, tsrp);
                if (!(tsp->flags & CM_SERVERFLAG_DOWN))
                    rwServers_alldown = 0;
            }
            if ((tflags & VLSF_ROVOL) && (flags & VLF_ROEXISTS)) {
                tsrp = cm_NewServerRef(tsp, roID);
                cm_InsertServerList(&volp->vol[ROVOL].serversp, tsrp);
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
                cm_InsertServerList(&volp->vol[BACKVOL].serversp, tsrp);

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
            cm_RandomizeServer(&volp->vol[ROVOL].serversp);
        }

        rwNewstate = rwServers_alldown ? vl_alldown : vl_online;
        roNewstate = roServers_alldown ? vl_alldown : vl_online;
        bkNewstate = bkServers_alldown ? vl_alldown : vl_online;

        _InterlockedAnd(&volp->flags, ~CM_VOLUMEFLAG_NOEXIST);
    } else if (code == CM_ERROR_NOSUCHVOLUME || code == VL_NOENT || code == VL_BADNAME) {
        _InterlockedOr(&volp->flags, CM_VOLUMEFLAG_NOEXIST);
    } else {
        rwNewstate = roNewstate = bkNewstate = vl_alldown;
    }

    if (volp->vol[RWVOL].state != rwNewstate) {
        if (volp->vol[RWVOL].ID)
            cm_VolumeStatusNotification(volp, volp->vol[RWVOL].ID, volp->vol[RWVOL].state, rwNewstate);
        volp->vol[RWVOL].state = rwNewstate;
    }
    if (volp->vol[ROVOL].state != roNewstate) {
        if (volp->vol[ROVOL].ID)
            cm_VolumeStatusNotification(volp, volp->vol[ROVOL].ID, volp->vol[ROVOL].state, roNewstate);
        volp->vol[ROVOL].state = roNewstate;
    }
    if (volp->vol[BACKVOL].state != bkNewstate) {
        if (volp->vol[BACKVOL].ID)
            cm_VolumeStatusNotification(volp, volp->vol[BACKVOL].ID, volp->vol[BACKVOL].state, bkNewstate);
        volp->vol[BACKVOL].state = bkNewstate;
    }

    volp->lastUpdateTime = time(NULL);

    if (code == 0)
        _InterlockedAnd(&volp->flags, ~CM_VOLUMEFLAG_RESET);

    _InterlockedAnd(&volp->flags, ~CM_VOLUMEFLAG_UPDATING_VL);
    osi_Log4(afsd_logp, "cm_UpdateVolumeLocation done, waking others name %s:%s flags 0x%x code 0x%x",
             osi_LogSaveString(afsd_logp,volp->cellp->name),
             osi_LogSaveString(afsd_logp,volp->namep), volp->flags, code);
    osi_Wakeup((LONG_PTR) &volp->flags);

    return code;
}

/* Requires read or write lock on cm_volumeLock */
void cm_GetVolume(cm_volume_t *volp)
{
    InterlockedIncrement(&volp->refCount);
}

cm_volume_t *cm_GetVolumeByFID(cm_fid_t *fidp)
{
    cm_volume_t *volp;
    afs_uint32 hash;

    lock_ObtainRead(&cm_volumeLock);
    hash = CM_VOLUME_ID_HASH(fidp->volume);
    /* The volumeID can be any one of the three types.  So we must
     * search the hash table for all three types until we find it.
     * We will search in the order of RO, RW, BK.
     */
    for ( volp = cm_data.volumeROIDHashTablep[hash]; volp; volp = volp->vol[ROVOL].nextp) {
        if ( fidp->cell == volp->cellp->cellID && fidp->volume == volp->vol[ROVOL].ID )
            break;
    }
    if (!volp) {
        /* try RW volumes */
        for ( volp = cm_data.volumeRWIDHashTablep[hash]; volp; volp = volp->vol[RWVOL].nextp) {
            if ( fidp->cell == volp->cellp->cellID && fidp->volume == volp->vol[RWVOL].ID )
                break;
        }
    }
    if (!volp) {
        /* try BK volumes */
        for ( volp = cm_data.volumeBKIDHashTablep[hash]; volp; volp = volp->vol[BACKVOL].nextp) {
            if ( fidp->cell == volp->cellp->cellID && fidp->volume == volp->vol[BACKVOL].ID )
                break;
        }
    }

    /* hold the volume if we found it */
    if (volp)
        cm_GetVolume(volp);

    lock_ReleaseRead(&cm_volumeLock);
    return volp;
}

long cm_FindVolumeByID(cm_cell_t *cellp, afs_uint32 volumeID, cm_user_t *userp,
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
	     ((unsigned) volumeID == volp->vol[RWVOL].ID ||
	       (unsigned) volumeID == volp->vol[ROVOL].ID ||
	       (unsigned) volumeID == volp->vol[BACKVOL].ID))
	    break;
    }

    volp2 = volp;
#endif /* SEARCH_ALL_VOLUMES */

    hash = CM_VOLUME_ID_HASH(volumeID);
    /* The volumeID can be any one of the three types.  So we must
     * search the hash table for all three types until we find it.
     * We will search in the order of RO, RW, BK.
     */
    for ( volp = cm_data.volumeROIDHashTablep[hash]; volp; volp = volp->vol[ROVOL].nextp) {
        if ( cellp == volp->cellp && volumeID == volp->vol[ROVOL].ID )
            break;
    }
    if (!volp) {
        /* try RW volumes */
        for ( volp = cm_data.volumeRWIDHashTablep[hash]; volp; volp = volp->vol[RWVOL].nextp) {
            if ( cellp == volp->cellp && volumeID == volp->vol[RWVOL].ID )
                break;
        }
    }
    if (!volp) {
        /* try BK volumes */
        for ( volp = cm_data.volumeBKIDHashTablep[hash]; volp; volp = volp->vol[BACKVOL].nextp) {
            if ( cellp == volp->cellp && volumeID == volp->vol[BACKVOL].ID )
                break;
        }
    }

#ifdef SEARCH_ALL_VOLUMES
    osi_assertx(volp == volp2, "unexpected cm_vol_t");
#endif

    /* hold the volume if we found it */
    if (volp)
        cm_GetVolume(volp);

    lock_ReleaseRead(&cm_volumeLock);

    /* return it held */
    if (volp) {
        lock_ObtainWrite(&volp->rw);

        code = 0;
        if ((volp->flags & CM_VOLUMEFLAG_RESET) && !(flags & CM_GETVOL_FLAG_NO_RESET)) {
            code = cm_UpdateVolumeLocation(cellp, userp, reqp, volp);
        }
        lock_ReleaseWrite(&volp->rw);
        if (code == 0) {
            *outVolpp = volp;

            if (!(flags & CM_GETVOL_FLAG_NO_LRU_UPDATE)) {
                lock_ObtainWrite(&cm_volumeLock);
                cm_AdjustVolumeLRU(volp);
                lock_ReleaseWrite(&cm_volumeLock);
            }
        } else {
            lock_ObtainRead(&cm_volumeLock);
            cm_PutVolume(volp);
            lock_ReleaseRead(&cm_volumeLock);
        }
        return code;
    }

    /* otherwise, we didn't find it so consult the VLDB */
    sprintf(volNameString, "%u", volumeID);
    code = cm_FindVolumeByName(cellp, volNameString, userp, reqp,
			      flags | CM_GETVOL_FLAG_IGNORE_LINKED_CELL, outVolpp);

    if (code == CM_ERROR_NOSUCHVOLUME && cellp->linkedName[0] &&
        !(flags & CM_GETVOL_FLAG_IGNORE_LINKED_CELL)) {
        cm_cell_t *linkedCellp = cm_GetCell(cellp->linkedName, flags);

        if (linkedCellp)
            code = cm_FindVolumeByID(linkedCellp, volumeID, userp, reqp,
                                     flags | CM_GETVOL_FLAG_IGNORE_LINKED_CELL,
                                     outVolpp);
    }
    return code;
}


long cm_FindVolumeByName(struct cm_cell *cellp, char *volumeNamep,
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
        afs_uint32 volType;
        /* otherwise, get from VLDB */

        /*
         * Change to a write lock so that we have exclusive use of
         * the first cm_volume_t with a refCount of 0 so that we
         * have time to increment it.
         */
        lock_ConvertRToW(&cm_volumeLock);

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

            InterlockedIncrement(&volp->refCount);
            lock_ReleaseWrite(&cm_volumeLock);
            lock_ObtainWrite(&volp->rw);
            lock_ObtainWrite(&cm_volumeLock);

            osi_Log2(afsd_logp, "Recycling Volume %s:%s",
                     volp->cellp->name, volp->namep);

            /* The volp is removed from the LRU queue in order to
             * prevent two threads from attempting to recycle the
             * same object.  This volp must be re-inserted back into
             * the LRU queue before this function exits.
             */
            if (volp->qflags & CM_VOLUME_QFLAG_IN_LRU_QUEUE)
                cm_RemoveVolumeFromLRU(volp);
            if (volp->qflags & CM_VOLUME_QFLAG_IN_HASH)
                cm_RemoveVolumeFromNameHashTable(volp);

            for ( volType = RWVOL; volType < NUM_VOL_TYPES; volType++) {
                if (volp->vol[volType].qflags & CM_VOLUME_QFLAG_IN_HASH)
                    cm_RemoveVolumeFromIDHashTable(volp, volType);
                if (volp->vol[volType].ID)
                    cm_VolumeStatusNotification(volp, volp->vol[volType].ID, volp->vol[volType].state, vl_unknown);
                volp->vol[volType].ID = 0;
                cm_SetFid(&volp->vol[volType].dotdotFid, 0, 0, 0, 0);
                lock_ReleaseWrite(&cm_volumeLock);
                cm_FreeServerList(&volp->vol[volType].serversp, CM_FREESERVERLIST_DELETE);
                lock_ObtainWrite(&cm_volumeLock);
            }
	} else {
	    volp = &cm_data.volumeBaseAddress[cm_data.currentVolumes++];
	    memset(volp, 0, sizeof(cm_volume_t));
	    volp->magic = CM_VOLUME_MAGIC;
	    volp->allNextp = cm_data.allVolumesp;
	    cm_data.allVolumesp = volp;
	    lock_InitializeRWLock(&volp->rw, "cm_volume_t rwlock", LOCK_HIERARCHY_VOLUME);
            lock_ReleaseWrite(&cm_volumeLock);
            lock_ObtainWrite(&volp->rw);
            lock_ObtainWrite(&cm_volumeLock);
            volp->refCount = 1;	/* starts off held */
        }
	volp->cellp = cellp;
	strncpy(volp->namep, name, VL_MAXNAMELEN);
	volp->namep[VL_MAXNAMELEN-1] = '\0';
	volp->flags = CM_VOLUMEFLAG_RESET;
        volp->lastUpdateTime = 0;

        for ( volType = RWVOL; volType < NUM_VOL_TYPES; volType++) {
            volp->vol[volType].state = vl_unknown;
            volp->vol[volType].nextp = NULL;
            volp->vol[volType].flags = 0;
        }
        volp->cbExpiresRO = 0;
        volp->cbServerpRO = NULL;
        volp->creationDateRO = 0;
        cm_AddVolumeToNameHashTable(volp);
        lock_ReleaseWrite(&cm_volumeLock);
    }
    else {
        if (volp)
            cm_GetVolume(volp);
        lock_ReleaseRead(&cm_volumeLock);

        if (!volp)
            return CM_ERROR_NOSUCHVOLUME;

        lock_ObtainWrite(&volp->rw);
    }

    /* if we get here we are holding the mutex */
    if ((volp->flags & CM_VOLUMEFLAG_RESET) && !(flags & CM_GETVOL_FLAG_NO_RESET)) {
        code = cm_UpdateVolumeLocation(cellp, userp, reqp, volp);
    }
    lock_ReleaseWrite(&volp->rw);

    if (code == 0 && (type == BACKVOL && volp->vol[BACKVOL].ID == 0 ||
                      type == ROVOL && volp->vol[ROVOL].ID == 0))
        code = CM_ERROR_NOSUCHVOLUME;

    if (code == 0) {
        *outVolpp = volp;

        lock_ObtainWrite(&cm_volumeLock);
        if (!(volp->qflags & CM_VOLUME_QFLAG_IN_LRU_QUEUE) ||
             (flags & CM_GETVOL_FLAG_NO_LRU_UPDATE))
            cm_AdjustVolumeLRU(volp);
        lock_ReleaseWrite(&cm_volumeLock);
    } else {
        /*
         * do not return it to the caller but do insert it in the LRU
         * otherwise it will be lost
         */
        lock_ObtainWrite(&cm_volumeLock);
        if (!(volp->qflags & CM_VOLUME_QFLAG_IN_LRU_QUEUE) ||
             (flags & CM_GETVOL_FLAG_NO_LRU_UPDATE))
            cm_AdjustVolumeLRU(volp);
        cm_PutVolume(volp);
        lock_ReleaseWrite(&cm_volumeLock);
    }

    if (code == CM_ERROR_NOSUCHVOLUME && cellp->linkedName[0] &&
        !(flags & CM_GETVOL_FLAG_IGNORE_LINKED_CELL)) {
        cm_cell_t *linkedCellp = cm_GetCell(cellp->linkedName, flags);

        if (linkedCellp)
            code = cm_FindVolumeByName(linkedCellp, volumeNamep, userp, reqp,
                                       flags | CM_GETVOL_FLAG_IGNORE_LINKED_CELL,
                                       outVolpp);
    }
    return code;
}

/*
 * Only call this function in response to a VNOVOL or VMOVED error
 * from a file server.  Do not call it in response to CM_ERROR_NOSUCHVOLUME
 * as that can lead to recursive calls.
 */
long cm_ForceUpdateVolume(cm_fid_t *fidp, cm_user_t *userp, cm_req_t *reqp)
{
    cm_cell_t *cellp;
    cm_volume_t *volp;
#ifdef SEARCH_ALL_VOLUMES
    cm_volume_t *volp2;
#endif
    afs_uint32  hash;
    long code;

    if (!fidp)
        return CM_ERROR_INVAL;

    cellp = cm_FindCellByID(fidp->cell, 0);
    if (!cellp)
        return CM_ERROR_NOSUCHCELL;

    /* search for the volume */
    lock_ObtainRead(&cm_volumeLock);
#ifdef SEARCH_ALL_VOLUMES
    for(volp = cm_data.allVolumesp; volp; volp=volp->allNextp) {
	if (cellp == volp->cellp &&
	     (fidp->volume == volp->vol[RWVOL].ID ||
	       fidp->volume == volp->vol[ROVOL].ID ||
	       fidp->volume == volp->vol[BACKVOL].ID))
	    break;
    }
#endif /* SEARCH_ALL_VOLUMES */

    hash = CM_VOLUME_ID_HASH(fidp->volume);
    /* The volumeID can be any one of the three types.  So we must
     * search the hash table for all three types until we find it.
     * We will search in the order of RO, RW, BK.
     */
    for ( volp = cm_data.volumeROIDHashTablep[hash]; volp; volp = volp->vol[ROVOL].nextp) {
        if ( cellp == volp->cellp && fidp->volume == volp->vol[ROVOL].ID )
            break;
    }
    if (!volp) {
        /* try RW volumes */
        for ( volp = cm_data.volumeRWIDHashTablep[hash]; volp; volp = volp->vol[RWVOL].nextp) {
            if ( cellp == volp->cellp && fidp->volume == volp->vol[RWVOL].ID )
                break;
        }
    }
    if (!volp) {
        /* try BK volumes */
        for ( volp = cm_data.volumeBKIDHashTablep[hash]; volp; volp = volp->vol[BACKVOL].nextp) {
            if ( cellp == volp->cellp && fidp->volume == volp->vol[BACKVOL].ID )
                break;
        }
    }

#ifdef SEARCH_ALL_VOLUMES
    osi_assertx(volp == volp2, "unexpected cm_vol_t");
#endif
    /* hold the volume if we found it */
    if (volp)
	cm_GetVolume(volp);

    lock_ReleaseRead(&cm_volumeLock);

    if (!volp)
        return CM_ERROR_NOSUCHVOLUME;

    /* update it */
    cm_data.mountRootGen = time(NULL);
    lock_ObtainWrite(&volp->rw);
    _InterlockedOr(&volp->flags, CM_VOLUMEFLAG_RESET);
    volp->lastUpdateTime = 0;

    code = cm_UpdateVolumeLocation(cellp, userp, reqp, volp);
    lock_ReleaseWrite(&volp->rw);

    lock_ObtainRead(&cm_volumeLock);
    cm_PutVolume(volp);
    lock_ReleaseRead(&cm_volumeLock);

    return code;
}

/* find the appropriate servers from a volume */
cm_serverRef_t **cm_GetVolServers(cm_volume_t *volp, afs_uint32 volume, cm_user_t *userp, cm_req_t *reqp)
{
    cm_serverRef_t **serverspp;
    cm_serverRef_t *current;
    int firstTry = 1;

  start:
    lock_ObtainWrite(&cm_serverLock);

    if (volume == volp->vol[RWVOL].ID)
        serverspp = &volp->vol[RWVOL].serversp;
    else if (volume == volp->vol[ROVOL].ID)
        serverspp = &volp->vol[ROVOL].serversp;
    else if (volume == volp->vol[BACKVOL].ID)
        serverspp = &volp->vol[BACKVOL].serversp;
    else {
        lock_ReleaseWrite(&cm_serverLock);
        if (firstTry) {
            afs_int32 code;
            firstTry = 0;
            lock_ObtainWrite(&volp->rw);
            _InterlockedOr(&volp->flags, CM_VOLUMEFLAG_RESET);
            volp->lastUpdateTime = 0;
            code = cm_UpdateVolumeLocation(volp->cellp, userp, reqp, volp);
            lock_ReleaseWrite(&volp->rw);
            if (code == 0)
                goto start;
        }
        return NULL;
    }

    /*
     * Increment the refCount on deleted items as well.
     * They will be freed by cm_FreeServerList when they get to zero
     */
    for (current = *serverspp; current; current = current->next)
        cm_GetServerRef(current, TRUE);

    lock_ReleaseWrite(&cm_serverLock);

    return serverspp;
}

void cm_PutVolume(cm_volume_t *volp)
{
    afs_int32 refCount = InterlockedDecrement(&volp->refCount);
    osi_assertx(refCount >= 0, "cm_volume_t refCount underflow has occurred");
}

/* return the read-only volume, if there is one, or the read-write volume if
 * not.
 */
long cm_GetROVolumeID(cm_volume_t *volp)
{
    long id;

    lock_ObtainRead(&volp->rw);
    if (volp->vol[ROVOL].ID && volp->vol[ROVOL].serversp)
	id = volp->vol[ROVOL].ID;
    else
	id = volp->vol[RWVOL].ID;
    lock_ReleaseRead(&volp->rw);

    return id;
}

void cm_RefreshVolumes(int lifetime)
{
    cm_volume_t *volp;
    afs_int32 refCount;
    time_t now;

    now = time(NULL);

    /* force mount point target updates */
    if (cm_data.mountRootGen + lifetime <= now)
        cm_data.mountRootGen = now;

    /*
     * force a re-loading of volume data from the vldb
     * if the lifetime for the cached data has expired
     */
    lock_ObtainRead(&cm_volumeLock);
    for (volp = cm_data.allVolumesp; volp; volp=volp->allNextp) {
	InterlockedIncrement(&volp->refCount);
	lock_ReleaseRead(&cm_volumeLock);

        if (!(volp->flags & CM_VOLUMEFLAG_RESET)) {
            lock_ObtainWrite(&volp->rw);
            if (volp->lastUpdateTime + lifetime <= now) {
                _InterlockedOr(&volp->flags, CM_VOLUMEFLAG_RESET);
                volp->lastUpdateTime = 0;
            }
            lock_ReleaseWrite(&volp->rw);
        }

        lock_ObtainRead(&cm_volumeLock);
        refCount = InterlockedDecrement(&volp->refCount);
	osi_assertx(refCount >= 0, "cm_volume_t refCount underflow");
    }
    lock_ReleaseRead(&cm_volumeLock);
}

void
cm_CheckOfflineVolumeState(cm_volume_t *volp, cm_vol_state_t *statep, afs_uint32 volID,
                           afs_uint32 *onlinep, afs_uint32 *volumeUpdatedp)
{
    cm_conn_t *connp;
    long code;
    AFSFetchVolumeStatus volStat;
    char *Name;
    char *OfflineMsg;
    char *MOTD;
    cm_req_t req;
    struct rx_connection * rxconnp;
    char volName[32];
    char offLineMsg[256];
    char motd[256];
    long alldown, alldeleted;
    cm_serverRef_t *serversp;
    cm_fid_t fid;

    Name = volName;
    OfflineMsg = offLineMsg;
    MOTD = motd;

    if (statep->ID != 0 && (!volID || volID == statep->ID)) {
        /* create fid for volume root so that VNOVOL and VMOVED errors can be processed */
        cm_SetFid(&fid, volp->cellp->cellID, statep->ID, 1, 1);

        if (!statep->serversp && !(*volumeUpdatedp)) {
            cm_InitReq(&req);
            code = cm_UpdateVolumeLocation(volp->cellp, cm_rootUserp, &req, volp);
            *volumeUpdatedp = 1;
        }

        lock_ObtainRead(&cm_serverLock);
        if (statep->serversp) {
            alldown = 1;
            alldeleted = 1;
            for (serversp = statep->serversp; serversp; serversp = serversp->next) {
                if (serversp->status == srv_deleted)
                    continue;

                alldeleted = 0;

                if (!(serversp->server->flags & CM_SERVERFLAG_DOWN))
                    alldown = 0;

                if (serversp->status == srv_busy || serversp->status == srv_offline)
                    serversp->status = srv_not_busy;
            }
            lock_ReleaseRead(&cm_serverLock);

            if (alldeleted && !(*volumeUpdatedp)) {
                cm_InitReq(&req);
                code = cm_UpdateVolumeLocation(volp->cellp, cm_rootUserp, &req, volp);
                *volumeUpdatedp = 1;
            }

            if (statep->state == vl_busy || statep->state == vl_offline || statep->state == vl_unknown ||
                (!alldown && statep->state == vl_alldown)) {
                cm_InitReq(&req);
                req.flags |= CM_REQ_OFFLINE_VOL_CHK;

                lock_ReleaseWrite(&volp->rw);
                do {
                    code = cm_ConnFromVolume(volp, statep->ID, cm_rootUserp, &req, &connp);
                    if (code)
                        continue;

                    rxconnp = cm_GetRxConn(connp);
                    code = RXAFS_GetVolumeStatus(rxconnp, statep->ID,
                                                 &volStat, &Name, &OfflineMsg, &MOTD);
                    rx_PutConnection(rxconnp);
                } while (cm_Analyze(connp, cm_rootUserp, &req, &fid, 0, NULL, NULL, NULL, code));
                code = cm_MapRPCError(code, &req);

                lock_ObtainWrite(&volp->rw);
                if (code == 0 && volStat.Online) {
                    cm_VolumeStatusNotification(volp, statep->ID, statep->state, vl_online);
                    statep->state = vl_online;
                    *onlinep = 1;
                } else if (code == CM_ERROR_NOACCESS) {
                    cm_VolumeStatusNotification(volp, statep->ID, statep->state, vl_unknown);
                    statep->state = vl_unknown;
                    *onlinep = 1;
                }
            } else if (alldown && statep->state != vl_alldown) {
                cm_VolumeStatusNotification(volp, statep->ID, statep->state, vl_alldown);
                statep->state = vl_alldown;
            }
        } else {
            lock_ReleaseRead(&cm_serverLock);
            if (statep->state != vl_alldown) {
                cm_VolumeStatusNotification(volp, statep->ID, statep->state, vl_alldown);
                statep->state = vl_alldown;
            }
        }
    }
}

/* The return code is 0 if the volume is not online and
 * 1 if the volume is online
 */
long
cm_CheckOfflineVolume(cm_volume_t *volp, afs_uint32 volID)
{
    long code;
    cm_req_t req;
    afs_uint32 online = 0;
    afs_uint32 volumeUpdated = 0;

    lock_ObtainWrite(&volp->rw);

    if (volp->flags & CM_VOLUMEFLAG_RESET) {
        cm_InitReq(&req);
        code = cm_UpdateVolumeLocation(volp->cellp, cm_rootUserp, &req, volp);
        volumeUpdated = 1;
    }

    cm_CheckOfflineVolumeState(volp, &volp->vol[RWVOL], volID, &online, &volumeUpdated);
    cm_CheckOfflineVolumeState(volp, &volp->vol[ROVOL], volID, &online, &volumeUpdated);
    cm_CheckOfflineVolumeState(volp, &volp->vol[BACKVOL], volID, &online, &volumeUpdated);

    lock_ReleaseWrite(&volp->rw);
    return online;
}


/*
 * called from the Daemon thread.
 * when checking the offline status, check those of the most recently used volumes first.
 */
void cm_CheckOfflineVolumes(void)
{
    cm_volume_t *volp;
    afs_int32 refCount;
    extern int daemon_ShutdownFlag;
    extern int powerStateSuspended;

    lock_ObtainRead(&cm_volumeLock);
    for (volp = cm_data.volumeLRULastp;
         volp && !daemon_ShutdownFlag && !powerStateSuspended;
         volp=(cm_volume_t *) osi_QPrev(&volp->q)) {
        /*
         * Skip volume entries that did not exist last time
         * the vldb was queried.  For those entries wait until
         * the next actual request is received for the volume
         * before checking its state.
         */
        if ((volp->qflags & CM_VOLUME_QFLAG_IN_HASH) &&
            !(volp->flags & CM_VOLUMEFLAG_NOEXIST)) {
            InterlockedIncrement(&volp->refCount);
            lock_ReleaseRead(&cm_volumeLock);
            cm_CheckOfflineVolume(volp, 0);
            lock_ObtainRead(&cm_volumeLock);
            refCount = InterlockedDecrement(&volp->refCount);
            osi_assertx(refCount >= 0, "cm_volume_t refCount underflow");
        }
    }
    lock_ReleaseRead(&cm_volumeLock);
}


static void
cm_UpdateVolumeStatusInt(cm_volume_t *volp, struct cm_vol_state *statep)
{
    enum volstatus newStatus;
    cm_serverRef_t *tsrp;
    cm_server_t *tsp;
    int someBusy = 0, someOffline = 0, allOffline = 1, allBusy = 1, allDown = 1;
    char addr[16];

    if (!volp || !statep) {
#ifdef DEBUG
        DebugBreak();
#endif
        return;
    }

    lock_ObtainWrite(&cm_serverLock);
    for (tsrp = statep->serversp; tsrp; tsrp=tsrp->next) {
        tsp = tsrp->server;
        sprintf(addr, "%d.%d.%d.%d",
                 ((tsp->addr.sin_addr.s_addr & 0xff)),
                 ((tsp->addr.sin_addr.s_addr & 0xff00)>> 8),
                 ((tsp->addr.sin_addr.s_addr & 0xff0000)>> 16),
                 ((tsp->addr.sin_addr.s_addr & 0xff000000)>> 24));

        if (tsrp->status == srv_deleted) {
            osi_Log2(afsd_logp, "cm_UpdateVolumeStatusInt volume %d server reference %s deleted",
                     statep->ID, osi_LogSaveString(afsd_logp,addr));
            continue;
        }
        if (tsp) {
            cm_GetServerNoLock(tsp);
            if (!(tsp->flags & CM_SERVERFLAG_DOWN)) {
                allDown = 0;
                if (tsrp->status == srv_busy) {
                    osi_Log2(afsd_logp, "cm_UpdateVolumeStatusInt volume %d server reference %s busy",
                              statep->ID, osi_LogSaveString(afsd_logp,addr));
                    allOffline = 0;
                    someBusy = 1;
                } else if (tsrp->status == srv_offline) {
                    osi_Log2(afsd_logp, "cm_UpdateVolumeStatusInt volume %d server reference %s offline",
                              statep->ID, osi_LogSaveString(afsd_logp,addr));
                    allBusy = 0;
                    someOffline = 1;
                } else {
                    osi_Log2(afsd_logp, "cm_UpdateVolumeStatusInt volume %d server reference %s online",
                              statep->ID, osi_LogSaveString(afsd_logp,addr));
                    allOffline = 0;
                    allBusy = 0;
                }
            } else {
                osi_Log2(afsd_logp, "cm_UpdateVolumeStatusInt volume %d server reference %s down",
                          statep->ID, osi_LogSaveString(afsd_logp,addr));
            }
            cm_PutServerNoLock(tsp);
        }
    }
    lock_ReleaseWrite(&cm_serverLock);

    osi_Log5(afsd_logp, "cm_UpdateVolumeStatusInt allDown %d allBusy %d someBusy %d someOffline %d allOffline %d",
             allDown, allBusy, someBusy, someOffline, allOffline);

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

void
cm_UpdateVolumeStatus(cm_volume_t *volp, afs_uint32 volID)
{

    if (volp->vol[RWVOL].ID == volID) {
        cm_UpdateVolumeStatusInt(volp, &volp->vol[RWVOL]);
    } else if (volp->vol[ROVOL].ID == volID) {
        cm_UpdateVolumeStatusInt(volp, &volp->vol[ROVOL]);
    } else if (volp->vol[BACKVOL].ID == volID) {
        cm_UpdateVolumeStatusInt(volp, &volp->vol[BACKVOL]);
    } else {
        /*
         * If we are called with volID == 0 then something has gone wrong.
         * Most likely a race occurred in the server volume list maintenance.
         * Since we don't know which volume's status should be updated,
         * just update all of them that are known to exist.  Better to be
         * correct than fast.
         */
        afs_uint32 volType;
        for ( volType = RWVOL; volType < NUM_VOL_TYPES; volType++) {
            if (volp->vol[volType].ID != 0)
                cm_UpdateVolumeStatusInt(volp, &volp->vol[volType]);
        }
    }
}

/*
** Finds all volumes that reside on this server and reorders their
** RO list according to the changed rank of server.
*/
void cm_ChangeRankVolume(cm_server_t *tsp)
{
    int 		code;
    cm_volume_t*	volp;
    afs_int32 refCount;

    /* find volumes which might have RO copy on server*/
    lock_ObtainRead(&cm_volumeLock);
    for(volp = cm_data.allVolumesp; volp; volp=volp->allNextp)
    {
	code = 1 ;	/* assume that list is unchanged */
	InterlockedIncrement(&volp->refCount);
	lock_ReleaseRead(&cm_volumeLock);
	lock_ObtainWrite(&volp->rw);

	if ((tsp->cellp==volp->cellp) && (volp->vol[ROVOL].serversp))
	    code =cm_ChangeRankServer(&volp->vol[ROVOL].serversp, tsp);

	/* this volume list was changed */
	if ( !code )
	    cm_RandomizeServer(&volp->vol[ROVOL].serversp);

	lock_ReleaseWrite(&volp->rw);
	lock_ObtainRead(&cm_volumeLock);
        refCount = InterlockedDecrement(&volp->refCount);
	osi_assertx(refCount >= 0, "cm_volume_t refCount underflow");
    }
    lock_ReleaseRead(&cm_volumeLock);
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

    sprintf(output, "%s - dumping volumes - cm_data.currentVolumes=%d, cm_data.maxVolumes=%d\r\n",
            cookie, cm_data.currentVolumes, cm_data.maxVolumes);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);

    for (volp = cm_data.allVolumesp; volp; volp=volp->allNextp)
    {
        time_t t;
        char *srvStr = NULL;
        afs_uint32 srvStrRpc = TRUE;
        char *cbt = NULL;
        char *cdrot = NULL;

        if (volp->cbServerpRO) {
            if (!((volp->cbServerpRO->flags & CM_SERVERFLAG_UUID) &&
                UuidToString((UUID *)&volp->cbServerpRO->uuid, &srvStr) == RPC_S_OK)) {
                afs_asprintf(&srvStr, "%.0I", volp->cbServerpRO->addr.sin_addr.s_addr);
                srvStrRpc = FALSE;
            }
        }
        if (volp->cbExpiresRO) {
            t = volp->cbExpiresRO;
            cbt = ctime(&t);
            if (cbt) {
                cbt = strdup(cbt);
                cbt[strlen(cbt)-1] = '\0';
            }
        }
        if (volp->creationDateRO) {
            t = volp->creationDateRO;
            cdrot = ctime(&t);
            if (cdrot) {
                cdrot = strdup(cdrot);
                cdrot[strlen(cdrot)-1] = '\0';
            }
        }

        sprintf(output,
                "%s - volp=0x%p cell=%s name=%s rwID=%u roID=%u bkID=%u flags=0x%x:%x "
                "cbServerpRO='%s' cbExpiresRO='%s' creationDateRO='%s' refCount=%u\r\n",
                 cookie, volp, volp->cellp->name, volp->namep, volp->vol[RWVOL].ID,
                 volp->vol[ROVOL].ID, volp->vol[BACKVOL].ID, volp->flags, volp->qflags,
                 srvStr ? srvStr : "<none>", cbt ? cbt : "<none>", cdrot ? cdrot : "<none>",
                 volp->refCount);
        WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);

        if (srvStr) {
            if (srvStrRpc)
                RpcStringFree(&srvStr);
            else
                free(srvStr);
        }
        if (cbt)
            free(cbt);
        if (cdrot)
            free(cdrot);
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

    if (volp->qflags & CM_VOLUME_QFLAG_IN_HASH)
        return;

    i = CM_VOLUME_NAME_HASH(volp->namep);

    volp->nameNextp = cm_data.volumeNameHashTablep[i];
    cm_data.volumeNameHashTablep[i] = volp;
    _InterlockedOr(&volp->qflags, CM_VOLUME_QFLAG_IN_HASH);
}

/* call with volume write-locked and mutex held */
void cm_RemoveVolumeFromNameHashTable(cm_volume_t *volp)
{
    cm_volume_t **lvolpp;
    cm_volume_t *tvolp;
    int i;

    if (volp->qflags & CM_VOLUME_QFLAG_IN_HASH) {
	/* hash it out first */
	i = CM_VOLUME_NAME_HASH(volp->namep);
	for (lvolpp = &cm_data.volumeNameHashTablep[i], tvolp = cm_data.volumeNameHashTablep[i];
	     tvolp;
	     lvolpp = &tvolp->nameNextp, tvolp = tvolp->nameNextp) {
	    if (tvolp == volp) {
		*lvolpp = volp->nameNextp;
		_InterlockedAnd(&volp->qflags, ~CM_VOLUME_QFLAG_IN_HASH);
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

    statep = cm_VolumeStateByType(volp, volType);

    if (statep->qflags & CM_VOLUME_QFLAG_IN_HASH)
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
    _InterlockedOr(&statep->qflags, CM_VOLUME_QFLAG_IN_HASH);
}


/* call with volume write-locked and mutex held */
void cm_RemoveVolumeFromIDHashTable(cm_volume_t *volp, afs_uint32 volType)
{
    cm_volume_t **lvolpp;
    cm_volume_t *tvolp;
    struct cm_vol_state * statep;
    int i;

    statep = cm_VolumeStateByType(volp, volType);

    if (statep->qflags & CM_VOLUME_QFLAG_IN_HASH) {
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
        default:
            osi_assertx(0, "invalid volume type");
        }
	do {
	    if (tvolp == volp) {
		*lvolpp = statep->nextp;
                _InterlockedAnd(&statep->qflags, ~CM_VOLUME_QFLAG_IN_HASH);
                statep->nextp = NULL;
		break;
	    }

            lvolpp = &tvolp->vol[volType].nextp;
            tvolp = tvolp->vol[volType].nextp;
	} while(tvolp);
    }
}

/* must be called with cm_volumeLock write-locked! */
void cm_AdjustVolumeLRU(cm_volume_t *volp)
{
    lock_AssertWrite(&cm_volumeLock);

    if (volp == cm_data.volumeLRUFirstp)
        return;

    if (volp->qflags & CM_VOLUME_QFLAG_IN_LRU_QUEUE)
        osi_QRemoveHT((osi_queue_t **) &cm_data.volumeLRUFirstp, (osi_queue_t **) &cm_data.volumeLRULastp, &volp->q);
    osi_QAddH((osi_queue_t **) &cm_data.volumeLRUFirstp, (osi_queue_t **) &cm_data.volumeLRULastp, &volp->q);
    _InterlockedOr(&volp->qflags, CM_VOLUME_QFLAG_IN_LRU_QUEUE);

    osi_assertx(cm_data.volumeLRULastp != NULL, "null cm_data.volumeLRULastp");
}

/* must be called with cm_volumeLock write-locked! */
void cm_MoveVolumeToLRULast(cm_volume_t *volp)
{
    lock_AssertWrite(&cm_volumeLock);

    if (volp == cm_data.volumeLRULastp)
        return;

    if (volp->qflags & CM_VOLUME_QFLAG_IN_LRU_QUEUE)
        osi_QRemoveHT((osi_queue_t **) &cm_data.volumeLRUFirstp, (osi_queue_t **) &cm_data.volumeLRULastp, &volp->q);
    osi_QAddT((osi_queue_t **) &cm_data.volumeLRUFirstp, (osi_queue_t **) &cm_data.volumeLRULastp, &volp->q);
    _InterlockedOr(&volp->qflags, CM_VOLUME_QFLAG_IN_LRU_QUEUE);

    osi_assertx(cm_data.volumeLRULastp != NULL, "null cm_data.volumeLRULastp");
}

/* must be called with cm_volumeLock write-locked! */
void cm_RemoveVolumeFromLRU(cm_volume_t *volp)
{
    lock_AssertWrite(&cm_volumeLock);

    if (volp->qflags & CM_VOLUME_QFLAG_IN_LRU_QUEUE) {
        osi_QRemoveHT((osi_queue_t **) &cm_data.volumeLRUFirstp, (osi_queue_t **) &cm_data.volumeLRULastp, &volp->q);
        _InterlockedAnd(&volp->qflags, ~CM_VOLUME_QFLAG_IN_LRU_QUEUE);
    }

    osi_assertx(cm_data.volumeLRULastp != NULL, "null cm_data.volumeLRULastp");
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

    if (volID == volp->vol[RWVOL].ID)
        ext = "";
    else if (volID == volp->vol[ROVOL].ID)
        ext = ".readonly";
    else if (volID == volp->vol[BACKVOL].ID)
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
    cm_vol_state_t * statep = cm_VolumeStateByID(volp, volID);
    if (statep)
        return statep->state;
    else
        return vl_unknown;
}

/* Renew .readonly volume callbacks that are more than
 * 30 minutes old.  (A volume callback is issued for 2 hours.)
 */
void
cm_VolumeRenewROCallbacks(void)
{
    cm_volume_t * volp;
    time_t minexp = time(NULL) + 90 * 60;
    extern int daemon_ShutdownFlag;
    extern int powerStateSuspended;

    lock_ObtainRead(&cm_volumeLock);
    for (volp = cm_data.allVolumesp;
         volp && !daemon_ShutdownFlag && !powerStateSuspended;
         volp=volp->allNextp) {
        if ( volp->cbExpiresRO > 0 && volp->cbExpiresRO < minexp) {
            cm_req_t      req;
            cm_fid_t      fid;
            cm_scache_t * scp;

            cm_SetFid(&fid, volp->cellp->cellID, volp->vol[ROVOL].ID, 1, 1);

            cm_InitReq(&req);

            lock_ReleaseRead(&cm_volumeLock);
            if (cm_GetSCache(&fid, &scp, cm_rootUserp, &req) == 0) {
                lock_ObtainWrite(&scp->rw);
                cm_GetCallback(scp, cm_rootUserp, &req, 1);
                lock_ReleaseWrite(&scp->rw);
                cm_ReleaseSCache(scp);
            }
            lock_ObtainRead(&cm_volumeLock);
        }
    }
    lock_ReleaseRead(&cm_volumeLock);
}

cm_vol_state_t *
cm_VolumeStateByType(cm_volume_t *volp, afs_uint32 volType)
{
    return &volp->vol[volType];
}

cm_vol_state_t *
cm_VolumeStateByID(cm_volume_t *volp, afs_uint32 id)
{
    cm_vol_state_t * statep = NULL;

    if (id == volp->vol[RWVOL].ID)
        statep = &volp->vol[RWVOL];
    else if (id == volp->vol[ROVOL].ID)
        statep = &volp->vol[ROVOL];
    else if (id == volp->vol[BACKVOL].ID)
        statep = &volp->vol[BACKVOL];

    return(statep);
}

cm_vol_state_t *
cm_VolumeStateByName(cm_volume_t *volp, char *volname)
{
    size_t len = strlen(volname);
    cm_vol_state_t *statep;

    if (cm_stricmp_utf8N(".readonly", &volname[len-9]) == 0)
        statep = &volp->vol[ROVOL];
    else if (cm_stricmp_utf8N(".backup", &volname[len-7]) == 0)
        statep = &volp->vol[BACKVOL];
    else
        statep = &volp->vol[RWVOL];

    return statep;
}

afs_int32
cm_VolumeType(cm_volume_t *volp, afs_uint32 id)
{
    if (id == volp->vol[RWVOL].ID)
        return(RWVOL);
    else if (id == volp->vol[ROVOL].ID)
        return(ROVOL);
    else if (id == volp->vol[BACKVOL].ID)
        return (BACKVOL);

    return -1;
}
