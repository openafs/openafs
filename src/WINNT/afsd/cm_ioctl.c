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
#include <afs/cellconfig.h>
#include <afs/ptserver.h>
#include <ubik.h>

#include <windows.h>
#include <errno.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <osi.h>

#include "afsd.h"
#include "afsd_init.h"
#include <WINNT\afsreg.h>

#include "smb.h"
#include "cm_server.h"

#include <rx/rxkad.h>
#include "afsrpc.h"

#include "cm_rpc.h"
#include <strsafe.h>
#include <winioctl.h>
#include <..\afsrdr\kif.h>
#include <rx\rx.h>

#ifdef _DEBUG
#include <crtdbg.h>
#endif

/* Copied from afs_tokens.h */
#define PIOCTL_LOGON	0x1
#define MAX_PATH 260

osi_mutex_t cm_Afsdsbmt_Lock;

extern afs_int32 cryptall;
extern char cm_NetbiosName[];

extern void afsi_log(char *pattern, ...);

void cm_InitIoctl(void)
{
    lock_InitializeMutex(&cm_Afsdsbmt_Lock, "AFSDSBMT.INI Access Lock");
}

long cm_CleanFile(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp)
{
    long code;

    lock_ObtainWrite(&scp->bufCreateLock);
    code = buf_CleanVnode(scp, userp, reqp);
        
    lock_ObtainMutex(&scp->mx);
    cm_DiscardSCache(scp);
    lock_ReleaseMutex(&scp->mx);

    lock_ReleaseWrite(&scp->bufCreateLock);
    osi_Log2(afsd_logp,"cm_CleanFile scp 0x%x returns error: [%x]",scp, code);
    return code;
}

long cm_FlushFile(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp)
{
    long code;

#ifdef AFS_FREELANCE_CLIENT
    if ( scp->fid.cell == AFS_FAKE_ROOT_CELL_ID && scp->fid.volume == AFS_FAKE_ROOT_VOL_ID ) {
	cm_noteLocalMountPointChange();
	return 0;
    }
#endif

    lock_ObtainWrite(&scp->bufCreateLock);
    code = buf_FlushCleanPages(scp, userp, reqp);
        
    lock_ObtainMutex(&scp->mx);
    cm_DiscardSCache(scp);

    lock_ReleaseMutex(&scp->mx);

    lock_ReleaseWrite(&scp->bufCreateLock);
    osi_Log2(afsd_logp,"cm_FlushFile scp 0x%x returns error: [%x]",scp, code);
    return code;
}

long cm_FlushParent(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp)
{
    long code = 0;
    cm_scache_t * pscp;
  
    pscp = cm_FindSCacheParent(scp);
  
    /* now flush the file */
    code = cm_FlushFile(pscp, userp, reqp);
    cm_ReleaseSCache(pscp);

    return code;
}


long cm_FlushVolume(cm_user_t *userp, cm_req_t *reqp, afs_uint32 cell, afs_uint32 volume)
{
    long code = 0;
    cm_scache_t *scp;
    int i;

#ifdef AFS_FREELANCE_CLIENT
    if ( cell == AFS_FAKE_ROOT_CELL_ID && volume == AFS_FAKE_ROOT_VOL_ID ) {
	cm_noteLocalMountPointChange();
	return 0;
    }
#endif

    lock_ObtainWrite(&cm_scacheLock);
    for (i=0; i<cm_data.scacheHashTableSize; i++) {
        for (scp = cm_data.scacheHashTablep[i]; scp; scp = scp->nextp) {
            if (scp->fid.volume == volume && scp->fid.cell == cell) {
                cm_HoldSCacheNoLock(scp);
                lock_ReleaseWrite(&cm_scacheLock);

                /* now flush the file */
                code = cm_FlushFile(scp, userp, reqp);
                lock_ObtainWrite(&cm_scacheLock);
                cm_ReleaseSCacheNoLock(scp);
            }
        }
    }
    lock_ReleaseWrite(&cm_scacheLock);

    return code;
}

/*
 * cm_ResetACLCache -- invalidate ACL info for a user that has just
 *			obtained or lost tokens
 */
void cm_ResetACLCache(cm_user_t *userp)
{
    cm_scache_t *scp;
    int hash;

    lock_ObtainWrite(&cm_scacheLock);
    for (hash=0; hash < cm_data.scacheHashTableSize; hash++) {
        for (scp=cm_data.scacheHashTablep[hash]; scp; scp=scp->nextp) {
            cm_HoldSCacheNoLock(scp);
            lock_ReleaseWrite(&cm_scacheLock);
            lock_ObtainMutex(&scp->mx);
            cm_InvalidateACLUser(scp, userp);
            lock_ReleaseMutex(&scp->mx);
            lock_ObtainWrite(&cm_scacheLock);
            cm_ReleaseSCacheNoLock(scp);
        }
    }
    lock_ReleaseWrite(&cm_scacheLock);
}       

/*
 *  TranslateExtendedChars - This is a fix for TR 54482.
 *
 *  If an extended character (80 - FF) is entered into a file
 *  or directory name in Windows, the character is translated
 *  into the OEM character map before being passed to us.  Why
 *  this occurs is unknown.  Our pioctl functions must match
 *  this translation for paths given via our own commands (like
 *  fs).  If we do not do this, then we will try to perform an
 *  operation on a non-translated path, which we will fail to 
 *  find, since the path was created with the translated chars.
 *  This function performs the required translation.
 */
void TranslateExtendedChars(char *str)
{
    if (!str || !*str)
        return;

    CharToOem(str, str);
}
        
/* parse the passed-in file name and do a namei on it.  If we fail,
 * return an error code, otherwise return the vnode located in *scpp.
 */
long cm_ParseIoctlPath(smb_ioctl_t *ioctlp, cm_user_t *userp, cm_req_t *reqp,
	cm_scache_t **scpp)
{
    long code;
#ifndef AFSIFS
    cm_scache_t *substRootp = NULL;
#endif
    char * relativePath = ioctlp->inDatap;

    osi_Log1(afsd_logp, "cm_ParseIoctlPath %s", osi_LogSaveString(afsd_logp,relativePath));

    /* This is usually the file name, but for StatMountPoint it is the path. */
    /* ioctlp->inDatap can be either of the form:
     *    \path\.
     *    \path\file
     *    \\netbios-name\submount\path\.
     *    \\netbios-name\submount\path\file
     */
    TranslateExtendedChars(relativePath);

#ifdef AFSIFS
    /* we have passed the whole path, including the afs prefix.
       when the pioctl call is made, we perform an ioctl to afsrdr
       and it returns the correct (full) path.  therefore, there is
       no drive letter, and the path is absolute. */
    code = cm_NameI(cm_data.rootSCachep, relativePath,
                     CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                     userp, "", reqp, scpp);

    if (code) {
	osi_Log1(afsd_logp,"cm_ParseIoctlPath code 0x%x", code);
	return code;
    }
#else /* AFSIFS */
    if (relativePath[0] == relativePath[1] &&
         relativePath[1] == '\\' && 
         !_strnicmp(cm_NetbiosName,relativePath+2,strlen(cm_NetbiosName))) 
    {
        char shareName[256];
        char *sharePath;
        int shareFound, i;

        /* We may have found a UNC path. 
         * If the first component is the NetbiosName,
         * then throw out the second component (the submount)
         * since it had better expand into the value of ioctl->tidPathp
         */
        char * p;
        p = relativePath + 2 + strlen(cm_NetbiosName) + 1;			/* buffer overflow vuln.? */
        if ( !_strnicmp("all", p, 3) )
            p += 4;

        for (i = 0; *p && *p != '\\'; i++,p++ ) {
            shareName[i] = *p;
        }
        p++;                    /* skip past trailing slash */
        shareName[i] = 0;       /* terminate string */

        shareFound = smb_FindShare(ioctlp->fidp->vcp, ioctlp->uidp, shareName, &sharePath);
        if ( shareFound ) {
            /* we found a sharename, therefore use the resulting path */
            code = cm_NameI(cm_data.rootSCachep, ioctlp->prefix->data,
                             CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                             userp, sharePath, reqp, &substRootp);
            free(sharePath);
            if (code) {
		osi_Log1(afsd_logp,"cm_ParseIoctlPath [1] code 0x%x", code);
                return code;
	    }

            code = cm_NameI(substRootp, p, CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                             userp, NULL, reqp, scpp);
	    cm_ReleaseSCache(substRootp);
            if (code) {
		osi_Log1(afsd_logp,"cm_ParseIoctlPath [2] code 0x%x", code);
                return code;
	    }
        } else {
            /* otherwise, treat the name as a cellname mounted off the afs root.
             * This requires that we reconstruct the shareName string with 
             * leading and trailing slashes.
             */
            p = relativePath + 2 + strlen(cm_NetbiosName) + 1;
            if ( !_strnicmp("all", p, 3) )
                p += 4;

            shareName[0] = '/';
            for (i = 1; *p && *p != '\\'; i++,p++ ) {
                shareName[i] = *p;
            }
            p++;                    /* skip past trailing slash */
            shareName[i++] = '/';	/* add trailing slash */
            shareName[i] = 0;       /* terminate string */


            code = cm_NameI(cm_data.rootSCachep, ioctlp->prefix->data,
                             CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                             userp, shareName, reqp, &substRootp);
            if (code) {
		osi_Log1(afsd_logp,"cm_ParseIoctlPath [3] code 0x%x", code);
                return code;
	    }

            code = cm_NameI(substRootp, p, CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                            userp, NULL, reqp, scpp);
            if (code) {
		cm_ReleaseSCache(substRootp);
		osi_Log1(afsd_logp,"cm_ParseIoctlPath code [4] 0x%x", code);
                return code;
	    }
        }
    } else {
        code = cm_NameI(cm_data.rootSCachep, ioctlp->prefix->data,
                         CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                         userp, ioctlp->tidPathp, reqp, &substRootp);
        if (code) {
	    osi_Log1(afsd_logp,"cm_ParseIoctlPath [6] code 0x%x", code);
            return code;
	}
        
        code = cm_NameI(substRootp, relativePath, CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                         userp, NULL, reqp, scpp);
        if (code) {
	    cm_ReleaseSCache(substRootp);
	    osi_Log1(afsd_logp,"cm_ParseIoctlPath [7] code 0x%x", code);
            return code;
	}
    }
#endif /* AFSIFS */

    /* # of bytes of path */
    code = (long)strlen(ioctlp->inDatap) + 1;
    ioctlp->inDatap += code;

    /* This is usually nothing, but for StatMountPoint it is the file name. */
    TranslateExtendedChars(ioctlp->inDatap);

    if (substRootp)
	cm_ReleaseSCache(substRootp);

    /* and return success */
    osi_Log1(afsd_logp,"cm_ParseIoctlPath [8] code 0x%x", code);
    return 0;
}

void cm_SkipIoctlPath(smb_ioctl_t *ioctlp)
{
    long temp;
        
    temp = (long) strlen(ioctlp->inDatap) + 1;
    ioctlp->inDatap += temp;
}       


/* format the specified path to look like "/afs/<cellname>/usr", by
 * adding "/afs" (if necessary) in front, changing any \'s to /'s, and
 * removing any trailing "/"'s. One weirdo caveat: "/afs" will be
 * intentionally returned as "/afs/"--this makes submount manipulation
 * easier (because we can always jump past the initial "/afs" to find
 * the AFS path that should be written into afsdsbmt.ini).
 */
void cm_NormalizeAfsPath(char *outpathp, long outlen, char *inpathp)
{
    char *cp;
    char bslash_mountRoot[256];
       
    strncpy(bslash_mountRoot, cm_mountRoot, sizeof(bslash_mountRoot) - 1);
    bslash_mountRoot[0] = '\\';
       
    if (!strnicmp (inpathp, cm_mountRoot, strlen(cm_mountRoot)))
        StringCbCopy(outpathp, outlen, inpathp);
    else if (!strnicmp (inpathp, bslash_mountRoot, strlen(bslash_mountRoot)))
        StringCbCopy(outpathp, outlen, inpathp);
    else if ((inpathp[0] == '/') || (inpathp[0] == '\\'))
        StringCbPrintfA(outpathp, outlen, "%s%s", cm_mountRoot, inpathp);
    else // inpathp looks like "<cell>/usr"
        StringCbPrintfA(outpathp, outlen, "%s/%s", cm_mountRoot, inpathp);

    for (cp = outpathp; *cp != 0; ++cp) {
        if (*cp == '\\')
            *cp = '/';
    }       

    if (strlen(outpathp) && (outpathp[strlen(outpathp)-1] == '/')) {
        outpathp[strlen(outpathp)-1] = 0;
    }

    if (!strcmpi (outpathp, cm_mountRoot)) {
        StringCbCopy(outpathp, outlen, cm_mountRoot);
    }
}

#define LEAF_SIZE 256
/* parse the passed-in file name and do a namei on its parent.  If we fail,
 * return an error code, otherwise return the vnode located in *scpp.
 */
long cm_ParseIoctlParent(smb_ioctl_t *ioctlp, cm_user_t *userp, cm_req_t *reqp,
			 cm_scache_t **scpp, char *leafp)
{
    long code;
    char tbuffer[1024];
    char *tp, *jp;
    cm_scache_t *substRootp = NULL;

    StringCbCopyA(tbuffer, sizeof(tbuffer), ioctlp->inDatap);
    tp = strrchr(tbuffer, '\\');
    jp = strrchr(tbuffer, '/');
    if (!tp)
        tp = jp;
    else if (jp && (tp - tbuffer) < (jp - tbuffer))
        tp = jp;
    if (!tp) {
        StringCbCopyA(tbuffer, sizeof(tbuffer), "\\");
        if (leafp) 
            StringCbCopyA(leafp, LEAF_SIZE, ioctlp->inDatap);
    }
    else {
        *tp = 0;
        if (leafp) 
            StringCbCopyA(leafp, LEAF_SIZE, tp+1);
    }   

    if (tbuffer[0] == tbuffer[1] &&
        tbuffer[1] == '\\' && 
        !_strnicmp(cm_NetbiosName,tbuffer+2,strlen(cm_NetbiosName))) 
    {
        char shareName[256];
        char *sharePath;
        int shareFound, i;

        /* We may have found a UNC path. 
         * If the first component is the NetbiosName,
         * then throw out the second component (the submount)
         * since it had better expand into the value of ioctl->tidPathp
         */
        char * p;
        p = tbuffer + 2 + strlen(cm_NetbiosName) + 1;
        if ( !_strnicmp("all", p, 3) )
            p += 4;

        for (i = 0; *p && *p != '\\'; i++,p++ ) {
            shareName[i] = *p;
        }
        p++;                    /* skip past trailing slash */
        shareName[i] = 0;       /* terminate string */

        shareFound = smb_FindShare(ioctlp->fidp->vcp, ioctlp->uidp, shareName, &sharePath);
        if ( shareFound ) {
            /* we found a sharename, therefore use the resulting path */
            code = cm_NameI(cm_data.rootSCachep, ioctlp->prefix->data,
                             CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                             userp, sharePath, reqp, &substRootp);
            free(sharePath);
            if (code) return code;

            code = cm_NameI(substRootp, p, CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                             userp, NULL, reqp, scpp);
	    cm_ReleaseSCache(substRootp);
            if (code) return code;
        } else {
            /* otherwise, treat the name as a cellname mounted off the afs root.
             * This requires that we reconstruct the shareName string with 
             * leading and trailing slashes.
             */
            p = tbuffer + 2 + strlen(cm_NetbiosName) + 1;
            if ( !_strnicmp("all", p, 3) )
                p += 4;

            shareName[0] = '/';
            for (i = 1; *p && *p != '\\'; i++,p++ ) {
                shareName[i] = *p;
            }
            p++;                    /* skip past trailing slash */
            shareName[i++] = '/';	/* add trailing slash */
            shareName[i] = 0;       /* terminate string */

            code = cm_NameI(cm_data.rootSCachep, ioctlp->prefix->data,
                             CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                             userp, shareName, reqp, &substRootp);
            if (code) return code;

            code = cm_NameI(substRootp, p, CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                            userp, NULL, reqp, scpp);
	    cm_ReleaseSCache(substRootp);
            if (code) return code;
        }
    } else {
        code = cm_NameI(cm_data.rootSCachep, ioctlp->prefix->data,
                        CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                        userp, ioctlp->tidPathp, reqp, &substRootp);
        if (code) return code;

        code = cm_NameI(substRootp, tbuffer, CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                        userp, NULL, reqp, scpp);
	cm_ReleaseSCache(substRootp);
        if (code) return code;
    }

    /* # of bytes of path */
    code = (long)strlen(ioctlp->inDatap) + 1;
    ioctlp->inDatap += code;

    /* and return success */
    return 0;
}

long cm_IoctlGetACL(smb_ioctl_t *ioctlp, cm_user_t *userp)
{
    cm_conn_t *connp;
    cm_scache_t *scp;
    AFSOpaque acl;
    AFSFetchStatus fileStatus;
    AFSVolSync volSync;
    long code;
    AFSFid fid;
    int tlen;
    cm_req_t req;
    struct rx_connection * callp;

    cm_InitReq(&req);

    code = cm_ParseIoctlPath(ioctlp, userp, &req, &scp);
    if (code) return code;

    /* now make the get acl call */
    fid.Volume = scp->fid.volume;
    fid.Vnode = scp->fid.vnode;
    fid.Unique = scp->fid.unique;
    do {
        acl.AFSOpaque_val = ioctlp->outDatap;
        acl.AFSOpaque_len = 0;
        code = cm_ConnFromFID(&scp->fid, userp, &req, &connp);
        if (code) continue;

        callp = cm_GetRxConn(connp);
        code = RXAFS_FetchACL(callp, &fid, &acl, &fileStatus, &volSync);
        rx_PutConnection(callp);

    } while (cm_Analyze(connp, userp, &req, &scp->fid, &volSync, NULL, NULL, code));
    code = cm_MapRPCError(code, &req);
    cm_ReleaseSCache(scp);

    if (code) return code;

    /* skip over return data */
    tlen = (int)strlen(ioctlp->outDatap) + 1;
    ioctlp->outDatap += tlen;

    /* and return success */
    return 0;
}

long cm_IoctlGetFileCellName(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    long code;
    cm_scache_t *scp;
    cm_cell_t *cellp;
    cm_req_t req;

    cm_InitReq(&req);

    code = cm_ParseIoctlPath(ioctlp, userp, &req, &scp);
    if (code) return code;

#ifdef AFS_FREELANCE_CLIENT
    if ( cm_freelanceEnabled && 
         scp->fid.cell==AFS_FAKE_ROOT_CELL_ID &&
         scp->fid.volume==AFS_FAKE_ROOT_VOL_ID &&
         scp->fid.vnode==0x1 && scp->fid.unique==0x1 ) {
        StringCbCopyA(ioctlp->outDatap, SMB_IOCTL_MAXDATA - (ioctlp->outDatap - ioctlp->outAllocp), "Freelance.Local.Root");
        ioctlp->outDatap += strlen(ioctlp->outDatap) + 1;
        code = 0;
    } else 
#endif /* AFS_FREELANCE_CLIENT */
    {
        cellp = cm_FindCellByID(scp->fid.cell);
        if (cellp) {
            StringCbCopyA(ioctlp->outDatap, SMB_IOCTL_MAXDATA - (ioctlp->outDatap - ioctlp->outAllocp), cellp->name);
            ioctlp->outDatap += strlen(ioctlp->outDatap) + 1;
            code = 0;
        }
        else 
            code = CM_ERROR_NOSUCHCELL;
    }

    cm_ReleaseSCache(scp);
    return code;
}

long cm_IoctlSetACL(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    cm_conn_t *connp;
    cm_scache_t *scp;
    AFSOpaque acl;
    AFSFetchStatus fileStatus;
    AFSVolSync volSync;
    long code;
    AFSFid fid;
    cm_req_t req;
    struct rx_connection * callp;

    cm_InitReq(&req);

    code = cm_ParseIoctlPath(ioctlp, userp, &req, &scp);
    if (code) return code;
	
    /* now make the get acl call */
    fid.Volume = scp->fid.volume;
    fid.Vnode = scp->fid.vnode;
    fid.Unique = scp->fid.unique;
    do {
        acl.AFSOpaque_val = ioctlp->inDatap;
        acl.AFSOpaque_len = (u_int)strlen(ioctlp->inDatap)+1;
        code = cm_ConnFromFID(&scp->fid, userp, &req, &connp);
        if (code) continue;

        callp = cm_GetRxConn(connp);
        code = RXAFS_StoreACL(callp, &fid, &acl, &fileStatus, &volSync);
        rx_PutConnection(callp);

    } while (cm_Analyze(connp, userp, &req, &scp->fid, &volSync, NULL, NULL, code));
    code = cm_MapRPCError(code, &req);

    /* invalidate cache info, since we just trashed the ACL cache */
    lock_ObtainMutex(&scp->mx);
    cm_DiscardSCache(scp);
    lock_ReleaseMutex(&scp->mx);

    cm_ReleaseSCache(scp);

    return code;
}



long cm_IoctlFlushAllVolumes(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    long code;
    cm_scache_t *scp;
    int i;
    cm_req_t req;

    cm_InitReq(&req);

    lock_ObtainWrite(&cm_scacheLock);
    for (i=0; i<cm_data.scacheHashTableSize; i++) {
        for (scp = cm_data.scacheHashTablep[i]; scp; scp = scp->nextp) {
	    cm_HoldSCacheNoLock(scp);
	    lock_ReleaseWrite(&cm_scacheLock);

	    /* now flush the file */
	    code = cm_FlushFile(scp, userp, &req);
	    lock_ObtainWrite(&cm_scacheLock);
	    cm_ReleaseSCacheNoLock(scp);
        }
    }
    lock_ReleaseWrite(&cm_scacheLock);

    return code;
}

long cm_IoctlFlushVolume(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    long code;
    cm_scache_t *scp;
    unsigned long volume;
    unsigned long cell;
    cm_req_t req;

    cm_InitReq(&req);

    code = cm_ParseIoctlPath(ioctlp, userp, &req, &scp);
    if (code) return code;
        
    volume = scp->fid.volume;
    cell = scp->fid.cell;
    cm_ReleaseSCache(scp);

    code = cm_FlushVolume(userp, &req, cell, volume);

    return code;
}

long cm_IoctlFlushFile(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    long code;
    cm_scache_t *scp;
    cm_req_t req;

    cm_InitReq(&req);

    code = cm_ParseIoctlPath(ioctlp, userp, &req, &scp);
    if (code) return code;
        
    cm_FlushFile(scp, userp, &req);
    cm_ReleaseSCache(scp);

    return 0;
}

long cm_IoctlSetVolumeStatus(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    cm_scache_t *scp;
    char volName[32];
    char offLineMsg[256];
    char motd[256];
    cm_conn_t *tcp;
    long code;
    AFSFetchVolumeStatus volStat;
    AFSStoreVolumeStatus storeStat;
    cm_volume_t *tvp;
    char *cp;
    cm_cell_t *cellp;
    cm_req_t req;
    struct rx_connection * callp;

    cm_InitReq(&req);

    code = cm_ParseIoctlPath(ioctlp, userp, &req, &scp);
    if (code) return code;

    cellp = cm_FindCellByID(scp->fid.cell);
    osi_assert(cellp);

    if (scp->flags & CM_SCACHEFLAG_RO) {
        cm_ReleaseSCache(scp);
        return CM_ERROR_READONLY;
    }

    code = cm_GetVolumeByID(cellp, scp->fid.volume, userp, &req, 
                            CM_GETVOL_FLAG_CREATE, &tvp);
    if (code) {
        cm_ReleaseSCache(scp);
        return code;
    }
    cm_PutVolume(tvp);

    /* Copy the junk out, using cp as a roving pointer. */
    cp = ioctlp->inDatap;
    memcpy((char *)&volStat, cp, sizeof(AFSFetchVolumeStatus));
    cp += sizeof(AFSFetchVolumeStatus);
    StringCbCopyA(volName, sizeof(volName), cp);
    cp += strlen(volName)+1;
    StringCbCopyA(offLineMsg, sizeof(offLineMsg), cp);
    cp +=  strlen(offLineMsg)+1;
    StringCbCopyA(motd, sizeof(motd), cp);
    storeStat.Mask = 0;
    if (volStat.MinQuota != -1) {
        storeStat.MinQuota = volStat.MinQuota;
        storeStat.Mask |= AFS_SETMINQUOTA;
    }
    if (volStat.MaxQuota != -1) {
        storeStat.MaxQuota = volStat.MaxQuota;
        storeStat.Mask |= AFS_SETMAXQUOTA;
    }

    do {
        code = cm_ConnFromFID(&scp->fid, userp, &req, &tcp);
        if (code) continue;

        callp = cm_GetRxConn(tcp);
        code = RXAFS_SetVolumeStatus(callp, scp->fid.volume,
                                      &storeStat, volName, offLineMsg, motd);
        rx_PutConnection(callp);

    } while (cm_Analyze(tcp, userp, &req, &scp->fid, NULL, NULL, NULL, code));
    code = cm_MapRPCError(code, &req);

    /* return on failure */
    cm_ReleaseSCache(scp);
    if (code) {
        return code;
    }

    /* we are sending parms back to make compat. with prev system.  should
     * change interface later to not ask for current status, just set
     * new status
     */
    cp = ioctlp->outDatap;
    memcpy(cp, (char *)&volStat, sizeof(VolumeStatus));
    cp += sizeof(VolumeStatus);
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), volName);
    cp += strlen(volName)+1;
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), offLineMsg);
    cp += strlen(offLineMsg)+1;
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), motd);
    cp += strlen(motd)+1;

    /* now return updated return data pointer */
    ioctlp->outDatap = cp;

    return 0;
}       

long cm_IoctlGetVolumeStatus(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    char volName[32];
    cm_scache_t *scp;
    char offLineMsg[256];
    char motd[256];
    cm_conn_t *connp;
    register long code;
    AFSFetchVolumeStatus volStat;
    register char *cp;
    char *Name;
    char *OfflineMsg;
    char *MOTD;
    cm_req_t req;
    struct rx_connection * callp;

    cm_InitReq(&req);

    code = cm_ParseIoctlPath(ioctlp, userp, &req, &scp);
    if (code) return code;

    Name = volName;
    OfflineMsg = offLineMsg;
    MOTD = motd;
    do {
        code = cm_ConnFromFID(&scp->fid, userp, &req, &connp);
        if (code) continue;

        callp = cm_GetRxConn(connp);
        code = RXAFS_GetVolumeStatus(callp, scp->fid.volume,
                                      &volStat, &Name, &OfflineMsg, &MOTD);
        rx_PutConnection(callp);

    } while (cm_Analyze(connp, userp, &req, &scp->fid, NULL, NULL, NULL, code));
    code = cm_MapRPCError(code, &req);

    cm_ReleaseSCache(scp);
    if (code) return code;

    /* Copy all this junk into msg->im_data, keeping track of the lengths. */
    cp = ioctlp->outDatap;
    memcpy(cp, (char *)&volStat, sizeof(AFSFetchVolumeStatus));
    cp += sizeof(AFSFetchVolumeStatus);
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), volName);
    cp += strlen(volName)+1;
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), offLineMsg);
    cp += strlen(offLineMsg)+1;
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), motd);
    cp += strlen(motd)+1;

    /* return new size */
    ioctlp->outDatap = cp;

    return 0;
}

long cm_IoctlGetFid(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    cm_scache_t *scp;
    register long code;
    register char *cp;
    cm_fid_t fid;
    cm_req_t req;

    cm_InitReq(&req);

    code = cm_ParseIoctlPath(ioctlp, userp, &req, &scp);
    if (code) return code;

    memset(&fid, 0, sizeof(cm_fid_t));
    fid.volume = scp->fid.volume;
    fid.vnode  = scp->fid.vnode;
    fid.unique = scp->fid.unique;

    cm_ReleaseSCache(scp);

    /* Copy all this junk into msg->im_data, keeping track of the lengths. */
    cp = ioctlp->outDatap;
    memcpy(cp, (char *)&fid, sizeof(cm_fid_t));
    cp += sizeof(cm_fid_t);

    /* return new size */
    ioctlp->outDatap = cp;

    return 0;
}

long cm_IoctlGetOwner(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    cm_scache_t *scp;
    register long code;
    register char *cp;
    cm_req_t req;

    cm_InitReq(&req);

    code = cm_ParseIoctlPath(ioctlp, userp, &req, &scp);
    if (code) return code;

    /* Copy all this junk into msg->im_data, keeping track of the lengths. */
    cp = ioctlp->outDatap;
    memcpy(cp, (char *)&scp->owner, sizeof(afs_uint32));
    cp += sizeof(afs_uint32);
    memcpy(cp, (char *)&scp->group, sizeof(afs_uint32));
    cp += sizeof(afs_uint32);

    /* return new size */
    ioctlp->outDatap = cp;

    cm_ReleaseSCache(scp);

    return 0;
}

long cm_IoctlWhereIs(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    long code;
    cm_scache_t *scp;
    cm_cell_t *cellp;
    cm_volume_t *tvp;
    cm_serverRef_t **tsrpp, *current;
    cm_server_t *tsp;
    unsigned long volume;
    char *cp;
    cm_req_t req;

    cm_InitReq(&req);

    code = cm_ParseIoctlPath(ioctlp, userp, &req, &scp);
    if (code) return code;
        
    volume = scp->fid.volume;

    cellp = cm_FindCellByID(scp->fid.cell);

    cm_ReleaseSCache(scp);

    if (!cellp)
	return CM_ERROR_NOSUCHCELL;

    code = cm_GetVolumeByID(cellp, volume, userp, &req, CM_GETVOL_FLAG_CREATE, &tvp);
    if (code) return code;
	
    cp = ioctlp->outDatap;
        
    lock_ObtainMutex(&tvp->mx);
    tsrpp = cm_GetVolServers(tvp, volume);
    lock_ObtainRead(&cm_serverLock);
    for (current = *tsrpp; current; current = current->next) {
        tsp = current->server;
        memcpy(cp, (char *)&tsp->addr.sin_addr.s_addr, sizeof(long));
        cp += sizeof(long);
    }
    lock_ReleaseRead(&cm_serverLock);
    cm_FreeServerList(tsrpp, 0);
    lock_ReleaseMutex(&tvp->mx);

    /* still room for terminating NULL, add it on */
    volume = 0;	/* reuse vbl */
    memcpy(cp, (char *)&volume, sizeof(long));
    cp += sizeof(long);

    ioctlp->outDatap = cp;
    cm_PutVolume(tvp);
    return 0;
}       

long cm_IoctlStatMountPoint(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    long code;
    cm_scache_t *dscp;
    cm_scache_t *scp;
    char *cp;
    cm_req_t req;

    cm_InitReq(&req);

    code = cm_ParseIoctlPath(ioctlp, userp, &req, &dscp);
    if (code) return code;

    cp = ioctlp->inDatap;

    code = cm_Lookup(dscp, cp, CM_FLAG_NOMOUNTCHASE, userp, &req, &scp);
    cm_ReleaseSCache(dscp);
    if (code) return code;
        
    lock_ObtainMutex(&scp->mx);

    /* now check that this is a real mount point */
    if (scp->fileType != CM_SCACHETYPE_MOUNTPOINT) {
        lock_ReleaseMutex(&scp->mx);
        cm_ReleaseSCache(scp);
        return CM_ERROR_INVAL;
    }

    code = cm_ReadMountPoint(scp, userp, &req);
    if (code == 0) {
        cp = ioctlp->outDatap;
        StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), scp->mountPointStringp);
        cp += strlen(cp) + 1;
        ioctlp->outDatap = cp;
    }
    lock_ReleaseMutex(&scp->mx);
    cm_ReleaseSCache(scp);

    return code;
}       

long cm_IoctlDeleteMountPoint(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    long code;
    cm_scache_t *dscp;
    cm_scache_t *scp;
    char *cp;
    cm_req_t req;

    cm_InitReq(&req);

    code = cm_ParseIoctlPath(ioctlp, userp, &req, &dscp);
    if (code) return code;

    cp = ioctlp->inDatap;

    code = cm_Lookup(dscp, cp, CM_FLAG_NOMOUNTCHASE, userp, &req, &scp);
        
    /* if something went wrong, bail out now */
    if (code) {
        goto done2;
    }
        
    lock_ObtainMutex(&scp->mx);
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {     
        lock_ReleaseMutex(&scp->mx);
        cm_ReleaseSCache(scp);
        goto done2;
    }

    /* now check that this is a real mount point */
    if (scp->fileType != CM_SCACHETYPE_MOUNTPOINT) {
        lock_ReleaseMutex(&scp->mx);
        cm_ReleaseSCache(scp);
        code = CM_ERROR_INVAL;
        goto done1;
    }

    /* time to make the RPC, so drop the lock */
    lock_ReleaseMutex(&scp->mx);
    cm_ReleaseSCache(scp);

    /* easier to do it this way */
    code = cm_Unlink(dscp, cp, userp, &req);
    if (code == 0 && (dscp->flags & CM_SCACHEFLAG_ANYWATCH))
        smb_NotifyChange(FILE_ACTION_REMOVED,
                          FILE_NOTIFY_CHANGE_DIR_NAME,
                          dscp, cp, NULL, TRUE);

  done1:
    lock_ObtainMutex(&scp->mx);
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseMutex(&scp->mx);

  done2:
    cm_ReleaseSCache(dscp);
    return code;
}

long cm_IoctlCheckServers(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    cm_cell_t *cellp;
    chservinfo_t csi;
    char *tp;
    char *cp;
    long temp;
    cm_server_t *tsp;
    int haveCell;
        
    cm_SkipIoctlPath(ioctlp);	/* we don't care about the path */
    tp = ioctlp->inDatap;
    haveCell = 0;

    memcpy(&temp, tp, sizeof(temp));
    if (temp == 0x12345678) {	/* For afs3.3 version */
        memcpy(&csi, tp, sizeof(csi));
        if (csi.tinterval >= 0) {
            cp = ioctlp->outDatap;
            memcpy(cp, (char *)&cm_daemonCheckDownInterval, sizeof(long));
            ioctlp->outDatap += sizeof(long);
            if (csi.tinterval > 0) {
                if (!smb_SUser(userp))
                    return CM_ERROR_NOACCESS;
                cm_daemonCheckDownInterval = csi.tinterval;
            }
            return 0;
        }
        if (csi.tsize)
            haveCell = 1;
        temp = csi.tflags;
        cp = csi.tbuffer;
    } else {	/* For pre afs3.3 versions */
        memcpy((char *)&temp, ioctlp->inDatap, sizeof(long));
        ioctlp->inDatap = cp = ioctlp->inDatap + sizeof(long);
        if (cp - ioctlp->inAllocp < ioctlp->inCopied)	/* still more data available */
            haveCell = 1;
    }       

    /* 
     * 1: fast check, don't contact servers.
     * 2: local cell only.
     */
    if (haveCell) {
        /* have cell name, too */
        cellp = cm_GetCell(cp, 0);
        if (!cellp) return CM_ERROR_NOSUCHCELL;
    }
    else cellp = (cm_cell_t *) 0;
    if (!cellp && (temp & 2)) {
        /* use local cell */
        cellp = cm_FindCellByID(1);
    }
    if (!(temp & 1)) {	/* if not fast, call server checker routine */
        /* check down servers */
        cm_CheckServers(CM_FLAG_CHECKDOWNSERVERS | CM_FLAG_CHECKUPSERVERS,
                         cellp);
    }       

    /* now return the current down server list */
    cp = ioctlp->outDatap;
    lock_ObtainRead(&cm_serverLock);
    for (tsp = cm_allServersp; tsp; tsp=tsp->allNextp) {
        if (cellp && tsp->cellp != cellp) continue;	/* cell spec'd and wrong */
        if ((tsp->flags & CM_SERVERFLAG_DOWN)
             && tsp->type == CM_SERVER_FILE) {
            memcpy(cp, (char *)&tsp->addr.sin_addr.s_addr, sizeof(long));
            cp += sizeof(long);
        }
    }
    lock_ReleaseRead(&cm_serverLock);

    ioctlp->outDatap = cp;
    return 0;
}

long cm_IoctlGag(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    /* we don't print anything superfluous, so we don't support the gag call */
    return CM_ERROR_INVAL;
}

long cm_IoctlCheckVolumes(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    cm_RefreshVolumes();
    return 0;
}       

long cm_IoctlSetCacheSize(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    afs_uint64 temp;
    long code;

    cm_SkipIoctlPath(ioctlp);

    memcpy(&temp, ioctlp->inDatap, sizeof(temp));
    if (temp == 0) 
        temp = cm_data.buf_nOrigBuffers;
    else {
        /* temp is in 1K units, convert to # of buffers */
        temp = temp / (cm_data.buf_blockSize / 1024);
    }       

    /* now adjust the cache size */
    code = buf_SetNBuffers(temp);

    return code;
}

long cm_IoctlTraceControl(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    long inValue;
        
    cm_SkipIoctlPath(ioctlp);
        
    memcpy(&inValue, ioctlp->inDatap, sizeof(long));

    /* print trace */
    if (inValue & 8) {
        afsd_ForceTrace(FALSE);
        buf_ForceTrace(FALSE);
    }
        
    if (inValue & 2) {
        /* set tracing value to low order bit */
        if ((inValue & 1) == 0) {
            /* disable tracing */
            osi_LogDisable(afsd_logp);
	    rx_DebugOnOff(FALSE);
        }
        else {
            /* enable tracing */
            osi_LogEnable(afsd_logp);
	    rx_DebugOnOff(TRUE);
        }
    }

    /* see if we're supposed to do a reset, too */
    if (inValue & 4) {
        osi_LogReset(afsd_logp);
    }

    /* and copy out tracing flag */
    inValue = afsd_logp->enabled;	/* use as a temp vbl */
    memcpy(ioctlp->outDatap, &inValue, sizeof(long));
    ioctlp->outDatap += sizeof(long);
    return 0;
}       

long cm_IoctlGetCacheParms(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    cm_cacheParms_t parms;

    memset(&parms, 0, sizeof(parms));

    /* first we get, in 1K units, the cache size */
    parms.parms[0] = cm_data.buf_nbuffers * (cm_data.buf_blockSize / 1024);

    /* and then the actual # of buffers in use (not in the free list, I guess,
     * will be what we do).
     */
    parms.parms[1] = (cm_data.buf_nbuffers - buf_CountFreeList()) * (cm_data.buf_blockSize / 1024);

    memcpy(ioctlp->outDatap, &parms, sizeof(parms));
    ioctlp->outDatap += sizeof(parms);

    return 0;
}

long cm_IoctlGetCell(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    long whichCell;
    long magic = 0;
    cm_cell_t *tcellp;
    cm_serverRef_t *serverRefp;
    cm_server_t *serverp;
    long i;
    char *cp;
    char *tp;
    char *basep;

    cm_SkipIoctlPath(ioctlp);

    tp = ioctlp->inDatap;

    memcpy((char *)&whichCell, tp, sizeof(long));
    tp += sizeof(long);

    /* see if more than one long passed in, ignoring the null pathname (the -1) */
    if (ioctlp->inCopied-1 > sizeof(long)) {
        memcpy((char *)&magic, tp, sizeof(long));
    }

    lock_ObtainRead(&cm_cellLock);
    for (tcellp = cm_data.allCellsp; tcellp; tcellp = tcellp->nextp) {
        if (whichCell == 0) break;
        whichCell--;
    }
    lock_ReleaseRead(&cm_cellLock);
    if (tcellp) {
        int max = 8;

        cp = ioctlp->outDatap;

        if (magic == 0x12345678) {
            memcpy(cp, (char *)&magic, sizeof(long));
            max = 13;
        }
        memset(cp, 0, max * sizeof(long));
        basep = cp;
        lock_ObtainRead(&cm_serverLock);	/* for going down server list */
        /* jaltman - do the reference counts to serverRefp contents need to be increased? */
        serverRefp = tcellp->vlServersp;
        for (i=0; i<max; i++) {
            if (!serverRefp) break;
            serverp = serverRefp->server;
            memcpy(cp, &serverp->addr.sin_addr.s_addr, sizeof(long));
            cp += sizeof(long);
            serverRefp = serverRefp->next;
        }
        lock_ReleaseRead(&cm_serverLock);
        cp = basep + max * sizeof(afs_int32);
        StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), tcellp->name);
        cp += strlen(tcellp->name)+1;
        ioctlp->outDatap = cp;
    }

    if (tcellp) 
        return 0;
    else 
        return CM_ERROR_NOMORETOKENS;	/* mapped to EDOM */
}

extern long cm_AddCellProc(void *rockp, struct sockaddr_in *addrp, char *namep);

long cm_IoctlNewCell(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    /* NT cache manager will read cell information from CellServDB each time
     * cell is accessed. So, this call is necessary only if list of server for a cell 
     * changes (or IP addresses of cell servers changes).
     * All that needs to be done is to refresh server information for all cells that 
     * are already loaded.
  
     * cell list will be cm_CellLock and cm_ServerLock will be held for write.
     */  
  
    cm_cell_t *cp;
  
    cm_SkipIoctlPath(ioctlp);
    lock_ObtainWrite(&cm_cellLock);
  
    for (cp = cm_data.allCellsp; cp; cp=cp->nextp) 
    {
        long code;
	lock_ObtainMutex(&cp->mx);
        /* delete all previous server lists - cm_FreeServerList will ask for write on cm_ServerLock*/
        cm_FreeServerList(&cp->vlServersp, CM_FREESERVERLIST_DELETE);
        cp->vlServersp = NULL;
        code = cm_SearchCellFile(cp->name, cp->name, cm_AddCellProc, cp);
#ifdef AFS_AFSDB_ENV
        if (code) {
            if (cm_dnsEnabled) {
                int ttl;
                code = cm_SearchCellByDNS(cp->name, cp->name, &ttl, cm_AddCellProc, cp);
                if ( code == 0 ) { /* got cell from DNS */
                    cp->flags |= CM_CELLFLAG_DNS;
                    cp->flags &= ~CM_CELLFLAG_VLSERVER_INVALID;
                    cp->timeout = time(0) + ttl;
                }
            }
        } 
        else {
            cp->flags &= ~CM_CELLFLAG_DNS;
        }
#endif /* AFS_AFSDB_ENV */
        if (code) {
            cp->flags |= CM_CELLFLAG_VLSERVER_INVALID;
        }
        else {
            cp->flags &= ~CM_CELLFLAG_VLSERVER_INVALID;
            cm_RandomizeServer(&cp->vlServersp);
        }
	lock_ReleaseMutex(&cp->mx);
    }
    
    lock_ReleaseWrite(&cm_cellLock);
    return 0;       
}

long cm_IoctlGetWsCell(smb_ioctl_t *ioctlp, cm_user_t *userp)
{
	long code = 0;

	if (cm_freelanceEnabled) {
            if (cm_GetRootCellName(ioctlp->outDatap))
                StringCbCopyA(ioctlp->outDatap, SMB_IOCTL_MAXDATA - (ioctlp->outDatap - ioctlp->outAllocp), "Freelance.Local.Root");
            ioctlp->outDatap += strlen(ioctlp->outDatap) +1;
	} else if (cm_data.rootCellp) {
	    /* return the default cellname to the caller */
	    StringCbCopyA(ioctlp->outDatap, SMB_IOCTL_MAXDATA - (ioctlp->outDatap - ioctlp->outAllocp), cm_data.rootCellp->name);
	    ioctlp->outDatap += strlen(ioctlp->outDatap) +1;
	} else {
	    /* if we don't know our default cell, return failure */
            code = CM_ERROR_NOSUCHCELL;
    }

    return code;
}

long cm_IoctlSysName(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    long setSysName, foundname = 0;
    char *cp, *cp2, inname[MAXSYSNAME], outname[MAXSYSNAME];
    int t, count, num = 0;
    char **sysnamelist[MAXSYSNAME];
        
    cm_SkipIoctlPath(ioctlp);

    memcpy(&setSysName, ioctlp->inDatap, sizeof(long));
    ioctlp->inDatap += sizeof(long);
        
    if (setSysName) {
        /* check my args */
        if ( setSysName < 0 || setSysName > MAXNUMSYSNAMES )
            return EINVAL;
        cp2 = ioctlp->inDatap;
        for ( cp=ioctlp->inDatap, count = 0; count < setSysName; count++ ) {
            /* won't go past end of ioctlp->inDatap since maxsysname*num < ioctlp->inDatap length */
            t = (int)strlen(cp);
            if (t >= MAXSYSNAME || t <= 0)
                return EINVAL;
            /* check for names that can shoot us in the foot */
            if (*cp == '.' && (cp[1] == 0 || (cp[1] == '.' && cp[2] == 0)))
                return EINVAL;
            cp += t + 1;
        }
        /* args ok */

        /* inname gets first entry in case we're being a translator */
        /* (we are never a translator) */
        t = (int)strlen(ioctlp->inDatap);
        memcpy(inname, ioctlp->inDatap, t + 1);
        ioctlp->inDatap += t + 1;
        num = count;
    }

    /* Not xlating, so local case */
    if (!cm_sysName)
        osi_panic("cm_IoctlSysName: !cm_sysName\n", __FILE__, __LINE__);

    if (!setSysName) {      /* user just wants the info */
        StringCbCopyA(outname, sizeof(outname), cm_sysName);
        foundname = cm_sysNameCount;
        *sysnamelist = cm_sysNameList;
    } else {        
        /* Local guy; only root can change sysname */
        /* clear @sys entries from the dnlc, once afs_lookup can
         * do lookups of @sys entries and thinks it can trust them */
        /* privs ok, store the entry, ... */
        StringCbCopyA(cm_sysName, sizeof(cm_sysName), inname);
        StringCbCopyA(cm_sysNameList[0], MAXSYSNAME, inname);
        if (setSysName > 1) {       /* ... or list */
            cp = ioctlp->inDatap;
            for (count = 1; count < setSysName; ++count) {
                if (!cm_sysNameList[count])
                    osi_panic("cm_IoctlSysName: no cm_sysNameList entry to write\n",
                               __FILE__, __LINE__);
                t = (int)strlen(cp);
                StringCbCopyA(cm_sysNameList[count], MAXSYSNAME, cp);
                cp += t + 1;
            }
        }
        cm_sysNameCount = setSysName;
    }

    if (!setSysName) {
        /* return the sysname to the caller */
        cp = ioctlp->outDatap;
        memcpy(cp, (char *)&foundname, sizeof(afs_int32));
        cp += sizeof(afs_int32);	/* skip found flag */
        if (foundname) {
            StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), outname);
            cp += strlen(outname) + 1;	/* skip name and terminating null char */
            for ( count=1; count < foundname ; ++count) {   /* ... or list */
                if ( !(*sysnamelist)[count] )
                    osi_panic("cm_IoctlSysName: no cm_sysNameList entry to read\n", 
                               __FILE__, __LINE__);
                t = (int)strlen((*sysnamelist)[count]);
                if (t >= MAXSYSNAME)
                    osi_panic("cm_IoctlSysName: sysname entry garbled\n", 
                               __FILE__, __LINE__);
                StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), (*sysnamelist)[count]);
                cp += t + 1;
            }
        }
        ioctlp->outDatap = cp;
    }
        
    /* done: success */
    return 0;
}

long cm_IoctlGetCellStatus(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    long temp;
    cm_cell_t *cellp;

    cm_SkipIoctlPath(ioctlp);

    cellp = cm_GetCell(ioctlp->inDatap, 0);
    if (!cellp) 
        return CM_ERROR_NOSUCHCELL;

    temp = 0;
    lock_ObtainMutex(&cellp->mx);
    if (cellp->flags & CM_CELLFLAG_SUID)
        temp |= CM_SETCELLFLAG_SUID;
    lock_ReleaseMutex(&cellp->mx);
        
    /* now copy out parm */
    memcpy(ioctlp->outDatap, &temp, sizeof(long));
    ioctlp->outDatap += sizeof(long);

    return 0;
}

long cm_IoctlSetCellStatus(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    long temp;
    cm_cell_t *cellp;

    cm_SkipIoctlPath(ioctlp);

    cellp = cm_GetCell(ioctlp->inDatap + 2*sizeof(long), 0);
    if (!cellp) 
        return CM_ERROR_NOSUCHCELL;

    memcpy((char *)&temp, ioctlp->inDatap, sizeof(long));

    lock_ObtainMutex(&cellp->mx);
    if (temp & CM_SETCELLFLAG_SUID)
        cellp->flags |= CM_CELLFLAG_SUID;
    else
        cellp->flags &= ~CM_CELLFLAG_SUID;
    lock_ReleaseMutex(&cellp->mx);

    return 0;
}

long cm_IoctlSetSPrefs(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    cm_SSetPref_t 	  *spin; /* input */
    cm_SPref_t        *srvin;   /* one input component */
    cm_server_t       *tsp;
    int 		  i, vlonly, noServers, type;
    struct sockaddr_in	tmp;
    unsigned short	  rank;

    cm_SkipIoctlPath(ioctlp);       /* we don't care about the path */

    spin	   = (cm_SSetPref_t *)ioctlp->inDatap;
    noServers  = spin->num_servers;
    vlonly     = spin->flags;
    if ( vlonly )
        type = CM_SERVER_VLDB;
    else    
        type = CM_SERVER_FILE;

    for ( i=0; i < noServers; i++) 
    {
        srvin          = &(spin->servers[i]);
        rank           = srvin->rank + (rand() & 0x000f);
        tmp.sin_addr   = srvin->host;
        tmp.sin_family = AF_INET;

        tsp = cm_FindServer(&tmp, type);
        if ( tsp )		/* an existing server - ref count increased */
        {
            tsp->ipRank = rank; /* no need to protect by mutex*/

            if (type == CM_SERVER_FILE)
            {   /* fileserver */
                /* find volumes which might have RO copy 
                /* on server and change the ordering of 
                 * their RO list 
                 */
                cm_ChangeRankVolume(tsp);
            }
            else 	
            {
                /* set preferences for an existing vlserver */
                cm_ChangeRankCellVLServer(tsp);
            }
        }
        else	/* add a new server without a cell */
        {
            tsp = cm_NewServer(&tmp, type, NULL); /* refcount = 1 */
            tsp->ipRank = rank;
        }
	lock_ObtainMutex(&tsp->mx);
	tsp->flags |= CM_SERVERFLAG_PREF_SET;
	lock_ReleaseMutex(&tsp->mx);
	cm_PutServer(tsp);  /* decrease refcount */
    }
    return 0;
}

long cm_IoctlGetSPrefs(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    cm_SPrefRequest_t *spin; /* input */
    cm_SPrefInfo_t    *spout;   /* output */
    cm_SPref_t        *srvout;   /* one output component */
    cm_server_t       *tsp;
    int 		  i, vlonly, noServers;

    cm_SkipIoctlPath(ioctlp);       /* we don't care about the path */

    spin      = (cm_SPrefRequest_t *)ioctlp->inDatap;
    spout     = (cm_SPrefInfo_t *) ioctlp->outDatap;
    srvout    = spout->servers;
    noServers = spin->num_servers; 
    vlonly    = spin->flags & CM_SPREF_VLONLY;
    spout->num_servers = 0;

    lock_ObtainRead(&cm_serverLock); /* get server lock */

    for (tsp=cm_allServersp, i=0; tsp && noServers; tsp=tsp->allNextp,i++){
        if (spin->offset > i) {
            continue;    /* catch up to where we left off */
        }

        if ( vlonly && (tsp->type == CM_SERVER_FILE) )
            continue;   /* ignore fileserver for -vlserver option*/
        if ( !vlonly && (tsp->type == CM_SERVER_VLDB) )
            continue;   /* ignore vlservers */

        srvout->host = tsp->addr.sin_addr;
        srvout->rank = tsp->ipRank;
        srvout++;	
        spout->num_servers++;
        noServers--;
    }
    lock_ReleaseRead(&cm_serverLock); /* release server lock */

    if ( tsp ) 	/* we ran out of space in the output buffer */
        spout->next_offset = i;
    else    
        spout->next_offset = 0; 
    ioctlp->outDatap += sizeof(cm_SPrefInfo_t) + 
        (spout->num_servers -1 ) * sizeof(cm_SPref_t) ;
    return 0;
}

long cm_IoctlStoreBehind(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    /* we ignore default asynchrony since we only have one way
     * of doing this today.
     */
    return 0;
}       

long cm_IoctlCreateMountPoint(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    char leaf[LEAF_SIZE];
    long code;
    cm_scache_t *dscp;
    cm_attr_t tattr;
    char *cp;
    cm_req_t req;
    char mpInfo[256];
    char fullCell[256];
    char volume[256];
    char cell[256];
    int ttl;

    cm_InitReq(&req);
        
    code = cm_ParseIoctlParent(ioctlp, userp, &req, &dscp, leaf);
    if (code) return code;

    /* Translate chars for the mount point name */
    TranslateExtendedChars(leaf);

    /* 
     * The fs command allows the user to specify partial cell names on NT.  These must
     * be expanded to the full cell name for mount points so that the mount points will
     * work on UNIX clients.
     */

    /* Extract the possibly partial cell name */
    StringCbCopyA(cell, sizeof(cell), ioctlp->inDatap + 1);      /* Skip the mp type character */
        
    if (cp = strchr(cell, ':')) {
        /* Extract the volume name */
        *cp = 0;
        StringCbCopyA(volume,  sizeof(volume), cp + 1);
	
        /* Get the full name for this cell */
        code = cm_SearchCellFile(cell, fullCell, 0, 0);
#ifdef AFS_AFSDB_ENV
        if (code && cm_dnsEnabled)
            code = cm_SearchCellByDNS(cell, fullCell, &ttl, 0, 0);
#endif
        if (code)
            return CM_ERROR_NOSUCHCELL;
	
        StringCbPrintfA(mpInfo, sizeof(mpInfo), "%c%s:%s", *ioctlp->inDatap, fullCell, volume);
    } else {
        /* No cell name specified */
        StringCbCopyA(mpInfo, sizeof(mpInfo), ioctlp->inDatap);
    }

#ifdef AFS_FREELANCE_CLIENT
    if (cm_freelanceEnabled && dscp == cm_data.rootSCachep) {
        /* we are adding the mount point to the root dir., so call
         * the freelance code to do the add. */
        osi_Log0(afsd_logp,"IoctlCreateMountPoint within Freelance root dir");
        code = cm_FreelanceAddMount(leaf, fullCell, volume, 
                                    *ioctlp->inDatap == '%', NULL);
	cm_ReleaseSCache(dscp);
        return code;
    }
#endif
    /* create the symlink with mode 644.  The lack of X bits tells
     * us that it is a mount point.
     */
    tattr.mask = CM_ATTRMASK_UNIXMODEBITS | CM_ATTRMASK_CLIENTMODTIME;
    tattr.unixModeBits = 0644;
    tattr.clientModTime = time(NULL);

    code = cm_SymLink(dscp, leaf, mpInfo, 0, &tattr, userp, &req);
    if (code == 0 && (dscp->flags & CM_SCACHEFLAG_ANYWATCH))
        smb_NotifyChange(FILE_ACTION_ADDED,
                         FILE_NOTIFY_CHANGE_DIR_NAME,
                         dscp, leaf, NULL, TRUE);

    cm_ReleaseSCache(dscp);
    return code;
}

long cm_IoctlSymlink(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    char leaf[LEAF_SIZE];
    long code;
    cm_scache_t *dscp;
    cm_attr_t tattr;
    char *cp;
    cm_req_t req;

    cm_InitReq(&req);

    code = cm_ParseIoctlParent(ioctlp, userp, &req, &dscp, leaf);
    if (code) return code;

    /* Translate chars for the link name */
    TranslateExtendedChars(leaf);

    /* Translate chars for the linked to name */
    TranslateExtendedChars(ioctlp->inDatap);

    cp = ioctlp->inDatap;		/* contents of link */

#ifdef AFS_FREELANCE_CLIENT
    if (cm_freelanceEnabled && dscp == cm_data.rootSCachep) {
        /* we are adding the symlink to the root dir., so call
         * the freelance code to do the add. */
        if (cp[0] == cp[1] && cp[1] == '\\' && 
            !_strnicmp(cm_NetbiosName,cp+2,strlen(cm_NetbiosName))) 
        {
            /* skip \\AFS\ or \\AFS\all\ */
            char * p;
            p = cp + 2 + strlen(cm_NetbiosName) + 1;
            if ( !_strnicmp("all", p, 3) )
                p += 4;
            cp = p;
        }
        osi_Log0(afsd_logp,"IoctlCreateSymlink within Freelance root dir");
        code = cm_FreelanceAddSymlink(leaf, cp, NULL);
	cm_ReleaseSCache(dscp);
        return code;
    }
#endif

    /* Create symlink with mode 0755. */
    tattr.mask = CM_ATTRMASK_UNIXMODEBITS;
    tattr.unixModeBits = 0755;

    code = cm_SymLink(dscp, leaf, cp, 0, &tattr, userp, &req);
    if (code == 0 && (dscp->flags & CM_SCACHEFLAG_ANYWATCH))
        smb_NotifyChange(FILE_ACTION_ADDED,
                          FILE_NOTIFY_CHANGE_FILE_NAME
                          | FILE_NOTIFY_CHANGE_DIR_NAME,
                          dscp, leaf, NULL, TRUE);

    cm_ReleaseSCache(dscp);

    return code;
}


long cm_IoctlListlink(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    long code;
    cm_scache_t *dscp;
    cm_scache_t *scp;
    char *cp;
    cm_space_t *spacep;
    cm_scache_t *newRootScp;
    cm_req_t req;

    cm_InitReq(&req);

    code = cm_ParseIoctlPath(ioctlp, userp, &req, &dscp);
    if (code) return code;

    cp = ioctlp->inDatap;

    code = cm_Lookup(dscp, cp, CM_FLAG_NOMOUNTCHASE, userp, &req, &scp);
    cm_ReleaseSCache(dscp);
    if (code) return code;

    /* Check that it's a real symlink */
    if (scp->fileType != CM_SCACHETYPE_SYMLINK &&
        scp->fileType != CM_SCACHETYPE_DFSLINK &&
        scp->fileType != CM_SCACHETYPE_INVALID) {
        cm_ReleaseSCache(scp);
        return CM_ERROR_INVAL;
    }

    code = cm_AssembleLink(scp, "", &newRootScp, &spacep, userp, &req);
    cm_ReleaseSCache(scp);
    if (code == 0) {
        cp = ioctlp->outDatap;
        if (newRootScp != NULL) {
            StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), cm_mountRoot);
            StringCbCatA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), "/");
            cp += strlen(cp);
        }
        StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), spacep->data);
        cp += strlen(cp) + 1;
        ioctlp->outDatap = cp;
        cm_FreeSpace(spacep);
        if (newRootScp != NULL)
            cm_ReleaseSCache(newRootScp);
        code = 0;
    } else if (code == CM_ERROR_PATH_NOT_COVERED && 
                scp->fileType == CM_SCACHETYPE_DFSLINK ||
               code == CM_ERROR_NOSUCHPATH &&
                scp->fileType == CM_SCACHETYPE_INVALID) {
        cp = ioctlp->outDatap;
        StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), spacep->data);
        cp += strlen(cp) + 1;
        ioctlp->outDatap = cp;
        cm_FreeSpace(spacep);
        if (newRootScp != NULL)
            cm_ReleaseSCache(newRootScp);
        code = 0;
    }

    return code;
}

long cm_IoctlIslink(struct smb_ioctl *ioctlp, struct cm_user *userp)
{/*CHECK FOR VALID SYMLINK*/
    long code;
    cm_scache_t *dscp;
    cm_scache_t *scp;
    char *cp;
    cm_req_t req;

    cm_InitReq(&req);

    code = cm_ParseIoctlPath(ioctlp, userp, &req, &dscp);
    if (code) return code;

    cp = ioctlp->inDatap;
    osi_LogEvent("cm_IoctlListlink",NULL," name[%s]",cp);

    code = cm_Lookup(dscp, cp, CM_FLAG_NOMOUNTCHASE, userp, &req, &scp);
    cm_ReleaseSCache(dscp);
    if (code) return code;

    /* Check that it's a real symlink */
    if (scp->fileType != CM_SCACHETYPE_SYMLINK &&
        scp->fileType != CM_SCACHETYPE_DFSLINK &&
        scp->fileType != CM_SCACHETYPE_INVALID)
        code = CM_ERROR_INVAL;
    cm_ReleaseSCache(scp);
    return code;
}

long cm_IoctlDeletelink(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    long code;
    cm_scache_t *dscp;
    cm_scache_t *scp;
    char *cp;
    cm_req_t req;

    cm_InitReq(&req);

    code = cm_ParseIoctlPath(ioctlp, userp, &req, &dscp);
    if (code) return code;

    cp = ioctlp->inDatap;

#ifdef AFS_FREELANCE_CLIENT
    if (cm_freelanceEnabled && dscp == cm_data.rootSCachep) {
        /* we are adding the mount point to the root dir., so call
         * the freelance code to do the add. */
        osi_Log0(afsd_logp,"IoctlDeletelink from Freelance root dir");
        code = cm_FreelanceRemoveSymlink(cp);
	cm_ReleaseSCache(dscp);
        return code;
    }
#endif

    code = cm_Lookup(dscp, cp, CM_FLAG_NOMOUNTCHASE, userp, &req, &scp);
        
    /* if something went wrong, bail out now */
    if (code)
        goto done3;
        
    lock_ObtainMutex(&scp->mx);
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code)
        goto done2;
	
    /* now check that this is a real symlink */
    if (scp->fileType != CM_SCACHETYPE_SYMLINK &&
        scp->fileType != CM_SCACHETYPE_DFSLINK &&
        scp->fileType != CM_SCACHETYPE_INVALID) {
        code = CM_ERROR_INVAL;
        goto done1;
    }
	
    /* time to make the RPC, so drop the lock */
    lock_ReleaseMutex(&scp->mx);
        
    /* easier to do it this way */
    code = cm_Unlink(dscp, cp, userp, &req);
    if (code == 0 && (dscp->flags & CM_SCACHEFLAG_ANYWATCH))
        smb_NotifyChange(FILE_ACTION_REMOVED,
                          FILE_NOTIFY_CHANGE_FILE_NAME
                          | FILE_NOTIFY_CHANGE_DIR_NAME,
                          dscp, cp, NULL, TRUE);

    lock_ObtainMutex(&scp->mx);
  done1:
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

  done2:
    lock_ReleaseMutex(&scp->mx);
    cm_ReleaseSCache(scp);

  done3:
    cm_ReleaseSCache(dscp);
    return code;
}

#ifdef QUERY_AFSID
long cm_UsernameToId(char *uname, cm_ucell_t * ucellp, afs_uint32* uid)
{
    afs_int32 code;
    namelist lnames;
    idlist lids;
    static struct afsconf_cell info;
    struct rx_connection *serverconns[MAXSERVERS];
    struct rx_securityClass *sc[3];
    afs_int32 scIndex = 2;	/* authenticated - we have a token */
    struct ubik_client *pruclient = NULL;
    struct afsconf_dir *tdir;
    int i;
    char * p, * r;

    tdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);
    code = afsconf_GetCellInfo(tdir, ucellp->cellp->name, "afsprot", &info);
    afsconf_Close(tdir);

    sc[0] = 0;
    sc[1] = 0;
    sc[2] = 0;

    /* we have the token that was given to us in the settoken 
     * call.   we just have to use it. 
     */
    scIndex = 2;	/* kerberos ticket */
    sc[2] = rxkad_NewClientSecurityObject(rxkad_clear, &ucellp->sessionKey,
					  ucellp->kvno, ucellp->ticketLen,
					  ucellp->ticketp);

    memset(serverconns, 0, sizeof(serverconns));	/* terminate list!!! */
    for (i = 0; i < info.numServers; i++)
	serverconns[i] =
	    rx_NewConnection(info.hostAddr[i].sin_addr.s_addr,
			     info.hostAddr[i].sin_port, PRSRV, sc[scIndex],
			     scIndex);

    code = ubik_ClientInit(serverconns, &pruclient);
    if (code) {
	return code;
    }

    code = rxs_Release(sc[scIndex]);

    lids.idlist_len = 0;
    lids.idlist_val = 0;
    lnames.namelist_len = 1;
    lnames.namelist_val = (prname *) malloc(PR_MAXNAMELEN);
    strncpy(lnames.namelist_val[0], uname, PR_MAXNAMELEN);
    lnames.namelist_val[0][PR_MAXNAMELEN-1] = '\0';
    for ( p=lnames.namelist_val[0], r=NULL; *p; p++ ) {
	if (isupper(*p))
	    *p = tolower(*p);
	if (*p == '@')
	    r = p;
    }
    if (r && !stricmp(r+1,ucellp->cellp->name))
	*r = '\0';

    code = ubik_Call(PR_NameToID, pruclient, 0, &lnames, &lids);
    if (lids.idlist_val) {
	*uid = *lids.idlist_val;
	free(lids.idlist_val);
    }
    if (lnames.namelist_val)
	free(lnames.namelist_val);

    if ( pruclient ) {
	ubik_ClientDestroy(pruclient);
	pruclient = NULL;
    }

    return 0;
}
#endif /* QUERY_AFSID */

long cm_IoctlSetToken(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    char *saveDataPtr;
    char *tp;
    int ticketLen;
    char *ticket;
    int ctSize;
    struct ClearToken ct;
    cm_cell_t *cellp;
    cm_ucell_t *ucellp;
    char *uname = NULL;
    afs_uuid_t uuid;
    int flags;
    char sessionKey[8];
#ifndef AFSIFS
    char *smbname;
#endif
    int release_userp = 0;
    char * wdir = NULL;

    saveDataPtr = ioctlp->inDatap;

    cm_SkipIoctlPath(ioctlp);

    tp = ioctlp->inDatap;

    /* ticket length */
    memcpy(&ticketLen, tp, sizeof(ticketLen));
    tp += sizeof(ticketLen);
    if (ticketLen < MINKTCTICKETLEN || ticketLen > MAXKTCTICKETLEN)
        return CM_ERROR_INVAL;

    /* remember ticket and skip over it for now */
    ticket = tp;
    tp += ticketLen;

    /* clear token size */
    memcpy(&ctSize, tp, sizeof(ctSize));
    tp += sizeof(ctSize);
    if (ctSize != sizeof(struct ClearToken))
        return CM_ERROR_INVAL;

    /* clear token */
    memcpy(&ct, tp, ctSize);
    tp += ctSize;
    if (ct.AuthHandle == -1)
        ct.AuthHandle = 999;	/* more rxvab compat stuff */

    /* more stuff, if any */
    if (ioctlp->inCopied > tp - saveDataPtr) {
        /* flags:  logon flag */
        memcpy(&flags, tp, sizeof(int));
        tp += sizeof(int);

        /* cell name */
        cellp = cm_GetCell(tp, CM_FLAG_CREATE);
        if (!cellp) return CM_ERROR_NOSUCHCELL;
        tp += strlen(tp) + 1;

        /* user name */
        uname = tp;
        tp += strlen(tp) + 1;

#ifndef AFSIFS	/* no SMB username, so we cannot logon based on this */
        if (flags & PIOCTL_LOGON) {
            /* SMB user name with which to associate tokens */
            smbname = tp;
            osi_Log2(smb_logp,"cm_IoctlSetToken for user [%s] smbname [%s]",
                     osi_LogSaveString(smb_logp,uname), osi_LogSaveString(smb_logp,smbname));
            fprintf(stderr, "SMB name = %s\n", smbname);
            tp += strlen(tp) + 1;
        } else {
            osi_Log1(smb_logp,"cm_IoctlSetToken for user [%s]",
                     osi_LogSaveString(smb_logp, uname));
        }
#endif

		/* uuid */
        memcpy(&uuid, tp, sizeof(uuid));
        if (!cm_FindTokenEvent(uuid, sessionKey))
            return CM_ERROR_INVAL;
    } else {
        cellp = cm_data.rootCellp;
        osi_Log0(smb_logp,"cm_IoctlSetToken - no name specified");
    }

#ifndef AFSIFS
    if (flags & PIOCTL_LOGON) {
        userp = smb_FindCMUserByName(smbname, ioctlp->fidp->vcp->rname,
				     SMB_FLAG_CREATE|SMB_FLAG_AFSLOGON);
	release_userp = 1;
    }
#endif /* AFSIFS */

    /* store the token */
    lock_ObtainMutex(&userp->mx);
    ucellp = cm_GetUCell(userp, cellp);
    osi_Log1(smb_logp,"cm_IoctlSetToken ucellp %lx", ucellp);
    ucellp->ticketLen = ticketLen;
    if (ucellp->ticketp)
        free(ucellp->ticketp);	/* Discard old token if any */
    ucellp->ticketp = malloc(ticketLen);
    memcpy(ucellp->ticketp, ticket, ticketLen);
    /*
     * Get the session key from the RPC, rather than from the pioctl.
     */
    /*
    memcpy(&ucellp->sessionKey, ct.HandShakeKey, sizeof(ct.HandShakeKey));
    */
    memcpy(ucellp->sessionKey.data, sessionKey, sizeof(sessionKey));
    ucellp->kvno = ct.AuthHandle;
    ucellp->expirationTime = ct.EndTimestamp;
    ucellp->gen++;
#ifdef QUERY_AFSID
    ucellp->uid = ANONYMOUSID;
#endif
    if (uname) {
        StringCbCopyA(ucellp->userName, MAXKTCNAMELEN, uname);
#ifdef QUERY_AFSID
	cm_UsernameToId(uname, ucellp, &ucellp->uid);
#endif
    }
    ucellp->flags |= CM_UCELLFLAG_RXKAD;
    lock_ReleaseMutex(&userp->mx);

    if (flags & PIOCTL_LOGON) {
        ioctlp->flags |= SMB_IOCTLFLAG_LOGON;
    }

    cm_ResetACLCache(userp);

    if (release_userp)
	cm_ReleaseUser(userp);

    return 0;
}

long cm_IoctlGetTokenIter(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    char *tp, *cp;
    int iterator;
    int temp;
    cm_ucell_t *ucellp;
    struct ClearToken ct;

    cm_SkipIoctlPath(ioctlp);

    tp = ioctlp->inDatap;
    cp = ioctlp->outDatap;

    /* iterator */
    memcpy(&iterator, tp, sizeof(iterator));
    tp += sizeof(iterator);

    lock_ObtainMutex(&userp->mx);

    /* look for token */
    for (;;iterator++) {
        ucellp = cm_FindUCell(userp, iterator);
        if (!ucellp) {
            lock_ReleaseMutex(&userp->mx);
            return CM_ERROR_NOMORETOKENS;
        }
        if (ucellp->flags & CM_UCELLFLAG_RXKAD)
            break;
    }       

    /* new iterator */
    temp = ucellp->iterator + 1;
    memcpy(cp, &temp, sizeof(temp));
    cp += sizeof(temp);

    /* ticket length */
    memcpy(cp, &ucellp->ticketLen, sizeof(ucellp->ticketLen));
    cp += sizeof(ucellp->ticketLen);

    /* ticket */
    memcpy(cp, ucellp->ticketp, ucellp->ticketLen);
    cp += ucellp->ticketLen;

    /* clear token size */
    temp = sizeof(ct);
    memcpy(cp, &temp, sizeof(temp));
    cp += sizeof(temp);

    /* clear token */
    ct.AuthHandle = ucellp->kvno;
    /*
     * Don't give out a real session key here
     */
    /*
    memcpy(ct.HandShakeKey, &ucellp->sessionKey, sizeof(ct.HandShakeKey));
    */
    memset(ct.HandShakeKey, 0, sizeof(ct.HandShakeKey));
    ct.ViceId = 37;			/* XXX */
    ct.BeginTimestamp = 0;		/* XXX */
    ct.EndTimestamp = ucellp->expirationTime;
    memcpy(cp, &ct, sizeof(ct));
    cp += sizeof(ct);

    /* Primary flag (unused) */
    temp = 0;
    memcpy(cp, &temp, sizeof(temp));
    cp += sizeof(temp);

    /* cell name */
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), ucellp->cellp->name);
    cp += strlen(cp) + 1;

    /* user name */
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), ucellp->userName);
    cp += strlen(cp) + 1;

    ioctlp->outDatap = cp;

    lock_ReleaseMutex(&userp->mx);

    return 0;
}

long cm_IoctlGetToken(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    char *cp;
    int temp;
    cm_cell_t *cellp;
    cm_ucell_t *ucellp;
    struct ClearToken ct;
    char *tp;
    afs_uuid_t uuid;
    cm_SkipIoctlPath(ioctlp);

    tp = ioctlp->inDatap;

    cp = ioctlp->outDatap;

    /* cell name is right here */
    cellp = cm_GetCell(tp, 0);
    if (!cellp) 
        return CM_ERROR_NOSUCHCELL;
    tp += strlen(tp) + 1;

    /* uuid */
    memcpy(&uuid, tp, sizeof(uuid));

    lock_ObtainMutex(&userp->mx);

    ucellp = cm_GetUCell(userp, cellp);
    if (!ucellp || !(ucellp->flags & CM_UCELLFLAG_RXKAD)) {
        lock_ReleaseMutex(&userp->mx);
        return CM_ERROR_NOMORETOKENS;
    }

    /* ticket length */
    memcpy(cp, &ucellp->ticketLen, sizeof(ucellp->ticketLen));
    cp += sizeof(ucellp->ticketLen);

    /* ticket */
    memcpy(cp, ucellp->ticketp, ucellp->ticketLen);
    cp += ucellp->ticketLen;

    /* clear token size */
    temp = sizeof(ct);
    memcpy(cp, &temp, sizeof(temp));
    cp += sizeof(temp);

    /* clear token */
    ct.AuthHandle = ucellp->kvno;
    /*
     * Don't give out a real session key here
     */
    /*
    memcpy(ct.HandShakeKey, &ucellp->sessionKey, sizeof(ct.HandShakeKey));
    */
    memset(ct.HandShakeKey, 0, sizeof(ct.HandShakeKey));
    ct.ViceId = 37;			/* XXX */
    ct.BeginTimestamp = 0;		/* XXX */
    ct.EndTimestamp = ucellp->expirationTime;
    memcpy(cp, &ct, sizeof(ct));
    cp += sizeof(ct);

    /* Primary flag (unused) */
    temp = 0;
    memcpy(cp, &temp, sizeof(temp));
    cp += sizeof(temp);

    /* cell name */
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), ucellp->cellp->name);
    cp += strlen(cp) + 1;

    /* user name */
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), ucellp->userName);
    cp += strlen(cp) + 1;

    ioctlp->outDatap = cp;

    lock_ReleaseMutex(&userp->mx);

    cm_RegisterNewTokenEvent(uuid, ucellp->sessionKey.data);

    return 0;
}

long cm_IoctlDelToken(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    char *cp;
    cm_cell_t *cellp;
    cm_ucell_t *ucellp;

    cm_SkipIoctlPath(ioctlp);

    cp = ioctlp->outDatap;

    /* cell name is right here */
    cellp = cm_GetCell(ioctlp->inDatap, 0);
    if (!cellp) return CM_ERROR_NOSUCHCELL;

    lock_ObtainMutex(&userp->mx);

    ucellp = cm_GetUCell(userp, cellp);
    if (!ucellp) {
        lock_ReleaseMutex(&userp->mx);
        return CM_ERROR_NOMORETOKENS;
    }

    osi_Log1(smb_logp,"cm_IoctlDelToken ucellp %lx", ucellp);

    if (ucellp->ticketp) {
        free(ucellp->ticketp);
        ucellp->ticketp = NULL;
    }
    ucellp->ticketLen = 0;
    memset(ucellp->sessionKey.data, 0, 8);
    ucellp->kvno = 0;
    ucellp->expirationTime = 0;
    ucellp->userName[0] = '\0';
    ucellp->flags &= ~CM_UCELLFLAG_RXKAD;
    ucellp->gen++;

    lock_ReleaseMutex(&userp->mx);

    cm_ResetACLCache(userp);

    return 0;
}

long cm_IoctlDelAllToken(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    cm_ucell_t *ucellp;

    lock_ObtainMutex(&userp->mx);

    for (ucellp = userp->cellInfop; ucellp; ucellp = ucellp->nextp) {
        osi_Log1(smb_logp,"cm_IoctlDelAllToken ucellp %lx", ucellp);

	if (ucellp->ticketp) {
	    free(ucellp->ticketp);
	    ucellp->ticketp = NULL;
	}
	ucellp->ticketLen = 0;
	memset(ucellp->sessionKey.data, 0, 8);
	ucellp->kvno = 0;
	ucellp->expirationTime = 0;
	ucellp->userName[0] = '\0';
        ucellp->flags &= ~CM_UCELLFLAG_RXKAD;
        ucellp->gen++;
    }

    lock_ReleaseMutex(&userp->mx);

    cm_ResetACLCache(userp);

    return 0;
}

long cm_IoctlMakeSubmount(smb_ioctl_t *ioctlp, cm_user_t *userp)
{
    char afspath[MAX_PATH];
    char *submountreqp;
    int nextAutoSubmount;
    HKEY hkSubmounts;
    DWORD dwType, dwSize;
    DWORD status;
    DWORD dwIndex;
    DWORD dwSubmounts;

    cm_SkipIoctlPath(ioctlp);

    /* Serialize this one, to prevent simultaneous mods
     * to afsdsbmt.ini
     */
    lock_ObtainMutex(&cm_Afsdsbmt_Lock);

    /* Parse the input parameters--first the required afs path,
     * then the requested submount name (which may be "").
     */
    cm_NormalizeAfsPath (afspath, sizeof(afspath), ioctlp->inDatap);
    submountreqp = ioctlp->inDatap + (strlen(ioctlp->inDatap)+1);

    /* If the caller supplied a suggested submount name, see if
     * that submount name is in use... if so, the submount's path
     * has to match our path.
     */

    RegCreateKeyEx( HKEY_LOCAL_MACHINE, 
                    AFSREG_CLT_OPENAFS_SUBKEY "\\Submounts",
                    0, 
                    "AFS", 
                    REG_OPTION_NON_VOLATILE,
                    KEY_READ|KEY_WRITE|KEY_QUERY_VALUE,
                    NULL, 
                    &hkSubmounts,
                    NULL );

    if (submountreqp && *submountreqp) {
        char submountPathNormalized[MAX_PATH];
        char submountPath[MAX_PATH];

        dwSize = sizeof(submountPath);
        status = RegQueryValueEx( hkSubmounts, submountreqp, 0,
                                  &dwType, submountPath, &dwSize);

        if (status != ERROR_SUCCESS) {

            /* The suggested submount name isn't in use now--
             * so we can safely map the requested submount name
             * to the supplied path. Remember not to write the
             * leading "/afs" when writing out the submount.
             */
            RegSetValueEx( hkSubmounts, submountreqp, 0,
                           REG_EXPAND_SZ, 
                           (strlen(&afspath[strlen(cm_mountRoot)])) ?
                           &afspath[strlen(cm_mountRoot)]:"/",
                           (strlen(&afspath[strlen(cm_mountRoot)])) ?
                           (DWORD)strlen(&afspath[strlen(cm_mountRoot)])+1:2);

            RegCloseKey( hkSubmounts );
            StringCbCopyA(ioctlp->outDatap, SMB_IOCTL_MAXDATA - (ioctlp->outDatap - ioctlp->outAllocp), submountreqp);
            ioctlp->outDatap += strlen(ioctlp->outDatap) +1;
            lock_ReleaseMutex(&cm_Afsdsbmt_Lock);
            return 0;
        }

        /* The suggested submount name is already in use--if the
         * supplied path matches the submount's path, we can still
         * use the suggested submount name.
         */
        cm_NormalizeAfsPath (submountPathNormalized, sizeof(submountPathNormalized), submountPath);
        if (!strcmp (submountPathNormalized, afspath)) {
            StringCbCopyA(ioctlp->outDatap, SMB_IOCTL_MAXDATA - (ioctlp->outDatap - ioctlp->outAllocp), submountreqp);
            ioctlp->outDatap += strlen(ioctlp->outDatap) +1;
            RegCloseKey( hkSubmounts );
            lock_ReleaseMutex(&cm_Afsdsbmt_Lock);
            return 0;
        }
    }

    RegQueryInfoKey( hkSubmounts,
                     NULL,  /* lpClass */
                     NULL,  /* lpcClass */
                     NULL,  /* lpReserved */
                     NULL,  /* lpcSubKeys */
                     NULL,  /* lpcMaxSubKeyLen */
                     NULL,  /* lpcMaxClassLen */
                     &dwSubmounts, /* lpcValues */
                     NULL,  /* lpcMaxValueNameLen */
                     NULL,  /* lpcMaxValueLen */
                     NULL,  /* lpcbSecurityDescriptor */
                     NULL   /* lpftLastWriteTime */
                     );


    /* Having obtained a list of all available submounts, start
     * searching that list for a path which matches the requested
     * AFS path. We'll also keep track of the highest "auto15"/"auto47"
     * submount, in case we need to add a new one later.
     */

    nextAutoSubmount = 1;

    for ( dwIndex = 0; dwIndex < dwSubmounts; dwIndex ++ ) {
        char submountPathNormalized[MAX_PATH];
        char submountPath[MAX_PATH] = "";
        DWORD submountPathLen = sizeof(submountPath);
        char submountName[MAX_PATH];
        DWORD submountNameLen = sizeof(submountName);

        dwType = 0;
        RegEnumValue( hkSubmounts, dwIndex, submountName, &submountNameLen, NULL,
                      &dwType, submountPath, &submountPathLen);
        if (dwType == REG_EXPAND_SZ) {
            char buf[MAX_PATH];
            StringCbCopyA(buf, MAX_PATH, submountPath);
            submountPathLen = ExpandEnvironmentStrings(buf, submountPath, MAX_PATH);
            if (submountPathLen > MAX_PATH)
                continue;
        }

        /* If this is an Auto### submount, remember its ### value */
        if ((!strnicmp (submountName, "auto", 4)) &&
             (isdigit (submountName[strlen("auto")]))) {
            int thisAutoSubmount;
            thisAutoSubmount = atoi (&submountName[strlen("auto")]);
            nextAutoSubmount = max (nextAutoSubmount,
                                     thisAutoSubmount+1);
        }       

        if ((submountPathLen == 0) ||
             (submountPathLen == sizeof(submountPath) - 1)) {
            continue;
        }

        /* See if the path for this submount matches the path
         * that our caller specified. If so, we can return
         * this submount.
         */
        cm_NormalizeAfsPath (submountPathNormalized, sizeof(submountPathNormalized), submountPath);
        if (!strcmp (submountPathNormalized, afspath)) {
            StringCbCopyA(ioctlp->outDatap, SMB_IOCTL_MAXDATA - (ioctlp->outDatap - ioctlp->outAllocp), submountName);
            ioctlp->outDatap += strlen(ioctlp->outDatap) +1;
            RegCloseKey(hkSubmounts);
            lock_ReleaseMutex(&cm_Afsdsbmt_Lock);
            return 0;

        }
    }

    /* We've been through the entire list of existing submounts, and
     * didn't find any which matched the specified path. So, we'll
     * just have to add one. Remember not to write the leading "/afs"
     * when writing out the submount.
     */

    StringCbPrintfA(ioctlp->outDatap, SMB_IOCTL_MAXDATA - (ioctlp->outDatap - ioctlp->outAllocp), "auto%ld", nextAutoSubmount);

    RegSetValueEx( hkSubmounts, 
                   ioctlp->outDatap,
                   0,
                   REG_EXPAND_SZ, 
                   (strlen(&afspath[strlen(cm_mountRoot)])) ?
                   &afspath[strlen(cm_mountRoot)]:"/",
                   (strlen(&afspath[strlen(cm_mountRoot)])) ?
                   (DWORD)strlen(&afspath[strlen(cm_mountRoot)])+1:2);

    ioctlp->outDatap += strlen(ioctlp->outDatap) +1;
    RegCloseKey(hkSubmounts);
    lock_ReleaseMutex(&cm_Afsdsbmt_Lock);
    return 0;
}

long cm_IoctlGetRxkcrypt(smb_ioctl_t *ioctlp, cm_user_t *userp)
{
    memcpy(ioctlp->outDatap, &cryptall, sizeof(cryptall));
    ioctlp->outDatap += sizeof(cryptall);

    return 0;
}

long cm_IoctlSetRxkcrypt(smb_ioctl_t *ioctlp, cm_user_t *userp)
{
    afs_int32 c = cryptall;

    cm_SkipIoctlPath(ioctlp);

    memcpy(&cryptall, ioctlp->inDatap, sizeof(cryptall));

    if (c != cryptall) {
	if (cryptall)
            LogEvent(EVENTLOG_INFORMATION_TYPE, MSG_CRYPT_ON);
	else
            LogEvent(EVENTLOG_INFORMATION_TYPE, MSG_CRYPT_OFF);
    }
    return 0;
}

long cm_IoctlRxStatProcess(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    afs_int32 flags;
    int code = 0;

    cm_SkipIoctlPath(ioctlp);

    memcpy((char *)&flags, ioctlp->inDatap, sizeof(afs_int32));
    if (!(flags & AFSCALL_RXSTATS_MASK) || (flags & ~AFSCALL_RXSTATS_MASK)) {
        return -1;
    }
    if (flags & AFSCALL_RXSTATS_ENABLE) {
        rx_enableProcessRPCStats();
    }
    if (flags & AFSCALL_RXSTATS_DISABLE) {
        rx_disableProcessRPCStats();
    }
    if (flags & AFSCALL_RXSTATS_CLEAR) {
        rx_clearProcessRPCStats(AFS_RX_STATS_CLEAR_ALL);
    }
    return 0;
}

long cm_IoctlRxStatPeer(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    afs_int32 flags;
    int code = 0;

    cm_SkipIoctlPath(ioctlp);

    memcpy((char *)&flags, ioctlp->inDatap, sizeof(afs_int32));
    if (!(flags & AFSCALL_RXSTATS_MASK) || (flags & ~AFSCALL_RXSTATS_MASK)) {
	return -1;
    }
    if (flags & AFSCALL_RXSTATS_ENABLE) {
        rx_enablePeerRPCStats();
    }
    if (flags & AFSCALL_RXSTATS_DISABLE) {
        rx_disablePeerRPCStats();
    }
    if (flags & AFSCALL_RXSTATS_CLEAR) {
        rx_clearPeerRPCStats(AFS_RX_STATS_CLEAR_ALL);
    }
    return 0;
}

long cm_IoctlGetSMBName(smb_ioctl_t *ioctlp, cm_user_t *userp)
{
  smb_user_t *uidp = ioctlp->uidp;

  if (uidp && uidp->unp) {
    memcpy(ioctlp->outDatap, uidp->unp->name, strlen(uidp->unp->name));
    ioctlp->outDatap += strlen(uidp->unp->name);
  }

  return 0;
}

long cm_IoctlUUIDControl(struct smb_ioctl * ioctlp, struct cm_user *userp)
{
    long cmd;
    afsUUID uuid;

    memcpy(&cmd, ioctlp->inDatap, sizeof(long));

    if (cmd) {             /* generate a new UUID */
        UuidCreate((UUID *) &uuid);
        cm_data.Uuid = uuid;
	cm_ForceNewConnectionsAllServers();
    }

    memcpy(ioctlp->outDatap, &cm_data.Uuid, sizeof(cm_data.Uuid));
    ioctlp->outDatap += sizeof(cm_data.Uuid);

    return 0;
}

/* 
 * functions to dump contents of various structures. 
 * In debug build (linked with crt debug library) will dump allocated but not freed memory
 */
extern int cm_DumpSCache(FILE *outputFile, char *cookie, int lock);
extern int cm_DumpBufHashTable(FILE *outputFile, char *cookie, int lock);
extern int smb_DumpVCP(FILE *outputFile, char *cookie, int lock);

long cm_IoctlMemoryDump(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    long inValue = 0;
    HANDLE hLogFile;
    char logfileName[MAX_PATH+1];
    char *cookie;
    DWORD dwSize;
  
#ifdef _DEBUG  
    static _CrtMemState memstate;
#endif
  
    cm_SkipIoctlPath(ioctlp);
    memcpy(&inValue, ioctlp->inDatap, sizeof(long));
  
    dwSize = GetEnvironmentVariable("TEMP", logfileName, sizeof(logfileName));
    if ( dwSize == 0 || dwSize > sizeof(logfileName) )
    {
        GetWindowsDirectory(logfileName, sizeof(logfileName));
    }
    strncat(logfileName, "\\afsd_alloc.log", sizeof(logfileName));

    hLogFile = CreateFile(logfileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  
    if (!hLogFile)
    {
      /* error */
      inValue = -1;
      memcpy(ioctlp->outDatap, &inValue, sizeof(long));
      ioctlp->outDatap += sizeof(long);
      
      return 0;               
    }
  
    SetFilePointer(hLogFile, 0, NULL, FILE_END);
  
    cookie = inValue ? "b" : "e";
  
#ifdef _DEBUG  
  
    if (inValue)
    {
      _CrtMemCheckpoint(&memstate);           
    }
    else
    {
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, hLogFile);
        _CrtMemDumpAllObjectsSince(&memstate);
    }
#endif
  
    /* dump all interesting data */
    cm_DumpSCache(hLogFile, cookie, 1);
    cm_DumpVolumes(hLogFile, cookie, 1);
    cm_DumpBufHashTable(hLogFile, cookie, 1);
    smb_DumpVCP(hLogFile, cookie, 1);

    CloseHandle(hLogFile);                          
  
    inValue = 0;	/* success */
    memcpy(ioctlp->outDatap, &inValue, sizeof(long));
    ioctlp->outDatap += sizeof(long);
  
    return 0;
}


static long 
cm_CheckServersStatus(cm_serverRef_t *serversp)
{
    long code = 0;
    cm_serverRef_t *tsrp;
    cm_server_t *tsp;
    int someBusy = 0, someOffline = 0, allOffline = 1, allBusy = 1, allDown = 1;

    if (serversp == NULL) {
	osi_Log1(afsd_logp, "cm_CheckServersStatus returning 0x%x", CM_ERROR_ALLDOWN);
	return CM_ERROR_ALLDOWN;
    }

    lock_ObtainRead(&cm_serverLock);
    for (tsrp = serversp; tsrp; tsrp=tsrp->next) {
        if (tsp = tsrp->server) {
            cm_GetServerNoLock(tsp);
            lock_ReleaseRead(&cm_serverLock);
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
                    cm_PutServer(tsp);
                    goto done;
                }
            }
            lock_ObtainRead(&cm_serverLock);
            cm_PutServerNoLock(tsp);
        }
    }   
    lock_ReleaseRead(&cm_serverLock);

    if (allDown) 
        code = CM_ERROR_ALLDOWN;
    else if (allBusy) 
        code = CM_ERROR_ALLBUSY;
    else if (allOffline || (someBusy && someOffline))
        code = CM_ERROR_ALLOFFLINE;

  done:
    osi_Log1(afsd_logp, "cm_CheckServersStatus returning 0x%x", code);
    return code;
}


long cm_IoctlPathAvailability(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
    long code;
    cm_scache_t *scp;
    cm_cell_t *cellp;
    cm_volume_t *tvp;
    cm_vol_state_t *statep;
    afs_uint32 volume;
    cm_req_t req;

    cm_InitReq(&req);

    code = cm_ParseIoctlPath(ioctlp, userp, &req, &scp);
    if (code) 
        return code;
        
    volume = scp->fid.volume;

    cellp = cm_FindCellByID(scp->fid.cell);

    cm_ReleaseSCache(scp);

    if (!cellp)
	return CM_ERROR_NOSUCHCELL;

    code = cm_GetVolumeByID(cellp, volume, userp, &req, CM_GETVOL_FLAG_CREATE, &tvp);
    if (code) 
        return code;
	
    if (volume == tvp->rw.ID)
        statep = &tvp->rw;
    else if (volume == tvp->ro.ID)
        statep = &tvp->ro;
    else
        statep = &tvp->bk;

    switch (statep->state) {
    case vl_online:
    case vl_unknown:
        code = 0;
        break;
    case vl_busy:
        code = CM_ERROR_ALLBUSY;
        break;
    case vl_offline:
        code = CM_ERROR_ALLOFFLINE;
        break;
    case vl_alldown:
        code = CM_ERROR_ALLDOWN;
        break;
    }
    cm_PutVolume(tvp);
    return code;
}       


