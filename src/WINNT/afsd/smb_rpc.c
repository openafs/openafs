/*
 * Copyright (c) 2009 Secure Endpoints Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define SMB_RPC_IMPL

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#pragma warning(push)
#pragma warning(disable: 4005)
#include <ntstatus.h>
#pragma warning(pop)
#include <stdlib.h>
#include <string.h>
#include <osi.h>

#include "afsd.h"
#include "smb.h"

#include <strsafe.h>

/**
   Meant to set up an endpoing by name, but only checks for a valid name

   We don't bother trying to locate the endpoint here because the RPC
   logic already does that using much more specific information than
   just the name.

 */
afs_int32
smb_RPC_SetupEndpointByname(smb_rpc_t * rpcp, const clientchar_t * epnamep)
{
    const char * secondary_name = NULL;

    if (!cm_ClientStrCmpI(epnamep, _C("wkssvc"))) {
	secondary_name = ".\\PIPE\\wkssvc";
    } else if (!cm_ClientStrCmpI(epnamep, _C("srvsvc"))) {
	secondary_name = ".\\PIPE\\srvsvc";
    } else {
	return CM_ERROR_NOSUCHPATH;
    }

    return MSRPC_InitConn(&rpcp->rpc_conn, secondary_name);
}

/**
   Setup a smb_fid:: structure for RPC

   \note Obtains fidp->mx */
afs_int32
smb_SetupRPCFid(smb_fid_t * fidp, const clientchar_t * _epnamep,
		unsigned short * file_type,
		unsigned short * device_state)
{
    smb_rpc_t *rpcp;
    afs_int32 code = 0;
    const clientchar_t * epnamep;

    epnamep = cm_ClientStrChr(_epnamep, _C('\\'));
    if (epnamep == NULL)
	epnamep = _epnamep;
    else
	epnamep = cm_ClientCharNext(epnamep);

    lock_ObtainMutex(&fidp->mx);
    fidp->flags |= SMB_FID_RPC;
    fidp->scp = &cm_data.fakeSCache;
    cm_HoldSCache(fidp->scp);
    if (fidp->rpcp == NULL) {
        rpcp = malloc(sizeof(*rpcp));
        memset(rpcp, 0, sizeof(*rpcp));
        fidp->rpcp = rpcp;
        rpcp->fidp = fidp;
    } else {
	rpcp = fidp->rpcp;
    }
    code = smb_RPC_SetupEndpointByname(rpcp, epnamep);
    lock_ReleaseMutex(&fidp->mx);

    if (code == 0) {
	*file_type = SMB_FILETYPE_MESSAGE_MODE_PIPE;
	*device_state =((0xff) |	/* instance count */
			SMB_DEVICESTATE_READMSGFROMPIPE |
			SMB_DEVICESTATE_MESSAGEMODEPIPE |
			SMB_DEVICESTATE_PIPECLIENTEND);
    }

    return code;
}

/**
   Cleanup a smb_fid:: structure that was used for RPC

   \note Called with fidp->mx locked */
void
smb_CleanupRPCFid(smb_fid_t * fidp)
{
    if (fidp->rpcp)
	MSRPC_FreeConn(&fidp->rpcp->rpc_conn);
}

afs_int32
smb_RPC_PrepareRead(smb_rpc_t * rpcp)
{
    return MSRPC_PrepareRead(&rpcp->rpc_conn);
}

afs_int32
smb_RPC_PrepareWrite(smb_rpc_t * rpcp)
{
    return 0;
}

afs_int32
smb_RPC_ReadPacketLength(smb_rpc_t * rpcp, afs_uint32 max_length)
{
    return MSRPC_ReadMessageLength(&rpcp->rpc_conn, max_length);
}

afs_int32
smb_RPC_ReadPacket(smb_rpc_t * rpcp, BYTE * buffer, afs_uint32 length)
{
    osi_Log1(smb_logp, "   RPC Read Packet for length %d", length);
    return MSRPC_ReadMessage(&rpcp->rpc_conn, buffer, length);
}

afs_int32
smb_RPC_WritePacket(smb_rpc_t * rpcp, BYTE * buffer, afs_uint32 length,
		    cm_user_t * userp)
{
    return MSRPC_WriteMessage(&rpcp->rpc_conn, buffer, length, userp);
}

/*! \brief Begin an RPC operation

  While generally we receive RPC requests one at a time, we have to
  protect against receiving multiple requests in parallel since
  there's nothing really preventing that from happening.

  This should be called before calling any of the smb_RPC_*()
  functions.  If the return value is non-zero, it should be considered
  unsafe to call any smb_RPC_*() function.

  Each successful call to smb_RPC_BeginOp() should be coupled with a
  call to smb_RPC_EndOp().

  \note Should be called with rpcp->fidp->mx locked.
 */
afs_int32
smb_RPC_BeginOp(smb_rpc_t * rpcp)
{
    if (rpcp == NULL)
	return CM_ERROR_INVAL;

    osi_assertx(rpcp->fidp, "No fidp assigned to smb_rpc_t");
    lock_AssertMutex(&rpcp->fidp->mx);

    while (rpcp->fidp->flags & SMB_FID_RPC_INCALL) {
	osi_SleepM((LONG_PTR) rpcp, &rpcp->fidp->mx);
	lock_ObtainMutex(&rpcp->fidp->mx);
    }

    rpcp->fidp->flags |= SMB_FID_RPC_INCALL;
    return 0;
}

/*! \brief End an RPC operation

  \see smb_RPC_BeginOp()
 */
afs_int32
smb_RPC_EndOp(smb_rpc_t * rpcp)
{
    lock_ObtainMutex(&rpcp->fidp->mx);
    osi_assertx(rpcp->fidp->flags & SMB_FID_RPC_INCALL, "RPC_EndOp() call without RPC_BeginOp()");
    rpcp->fidp->flags &= ~SMB_FID_RPC_INCALL;
    lock_ReleaseMutex(&rpcp->fidp->mx);

    osi_Wakeup((LONG_PTR) rpcp);

    return 0;
}

/**
   Handle a SMB_COM_READ for an RPC fid

   Called from smb_ReceiveCoreRead when we receive a read on the RPC
   fid */
afs_int32
smb_RPCRead(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    smb_rpc_t *rpcp;
    long count;
    char *op;
    afs_int32 code;
    cm_user_t *userp;

    count = smb_GetSMBParm(inp, 1);
    userp = smb_GetUserFromVCP(vcp, inp);

    osi_Log3(smb_logp, "smb_RPCRead for user[0x%p] fid[0x%p (%d)]",
	     userp, fidp, fidp->fid);

    lock_ObtainMutex(&fidp->mx);
    rpcp = fidp->rpcp;
    code = smb_RPC_BeginOp(rpcp);
    lock_ReleaseMutex(&fidp->mx);

    if (code) {
	cm_ReleaseUser(userp);

	return code;
    }

    code = smb_RPC_PrepareRead(rpcp);

    if (code) {
        cm_ReleaseUser(userp);

	smb_RPC_EndOp(rpcp);

        return code;
    }

    count = smb_RPC_ReadPacketLength(rpcp, count);

    /* now set the parms for a read of count bytes */
    smb_SetSMBParm(outp, 0, count);
    smb_SetSMBParm(outp, 1, 0);
    smb_SetSMBParm(outp, 2, 0);
    smb_SetSMBParm(outp, 3, 0);
    smb_SetSMBParm(outp, 4, 0);

    smb_SetSMBDataLength(outp, count+3);

    op = smb_GetSMBData(outp, NULL);
    *op++ = 1;
    *op++ = (char)(count & 0xff);
    *op++ = (char)((count >> 8) & 0xff);

    smb_RPC_ReadPacket(rpcp, op, count);

    smb_RPC_EndOp(rpcp);

    cm_ReleaseUser(userp);

    return 0;
}

/**
   Handle SMB_COM_WRITE for an RPC fid

   Called from smb_ReceiveCoreWrite when we receive a write call on
   the RPC file descriptor.
 */
afs_int32
smb_RPCWrite(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    smb_rpc_t *rpcp;
    long count;
    afs_int32 code;
    char *op;
    int inDataBlockCount;
    cm_user_t *userp = NULL;

    code = 0;
    count = smb_GetSMBParm(inp, 1);
    userp = smb_GetUserFromVCP(vcp, inp);

    osi_Log3(smb_logp, "smb_RPCWrite for user[0x%p] fid[0x%p (%d)]",
	     userp, fidp, fidp->fid);

    if (userp == NULL) {
	code = CM_ERROR_BADSMB;
	goto done;
    }

    lock_ObtainMutex(&fidp->mx);
    rpcp = fidp->rpcp;
    code = smb_RPC_BeginOp(rpcp);
    lock_ReleaseMutex(&fidp->mx);
    if (code)
	goto done;

    smb_RPC_PrepareWrite(rpcp);

    op = smb_GetSMBData(inp, NULL);
    op = smb_ParseDataBlock(op, NULL, &inDataBlockCount);

    code = smb_RPC_WritePacket(rpcp, op, count, userp);

    smb_RPC_EndOp(rpcp);

  done:
    /* return # of bytes written */
    if (code == 0) {
        smb_SetSMBParm(outp, 0, count);
        smb_SetSMBDataLength(outp, 0);
    }

    if (userp)
	cm_ReleaseUser(userp);

    return code;
}

/**
   Handle SMB_COM_WRITE_ANDX for an RPC fid

   Called from smb_ReceiveV3WriteX when we receive a write call on the
   RPC file descriptor.
 */
afs_int32
smb_RPCV3Write(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    smb_rpc_t *rpcp;
    long count;
    afs_int32 code;
    char *op;
    cm_user_t *userp = NULL;

    code = 0;
    count = smb_GetSMBParm(inp, 10);
    userp = smb_GetUserFromVCP(vcp, inp);

    osi_Log3(smb_logp, "smb_RPCV3Write for user[0x%p] fid[0x%p (%d)]",
	     userp, fidp, fidp->fid);

    if (userp == NULL) {
	code = CM_ERROR_BADSMB;
	goto done;
    }

    lock_ObtainMutex(&fidp->mx);
    rpcp = fidp->rpcp;
    code = smb_RPC_BeginOp(rpcp);
    lock_ReleaseMutex(&fidp->mx);

    if (code)
	goto done;

    smb_RPC_PrepareWrite(rpcp);

    op = inp->data + smb_GetSMBParm(inp, 11);

    code = smb_RPC_WritePacket(rpcp, op, count, userp);

    smb_RPC_EndOp(rpcp);

  done:
    /* return # of bytes written */
    if (code == 0) {
        smb_SetSMBParm(outp, 2, count);
        smb_SetSMBParm(outp, 3, 0); /* reserved */
        smb_SetSMBParm(outp, 4, 0); /* reserved */
        smb_SetSMBParm(outp, 5, 0); /* reserved */
        smb_SetSMBDataLength(outp, 0);
    }

    if (userp)
	cm_ReleaseUser(userp);

    return code;
}


/**
   Handle SMB_COM_READ_ANDX for an RPC fid

   Called from smb_ReceiveV3ReadX to handle RPC descriptor reads */
afs_int32
smb_RPCV3Read(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    smb_rpc_t *rpcp;
    long count;
    afs_int32 code;
    char *op;
    cm_user_t *userp;
    smb_user_t *uidp;

    count = smb_GetSMBParm(inp, 5);

    uidp = smb_FindUID(vcp, ((smb_t *)inp)->uid, 0);
    if (!uidp)
        return CM_ERROR_BADSMB;
    userp = smb_GetUserFromUID(uidp);
    osi_assertx(userp != NULL, "null cm_user_t");

    osi_Log3(smb_logp, "smb_RPCV3Read for user[0x%p] fid[0x%p (%d)]",
	     userp, fidp, fidp->fid);

    if (uidp && uidp->unp) {
        osi_Log3(afsd_logp, "RPC uid %d user %x name %S",
		 uidp->userID, userp,
		 osi_LogSaveClientString(afsd_logp, uidp->unp->name));
    } else {
        if (uidp)
	    osi_Log2(afsd_logp, "RPC uid %d user %x no name",
                     uidp->userID, userp);
        else
	    osi_Log1(afsd_logp, "RPC no uid user %x no name",
		     userp);
    }

    lock_ObtainMutex(&fidp->mx);
    rpcp = fidp->rpcp;
    code = smb_RPC_BeginOp(rpcp);
    lock_ReleaseMutex(&fidp->mx);

    if (code) {
	if (uidp)
	    smb_ReleaseUID(uidp);
	cm_ReleaseUser(userp);
	return code;
    }

    code = smb_RPC_PrepareRead(rpcp);
    if (uidp) {
        smb_ReleaseUID(uidp);
    }
    if (code) {
	cm_ReleaseUser(userp);
	smb_RPC_EndOp(rpcp);
	return code;
    }

    count = smb_RPC_ReadPacketLength(rpcp, count);

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

    /* get op ptr after putting in the last parm, since otherwise we don't
     * know where the data really is.
     */
    op = smb_GetSMBData(outp, NULL);

    /* now fill in offset from start of SMB header to first data byte (to op) */
    smb_SetSMBParm(outp, 6, ((int) (op - outp->data)));

    /* set the packet data length the count of the # of bytes */
    smb_SetSMBDataLength(outp, count);

    smb_RPC_ReadPacket(rpcp, op, count);

    smb_RPC_EndOp(rpcp);

    /* and cleanup things */
    cm_ReleaseUser(userp);

    return 0;
}

afs_int32
smb_RPCNmpipeTransact(smb_fid_t *fidp, smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *op)
{
    smb_tran2Packet_t *outp = NULL;
    struct smb_rpc *rpcp;
    afs_int32 code = 0;
    cm_user_t * userp = NULL;
    smb_user_t * uidp = NULL;
    int len;

    osi_Log0(smb_logp, "smb_RPCNmpipeTransact() begin");

    uidp = smb_FindUID(vcp, p->uid, 0);
    if (!uidp)
	return CM_ERROR_BADSMB;
    userp = smb_GetUserFromUID(uidp);
    osi_assertx(userp != NULL, "null cm_user_t");

    if (uidp && uidp->unp) {
        osi_Log3(afsd_logp, "RPC Transact uid %d user %x name %S",
		 uidp->userID, userp,
		 osi_LogSaveClientString(afsd_logp, uidp->unp->name));
    } else {
        if (uidp)
	    osi_Log2(afsd_logp, "RPC Transact uid %d user %x no name",
                     uidp->userID, userp);
        else
	    osi_Log1(afsd_logp, "RPC Transact no uid user %x no name",
		     userp);
    }

    lock_ObtainMutex(&fidp->mx);
    rpcp = fidp->rpcp;
    code = smb_RPC_BeginOp(rpcp);

    if (code) {
	osi_Log0(smb_logp, "Can't begin RPC op.  Aborting");
	lock_ReleaseMutex(&fidp->mx);

	smb_ReleaseUID(uidp);
	cm_ReleaseUser(userp);
	return code;
    }

    osi_assertx((fidp->flags & SMB_FID_RPC), "FID wasn't setup for RPC");
    osi_assertx(fidp->rpcp, "smb_rpc_t not associated with RPC FID");

    lock_ReleaseMutex(&fidp->mx);

    code = smb_RPC_PrepareWrite(rpcp);
    if (code)
	goto done;

    code = smb_RPC_WritePacket(rpcp, p->datap, p->totalData, userp);
    if (code)
	goto done;

    code = smb_RPC_PrepareRead(rpcp);
    if (code)
	goto done;

    len = smb_RPC_ReadPacketLength(rpcp, p->maxReturnData);

    outp = smb_GetTran2ResponsePacket(vcp, p, op, 0, len);
    if (len > 0) {
	code = smb_RPC_ReadPacket(rpcp, outp->datap, len);

	if (code == CM_ERROR_RPC_MOREDATA) {
	    outp->error_code = CM_ERROR_RPC_MOREDATA;
	}
    }

    if (code == 0 || code == CM_ERROR_RPC_MOREDATA)
	smb_SendTran2Packet(vcp, outp, op);
    smb_FreeTran2Packet(outp);

 done:
    smb_RPC_EndOp(rpcp);

    osi_Log1(smb_logp, "smb_RPCNmpipeTransact() end code=%d", code);

    if (uidp)
	smb_ReleaseUID(uidp);
    if (userp)
	cm_ReleaseUser(userp);

    return code;
}
