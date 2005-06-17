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
#endif /* !DJGPP */
#include <stddef.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>

#include <osi.h>

#include "afsd.h"

/* Used by cm_FollowMountPoint */
#define RWVOL	0
#define ROVOL	1
#define BACKVOL	2

#ifdef DEBUG
extern void afsi_log(char *pattern, ...);
#endif

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

/* characters that are legal in an 8.3 name */
/*
 * We used to have 1's for all characters from 128 to 254.  But
 * the NT client behaves better if we create an 8.3 name for any
 * name that has a character with the high bit on, and if we
 * delete those characters from 8.3 names.  In particular, see
 * Sybase defect 10859.
 */
char cm_LegalChars[256] = {
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0,
 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* return true iff component is a valid 8.3 name */
int cm_Is8Dot3(char *namep)
{
    int sawDot = 0;
    unsigned char tc;
    int charCount = 0;
        
    /*
     * can't have a leading dot;
     * special case for . and ..
     */
    if (namep[0] == '.') {
        if (namep[1] == 0)
            return 1;
        if (namep[1] == '.' && namep[2] == 0)
            return 1;
        return 0;
    }
    while (tc = *namep++) {
        if (tc == '.') {
            /* saw another dot */
            if (sawDot) return 0;	/* second dot */
            sawDot = 1;
            charCount = 0;
            continue;
        }
        if (cm_LegalChars[tc] == 0)
            return 0;
        charCount++;
        if (!sawDot && charCount > 8)
            /* more than 8 chars in name */
            return 0;
        if (sawDot && charCount > 3)
            /* more than 3 chars in extension */
            return 0;
    }
    return 1;
}

/*
 * Number unparsing map for generating 8.3 names;
 * The version taken from DFS was on drugs.  
 * You can't include '&' and '@' in a file name.
 */
char cm_8Dot3Mapping[42] =
{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'J', 'K', 
 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 
 'V', 'W', 'X', 'Y', 'Z', '_', '-', '$', '#', '!', '+', '='
};
int cm_8Dot3MapSize = sizeof(cm_8Dot3Mapping);

void cm_Gen8Dot3Name(cm_dirEntry_t *dep, char *shortName, char **shortNameEndp)
{
    char number[12];
    int i, nsize = 0;
    int vnode = ntohl(dep->fid.vnode);
    char *lastDot;
    int validExtension = 0;
    char tc, *temp, *name;

    /* Unparse the file's vnode number to get a "uniquifier" */
    do {
        number[nsize] = cm_8Dot3Mapping[vnode % cm_8Dot3MapSize];
        nsize++;
        vnode /= cm_8Dot3MapSize;
    } while (vnode);

    /*
     * Look for valid extension.  There has to be a dot, and
     * at least one of the characters following has to be legal.
     */
    lastDot = strrchr(dep->name, '.');
    if (lastDot) {
        temp = lastDot; temp++;
        while (tc = *temp++)
            if (cm_LegalChars[tc])
                break;
        if (tc)
            validExtension = 1;
    }       

    /* Copy name characters */
    name = dep->name;
    for (i = 0, name = dep->name;
          i < (7 - nsize) && name != lastDot; ) {
        tc = *name++;

        if (tc == 0)
            break;
        if (!cm_LegalChars[tc])
            continue;
        i++;
        *shortName++ = toupper(tc);
    }

    /* tilde */
    *shortName++ = '~';

    /* Copy uniquifier characters */
    memcpy(shortName, number, nsize);
    shortName += nsize;

    if (validExtension) {
        /* Copy extension characters */
        *shortName++ = *lastDot++;	/* copy dot */
        for (i = 0, tc = *lastDot++;
              i < 3 && tc;
              tc = *lastDot++) {
            if (cm_LegalChars[tc]) {
                i++;
                *shortName++ = toupper(tc);
            }
        }
    }

    /* Trailing null */
    *shortName = 0;

    if (shortNameEndp)
        *shortNameEndp = shortName;
}       

/* return success if we can open this file in this mode */
long cm_CheckOpen(cm_scache_t *scp, int openMode, int trunc, cm_user_t *userp,
                  cm_req_t *reqp)
{
    long rights;
    long code;

    rights = 0;
    if (openMode != 1) rights |= PRSFS_READ;
    if (openMode == 1 || openMode == 2 || trunc) rights |= PRSFS_WRITE;
        
    lock_ObtainMutex(&scp->mx);

    code = cm_SyncOp(scp, NULL, userp, reqp, rights,
                      CM_SCACHESYNC_GETSTATUS
                      | CM_SCACHESYNC_NEEDCALLBACK);
    lock_ReleaseMutex(&scp->mx);

    return code;
}

/* return success if we can open this file in this mode */
long cm_CheckNTOpen(cm_scache_t *scp, unsigned int desiredAccess,
                    unsigned int createDisp, cm_user_t *userp, cm_req_t *reqp)
{
    long rights;
    long code;

    /* Always allow delete; the RPC will tell us if it's OK */
    if (desiredAccess == DELETE)
        return 0;

    rights = 0;

    if (desiredAccess & AFS_ACCESS_READ)
        rights |= PRSFS_READ;

    if ((desiredAccess & AFS_ACCESS_WRITE)
         || createDisp == 4)
        rights |= PRSFS_WRITE;

    lock_ObtainMutex(&scp->mx);

    code = cm_SyncOp(scp, NULL, userp, reqp, rights,
                      CM_SCACHESYNC_GETSTATUS
                      | CM_SCACHESYNC_NEEDCALLBACK);
    lock_ReleaseMutex(&scp->mx);

    /*
     * If the open will fail because the volume is readonly, then we will
     * return an access denied error instead.  This is to help brain-dead
     * apps run correctly on replicated volumes.
     * See defect 10007 for more information.
     */
    if (code == CM_ERROR_READONLY)
        code = CM_ERROR_NOACCESS;

    return code;
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
    cm_dirEntry_t *dep;
    unsigned short *hashTable;
    unsigned int i, idx;
    int BeyondPage = 0, HaveDot = 0, HaveDotDot = 0;

    /* First check permissions */
    lock_ObtainMutex(&dscp->mx);
    code = cm_SyncOp(dscp, NULL, userp, reqp, PRSFS_DELETE,
                      CM_SCACHESYNC_GETSTATUS
                      | CM_SCACHESYNC_NEEDCALLBACK);
    lock_ReleaseMutex(&dscp->mx);
    if (code)
        return code;

    /* If deleting directory, must be empty */

    if (scp->fileType != CM_SCACHETYPE_DIRECTORY)
        return code;

    thyper.HighPart = 0; thyper.LowPart = 0;
    lock_ObtainRead(&scp->bufCreateLock);
    code = buf_Get(scp, &thyper, &bufferp);
    lock_ReleaseRead(&scp->bufCreateLock);
    if (code)
        return code;

    lock_ObtainMutex(&bufferp->mx);
    lock_ObtainMutex(&scp->mx);
    while (1) {
        code = cm_SyncOp(scp, bufferp, userp, reqp, 0,
                          CM_SCACHESYNC_NEEDCALLBACK
                          | CM_SCACHESYNC_READ
                          | CM_SCACHESYNC_BUFLOCKED);
        if (code)
            break;

        if (cm_HaveBuffer(scp, bufferp, 1))
            break;

        /* otherwise, load the buffer and try again */
        lock_ReleaseMutex(&bufferp->mx);
        code = cm_GetBuffer(scp, bufferp, NULL, userp, reqp);
        lock_ReleaseMutex(&scp->mx);
        lock_ObtainMutex(&bufferp->mx);
        lock_ObtainMutex(&scp->mx);
        if (code)
            break;
    }

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
    lock_ReleaseMutex(&scp->mx);
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
    cm_dirEntry_t *dep;
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
    lock_ObtainMutex(&scp->mx);
    code = cm_SyncOp(scp, NULL, userp, reqp, PRSFS_LOOKUP,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        lock_ReleaseMutex(&scp->mx);
        return code;
    }
        
    if (scp->fileType != CM_SCACHETYPE_DIRECTORY) {
        lock_ReleaseMutex(&scp->mx);
        return CM_ERROR_NOTDIR;
    }   

    if (retscp) 			/* if this is a lookup call */
    {
        cm_lookupSearch_t*	sp = parmp;

#ifdef AFS_FREELANCE_CLIENT
	/* Freelance entries never end up in the DNLC because they
	 * do not have an associated cm_server_t
	 */
    if ( !(cm_freelanceEnabled &&
            sp->fid.cell==AFS_FAKE_ROOT_CELL_ID &&
            sp->fid.volume==AFS_FAKE_ROOT_VOL_ID ) )
#endif /* AFS_FREELANCE_CLIENT */
    {
        int casefold = sp->caseFold;
        sp->caseFold = 0; /* we have a strong preference for exact matches */
        if ( *retscp = cm_dnlcLookup(scp, sp))	/* dnlc hit */
        {
            sp->caseFold = casefold;
            lock_ReleaseMutex(&scp->mx);
            return 0;
        }
        sp->caseFold = casefold;
    }
    }	

    /*
     * XXX We only get the length once.  It might change when we drop the
     * lock.
     */
    dirLength = scp->length;

    lock_ReleaseMutex(&scp->mx);

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

            lock_ObtainRead(&scp->bufCreateLock);
            code = buf_Get(scp, &thyper, &bufferp);
            lock_ReleaseRead(&scp->bufCreateLock);
            if (code) {
                /* if buf_Get() fails we do not have a buffer object to lock */
                bufferp = NULL;
                break;
            }

#ifdef AFSIFS
            /* Why was this added for IFS? - jaltman 06/18/2006 */
            lock_ObtainMutex(&scp->mx);
            if ((scp->flags & CM_SCACHEFLAG_BULKSTATTING) == 0
                 && (scp->bulkStatProgress.QuadPart <= thyper.QuadPart))
            {
                scp->flags |= CM_SCACHEFLAG_BULKSTATTING;
                cm_TryBulkStat(scp, &thyper, userp, reqp);
                scp->flags &= ~CM_SCACHEFLAG_BULKSTATTING;
                scp->bulkStatProgress = thyper;
            }
            lock_ReleaseMutex(&scp->mx);
#endif

            lock_ObtainMutex(&bufferp->mx);
            bufferOffset = thyper;

            /* now get the data in the cache */
            while (1) {
                lock_ObtainMutex(&scp->mx);
                code = cm_SyncOp(scp, bufferp, userp, reqp,
                                  PRSFS_LOOKUP,
                                  CM_SCACHESYNC_NEEDCALLBACK
                                  | CM_SCACHESYNC_READ
                                  | CM_SCACHESYNC_BUFLOCKED);
                if (code) {
                    lock_ReleaseMutex(&scp->mx);
                    break;
                }
                                
                if (cm_HaveBuffer(scp, bufferp, 1)) {
                    lock_ReleaseMutex(&scp->mx);
                    break;
                }

                /* otherwise, load the buffer and try again */
                lock_ReleaseMutex(&bufferp->mx);
                code = cm_GetBuffer(scp, bufferp, NULL, userp,
                                    reqp);
                lock_ReleaseMutex(&scp->mx);
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

int cm_NoneUpper(char *s)
{
    char c;
    while (c = *s++)
        if (c >= 'A' && c <= 'Z')
            return 0;
    return 1;
}

int cm_NoneLower(char *s)
{
    char c;
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
    char shortName[13];
    char *matchName;

    sp = (cm_lookupSearch_t *) rockp;

    matchName = dep->name;
    if (sp->caseFold)
        match = cm_stricmp(matchName, sp->searchNamep);
    else
        match = strcmp(matchName, sp->searchNamep);

    if (match != 0
         && sp->hasTilde
         && !cm_Is8Dot3(dep->name)) {
        matchName = shortName;
        cm_Gen8Dot3Name(dep, shortName, NULL);
        if (sp->caseFold)
            match = cm_stricmp(matchName, sp->searchNamep);
        else
            match = strcmp(matchName, sp->searchNamep);
    }       

    if (match != 0)
        return 0;

    sp->found = 1;
    if(!sp->caseFold) 
        sp->ExactFound = 1;

    if (!sp->caseFold || matchName == shortName) {
        sp->fid.vnode = ntohl(dep->fid.vnode);
        sp->fid.unique = ntohl(dep->fid.unique);
        return CM_ERROR_STOPNOW;
    }

    /*
     * If we get here, we are doing a case-insensitive search, and we
     * have found a match.  Now we determine what kind of match it is:
     * exact, lower-case, upper-case, or none of the above.  This is done
     * in order to choose among matches, if there are more than one.
     */

    /* Exact matches are the best. */
    match = strcmp(matchName, sp->searchNamep);
    if (match == 0) {
        sp->ExactFound = 1;
        sp->fid.vnode = ntohl(dep->fid.vnode);
        sp->fid.unique = ntohl(dep->fid.unique);
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
    sp->fid.vnode = ntohl(dep->fid.vnode);
    sp->fid.unique = ntohl(dep->fid.unique);
    return 0;
}       

/* read the contents of a mount point into the appropriate string.
 * called with locked scp, and returns with locked scp.
 */
long cm_ReadMountPoint(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp)
{
    long code;
    cm_buf_t *bufp;
    osi_hyper_t thyper;
    int tlen;

    if (scp->mountPointStringp[0]) 
        return 0;
        
    /* otherwise, we have to read it in */
    lock_ReleaseMutex(&scp->mx);

    lock_ObtainRead(&scp->bufCreateLock);
    thyper.LowPart = thyper.HighPart = 0;
    code = buf_Get(scp, &thyper, &bufp);
    lock_ReleaseRead(&scp->bufCreateLock);

    lock_ObtainMutex(&scp->mx);
    if (code) {
        return code;
    }
    while (1) {
        code = cm_SyncOp(scp, bufp, userp, reqp, 0,
                          CM_SCACHESYNC_READ | CM_SCACHESYNC_NEEDCALLBACK);
        if (code) {
            goto done;
        }

        if (cm_HaveBuffer(scp, bufp, 0)) 
            break;

        /* otherwise load buffer */
        code = cm_GetBuffer(scp, bufp, NULL, userp, reqp);
        if (code) {
            goto done;
        }
    }
    /* locked, has callback, has valid data in buffer */
    if ((tlen = scp->length.LowPart) > 1000) 
        return CM_ERROR_TOOBIG;
    if (tlen <= 0) {
        code = CM_ERROR_INVAL;
        goto done;
    }

    /* someone else did the work while we were out */
    if (scp->mountPointStringp[0]) {
        code = 0;
        goto done;
    }

    /* otherwise, copy out the link */
    memcpy(scp->mountPointStringp, bufp->datap, tlen);

    /* now make it null-terminated.  Note that the original contents of a
     * link that is a mount point is "#volname." where "." is there just to
     * be turned into a null.  That is, we can trash the last char of the
     * link without damaging the vol name.  This is a stupid convention,
     * but that's the protocol.
     */
    scp->mountPointStringp[tlen-1] = 0;
    code = 0;

  done:
    if (bufp) 
        buf_Release(bufp);
    return code;
}

/* called with a locked scp and chases the mount point, yielding outScpp.
 * scp remains locked, just for simplicity of describing the interface.
 */
long cm_FollowMountPoint(cm_scache_t *scp, cm_scache_t *dscp, cm_user_t *userp,
                         cm_req_t *reqp, cm_scache_t **outScpp)
{
    char *cellNamep;
    char *volNamep;
    int tlen;
    long code;
    char *cp;
    char *mpNamep;
    cm_volume_t *volp;
    cm_cell_t *cellp;
    char mtType;
    cm_fid_t tfid;
    size_t vnLength;
    int type;

    if (scp->mountRootFid.cell != 0 && scp->mountRootGen >= cm_data.mountRootGen) {
        tfid = scp->mountRootFid;
        lock_ReleaseMutex(&scp->mx);
        code = cm_GetSCache(&tfid, outScpp, userp, reqp);
        lock_ObtainMutex(&scp->mx);
        return code;
    }

    /* parse the volume name */
    mpNamep = scp->mountPointStringp;
    osi_assert(mpNamep[0]);
    tlen = strlen(scp->mountPointStringp);
    mtType = *scp->mountPointStringp;
    cellNamep = malloc(tlen);
    volNamep = malloc(tlen);

    cp = strrchr(mpNamep, ':');
    if (cp) {
        /* cellular mount point */
        memset(cellNamep, 0, tlen);
        strncpy(cellNamep, mpNamep+1, cp - mpNamep - 1);
        strcpy(volNamep, cp+1);
        /* now look up the cell */
        cellp = cm_GetCell(cellNamep, CM_FLAG_CREATE);
    }
    else {
        /* normal mt pt */
        strcpy(volNamep, mpNamep+1);

        cellp = cm_FindCellByID(scp->fid.cell);
    }

    if (!cellp) {
        code = CM_ERROR_NOSUCHCELL;
        goto done;
    }

    vnLength = strlen(volNamep);
    if (vnLength >= 8 && strcmp(volNamep + vnLength - 7, ".backup") == 0)
        type = BACKVOL;
    else if (vnLength >= 10
              && strcmp(volNamep + vnLength - 9, ".readonly") == 0)
        type = ROVOL;
    else
        type = RWVOL;

    /* check for backups within backups */
    if (type == BACKVOL
         && (scp->flags & (CM_SCACHEFLAG_RO | CM_SCACHEFLAG_PURERO))
         == CM_SCACHEFLAG_RO) {
        code = CM_ERROR_NOSUCHVOLUME;
        goto done;
    }

    /* now we need to get the volume */
    lock_ReleaseMutex(&scp->mx);
    code = cm_GetVolumeByName(cellp, volNamep, userp, reqp, 0, &volp);
    lock_ObtainMutex(&scp->mx);
        
    if (code == 0) {
        /* save the parent of the volume root for this is the 
         * place where the volume is mounted and we must remember 
         * this in the volume structure rather than just in the 
         * scache entry lest the scache entry gets recycled 
         * (defect 11489)
         */
        lock_ObtainMutex(&volp->mx);
        volp->dotdotFid = dscp->fid;
        lock_ReleaseMutex(&volp->mx);

        scp->mountRootFid.cell = cellp->cellID;
        /* if the mt pt is in a read-only volume (not just a
         * backup), and if there is a read-only volume for the
         * target, and if this is a type '#' mount point, use
         * the read-only, otherwise use the one specified.
         */
        if (mtType == '#' && (scp->flags & CM_SCACHEFLAG_PURERO)
             && volp->roID != 0 && type == RWVOL)
            type = ROVOL;
        if (type == ROVOL)
            scp->mountRootFid.volume = volp->roID;
        else if (type == BACKVOL)
            scp->mountRootFid.volume = volp->bkID;
        else
            scp->mountRootFid.volume = volp->rwID;

        /* the rest of the fid is a magic number */
        scp->mountRootFid.vnode = 1;
        scp->mountRootFid.unique = 1;
        scp->mountRootGen = cm_data.mountRootGen;

        tfid = scp->mountRootFid;
        lock_ReleaseMutex(&scp->mx);
        code = cm_GetSCache(&tfid, outScpp, userp, reqp);
        lock_ObtainMutex(&scp->mx);
    }

  done:
    free(cellNamep);
    free(volNamep);
    return code;
}       

long cm_LookupInternal(cm_scache_t *dscp, char *namep, long flags, cm_user_t *userp,
                       cm_req_t *reqp, cm_scache_t **outpScpp)
{
    long code;
    int dnlcHit = 1;	/* did we hit in the dnlc? yes, we did */
    cm_scache_t *tscp = NULL;
    cm_scache_t *mountedScp;
    cm_lookupSearch_t rock;
    int getroot;

    if (dscp->fid.vnode == 1 && dscp->fid.unique == 1
         && strcmp(namep, "..") == 0) {
        if (dscp->dotdotFid.volume == 0)
            return CM_ERROR_NOSUCHVOLUME;
        rock.fid = dscp->dotdotFid;
        goto haveFid;
    }

    memset(&rock, 0, sizeof(rock));
    rock.fid.cell = dscp->fid.cell;
    rock.fid.volume = dscp->fid.volume;
    rock.searchNamep = namep;
    rock.caseFold = (flags & CM_FLAG_CASEFOLD);
    rock.hasTilde = ((strchr(namep, '~') != NULL) ? 1 : 0);

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
                return CM_ERROR_NOSUCHPATH;
            else
                return CM_ERROR_NOSUCHFILE;
        }
        return code;
    }

    getroot = (dscp==cm_data.rootSCachep) ;
    if (!rock.found) {
        if (!cm_freelanceEnabled || !getroot) {
            if (flags & CM_FLAG_CHECKPATH)
                return CM_ERROR_NOSUCHPATH;
            else
                return CM_ERROR_NOSUCHFILE;
        }
        else {  /* nonexistent dir on freelance root, so add it */
            char fullname[200] = ".";
            int  found = 0;

            osi_Log1(afsd_logp,"cm_Lookup adding mount for non-existent directory: %s", 
                      osi_LogSaveString(afsd_logp,namep));
            if (namep[0] == '.') {
                if (cm_GetCell_Gen(&namep[1], &fullname[1], CM_FLAG_CREATE)) {
                    found = 1;
                    if ( stricmp(&namep[1], &fullname[1]) )
                        code = cm_FreelanceAddSymlink(namep, fullname, &rock.fid);
                    else
                        code = cm_FreelanceAddMount(namep, &fullname[1], "root.cell.", 1, &rock.fid);
                }
            } else {
                if (cm_GetCell_Gen(namep, fullname, CM_FLAG_CREATE)) {
                    found = 1;
                    if ( stricmp(namep, fullname) )
                        code = cm_FreelanceAddSymlink(namep, fullname, &rock.fid);
                    else
                        code = cm_FreelanceAddMount(namep, fullname, "root.cell.", 0, &rock.fid);
                }
            }
            if (!found || code < 0) {   /* add mount point failed, so give up */
                if (flags & CM_FLAG_CHECKPATH)
                    return CM_ERROR_NOSUCHPATH;
                else
                    return CM_ERROR_NOSUCHFILE;
            }
            tscp = NULL;   /* to force call of cm_GetSCache */
        }
    }

  haveFid:       
    if ( !tscp )    /* we did not find it in the dnlc */
    {
        dnlcHit = 0;	
        code = cm_GetSCache(&rock.fid, &tscp, userp, reqp);
        if (code) 
            return code;
    }       
    /* tscp is now held */

    lock_ObtainMutex(&tscp->mx);
    code = cm_SyncOp(tscp, NULL, userp, reqp, 0,
                      CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_NEEDCALLBACK);
    if (code) { 
        lock_ReleaseMutex(&tscp->mx);
        cm_ReleaseSCache(tscp);
        return code;
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
        lock_ReleaseMutex(&tscp->mx);
        cm_ReleaseSCache(tscp);
        if (code) {
            return code;
        }
        tscp = mountedScp;
    }
    else {
        lock_ReleaseMutex(&tscp->mx);
    }

    /* copy back pointer */
    *outpScpp = tscp;

    /* insert scache in dnlc */
    if ( !dnlcHit && !(flags & CM_FLAG_NOMOUNTCHASE) && rock.ExactFound ) {
        /* lock the directory entry to prevent racing callback revokes */
        lock_ObtainMutex(&dscp->mx);
        if ( dscp->cbServerp && dscp->cbExpires )
            cm_dnlcEnter(dscp, namep, tscp);
        lock_ReleaseMutex(&dscp->mx);
    }

    /* and return */
    return 0;
}

int cm_ExpandSysName(char *inp, char *outp, long outSize, unsigned int index)
{
    char *tp;
    int prefixCount;

    tp = strrchr(inp, '@');
    if (tp == NULL) 
        return 0;		/* no @sys */

    if (strcmp(tp, "@sys") != 0) 
        return 0;	/* no @sys */

    /* caller just wants to know if this is a valid @sys type of name */
    if (outp == NULL) 
        return 1;

    if (index >= MAXNUMSYSNAMES)
        return -1;

    /* otherwise generate the properly expanded @sys name */
    prefixCount = tp - inp;

    strncpy(outp, inp, prefixCount);	/* copy out "a." from "a.@sys" */
    outp[prefixCount] = 0;		/* null terminate the "a." */
    strcat(outp, cm_sysNameList[index]);/* append i386_nt40 */
    return 1;
}   

long cm_Lookup(cm_scache_t *dscp, char *namep, long flags, cm_user_t *userp,
               cm_req_t *reqp, cm_scache_t **outpScpp)
{
    long code;
    char tname[256];
    int sysNameIndex = 0;
    cm_scache_t *scp = 0;

    if ( stricmp(namep,SMB_IOCTL_FILENAME_NOSLASH) == 0 ) {
        if (flags & CM_FLAG_CHECKPATH)
            return CM_ERROR_NOSUCHPATH;
        else
            return CM_ERROR_NOSUCHFILE;
    }

    for ( sysNameIndex = 0; sysNameIndex < MAXNUMSYSNAMES; sysNameIndex++) {
        code = cm_ExpandSysName(namep, tname, sizeof(tname), sysNameIndex);
        if (code > 0) {
            code = cm_LookupInternal(dscp, tname, flags, userp, reqp, &scp);
            if (code == 0) {
                *outpScpp = scp;
                return 0;
            }
            if (scp) {
                cm_ReleaseSCache(scp);
                scp = 0;
            }
        } else {
            return cm_LookupInternal(dscp, namep, flags, userp, reqp, outpScpp);
        }
    }

    /* None of the possible sysName expansions could be found */
    if (flags & CM_FLAG_CHECKPATH)
        return CM_ERROR_NOSUCHPATH;
    else
        return CM_ERROR_NOSUCHFILE;
}

long cm_Unlink(cm_scache_t *dscp, char *namep, cm_user_t *userp, cm_req_t *reqp)
{
    long code;
    cm_conn_t *connp;
    AFSFid afsFid;
    int sflags;
    AFSFetchStatus newDirStatus;
    AFSVolSync volSync;
    struct rx_connection * callp;

#ifdef AFS_FREELANCE_CLIENT
    if (cm_freelanceEnabled && dscp == cm_data.rootSCachep) {
        /* deleting a mount point from the root dir. */
        code = cm_FreelanceRemoveMount(namep);
        return code;
    }
#endif  

    /* make sure we don't screw up the dir status during the merge */
    lock_ObtainMutex(&dscp->mx);
    sflags = CM_SCACHESYNC_STOREDATA;
    code = cm_SyncOp(dscp, NULL, userp, reqp, 0, sflags);
    lock_ReleaseMutex(&dscp->mx);
    if (code) 
        return code;

    /* make the RPC */
    afsFid.Volume = dscp->fid.volume;
    afsFid.Vnode = dscp->fid.vnode;
    afsFid.Unique = dscp->fid.unique;

    osi_Log1(afsd_logp, "CALL RemoveFile scp 0x%x", (long) dscp);
    do {
        code = cm_Conn(&dscp->fid, userp, reqp, &connp);
        if (code) 
            continue;

        callp = cm_GetRxConn(connp);
        code = RXAFS_RemoveFile(callp, &afsFid, namep,
                                 &newDirStatus, &volSync);
        rx_PutConnection(callp);

    } while (cm_Analyze(connp, userp, reqp, &dscp->fid, &volSync, NULL, NULL, code));
    code = cm_MapRPCError(code, reqp);

    if (code)
        osi_Log1(afsd_logp, "CALL RemoveFile FAILURE, code 0x%x", code);
    else
        osi_Log0(afsd_logp, "CALL RemoveFile SUCCESS");

    lock_ObtainMutex(&dscp->mx);
    cm_dnlcRemove(dscp, namep);
    cm_SyncOpDone(dscp, NULL, sflags);
    if (code == 0) 
        cm_MergeStatus(dscp, &newDirStatus, &volSync, userp, 0);
    lock_ReleaseMutex(&dscp->mx);

    return code;
}

/* called with a locked vnode, and fills in the link info.
 * returns this the vnode still locked.
 */
long cm_HandleLink(cm_scache_t *linkScp, cm_user_t *userp, cm_req_t *reqp)
{
    long code;
    cm_buf_t *bufp;
    long temp;
    osi_hyper_t thyper;

    lock_AssertMutex(&linkScp->mx);
    if (!linkScp->mountPointStringp[0]) {
        /* read the link data */
        lock_ReleaseMutex(&linkScp->mx);
        thyper.LowPart = thyper.HighPart = 0;
        code = buf_Get(linkScp, &thyper, &bufp);
        lock_ObtainMutex(&linkScp->mx);
        if (code) 
            return code;
        while (1) {
            code = cm_SyncOp(linkScp, bufp, userp, reqp, 0,
                              CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_READ);
            if (code) {
                buf_Release(bufp);
                return code;
            }
            if (cm_HaveBuffer(linkScp, bufp, 0)) 
                break;

            code = cm_GetBuffer(linkScp, bufp, NULL, userp, reqp);
            if (code) {
                buf_Release(bufp);
                return code;
            }
        } /* while loop to get the data */
                
        /* now if we still have no link read in,
         * copy the data from the buffer */
        if ((temp = linkScp->length.LowPart) >= MOUNTPOINTLEN) {
            buf_Release(bufp);
            return CM_ERROR_TOOBIG;
        }

        /* otherwise, it fits; make sure it is still null (could have
         * lost race with someone else referencing this link above),
         * and if so, copy in the data.
         */
        if (!linkScp->mountPointStringp[0]) {
            strncpy(linkScp->mountPointStringp, bufp->datap, temp);
            linkScp->mountPointStringp[temp] = 0;	/* null terminate */
        }
        buf_Release(bufp);
    }	/* don't have sym link contents cached */

    return 0;
}       

/* called with a held vnode and a path suffix, with the held vnode being a
 * symbolic link.  Our goal is to generate a new path to interpret, and return
 * this new path in newSpaceBufferp.  If the new vnode is relative to a dir
 * other than the directory containing the symbolic link, then the new root is
 * returned in *newRootScpp, otherwise a null is returned there.
 */
long cm_AssembleLink(cm_scache_t *linkScp, char *pathSuffixp,
                      cm_scache_t **newRootScpp, cm_space_t **newSpaceBufferp,
                      cm_user_t *userp, cm_req_t *reqp)
{
    long code = 0;
    long len;
    char *linkp;
    cm_space_t *tsp;

    lock_ObtainMutex(&linkScp->mx);
    code = cm_HandleLink(linkScp, userp, reqp);
    if (code) 
        goto done;

    /* if we may overflow the buffer, bail out; buffer is signficantly
     * bigger than max path length, so we don't really have to worry about
     * being a little conservative here.
     */
    if (strlen(linkScp->mountPointStringp) + strlen(pathSuffixp) + 2
         >= CM_UTILS_SPACESIZE)
        return CM_ERROR_TOOBIG;

    tsp = cm_GetSpace();
    linkp = linkScp->mountPointStringp;
    if (strncmp(linkp, cm_mountRoot, cm_mountRootLen) == 0) {
        if (strlen(linkp) > cm_mountRootLen)
            strcpy(tsp->data, linkp+cm_mountRootLen+1);
        else
            tsp->data[0] = 0;
        *newRootScpp = cm_data.rootSCachep;
        cm_HoldSCache(cm_data.rootSCachep);
    } else if (linkp[0] == '\\' && linkp[1] == '\\') {
        if (!strnicmp(&linkp[2], cm_NetbiosName, (len = strlen(cm_NetbiosName)))) 
        {
            char * p = &linkp[len + 3];
            if (strnicmp(p, "all", 3) == 0)
                p += 4;

            strcpy(tsp->data, p);
            for (p = tsp->data; *p; p++) {
                if (*p == '\\')
                    *p = '/';
            }
            *newRootScpp = cm_data.rootSCachep;
            cm_HoldSCache(cm_data.rootSCachep);
        } else {
            linkScp->fileType = CM_SCACHETYPE_DFSLINK;
            strcpy(tsp->data, linkp);
            *newRootScpp = NULL;
            code = CM_ERROR_PATH_NOT_COVERED;
        }
    } else if ( !strnicmp(linkp, "msdfs:", (len = strlen("msdfs:"))) ) {
        linkScp->fileType = CM_SCACHETYPE_DFSLINK;
        strcpy(tsp->data, linkp);
        *newRootScpp = NULL;
        code = CM_ERROR_PATH_NOT_COVERED;
    } else if (*linkp == '\\' || *linkp == '/') {
#if 0   
        /* formerly, this was considered to be from the AFS root,
         * but this seems to create problems.  instead, we will just
         * reject the link */
        strcpy(tsp->data, linkp+1);
        *newRootScpp = cm_data.rootSCachep;
        cm_HoldSCache(cm_data.rootSCachep);
#else
        /* we still copy the link data into the response so that 
         * the user can see what the link points to
         */
        linkScp->fileType = CM_SCACHETYPE_INVALID;
        strcpy(tsp->data, linkp);
        *newRootScpp = NULL;
        code = CM_ERROR_NOSUCHPATH;
#endif  
    } else {
        /* a relative link */
        strcpy(tsp->data, linkp);
        *newRootScpp = NULL;
    }
    if (pathSuffixp[0] != 0) {	/* if suffix string is non-null */
        strcat(tsp->data, "\\");
        strcat(tsp->data, pathSuffixp);
    }
    *newSpaceBufferp = tsp;

  done:
    lock_ReleaseMutex(&linkScp->mx);
    return code;
}

long cm_NameI(cm_scache_t *rootSCachep, char *pathp, long flags,
               cm_user_t *userp, char *tidPathp, cm_req_t *reqp, cm_scache_t **outScpp)
{
    long code;
    char *tp;			/* ptr moving through input buffer */
    char tc;			/* temp char */
    int haveComponent;		/* has new component started? */
    char component[256];	/* this is the new component */
    char *cp;			/* component name being assembled */
    cm_scache_t *tscp;		/* current location in the hierarchy */
    cm_scache_t *nscp;		/* next dude down */
    cm_scache_t *dirScp;	/* last dir we searched */
    cm_scache_t *linkScp;	/* new root for the symlink we just
    * looked up */
    cm_space_t *psp;		/* space for current path, if we've hit
    * any symlinks */
    cm_space_t *tempsp;		/* temp vbl */
    char *restp;		/* rest of the pathname to interpret */
    int symlinkCount;		/* count of # of symlinks traversed */
    int extraFlag;		/* avoid chasing mt pts for dir cmd */
    int phase = 1;		/* 1 = tidPathp, 2 = pathp */

    tp = tidPathp;
    if (tp == NULL) {
        tp = pathp;
        phase = 2;
    }
    if (tp == NULL) {
        tp = "";
    }
    haveComponent = 0;
    psp = NULL;
    tscp = rootSCachep;
    cm_HoldSCache(tscp);
    symlinkCount = 0;
    dirScp = 0;

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
                if (code) {
                    cm_ReleaseSCache(tscp);
                    if (dirScp)
                        cm_ReleaseSCache(dirScp);
                    if (psp) 
                        cm_FreeSpace(psp);
                    if (code == CM_ERROR_NOSUCHFILE && tscp->fileType == CM_SCACHETYPE_SYMLINK)
                        return CM_ERROR_NOSUCHPATH;
                    else
                        return code;
                }
                haveComponent = 0;	/* component done */
                if (dirScp)
                    cm_ReleaseSCache(dirScp);
                dirScp = tscp;		/* for some symlinks */
                tscp = nscp;	        /* already held */
                nscp = 0;
                if (tc == 0 && !(flags & CM_FLAG_FOLLOW) && phase == 2) {
                    code = 0;
                    if (dirScp) {
                        cm_ReleaseSCache(dirScp);
                        dirScp = 0;
                    }
                    break;
                }

                /* now, if tscp is a symlink, we should follow
                 * it and assemble the path again.
                 */
                lock_ObtainMutex(&tscp->mx);
                code = cm_SyncOp(tscp, NULL, userp, reqp, 0,
                                  CM_SCACHESYNC_GETSTATUS
                                  | CM_SCACHESYNC_NEEDCALLBACK);
                if (code) {
                    lock_ReleaseMutex(&tscp->mx);
                    cm_ReleaseSCache(tscp);
                    tscp = 0;
                    if (dirScp) {
                        cm_ReleaseSCache(dirScp);
                        dirScp = 0;
                    }
                    break;
                }
                if (tscp->fileType == CM_SCACHETYPE_SYMLINK) {
                    /* this is a symlink; assemble a new buffer */
                    lock_ReleaseMutex(&tscp->mx);
                    if (symlinkCount++ >= MAX_SYMLINK_COUNT) {
                        cm_ReleaseSCache(tscp);
                        tscp = 0;
                        if (dirScp) {
                            cm_ReleaseSCache(dirScp);
                            dirScp = 0;
                        }
                        if (psp) 
                            cm_FreeSpace(psp);
                        return CM_ERROR_TOO_MANY_SYMLINKS;
                    }
                    if (tc == 0) 
                        restp = "";
                    else 
                        restp = tp;
                    code = cm_AssembleLink(tscp, restp, &linkScp, &tempsp, userp, reqp);
                    if (code) {
                        /* something went wrong */
                        cm_ReleaseSCache(tscp);
                        tscp = 0;
                        if (dirScp) {
                            cm_ReleaseSCache(dirScp);
                            dirScp = 0;
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
                    tp = psp->data;
                    cm_ReleaseSCache(tscp);
                    tscp = linkScp;
                    linkScp = 0;
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
                        dirScp = 0;
                    }
                } else {
                    /* not a symlink, we may be done */
                    lock_ReleaseMutex(&tscp->mx);
                    if (tc == 0) {
                        if (phase == 1) {
                            phase = 2;
                            tp = pathp;
                            continue;
                        }
                        if (dirScp) {
                            cm_ReleaseSCache(dirScp);
                            dirScp = 0;
                        }
                        code = 0;
                        break;
                    }
                }
                if (dirScp) {
                    cm_ReleaseSCache(dirScp);
                    dirScp = 0;
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

    osi_Log1(afsd_logp, "Evaluating symlink scp 0x%x", linkScp);

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

    code = cm_NameI(newRootScp, spacep->data,
                     CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW | CM_FLAG_DIRSEARCH,
                     userp, NULL, reqp, outScpp);

    if (code == CM_ERROR_NOSUCHFILE)
        code = CM_ERROR_NOSUCHPATH;

    /* this stuff is allocated no matter what happened on the namei call,
     * so free it */
    cm_FreeSpace(spacep);
    cm_ReleaseSCache(newRootScp);

    return code;
}

/* make this big enough so that one buffer of dir pages won't overflow.  We'll
 * check anyway, but we want to minimize the chance that we have to leave stuff
 * unstat'd.
 */
#define CM_BULKMAX		128

/* rock for bulk stat calls */
typedef struct cm_bulkStat {
    osi_hyper_t bufOffset;	/* only do it for things in this buffer page */

    /* info for the actual call */
    int counter;			/* next free slot */
    AFSFid fids[CM_BULKMAX];
    AFSFetchStatus stats[CM_BULKMAX];
    AFSCallBack callbacks[CM_BULKMAX];
} cm_bulkStat_t;

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

    tfid.cell = scp->fid.cell;
    tfid.volume = scp->fid.volume;
    tfid.vnode = ntohl(dep->fid.vnode);
    tfid.unique = ntohl(dep->fid.unique);
    tscp = cm_FindSCache(&tfid);
    if (tscp) {
        if (lock_TryMutex(&tscp->mx)) {
            /* we have an entry that we can look at */
            if (cm_HaveCallback(tscp)) {
                /* we have a callback on it.  Don't bother
                 * fetching this stat entry, since we're happy
                 * with the info we have.
                 */
                lock_ReleaseMutex(&tscp->mx);
                cm_ReleaseSCache(tscp);
                return 0;
            }
            lock_ReleaseMutex(&tscp->mx);
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

/* called with a locked scp and a pointer to a buffer.  Make bulk stat
 * calls on all undeleted files in the page of the directory specified.
 */
void cm_TryBulkStat(cm_scache_t *dscp, osi_hyper_t *offsetp, cm_user_t *userp,
                     cm_req_t *reqp)
{
    long code;
    cm_bulkStat_t bb;	/* this is *BIG*, probably 12K or so;
                         * watch for stack problems */
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
    struct rx_connection * callp;

    osi_Log1(afsd_logp, "cm_TryBulkStat dir 0x%x", (long) dscp);

    /* should be on a buffer boundary */
    osi_assert((offsetp->LowPart & (cm_data.buf_blockSize - 1)) == 0);

    bb.counter = 0;
    bb.bufOffset = *offsetp;

    lock_ReleaseMutex(&dscp->mx);
    /* first, assemble the file IDs we need to stat */
    code = cm_ApplyDir(dscp, cm_TryBulkProc, (void *) &bb, offsetp, userp, reqp, NULL);

    /* if we failed, bail out early */
    if (code && code != CM_ERROR_STOPNOW) {
        lock_ObtainMutex(&dscp->mx);
        return;
    }

    /* otherwise, we may have one or more bulk stat's worth of stuff in bb;
     * make the calls to create the entries.  Handle AFSCBMAX files at a
     * time.
     */
    filex = 0;
    while (filex < bb.counter) {
        filesThisCall = bb.counter - filex;
        if (filesThisCall > AFSCBMAX) 
            filesThisCall = AFSCBMAX;

        fidStruct.AFSCBFids_len = filesThisCall;
        fidStruct.AFSCBFids_val = &bb.fids[filex];
        statStruct.AFSBulkStats_len = filesThisCall;
        statStruct.AFSBulkStats_val = &bb.stats[filex];
        callbackStruct.AFSCBs_len = filesThisCall;
        callbackStruct.AFSCBs_val = &bb.callbacks[filex];
        cm_StartCallbackGrantingCall(NULL, &cbReq);
        osi_Log1(afsd_logp, "CALL BulkStatus, %d entries", filesThisCall);
        do {
            code = cm_Conn(&dscp->fid, userp, reqp, &connp);
            if (code) 
                continue;

            callp = cm_GetRxConn(connp);
            code = RXAFS_BulkStatus(callp, &fidStruct,
                                     &statStruct, &callbackStruct, &volSync);
            rx_PutConnection(callp);

        } while (cm_Analyze(connp, userp, reqp, &dscp->fid,
                             &volSync, NULL, &cbReq, code));
        code = cm_MapRPCError(code, reqp);

        if (code)
            osi_Log1(afsd_logp, "CALL BulkStatus FAILURE code 0x%x", code);
        else
            osi_Log0(afsd_logp, "CALL BulkStatus SUCCESS");

        /* may as well quit on an error, since we're not going to do
         * much better on the next immediate call, either.
         */
        if (code) {
            cm_EndCallbackGrantingCall(NULL, &cbReq, NULL, 0);
            break;
        }

        /* otherwise, we should do the merges */
        for (i = 0; i<filesThisCall; i++) {
            j = filex + i;
            tfid.cell = dscp->fid.cell;
            tfid.volume = bb.fids[j].Volume;
            tfid.vnode = bb.fids[j].Vnode;
            tfid.unique = bb.fids[j].Unique;
            code = cm_GetSCache(&tfid, &scp, userp, reqp);
            if (code != 0) 
                continue;

            /* otherwise, if this entry has no callback info, 
             * merge in this.
             */
            lock_ObtainMutex(&scp->mx);
            /* now, we have to be extra paranoid on merging in this
             * information, since we didn't use cm_SyncOp before
             * starting the fetch to make sure that no bad races
             * were occurring.  Specifically, we need to make sure
             * we don't obliterate any newer information in the
             * vnode than have here.
             *
             * Right now, be pretty conservative: if there's a
             * callback or a pending call, skip it.
             */
            if (scp->cbServerp == NULL
                 && !(scp->flags &
                       (CM_SCACHEFLAG_FETCHING
                         | CM_SCACHEFLAG_STORING
                         | CM_SCACHEFLAG_SIZESTORING))) {
                cm_EndCallbackGrantingCall(scp, &cbReq,
                                            &bb.callbacks[j],
                                            CM_CALLBACK_MAINTAINCOUNT);
                cm_MergeStatus(scp, &bb.stats[j], &volSync,
                                userp, 0);
            }       
            lock_ReleaseMutex(&scp->mx);
            cm_ReleaseSCache(scp);
        } /* all files in the response */
        /* now tell it to drop the count,
         * after doing the vnode processing above */
        cm_EndCallbackGrantingCall(NULL, &cbReq, NULL, 0);

        filex += filesThisCall;
    }	/* while there are still more files to process */
    lock_ObtainMutex(&dscp->mx);
    osi_Log0(afsd_logp, "END cm_TryBulkStat");
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
    lock_ObtainMutex(&scp->mx);
    code = cm_SyncOp(scp, NULL, userp, reqp, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) 
        goto done;
        
    if (scp->fileType != CM_SCACHETYPE_FILE) {
        code = CM_ERROR_ISDIR;
        goto done;
    }

  startover:
    if (LargeIntegerLessThan(*sizep, scp->length))
        shrinking = 1;
    else
        shrinking = 0;

    lock_ReleaseMutex(&scp->mx);

    /* can't hold scp->mx lock here, since we may wait for a storeback to
     * finish if the buffer package is cleaning a buffer by storing it to
     * the server.
     */
    if (shrinking)
        buf_Truncate(scp, userp, reqp, sizep);

    /* now ensure that file length is short enough, and update truncPos */
    lock_ObtainMutex(&scp->mx);

    /* make sure we have a callback (so we have the right value for the
     * length), and wait for it to be safe to do a truncate.
     */
    code = cm_SyncOp(scp, NULL, userp, reqp, PRSFS_WRITE,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS
                      | CM_SCACHESYNC_SETSTATUS | CM_SCACHESYNC_SETSIZE);
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

  done:
    lock_ReleaseMutex(&scp->mx);
    lock_ReleaseWrite(&scp->bufCreateLock);

    return code;
}

/* set the file size or other attributes (but not both at once) */
long cm_SetAttr(cm_scache_t *scp, cm_attr_t *attrp, cm_user_t *userp,
                cm_req_t *reqp)
{
    long code;
    int flags;
    AFSFetchStatus afsOutStatus;
    AFSVolSync volSync;
    cm_conn_t *connp;
    AFSFid tfid;
    AFSStoreStatus afsInStatus;
    struct rx_connection * callp;

    /* handle file length setting */
    if (attrp->mask & CM_ATTRMASK_LENGTH)
        return cm_SetLength(scp, &attrp->length, userp, reqp);

    flags = CM_SCACHESYNC_STORESTATUS;

    lock_ObtainMutex(&scp->mx);
    /* otherwise, we have to make an RPC to get the status */
    code = cm_SyncOp(scp, NULL, userp, reqp, 0, CM_SCACHESYNC_STORESTATUS);

    /* make the attr structure */
    cm_StatusFromAttr(&afsInStatus, scp, attrp);

    tfid.Volume = scp->fid.volume;
    tfid.Vnode = scp->fid.vnode;
    tfid.Unique = scp->fid.unique;

    lock_ReleaseMutex(&scp->mx);
    if (code) 
        return code;

    /* now make the RPC */
    osi_Log1(afsd_logp, "CALL StoreStatus scp 0x%x", (long) scp);
    do {
        code = cm_Conn(&scp->fid, userp, reqp, &connp);
        if (code) 
            continue;

        callp = cm_GetRxConn(connp);
        code = RXAFS_StoreStatus(callp, &tfid,
                                  &afsInStatus, &afsOutStatus, &volSync);
        rx_PutConnection(callp);

    } while (cm_Analyze(connp, userp, reqp,
                         &scp->fid, &volSync, NULL, NULL, code));
    code = cm_MapRPCError(code, reqp);

    if (code)
        osi_Log1(afsd_logp, "CALL StoreStatus FAILURE, code 0x%x", code);
    else
        osi_Log0(afsd_logp, "CALL StoreStatus SUCCESS");

    lock_ObtainMutex(&scp->mx);
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_STORESTATUS);
    if (code == 0)
        cm_MergeStatus(scp, &afsOutStatus, &volSync, userp,
                        CM_MERGEFLAG_FORCE);
	
    /* if we're changing the mode bits, discard the ACL cache, 
     * since we changed the mode bits.
     */
    if (afsInStatus.Mask & AFS_SETMODE) cm_FreeAllACLEnts(scp);
    lock_ReleaseMutex(&scp->mx);
    return code;
}       

long cm_Create(cm_scache_t *dscp, char *namep, long flags, cm_attr_t *attrp,
               cm_scache_t **scpp, cm_user_t *userp, cm_req_t *reqp)
{       
    cm_conn_t *connp;
    long code;
    AFSFid dirAFSFid;
    cm_callbackRequest_t cbReq;
    AFSFid newAFSFid;
    cm_fid_t newFid;
    cm_scache_t *scp;
    int didEnd;
    AFSStoreStatus inStatus;
    AFSFetchStatus updatedDirStatus;
    AFSFetchStatus newFileStatus;
    AFSCallBack newFileCallback;
    AFSVolSync volSync;
    struct rx_connection * callp;

    /* can't create names with @sys in them; must expand it manually first.
     * return "invalid request" if they try.
     */
    if (cm_ExpandSysName(namep, NULL, 0, 0)) {
        return CM_ERROR_ATSYS;
    }

    /* before starting the RPC, mark that we're changing the file data, so
     * that someone who does a chmod will know to wait until our call
     * completes.
     */
    lock_ObtainMutex(&dscp->mx);
    code = cm_SyncOp(dscp, NULL, userp, reqp, 0, CM_SCACHESYNC_STOREDATA);
    if (code == 0) {
        cm_StartCallbackGrantingCall(NULL, &cbReq);
    }
    lock_ReleaseMutex(&dscp->mx);
    if (code) {
        return code;
    }
    didEnd = 0;

    cm_StatusFromAttr(&inStatus, NULL, attrp);

    /* try the RPC now */
    osi_Log1(afsd_logp, "CALL CreateFile scp 0x%x", (long) dscp);
    do {
        code = cm_Conn(&dscp->fid, userp, reqp, &connp);
        if (code) 
            continue;

        dirAFSFid.Volume = dscp->fid.volume;
        dirAFSFid.Vnode = dscp->fid.vnode;
        dirAFSFid.Unique = dscp->fid.unique;

        callp = cm_GetRxConn(connp);
        code = RXAFS_CreateFile(connp->callp, &dirAFSFid, namep,
                                 &inStatus, &newAFSFid, &newFileStatus,
                                 &updatedDirStatus, &newFileCallback,
                                 &volSync);
        rx_PutConnection(callp);

    } while (cm_Analyze(connp, userp, reqp,
                         &dscp->fid, &volSync, NULL, &cbReq, code));
    code = cm_MapRPCError(code, reqp);
        
    if (code)
        osi_Log1(afsd_logp, "CALL CreateFile FAILURE, code 0x%x", code);
    else
        osi_Log0(afsd_logp, "CALL CreateFile SUCCESS");

    lock_ObtainMutex(&dscp->mx);
    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_STOREDATA);
    if (code == 0) {
        cm_MergeStatus(dscp, &updatedDirStatus, &volSync, userp, 0);
    }
    lock_ReleaseMutex(&dscp->mx);

    /* now try to create the file's entry, too, but be careful to 
     * make sure that we don't merge in old info.  Since we weren't locking
     * out any requests during the file's creation, we may have pretty old
     * info.
     */
    if (code == 0) {
        newFid.cell = dscp->fid.cell;
        newFid.volume = dscp->fid.volume;
        newFid.vnode = newAFSFid.Vnode;
        newFid.unique = newAFSFid.Unique;
        code = cm_GetSCache(&newFid, &scp, userp, reqp);
        if (code == 0) {
            lock_ObtainMutex(&scp->mx);
            if (!cm_HaveCallback(scp)) {
                cm_MergeStatus(scp, &newFileStatus, &volSync,
                                userp, 0);
                cm_EndCallbackGrantingCall(scp, &cbReq,
                                            &newFileCallback, 0);
                didEnd = 1;     
            }       
            lock_ReleaseMutex(&scp->mx);
            *scpp = scp;
        }
    }

    /* make sure we end things properly */
    if (!didEnd)
        cm_EndCallbackGrantingCall(NULL, &cbReq, NULL, 0);

    return code;
}       

long cm_FSync(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp)
{
    long code;

    lock_ObtainWrite(&scp->bufCreateLock);
    code = buf_CleanVnode(scp, userp, reqp);
    lock_ReleaseWrite(&scp->bufCreateLock);
    if (code == 0) {
        lock_ObtainMutex(&scp->mx);
        scp->flags &= ~(CM_SCACHEFLAG_OVERQUOTA
                         | CM_SCACHEFLAG_OUTOFSPACE);
        if (scp->mask & (CM_SCACHEMASK_TRUNCPOS
                          | CM_SCACHEMASK_CLIENTMODTIME
                          | CM_SCACHEMASK_LENGTH))
            code = cm_StoreMini(scp, userp, reqp);
        lock_ReleaseMutex(&scp->mx);
    }
    return code;
}

long cm_MakeDir(cm_scache_t *dscp, char *namep, long flags, cm_attr_t *attrp,
                 cm_user_t *userp, cm_req_t *reqp)
{
    cm_conn_t *connp;
    long code;
    AFSFid dirAFSFid;
    cm_callbackRequest_t cbReq;
    AFSFid newAFSFid;
    cm_fid_t newFid;
    cm_scache_t *scp;
    int didEnd;
    AFSStoreStatus inStatus;
    AFSFetchStatus updatedDirStatus;
    AFSFetchStatus newDirStatus;
    AFSCallBack newDirCallback;
    AFSVolSync volSync;
    struct rx_connection * callp;

    /* can't create names with @sys in them; must expand it manually first.
     * return "invalid request" if they try.
     */
    if (cm_ExpandSysName(namep, NULL, 0, 0)) {
        return CM_ERROR_ATSYS;
    }

    /* before starting the RPC, mark that we're changing the directory
     * data, so that someone who does a chmod on the dir will wait until
     * our call completes.
     */
    lock_ObtainMutex(&dscp->mx);
    code = cm_SyncOp(dscp, NULL, userp, reqp, 0, CM_SCACHESYNC_STOREDATA);
    if (code == 0) {
        cm_StartCallbackGrantingCall(NULL, &cbReq);
    }
    lock_ReleaseMutex(&dscp->mx);
    if (code) {
        return code;
    }
    didEnd = 0;

    cm_StatusFromAttr(&inStatus, NULL, attrp);

    /* try the RPC now */
    osi_Log1(afsd_logp, "CALL MakeDir scp 0x%x", (long) dscp);
    do {
        code = cm_Conn(&dscp->fid, userp, reqp, &connp);
        if (code) 
            continue;

        dirAFSFid.Volume = dscp->fid.volume;
        dirAFSFid.Vnode = dscp->fid.vnode;
        dirAFSFid.Unique = dscp->fid.unique;

        callp = cm_GetRxConn(connp);
        code = RXAFS_MakeDir(connp->callp, &dirAFSFid, namep,
                              &inStatus, &newAFSFid, &newDirStatus,
                              &updatedDirStatus, &newDirCallback,
                              &volSync);
        rx_PutConnection(callp);

    } while (cm_Analyze(connp, userp, reqp,
                         &dscp->fid, &volSync, NULL, &cbReq, code));
    code = cm_MapRPCError(code, reqp);
        
    if (code)
        osi_Log1(afsd_logp, "CALL MakeDir FAILURE, code 0x%x", code);
    else
        osi_Log0(afsd_logp, "CALL MakeDir SUCCESS");

    lock_ObtainMutex(&dscp->mx);
    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_STOREDATA);
    if (code == 0) {
        cm_MergeStatus(dscp, &updatedDirStatus, &volSync, userp, 0);
    }
    lock_ReleaseMutex(&dscp->mx);

    /* now try to create the new dir's entry, too, but be careful to 
     * make sure that we don't merge in old info.  Since we weren't locking
     * out any requests during the file's creation, we may have pretty old
     * info.
     */
    if (code == 0) {
        newFid.cell = dscp->fid.cell;
        newFid.volume = dscp->fid.volume;
        newFid.vnode = newAFSFid.Vnode;
        newFid.unique = newAFSFid.Unique;
        code = cm_GetSCache(&newFid, &scp, userp, reqp);
        if (code == 0) {
            lock_ObtainMutex(&scp->mx);
            if (!cm_HaveCallback(scp)) {
                cm_MergeStatus(scp, &newDirStatus, &volSync,
                                userp, 0);
                cm_EndCallbackGrantingCall(scp, &cbReq,
                                            &newDirCallback, 0);
                didEnd = 1;             
            }
            lock_ReleaseMutex(&scp->mx);
            cm_ReleaseSCache(scp);
        }
    }

    /* make sure we end things properly */
    if (!didEnd)
        cm_EndCallbackGrantingCall(NULL, &cbReq, NULL, 0);

    /* and return error code */
    return code;
}       

long cm_Link(cm_scache_t *dscp, char *namep, cm_scache_t *sscp, long flags,
             cm_user_t *userp, cm_req_t *reqp)
{
    cm_conn_t *connp;
    long code = 0;
    AFSFid dirAFSFid;
    AFSFid existingAFSFid;
    AFSFetchStatus updatedDirStatus;
    AFSFetchStatus newLinkStatus;
    AFSVolSync volSync;
    struct rx_connection * callp;

    if (dscp->fid.cell != sscp->fid.cell ||
        dscp->fid.volume != sscp->fid.volume) {
        return CM_ERROR_CROSSDEVLINK;
    }

    lock_ObtainMutex(&dscp->mx);
    code = cm_SyncOp(dscp, NULL, userp, reqp, 0, CM_SCACHESYNC_STOREDATA);
    lock_ReleaseMutex(&dscp->mx);

    if (code)
        return code;

    /* try the RPC now */
    osi_Log1(afsd_logp, "CALL Link scp 0x%x", (long) dscp);
    do {
        code = cm_Conn(&dscp->fid, userp, reqp, &connp);
        if (code) continue;

        dirAFSFid.Volume = dscp->fid.volume;
        dirAFSFid.Vnode = dscp->fid.vnode;
        dirAFSFid.Unique = dscp->fid.unique;

        existingAFSFid.Volume = sscp->fid.volume;
        existingAFSFid.Vnode = sscp->fid.vnode;
        existingAFSFid.Unique = sscp->fid.unique;

        callp = cm_GetRxConn(connp);
        code = RXAFS_Link(callp, &dirAFSFid, namep, &existingAFSFid,
            &newLinkStatus, &updatedDirStatus, &volSync);
        rx_PutConnection(callp);
        osi_Log1(smb_logp,"  RXAFS_Link returns 0x%x", code);

    } while (cm_Analyze(connp, userp, reqp,
        &dscp->fid, &volSync, NULL, NULL, code));

    code = cm_MapRPCError(code, reqp);

    if (code)
        osi_Log1(afsd_logp, "CALL Link FAILURE, code 0x%x", code);
    else
        osi_Log0(afsd_logp, "CALL Link SUCCESS");

    lock_ObtainMutex(&dscp->mx);
    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_STOREDATA);
    if (code == 0) {
        cm_MergeStatus(dscp, &updatedDirStatus, &volSync, userp, 0);
    }
    lock_ReleaseMutex(&dscp->mx);

    return code;
}

long cm_SymLink(cm_scache_t *dscp, char *namep, char *contentsp, long flags,
                cm_attr_t *attrp, cm_user_t *userp, cm_req_t *reqp)
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
    struct rx_connection * callp;

    /* before starting the RPC, mark that we're changing the directory data,
     * so that someone who does a chmod on the dir will wait until our
     * call completes.
     */
    lock_ObtainMutex(&dscp->mx);
    code = cm_SyncOp(dscp, NULL, userp, reqp, 0, CM_SCACHESYNC_STOREDATA);
    lock_ReleaseMutex(&dscp->mx);
    if (code) {
        return code;
    }

    cm_StatusFromAttr(&inStatus, NULL, attrp);

    /* try the RPC now */
    osi_Log1(afsd_logp, "CALL Symlink scp 0x%x", (long) dscp);
    do {
        code = cm_Conn(&dscp->fid, userp, reqp, &connp);
        if (code) 
            continue;

        dirAFSFid.Volume = dscp->fid.volume;
        dirAFSFid.Vnode = dscp->fid.vnode;
        dirAFSFid.Unique = dscp->fid.unique;

        callp = cm_GetRxConn(connp);
        code = RXAFS_Symlink(callp, &dirAFSFid, namep, contentsp,
                              &inStatus, &newAFSFid, &newLinkStatus,
                              &updatedDirStatus, &volSync);
        rx_PutConnection(callp);

    } while (cm_Analyze(connp, userp, reqp,
                         &dscp->fid, &volSync, NULL, NULL, code));
    code = cm_MapRPCError(code, reqp);
        
    if (code)
        osi_Log1(afsd_logp, "CALL Symlink FAILURE, code 0x%x", code);
    else
        osi_Log0(afsd_logp, "CALL Symlink SUCCESS");

    lock_ObtainMutex(&dscp->mx);
    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_STOREDATA);
    if (code == 0) {
        cm_MergeStatus(dscp, &updatedDirStatus, &volSync, userp, 0);
    }
    lock_ReleaseMutex(&dscp->mx);

    /* now try to create the new dir's entry, too, but be careful to 
     * make sure that we don't merge in old info.  Since we weren't locking
     * out any requests during the file's creation, we may have pretty old
     * info.
     */
    if (code == 0) {
        newFid.cell = dscp->fid.cell;
        newFid.volume = dscp->fid.volume;
        newFid.vnode = newAFSFid.Vnode;
        newFid.unique = newAFSFid.Unique;
        code = cm_GetSCache(&newFid, &scp, userp, reqp);
        if (code == 0) {
            lock_ObtainMutex(&scp->mx);
            if (!cm_HaveCallback(scp)) {
                cm_MergeStatus(scp, &newLinkStatus, &volSync,
                                userp, 0);
            }       
            lock_ReleaseMutex(&scp->mx);
            cm_ReleaseSCache(scp);
        }
    }
	
    /* and return error code */
    return code;
}

long cm_RemoveDir(cm_scache_t *dscp, char *namep, cm_user_t *userp,
                   cm_req_t *reqp)
{
    cm_conn_t *connp;
    long code;
    AFSFid dirAFSFid;
    int didEnd;
    AFSFetchStatus updatedDirStatus;
    AFSVolSync volSync;
    struct rx_connection * callp;

    /* before starting the RPC, mark that we're changing the directory data,
     * so that someone who does a chmod on the dir will wait until our
     * call completes.
     */
    lock_ObtainMutex(&dscp->mx);
    code = cm_SyncOp(dscp, NULL, userp, reqp, 0, CM_SCACHESYNC_STOREDATA);
    lock_ReleaseMutex(&dscp->mx);
    if (code) {
        return code;
    }
    didEnd = 0;

    /* try the RPC now */
    osi_Log1(afsd_logp, "CALL RemoveDir scp 0x%x", (long) dscp);
    do {
        code = cm_Conn(&dscp->fid, userp, reqp, &connp);
        if (code) 
            continue;

        dirAFSFid.Volume = dscp->fid.volume;
        dirAFSFid.Vnode = dscp->fid.vnode;
        dirAFSFid.Unique = dscp->fid.unique;

        callp = cm_GetRxConn(connp);
        code = RXAFS_RemoveDir(callp, &dirAFSFid, namep,
                                &updatedDirStatus, &volSync);
        rx_PutConnection(callp);

    } while (cm_Analyze(connp, userp, reqp,
                         &dscp->fid, &volSync, NULL, NULL, code));
    code = cm_MapRPCErrorRmdir(code, reqp);

    if (code)
        osi_Log1(afsd_logp, "CALL RemoveDir FAILURE, code 0x%x", code);
    else
        osi_Log0(afsd_logp, "CALL RemoveDir SUCCESS");

    lock_ObtainMutex(&dscp->mx);
    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_STOREDATA);
    if (code == 0) {
        cm_dnlcRemove(dscp, namep); 
        cm_MergeStatus(dscp, &updatedDirStatus, &volSync, userp, 0);
    }
    lock_ReleaseMutex(&dscp->mx);

    /* and return error code */
    return code;
}

long cm_Open(cm_scache_t *scp, int type, cm_user_t *userp)
{
    /* grab mutex on contents */
    lock_ObtainMutex(&scp->mx);

    /* reset the prefetch info */
    scp->prefetch.base.LowPart = 0;		/* base */
    scp->prefetch.base.HighPart = 0;
    scp->prefetch.end.LowPart = 0;		/* and end */
    scp->prefetch.end.HighPart = 0;

    /* release mutex on contents */
    lock_ReleaseMutex(&scp->mx);

    /* we're done */
    return 0;
}       

long cm_Rename(cm_scache_t *oldDscp, char *oldNamep, cm_scache_t *newDscp,
                char *newNamep, cm_user_t *userp, cm_req_t *reqp)
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
    struct rx_connection * callp;

    /* before starting the RPC, mark that we're changing the directory data,
     * so that someone who does a chmod on the dir will wait until our call
     * completes.  We do this in vnode order so that we don't deadlock,
     * which makes the code a little verbose.
     */
    if (oldDscp == newDscp) {
        /* check for identical names */
        if (strcmp(oldNamep, newNamep) == 0)
            return CM_ERROR_RENAME_IDENTICAL;

        oneDir = 1;
        lock_ObtainMutex(&oldDscp->mx);
        cm_dnlcRemove(oldDscp, oldNamep);
        cm_dnlcRemove(oldDscp, newNamep);
        code = cm_SyncOp(oldDscp, NULL, userp, reqp, 0,
                          CM_SCACHESYNC_STOREDATA);
        lock_ReleaseMutex(&oldDscp->mx);
    }
    else {
        /* two distinct dir vnodes */
        oneDir = 0;
        if (oldDscp->fid.cell != newDscp->fid.cell ||
             oldDscp->fid.volume != newDscp->fid.volume)
            return CM_ERROR_CROSSDEVLINK;

        /* shouldn't happen that we have distinct vnodes for two
         * different files, but could due to deliberate attack, or
         * stale info.  Avoid deadlocks and quit now.
         */
        if (oldDscp->fid.vnode == newDscp->fid.vnode)
            return CM_ERROR_CROSSDEVLINK;

        if (oldDscp->fid.vnode < newDscp->fid.vnode) {
            lock_ObtainMutex(&oldDscp->mx);
            cm_dnlcRemove(oldDscp, oldNamep);
            code = cm_SyncOp(oldDscp, NULL, userp, reqp, 0,
                              CM_SCACHESYNC_STOREDATA);
            lock_ReleaseMutex(&oldDscp->mx);
            if (code == 0) {
                lock_ObtainMutex(&newDscp->mx);
                cm_dnlcRemove(newDscp, newNamep);
                code = cm_SyncOp(newDscp, NULL, userp, reqp, 0,
                                  CM_SCACHESYNC_STOREDATA);
                lock_ReleaseMutex(&newDscp->mx);
                if (code) {
                    /* cleanup first one */
                    lock_ObtainMutex(&newDscp->mx);
                    cm_SyncOpDone(oldDscp, NULL,
                                   CM_SCACHESYNC_STOREDATA);
                    lock_ReleaseMutex(&oldDscp->mx);
                }       
            }
        }
        else {
            /* lock the new vnode entry first */
            lock_ObtainMutex(&newDscp->mx);
            cm_dnlcRemove(newDscp, newNamep);
            code = cm_SyncOp(newDscp, NULL, userp, reqp, 0,
                              CM_SCACHESYNC_STOREDATA);
            lock_ReleaseMutex(&newDscp->mx);
            if (code == 0) {
                lock_ObtainMutex(&oldDscp->mx);
                cm_dnlcRemove(oldDscp, oldNamep);
                code = cm_SyncOp(oldDscp, NULL, userp, reqp, 0,
                                  CM_SCACHESYNC_STOREDATA);
                lock_ReleaseMutex(&oldDscp->mx);
                if (code) {
                    /* cleanup first one */
                    lock_ObtainMutex(&newDscp->mx);
                    cm_SyncOpDone(newDscp, NULL,
                                   CM_SCACHESYNC_STOREDATA);
                    lock_ReleaseMutex(&newDscp->mx);
                }       
            }
        }
    }	/* two distinct vnodes */

    if (code) {
        return code;
    }
    didEnd = 0;

    /* try the RPC now */
    osi_Log2(afsd_logp, "CALL Rename old scp 0x%x new scp 0x%x", 
              (long) oldDscp, (long) newDscp);
    do {
        code = cm_Conn(&oldDscp->fid, userp, reqp, &connp);
        if (code) 
            continue;

        oldDirAFSFid.Volume = oldDscp->fid.volume;
        oldDirAFSFid.Vnode = oldDscp->fid.vnode;
        oldDirAFSFid.Unique = oldDscp->fid.unique;
        newDirAFSFid.Volume = newDscp->fid.volume;
        newDirAFSFid.Vnode = newDscp->fid.vnode;
        newDirAFSFid.Unique = newDscp->fid.unique;

        callp = cm_GetRxConn(connp);
        code = RXAFS_Rename(callp, &oldDirAFSFid, oldNamep,
                             &newDirAFSFid, newNamep,
                             &updatedOldDirStatus, &updatedNewDirStatus,
                             &volSync);
        rx_PutConnection(callp);

    } while (cm_Analyze(connp, userp, reqp, &oldDscp->fid,
                         &volSync, NULL, NULL, code));
    code = cm_MapRPCError(code, reqp);
        
    if (code)
        osi_Log1(afsd_logp, "CALL Rename FAILURE, code 0x%x", code);
    else
        osi_Log0(afsd_logp, "CALL Rename SUCCESS");

    /* update the individual stat cache entries for the directories */
    lock_ObtainMutex(&oldDscp->mx);
    cm_SyncOpDone(oldDscp, NULL, CM_SCACHESYNC_STOREDATA);
    if (code == 0) {
        cm_MergeStatus(oldDscp, &updatedOldDirStatus, &volSync,
                        userp, 0);
    }
    lock_ReleaseMutex(&oldDscp->mx);

    /* and update it for the new one, too, if necessary */
    if (!oneDir) {
        lock_ObtainMutex(&newDscp->mx);
        cm_SyncOpDone(newDscp, NULL, CM_SCACHESYNC_STOREDATA);
        if (code == 0) {
            cm_MergeStatus(newDscp, &updatedNewDirStatus, &volSync,
                            userp, 0);
        }
        lock_ReleaseMutex(&newDscp->mx);
    }

    /* and return error code */
    return code;
}

long cm_Lock(cm_scache_t *scp, unsigned char LockType,
              LARGE_INTEGER LOffset, LARGE_INTEGER LLength,
              u_long Timeout, cm_user_t *userp, cm_req_t *reqp,
              void **lockpp)
{
    long code;
    int Which = ((LockType & LOCKING_ANDX_SHARED_LOCK) ? LockRead : LockWrite);
    AFSFid tfid;
    AFSVolSync volSync;
    cm_conn_t *connp;
    cm_file_lock_t *fileLock;
    osi_queue_t *q;
    int found = 0;
    struct rx_connection * callp;

    /* Look for a conflict.  Also, if we are asking for a shared lock,
     * look for another shared lock, so we don't have to do an RPC.
     */
    q = scp->fileLocks;
    while (q) {
        fileLock = (cm_file_lock_t *)((char *) q - offsetof(cm_file_lock_t, fileq));
        if ((fileLock->flags & (CM_FILELOCK_FLAG_INVALID | CM_FILELOCK_FLAG_WAITING)) == 0) {
            if ((LockType & LOCKING_ANDX_SHARED_LOCK) == 0 ||
                (fileLock->LockType & LOCKING_ANDX_SHARED_LOCK) == 0)
                return CM_ERROR_WOULDBLOCK;
            found = 1;
        }
        q = osi_QNext(q);
    }

    if (found)
        code = 0;
    else {
        tfid.Volume = scp->fid.volume;
        tfid.Vnode = scp->fid.vnode;
        tfid.Unique = scp->fid.unique;
        lock_ReleaseMutex(&scp->mx);
        do {
            code = cm_Conn(&scp->fid, userp, reqp, &connp);
            if (code) 
                break;

            callp = cm_GetRxConn(connp);
            code = RXAFS_SetLock(callp, &tfid, Which,
                                  &volSync);
            rx_PutConnection(callp);

        } while (cm_Analyze(connp, userp, reqp, &scp->fid, &volSync,
                             NULL, NULL, code));
        lock_ObtainMutex(&scp->mx);
        code = cm_MapRPCError(code, reqp);
    }

    if (code == 0 || Timeout != 0) {
        fileLock = malloc(sizeof(cm_file_lock_t));
        fileLock->LockType = LockType;
        cm_HoldUser(userp);
        fileLock->userp = userp;
        fileLock->fid = scp->fid;
        fileLock->LOffset = LOffset;
        fileLock->LLength = LLength;
        fileLock->flags = (code == 0 ? 0 : CM_FILELOCK_FLAG_WAITING);
        osi_QAdd(&scp->fileLocks, &fileLock->fileq);
        lock_ObtainWrite(&cm_scacheLock);
        osi_QAdd(&cm_allFileLocks, &fileLock->q);
        lock_ReleaseWrite(&cm_scacheLock);
        if (code != 0) 
            *lockpp = fileLock;
    }
    return code;
}

long cm_Unlock(cm_scache_t *scp, unsigned char LockType,
                LARGE_INTEGER LOffset, LARGE_INTEGER LLength,
                cm_user_t *userp, cm_req_t *reqp)
{
    long code = 0;
    int Which = ((LockType & LOCKING_ANDX_SHARED_LOCK) ? LockRead : LockWrite);
    AFSFid tfid;
    AFSVolSync volSync;
    cm_conn_t *connp;
    cm_file_lock_t *fileLock, *ourLock;
    osi_queue_t *q, *qq;
    int anotherReader = 0;
    int smallLock = 0;
    int found = 0;
    struct rx_connection * callp;

    if (LargeIntegerLessThan(LLength, scp->length))
        smallLock = 1;

    /* Look for our own lock on the list, so as to remove it.
     * Also, determine if we're the last reader; if not, avoid an RPC.
     */
    q = scp->fileLocks;
    while (q) {
        fileLock = (cm_file_lock_t *)
            ((char *) q - offsetof(cm_file_lock_t, fileq));
        if (!found
             && fileLock->userp == userp
             && LargeIntegerEqualTo(fileLock->LOffset, LOffset)
             && LargeIntegerEqualTo(fileLock->LLength, LLength)) {
            found = 1;
            ourLock = fileLock;
            qq = q;
        }
        else if (fileLock->LockType & LOCKING_ANDX_SHARED_LOCK)
            anotherReader = 1;
        q = osi_QNext(q);
    }

    /* ignore byte ranges */
    if (smallLock && !found)
        return 0;

    /* don't try to unlock other people's locks */
    if (!found)
        return CM_ERROR_WOULDBLOCK;

    /* discard lock record */
    osi_QRemove(&scp->fileLocks, qq);
    /*
     * Don't delete it here; let the daemon delete it, to simplify
     * the daemon's traversal of the list.
     */
    lock_ObtainWrite(&cm_scacheLock);
    ourLock->flags |= CM_FILELOCK_FLAG_INVALID;
    cm_ReleaseUser(ourLock->userp);
    lock_ReleaseWrite(&cm_scacheLock);

    if (!anotherReader) {
        tfid.Volume = scp->fid.volume;
        tfid.Vnode = scp->fid.vnode;
        tfid.Unique = scp->fid.unique;
        lock_ReleaseMutex(&scp->mx);
        osi_Log1(afsd_logp, "CALL ReleaseLock scp 0x%x", (long) scp);
        do {
            code = cm_Conn(&scp->fid, userp, reqp, &connp);
            if (code) 
                break;

            callp = cm_GetRxConn(connp);
            code = RXAFS_ReleaseLock(callp, &tfid, &volSync);
            rx_PutConnection(callp);

        } while (cm_Analyze(connp, userp, reqp, &scp->fid, &volSync,
                             NULL, NULL, code));
        code = cm_MapRPCError(code, reqp);

        if (code)
            osi_Log1(afsd_logp, "CALL ReleaseLock FAILURE, code 0x%x", code);
        else
            osi_Log0(afsd_logp, "CALL ReleaseLock SUCCESS");

        lock_ObtainMutex(&scp->mx);
    }

    return code;
}

void cm_CheckLocks()
{
    osi_queue_t *q, *nq;
    cm_file_lock_t *fileLock;
    cm_req_t req;
    AFSFid tfid;
    AFSVolSync volSync;
    cm_conn_t *connp;
    long code;
    struct rx_connection * callp;

    cm_InitReq(&req);

    lock_ObtainWrite(&cm_scacheLock);
    q = cm_allFileLocks;
    while (q) {
        fileLock = (cm_file_lock_t *) q;
        nq = osi_QNext(q);
        if (fileLock->flags & CM_FILELOCK_FLAG_INVALID) {
            osi_QRemove(&cm_allFileLocks, q);
            free(fileLock);
        }
        else if (!(fileLock->flags & CM_FILELOCK_FLAG_WAITING)) {
            tfid.Volume = fileLock->fid.volume;
            tfid.Vnode = fileLock->fid.vnode;
            tfid.Unique = fileLock->fid.unique;
            lock_ReleaseWrite(&cm_scacheLock);
            osi_Log1(afsd_logp, "CALL ExtendLock lock 0x%x", (long) fileLock);
            do {
                code = cm_Conn(&fileLock->fid, fileLock->userp,
                                &req, &connp);
                if (code) 
                    break;

                callp = cm_GetRxConn(connp);
                code = RXAFS_ExtendLock(callp, &tfid,
                                         &volSync);
                rx_PutConnection(callp);

            } while (cm_Analyze(connp, fileLock->userp, &req,
                                 &fileLock->fid, &volSync, NULL, NULL,
                                 code));
            code = cm_MapRPCError(code, &req);
            if (code)
                osi_Log1(afsd_logp, "CALL ExtendLock FAILURE, code 0x%x", code);
            else
                osi_Log0(afsd_logp, "CALL ExtendLock SUCCESS");

            lock_ObtainWrite(&cm_scacheLock);
        }
        q = nq;
    }
    lock_ReleaseWrite(&cm_scacheLock);
}       

long cm_RetryLock(cm_file_lock_t *oldFileLock, int vcp_is_dead)
{
    long code;
    int Which = ((oldFileLock->LockType & LOCKING_ANDX_SHARED_LOCK) ? LockRead : LockWrite);
    cm_scache_t *scp;
    AFSFid tfid;
    AFSVolSync volSync;
    cm_conn_t *connp;
    cm_file_lock_t *fileLock;
    osi_queue_t *q;
    cm_req_t req;
    int found = 0;
    struct rx_connection * callp;

    if (vcp_is_dead) {
        code = CM_ERROR_TIMEDOUT;
        goto handleCode;
    }

    cm_InitReq(&req);

    /* Look for a conflict.  Also, if we are asking for a shared lock,
     * look for another shared lock, so we don't have to do an RPC.
     */
    code = cm_GetSCache(&oldFileLock->fid, &scp, oldFileLock->userp, &req);
    if (code)
        return code;

    q = scp->fileLocks;
    while (q) {
        fileLock = (cm_file_lock_t *)
            ((char *) q - offsetof(cm_file_lock_t, fileq));
        if ((fileLock->flags &
              (CM_FILELOCK_FLAG_INVALID | CM_FILELOCK_FLAG_WAITING))
             == 0) {
            if ((oldFileLock->LockType & LOCKING_ANDX_SHARED_LOCK) == 0
                 || (fileLock->LockType & LOCKING_ANDX_SHARED_LOCK) == 0) {
                cm_ReleaseSCache(scp);
                return CM_ERROR_WOULDBLOCK;
            }
            found = 1;
        }
        q = osi_QNext(q);
    }

    if (found)
        code = 0;
    else {
        tfid.Volume = oldFileLock->fid.volume;
        tfid.Vnode = oldFileLock->fid.vnode;
        tfid.Unique = oldFileLock->fid.unique;
        osi_Log1(afsd_logp, "CALL SetLock lock 0x%x", (long) oldFileLock);
        do {
            code = cm_Conn(&oldFileLock->fid, oldFileLock->userp,
                            &req, &connp);
            if (code) 
                break;

            callp = cm_GetRxConn(connp);
            code = RXAFS_SetLock(callp, &tfid, Which,
                                  &volSync);
            rx_PutConnection(callp);

        } while (cm_Analyze(connp, oldFileLock->userp, &req,
                             &oldFileLock->fid, &volSync,
                             NULL, NULL, code));
        code = cm_MapRPCError(code, &req);

        if (code)
            osi_Log1(afsd_logp, "CALL SetLock FAILURE, code 0x%x", code);
        else
            osi_Log0(afsd_logp, "CALL SetLock SUCCESS");
    }

  handleCode:
    if (code != 0 && code != CM_ERROR_WOULDBLOCK) {
        lock_ObtainMutex(&scp->mx);
        osi_QRemove(&scp->fileLocks, &oldFileLock->fileq);
        lock_ReleaseMutex(&scp->mx);
    }
    lock_ObtainWrite(&cm_scacheLock);
    if (code == 0)
        oldFileLock->flags = 0;
    else if (code != CM_ERROR_WOULDBLOCK) {
        oldFileLock->flags |= CM_FILELOCK_FLAG_INVALID;
        cm_ReleaseUser(oldFileLock->userp);
        oldFileLock->userp = NULL;
    }
    lock_ReleaseWrite(&cm_scacheLock);

    return code;
}
