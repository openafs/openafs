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
#endif /* !DJGPP */
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <osi.h>

#include "afsd.h"

#include "smb.h"

extern osi_hyper_t hzero;

smb_packet_t *smb_Directory_Watches = NULL;
osi_mutex_t smb_Dir_Watch_Lock;

smb_tran2Dispatch_t smb_tran2DispatchTable[SMB_TRAN2_NOPCODES];

/* protected by the smb_globalLock */
smb_tran2Packet_t *smb_tran2AssemblyQueuep;

/* retrieve a held reference to a user structure corresponding to an incoming
 * request */
cm_user_t *smb_GetTran2User(smb_vc_t *vcp, smb_tran2Packet_t *inp)
{
	smb_user_t *uidp;
        cm_user_t *up = NULL;
        
        uidp = smb_FindUID(vcp, inp->uid, 0);
        if (!uidp) return NULL;
        
	lock_ObtainMutex(&uidp->mx);
        if (uidp->unp) {
          up = uidp->unp->userp;
          cm_HoldUser(up);
        }
	lock_ReleaseMutex(&uidp->mx);

        smb_ReleaseUID(uidp);
        
        return up;
}

/*
 * Return extended attributes.
 * Right now, we aren't using any of the "new" bits, so this looks exactly
 * like smb_Attributes() (see smb.c).
 */
unsigned long smb_ExtAttributes(cm_scache_t *scp)
{
	unsigned long attrs;

	if (scp->fileType == CM_SCACHETYPE_DIRECTORY
	    || scp->fileType == CM_SCACHETYPE_MOUNTPOINT)
		attrs = SMB_ATTR_DIRECTORY;
	else
		attrs = 0;
	/*
	 * We used to mark a file RO if it was in an RO volume, but that
	 * turns out to be impolitic in NT.  See defect 10007.
	 */
#ifdef notdef
	if ((scp->unixModeBits & 0222) == 0 || (scp->flags & CM_SCACHEFLAG_RO))
#endif
	if ((scp->unixModeBits & 0222) == 0)
		attrs |= SMB_ATTR_READONLY;		/* Read-only */

	if (attrs == 0)
		attrs = SMB_ATTR_NORMAL;		/* FILE_ATTRIBUTE_NORMAL */

	return attrs;
}

int smb_V3IsStarMask(char *maskp)
{
        char tc;

	while (tc = *maskp++)
	  if (tc == '?' || tc == '*') return 1;
	return 0;
}

unsigned char *smb_ParseString(unsigned char *inp, char **chainpp)
{
        if (chainpp) {
		/* skip over null-terminated string */
		*chainpp = inp + strlen(inp) + 1;
        }
        return inp;
}

long smb_ReceiveV3SessionSetupX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    char *tp;
    char *usern, *pwd, *pwdx;
    smb_user_t *uidp;
    unsigned short newUid;
    unsigned long caps;
    cm_user_t *userp;
    smb_username_t *unp;
    char *s1 = " ";

    /* Check for bad conns */
    if (vcp->flags & SMB_VCFLAG_REMOTECONN)
        return CM_ERROR_REMOTECONN;

    /* For NT LM 0.12 and up, get capabilities */
    if (vcp->flags & SMB_VCFLAG_USENT) {
        caps = smb_GetSMBParm(inp, 11);
        if (caps & 0x40)
            vcp->flags |= SMB_VCFLAG_STATUS32;
        /* for now, ignore other capability bits */
    }

    /* Parse the data */
    tp = smb_GetSMBData(inp, NULL);
    if (vcp->flags & SMB_VCFLAG_USENT)
        pwdx = smb_ParseString(tp, &tp);
    pwd = smb_ParseString(tp, &tp);
    usern = smb_ParseString(tp, &tp);

    /* On Windows 2000, this function appears to be called more often than
       it is expected to be called. This resulted in multiple smb_user_t
       records existing all for the same user session which results in all
       of the users tokens disappearing.

       To avoid this problem, we look for an existing smb_user_t record
       based on the users name, and use that one if we find it.
    */

    uidp = smb_FindUserByNameThisSession(vcp, usern);
    if (uidp) {   /* already there, so don't create a new one */
        unp = uidp->unp;
        userp = unp->userp;
        newUid = (unsigned short)uidp->userID;  /* For some reason these are different types!*/
		osi_LogEvent("AFS smb_ReceiveV3SessionSetupX",NULL,"FindUserByName:Lana[%d],lsn[%d],userid[%d],name[%s]",vcp->lana,vcp->lsn,newUid,usern);
		osi_Log3(afsd_logp,"smb_ReceiveV3SessionSetupX FindUserByName:Lana[%d],lsn[%d],userid[%d]",vcp->lana,vcp->lsn,newUid);
        smb_ReleaseUID(uidp);
    }
    else {
      /* do a global search for the username/machine name pair */
        unp = smb_FindUserByName(usern, vcp->rname, SMB_FLAG_CREATE);

        /* Create a new UID and cm_user_t structure */
        userp = unp->userp;
        if (!userp)
          userp = cm_NewUser();
        lock_ObtainMutex(&vcp->mx);
        if (!vcp->uidCounter)
            vcp->uidCounter++; /* handle unlikely wraparounds */
        newUid = (strlen(usern)==0)?0:vcp->uidCounter++;
        lock_ReleaseMutex(&vcp->mx);

        /* Create a new smb_user_t structure and connect them up */
        lock_ObtainMutex(&unp->mx);
        unp->userp = userp;
        lock_ReleaseMutex(&unp->mx);

        uidp = smb_FindUID(vcp, newUid, SMB_FLAG_CREATE);
        lock_ObtainMutex(&uidp->mx);
        uidp->unp = unp;
		osi_LogEvent("AFS smb_ReceiveV3SessionSetupX",NULL,"MakeNewUser:VCP[%x],Lana[%d],lsn[%d],userid[%d],TicketKTCName[%s]",(int)vcp,vcp->lana,vcp->lsn,newUid,usern);
		osi_Log4(afsd_logp,"smb_ReceiveV3SessionSetupX MakeNewUser:VCP[%x],Lana[%d],lsn[%d],userid[%d]",vcp,vcp->lana,vcp->lsn,newUid);
        lock_ReleaseMutex(&uidp->mx);
        smb_ReleaseUID(uidp);
    }

    /* Return UID to the client */
    ((smb_t *)outp)->uid = newUid;
    /* Also to the next chained message */
    ((smb_t *)inp)->uid = newUid;

    osi_Log3(afsd_logp, "SMB3 session setup name %s creating ID %d%s",
             osi_LogSaveString(afsd_logp, usern), newUid, osi_LogSaveString(afsd_logp, s1));
    smb_SetSMBParm(outp, 2, 0);
    smb_SetSMBDataLength(outp, 0);
    return 0;
}

long smb_ReceiveV3UserLogoffX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
	smb_user_t *uidp;

	/* don't get tokens from this VC */
	vcp->flags |= SMB_VCFLAG_ALREADYDEAD;

	inp->flags |= SMB_PACKETFLAG_PROFILE_UPDATE_OK;

	/* find the tree and free it */
    uidp = smb_FindUID(vcp, ((smb_t *)inp)->uid, 0);
    /* TODO: smb_ReleaseUID() ? */
    if (uidp) {
		char *s1 = NULL, *s2 = NULL;

		if (s2 == NULL) s2 = " ";
		if (s1 == NULL) {s1 = s2; s2 = " ";}

		osi_Log4(afsd_logp, "SMB3 user logoffX uid %d name %s%s%s",
			 uidp->userID,
			 osi_LogSaveString(afsd_logp,
                 (uidp->unp) ? uidp->unp->name: " "), s1, s2);

		lock_ObtainMutex(&uidp->mx);
		uidp->flags |= SMB_USERFLAG_DELETE;
		/*
		 * it doesn't get deleted right away
		 * because the vcp points to it
		 */
                lock_ReleaseMutex(&uidp->mx);
        }
	else
		osi_Log0(afsd_logp, "SMB3 user logoffX");

        smb_SetSMBDataLength(outp, 0);
        return 0;
}

long smb_ReceiveV3TreeConnectX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
        smb_tid_t *tidp;
        unsigned short newTid;
        char shareName[256];
	char *sharePath;
	int shareFound;
        char *tp;
        char *pathp;
        char *passwordp;
	char *servicep;
        cm_user_t *userp;
        
	osi_Log0(afsd_logp, "SMB3 receive tree connect");

	/* parse input parameters */
	tp = smb_GetSMBData(inp, NULL);
        passwordp = smb_ParseString(tp, &tp);
	pathp = smb_ParseString(tp, &tp);
	servicep = smb_ParseString(tp, &tp);

	tp = strrchr(pathp, '\\');
        if (!tp) {
                return CM_ERROR_BADSMB;
        }
        strcpy(shareName, tp+1);

	if (strcmp(servicep, "IPC") == 0 || strcmp(shareName, "IPC$") == 0)
		return CM_ERROR_NOIPC;

        userp = smb_GetUser(vcp, inp);

	lock_ObtainMutex(&vcp->mx);
        newTid = vcp->tidCounter++;
	lock_ReleaseMutex(&vcp->mx);
        
	tidp = smb_FindTID(vcp, newTid, SMB_FLAG_CREATE);
	shareFound = smb_FindShare(vcp, inp, shareName, &sharePath);
	if (!shareFound) {
		smb_ReleaseTID(tidp);
		return CM_ERROR_BADSHARENAME;
	}
        lock_ObtainMutex(&tidp->mx);
        tidp->userp = userp;
	tidp->pathname = sharePath;
        lock_ReleaseMutex(&tidp->mx);
        smb_ReleaseTID(tidp);

	if (vcp->flags & SMB_VCFLAG_USENT)
		smb_SetSMBParm(outp, 2, 0);	/* OptionalSupport bits */

	((smb_t *)outp)->tid = newTid;
	((smb_t *)inp)->tid = newTid;
	tp = smb_GetSMBData(outp, NULL);
        *tp++ = 'A';
        *tp++ = ':';
        *tp++ = 0;
        smb_SetSMBDataLength(outp, 3);
        
        osi_Log1(afsd_logp, "SMB3 tree connect created ID %d", newTid);
        return 0;
}

/* must be called with global tran lock held */
smb_tran2Packet_t *smb_FindTran2Packet(smb_vc_t *vcp, smb_packet_t *inp)
{
	smb_tran2Packet_t *tp;
        smb_t *smbp;
        
        smbp = (smb_t *) inp->data;
	for(tp = smb_tran2AssemblyQueuep; tp; tp = (smb_tran2Packet_t *) osi_QNext(&tp->q)) {
		if (tp->vcp == vcp && tp->mid == smbp->mid && tp->tid == smbp->tid)
                	return tp;
        }
        return NULL;
}

smb_tran2Packet_t *smb_NewTran2Packet(smb_vc_t *vcp, smb_packet_t *inp,
	int totalParms, int totalData)
{
	smb_tran2Packet_t *tp;
    smb_t *smbp;
        
    smbp = (smb_t *) inp->data;
	tp = malloc(sizeof(*tp));
    memset(tp, 0, sizeof(*tp));
    tp->vcp = vcp;
    smb_HoldVC(vcp);
    tp->curData = tp->curParms = 0;
    tp->totalData = totalData;
    tp->totalParms = totalParms;
    tp->tid = smbp->tid;
    tp->mid = smbp->mid;
    tp->uid = smbp->uid;
    tp->pid = smbp->pid;
	tp->res[0] = smbp->res[0];
	osi_QAdd((osi_queue_t **)&smb_tran2AssemblyQueuep, &tp->q);
    tp->opcode = smb_GetSMBParm(inp, 14);
	if (totalParms != 0)
        tp->parmsp = malloc(totalParms);
	if (totalData != 0)
        tp->datap = malloc(totalData);
	tp->flags |= SMB_TRAN2PFLAG_ALLOC;
    return tp;
}

smb_tran2Packet_t *smb_GetTran2ResponsePacket(smb_vc_t *vcp,
	smb_tran2Packet_t *inp, smb_packet_t *outp,
	int totalParms, int totalData)
{
	smb_tran2Packet_t *tp;
	unsigned short parmOffset;
	unsigned short dataOffset;
	unsigned short dataAlign;
        
	tp = malloc(sizeof(*tp));
        memset(tp, 0, sizeof(*tp));
        tp->vcp = NULL;
        tp->curData = tp->curParms = 0;
        tp->totalData = totalData;
        tp->totalParms = totalParms;
	tp->oldTotalParms = totalParms;
        tp->tid = inp->tid;
        tp->mid = inp->mid;
        tp->uid = inp->uid;
        tp->pid = inp->pid;
	tp->res[0] = inp->res[0];
        tp->opcode = inp->opcode;

	/*
	 * We calculate where the parameters and data will start.
	 * This calculation must parallel the calculation in
	 * smb_SendTran2Packet.
	 */

	parmOffset = 10*2 + 35;
	parmOffset++;			/* round to even */
	tp->parmsp = (unsigned short *) (outp->data + parmOffset);

	dataOffset = parmOffset + totalParms;
	dataAlign = dataOffset & 2;	/* quad-align */
	dataOffset += dataAlign;
	tp->datap = outp->data + dataOffset;

        return tp;
}

/* free a tran2 packet; must be called with smb_globalLock held */
void smb_FreeTran2Packet(smb_tran2Packet_t *t2p)
{
    if (t2p->vcp) smb_ReleaseVC(t2p->vcp);
	if (t2p->flags & SMB_TRAN2PFLAG_ALLOC) {
		if (t2p->parmsp)
			free(t2p->parmsp);
		if (t2p->datap)
			free(t2p->datap);
	}
    free(t2p);
}

/* called with a VC, an input packet to respond to, and an error code.
 * sends an error response.
 */
void smb_SendTran2Error(smb_vc_t *vcp, smb_tran2Packet_t *t2p,
	smb_packet_t *tp, long code)
{
        smb_t *smbp;
        unsigned short errCode;
        unsigned char errClass;
	unsigned long NTStatus;

        if (vcp->flags & SMB_VCFLAG_STATUS32)
		smb_MapNTError(code, &NTStatus);
	else
		smb_MapCoreError(code, vcp, &errCode, &errClass);

        smb_FormatResponsePacket(vcp, NULL, tp);
        smbp = (smb_t *) tp;
        
	/* We can handle long names */
	if (vcp->flags & SMB_VCFLAG_USENT)
		smbp->flg2 |= 0x40;	/* IS_LONG_NAME */
        
        /* now copy important fields from the tran 2 packet */
        smbp->com = 0x32;		/* tran 2 response */
        smbp->tid = t2p->tid;
        smbp->mid = t2p->mid;
        smbp->pid = t2p->pid;
        smbp->uid = t2p->uid;
	smbp->res[0] = t2p->res[0];
	if (vcp->flags & SMB_VCFLAG_STATUS32) {
		smbp->rcls = (unsigned char) (NTStatus & 0xff);
		smbp->reh = (unsigned char) ((NTStatus >> 8) & 0xff);
		smbp->errLow = (unsigned char) ((NTStatus >> 16) & 0xff);
		smbp->errHigh = (unsigned char) ((NTStatus >> 24) & 0xff);
		smbp->flg2 |= 0x4000;
	}
	else {
	        smbp->rcls = errClass;
		smbp->errLow = (unsigned char) (errCode & 0xff);
		smbp->errHigh = (unsigned char) ((errCode >> 8) & 0xff);
	}
        
        /* send packet */
        smb_SendPacket(vcp, tp);
}        

void smb_SendTran2Packet(smb_vc_t *vcp, smb_tran2Packet_t *t2p, smb_packet_t *tp)
{
        smb_t *smbp;
        unsigned short parmOffset;
	unsigned short dataOffset;
	unsigned short totalLength;
	unsigned short dataAlign;
        char *datap;
        
        smb_FormatResponsePacket(vcp, NULL, tp);
        smbp = (smb_t *) tp;

	/* We can handle long names */
	if (vcp->flags & SMB_VCFLAG_USENT)
		smbp->flg2 |= 0x40;	/* IS_LONG_NAME */
        
        /* now copy important fields from the tran 2 packet */
        smbp->com = 0x32;		/* tran 2 response */
        smbp->tid = t2p->tid;
        smbp->mid = t2p->mid;
        smbp->pid = t2p->pid;
        smbp->uid = t2p->uid;
	smbp->res[0] = t2p->res[0];

        totalLength = 1 + t2p->totalData + t2p->totalParms;

        /* now add the core parameters (tran2 info) to the packet */
        smb_SetSMBParm(tp, 0, t2p->totalParms);	/* parm bytes */
        smb_SetSMBParm(tp, 1, t2p->totalData);	/* data bytes */
        smb_SetSMBParm(tp, 2, 0);		/* reserved */
        smb_SetSMBParm(tp, 3, t2p->totalParms);	/* parm bytes in this packet */
	parmOffset = 10*2 + 35;			/* parm offset in packet */
	parmOffset++;				/* round to even */
        smb_SetSMBParm(tp, 4, parmOffset);	/* 11 parm words plus *
						 * hdr, bcc and wct */
        smb_SetSMBParm(tp, 5, 0);		/* parm displacement */
        smb_SetSMBParm(tp, 6, t2p->totalData);	/* data in this packet */
	dataOffset = parmOffset + t2p->oldTotalParms;
	dataAlign = dataOffset & 2;		/* quad-align */
	dataOffset += dataAlign;
        smb_SetSMBParm(tp, 7, dataOffset);	/* offset of data */
        smb_SetSMBParm(tp, 8, 0);		/* data displacement */
        smb_SetSMBParm(tp, 9, 0);		/* low: setup word count *
						 * high: resvd */
        
        datap = smb_GetSMBData(tp, NULL);
	*datap++ = 0;				/* we rounded to even */

	totalLength += dataAlign;
        smb_SetSMBDataLength(tp, totalLength);
        
        /* next, send the datagram */
        smb_SendPacket(vcp, tp);
}

long smb_ReceiveV3Tran2A(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
        smb_tran2Packet_t *asp;
        int totalParms;
        int totalData;
        int parmDisp;
        int dataDisp;
        int parmOffset;
        int dataOffset;
        int parmCount;
        int dataCount;
        int firstPacket;
        long code;

	/* We sometimes see 0 word count.  What to do? */
	if (*inp->wctp == 0) {
#ifndef DJGPP
		HANDLE h;
		char *ptbuf[1];

		osi_Log0(afsd_logp, "TRANSACTION2 word count = 0"); 

		h = RegisterEventSource(NULL, AFS_DAEMON_EVENT_NAME);
		ptbuf[0] = "Transaction2 word count = 0";
		ReportEvent(h, EVENTLOG_WARNING_TYPE, 0, 1003, NULL,
			    1, inp->ncb_length, ptbuf, inp);
		DeregisterEventSource(h);
#else /* DJGPP */
		osi_Log0(afsd_logp, "TRANSACTION2 word count = 0"); 
#endif /* !DJGPP */

                smb_SetSMBDataLength(outp, 0);
                smb_SendPacket(vcp, outp);
		return 0;
	}

        totalParms = smb_GetSMBParm(inp, 0);
        totalData = smb_GetSMBParm(inp, 1);
        
        firstPacket = (inp->inCom == 0x32);
        
	/* find the packet we're reassembling */
	lock_ObtainWrite(&smb_globalLock);
        asp = smb_FindTran2Packet(vcp, inp);
        if (!asp) {
        	asp = smb_NewTran2Packet(vcp, inp, totalParms, totalData);
	}
        lock_ReleaseWrite(&smb_globalLock);
        
        /* now merge in this latest packet; start by looking up offsets */
	if (firstPacket) {
		parmDisp = dataDisp = 0;
                parmOffset = smb_GetSMBParm(inp, 10);
                dataOffset = smb_GetSMBParm(inp, 12);
                parmCount = smb_GetSMBParm(inp, 9);
                dataCount = smb_GetSMBParm(inp, 11);
		asp->maxReturnParms = smb_GetSMBParm(inp, 2);
                asp->maxReturnData = smb_GetSMBParm(inp, 3);

		osi_Log3(afsd_logp, "SMB3 received T2 init packet total data %d, cur data %d, max return data %d",
                	totalData, dataCount, asp->maxReturnData);
        }
        else {
	        parmDisp = smb_GetSMBParm(inp, 4);
	        parmOffset = smb_GetSMBParm(inp, 3);
	        dataDisp = smb_GetSMBParm(inp, 7);
	        dataOffset = smb_GetSMBParm(inp, 6);
	        parmCount = smb_GetSMBParm(inp, 2);
	        dataCount = smb_GetSMBParm(inp, 5);
                
                osi_Log2(afsd_logp, "SMB3 received T2 aux packet parms %d, data %d",
                	parmCount, dataCount);
	}

    /* now copy the parms and data */
    if ( parmCount != 0 )
    {
        memcpy(((char *)asp->parmsp) + parmDisp, inp->data + parmOffset, parmCount);
    }
    if ( dataCount != 0 ) {
        memcpy(asp->datap + dataDisp, inp->data + dataOffset, dataCount);
    }

        /* account for new bytes */
        asp->curData += dataCount;
        asp->curParms += parmCount;
        
        /* finally, if we're done, remove the packet from the queue and dispatch it */
        if (asp->totalData <= asp->curData && asp->totalParms <= asp->curParms) {
		/* we've received it all */
                lock_ObtainWrite(&smb_globalLock);
		osi_QRemove((osi_queue_t **) &smb_tran2AssemblyQueuep, &asp->q);
                lock_ReleaseWrite(&smb_globalLock);
                
            /* now dispatch it */
            if ( asp->opcode >= 0 && asp->opcode < 20 && smb_tran2DispatchTable[asp->opcode].procp) {
                osi_LogEvent("AFS-Dispatch-2[%s]",myCrt_2Dispatch(asp->opcode),"vcp[%x] lana[%d] lsn[%d]",(int)vcp,vcp->lana,vcp->lsn);
                osi_Log4(afsd_logp,"AFS Server - Dispatch-2 %s vcp[%x] lana[%d] lsn[%d]",myCrt_2Dispatch(asp->opcode),vcp,vcp->lana,vcp->lsn);
                code = (*smb_tran2DispatchTable[asp->opcode].procp)(vcp, asp, outp);
            }
            else {
                osi_LogEvent("AFS-Dispatch-2 [invalid]", NULL, "op[%x] vcp[%x] lana[%d] lsn[%d]", asp->opcode, vcp, vcp->lana, vcp->lsn);
                osi_Log4(afsd_logp,"AFS Server - Dispatch-2 [INVALID] op[%x] vcp[%x] lana[%d] lsn[%d]", asp->opcode, vcp, vcp->lana, vcp->lsn);
                code = CM_ERROR_BADOP;
            }

		/* if an error is returned, we're supposed to send an error packet,
                 * otherwise the dispatched function already did the data sending.
		 * We give dispatched proc the responsibility since it knows how much
                 * space to allocate.
                 */
                if (code != 0) {
                        smb_SendTran2Error(vcp, asp, outp, code);
                }

		/* free the input tran 2 packet */
		lock_ObtainWrite(&smb_globalLock);
                smb_FreeTran2Packet(asp);
		lock_ReleaseWrite(&smb_globalLock);
        }
        else if (firstPacket) {
		/* the first packet in a multi-packet request, we need to send an
                 * ack to get more data.
                 */
                smb_SetSMBDataLength(outp, 0);
                smb_SendPacket(vcp, outp);
        }

	return 0;
}

long smb_ReceiveTran2Open(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *op)
{
	char *pathp;
    smb_tran2Packet_t *outp;
    long code;
	cm_space_t *spacep;
    int excl;
    cm_user_t *userp;
    cm_scache_t *dscp;		/* dir we're dealing with */
    cm_scache_t *scp;		/* file we're creating */
    cm_attr_t setAttr;
    int initialModeBits;
    smb_fid_t *fidp;
    int attributes;
    char *lastNamep;
    long dosTime;
    int openFun;
    int trunc;
    int openMode;
    int extraInfo;
    int openAction;
    int parmSlot;			/* which parm we're dealing with */
    long returnEALength;
	char *tidPathp;
	cm_req_t req;

	cm_InitReq(&req);

    scp = NULL;
        
	extraInfo = (p->parmsp[0] & 1);	/* return extra info */
    returnEALength = (p->parmsp[0] & 8);	/* return extended attr length */

	openFun = p->parmsp[6];		/* open function */
    excl = ((openFun & 3) == 0);
    trunc = ((openFun & 3) == 2);	/* truncate it */
	openMode = (p->parmsp[1] & 0x7);
    openAction = 0;			/* tracks what we did */

    attributes = p->parmsp[3];
    dosTime = p->parmsp[4] | (p->parmsp[5] << 16);
        
	/* compute initial mode bits based on read-only flag in attributes */
    initialModeBits = 0666;
    if (attributes & 1) initialModeBits &= ~0222;
        
    pathp = (char *) (&p->parmsp[14]);
        
    outp = smb_GetTran2ResponsePacket(vcp, p, op, 40, 0);

	spacep = cm_GetSpace();
    smb_StripLastComponent(spacep->data, &lastNamep, pathp);

	if (lastNamep && strcmp(lastNamep, SMB_IOCTL_FILENAME) == 0) {
		/* special case magic file name for receiving IOCTL requests
         * (since IOCTL calls themselves aren't getting through).
         */
        fidp = smb_FindFID(vcp, 0, SMB_FLAG_CREATE);
        smb_SetupIoctlFid(fidp, spacep);

        /* copy out remainder of the parms */
		parmSlot = 0;
		outp->parmsp[parmSlot] = fidp->fid; parmSlot++;
		if (extraInfo) {
            outp->parmsp[parmSlot] = /* attrs */ 0; parmSlot++;
            outp->parmsp[parmSlot] = 0; parmSlot++;	/* mod time */
            outp->parmsp[parmSlot] = 0; parmSlot++;
            outp->parmsp[parmSlot] = 0; parmSlot++;	/* len */
            outp->parmsp[parmSlot] = 0x7fff; parmSlot++;
            outp->parmsp[parmSlot] = openMode; parmSlot++;
            outp->parmsp[parmSlot] = 0; parmSlot++; /* file type 0 ==> normal file or dir */
            outp->parmsp[parmSlot] = 0; parmSlot++; /* IPC junk */
		}   
		/* and the final "always present" stuff */
        outp->parmsp[parmSlot] = /* openAction found existing file */ 1; parmSlot++;
		/* next write out the "unique" ID */
		outp->parmsp[parmSlot] = 0x1234; parmSlot++;
		outp->parmsp[parmSlot] = 0x5678; parmSlot++;
        outp->parmsp[parmSlot] = 0; parmSlot++;
		if (returnEALength) {
			outp->parmsp[parmSlot] = 0; parmSlot++;
			outp->parmsp[parmSlot] = 0; parmSlot++;
        }
                
        outp->totalData = 0;
        outp->totalParms = parmSlot * 2;
                
        smb_SendTran2Packet(vcp, outp, op);
                
        smb_FreeTran2Packet(outp);

		/* and clean up fid reference */
        smb_ReleaseFID(fidp);
        return 0;
    }

#ifdef DEBUG_VERBOSE
	{
		char *hexp, *asciip;
		asciip = (lastNamep ? lastNamep : pathp);
		hexp = osi_HexifyString( asciip );
		DEBUG_EVENT2("AFS","T2Open H[%s] A[%s]", hexp, asciip);
		free(hexp);
	}
#endif

	userp = smb_GetTran2User(vcp, p);
    /* In the off chance that userp is NULL, we log and abandon */
    if(!userp) {
        osi_Log1(afsd_logp, "ReceiveTran2Open user [%d] not resolvable", p->uid);
        smb_FreeTran2Packet(outp);
        return CM_ERROR_BADSMB;
    }

	tidPathp = smb_GetTIDPath(vcp, p->tid);

	dscp = NULL;
	code = cm_NameI(cm_rootSCachep, pathp,
                    CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                    userp, tidPathp, &req, &scp);
	if (code != 0) {
		code = cm_NameI(cm_rootSCachep, spacep->data,
                        CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                        userp, tidPathp, &req, &dscp);
		cm_FreeSpace(spacep);

        if (code) {
            cm_ReleaseUser(userp);
			smb_FreeTran2Packet(outp);
            return code;
        }
        
        /* otherwise, scp points to the parent directory.  Do a lookup,
		 * and truncate the file if we find it, otherwise we create the
		 * file.
         */
        if (!lastNamep) lastNamep = pathp;
        else lastNamep++;
        code = cm_Lookup(dscp, lastNamep, CM_FLAG_CASEFOLD, userp,
                         &req, &scp);
        if (code && code != CM_ERROR_NOSUCHFILE) {
			cm_ReleaseSCache(dscp);
            cm_ReleaseUser(userp);
			smb_FreeTran2Packet(outp);
            return code;
        }
	}
    else {
        cm_FreeSpace(spacep);
	}
        
    /* if we get here, if code is 0, the file exists and is represented by
     * scp.  Otherwise, we have to create it.
     */
	if (code == 0) {
        code = cm_CheckOpen(scp, openMode, trunc, userp, &req);
        if (code) {
            if (dscp) cm_ReleaseSCache(dscp);
            cm_ReleaseSCache(scp);
            cm_ReleaseUser(userp);
			smb_FreeTran2Packet(outp);
            return code;
        }

		if (excl) {
			/* oops, file shouldn't be there */
            if (dscp) cm_ReleaseSCache(dscp);
            cm_ReleaseSCache(scp);
            cm_ReleaseUser(userp);
			smb_FreeTran2Packet(outp);
            return CM_ERROR_EXISTS;
        }

		if (trunc) {
			setAttr.mask = CM_ATTRMASK_LENGTH;
            setAttr.length.LowPart = 0;
            setAttr.length.HighPart = 0;
			code = cm_SetAttr(scp, &setAttr, userp, &req);
            openAction = 3;	/* truncated existing file */
		}   
        else openAction = 1;	/* found existing file */
    }
	else if (!(openFun & SMB_ATTR_DIRECTORY)) {
		/* don't create if not found */
        if (dscp) cm_ReleaseSCache(dscp);
        osi_assert(scp == NULL);
        cm_ReleaseUser(userp);
		smb_FreeTran2Packet(outp);
        return CM_ERROR_NOSUCHFILE;
    }
    else {
		osi_assert(dscp != NULL && scp == NULL);
		openAction = 2;	/* created file */
		setAttr.mask = CM_ATTRMASK_CLIENTMODTIME;
		smb_UnixTimeFromSearchTime(&setAttr.clientModTime, dosTime);
        code = cm_Create(dscp, lastNamep, 0, &setAttr, &scp, userp,
                         &req);
		if (code == 0 && (dscp->flags & CM_SCACHEFLAG_ANYWATCH))
			smb_NotifyChange(FILE_ACTION_ADDED,
                             FILE_NOTIFY_CHANGE_FILE_NAME,  
                             dscp, lastNamep, NULL, TRUE);
        if (!excl && code == CM_ERROR_EXISTS) {
			/* not an exclusive create, and someone else tried
			 * creating it already, then we open it anyway.  We
			 * don't bother retrying after this, since if this next
			 * fails, that means that the file was deleted after we
			 * started this call.
             */
            code = cm_Lookup(dscp, lastNamep, CM_FLAG_CASEFOLD,
                             userp, &req, &scp);
            if (code == 0) {
                if (trunc) {
					setAttr.mask = CM_ATTRMASK_LENGTH;
                    setAttr.length.LowPart = 0;
                    setAttr.length.HighPart = 0;
                    code = cm_SetAttr(scp, &setAttr, userp,
                                      &req);
                }   
			}	/* lookup succeeded */
        }
    }
        
	/* we don't need this any longer */
	if (dscp) cm_ReleaseSCache(dscp);

    if (code) {
		/* something went wrong creating or truncating the file */
        if (scp) cm_ReleaseSCache(scp);
        cm_ReleaseUser(userp);
		smb_FreeTran2Packet(outp);
        return code;
    }
        
	/* make sure we're about to open a file */
	if (scp->fileType != CM_SCACHETYPE_FILE) {
		cm_ReleaseSCache(scp);
		cm_ReleaseUser(userp);
		smb_FreeTran2Packet(outp);
		return CM_ERROR_ISDIR;
	}

    /* now all we have to do is open the file itself */
    fidp = smb_FindFID(vcp, 0, SMB_FLAG_CREATE);
    osi_assert(fidp);
	
	/* save a pointer to the vnode */
    fidp->scp = scp;
        
	/* compute open mode */
    if (openMode != 1) fidp->flags |= SMB_FID_OPENREAD;
    if (openMode == 1 || openMode == 2)
        fidp->flags |= SMB_FID_OPENWRITE;

	smb_ReleaseFID(fidp);
        
	cm_Open(scp, 0, userp);

    /* copy out remainder of the parms */
	parmSlot = 0;
	outp->parmsp[parmSlot] = fidp->fid; parmSlot++;
	lock_ObtainMutex(&scp->mx);
	if (extraInfo) {
        outp->parmsp[parmSlot] = smb_Attributes(scp); parmSlot++;
		smb_SearchTimeFromUnixTime(&dosTime, scp->clientModTime);
        outp->parmsp[parmSlot] = (unsigned short)(dosTime & 0xffff); parmSlot++;
        outp->parmsp[parmSlot] = (unsigned short)((dosTime>>16) & 0xffff); parmSlot++;
        outp->parmsp[parmSlot] = (unsigned short) (scp->length.LowPart & 0xffff);
        parmSlot++;
        outp->parmsp[parmSlot] = (unsigned short) ((scp->length.LowPart >> 16) & 0xffff);
        parmSlot++;
        outp->parmsp[parmSlot] = openMode; parmSlot++;
        outp->parmsp[parmSlot] = 0; parmSlot++; /* file type 0 ==> normal file or dir */
        outp->parmsp[parmSlot] = 0; parmSlot++; /* IPC junk */
	}   
	/* and the final "always present" stuff */
    outp->parmsp[parmSlot] = openAction; parmSlot++;
	/* next write out the "unique" ID */
	outp->parmsp[parmSlot] = (unsigned short) (scp->fid.vnode & 0xffff); parmSlot++;
	outp->parmsp[parmSlot] = (unsigned short) (scp->fid.volume & 0xffff); parmSlot++;
    outp->parmsp[parmSlot] = 0; parmSlot++;
    if (returnEALength) {
		outp->parmsp[parmSlot] = 0; parmSlot++;
		outp->parmsp[parmSlot] = 0; parmSlot++;
    }
	lock_ReleaseMutex(&scp->mx);
	outp->totalData = 0;		/* total # of data bytes */
    outp->totalParms = parmSlot * 2;	/* shorts are two bytes */

	smb_SendTran2Packet(vcp, outp, op);
        
    smb_FreeTran2Packet(outp);

    cm_ReleaseUser(userp);
    /* leave scp held since we put it in fidp->scp */
    return 0;
}   

long smb_ReceiveTran2FindFirst(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *outp)
{
        return CM_ERROR_BADOP;
}

long smb_ReceiveTran2FindNext(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *outp)
{
        return CM_ERROR_BADOP;
}

long smb_ReceiveTran2QFSInfo(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *op)
{
	smb_tran2Packet_t *outp;
        smb_tran2QFSInfo_t qi;
	int responseSize;
	osi_hyper_t temp;
	static char FSname[6] = {'A', 0, 'F', 0, 'S', 0};
        
	osi_Log1(afsd_logp, "T2 QFSInfo type 0x%x", p->parmsp[0]);

	switch (p->parmsp[0]) {
	case 1: responseSize = sizeof(qi.u.allocInfo); break;
	case 2: responseSize = sizeof(qi.u.volumeInfo); break;
	case 0x102: responseSize = sizeof(qi.u.FSvolumeInfo); break;
	case 0x103: responseSize = sizeof(qi.u.FSsizeInfo); break;
	case 0x104: responseSize = sizeof(qi.u.FSdeviceInfo); break;
	case 0x105: responseSize = sizeof(qi.u.FSattributeInfo); break;
	default: return CM_ERROR_INVAL;
	}

        outp = smb_GetTran2ResponsePacket(vcp, p, op, 0, responseSize);
	switch (p->parmsp[0]) {
	case 1:
		/* alloc info */
                qi.u.allocInfo.FSID = 0;
                qi.u.allocInfo.sectorsPerAllocUnit = 1;
                qi.u.allocInfo.totalAllocUnits = 0x7fffffff;
                qi.u.allocInfo.availAllocUnits = 0x3fffffff;
                qi.u.allocInfo.bytesPerSector = 1024;
		break;

        case 2:
		/* volume info */
        qi.u.volumeInfo.vsn = 1234;
        qi.u.volumeInfo.vnCount = 4;
		/* we're supposed to pad it out with zeroes to the end */
		memset(&qi.u.volumeInfo.label, 0, sizeof(qi.u.volumeInfo.label));
        memcpy(qi.u.volumeInfo.label, "AFS", 4);
		break;

	case 0x102:
		/* FS volume info */
		memset((char *)&qi.u.FSvolumeInfo.vct, 0, sizeof(FILETIME));
		qi.u.FSvolumeInfo.vsn = 1234;
		qi.u.FSvolumeInfo.vnCount = 8;
		memcpy(qi.u.FSvolumeInfo.label, "A\0F\0S\0\0", 8);
		break;

	case 0x103:
		/* FS size info */
		temp.HighPart = 0;
		temp.LowPart = 0x7fffffff;
		qi.u.FSsizeInfo.totalAllocUnits = temp;
		temp.LowPart = 0x3fffffff;
		qi.u.FSsizeInfo.availAllocUnits = temp;
		qi.u.FSsizeInfo.sectorsPerAllocUnit = 1;
		qi.u.FSsizeInfo.bytesPerSector = 1024;
		break;

	case 0x104:
		/* FS device info */
		qi.u.FSdeviceInfo.devType = 0;	/* don't have a number */
		qi.u.FSdeviceInfo.characteristics = 0x50; /* remote, virtual */
		break;

	case 0x105:
		/* FS attribute info */
		/* attributes, defined in WINNT.H:
		 *	FILE_CASE_SENSITIVE_SEARCH	0x1
		 *	FILE_CASE_PRESERVED_NAMES	0x2
		 *	<no name defined>		0x4000
		 *	   If bit 0x4000 is not set, Windows 95 thinks
		 *	   we can't handle long (non-8.3) names,
		 *	   despite our protestations to the contrary.
		 */
		qi.u.FSattributeInfo.attributes = 0x4003;
		qi.u.FSattributeInfo.maxCompLength = 255;
		qi.u.FSattributeInfo.FSnameLength = 6;
		memcpy(qi.u.FSattributeInfo.FSname, FSname, 6);
		break;
        }
        
	/* copy out return data, and set corresponding sizes */
	outp->totalParms = 0;
        outp->totalData = responseSize;
        memcpy(outp->datap, &qi, responseSize);

	/* send and free the packets */
	smb_SendTran2Packet(vcp, outp, op);
        smb_FreeTran2Packet(outp);

        return 0;
}

long smb_ReceiveTran2SetFSInfo(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *outp)
{
        return CM_ERROR_BADOP;
}

struct smb_ShortNameRock {
	char *maskp;
	unsigned int vnode;
	char *shortName;
	size_t shortNameLen;
};

int cm_GetShortNameProc(cm_scache_t *scp, cm_dirEntry_t *dep, void *vrockp,
	osi_hyper_t *offp)
{
	struct smb_ShortNameRock *rockp;
	char *shortNameEnd;

	rockp = vrockp;
	/* compare both names and vnodes, though probably just comparing vnodes
	 * would be safe enough.
	 */
	if (stricmp(dep->name, rockp->maskp) != 0)
		return 0;
	if (ntohl(dep->fid.vnode) != rockp->vnode)
		return 0;
	/* This is the entry */
	cm_Gen8Dot3Name(dep, rockp->shortName, &shortNameEnd);
	rockp->shortNameLen = shortNameEnd - rockp->shortName;
	return CM_ERROR_STOPNOW;
}

long cm_GetShortName(char *pathp, cm_user_t *userp, cm_req_t *reqp,
	char *tidPathp, int vnode, char *shortName, size_t *shortNameLenp)
{
	struct smb_ShortNameRock rock;
	char *lastNamep;
	cm_space_t *spacep;
	cm_scache_t *dscp;
	int caseFold = CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD;
	long code;
	osi_hyper_t thyper;

	spacep = cm_GetSpace();
	smb_StripLastComponent(spacep->data, &lastNamep, pathp);

	code = cm_NameI(cm_rootSCachep, spacep->data, caseFold, userp, tidPathp,
			reqp, &dscp);
	cm_FreeSpace(spacep);
	if (code) return code;

	if (!lastNamep) lastNamep = pathp;
	else lastNamep++;
	thyper.LowPart = 0;
	thyper.HighPart = 0;
	rock.shortName = shortName;
	rock.vnode = vnode;
	rock.maskp = lastNamep;
	code = cm_ApplyDir(dscp, cm_GetShortNameProc, &rock, &thyper, userp,
			   reqp, NULL);

	cm_ReleaseSCache(dscp);

	if (code == 0)
		return CM_ERROR_NOSUCHFILE;
	if (code == CM_ERROR_STOPNOW) {
		*shortNameLenp = rock.shortNameLen;
		return 0;
	}
	return code;
}

long smb_ReceiveTran2QPathInfo(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *opx)
{
	smb_tran2Packet_t *outp;
        unsigned long dosTime;
	FILETIME ft;
        unsigned short infoLevel;
        int nbytesRequired;
        unsigned short attributes;
	unsigned long extAttributes;
	char shortName[13];
	unsigned int len;
        cm_user_t *userp;
	cm_space_t *spacep;
        cm_scache_t *scp, *dscp;
        long code;
        char *op;
	char *tidPathp;
	char *lastComp;
	cm_req_t req;

	cm_InitReq(&req);

	infoLevel = p->parmsp[0];
        if (infoLevel == 6) nbytesRequired = 0;
        else if (infoLevel == 1) nbytesRequired = 22;
        else if (infoLevel == 2) nbytesRequired = 26;
	else if (infoLevel == 0x101) nbytesRequired = 40;
	else if (infoLevel == 0x102) nbytesRequired = 24;
	else if (infoLevel == 0x103) nbytesRequired = 4;
	else if (infoLevel == 0x108) nbytesRequired = 30;
        else {
		osi_Log2(afsd_logp, "Bad Tran2 op 0x%x infolevel 0x%x",
			 p->opcode, infoLevel);
		smb_SendTran2Error(vcp, p, opx, CM_ERROR_INVAL);
                return 0;
        }
	osi_Log2(afsd_logp, "T2 QPathInfo type 0x%x path %s", infoLevel,
		 osi_LogSaveString(afsd_logp, (char *)(&p->parmsp[3])));

        outp = smb_GetTran2ResponsePacket(vcp, p, opx, 2, nbytesRequired);

	if (infoLevel > 0x100)
		outp->totalParms = 2;
	else
		outp->totalParms = 0;
        outp->totalData = nbytesRequired;
        
        /* now, if we're at infoLevel 6, we're only being asked to check
         * the syntax, so we just OK things now.  In particular, we're *not*
         * being asked to verify anything about the state of any parent dirs.
         */
	if (infoLevel == 6) {
		smb_SendTran2Packet(vcp, outp, opx);
                smb_FreeTran2Packet(outp);
		return 0;
        }
        
        userp = smb_GetTran2User(vcp, p);
        if(!userp) {
        	osi_Log1(afsd_logp, "ReceiveTran2QPathInfo unable to resolve user [%d]", p->uid);
        	smb_FreeTran2Packet(outp);
        	return CM_ERROR_BADSMB;
        }

	tidPathp = smb_GetTIDPath(vcp, p->tid);

	/*
	 * XXX Strange hack XXX
	 *
	 * As of Patch 7 (13 January 98), we are having the following problem:
	 * In NT Explorer 4.0, whenever we click on a directory, AFS gets
	 * requests to look up "desktop.ini" in all the subdirectories.
	 * This can cause zillions of timeouts looking up non-existent cells
	 * and volumes, especially in the top-level directory.
	 *
	 * We have not found any way to avoid this or work around it except
	 * to explicitly ignore the requests for mount points that haven't
	 * yet been evaluated and for directories that haven't yet been
	 * fetched.
	 */
	if (infoLevel == 0x101) {
		spacep = cm_GetSpace();
		smb_StripLastComponent(spacep->data, &lastComp,
					(char *)(&p->parmsp[3]));
		/* Make sure that lastComp is not NULL */
		if (lastComp) {
		    if (strcmp(lastComp, "\\desktop.ini") == 0) {
                code = cm_NameI(cm_rootSCachep, spacep->data,
                                CM_FLAG_CASEFOLD
                                | CM_FLAG_DIRSEARCH
                                | CM_FLAG_FOLLOW,
                                userp, tidPathp, &req, &dscp);
                if (code == 0) {
                    if (dscp->fileType == CM_SCACHETYPE_MOUNTPOINT
                         && !dscp->mountRootFidp)
                        code = CM_ERROR_NOSUCHFILE;
                    else if (dscp->fileType == CM_SCACHETYPE_DIRECTORY) {
                        cm_buf_t *bp = buf_Find(dscp, &hzero);
                        if (bp)
                            buf_Release(bp);
                        else
                            code = CM_ERROR_NOSUCHFILE;
                    }
                    cm_ReleaseSCache(dscp);
                    if (code) {
                        cm_FreeSpace(spacep);
                        cm_ReleaseUser(userp);
                        smb_SendTran2Error(vcp, p, opx, code);
                        smb_FreeTran2Packet(outp);
                        return 0;
                    }
                }
            }
        }
		cm_FreeSpace(spacep);
	}

	/* now do namei and stat, and copy out the info */
        code = cm_NameI(cm_rootSCachep, (char *)(&p->parmsp[3]),
        	CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD, userp, tidPathp, &req, &scp);

	if (code) {
		cm_ReleaseUser(userp);
                smb_SendTran2Error(vcp, p, opx, code);
                smb_FreeTran2Packet(outp);
                return 0;
        }

        lock_ObtainMutex(&scp->mx);
        code = cm_SyncOp(scp, NULL, userp, &req, 0,
        	CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
	if (code) goto done;
        
        /* now we have the status in the cache entry, and everything is locked.
	 * Marshall the output data.
         */
	op = outp->datap;
	/* for info level 108, figure out short name */
	if (infoLevel == 0x108) {
		code = cm_GetShortName((char *)(&p->parmsp[3]), userp, &req,
					tidPathp, scp->fid.vnode, shortName,
					(size_t *) &len);
		if (code) {
			goto done;
		}

		op = outp->datap;
		*((u_long *)op) = len * 2; op += 4;
		mbstowcs((unsigned short *)op, shortName, len);
		op += (len * 2);

		goto done;
	}
	if (infoLevel == 1 || infoLevel == 2) {
		smb_SearchTimeFromUnixTime(&dosTime, scp->clientModTime);
	        *((u_long *)op) = dosTime; op += 4;	/* creation time */
	        *((u_long *)op) = dosTime; op += 4;	/* access time */
	        *((u_long *)op) = dosTime; op += 4;	/* write time */
	        *((u_long *)op) = scp->length.LowPart; op += 4;	/* length */
	        *((u_long *)op) = scp->length.LowPart; op += 4;	/* alloc size */
		attributes = smb_Attributes(scp);
		*((u_short *)op) = attributes; op += 2;	/* attributes */
	}
	else if (infoLevel == 0x101) {
		smb_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);
		*((FILETIME *)op) = ft; op += 8;	/* creation time */
		*((FILETIME *)op) = ft; op += 8;	/* last access time */
		*((FILETIME *)op) = ft; op += 8;	/* last write time */
		*((FILETIME *)op) = ft; op += 8;	/* last change time */
		extAttributes = smb_ExtAttributes(scp);
		*((u_long *)op) = extAttributes; op += 4; /* extended attribs */
		*((u_long *)op) = 0; op += 4;	/* don't know what this is */
	}
	else if (infoLevel == 0x102) {
		*((LARGE_INTEGER *)op) = scp->length; op += 8;	/* alloc size */
		*((LARGE_INTEGER *)op) = scp->length; op += 8;	/* EOF */
		*((u_long *)op) = scp->linkCount; op += 4;
		*op++ = 0;
		*op++ = 0;
		*op++ = (scp->fileType == CM_SCACHETYPE_DIRECTORY ? 1 : 0);
		*op++ = 0;
	}
	else if (infoLevel == 0x103) {
   		memset(op, 0, 4); op += 4;	/* EA size */
	}

        /* now, if we are being asked about extended attrs, return a 0 size */
	if (infoLevel == 2) {
		*((u_long *)op) = 0; op += 4;
	}
        

	/* send and free the packets */
done:
	lock_ReleaseMutex(&scp->mx);
        cm_ReleaseSCache(scp);
        cm_ReleaseUser(userp);
	if (code == 0) smb_SendTran2Packet(vcp, outp, opx);
        else smb_SendTran2Error(vcp, p, opx, code);
        smb_FreeTran2Packet(outp);

        return 0;
}

long smb_ReceiveTran2SetPathInfo(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *outp)
{
        return CM_ERROR_BADOP;
}

long smb_ReceiveTran2QFileInfo(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *opx)
{
	smb_tran2Packet_t *outp;
	FILETIME ft;
	unsigned long attributes;
	unsigned short infoLevel;
	int nbytesRequired;
	unsigned short fid;
	cm_user_t *userp;
    smb_fid_t *fidp;
	cm_scache_t *scp;
	char *op;
	long code;
	cm_req_t req;

	cm_InitReq(&req);

    fid = p->parmsp[0];
    fidp = smb_FindFID(vcp, fid, 0);

	if (fidp == NULL) {
		smb_SendTran2Error(vcp, p, opx, CM_ERROR_BADFD);
		return 0;
	}

	infoLevel = p->parmsp[1];
	if (infoLevel == 0x101) nbytesRequired = 40;
	else if (infoLevel == 0x102) nbytesRequired = 24;
	else if (infoLevel == 0x103) nbytesRequired = 4;
	else if (infoLevel == 0x104) nbytesRequired = 6;
	else {
		osi_Log2(afsd_logp, "Bad Tran2 op 0x%x infolevel 0x%x",
                 p->opcode, infoLevel);
		smb_SendTran2Error(vcp, p, opx, CM_ERROR_INVAL);
        smb_ReleaseFID(fidp);
		return 0;
	}
	osi_Log2(afsd_logp, "T2 QFileInfo type 0x%x fid %d", infoLevel, fid);

	outp = smb_GetTran2ResponsePacket(vcp, p, opx, 2, nbytesRequired);

	if (infoLevel > 0x100)
		outp->totalParms = 2;
	else
		outp->totalParms = 0;
	outp->totalData = nbytesRequired;

	userp = smb_GetTran2User(vcp, p);
    if(!userp) {
    	osi_Log1(afsd_logp, "ReceiveTran2QFileInfo unable to resolve user [%d]", p->uid);
    	code = CM_ERROR_BADSMB;
    	goto done;
    }

	scp = fidp->scp;
	lock_ObtainMutex(&scp->mx);
	code = cm_SyncOp(scp, NULL, userp, &req, 0,
                     CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
	if (code) goto done;

	/* now we have the status in the cache entry, and everything is locked.
	 * Marshall the output data.
	 */
	op = outp->datap;
	if (infoLevel == 0x101) {
		smb_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);
		*((FILETIME *)op) = ft; op += 8;	/* creation time */
		*((FILETIME *)op) = ft; op += 8;	/* last access time */
		*((FILETIME *)op) = ft; op += 8;	/* last write time */
		*((FILETIME *)op) = ft; op += 8;	/* last change time */
		attributes = smb_ExtAttributes(scp);
		*((u_long *)op) = attributes; op += 4;
		*((u_long *)op) = 0; op += 4;
	}
	else if (infoLevel == 0x102) {
		*((LARGE_INTEGER *)op) = scp->length; op += 8;	/* alloc size */
		*((LARGE_INTEGER *)op) = scp->length; op += 8;	/* EOF */
		*((u_long *)op) = scp->linkCount; op += 4;
		*op++ = ((fidp->flags & SMB_FID_DELONCLOSE) ? 1 : 0);
		*op++ = (scp->fileType == CM_SCACHETYPE_DIRECTORY ? 1 : 0);
		*op++ = 0;
		*op++ = 0;
	}
	else if (infoLevel == 0x103) {
		*((u_long *)op) = 0; op += 4;
	}
	else if (infoLevel == 0x104) {
		unsigned long len;
		char *name;

		if (fidp->NTopen_wholepathp)
			name = fidp->NTopen_wholepathp;
		else
			name = "\\";	/* probably can't happen */
		len = strlen(name);
		outp->totalData = (len*2) + 4;	/* this is actually what we want to return */
		*((u_long *)op) = len * 2; op += 4;
		mbstowcs((unsigned short *)op, name, len); op += (len * 2);
	}

	/* send and free the packets */
done:
	lock_ReleaseMutex(&scp->mx);
	cm_ReleaseUser(userp);
	smb_ReleaseFID(fidp);
	if (code == 0) smb_SendTran2Packet(vcp, outp, opx);
	else smb_SendTran2Error(vcp, p, opx, code);
	smb_FreeTran2Packet(outp);

	return 0;
}

long smb_ReceiveTran2SetFileInfo(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *op)
{
	long code;
	unsigned short fid;
	smb_fid_t *fidp;
	unsigned short infoLevel;
	smb_tran2Packet_t *outp;
	cm_user_t *userp;
	cm_scache_t *scp;
	cm_req_t req;

	cm_InitReq(&req);

    fid = p->parmsp[0];
	fidp = smb_FindFID(vcp, fid, 0);

	if (fidp == NULL) {
		smb_SendTran2Error(vcp, p, op, CM_ERROR_BADFD);
		return 0;
	}

	infoLevel = p->parmsp[1];
	if (infoLevel > 0x104 || infoLevel < 0x101) {
		osi_Log2(afsd_logp, "Bad Tran2 op 0x%x infolevel 0x%x",
			 p->opcode, infoLevel);
		smb_SendTran2Error(vcp, p, op, CM_ERROR_INVAL);
        smb_ReleaseFID(fidp);
		return 0;
	}

	if (infoLevel == 0x102 && !(fidp->flags & SMB_FID_OPENDELETE)) {
		smb_SendTran2Error(vcp, p, op, CM_ERROR_NOACCESS);
        smb_ReleaseFID(fidp);
		return 0;
	}
	if ((infoLevel == 0x103 || infoLevel == 0x104)
	    && !(fidp->flags & SMB_FID_OPENWRITE)) {
		smb_SendTran2Error(vcp, p, op, CM_ERROR_NOACCESS);
        smb_ReleaseFID(fidp);
		return 0;
	}

	osi_Log1(afsd_logp, "T2 SFileInfo type 0x%x", infoLevel);

	outp = smb_GetTran2ResponsePacket(vcp, p, op, 2, 0);

	outp->totalParms = 2;
	outp->totalData = 0;

	userp = smb_GetTran2User(vcp, p);
    if(!userp) {
    	osi_Log1(afsd_logp,"ReceiveTran2SetFileInfo unable to resolve user [%d]", p->uid);
    	code = CM_ERROR_BADSMB;
    	goto done;
    }

	scp = fidp->scp;

	if (infoLevel == 0x101) {
		FILETIME lastMod;
		unsigned int attribute;
		cm_attr_t attr;

		/* lock the vnode with a callback; we need the current status
		 * to determine what the new status is, in some cases.
		 */
		lock_ObtainMutex(&scp->mx);
		code = cm_SyncOp(scp, NULL, userp, &req, 0,
                         CM_SCACHESYNC_GETSTATUS
                         | CM_SCACHESYNC_NEEDCALLBACK);
		if (code) {
			lock_ReleaseMutex(&scp->mx);
			goto done;
		}

		/* prepare for setattr call */
		attr.mask = 0;
		
		lastMod = *((FILETIME *)(p->datap + 16));
		/* when called as result of move a b, lastMod is (-1, -1). 
         * If the check for -1 is not present, timestamp
		 * of the resulting file will be 1969 (-1)
		 */
		if (LargeIntegerNotEqualToZero(*((LARGE_INTEGER *)&lastMod)) && 
            lastMod.dwLowDateTime != -1 && lastMod.dwHighDateTime != -1) {
			attr.mask |= CM_ATTRMASK_CLIENTMODTIME;
			smb_UnixTimeFromLargeSearchTime(&attr.clientModTime,
							&lastMod);
			fidp->flags |= SMB_FID_MTIMESETDONE;
		}
		
		attribute = *((u_long *)(p->datap + 32));
		if (attribute != 0) {
			if ((scp->unixModeBits & 0222)
			    && (attribute & 1) != 0) {
				/* make a writable file read-only */
				attr.mask |= CM_ATTRMASK_UNIXMODEBITS;
				attr.unixModeBits = scp->unixModeBits & ~0222;
			}
			else if ((scp->unixModeBits & 0222) == 0
				 && (attribute & 1) == 0) {
				/* make a read-only file writable */
				attr.mask |= CM_ATTRMASK_UNIXMODEBITS;
				attr.unixModeBits = scp->unixModeBits | 0222;
			}
		}
		lock_ReleaseMutex(&scp->mx);

		/* call setattr */
		if (attr.mask)
			code = cm_SetAttr(scp, &attr, userp, &req);
		else
			code = 0;
	}
	else if (infoLevel == 0x103 || infoLevel == 0x104) {
		LARGE_INTEGER size = *((LARGE_INTEGER *)(p->datap));
		cm_attr_t attr;

		attr.mask = CM_ATTRMASK_LENGTH;
		attr.length.LowPart = size.LowPart;
		attr.length.HighPart = size.HighPart;
		code = cm_SetAttr(scp, &attr, userp, &req);
	}
	else if (infoLevel == 0x102) {
		if (*((char *)(p->datap))) {
			code = cm_CheckNTDelete(fidp->NTopen_dscp, scp, userp,
						&req);
			if (code == 0)
				fidp->flags |= SMB_FID_DELONCLOSE;
		}
		else {
			code = 0;
			fidp->flags &= ~SMB_FID_DELONCLOSE;
		}
	}
done:
	cm_ReleaseUser(userp);
	smb_ReleaseFID(fidp);
	if (code == 0) smb_SendTran2Packet(vcp, outp, op);
	else smb_SendTran2Error(vcp, p, op, code);
	smb_FreeTran2Packet(outp);

	return 0;
}

long smb_ReceiveTran2FSCTL(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *outp)
{
        return CM_ERROR_BADOP;
}

long smb_ReceiveTran2IOCTL(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *outp)
{
        return CM_ERROR_BADOP;
}

long smb_ReceiveTran2FindNotifyFirst(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *outp)
{
        return CM_ERROR_BADOP;
}

long smb_ReceiveTran2FindNotifyNext(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *outp)
{
        return CM_ERROR_BADOP;
}

long smb_ReceiveTran2MKDir(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *outp)
{
        return CM_ERROR_BADOP;
}

long smb_ApplyV3DirListPatches(cm_scache_t *dscp,
	smb_dirListPatch_t **dirPatchespp, int infoLevel, cm_user_t *userp,
	cm_req_t *reqp)
{
	long code;
        cm_scache_t *scp;
        cm_scache_t *targetScp;			/* target if scp is a symlink */
        char *dptr;
        long dosTime;
	FILETIME ft;
        int shortTemp;
        unsigned short attr;
	unsigned long lattr;
        smb_dirListPatch_t *patchp;
        smb_dirListPatch_t *npatchp;
        
        for(patchp = *dirPatchespp; patchp; patchp =
        	(smb_dirListPatch_t *) osi_QNext(&patchp->q)) {
		code = cm_GetSCache(&patchp->fid, &scp, userp, reqp);
                if (code) continue;
                lock_ObtainMutex(&scp->mx);
                code = cm_SyncOp(scp, NULL, userp, reqp, 0,
                	CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
		if (code) {
			lock_ReleaseMutex(&scp->mx);
			cm_ReleaseSCache(scp);
			continue;
                }
                
                /* now watch for a symlink */
                if (scp->fileType == CM_SCACHETYPE_SYMLINK) {
			lock_ReleaseMutex(&scp->mx);
                        code = cm_EvaluateSymLink(dscp, scp, &targetScp, userp,
						  reqp);
                        if (code == 0) {
				/* we have a more accurate file to use (the
				 * target of the symbolic link).  Otherwise,
				 * we'll just use the symlink anyway.
                                 */
				osi_Log2(afsd_logp, "symlink vp %x to vp %x",
					 scp, targetScp);
				cm_ReleaseSCache(scp);
                                scp = targetScp;
                        }
                        lock_ObtainMutex(&scp->mx);
                }

		dptr = patchp->dptr;

		if (infoLevel >= 0x101) {
			/* get filetime */
			smb_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);

			/* copy to Creation Time */
			*((FILETIME *)dptr) = ft;
			dptr += 8;

			/* copy to Last Access Time */
			*((FILETIME *)dptr) = ft;
			dptr += 8;

			/* copy to Last Write Time */
			*((FILETIME *)dptr) = ft;
			dptr += 8;

			/* copy to Change Time */
			*((FILETIME *)dptr) = ft;
			dptr += 8;

			/* Use length for both file length and alloc length */
			*((LARGE_INTEGER *)dptr) = scp->length;
			dptr += 8;
			*((LARGE_INTEGER *)dptr) = scp->length;
			dptr += 8;

			/* Copy attributes */
			lattr = smb_ExtAttributes(scp);
            /* merge in hidden (dot file) attribute */
 			if( patchp->flags & SMB_DIRLISTPATCH_DOTFILE )
 				lattr |= SMB_ATTR_HIDDEN;
			*((u_long *)dptr) = lattr;
			dptr += 4;
		}
		else {
			/* get dos time */
			smb_SearchTimeFromUnixTime(&dosTime, scp->clientModTime);

			/* and copy out date */
			shortTemp = (dosTime>>16) & 0xffff;
			*((u_short *)dptr) = shortTemp;
			dptr += 2;

			/* copy out creation time */
			shortTemp = dosTime & 0xffff;
			*((u_short *)dptr) = shortTemp;
			dptr += 2;

			/* and copy out date */
			shortTemp = (dosTime>>16) & 0xffff;
			*((u_short *)dptr) = shortTemp;
			dptr += 2;
			
			/* copy out access time */
			shortTemp = dosTime & 0xffff;
			*((u_short *)dptr) = shortTemp;
			dptr += 2;

			/* and copy out date */
			shortTemp = (dosTime>>16) & 0xffff;
			*((u_short *)dptr) = shortTemp;
			dptr += 2;
			
			/* copy out mod time */
			shortTemp = dosTime & 0xffff;
			*((u_short *)dptr) = shortTemp;
			dptr += 2;

			/* copy out file length and alloc length,
			 * using the same for both
			 */
			*((u_long *)dptr) = scp->length.LowPart;
			dptr += 4;
			*((u_long *)dptr) = scp->length.LowPart;
			dptr += 4;

			/* finally copy out attributes as short */
			attr = smb_Attributes(scp);
            /* merge in hidden (dot file) attribute */
            if( patchp->flags & SMB_DIRLISTPATCH_DOTFILE )
                attr |= SMB_ATTR_HIDDEN;
			*dptr++ = attr & 0xff;
			*dptr++ = (attr >> 8) & 0xff;
		}

                lock_ReleaseMutex(&scp->mx);
                cm_ReleaseSCache(scp);
	}
        
        /* now free the patches */
        for(patchp = *dirPatchespp; patchp; patchp = npatchp) {
		npatchp = (smb_dirListPatch_t *) osi_QNext(&patchp->q);
                free(patchp);
	}
        
        /* and mark the list as empty */
        *dirPatchespp = NULL;

        return code;
}

/* do a case-folding search of the star name mask with the name in namep.
 * Return 1 if we match, otherwise 0.
 */
int smb_V3MatchMask(char *namep, char *maskp, int flags)
{
	unsigned char tcp1, tcp2;	/* Pattern characters */
        unsigned char tcn1;		/* Name characters */
	int sawDot = 0, sawStar = 0;
	char *starNamep, *starMaskp;
	static char nullCharp[] = {0};

	/* make sure we only match 8.3 names, if requested */
	if ((flags & CM_FLAG_8DOT3) && !cm_Is8Dot3(namep)) return 0;

	/* loop */
	while (1) {
		/* Next pattern character */
		tcp1 = *maskp++;

		/* Next name character */
		tcn1 = *namep;

		if (tcp1 == 0) {
			/* 0 - end of pattern */
			if (tcn1 == 0)
				return 1;
			else
				return 0;
		}
		else if (tcp1 == '.' || tcp1 == '"') {
			if (sawDot) {
				if (tcn1 == '.') {
					namep++;
					continue;
				} else
					return 0;
			}
			else {
				/*
				 * first dot in pattern;
				 * must match dot or end of name
				 */
				sawDot = 1;
				if (tcn1 == 0)
					continue;
				else if (tcn1 == '.') {
					sawStar = 0;
					namep++;
					continue;
				}
				else
					return 0;
			}
		}
		else if (tcp1 == '?') {
			if (tcn1 == 0 || tcn1 == '.')
				return 0;
			namep++;
			continue;
		}
		else if (tcp1 == '>') {
			if (tcn1 != 0 && tcn1 != '.')
				namep++;
			continue;
		}
		else if (tcp1 == '*' || tcp1 == '<') {
			tcp2 = *maskp++;
			if (tcp2 == 0)
				return 1;
			else if (tcp2 == '.' || tcp2 == '"') {
				while (tcn1 != '.' && tcn1 != 0)
					tcn1 = *++namep;
				if (tcn1 == 0) {
					if (sawDot)
						return 0;
					else
						continue;
				}
				else {
					namep++;
					continue;
				}
			}
			else {
				/*
				 * pattern character after '*' is not null or
				 * period.  If it is '?' or '>', we are not
				 * going to understand it.  If it is '*' or
				 * '<', we are going to skip over it.  None of
				 * these are likely, I hope.
				 */
				/* skip over '*' and '<' */
				while (tcp2 == '*' || tcp2 == '<')
					tcp2 = *maskp++;

				/* skip over characters that don't match tcp2 */
				while (tcn1 != '.' && tcn1 != 0
					&& cm_foldUpper[tcn1] != cm_foldUpper[tcp2])
					tcn1 = *++namep;

				/* No match */
				if (tcn1 == '.' || tcn1 == 0)
					return 0;

				/* Remember where we are */
				sawStar = 1;
				starMaskp = maskp;
				starNamep = namep;

				namep++;
				continue;
			}
		}
		else {
			/* tcp1 is not a wildcard */
			if (cm_foldUpper[tcn1] == cm_foldUpper[tcp1]) {
				/* they match */
				namep++;
				continue;
			}
			/* if trying to match a star pattern, go back */
			if (sawStar) {
				maskp = starMaskp - 2;
				namep = starNamep + 1;
				sawStar = 0;
				continue;
			}
			/* that's all */
			return 0;
		}
	}
}

long smb_ReceiveTran2SearchDir(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *opx)
{
	int attribute;
        long nextCookie;
        char *tp;
        long code;
        char *pathp;
        cm_dirEntry_t *dep;
        int maxCount;
        smb_dirListPatch_t *dirListPatchesp;
        smb_dirListPatch_t *curPatchp;
        cm_buf_t *bufferp;
        long temp;
        long orbytes;			/* # of bytes in this output record */
        long ohbytes;			/* # of bytes, except file name */
        long onbytes;			/* # of bytes in name, incl. term. null */
        osi_hyper_t dirLength;
        osi_hyper_t bufferOffset;
        osi_hyper_t curOffset;
        osi_hyper_t thyper;
        smb_dirSearch_t *dsp;
        cm_scache_t *scp;
        long entryInDir;
        long entryInBuffer;
	cm_pageHeader_t *pageHeaderp;
        cm_user_t *userp = NULL;
        int slotInPage;
        int returnedNames;
        long nextEntryCookie;
        int numDirChunks;		/* # of 32 byte dir chunks in this entry */
        char *op;			/* output data ptr */
	char *origOp;			/* original value of op */
        cm_space_t *spacep;		/* for pathname buffer */
        long maxReturnData;		/* max # of return data */
        long maxReturnParms;		/* max # of return parms */
        long bytesInBuffer;		/* # data bytes in the output buffer */
        int starPattern;
        char *maskp;			/* mask part of path */
        int infoLevel;
        int searchFlags;
        int eos;
        smb_tran2Packet_t *outp;	/* response packet */
	char *tidPathp;
	int align;
	char shortName[13];		/* 8.3 name if needed */
	int NeedShortName;
	char *shortNameEnd;
        int fileType;
        cm_fid_t fid;
        
        cm_req_t req;

	cm_InitReq(&req);

	eos = 0;
	if (p->opcode == 1) {
		/* find first; obtain basic parameters from request */
                attribute = p->parmsp[0];
                maxCount = p->parmsp[1];
                infoLevel = p->parmsp[3];
                searchFlags = p->parmsp[2];
                dsp = smb_NewDirSearch(1);
                dsp->attribute = attribute;
                pathp = ((char *) p->parmsp) + 12;	/* points to path */
                nextCookie = 0;
                maskp = strrchr(pathp, '\\');
                if (maskp == NULL) maskp = pathp;
		else maskp++;	/* skip over backslash */
                strcpy(dsp->mask, maskp);	/* and save mask */
   		/* track if this is likely to match a lot of entries */
   	        starPattern = smb_V3IsStarMask(maskp);
        }
        else {
		osi_assert(p->opcode == 2);
                /* find next; obtain basic parameters from request or open dir file */
                dsp = smb_FindDirSearch(p->parmsp[0]);
                if (!dsp) return CM_ERROR_BADFD;
                attribute = dsp->attribute;
                maxCount = p->parmsp[1];
                infoLevel = p->parmsp[2];
                searchFlags = p->parmsp[5];
                pathp = NULL;
                nextCookie = p->parmsp[3] | (p->parmsp[4] << 16);
                maskp = dsp->mask;
		starPattern = 1;	/* assume, since required a Find Next */
        }

	osi_Log4(afsd_logp,
		 "T2 search dir attr 0x%x, info level %d, max count %d, flags 0x%x",
        	 attribute, infoLevel, maxCount, searchFlags);

	osi_Log2(afsd_logp, "...T2 search op %d, nextCookie 0x%x",
		 p->opcode, nextCookie);

	if (infoLevel >= 0x101)
		searchFlags &= ~4;	/* no resume keys */

        dirListPatchesp = NULL;

	maxReturnData = p->maxReturnData;
        if (p->opcode == 1)	/* find first */
        	maxReturnParms = 10;	/* bytes */
	else
        	maxReturnParms = 8;	/* bytes */

#ifndef CM_CONFIG_MULTITRAN2RESPONSES
        if (maxReturnData > 6000) maxReturnData = 6000;
#endif /* CM_CONFIG_MULTITRAN2RESPONSES */

	outp = smb_GetTran2ResponsePacket(vcp, p, opx, maxReturnParms,
					  maxReturnData);

        osi_Log1(afsd_logp, "T2 receive search dir %s",
			osi_LogSaveString(afsd_logp, pathp));
        
        /* bail out if request looks bad */
    if (p->opcode == 1 && !pathp) {
        smb_ReleaseDirSearch(dsp);
        smb_FreeTran2Packet(outp);
        return CM_ERROR_BADSMB;
    }
        
	osi_Log2(afsd_logp, "T2 dir search cookie 0x%x, connection %d",
        	nextCookie, dsp->cookie);

 	userp = smb_GetTran2User(vcp, p);
    if (!userp) {
    	osi_Log1(afsd_logp, "T2 dir search unable to resolve user [%d]", p->uid);
    	smb_ReleaseDirSearch(dsp);
    	smb_FreeTran2Packet(outp);
    	return CM_ERROR_BADSMB;
    }

	/* try to get the vnode for the path name next */
	lock_ObtainMutex(&dsp->mx);
	if (dsp->scp) {
		scp = dsp->scp;
                cm_HoldSCache(scp);
                code = 0;
        }
        else {
		spacep = cm_GetSpace();
	        smb_StripLastComponent(spacep->data, NULL, pathp);
                lock_ReleaseMutex(&dsp->mx);

		tidPathp = smb_GetTIDPath(vcp, p->tid);
	        code = cm_NameI(cm_rootSCachep, spacep->data,
				CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                		userp, tidPathp, &req, &scp);
	        cm_FreeSpace(spacep);

                lock_ObtainMutex(&dsp->mx);
		if (code == 0) {
		        if (dsp->scp != 0) cm_ReleaseSCache(dsp->scp);
			dsp->scp = scp;
			/* we need one hold for the entry we just stored into,
                         * and one for our own processing.  When we're done
			 * with this function, we'll drop the one for our own
			 * processing.  We held it once from the namei call,
			 * and so we do another hold now.
                         */
                        cm_HoldSCache(scp);
   			lock_ObtainMutex(&scp->mx);
   			if ((scp->flags & CM_SCACHEFLAG_BULKSTATTING) == 0
   			    && LargeIntegerGreaterOrEqualToZero(scp->bulkStatProgress)) {
			        scp->flags |= CM_SCACHEFLAG_BULKSTATTING;
				dsp->flags |= SMB_DIRSEARCH_BULKST;
   			}
   			lock_ReleaseMutex(&scp->mx);
                }
        }
	lock_ReleaseMutex(&dsp->mx);
        if (code) {
		cm_ReleaseUser(userp);
                smb_FreeTran2Packet(outp);
		smb_DeleteDirSearch(dsp);
		smb_ReleaseDirSearch(dsp);
                return code;
        }

        /* get the directory size */
	lock_ObtainMutex(&scp->mx);
        code = cm_SyncOp(scp, NULL, userp, &req, 0,
        	CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
	if (code) {
		lock_ReleaseMutex(&scp->mx);
                cm_ReleaseSCache(scp);
                cm_ReleaseUser(userp);
                smb_FreeTran2Packet(outp);
		smb_DeleteDirSearch(dsp);
		smb_ReleaseDirSearch(dsp);
                return code;
        }
        
        dirLength = scp->length;
        bufferp = NULL;
        bufferOffset.LowPart = bufferOffset.HighPart = 0;
        curOffset.HighPart = 0;
        curOffset.LowPart = nextCookie;
	origOp = outp->datap;

        code = 0;
        returnedNames = 0;
        bytesInBuffer = 0;
        while (1) {
		op = origOp;
		if (searchFlags & 4)
			/* skip over resume key */
			op += 4;

		/* make sure that curOffset.LowPart doesn't point to the first
                 * 32 bytes in the 2nd through last dir page, and that it doesn't
                 * point at the first 13 32-byte chunks in the first dir page,
                 * since those are dir and page headers, and don't contain useful
                 * information.
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
            if (LargeIntegerGreaterThanOrEqualTo(curOffset, dirLength)) {
                eos = 1;
                break;
            }

            /* check if we've returned all the names that will fit in the
             * response packet; we check return count as well as the number
             * of bytes requested.  We check the # of bytes after we find
             * the dir entry, since we'll need to check its size.
             */
            if (returnedNames >= maxCount) {
                break;
            }
                
            /* see if we can use the bufferp we have now; compute in which
             * page the current offset would be, and check whether that's
             * the offset of the buffer we have.  If not, get the buffer.
             */
            thyper.HighPart = curOffset.HighPart;
            thyper.LowPart = curOffset.LowPart & ~(buf_bufferSize-1);
            if (!bufferp || !LargeIntegerEqualTo(thyper, bufferOffset)) {
			/* wrong buffer */
                if (bufferp) {
                    buf_Release(bufferp);
                    bufferp = NULL;
			}   
			lock_ReleaseMutex(&scp->mx);
			lock_ObtainRead(&scp->bufCreateLock);
                        code = buf_Get(scp, &thyper, &bufferp);
			lock_ReleaseRead(&scp->bufCreateLock);

			/* now, if we're doing a star match, do bulk fetching
			 * of all of the status info for files in the dir.
                         */
                        if (starPattern) {
				smb_ApplyV3DirListPatches(scp, &dirListPatchesp,
							  infoLevel, userp,
							  &req);
   				if ((dsp->flags & SMB_DIRSEARCH_BULKST)
   				    && LargeIntegerGreaterThanOrEqualTo(
   						thyper, scp->bulkStatProgress)) {
   					/* Don't bulk stat if risking timeout */
   					int now = GetCurrentTime();
   					if (now - req.startTime > 5000) {
   						scp->bulkStatProgress = thyper;
   						scp->flags &= ~CM_SCACHEFLAG_BULKSTATTING;
   						dsp->flags &= ~SMB_DIRSEARCH_BULKST;
   					} else
					        cm_TryBulkStat(scp, &thyper,
							       userp, &req);
   				}
			}

                        lock_ObtainMutex(&scp->mx);
                        if (code) break;
                        bufferOffset = thyper;

                        /* now get the data in the cache */
                        while (1) {
				code = cm_SyncOp(scp, bufferp, userp, &req,
					PRSFS_LOOKUP,
                                	CM_SCACHESYNC_NEEDCALLBACK
					| CM_SCACHESYNC_READ);
				if (code) break;
                                
                                if (cm_HaveBuffer(scp, bufferp, 0)) break;
                                
                                /* otherwise, load the buffer and try again */
                                code = cm_GetBuffer(scp, bufferp, NULL, userp,
						    &req);
                                if (code) break;
                        }
                        if (code) {
				buf_Release(bufferp);
                                bufferp = NULL;
                        	break;
			}
                }	/* if (wrong buffer) ... */
                
                /* now we have the buffer containing the entry we're interested
		 * in; copy it out if it represents a non-deleted entry.
                 */
		entryInDir = curOffset.LowPart & (2048-1);
                entryInBuffer = curOffset.LowPart & (buf_bufferSize - 1);

		/* page header will help tell us which entries are free.  Page
		 * header can change more often than once per buffer, since
		 * AFS 3 dir page size may be less than (but not more than)
		 * a buffer package buffer.
                 */
		/* only look intra-buffer */
		temp = curOffset.LowPart & (buf_bufferSize - 1);
                temp &= ~(2048 - 1);	/* turn off intra-page bits */
		pageHeaderp = (cm_pageHeader_t *) (bufferp->datap + temp);

		/* now determine which entry we're looking at in the page.
		 * If it is free (there's a free bitmap at the start of the
		 * dir), we should skip these 32 bytes.
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
		 * since we'll need it when writing out the cookie into the dir
		 * listing stream.
                 *
                 * XXXX Probably should do more sanity checking.
                 */
		numDirChunks = cm_NameEntries(dep->name, &onbytes);
		
                /* compute offset of cookie representing next entry */
                nextEntryCookie = curOffset.LowPart
				    + (CM_DIR_CHUNKSIZE * numDirChunks);

		/* Need 8.3 name? */
		NeedShortName = 0;
		if (infoLevel == 0x104
		    && dep->fid.vnode != 0
		    && !cm_Is8Dot3(dep->name)) {
			cm_Gen8Dot3Name(dep, shortName, &shortNameEnd);
			NeedShortName = 1;
		}

                if (dep->fid.vnode != 0
		    && (smb_V3MatchMask(dep->name, maskp, CM_FLAG_CASEFOLD)
			|| (NeedShortName
			    && smb_V3MatchMask(shortName, maskp,
						CM_FLAG_CASEFOLD)))) {

                        /* Eliminate entries that don't match requested
                           attributes */
                    if (smb_hideDotFiles && !(dsp->attribute & SMB_ATTR_HIDDEN) && 
                        smb_IsDotFile(dep->name))
                        goto nextEntry; /* no hidden files */
                    
                    if (!(dsp->attribute & SMB_ATTR_DIRECTORY))  /* no directories */
                    {
                            /* We have already done the cm_TryBulkStat above */
                            fid.cell = scp->fid.cell;
                            fid.volume = scp->fid.volume;
                            fid.vnode = ntohl(dep->fid.vnode);
                            fid.unique = ntohl(dep->fid.unique);
                            fileType = cm_FindFileType(&fid);
                            /*osi_Log2(afsd_logp, "smb_ReceiveTran2SearchDir: file %s "
                              "has filetype %d", dep->name,
                              fileType);*/
                            if (fileType == CM_SCACHETYPE_DIRECTORY)
                              goto nextEntry;
                        }

			/* finally check if this name will fit */

			/* standard dir entry stuff */
			if (infoLevel < 0x101)
				ohbytes = 23;	/* pre-NT */
			else if (infoLevel == 0x103)
				ohbytes = 12;	/* NT names only */
			else
				ohbytes = 64;	/* NT */

			if (infoLevel == 0x104)
				ohbytes += 26;	/* Short name & length */

	                if (searchFlags & 4) {
                        	ohbytes += 4;	/* if resume key required */
			}

                        if (infoLevel != 1
			    && infoLevel != 0x101
			    && infoLevel != 0x103)
				ohbytes += 4;	/* EASIZE */

			/* add header to name & term. null */
			orbytes = onbytes + ohbytes + 1;

			/* now, we round up the record to a 4 byte alignment,
			 * and we make sure that we have enough room here for
			 * even the aligned version (so we don't have to worry
			 * about an * overflow when we pad things out below).
			 * That's the reason for the alignment arithmetic below.
                         */
			if (infoLevel >= 0x101)
				align = (4 - (orbytes & 3)) & 3;
			else
				align = 0;
			if (orbytes + bytesInBuffer + align > maxReturnData)
                                break;

			/* this is one of the entries to use: it is not deleted
			 * and it matches the star pattern we're looking for.
			 * Put out the name, preceded by its length.
                         */
			/* First zero everything else */
			memset(origOp, 0, ohbytes);

			if (infoLevel <= 0x101)
                        	*(origOp + ohbytes - 1) = (unsigned char) onbytes;
			else if (infoLevel == 0x103)
				*((u_long *)(op + 8)) = onbytes;
			else
				*((u_long *)(op + 60)) = onbytes;
                        strcpy(origOp+ohbytes, dep->name);

			/* Short name if requested and needed */
                        if (infoLevel == 0x104) {
				if (NeedShortName) {
					strcpy(op + 70, shortName);
					*(op + 68) = shortNameEnd - shortName;
				}
			}

	                /* now, adjust the # of entries copied */
	                returnedNames++;

			/* NextEntryOffset and FileIndex */
			if (infoLevel >= 101) {
				int entryOffset = orbytes + align;
				*((u_long *)op) = entryOffset;
				*((u_long *)(op+4)) = nextEntryCookie;
			}

                        /* now we emit the attribute.  This is tricky, since
                         * we need to really stat the file to find out what
			 * type of entry we've got.  Right now, we're copying
			 * out data from * a buffer, while holding the scp
			 * locked, so it isn't really convenient to stat
			 * something now.  We'll put in a place holder
                         * now, and make a second pass before returning this
			 * to get the real attributes.  So, we just skip the
			 * data for now, and adjust it later.  We allocate a
			 * patch record to make it easy to find this point
			 * later.  The replay will happen at a time when it is
			 * safe to unlock the directory.
                         */
			if (infoLevel != 0x103) {
				curPatchp = malloc(sizeof(*curPatchp));
                        	osi_QAdd((osi_queue_t **) &dirListPatchesp,
					 &curPatchp->q);
				curPatchp->dptr = op;
				if (infoLevel >= 0x101)
					curPatchp->dptr += 8;

                if (smb_hideDotFiles && smb_IsDotFile(dep->name)) {
                    curPatchp->flags = SMB_DIRLISTPATCH_DOTFILE;
                }
                else
                    curPatchp->flags = 0;

				curPatchp->fid.cell = scp->fid.cell;
				curPatchp->fid.volume = scp->fid.volume;
				curPatchp->fid.vnode = ntohl(dep->fid.vnode);
				curPatchp->fid.unique = ntohl(dep->fid.unique);

                                /* temp */
                                curPatchp->dep = dep;
			}

			if (searchFlags & 4)
				/* put out resume key */
				*((u_long *)origOp) = nextEntryCookie;

			/* Adjust byte ptr and count */
			origOp += orbytes;	/* skip entire record */
                        bytesInBuffer += orbytes;

			/* and pad the record out */
                        while (--align >= 0) {
				*origOp++ = 0;
                                bytesInBuffer++;
                        }
                        
		}	/* if we're including this name */
                
nextEntry:
                /* and adjust curOffset to be where the new cookie is */
		thyper.HighPart = 0;
                thyper.LowPart = CM_DIR_CHUNKSIZE * numDirChunks;
                curOffset = LargeIntegerAdd(thyper, curOffset);
        }		/* while copying data for dir listing */

	/* release the mutex */
	lock_ReleaseMutex(&scp->mx);
        if (bufferp) buf_Release(bufferp);

	/* apply and free last set of patches; if not doing a star match, this
	 * will be empty, but better safe (and freeing everything) than sorry.
         */
        smb_ApplyV3DirListPatches(scp, &dirListPatchesp, infoLevel, userp,
				  &req);
        
        /* now put out the final parameters */
	if (returnedNames == 0) eos = 1;
        if (p->opcode == 1) {
		/* find first */
                outp->parmsp[0] = (unsigned short) dsp->cookie;
                outp->parmsp[1] = returnedNames;
                outp->parmsp[2] = eos;
                outp->parmsp[3] = 0;		/* nothing wrong with EAS */
                outp->parmsp[4] = 0;	/* don't need last name to continue
					 * search, cookie is enough.  Normally,
					 * this is the offset of the file name
					 * of the last entry returned.
                                         */
		outp->totalParms = 10;	/* in bytes */
        }
        else {
		/* find next */
                outp->parmsp[0] = returnedNames;
                outp->parmsp[1] = eos;
                outp->parmsp[2] = 0;	/* EAS error */
                outp->parmsp[3] = 0;	/* last name, as above */
                outp->totalParms = 8;	/* in bytes */
        }

	/* return # of bytes in the buffer */
        outp->totalData = bytesInBuffer;

	osi_Log2(afsd_logp, "T2 search dir done, %d names, code %d",
        	returnedNames, code);

	/* Return error code if unsuccessful on first request */
	if (code == 0 && p->opcode == 1 && returnedNames == 0)
		code = CM_ERROR_NOSUCHFILE;

	/* if we're supposed to close the search after this request, or if
         * we're supposed to close the search if we're done, and we're done,
         * or if something went wrong, close the search.
         */
        /* ((searchFlags & 1) || ((searchFlags & 2) && eos) */
	if ((searchFlags & 1) || (returnedNames == 0) || ((searchFlags & 2) &&
							  eos) || code != 0)
	    smb_DeleteDirSearch(dsp);
	if (code)
        	smb_SendTran2Error(vcp, p, opx, code);
	else {
        	smb_SendTran2Packet(vcp, outp, opx);
	}
	smb_FreeTran2Packet(outp);
    smb_ReleaseDirSearch(dsp);
    cm_ReleaseSCache(scp);
    cm_ReleaseUser(userp);
    return 0;
}

long smb_ReceiveV3FindClose(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    int dirHandle;
    smb_dirSearch_t *dsp;

    dirHandle = smb_GetSMBParm(inp, 0);
	
    osi_Log1(afsd_logp, "SMB3 find close handle %d", dirHandle);

    dsp = smb_FindDirSearch(dirHandle);
        
    if (!dsp)
		return CM_ERROR_BADFD;
	
    /* otherwise, we have an FD to destroy */
    smb_DeleteDirSearch(dsp);
    smb_ReleaseDirSearch(dsp);
        
	/* and return results */
	smb_SetSMBDataLength(outp, 0);

    return 0;
}

long smb_ReceiveV3FindNotifyClose(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
	smb_SetSMBDataLength(outp, 0);
    return 0;
}

long smb_ReceiveV3OpenX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
	char *pathp;
    long code;
	cm_space_t *spacep;
    int excl;
    cm_user_t *userp;
    cm_scache_t *dscp;		/* dir we're dealing with */
    cm_scache_t *scp;		/* file we're creating */
    cm_attr_t setAttr;
    int initialModeBits;
    smb_fid_t *fidp;
    int attributes;
    char *lastNamep;
    long dosTime;
    int openFun;
    int trunc;
    int openMode;
    int extraInfo;
    int openAction;
    int parmSlot;			/* which parm we're dealing with */
	char *tidPathp;
	cm_req_t req;

	cm_InitReq(&req);

    scp = NULL;
        
	extraInfo = (smb_GetSMBParm(inp, 2) & 1);	/* return extra info */
	openFun = smb_GetSMBParm(inp, 8);	/* open function */
    excl = ((openFun & 3) == 0);
    trunc = ((openFun & 3) == 2);		/* truncate it */
	openMode = (smb_GetSMBParm(inp, 3) & 0x7);
    openAction = 0;			/* tracks what we did */

    attributes = smb_GetSMBParm(inp, 5);
    dosTime = smb_GetSMBParm(inp, 6) | (smb_GetSMBParm(inp, 7) << 16);

	/* compute initial mode bits based on read-only flag in attributes */
    initialModeBits = 0666;
    if (attributes & 1) initialModeBits &= ~0222;
        
    pathp = smb_GetSMBData(inp, NULL);

	spacep = inp->spacep;
    smb_StripLastComponent(spacep->data, &lastNamep, pathp);

	if (lastNamep && strcmp(lastNamep, SMB_IOCTL_FILENAME) == 0) {
		/* special case magic file name for receiving IOCTL requests
         * (since IOCTL calls themselves aren't getting through).
         */
#ifdef NOTSERVICE
        osi_Log0(afsd_logp, "IOCTL Open");
#endif

        fidp = smb_FindFID(vcp, 0, SMB_FLAG_CREATE);
        smb_SetupIoctlFid(fidp, spacep);

		/* set inp->fid so that later read calls in same msg can find fid */
        inp->fid = fidp->fid;
        
        /* copy out remainder of the parms */
		parmSlot = 2;
		smb_SetSMBParm(outp, parmSlot, fidp->fid); parmSlot++;
		if (extraInfo) {
            smb_SetSMBParm(outp, parmSlot, /* attrs */ 0); parmSlot++;
            smb_SetSMBParm(outp, parmSlot, 0); parmSlot++;	/* mod time */
            smb_SetSMBParm(outp, parmSlot, 0); parmSlot++;
            smb_SetSMBParm(outp, parmSlot, 0); parmSlot++;	/* len */
            smb_SetSMBParm(outp, parmSlot, 0x7fff); parmSlot++;
            smb_SetSMBParm(outp, parmSlot, openMode); parmSlot++;
            smb_SetSMBParm(outp, parmSlot, 0); parmSlot++; /* file type 0 ==> normal file or dir */
            smb_SetSMBParm(outp, parmSlot, 0); parmSlot++; /* IPC junk */
		}   
		/* and the final "always present" stuff */
        smb_SetSMBParm(outp, parmSlot, /* openAction found existing file */ 1); parmSlot++;
		/* next write out the "unique" ID */
		smb_SetSMBParm(outp, parmSlot, 0x1234); parmSlot++;
		smb_SetSMBParm(outp, parmSlot, 0x5678); parmSlot++;
        smb_SetSMBParm(outp, parmSlot, 0); parmSlot++;
        smb_SetSMBDataLength(outp, 0);

		/* and clean up fid reference */
        smb_ReleaseFID(fidp);
        return 0;
    }

#ifdef DEBUG_VERBOSE
    {
    	char *hexp, *asciip;
    	asciip = (lastNamep ? lastNamep : pathp );
    	hexp = osi_HexifyString(asciip);
    	DEBUG_EVENT2("AFS", "V3Open H[%s] A[%s]", hexp, asciip );
    	free(hexp);
    }
#endif
    userp = smb_GetUser(vcp, inp);

	dscp = NULL;
	tidPathp = smb_GetTIDPath(vcp, ((smb_t *)inp)->tid);
	code = cm_NameI(cm_rootSCachep, pathp,
                    CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                    userp, tidPathp, &req, &scp);
	if (code != 0) {
		code = cm_NameI(cm_rootSCachep, spacep->data,
                        CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                        userp, tidPathp, &req, &dscp);

        if (code) {
            cm_ReleaseUser(userp);
            return code;
        }
        
        /* otherwise, scp points to the parent directory.  Do a lookup,
         * and truncate the file if we find it, otherwise we create the
         * file.
         */
        if (!lastNamep) lastNamep = pathp;
        else lastNamep++;
        code = cm_Lookup(dscp, lastNamep, CM_FLAG_CASEFOLD, userp,
                          &req, &scp);
        if (code && code != CM_ERROR_NOSUCHFILE) {
			cm_ReleaseSCache(dscp);
            cm_ReleaseUser(userp);
            return code;
        }
	}
        
    /* if we get here, if code is 0, the file exists and is represented by
     * scp.  Otherwise, we have to create it.  The dir may be represented
     * by dscp, or we may have found the file directly.  If code is non-zero,
     * scp is NULL.
     */
	if (code == 0) {
        code = cm_CheckOpen(scp, openMode, trunc, userp, &req);
        if (code) {
            if (dscp) cm_ReleaseSCache(dscp);
            cm_ReleaseSCache(scp);
            cm_ReleaseUser(userp);
            return code;
        }

		if (excl) {
			/* oops, file shouldn't be there */
            if (dscp) cm_ReleaseSCache(dscp);
            cm_ReleaseSCache(scp);
            cm_ReleaseUser(userp);
            return CM_ERROR_EXISTS;
        }

		if (trunc) {
			setAttr.mask = CM_ATTRMASK_LENGTH;
            setAttr.length.LowPart = 0;
            setAttr.length.HighPart = 0;
			code = cm_SetAttr(scp, &setAttr, userp, &req);
            openAction = 3;	/* truncated existing file */
		}
        else openAction = 1;	/* found existing file */
    }
	else if (!(openFun & 0x10)) {
		/* don't create if not found */
        if (dscp) cm_ReleaseSCache(dscp);
        cm_ReleaseUser(userp);
        return CM_ERROR_NOSUCHFILE;
    }
    else {
		osi_assert(dscp != NULL);
		osi_Log1(afsd_logp, "smb_ReceiveV3OpenX creating file %s",
                 osi_LogSaveString(afsd_logp, lastNamep));
		openAction = 2;	/* created file */
		setAttr.mask = CM_ATTRMASK_CLIENTMODTIME;
		smb_UnixTimeFromDosUTime(&setAttr.clientModTime, dosTime);
        code = cm_Create(dscp, lastNamep, 0, &setAttr, &scp, userp,
                         &req);
		if (code == 0 && (dscp->flags & CM_SCACHEFLAG_ANYWATCH))
			smb_NotifyChange(FILE_ACTION_ADDED,
                             FILE_NOTIFY_CHANGE_FILE_NAME,
                             dscp, lastNamep, NULL, TRUE);
        if (!excl && code == CM_ERROR_EXISTS) {
			/* not an exclusive create, and someone else tried
			 * creating it already, then we open it anyway.  We
			 * don't bother retrying after this, since if this next
			 * fails, that means that the file was deleted after we
			 * started this call.
             */
            code = cm_Lookup(dscp, lastNamep, CM_FLAG_CASEFOLD,
                             userp, &req, &scp);
            if (code == 0) {
                if (trunc) {
					setAttr.mask = CM_ATTRMASK_LENGTH;
                    setAttr.length.LowPart = 0;
                    setAttr.length.HighPart = 0;
                    code = cm_SetAttr(scp, &setAttr, userp, &req);
                }   
			}	/* lookup succeeded */
        }
    }
        
	/* we don't need this any longer */
	if (dscp) cm_ReleaseSCache(dscp);

    if (code) {
		/* something went wrong creating or truncating the file */
        if (scp) cm_ReleaseSCache(scp);
        cm_ReleaseUser(userp);
        return code;
    }
        
	/* make sure we're about to open a file */
	if (scp->fileType != CM_SCACHETYPE_FILE) {
		cm_ReleaseSCache(scp);
		cm_ReleaseUser(userp);
		return CM_ERROR_ISDIR;
	}

    /* now all we have to do is open the file itself */
    fidp = smb_FindFID(vcp, 0, SMB_FLAG_CREATE);
    osi_assert(fidp);
	
	/* save a pointer to the vnode */
    fidp->scp = scp;
        
	/* compute open mode */
    if (openMode != 1) fidp->flags |= SMB_FID_OPENREAD;
    if (openMode == 1 || openMode == 2)
        fidp->flags |= SMB_FID_OPENWRITE;

	smb_ReleaseFID(fidp);
        
	cm_Open(scp, 0, userp);

	/* set inp->fid so that later read calls in same msg can find fid */
    inp->fid = fidp->fid;
        
    /* copy out remainder of the parms */
	parmSlot = 2;
	smb_SetSMBParm(outp, parmSlot, fidp->fid); parmSlot++;
	lock_ObtainMutex(&scp->mx);
	if (extraInfo) {
        smb_SetSMBParm(outp, parmSlot, smb_Attributes(scp)); parmSlot++;
		smb_DosUTimeFromUnixTime(&dosTime, scp->clientModTime);
        smb_SetSMBParm(outp, parmSlot, dosTime & 0xffff); parmSlot++;
        smb_SetSMBParm(outp, parmSlot, (dosTime>>16) & 0xffff); parmSlot++;
        smb_SetSMBParm(outp, parmSlot, scp->length.LowPart & 0xffff); parmSlot++;
        smb_SetSMBParm(outp, parmSlot, (scp->length.LowPart >> 16) & 0xffff); parmSlot++;
        smb_SetSMBParm(outp, parmSlot, openMode); parmSlot++;
        smb_SetSMBParm(outp, parmSlot, 0); parmSlot++; /* file type 0 ==> normal file or dir */
        smb_SetSMBParm(outp, parmSlot, 0); parmSlot++; /* IPC junk */
	}
	/* and the final "always present" stuff */
    smb_SetSMBParm(outp, parmSlot, openAction); parmSlot++;
	/* next write out the "unique" ID */
	smb_SetSMBParm(outp, parmSlot, scp->fid.vnode & 0xffff); parmSlot++;
	smb_SetSMBParm(outp, parmSlot, scp->fid.volume & 0xffff); parmSlot++;
    smb_SetSMBParm(outp, parmSlot, 0); parmSlot++;
	lock_ReleaseMutex(&scp->mx);
    smb_SetSMBDataLength(outp, 0);

	osi_Log1(afsd_logp, "SMB OpenX opening fid %d", fidp->fid);

    cm_ReleaseUser(userp);
    /* leave scp held since we put it in fidp->scp */
    return 0;
}   

long smb_ReceiveV3LockingX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
	cm_req_t req;
	cm_user_t *userp;
	unsigned short fid;
	smb_fid_t *fidp;
	cm_scache_t *scp;
	unsigned char LockType;
	unsigned short NumberOfUnlocks, NumberOfLocks;
	unsigned long Timeout;
	char *op;
	LARGE_INTEGER LOffset, LLength;
	smb_waitingLock_t *waitingLock;
	void *lockp;
	long code;
	int i;

	cm_InitReq(&req);

	fid = smb_GetSMBParm(inp, 2);
	fid = smb_ChainFID(fid, inp);

	fidp = smb_FindFID(vcp, fid, 0);
	if (!fidp || (fidp->flags & SMB_FID_IOCTL)) {
		return CM_ERROR_BADFD;
	}
	/* set inp->fid so that later read calls in same msg can find fid */
    inp->fid = fid;

	userp = smb_GetUser(vcp, inp);

	scp = fidp->scp;

	lock_ObtainMutex(&scp->mx);
	code = cm_SyncOp(scp, NULL, userp, &req, 0,
			 CM_SCACHESYNC_NEEDCALLBACK
			 | CM_SCACHESYNC_GETSTATUS
			 | CM_SCACHESYNC_LOCK);
	if (code) goto doneSync;

	LockType = smb_GetSMBParm(inp, 3) & 0xff;
	Timeout = (smb_GetSMBParm(inp, 5) << 16) + smb_GetSMBParm(inp, 4);
	NumberOfUnlocks = smb_GetSMBParm(inp, 6);
	NumberOfLocks = smb_GetSMBParm(inp, 7);

	op = smb_GetSMBData(inp, NULL);

	for (i=0; i<NumberOfUnlocks; i++) {
		if (LockType & 0x10) {
			/* Large Files */
			LOffset.HighPart = *((LONG *)(op + 4));
			LOffset.LowPart = *((DWORD *)(op + 8));
			LLength.HighPart = *((LONG *)(op + 12));
			LLength.LowPart = *((DWORD *)(op + 16));
			op += 20;
		}
		else {
			/* Not Large Files */
			LOffset.HighPart = 0;
			LOffset.LowPart = *((DWORD *)(op + 2));
			LLength.HighPart = 0;
			LLength.LowPart = *((DWORD *)(op + 6));
			op += 10;
		}
		if (LargeIntegerNotEqualToZero(LOffset))
			continue;
		/* Do not check length -- length check done in cm_Unlock */

		code = cm_Unlock(scp, LockType, LOffset, LLength, userp, &req);
		if (code) goto done;
	}

	for (i=0; i<NumberOfLocks; i++) {
		if (LockType & 0x10) {
			/* Large Files */
			LOffset.HighPart = *((LONG *)(op + 4));
			LOffset.LowPart = *((DWORD *)(op + 8));
			LLength.HighPart = *((LONG *)(op + 12));
			LLength.LowPart = *((DWORD *)(op + 16));
			op += 20;
		}
		else {
			/* Not Large Files */
			LOffset.HighPart = 0;
			LOffset.LowPart = *((DWORD *)(op + 2));
			LLength.HighPart = 0;
			LLength.LowPart = *((DWORD *)(op + 6));
			op += 10;
		}
		if (LargeIntegerNotEqualToZero(LOffset))
			continue;
		if (LargeIntegerLessThan(LOffset, scp->length))
			continue;

		code = cm_Lock(scp, LockType, LOffset, LLength, Timeout,
				userp, &req, &lockp);
		if (code == CM_ERROR_WOULDBLOCK && Timeout != 0) {
			/* Put on waiting list */
			waitingLock = malloc(sizeof(smb_waitingLock_t));
			waitingLock->vcp = vcp;
			waitingLock->inp = smb_CopyPacket(inp);
			waitingLock->outp = smb_CopyPacket(outp);
			waitingLock->timeRemaining = Timeout;
			waitingLock->lockp = lockp;
			lock_ObtainWrite(&smb_globalLock);
			osi_QAdd((osi_queue_t **)&smb_allWaitingLocks,
				 &waitingLock->q);
			osi_Wakeup((long) &smb_allWaitingLocks);
			lock_ReleaseWrite(&smb_globalLock);
			/* don't send reply immediately */
			outp->flags |= SMB_PACKETFLAG_NOSEND;
		}
		if (code) break;
	}

	if (code) {
		/* release any locks acquired before the failure */
	}
	else
		smb_SetSMBDataLength(outp, 0);
done:
	cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_LOCK);
doneSync:
	lock_ReleaseMutex(&scp->mx);
	cm_ReleaseUser(userp);
	smb_ReleaseFID(fidp);

	return code;
}

long smb_ReceiveV3GetAttributes(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
	unsigned short fid;
    smb_fid_t *fidp;
    cm_scache_t *scp;
    long code;
    long searchTime;
    cm_user_t *userp;
	cm_req_t req;

	cm_InitReq(&req);

    fid = smb_GetSMBParm(inp, 0);
    fid = smb_ChainFID(fid, inp);
        
    fidp = smb_FindFID(vcp, fid, 0);
    if (!fidp || (fidp->flags & SMB_FID_IOCTL)) {
		return CM_ERROR_BADFD;
    }
        
    userp = smb_GetUser(vcp, inp);
        
    scp = fidp->scp;
        
    /* otherwise, stat the file */
	lock_ObtainMutex(&scp->mx);
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                     CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
	if (code) goto done;

	/* decode times.  We need a search time, but the response to this
     * call provides the date first, not the time, as returned in the
     * searchTime variable.  So we take the high-order bits first.
     */
	smb_SearchTimeFromUnixTime(&searchTime, scp->clientModTime);
    smb_SetSMBParm(outp, 0, (searchTime >> 16) & 0xffff);	/* ctime */
    smb_SetSMBParm(outp, 1, searchTime & 0xffff);
    smb_SetSMBParm(outp, 2, (searchTime >> 16) & 0xffff);	/* atime */
    smb_SetSMBParm(outp, 3, searchTime & 0xffff);
    smb_SetSMBParm(outp, 4, (searchTime >> 16) & 0xffff);	/* mtime */
    smb_SetSMBParm(outp, 5, searchTime & 0xffff);

    /* now handle file size and allocation size */
    smb_SetSMBParm(outp, 6, scp->length.LowPart & 0xffff);		/* file size */
    smb_SetSMBParm(outp, 7, (scp->length.LowPart >> 16) & 0xffff);
    smb_SetSMBParm(outp, 8, scp->length.LowPart & 0xffff);		/* alloc size */
    smb_SetSMBParm(outp, 9, (scp->length.LowPart >> 16) & 0xffff);

	/* file attribute */
    smb_SetSMBParm(outp, 10, smb_Attributes(scp));
        
    /* and finalize stuff */
    smb_SetSMBDataLength(outp, 0);
    code = 0;

done:
	lock_ReleaseMutex(&scp->mx);
	cm_ReleaseUser(userp);
	smb_ReleaseFID(fidp);
	return code;
}

long smb_ReceiveV3SetAttributes(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
	unsigned short fid;
    smb_fid_t *fidp;
    cm_scache_t *scp;
    long code;
	long searchTime;
    long unixTime;
    cm_user_t *userp;
    cm_attr_t attrs;
	cm_req_t req;

	cm_InitReq(&req);

    fid = smb_GetSMBParm(inp, 0);
    fid = smb_ChainFID(fid, inp);
        
    fidp = smb_FindFID(vcp, fid, 0);
    if (!fidp || (fidp->flags & SMB_FID_IOCTL)) {
		return CM_ERROR_BADFD;
    }
        
    userp = smb_GetUser(vcp, inp);
        
    scp = fidp->scp;
        
	/* now prepare to call cm_setattr.  This message only sets various times,
     * and AFS only implements mtime, and we'll set the mtime if that's
     * requested.  The others we'll ignore.
     */
	searchTime = smb_GetSMBParm(inp, 5) | (smb_GetSMBParm(inp, 6) << 16);
        
    if (searchTime != 0) {
		smb_UnixTimeFromSearchTime(&unixTime, searchTime);

        if ( unixTime != -1 ) {
            attrs.mask = CM_ATTRMASK_CLIENTMODTIME;
            attrs.clientModTime = unixTime;
            code = cm_SetAttr(scp, &attrs, userp, &req);

            osi_Log1(afsd_logp, "SMB receive V3SetAttributes [fid=%ld]", fid);
        } else {
            osi_Log1(afsd_logp, "**smb_UnixTimeFromSearchTime failed searchTime=%ld", searchTime);
        }
    }
    else code = 0;

	cm_ReleaseUser(userp);
	smb_ReleaseFID(fidp);
	return code;
}


long smb_ReceiveV3ReadX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
	osi_hyper_t offset;
    long count, finalCount;
    unsigned short fd;
    smb_fid_t *fidp;
    long code;
    cm_user_t *userp;
    char *op;
        
    fd = smb_GetSMBParm(inp, 2);
    count = smb_GetSMBParm(inp, 5);
    offset.HighPart = 0;	/* too bad */
    offset.LowPart = smb_GetSMBParm(inp, 3) | (smb_GetSMBParm(inp, 4) << 16);

    osi_Log3(afsd_logp, "smb_ReceiveV3Read fd %d, off 0x%x, size 0x%x",
             fd, offset.LowPart, count);
        
	fd = smb_ChainFID(fd, inp);
    fidp = smb_FindFID(vcp, fd, 0);
    if (!fidp) {
		return CM_ERROR_BADFD;
    }
	/* set inp->fid so that later read calls in same msg can find fid */
    inp->fid = fd;

    if (fidp->flags & SMB_FID_IOCTL) {
		return smb_IoctlV3Read(fidp, vcp, inp, outp);
    }
        
	userp = smb_GetUser(vcp, inp);

	/* 0 and 1 are reserved for request chaining, were setup by our caller,
     * and will be further filled in after we return.
     */
    smb_SetSMBParm(outp, 2, 0);	/* remaining bytes, for pipes */
    smb_SetSMBParm(outp, 3, 0);	/* resvd */
    smb_SetSMBParm(outp, 4, 0);	/* resvd */
	smb_SetSMBParm(outp, 5, count);	/* # of bytes we're going to read */
    /* fill in #6 when we have all the parameters' space reserved */
    smb_SetSMBParm(outp, 7, 0);	/* resv'd */
    smb_SetSMBParm(outp, 8, 0);	/* resv'd */
    smb_SetSMBParm(outp, 9, 0);	/* resv'd */
    smb_SetSMBParm(outp, 10, 0);	/* resv'd */
	smb_SetSMBParm(outp, 11, 0);	/* reserved */

	/* get op ptr after putting in the parms, since otherwise we don't
     * know where the data really is.
     */
    op = smb_GetSMBData(outp, NULL);
        
    /* now fill in offset from start of SMB header to first data byte (to op) */
    smb_SetSMBParm(outp, 6, ((int) (op - outp->data)));

	/* set the packet data length the count of the # of bytes */
    smb_SetSMBDataLength(outp, count);

#ifndef DJGPP
	code = smb_ReadData(fidp, &offset, count, op, userp, &finalCount);
#else /* DJGPP */
	code = smb_ReadData(fidp, &offset, count, op, userp, &finalCount, FALSE);
#endif /* !DJGPP */

	/* fix some things up */
	smb_SetSMBParm(outp, 5, finalCount);
	smb_SetSMBDataLength(outp, finalCount);

    smb_ReleaseFID(fidp);

    cm_ReleaseUser(userp);
    return code;
}   
        
/*
 * Values for createDisp, copied from NTDDK.H
 *
 *  FILE_SUPERSEDE	0	(???)
 *  FILE_OPEN		1	(open)
 *  FILE_CREATE		2	(exclusive)
 *  FILE_OPEN_IF	3	(non-exclusive)
 *  FILE_OVERWRITE	4	(open & truncate, but do not create)
 *  FILE_OVERWRITE_IF	5	(open & truncate, or create)
 */

long smb_ReceiveNTCreateX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
	char *pathp, *realPathp;
	long code;
	cm_space_t *spacep;
	cm_user_t *userp;
	cm_scache_t *dscp;		/* parent dir */
	cm_scache_t *scp;		/* file to create or open */
	cm_attr_t setAttr;
	char *lastNamep;
    char *treeStartp;
	unsigned short nameLength;
	unsigned int flags;
	unsigned int requestOpLock;
	unsigned int requestBatchOpLock;
	unsigned int mustBeDir;
    unsigned int treeCreate;
	int realDirFlag;
	unsigned int desiredAccess;
	unsigned int extAttributes;
	unsigned int createDisp;
	unsigned int createOptions;
	int initialModeBits;
	unsigned short baseFid;
	smb_fid_t *baseFidp;
	smb_fid_t *fidp;
	cm_scache_t *baseDirp;
	unsigned short openAction;
	int parmSlot;
	long fidflags;
	FILETIME ft;
	LARGE_INTEGER sz;
	char *tidPathp;
	BOOL foundscp;
	cm_req_t req;

	cm_InitReq(&req);

    treeCreate = FALSE;
	foundscp = FALSE;
	scp = NULL;

	nameLength = smb_GetSMBOffsetParm(inp, 2, 1);
	flags = smb_GetSMBOffsetParm(inp, 3, 1)
		  | (smb_GetSMBOffsetParm(inp, 4, 1) << 16);
	requestOpLock = flags & 0x02;
	requestBatchOpLock = flags & 0x04;
	mustBeDir = flags & 0x08;

	/*
	 * Why all of a sudden 32-bit FID?
	 * We will reject all bits higher than 16.
	 */
	if (smb_GetSMBOffsetParm(inp, 6, 1) != 0)
		return CM_ERROR_INVAL;
	baseFid = smb_GetSMBOffsetParm(inp, 5, 1);
	desiredAccess = smb_GetSMBOffsetParm(inp, 7, 1)
			  | (smb_GetSMBOffsetParm(inp, 8, 1) << 16);
	extAttributes = smb_GetSMBOffsetParm(inp, 13, 1)
			  | (smb_GetSMBOffsetParm(inp, 14, 1) << 16);
	createDisp = smb_GetSMBOffsetParm(inp, 17, 1)
			| (smb_GetSMBOffsetParm(inp, 18, 1) << 16);
	createOptions = smb_GetSMBOffsetParm(inp, 19, 1)
			  | (smb_GetSMBOffsetParm(inp, 20, 1) << 16);

	/* mustBeDir is never set; createOptions directory bit seems to be
         * more important
	 */
	if (createOptions & 1)
		realDirFlag = 1;
	else if (createOptions & 0x40)
		realDirFlag = 0;
	else
		realDirFlag = -1;

	/*
	 * compute initial mode bits based on read-only flag in
	 * extended attributes
	 */
	initialModeBits = 0666;
	if (extAttributes & 1) initialModeBits &= ~0222;

	pathp = smb_GetSMBData(inp, NULL);
	/* Sometimes path is not null-terminated, so we make a copy. */
	realPathp = malloc(nameLength+1);
	memcpy(realPathp, pathp, nameLength);
	realPathp[nameLength] = 0;

	spacep = inp->spacep;
	smb_StripLastComponent(spacep->data, &lastNamep, realPathp);

    osi_Log1(afsd_logp,"NTCreateX for [%s]",osi_LogSaveString(afsd_logp,realPathp));
    osi_Log4(afsd_logp,"NTCreateX da=[%x] ea=[%x] cd=[%x] co=[%x]", desiredAccess, extAttributes, createDisp, createOptions);

	if (lastNamep && strcmp(lastNamep, SMB_IOCTL_FILENAME) == 0) {
		/* special case magic file name for receiving IOCTL requests
		 * (since IOCTL calls themselves aren't getting through).
		 */
		fidp = smb_FindFID(vcp, 0, SMB_FLAG_CREATE);
		smb_SetupIoctlFid(fidp, spacep);

		/* set inp->fid so that later read calls in same msg can find fid */
		inp->fid = fidp->fid;

		/* out parms */
		parmSlot = 2;
		smb_SetSMBParmByte(outp, parmSlot, 0);	/* oplock */
		smb_SetSMBParm(outp, parmSlot, fidp->fid); parmSlot++;
		smb_SetSMBParmLong(outp, parmSlot, 1); parmSlot += 2; /* Action */
		/* times */
		memset(&ft, 0, sizeof(ft));
		smb_SetSMBParmDouble(outp, parmSlot, (char *)&ft); parmSlot += 4;
		smb_SetSMBParmDouble(outp, parmSlot, (char *)&ft); parmSlot += 4;
		smb_SetSMBParmDouble(outp, parmSlot, (char *)&ft); parmSlot += 4;
		smb_SetSMBParmDouble(outp, parmSlot, (char *)&ft); parmSlot += 4;
		smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2; /* attr */
		sz.HighPart = 0x7fff; sz.LowPart = 0;
		smb_SetSMBParmDouble(outp, parmSlot, (char *)&sz); parmSlot += 4; /* alen */
		smb_SetSMBParmDouble(outp, parmSlot, (char *)&sz); parmSlot += 4; /* len */
		smb_SetSMBParm(outp, parmSlot, 0); parmSlot++;	/* filetype */
		smb_SetSMBParm(outp, parmSlot, 0); parmSlot++;	/* dev state */
		smb_SetSMBParmByte(outp, parmSlot, 0);	/* is a dir? */
		smb_SetSMBDataLength(outp, 0);

		/* clean up fid reference */
		smb_ReleaseFID(fidp);
		free(realPathp);
		return 0;
	}

#ifdef DEBUG_VERBOSE
    {
    	char *hexp, *asciip;
    	asciip = (lastNamep? lastNamep : realPathp);
    	hexp = osi_HexifyString( asciip );
    	DEBUG_EVENT2("AFS", "NTCreateX H[%s] A[%s]", hexp, asciip);
    	free(hexp);
    }
#endif
    userp = smb_GetUser(vcp, inp);
    if (!userp) {
    	osi_Log1(afsd_logp, "NTCreateX Invalid user [%d]", ((smb_t *) inp)->uid);
    	free(realPathp);
    	return CM_ERROR_INVAL;
    }

	if (baseFid == 0) {
		baseDirp = cm_rootSCachep;
		tidPathp = smb_GetTIDPath(vcp, ((smb_t *)inp)->tid);
	}
	else {
        baseFidp = smb_FindFID(vcp, baseFid, 0);
        if (!baseFidp) {
        	osi_Log1(afsd_logp, "NTCreateX Invalid base fid [%d]", baseFid);
        	free(realPathp);
        	cm_ReleaseUser(userp);
        	return CM_ERROR_INVAL;
        }
		baseDirp = baseFidp->scp;
		tidPathp = NULL;
	}

    osi_Log1(afsd_logp, "NTCreateX tidPathp=[%s]", (tidPathp==NULL)?"null": osi_LogSaveString(afsd_logp,tidPathp));
	
    /* compute open mode */
	fidflags = 0;
	if (desiredAccess & DELETE)
		fidflags |= SMB_FID_OPENDELETE;
	if (desiredAccess & AFS_ACCESS_READ)
		fidflags |= SMB_FID_OPENREAD;
	if (desiredAccess & AFS_ACCESS_WRITE)
		fidflags |= SMB_FID_OPENWRITE;

	dscp = NULL;
	code = 0;
	code = cm_NameI(baseDirp, realPathp, CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
			userp, tidPathp, &req, &scp);
	if (code == 0) foundscp = TRUE;
	if (code != 0
	    || (fidflags & (SMB_FID_OPENDELETE | SMB_FID_OPENWRITE))) {
		/* look up parent directory */
        /* If we are trying to create a path (i.e. multiple nested directories), then we don't *need*
        the immediate parent.  We have to work our way up realPathp until we hit something that we
        recognize.
        */

        while(1) {
            char *tp;

            code = cm_NameI(baseDirp, spacep->data,
                             CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                             userp, tidPathp, &req, &dscp);

            if (code && 
                (tp = strrchr(spacep->data,'\\')) &&
                (createDisp == 2) &&
                (realDirFlag == 1)) {
                *tp++ = 0;
                treeCreate = TRUE;
                treeStartp = realPathp + (tp - spacep->data);

                if (*tp && !smb_IsLegalFilename(tp)) {
                    if(baseFid != 0) smb_ReleaseFID(baseFidp);
                    cm_ReleaseUser(userp);
                    free(realPathp);
                    return CM_ERROR_BADNTFILENAME;
                }
            }
            else
                break;
        }

        if (baseFid != 0) smb_ReleaseFID(baseFidp);

        if (code) {
            osi_Log0(afsd_logp,"NTCreateX parent not found");
            cm_ReleaseUser(userp);
            free(realPathp);
            return code;
        }

        if(treeCreate && dscp->fileType == CM_SCACHETYPE_FILE) {
            /* A file exists where we want a directory. */
            cm_ReleaseSCache(dscp);
            cm_ReleaseUser(userp);
            free(realPathp);
            return CM_ERROR_EXISTS;
        }

        if (!lastNamep) lastNamep = realPathp;
        else lastNamep++;

        if (!smb_IsLegalFilename(lastNamep)) {
            cm_ReleaseSCache(dscp);
            cm_ReleaseUser(userp);
            free(realPathp);
            return CM_ERROR_BADNTFILENAME;
        }

        if (!foundscp && !treeCreate) {
			code = cm_Lookup(dscp, lastNamep,
					 CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
					 userp, &req, &scp);
			if (code && code != CM_ERROR_NOSUCHFILE) {
				cm_ReleaseSCache(dscp);
				cm_ReleaseUser(userp);
				free(realPathp);
				return code;
			}
		}
	}
	else {
		if (baseFid != 0) smb_ReleaseFID(baseFidp);
	}

	/* if we get here, if code is 0, the file exists and is represented by
	 * scp.  Otherwise, we have to create it.  The dir may be represented
	 * by dscp, or we may have found the file directly.  If code is non-zero,
	 * scp is NULL.
	 */
	if (code == 0 && !treeCreate) {
		code = cm_CheckNTOpen(scp, desiredAccess, createDisp, userp,
				      &req);
		if (code) {
			if (dscp) cm_ReleaseSCache(dscp);
			cm_ReleaseSCache(scp);
			cm_ReleaseUser(userp);
			free(realPathp);
			return code;
		}

		if (createDisp == 2) {
			/* oops, file shouldn't be there */
			if (dscp) cm_ReleaseSCache(dscp);
			cm_ReleaseSCache(scp);
			cm_ReleaseUser(userp);
			free(realPathp);
			return CM_ERROR_EXISTS;
		}

		if (createDisp == 4
		    || createDisp == 5) {
			setAttr.mask = CM_ATTRMASK_LENGTH;
			setAttr.length.LowPart = 0;
			setAttr.length.HighPart = 0;
			code = cm_SetAttr(scp, &setAttr, userp, &req);
			openAction = 3;	/* truncated existing file */
		}
		else openAction = 1;	/* found existing file */
	}
	else if (createDisp == 1 || createDisp == 4) {
		/* don't create if not found */
		if (dscp) cm_ReleaseSCache(dscp);
		cm_ReleaseUser(userp);
		free(realPathp);
		return CM_ERROR_NOSUCHFILE;
	}
	else if (realDirFlag == 0 || realDirFlag == -1) {
		osi_assert(dscp != NULL);
		osi_Log1(afsd_logp, "smb_ReceiveNTCreateX creating file %s",
				osi_LogSaveString(afsd_logp, lastNamep));
		openAction = 2;		/* created file */
		setAttr.mask = CM_ATTRMASK_CLIENTMODTIME;
		setAttr.clientModTime = time(NULL);
		code = cm_Create(dscp, lastNamep, 0, &setAttr, &scp, userp,
				 &req);
		if (code == 0 && (dscp->flags & CM_SCACHEFLAG_ANYWATCH))
			smb_NotifyChange(FILE_ACTION_ADDED,
					 FILE_NOTIFY_CHANGE_FILE_NAME,
					 dscp, lastNamep, NULL, TRUE);
		if (code == CM_ERROR_EXISTS && createDisp != 2) {
			/* Not an exclusive create, and someone else tried
			 * creating it already, then we open it anyway.  We
			 * don't bother retrying after this, since if this next
			 * fails, that means that the file was deleted after we
			 * started this call.
			 */
			code = cm_Lookup(dscp, lastNamep, CM_FLAG_CASEFOLD,
					 userp, &req, &scp);
			if (code == 0) {
				if (createDisp == 5) {
					setAttr.mask = CM_ATTRMASK_LENGTH;
					setAttr.length.LowPart = 0;
					setAttr.length.HighPart = 0;
					code = cm_SetAttr(scp, &setAttr, userp,
							  &req);
				}
			}	/* lookup succeeded */
		}
	}
	else {
        char *tp, *pp;
        char *cp; /* This component */
        int clen = 0; /* length of component */
        cm_scache_t *tscp;
        int isLast = 0;
		
        /* create directory */
		if ( !treeCreate ) treeStartp = lastNamep;
        osi_assert(dscp != NULL);
        osi_Log1(afsd_logp, "smb_ReceiveNTCreateX creating directory [%s]",
				osi_LogSaveString(afsd_logp, treeStartp));
		openAction = 2;		/* created directory */

		setAttr.mask = CM_ATTRMASK_CLIENTMODTIME;
		setAttr.clientModTime = time(NULL);
		
		pp = treeStartp;
		cp = spacep->data;
		tscp = dscp;

		while(pp && *pp) {
			tp = strchr(pp, '\\');
			if(!tp) {
				strcpy(cp,pp);
                clen = strlen(cp);
				isLast = 1; /* indicate last component.  the supplied path never ends in a slash */
			}
			else {
				clen = tp - pp;
				strncpy(cp,pp,clen);
				*(cp + clen) = 0;
				tp++;
			}
			pp = tp;

			if(clen == 0) continue; /* the supplied path can't have consecutive slashes either , but */

			/* cp is the next component to be created. */
			code = cm_MakeDir(tscp, cp, 0, &setAttr, userp, &req);
			if (code == 0 && (tscp->flags & CM_SCACHEFLAG_ANYWATCH))
				smb_NotifyChange(FILE_ACTION_ADDED,
				FILE_NOTIFY_CHANGE_DIR_NAME,
				tscp, cp, NULL, TRUE);
			if (code == 0 || 
				(code == CM_ERROR_EXISTS && createDisp != 2)) {
					/* Not an exclusive create, and someone else tried
					* creating it already, then we open it anyway.  We
					* don't bother retrying after this, since if this next
					* fails, that means that the file was deleted after we
					* started this call.
					*/
					code = cm_Lookup(tscp, cp, CM_FLAG_CASEFOLD,
						userp, &req, &scp);
				}
			if(code) break;

			if(!isLast) { /* for anything other than dscp, release it unless it's the last one */
				cm_ReleaseSCache(tscp);
				tscp = scp; /* Newly created directory will be next parent */
			}
		}

		/* 
		if we get here and code == 0, then scp is the last directory created, and tscp is the
		parent of scp.  dscp got released if dscp != tscp. both tscp and scp are held.
		*/
		dscp = tscp;
	}

	if (code) {
		/* something went wrong creating or truncating the file */
		if (scp) cm_ReleaseSCache(scp);
        if (dscp) cm_ReleaseSCache(dscp);
		cm_ReleaseUser(userp);
		free(realPathp);
		return code;
	}

	/* make sure we have file vs. dir right (only applies for single component case) */
	if (realDirFlag == 0 && scp->fileType != CM_SCACHETYPE_FILE) {
		cm_ReleaseSCache(scp);
        if (dscp) cm_ReleaseSCache(dscp);
		cm_ReleaseUser(userp);
		free(realPathp);
		return CM_ERROR_ISDIR;
	}
    /* (only applies to single component case) */
	if (realDirFlag == 1 && scp->fileType == CM_SCACHETYPE_FILE) {
		cm_ReleaseSCache(scp);
        if (dscp) cm_ReleaseSCache(dscp);
		cm_ReleaseUser(userp);
		free(realPathp);
		return CM_ERROR_NOTDIR;
	}

	/* open the file itself */
	fidp = smb_FindFID(vcp, 0, SMB_FLAG_CREATE);
	osi_assert(fidp);
	/* save a pointer to the vnode */
	fidp->scp = scp;

	fidp->flags = fidflags;

	/* save parent dir and pathname for delete or change notification */
	if (fidflags & (SMB_FID_OPENDELETE | SMB_FID_OPENWRITE)) {
		fidp->flags |= SMB_FID_NTOPEN;
		fidp->NTopen_dscp = dscp;
		cm_HoldSCache(dscp);
		fidp->NTopen_pathp = strdup(lastNamep);
	}
	fidp->NTopen_wholepathp = realPathp;

	/* we don't need this any longer */
	if (dscp) cm_ReleaseSCache(dscp);
	cm_Open(scp, 0, userp);

	/* set inp->fid so that later read calls in same msg can find fid */
	inp->fid = fidp->fid;

	/* out parms */
	parmSlot = 2;
	lock_ObtainMutex(&scp->mx);
	smb_SetSMBParmByte(outp, parmSlot, 0);	/* oplock */
	smb_SetSMBParm(outp, parmSlot, fidp->fid); parmSlot++;
	smb_SetSMBParmLong(outp, parmSlot, openAction); parmSlot += 2;
	smb_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);
	smb_SetSMBParmDouble(outp, parmSlot, (char *)&ft); parmSlot += 4;
	smb_SetSMBParmDouble(outp, parmSlot, (char *)&ft); parmSlot += 4;
	smb_SetSMBParmDouble(outp, parmSlot, (char *)&ft); parmSlot += 4;
	smb_SetSMBParmDouble(outp, parmSlot, (char *)&ft); parmSlot += 4;
	smb_SetSMBParmLong(outp, parmSlot, smb_ExtAttributes(scp));
						parmSlot += 2;
	smb_SetSMBParmDouble(outp, parmSlot, (char *)&scp->length); parmSlot += 4;
	smb_SetSMBParmDouble(outp, parmSlot, (char *)&scp->length); parmSlot += 4;
	smb_SetSMBParm(outp, parmSlot, 0); parmSlot++;	/* filetype */
	smb_SetSMBParm(outp, parmSlot, 0); parmSlot++;	/* dev state */
	smb_SetSMBParmByte(outp, parmSlot,
		scp->fileType == CM_SCACHETYPE_DIRECTORY); /* is a dir? */
	lock_ReleaseMutex(&scp->mx);
	smb_SetSMBDataLength(outp, 0);

	osi_Log2(afsd_logp, "SMB NT CreateX opening fid %d path %s", fidp->fid,
		 osi_LogSaveString(afsd_logp, realPathp));

	smb_ReleaseFID(fidp);

	cm_ReleaseUser(userp);

    /* Can't free realPathp if we get here since fidp->NTopen_wholepathp is pointing there */

	/* leave scp held since we put it in fidp->scp */
	return 0;
}

/*
 * A lot of stuff copied verbatim from NT Create&X to NT Tran Create.
 * Instead, ultimately, would like to use a subroutine for common code.
 */
long smb_ReceiveNTTranCreate(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
	char *pathp, *realPathp;
	long code;
	cm_space_t *spacep;
	cm_user_t *userp;
	cm_scache_t *dscp;		/* parent dir */
	cm_scache_t *scp;		/* file to create or open */
	cm_attr_t setAttr;
	char *lastNamep;
	unsigned long nameLength;
	unsigned int flags;
	unsigned int requestOpLock;
	unsigned int requestBatchOpLock;
	unsigned int mustBeDir;
    unsigned int extendedRespRequired;
	int realDirFlag;
	unsigned int desiredAccess;
#ifdef DEBUG_VERBOSE    
    unsigned int allocSize;
    unsigned int shareAccess;
#endif
	unsigned int extAttributes;
	unsigned int createDisp;
#ifdef DEBUG_VERBOSE
    unsigned int sdLen;
#endif
	unsigned int createOptions;
	int initialModeBits;
	unsigned short baseFid;
	smb_fid_t *baseFidp;
	smb_fid_t *fidp;
	cm_scache_t *baseDirp;
	unsigned short openAction;
	int parmSlot;
	long fidflags;
	FILETIME ft;
	char *tidPathp;
	BOOL foundscp;
	int parmOffset, dataOffset;
	char *parmp;
	ULONG *lparmp;
	char *outData;
	cm_req_t req;

	cm_InitReq(&req);

	foundscp = FALSE;
	scp = NULL;

	parmOffset = smb_GetSMBOffsetParm(inp, 11, 1)
			| (smb_GetSMBOffsetParm(inp, 12, 1) << 16);
	parmp = inp->data + parmOffset;
	lparmp = (ULONG *) parmp;

	flags = lparmp[0];
	requestOpLock = flags & 0x02;
	requestBatchOpLock = flags & 0x04;
	mustBeDir = flags & 0x08;
    extendedRespRequired = flags & 0x10;

	/*
	 * Why all of a sudden 32-bit FID?
	 * We will reject all bits higher than 16.
	 */
	if (lparmp[1] & 0xFFFF0000)
		return CM_ERROR_INVAL;
	baseFid = (unsigned short)lparmp[1];
	desiredAccess = lparmp[2];
#ifdef DEBUG_VERBOSE
    allocSize = lparmp[3];
#endif /* DEBUG_VERSOSE */
	extAttributes = lparmp[5];
#ifdef DEBUG_VEROSE
    shareAccess = lparmp[6];
#endif
	createDisp = lparmp[7];
	createOptions = lparmp[8];
#ifdef DEBUG_VERBOSE
    sdLen = lparmp[9];
#endif
	nameLength = lparmp[11];

#ifdef DEBUG_VERBOSE
	osi_Log4(afsd_logp,"NTTransCreate with da[%x],ea[%x],sa[%x],cd[%x]",desiredAccess,extAttributes,shareAccess,createDisp);
	osi_Log2(afsd_logp,"... co[%x],sdl[%x],as[%x]",createOptions,sdLen,allocSize);
	osi_Log1(afsd_logp,"... flags[%x]",flags);
#endif

	/* mustBeDir is never set; createOptions directory bit seems to be
         * more important
	 */
	if (createOptions & 1)
		realDirFlag = 1;
	else if (createOptions & 0x40)
		realDirFlag = 0;
	else
		realDirFlag = -1;

	/*
	 * compute initial mode bits based on read-only flag in
	 * extended attributes
	 */
	initialModeBits = 0666;
	if (extAttributes & 1) initialModeBits &= ~0222;

	pathp = parmp + (13 * sizeof(ULONG)) + sizeof(UCHAR);
	/* Sometimes path is not null-terminated, so we make a copy. */
	realPathp = malloc(nameLength+1);
	memcpy(realPathp, pathp, nameLength);
	realPathp[nameLength] = 0;

	spacep = cm_GetSpace();
	smb_StripLastComponent(spacep->data, &lastNamep, realPathp);

	/*
	 * Nothing here to handle SMB_IOCTL_FILENAME.
	 * Will add it if necessary.
	 */

#ifdef DEBUG_VERBOSE
	{
		char *hexp, *asciip;
		asciip = (lastNamep? lastNamep : realPathp);
		hexp = osi_HexifyString( asciip );
		DEBUG_EVENT2("AFS", "NTTranCreate H[%s] A[%s]", hexp, asciip);
		free(hexp);
	}
#endif

	userp = smb_GetUser(vcp, inp);
    if(!userp) {
    	osi_Log1(afsd_logp, "NTTranCreate invalid user [%d]", ((smb_t *) inp)->uid);
    	free(realPathp);
    	return CM_ERROR_INVAL;
    }

	if (baseFid == 0) {
		baseDirp = cm_rootSCachep;
		tidPathp = smb_GetTIDPath(vcp, ((smb_t *)inp)->tid);
	}
	else {
        baseFidp = smb_FindFID(vcp, baseFid, 0);
        if(!baseFidp) {
        	osi_Log1(afsd_logp, "NTTranCreate Invalid fid [%d]", baseFid);
        	free(realPathp);
        	cm_ReleaseUser(userp);
        	return CM_ERROR_INVAL;
        }
		baseDirp = baseFidp->scp;
		tidPathp = NULL;
	}

    /* compute open mode */
    fidflags = 0;
    if (desiredAccess & DELETE)
        fidflags |= SMB_FID_OPENDELETE;
    if (desiredAccess & AFS_ACCESS_READ)
        fidflags |= SMB_FID_OPENREAD;
    if (desiredAccess & AFS_ACCESS_WRITE)
        fidflags |= SMB_FID_OPENWRITE;

	dscp = NULL;
	code = 0;
	code = cm_NameI(baseDirp, realPathp, CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
			userp, tidPathp, &req, &scp);
	if (code == 0) foundscp = TRUE;
	if (code != 0
	    || (fidflags & (SMB_FID_OPENDELETE | SMB_FID_OPENWRITE))) {
		/* look up parent directory */
		code = cm_NameI(baseDirp, spacep->data,
				CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
				userp, tidPathp, &req, &dscp);
		cm_FreeSpace(spacep);

		if (baseFid != 0) {
           smb_ReleaseFID(baseFidp);
           baseFidp = 0;
        }

		if (code) {
			cm_ReleaseUser(userp);
			free(realPathp);
			return code;
		}

		if (!lastNamep) lastNamep = realPathp;
		else lastNamep++;

        if (!smb_IsLegalFilename(lastNamep))
            return CM_ERROR_BADNTFILENAME;

		if (!foundscp) {
			code = cm_Lookup(dscp, lastNamep,
                             CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                             userp, &req, &scp);
			if (code && code != CM_ERROR_NOSUCHFILE) {
				cm_ReleaseSCache(dscp);
				cm_ReleaseUser(userp);
				free(realPathp);
				return code;
			}
		}
	}
	else {
		if (baseFid != 0) {
            smb_ReleaseFID(baseFidp);
            baseFidp = 0;
        }
		cm_FreeSpace(spacep);
	}

	/* if we get here, if code is 0, the file exists and is represented by
	 * scp.  Otherwise, we have to create it.  The dir may be represented
	 * by dscp, or we may have found the file directly.  If code is non-zero,
	 * scp is NULL.
	 */
	if (code == 0) {
		code = cm_CheckNTOpen(scp, desiredAccess, createDisp, userp,
				      &req);
		if (code) {
			if (dscp) cm_ReleaseSCache(dscp);
			cm_ReleaseSCache(scp);
			cm_ReleaseUser(userp);
			free(realPathp);
			return code;
		}

		if (createDisp == 2) {
			/* oops, file shouldn't be there */
			if (dscp) cm_ReleaseSCache(dscp);
			cm_ReleaseSCache(scp);
			cm_ReleaseUser(userp);
			free(realPathp);
			return CM_ERROR_EXISTS;
		}

		if (createDisp == 4
		    || createDisp == 5) {
			setAttr.mask = CM_ATTRMASK_LENGTH;
			setAttr.length.LowPart = 0;
			setAttr.length.HighPart = 0;
			code = cm_SetAttr(scp, &setAttr, userp, &req);
			openAction = 3;	/* truncated existing file */
		}
		else openAction = 1;	/* found existing file */
	}
	else if (createDisp == 1 || createDisp == 4) {
		/* don't create if not found */
		if (dscp) cm_ReleaseSCache(dscp);
		cm_ReleaseUser(userp);
		free(realPathp);
		return CM_ERROR_NOSUCHFILE;
	}
	else if (realDirFlag == 0 || realDirFlag == -1) {
		osi_assert(dscp != NULL);
		osi_Log1(afsd_logp, "smb_ReceiveNTTranCreate creating file %s",
                 osi_LogSaveString(afsd_logp, lastNamep));
		openAction = 2;		/* created file */
		setAttr.mask = CM_ATTRMASK_CLIENTMODTIME;
		setAttr.clientModTime = time(NULL);
		code = cm_Create(dscp, lastNamep, 0, &setAttr, &scp, userp,
				 &req);
		if (code == 0 && (dscp->flags & CM_SCACHEFLAG_ANYWATCH))
			smb_NotifyChange(FILE_ACTION_ADDED,
					 FILE_NOTIFY_CHANGE_FILE_NAME,
					 dscp, lastNamep, NULL, TRUE);
		if (code == CM_ERROR_EXISTS && createDisp != 2) {
			/* Not an exclusive create, and someone else tried
			 * creating it already, then we open it anyway.  We
			 * don't bother retrying after this, since if this next
			 * fails, that means that the file was deleted after we
			 * started this call.
			 */
			code = cm_Lookup(dscp, lastNamep, CM_FLAG_CASEFOLD,
					 userp, &req, &scp);
			if (code == 0) {
				if (createDisp == 5) {
					setAttr.mask = CM_ATTRMASK_LENGTH;
					setAttr.length.LowPart = 0;
					setAttr.length.HighPart = 0;
					code = cm_SetAttr(scp, &setAttr, userp,
							  &req);
				}
			}	/* lookup succeeded */
		}
	}
	else {
		/* create directory */
		osi_assert(dscp != NULL);
		osi_Log1(afsd_logp,
				"smb_ReceiveNTTranCreate creating directory %s",
				osi_LogSaveString(afsd_logp, lastNamep));
		openAction = 2;		/* created directory */
		setAttr.mask = CM_ATTRMASK_CLIENTMODTIME;
		setAttr.clientModTime = time(NULL);
		code = cm_MakeDir(dscp, lastNamep, 0, &setAttr, userp, &req);
		if (code == 0 && (dscp->flags & CM_SCACHEFLAG_ANYWATCH))
			smb_NotifyChange(FILE_ACTION_ADDED,
					 FILE_NOTIFY_CHANGE_DIR_NAME,
					 dscp, lastNamep, NULL, TRUE);
		if (code == 0
		    || (code == CM_ERROR_EXISTS && createDisp != 2)) {
			/* Not an exclusive create, and someone else tried
			 * creating it already, then we open it anyway.  We
			 * don't bother retrying after this, since if this next
			 * fails, that means that the file was deleted after we
			 * started this call.
			 */
			code = cm_Lookup(dscp, lastNamep, CM_FLAG_CASEFOLD,
					 userp, &req, &scp);
		}
	}

	if (code) {
		/* something went wrong creating or truncating the file */
		if (scp) cm_ReleaseSCache(scp);
		cm_ReleaseUser(userp);
		free(realPathp);
		return code;
	}

	/* make sure we have file vs. dir right */
	if (realDirFlag == 0 && scp->fileType != CM_SCACHETYPE_FILE) {
		cm_ReleaseSCache(scp);
		cm_ReleaseUser(userp);
		free(realPathp);
		return CM_ERROR_ISDIR;
	}
	if (realDirFlag == 1 && scp->fileType == CM_SCACHETYPE_FILE) {
		cm_ReleaseSCache(scp);
		cm_ReleaseUser(userp);
		free(realPathp);
		return CM_ERROR_NOTDIR;
	}

	/* open the file itself */
	fidp = smb_FindFID(vcp, 0, SMB_FLAG_CREATE);
	osi_assert(fidp);

	/* save a pointer to the vnode */
	fidp->scp = scp;

	fidp->flags = fidflags;

    /* save parent dir and pathname for deletion or change notification */
    if (fidflags & (SMB_FID_OPENDELETE | SMB_FID_OPENWRITE)) {
        fidp->flags |= SMB_FID_NTOPEN;
        fidp->NTopen_dscp = dscp;
        cm_HoldSCache(dscp);
        fidp->NTopen_pathp = strdup(lastNamep);
    }
	fidp->NTopen_wholepathp = realPathp;

	/* we don't need this any longer */
	if (dscp) cm_ReleaseSCache(dscp);

	cm_Open(scp, 0, userp);

	/* set inp->fid so that later read calls in same msg can find fid */
	inp->fid = fidp->fid;

    /* check whether we are required to send an extended response */
    if (!extendedRespRequired) {
        /* out parms */
        parmOffset = 8*4 + 39;
        parmOffset += 1;	/* pad to 4 */
        dataOffset = parmOffset + 70;

        parmSlot = 1;
        outp->oddByte = 1;
        /* Total Parameter Count */
        smb_SetSMBParmLong(outp, parmSlot, 70); parmSlot += 2;
        /* Total Data Count */
        smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
        /* Parameter Count */
        smb_SetSMBParmLong(outp, parmSlot, 70); parmSlot += 2;
        /* Parameter Offset */
        smb_SetSMBParmLong(outp, parmSlot, parmOffset); parmSlot += 2;
        /* Parameter Displacement */
        smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
        /* Data Count */
        smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
        /* Data Offset */
        smb_SetSMBParmLong(outp, parmSlot, dataOffset); parmSlot += 2;
        /* Data Displacement */
        smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
        smb_SetSMBParmByte(outp, parmSlot, 0);	/* Setup Count */
        smb_SetSMBDataLength(outp, 70);

        lock_ObtainMutex(&scp->mx);
        outData = smb_GetSMBData(outp, NULL);
        outData++;			/* round to get to parmOffset */
        *outData = 0; outData++;	/* oplock */
        *outData = 0; outData++;	/* reserved */
        *((USHORT *)outData) = fidp->fid; outData += 2;	/* fid */
        *((ULONG *)outData) = openAction; outData += 4;
        *((ULONG *)outData) = 0; outData += 4;	/* EA error offset */
        smb_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);
        *((FILETIME *)outData) = ft; outData += 8;	/* creation time */
        *((FILETIME *)outData) = ft; outData += 8;	/* last access time */
        *((FILETIME *)outData) = ft; outData += 8;	/* last write time */
        *((FILETIME *)outData) = ft; outData += 8;	/* change time */
        *((ULONG *)outData) = smb_ExtAttributes(scp); outData += 4;
        *((LARGE_INTEGER *)outData) = scp->length; outData += 8; /* alloc sz */
        *((LARGE_INTEGER *)outData) = scp->length; outData += 8; /* EOF */
        *((USHORT *)outData) = 0; outData += 2;	/* filetype */
        *((USHORT *)outData) = 0; outData += 2;	/* dev state */
        *((USHORT *)outData) = (scp->fileType == CM_SCACHETYPE_DIRECTORY);
        outData += 2;	/* is a dir? */
        lock_ReleaseMutex(&scp->mx);
    } else {
        /* out parms */
        parmOffset = 8*4 + 39;
        parmOffset += 1;	/* pad to 4 */
        dataOffset = parmOffset + 104;
        
        parmSlot = 1;
        outp->oddByte = 1;
        /* Total Parameter Count */
        smb_SetSMBParmLong(outp, parmSlot, 101); parmSlot += 2;
        /* Total Data Count */
        smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
        /* Parameter Count */
        smb_SetSMBParmLong(outp, parmSlot, 101); parmSlot += 2;
        /* Parameter Offset */
        smb_SetSMBParmLong(outp, parmSlot, parmOffset); parmSlot += 2;
        /* Parameter Displacement */
        smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
        /* Data Count */
        smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
        /* Data Offset */
        smb_SetSMBParmLong(outp, parmSlot, dataOffset); parmSlot += 2;
        /* Data Displacement */
        smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
        smb_SetSMBParmByte(outp, parmSlot, 0);	/* Setup Count */
        smb_SetSMBDataLength(outp, 105);
        
        lock_ObtainMutex(&scp->mx);
        outData = smb_GetSMBData(outp, NULL);
        outData++;			/* round to get to parmOffset */
        *outData = 0; outData++;	/* oplock */
        *outData = 1; outData++;	/* response type */
        *((USHORT *)outData) = fidp->fid; outData += 2;	/* fid */
        *((ULONG *)outData) = openAction; outData += 4;
        *((ULONG *)outData) = 0; outData += 4;	/* EA error offset */
        smb_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);
        *((FILETIME *)outData) = ft; outData += 8;	/* creation time */
        *((FILETIME *)outData) = ft; outData += 8;	/* last access time */
        *((FILETIME *)outData) = ft; outData += 8;	/* last write time */
        *((FILETIME *)outData) = ft; outData += 8;	/* change time */
        *((ULONG *)outData) = smb_ExtAttributes(scp); outData += 4;
        *((LARGE_INTEGER *)outData) = scp->length; outData += 8; /* alloc sz */
        *((LARGE_INTEGER *)outData) = scp->length; outData += 8; /* EOF */
        *((USHORT *)outData) = 0; outData += 2;	/* filetype */
        *((USHORT *)outData) = 0; outData += 2;	/* dev state */
        *((USHORT *)outData) = (scp->fileType == CM_SCACHETYPE_DIRECTORY);
        outData += 1;	/* is a dir? */
        memset(outData,0,24); outData += 24; /* Volume ID and file ID */
        *((ULONG *)outData) = 0x001f01ffL; outData += 4; /* Maxmimal access rights */
        *((ULONG *)outData) = 0; outData += 4; /* Guest Access rights */
        lock_ReleaseMutex(&scp->mx);
    }

	osi_Log1(afsd_logp, "SMB NTTranCreate opening fid %d", fidp->fid);

	smb_ReleaseFID(fidp);

	cm_ReleaseUser(userp);

   	/* free(realPathp); Can't free realPathp here because fidp->NTopen_wholepathp points there */
	/* leave scp held since we put it in fidp->scp */
	return 0;
}

long smb_ReceiveNTTranNotifyChange(smb_vc_t *vcp, smb_packet_t *inp,
	smb_packet_t *outp)
{
	smb_packet_t *savedPacketp;
	ULONG filter; USHORT fid, watchtree;
	smb_fid_t *fidp;
	cm_scache_t *scp;
        
	filter = smb_GetSMBParm(inp, 19)
			| (smb_GetSMBParm(inp, 20) << 16);
	fid = smb_GetSMBParm(inp, 21);
	watchtree = smb_GetSMBParm(inp, 22) && 0xffff;  /* TODO: should this be 0xff ? */

    fidp = smb_FindFID(vcp, fid, 0);
    if (!fidp) {
        osi_Log1(afsd_logp, "ERROR: NotifyChange given invalid fid [%d]", fid);
        return CM_ERROR_BADFD;
    }

	savedPacketp = smb_CopyPacket(inp);
	savedPacketp->vcp = vcp; /* TODO: refcount vcp? */
	lock_ObtainMutex(&smb_Dir_Watch_Lock);
	savedPacketp->nextp = smb_Directory_Watches;
	smb_Directory_Watches = savedPacketp;
	lock_ReleaseMutex(&smb_Dir_Watch_Lock);

    osi_Log4(afsd_logp, "Request for NotifyChange filter 0x%x fid %d wtree %d file %s",
             filter, fid, watchtree, osi_LogSaveString(afsd_logp, fidp->NTopen_wholepathp));

    scp = fidp->scp;
    lock_ObtainMutex(&scp->mx);
    if (watchtree)
        scp->flags |= CM_SCACHEFLAG_WATCHEDSUBTREE;
    else
        scp->flags |= CM_SCACHEFLAG_WATCHED;
    lock_ReleaseMutex(&scp->mx);
    smb_ReleaseFID(fidp);

	outp->flags |= SMB_PACKETFLAG_NOSEND;
	return 0;
}

unsigned char nullSecurityDesc[36] = {
	0x01,				/* security descriptor revision */
	0x00,				/* reserved, should be zero */
	0x00, 0x80,			/* security descriptor control;
					 * 0x8000 : self-relative format */
	0x14, 0x00, 0x00, 0x00,		/* offset of owner SID */
	0x1c, 0x00, 0x00, 0x00,		/* offset of group SID */
	0x00, 0x00, 0x00, 0x00,		/* offset of DACL would go here */
	0x00, 0x00, 0x00, 0x00,		/* offset of SACL would go here */
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					/* "null SID" owner SID */
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
					/* "null SID" group SID */
};

long smb_ReceiveNTTranQuerySecurityDesc(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
	int parmOffset, parmCount, dataOffset, dataCount;
	int parmSlot;
	int maxData;
	char *outData;
	char *parmp;
	USHORT *sparmp;
	ULONG *lparmp;
	USHORT fid;
	ULONG securityInformation;

	parmOffset = smb_GetSMBOffsetParm(inp, 11, 1)
			| (smb_GetSMBOffsetParm(inp, 12, 1) << 16);
	parmp = inp->data + parmOffset;
	sparmp = (USHORT *) parmp;
	lparmp = (ULONG *) parmp;

	fid = sparmp[0];
	securityInformation = lparmp[1];

	maxData = smb_GetSMBOffsetParm(inp, 7, 1)
			| (smb_GetSMBOffsetParm(inp, 8, 1) << 16);

	if (maxData < 36)
		dataCount = 0;
	else
		dataCount = 36;

	/* out parms */
	parmOffset = 8*4 + 39;
	parmOffset += 1;	/* pad to 4 */
	parmCount = 4;
	dataOffset = parmOffset + parmCount;

	parmSlot = 1;
	outp->oddByte = 1;
	/* Total Parameter Count */
	smb_SetSMBParmLong(outp, parmSlot, parmCount); parmSlot += 2;
	/* Total Data Count */
	smb_SetSMBParmLong(outp, parmSlot, dataCount); parmSlot += 2;
	/* Parameter Count */
	smb_SetSMBParmLong(outp, parmSlot, parmCount); parmSlot += 2;
	/* Parameter Offset */
	smb_SetSMBParmLong(outp, parmSlot, parmOffset); parmSlot += 2;
	/* Parameter Displacement */
	smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
	/* Data Count */
	smb_SetSMBParmLong(outp, parmSlot, dataCount); parmSlot += 2;
	/* Data Offset */
	smb_SetSMBParmLong(outp, parmSlot, dataOffset); parmSlot += 2;
	/* Data Displacement */
	smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
	smb_SetSMBParmByte(outp, parmSlot, 0);	/* Setup Count */
	smb_SetSMBDataLength(outp, 1 + parmCount + dataCount);

	outData = smb_GetSMBData(outp, NULL);
	outData++;			/* round to get to parmOffset */
	*((ULONG *)outData) = 36; outData += 4;	/* length */

	if (maxData >= 36) {
		memcpy(outData, nullSecurityDesc, 36);
		outData += 36;
		return 0;
	} else
		return CM_ERROR_BUFFERTOOSMALL;
}

long smb_ReceiveNTTransact(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
	unsigned short function;

	function = smb_GetSMBParm(inp, 18);

	osi_Log1(afsd_logp, "SMB NT Transact function %d", function);

	/* We can handle long names */
	if (vcp->flags & SMB_VCFLAG_USENT)
		((smb_t *)outp)->flg2 |= 0x40;	/* IS_LONG_NAME */
        
	switch (function) {

		case 6: return smb_ReceiveNTTranQuerySecurityDesc(vcp, inp, outp);

		case 4: return smb_ReceiveNTTranNotifyChange(vcp, inp, outp);

		case 1: return smb_ReceiveNTTranCreate(vcp, inp, outp);

		default: return CM_ERROR_INVAL;
	}
}

/*
 * smb_NotifyChange -- find relevant change notification messages and
 *		       reply to them
 *
 * If we don't know the file name (i.e. a callback break), filename is
 * NULL, and we return a zero-length list.
 */
void smb_NotifyChange(DWORD action, DWORD notifyFilter,
	cm_scache_t *dscp, char *filename, char *otherFilename,
	BOOL isDirectParent)
{
	smb_packet_t *watch, *lastWatch, *nextWatch;
	ULONG parmSlot, parmCount, parmOffset, dataOffset, nameLen;
	char *outData, *oldOutData;
	ULONG filter;
	USHORT fid, wtree;
	ULONG maxLen;
	BOOL twoEntries = FALSE;
	ULONG otherNameLen, oldParmCount = 0;
	DWORD otherAction;
	smb_vc_t *vcp;
	smb_fid_t *fidp;

	/* Get ready for rename within directory */
	if (action == FILE_ACTION_RENAMED_OLD_NAME && otherFilename != NULL) {
		twoEntries = TRUE;
		otherAction = FILE_ACTION_RENAMED_NEW_NAME;
	}

	lock_ObtainMutex(&smb_Dir_Watch_Lock);
	watch = smb_Directory_Watches;
	while (watch) {
		filter = smb_GetSMBParm(watch, 19)
				| (smb_GetSMBParm(watch, 20) << 16);
		fid = smb_GetSMBParm(watch, 21);
		wtree = smb_GetSMBParm(watch, 22) & 0xffff;  /* TODO: should this be 0xff ? */
		maxLen = smb_GetSMBOffsetParm(watch, 5, 1)
				| (smb_GetSMBOffsetParm(watch, 6, 1) << 16);
		vcp = watch->vcp;

		/*
		 * Strange hack - bug in NT Client and NT Server that we
		 * must emulate?
		 */
		if (filter == 3 && wtree)
			filter = 0x17;

		fidp = smb_FindFID(vcp, fid, 0);
        if (!fidp) {
        	lastWatch = watch;
        	watch = watch->nextp;
        	continue;
        }
		if (fidp->scp != dscp
		    || (filter & notifyFilter) == 0
		    || (!isDirectParent && !wtree)) {
			smb_ReleaseFID(fidp);
			lastWatch = watch;
			watch = watch->nextp;
			continue;
		}
		smb_ReleaseFID(fidp);

		osi_Log4(afsd_logp,
			 "Sending Change Notification for fid %d filter 0x%x wtree %d file %s",
			 fid, filter, wtree, osi_LogSaveString(afsd_logp, filename));

		nextWatch = watch->nextp;
		if (watch == smb_Directory_Watches)
			smb_Directory_Watches = nextWatch;
		else
			lastWatch->nextp = nextWatch;

		/* Turn off WATCHED flag in dscp */
		lock_ObtainMutex(&dscp->mx);
		if (wtree)
			dscp->flags &= ~CM_SCACHEFLAG_WATCHEDSUBTREE;
		else
			dscp->flags &= ~CM_SCACHEFLAG_WATCHED;
		lock_ReleaseMutex(&dscp->mx);

		/* Convert to response packet */
		((smb_t *) watch)->reb = 0x80;
		((smb_t *) watch)->wct = 0;

		/* out parms */
		if (filename == NULL)
			parmCount = 0;
		else {
			nameLen = strlen(filename);
			parmCount = 3*4 + nameLen*2;
			parmCount = (parmCount + 3) & ~3;	/* pad to 4 */
			if (twoEntries) {
				otherNameLen = strlen(otherFilename);
				oldParmCount = parmCount;
				parmCount += 3*4 + otherNameLen*2;
				parmCount = (parmCount + 3) & ~3; /* pad to 4 */
			}
			if (maxLen < parmCount)
				parmCount = 0;	/* not enough room */
		}
		parmOffset = 8*4 + 39;
		parmOffset += 1;			/* pad to 4 */
		dataOffset = parmOffset + parmCount;

		parmSlot = 1;
		watch->oddByte = 1;
		/* Total Parameter Count */
		smb_SetSMBParmLong(watch, parmSlot, parmCount); parmSlot += 2;
		/* Total Data Count */
		smb_SetSMBParmLong(watch, parmSlot, 0); parmSlot += 2;
		/* Parameter Count */
		smb_SetSMBParmLong(watch, parmSlot, parmCount); parmSlot += 2;
		/* Parameter Offset */
		smb_SetSMBParmLong(watch, parmSlot, parmOffset); parmSlot += 2;
		/* Parameter Displacement */
		smb_SetSMBParmLong(watch, parmSlot, 0); parmSlot += 2;
		/* Data Count */
		smb_SetSMBParmLong(watch, parmSlot, 0); parmSlot += 2;
		/* Data Offset */
		smb_SetSMBParmLong(watch, parmSlot, dataOffset); parmSlot += 2;
		/* Data Displacement */
		smb_SetSMBParmLong(watch, parmSlot, 0); parmSlot += 2;
		smb_SetSMBParmByte(watch, parmSlot, 0);	/* Setup Count */
		smb_SetSMBDataLength(watch, parmCount + 1);

		if (parmCount != 0) {
			outData = smb_GetSMBData(watch, NULL);
			outData++;	/* round to get to parmOffset */
			oldOutData = outData;
			*((DWORD *)outData) = oldParmCount; outData += 4;
					/* Next Entry Offset */
			*((DWORD *)outData) = action; outData += 4;
					/* Action */
			*((DWORD *)outData) = nameLen*2; outData += 4;
					/* File Name Length */
			mbstowcs((WCHAR *)outData, filename, nameLen);
					/* File Name */
			if (twoEntries) {
				outData = oldOutData + oldParmCount;
				*((DWORD *)outData) = 0; outData += 4;
					/* Next Entry Offset */
				*((DWORD *)outData) = otherAction; outData += 4;
					/* Action */
				*((DWORD *)outData) = otherNameLen*2;
				outData += 4;	/* File Name Length */
				mbstowcs((WCHAR *)outData, otherFilename,
					 otherNameLen);	/* File Name */
			}
		}

		/*
		 * If filename is null, we don't know the cause of the
		 * change notification.  We return zero data (see above),
		 * and set error code to NT_STATUS_NOTIFY_ENUM_DIR
		 * (= 0x010C).  We set the error code here by hand, without
		 * modifying wct and bcc.
		 */
		if (filename == NULL) {
			((smb_t *) watch)->rcls = 0x0C;
			((smb_t *) watch)->reh = 0x01;
			((smb_t *) watch)->errLow = 0;
			((smb_t *) watch)->errHigh = 0;
			/* Set NT Status codes flag */
			((smb_t *) watch)->flg2 |= 0x4000;
		}

		smb_SendPacket(vcp, watch);
		smb_FreePacket(watch);
		watch = nextWatch;
	}
	lock_ReleaseMutex(&smb_Dir_Watch_Lock);
}

long smb_ReceiveNTCancel(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
	unsigned char *replyWctp;
	smb_packet_t *watch, *lastWatch;
	USHORT fid, watchtree;
	smb_fid_t *fidp;
	cm_scache_t *scp;

	osi_Log0(afsd_logp, "SMB3 receive NT cancel");

	lock_ObtainMutex(&smb_Dir_Watch_Lock);
	watch = smb_Directory_Watches;
	while (watch) {
		if (((smb_t *)watch)->uid == ((smb_t *)inp)->uid
		    && ((smb_t *)watch)->pid == ((smb_t *)inp)->pid
		    && ((smb_t *)watch)->mid == ((smb_t *)inp)->mid
		    && ((smb_t *)watch)->tid == ((smb_t *)inp)->tid) {
			if (watch == smb_Directory_Watches)
				smb_Directory_Watches = watch->nextp;
			else
				lastWatch->nextp = watch->nextp;
			lock_ReleaseMutex(&smb_Dir_Watch_Lock);

			/* Turn off WATCHED flag in scp */
			fid = smb_GetSMBParm(watch, 21);
			watchtree = smb_GetSMBParm(watch, 22) & 0xffff;

			fidp = smb_FindFID(vcp, fid, 0);
            if (fidp) {
                osi_Log3(afsd_logp, "Cancelling change notification for fid %d wtree %d file %s", 
                         fid, watchtree,
                         osi_LogSaveString(afsd_logp, (fidp)?fidp->NTopen_wholepathp:""));

                scp = fidp->scp;
                lock_ObtainMutex(&scp->mx);
                if (watchtree)
                    scp->flags &= ~CM_SCACHEFLAG_WATCHEDSUBTREE;
                else
                    scp->flags &= ~CM_SCACHEFLAG_WATCHED;
                lock_ReleaseMutex(&scp->mx);
                smb_ReleaseFID(fidp);
            } else {
                osi_Log2(afsd_logp,"NTCancel unable to resolve fid [%d] in vcp[%x]", fid,vcp);
            }

			/* assume STATUS32; return 0xC0000120 (CANCELED) */
			replyWctp = watch->wctp;
			*replyWctp++ = 0;
			*replyWctp++ = 0;
			*replyWctp++ = 0;
			((smb_t *)watch)->rcls = 0x20;
			((smb_t *)watch)->reh = 0x1;
			((smb_t *)watch)->errLow = 0;
			((smb_t *)watch)->errHigh = 0xC0;
			((smb_t *)watch)->flg2 |= 0x4000;
			smb_SendPacket(vcp, watch);
			smb_FreePacket(watch);
			return 0;
		}
		lastWatch = watch;
		watch = watch->nextp;
	}
	lock_ReleaseMutex(&smb_Dir_Watch_Lock);

	return 0;
}

void smb3_Init()
{
	lock_InitializeMutex(&smb_Dir_Watch_Lock, "Directory Watch List Lock");
}

cm_user_t *smb_FindCMUserByName(/*smb_vc_t *vcp,*/ char *usern, char *machine)
{
    /*int newUid;*/
    smb_username_t *unp;

    unp = smb_FindUserByName(usern, machine, SMB_FLAG_CREATE);
    if (!unp->userp) {
        lock_ObtainMutex(&unp->mx);
        unp->userp = cm_NewUser();
        lock_ReleaseMutex(&unp->mx);
		osi_LogEvent("AFS smb_FindCMUserByName : New User",NULL,"name[%s] machine[%s]",usern,machine);
    }  else	{
		osi_LogEvent("AFS smb_FindCMUserByName : Found",NULL,"name[%s] machine[%s]",usern,machine);
	}
    return unp->userp;
}

