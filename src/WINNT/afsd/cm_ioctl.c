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
#else
#include <sys/socket.h>
#endif /* !DJGPP */
#include <errno.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <osi.h>

#include "afsd.h"
#include "afsd_init.h"

#include "smb.h"

#ifndef DJGPP
#include <rxkad.h>
#include "afsrpc.h"
#else
#include <rx/rxkad.h>
#include "afsrpc95.h"
#endif

#include "cm_rpc.h"

/* Copied from afs_tokens.h */
#define PIOCTL_LOGON	0x1
#define MAX_PATH 260

osi_mutex_t cm_Afsdsbmt_Lock;

extern afs_int32 cryptall;

void cm_InitIoctl(void)
{
	lock_InitializeMutex(&cm_Afsdsbmt_Lock, "AFSDSBMT.INI Access Lock");
}

long cm_FlushFile(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp)
{
	long code;

	lock_ObtainWrite(&scp->bufCreateLock);
	code = buf_FlushCleanPages(scp, userp, reqp);
        
        lock_ObtainMutex(&scp->mx);
	scp->cbServerp = NULL;
        scp->cbExpires = 0;
        lock_ReleaseMutex(&scp->mx);

	lock_ReleaseWrite(&scp->bufCreateLock);
	cm_dnlcPurgedp(scp);

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
	for (hash=0; hash < cm_hashTableSize; hash++) {
		for (scp=cm_hashTablep[hash]; scp; scp=scp->nextp) {
			scp->refCount++;
			lock_ReleaseWrite(&cm_scacheLock);
			lock_ObtainMutex(&scp->mx);
			cm_InvalidateACLUser(scp, userp);
			lock_ReleaseMutex(&scp->mx);
			lock_ObtainWrite(&cm_scacheLock);
			scp->refCount--;
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
  char *p;
  
        if (!str || !*str)
                return;

#ifndef DJGPP
        CharToOem(str, str);
#else
        p = str;
        while (*p) *p++ &= 0x7f;  /* turn off high bit; probably not right */
#endif
}
        
/* parse the passed-in file name and do a namei on it.  If we fail,
 * return an error code, otherwise return the vnode located in *scpp.
 */
long cm_ParseIoctlPath(smb_ioctl_t *ioctlp, cm_user_t *userp, cm_req_t *reqp,
	cm_scache_t **scpp)
{
	long code;
	cm_scache_t *substRootp;

        /* This is usually the file name, but for StatMountPoint it is the path. */
	TranslateExtendedChars(ioctlp->inDatap);

	code = cm_NameI(cm_rootSCachep, ioctlp->prefix->data,
		CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
		userp, ioctlp->tidPathp, reqp, &substRootp);
	if (code) return code;
        
        code = cm_NameI(substRootp, ioctlp->inDatap, CM_FLAG_FOLLOW,
        	userp, NULL, reqp, scpp);
	if (code) return code;
        
	/* # of bytes of path */
        code = strlen(ioctlp->inDatap) + 1;
        ioctlp->inDatap += code;

        /* This is usually nothing, but for StatMountPoint it is the file name. */
        TranslateExtendedChars(ioctlp->inDatap);

	/* and return success */
        return 0;
}

void cm_SkipIoctlPath(smb_ioctl_t *ioctlp)
{
	long temp;
        
        temp = strlen(ioctlp->inDatap) + 1;
        ioctlp->inDatap += temp;
}


/* format the specified path to look like "/afs/<cellname>/usr", by
 * adding "/afs" (if necessary) in front, changing any \'s to /'s, and
 * removing any trailing "/"'s. One weirdo caveat: "/afs" will be
 * intentionally returned as "/afs/"--this makes submount manipulation
 * easier (because we can always jump past the initial "/afs" to find
 * the AFS path that should be written into afsdsbmt.ini).
 */
void cm_NormalizeAfsPath (char *outpathp, char *inpathp)
{
	char *cp;

	if (!strnicmp (inpathp, "/afs", strlen("/afs")))
		lstrcpy (outpathp, inpathp);
	else if (!strnicmp (inpathp, "\\afs", strlen("\\afs")))
		lstrcpy (outpathp, inpathp);
	else if ((inpathp[0] == '/') || (inpathp[0] == '\\'))
		sprintf (outpathp, "/afs%s", inpathp);
	else // inpathp looks like "<cell>/usr"
		sprintf (outpathp, "/afs/%s", inpathp);

	for (cp = outpathp; *cp != 0; ++cp) {
		if (*cp == '\\')
			*cp = '/';
	}

	if (strlen(outpathp) && (outpathp[strlen(outpathp)-1] == '/')) {
           outpathp[strlen(outpathp)-1] = 0;
	}

	if (!strcmpi (outpathp, "/afs")) {
           strcpy (outpathp, "/afs/");
	}
}

/* parse the passed-in file name and do a namei on its parent.  If we fail,
 * return an error code, otherwise return the vnode located in *scpp.
 */
long cm_ParseIoctlParent(smb_ioctl_t *ioctlp, cm_user_t *userp, cm_req_t *reqp,
			 cm_scache_t **scpp, char *leafp)
{
	long code;
        char tbuffer[1024];
        char *tp, *jp;
	cm_scache_t *substRootp;

	strcpy(tbuffer, ioctlp->inDatap);
        tp = strrchr(tbuffer, '\\');
	jp = strrchr(tbuffer, '/');
	if (!tp)
		tp = jp;
	else if (jp && (tp - tbuffer) < (jp - tbuffer))
		tp = jp;
        if (!tp) {
        	strcpy(tbuffer, "\\");
                if (leafp) strcpy(leafp, ioctlp->inDatap);
	}
        else {
        	*tp = 0;
                if (leafp) strcpy(leafp, tp+1);
	}

	code = cm_NameI(cm_rootSCachep, ioctlp->prefix->data,
		CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
		userp, ioctlp->tidPathp, reqp, &substRootp);
	if (code) return code;

        code = cm_NameI(substRootp, tbuffer, CM_FLAG_FOLLOW,
        	userp, NULL, reqp, scpp);
	if (code) return code;
        
	/* # of bytes of path */
        code = strlen(ioctlp->inDatap) + 1;
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
		code = cm_Conn(&scp->fid, userp, &req, &connp);
                if (code) continue;
                
                code = RXAFS_FetchACL(connp->callp, &fid, &acl, &fileStatus, &volSync);
	} while (cm_Analyze(connp, userp, &req, &scp->fid,
			    &volSync, NULL, code));
	code = cm_MapRPCError(code, &req);
	cm_ReleaseSCache(scp);
        
        if (code) return code;
        
	/* skip over return data */
        tlen = strlen(ioctlp->outDatap) + 1;
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
        
        cellp = cm_FindCellByID(scp->fid.cell);
        if (cellp) {
		strcpy(ioctlp->outDatap, cellp->namep);
                ioctlp->outDatap += strlen(ioctlp->outDatap) + 1;
                code = 0;
        }
        else code = CM_ERROR_NOSUCHCELL;
        
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

        cm_InitReq(&req);

        code = cm_ParseIoctlPath(ioctlp, userp, &req, &scp);
        if (code) return code;
	
	/* now make the get acl call */
	fid.Volume = scp->fid.volume;
        fid.Vnode = scp->fid.vnode;
        fid.Unique = scp->fid.unique;
	do {
	        acl.AFSOpaque_val = ioctlp->inDatap;
	        acl.AFSOpaque_len = strlen(ioctlp->inDatap)+1;
		code = cm_Conn(&scp->fid, userp, &req, &connp);
                if (code) continue;
                
                code = RXAFS_StoreACL(connp->callp, &fid, &acl, &fileStatus, &volSync);
	} while (cm_Analyze(connp, userp, &req, &scp->fid,
			    &volSync, NULL, code));
	code = cm_MapRPCError(code, &req);

	/* invalidate cache info, since we just trashed the ACL cache */
	lock_ObtainMutex(&scp->mx);
        cm_DiscardSCache(scp);
	lock_ReleaseMutex(&scp->mx);

	cm_ReleaseSCache(scp);
        
        return code;
}

long cm_IoctlFlushVolume(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
	long code;
        cm_scache_t *scp;
        unsigned long volume;
        int i;
	cm_req_t req;

        cm_InitReq(&req);

        code = cm_ParseIoctlPath(ioctlp, userp, &req, &scp);
        if (code) return code;
        
	volume = scp->fid.volume;
        cm_ReleaseSCache(scp);

	lock_ObtainWrite(&cm_scacheLock);
	for(i=0; i<cm_hashTableSize; i++) {
		for(scp = cm_hashTablep[i]; scp; scp = scp->nextp) {
			if (scp->fid.volume == volume) {
				scp->refCount++;
                                lock_ReleaseWrite(&cm_scacheLock);

				/* now flush the file */
				cm_FlushFile(scp, userp, &req);

                                lock_ObtainWrite(&cm_scacheLock);
                                scp->refCount--;
                        }
                }
        }
	lock_ReleaseWrite(&cm_scacheLock);

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

	cm_InitReq(&req);

	code = cm_ParseIoctlPath(ioctlp, userp, &req, &scp);
        if (code) return code;

	cellp = cm_FindCellByID(scp->fid.cell);
        osi_assert(cellp);

	if (scp->flags & CM_SCACHEFLAG_RO) {
        	cm_ReleaseSCache(scp);
        	return CM_ERROR_READONLY;
	}

	code = cm_GetVolumeByID(cellp, scp->fid.volume, userp, &req, &tvp);
        if (code) {
		cm_ReleaseSCache(scp);
        	return code;
	}

	/* Copy the junk out, using cp as a roving pointer. */
	cp = ioctlp->inDatap;
	memcpy((char *)&volStat, cp, sizeof(AFSFetchVolumeStatus));
	cp += sizeof(AFSFetchVolumeStatus);
	strcpy(volName, cp);
	cp += strlen(volName)+1;
	strcpy(offLineMsg, cp);
	cp +=  strlen(offLineMsg)+1;
	strcpy(motd, cp);
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
		code = cm_Conn(&scp->fid, userp, &req, &tcp);
		if (code) continue;

		code = RXAFS_SetVolumeStatus(tcp->callp, scp->fid.volume,
			&storeStat, volName, offLineMsg, motd);
	} while (cm_Analyze(tcp, userp, &req, &scp->fid, NULL, NULL, code));
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
	strcpy(cp, volName);
	cp += strlen(volName)+1;
	strcpy(cp, offLineMsg);
	cp += strlen(offLineMsg)+1;
	strcpy(cp, motd);
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
	cm_conn_t *tcp;
	register long code;
	AFSFetchVolumeStatus volStat;
	register char *cp;
	char *Name;
        char *OfflineMsg;
        char *MOTD;
	cm_req_t req;

	cm_InitReq(&req);

	code = cm_ParseIoctlPath(ioctlp, userp, &req, &scp);
        if (code) return code;

	Name = volName;
	OfflineMsg = offLineMsg;
	MOTD = motd;
	do {
		code = cm_Conn(&scp->fid, userp, &req, &tcp);
                if (code) continue;

		code = RXAFS_GetVolumeStatus(tcp->callp, scp->fid.volume,
			&volStat, &Name, &OfflineMsg, &MOTD);
	} while (cm_Analyze(tcp, userp, &req, &scp->fid, NULL, NULL, code));
	code = cm_MapRPCError(code, &req);

	cm_ReleaseSCache(scp);
	if (code) return code;

        /* Copy all this junk into msg->im_data, keeping track of the lengths. */
	cp = ioctlp->outDatap;
	memcpy(cp, (char *)&volStat, sizeof(AFSFetchVolumeStatus));
	cp += sizeof(AFSFetchVolumeStatus);
	strcpy(cp, volName);
	cp += strlen(volName)+1;
	strcpy(cp, offLineMsg);
	cp += strlen(offLineMsg)+1;
	strcpy(cp, motd);
	cp += strlen(motd)+1;

	/* return new size */
	ioctlp->outDatap = cp;

	return 0;
}

long cm_IoctlWhereIs(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
	long code;
        cm_scache_t *scp;
        cm_cell_t *cellp;
        cm_volume_t *tvp;
	cm_serverRef_t *tsrp;
        cm_server_t *tsp;
        unsigned long volume;
        char *cp;
        cm_req_t req;

	cm_InitReq(&req);

        code = cm_ParseIoctlPath(ioctlp, userp, &req, &scp);
        if (code) return code;
        
	volume = scp->fid.volume;

	cellp = cm_FindCellByID(scp->fid.cell);
        osi_assert(cellp);

        cm_ReleaseSCache(scp);

	code = cm_GetVolumeByID(cellp, volume, userp, &req, &tvp);
        if (code) return code;
	
        cp = ioctlp->outDatap;
        
	lock_ObtainMutex(&tvp->mx);
	tsrp = cm_GetVolServers(tvp, volume);
	lock_ObtainRead(&cm_serverLock);
	while(tsrp) {
		tsp = tsrp->server;
		memcpy(cp, (char *)&tsp->addr.sin_addr.s_addr, sizeof(long));
		cp += sizeof(long);
                tsrp = tsrp->next;
	}
	lock_ReleaseRead(&cm_serverLock);
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
                strcpy(cp, scp->mountPointStringp);
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
		goto done;
	}
        
	lock_ObtainMutex(&scp->mx);
        code = cm_SyncOp(scp, NULL, userp, &req, 0,
        	CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        if (code) {
        	lock_ReleaseMutex(&scp->mx);
        	cm_ReleaseSCache(scp);
		goto done;
	}
	
        /* now check that this is a real mount point */
        if (scp->fileType != CM_SCACHETYPE_MOUNTPOINT) {
		lock_ReleaseMutex(&scp->mx);
        	cm_ReleaseSCache(scp);
                code = CM_ERROR_INVAL;
                goto done;
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

done:
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
			memcpy(cp, (char *)&cm_daemonCheckInterval, sizeof(long));
			ioctlp->outDatap += sizeof(long);
			if (csi.tinterval > 0) {
				if (!smb_SUser(userp))
					return CM_ERROR_NOACCESS;
				cm_daemonCheckInterval = csi.tinterval;
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
	for(tsp = cm_allServersp; tsp; tsp=tsp->allNextp) {
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
	cm_CheckVolumes();
	return 0;
}

long cm_IoctlSetCacheSize(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
	long temp;
        long code;
        
	cm_SkipIoctlPath(ioctlp);

        memcpy(&temp, ioctlp->inDatap, sizeof(temp));
        if (temp == 0) temp = buf_nOrigBuffers;
        else {
		/* temp is in 1K units, convert to # of buffers */
                temp = temp / (buf_bufferSize / 1024);
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
	}
        
        if (inValue & 2) {
        	/* set tracing value to low order bit */
        	if ((inValue & 1) == 0) {
			/* disable tracing */
	                osi_LogDisable(afsd_logp);
	        }
	        else {
			/* enable tracing */
	        	osi_LogEnable(afsd_logp);
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
        parms.parms[0] = buf_nbuffers * (buf_bufferSize / 1024);
        
        /* and then the actual # of buffers in use (not in the free list, I guess,
         * will be what we do).
         */
        parms.parms[1] = (buf_nbuffers - buf_CountFreeList()) * (buf_bufferSize / 1024);
        
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
	for(tcellp = cm_allCellsp; tcellp; tcellp = tcellp->nextp) {
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
		serverRefp = tcellp->vlServersp;
		for(i=0; i<max; i++) {
			if (!serverRefp) break;
			serverp = serverRefp->server;
			memcpy(cp, &serverp->addr.sin_addr.s_addr, sizeof(long));
			cp += sizeof(long);
                        serverRefp = serverRefp->next;
		}
		lock_ReleaseRead(&cm_serverLock);
		cp = basep + max * sizeof(afs_int32);
		strcpy(cp, tcellp->namep);
		cp += strlen(tcellp->namep)+1;
		ioctlp->outDatap = cp;
	}

	if (tcellp) return 0;
	else return CM_ERROR_NOMORETOKENS;	/* mapped to EDOM */
}

long cm_IoctlNewCell(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
	/* don't need to do, since NT cache manager will re-read afsdcell.ini
         * on every access to a new cell.
         */
	return CM_ERROR_INVAL;
}

long cm_IoctlGetWsCell(smb_ioctl_t *ioctlp, cm_user_t *userp)
{
	/* if we don't know our default cell, return failure */
	if (cm_rootCellp == NULL) {
		return CM_ERROR_NOSUCHCELL;
	}

	/* return the default cellname to the caller */
	strcpy(ioctlp->outDatap, cm_rootCellp->namep);
	ioctlp->outDatap += strlen(ioctlp->outDatap) +1;
        
	/* done: success */
        return 0;
}

long cm_IoctlSysName(struct smb_ioctl *ioctlp, struct cm_user *userp)
{
	long setSysName;
        char *cp;
        
	cm_SkipIoctlPath(ioctlp);

        memcpy(&setSysName, ioctlp->inDatap, sizeof(long));
        ioctlp->inDatap += sizeof(long);
        
        if (setSysName) {
		strcpy(cm_sysName, ioctlp->inDatap);
        }
        else {
		/* return the sysname to the caller */
                setSysName = 1;	/* really means "found sys name */
		cp = ioctlp->outDatap;
                memcpy(cp, &setSysName, sizeof(long));
                cp += sizeof(long);	/* skip found flag */
                strcpy(cp, cm_sysName);
                cp += strlen(cp) + 1;	/* skip name and terminating null char */
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
	if (!cellp) return CM_ERROR_NOSUCHCELL;

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
	if (!cellp) return CM_ERROR_NOSUCHCELL;

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
	else    type = CM_SERVER_FILE;

	for ( i=0; i < noServers; i++) 
	{
		srvin          = &(spin->servers[i]);
		rank           = srvin->rank + (rand() & 0x000f);
		tmp.sin_addr   = srvin->host;
		tmp.sin_family = AF_INET;

		tsp = cm_FindServer(&tmp, type);
		if ( tsp )		/* an existing server */
		{
			tsp->ipRank = rank; /* no need to protect by mutex*/

			if ( type == CM_SERVER_FILE) /* fileserver */
			{
			    /* find volumes which might have RO copy 
			    /* on server and change the ordering of 
			    ** their RO list */
			    cm_ChangeRankVolume(tsp);
			}
			else 	
			{
			    /* set preferences for an existing vlserver */
			    cm_ChangeRankCellVLServer(tsp);
			}
		}
		else			/* add a new server without a cell*/
		{
			tsp = cm_NewServer(&tmp, type, NULL);
			tsp->ipRank = rank;
		}
		cm_PutServer(tsp);
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

	for(tsp=cm_allServersp, i=0; tsp && noServers; tsp=tsp->allNextp,i++){
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
	char leaf[256];
        long code;
        cm_scache_t *dscp;
        cm_attr_t tattr;
        char *cp;
	cm_req_t req;
        char mpInfo[256];
        char fullCell[256];
	char volume[256];
	char cell[256];

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
	strcpy(cell, ioctlp->inDatap + 1);      /* Skip the mp type character */
        
        if (cp = strchr(cell, ':')) {
		/* Extract the volume name */
	        *cp = 0;
		strcpy(volume,  cp + 1);
	
	        /* Get the full name for this cell */
	        code = cm_SearchCellFile(cell, fullCell, 0, 0);
		if (code)
			return CM_ERROR_NOSUCHCELL;
	
	        sprintf(mpInfo, "%c%s:%s", *ioctlp->inDatap, fullCell, volume);
	} else {
	        /* No cell name specified */
	        strcpy(mpInfo, ioctlp->inDatap);
        }

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
	char leaf[256];
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

extern long cm_AssembleLink(cm_scache_t *linkScp, char *pathSuffixp,
	cm_scache_t **newRootScpp, cm_space_t **newSpaceBufferp,
	cm_user_t *userp, cm_req_t *reqp);

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
	if (scp->fileType != CM_SCACHETYPE_SYMLINK){
		cm_ReleaseSCache(scp);
		return CM_ERROR_INVAL;
	}

	code = cm_AssembleLink(scp, "", &newRootScp, &spacep, userp, &req);
	cm_ReleaseSCache(scp);
	if (code == 0) {
		cp = ioctlp->outDatap;
		if (newRootScp != NULL) {
			strcpy(cp, "/afs/");
			cp += strlen(cp);
		}
		strcpy(cp, spacep->data);
		cp += strlen(cp) + 1;
		ioctlp->outDatap = cp;
		cm_FreeSpace(spacep);
		if (newRootScp != NULL)
			cm_ReleaseSCache(newRootScp);
	}

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

	code = cm_Lookup(dscp, cp, CM_FLAG_NOMOUNTCHASE, userp, &req, &scp);
        
	/* if something went wrong, bail out now */
        if (code) {
		goto done;
	}
        
	lock_ObtainMutex(&scp->mx);
        code = cm_SyncOp(scp, NULL, userp, &req, 0,
        	CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        if (code) {
        	lock_ReleaseMutex(&scp->mx);
        	cm_ReleaseSCache(scp);
		goto done;
	}
	
        /* now check that this is a real symlink */
        if (scp->fileType != CM_SCACHETYPE_SYMLINK) {
		lock_ReleaseMutex(&scp->mx);
        	cm_ReleaseSCache(scp);
                code = CM_ERROR_INVAL;
                goto done;
        }
	
        /* time to make the RPC, so drop the lock */
	lock_ReleaseMutex(&scp->mx);
        cm_ReleaseSCache(scp);
        
	/* easier to do it this way */
        code = cm_Unlink(dscp, cp, userp, &req);
	if (code == 0 && (dscp->flags & CM_SCACHEFLAG_ANYWATCH))
		smb_NotifyChange(FILE_ACTION_REMOVED,
				 FILE_NOTIFY_CHANGE_FILE_NAME
				   | FILE_NOTIFY_CHANGE_DIR_NAME,
				 dscp, cp, NULL, TRUE);

done:
	cm_ReleaseSCache(dscp);
	return code;
}

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

#ifndef DJGPP   /* for win95, session key is back in pioctl */
		/* uuid */
		memcpy(&uuid, tp, sizeof(uuid));
		if (!cm_FindTokenEvent(uuid, sessionKey))
			return CM_ERROR_INVAL;
#endif /* !DJGPP */
	} else
		cellp = cm_rootCellp;

	/* store the token */
	lock_ObtainMutex(&userp->mx);
	ucellp = cm_GetUCell(userp, cellp);
	ucellp->ticketLen = ticketLen;
	if (ucellp->ticketp)
		free(ucellp->ticketp);	/* Discard old token if any */
	ucellp->ticketp = malloc(ticketLen);
	memcpy(ucellp->ticketp, ticket, ticketLen);
#ifndef DJGPP
	/*
	 * Get the session key from the RPC, rather than from the pioctl.
	 */
	/*
	memcpy(&ucellp->sessionKey, ct.HandShakeKey, sizeof(ct.HandShakeKey));
	 */
	memcpy(ucellp->sessionKey.data, sessionKey, sizeof(sessionKey));
#else
        /* for win95, we are getting the session key from the pioctl */
        memcpy(&ucellp->sessionKey, ct.HandShakeKey, sizeof(ct.HandShakeKey));
#endif /* !DJGPP */
	ucellp->kvno = ct.AuthHandle;
	ucellp->expirationTime = ct.EndTimestamp;
	ucellp->gen++;
	if (uname) strcpy(ucellp->userName, uname);
	ucellp->flags |= CM_UCELLFLAG_RXKAD;
	lock_ReleaseMutex(&userp->mx);

	if (flags & PIOCTL_LOGON) {
		ioctlp->flags |= SMB_IOCTLFLAG_LOGON;
	}

	cm_ResetACLCache(userp);

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
#ifndef DJGPP
	/*
	 * Don't give out a real session key here
	 */
	/*
	memcpy(ct.HandShakeKey, &ucellp->sessionKey, sizeof(ct.HandShakeKey));
	 */
	memset(ct.HandShakeKey, 0, sizeof(ct.HandShakeKey));
#else
	memcpy(ct.HandShakeKey, &ucellp->sessionKey, sizeof(ct.HandShakeKey));
#endif /* !DJGPP */
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
	strcpy(cp, ucellp->cellp->namep);
	cp += strlen(cp) + 1;

	/* user name */
	strcpy(cp, ucellp->userName);
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
#ifndef DJGPP
	afs_uuid_t uuid;
#endif /* !DJGPP */

	cm_SkipIoctlPath(ioctlp);

	tp = ioctlp->inDatap;

	cp = ioctlp->outDatap;

	/* cell name is right here */
	cellp = cm_GetCell(tp, 0);
	if (!cellp) return CM_ERROR_NOSUCHCELL;
	tp += strlen(tp) + 1;

#ifndef DJGPP
	/* uuid */
	memcpy(&uuid, tp, sizeof(uuid));
#endif /* !DJGPP */

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
#ifndef DJGPP
	/*
	 * Don't give out a real session key here
	 */
	/*
	memcpy(ct.HandShakeKey, &ucellp->sessionKey, sizeof(ct.HandShakeKey));
	 */
	memset(ct.HandShakeKey, 0, sizeof(ct.HandShakeKey));
#else
        memcpy(ct.HandShakeKey, &ucellp->sessionKey, sizeof(ct.HandShakeKey));
#endif /* !DJGPP */
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
	strcpy(cp, ucellp->cellp->namep);
	cp += strlen(cp) + 1;

	/* user name */
	strcpy(cp, ucellp->userName);
	cp += strlen(cp) + 1;

	ioctlp->outDatap = cp;

	lock_ReleaseMutex(&userp->mx);

#ifndef DJGPP
	cm_RegisterNewTokenEvent(uuid, ucellp->sessionKey.data);
#endif /* !DJGPP */

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

	if (ucellp->ticketp) {
		free(ucellp->ticketp);
		ucellp->ticketp = NULL;
	}
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
	int iteration;
	int submountDataSize;
	char *submountData;
	char *submountName;
	int nextAutoSubmount;

	cm_SkipIoctlPath(ioctlp);

	/* Serialize this one, to prevent simultaneous mods
	 * to afsdsbmt.ini
	 */
	lock_ObtainMutex(&cm_Afsdsbmt_Lock);

	/* Parse the input parameters--first the required afs path,
	 * then the requested submount name (which may be "").
	 */
	cm_NormalizeAfsPath (afspath, ioctlp->inDatap);
	submountreqp = ioctlp->inDatap + (strlen(ioctlp->inDatap)+1);

	/* If the caller supplied a suggested submount name, see if
	 * that submount name is in use... if so, the submount's path
	 * has to match our path.
	 */
	if (submountreqp && *submountreqp) {
		char submountPathNormalized[MAX_PATH];
		char submountPath[MAX_PATH];
		int submountPathLen;

		submountPathLen = GetPrivateProfileString("AFS Submounts",
					submountreqp, "", submountPath,
					sizeof(submountPath), "afsdsbmt.ini");

		if ((submountPathLen == 0) ||
		    (submountPathLen == sizeof(submountPath) - 1)) {

			/* The suggested submount name isn't in use now--
			 * so we can safely map the requested submount name
			 * to the supplied path. Remember not to write the
			 * leading "/afs" when writing out the submount.
			 */
			WritePrivateProfileString("AFS Submounts",
					submountreqp, &afspath[strlen("/afs")],
					"afsdsbmt.ini");

			strcpy(ioctlp->outDatap, submountreqp);
			ioctlp->outDatap += strlen(ioctlp->outDatap) +1;
			lock_ReleaseMutex(&cm_Afsdsbmt_Lock);
        		return 0;
		}

		/* The suggested submount name is already in use--if the
		 * supplied path matches the submount's path, we can still
		 * use the suggested submount name.
		 */
		cm_NormalizeAfsPath (submountPathNormalized, submountPath);
		if (!strcmp (submountPathNormalized, afspath)) {
			strcpy(ioctlp->outDatap, submountreqp);
			ioctlp->outDatap += strlen(ioctlp->outDatap) +1;
			lock_ReleaseMutex(&cm_Afsdsbmt_Lock);
        		return 0;
		}
	}

	/* At this point, the user either didn't request a particular
	 * submount name, or that submount name couldn't be used.
	 * Look through afsdsbmt.ini to see if there are any submounts
	 * already associated with the specified path. The first
	 * step in doing that search is to load the AFS Submounts
	 * section of afsdsbmt.ini into memory.
	 */

	submountDataSize = 1024;
	submountData = malloc (submountDataSize);

	for (iteration = 0; iteration < 5; ++iteration) {

		int sectionSize;
		sectionSize = GetPrivateProfileString("AFS Submounts",
					NULL, "", submountData,
					submountDataSize, "afsdsbmt.ini");
		if (sectionSize < submountDataSize-2)
			break;

		free (submountData);
		submountDataSize *= 2;
		submountData = malloc (submountDataSize);
	}

	/* Having obtained a list of all available submounts, start
	 * searching that list for a path which matches the requested
	 * AFS path. We'll also keep track of the highest "auto15"/"auto47"
	 * submount, in case we need to add a new one later.
	 */

	nextAutoSubmount = 1;

	for (submountName = submountData;
        	submountName && *submountName;
        	submountName += 1+strlen(submountName)) {

		char submountPathNormalized[MAX_PATH];
		char submountPath[MAX_PATH] = "";
		int submountPathLen;

		/* If this is an Auto### submount, remember its ### value */

		if ((!strnicmp (submountName, "auto", 4)) &&
		    (isdigit (submountName[strlen("auto")]))) {
			int thisAutoSubmount;
			thisAutoSubmount = atoi (&submountName[strlen("auto")]);
			nextAutoSubmount = max (nextAutoSubmount,
						thisAutoSubmount+1);
		}

		/* We have the name of a submount in the AFS Submounts
		 * section; read that entry to find out what path it
		 * maps to.
		 */
		submountPathLen = GetPrivateProfileString("AFS Submounts",
					submountName, "", submountPath,
					sizeof(submountPath), "afsdsbmt.ini");

		if ((submountPathLen == 0) ||
		    (submountPathLen == sizeof(submountPath) - 1)) {
			continue;
		}

		/* See if the path for this submount matches the path
		 * that our caller specified. If so, we can return
		 * this submount.
		 */
		cm_NormalizeAfsPath (submountPathNormalized, submountPath);
		if (!strcmp (submountPathNormalized, afspath)) {

			strcpy(ioctlp->outDatap, submountName);
			ioctlp->outDatap += strlen(ioctlp->outDatap) +1;
			free (submountData);
			lock_ReleaseMutex(&cm_Afsdsbmt_Lock);
        		return 0;

		}
	}

	free (submountData);

	/* We've been through the entire list of existing submounts, and
	 * didn't find any which matched the specified path. So, we'll
	 * just have to add one. Remember not to write the leading "/afs"
	 * when writing out the submount.
	 */

	sprintf(ioctlp->outDatap, "auto%ld", nextAutoSubmount);

	WritePrivateProfileString("AFS Submounts", ioctlp->outDatap,
					&afspath[lstrlen("/afs")],
					"afsdsbmt.ini");

	ioctlp->outDatap += strlen(ioctlp->outDatap) +1;
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
	cm_SkipIoctlPath(ioctlp);

	memcpy(&cryptall, ioctlp->inDatap, sizeof(cryptall));

	return 0;
}

#ifdef DJGPP
extern int afsd_shutdown(int);
extern int afs_shutdown;

long cm_IoctlShutdown(smb_ioctl_t *ioctlp, cm_user_t *userp)
{
  afs_shutdown = 1;   /* flag to shut down */
  return 0;
}
#endif /* DJGPP */
