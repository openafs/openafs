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
#include <stddef.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <osi.h>

#include "afsd.h"
#include "smb.h"
#include "cm_btree.h"

#include <strsafe.h>

#ifdef DEBUG
extern void afsi_log(char *pattern, ...);
#endif

int cm_enableServerLocks = 1;

int cm_followBackupPath = 0;

/*
 * Case-folding array.  This was constructed by inspecting of SMBtrace output.
 * I do not know anything more about it.
 */
unsigned char cm_foldUpper[256] = {
     0x0,  0x1,  0x2,  0x3,  0x4,  0x5,  0x6,  0x7,
     0x8,  0x9,  0xa,  0xb,  0xc,  0xd,  0xe,  0xf,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x58, 0x59, 0x5a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
    0x80, 0x9a, 0x90, 0x41, 0x8e, 0x41, 0x8f, 0x80,
    0x45, 0x45, 0x45, 0x49, 0x49, 0x49, 0x8e, 0x8f,
    0x90, 0x92, 0x92, 0x4f, 0x99, 0x4f, 0x55, 0x55,
    0x59, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
    0x41, 0x49, 0x4f, 0x55, 0xa5, 0xa5, 0x56, 0xa7,
    0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
    0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
    0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
    0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
    0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

/*
 * Case-insensitive string comparison.  We used to use stricmp, but it doesn't
 * know about 8-bit characters (e.g. 129 is lowercase u-umlaut, 154 is
 * upper-case u-umlaut).
 */
int cm_stricmp(const char *str1, const char *str2)
{
    char c1, c2;

    while (1) {
        if (*str1 == 0)
            if (*str2 == 0)
                return 0;
            else
                return -1;
        if (*str2 == 0)
            return 1;
        c1 = (char) cm_foldUpper[(unsigned char)(*str1++)];
        c2 = (char) cm_foldUpper[(unsigned char)(*str2++)];
        if (c1 < c2)
            return -1;
        if (c1 > c2)
            return 1;
    }
}



/* return success if we can open this file in this mode */
long cm_CheckOpen(cm_scache_t *scp, int openMode, int trunc, cm_user_t *userp,
                  cm_req_t *reqp)
{
    long rights;
    long code;

    rights = 0;
    if (openMode != 1)
	rights |= PRSFS_READ;
    if (openMode == 1 || openMode == 2 || trunc)
	rights |= PRSFS_WRITE;

    lock_ObtainWrite(&scp->rw);

    code = cm_SyncOp(scp, NULL, userp, reqp, rights,
                      CM_SCACHESYNC_GETSTATUS
                     | CM_SCACHESYNC_NEEDCALLBACK
                     | CM_SCACHESYNC_LOCK);

    if (code == 0 &&
        ((rights & PRSFS_WRITE) || (rights & PRSFS_READ)) &&
        scp->fileType == CM_SCACHETYPE_FILE) {

        cm_key_t key;
        unsigned int sLockType;
        LARGE_INTEGER LOffset, LLength;

        /* Check if there's some sort of lock on the file at the
           moment. */

        key = cm_GenerateKey(CM_SESSION_CMINT,0,0);

        if (rights & PRSFS_WRITE)
            sLockType = 0;
        else
            sLockType = LOCKING_ANDX_SHARED_LOCK;

        LOffset.HighPart = CM_FLSHARE_OFFSET_HIGH;
        LOffset.LowPart  = CM_FLSHARE_OFFSET_LOW;
        LLength.HighPart = CM_FLSHARE_LENGTH_HIGH;
        LLength.LowPart  = CM_FLSHARE_LENGTH_LOW;

        code = cm_Lock(scp, sLockType, LOffset, LLength, key, 0, userp, reqp, NULL);

        if (code == 0) {
            cm_Unlock(scp, sLockType, LOffset, LLength, key, 0, userp, reqp);
        } else {
            /* In this case, we allow the file open to go through even
               though we can't enforce mandatory locking on the
               file. */
            if (code == CM_ERROR_NOACCESS &&
                !(rights & PRSFS_WRITE))
                code = 0;
            else {
		if (code == CM_ERROR_LOCK_NOT_GRANTED)
		    code = CM_ERROR_SHARING_VIOLATION;
	    }
        }

    } else if (code != 0) {
        goto _done;
    }

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_LOCK);

 _done:

    lock_ReleaseWrite(&scp->rw);

    return code;
}

/* return success if we can open this file in this mode */
long cm_CheckNTOpen(cm_scache_t *scp, unsigned int desiredAccess,
                    unsigned int createDisp, cm_user_t *userp, cm_req_t *reqp,
		    cm_lock_data_t **ldpp)
{
    long rights;
    long code = 0;

    osi_assertx(ldpp != NULL, "null cm_lock_data_t");
    *ldpp = NULL;

    /* Ignore the SYNCHRONIZE privilege */
    desiredAccess &= ~SYNCHRONIZE;

    /* Always allow delete; the RPC will tell us if it's OK */
    rights = 0;

    if (desiredAccess == DELETE)
        goto done_2;

    if (desiredAccess & (AFS_ACCESS_READ|AFS_ACCESS_EXECUTE))
        rights |= (scp->fileType == CM_SCACHETYPE_DIRECTORY ? PRSFS_LOOKUP : PRSFS_READ);

    /* We used to require PRSFS_WRITE if createDisp was 4
       (OPEN_ALWAYS) even if AFS_ACCESS_WRITE was not requested.
       However, we don't need to do that since the existence of the
       scp implies that we don't need to create it. */
    if (desiredAccess & AFS_ACCESS_WRITE)
        rights |= PRSFS_WRITE;

    if (desiredAccess & DELETE)
        rights |= PRSFS_DELETE;

    lock_ObtainWrite(&scp->rw);

    code = cm_SyncOp(scp, NULL, userp, reqp, rights,
                      CM_SCACHESYNC_GETSTATUS
                     | CM_SCACHESYNC_NEEDCALLBACK
                     | CM_SCACHESYNC_LOCK);

    /*
     * If the open will fail because the volume is readonly, then we will
     * return an access denied error instead.  This is to help brain-dead
     * apps run correctly on replicated volumes.
     * See defect 10007 for more information.
     */
    if (code == CM_ERROR_READONLY)
        code = CM_ERROR_NOACCESS;

    if (code == 0 &&
             ((rights & PRSFS_WRITE) || (rights & PRSFS_READ)) &&
             scp->fileType == CM_SCACHETYPE_FILE) {
        cm_key_t key;
        unsigned int sLockType;
        LARGE_INTEGER LOffset, LLength;

        /* Check if there's some sort of lock on the file at the
           moment. */

        key = cm_GenerateKey(CM_SESSION_CMINT,0,0);
        if (rights & PRSFS_WRITE)
            sLockType = 0;
        else
            sLockType = LOCKING_ANDX_SHARED_LOCK;

        /* single byte lock at offset 0x0100 0000 0000 0000 */
        LOffset.HighPart = CM_FLSHARE_OFFSET_HIGH;
        LOffset.LowPart  = CM_FLSHARE_OFFSET_LOW;
        LLength.HighPart = CM_FLSHARE_LENGTH_HIGH;
        LLength.LowPart  = CM_FLSHARE_LENGTH_LOW;

        code = cm_Lock(scp, sLockType, LOffset, LLength, key, 0, userp, reqp, NULL);

        if (code == 0) {
	    (*ldpp) = (cm_lock_data_t *)malloc(sizeof(cm_lock_data_t));
	    if (!*ldpp) {
		code = ENOMEM;
		goto _done;
	    }

	    (*ldpp)->key = key;
	    (*ldpp)->sLockType = sLockType;
	    (*ldpp)->LOffset.HighPart = LOffset.HighPart;
	    (*ldpp)->LOffset.LowPart = LOffset.LowPart;
	    (*ldpp)->LLength.HighPart = LLength.HighPart;
	    (*ldpp)->LLength.LowPart = LLength.LowPart;
        } else {
            /* In this case, we allow the file open to go through even
               though we can't enforce mandatory locking on the
               file. */
            if (code == CM_ERROR_NOACCESS &&
                !(rights & PRSFS_WRITE))
                code = 0;
            else {
		if (code == CM_ERROR_LOCK_NOT_GRANTED)
		    code = CM_ERROR_SHARING_VIOLATION;
	    }
        }
    } else if (code != 0) {
        goto _done;
    }

 _done:
    lock_ReleaseWrite(&scp->rw);

 done_2:
    osi_Log3(afsd_logp,"cm_CheckNTOpen scp 0x%p ldp 0x%p code 0x%x", scp, *ldpp, code);
    return code;
}

extern long cm_CheckNTOpenDone(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp,
			       cm_lock_data_t ** ldpp)
{
    osi_Log2(afsd_logp,"cm_CheckNTOpenDone scp 0x%p ldp 0x%p", scp, *ldpp);
    lock_ObtainWrite(&scp->rw);
    if (*ldpp) {
	cm_Unlock(scp, (*ldpp)->sLockType, (*ldpp)->LOffset, (*ldpp)->LLength,
		  (*ldpp)->key, 0, userp, reqp);
	free(*ldpp);
	*ldpp = NULL;
    }
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_LOCK);
    lock_ReleaseWrite(&scp->rw);
    return 0;
}
/*
 * When CAP_NT_SMBS has been negotiated, deletion (of files or directories) is
 * done in three steps:
 * (1) open for deletion (NT_CREATE_AND_X)
 * (2) set for deletion on close (NT_TRANSACTION2, SET_FILE_INFO)
 * (3) close (CLOSE)
 * We must not do the RPC until step 3.  But if we are going to return an error
 * code (e.g. directory not empty), we must return it by step 2, otherwise most
 * clients will not notice it.  So we do a preliminary check.  For deleting
 * files, this is almost free, since we have already done the RPC to get the
 * parent directory's status bits.  But for deleting directories, we must do an
 * additional RPC to get the directory's data to check if it is empty.  Sigh.
 */
long cm_CheckNTDelete(cm_scache_t *dscp, cm_scache_t *scp, cm_user_t *userp,
	cm_req_t *reqp)
{
    long code;
    osi_hyper_t thyper;
    cm_buf_t *bufferp;
    cm_dirEntry_t *dep = 0;
    unsigned short *hashTable;
    unsigned int i, idx;
    int BeyondPage = 0, HaveDot = 0, HaveDotDot = 0;
    int releaseLock = 0;

    /* First check permissions */
    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, reqp, PRSFS_DELETE,
                      CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_NEEDCALLBACK);
    if (!code)
        cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&scp->rw);
    if (code)
        return code;

    /* If deleting directory, must be empty */

    if (scp->fileType != CM_SCACHETYPE_DIRECTORY)
        return code;

    thyper.HighPart = 0; thyper.LowPart = 0;
    code = buf_Get(scp, &thyper, reqp, &bufferp);
    if (code)
        return code;

    lock_ObtainMutex(&bufferp->mx);
    lock_ObtainWrite(&scp->rw);
    releaseLock = 1;
    while (1) {
        code = cm_SyncOp(scp, bufferp, userp, reqp, 0,
                          CM_SCACHESYNC_NEEDCALLBACK
                          | CM_SCACHESYNC_READ
                          | CM_SCACHESYNC_BUFLOCKED);
        if (code)
            goto done;

        if (cm_HaveBuffer(scp, bufferp, 1))
            break;

        /* otherwise, load the buffer and try again */
        lock_ReleaseMutex(&bufferp->mx);
        code = cm_GetBuffer(scp, bufferp, NULL, userp, reqp);
        lock_ReleaseWrite(&scp->rw);
        lock_ObtainMutex(&bufferp->mx);
        lock_ObtainWrite(&scp->rw);
	cm_SyncOpDone(scp, bufferp, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_READ | CM_SCACHESYNC_BUFLOCKED);
        if (code)
            goto done;
    }

    lock_ReleaseWrite(&scp->rw);
    releaseLock = 0;

    /* We try to determine emptiness without looking beyond the first page,
     * and without assuming "." and ".." are present and are on the first
     * page (though these assumptions might, after all, be reasonable).
     */
    hashTable = (unsigned short *)(bufferp->datap + (32 * 5));
    for (i=0; i<128; i++) {
        idx = ntohs(hashTable[i]);
        while (idx) {
            if (idx >= 64) {
                BeyondPage = 1;
                break;
            }
            dep = (cm_dirEntry_t *)(bufferp->datap + (32 * idx));
            if (strcmp(dep->name, ".") == 0)
                HaveDot = 1;
            else if (strcmp(dep->name, "..") == 0)
                HaveDotDot = 1;
            else {
                code = CM_ERROR_NOTEMPTY;
                goto done;
            }
            idx = ntohs(dep->next);
        }
    }
    if (BeyondPage && HaveDot && HaveDotDot)
        code = CM_ERROR_NOTEMPTY;
    else
        code = 0;
  done:
    lock_ReleaseMutex(&bufferp->mx);
    buf_Release(bufferp);
    if (releaseLock)
        lock_ReleaseWrite(&scp->rw);
    return code;
}

/*
 * Iterate through all entries in a directory.
 * When the function funcp is called, the buffer is locked but the
 * directory vnode is not.
 *
 * If the retscp parameter is not NULL, the parmp must be a
 * cm_lookupSearch_t object.
 */
long cm_ApplyDir(cm_scache_t *scp, cm_DirFuncp_t funcp, void *parmp,
                 osi_hyper_t *startOffsetp, cm_user_t *userp, cm_req_t *reqp,
                 cm_scache_t **retscp)
{
    char *tp;
    long code;
    cm_dirEntry_t *dep = 0;
    cm_buf_t *bufferp;
    long temp;
    osi_hyper_t dirLength;
    osi_hyper_t bufferOffset;
    osi_hyper_t curOffset;
    osi_hyper_t thyper;
    long entryInDir;
    long entryInBuffer;
    cm_pageHeader_t *pageHeaderp;
    int slotInPage;
    long nextEntryCookie;
    int numDirChunks;	/* # of 32 byte dir chunks in this entry */

    /* get the directory size */
    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, reqp, PRSFS_LOOKUP,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&scp->rw);
    if (code)
        return code;

    if (scp->fileType != CM_SCACHETYPE_DIRECTORY)
        return CM_ERROR_NOTDIR;

    if (retscp) 			/* if this is a lookup call */
    {
        cm_lookupSearch_t*	sp = parmp;

        if (
#ifdef AFS_FREELANCE_CLIENT
	/* Freelance entries never end up in the DNLC because they
	 * do not have an associated cm_server_t
	 */
            !(cm_freelanceEnabled &&
            sp->fid.cell==AFS_FAKE_ROOT_CELL_ID &&
              sp->fid.volume==AFS_FAKE_ROOT_VOL_ID )
#else /* !AFS_FREELANCE_CLIENT */
            TRUE
#endif
            )
        {
            int casefold = sp->caseFold;
            sp->caseFold = 0; /* we have a strong preference for exact matches */
            if ( *retscp = cm_dnlcLookup(scp, sp))	/* dnlc hit */
            {
                sp->caseFold = casefold;
                return 0;
            }
            sp->caseFold = casefold;
        }

        /*
         * see if we can find it using the directory hash tables.
         * we can only do exact matches, since the hash is case
         * sensitive.
         */
        if (funcp != (cm_DirFuncp_t)cm_BPlusDirFoo)
        {
            cm_dirOp_t dirop;
#ifdef USE_BPLUS
            int usedBplus = 0;
#endif

            code = ENOENT;

            code = cm_BeginDirOp(scp, userp, reqp, CM_DIRLOCK_READ,
                                 CM_DIROP_FLAG_NONE, &dirop);
            if (code == 0) {

#ifdef USE_BPLUS
                code = cm_BPlusDirLookup(&dirop, sp->nsearchNamep, &sp->fid);
                if (code != EINVAL)
                    usedBplus = 1;
                else
#endif
                    code = cm_DirLookup(&dirop, sp->searchNamep, &sp->fid);

                cm_EndDirOp(&dirop);
            }

            if (code == 0) {
                /* found it */
                sp->found = TRUE;
                sp->ExactFound = TRUE;
                *retscp = NULL; /* force caller to call cm_GetSCache() */
                return 0;
            }
#ifdef USE_BPLUS
            if (usedBplus) {
                if (sp->caseFold && code == CM_ERROR_INEXACT_MATCH) {
                    /* found it */
                    sp->found = TRUE;
                    sp->ExactFound = FALSE;
                    *retscp = NULL; /* force caller to call cm_GetSCache() */
                    return 0;
                }

                return CM_ERROR_BPLUS_NOMATCH;
            }
#endif
        }
    }

    /*
     * XXX We only get the length once.  It might change when we drop the
     * lock.
     */
    dirLength = scp->length;

    bufferp = NULL;
    bufferOffset.LowPart = bufferOffset.HighPart = 0;
    if (startOffsetp)
        curOffset = *startOffsetp;
    else {
        curOffset.HighPart = 0;
        curOffset.LowPart = 0;
    }

    while (1) {
        /* make sure that curOffset.LowPart doesn't point to the first
         * 32 bytes in the 2nd through last dir page, and that it
         * doesn't point at the first 13 32-byte chunks in the first
         * dir page, since those are dir and page headers, and don't
         * contain useful information.
         */
        temp = curOffset.LowPart & (2048-1);
        if (curOffset.HighPart == 0 && curOffset.LowPart < 2048) {
            /* we're in the first page */
            if (temp < 13*32) temp = 13*32;
        }
        else {
            /* we're in a later dir page */
            if (temp < 32) temp = 32;
        }

        /* make sure the low order 5 bits are zero */
        temp &= ~(32-1);

        /* now put temp bits back ito curOffset.LowPart */
        curOffset.LowPart &= ~(2048-1);
        curOffset.LowPart |= temp;

        /* check if we've passed the dir's EOF */
        if (LargeIntegerGreaterThanOrEqualTo(curOffset, dirLength))
            break;

        /* see if we can use the bufferp we have now; compute in which
         * page the current offset would be, and check whether that's
         * the offset of the buffer we have.  If not, get the buffer.
         */
        thyper.HighPart = curOffset.HighPart;
        thyper.LowPart = curOffset.LowPart & ~(cm_data.buf_blockSize-1);
        if (!bufferp || !LargeIntegerEqualTo(thyper, bufferOffset)) {
            /* wrong buffer */
            if (bufferp) {
                lock_ReleaseMutex(&bufferp->mx);
                buf_Release(bufferp);
                bufferp = NULL;
            }

            code = buf_Get(scp, &thyper, reqp, &bufferp);
            if (code) {
                /* if buf_Get() fails we do not have a buffer object to lock */
                bufferp = NULL;
                break;
            }

            lock_ObtainMutex(&bufferp->mx);
            bufferOffset = thyper;

            /* now get the data in the cache */
            while (1) {
                lock_ObtainWrite(&scp->rw);
                code = cm_SyncOp(scp, bufferp, userp, reqp,
                                  PRSFS_LOOKUP,
                                  CM_SCACHESYNC_NEEDCALLBACK
                                  | CM_SCACHESYNC_READ
                                  | CM_SCACHESYNC_BUFLOCKED);
                if (code) {
                    lock_ReleaseWrite(&scp->rw);
                    break;
                }
		cm_SyncOpDone(scp, bufferp, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_READ | CM_SCACHESYNC_BUFLOCKED);

                if (cm_HaveBuffer(scp, bufferp, 1)) {
                    lock_ReleaseWrite(&scp->rw);
                    break;
                }

                /* otherwise, load the buffer and try again */
                lock_ReleaseMutex(&bufferp->mx);
                code = cm_GetBuffer(scp, bufferp, NULL, userp,
                                    reqp);
                lock_ReleaseWrite(&scp->rw);
                lock_ObtainMutex(&bufferp->mx);
                if (code)
                    break;
            }
            if (code) {
                lock_ReleaseMutex(&bufferp->mx);
                buf_Release(bufferp);
                bufferp = NULL;
                break;
            }
        }	/* if (wrong buffer) ... */

        /* now we have the buffer containing the entry we're interested
         * in; copy it out if it represents a non-deleted entry.
         */
        entryInDir = curOffset.LowPart & (2048-1);
        entryInBuffer = curOffset.LowPart & (cm_data.buf_blockSize - 1);

        /* page header will help tell us which entries are free.  Page
         * header can change more often than once per buffer, since
         * AFS 3 dir page size may be less than (but not more than) a
         * buffer package buffer.
         */
        /* only look intra-buffer */
        temp = curOffset.LowPart & (cm_data.buf_blockSize - 1);
        temp &= ~(2048 - 1);	/* turn off intra-page bits */
        pageHeaderp = (cm_pageHeader_t *) (bufferp->datap + temp);

        /* now determine which entry we're looking at in the page.  If
         * it is free (there's a free bitmap at the start of the dir),
         * we should skip these 32 bytes.
         */
        slotInPage = (entryInDir & 0x7e0) >> 5;
        if (!(pageHeaderp->freeBitmap[slotInPage>>3]
               & (1 << (slotInPage & 0x7)))) {
            /* this entry is free */
            numDirChunks = 1;	/* only skip this guy */
            goto nextEntry;
        }

        tp = bufferp->datap + entryInBuffer;
        dep = (cm_dirEntry_t *) tp;	/* now points to AFS3 dir entry */

        /*
         * here are some consistency checks
         */
        if (dep->flag != CM_DIR_FFIRST ||
            strlen(dep->name) > 256) {
            code = CM_ERROR_INVAL;
            osi_Log2(afsd_logp,
                     "cm_ApplyDir invalid directory entry for scp %p bufp %p",
                     scp, bufferp);
            osi_Log4(afsd_logp,"... cell %u vol %u vnode %u uniq %u",
                     scp->fid.cell, scp->fid.volume, scp->fid.vnode, scp->fid.unique);
            bufferp->dataVersion = CM_BUF_VERSION_BAD;
            break;
        }

        /* while we're here, compute the next entry's location, too,
         * since we'll need it when writing out the cookie into the
         * dir listing stream.
         */
        numDirChunks = cm_NameEntries(dep->name, NULL);

        /* compute the offset of the cookie representing the next entry */
        nextEntryCookie = curOffset.LowPart
            + (CM_DIR_CHUNKSIZE * numDirChunks);

        if (dep->fid.vnode != 0) {
            /* this is one of the entries to use: it is not deleted */
            code = (*funcp)(scp, dep, parmp, &curOffset);
            if (code)
                break;
        }	/* if we're including this name */

      nextEntry:
        /* and adjust curOffset to be where the new cookie is */
        thyper.HighPart = 0;
        thyper.LowPart = CM_DIR_CHUNKSIZE * numDirChunks;
        curOffset = LargeIntegerAdd(thyper, curOffset);
    }		/* while copying data for dir listing */

    /* release the mutex */
    if (bufferp) {
        lock_ReleaseMutex(&bufferp->mx);
        buf_Release(bufferp);
    }
    return code;
}

int cm_NoneUpper(normchar_t *s)
{
    normchar_t c;
    while (c = *s++)
        if (c >= 'A' && c <= 'Z')
            return 0;
    return 1;
}

int cm_NoneLower(normchar_t *s)
{
    normchar_t c;
    while (c = *s++)
        if (c >= 'a' && c <= 'z')
            return 0;
    return 1;
}

long cm_LookupSearchProc(cm_scache_t *scp, cm_dirEntry_t *dep, void *rockp,
                         osi_hyper_t *offp)
{
    cm_lookupSearch_t *sp;
    int match;
    normchar_t matchName[MAX_PATH];
    int looking_for_short_name = FALSE;

    sp = (cm_lookupSearch_t *) rockp;

    if (cm_FsStringToNormString(dep->name, -1, matchName, lengthof(matchName)) == 0) {
        /* Can't normalize FS string. */
        return 0;
    }

    if (sp->caseFold)
        match = cm_NormStrCmpI(matchName, sp->nsearchNamep);
    else
        match = cm_NormStrCmp(matchName, sp->nsearchNamep);

    if (match != 0
        && sp->hasTilde
        && !cm_Is8Dot3(matchName)) {

        cm_Gen8Dot3NameInt(dep->name, &dep->fid, matchName, NULL);
        if (sp->caseFold)
            match = cm_NormStrCmpI(matchName, sp->nsearchNamep);
        else
            match = cm_NormStrCmp(matchName, sp->nsearchNamep);
        looking_for_short_name = TRUE;
    }

    if (match != 0)
        return 0;

    sp->found = 1;
    if (!sp->caseFold)
        sp->ExactFound = 1;

    if (!sp->caseFold || looking_for_short_name) {
        cm_SetFid(&sp->fid, sp->fid.cell, sp->fid.volume, ntohl(dep->fid.vnode), ntohl(dep->fid.unique));
        return CM_ERROR_STOPNOW;
    }

    /*
     * If we get here, we are doing a case-insensitive search, and we
     * have found a match.  Now we determine what kind of match it is:
     * exact, lower-case, upper-case, or none of the above.  This is done
     * in order to choose among matches, if there are more than one.
     */

    /* Exact matches are the best. */
    match = cm_NormStrCmp(matchName, sp->nsearchNamep);
    if (match == 0) {
        sp->ExactFound = 1;
        cm_SetFid(&sp->fid, sp->fid.cell, sp->fid.volume, ntohl(dep->fid.vnode), ntohl(dep->fid.unique));
        return CM_ERROR_STOPNOW;
    }

    /* Lower-case matches are next. */
    if (sp->LCfound)
        return 0;
    if (cm_NoneUpper(matchName)) {
        sp->LCfound = 1;
        goto inexact;
    }

    /* Upper-case matches are next. */
    if (sp->UCfound)
        return 0;
    if (cm_NoneLower(matchName)) {
        sp->UCfound = 1;
        goto inexact;
    }

    /* General matches are last. */
    if (sp->NCfound)
        return 0;
    sp->NCfound = 1;

  inexact:
    cm_SetFid(&sp->fid, sp->fid.cell, sp->fid.volume, ntohl(dep->fid.vnode), ntohl(dep->fid.unique));
    return 0;
}

/* read the contents of a mount point into the appropriate string.
 * called with write locked scp, and returns with locked scp.
 */
long cm_ReadMountPoint(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp)
{
    long code;

    if (scp->mountPointStringp[0])
        return 0;

#ifdef AFS_FREELANCE_CLIENT
    /* File servers do not have data for freelance entries */
    if (cm_freelanceEnabled &&
        scp->fid.cell==AFS_FAKE_ROOT_CELL_ID &&
        scp->fid.volume==AFS_FAKE_ROOT_VOL_ID )
    {
        code = cm_FreelanceFetchMountPointString(scp);
    } else
#endif /* AFS_FREELANCE_CLIENT */
    {
        char temp[MOUNTPOINTLEN];
        osi_hyper_t offset;

        /* otherwise, we have to read it in */
        offset.LowPart = offset.HighPart = 0;
        code = cm_GetData(scp, &offset, temp, MOUNTPOINTLEN, userp, reqp);
        if (code)
            return code;

        /*
         * scp->length is the actual length of the mount point string.
         * It is current because cm_GetData merged the most up to date
         * status info into scp and has not dropped the rwlock since.
         */
        if (scp->length.LowPart > MOUNTPOINTLEN - 1)
            return CM_ERROR_TOOBIG;
        if (scp->length.LowPart == 0)
            return CM_ERROR_INVAL;

        /* convert the terminating dot to a NUL */
        temp[scp->length.LowPart - 1] = 0;
        memcpy(scp->mountPointStringp, temp, scp->length.LowPart);
    }

    return code;
}


/* called with a locked scp and chases the mount point, yielding outScpp.
 * scp remains write locked, just for simplicity of describing the interface.
 */
long cm_FollowMountPoint(cm_scache_t *scp, cm_scache_t *dscp, cm_user_t *userp,
                         cm_req_t *reqp, cm_scache_t **outScpp)
{
    fschar_t *cellNamep = NULL;
    fschar_t *volNamep = NULL;
    afs_uint32 code;
    fschar_t *cp;
    fschar_t *mpNamep;
    cm_volume_t *volp = NULL;
    cm_cell_t *cellp;
    fschar_t mtType;
    cm_fid_t tfid;
    size_t vnLength;
    int targetType;

    *outScpp = NULL;

    if (scp->mountRootFid.cell != 0 && scp->mountRootGen >= cm_data.mountRootGen) {
        tfid = scp->mountRootFid;
        lock_ReleaseWrite(&scp->rw);
        code = cm_GetSCache(&tfid, outScpp, userp, reqp);
        lock_ObtainWrite(&scp->rw);
        return code;
    }

    /* parse the volume name */
    mpNamep = scp->mountPointStringp;
    if (!mpNamep[0])
	return CM_ERROR_NOSUCHPATH;
    mtType = *scp->mountPointStringp;

    cp = cm_FsStrChr(mpNamep, _FS(':'));
    if (cp) {
        /* cellular mount point */
        cellNamep = (fschar_t *)malloc((cp - mpNamep) * sizeof(fschar_t));
        cm_FsStrCpyN(cellNamep, cp - mpNamep, mpNamep + 1, cp - mpNamep - 1);
        volNamep = cm_FsStrDup(cp+1);

        /* now look up the cell */
        lock_ReleaseWrite(&scp->rw);
        cellp = cm_GetCell(cellNamep, CM_FLAG_CREATE);
        lock_ObtainWrite(&scp->rw);
    } else {
        /* normal mt pt */
        volNamep = cm_FsStrDup(mpNamep + 1);

#ifdef AFS_FREELANCE_CLIENT
        /*
         * Mount points in the Freelance cell should default
         * to the workstation cell.
         */
        if (cm_freelanceEnabled &&
             scp->fid.cell==AFS_FAKE_ROOT_CELL_ID &&
             scp->fid.volume==AFS_FAKE_ROOT_VOL_ID )
        {
            fschar_t rootCellName[256]="";
            cm_GetRootCellName(rootCellName);
            cellp = cm_GetCell(rootCellName, 0);
        } else
#endif /* AFS_FREELANCE_CLIENT */
            cellp = cm_FindCellByID(scp->fid.cell, 0);
    }

    if (!cellp) {
        code = CM_ERROR_NOSUCHCELL;
        goto done;
    }

    vnLength = cm_FsStrLen(volNamep);
    if (vnLength >= 8 && cm_FsStrCmp(volNamep + vnLength - 7, ".backup") == 0)
        targetType = BACKVOL;
    else if (vnLength >= 10
             && cm_FsStrCmp(volNamep + vnLength - 9, ".readonly") == 0)
        targetType = ROVOL;
    else
        targetType = RWVOL;

    /* check for backups within backups */
    if (targetType == BACKVOL
         && (scp->flags & (CM_SCACHEFLAG_RO | CM_SCACHEFLAG_PURERO))
         == CM_SCACHEFLAG_RO) {
        code = CM_ERROR_NOSUCHVOLUME;
        goto done;
    }

    /* now we need to get the volume */
    lock_ReleaseWrite(&scp->rw);
    if (cm_VolNameIsID(volNamep)) {
        code = cm_FindVolumeByID(cellp, atoi(volNamep), userp, reqp,
                                CM_GETVOL_FLAG_CREATE, &volp);
    } else {
        code = cm_FindVolumeByName(cellp, volNamep, userp, reqp,
                                  CM_GETVOL_FLAG_CREATE, &volp);
    }
    lock_ObtainWrite(&scp->rw);

    if (code == 0) {
        afs_uint32 cell, volume;
        cm_vol_state_t *statep;

        cell = cellp->cellID;

        /* if the mt pt originates in a .backup volume (not a .readonly)
         * and FollowBackupPath is active, and if there is a .backup
         * volume for the target, then use the .backup of the target
         * instead of the read-write.
         */
        if (cm_followBackupPath &&
            volp->vol[BACKVOL].ID != 0 &&
            (dscp->flags & (CM_SCACHEFLAG_RO|CM_SCACHEFLAG_PURERO)) == CM_SCACHEFLAG_RO &&
            (targetType == RWVOL || targetType == ROVOL && volp->vol[ROVOL].ID == 0)
            ) {
            targetType = BACKVOL;
        }
        /* if the mt pt is in a read-only volume (not just a
         * backup), and if there is a read-only volume for the
         * target, and if this is a targetType '#' mount point, use
         * the read-only, otherwise use the one specified.
         */
        else if (mtType == '#' && targetType == RWVOL &&
                 (scp->flags & CM_SCACHEFLAG_PURERO) &&
                 volp->vol[ROVOL].ID != 0) {
            targetType = ROVOL;
        }

        lock_ObtainWrite(&volp->rw);
        statep = cm_VolumeStateByType(volp, targetType);
        volume = statep->ID;
        statep->dotdotFid = dscp->fid;
        lock_ReleaseWrite(&volp->rw);

        /* the rest of the fid is a magic number */
        cm_SetFid(&scp->mountRootFid, cell, volume, 1, 1);
        scp->mountRootGen = cm_data.mountRootGen;

        tfid = scp->mountRootFid;
        lock_ReleaseWrite(&scp->rw);
        code = cm_GetSCache(&tfid, outScpp, userp, reqp);
        lock_ObtainWrite(&scp->rw);
    }

  done:
    if (volp)
	cm_PutVolume(volp);
    if (cellNamep)
        free(cellNamep);
    if (volNamep)
        free(volNamep);
    return code;
}

long cm_LookupInternal(cm_scache_t *dscp, clientchar_t *cnamep, long flags, cm_user_t *userp,
                       cm_req_t *reqp, cm_scache_t **outScpp)
{
    long code;
    int dnlcHit = 1;	/* did we hit in the dnlc? yes, we did */
    cm_scache_t *tscp = NULL;
    cm_scache_t *mountedScp;
    cm_lookupSearch_t rock;
    int getroot;
    normchar_t *nnamep = NULL;
    fschar_t *fnamep = NULL;
    size_t fnlen;

    *outScpp = NULL;

    memset(&rock, 0, sizeof(rock));

    if (dscp->fid.vnode == 1 && dscp->fid.unique == 1
        && cm_ClientStrCmp(cnamep, _C("..")) == 0) {
        if (dscp->dotdotFid.volume == 0)
            return CM_ERROR_NOSUCHVOLUME;
        rock.fid = dscp->dotdotFid;
        goto haveFid;
    } else if (cm_ClientStrCmp(cnamep, _C(".")) == 0) {
	rock.fid = dscp->fid;
	goto haveFid;
    }

    nnamep = cm_ClientStringToNormStringAlloc(cnamep, -1, NULL);
    if (!nnamep) {
        code = CM_ERROR_NOSUCHFILE;
        goto done;
    }
    fnamep = cm_ClientStringToFsStringAlloc(cnamep, -1, NULL);
    if (!fnamep) {
        code = CM_ERROR_NOSUCHFILE;
        goto done;
    }

retry_lookup:
    if (flags & CM_FLAG_NOMOUNTCHASE) {
        /* In this case, we should go and call cm_Dir* functions
           directly since the following cm_ApplyDir() function will
           not. */

        cm_dirOp_t dirop;
#ifdef USE_BPLUS
        int usedBplus = 0;
#endif

        code = cm_BeginDirOp(dscp, userp, reqp, CM_DIRLOCK_READ,
                             CM_DIROP_FLAG_NONE, &dirop);
        if (code == 0) {
#ifdef USE_BPLUS
            code = cm_BPlusDirLookup(&dirop, nnamep, &rock.fid);
            if (code != EINVAL)
                usedBplus = 1;
            else
#endif
                code = cm_DirLookup(&dirop, fnamep, &rock.fid);

            cm_EndDirOp(&dirop);
        }

        if (code == 0) {
            /* found it */
            rock.found = TRUE;
            goto haveFid;
        }
#ifdef USE_BPLUS
        if (usedBplus) {
            if (code == CM_ERROR_INEXACT_MATCH && (flags & CM_FLAG_CASEFOLD)) {
                /* found it */
                code = 0;
                rock.found = TRUE;
                goto haveFid;
            }

            code = CM_ERROR_BPLUS_NOMATCH;
            goto notfound;
        }
#endif
    }

    rock.fid.cell = dscp->fid.cell;
    rock.fid.volume = dscp->fid.volume;
    rock.searchNamep = fnamep;
    rock.nsearchNamep = nnamep;
    rock.caseFold = (flags & CM_FLAG_CASEFOLD);
    rock.hasTilde = ((cm_ClientStrChr(cnamep, '~') != NULL) ? 1 : 0);

    /* If NOMOUNTCHASE, bypass DNLC by passing NULL scp pointer */
    code = cm_ApplyDir(dscp, cm_LookupSearchProc, &rock, NULL, userp, reqp,
                       (flags & CM_FLAG_NOMOUNTCHASE) ? NULL : &tscp);

    /* code == 0 means we fell off the end of the dir, while stopnow means
     * that we stopped early, probably because we found the entry we're
     * looking for.  Any other non-zero code is an error.
     */
    if (code && code != CM_ERROR_STOPNOW) {
        /* if the cm_scache_t we are searching in is not a directory
         * we must return path not found because the error
         * is to describe the final component not an intermediary
         */
        if (code == CM_ERROR_NOTDIR) {
            if (flags & CM_FLAG_CHECKPATH)
                code = CM_ERROR_NOSUCHPATH;
            else
                code = CM_ERROR_NOSUCHFILE;
        }
        goto done;
    }

notfound:
    getroot = (dscp==cm_data.rootSCachep) ;
    if (!rock.found) {
        if (!cm_freelanceEnabled || !getroot) {
            if (flags & CM_FLAG_CHECKPATH)
                code = CM_ERROR_NOSUCHPATH;
            else
                code = CM_ERROR_NOSUCHFILE;
            goto done;
        }
        else if (!cm_ClientStrChr(cnamep, '#') &&
                 !cm_ClientStrChr(cnamep, '%') &&
                 cm_ClientStrCmpI(cnamep, _C("srvsvc")) &&
                 cm_ClientStrCmpI(cnamep, _C("wkssvc")) &&
                 cm_ClientStrCmpI(cnamep, _C("ipc$")))
        {
            /* nonexistent dir on freelance root, so add it */
            fschar_t fullname[CELL_MAXNAMELEN + 1] = ".";  /* +1 so that when we skip the . the size is still CELL_MAXNAMELEN */
            int  found = 0;
            int  retry = 0;

            osi_Log1(afsd_logp,"cm_Lookup adding mount for non-existent directory: %S",
                     osi_LogSaveClientString(afsd_logp,cnamep));

            /*
             * There is an ugly behavior where a share name "foo" will be searched
             * for as "fo".  If the searched for name differs by an already existing
             * symlink or mount point in the Freelance directory, do not add the
             * new value automatically.
             */

            code = -1;
            fnlen = strlen(fnamep);
            if ( fnamep[fnlen-1] == '.') {
                fnamep[fnlen-1] = '\0';
                fnlen--;
                retry = 1;
            }

            if (cnamep[0] == '.') {
                if (cm_GetCell_Gen(&fnamep[1], &fullname[1], CM_FLAG_CREATE)) {
                    found = 1;
                    code = cm_FreelanceAddMount(fullname, &fullname[1], "root.cell", 1, &rock.fid);
                    if ( cm_FsStrCmpI(&fnamep[1], &fullname[1])) {
                        /*
                         * Do not permit symlinks that are one of:
                         *  . the cellname followed by a dot
                         *  . the cellname minus a single character
                         *  . a substring of the cellname that does not consist of full components
                         */
                        if ( cm_strnicmp_utf8(&fnamep[1], fullname, (int)fnlen-1) == 0 &&
                             (fnlen-1 == strlen(fullname)-1 || fullname[fnlen-1] != '.'))
                        {
                            /* do not add; substitute fullname for the search */
                            free(fnamep);
                            fnamep = malloc(strlen(fullname)+2);
                            fnamep[0] = '.';
                            strncpy(&fnamep[1], fullname, strlen(fullname)+1);
                            retry = 1;
                        } else {
                            code = cm_FreelanceAddSymlink(fnamep, fullname, &rock.fid);
                        }
                    }
                }
            } else {
                if (cm_GetCell_Gen(fnamep, fullname, CM_FLAG_CREATE)) {
                    found = 1;
                    code = cm_FreelanceAddMount(fullname, fullname, "root.cell", 0, &rock.fid);
                    if ( cm_FsStrCmpI(fnamep, fullname)) {
                        /*
                         * Do not permit symlinks that are one of:
                         *  . the cellname followed by a dot
                         *  . the cellname minus a single character
                         *  . a substring of the cellname that does not consist of full components
                         */
                        if ( cm_strnicmp_utf8(fnamep, fullname, (int)fnlen-1) == 0 &&
                             (fnlen == strlen(fullname)-1 || fullname[fnlen] != '.'))
                        {
                            /* do not add; substitute fullname for the search */
                                free(fnamep);
                                fnamep = strdup(fullname);
                                code = 0;
                                retry = 1;
                        } else {
                            code = cm_FreelanceAddSymlink(fnamep, fullname, &rock.fid);
                        }
                    }
                }
            }

            if (retry) {
                if (nnamep)
                    free(nnamep);
                nnamep = cm_FsStringToNormStringAlloc(fnamep, -1, NULL);
                goto retry_lookup;
            }

            if (!found || code) {   /* add mount point failed, so give up */
                if (flags & CM_FLAG_CHECKPATH)
                    code = CM_ERROR_NOSUCHPATH;
                else
                    code = CM_ERROR_NOSUCHFILE;
                goto done;
            }
            tscp = NULL;   /* to force call of cm_GetSCache */
        } else {
            if (flags & CM_FLAG_CHECKPATH)
                code = CM_ERROR_NOSUCHPATH;
            else
                code = CM_ERROR_NOSUCHFILE;
            goto done;
        }
    }

  haveFid:
    if ( !tscp )    /* we did not find it in the dnlc */
    {
        dnlcHit = 0;
        code = cm_GetSCache(&rock.fid, &tscp, userp, reqp);
        if (code)
            goto done;
    }
    /* tscp is now held */

    lock_ObtainWrite(&tscp->rw);

    /*
     * Do not get status if we do not already have a callback or know the type.
     * The process of reading the mount point string will obtain status information
     * in a single RPC.  No reason to add a second round trip.
     *
     * If we do have a callback, use cm_SyncOp to get status in case the
     * current cm_user_t is not the same as the one that obtained the
     * mount point string contents.
     */
    if (cm_HaveCallback(tscp) || tscp->fileType == CM_SCACHETYPE_UNKNOWN) {
        code = cm_SyncOp(tscp, NULL, userp, reqp, 0,
                          CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_NEEDCALLBACK);
        if (code) {
            lock_ReleaseWrite(&tscp->rw);
            cm_ReleaseSCache(tscp);
            goto done;
        }
        cm_SyncOpDone(tscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    }
    /* tscp is now locked */

    if (!(flags & CM_FLAG_NOMOUNTCHASE)
         && tscp->fileType == CM_SCACHETYPE_MOUNTPOINT) {
        /* mount points are funny: they have a volume name to mount
         * the root of.
         */
        code = cm_ReadMountPoint(tscp, userp, reqp);
        if (code == 0)
            code = cm_FollowMountPoint(tscp, dscp, userp, reqp,
                                       &mountedScp);
        lock_ReleaseWrite(&tscp->rw);
        cm_ReleaseSCache(tscp);
        if (code)
            goto done;

        tscp = mountedScp;
    }
    else {
        lock_ReleaseWrite(&tscp->rw);
    }

    /* copy back pointer */
    *outScpp = tscp;

    /* insert scache in dnlc */
    if ( !dnlcHit && !(flags & CM_FLAG_NOMOUNTCHASE) && rock.ExactFound ) {
        /* lock the directory entry to prevent racing callback revokes */
        lock_ObtainRead(&dscp->rw);
        if ( dscp->cbServerp != NULL && dscp->cbExpires > 0 ) {
            /* TODO: reuse nnamep from above */
            if (nnamep)
                free(nnamep);
            nnamep = cm_ClientStringToNormStringAlloc(cnamep, -1, NULL);
            if (nnamep)
                cm_dnlcEnter(dscp, nnamep, tscp);
        }
        lock_ReleaseRead(&dscp->rw);
    }

    /* and return */
  done:
    if (fnamep) {
        free (fnamep);
        fnamep = NULL;
    }
    if (nnamep) {
        free (nnamep);
        nnamep = NULL;
    }

    return code;
}

int cm_ExpandSysName(clientchar_t *inp, clientchar_t *outp, long outSizeCch, unsigned int index)
{
    clientchar_t *tp;
    int prefixCount;

    tp = cm_ClientStrRChr(inp, '@');
    if (tp == NULL)
        return 0;		/* no @sys */

    if (cm_ClientStrCmp(tp, _C("@sys")) != 0)
        return 0;	/* no @sys */

    /* caller just wants to know if this is a valid @sys type of name */
    if (outp == NULL)
        return 1;

    if (index >= cm_sysNameCount)
        return -1;

    /* otherwise generate the properly expanded @sys name */
    prefixCount = (int)(tp - inp);

    cm_ClientStrCpyN(outp, outSizeCch, inp, prefixCount);	/* copy out "a." from "a.@sys" */
    outp[prefixCount] = 0;		/* null terminate the "a." */
    cm_ClientStrCat(outp, outSizeCch, cm_sysNameList[index]);/* append i386_nt40 */
    return 1;
}

long cm_EvaluateVolumeReference(clientchar_t * namep, long flags, cm_user_t * userp,
                                cm_req_t *reqp, cm_scache_t ** outScpp)
{
    afs_uint32    code = 0;
    fschar_t      cellName[CELL_MAXNAMELEN];
    fschar_t      volumeName[VL_MAXNAMELEN];
    size_t        len;
    fschar_t *        cp;
    fschar_t *        tp;
    fschar_t *        fnamep = NULL;

    cm_cell_t *   cellp = NULL;
    cm_volume_t * volp = NULL;
    cm_fid_t      fid;
    afs_uint32    volume;
    int           volType;
    int           mountType = RWVOL;

    osi_Log1(afsd_logp, "cm_EvaluateVolumeReference for string [%S]",
             osi_LogSaveClientString(afsd_logp, namep));

    if (cm_ClientStrCmpNI(namep, _C(CM_PREFIX_VOL), CM_PREFIX_VOL_CCH) != 0) {
        goto _exit_invalid_path;
    }

    /* namep is assumed to look like the following:

       @vol:<cellname>%<volume>\0
       or
       @vol:<cellname>#<volume>\0

     */

    fnamep = cm_ClientStringToFsStringAlloc(namep, -1, NULL);
    cp = fnamep + CM_PREFIX_VOL_CCH; /* cp points to cell name, hopefully */
    tp = cm_FsStrChr(cp, '%');
    if (tp == NULL)
        tp = cm_FsStrChr(cp, '#');
    if (tp == NULL ||
        (len = tp - cp) == 0 ||
        len > CELL_MAXNAMELEN)
        goto _exit_invalid_path;
    cm_FsStrCpyN(cellName, lengthof(cellName), cp, len);

    if (*tp == '#')
        mountType = ROVOL;

    cp = tp+1;                  /* cp now points to volume, supposedly */
    cm_FsStrCpy(volumeName, lengthof(volumeName), cp);

    /* OK, now we have the cell and the volume */
    osi_Log2(afsd_logp, "   Found cell [%s] and volume [%s]",
             osi_LogSaveFsString(afsd_logp, cellName),
             osi_LogSaveFsString(afsd_logp, volumeName));

    cellp = cm_GetCell(cellName, CM_FLAG_CREATE);
    if (cellp == NULL) {
        goto _exit_invalid_path;
    }

    len = cm_FsStrLen(volumeName);
    if (len >= 8 && cm_FsStrCmp(volumeName + len - 7, ".backup") == 0)
        volType = BACKVOL;
    else if (len >= 10 &&
             cm_FsStrCmp(volumeName + len - 9, ".readonly") == 0)
        volType = ROVOL;
    else
        volType = RWVOL;

    if (cm_VolNameIsID(volumeName)) {
        code = cm_FindVolumeByID(cellp, atoi(volumeName), userp, reqp,
                                CM_GETVOL_FLAG_CREATE, &volp);
    } else {
        code = cm_FindVolumeByName(cellp, volumeName, userp, reqp,
                                  CM_GETVOL_FLAG_CREATE, &volp);
    }

    if (code != 0)
        goto _exit_cleanup;

    if (volType == BACKVOL)
        volume = volp->vol[BACKVOL].ID;
    else if (volType == ROVOL ||
             (volType == RWVOL && mountType == ROVOL && volp->vol[ROVOL].ID != 0))
        volume = volp->vol[ROVOL].ID;
    else
        volume = volp->vol[RWVOL].ID;

    cm_SetFid(&fid, cellp->cellID, volume, 1, 1);

    code = cm_GetSCache(&fid, outScpp, userp, reqp);

  _exit_cleanup:
    if (fnamep)
        free(fnamep);

    if (volp)
        cm_PutVolume(volp);

    if (code == 0)
        return code;

 _exit_invalid_path:
    if (flags & CM_FLAG_CHECKPATH)
        return CM_ERROR_NOSUCHPATH;
    else
        return CM_ERROR_NOSUCHFILE;
}

#ifdef DEBUG_REFCOUNT
long cm_LookupDbg(cm_scache_t *dscp, clientchar_t *namep, long flags, cm_user_t *userp,
               cm_req_t *reqp, cm_scache_t **outScpp, char * file, long line)
#else
long cm_Lookup(cm_scache_t *dscp, clientchar_t *namep, long flags, cm_user_t *userp,
               cm_req_t *reqp, cm_scache_t **outScpp)
#endif
{
    long code;
    clientchar_t tname[AFSPATHMAX];
    int sysNameIndex = 0;
    cm_scache_t *scp = NULL;

#ifdef DEBUG_REFCOUNT
    afsi_log("%s:%d cm_Lookup dscp 0x%p ref %d", file, line, dscp, dscp->refCount, file, line);
    osi_Log2(afsd_logp, "cm_Lookup dscp 0x%p ref %d", dscp, dscp->refCount);
#endif

    if ( cm_ClientStrCmpI(namep,_C(SMB_IOCTL_FILENAME_NOSLASH)) == 0 ) {
        if (flags & CM_FLAG_CHECKPATH)
            return CM_ERROR_NOSUCHPATH;
        else
            return CM_ERROR_NOSUCHFILE;
    }

    if (dscp == cm_data.rootSCachep &&
        cm_ClientStrCmpNI(namep, _C(CM_PREFIX_VOL), CM_PREFIX_VOL_CCH) == 0) {
        return cm_EvaluateVolumeReference(namep, flags, userp, reqp, outScpp);
    }

    if (cm_ExpandSysName(namep, NULL, 0, 0) > 0) {
        for ( sysNameIndex = 0; sysNameIndex < MAXNUMSYSNAMES; sysNameIndex++) {
            code = cm_ExpandSysName(namep, tname, lengthof(tname), sysNameIndex);
            if (code > 0) {
                code = cm_LookupInternal(dscp, tname, flags, userp, reqp, &scp);
#ifdef DEBUG_REFCOUNT
                afsi_log("%s:%d cm_LookupInternal (1) code 0x%x dscp 0x%p ref %d scp 0x%p ref %d", file, line, code, dscp, dscp->refCount, scp, scp ? scp->refCount : 0);
                osi_Log3(afsd_logp, "cm_LookupInternal (1) code 0x%x dscp 0x%p scp 0x%p", code, dscp, scp);
#endif

                if (code == 0) {
                    *outScpp = scp;
                    return 0;
                }
                if (scp) {
                    cm_ReleaseSCache(scp);
                    scp = NULL;
                }
            } else {
                code = cm_LookupInternal(dscp, namep, flags, userp, reqp, &scp);
#ifdef DEBUG_REFCOUNT
                afsi_log("%s:%d cm_LookupInternal (2) code 0x%x dscp 0x%p ref %d scp 0x%p ref %d", file, line, code, dscp, dscp->refCount, scp, scp ? scp->refCount : 0);
                osi_Log3(afsd_logp, "cm_LookupInternal (2) code 0x%x dscp 0x%p scp 0x%p", code, dscp, scp);
#endif
                *outScpp = scp;
                return code;
            }
        }
    } else {
        code = cm_LookupInternal(dscp, namep, flags, userp, reqp, &scp);
#ifdef DEBUG_REFCOUNT
        afsi_log("%s:%d cm_LookupInternal (2) code 0x%x dscp 0x%p ref %d scp 0x%p ref %d", file, line, code, dscp, dscp->refCount, scp, scp ? scp->refCount : 0);
        osi_Log3(afsd_logp, "cm_LookupInternal (2) code 0x%x dscp 0x%p scp 0x%p", code, dscp, scp);
#endif
        *outScpp = scp;
        return code;
    }

    /* None of the possible sysName expansions could be found */
    if (flags & CM_FLAG_CHECKPATH)
        return CM_ERROR_NOSUCHPATH;
    else
        return CM_ERROR_NOSUCHFILE;
}

/*! \brief Unlink a file name

  Encapsulates a call to RXAFS_RemoveFile().

  \param[in] dscp cm_scache_t pointing at the directory containing the
      name to be unlinked.

  \param[in] fnamep Original name to be unlinked.  This is the
      name that will be passed into the RXAFS_RemoveFile() call.
      This parameter is optional.  If not provided, the value will
      be looked up.

  \param[in] came Client name to be unlinked.  This name will be used
      to update the local directory caches.

  \param[in] userp cm_user_t for the request.

  \param[in] reqp Request tracker.

 */
long cm_Unlink(cm_scache_t *dscp, fschar_t *fnamep, clientchar_t * cnamep,
               cm_user_t *userp, cm_req_t *reqp)
{
    long code;
    cm_conn_t *connp;
    AFSFid afsFid;
    int sflags;
    AFSFetchStatus newDirStatus;
    AFSVolSync volSync;
    struct rx_connection * rxconnp;
    cm_dirOp_t dirop;
    cm_scache_t *scp = NULL;
    int free_fnamep = FALSE;

    memset(&volSync, 0, sizeof(volSync));

    if (fnamep == NULL) {
        code = -1;
#ifdef USE_BPLUS
        code = cm_BeginDirOp(dscp, userp, reqp, CM_DIRLOCK_READ,
                             CM_DIROP_FLAG_NONE, &dirop);
        if (code == 0) {
            code = cm_BPlusDirLookupOriginalName(&dirop, cnamep, &fnamep);
            if (code == 0)
                free_fnamep = TRUE;
            cm_EndDirOp(&dirop);
        }
#endif
        if (code)
            goto done;
    }

#ifdef AFS_FREELANCE_CLIENT
    if (cm_freelanceEnabled && dscp == cm_data.rootSCachep) {
        /* deleting a mount point from the root dir. */
        code = cm_FreelanceRemoveMount(fnamep);
        goto done;
    }
#endif

    code = cm_Lookup(dscp, cnamep, CM_FLAG_NOMOUNTCHASE, userp, reqp, &scp);
    if (code)
        goto done;

    /* Check for RO volume */
    if (dscp->flags & CM_SCACHEFLAG_RO) {
        code = CM_ERROR_READONLY;
        goto done;
    }

    /* make sure we don't screw up the dir status during the merge */
    code = cm_BeginDirOp(dscp, userp, reqp, CM_DIRLOCK_NONE,
                         CM_DIROP_FLAG_NONE, &dirop);

    lock_ObtainWrite(&dscp->rw);
    sflags = CM_SCACHESYNC_STOREDATA;
    code = cm_SyncOp(dscp, NULL, userp, reqp, 0, sflags);
    lock_ReleaseWrite(&dscp->rw);
    if (code) {
        cm_EndDirOp(&dirop);
        goto done;
    }

    /* make the RPC */
    InterlockedIncrement(&dscp->activeRPCs);

    afsFid.Volume = dscp->fid.volume;
    afsFid.Vnode = dscp->fid.vnode;
    afsFid.Unique = dscp->fid.unique;

    osi_Log1(afsd_logp, "CALL RemoveFile scp 0x%p", dscp);
    do {
        code = cm_ConnFromFID(&dscp->fid, userp, reqp, &connp);
        if (code)
            continue;

        rxconnp = cm_GetRxConn(connp);
        code = RXAFS_RemoveFile(rxconnp, &afsFid, fnamep,
                                &newDirStatus, &volSync);
        rx_PutConnection(rxconnp);

    } while (cm_Analyze(connp, userp, reqp, &dscp->fid, 1, &volSync, NULL, NULL, code));
    code = cm_MapRPCError(code, reqp);

    if (code)
        osi_Log1(afsd_logp, "CALL RemoveFile FAILURE, code 0x%x", code);
    else
        osi_Log0(afsd_logp, "CALL RemoveFile SUCCESS");

    if (dirop.scp) {
        lock_ObtainWrite(&dirop.scp->dirlock);
        dirop.lockType = CM_DIRLOCK_WRITE;
    }
    lock_ObtainWrite(&dscp->rw);
    cm_dnlcRemove(dscp, cnamep);
    if (code == 0) {
        cm_MergeStatus(NULL, dscp, &newDirStatus, &volSync, userp, reqp, CM_MERGEFLAG_DIROP);
    } else {
        InterlockedDecrement(&scp->activeRPCs);
        if (code == CM_ERROR_NOSUCHFILE) {
            /* windows would not have allowed the request to delete the file
             * if it did not believe the file existed.  therefore, we must
             * have an inconsistent view of the world.
             */
            dscp->cbServerp = NULL;
        }
    }
    cm_SyncOpDone(dscp, NULL, sflags);
    lock_ReleaseWrite(&dscp->rw);

    if (code == 0 && cm_CheckDirOpForSingleChange(&dirop) && cnamep) {
        cm_DirDeleteEntry(&dirop, fnamep);
#ifdef USE_BPLUS
        cm_BPlusDirDeleteEntry(&dirop, cnamep);
#endif
    }
    cm_EndDirOp(&dirop);

    if (scp) {
        cm_ReleaseSCache(scp);
        if (code == 0) {
	    lock_ObtainWrite(&scp->rw);
            if (--scp->linkCount == 0) {
                scp->flags |= CM_SCACHEFLAG_DELETED;
		lock_ObtainWrite(&cm_scacheLock);
                cm_AdjustScacheLRU(scp);
                cm_RemoveSCacheFromHashTable(scp);
		lock_ReleaseWrite(&cm_scacheLock);
            }
            cm_DiscardSCache(scp);
	    lock_ReleaseWrite(&scp->rw);
        }
    }

  done:
    if (free_fnamep)
        free(fnamep);

    return code;
}

/* called with a write locked vnode, and fills in the link info.
 * returns this the vnode still write locked.
 */
long cm_HandleLink(cm_scache_t *linkScp, cm_user_t *userp, cm_req_t *reqp)
{
    long code = 0;

    lock_AssertWrite(&linkScp->rw);
    if (!linkScp->mountPointStringp[0]) {

#ifdef AFS_FREELANCE_CLIENT
	/* File servers do not have data for freelance entries */
        if (cm_freelanceEnabled &&
            linkScp->fid.cell==AFS_FAKE_ROOT_CELL_ID &&
            linkScp->fid.volume==AFS_FAKE_ROOT_VOL_ID )
        {
            code = cm_FreelanceFetchMountPointString(linkScp);
        } else
#endif /* AFS_FREELANCE_CLIENT */
        {
            char temp[MOUNTPOINTLEN];
            osi_hyper_t offset;

            /* read the link data from the file server */
            offset.LowPart = offset.HighPart = 0;
            code = cm_GetData(linkScp, &offset, temp, MOUNTPOINTLEN, userp, reqp);
            if (code)
                return code;

            /*
             * linkScp->length is the actual length of the symlink target string.
             * It is current because cm_GetData merged the most up to date
             * status info into scp and has not dropped the rwlock since.
             */
            if (linkScp->length.LowPart > MOUNTPOINTLEN - 1)
                return CM_ERROR_TOOBIG;
            if (linkScp->length.LowPart == 0)
                return CM_ERROR_INVAL;

            /* make sure we are NUL terminated */
            temp[linkScp->length.LowPart] = 0;
            memcpy(linkScp->mountPointStringp, temp, linkScp->length.LowPart + 1);
        }

        if ( !strnicmp(linkScp->mountPointStringp, "msdfs:", strlen("msdfs:")) )
            linkScp->fileType = CM_SCACHETYPE_DFSLINK;

    }	/* don't have symlink contents cached */

    return code;
}

/* called with a held vnode and a path suffix, with the held vnode being a
 * symbolic link.  Our goal is to generate a new path to interpret, and return
 * this new path in newSpaceBufferp.  If the new vnode is relative to a dir
 * other than the directory containing the symbolic link, then the new root is
 * returned in *newRootScpp, otherwise a null is returned there.
 */
long cm_AssembleLink(cm_scache_t *linkScp, fschar_t *pathSuffixp,
                     cm_scache_t **newRootScpp, cm_space_t **newSpaceBufferp,
                     cm_user_t *userp, cm_req_t *reqp)
{
    long code = 0;
    long len;
    fschar_t *linkp;
    cm_space_t *tsp;

    *newRootScpp = NULL;
    *newSpaceBufferp = NULL;

    lock_ObtainWrite(&linkScp->rw);
    /*
     * Do not get status if we do not already have a callback.
     * The process of reading the symlink string will obtain status information
     * in a single RPC.  No reason to add a second round trip.
     *
     * If we do have a callback, use cm_SyncOp to get status in case the
     * current cm_user_t is not the same as the one that obtained the
     * symlink string contents.
     */
    if (cm_HaveCallback(linkScp)) {
        code = cm_SyncOp(linkScp, NULL, userp, reqp, 0,
                          CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_NEEDCALLBACK);
        if (code) {
            lock_ReleaseWrite(&linkScp->rw);
            cm_ReleaseSCache(linkScp);
            goto done;
        }
        cm_SyncOpDone(linkScp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    }
    code = cm_HandleLink(linkScp, userp, reqp);
    if (code)
        goto done;

    /* if we may overflow the buffer, bail out; buffer is signficantly
     * bigger than max path length, so we don't really have to worry about
     * being a little conservative here.
     */
    if (cm_FsStrLen(linkScp->mountPointStringp) + cm_FsStrLen(pathSuffixp) + 2
        >= CM_UTILS_SPACESIZE) {
        code = CM_ERROR_TOOBIG;
        goto done;
    }

    tsp = cm_GetSpace();
    linkp = linkScp->mountPointStringp;
    if (strncmp(linkp, cm_mountRoot, cm_mountRootLen) == 0) {
        if (strlen(linkp) > cm_mountRootLen)
            StringCbCopyA((char *) tsp->data, sizeof(tsp->data), linkp+cm_mountRootLen+1);
        else
            tsp->data[0] = 0;
        *newRootScpp = cm_RootSCachep(userp, reqp);
        cm_HoldSCache(*newRootScpp);
    } else if (linkp[0] == '\\' && linkp[1] == '\\') {
        if (!strnicmp(&linkp[2], cm_NetbiosName, (len = (long)strlen(cm_NetbiosName))))
        {
            char * p = &linkp[len + 3];
            if (strnicmp(p, "all", 3) == 0)
                p += 4;

            StringCbCopyA(tsp->data, sizeof(tsp->data), p);
            for (p = tsp->data; *p; p++) {
                if (*p == '\\')
                    *p = '/';
            }
            *newRootScpp = cm_RootSCachep(userp, reqp);
            cm_HoldSCache(*newRootScpp);
        } else {
            linkScp->fileType = CM_SCACHETYPE_DFSLINK;
            StringCchCopyA(tsp->data,lengthof(tsp->data), linkp);
            code = CM_ERROR_PATH_NOT_COVERED;
        }
    } else if ( linkScp->fileType == CM_SCACHETYPE_DFSLINK ||
                !strnicmp(linkp, "msdfs:", (len = (long)strlen("msdfs:"))) ) {
        linkScp->fileType = CM_SCACHETYPE_DFSLINK;
        StringCchCopyA(tsp->data,lengthof(tsp->data), linkp);
        code = CM_ERROR_PATH_NOT_COVERED;
    } else if (*linkp == '\\' || *linkp == '/') {
#if 0
        /* formerly, this was considered to be from the AFS root,
         * but this seems to create problems.  instead, we will just
         * reject the link */
        StringCchCopyA(tsp->data,lengthof(tsp->data), linkp+1);
        *newRootScpp = cm_RootSCachep(userp, reqp);
        cm_HoldSCache(*newRootScpp);
#else
        /* we still copy the link data into the response so that
         * the user can see what the link points to
         */
        linkScp->fileType = CM_SCACHETYPE_INVALID;
        StringCchCopyA(tsp->data,lengthof(tsp->data), linkp);
        code = CM_ERROR_NOSUCHPATH;
#endif
    } else {
        /* a relative link */
        StringCchCopyA(tsp->data,lengthof(tsp->data), linkp);
    }
    if (pathSuffixp[0] != 0) {	/* if suffix string is non-null */
        StringCchCatA(tsp->data,lengthof(tsp->data), "\\");
        StringCchCatA(tsp->data,lengthof(tsp->data), pathSuffixp);
    }

    if (code == 0) {
        clientchar_t * cpath = cm_FsStringToClientStringAlloc(tsp->data, -1, NULL);
        if (cpath != NULL) {
        cm_ClientStrCpy(tsp->wdata, lengthof(tsp->wdata), cpath);
        free(cpath);
        *newSpaceBufferp = tsp;
    } else {
            code = CM_ERROR_NOSUCHPATH;
        }
    }

    if (code != 0) {
        cm_FreeSpace(tsp);

        if (code == CM_ERROR_PATH_NOT_COVERED && reqp->tidPathp && reqp->relPathp) {
            cm_VolStatus_Notify_DFS_Mapping(linkScp, reqp->tidPathp, reqp->relPathp);
        }
    }

 done:
    lock_ReleaseWrite(&linkScp->rw);
    return code;
}
#ifdef DEBUG_REFCOUNT
long cm_NameIDbg(cm_scache_t *rootSCachep, clientchar_t *pathp, long flags,
                 cm_user_t *userp, clientchar_t *tidPathp, cm_req_t *reqp,
                 cm_scache_t **outScpp,
                 char * file, long line)
#else
long cm_NameI(cm_scache_t *rootSCachep, clientchar_t *pathp, long flags,
              cm_user_t *userp, clientchar_t *tidPathp,
              cm_req_t *reqp, cm_scache_t **outScpp)
#endif
{
    long code;
    clientchar_t *tp;			/* ptr moving through input buffer */
    clientchar_t tc;			/* temp char */
    int haveComponent;		/* has new component started? */
    clientchar_t component[AFSPATHMAX];	/* this is the new component */
    clientchar_t *cp;			/* component name being assembled */
    cm_scache_t *tscp;		/* current location in the hierarchy */
    cm_scache_t *nscp;		/* next dude down */
    cm_scache_t *dirScp;	/* last dir we searched */
    cm_scache_t *linkScp;	/* new root for the symlink we just
    * looked up */
    cm_space_t *psp;		/* space for current path, if we've hit
    * any symlinks */
    cm_space_t *tempsp;		/* temp vbl */
    clientchar_t *restp;		/* rest of the pathname to interpret */
    int symlinkCount;		/* count of # of symlinks traversed */
    int extraFlag;		/* avoid chasing mt pts for dir cmd */
    int phase = 1;		/* 1 = tidPathp, 2 = pathp */
#define MAX_FID_COUNT 512
    cm_fid_t fids[MAX_FID_COUNT]; /* array of fids processed in this path walk */
    int fid_count = 0;          /* number of fids processed in this path walk */
    int i;

    *outScpp = NULL;

#ifdef DEBUG_REFCOUNT
    afsi_log("%s:%d cm_NameI rootscp 0x%p ref %d", file, line, rootSCachep, rootSCachep->refCount);
    osi_Log4(afsd_logp,"cm_NameI rootscp 0x%p path %S tidpath %S flags 0x%x",
             rootSCachep, pathp ? pathp : L"<NULL>", tidPathp ? tidPathp : L"<NULL>",
             flags);
#endif

    tp = tidPathp;
    if (tp == NULL) {
        tp = pathp;
        phase = 2;
    }
    if (tp == NULL) {
        tp = _C("");
    }
    haveComponent = 0;
    psp = NULL;
    tscp = rootSCachep;
    cm_HoldSCache(tscp);
    symlinkCount = 0;
    dirScp = NULL;


    while (1) {
        tc = *tp++;

        /* map Unix slashes into DOS ones so we can interpret Unix
         * symlinks properly
         */
        if (tc == '/')
            tc = '\\';

        if (!haveComponent) {
            if (tc == '\\') {
                continue;
            } else if (tc == 0) {
                if (phase == 1) {
                    phase = 2;
                    tp = pathp;
                    continue;
                }
                code = 0;
                break;
            } else {
                haveComponent = 1;
                cp = component;
                *cp++ = tc;
            }
        } else {
            /* we have a component here */
            if (tc == 0 || tc == '\\') {
                /* end of the component; we're at the last
                 * component if tc == 0.  However, if the last
                 * is a symlink, we have more to do.
                 */
                *cp++ = 0;	/* add null termination */
		extraFlag = 0;
		if ((flags & CM_FLAG_DIRSEARCH) && tc == 0)
		    extraFlag = CM_FLAG_NOMOUNTCHASE;
		code = cm_Lookup(tscp, component,
                                 flags | extraFlag,
                                 userp, reqp, &nscp);

                if (code == 0) {
                    if (!cm_ClientStrCmp(component,_C("..")) ||
                        !cm_ClientStrCmp(component,_C("."))) {
                        /*
                         * roll back the fid list until we find the
                         * fid that matches where we are now.  Its not
                         * necessarily one or two fids because they
                         * might have been symlinks or mount points or
                         * both that were crossed.
                         */
                        for ( i=fid_count-1; i>=0; i--) {
                            if (!cm_FidCmp(&nscp->fid, &fids[i]))
                                break;
                        }
                        fid_count = i+1;
                    } else {
                        /* add the new fid to the list */
                        if (fid_count == MAX_FID_COUNT) {
                            code = CM_ERROR_TOO_MANY_SYMLINKS;
                            cm_ReleaseSCache(nscp);
                            nscp = NULL;
                            break;
                        }
                        fids[fid_count++] = nscp->fid;
                    }
                }

                if (code) {
		    cm_ReleaseSCache(tscp);
		    if (dirScp)
			cm_ReleaseSCache(dirScp);
		    if (psp)
			cm_FreeSpace(psp);
		    if ((code == CM_ERROR_NOSUCHFILE || code == CM_ERROR_BPLUS_NOMATCH) &&
                        tscp->fileType == CM_SCACHETYPE_SYMLINK) {
			osi_Log0(afsd_logp,"cm_NameI code CM_ERROR_NOSUCHPATH");
			return CM_ERROR_NOSUCHPATH;
		    } else {
			osi_Log1(afsd_logp,"cm_NameI code 0x%x", code);
			return code;
		    }
		}

		haveComponent = 0;	/* component done */
		if (dirScp)
		    cm_ReleaseSCache(dirScp);
		dirScp = tscp;		/* for some symlinks */
		tscp = nscp;	        /* already held */
		nscp = NULL;
		if (tc == 0 && !(flags & CM_FLAG_FOLLOW) && phase == 2) {
                    code = 0;
                    if (dirScp) {
                        cm_ReleaseSCache(dirScp);
                        dirScp = NULL;
                    }
                    break;
                }

                /* now, if tscp is a symlink, we should follow it and
                 * assemble the path again.
                 */
                lock_ObtainWrite(&tscp->rw);
                code = cm_SyncOp(tscp, NULL, userp, reqp, 0,
                                  CM_SCACHESYNC_GETSTATUS
                                  | CM_SCACHESYNC_NEEDCALLBACK);
                if (code) {
                    lock_ReleaseWrite(&tscp->rw);
                    cm_ReleaseSCache(tscp);
                    tscp = NULL;
                    if (dirScp) {
                        cm_ReleaseSCache(dirScp);
                        dirScp = NULL;
                    }
                    break;
                }
		cm_SyncOpDone(tscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

                if (tscp->fileType == CM_SCACHETYPE_SYMLINK) {
                    /* this is a symlink; assemble a new buffer */
                    lock_ReleaseWrite(&tscp->rw);
                    if (symlinkCount++ >= MAX_SYMLINK_COUNT) {
                        cm_ReleaseSCache(tscp);
                        tscp = NULL;
                        if (dirScp) {
                            cm_ReleaseSCache(dirScp);
                            dirScp = NULL;
                        }
                        if (psp)
                            cm_FreeSpace(psp);
			osi_Log0(afsd_logp,"cm_NameI code CM_ERROR_TOO_MANY_SYMLINKS");
                        return CM_ERROR_TOO_MANY_SYMLINKS;
                    }
                    if (tc == 0)
                        restp = _C("");
                    else
                        restp = tp;

                    {
                        fschar_t * frestp;

                        /* TODO: make this better */
                        frestp = cm_ClientStringToFsStringAlloc(restp, -1, NULL);
                        code = cm_AssembleLink(tscp, frestp, &linkScp, &tempsp, userp, reqp);
                        free(frestp);
                    }

                    if (code == 0 && linkScp != NULL) {
                        if (linkScp == cm_data.rootSCachep) {
                            fid_count = 0;
                            i = 0;
                        } else {
                            for ( i=0; i<fid_count; i++) {
                                if ( !cm_FidCmp(&linkScp->fid, &fids[i]) ) {
                                    code = CM_ERROR_TOO_MANY_SYMLINKS;
                                    cm_ReleaseSCache(linkScp);
                                    nscp = NULL;
                                    break;
                                }
                            }
                        }
                        if (i == fid_count && fid_count < MAX_FID_COUNT) {
                            fids[fid_count++] = linkScp->fid;
                        }
                    }

                    if (code) {
                        /* something went wrong */
                        cm_ReleaseSCache(tscp);
                        tscp = NULL;
                        if (dirScp) {
                            cm_ReleaseSCache(dirScp);
                            dirScp = NULL;
                        }
                        break;
                    }

                    /* otherwise, tempsp has the new path,
                     * and linkScp is the new root from
                     * which to interpret that path.
                     * Continue with the namei processing,
                     * also doing the bookkeeping for the
                     * space allocation and tracking the
                     * vnode reference counts.
                     */
                    if (psp)
                        cm_FreeSpace(psp);
                    psp = tempsp;
                    tp = psp->wdata;
                    cm_ReleaseSCache(tscp);
                    tscp = linkScp;
                    linkScp = NULL;
                    /* already held
                     * by AssembleLink
                     * now, if linkScp is null, that's
                     * AssembleLink's way of telling us that
                     * the sym link is relative to the dir
                     * containing the link.  We have a ref
                     * to it in dirScp, and we hold it now
                     * and reuse it as the new spot in the
                     * dir hierarchy.
                     */
                    if (tscp == NULL) {
                        tscp = dirScp;
                        dirScp = NULL;
                    }
                } else {
                    /* not a symlink, we may be done */
                    lock_ReleaseWrite(&tscp->rw);
                    if (tc == 0) {
                        if (phase == 1) {
                            phase = 2;
                            tp = pathp;
                            continue;
                        }
                        if (dirScp) {
                            cm_ReleaseSCache(dirScp);
                            dirScp = NULL;
                        }
                        code = 0;
                        break;
                    }
                }
                if (dirScp) {
                    cm_ReleaseSCache(dirScp);
                    dirScp = NULL;
                }
            } /* end of a component */
            else
                *cp++ = tc;
        } /* we have a component */
    } /* big while loop over all components */

    /* already held */
    if (dirScp)
        cm_ReleaseSCache(dirScp);
    if (psp)
        cm_FreeSpace(psp);
    if (code == 0)
        *outScpp = tscp;
    else if (tscp)
        cm_ReleaseSCache(tscp);

#ifdef DEBUG_REFCOUNT
    afsi_log("%s:%d cm_NameI code 0x%x outScpp 0x%p ref %d", file, line, code, *outScpp, (*outScpp) ? (*outScpp)->refCount : 0);
#endif
    osi_Log2(afsd_logp,"cm_NameI code 0x%x outScpp 0x%p", code, *outScpp);
    return code;
}

/* called with a dir, and a vnode within the dir that happens to be a symlink.
 * We chase the link, and return a held pointer to the target, if it exists,
 * in *outScpp.  If we succeed, we return 0, otherwise we return an error code
 * and do not hold or return a target vnode.
 *
 * This is very similar to calling cm_NameI with the last component of a name,
 * which happens to be a symlink, except that we've already passed by the name.
 *
 * This function is typically called by the directory listing functions, which
 * encounter symlinks but need to return the proper file length so programs
 * like "more" work properly when they make use of the attributes retrieved from
 * the dir listing.
 *
 * The input vnode should not be locked when this function is called.
 */
long cm_EvaluateSymLink(cm_scache_t *dscp, cm_scache_t *linkScp,
                         cm_scache_t **outScpp, cm_user_t *userp, cm_req_t *reqp)
{
    long code;
    cm_space_t *spacep;
    cm_scache_t *newRootScp;

    *outScpp = NULL;

    osi_Log1(afsd_logp, "Evaluating symlink scp 0x%p", linkScp);

    code = cm_AssembleLink(linkScp, "", &newRootScp, &spacep, userp, reqp);
    if (code)
        return code;

    /* now, if newRootScp is NULL, we're really being told that the symlink
     * is relative to the current directory (dscp).
     */
    if (newRootScp == NULL) {
        newRootScp = dscp;
        cm_HoldSCache(dscp);
    }

    code = cm_NameI(newRootScp, spacep->wdata,
                    CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW | CM_FLAG_DIRSEARCH,
                    userp, NULL, reqp, outScpp);

    if (code == CM_ERROR_NOSUCHFILE || code == CM_ERROR_BPLUS_NOMATCH)
        code = CM_ERROR_NOSUCHPATH;

    /* this stuff is allocated no matter what happened on the namei call,
     * so free it */
    cm_FreeSpace(spacep);
    cm_ReleaseSCache(newRootScp);

    if (linkScp == *outScpp) {
        cm_ReleaseSCache(*outScpp);
        *outScpp = NULL;
        code = CM_ERROR_NOSUCHPATH;
    }

    return code;
}

/* for a given entry, make sure that it isn't in the stat cache, and then
 * add it to the list of file IDs to be obtained.
 *
 * Don't bother adding it if we already have a vnode.  Note that the dir
 * is locked, so we have to be careful checking the vnode we're thinking of
 * processing, to avoid deadlocks.
 */
long cm_TryBulkProc(cm_scache_t *scp, cm_dirEntry_t *dep, void *rockp,
                     osi_hyper_t *offp)
{
    osi_hyper_t thyper;
    cm_bulkStat_t *bsp;
    int i;
    cm_scache_t *tscp;
    cm_fid_t tfid;

    bsp = rockp;

    /* Don't overflow bsp. */
    if (bsp->counter >= CM_BULKMAX)
        return CM_ERROR_STOPNOW;

    thyper.LowPart = cm_data.buf_blockSize;
    thyper.HighPart = 0;
    thyper = LargeIntegerAdd(thyper, bsp->bufOffset);

    /* thyper is now the first byte past the end of the record we're
     * interested in, and bsp->bufOffset is the first byte of the record
     * we're interested in.
     * Skip data in the others.
     * Skip '.' and '..'
     */
    if (LargeIntegerLessThan(*offp, bsp->bufOffset))
        return 0;
    if (LargeIntegerGreaterThanOrEqualTo(*offp, thyper))
        return CM_ERROR_STOPNOW;
    if (strcmp(dep->name, ".") == 0 || strcmp(dep->name, "..") == 0)
        return 0;

    cm_SetFid(&tfid, scp->fid.cell, scp->fid.volume, ntohl(dep->fid.vnode), ntohl(dep->fid.unique));
    tscp = cm_FindSCache(&tfid);
    if (tscp) {
        if (lock_TryWrite(&tscp->rw)) {
            /* we have an entry that we can look at */
            if (!(tscp->flags & CM_SCACHEFLAG_EACCESS) && cm_HaveCallback(tscp)) {
                /* we have a callback on it.  Don't bother
                 * fetching this stat entry, since we're happy
                 * with the info we have.
                 */
                lock_ReleaseWrite(&tscp->rw);
                cm_ReleaseSCache(tscp);
                return 0;
            }
            lock_ReleaseWrite(&tscp->rw);
        }	/* got lock */
        cm_ReleaseSCache(tscp);
    }	/* found entry */

#ifdef AFS_FREELANCE_CLIENT
    // yj: if this is a mountpoint under root.afs then we don't want it
    // to be bulkstat-ed, instead, we call getSCache directly and under
    // getSCache, it is handled specially.
    if 	( cm_freelanceEnabled &&
          tfid.cell==AFS_FAKE_ROOT_CELL_ID &&
          tfid.volume==AFS_FAKE_ROOT_VOL_ID &&
          !(tfid.vnode==0x1 && tfid.unique==0x1) )
    {
        osi_Log0(afsd_logp, "cm_TryBulkProc Freelance calls cm_SCache on root.afs mountpoint");
        return cm_GetSCache(&tfid, &tscp, NULL, NULL);
    }
#endif /* AFS_FREELANCE_CLIENT */

    i = bsp->counter++;
    bsp->fids[i].Volume = scp->fid.volume;
    bsp->fids[i].Vnode = tfid.vnode;
    bsp->fids[i].Unique = tfid.unique;
    return 0;
}

afs_int32
cm_TryBulkStatRPC(cm_scache_t *dscp, cm_bulkStat_t *bbp, cm_user_t *userp, cm_req_t *reqp)
{
    afs_int32 code = 0;
    AFSCBFids fidStruct;
    AFSBulkStats statStruct;
    cm_conn_t *connp;
    AFSCBs callbackStruct;
    long filex;
    AFSVolSync volSync;
    cm_callbackRequest_t cbReq;
    long filesThisCall;
    long i;
    long j;
    cm_scache_t *scp;
    cm_fid_t tfid;
    struct rx_connection * rxconnp;
    int inlinebulk;		/* Did we use InlineBulkStatus RPC or not? */

    memset(&volSync, 0, sizeof(volSync));

    /* otherwise, we may have one or more bulk stat's worth of stuff in bb;
     * make the calls to create the entries.  Handle AFSCBMAX files at a
     * time.
     */
    for (filex = 0; filex < bbp->counter; filex += filesThisCall) {
        filesThisCall = bbp->counter - filex;
        if (filesThisCall > AFSCBMAX)
            filesThisCall = AFSCBMAX;

        fidStruct.AFSCBFids_len = filesThisCall;
        fidStruct.AFSCBFids_val = &bbp->fids[filex];
        statStruct.AFSBulkStats_len = filesThisCall;
        statStruct.AFSBulkStats_val = &bbp->stats[filex];
        callbackStruct.AFSCBs_len = filesThisCall;
        callbackStruct.AFSCBs_val = &bbp->callbacks[filex];
        cm_StartCallbackGrantingCall(NULL, &cbReq);
        osi_Log1(afsd_logp, "CALL BulkStatus, %d entries", filesThisCall);

        /*
         * Whenever cm_Analyze is called for a RXAFS_ RPC there must
         * be a FID provided.  However, the error code from RXAFS_BulkStatus
         * or RXAFS_InlinkBulkStatus does not apply to any FID.  Therefore,
         * we generate an invalid FID to match with the RPC error.
         */
        cm_SetFid(&tfid, dscp->fid.cell, dscp->fid.volume, 0, 0);

        do {
            inlinebulk = 0;

            code = cm_ConnFromFID(&tfid, userp, reqp, &connp);
            if (code)
                continue;

            rxconnp = cm_GetRxConn(connp);
	    if (!(connp->serverp->flags & CM_SERVERFLAG_NOINLINEBULK)) {
		code = RXAFS_InlineBulkStatus(rxconnp, &fidStruct,
                                              &statStruct, &callbackStruct, &volSync);
		if (code == RXGEN_OPCODE) {
		    cm_SetServerNoInlineBulk(connp->serverp, 0);
		} else {
		    inlinebulk = 1;
		}
	    }
	    if (!inlinebulk) {
		code = RXAFS_BulkStatus(rxconnp, &fidStruct,
					&statStruct, &callbackStruct, &volSync);
	    }
            rx_PutConnection(rxconnp);

            /*
             * If InlineBulk RPC was called and it succeeded,
             * then pull out the return code from the status info
             * and use it for cm_Analyze so that we can failover to other
             * .readonly volume instances.  But only do it for errors that
             * are volume global.
             */
            if (inlinebulk && code == 0 && (&bbp->stats[0])->errorCode) {
                osi_Log1(afsd_logp, "cm_TryBulkStat inline-bulk stat error: %d",
                          (&bbp->stats[0])->errorCode);
                switch ((&bbp->stats[0])->errorCode) {
                case VBUSY:
                case VRESTARTING:
                case VNOVOL:
                case VMOVED:
                case VOFFLINE:
                case VSALVAGE:
                case VNOSERVICE:
                case VIO:
                    code = (&bbp->stats[0])->errorCode;
                    break;
                default:
                    /* Rx and Rxkad errors are volume global */
                    if ( (&bbp->stats[0])->errorCode >= -64 && (&bbp->stats[0])->errorCode < 0 ||
                         (&bbp->stats[0])->errorCode >= ERROR_TABLE_BASE_RXK && (&bbp->stats[0])->errorCode < ERROR_TABLE_BASE_RXK + 256)
                        code = (&bbp->stats[0])->errorCode;
                }
            }
        } while (cm_Analyze(connp, userp, reqp, &tfid, 0, &volSync, NULL, &cbReq, code));
        code = cm_MapRPCError(code, reqp);

        /*
         * might as well quit on an error, since we're not going to do
         * much better on the next immediate call, either.
         */
        if (code) {
            osi_Log2(afsd_logp, "CALL %sBulkStatus FAILURE code 0x%x",
		      inlinebulk ? "Inline" : "", code);
            cm_EndCallbackGrantingCall(NULL, &cbReq, NULL, NULL, 0);
            break;
        }

        /*
         * The bulk RPC has succeeded or at least not failed with a
         * volume global error result.  For items that have inlineBulk
         * errors we must call cm_Analyze in order to perform required
         * logging of errors.
         *
         * If the RPC was not inline bulk or the entry either has no error
         * the status must be merged.
         */
        osi_Log1(afsd_logp, "CALL %sBulkStatus SUCCESS", inlinebulk ? "Inline" : "");

        for (i = 0; i<filesThisCall; i++) {
            j = filex + i;
            cm_SetFid(&tfid, dscp->fid.cell, bbp->fids[j].Volume, bbp->fids[j].Vnode, bbp->fids[j].Unique);

            if (inlinebulk && (&bbp->stats[j])->errorCode) {
                cm_req_t treq = *reqp;
                cm_Analyze(NULL, userp, &treq, &tfid, 0, &volSync, NULL, &cbReq, (&bbp->stats[j])->errorCode);
            } else {
                code = cm_GetSCache(&tfid, &scp, userp, reqp);
                if (code != 0)
                    continue;

                /*
                 * otherwise, if this entry has no callback info,
                 * merge in this.  If there is existing callback info
                 * we skip the merge because the existing data must be
                 * current (we have a callback) and the response from
                 * a non-inline bulk rpc might actually be wrong.
                 *
                 * now, we have to be extra paranoid on merging in this
                 * information, since we didn't use cm_SyncOp before
                 * starting the fetch to make sure that no bad races
                 * were occurring.  Specifically, we need to make sure
                 * we don't obliterate any newer information in the
                 * vnode than have here.
                 *
                 * Right now, be pretty conservative: if there's a
                 * callback or a pending call, skip it.
                 * However, if the prior attempt to obtain status
                 * was refused access or the volume is .readonly,
                 * take the data in any case since we have nothing
                 * better for the in flight directory enumeration that
                 * resulted in this function being called.
                 */
                lock_ObtainRead(&scp->rw);
                if ((scp->cbServerp == NULL &&
                     !(scp->flags & (CM_SCACHEFLAG_FETCHING | CM_SCACHEFLAG_STORING | CM_SCACHEFLAG_SIZESTORING))) ||
                     (scp->flags & CM_SCACHEFLAG_PURERO) ||
                     (scp->flags & CM_SCACHEFLAG_EACCESS))
                {
                    lock_ConvertRToW(&scp->rw);
                    cm_EndCallbackGrantingCall(scp, &cbReq,
                                               &bbp->callbacks[j],
                                               &volSync,
                                               CM_CALLBACK_MAINTAINCOUNT);
                    InterlockedIncrement(&scp->activeRPCs);
                    cm_MergeStatus(dscp, scp, &bbp->stats[j], &volSync, userp, reqp, 0);
                    lock_ReleaseWrite(&scp->rw);
                } else {
                    lock_ReleaseRead(&scp->rw);
                }
                cm_ReleaseSCache(scp);
            }
        } /* all files in the response */
        /* now tell it to drop the count,
         * after doing the vnode processing above */
        cm_EndCallbackGrantingCall(NULL, &cbReq, NULL, NULL, 0);
    }	/* while there are still more files to process */

    return code;
}

/* called with a write locked scp and a pointer to a buffer.  Make bulk stat
 * calls on all undeleted files in the page of the directory specified.
 */
afs_int32
cm_TryBulkStat(cm_scache_t *dscp, osi_hyper_t *offsetp, cm_user_t *userp,
	       cm_req_t *reqp)
{
    afs_int32 code;
    cm_bulkStat_t *bbp;

    osi_Log1(afsd_logp, "cm_TryBulkStat dir 0x%p", dscp);

    /* should be on a buffer boundary */
    osi_assertx((offsetp->LowPart & (cm_data.buf_blockSize - 1)) == 0, "invalid offset");

    bbp = malloc(sizeof(cm_bulkStat_t));
    memset(bbp, 0, sizeof(cm_bulkStat_t));
    bbp->bufOffset = *offsetp;

    lock_ReleaseWrite(&dscp->rw);
    /* first, assemble the file IDs we need to stat */
    code = cm_ApplyDir(dscp, cm_TryBulkProc, (void *) bbp, offsetp, userp, reqp, NULL);

    /* if we failed, bail out early */
    if (code && code != CM_ERROR_STOPNOW) {
        free(bbp);
        lock_ObtainWrite(&dscp->rw);
        return code;
    }

    code = cm_TryBulkStatRPC(dscp, bbp, userp, reqp);
    osi_Log1(afsd_logp, "END cm_TryBulkStat code 0x%x", code);

    lock_ObtainWrite(&dscp->rw);
    free(bbp);
    return 0;
}

void cm_StatusFromAttr(AFSStoreStatus *statusp, cm_scache_t *scp, cm_attr_t *attrp)
{
    long mask;

    /* initialize store back mask as inexpensive local variable */
    mask = 0;
    memset(statusp, 0, sizeof(AFSStoreStatus));

    /* copy out queued info from scache first, if scp passed in */
    if (scp) {
        if (scp->mask & CM_SCACHEMASK_CLIENTMODTIME) {
            statusp->ClientModTime = scp->clientModTime;
            mask |= AFS_SETMODTIME;
            scp->mask &= ~CM_SCACHEMASK_CLIENTMODTIME;
        }
    }

    if (attrp) {
        /* now add in our locally generated request */
        if (attrp->mask & CM_ATTRMASK_CLIENTMODTIME) {
            statusp->ClientModTime = attrp->clientModTime;
            mask |= AFS_SETMODTIME;
        }
        if (attrp->mask & CM_ATTRMASK_UNIXMODEBITS) {
            statusp->UnixModeBits = attrp->unixModeBits;
            mask |= AFS_SETMODE;
        }
        if (attrp->mask & CM_ATTRMASK_OWNER) {
            statusp->Owner = attrp->owner;
            mask |= AFS_SETOWNER;
        }
        if (attrp->mask & CM_ATTRMASK_GROUP) {
            statusp->Group = attrp->group;
            mask |= AFS_SETGROUP;
        }
    }
    statusp->Mask = mask;
}

/* set the file size, and make sure that all relevant buffers have been
 * truncated.  Ensure that any partially truncated buffers have been zeroed
 * to the end of the buffer.
 */
long cm_SetLength(cm_scache_t *scp, osi_hyper_t *sizep, cm_user_t *userp,
                   cm_req_t *reqp)
{
    long code;
    int shrinking;

    /* start by locking out buffer creation */
    lock_ObtainWrite(&scp->bufCreateLock);

    /* verify that this is a file, not a dir or a symlink */
    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, reqp, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code)
        goto done;
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

    if (scp->fileType != CM_SCACHETYPE_FILE) {
        code = CM_ERROR_ISDIR;
        goto done;
    }

  startover:
    if (LargeIntegerLessThan(*sizep, scp->length))
        shrinking = 1;
    else
        shrinking = 0;

    lock_ReleaseWrite(&scp->rw);

    /* can't hold scp->rw lock here, since we may wait for a storeback to
     * finish if the buffer package is cleaning a buffer by storing it to
     * the server.
     */
    if (shrinking)
        buf_Truncate(scp, userp, reqp, sizep);

    /* now ensure that file length is short enough, and update truncPos */
    lock_ObtainWrite(&scp->rw);

    /* make sure we have a callback (so we have the right value for the
     * length), and wait for it to be safe to do a truncate.
     */
    code = cm_SyncOp(scp, NULL, userp, reqp, PRSFS_WRITE,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS
                      | CM_SCACHESYNC_SETSTATUS | CM_SCACHESYNC_SETSIZE);

    /* If we only have 'i' bits, then we should still be able to set
       the size of a file we created. */
    if (code == CM_ERROR_NOACCESS && scp->creator == userp) {
        code = cm_SyncOp(scp, NULL, userp, reqp, PRSFS_INSERT,
                         CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS
                         | CM_SCACHESYNC_SETSTATUS | CM_SCACHESYNC_SETSIZE);
    }

    if (code)
        goto done;

    if (LargeIntegerLessThan(*sizep, scp->length)) {
        /* a real truncation.  If truncPos is not set yet, or is bigger
         * than where we're truncating the file, set truncPos to this
         * new value.
         */
        if (!shrinking)
            goto startover;
        if (!(scp->mask & CM_SCACHEMASK_TRUNCPOS)
             || LargeIntegerLessThan(*sizep, scp->length)) {
            /* set trunc pos */
            scp->truncPos = *sizep;
            scp->mask |= CM_SCACHEMASK_TRUNCPOS;
        }
        /* in either case, the new file size has been changed */
        scp->length = *sizep;
        scp->mask |= CM_SCACHEMASK_LENGTH;
    }
    else if (LargeIntegerGreaterThan(*sizep, scp->length)) {
        /* really extending the file */
        scp->length = *sizep;
        scp->mask |= CM_SCACHEMASK_LENGTH;
    }

    /* done successfully */
    code = 0;

    cm_SyncOpDone(scp, NULL,
		   CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS
		   | CM_SCACHESYNC_SETSTATUS | CM_SCACHESYNC_SETSIZE);

  done:
    lock_ReleaseWrite(&scp->rw);
    lock_ReleaseWrite(&scp->bufCreateLock);

    return code;
}

/* set the file size or other attributes (but not both at once) */
long cm_SetAttr(cm_scache_t *scp, cm_attr_t *attrp, cm_user_t *userp,
                cm_req_t *reqp)
{
    long code;
    AFSFetchStatus afsOutStatus;
    AFSVolSync volSync;
    cm_conn_t *connp;
    AFSFid tfid;
    AFSStoreStatus afsInStatus;
    struct rx_connection * rxconnp;

    memset(&volSync, 0, sizeof(volSync));

    /* handle file length setting */
    if (attrp->mask & CM_ATTRMASK_LENGTH)
        return cm_SetLength(scp, &attrp->length, userp, reqp);

    lock_ObtainWrite(&scp->rw);
    /* Check for RO volume */
    if (scp->flags & CM_SCACHEFLAG_RO) {
        code = CM_ERROR_READONLY;
	lock_ReleaseWrite(&scp->rw);
        return code;
    }

    /* otherwise, we have to make an RPC to get the status */
    code = cm_SyncOp(scp, NULL, userp, reqp, 0, CM_SCACHESYNC_STORESTATUS);
    if (code) {
	lock_ReleaseWrite(&scp->rw);
        return code;
    }
    lock_ConvertWToR(&scp->rw);

    /* make the attr structure */
    cm_StatusFromAttr(&afsInStatus, scp, attrp);

    tfid.Volume = scp->fid.volume;
    tfid.Vnode = scp->fid.vnode;
    tfid.Unique = scp->fid.unique;
    lock_ReleaseRead(&scp->rw);

    /* now make the RPC */
    InterlockedIncrement(&scp->activeRPCs);

    osi_Log1(afsd_logp, "CALL StoreStatus scp 0x%p", scp);
    do {
        code = cm_ConnFromFID(&scp->fid, userp, reqp, &connp);
        if (code)
            continue;

        rxconnp = cm_GetRxConn(connp);
        code = RXAFS_StoreStatus(rxconnp, &tfid,
                                  &afsInStatus, &afsOutStatus, &volSync);
        rx_PutConnection(rxconnp);

    } while (cm_Analyze(connp, userp, reqp,
                         &scp->fid, 1, &volSync, NULL, NULL, code));
    code = cm_MapRPCError(code, reqp);

    if (code)
        osi_Log1(afsd_logp, "CALL StoreStatus FAILURE, code 0x%x", code);
    else
        osi_Log0(afsd_logp, "CALL StoreStatus SUCCESS");

    lock_ObtainWrite(&scp->rw);
    if (code == 0)
        cm_MergeStatus(NULL, scp, &afsOutStatus, &volSync, userp, reqp,
                        CM_MERGEFLAG_FORCE|CM_MERGEFLAG_STOREDATA);
    else
        InterlockedDecrement(&scp->activeRPCs);
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_STORESTATUS);

    /* if we're changing the mode bits, discard the ACL cache,
     * since we changed the mode bits.
     */
    if (afsInStatus.Mask & AFS_SETMODE)
	cm_FreeAllACLEnts(scp);
    lock_ReleaseWrite(&scp->rw);
    return code;
}

long cm_Create(cm_scache_t *dscp, clientchar_t *cnamep, long flags, cm_attr_t *attrp,
               cm_scache_t **scpp, cm_user_t *userp, cm_req_t *reqp)
{
    cm_conn_t *connp;
    long code;
    AFSFid dirAFSFid;
    cm_callbackRequest_t cbReq;
    AFSFid newAFSFid;
    cm_fid_t newFid;
    cm_scache_t *scp = NULL;
    int didEnd;
    AFSStoreStatus inStatus;
    AFSFetchStatus updatedDirStatus;
    AFSFetchStatus newFileStatus;
    AFSCallBack newFileCallback;
    AFSVolSync volSync;
    struct rx_connection * rxconnp;
    cm_dirOp_t dirop;
    fschar_t * fnamep = NULL;

    memset(&volSync, 0, sizeof(volSync));

    /* can't create names with @sys in them; must expand it manually first.
     * return "invalid request" if they try.
     */
    if (cm_ExpandSysName(cnamep, NULL, 0, 0)) {
        return CM_ERROR_ATSYS;
    }

#ifdef AFS_FREELANCE_CLIENT
    /* Freelance root volume does not hold files */
    if (cm_freelanceEnabled &&
        dscp->fid.cell==AFS_FAKE_ROOT_CELL_ID &&
        dscp->fid.volume==AFS_FAKE_ROOT_VOL_ID )
    {
        return CM_ERROR_NOACCESS;
    }
#endif /* AFS_FREELANCE_CLIENT */

    /* Check for RO volume */
    if (dscp->flags & CM_SCACHEFLAG_RO)
        return CM_ERROR_READONLY;

    /* before starting the RPC, mark that we're changing the file data, so
     * that someone who does a chmod will know to wait until our call
     * completes.
     */
    cm_BeginDirOp(dscp, userp, reqp, CM_DIRLOCK_NONE, CM_DIROP_FLAG_NONE,
                  &dirop);
    lock_ObtainWrite(&dscp->rw);
    code = cm_SyncOp(dscp, NULL, userp, reqp, 0, CM_SCACHESYNC_STOREDATA);
    lock_ReleaseWrite(&dscp->rw);
    if (code == 0) {
        cm_StartCallbackGrantingCall(NULL, &cbReq);
    } else {
        cm_EndDirOp(&dirop);
    }
    if (code) {
        return code;
    }
    didEnd = 0;

    fnamep = cm_ClientStringToFsStringAlloc(cnamep, -1, NULL);

    cm_StatusFromAttr(&inStatus, NULL, attrp);

    /* try the RPC now */
    InterlockedIncrement(&dscp->activeRPCs);
    osi_Log1(afsd_logp, "CALL CreateFile scp 0x%p", dscp);
    do {
        code = cm_ConnFromFID(&dscp->fid, userp, reqp, &connp);
        if (code)
            continue;

        dirAFSFid.Volume = dscp->fid.volume;
        dirAFSFid.Vnode = dscp->fid.vnode;
        dirAFSFid.Unique = dscp->fid.unique;

        rxconnp = cm_GetRxConn(connp);
        code = RXAFS_CreateFile(connp->rxconnp, &dirAFSFid, fnamep,
                                 &inStatus, &newAFSFid, &newFileStatus,
                                 &updatedDirStatus, &newFileCallback,
                                 &volSync);
        rx_PutConnection(rxconnp);

    } while (cm_Analyze(connp, userp, reqp,
                         &dscp->fid, 1, &volSync, NULL, &cbReq, code));
    code = cm_MapRPCError(code, reqp);

    if (code)
        osi_Log1(afsd_logp, "CALL CreateFile FAILURE, code 0x%x", code);
    else
        osi_Log0(afsd_logp, "CALL CreateFile SUCCESS");

    if (dirop.scp) {
        lock_ObtainWrite(&dirop.scp->dirlock);
        dirop.lockType = CM_DIRLOCK_WRITE;
    }
    lock_ObtainWrite(&dscp->rw);
    if (code == 0)
        cm_MergeStatus(NULL, dscp, &updatedDirStatus, &volSync, userp, reqp, CM_MERGEFLAG_DIROP);
    else
        InterlockedDecrement(&dscp->activeRPCs);
    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_STOREDATA);
    lock_ReleaseWrite(&dscp->rw);

    /* now try to create the file's entry, too, but be careful to
     * make sure that we don't merge in old info.  Since we weren't locking
     * out any requests during the file's creation, we may have pretty old
     * info.
     */
    if (code == 0) {
        cm_SetFid(&newFid, dscp->fid.cell, dscp->fid.volume, newAFSFid.Vnode, newAFSFid.Unique);
        code = cm_GetSCache(&newFid, &scp, userp, reqp);
        if (code == 0) {
            lock_ObtainWrite(&scp->rw);
	    scp->creator = userp;		/* remember who created it */
            if (!cm_HaveCallback(scp)) {
                cm_EndCallbackGrantingCall(scp, &cbReq,
                                           &newFileCallback, &volSync, 0);
                InterlockedIncrement(&scp->activeRPCs);
                cm_MergeStatus(dscp, scp, &newFileStatus, &volSync,
                               userp, reqp, 0);
                didEnd = 1;
            }
            lock_ReleaseWrite(&scp->rw);
        }
    }

    /* make sure we end things properly */
    if (!didEnd)
        cm_EndCallbackGrantingCall(NULL, &cbReq, NULL, NULL, 0);

    if (scp && cm_CheckDirOpForSingleChange(&dirop)) {
        cm_DirCreateEntry(&dirop, fnamep, &newFid);
#ifdef USE_BPLUS
        cm_BPlusDirCreateEntry(&dirop, cnamep, &newFid);
#endif
    }
    cm_EndDirOp(&dirop);

    if (fnamep)
        free(fnamep);

    if (scp) {
        if (scpp)
            *scpp = scp;
        else
            cm_ReleaseSCache(scp);
    }
    return code;
}

/*
 * locked if TRUE means write-locked
 * else the cm_scache_t rw must not be held
 */
long cm_FSync(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp, afs_uint32 locked)
{
    long code;

    if (locked)
        lock_ReleaseWrite(&scp->rw);

    osi_Log2(afsd_logp, "cm_FSync scp 0x%p userp 0x%p", scp, userp);

    code = buf_CleanVnode(scp, userp, reqp);
    if (code == 0) {
        lock_ObtainWrite(&scp->rw);

        if (scp->mask & (CM_SCACHEMASK_TRUNCPOS
                          | CM_SCACHEMASK_CLIENTMODTIME
                          | CM_SCACHEMASK_LENGTH))
            code = cm_StoreMini(scp, userp, reqp);

        if (scp->flags & (CM_SCACHEFLAG_OVERQUOTA | CM_SCACHEFLAG_OUTOFSPACE)) {
	    code = (scp->flags & CM_SCACHEFLAG_OVERQUOTA) ? CM_ERROR_QUOTA : CM_ERROR_SPACE;
	    scp->flags &= ~(CM_SCACHEFLAG_OVERQUOTA | CM_SCACHEFLAG_OUTOFSPACE);
	}

        if (!locked)
            lock_ReleaseWrite(&scp->rw);
    } else if (locked) {
        lock_ObtainWrite(&scp->rw);
    }
    return code;
}

long cm_MakeDir(cm_scache_t *dscp, clientchar_t *cnamep, long flags, cm_attr_t *attrp,
                cm_user_t *userp, cm_req_t *reqp, cm_scache_t **scpp)
{
    cm_conn_t *connp;
    long code;
    AFSFid dirAFSFid;
    cm_callbackRequest_t cbReq;
    AFSFid newAFSFid;
    cm_fid_t newFid;
    cm_scache_t *scp = NULL;
    int didEnd;
    AFSStoreStatus inStatus;
    AFSFetchStatus updatedDirStatus;
    AFSFetchStatus newDirStatus;
    AFSCallBack newDirCallback;
    AFSVolSync volSync;
    struct rx_connection * rxconnp;
    cm_dirOp_t dirop;
    fschar_t * fnamep = NULL;

    memset(&volSync, 0, sizeof(volSync));

    /* can't create names with @sys in them; must expand it manually first.
     * return "invalid request" if they try.
     */
    if (cm_ExpandSysName(cnamep, NULL, 0, 0)) {
        return CM_ERROR_ATSYS;
    }

#ifdef AFS_FREELANCE_CLIENT
    /* Freelance root volume does not hold subdirectories */
    if (cm_freelanceEnabled &&
        dscp->fid.cell==AFS_FAKE_ROOT_CELL_ID &&
        dscp->fid.volume==AFS_FAKE_ROOT_VOL_ID )
    {
        return CM_ERROR_NOACCESS;
    }
#endif /* AFS_FREELANCE_CLIENT */

    /* Check for RO volume */
    if (dscp->flags & CM_SCACHEFLAG_RO)
        return CM_ERROR_READONLY;

    /* before starting the RPC, mark that we're changing the directory
     * data, so that someone who does a chmod on the dir will wait until
     * our call completes.
     */
    cm_BeginDirOp(dscp, userp, reqp, CM_DIRLOCK_NONE, CM_DIROP_FLAG_NONE,
                  &dirop);
    lock_ObtainWrite(&dscp->rw);
    code = cm_SyncOp(dscp, NULL, userp, reqp, 0, CM_SCACHESYNC_STOREDATA);
    lock_ReleaseWrite(&dscp->rw);
    if (code == 0) {
        cm_StartCallbackGrantingCall(NULL, &cbReq);
    } else {
        cm_EndDirOp(&dirop);
    }
    if (code) {
        return code;
    }
    didEnd = 0;

    fnamep = cm_ClientStringToFsStringAlloc(cnamep, -1, NULL);
    cm_StatusFromAttr(&inStatus, NULL, attrp);

    /* try the RPC now */
    InterlockedIncrement(&dscp->activeRPCs);
    osi_Log1(afsd_logp, "CALL MakeDir scp 0x%p", dscp);
    do {
        code = cm_ConnFromFID(&dscp->fid, userp, reqp, &connp);
        if (code)
            continue;

        dirAFSFid.Volume = dscp->fid.volume;
        dirAFSFid.Vnode = dscp->fid.vnode;
        dirAFSFid.Unique = dscp->fid.unique;

        rxconnp = cm_GetRxConn(connp);
        code = RXAFS_MakeDir(connp->rxconnp, &dirAFSFid, fnamep,
                              &inStatus, &newAFSFid, &newDirStatus,
                              &updatedDirStatus, &newDirCallback,
                              &volSync);
        rx_PutConnection(rxconnp);

    } while (cm_Analyze(connp, userp, reqp,
                        &dscp->fid, 1, &volSync, NULL, &cbReq, code));
    code = cm_MapRPCError(code, reqp);

    if (code)
        osi_Log1(afsd_logp, "CALL MakeDir FAILURE, code 0x%x", code);
    else
        osi_Log0(afsd_logp, "CALL MakeDir SUCCESS");

    if (dirop.scp) {
        lock_ObtainWrite(&dirop.scp->dirlock);
        dirop.lockType = CM_DIRLOCK_WRITE;
    }
    lock_ObtainWrite(&dscp->rw);
    if (code == 0)
        cm_MergeStatus(NULL, dscp, &updatedDirStatus, &volSync, userp, reqp, CM_MERGEFLAG_DIROP);
    else
        InterlockedDecrement(&dscp->activeRPCs);
    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_STOREDATA);
    lock_ReleaseWrite(&dscp->rw);

    /* now try to create the new dir's entry, too, but be careful to
     * make sure that we don't merge in old info.  Since we weren't locking
     * out any requests during the file's creation, we may have pretty old
     * info.
     */
    if (code == 0) {
        cm_SetFid(&newFid, dscp->fid.cell, dscp->fid.volume, newAFSFid.Vnode, newAFSFid.Unique);
        code = cm_GetSCache(&newFid, &scp, userp, reqp);
        if (code == 0) {
            lock_ObtainWrite(&scp->rw);
            if (!cm_HaveCallback(scp)) {
                cm_EndCallbackGrantingCall(scp, &cbReq,
                                            &newDirCallback, &volSync, 0);
                InterlockedIncrement(&scp->activeRPCs);
                cm_MergeStatus(dscp, scp, &newDirStatus, &volSync,
                                userp, reqp, 0);
                didEnd = 1;
            }
            lock_ReleaseWrite(&scp->rw);
        }
    }

    /* make sure we end things properly */
    if (!didEnd)
        cm_EndCallbackGrantingCall(NULL, &cbReq, NULL, NULL, 0);

    if (scp && cm_CheckDirOpForSingleChange(&dirop)) {
        cm_DirCreateEntry(&dirop, fnamep, &newFid);
#ifdef USE_BPLUS
        cm_BPlusDirCreateEntry(&dirop, cnamep, &newFid);
#endif
    }
    cm_EndDirOp(&dirop);

    free(fnamep);

    if (scp) {
        if (scpp)
            *scpp = scp;
        else
            cm_ReleaseSCache(scp);
    }

    /* and return error code */
    return code;
}

long cm_Link(cm_scache_t *dscp, clientchar_t *cnamep, cm_scache_t *sscp, long flags,
             cm_user_t *userp, cm_req_t *reqp)
{
    cm_conn_t *connp;
    long code = 0;
    AFSFid dirAFSFid;
    AFSFid existingAFSFid;
    AFSFetchStatus updatedDirStatus;
    AFSFetchStatus newLinkStatus;
    AFSVolSync volSync;
    struct rx_connection * rxconnp;
    cm_dirOp_t dirop;
    fschar_t * fnamep = NULL;

    memset(&volSync, 0, sizeof(volSync));

    if (dscp->fid.cell != sscp->fid.cell ||
        dscp->fid.volume != sscp->fid.volume) {
        return CM_ERROR_CROSSDEVLINK;
    }

    /* Check for RO volume */
    if (dscp->flags & CM_SCACHEFLAG_RO)
        return CM_ERROR_READONLY;

    cm_BeginDirOp(dscp, userp, reqp, CM_DIRLOCK_NONE, CM_DIROP_FLAG_NONE,
                  &dirop);
    lock_ObtainWrite(&dscp->rw);
    code = cm_SyncOp(dscp, NULL, userp, reqp, 0, CM_SCACHESYNC_STOREDATA);
    lock_ReleaseWrite(&dscp->rw);
    if (code != 0)
        cm_EndDirOp(&dirop);

    if (code)
        return code;

    fnamep = cm_ClientStringToFsStringAlloc(cnamep, -1, NULL);

    /* try the RPC now */
    InterlockedIncrement(&dscp->activeRPCs);
    osi_Log1(afsd_logp, "CALL Link scp 0x%p", dscp);
    do {
        code = cm_ConnFromFID(&dscp->fid, userp, reqp, &connp);
        if (code) continue;

        dirAFSFid.Volume = dscp->fid.volume;
        dirAFSFid.Vnode = dscp->fid.vnode;
        dirAFSFid.Unique = dscp->fid.unique;

        existingAFSFid.Volume = sscp->fid.volume;
        existingAFSFid.Vnode = sscp->fid.vnode;
        existingAFSFid.Unique = sscp->fid.unique;

        rxconnp = cm_GetRxConn(connp);
        code = RXAFS_Link(rxconnp, &dirAFSFid, fnamep, &existingAFSFid,
            &newLinkStatus, &updatedDirStatus, &volSync);
        rx_PutConnection(rxconnp);
        osi_Log1(afsd_logp,"  RXAFS_Link returns 0x%x", code);

    } while (cm_Analyze(connp, userp, reqp, &dscp->fid, 1, &volSync, NULL, NULL, code));

    code = cm_MapRPCError(code, reqp);

    if (code)
        osi_Log1(afsd_logp, "CALL Link FAILURE, code 0x%x", code);
    else
        osi_Log0(afsd_logp, "CALL Link SUCCESS");

    if (dirop.scp) {
        lock_ObtainWrite(&dirop.scp->dirlock);
        dirop.lockType = CM_DIRLOCK_WRITE;
    }
    lock_ObtainWrite(&dscp->rw);
    if (code == 0) {
        cm_MergeStatus(NULL, dscp, &updatedDirStatus, &volSync, userp, reqp, CM_MERGEFLAG_DIROP);
    } else {
        InterlockedDecrement(&dscp->activeRPCs);
    }
    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_STOREDATA);
    lock_ReleaseWrite(&dscp->rw);

    if (code == 0) {
        if (cm_CheckDirOpForSingleChange(&dirop)) {
            cm_DirCreateEntry(&dirop, fnamep, &sscp->fid);
#ifdef USE_BPLUS
            cm_BPlusDirCreateEntry(&dirop, cnamep, &sscp->fid);
#endif
        }
    }
    cm_EndDirOp(&dirop);

    /* Update the linked object status */
    if (code == 0) {
        lock_ObtainWrite(&sscp->rw);
        InterlockedIncrement(&sscp->activeRPCs);
        cm_MergeStatus(NULL, sscp, &newLinkStatus, &volSync, userp, reqp, 0);
        lock_ReleaseWrite(&sscp->rw);
    }

    free(fnamep);

    return code;
}

long cm_SymLink(cm_scache_t *dscp, clientchar_t *cnamep, fschar_t *contentsp, long flags,
                cm_attr_t *attrp, cm_user_t *userp, cm_req_t *reqp, cm_scache_t **scpp)
{
    cm_conn_t *connp;
    long code;
    AFSFid dirAFSFid;
    AFSFid newAFSFid;
    cm_fid_t newFid;
    cm_scache_t *scp;
    AFSStoreStatus inStatus;
    AFSFetchStatus updatedDirStatus;
    AFSFetchStatus newLinkStatus;
    AFSVolSync volSync;
    struct rx_connection * rxconnp;
    cm_dirOp_t dirop;
    fschar_t *fnamep = NULL;

    if (scpp)
        *scpp = NULL;

    /* Check for RO volume */
    if (dscp->flags & CM_SCACHEFLAG_RO)
        return CM_ERROR_READONLY;

    memset(&volSync, 0, sizeof(volSync));

    /* before starting the RPC, mark that we're changing the directory data,
     * so that someone who does a chmod on the dir will wait until our
     * call completes.
     */
    cm_BeginDirOp(dscp, userp, reqp, CM_DIRLOCK_NONE, CM_DIROP_FLAG_NONE,
                  &dirop);
    lock_ObtainWrite(&dscp->rw);
    code = cm_SyncOp(dscp, NULL, userp, reqp, 0, CM_SCACHESYNC_STOREDATA);
    lock_ReleaseWrite(&dscp->rw);
    if (code != 0)
        cm_EndDirOp(&dirop);
    if (code) {
        return code;
    }

    fnamep = cm_ClientStringToFsStringAlloc(cnamep, -1, NULL);

    cm_StatusFromAttr(&inStatus, NULL, attrp);

    /* try the RPC now */
    InterlockedIncrement(&dscp->activeRPCs);
    osi_Log1(afsd_logp, "CALL Symlink scp 0x%p", dscp);
    do {
        code = cm_ConnFromFID(&dscp->fid, userp, reqp, &connp);
        if (code)
            continue;

        dirAFSFid.Volume = dscp->fid.volume;
        dirAFSFid.Vnode = dscp->fid.vnode;
        dirAFSFid.Unique = dscp->fid.unique;

        rxconnp = cm_GetRxConn(connp);
        code = RXAFS_Symlink(rxconnp, &dirAFSFid, fnamep, contentsp,
                              &inStatus, &newAFSFid, &newLinkStatus,
                              &updatedDirStatus, &volSync);
        rx_PutConnection(rxconnp);

    } while (cm_Analyze(connp, userp, reqp,
                         &dscp->fid, 1, &volSync, NULL, NULL, code));
    code = cm_MapRPCError(code, reqp);

    if (code)
        osi_Log1(afsd_logp, "CALL Symlink FAILURE, code 0x%x", code);
    else
        osi_Log0(afsd_logp, "CALL Symlink SUCCESS");

    if (dirop.scp) {
        lock_ObtainWrite(&dirop.scp->dirlock);
        dirop.lockType = CM_DIRLOCK_WRITE;
    }
    lock_ObtainWrite(&dscp->rw);
    if (code == 0)
        cm_MergeStatus(NULL, dscp, &updatedDirStatus, &volSync, userp, reqp, CM_MERGEFLAG_DIROP);
    else
        InterlockedDecrement(&dscp->activeRPCs);
    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_STOREDATA);
    lock_ReleaseWrite(&dscp->rw);

    if (code == 0) {
        if (cm_CheckDirOpForSingleChange(&dirop)) {
            cm_SetFid(&newFid, dscp->fid.cell, dscp->fid.volume, newAFSFid.Vnode, newAFSFid.Unique);

            cm_DirCreateEntry(&dirop, fnamep, &newFid);
#ifdef USE_BPLUS
            cm_BPlusDirCreateEntry(&dirop, cnamep, &newFid);
#endif
        }
    }
    cm_EndDirOp(&dirop);

    /* now try to create the new dir's entry, too, but be careful to
     * make sure that we don't merge in old info.  Since we weren't locking
     * out any requests during the file's creation, we may have pretty old
     * info.
     */
    if (code == 0) {
        cm_SetFid(&newFid, dscp->fid.cell, dscp->fid.volume, newAFSFid.Vnode, newAFSFid.Unique);
        code = cm_GetSCache(&newFid, &scp, userp, reqp);
        if (code == 0) {
            lock_ObtainWrite(&scp->rw);
            if (!cm_HaveCallback(scp)) {
                InterlockedIncrement(&scp->activeRPCs);
                cm_MergeStatus(dscp, scp, &newLinkStatus, &volSync,
                                userp, reqp, 0);
            }
            lock_ReleaseWrite(&scp->rw);

            if (scpp) {
                *scpp = scp;
            } else {
                cm_ReleaseSCache(scp);
            }
        }
    }

    free(fnamep);

    /* and return error code */
    return code;
}

/*! \brief Remove a directory

  Encapsulates a call to RXAFS_RemoveDir().

  \param[in] dscp cm_scache_t for the directory containing the
      directory to be removed.

  \param[in] fnamep This will be the original name of the directory
      as known to the file server.   It will be passed in to RXAFS_RemoveDir().
      This parameter is optional.  If it is not provided the value
      will be looked up.

  \param[in] cnamep Normalized name used to update the local
      directory caches.

  \param[in] userp cm_user_t for the request.

  \param[in] reqp Request tracker.
*/
long cm_RemoveDir(cm_scache_t *dscp, fschar_t *fnamep, clientchar_t *cnamep, cm_user_t *userp, cm_req_t *reqp)
{
    cm_conn_t *connp;
    long code;
    AFSFid dirAFSFid;
    int didEnd;
    AFSFetchStatus updatedDirStatus;
    AFSVolSync volSync;
    struct rx_connection * rxconnp;
    cm_dirOp_t dirop;
    cm_scache_t *scp = NULL;
    int free_fnamep = FALSE;

    memset(&volSync, 0, sizeof(volSync));

    if (fnamep == NULL) {
        code = -1;
#ifdef USE_BPLUS
        code = cm_BeginDirOp(dscp, userp, reqp, CM_DIRLOCK_READ,
                             CM_DIROP_FLAG_NONE, &dirop);
        if (code == 0) {
            code = cm_BPlusDirLookupOriginalName(&dirop, cnamep, &fnamep);
            if (code == 0)
                free_fnamep = TRUE;
            cm_EndDirOp(&dirop);
        }
#endif
        if (code)
            goto done;
    }

    code = cm_Lookup(dscp, cnamep, CM_FLAG_NOMOUNTCHASE, userp, reqp, &scp);
    if (code)
        goto done;

    /* Check for RO volume */
    if (dscp->flags & CM_SCACHEFLAG_RO) {
        code = CM_ERROR_READONLY;
        goto done;
    }

    /* before starting the RPC, mark that we're changing the directory data,
     * so that someone who does a chmod on the dir will wait until our
     * call completes.
     */
    cm_BeginDirOp(dscp, userp, reqp, CM_DIRLOCK_NONE, CM_DIROP_FLAG_NONE,
                  &dirop);
    lock_ObtainWrite(&dscp->rw);
    code = cm_SyncOp(dscp, NULL, userp, reqp, 0, CM_SCACHESYNC_STOREDATA);
    lock_ReleaseWrite(&dscp->rw);
    if (code) {
        cm_EndDirOp(&dirop);
        goto done;
    }
    didEnd = 0;

    /* try the RPC now */
    InterlockedIncrement(&dscp->activeRPCs);
    osi_Log1(afsd_logp, "CALL RemoveDir scp 0x%p", dscp);
    do {
        code = cm_ConnFromFID(&dscp->fid, userp, reqp, &connp);
        if (code)
            continue;

        dirAFSFid.Volume = dscp->fid.volume;
        dirAFSFid.Vnode = dscp->fid.vnode;
        dirAFSFid.Unique = dscp->fid.unique;

        rxconnp = cm_GetRxConn(connp);
        code = RXAFS_RemoveDir(rxconnp, &dirAFSFid, fnamep,
                               &updatedDirStatus, &volSync);
        rx_PutConnection(rxconnp);

    } while (cm_Analyze(connp, userp, reqp,
                        &dscp->fid, 1, &volSync, NULL, NULL, code));
    code = cm_MapRPCErrorRmdir(code, reqp);

    if (code)
        osi_Log1(afsd_logp, "CALL RemoveDir FAILURE, code 0x%x", code);
    else
        osi_Log0(afsd_logp, "CALL RemoveDir SUCCESS");

    if (dirop.scp) {
        lock_ObtainWrite(&dirop.scp->dirlock);
        dirop.lockType = CM_DIRLOCK_WRITE;
    }
    lock_ObtainWrite(&dscp->rw);
    if (code == 0) {
        cm_dnlcRemove(dscp, cnamep);
        cm_MergeStatus(NULL, dscp, &updatedDirStatus, &volSync, userp, reqp, CM_MERGEFLAG_DIROP);
    } else {
        InterlockedDecrement(&dscp->activeRPCs);
    }
    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_STOREDATA);
    lock_ReleaseWrite(&dscp->rw);

    if (code == 0) {
        if (cm_CheckDirOpForSingleChange(&dirop) && cnamep != NULL) {
            cm_DirDeleteEntry(&dirop, fnamep);
#ifdef USE_BPLUS
            cm_BPlusDirDeleteEntry(&dirop, cnamep);
#endif
        }
    }
    cm_EndDirOp(&dirop);

    if (scp) {
        cm_ReleaseSCache(scp);
        if (code == 0) {
	    lock_ObtainWrite(&scp->rw);
            scp->flags |= CM_SCACHEFLAG_DELETED;
            lock_ObtainWrite(&cm_scacheLock);
            cm_AdjustScacheLRU(scp);
            cm_RemoveSCacheFromHashTable(scp);
            lock_ReleaseWrite(&cm_scacheLock);
	    lock_ReleaseWrite(&scp->rw);
        }
    }

  done:
    if (free_fnamep)
        free(fnamep);

    /* and return error code */
    return code;
}

long cm_Open(cm_scache_t *scp, int type, cm_user_t *userp)
{
    /* grab mutex on contents */
    lock_ObtainWrite(&scp->rw);

    /* reset the prefetch info */
    scp->prefetch.base.LowPart = 0;		/* base */
    scp->prefetch.base.HighPart = 0;
    scp->prefetch.end.LowPart = 0;		/* and end */
    scp->prefetch.end.HighPart = 0;

    /* release mutex on contents */
    lock_ReleaseWrite(&scp->rw);

    /* we're done */
    return 0;
}

/*! \brief Rename a file or directory

  Encapsulates a RXAFS_Rename() call.

  \param[in] oldDscp cm_scache_t for the directory containing the old
      name.

  \param[in] oldNamep The original old name known to the file server.
      This is the name that will be passed into the RXAFS_Rename().
      If it is not provided, it will be looked up.

  \param[in] normalizedOldNamep Normalized old name.  This is used for
  updating local directory caches.

  \param[in] newDscp cm_scache_t for the directory containing the new
  name.

  \param[in] newNamep New name. Normalized.

  \param[in] userp cm_user_t for the request.

  \param[in,out] reqp Request tracker.

*/
long cm_Rename(cm_scache_t *oldDscp, fschar_t *oldNamep, clientchar_t *cOldNamep,
               cm_scache_t *newDscp, clientchar_t *cNewNamep, cm_user_t *userp,
               cm_req_t *reqp)
{
    cm_conn_t *connp;
    long code;
    AFSFid oldDirAFSFid;
    AFSFid newDirAFSFid;
    int didEnd;
    AFSFetchStatus updatedOldDirStatus;
    AFSFetchStatus updatedNewDirStatus;
    AFSVolSync volSync;
    int oneDir;
    struct rx_connection * rxconnp;
    cm_dirOp_t oldDirOp;
    cm_fid_t   fileFid;
    int        diropCode = -1;
    cm_dirOp_t newDirOp;
    fschar_t * newNamep = NULL;
    int free_oldNamep = FALSE;
    cm_scache_t *oldScp = NULL, *newScp = NULL;

    memset(&volSync, 0, sizeof(volSync));

    if (cOldNamep == NULL || cNewNamep == NULL ||
        cm_ClientStrLen(cOldNamep) == 0 ||
        cm_ClientStrLen(cNewNamep) == 0)
        return CM_ERROR_INVAL;

    /*
     * Before we permit the operation, make sure that we do not already have
     * an object in the destination directory that has a case-insensitive match
     * for this name UNLESS the matching object is the object we are renaming.
     */
    code = cm_Lookup(oldDscp, cOldNamep, 0, userp, reqp, &oldScp);
    if (code) {
        osi_Log2(afsd_logp, "cm_Rename oldDscp 0x%p cOldName %S old name lookup failed",
                 oldDscp, osi_LogSaveStringW(afsd_logp, cOldNamep));
        goto done;
    }

    /* Case sensitive lookup.  If this succeeds we are done. */
    code = cm_Lookup(newDscp, cNewNamep, 0, userp, reqp, &newScp);
    if (code) {
        /*
         * Case insensitive lookup.  If this succeeds, it could have found the
         * same file with a name that differs only by case or it could be a
         * different file entirely.
         */
        code = cm_Lookup(newDscp, cNewNamep, CM_FLAG_CASEFOLD, userp, reqp, &newScp);
        if (code == 0) {
            /* found a matching object with the new name */
            if (cm_FidCmp(&oldScp->fid, &newScp->fid)) {
                /* and they don't match so return an error */
                osi_Log2(afsd_logp, "cm_Rename newDscp 0x%p cNewName %S new name already exists",
                          newDscp, osi_LogSaveStringW(afsd_logp, cNewNamep));
                code = CM_ERROR_EXISTS;
            }
            cm_ReleaseSCache(newScp);
            newScp = NULL;
        } else if (code == CM_ERROR_AMBIGUOUS_FILENAME) {
            code = CM_ERROR_EXISTS;
        } else {
            /* The target does not exist.  Clear the error and perform the rename. */
            code = 0;
        }
    }

    /* Check for RO volume */
    if (code == 0 &&
        (oldDscp->flags & CM_SCACHEFLAG_RO) || (newDscp->flags & CM_SCACHEFLAG_RO)) {
        code = CM_ERROR_READONLY;
    }

    if (code)
        goto done;

    if (oldNamep == NULL) {
        code = -1;
#ifdef USE_BPLUS
        code = cm_BeginDirOp(oldDscp, userp, reqp, CM_DIRLOCK_READ,
                             CM_DIROP_FLAG_NONE, &oldDirOp);
        if (code == 0) {
            code = cm_BPlusDirLookupOriginalName(&oldDirOp, cOldNamep, &oldNamep);
            if (code == 0)
                free_oldNamep = TRUE;
            cm_EndDirOp(&oldDirOp);
        }
#endif
        if (code) {
            osi_Log2(afsd_logp, "cm_Rename oldDscp 0x%p cOldName %S Original Name lookup failed",
                      oldDscp, osi_LogSaveStringW(afsd_logp, cOldNamep));
            goto done;
        }
    }


    /* before starting the RPC, mark that we're changing the directory data,
     * so that someone who does a chmod on the dir will wait until our call
     * completes.  We do this in vnode order so that we don't deadlock,
     * which makes the code a little verbose.
     */
    if (oldDscp == newDscp) {
        /* check for identical names */
        if (cm_ClientStrCmp(cOldNamep, cNewNamep) == 0) {
            osi_Log2(afsd_logp, "cm_Rename oldDscp 0x%p newDscp 0x%p CM_ERROR_RENAME_IDENTICAL",
                      oldDscp, newDscp);
            code = CM_ERROR_RENAME_IDENTICAL;
            goto done;
        }

        oneDir = 1;
        cm_BeginDirOp(oldDscp, userp, reqp, CM_DIRLOCK_NONE,
                      CM_DIROP_FLAG_NONE, &oldDirOp);
        lock_ObtainWrite(&oldDscp->rw);
        cm_dnlcRemove(oldDscp, cOldNamep);
        cm_dnlcRemove(oldDscp, cNewNamep);
        code = cm_SyncOp(oldDscp, NULL, userp, reqp, 0,
                          CM_SCACHESYNC_STOREDATA);
        lock_ReleaseWrite(&oldDscp->rw);
        if (code != 0) {
            cm_EndDirOp(&oldDirOp);
        }
    }
    else {
        /* two distinct dir vnodes */
        oneDir = 0;
        if (oldDscp->fid.cell != newDscp->fid.cell ||
             oldDscp->fid.volume != newDscp->fid.volume) {
            osi_Log2(afsd_logp, "cm_Rename oldDscp 0x%p newDscp 0x%p CM_ERROR_CROSSDEVLINK",
                      oldDscp, newDscp);
            code = CM_ERROR_CROSSDEVLINK;
            goto done;
        }

        /* shouldn't happen that we have distinct vnodes for two
         * different files, but could due to deliberate attack, or
         * stale info.  Avoid deadlocks and quit now.
         */
        if (oldDscp->fid.vnode == newDscp->fid.vnode) {
            osi_Log2(afsd_logp, "cm_Rename oldDscp 0x%p newDscp 0x%p vnode collision",
                      oldDscp, newDscp);
            code = CM_ERROR_CROSSDEVLINK;
            goto done;
        }

        if (oldDscp->fid.vnode < newDscp->fid.vnode) {
            cm_BeginDirOp(oldDscp, userp, reqp, CM_DIRLOCK_NONE,
                          CM_DIROP_FLAG_NONE, &oldDirOp);
            lock_ObtainWrite(&oldDscp->rw);
            cm_dnlcRemove(oldDscp, cOldNamep);
            code = cm_SyncOp(oldDscp, NULL, userp, reqp, 0,
                             CM_SCACHESYNC_STOREDATA);
            lock_ReleaseWrite(&oldDscp->rw);
            if (code != 0)
                cm_EndDirOp(&oldDirOp);
            if (code == 0) {
                cm_BeginDirOp(newDscp, userp, reqp, CM_DIRLOCK_NONE,
                              CM_DIROP_FLAG_NONE, &newDirOp);
                lock_ObtainWrite(&newDscp->rw);
                cm_dnlcRemove(newDscp, cNewNamep);
                code = cm_SyncOp(newDscp, NULL, userp, reqp, 0,
                                 CM_SCACHESYNC_STOREDATA);
                lock_ReleaseWrite(&newDscp->rw);
                if (code) {
                    cm_EndDirOp(&newDirOp);

                    /* cleanup first one */
                    lock_ObtainWrite(&oldDscp->rw);
                    cm_SyncOpDone(oldDscp, NULL,
                                   CM_SCACHESYNC_STOREDATA);
                    lock_ReleaseWrite(&oldDscp->rw);
                    cm_EndDirOp(&oldDirOp);
                }
            }
        }
        else {
            /* lock the new vnode entry first */
            cm_BeginDirOp(newDscp, userp, reqp, CM_DIRLOCK_NONE,
                          CM_DIROP_FLAG_NONE, &newDirOp);
            lock_ObtainWrite(&newDscp->rw);
            cm_dnlcRemove(newDscp, cNewNamep);
            code = cm_SyncOp(newDscp, NULL, userp, reqp, 0,
                              CM_SCACHESYNC_STOREDATA);
            lock_ReleaseWrite(&newDscp->rw);
            if (code != 0)
                cm_EndDirOp(&newDirOp);
            if (code == 0) {
                cm_BeginDirOp(oldDscp, userp, reqp, CM_DIRLOCK_NONE,
                              CM_DIROP_FLAG_NONE, &oldDirOp);
                lock_ObtainWrite(&oldDscp->rw);
                cm_dnlcRemove(oldDscp, cOldNamep);
                code = cm_SyncOp(oldDscp, NULL, userp, reqp, 0,
                                  CM_SCACHESYNC_STOREDATA);
                lock_ReleaseWrite(&oldDscp->rw);
                if (code != 0)
                    cm_EndDirOp(&oldDirOp);
                if (code) {
                    /* cleanup first one */
                    lock_ObtainWrite(&newDscp->rw);
                    cm_SyncOpDone(newDscp, NULL,
                                   CM_SCACHESYNC_STOREDATA);
                    lock_ReleaseWrite(&newDscp->rw);
                    cm_EndDirOp(&newDirOp);
                }
            }
        }
    }	/* two distinct vnodes */

    if (code)
        goto done;

    didEnd = 0;

    newNamep = cm_ClientStringToFsStringAlloc(cNewNamep, -1, NULL);

    /* try the RPC now */
    InterlockedIncrement(&oldDscp->activeRPCs);
    if (!oneDir)
        InterlockedIncrement(&newDscp->activeRPCs);
    osi_Log2(afsd_logp, "CALL Rename old scp 0x%p new scp 0x%p",
              oldDscp, newDscp);
    do {
        code = cm_ConnFromFID(&oldDscp->fid, userp, reqp, &connp);
        if (code)
            continue;

        oldDirAFSFid.Volume = oldDscp->fid.volume;
        oldDirAFSFid.Vnode = oldDscp->fid.vnode;
        oldDirAFSFid.Unique = oldDscp->fid.unique;
        newDirAFSFid.Volume = newDscp->fid.volume;
        newDirAFSFid.Vnode = newDscp->fid.vnode;
        newDirAFSFid.Unique = newDscp->fid.unique;

        rxconnp = cm_GetRxConn(connp);
        code = RXAFS_Rename(rxconnp, &oldDirAFSFid, oldNamep,
                            &newDirAFSFid, newNamep,
                            &updatedOldDirStatus, &updatedNewDirStatus,
                            &volSync);
        rx_PutConnection(rxconnp);

    } while (cm_Analyze(connp, userp, reqp, &oldDscp->fid, 1,
                         &volSync, NULL, NULL, code));
    code = cm_MapRPCError(code, reqp);

    if (code)
        osi_Log1(afsd_logp, "CALL Rename FAILURE, code 0x%x", code);
    else
        osi_Log0(afsd_logp, "CALL Rename SUCCESS");

    /* update the individual stat cache entries for the directories */
    if (oldDirOp.scp) {
        lock_ObtainWrite(&oldDirOp.scp->dirlock);
        oldDirOp.lockType = CM_DIRLOCK_WRITE;
    }
    lock_ObtainWrite(&oldDscp->rw);

    if (code == 0)
        cm_MergeStatus(NULL, oldDscp, &updatedOldDirStatus, &volSync,
                       userp, reqp, CM_MERGEFLAG_DIROP);
    else
        InterlockedDecrement(&oldDscp->activeRPCs);
    cm_SyncOpDone(oldDscp, NULL, CM_SCACHESYNC_STOREDATA);
    lock_ReleaseWrite(&oldDscp->rw);

    if (code == 0 && cm_CheckDirOpForSingleChange(&oldDirOp)) {
#ifdef USE_BPLUS
        diropCode = cm_BPlusDirLookup(&oldDirOp, cOldNamep, &fileFid);
        if (diropCode == CM_ERROR_INEXACT_MATCH)
            diropCode = 0;
        else if (diropCode == EINVAL)
#endif
            diropCode = cm_DirLookup(&oldDirOp, oldNamep, &fileFid);

        if (diropCode == 0) {
            if (oneDir) {
                diropCode = cm_DirCreateEntry(&oldDirOp, newNamep, &fileFid);
#ifdef USE_BPLUS
                cm_BPlusDirCreateEntry(&oldDirOp, cNewNamep, &fileFid);
#endif
            }

            if (diropCode == 0) {
                diropCode = cm_DirDeleteEntry(&oldDirOp, oldNamep);
#ifdef USE_BPLUS
                cm_BPlusDirDeleteEntry(&oldDirOp, cOldNamep);
#endif
            }
        }
    }
    cm_EndDirOp(&oldDirOp);

    /* and update it for the new one, too, if necessary */
    if (!oneDir) {
        if (newDirOp.scp) {
            lock_ObtainWrite(&newDirOp.scp->dirlock);
            newDirOp.lockType = CM_DIRLOCK_WRITE;
        }
        lock_ObtainWrite(&newDscp->rw);
        if (code == 0)
            cm_MergeStatus(NULL, newDscp, &updatedNewDirStatus, &volSync,
                            userp, reqp, CM_MERGEFLAG_DIROP);
        else
            InterlockedIncrement(&newDscp->activeRPCs);
        cm_SyncOpDone(newDscp, NULL, CM_SCACHESYNC_STOREDATA);
        lock_ReleaseWrite(&newDscp->rw);

#if 0
        /*
         * The following optimization does not work.
         * When the file server processed a RXAFS_Rename() request the
         * FID of the object being moved between directories is not
         * preserved.  The client does not know the new FID nor the
         * version number of the target.  Not only can we not create
         * the directory entry in the new directory, but we can't
         * preserve the cached data for the file.  It must be re-read
         * from the file server.  - jaltman, 2009/02/20
         */
        if (code == 0) {
            /* we only make the local change if we successfully made
               the change in the old directory AND there was only one
               change in the new directory */
            if (diropCode == 0 && cm_CheckDirOpForSingleChange(&newDirOp)) {
                cm_DirCreateEntry(&newDirOp, newNamep, &fileFid);
#ifdef USE_BPLUS
                cm_BPlusDirCreateEntry(&newDirOp, cNewNamep, &fileFid);
#endif
            }
        }
#endif /* 0 */
        cm_EndDirOp(&newDirOp);
    }

    /*
     * After the rename the file server has invalidated the callbacks
     * on the file that was moved nor do we have a directory reference
     * to it anymore.
     */
    lock_ObtainWrite(&oldScp->rw);
    cm_DiscardSCache(oldScp);
    lock_ReleaseWrite(&oldScp->rw);

  done:
    if (oldScp)
        cm_ReleaseSCache(oldScp);

    if (free_oldNamep)
        free(oldNamep);

    free(newNamep);

    /* and return error code */
    return code;
}

/* Byte range locks:

   The OpenAFS Windows client has to fake byte range locks given no
   server side support for such locks.  This is implemented as keyed
   byte range locks on the cache manager.

   Keyed byte range locks:

   Each cm_scache_t structure keeps track of a list of keyed locks.
   The key for a lock identifies an owner of a set of locks (referred
   to as a client).  Each key is represented by a value.  The set of
   key values used within a specific cm_scache_t structure form a
   namespace that has a scope of just that cm_scache_t structure.  The
   same key value can be used with another cm_scache_t structure and
   correspond to a completely different client.  However it is
   advantageous for the SMB or IFS layer to make sure that there is a
   1-1 mapping between client and keys over all cm_scache_t objects.

   Assume a client C has key Key(C) (although, since the scope of the
   key is a cm_scache_t, the key can be Key(C,S), where S is the
   cm_scache_t.  But assume a 1-1 relation between keys and clients).
   A byte range (O,+L) denotes byte addresses (O) through (O+L-1)
   inclusive (a.k.a. [O,O+L-1]).  The function Key(x) is implemented
   through cm_generateKey() function for both SMB and IFS.

   The list of locks for a cm_scache_t object S is maintained in
   S->fileLocks.  The cache manager will set a lock on the AFS file
   server in order to assert the locks in S->fileLocks.  If only
   shared locks are in place for S, then the cache manager will obtain
   a LockRead lock, while if there are any exclusive locks, it will
   obtain a LockWrite lock.  If the exclusive locks are all released
   while the shared locks remain, then the cache manager will
   downgrade the lock from LockWrite to LockRead.  Similarly, if an
   exclusive lock is obtained when only shared locks exist, then the
   cache manager will try to upgrade the lock from LockRead to
   LockWrite.

   Each lock L owned by client C maintains a key L->key such that
   L->key == Key(C), the effective range defined by L->LOffset and
   L->LLength such that the range of bytes affected by the lock is
   (L->LOffset, +L->LLength), a type maintained in L->LockType which
   is either exclusive or shared.

   Lock states:

   A lock exists iff it is in S->fileLocks for some cm_scache_t
   S. Existing locks are in one of the following states: ACTIVE,
   WAITLOCK, WAITUNLOCK, LOST, DELETED.

   The following sections describe each lock and the associated
   transitions.

   1. ACTIVE: A lock L is ACTIVE iff the cache manager has asserted
      the lock with the AFS file server.  This type of lock can be
      exercised by a client to read or write to the locked region (as
      the lock allows).

      1.1 ACTIVE->LOST: When the AFS file server fails to extend a
        server lock that was required to assert the lock.  Before
        marking the lock as lost, the cache manager checks if the file
        has changed on the server.  If the file has not changed, then
        the cache manager will attempt to obtain a new server lock
        that is sufficient to assert the client side locks for the
        file.  If any of these fail, the lock is marked as LOST.
        Otherwise, it is left as ACTIVE.

      1.2 ACTIVE->DELETED: Lock is released.

   2. WAITLOCK: A lock is in a WAITLOCK state if the cache manager
      grants the lock but the lock is yet to be asserted with the AFS
      file server.  Once the file server grants the lock, the state
      will transition to an ACTIVE lock.

      2.1 WAITLOCK->ACTIVE: The server granted the lock.

      2.2 WAITLOCK->DELETED: Lock is abandoned, or timed out during
        waiting.

      2.3 WAITLOCK->LOST: One or more locks from this client were
        marked as LOST.  No further locks will be granted to this
        client until all lost locks are removed.

   3. WAITUNLOCK: A lock is in a WAITUNLOCK state if the cache manager
      receives a request for a lock that conflicts with an existing
      ACTIVE or WAITLOCK lock.  The lock will be placed in the queue
      and will be granted at such time the conflicting locks are
      removed, at which point the state will transition to either
      WAITLOCK or ACTIVE.

      3.1 WAITUNLOCK->ACTIVE: The conflicting lock was removed.  The
        current serverLock is sufficient to assert this lock, or a
        sufficient serverLock is obtained.

      3.2 WAITUNLOCK->WAITLOCK: The conflicting lock was removed,
        however the required serverLock is yet to be asserted with the
        server.

      3.3 WAITUNLOCK->DELETED: The lock is abandoned, timed out or
        released.

      3.5 WAITUNLOCK->LOST: One or more locks from this client were
        marked as LOST.  No further locks will be granted to this
        client until all lost locks are removed.

   4. LOST: A lock L is LOST if the server lock that was required to
      assert the lock could not be obtained or if it could not be
      extended, or if other locks by the same client were LOST.
      Essentially, once a lock is LOST, the contract between the cache
      manager and that specific client is no longer valid.

      The cache manager rechecks the server lock once every minute and
      extends it as appropriate.  If this is not done for 5 minutes,
      the AFS file server will release the lock (the 5 minute timeout
      is based on current file server code and is fairly arbitrary).
      Once released, the lock cannot be re-obtained without verifying
      that the contents of the file hasn't been modified since the
      time the lock was released.  Re-obtaining the lock without
      verifying this may lead to data corruption.  If the lock can not
      be obtained safely, then all active locks for the cm_scache_t
      are marked as LOST.

      4.1 LOST->DELETED: The lock is released.

   5. DELETED: The lock is no longer relevant.  Eventually, it will
      get removed from the cm_scache_t. In the meantime, it will be
      treated as if it does not exist.

      5.1 DELETED->not exist: The lock is removed from the
        cm_scache_t.

   The following are classifications of locks based on their state.

   6* A lock L is ACCEPTED if it is ACTIVE or WAITLOCK.  These locks
      have been accepted by the cache manager, but may or may not have
      been granted back to the client.

   7* A lock L is QUEUED if it is ACTIVE, WAITLOCK or WAITUNLOCK.

   8* A lock L is WAITING if it is WAITLOCK or WAITUNLOCK.

   Lock operation:

   A client C can READ range (Offset,+Length) of a file represented by
   cm_scache_t S iff (1):

   1. for all _a_ in (Offset,+Length), all of the following is true:

       1.1 For each ACTIVE lock L in S->fileLocks such that _a_ in
         (L->LOffset,+L->LLength); L->key == Key(C) OR L->LockType is
         shared.

       1.2 For each LOST lock L in S->fileLocks such that _a_ in
         (L->LOffset,+L->LLength); L->LockType is shared AND L->key !=
         Key(C)

       (When locks are lost on an cm_scache_t, all locks are lost.  By
       4.2 (below), if there is an exclusive LOST lock, then there
       can't be any overlapping ACTIVE locks.)

   A client C can WRITE range (Offset,+Length) of cm_scache_t S iff (2):

   2. for all _a_ in (Offset,+Length), one of the following is true:

       2.1 Byte _a_ of S is unowned (as specified in 1.1) AND there
         does not exist a LOST lock L such that _a_ in
         (L->LOffset,+L->LLength).

       2.2 Byte _a_ of S is owned by C under lock L (as specified in
         1.2) AND L->LockType is exclusive.

   A client C can OBTAIN a lock L on cm_scache_t S iff (both 3 and 4):

   3. for all _a_ in (L->LOffset,+L->LLength), ALL of the following is
      true:

       3.1 If L->LockType is exclusive then there does NOT exist a
         ACCEPTED lock M in S->fileLocks such that _a_ in
         (M->LOffset,+M->LLength).

         (If we count all QUEUED locks then we hit cases such as
         cascading waiting locks where the locks later on in the queue
         can be granted without compromising file integrity.  On the
         other hand if only ACCEPTED locks are considered, then locks
         that were received earlier may end up waiting for locks that
         were received later to be unlocked. The choice of ACCEPTED
         locks was made to mimic the Windows byte range lock
         semantics.)

       3.2 If L->LockType is shared then for each ACCEPTED lock M in
         S->fileLocks, if _a_ in (M->LOffset,+M->LLength) then
         M->LockType is shared.

   4. For all LOST locks M in S->fileLocks, ALL of the following are true:

       4.1 M->key != Key(C)

       4.2 If M->LockType is exclusive, then (L->LOffset,+L->LLength)
         and (M->LOffset,+M->LLength) do not intersect.

         (Note: If a client loses a lock, it loses all locks.
         Subsequently, it will not be allowed to obtain any more locks
         until all existing LOST locks that belong to the client are
         released.  Once all locks are released by a single client,
         there exists no further contract between the client and AFS
         about the contents of the file, hence the client can then
         proceed to obtain new locks and establish a new contract.

         This doesn't quite work as you think it should, because most
         applications aren't built to deal with losing locks they
         thought they once had.  For now, we don't have a good
         solution to lost locks.

         Also, for consistency reasons, we have to hold off on
         granting locks that overlap exclusive LOST locks.)

   A client C can only unlock locks L in S->fileLocks which have
   L->key == Key(C).

   The representation and invariants are as follows:

   - Each cm_scache_t structure keeps:

       - A queue of byte-range locks (cm_scache_t::fileLocks) which
         are of type cm_file_lock_t.

       - A record of the highest server-side lock that has been
         obtained for this object (cm_scache_t::serverLock), which is
         one of (-1), LockRead, LockWrite.

       - A count of ACCEPTED exclusive and shared locks that are in the
         queue (cm_scache_t::sharedLocks and
         cm_scache_t::exclusiveLocks)

   - Each cm_file_lock_t structure keeps:

       - The type of lock (cm_file_lock_t::LockType)

       - The key associated with the lock (cm_file_lock_t::key)

       - The offset and length of the lock (cm_file_lock_t::LOffset
         and cm_file_lock_t::LLength)

       - The state of the lock.

       - Time of issuance or last successful extension

   Semantic invariants:

       I1. The number of ACCEPTED locks in S->fileLocks are
           (S->sharedLocks + S->exclusiveLocks)

   External invariants:

       I3. S->serverLock is the lock that we have asserted with the
           AFS file server for this cm_scache_t.

       I4. S->serverLock == LockRead iff there is at least one ACTIVE
           shared lock, but no ACTIVE exclusive locks.

       I5. S->serverLock == LockWrite iff there is at least one ACTIVE
           exclusive lock.

       I6. If L is a LOST lock, then for each lock M in S->fileLocks,
           M->key == L->key IMPLIES M is LOST or DELETED.

   --asanka
 */

#define IS_LOCK_ACTIVE(lockp)     (((lockp)->flags & (CM_FILELOCK_FLAG_DELETED|CM_FILELOCK_FLAG_WAITLOCK|CM_FILELOCK_FLAG_WAITUNLOCK|CM_FILELOCK_FLAG_LOST)) == 0)

#define IS_LOCK_WAITLOCK(lockp)   (((lockp)->flags & (CM_FILELOCK_FLAG_DELETED|CM_FILELOCK_FLAG_WAITLOCK|CM_FILELOCK_FLAG_WAITUNLOCK|CM_FILELOCK_FLAG_LOST)) == CM_FILELOCK_FLAG_WAITLOCK)

#define IS_LOCK_WAITUNLOCK(lockp) (((lockp)->flags & (CM_FILELOCK_FLAG_DELETED|CM_FILELOCK_FLAG_WAITLOCK|CM_FILELOCK_FLAG_WAITUNLOCK|CM_FILELOCK_FLAG_LOST)) == CM_FILELOCK_FLAG_WAITUNLOCK)

#define IS_LOCK_LOST(lockp)       (((lockp)->flags & (CM_FILELOCK_FLAG_DELETED|CM_FILELOCK_FLAG_LOST)) == CM_FILELOCK_FLAG_LOST)

#define IS_LOCK_DELETED(lockp)    (((lockp)->flags & CM_FILELOCK_FLAG_DELETED) == CM_FILELOCK_FLAG_DELETED)

/* unsafe */
#define IS_LOCK_ACCEPTED(lockp)   (IS_LOCK_ACTIVE(lockp) || IS_LOCK_WAITLOCK(lockp))

/* unsafe */
#define IS_LOCK_CLIENTONLY(lockp) ((((lockp)->scp->flags & CM_SCACHEFLAG_RO) == CM_SCACHEFLAG_RO) || (((lockp)->flags & CM_FILELOCK_FLAG_CLIENTONLY) == CM_FILELOCK_FLAG_CLIENTONLY))

/* unsafe */
#define INTERSECT_RANGE(r1,r2) (((r2).offset+(r2).length) > (r1).offset && ((r1).offset +(r1).length) > (r2).offset)

/* unsafe */
#define CONTAINS_RANGE(r1,r2) (((r2).offset+(r2).length) <= ((r1).offset+(r1).length) && (r1).offset <= (r2).offset)

#if defined(VICED_CAPABILITY_USE_BYTE_RANGE_LOCKS) && !defined(LOCK_TESTING)
#define SCP_SUPPORTS_BRLOCKS(scp) ((scp)->cbServerp && ((scp)->cbServerp->capabilities & VICED_CAPABILITY_USE_BYTE_RANGE_LOCKS))
#else
#define SCP_SUPPORTS_BRLOCKS(scp) (1)
#endif

#define SERVERLOCKS_ENABLED(scp) (!((scp)->flags & CM_SCACHEFLAG_RO) && cm_enableServerLocks && SCP_SUPPORTS_BRLOCKS(scp))

#if defined(VICED_CAPABILITY_WRITELOCKACL)
#define SCP_SUPPORTS_WRITELOCKACL(scp) ((scp)->cbServerp && ((scp->cbServerp->capabilities & VICED_CAPABILITY_WRITELOCKACL)))
#else
#define SCP_SUPPORTS_WRITELOCKACL(scp) (0)

/* This should really be defined in any build that this code is being
   compiled. */
#error  VICED_CAPABILITY_WRITELOCKACL not defined.
#endif

static void cm_LockRangeSubtract(cm_range_t * pos, const cm_range_t * neg)
{
    afs_int64 int_begin;
    afs_int64 int_end;

    int_begin = MAX(pos->offset, neg->offset);
    int_end = MIN(pos->offset+pos->length, neg->offset+neg->length);

    if (int_begin < int_end) {
        if (int_begin == pos->offset) {
            pos->length = pos->offset + pos->length - int_end;
            pos->offset = int_end;
        } else if (int_end == pos->offset + pos->length) {
            pos->length = int_begin - pos->offset;
        }

        /* We only subtract ranges if the resulting range is
           contiguous.  If we try to support non-contigous ranges, we
           aren't actually improving performance. */
    }
}

/* Called with scp->rw held.  Returns 0 if all is clear to read the
   specified range by the client identified by key.
 */
long cm_LockCheckRead(cm_scache_t *scp,
                      LARGE_INTEGER LOffset,
                      LARGE_INTEGER LLength,
                      cm_key_t key)
{
#ifndef ADVISORY_LOCKS

    cm_file_lock_t *fileLock;
    osi_queue_t *q;
    long code = 0;
    cm_range_t range;
    int substract_ranges = FALSE;

    range.offset = LOffset.QuadPart;
    range.length = LLength.QuadPart;

    /*

     1. for all _a_ in (Offset,+Length), all of the following is true:

       1.1 For each ACTIVE lock L in S->fileLocks such that _a_ in
         (L->LOffset,+L->LLength); L->key == Key(C) OR L->LockType is
         shared.

       1.2 For each LOST lock L in S->fileLocks such that _a_ in
         (L->LOffset,+L->LLength); L->LockType is shared AND L->key !=
         Key(C)

    */

    lock_ObtainRead(&cm_scacheLock);

    for (q = scp->fileLocksH; q && range.length > 0; q = osi_QNext(q)) {
        fileLock =
            (cm_file_lock_t *)((char *) q - offsetof(cm_file_lock_t, fileq));

        if (INTERSECT_RANGE(range, fileLock->range)) {
            if (IS_LOCK_ACTIVE(fileLock)) {
                if (cm_KeyEquals(&fileLock->key, &key, 0)) {

                    /* If there is an active lock for this client, it
                       is safe to substract ranges.*/
                    cm_LockRangeSubtract(&range, &fileLock->range);
                    substract_ranges = TRUE;
                } else {
                    if (fileLock->lockType != LockRead) {
                        code = CM_ERROR_LOCK_CONFLICT;
                        break;
                    }

                    /* even if the entire range is locked for reading,
                       we still can't grant the lock at this point
                       because the client may have lost locks. That
                       is, unless we have already seen an active lock
                       belonging to the client, in which case there
                       can't be any lost locks for this client. */
                    if (substract_ranges)
                        cm_LockRangeSubtract(&range, &fileLock->range);
                }
            } else if (IS_LOCK_LOST(fileLock) &&
                       (cm_KeyEquals(&fileLock->key, &key, 0) || fileLock->lockType == LockWrite)) {
                code = CM_ERROR_BADFD;
                break;
            }
        }
    }

    lock_ReleaseRead(&cm_scacheLock);

    osi_Log4(afsd_logp, "cm_LockCheckRead scp 0x%x offset %d length %d code 0x%x",
	      scp, (unsigned long)LOffset.QuadPart, (unsigned long)LLength.QuadPart, code);

    return code;

#else

    return 0;

#endif
}

/* Called with scp->rw held.  Returns 0 if all is clear to write the
   specified range by the client identified by key.
 */
long cm_LockCheckWrite(cm_scache_t *scp,
                       LARGE_INTEGER LOffset,
                       LARGE_INTEGER LLength,
                       cm_key_t key)
{
#ifndef ADVISORY_LOCKS

    cm_file_lock_t *fileLock;
    osi_queue_t *q;
    long code = 0;
    cm_range_t range;

    range.offset = LOffset.QuadPart;
    range.length = LLength.QuadPart;

    /*
   A client C can WRITE range (Offset,+Length) of cm_scache_t S iff (2):

   2. for all _a_ in (Offset,+Length), one of the following is true:

       2.1 Byte _a_ of S is unowned AND there does not exist a LOST
         lock L such that _a_ in (L->LOffset,+L->LLength).

       2.2 Byte _a_ of S is owned by C under lock L AND L->LockType is
         exclusive.
    */

    lock_ObtainRead(&cm_scacheLock);

    for (q = scp->fileLocksH; q && range.length > 0; q = osi_QNext(q)) {
        fileLock =
            (cm_file_lock_t *)((char *) q - offsetof(cm_file_lock_t, fileq));

        if (INTERSECT_RANGE(range, fileLock->range)) {
            if (IS_LOCK_ACTIVE(fileLock)) {
                if (cm_KeyEquals(&fileLock->key, &key, 0)) {
                    if (fileLock->lockType == LockWrite) {

                        /* if there is an active lock for this client, it
                           is safe to substract ranges */
                        cm_LockRangeSubtract(&range, &fileLock->range);
                    } else {
                        code = CM_ERROR_LOCK_CONFLICT;
                        break;
                    }
                } else {
                    code = CM_ERROR_LOCK_CONFLICT;
                    break;
                }
            } else if (IS_LOCK_LOST(fileLock)) {
                code = CM_ERROR_BADFD;
                break;
            }
        }
    }

    lock_ReleaseRead(&cm_scacheLock);

    osi_Log4(afsd_logp, "cm_LockCheckWrite scp 0x%x offset %d length %d code 0x%x",
	      scp, (unsigned long)LOffset.QuadPart, (unsigned long)LLength.QuadPart, code);

    return code;

#else

    return 0;

#endif
}

/* Called with cm_scacheLock write locked */
static cm_file_lock_t * cm_GetFileLock(void) {
    cm_file_lock_t * l;

    l = (cm_file_lock_t *) cm_freeFileLocks;
    if (l) {
        osi_QRemove(&cm_freeFileLocks, &l->q);
    } else {
        l = malloc(sizeof(cm_file_lock_t));
        osi_assertx(l, "null cm_file_lock_t");
    }

    memset(l, 0, sizeof(cm_file_lock_t));

    return l;
}

/* Called with cm_scacheLock write locked */
static void cm_PutFileLock(cm_file_lock_t *l) {
    osi_QAdd(&cm_freeFileLocks, &l->q);
}

/* called with scp->rw held.  May release it during processing, but
   leaves it held on exit. */
long cm_IntSetLock(cm_scache_t * scp, cm_user_t * userp, int lockType,
                   cm_req_t * reqp) {
    long code = 0;
    AFSFid tfid;
    cm_fid_t cfid;
    cm_conn_t * connp;
    struct rx_connection * rxconnp;
    AFSVolSync volSync;
    afs_uint32 reqflags = reqp->flags;

    memset(&volSync, 0, sizeof(volSync));

    tfid.Volume = scp->fid.volume;
    tfid.Vnode = scp->fid.vnode;
    tfid.Unique = scp->fid.unique;
    cfid = scp->fid;

    osi_Log2(afsd_logp, "CALL SetLock scp 0x%p for lock %d", scp, lockType);

    reqp->flags |= CM_REQ_NORETRY;
    lock_ReleaseWrite(&scp->rw);

    do {
        code = cm_ConnFromFID(&cfid, userp, reqp, &connp);
        if (code)
            break;

        rxconnp = cm_GetRxConn(connp);
        code = RXAFS_SetLock(rxconnp, &tfid, lockType,
                             &volSync);
        rx_PutConnection(rxconnp);

    } while (cm_Analyze(connp, userp, reqp, &cfid, 1, &volSync,
                        NULL, NULL, code));

    code = cm_MapRPCError(code, reqp);
    if (code) {
        osi_Log1(afsd_logp, "CALL SetLock FAILURE, code 0x%x", code);
    } else {
        osi_Log0(afsd_logp, "CALL SetLock SUCCESS");
    }

    lock_ObtainWrite(&scp->rw);
    reqp->flags = reqflags;
    return code;
}

/* called with scp->rw held.  Releases it during processing */
long cm_IntReleaseLock(cm_scache_t * scp, cm_user_t * userp,
                       cm_req_t * reqp) {
    long code = 0;
    AFSFid tfid;
    cm_fid_t cfid;
    cm_conn_t * connp;
    struct rx_connection * rxconnp;
    AFSVolSync volSync;

    if (scp->flags & CM_SCACHEFLAG_DELETED) {
        osi_Log1(afsd_logp, "CALL ReleaseLock on Deleted Vnode scp 0x%p", scp);
        return 0;
    }

    memset(&volSync, 0, sizeof(volSync));

    tfid.Volume = scp->fid.volume;
    tfid.Vnode = scp->fid.vnode;
    tfid.Unique = scp->fid.unique;
    cfid = scp->fid;

    lock_ReleaseWrite(&scp->rw);

    osi_Log1(afsd_logp, "CALL ReleaseLock scp 0x%p", scp);

    do {
        code = cm_ConnFromFID(&cfid, userp, reqp, &connp);
        if (code)
            break;

        rxconnp = cm_GetRxConn(connp);
        code = RXAFS_ReleaseLock(rxconnp, &tfid, &volSync);
        rx_PutConnection(rxconnp);

    } while (cm_Analyze(connp, userp, reqp, &cfid, 1, &volSync,
                        NULL, NULL, code));
    code = cm_MapRPCError(code, reqp);
    if (code)
        osi_Log1(afsd_logp,
                 "CALL ReleaseLock FAILURE, code 0x%x", code);
    else
        osi_Log0(afsd_logp,
                 "CALL ReleaseLock SUCCESS");

    lock_ObtainWrite(&scp->rw);

    return (code != CM_ERROR_BADFD ? code : 0);
}

/* called with scp->rw held.  May release it during processing, but
   will exit with lock held.

   This will return:

   - 0 if the user has permission to get the specified lock for the scp

   - CM_ERROR_NOACCESS if not

   Any other error from cm_SyncOp will be sent down untranslated.

   If CM_ERROR_NOACCESS is returned and lock_type is LockRead, then
   phas_insert (if non-NULL) will receive a boolean value indicating
   whether the user has INSERT permission or not.
*/
long cm_LockCheckPerms(cm_scache_t * scp,
                       int lock_type,
                       cm_user_t * userp,
                       cm_req_t * reqp,
                       int * phas_insert)
{
    long rights = 0;
    long code = 0, code2 = 0;

    /* lock permissions are slightly tricky because of the 'i' bit.
       If the user has PRSFS_LOCK, she can read-lock the file.  If the
       user has PRSFS_WRITE, she can write-lock the file.  However, if
       the user has PRSFS_INSERT, then she can write-lock new files,
       but not old ones.  Since we don't have information about
       whether a file is new or not, we assume that if the user owns
       the scp, then she has the permissions that are granted by
       PRSFS_INSERT. */

    osi_Log3(afsd_logp, "cm_LockCheckPerms for scp[0x%p] type[%d] user[0x%p]",
             scp, lock_type, userp);

    if (lock_type == LockRead)
        rights |= PRSFS_LOCK;
    else if (lock_type == LockWrite)
        rights |= PRSFS_WRITE | PRSFS_LOCK;
    else {
        /* hmmkay */
        osi_assertx(FALSE, "invalid lock type");
        return 0;
    }

    if (phas_insert)
        *phas_insert = FALSE;

    code = cm_SyncOp(scp, NULL, userp, reqp, rights,
                     CM_SCACHESYNC_GETSTATUS |
                     CM_SCACHESYNC_NEEDCALLBACK);

    if (phas_insert && scp->creator == userp) {

        /* If this file was created by the user, then we check for
           PRSFS_INSERT.  If the file server is recent enough, then
           this should be sufficient for her to get a write-lock (but
           not necessarily a read-lock). VICED_CAPABILITY_WRITELOCKACL
           indicates whether a file server supports getting write
           locks when the user only has PRSFS_INSERT.

           If the file was not created by the user we skip the check
           because the INSERT bit will not apply to this user even
           if it is set.
         */

        code2 = cm_SyncOp(scp, NULL, userp, reqp, PRSFS_INSERT,
                         CM_SCACHESYNC_GETSTATUS |
                         CM_SCACHESYNC_NEEDCALLBACK);

	if (code2 == CM_ERROR_NOACCESS) {
	    osi_Log0(afsd_logp, "cm_LockCheckPerms user has no INSERT bits");
        } else {
            *phas_insert = TRUE;
            osi_Log0(afsd_logp, "cm_LockCheckPerms user has INSERT bits");
        }
    }

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

    osi_Log1(afsd_logp, "cm_LockCheckPerms returning code %d", code);

    return code;
}

/* called with scp->rw held */
long cm_Lock(cm_scache_t *scp, unsigned char sLockType,
             LARGE_INTEGER LOffset, LARGE_INTEGER LLength,
             cm_key_t key,
             int allowWait, cm_user_t *userp, cm_req_t *reqp,
             cm_file_lock_t **lockpp)
{
    long code = 0;
    int Which = ((sLockType & LOCKING_ANDX_SHARED_LOCK) ? LockRead : LockWrite);
    cm_file_lock_t *fileLock;
    osi_queue_t *q;
    cm_range_t range;
    int wait_unlock = FALSE;
    int force_client_lock = FALSE;

    osi_Log4(afsd_logp, "cm_Lock scp 0x%x type 0x%x offset %d length %d",
             scp, sLockType, (unsigned long)LOffset.QuadPart, (unsigned long)LLength.QuadPart);
    osi_Log4(afsd_logp, "... allowWait %d key <0x%x, 0x%x, 0x%x>", allowWait,
             key.process_id, key.session_id, key.file_id);

    /*
   A client C can OBTAIN a lock L on cm_scache_t S iff (both 3 and 4):

   3. for all _a_ in (L->LOffset,+L->LLength), ALL of the following is
      true:

       3.1 If L->LockType is exclusive then there does NOT exist a
         ACCEPTED lock M in S->fileLocks such that _a_ in
         (M->LOffset,+M->LLength).

       3.2 If L->LockType is shared then for each ACCEPTED lock M in
         S->fileLocks, if _a_ in (M->LOffset,+M->LLength) then
         M->LockType is shared.

   4. For all LOST locks M in S->fileLocks, ALL of the following are true:

       4.1 M->key != Key(C)

       4.2 If M->LockType is exclusive, then (L->LOffset,+L->LLength)
         and (M->LOffset,+M->LLength) do not intersect.
    */

    range.offset = LOffset.QuadPart;
    range.length = LLength.QuadPart;

    lock_ObtainRead(&cm_scacheLock);

    for (q = scp->fileLocksH; q; q = osi_QNext(q)) {
        fileLock =
            (cm_file_lock_t *)((char *) q - offsetof(cm_file_lock_t, fileq));

        if (IS_LOCK_LOST(fileLock)) {
            if (cm_KeyEquals(&fileLock->key, &key, 0)) {
                code = CM_ERROR_BADFD;
                break;
            } else if (fileLock->lockType == LockWrite && INTERSECT_RANGE(range, fileLock->range)) {
                code = CM_ERROR_WOULDBLOCK;
                wait_unlock = TRUE;
                break;
            }
        }

        /* we don't need to check for deleted locks here since deleted
           locks are dequeued from scp->fileLocks */
        if (IS_LOCK_ACCEPTED(fileLock) &&
           INTERSECT_RANGE(range, fileLock->range)) {

            if ((sLockType & LOCKING_ANDX_SHARED_LOCK) == 0 ||
                fileLock->lockType != LockRead) {
                wait_unlock = TRUE;
                code = CM_ERROR_WOULDBLOCK;
                break;
            }
        }
    }

    lock_ReleaseRead(&cm_scacheLock);

    if (code == 0 && SERVERLOCKS_ENABLED(scp)) {
        if (Which == scp->serverLock ||
           (Which == LockRead && scp->serverLock == LockWrite)) {

            int has_insert = 0;

            /* we already have the lock we need */
            osi_Log3(afsd_logp, "   we already have the correct lock. exclusives[%d], shared[%d], serverLock[%d]",
                     scp->exclusiveLocks, scp->sharedLocks, (int)(signed char) scp->serverLock);

            code = cm_LockCheckPerms(scp, Which, userp, reqp, &has_insert);

            /* special case: if we don't have permission to read-lock
               the file, then we force a clientside lock.  This is to
               compensate for applications that obtain a read-lock for
               reading files off of directories that don't grant
               read-locks to the user. */
            if (code == CM_ERROR_NOACCESS && Which == LockRead) {

                if (has_insert && SCP_SUPPORTS_WRITELOCKACL(scp)) {
                    osi_Log0(afsd_logp, "   User has no read-lock perms, but has INSERT perms.");
                    code = 0;
                } else {
                    osi_Log0(afsd_logp, "   User has no read-lock perms. Forcing client-side lock");
                    force_client_lock = TRUE;
                }
            }

        } else if ((scp->exclusiveLocks > 0) ||
                   (scp->sharedLocks > 0 && scp->serverLock != LockRead)) {
            int has_insert = 0;

            /* We are already waiting for some other lock.  We should
               wait for the daemon to catch up instead of generating a
               flood of SetLock calls. */
            osi_Log3(afsd_logp, "   already waiting for other lock. exclusives[%d], shared[%d], serverLock[%d]",
                     scp->exclusiveLocks, scp->sharedLocks, (int)(signed char) scp->serverLock);

            /* see if we have permission to create the lock in the
               first place. */
            code = cm_LockCheckPerms(scp, Which, userp, reqp, &has_insert);
            if (code == 0)
		code = CM_ERROR_WOULDBLOCK;
            else if (code == CM_ERROR_NOACCESS && Which == LockRead) {

                if (has_insert && SCP_SUPPORTS_WRITELOCKACL(scp)) {
                    osi_Log0(afsd_logp,
                             "   User has no read-lock perms, but has INSERT perms.");
                    code = CM_ERROR_WOULDBLOCK;
                } else {
                    osi_Log0(afsd_logp,
                             "   User has no read-lock perms. Forcing client-side lock");
                    force_client_lock = TRUE;
                }
            }

            /* leave any other codes as-is */

        } else {
            int newLock;
            int check_data_version = FALSE;
            int has_insert = 0;

            /* first check if we have permission to elevate or obtain
               the lock. */
            code = cm_LockCheckPerms(scp, Which, userp, reqp, &has_insert);
            if (code) {
                if (code == CM_ERROR_NOACCESS && Which == LockRead &&
                    (!has_insert || !SCP_SUPPORTS_WRITELOCKACL(scp))) {
                    osi_Log0(afsd_logp, "   User has no read-lock perms.  Forcing client-side lock");
                    force_client_lock = TRUE;
                }
                goto check_code;
            }

            /* has_insert => (Which == LockRead, code == CM_ERROR_NOACCESS) */

            if (scp->serverLock == LockRead && Which == LockWrite) {

                /* We want to escalate the lock to a LockWrite.
                 * Unfortunately that's not really possible without
                 * letting go of the current lock.  But for now we do
                 * it anyway. */

                osi_Log0(afsd_logp,
                         "   attempting to UPGRADE from LockRead to LockWrite.");
                osi_Log1(afsd_logp,
                         "   dataVersion on scp: %I64d", scp->dataVersion);

                /* we assume at this point (because scp->serverLock
                   was valid) that we had a valid server lock. */
                scp->lockDataVersion = scp->dataVersion;
                check_data_version = TRUE;

                code = cm_IntReleaseLock(scp, userp, reqp);

                if (code) {
                    /* We couldn't release the lock */
                    goto check_code;
                } else {
                    scp->serverLock = -1;
                }
            }

            /* We need to obtain a server lock of type Which in order
             * to assert this file lock */
#ifndef AGGRESSIVE_LOCKS
            newLock = Which;
#else
            newLock = LockWrite;
#endif

            code = cm_IntSetLock(scp, userp, newLock, reqp);

#ifdef AGGRESSIVE_LOCKS
            if ((code == CM_ERROR_WOULDBLOCK ||
                 code == CM_ERROR_NOACCESS) && newLock != Which) {
                /* we wanted LockRead.  We tried LockWrite. Now try
                 * LockRead again */
                newLock = Which;

                /* am I sane? */
                osi_assertx(newLock == LockRead, "lock type not read");

                code = cm_IntSetLock(scp, userp, newLock, reqp);
            }
#endif

            if (code == CM_ERROR_NOACCESS) {
                if (Which == LockRead) {
                    if (has_insert && SCP_SUPPORTS_WRITELOCKACL(scp)) {
                        long tcode;
                        /* We requested a read-lock, but we have permission to
                         * get a write-lock. Try that */

                        tcode = cm_LockCheckPerms(scp, LockWrite, userp, reqp, NULL);

                        if (tcode == 0) {
                            newLock = LockWrite;

                            osi_Log0(afsd_logp, "   User has 'i' perms and the request was for a LockRead.  Trying to get a LockWrite instead");

                            code = cm_IntSetLock(scp, userp, newLock, reqp);
                        }
                    } else {
                        osi_Log0(afsd_logp, "   User has no read-lock perms.  Forcing client-side lock");
                        force_client_lock = TRUE;
                    }
                } else if (Which == LockWrite &&
                           scp->creator == userp && !SCP_SUPPORTS_WRITELOCKACL(scp)) {
                    long tcode;

                    /* Special case: if the lock request was for a
                     * LockWrite and the user owns the file and we weren't
                     * allowed to obtain the serverlock, we either lost a
                     * race (the permissions changed from under us), or we
                     * have 'i' bits, but we aren't allowed to lock the
                     * file. */

                    /* check if we lost a race... */
                    tcode = cm_LockCheckPerms(scp, Which, userp, reqp, NULL);

                    if (tcode == 0) {
                        osi_Log0(afsd_logp, "   User has 'i' perms but can't obtain write locks. Using client-side locks.");
                        force_client_lock = TRUE;
                    }
                }
            }

            if (code == 0 && check_data_version &&
               scp->dataVersion != scp->lockDataVersion) {
                /* We lost a race.  Although we successfully obtained
                 * a lock, someone modified the file in between.  The
                 * locks have all been technically lost. */

                osi_Log0(afsd_logp,
                         "  Data version mismatch while upgrading lock.");
                osi_Log2(afsd_logp,
                         "  Data versions before=%I64d, after=%I64d",
                         scp->lockDataVersion,
                         scp->dataVersion);
                osi_Log1(afsd_logp,
                         "  Releasing stale lock for scp 0x%x", scp);

                code = cm_IntReleaseLock(scp, userp, reqp);

                scp->serverLock = -1;

                code = CM_ERROR_INVAL;
            } else if (code == 0) {
                scp->serverLock = newLock;
                scp->lockDataVersion = scp->dataVersion;
            }

            if (code != 0 &&
                (scp->sharedLocks > 0 || scp->exclusiveLocks > 0) &&
                scp->serverLock == -1) {
                /* Oops. We lost the lock. */
                cm_LockMarkSCacheLost(scp);
            }
        }
    } else if (code == 0) {     /* server locks not enabled */
        osi_Log0(afsd_logp,
                 "  Skipping server lock for scp");
    }

 check_code:

    if (code != 0 && !force_client_lock) {
        /* Special case error translations

           Applications don't expect certain errors from a
           LockFile/UnlockFile call.  We need to translate some error
           code to codes that apps expect and handle. */

        /* We shouldn't actually need to handle this case since we
           simulate locks for RO scps anyway. */
        if (code == CM_ERROR_READONLY) {
            osi_Log0(afsd_logp, "   Reinterpreting CM_ERROR_READONLY as CM_ERROR_NOACCESS");
            code = CM_ERROR_NOACCESS;
        }
    }

    if (code == 0 || (code == CM_ERROR_WOULDBLOCK && allowWait) ||
        force_client_lock) {

        /* clear the error if we are forcing a client lock, so we
           don't get confused later. */
        if (force_client_lock && code != CM_ERROR_WOULDBLOCK)
            code = 0;

        lock_ObtainWrite(&cm_scacheLock);
        fileLock = cm_GetFileLock();
        lock_ReleaseWrite(&cm_scacheLock);
#ifdef DEBUG
        fileLock->fid = scp->fid;
#endif
        fileLock->key = key;
        fileLock->lockType = Which;
        cm_HoldUser(userp);
        fileLock->userp = userp;
        fileLock->range = range;
        fileLock->flags = (code == 0 ? 0 :
                           ((wait_unlock)?
                            CM_FILELOCK_FLAG_WAITUNLOCK :
                            CM_FILELOCK_FLAG_WAITLOCK));

        if (force_client_lock || !SERVERLOCKS_ENABLED(scp))
            fileLock->flags |= CM_FILELOCK_FLAG_CLIENTONLY;

        fileLock->lastUpdate = (code == 0 && !force_client_lock) ? time(NULL) : 0;

        lock_ObtainWrite(&cm_scacheLock);
        osi_QAddT(&scp->fileLocksH, &scp->fileLocksT, &fileLock->fileq);
        cm_HoldSCacheNoLock(scp);
        fileLock->scp = scp;
        osi_QAdd(&cm_allFileLocks, &fileLock->q);
        lock_ReleaseWrite(&cm_scacheLock);

        if (code != 0) {
            *lockpp = fileLock;
        }

        if (IS_LOCK_CLIENTONLY(fileLock)) {
            scp->clientLocks++;
        } else if (IS_LOCK_ACCEPTED(fileLock)) {
            if (Which == LockRead)
                scp->sharedLocks++;
            else
                scp->exclusiveLocks++;
        }

        osi_Log3(afsd_logp,
                 "cm_Lock Lock added 0x%p flags 0x%x to scp [0x%p]",
                 fileLock, fileLock->flags, scp);
        osi_Log4(afsd_logp,
                 "   exclusives[%d] shared[%d] client[%d] serverLock[%d]",
                 scp->exclusiveLocks, scp->sharedLocks, scp->clientLocks,
                 (int)(signed char) scp->serverLock);
    } else {
        osi_Log1(afsd_logp,
                 "cm_Lock Rejecting lock (code = 0x%x)", code);
    }

    /* Convert from would block to lock not granted */
    if (code == CM_ERROR_WOULDBLOCK)
        code = CM_ERROR_LOCK_NOT_GRANTED;

    return code;
}

static long
cm_IntUnlock(cm_scache_t * scp,
             cm_user_t * userp,
             cm_req_t *  reqp)
{
    long code = 0;

    osi_assertx(scp->sharedLocks >= 0, "scp->sharedLocks < 0");
    osi_assertx(scp->exclusiveLocks >= 0, "scp->exclusiveLocks < 0");
    osi_assertx(scp->clientLocks >= 0, "scp->clientLocks < 0");

    if (!SERVERLOCKS_ENABLED(scp)) {
        osi_Log0(afsd_logp, "  Skipping server lock for scp");
        goto done;
    }

    /* Ideally we would go through the rest of the locks to determine
     * if one or more locks that were formerly in WAITUNLOCK can now
     * be put to ACTIVE or WAITLOCK and update scp->exclusiveLocks and
     * scp->sharedLocks accordingly.  However, the retrying of locks
     * in that manner is done cm_RetryLock() manually.
     */

    if (scp->serverLock == LockWrite &&
        scp->exclusiveLocks == 0 &&
        scp->sharedLocks > 0) {
        /* The serverLock should be downgraded to LockRead */
        osi_Log0(afsd_logp, "  DOWNGRADE lock from LockWrite to LockRead");

        /* Make sure there are no dirty buffers left. */
        code = cm_FSync(scp, userp, reqp, TRUE);

        /* since scp->serverLock looked sane, we are going to assume
           that we have a valid server lock. */
        scp->lockDataVersion = scp->dataVersion;
        osi_Log1(afsd_logp, "  dataVersion on scp = %I64d", scp->dataVersion);

        /* before we downgrade, make sure that we have enough
           permissions to get the read lock. */
        code = cm_LockCheckPerms(scp, LockRead, userp, reqp, NULL);
        if (code != 0) {

            osi_Log0(afsd_logp, "  SKIPPING downgrade because user doesn't have perms to get downgraded lock");

            code = 0;
            goto done;
        }

        code = cm_IntReleaseLock(scp, userp, reqp);

        if (code) {
            /* so we couldn't release it.  Just let the lock be for now */
            code = 0;
            goto done;
        } else {
            scp->serverLock = -1;
        }

        code = cm_IntSetLock(scp, userp, LockRead, reqp);

        if (code == 0 && scp->lockDataVersion == scp->dataVersion) {
            scp->serverLock = LockRead;
        } else if (code == 0 && scp->lockDataVersion != scp->dataVersion) {
            /* We lost a race condition.  Although we have a valid
               lock on the file, the data has changed and essentially
               we have lost the lock we had during the transition. */

            osi_Log0(afsd_logp, "Data version mismatch during lock downgrade");
            osi_Log2(afsd_logp, "  Data versions before=%I64d, after=%I64d",
                     scp->lockDataVersion,
                     scp->dataVersion);

            code = cm_IntReleaseLock(scp, userp, reqp);

            code = CM_ERROR_INVAL;
            scp->serverLock = -1;
        }

        if (code != 0 &&
            (scp->sharedLocks > 0 || scp->exclusiveLocks > 0) &&
                (scp->serverLock == -1)) {
                /* Oopsie */
                cm_LockMarkSCacheLost(scp);
            }

        /* failure here has no bearing on the return value of cm_Unlock() */
        code = 0;

    } else if (scp->serverLock != (-1) &&
              scp->exclusiveLocks == 0 &&
              scp->sharedLocks == 0) {
        /* The serverLock should be released entirely */

        if (scp->serverLock == LockWrite) {
            osi_Log0(afsd_logp, "  RELEASE LockWrite -> LockNone");

            /* Make sure there are no dirty buffers left. */
            code = cm_FSync(scp, userp, reqp, TRUE);
        } else {
            osi_Log0(afsd_logp, "  RELEASE LockRead -> LockNone");
        }

        code = cm_IntReleaseLock(scp, userp, reqp);

        if (code == 0)
            scp->serverLock = (-1);
    }

  done:
    return code;
}
/* Called with scp->rw held */
long cm_UnlockByKey(cm_scache_t * scp,
		    cm_key_t key,
		    afs_uint32 flags,
		    cm_user_t * userp,
		    cm_req_t * reqp)
{
    long code = 0;
    cm_file_lock_t *fileLock;
    osi_queue_t *q, *qn;
    int n_unlocks = 0;

    osi_Log4(afsd_logp, "cm_UnlockByKey scp 0x%p key <0x%x,0x%x,0x%x",
             scp, key.process_id, key.session_id, key.file_id);
    osi_Log1(afsd_logp, "    flags=0x%x", flags);

    lock_ObtainWrite(&cm_scacheLock);

    for (q = scp->fileLocksH; q; q = qn) {
        qn = osi_QNext(q);

        fileLock = (cm_file_lock_t *)
            ((char *) q - offsetof(cm_file_lock_t, fileq));

#ifdef DEBUG
        osi_Log4(afsd_logp, "   Checking lock[0x%x] range[%d,+%d] type[%d]",
                 fileLock,
                 (unsigned long) fileLock->range.offset,
                 (unsigned long) fileLock->range.length,
                fileLock->lockType);
        osi_Log4(afsd_logp, "     key<0x%x, 0x%x, 0x%x> flags[0x%x]",
                 fileLock->key.process_id, fileLock->key.session_id, fileLock->key.file_id,
                 fileLock->flags);

        if (cm_FidCmp(&fileLock->fid, &fileLock->scp->fid)) {
            osi_Log0(afsd_logp, "!!fileLock->fid != scp->fid");
            osi_Log4(afsd_logp, "  fileLock->fid(cell=[%d], volume=[%d], vnode=[%d], unique=[%d]",
                     fileLock->fid.cell,
                     fileLock->fid.volume,
                     fileLock->fid.vnode,
                     fileLock->fid.unique);
            osi_Log4(afsd_logp, "  scp->fid(cell=[%d], volume=[%d], vnode=[%d], unique=[%d]",
                     fileLock->scp->fid.cell,
                     fileLock->scp->fid.volume,
                     fileLock->scp->fid.vnode,
                     fileLock->scp->fid.unique);
            osi_assertx(FALSE, "invalid fid value");
        }
#endif

        if (!IS_LOCK_DELETED(fileLock) &&
            cm_KeyEquals(&fileLock->key, &key, flags)) {
            osi_Log3(afsd_logp, "...Unlock range [%d,+%d] type %d",
                    fileLock->range.offset,
                    fileLock->range.length,
                    fileLock->lockType);

            if (scp->fileLocksT == q)
                scp->fileLocksT = osi_QPrev(q);
            osi_QRemoveHT(&scp->fileLocksH, &scp->fileLocksT, q);

            if (IS_LOCK_CLIENTONLY(fileLock)) {
                scp->clientLocks--;
            } else if (IS_LOCK_ACCEPTED(fileLock)) {
                if (fileLock->lockType == LockRead)
                    scp->sharedLocks--;
                else
                    scp->exclusiveLocks--;
            }

            fileLock->flags |= CM_FILELOCK_FLAG_DELETED;

            cm_ReleaseUser(fileLock->userp);
            cm_ReleaseSCacheNoLock(scp);

            fileLock->userp = NULL;
            fileLock->scp = NULL;

            n_unlocks++;
        }
    }

    lock_ReleaseWrite(&cm_scacheLock);

    if (n_unlocks == 0) {
        osi_Log0(afsd_logp, "cm_UnlockByKey no locks found");
        osi_Log3(afsd_logp, "   Leaving scp with exclusives[%d], shared[%d], serverLock[%d]",
                 scp->exclusiveLocks, scp->sharedLocks, (int)(signed char) scp->serverLock);

        return 0;
    }

    osi_Log1(afsd_logp, "cm_UnlockByKey done with %d locks", n_unlocks);

    code = cm_IntUnlock(scp, userp, reqp);

    osi_Log1(afsd_logp, "cm_UnlockByKey code 0x%x", code);
    osi_Log4(afsd_logp, "   Leaving scp with excl[%d], shared[%d], client[%d], serverLock[%d]",
             scp->exclusiveLocks, scp->sharedLocks, scp->clientLocks,
             (int)(signed char) scp->serverLock);

    return code;
}

long cm_Unlock(cm_scache_t *scp,
               unsigned char sLockType,
               LARGE_INTEGER LOffset, LARGE_INTEGER LLength,
               cm_key_t key,
               afs_uint32 flags,
               cm_user_t *userp,
               cm_req_t *reqp)
{
    long code = 0;
    int Which = ((sLockType & LOCKING_ANDX_SHARED_LOCK) ? LockRead : LockWrite);
    cm_file_lock_t *fileLock;
    osi_queue_t *q;
    int release_userp = FALSE;
    int exact_match = !(flags & CM_UNLOCK_FLAG_MATCH_RANGE);
    int lock_found  = 0;
    LARGE_INTEGER RangeEnd;

    osi_Log4(afsd_logp, "cm_Unlock scp 0x%p type 0x%x offset 0x%x length 0x%x",
             scp, sLockType, (unsigned long)LOffset.QuadPart, (unsigned long)LLength.QuadPart);
    osi_Log4(afsd_logp, "... key <0x%x,0x%x,0x%x> flags 0x%x",
             key.process_id, key.session_id, key.file_id, flags);

    if (!exact_match)
        RangeEnd.QuadPart = LOffset.QuadPart + LLength.QuadPart;

  try_again:
    lock_ObtainRead(&cm_scacheLock);

    for (q = scp->fileLocksH; q; q = osi_QNext(q)) {
        fileLock = (cm_file_lock_t *)
            ((char *) q - offsetof(cm_file_lock_t, fileq));

#ifdef DEBUG
        if (cm_FidCmp(&fileLock->fid, &fileLock->scp->fid)) {
            osi_Log0(afsd_logp, "!!fileLock->fid != scp->fid");
            osi_Log4(afsd_logp, "  fileLock->fid(cell=[%d], volume=[%d], vnode=[%d], unique=[%d]",
                     fileLock->fid.cell,
                     fileLock->fid.volume,
                     fileLock->fid.vnode,
                     fileLock->fid.unique);
            osi_Log4(afsd_logp, "  scp->fid(cell=[%d], volume=[%d], vnode=[%d], unique=[%d]",
                     fileLock->scp->fid.cell,
                     fileLock->scp->fid.volume,
                     fileLock->scp->fid.vnode,
                     fileLock->scp->fid.unique);
            osi_assertx(FALSE, "invalid fid value");
        }
#endif
        if (exact_match) {
            if (!IS_LOCK_DELETED(fileLock) &&
                 cm_KeyEquals(&fileLock->key, &key, 0) &&
                 fileLock->range.offset == LOffset.QuadPart &&
                 fileLock->range.length == LLength.QuadPart) {
                lock_found = 1;
                break;
            }
        } else {

            if (!IS_LOCK_DELETED(fileLock) &&
                 cm_KeyEquals(&fileLock->key, &key, 0) &&
                 fileLock->range.offset >= LOffset.QuadPart &&
                 fileLock->range.offset < RangeEnd.QuadPart &&
                 (fileLock->range.offset + fileLock->range.length) <= RangeEnd.QuadPart) {
                lock_found = 1;
                break;
            }
        }
    }

    if (!q) {
        lock_ReleaseRead(&cm_scacheLock);

        if (lock_found && !exact_match) {
            code = 0;
            goto done;
        } else {
            osi_Log0(afsd_logp, "cm_Unlock lock not found; failure");

            /* The lock didn't exist anyway. *shrug* */
            return CM_ERROR_RANGE_NOT_LOCKED;
        }
    }

    /* discard lock record */
    lock_ConvertRToW(&cm_scacheLock);
    if (scp->fileLocksT == q)
        scp->fileLocksT = osi_QPrev(q);
    osi_QRemoveHT(&scp->fileLocksH, &scp->fileLocksT, q);

    /*
     * Don't delete it here; let the daemon delete it, to simplify
     * the daemon's traversal of the list.
     */

    if (IS_LOCK_CLIENTONLY(fileLock)) {
        scp->clientLocks--;
    } else if (IS_LOCK_ACCEPTED(fileLock)) {
        if (fileLock->lockType == LockRead)
            scp->sharedLocks--;
        else
            scp->exclusiveLocks--;
    }

    fileLock->flags |= CM_FILELOCK_FLAG_DELETED;
    if (userp != NULL) {
        cm_ReleaseUser(fileLock->userp);
    } else {
        userp = fileLock->userp;
        release_userp = TRUE;
    }
    fileLock->userp = NULL;
    cm_ReleaseSCacheNoLock(scp);
    fileLock->scp = NULL;
    lock_ReleaseWrite(&cm_scacheLock);

    code = cm_IntUnlock(scp, userp, reqp);

    if (release_userp) {
        cm_ReleaseUser(userp);
        release_userp = FALSE;
    }

    if (!exact_match) {
        osi_Log1(afsd_logp, "cm_Unlock not exact match, searching for next lock, code 0x%x", code);
        goto try_again;         /* might be more than one lock in the range */
    }

 done:

    osi_Log1(afsd_logp, "cm_Unlock code 0x%x", code);
    osi_Log4(afsd_logp, "  leaving scp with excl[%d], shared[%d], client[%d], serverLock[%d]",
             scp->exclusiveLocks, scp->sharedLocks, scp->clientLocks,
             (int)(signed char) scp->serverLock);

    return code;
}

/* called with scp->rw held */
void cm_LockMarkSCacheLost(cm_scache_t * scp)
{
    cm_file_lock_t *fileLock;
    osi_queue_t *q;

    osi_Log1(afsd_logp, "cm_LockMarkSCacheLost scp 0x%x", scp);

    /* cm_scacheLock needed because we are modifying fileLock->flags */
    lock_ObtainWrite(&cm_scacheLock);

    for (q = scp->fileLocksH; q; q = osi_QNext(q)) {
        fileLock =
            (cm_file_lock_t *)((char *) q - offsetof(cm_file_lock_t, fileq));

        if (IS_LOCK_ACTIVE(fileLock) &&
            !IS_LOCK_CLIENTONLY(fileLock)) {
            if (fileLock->lockType == LockRead)
                scp->sharedLocks--;
            else
                scp->exclusiveLocks--;

            fileLock->flags |= CM_FILELOCK_FLAG_LOST;
        }
    }

    scp->serverLock = -1;
    scp->lockDataVersion = CM_SCACHE_VERSION_BAD;
    lock_ReleaseWrite(&cm_scacheLock);
}

/* Called with no relevant locks held */
void cm_CheckLocks()
{
    osi_queue_t *q, *nq;
    cm_file_lock_t *fileLock;
    cm_req_t req;
    AFSFid tfid;
    AFSVolSync volSync;
    cm_conn_t *connp;
    long code;
    struct rx_connection * rxconnp;
    cm_scache_t * scp;

    memset(&volSync, 0, sizeof(volSync));

    cm_InitReq(&req);

    lock_ObtainWrite(&cm_scacheLock);

    cm_lockRefreshCycle++;

    osi_Log1(afsd_logp, "cm_CheckLocks starting lock check cycle %d", cm_lockRefreshCycle);

    for (q = cm_allFileLocks; q; q = nq) {
        fileLock = (cm_file_lock_t *) q;
        nq = osi_QNext(q);
	code = -1;

        if (IS_LOCK_DELETED(fileLock)) {

            osi_QRemove(&cm_allFileLocks, q);
            cm_PutFileLock(fileLock);

        } else if (IS_LOCK_ACTIVE(fileLock) && !IS_LOCK_CLIENTONLY(fileLock)) {

            /* Server locks must have been enabled for us to have
               received an active non-client-only lock. */
            osi_assertx(cm_enableServerLocks, "!cm_enableServerLocks");

            scp = fileLock->scp;
            osi_assertx(scp != NULL, "null cm_scache_t");

            cm_HoldSCacheNoLock(scp);

#ifdef DEBUG
            if (cm_FidCmp(&fileLock->fid, &fileLock->scp->fid)) {
                osi_Log0(afsd_logp, "!!fileLock->fid != scp->fid");
                osi_Log4(afsd_logp, "  fileLock->fid(cell=[%d], volume=[%d], vnode=[%d], unique=[%d]",
                         fileLock->fid.cell,
                         fileLock->fid.volume,
                         fileLock->fid.vnode,
                         fileLock->fid.unique);
                osi_Log4(afsd_logp, "  scp->fid(cell=[%d], volume=[%d], vnode=[%d], unique=[%d]",
                         fileLock->scp->fid.cell,
                         fileLock->scp->fid.volume,
                         fileLock->scp->fid.vnode,
                         fileLock->scp->fid.unique);
                osi_assertx(FALSE, "invalid fid");
            }
#endif
            /* Server locks are extended once per scp per refresh
               cycle. */
            if (scp->lastRefreshCycle != cm_lockRefreshCycle) {

                int scp_done = FALSE;

                osi_Log1(afsd_logp, "cm_CheckLocks Updating scp 0x%x", scp);

                lock_ReleaseWrite(&cm_scacheLock);
                lock_ObtainWrite(&scp->rw);

                /* did the lock change while we weren't holding the lock? */
                if (!IS_LOCK_ACTIVE(fileLock))
                    goto post_syncopdone;

                code = cm_SyncOp(scp, NULL, fileLock->userp, &req, 0,
                                 CM_SCACHESYNC_NEEDCALLBACK
                                 | CM_SCACHESYNC_GETSTATUS
                                 | CM_SCACHESYNC_LOCK);

                if (code) {
                    osi_Log1(afsd_logp,
                             "cm_CheckLocks SyncOp failure code 0x%x", code);
                    goto post_syncopdone;
                }

                /* cm_SyncOp releases scp->rw during which the lock
                   may get released. */
                if (!IS_LOCK_ACTIVE(fileLock))
                    goto pre_syncopdone;

                if (scp->serverLock != -1 && !(scp->flags & CM_SCACHEFLAG_DELETED)) {
                    cm_fid_t cfid;
                    cm_user_t * userp;

                    tfid.Volume = scp->fid.volume;
                    tfid.Vnode = scp->fid.vnode;
                    tfid.Unique = scp->fid.unique;
                    cfid = scp->fid;
                    userp = fileLock->userp;

                    osi_Log3(afsd_logp, "CALL ExtendLock lock 0x%p for scp=0x%p with lock %d",
                             fileLock,
                             scp,
                             (int) scp->serverLock);

                    lock_ReleaseWrite(&scp->rw);

                    do {
                        code = cm_ConnFromFID(&cfid, userp,
                                       &req, &connp);
                        if (code)
                            break;

                        rxconnp = cm_GetRxConn(connp);
                        code = RXAFS_ExtendLock(rxconnp, &tfid,
                                                &volSync);
                        rx_PutConnection(rxconnp);

                        osi_Log1(afsd_logp, "   ExtendLock returns %d", code);

                    } while (cm_Analyze(connp, userp, &req,
                                        &cfid, 1, &volSync, NULL, NULL,
                                        code));

                    code = cm_MapRPCError(code, &req);

                    lock_ObtainWrite(&scp->rw);

                    if (code) {
                        osi_Log1(afsd_logp, "CALL ExtendLock FAILURE, code 0x%x", code);
                    } else {
                        osi_Log0(afsd_logp, "CALL ExtendLock SUCCESS");
                        scp->lockDataVersion = scp->dataVersion;
                    }

                    if ((code == EINVAL || code == CM_ERROR_INVAL) &&
                        scp->lockDataVersion == scp->dataVersion) {
                        int lockType;

                        lockType =
                            (scp->exclusiveLocks > 0) ? LockWrite: LockRead;

                        /* we might still have a chance to obtain a
                           new lock */

                        code = cm_IntSetLock(scp, userp, lockType, &req);

                        if (code) {
                            code = CM_ERROR_INVAL;
                        } else if (scp->lockDataVersion != scp->dataVersion) {

                            /* now check if we still have the file at
                               the right data version. */
                            osi_Log1(afsd_logp,
                                     "Data version mismatch on scp 0x%p",
                                     scp);
                            osi_Log2(afsd_logp,
                                     "   Data versions: before=%I64d, after=%I64d",
                                     scp->lockDataVersion,
                                     scp->dataVersion);

                            code = cm_IntReleaseLock(scp, userp, &req);

                            code = CM_ERROR_INVAL;
                        }
                    }

                    if (code == EINVAL || code == CM_ERROR_INVAL ||
                        code == CM_ERROR_BADFD) {
                        cm_LockMarkSCacheLost(scp);
                    }

                } else {
                    /* interestingly, we have found an active lock
                       belonging to an scache that has no
                       serverLock */
                    cm_LockMarkSCacheLost(scp);
                }

                scp_done = TRUE;

            pre_syncopdone:

                cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_LOCK);

            post_syncopdone:
                lock_ReleaseWrite(&scp->rw);

                lock_ObtainWrite(&cm_scacheLock);

                if (code == 0) {
                    fileLock->lastUpdate = time(NULL);
                }

                if (scp_done)
                    scp->lastRefreshCycle = cm_lockRefreshCycle;

            } else {
                /* we have already refreshed the locks on this scp */
                fileLock->lastUpdate = time(NULL);
            }

            cm_ReleaseSCacheNoLock(scp);

        } else if (IS_LOCK_ACTIVE(fileLock) && IS_LOCK_CLIENTONLY(fileLock)) {
            /* TODO: Check callbacks */
        }
    }

    lock_ReleaseWrite(&cm_scacheLock);
    osi_Log1(afsd_logp, "cm_CheckLocks completes lock check cycle %d", cm_lockRefreshCycle);
}

/* NOT called with scp->rw held. */
long cm_RetryLock(cm_file_lock_t *oldFileLock, int client_is_dead)
{
    long code = 0;
    cm_scache_t *scp = NULL;
    cm_file_lock_t *fileLock;
    osi_queue_t *q;
    cm_req_t req;
    int newLock = -1;
    int force_client_lock = FALSE;
    int has_insert = FALSE;
    int check_data_version = FALSE;

    cm_InitReq(&req);

    if (client_is_dead) {
        code = CM_ERROR_TIMEDOUT;
        goto updateLock;
    }

    lock_ObtainRead(&cm_scacheLock);

    osi_Log2(afsd_logp, "cm_RetryLock checking lock %p (scp=%p)", oldFileLock, oldFileLock->scp);
    osi_Log4(afsd_logp, "    offset(%x:%x) length(%x:%x)",
             (unsigned)(oldFileLock->range.offset >> 32),
             (unsigned)(oldFileLock->range.offset & 0xffffffff),
             (unsigned)(oldFileLock->range.length >> 32),
             (unsigned)(oldFileLock->range.length & 0xffffffff));
    osi_Log4(afsd_logp, "    key<0x%x,0x%x,0x%x> flags=%x",
             oldFileLock->key.process_id, oldFileLock->key.session_id, oldFileLock->key.file_id,
             (unsigned)(oldFileLock->flags));

    /* if the lock has already been granted, then we have nothing to do */
    if (IS_LOCK_ACTIVE(oldFileLock)) {
        lock_ReleaseRead(&cm_scacheLock);
        osi_Log0(afsd_logp, "cm_RetryLock lock already granted");
        return 0;
    }

    /* we can't do anything with lost or deleted locks at the moment. */
    if (IS_LOCK_LOST(oldFileLock) || IS_LOCK_DELETED(oldFileLock)) {
        code = CM_ERROR_BADFD;
        osi_Log0(afsd_logp, "cm_RetryLock lock is lost or deleted");
        lock_ReleaseRead(&cm_scacheLock);
        goto updateLock;
    }

    scp = oldFileLock->scp;

    osi_assertx(scp != NULL, "null cm_scache_t");

    lock_ReleaseRead(&cm_scacheLock);
    lock_ObtainWrite(&scp->rw);

    code = cm_LockCheckPerms(scp, oldFileLock->lockType,
                             oldFileLock->userp,
                             &req, &has_insert);

    if (code == CM_ERROR_NOACCESS && oldFileLock->lockType == LockRead) {
        if (!has_insert || !SCP_SUPPORTS_WRITELOCKACL(scp)) {
        force_client_lock = TRUE;
        }
        code = 0;
    } else if (code) {
        lock_ReleaseWrite(&scp->rw);
        return code;
    }

    lock_ObtainWrite(&cm_scacheLock);

    /* Check if we already have a sufficient server lock to allow this
       lock to go through. */
    if (IS_LOCK_WAITLOCK(oldFileLock) &&
        (!SERVERLOCKS_ENABLED(scp) ||
         scp->serverLock == oldFileLock->lockType ||
         scp->serverLock == LockWrite)) {

        oldFileLock->flags &= ~CM_FILELOCK_FLAG_WAITLOCK;

        if (SERVERLOCKS_ENABLED(scp)) {
            osi_Log1(afsd_logp, "cm_RetryLock Server lock (%d) is sufficient for lock.  Granting",
                     (int) scp->serverLock);
        } else {
            osi_Log0(afsd_logp, "cm_RetryLock skipping server lock for scp");
        }

        lock_ReleaseWrite(&cm_scacheLock);
        lock_ReleaseWrite(&scp->rw);

        return 0;
    }

    if (IS_LOCK_WAITUNLOCK(oldFileLock)) {

        /* check if the conflicting locks have dissappeared already */
        for (q = scp->fileLocksH; q; q = osi_QNext(q)) {

            fileLock = (cm_file_lock_t *)
                ((char *) q - offsetof(cm_file_lock_t, fileq));

            if (IS_LOCK_LOST(fileLock)) {
                if (cm_KeyEquals(&fileLock->key, &oldFileLock->key, 0)) {
                    code = CM_ERROR_BADFD;
                    oldFileLock->flags |= CM_FILELOCK_FLAG_LOST;
                    osi_Log1(afsd_logp, "    found lost lock %p for same key.  Marking lock as lost",
                             fileLock);
                    break;
                } else if (fileLock->lockType == LockWrite &&
                           INTERSECT_RANGE(oldFileLock->range, fileLock->range)) {
                    osi_Log1(afsd_logp, "    found conflicting LOST lock %p", fileLock);
                    code = CM_ERROR_WOULDBLOCK;
                    break;
                }
            }

            if (IS_LOCK_ACCEPTED(fileLock) &&
                INTERSECT_RANGE(oldFileLock->range, fileLock->range)) {

                if (oldFileLock->lockType != LockRead ||
                   fileLock->lockType != LockRead) {

                    osi_Log1(afsd_logp, "    found conflicting lock %p", fileLock);
                    code = CM_ERROR_WOULDBLOCK;
                    break;
                }
            }
        }
    }

    if (code != 0) {
        lock_ReleaseWrite(&cm_scacheLock);
        lock_ReleaseWrite(&scp->rw);

        goto handleCode;
    }

    /* when we get here, the lock is either a WAITUNLOCK or WAITLOCK.
       If it is WAITUNLOCK, then we didn't find any conflicting lock
       but we haven't verfied whether the serverLock is sufficient to
       assert it.  If it is WAITLOCK, then the serverLock is
       insufficient to assert it. Eitherway, we are ready to accept
       the lock as either ACTIVE or WAITLOCK depending on the
       serverLock. */

    /* First, promote the WAITUNLOCK to a WAITLOCK */
    if (IS_LOCK_WAITUNLOCK(oldFileLock)) {
        if (oldFileLock->lockType == LockRead)
            scp->sharedLocks++;
        else
            scp->exclusiveLocks++;

        oldFileLock->flags &= ~CM_FILELOCK_FLAG_WAITUNLOCK;
        oldFileLock->flags |= CM_FILELOCK_FLAG_WAITLOCK;
    }

    osi_assertx(IS_LOCK_WAITLOCK(oldFileLock), "!IS_LOCK_WAITLOCK");

    if (force_client_lock ||
        !SERVERLOCKS_ENABLED(scp) ||
        scp->serverLock == oldFileLock->lockType ||
        (oldFileLock->lockType == LockRead &&
         scp->serverLock == LockWrite)) {

        oldFileLock->flags &= ~CM_FILELOCK_FLAG_WAITLOCK;

        if ((force_client_lock ||
             !SERVERLOCKS_ENABLED(scp)) &&
            !IS_LOCK_CLIENTONLY(oldFileLock)) {

            oldFileLock->flags |= CM_FILELOCK_FLAG_CLIENTONLY;

            if (oldFileLock->lockType == LockRead)
                scp->sharedLocks--;
            else
                scp->exclusiveLocks--;

            scp->clientLocks++;
        }

        lock_ReleaseWrite(&cm_scacheLock);
        lock_ReleaseWrite(&scp->rw);

        return 0;

    } else {
        cm_user_t * userp;

        code = cm_SyncOp(scp, NULL, oldFileLock->userp, &req, 0,
                         CM_SCACHESYNC_NEEDCALLBACK
			 | CM_SCACHESYNC_GETSTATUS
			 | CM_SCACHESYNC_LOCK);
        if (code) {
            osi_Log1(afsd_logp, "cm_RetryLock SyncOp failure code 0x%x", code);
            lock_ReleaseWrite(&cm_scacheLock);
            goto post_syncopdone;
        }

        if (!IS_LOCK_WAITLOCK(oldFileLock))
            goto pre_syncopdone;

        userp = oldFileLock->userp;

#ifndef AGGRESSIVE_LOCKS
        newLock = oldFileLock->lockType;
#else
        newLock = LockWrite;
#endif

        if (has_insert) {
            /* if has_insert is non-zero, then:
               - the lock a LockRead
               - we don't have permission to get a LockRead
               - we do have permission to get a LockWrite
               - the server supports VICED_CAPABILITY_WRITELOCKACL
            */

            newLock = LockWrite;
        }

        lock_ReleaseWrite(&cm_scacheLock);

        /* when we get here, either we have a read-lock and want a
           write-lock or we don't have any locks and we want some
           lock. */

        if (scp->serverLock == LockRead) {

            osi_assertx(newLock == LockWrite, "!LockWrite");

            osi_Log0(afsd_logp, "  Attempting to UPGRADE from LockRead to LockWrite");

            scp->lockDataVersion = scp->dataVersion;
            check_data_version = TRUE;

            code = cm_IntReleaseLock(scp, userp, &req);

            if (code)
                goto pre_syncopdone;
            else
                scp->serverLock = -1;
        }

        code = cm_IntSetLock(scp, userp, newLock, &req);

        if (code == 0) {
            if (scp->dataVersion != scp->lockDataVersion) {
                /* we lost a race.  too bad */

                osi_Log0(afsd_logp,
                         "  Data version mismatch while upgrading lock.");
                osi_Log2(afsd_logp,
                         "  Data versions before=%I64d, after=%I64d",
                         scp->lockDataVersion,
                         scp->dataVersion);
                osi_Log1(afsd_logp,
                         "  Releasing stale lock for scp 0x%x", scp);

                code = cm_IntReleaseLock(scp, userp, &req);

                scp->serverLock = -1;

                code = CM_ERROR_INVAL;

                cm_LockMarkSCacheLost(scp);
            } else {
                scp->serverLock = newLock;
            }
        }

    pre_syncopdone:
        cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_LOCK);
    post_syncopdone:
        ;
    }

  handleCode:
    if (code != 0 && code != CM_ERROR_WOULDBLOCK) {
	lock_ObtainWrite(&cm_scacheLock);
        if (scp->fileLocksT == &oldFileLock->fileq)
            scp->fileLocksT = osi_QPrev(&oldFileLock->fileq);
        osi_QRemoveHT(&scp->fileLocksH, &scp->fileLocksT, &oldFileLock->fileq);
	lock_ReleaseWrite(&cm_scacheLock);
    }
    lock_ReleaseWrite(&scp->rw);

  updateLock:
    lock_ObtainWrite(&cm_scacheLock);
    if (code == 0) {
        oldFileLock->flags &= ~CM_FILELOCK_FLAG_WAITLOCK;
    } else if (code != CM_ERROR_WOULDBLOCK) {
        oldFileLock->flags |= CM_FILELOCK_FLAG_DELETED;
        cm_ReleaseUser(oldFileLock->userp);
        oldFileLock->userp = NULL;
        if (oldFileLock->scp) {
            cm_ReleaseSCacheNoLock(oldFileLock->scp);
            oldFileLock->scp = NULL;
        }
    }
    lock_ReleaseWrite(&cm_scacheLock);

    return code;
}

cm_key_t cm_GenerateKey(afs_uint16 session_id, afs_offs_t process_id, afs_uint16 file_id)
{
    cm_key_t key;

    key.process_id = process_id;
    key.session_id = session_id;
    key.file_id = file_id;

    return key;
}

int cm_KeyEquals(cm_key_t *k1, cm_key_t *k2, int flags)
{
    return (k1->session_id == k2->session_id) && (k1->file_id == k2->file_id) &&
        ((flags & CM_UNLOCK_BY_FID) || (k1->process_id == k2->process_id));
}

void cm_ReleaseAllLocks(void)
{
    cm_scache_t *scp;
    cm_req_t req;
    cm_user_t *userp;
    cm_key_t   key;
    cm_file_lock_t *fileLock;
    unsigned int i;

    for (i = 0; i < cm_data.scacheHashTableSize; i++)
    {
	for ( scp = cm_data.scacheHashTablep[i]; scp; scp = scp->nextp ) {
	    while (scp->fileLocksH != NULL) {
		lock_ObtainWrite(&scp->rw);
		lock_ObtainWrite(&cm_scacheLock);
		if (!scp->fileLocksH) {
		    lock_ReleaseWrite(&cm_scacheLock);
		    lock_ReleaseWrite(&scp->rw);
		    break;
		}
		fileLock = (cm_file_lock_t *)((char *) scp->fileLocksH - offsetof(cm_file_lock_t, fileq));
		userp = fileLock->userp;
		cm_HoldUser(userp);
		key = fileLock->key;
		cm_HoldSCacheNoLock(scp);
		lock_ReleaseWrite(&cm_scacheLock);
		cm_UnlockByKey(scp, key, 0, userp, &req);
		cm_ReleaseSCache(scp);
		cm_ReleaseUser(userp);
		lock_ReleaseWrite(&scp->rw);
	    }
	}
    }
}
