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

#ifndef _RPC_TEST_PROCS_H
#define _RPC_TEST_PROCS_H

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#include <afs/afsutil.h>
#include <rx/rx.h>
#include <rx/xdr.h>
#include <afs/afs_consts.h>
#include <afs/afsint.h>
#define FSINT_COMMON_XG 1
#include <afs/afscbint.h>
#include <pthread.h>

#define RPC_TEST_REQ_CTX_FLAG_NONE         0x0000
#define RPC_TEST_REQ_CTX_FLAG_XCB          0x0001

typedef struct rpc_test_request_ctx {
    afs_uint32 flags;
    /* sync */
    pthread_mutex_t mtx;
    /* local address */
    char cb_svc_name[256];
    char cb_if_s[256];
    char cb_listen_addr_s[256];
    char cb_prefix_s[256];
    char fs_addr_s[256];
    interfaceAddr cb_listen_addr;
    interfaceAddr fs_addr;
    /* rx connection */
    afs_uint32 cno;
    afs_uint32 cb_port;
    struct rx_connection *conn;
    struct rx_securityClass *sc;
    struct rx_service *svc; /* until rx fixes client socket mgmt */
    afs_int32 sc_index;
    /* stats */
    afs_uint32 calls;
} rpc_test_request_ctx;


#define CTX_FOR_RXCALL(call) \
    (rx_GetServiceSpecific((rx_ConnectionOf(call))->service, ctx_key))

afs_int32 rpc_test_PkgInit();
void rpc_test_PkgShutdown();

/* call channel, callback RPC server multiplexing */
afs_int32 init_fs_channel(rpc_test_request_ctx **ctx /* out */, char *cb_if,
    char *listen_addr_s, char *prefix, char *fs_addr_s, afs_uint32 flags);
afs_int32 destroy_fs_channel(rpc_test_request_ctx *ctx);

/* test proc wrappers */
afs_int32 rpc_test_fetch_status();
afs_int32 rpc_test_afs_fetch_status(rpc_test_request_ctx *ctx, AFSFid *fid,
                              AFSFetchStatus *outstatus);
#if defined(AFS_BYTE_RANGE_FLOCKS) /* when will then be now?  soon. */
afs_int32 rpc_test_afs_set_byterangelock(rpc_test_request_ctx *ctx,
    AFSByteRangeLock * Lock);
afs_int32 rpc_test_afs_release_byterangelock(rpc_test_request_ctx *ctx,
    AFSByteRangeLock * Lock);
afs_int32 rpc_test_afs_upgrade_byterangelock(rpc_test_request_ctx *ctx,
    AFSByteRangeLock * Lock);
afs_int32 rpc_test_afs_downgrade_byterangelock(rpc_test_request_ctx *ctx,
    AFSByteRangeLock * Lock);
#endif /* AFS_BYTE_RANGE_FLOCKS */

afs_int32 rpc_test_afs_store_status(rpc_test_request_ctx *ctx, AFSFid *fid,
    AFSStoreStatus *instatus, AFSFetchStatus *outstatus);
afs_int32 rpc_test_afs_storedata_range(rpc_test_request_ctx *ctx, AFSFid *fid,
    void *stuff);

#endif /* _RPC_TEST_PROCS_H */
