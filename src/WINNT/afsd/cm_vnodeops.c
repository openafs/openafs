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
#include <errno.h>

#include <osi.h>

#include "afsd.h"
#include "cm_btree.h"

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

void cm_Gen8Dot3NameInt(const char * longname, cm_dirFid_t * pfid,
                        char *shortName, char **shortNameEndp)
{
    char number[12];
    int i, nsize = 0;
    int vnode = ntohl(pfid->vnode);
    char *lastDot;
    int validExtension = 0;
    char tc, *temp;
    const char *name;

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
    lastDot = strrchr(longname, '.');
    if (lastDot) {
        temp = lastDot; temp++;
        while (tc = *temp++)
            if (cm_LegalChars[tc])
                break;
        if (tc)
            validExtension = 1;
    }

    /* Copy name characters */
    for (i = 0, name = longname;
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
    if (openMode != 1) 
	rights |= PRSFS_READ;
    if (openMode == 1 || openMode == 2 || trunc) 
	rights |= PRSFS_WRITE;
        
    lock_ObtainMutex(&scp->mx);

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
            cm_Unlock(scp, sLockType, LOffset, LLength, key, userp, reqp);
        } else {
            /* In this case, we allow the file open to go through even
               though we can't enforce mandatory locking on the
               file. */
            if (code == CM_ERROR_NOACCESS &&
                !(rights & PRSFS_WRITE))
                code = 0;
            else {
		switch (code) {
		case CM_ERROR_ALLOFFLINE:
		case CM_ERROR_ALLDOWN:
		case CM_ERROR_ALLBUSY:
		case CM_ERROR_TIMEDOUT:
		case CM_ERROR_RETRY:
		case CM_ERROR_WOULDBLOCK:
		    break;
		default:
		    code = CM_ERROR_SHARING_VIOLATION;
		}
	    }
        }

    } else if (code != 0) {
        goto _done;
    }

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_LOCK);

 _done:

    lock_ReleaseMutex(&scp->mx);

    return code;
}

/* return success if we can open this file in this mode */
long cm_CheckNTOpen(cm_scache_t *scp, unsigned int desiredAccess,
                    unsigned int createDisp, cm_user_t *userp, cm_req_t *reqp, 
		    cm_lock_data_t **ldpp)
{
    long rights;
    long code;

    osi_assertx(ldpp != NULL, "null cm_lock_data_t");
    *ldpp = NULL;

    /* Always allow delete; the RPC will tell us if it's OK */
    if (desiredAccess == DELETE)
        return 0;

    rights = 0;

    if (desiredAccess & AFS_ACCESS_READ)
        rights |= (scp->fileType == CM_SCACHETYPE_DIRECTORY ? PRSFS_LOOKUP : PRSFS_READ);

    /* We used to require PRSFS_WRITE if createDisp was 4
       (OPEN_ALWAYS) even if AFS_ACCESS_WRITE was not requested.
       However, we don't need to do that since the existence of the
       scp implies that we don't need to create it. */
    if (desiredAccess & AFS_ACCESS_WRITE)
        rights |= PRSFS_WRITE;

    lock_ObtainMutex(&scp->mx);

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
		switch (code) {
		case CM_ERROR_ALLOFFLINE:
		case CM_ERROR_ALLDOWN:
		case CM_ERROR_ALLBUSY:
		case CM_ERROR_TIMEDOUT:
		case CM_ERROR_RETRY:
		case CM_ERROR_WOULDBLOCK:
		    break;
		default:
		    code = CM_ERROR_SHARING_VIOLATION;
		}
	    }
        }
    } else if (code != 0) {
        goto _done;
    }

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_LOCK);

 _done:
    lock_ReleaseMutex(&scp->mx);

    return code;
}

extern long cm_CheckNTOpenDone(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp, 
			       cm_lock_data_t ** ldpp)
{
    if (*ldpp) {
	lock_ObtainMutex(&scp->mx);
	cm_Unlock(scp, (*ldpp)->sLockType, (*ldpp)->LOffset, (*ldpp)->LLength, 
		  (*ldpp)->key, userp, reqp);
	lock_ReleaseMutex(&scp->mx);
	free(*ldpp);
	*ldpp = NULL;
    }
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

    /* First check permissions */
    lock_ObtainMutex(&scp->mx);
    code = cm_SyncOp(scp, NULL, userp, reqp, PRSFS_DELETE,
                      CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_NEEDCALLBACK);
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseMutex(&scp->mx);
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
	cm_SyncOpDone(scp, bufferp, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_READ | CM_SCACHESYNC_BUFLOCKED);
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
    lock_ObtainMutex(&scp->mx);
    code = cm_SyncOp(scp, NULL, userp, reqp, PRSFS_LOOKUP,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseMutex(&scp->mx);
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

            /* see if we can find it using the directory hash tables.
               we can only do exact matches, since the hash is case
               sensitive. */
            {
                cm_dirOp_t dirop;
#ifdef USE_BPLUS
                int usedBplus = 0;
#endif

                code = ENOENT;

                code = cm_BeginDirOp(scp, userp, reqp, CM_DIRLOCK_READ, &dirop);
                if (code == 0) {

#ifdef USE_BPLUS
                    code = cm_BPlusDirLookup(&dirop, sp->searchNamep, &sp->fid);
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

            lock_ObtainRead(&scp->bufCreateLock);
            code = buf_Get(scp, &thyper, &bufferp);
            lock_ReleaseRead(&scp->bufCreateLock);
            if (code) {
                /* if buf_Get() fails we do not have a buffer object to lock */
                bufferp = NULL;
                break;
            }

#ifdef AFSIFS
	    /* for the IFS version, we bulkstat the dirents because this
	       routine is used in place of smb_ReceiveCoreSearchDir.  our
	       other option is to modify smb_ReceiveCoreSearchDir itself, 
	       but this seems to be the proper use for cm_ApplyDir. */
            lock_ObtainMutex(&scp->mx);
            if ((scp->flags & CM_SCACHEFLAG_BULKSTATTING) == 0
                 && (scp->bulkStatProgress.QuadPart <= thyper.QuadPart))
            {
                scp->flags |= CM_SCACHEFLAG_BULKSTATTING;
                code = cm_TryBulkStat(scp, &thyper, userp, reqp);
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
		cm_SyncOpDone(scp, bufferp, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_READ | CM_SCACHESYNC_BUFLOCKED);
                                
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
    if (!sp->caseFold) 
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
    if (code)
        return code;

    while (1) {
        code = cm_SyncOp(scp, bufp, userp, reqp, 0,
                          CM_SCACHESYNC_READ | CM_SCACHESYNC_NEEDCALLBACK);
        if (code)
            goto done;

	cm_SyncOpDone(scp, bufp, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_READ);

        if (cm_HaveBuffer(scp, bufp, 0)) 
            break;

        /* otherwise load buffer */
        code = cm_GetBuffer(scp, bufp, NULL, userp, reqp);
        if (code)
            goto done;
    }
    /* locked, has callback, has valid data in buffer */
    if ((tlen = scp->length.LowPart) > MOUNTPOINTLEN - 1) 
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
    cm_volume_t *volp = NULL;
    cm_cell_t *cellp;
    char mtType;
    cm_fid_t tfid;
    size_t vnLength;
    int targetType;

    if (scp->mountRootFid.cell != 0 && scp->mountRootGen >= cm_data.mountRootGen) {
        tfid = scp->mountRootFid;
        lock_ReleaseMutex(&scp->mx);
        code = cm_GetSCache(&tfid, outScpp, userp, reqp);
        lock_ObtainMutex(&scp->mx);
        return code;
    }

    /* parse the volume name */
    mpNamep = scp->mountPointStringp;
    if (!mpNamep[0])
	return CM_ERROR_NOSUCHPATH;
    tlen = (int)strlen(scp->mountPointStringp);
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
        lock_ReleaseMutex(&scp->mx);
        cellp = cm_GetCell(cellNamep, CM_FLAG_CREATE);
        lock_ObtainMutex(&scp->mx);
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
        targetType = BACKVOL;
    else if (vnLength >= 10
              && strcmp(volNamep + vnLength - 9, ".readonly") == 0)
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
    lock_ReleaseMutex(&scp->mx);
    if (cm_VolNameIsID(volNamep)) {
        code = cm_GetVolumeByID(cellp, atoi(volNamep), userp, reqp, 
                                CM_GETVOL_FLAG_CREATE, &volp);
    } else {
        code = cm_GetVolumeByName(cellp, volNamep, userp, reqp, 
                                  CM_GETVOL_FLAG_CREATE, &volp);
    }
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
        
        /* if the mt pt originates in a .backup volume (not a .readonly)
         * and FollowBackupPath is active, and if there is a .backup
         * volume for the target, then use the .backup of the target
         * instead of the read-write.
         */
        if (cm_followBackupPath && targetType == RWVOL &&
            (scp->flags & CM_SCACHEFLAG_RO|CM_SCACHEFLAG_PURERO) == CM_SCACHEFLAG_RO &&
            volp->bk.ID != 0) {
            targetType = BACKVOL;
        } 
        /* if the mt pt is in a read-only volume (not just a
         * backup), and if there is a read-only volume for the
         * target, and if this is a targetType '#' mount point, use
         * the read-only, otherwise use the one specified.
         */
        else if (mtType == '#' && targetType == RWVOL && 
                 (scp->flags & CM_SCACHEFLAG_PURERO) && 
                 volp->ro.ID != 0) {
            targetType = ROVOL;
        }
        if (targetType == ROVOL)
            scp->mountRootFid.volume = volp->ro.ID;
        else if (targetType == BACKVOL)
            scp->mountRootFid.volume = volp->bk.ID;
        else
            scp->mountRootFid.volume = volp->rw.ID;

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
    if (volp)
	cm_PutVolume(volp);
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

    memset(&rock, 0, sizeof(rock));

    if (dscp->fid.vnode == 1 && dscp->fid.unique == 1
         && strcmp(namep, "..") == 0) {
        if (dscp->dotdotFid.volume == 0)
            return CM_ERROR_NOSUCHVOLUME;
        rock.fid = dscp->dotdotFid;
        goto haveFid;
    } else if (strcmp(namep, ".") == 0) {
	rock.fid = dscp->fid;
	goto haveFid;
    }

    if (flags & CM_FLAG_NOMOUNTCHASE) {
        /* In this case, we should go and call cm_Dir* functions
           directly since the following cm_ApplyDir() function will
           not. */

        cm_dirOp_t dirop;
#ifdef USE_BPLUS
        int usedBplus = 0;
#endif

        code = cm_BeginDirOp(dscp, userp, reqp, CM_DIRLOCK_READ, &dirop);
        if (code == 0) {
#ifdef USE_BPLUS
            code = cm_BPlusDirLookup(&dirop, namep, &rock.fid);
            if (code != EINVAL)
                usedBplus = 1;
            else
#endif
                code = cm_DirLookup(&dirop, namep, &rock.fid);

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
            
            return CM_ERROR_BPLUS_NOMATCH;
        }
#endif
    }

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
    cm_SyncOpDone(tscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
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
        if ( dscp->cbServerp != NULL && dscp->cbExpires > 0 )
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
    prefixCount = (int)(tp - inp);

    strncpy(outp, inp, prefixCount);	/* copy out "a." from "a.@sys" */
    outp[prefixCount] = 0;		/* null terminate the "a." */
    strcat(outp, cm_sysNameList[index]);/* append i386_nt40 */
    return 1;
}   

long cm_EvaluateVolumeReference(char * namep, long flags, cm_user_t * userp,
                                cm_req_t *reqp, cm_scache_t ** outpScpp)
{
    long          code = 0;
    char          cellName[CELL_MAXNAMELEN];
    char          volumeName[VL_MAXNAMELEN];
    size_t        len;
    char *        cp;
    char *        tp;

    cm_cell_t *   cellp = NULL;
    cm_volume_t * volp = NULL;
    cm_fid_t      fid;
    int           volType;
    int           mountType = RWVOL;

    osi_Log1(afsd_logp, "cm_EvaluateVolumeReference for string [%s]",
             osi_LogSaveString(afsd_logp, namep));

    if (strnicmp(namep, CM_PREFIX_VOL, CM_PREFIX_VOL_CCH) != 0) {
        goto _exit_invalid_path;
    }

    /* namep is assumed to look like the following:

       @vol:<cellname>%<volume>\0
       or
       @vol:<cellname>#<volume>\0

     */

    cp = namep + CM_PREFIX_VOL_CCH; /* cp points to cell name, hopefully */
    tp = strchr(cp, '%');
    if (tp == NULL)
        tp = strchr(cp, '#');
    if (tp == NULL ||
        (len = tp - cp) == 0 ||
        len > CELL_MAXNAMELEN)
        goto _exit_invalid_path;
    strncpy(cellName, cp, len);
    cellName[len] = '\0';

    if (*tp == '#')
        mountType = ROVOL;

    cp = tp+1;                  /* cp now points to volume, supposedly */
    strncpy(volumeName, cp, VL_MAXNAMELEN-1);
    volumeName[VL_MAXNAMELEN - 1] = 0;

    /* OK, now we have the cell and the volume */
    osi_Log2(afsd_logp, "   Found cell [%s] and volume [%s]",
             osi_LogSaveString(afsd_logp, cellName),
             osi_LogSaveString(afsd_logp, volumeName));

    cellp = cm_GetCell(cellName, CM_FLAG_CREATE);
    if (cellp == NULL) {
        goto _exit_invalid_path;
    }

    len = strlen(volumeName);
    if (len >= 8 && strcmp(volumeName + len - 7, ".backup") == 0)
        volType = BACKVOL;
    else if (len >= 10 &&
             strcmp(volumeName + len - 9, ".readonly") == 0)
        volType = ROVOL;
    else
        volType = RWVOL;

    if (cm_VolNameIsID(volumeName)) {
        code = cm_GetVolumeByID(cellp, atoi(volumeName), userp, reqp,
                                CM_GETVOL_FLAG_CREATE, &volp);
    } else {
        code = cm_GetVolumeByName(cellp, volumeName, userp, reqp,
                                  CM_GETVOL_FLAG_CREATE, &volp);
    }

    if (code != 0)
        goto _exit_cleanup;

    fid.cell = cellp->cellID;

    if (volType == BACKVOL)
        fid.volume = volp->bk.ID;
    else if (volType == ROVOL ||
             (volType == RWVOL && mountType == ROVOL && volp->ro.ID != 0))
        fid.volume = volp->ro.ID;
    else
        fid.volume = volp->rw.ID;

    fid.vnode = 1;
    fid.unique = 1;

    code = cm_GetSCache(&fid, outpScpp, userp, reqp);

 _exit_cleanup:
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
long cm_LookupDbg(cm_scache_t *dscp, char *namep, long flags, cm_user_t *userp,
               cm_req_t *reqp, cm_scache_t **outpScpp, char * file, long line)
#else
long cm_Lookup(cm_scache_t *dscp, char *namep, long flags, cm_user_t *userp,
               cm_req_t *reqp, cm_scache_t **outpScpp)
#endif
{
    long code;
    char tname[AFSPATHMAX];
    int sysNameIndex = 0;
    cm_scache_t *scp = NULL;

#ifdef DEBUG_REFCOUNT
    afsi_log("%s:%d cm_Lookup dscp 0x%p ref %d", file, line, dscp, dscp->refCount, file, line);
    osi_Log2(afsd_logp, "cm_Lookup dscp 0x%p ref %d", dscp, dscp->refCount);
#endif

    if ( stricmp(namep,SMB_IOCTL_FILENAME_NOSLASH) == 0 ) {
        if (flags & CM_FLAG_CHECKPATH)
            return CM_ERROR_NOSUCHPATH;
        else
            return CM_ERROR_NOSUCHFILE;
    }

    if (dscp == cm_data.rootSCachep &&
        strnicmp(namep, CM_PREFIX_VOL, CM_PREFIX_VOL_CCH) == 0) {
        return cm_EvaluateVolumeReference(namep, flags, userp, reqp, outpScpp);
    }

    if (cm_ExpandSysName(namep, NULL, 0, 0) > 0) {
        for ( sysNameIndex = 0; sysNameIndex < MAXNUMSYSNAMES; sysNameIndex++) {
            code = cm_ExpandSysName(namep, tname, sizeof(tname), sysNameIndex);
            if (code > 0) {
                code = cm_LookupInternal(dscp, tname, flags, userp, reqp, &scp);
#ifdef DEBUG_REFCOUNT
                afsi_log("%s:%d cm_LookupInternal (1) code 0x%x dscp 0x%p ref %d scp 0x%p ref %d", file, line, code, dscp, dscp->refCount, scp, scp ? scp->refCount : 0);
                osi_Log3(afsd_logp, "cm_LookupInternal (1) code 0x%x dscp 0x%p scp 0x%p", code, dscp, scp);
#endif

                if (code == 0) {
                    *outpScpp = scp;
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
                *outpScpp = scp;
                return code;
            }
        }
    } else {
        code = cm_LookupInternal(dscp, namep, flags, userp, reqp, &scp);
#ifdef DEBUG_REFCOUNT
        afsi_log("%s:%d cm_LookupInternal (2) code 0x%x dscp 0x%p ref %d scp 0x%p ref %d", file, line, code, dscp, dscp->refCount, scp, scp ? scp->refCount : 0);
        osi_Log3(afsd_logp, "cm_LookupInternal (2) code 0x%x dscp 0x%p scp 0x%p", code, dscp, scp);
#endif
        *outpScpp = scp;
        return code;
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
    cm_dirOp_t dirop;

#ifdef AFS_FREELANCE_CLIENT
    if (cm_freelanceEnabled && dscp == cm_data.rootSCachep) {
        /* deleting a mount point from the root dir. */
        code = cm_FreelanceRemoveMount(namep);
        return code;
    }
#endif  

    /* make sure we don't screw up the dir status during the merge */
    code = cm_BeginDirOp(dscp, userp, reqp, CM_DIRLOCK_NONE, &dirop);

    lock_ObtainMutex(&dscp->mx);
    sflags = CM_SCACHESYNC_STOREDATA;
    code = cm_SyncOp(dscp, NULL, userp, reqp, 0, sflags);
    lock_ReleaseMutex(&dscp->mx);
    if (code) {
        cm_EndDirOp(&dirop);
        return code;
    }

    /* make the RPC */
    afsFid.Volume = dscp->fid.volume;
    afsFid.Vnode = dscp->fid.vnode;
    afsFid.Unique = dscp->fid.unique;

    osi_Log1(afsd_logp, "CALL RemoveFile scp 0x%p", dscp);
    do {
        code = cm_ConnFromFID(&dscp->fid, userp, reqp, &connp);
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

    if (dirop.scp) {
        lock_ObtainWrite(&dirop.scp->dirlock);
        dirop.lockType = CM_DIRLOCK_WRITE;
    }
    lock_ObtainMutex(&dscp->mx);
    cm_dnlcRemove(dscp, namep);
    cm_SyncOpDone(dscp, NULL, sflags);
    if (code == 0) {
        cm_MergeStatus(NULL, dscp, &newDirStatus, &volSync, userp, CM_MERGEFLAG_DIROP);
    } else if (code == CM_ERROR_NOSUCHFILE) {
	/* windows would not have allowed the request to delete the file 
	 * if it did not believe the file existed.  therefore, we must 
	 * have an inconsistent view of the world.
	 */
	dscp->cbServerp = NULL;
    }
    lock_ReleaseMutex(&dscp->mx);

    if (code == 0 && cm_CheckDirOpForSingleChange(&dirop)) {
        cm_DirDeleteEntry(&dirop, namep);
#ifdef USE_BPLUS
        cm_BPlusDirDeleteEntry(&dirop, namep);
#endif
    }
    cm_EndDirOp(&dirop);

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
	    cm_SyncOpDone(linkScp, bufp, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_READ);

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

            if ( !strnicmp(linkScp->mountPointStringp, "msdfs:", strlen("msdfs:")) )
                 linkScp->fileType = CM_SCACHETYPE_DFSLINK;
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

    *newRootScpp = NULL;
    *newSpaceBufferp = NULL;

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
        if (!strnicmp(&linkp[2], cm_NetbiosName, (len = (long)strlen(cm_NetbiosName)))) 
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
            code = CM_ERROR_PATH_NOT_COVERED;
        }
    } else if ( linkScp->fileType == CM_SCACHETYPE_DFSLINK ||
                !strnicmp(linkp, "msdfs:", (len = (long)strlen("msdfs:"))) ) {
        linkScp->fileType = CM_SCACHETYPE_DFSLINK;
        strcpy(tsp->data, linkp);
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
        code = CM_ERROR_NOSUCHPATH;
#endif  
    } else {
        /* a relative link */
        strcpy(tsp->data, linkp);
    }
    if (pathSuffixp[0] != 0) {	/* if suffix string is non-null */
        strcat(tsp->data, "\\");
        strcat(tsp->data, pathSuffixp);
    }
    if (code == 0)
        *newSpaceBufferp = tsp;
    else {
        cm_FreeSpace(tsp);

        if (code == CM_ERROR_PATH_NOT_COVERED && reqp->tidPathp && reqp->relPathp)
            cm_VolStatus_Notify_DFS_Mapping(linkScp, reqp->tidPathp, reqp->relPathp);
    }

  done:
    lock_ReleaseMutex(&linkScp->mx);
    return code;
}
#ifdef DEBUG_REFCOUNT
long cm_NameIDbg(cm_scache_t *rootSCachep, char *pathp, long flags,
               cm_user_t *userp, char *tidPathp, cm_req_t *reqp, cm_scache_t **outScpp, 
	       char * file, long line)
#else
long cm_NameI(cm_scache_t *rootSCachep, char *pathp, long flags,
               cm_user_t *userp, char *tidPathp, cm_req_t *reqp, cm_scache_t **outScpp)
#endif
{
    long code;
    char *tp;			/* ptr moving through input buffer */
    char tc;			/* temp char */
    int haveComponent;		/* has new component started? */
    char component[AFSPATHMAX];	/* this is the new component */
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
#define MAX_FID_COUNT 512
    cm_fid_t fids[MAX_FID_COUNT]; /* array of fids processed in this path walk */
    int fid_count = 0;          /* number of fids processed in this path walk */
    int i;

#ifdef DEBUG_REFCOUNT
    afsi_log("%s:%d cm_NameI rootscp 0x%p ref %d", file, line, rootSCachep, rootSCachep->refCount);
    osi_Log4(afsd_logp,"cm_NameI rootscp 0x%p path %s tidpath %s flags 0x%x",
	      rootSCachep, pathp ? pathp : "<NULL>", tidPathp ? tidPathp : "<NULL>", 
	      flags);
#endif

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
                    if (!strcmp(component,"..") || !strcmp(component,".")) {
                        /* 
                         * roll back the fid list until we find the fid 
                         * that matches where we are now.  Its not necessarily
                         * one or two fids because they might have been 
                         * symlinks or mount points or both that were crossed.  
                         */
                        for ( i=fid_count-1; i>=0; i--) {
                            if (!cm_FidCmp(&nscp->fid, &fids[i]))
                                break;
                        }
                    } else {
                        /* add the new fid to the list */
                        for ( i=0; i<fid_count; i++) {
                            if ( !cm_FidCmp(&nscp->fid, &fids[i]) ) {
                                code = CM_ERROR_TOO_MANY_SYMLINKS;
                                cm_ReleaseSCache(nscp);
                                nscp = NULL;
                                break;
                            }
                        }
                        if (i == fid_count && fid_count < MAX_FID_COUNT) {
                            fids[fid_count++] = nscp->fid;
                        }
                    }
                }

                if (code) {
		    cm_ReleaseSCache(tscp);
		    if (dirScp)
			cm_ReleaseSCache(dirScp);
		    if (psp) 
			cm_FreeSpace(psp);
		    if ((code == CM_ERROR_NOSUCHFILE || code == CM_ERROR_BPLUS_NOMATCH) && 
                         tscp->fileType == CM_SCACHETYPE_SYMLINK) 
                    {
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
                    lock_ReleaseMutex(&tscp->mx);
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
                        restp = "";
                    else 
                        restp = tp;
                    code = cm_AssembleLink(tscp, restp, &linkScp, &tempsp, userp, reqp);

                    if (code == 0 && linkScp != NULL) {
                        if (linkScp == cm_data.rootSCachep) 
                            fid_count = 0;
                        else {
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
                    tp = psp->data;
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
                    lock_ReleaseMutex(&tscp->mx);
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
    afsi_log("%s:%d cm_NameI code 0x%x outScpp 0x%p ref %d", file, line, code, *outScpp, (*outScpp)->refCount);
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

    code = cm_NameI(newRootScp, spacep->data,
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

/* make this big enough so that one buffer of dir pages won't overflow.  We'll
 * check anyway, but we want to minimize the chance that we have to leave stuff
 * unstat'd.
 */
#define CM_BULKMAX		(3 * AFSCBMAX)

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
            if (!(tscp->flags & CM_SCACHEFLAG_EACCESS) && cm_HaveCallback(tscp)) {
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
afs_int32
cm_TryBulkStat(cm_scache_t *dscp, osi_hyper_t *offsetp, cm_user_t *userp,
	       cm_req_t *reqp)
{
    long code;
    cm_bulkStat_t bb;	/* this is *BIG*, probably 16K or so;
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
    int inlinebulk = 0;		/* Did we use InlineBulkStatus RPC or not? */

    osi_Log1(afsd_logp, "cm_TryBulkStat dir 0x%p", dscp);

    /* should be on a buffer boundary */
    osi_assertx((offsetp->LowPart & (cm_data.buf_blockSize - 1)) == 0, "invalid offset");

    memset(&bb, 0, sizeof(bb));
    bb.bufOffset = *offsetp;

    lock_ReleaseMutex(&dscp->mx);
    /* first, assemble the file IDs we need to stat */
    code = cm_ApplyDir(dscp, cm_TryBulkProc, (void *) &bb, offsetp, userp, reqp, NULL);

    /* if we failed, bail out early */
    if (code && code != CM_ERROR_STOPNOW) {
        lock_ObtainMutex(&dscp->mx);
        return code;
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
            code = cm_ConnFromFID(&dscp->fid, userp, reqp, &connp);
            if (code) 
                continue;

            callp = cm_GetRxConn(connp);
	    if (!(connp->serverp->flags & CM_SERVERFLAG_NOINLINEBULK)) {
		code = RXAFS_InlineBulkStatus(callp, &fidStruct,
                                     &statStruct, &callbackStruct, &volSync);
		if (code == RXGEN_OPCODE) {
		    cm_SetServerNoInlineBulk(connp->serverp, 0);
		} else {
		    inlinebulk = 1;
		}
	    }
	    if (!inlinebulk) {
		code = RXAFS_BulkStatus(callp, &fidStruct,
					&statStruct, &callbackStruct, &volSync);
	    }
            rx_PutConnection(callp);

        } while (cm_Analyze(connp, userp, reqp, &dscp->fid,
                             &volSync, NULL, &cbReq, code));
        code = cm_MapRPCError(code, reqp);
        if (code)
            osi_Log2(afsd_logp, "CALL %sBulkStatus FAILURE code 0x%x", 
		      inlinebulk ? "Inline" : "", code);
        else
            osi_Log1(afsd_logp, "CALL %sBulkStatus SUCCESS", inlinebulk ? "Inline" : "");

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
            if ((scp->cbServerp == NULL || (scp->flags & CM_SCACHEFLAG_EACCESS))
                 && !(scp->flags &
                       (CM_SCACHEFLAG_FETCHING
                         | CM_SCACHEFLAG_STORING
                         | CM_SCACHEFLAG_SIZESTORING))) {
                cm_EndCallbackGrantingCall(scp, &cbReq,
                                            &bb.callbacks[j],
                                            CM_CALLBACK_MAINTAINCOUNT);
                cm_MergeStatus(dscp, scp, &bb.stats[j], &volSync, userp, 0);
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

    /* If we did the InlineBulk RPC pull out the return code and log it */
    if (inlinebulk) {
	if ((&bb.stats[0])->errorCode) {
	    osi_Log1(afsd_logp, "cm_TryBulkStat bulk stat error: %d", 
		     (&bb.stats[0])->errorCode);
	}
    }

    osi_Log0(afsd_logp, "END cm_TryBulkStat");
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
    lock_ObtainMutex(&scp->mx);
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
    lock_ReleaseMutex(&scp->mx);
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
    struct rx_connection * callp;

    /* handle file length setting */
    if (attrp->mask & CM_ATTRMASK_LENGTH)
        return cm_SetLength(scp, &attrp->length, userp, reqp);

    lock_ObtainMutex(&scp->mx);
    /* otherwise, we have to make an RPC to get the status */
    code = cm_SyncOp(scp, NULL, userp, reqp, 0, CM_SCACHESYNC_STORESTATUS);
    if (code) {
	lock_ReleaseMutex(&scp->mx);
        return code;
    }

    /* make the attr structure */
    cm_StatusFromAttr(&afsInStatus, scp, attrp);

    tfid.Volume = scp->fid.volume;
    tfid.Vnode = scp->fid.vnode;
    tfid.Unique = scp->fid.unique;
	lock_ReleaseMutex(&scp->mx);

    /* now make the RPC */
    osi_Log1(afsd_logp, "CALL StoreStatus scp 0x%p", scp);
    do {
        code = cm_ConnFromFID(&scp->fid, userp, reqp, &connp);
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
        cm_MergeStatus(NULL, scp, &afsOutStatus, &volSync, userp,
                        CM_MERGEFLAG_FORCE|CM_MERGEFLAG_STOREDATA);
	
    /* if we're changing the mode bits, discard the ACL cache, 
     * since we changed the mode bits.
     */
    if (afsInStatus.Mask & AFS_SETMODE) 
	cm_FreeAllACLEnts(scp);
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
    cm_scache_t *scp = NULL;
    int didEnd;
    AFSStoreStatus inStatus;
    AFSFetchStatus updatedDirStatus;
    AFSFetchStatus newFileStatus;
    AFSCallBack newFileCallback;
    AFSVolSync volSync;
    struct rx_connection * callp;
    cm_dirOp_t dirop;

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
    cm_BeginDirOp(dscp, userp, reqp, CM_DIRLOCK_NONE, &dirop);
    lock_ObtainMutex(&dscp->mx);
    code = cm_SyncOp(dscp, NULL, userp, reqp, 0, CM_SCACHESYNC_STOREDATA);
    lock_ReleaseMutex(&dscp->mx);
    if (code == 0) {
        cm_StartCallbackGrantingCall(NULL, &cbReq);
    } else {
        cm_EndDirOp(&dirop);
    }
    if (code) {
        return code;
    }
    didEnd = 0;

    cm_StatusFromAttr(&inStatus, NULL, attrp);

    /* try the RPC now */
    osi_Log1(afsd_logp, "CALL CreateFile scp 0x%p", dscp);
    do {
        code = cm_ConnFromFID(&dscp->fid, userp, reqp, &connp);
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

    if (dirop.scp) {
        lock_ObtainWrite(&dirop.scp->dirlock);
        dirop.lockType = CM_DIRLOCK_WRITE;
    }
    lock_ObtainMutex(&dscp->mx);
    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_STOREDATA);
    if (code == 0) {
        cm_MergeStatus(NULL, dscp, &updatedDirStatus, &volSync, userp, CM_MERGEFLAG_DIROP);
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
	    scp->creator = userp;		/* remember who created it */
            if (!cm_HaveCallback(scp)) {
                cm_MergeStatus(dscp, scp, &newFileStatus, &volSync,
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

    if (scp && cm_CheckDirOpForSingleChange(&dirop)) {
        cm_DirCreateEntry(&dirop, namep, &newFid);
#ifdef USE_BPLUS
        cm_BPlusDirCreateEntry(&dirop, namep, &newFid);
#endif
    }
    cm_EndDirOp(&dirop);

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

        if (scp->mask & (CM_SCACHEMASK_TRUNCPOS
                          | CM_SCACHEMASK_CLIENTMODTIME
                          | CM_SCACHEMASK_LENGTH))
            code = cm_StoreMini(scp, userp, reqp);

        if (scp->flags & (CM_SCACHEFLAG_OVERQUOTA | CM_SCACHEFLAG_OUTOFSPACE)) {
	    code = (scp->flags & CM_SCACHEFLAG_OVERQUOTA) ? CM_ERROR_QUOTA : CM_ERROR_SPACE;
	    scp->flags &= ~(CM_SCACHEFLAG_OVERQUOTA | CM_SCACHEFLAG_OUTOFSPACE);
	}

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
    cm_scache_t *scp = NULL;
    int didEnd;
    AFSStoreStatus inStatus;
    AFSFetchStatus updatedDirStatus;
    AFSFetchStatus newDirStatus;
    AFSCallBack newDirCallback;
    AFSVolSync volSync;
    struct rx_connection * callp;
    cm_dirOp_t dirop;

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
    cm_BeginDirOp(dscp, userp, reqp, CM_DIRLOCK_NONE, &dirop);
    lock_ObtainMutex(&dscp->mx);
    code = cm_SyncOp(dscp, NULL, userp, reqp, 0, CM_SCACHESYNC_STOREDATA);
    lock_ReleaseMutex(&dscp->mx);
    if (code == 0) {
        cm_StartCallbackGrantingCall(NULL, &cbReq);
    } else {
        cm_EndDirOp(&dirop);
    }
    if (code) {
        return code;
    }
    didEnd = 0;

    cm_StatusFromAttr(&inStatus, NULL, attrp);

    /* try the RPC now */
    osi_Log1(afsd_logp, "CALL MakeDir scp 0x%p", dscp);
    do {
        code = cm_ConnFromFID(&dscp->fid, userp, reqp, &connp);
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

    if (dirop.scp) {
        lock_ObtainWrite(&dirop.scp->dirlock);
        dirop.lockType = CM_DIRLOCK_WRITE;
    }
    lock_ObtainMutex(&dscp->mx);
    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_STOREDATA);
    if (code == 0) {
        cm_MergeStatus(NULL, dscp, &updatedDirStatus, &volSync, userp, CM_MERGEFLAG_DIROP);
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
                cm_MergeStatus(dscp, scp, &newDirStatus, &volSync,
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

    if (scp && cm_CheckDirOpForSingleChange(&dirop)) {
        cm_DirCreateEntry(&dirop, namep, &newFid);
#ifdef USE_BPLUS
        cm_BPlusDirCreateEntry(&dirop, namep, &newFid);
#endif
    }
    cm_EndDirOp(&dirop);

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
    cm_dirOp_t dirop;

    if (dscp->fid.cell != sscp->fid.cell ||
        dscp->fid.volume != sscp->fid.volume) {
        return CM_ERROR_CROSSDEVLINK;
    }

    cm_BeginDirOp(dscp, userp, reqp, CM_DIRLOCK_NONE, &dirop);
    lock_ObtainMutex(&dscp->mx);
    code = cm_SyncOp(dscp, NULL, userp, reqp, 0, CM_SCACHESYNC_STOREDATA);
    lock_ReleaseMutex(&dscp->mx);
    if (code != 0)
        cm_EndDirOp(&dirop);

    if (code)
        return code;

    /* try the RPC now */
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

    if (dirop.scp) {
        lock_ObtainWrite(&dirop.scp->dirlock);
        dirop.lockType = CM_DIRLOCK_WRITE;
    }
    lock_ObtainMutex(&dscp->mx);
    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_STOREDATA);
    if (code == 0) {
        cm_MergeStatus(NULL, dscp, &updatedDirStatus, &volSync, userp, CM_MERGEFLAG_DIROP);
    }
    lock_ReleaseMutex(&dscp->mx);

    if (code == 0) {
        if (cm_CheckDirOpForSingleChange(&dirop)) {
            cm_DirCreateEntry(&dirop, namep, &sscp->fid);
#ifdef USE_BPLUS
            cm_BPlusDirCreateEntry(&dirop, namep, &sscp->fid);
#endif
        }
    }
    cm_EndDirOp(&dirop);

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
    cm_dirOp_t dirop;

    /* before starting the RPC, mark that we're changing the directory data,
     * so that someone who does a chmod on the dir will wait until our
     * call completes.
     */
    cm_BeginDirOp(dscp, userp, reqp, CM_DIRLOCK_NONE, &dirop);
    lock_ObtainMutex(&dscp->mx);
    code = cm_SyncOp(dscp, NULL, userp, reqp, 0, CM_SCACHESYNC_STOREDATA);
    lock_ReleaseMutex(&dscp->mx);
    if (code != 0)
        cm_EndDirOp(&dirop);
    if (code) {
        return code;
    }

    cm_StatusFromAttr(&inStatus, NULL, attrp);

    /* try the RPC now */
    osi_Log1(afsd_logp, "CALL Symlink scp 0x%p", dscp);
    do {
        code = cm_ConnFromFID(&dscp->fid, userp, reqp, &connp);
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

    if (dirop.scp) {
        lock_ObtainWrite(&dirop.scp->dirlock);
        dirop.lockType = CM_DIRLOCK_WRITE;
    }
    lock_ObtainMutex(&dscp->mx);
    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_STOREDATA);
    if (code == 0) {
        cm_MergeStatus(NULL, dscp, &updatedDirStatus, &volSync, userp, CM_MERGEFLAG_DIROP);
    }
    lock_ReleaseMutex(&dscp->mx);

    if (code == 0) {
        if (cm_CheckDirOpForSingleChange(&dirop)) {
            newFid.cell = dscp->fid.cell;
            newFid.volume = dscp->fid.volume;
            newFid.vnode = newAFSFid.Vnode;
            newFid.unique = newAFSFid.Unique;

            cm_DirCreateEntry(&dirop, namep, &newFid);
#ifdef USE_BPLUS
            cm_BPlusDirCreateEntry(&dirop, namep, &newFid);
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
        newFid.cell = dscp->fid.cell;
        newFid.volume = dscp->fid.volume;
        newFid.vnode = newAFSFid.Vnode;
        newFid.unique = newAFSFid.Unique;
        code = cm_GetSCache(&newFid, &scp, userp, reqp);
        if (code == 0) {
            lock_ObtainMutex(&scp->mx);
            if (!cm_HaveCallback(scp)) {
                cm_MergeStatus(dscp, scp, &newLinkStatus, &volSync,
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
    cm_dirOp_t dirop;

    /* before starting the RPC, mark that we're changing the directory data,
     * so that someone who does a chmod on the dir will wait until our
     * call completes.
     */
    cm_BeginDirOp(dscp, userp, reqp, CM_DIRLOCK_NONE, &dirop);
    lock_ObtainMutex(&dscp->mx);
    code = cm_SyncOp(dscp, NULL, userp, reqp, 0, CM_SCACHESYNC_STOREDATA);
    lock_ReleaseMutex(&dscp->mx);
    if (code) {
        cm_EndDirOp(&dirop);
        return code;
    }
    didEnd = 0;

    /* try the RPC now */
    osi_Log1(afsd_logp, "CALL RemoveDir scp 0x%p", dscp);
    do {
        code = cm_ConnFromFID(&dscp->fid, userp, reqp, &connp);
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

    if (dirop.scp) {
        lock_ObtainWrite(&dirop.scp->dirlock);
        dirop.lockType = CM_DIRLOCK_WRITE;
    }
    lock_ObtainMutex(&dscp->mx);
    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_STOREDATA);
    if (code == 0) {
        cm_dnlcRemove(dscp, namep); 
        cm_MergeStatus(NULL, dscp, &updatedDirStatus, &volSync, userp, CM_MERGEFLAG_DIROP);
    }
    lock_ReleaseMutex(&dscp->mx);

    if (code == 0) {
        if (cm_CheckDirOpForSingleChange(&dirop)) {
            cm_DirDeleteEntry(&dirop, namep);
#ifdef USE_BPLUS
            cm_BPlusDirDeleteEntry(&dirop, namep);
#endif
        }
    }
    cm_EndDirOp(&dirop);

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
    cm_dirOp_t oldDirOp;
    cm_fid_t   fileFid;
    int        diropCode = -1;
    cm_dirOp_t newDirOp;

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
        cm_BeginDirOp(oldDscp, userp, reqp, CM_DIRLOCK_NONE, &oldDirOp);
        lock_ObtainMutex(&oldDscp->mx);
        cm_dnlcRemove(oldDscp, oldNamep);
        cm_dnlcRemove(oldDscp, newNamep);
        code = cm_SyncOp(oldDscp, NULL, userp, reqp, 0,
                          CM_SCACHESYNC_STOREDATA);
        lock_ReleaseMutex(&oldDscp->mx);
        if (code != 0) {
            cm_EndDirOp(&oldDirOp);
        }
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
            cm_BeginDirOp(oldDscp, userp, reqp, CM_DIRLOCK_NONE, &oldDirOp);
            lock_ObtainMutex(&oldDscp->mx);
            cm_dnlcRemove(oldDscp, oldNamep);
            code = cm_SyncOp(oldDscp, NULL, userp, reqp, 0,
                              CM_SCACHESYNC_STOREDATA);
            lock_ReleaseMutex(&oldDscp->mx);
            if (code != 0)
                cm_EndDirOp(&oldDirOp);
            if (code == 0) {
                cm_BeginDirOp(newDscp, userp, reqp, CM_DIRLOCK_NONE, &newDirOp);
                lock_ObtainMutex(&newDscp->mx);
                cm_dnlcRemove(newDscp, newNamep);
                code = cm_SyncOp(newDscp, NULL, userp, reqp, 0,
                                  CM_SCACHESYNC_STOREDATA);
                lock_ReleaseMutex(&newDscp->mx);
                if (code) {
                    cm_EndDirOp(&newDirOp);

                    /* cleanup first one */
                    lock_ObtainMutex(&oldDscp->mx);
                    cm_SyncOpDone(oldDscp, NULL,
                                   CM_SCACHESYNC_STOREDATA);
                    lock_ReleaseMutex(&oldDscp->mx);
                    cm_EndDirOp(&oldDirOp);
                }       
            }
        }
        else {
            /* lock the new vnode entry first */
            cm_BeginDirOp(newDscp, userp, reqp, CM_DIRLOCK_NONE, &newDirOp);
            lock_ObtainMutex(&newDscp->mx);
            cm_dnlcRemove(newDscp, newNamep);
            code = cm_SyncOp(newDscp, NULL, userp, reqp, 0,
                              CM_SCACHESYNC_STOREDATA);
            lock_ReleaseMutex(&newDscp->mx);
            if (code != 0)
                cm_EndDirOp(&newDirOp);
            if (code == 0) {
                cm_BeginDirOp(oldDscp, userp, reqp, CM_DIRLOCK_NONE, &oldDirOp);
                lock_ObtainMutex(&oldDscp->mx);
                cm_dnlcRemove(oldDscp, oldNamep);
                code = cm_SyncOp(oldDscp, NULL, userp, reqp, 0,
                                  CM_SCACHESYNC_STOREDATA);
                lock_ReleaseMutex(&oldDscp->mx);
                if (code != 0)
                    cm_EndDirOp(&oldDirOp);
                if (code) {
                    /* cleanup first one */
                    lock_ObtainMutex(&newDscp->mx);
                    cm_SyncOpDone(newDscp, NULL,
                                   CM_SCACHESYNC_STOREDATA);
                    lock_ReleaseMutex(&newDscp->mx);
                    cm_EndDirOp(&newDirOp);
                }       
            }
        }
    }	/* two distinct vnodes */

    if (code) {
        return code;
    }
    didEnd = 0;

    /* try the RPC now */
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
    if (oldDirOp.scp) {
        lock_ObtainWrite(&oldDirOp.scp->dirlock);
        oldDirOp.lockType = CM_DIRLOCK_WRITE;
    }
    lock_ObtainMutex(&oldDscp->mx);
    cm_SyncOpDone(oldDscp, NULL, CM_SCACHESYNC_STOREDATA);

    if (code == 0)
        cm_MergeStatus(NULL, oldDscp, &updatedOldDirStatus, &volSync,
                        userp, CM_MERGEFLAG_DIROP);
    lock_ReleaseMutex(&oldDscp->mx);

    if (code == 0) {
        if (cm_CheckDirOpForSingleChange(&oldDirOp)) {

#ifdef USE_BPLUS
            diropCode = cm_BPlusDirLookup(&oldDirOp, oldNamep, &fileFid);
            if (diropCode == CM_ERROR_INEXACT_MATCH)
                diropCode = 0;
            else if (diropCode == EINVAL)
#endif
                diropCode = cm_DirLookup(&oldDirOp, oldNamep, &fileFid);

            if (diropCode == 0) {
                if (oneDir) {
                    diropCode = cm_DirCreateEntry(&oldDirOp, newNamep, &fileFid);
#ifdef USE_BPLUS
                    cm_BPlusDirCreateEntry(&oldDirOp, newNamep, &fileFid);
#endif
                }

                if (diropCode == 0) { 
                    diropCode = cm_DirDeleteEntry(&oldDirOp, oldNamep);
#ifdef USE_BPLUS
                    cm_BPlusDirDeleteEntry(&oldDirOp, oldNamep);
#endif
                }
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
        lock_ObtainMutex(&newDscp->mx);
        cm_SyncOpDone(newDscp, NULL, CM_SCACHESYNC_STOREDATA);
        if (code == 0)
            cm_MergeStatus(NULL, newDscp, &updatedNewDirStatus, &volSync,
                            userp, CM_MERGEFLAG_DIROP);
        lock_ReleaseMutex(&newDscp->mx);

        if (code == 0) {
            /* we only make the local change if we successfully made
               the change in the old directory AND there was only one
               change in the new directory */
            if (diropCode == 0 && cm_CheckDirOpForSingleChange(&newDirOp)) {
                cm_DirCreateEntry(&newDirOp, newNamep, &fileFid);
#ifdef USE_BPLUS
                cm_BPlusDirCreateEntry(&newDirOp, newNamep, &fileFid);
#endif
            }
        }
        cm_EndDirOp(&newDirOp);
    }

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

/* Called with scp->mx held.  Returns 0 if all is clear to read the
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
                if (fileLock->key == key) {

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
                      (fileLock->key == key || fileLock->lockType == LockWrite)) {
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

/* Called with scp->mx held.  Returns 0 if all is clear to write the
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
                if (fileLock->key == key) {
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

/* Forward dcl. */
static void cm_LockMarkSCacheLost(cm_scache_t * scp);

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

/* called with scp->mx held.  May release it during processing, but
   leaves it held on exit. */
long cm_IntSetLock(cm_scache_t * scp, cm_user_t * userp, int lockType,
                   cm_req_t * reqp) {
    long code = 0;
    AFSFid tfid;
    cm_fid_t cfid;
    cm_conn_t * connp;
    struct rx_connection * callp;
    AFSVolSync volSync;

    tfid.Volume = scp->fid.volume;
    tfid.Vnode = scp->fid.vnode;
    tfid.Unique = scp->fid.unique;
    cfid = scp->fid;

    osi_Log2(afsd_logp, "CALL SetLock scp 0x%p for lock %d", scp, lockType);

    lock_ReleaseMutex(&scp->mx);

    do {
        code = cm_ConnFromFID(&cfid, userp, reqp, &connp);
        if (code) 
            break;

        callp = cm_GetRxConn(connp);
        code = RXAFS_SetLock(callp, &tfid, lockType,
                             &volSync);
        rx_PutConnection(callp);

    } while (cm_Analyze(connp, userp, reqp, &cfid, &volSync,
                        NULL, NULL, code));

    code = cm_MapRPCError(code, reqp);
    if (code) {
        osi_Log1(afsd_logp, "CALL SetLock FAILURE, code 0x%x", code);
    } else {
        osi_Log0(afsd_logp, "CALL SetLock SUCCESS");
    }

    lock_ObtainMutex(&scp->mx);

    return code;
}

/* called with scp->mx held.  Releases it during processing */
long cm_IntReleaseLock(cm_scache_t * scp, cm_user_t * userp,
                       cm_req_t * reqp) {
    long code = 0;
    AFSFid tfid;
    cm_fid_t cfid;
    cm_conn_t * connp;
    struct rx_connection * callp;
    AFSVolSync volSync;

    tfid.Volume = scp->fid.volume;
    tfid.Vnode = scp->fid.vnode;
    tfid.Unique = scp->fid.unique;
    cfid = scp->fid;

    lock_ReleaseMutex(&scp->mx);

    osi_Log1(afsd_logp, "CALL ReleaseLock scp 0x%p", scp);

    do {
        code = cm_ConnFromFID(&cfid, userp, reqp, &connp);
        if (code) 
            break;

        callp = cm_GetRxConn(connp);
        code = RXAFS_ReleaseLock(callp, &tfid, &volSync);
        rx_PutConnection(callp);

    } while (cm_Analyze(connp, userp, reqp, &cfid, &volSync,
                        NULL, NULL, code));
    code = cm_MapRPCError(code, reqp);
    if (code)
        osi_Log1(afsd_logp,
                 "CALL ReleaseLock FAILURE, code 0x%x", code);
    else
        osi_Log0(afsd_logp,
                 "CALL ReleaseLock SUCCESS");
        
    lock_ObtainMutex(&scp->mx);

    return code;
}

/* called with scp->mx held.  May release it during processing, but
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

/* called with scp->mx held */
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
    osi_Log3(afsd_logp, "... allowWait %d key 0x%x:%x", allowWait, 
             (unsigned long)(key >> 32), (unsigned long)(key & 0xffffffff));

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
            if (fileLock->key == key) {
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

    return code;
}

static int cm_KeyEquals(cm_key_t k1, cm_key_t k2, int flags);

/* Called with scp->mx held */
long cm_UnlockByKey(cm_scache_t * scp,
		    cm_key_t key,
		    int flags,
		    cm_user_t * userp,
		     cm_req_t * reqp)
{
    long code = 0;
    cm_file_lock_t *fileLock;
    osi_queue_t *q, *qn;
    int n_unlocks = 0;

    osi_Log4(afsd_logp, "cm_UnlockByKey scp 0x%p key 0x%x:%x flags=0x%x",
             scp,
             (unsigned long)(key >> 32),
             (unsigned long)(key & 0xffffffff),
             flags);

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
        osi_Log3(afsd_logp, "     key[0x%x:%x] flags[0x%x]",
                 (unsigned long)(fileLock->key >> 32),
                 (unsigned long)(fileLock->key & 0xffffffff),
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
            cm_KeyEquals(fileLock->key, key, flags)) {
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

        /* since scp->serverLock looked sane, we are going to assume
           that we have a valid server lock. */
        scp->lockDataVersion = scp->dataVersion;
        osi_Log1(afsd_logp, "  dataVersion on scp = %I64d", scp->dataVersion);

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

        /* failure here has no bearing on the return value of
           cm_Unlock() */
        code = 0;

    } else if (scp->serverLock != (-1) &&
              scp->exclusiveLocks == 0 &&
              scp->sharedLocks == 0) {
        /* The serverLock should be released entirely */

        code = cm_IntReleaseLock(scp, userp, reqp);

        if (code == 0)
            scp->serverLock = (-1);
    }

 done:

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
               cm_user_t *userp, 
               cm_req_t *reqp)
{
    long code = 0;
    int Which = ((sLockType & LOCKING_ANDX_SHARED_LOCK) ? LockRead : LockWrite);
    cm_file_lock_t *fileLock;
    osi_queue_t *q;
    int release_userp = FALSE;

    osi_Log4(afsd_logp, "cm_Unlock scp 0x%p type 0x%x offset %d length %d",
             scp, sLockType, (unsigned long)LOffset.QuadPart, (unsigned long)LLength.QuadPart);
    osi_Log2(afsd_logp, "... key 0x%x:%x",
             (unsigned long) (key >> 32), (unsigned long) (key & 0xffffffff));

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
        if (!IS_LOCK_DELETED(fileLock) &&
            fileLock->key == key &&
            fileLock->range.offset == LOffset.QuadPart &&
            fileLock->range.length == LLength.QuadPart) {
            break;
        }
    }

    if (!q) {
        osi_Log0(afsd_logp, "cm_Unlock lock not found; failure");
        
        lock_ReleaseRead(&cm_scacheLock);

        /* The lock didn't exist anyway. *shrug* */
        return 0;
    }

    lock_ReleaseRead(&cm_scacheLock);

    /* discard lock record */
    lock_ObtainWrite(&cm_scacheLock);
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

    if (!SERVERLOCKS_ENABLED(scp)) {
        osi_Log0(afsd_logp, "   Skipping server locks for scp");
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

        /* Since we already had a lock, we assume that there is a
           valid server lock. */
        scp->lockDataVersion = scp->dataVersion;
        osi_Log1(afsd_logp, "   dataVersion on scp is %I64d", scp->dataVersion);

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
            /* Lost a race.  We obtained a new lock, but that is
               meaningless since someone modified the file
               inbetween. */

            osi_Log0(afsd_logp,
                     "Data version mismatch while downgrading lock");
            osi_Log2(afsd_logp,
                     "  Data versions before=%I64d, after=%I64d",
                     scp->lockDataVersion,
                     scp->dataVersion);
            
            code = cm_IntReleaseLock(scp, userp, reqp);

            scp->serverLock = -1;
            code = CM_ERROR_INVAL;
        }

        if (code != 0 &&
            (scp->sharedLocks > 0 || scp->exclusiveLocks > 0) &&
                (scp->serverLock == -1)) {
                /* Oopsie */
                cm_LockMarkSCacheLost(scp);
            }

        /* failure here has no bearing on the return value of
           cm_Unlock() */
        code = 0;

    } else if (scp->serverLock != (-1) &&
              scp->exclusiveLocks == 0 &&
              scp->sharedLocks == 0) {
        /* The serverLock should be released entirely */

        code = cm_IntReleaseLock(scp, userp, reqp);

        if (code == 0) {
            scp->serverLock = (-1);
        }
    }

    if (release_userp)
        cm_ReleaseUser(userp);

 done:

    osi_Log1(afsd_logp, "cm_Unlock code 0x%x", code);
    osi_Log4(afsd_logp, "  leaving scp with excl[%d], shared[%d], client[%d], serverLock[%d]",
             scp->exclusiveLocks, scp->sharedLocks, scp->clientLocks,
             (int)(signed char) scp->serverLock);

    return code;
}

/* called with scp->mx held */
static void cm_LockMarkSCacheLost(cm_scache_t * scp)
{
    cm_file_lock_t *fileLock;
    osi_queue_t *q;

    osi_Log1(afsd_logp, "cm_LockMarkSCacheLost scp 0x%x", scp);

#ifdef DEBUG
    /* With the current code, we can't lose a lock on a RO scp */
    osi_assertx(!(scp->flags & CM_SCACHEFLAG_RO), "CM_SCACHEFLAG_RO unexpected");
#endif

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
    scp->lockDataVersion = -1;
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
    struct rx_connection * callp;
    cm_scache_t * scp;

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
                lock_ObtainMutex(&scp->mx);

                /* did the lock change while we weren't holding the lock? */
                if (!IS_LOCK_ACTIVE(fileLock))
                    goto post_syncopdone;

                code = cm_SyncOp(scp, NULL, fileLock->userp, &req, 0,
                                 CM_SCACHESYNC_NEEDCALLBACK
                                 | CM_SCACHESYNC_GETSTATUS
                                 | CM_SCACHESYNC_LOCK);

                if (code) {
                    osi_Log1(smb_logp,
                             "cm_CheckLocks SyncOp failure code 0x%x", code);
                    goto post_syncopdone;
                }

                /* cm_SyncOp releases scp->mx during which the lock
                   may get released. */
                if (!IS_LOCK_ACTIVE(fileLock))
                    goto pre_syncopdone;

                if (scp->serverLock != -1) {
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

                    lock_ReleaseMutex(&scp->mx);

                    do {
                        code = cm_ConnFromFID(&cfid, userp,
                                       &req, &connp);
                        if (code) 
                            break;

                        callp = cm_GetRxConn(connp);
                        code = RXAFS_ExtendLock(callp, &tfid,
                                                &volSync);
                        rx_PutConnection(callp);

                        osi_Log1(afsd_logp, "   ExtendLock returns %d", code);

                    } while (cm_Analyze(connp, userp, &req,
                                        &cfid, &volSync, NULL, NULL,
                                        code));

                    code = cm_MapRPCError(code, &req);

                    lock_ObtainMutex(&scp->mx);

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

                    if (code == EINVAL || code == CM_ERROR_INVAL) {
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
                lock_ReleaseMutex(&scp->mx);

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

/* NOT called with scp->mx held. */
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
    osi_Log3(afsd_logp, "    key(%x:%x) flags=%x",
             (unsigned)(oldFileLock->key >> 32),
             (unsigned)(oldFileLock->key & 0xffffffff),
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
    lock_ObtainMutex(&scp->mx);

    code = cm_LockCheckPerms(scp, oldFileLock->lockType,
                             oldFileLock->userp,
                             &req, &has_insert);

    if (code == CM_ERROR_NOACCESS && oldFileLock->lockType == LockRead) {
        if (!has_insert || !SCP_SUPPORTS_WRITELOCKACL(scp)) {
        force_client_lock = TRUE;
        }
        code = 0;
    } else if (code) {
        lock_ReleaseMutex(&scp->mx);
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
        lock_ReleaseMutex(&scp->mx);

        return 0;
    }

    if (IS_LOCK_WAITUNLOCK(oldFileLock)) {

        /* check if the conflicting locks have dissappeared already */
        for (q = scp->fileLocksH; q; q = osi_QNext(q)) {

            fileLock = (cm_file_lock_t *)
                ((char *) q - offsetof(cm_file_lock_t, fileq));

            if (IS_LOCK_LOST(fileLock)) {
                if (fileLock->key == oldFileLock->key) {
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
        lock_ReleaseMutex(&scp->mx);

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
        lock_ReleaseMutex(&scp->mx);

        return 0;

    } else {
        cm_user_t * userp;

        code = cm_SyncOp(scp, NULL, oldFileLock->userp, &req, 0,
                         CM_SCACHESYNC_NEEDCALLBACK
			 | CM_SCACHESYNC_GETSTATUS
			 | CM_SCACHESYNC_LOCK);
        if (code) {
            osi_Log1(smb_logp, "cm_RetryLock SyncOp failure code 0x%x", code);
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
    lock_ReleaseMutex(&scp->mx);

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

cm_key_t cm_GenerateKey(unsigned int session_id, unsigned long process_id, unsigned int file_id)
{
#ifdef DEBUG
    osi_assertx((process_id & 0xffffffff) == process_id, "unexpected process_id");
    osi_assertx((session_id & 0xffff) == session_id, "unexpected session_id");
    osi_assertx((file_id & 0xffff) == file_id, "unexpected file_id");
#endif

    return 
        (((cm_key_t) (process_id & 0xffffffff)) << 32) |
        (((cm_key_t) (session_id & 0xffff)) << 16) |
        (((cm_key_t) (file_id & 0xffff)));
}

static int cm_KeyEquals(cm_key_t k1, cm_key_t k2, int flags)
{
    if (flags & CM_UNLOCK_BY_FID) {
        return ((k1 & 0xffffffff) == (k2 & 0xffffffff));
    } else {
        return (k1 == k2);
    }
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
		lock_ObtainMutex(&scp->mx);
		lock_ObtainWrite(&cm_scacheLock);
		if (!scp->fileLocksH) {
		    lock_ReleaseWrite(&cm_scacheLock);
		    lock_ReleaseMutex(&scp->mx);
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
		lock_ReleaseMutex(&scp->mx);
	    }
	}
    }
}
