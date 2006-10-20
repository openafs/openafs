/* 
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */


#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <rpc.h>
#include <malloc.h>
#include "osi.h"
#include "dbrpc.h"
#include <assert.h>

long osi_maxCalls = OSI_MAXRPCCALLS;

static UUID debugType = { /* 95EEEAE0-1BD3-101B-8953-204C4F4F5020 */
	0x95EEEAE0,
	0x1BD3,
	0x101B,
	{0x89, 0x53, 0x20, 0x4C, 0x4F, 0x4F, 0x50, 0x20}
};

void dbrpc_Ping(handle_t handle)
{
	return;
}

void dbrpc_GetFormat(handle_t handle, unsigned char *namep, long region, long index,
	osi_remFormat_t *formatp, long *codep)
{
	osi_fdType_t *typep;
	osi_fdTypeFormat_t *fp;

	typep = osi_FindFDType(namep);
	if (typep == NULL) {
		*codep = OSI_DBRPC_NOENTRY;	/* no such type */
		return;
	}

	for(fp = typep->formatListp; fp; fp=fp->nextp) {
		if (region == fp->region && index == fp->index) {
			/* found the one to return */
			strncpy(formatp->label, fp->labelp, sizeof(formatp->label));
			formatp->format = fp->format;
			*codep = 0;
			return;
		}
	}

	/* if we get here, we didn't find the type, so return no such entry */
	*codep = OSI_DBRPC_EOF;
	return;
}

void dbrpc_Open(handle_t handle, unsigned char *namep, osi_remHyper_t *fnp, long *codep)
{
	osi_fd_t *fdp;

	fdp = osi_AllocFD(namep);
	if (!fdp) {
		*codep = OSI_DBRPC_NOFD;
	}
	else {
		/* wire supports 64 bits but we only use 32 */
		fnp->LowPart = fdp->fd;
		fnp->HighPart = 0;
		*codep = 0;
	}
}

void dbrpc_GetInfo(handle_t handle, osi_remHyper_t *fnp, osi_remGetInfoParms_t *parmsp,
	long *codep)
{
	osi_fd_t *fdp;

	/* always init return values */
	parmsp->icount = 0;
	parmsp->scount = 0;

	fdp = osi_FindFD(fnp->LowPart);
	if (fdp) {
		*codep = (fdp->opsp->GetInfo)(fdp, parmsp);
	}
	else *codep = OSI_DBRPC_NOFD;

	return;
}

void dbrpc_Close(handle_t handle, osi_remHyper_t *fnp, long *codep)
{
	osi_fd_t *fdp;

	fdp = osi_FindFD(fnp->LowPart);
	if (fdp) {
		*codep = osi_CloseFD(fdp);
	}
	else
		*codep = OSI_DBRPC_NOFD;
}

#ifdef notdef
long osi_CleanupRPCEntry(char *exportName)
{
	UUID_VECTOR uuidvp;
	RPC_NS_HANDLE thandle;
	UUID tuuid;
	long code;

	code = RpcNsEntryObjectInqBegin(RPC_C_NS_SYNTAX_DCE, exportName, &thandle);
	if (code != RPC_S_OK && code != RPC_S_ENTRY_NOT_FOUND) return code;
	while(1) {
		code = RpcNsEntryObjectInqNext(thandle, &tuuid);
		if (code == RPC_S_NO_MORE_MEMBERS) {
			code = 0;
			break;
		}
		else if (code != RPC_S_OK) {
			break;
		}
		uuidvp.Count = 1;
		uuidvp.Uuid[0] = &tuuid;
		code = RpcNsBindingUnexport(RPC_C_NS_SYNTAX_DCE, exportName, dbrpc_v1_0_s_ifspec,
			&uuidvp);
		if (code != RPC_S_OK && code != RPC_S_INTERFACE_NOT_FOUND) break;
	}
	RpcNsEntryObjectInqDone(&thandle);
	return code;
}
#endif /* notdef */

long osi_InitDebug(osi_uid_t *exportIDp)
{
	RPC_STATUS rpcStatus;
	RPC_BINDING_VECTOR *bindingVector;
	UUID_VECTOR uuidVector;
	static osi_once_t once;

	if (!osi_Once(&once)) return 0;

	osi_Init();

	osi_InitFD();

	/* create local socket */
	rpcStatus = RpcServerUseAllProtseqs(osi_maxCalls, (void *) 0);
	if (rpcStatus != RPC_S_OK) goto done;

	/* register sockets with runtime */
	rpcStatus = RpcServerRegisterIf(dbrpc_v1_0_s_ifspec, &debugType, NULL);
	if (rpcStatus != RPC_S_OK) goto done;

	rpcStatus = RpcObjectSetType(exportIDp, &debugType);
	if (rpcStatus != RPC_S_OK) goto done;

	rpcStatus = RpcServerInqBindings(&bindingVector);
	if (rpcStatus != RPC_S_OK) goto done;

	/* the UUID_VECTOR structure contains an array of pointers to UUIDs,
	 * amazingly enough.  Aren't those folks C programmers?  Anyway, this
	 * represents the set of objects this server supports, and we register
	 * our unique UUID so that someone who finds our name can track down our
	 * unique instance, since the endpoint mapper doesn't see the name, but
	 * only sees the interface (duplicated of course) and the object UUID.
	 */
	uuidVector.Count = 1;
	uuidVector.Uuid[0] = exportIDp;

#ifdef notdef
	/* don't use CDS any longer; too big and slow */
	rpcStatus = osi_CleanupRPCEntry(exportName);
	if (rpcStatus) goto done;
#endif /* notdef */

	rpcStatus = RpcEpRegister(dbrpc_v1_0_s_ifspec, bindingVector,
		&uuidVector, (unsigned char *) 0);
	if (rpcStatus != RPC_S_OK) goto done;

#ifdef notdef
	/* don't use CDS */
	rpcStatus = RpcNsBindingExport(RPC_C_NS_SYNTAX_DCE, exportName,
		dbrpc_v1_0_s_ifspec, bindingVector, &uuidVector);
#endif /* notdef */

	rpcStatus = RpcBindingVectorFree(&bindingVector);
	if (rpcStatus != RPC_S_OK) goto done;

#ifdef	OSISTARTRPCSERVER	/* Now done later */

	/* now start server listening with appropriate # of threads */
	rpcStatus = RpcServerListen(osi_maxCalls, osi_maxCalls, /* dontwait */1);
	if (rpcStatus != RPC_S_OK) goto done;

#endif	/* OSISTARTRPCSERVER */

	rpcStatus = 0;

done:
	osi_EndOnce(&once);
	return rpcStatus;
}
