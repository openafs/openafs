/*
 * Copyright (c) 2010, Linux Box Corporation.
 * All Rights Reserved.
 *
 * Portions Copyright (c) 2007, Hartmut Reuter,
 * RZG, Max-Planck-Institut f. Plasmaphysik.
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#include "rpc_test_procs.h"

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <afs/vice.h>
#include <afs/cmd.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>

#include <afs/com_err.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_DIRECT_H
#include <direct.h>
#endif
#ifdef AFS_DARWIN_ENV
#include <sys/malloc.h>
#else
#include <malloc.h>
#endif
#include <afs/errors.h>
#include <afs/sys_prototypes.h>
#include <rx/rx_prototypes.h>
#ifdef AFS_PTHREAD_ENV
#include <assert.h>
#endif

extern const char *prog;
extern pthread_key_t ctx_key;

#if defined(AFS_EXTENDED_CALLBACK)
#define RPC_TEST_EXTENDED_CALLBACK  1

afs_int32 SRXAFSCB_ExtendedCallBack(
    /*IN */ struct rx_call *a_call,
    /*IN */ HostIdentifier * Server,
    /*IN */ AFSXCBInvocationSeq * Invocations_Array,
    /*OUT*/ AFSExtendedCallBackRSeq * CallBack_Result_Array)
{
    rpc_test_request_ctx *ctx;

    ctx = CTX_FOR_RXCALL(a_call);

    printf("%s: SRXAFSCB_ExtendedCallBack: enter (%s)\n", prog,
        ctx->cb_svc_name);

    return (0);
};
#endif /* AFS_EXTENDED_CALLBACK */

#if defined(AFS_BYTE_RANGE_FLOCKS)
afs_int32 SRXAFSCB_AsyncIssueByteRangeLock(
	/*IN */ struct rx_call *a_call,
	/*IN */ HostIdentifier * Server,
	/*IN */ AFSByteRangeLockSeq Locks_Array)
{
    rpc_test_request_ctx *ctx = CTX_FOR_RXCALL(a_call);

    printf("%s: SRXAFSCB_AsyncIssueByteRangeLock: enter (%s)\n", prog,
        ctx->cb_svc_name);

    return (0);
}
#endif /* AFS_BYTE_RANGE_FLOCKS */

afs_int32
SRXAFSCB_CallBack(struct rx_call *a_call, AFSCBFids *Fids_Array,
		  AFSCBs *CallBack_Array)
{
    rpc_test_request_ctx *ctx = CTX_FOR_RXCALL(a_call);

    printf("%s: SRXAFSCB_CallBack: enter (%s)\n", prog,
        ctx->cb_svc_name);

    return (0);
}


afs_int32
SRXAFSCB_InitCallBackState(struct rx_call *a_call)
{
    return (0);
}


afs_int32
SRXAFSCB_Probe(struct rx_call *a_call)
{
    return (0);
}


afs_int32
SRXAFSCB_GetCE(struct rx_call *a_call,
	       afs_int32 index,
	       AFSDBCacheEntry * ce)
{
    return(0);
}


afs_int32
SRXAFSCB_GetLock(struct rx_call *a_call,
		 afs_int32 index,
		 AFSDBLock * lock)
{
    return(0);
}


afs_int32
SRXAFSCB_XStatsVersion(struct rx_call *a_call,
		       afs_int32 * versionNumberP)
{
    return(0);
}


afs_int32
SRXAFSCB_GetXStats(struct rx_call *a_call,
		   afs_int32 clientVersionNumber,
		   afs_int32 collectionNumber,
		   afs_int32 * srvVersionNumberP,
		   afs_int32 * timeP,
		   AFSCB_CollData * dataP)
{
    return(0);
}

afs_int32
SRXAFSCB_ProbeUuid(struct rx_call *a_call, afsUUID *a_uuid)
{
    rpc_test_request_ctx *ctx = CTX_FOR_RXCALL(a_call);
    if ( !afs_uuid_equal(&ctx->cb_listen_addr.uuid, a_uuid) )
        return (1);
    else
        return (0);
}


afs_int32
SRXAFSCB_WhoAreYou(struct rx_call *a_call, struct interfaceAddr *addr)
{
    return SRXAFSCB_TellMeAboutYourself(a_call, addr, NULL);
}


afs_int32
SRXAFSCB_InitCallBackState2(struct rx_call *a_call, struct interfaceAddr *
			    addr)
{
    return RXGEN_OPCODE;
}


afs_int32
SRXAFSCB_InitCallBackState3(struct rx_call *a_call, afsUUID *a_uuid)
{
    return (0);
}


afs_int32
SRXAFSCB_GetCacheConfig(struct rx_call *a_call, afs_uint32 callerVersion,
			afs_uint32 *serverVersion, afs_uint32 *configCount,
			cacheConfig *config)
{
    return RXGEN_OPCODE;
}

afs_int32
SRXAFSCB_GetLocalCell(struct rx_call *a_call, char **a_name)
{
    return RXGEN_OPCODE;
}


afs_int32
SRXAFSCB_GetCellServDB(struct rx_call *a_call, afs_int32 a_index,
		       char **a_name, serverList *a_hosts)
{
    return RXGEN_OPCODE;
}


afs_int32
SRXAFSCB_GetServerPrefs(struct rx_call *a_call, afs_int32 a_index,
			afs_int32 *a_srvr_addr, afs_int32 *a_srvr_rank)
{
    return RXGEN_OPCODE;
}


afs_int32
SRXAFSCB_TellMeAboutYourself(struct rx_call *a_call, struct interfaceAddr *
			     addr, Capabilities *capabilities)
{
    afs_int32 code;
    rpc_test_request_ctx *ctx = CTX_FOR_RXCALL(a_call);

    printf("%s: SRXAFSCB_TellMeAboutYourself: enter (%s)\n", prog,
        ctx->cb_svc_name);

    addr->numberOfInterfaces = ctx->cb_listen_addr.numberOfInterfaces;
    addr->uuid = ctx->cb_listen_addr.uuid;

    if (capabilities) {
        afs_uint32 *dataBuffP;
        afs_int32 dataBytes;

        dataBytes = 1 * sizeof(afs_uint32);
        dataBuffP = (afs_uint32 *) xdr_alloc(dataBytes);
        dataBuffP[0] = CLIENT_CAPABILITY_ERRORTRANS;
#if defined(AFS_EXTENDED_CALLBACK)
        if (ctx->flags & RPC_TEST_REQ_CTX_FLAG_XCB)
            dataBuffP[0] |= CLIENT_CAPABILITY_EXT_CALLBACK;
#endif /* AFS_EXTENDED_CALLBACK */
        capabilities->Capabilities_len = dataBytes / sizeof(afs_uint32);
        capabilities->Capabilities_val = dataBuffP;
    }

    return (0);

}        /* SRXAFSCB_TellMeAboutYourself */


afs_int32
SRXAFSCB_GetCellByNum(struct rx_call *a_call, afs_int32 a_cellnum,
		      char **a_name, serverList *a_hosts)
{
    return RXGEN_OPCODE;
}


afs_int32
SRXAFSCB_GetCE64(struct rx_call *a_call, afs_int32 a_index,
		 struct AFSDBCacheEntry64 *a_result)
{
    return RXGEN_OPCODE;
}
