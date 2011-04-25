/*
 *  Copyright (c) 2008 - Secure Endpoints Inc.
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include "afsd.h"

static cm_fid_stats_t ** fidStatsHashTablep = NULL;
static afs_uint32        fidStatsHashTableSize = 0;

/*
 * algorithm and implementation adapted from code written by
 * Frank Pilhofer <fp@fpx.de>
 */
afs_uint32 nearest_prime(afs_uint32 s)
{
#define TEST(f,x)	(*(f+(x)/16)&(1<<(((x)%16L)/2)))
#define SET(f,x)	*(f+(x)/16)|=1<<(((x)%16L)/2)
    unsigned char *feld=NULL, *zzz;
    afs_uint32 teste=1, mom, hits=1, count, max, alloc, largest_prime = 0;

    max = s + 10000L;

    while (feld==NULL)
        zzz = feld = malloc (alloc=((max>>4)+1L));

    for (count=0; count<alloc; count++)
        *zzz++ = 0x00;

    while ((teste+=2) < max) {
        if (!TEST(feld, teste)) {
            for (mom=3L*teste; mom<max; mom+=teste<<1)
                SET (feld, mom);
        }
    }

    count=s-2;
    if (s%2==0)
        count++;
    while ((count+=2)<max) {
        if (!TEST(feld,count)) {
            largest_prime = count;
            break;
        }
    }
    free (feld);
    return largest_prime;
}

/* We never free these objects so we don't need to
 * worry about reuse or tracking the allocations
 *
 * The management of the free allocation is not
 * thread safe because we never expect this code
 * to be called from more than one thread.
 */
cm_fid_stats_t * cm_PerformanceGetNew(void)
{
    static cm_fid_stats_t * allocp = NULL;
    static afs_int32        allocSize = 0;
    static afs_int32        nextFree = 0;

    if (nextFree < allocSize)
        return &allocp[nextFree++];

    allocp = (cm_fid_stats_t *)malloc(32768);
    if (allocp == NULL)
        return NULL;

    allocSize = 32768/sizeof(cm_fid_stats_t);
    memset(allocp, 0, 32768);
    nextFree = 1;
    return allocp;
}

void cm_PerformanceInsertToHashTable(cm_fid_stats_t *statp)
{
    afs_uint32 hash = statp->fid.hash % fidStatsHashTableSize;

    statp->nextp = fidStatsHashTablep[hash];
    fidStatsHashTablep[hash] = statp;
}

void cm_PerformanceAddSCache(cm_scache_t *scp)
{
    cm_fid_stats_t * statp = cm_PerformanceGetNew();

    if (!statp)
        return;

    lock_ObtainRead(&scp->rw);
    if (!(scp->flags & CM_SCACHEFLAG_DELETED)) {
        statp->fid = scp->fid;
        statp->fileLength = scp->length;
        statp->fileType   = scp->fileType;
        statp->flags = CM_FIDSTATS_FLAG_HAVE_SCACHE;
        lock_ConvertRToW(&scp->rw);
        if (cm_HaveCallback(scp))
            statp->flags |= CM_FIDSTATS_FLAG_CALLBACK;
        lock_ConvertWToR(&scp->rw);
        if (scp->flags & CM_SCACHEFLAG_RO)
            statp->flags |= CM_FIDSTATS_FLAG_RO;
        if (scp->flags & CM_SCACHEFLAG_PURERO)
            statp->flags |= CM_FIDSTATS_FLAG_PURERO;
    }
    lock_ReleaseRead(&scp->rw);

#if 0
    if (statp->fid.vnode == 1) {
        cm_volume_t *volp = NULL;
        cm_cell_t *cellp = NULL;
        cm_req_t req;

        cm_InitReq(&req);

        cellp = cm_FindCellByID(statp->fid.cell, 0);
        if (cellp) {
            if (!cm_FindVolumeByID(cellp, statp->fid.volume, cm_rootUserp, &req, 0, &volp)) {
                statp->flags |= CM_FIDSTATS_HAVE_VOLUME;
                cm_PutVolume(volp);
            }
        }
    }
#endif

    cm_PerformanceInsertToHashTable(statp);
}


void cm_PerformanceTuningInit(void)
{
    afs_uint32 i;
    cm_scache_t *scp;
    cm_volume_t *volp;
    cm_buf_t *bp;
    cm_fid_t fid;
    afs_uint32 hash;
    cm_fid_stats_t * statp;

    fidStatsHashTableSize = nearest_prime(cm_data.stats/3);
    if (fidStatsHashTableSize == 0)
        fidStatsHashTableSize = cm_data.stats/3;
    fidStatsHashTablep = (cm_fid_stats_t **)malloc(fidStatsHashTableSize * sizeof(cm_fid_stats_t *));
    if (fidStatsHashTablep == NULL) {
        fidStatsHashTableSize = 0;
        return;
    }

    memset(fidStatsHashTablep, 0, fidStatsHashTableSize * sizeof(cm_fid_stats_t *));

    lock_ObtainRead(&cm_scacheLock);
    for (i=0; i<cm_data.scacheHashTableSize; i++) {
        for (scp=cm_data.scacheHashTablep[i]; scp; scp=scp->nextp) {
            if (scp->fid.cell == 0)
                continue;
            lock_ReleaseRead(&cm_scacheLock);
            cm_PerformanceAddSCache(scp);
            lock_ObtainRead(&cm_scacheLock);
        }
    }
    lock_ReleaseRead(&cm_scacheLock);

    lock_ObtainRead(&cm_volumeLock);
    for(volp = cm_data.allVolumesp; volp; volp=volp->allNextp) {
        if (volp->vol[RWVOL].ID) {
            cm_SetFid(&fid, volp->cellp->cellID, volp->vol[RWVOL].ID, 1, 1);
            hash = fid.hash % fidStatsHashTableSize;

            for (statp = fidStatsHashTablep[hash]; statp; statp = statp->nextp) {
                if (!cm_FidCmp(&fid, &statp->fid)) {
                    statp->flags |= CM_FIDSTATS_FLAG_HAVE_VOLUME;
                    break;
                }
            }
            if (!statp) {
                statp = cm_PerformanceGetNew();
                statp->fid = fid;
                statp->fileType = CM_SCACHETYPE_DIRECTORY;
                statp->flags = CM_FIDSTATS_FLAG_HAVE_VOLUME;
                cm_PerformanceInsertToHashTable(statp);
            }
        }
        if (volp->vol[ROVOL].ID) {
            cm_SetFid(&fid, volp->cellp->cellID, volp->vol[ROVOL].ID, 1, 1);
            hash = fid.hash % fidStatsHashTableSize;

            for (statp = fidStatsHashTablep[hash]; statp; statp = statp->nextp) {
                if (!cm_FidCmp(&fid, &statp->fid)) {
                    statp->flags |= CM_FIDSTATS_FLAG_HAVE_VOLUME;
                    break;
                }
            }
            if (!statp) {
                statp = cm_PerformanceGetNew();
                statp->fid = fid;
                statp->fileType = CM_SCACHETYPE_DIRECTORY;
                statp->flags = CM_FIDSTATS_FLAG_HAVE_VOLUME | CM_FIDSTATS_FLAG_RO | CM_FIDSTATS_FLAG_PURERO;
                cm_PerformanceInsertToHashTable(statp);
            }
        }
        if (volp->vol[BACKVOL].ID) {
            cm_SetFid(&fid, volp->cellp->cellID, volp->vol[BACKVOL].ID, 1, 1);
            hash = fid.hash % fidStatsHashTableSize;

            for (statp = fidStatsHashTablep[hash]; statp; statp = statp->nextp) {
                if (!cm_FidCmp(&fid, &statp->fid)) {
                    statp->flags |= CM_FIDSTATS_FLAG_HAVE_VOLUME;
                    break;
                }
            }
            if (!statp) {
                statp = cm_PerformanceGetNew();
                statp->fid = fid;
                statp->fileType = CM_SCACHETYPE_DIRECTORY;
                statp->flags = CM_FIDSTATS_FLAG_HAVE_VOLUME | CM_FIDSTATS_FLAG_RO;
                cm_PerformanceInsertToHashTable(statp);
            }
        }
    }
    lock_ReleaseRead(&cm_volumeLock);

    lock_ObtainRead(&buf_globalLock);
    for (bp = cm_data.buf_allp; bp; bp=bp->allp) {
        int valid = 0;

        if (bp->fid.cell == 0)
            continue;

        lock_ReleaseRead(&buf_globalLock);
        scp = cm_FindSCache(&bp->fid);
        if (scp) {
            lock_ObtainMutex(&bp->mx);
            lock_ObtainRead(&scp->rw);
            valid = cm_HaveBuffer(scp, bp, TRUE);
            lock_ReleaseRead(&scp->rw);
            lock_ReleaseMutex(&bp->mx);
            cm_ReleaseSCache(scp);

            if (valid) {
                hash = bp->fid.hash % fidStatsHashTableSize;
                for (statp = fidStatsHashTablep[hash]; statp; statp = statp->nextp) {
                    if (!cm_FidCmp(&bp->fid, &statp->fid)) {
                        statp->buffers++;
                        break;
                    }
                }
            }
        }
        lock_ObtainRead(&buf_globalLock);
    }
    lock_ReleaseRead(&buf_globalLock);

    cm_PerformancePrintReport();
}

void cm_PerformanceTuningCheck(void)
{
    afs_uint32 i;
    cm_scache_t *scp;
    cm_volume_t *volp;
    cm_buf_t *bp;
    cm_fid_t fid;
    afs_uint32 hash;
    cm_fid_stats_t * statp;

    if (fidStatsHashTablep == NULL)
        return;

    /* Clean all cm_fid_stat_t objects first */
    for (i = 0; i < fidStatsHashTableSize; i++) {
        for (statp = fidStatsHashTablep[i]; statp; statp = statp->nextp) {
            statp->flags &= (CM_FIDSTATS_FLAG_RO | CM_FIDSTATS_FLAG_PURERO);
            statp->buffers = 0;
            statp->fileLength.QuadPart = 0;
        }
    }

    lock_ObtainRead(&cm_scacheLock);
    for (i=0; i<cm_data.scacheHashTableSize; i++) {
        for (scp=cm_data.scacheHashTablep[i]; scp; scp=scp->nextp) {
            if (scp->fid.cell == 0)
                continue;
            hash = scp->fid.hash % fidStatsHashTableSize;

            for (statp = fidStatsHashTablep[hash]; statp; statp = statp->nextp) {
                if (!cm_FidCmp(&fid, &statp->fid)) {
                    statp->fileType = scp->fileType;
                    if (cm_HaveCallback(scp))
                        statp->flags |= CM_FIDSTATS_FLAG_CALLBACK;
                    statp->flags |= CM_FIDSTATS_FLAG_HAVE_SCACHE;
                    break;
                }
            }
            if (!statp) {
                lock_ReleaseRead(&cm_scacheLock);
                cm_PerformanceAddSCache(scp);
                lock_ObtainRead(&cm_scacheLock);
            }
        }
    }
    lock_ReleaseRead(&cm_scacheLock);

    lock_ObtainRead(&cm_volumeLock);
    for(volp = cm_data.allVolumesp; volp; volp=volp->allNextp) {
        if (volp->vol[RWVOL].ID) {
            cm_SetFid(&fid, volp->cellp->cellID, volp->vol[RWVOL].ID, 1, 1);
            hash = fid.hash % fidStatsHashTableSize;

            for (statp = fidStatsHashTablep[hash]; statp; statp = statp->nextp) {
                if (!cm_FidCmp(&fid, &statp->fid)) {
                    statp->flags |= CM_FIDSTATS_FLAG_HAVE_VOLUME;
                    break;
                }
            }
            if (!statp) {
                statp = cm_PerformanceGetNew();
                statp->fid = fid;
                statp->fileType = CM_SCACHETYPE_DIRECTORY;
                statp->flags = CM_FIDSTATS_FLAG_HAVE_VOLUME;
                cm_PerformanceInsertToHashTable(statp);
            }
        }
        if (volp->vol[ROVOL].ID) {
            cm_SetFid(&fid, volp->cellp->cellID, volp->vol[ROVOL].ID, 1, 1);
            hash = fid.hash % fidStatsHashTableSize;

            for (statp = fidStatsHashTablep[hash]; statp; statp = statp->nextp) {
                if (!cm_FidCmp(&fid, &statp->fid)) {
                    statp->flags |= CM_FIDSTATS_FLAG_HAVE_VOLUME;
                    break;
                }
            }
            if (!statp) {
                statp = cm_PerformanceGetNew();
                statp->fid = fid;
                statp->fileType = CM_SCACHETYPE_DIRECTORY;
                statp->flags = CM_FIDSTATS_FLAG_HAVE_VOLUME | CM_FIDSTATS_FLAG_RO | CM_FIDSTATS_FLAG_PURERO;
                cm_PerformanceInsertToHashTable(statp);
            }
        }
        if (volp->vol[BACKVOL].ID) {
            cm_SetFid(&fid, volp->cellp->cellID, volp->vol[BACKVOL].ID, 1, 1);
            hash = fid.hash % fidStatsHashTableSize;

            for (statp = fidStatsHashTablep[hash]; statp; statp = statp->nextp) {
                if (!cm_FidCmp(&fid, &statp->fid)) {
                    statp->flags |= CM_FIDSTATS_FLAG_HAVE_VOLUME;
                    break;
                }
            }
            if (!statp) {
                statp = cm_PerformanceGetNew();
                statp->fid = fid;
                statp->fileType = CM_SCACHETYPE_DIRECTORY;
                statp->flags = CM_FIDSTATS_FLAG_HAVE_VOLUME | CM_FIDSTATS_FLAG_RO;
                cm_PerformanceInsertToHashTable(statp);
            }
        }
    }
    lock_ReleaseRead(&cm_volumeLock);

    lock_ObtainRead(&buf_globalLock);
    for (bp = cm_data.buf_allp; bp; bp=bp->allp) {
        int valid = 0;

        if (bp->fid.cell == 0)
            continue;

        lock_ReleaseRead(&buf_globalLock);
        scp = cm_FindSCache(&bp->fid);
        if (scp) {
            lock_ObtainMutex(&bp->mx);
            lock_ObtainRead(&scp->rw);
            valid = cm_HaveBuffer(scp, bp, TRUE);
            lock_ReleaseRead(&scp->rw);
            lock_ReleaseMutex(&bp->mx);
            cm_ReleaseSCache(scp);

            if (valid) {
                hash = bp->fid.hash % fidStatsHashTableSize;
                for (statp = fidStatsHashTablep[hash]; statp; statp = statp->nextp) {
                    if (!cm_FidCmp(&bp->fid, &statp->fid)) {
                        statp->buffers++;
                        break;
                    }
                }
            }
        }
        lock_ObtainRead(&buf_globalLock);
    }
    lock_ReleaseRead(&buf_globalLock);

    cm_PerformancePrintReport();
}

void cm_PerformancePrintReport(void)
{
    afs_uint32 i;
    cm_fid_stats_t *statp;

    afs_uint32 rw_vols = 0, ro_vols = 0, bk_vols = 0;
    afs_uint32 fid_cnt = 0, fid_w_vol = 0, fid_w_scache = 0, fid_w_buffers = 0, fid_w_callbacks = 0;
    afs_uint32 fid_w_scache_no_vol = 0, fid_w_scache_no_buf = 0, fid_w_vol_no_scache = 0, fid_w_buf_no_scache = 0;
    afs_uint32 fid_file = 0, fid_dir = 0, fid_mp = 0, fid_sym = 0, fid_other = 0;
    afs_uint32 fid_0k = 0, fid_1k = 0, fid_4k = 0, fid_64k = 0, fid_1m = 0, fid_20m = 0, fid_100m = 0, fid_1g = 0, fid_2g = 0, fid_large = 0;

    HANDLE hLogFile;
    char logfileName[MAX_PATH+1];
    DWORD dwSize;

    if (fidStatsHashTablep == NULL)
        return;

    /* Clean all cm_fid_stat_t objects first */
    for (i = 0; i < fidStatsHashTableSize; i++) {
        for (statp = fidStatsHashTablep[i]; statp; statp = statp->nextp) {
            /* summarize the data */
            fid_cnt++;

            if ((statp->flags & (CM_FIDSTATS_FLAG_RO | CM_FIDSTATS_FLAG_PURERO)) == (CM_FIDSTATS_FLAG_RO | CM_FIDSTATS_FLAG_PURERO))
                ro_vols++;
            else if (statp->flags & CM_FIDSTATS_FLAG_RO)
                bk_vols++;
            else
                rw_vols++;

            if (statp->flags & CM_FIDSTATS_FLAG_HAVE_VOLUME)
                fid_w_vol++;

            if (statp->flags & CM_FIDSTATS_FLAG_HAVE_SCACHE)
                fid_w_scache++;

            if (statp->flags & CM_FIDSTATS_FLAG_CALLBACK)
                fid_w_callbacks++;

            if (statp->buffers > 0)
                fid_w_buffers++;

            if ((statp->flags & CM_FIDSTATS_FLAG_HAVE_SCACHE) &&
                !(statp->flags & CM_FIDSTATS_FLAG_HAVE_VOLUME))
                fid_w_scache_no_vol++;

            if ((statp->flags & CM_FIDSTATS_FLAG_HAVE_SCACHE) &&
                statp->buffers == 0)
                fid_w_scache_no_buf++;

            if ((statp->flags & CM_FIDSTATS_FLAG_HAVE_VOLUME) &&
                !(statp->flags & CM_FIDSTATS_FLAG_HAVE_SCACHE))
                fid_w_vol_no_scache++;

            if (!(statp->flags & CM_FIDSTATS_FLAG_HAVE_SCACHE) &&
                statp->buffers > 0)
                fid_w_buf_no_scache++;

            switch (statp->fileType) {
            case CM_SCACHETYPE_FILE       :
                fid_file++;
                break;
            case CM_SCACHETYPE_DIRECTORY  :
                fid_dir++;
                break;
            case CM_SCACHETYPE_SYMLINK    :
                fid_sym++;
                break;
            case CM_SCACHETYPE_MOUNTPOINT :
                fid_mp++;
                break;
            case CM_SCACHETYPE_DFSLINK    :
            case CM_SCACHETYPE_INVALID    :
            default:
                fid_other++;
            }

            if (statp->fileType == CM_SCACHETYPE_FILE) {
                if (statp->fileLength.HighPart == 0) {
                    if (statp->fileLength.LowPart == 0)
                        fid_0k++;
                    else if (statp->fileLength.LowPart <= 1024)
                        fid_1k++;
                    else if (statp->fileLength.LowPart <= 4096)
                        fid_4k++;
                    else if (statp->fileLength.LowPart <= 65536)
                        fid_64k++;
                    else if (statp->fileLength.LowPart <= 1024*1024)
                        fid_1m++;
                    else if (statp->fileLength.LowPart <= 20*1024*1024)
                        fid_20m++;
                    else if (statp->fileLength.LowPart <= 100*1024*1024)
                        fid_100m++;
                    else if (statp->fileLength.LowPart <= 1024*1024*1024)
                        fid_1g++;
                    else
                        fid_2g++;
                } else {
                    fid_large++;
                }
            }
        }
    }

    dwSize = GetEnvironmentVariable("TEMP", logfileName, sizeof(logfileName));
    if ( dwSize == 0 || dwSize > sizeof(logfileName) )
    {
        GetWindowsDirectory(logfileName, sizeof(logfileName));
    }
    strncat(logfileName, "\\afsd_performance.log", sizeof(logfileName));

    hLogFile = CreateFile(logfileName, FILE_APPEND_DATA, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, NULL);

    if (hLogFile) {
        char output[1024];
        char t[100];
        int zilch;

        GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, t, sizeof(t));

        StringCbPrintfA(output, sizeof(output),
                        "TIME - %s\r\n"
                        "INUSE- stats=(%u of %u) vols=(%u of %u) bufs=(%I64u of %I64u)\r\n"
                        "FIDs - total=%u haveVol=%u haveStat=%u haveCB=%u haveBuf=%u haveStatNoVol=%u haveVolNoStat=%u haveStatNoBuf=%u haveBufNoStat=%u\r\n"
                        "VOLs - rw=%u ro=%u bk=%u\r\n"
                        "TYPEs- file=%u dir=%u mp=%u sym=%u unk=%u\r\n"
                        "SIZEs- 0kb=%u 1kb=%u 4kb=%u 64kb=%u 1mb=%u 20m=%u 100mb=%u 1gb=%u 2gb=%u larger=%u\r\n\r\n",
                         t,
                         cm_data.currentSCaches, cm_data.maxSCaches, cm_data.currentVolumes, cm_data.maxVolumes,
                         cm_data.buf_nbuffers - buf_CountFreeList(), cm_data.buf_nbuffers,
                         fid_cnt, fid_w_vol, fid_w_scache, fid_w_callbacks, fid_w_buffers,
                         fid_w_scache_no_vol, fid_w_vol_no_scache, fid_w_scache_no_buf, fid_w_buf_no_scache,
                         rw_vols, ro_vols, bk_vols,
                         fid_file, fid_dir, fid_mp, fid_sym, fid_other,
                         fid_0k, fid_1k, fid_4k, fid_64k, fid_1m, fid_20m, fid_100m, fid_1g, fid_2g, fid_large
                         );
        WriteFile(hLogFile, output, (DWORD)strlen(output), &zilch, NULL);

        CloseHandle(hLogFile);
    }


}
