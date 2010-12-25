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

#ifdef AFS_NT40_ENV
#else
#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <string.h>
#include <fcntl.h>
#ifdef AFS_NT40_ENV
#include <io.h>
#include <windows.h>
#include <WINNT/afsevent.h>
#else
#include <pwd.h>
#include <afs/venus.h>
#include <sys/time.h>
#include <netdb.h>
#endif
#include <afs/afsint.h>
#define FSINT_COMMON_XG 1
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
const int ctx_key = 1;

#if 1
#define RPC_TEST_GLOBAL_RX_INIT 1
#else
#undef RPC_TEST_GLOBAL_RX_INIT
#endif

const afs_uint32 fs_port = 7000;

typedef struct rpc_test_pkg_params {
    pthread_mutex_t mtx;
    pthread_mutexattr_t mtx_attrs;
    afs_uint32 cb_next_port;
    afs_uint32 next_cno;
} rpc_test_pkg_params;
static rpc_test_pkg_params rpc_test_params;

afs_int32 rpc_test_PkgInit()
{
    afs_int32 code = 0;
    static afs_uint32 rpc_test_initialized = 0; /* once */

    if (!rpc_test_initialized) {
        rpc_test_initialized = 1;
    } else {
        printf("%s: rpc_test_PkgInit: package already initialized\n");
        exit(1);
    }

#ifndef AFS_NT40_ENV
    code = pthread_mutexattr_init(&rpc_test_params.mtx_attrs);
    if (code) {
        printf("%s: rpc_test_PkgInit: pthread_mutexattr_init failed\n", prog);
        exit(1);
    }
    code = pthread_mutex_init(&rpc_test_params.mtx, &rpc_test_params.mtx_attrs);
    if (code) {
        printf("%s: rpc_test_PkgInit: pthread_mutex_init failed\n", prog);
        exit(1);
    }
#endif

    /* start connection sequence */
    rpc_test_params.next_cno = 1;

    /* set the starting port in sequence */
    rpc_test_params.cb_next_port = 7105;

#if defined(RPC_TEST_GLOBAL_RX_INIT)
    rx_Init(0);
#endif

    return (code);

}        /* rpc_test_PkgInit */

static void *
init_callback_service_lwp(void *arg)
{
    struct rx_securityClass *sc;
    struct rx_service *svc;
    afs_int32 code = 0;

    rpc_test_request_ctx *ctx = (rpc_test_request_ctx *) arg;

    printf("%s: init_callback_service_lwp: listen_addr: %s "
	   "(%d) cb_port: %d\n",
	   prog, ctx->cb_listen_addr_s, ctx->cb_listen_addr.addr_in[0],
	   ctx->cb_port);

    sc = (struct rx_securityClass *) rxnull_NewServerSecurityObject();
    if (!sc) {
	fprintf(stderr,"rxnull_NewServerSecurityObject failed for callback "
                "service\n");
	exit(1);
    }

#if defined(RPC_TEST_GLOBAL_RX_INIT)
    svc = rx_NewServiceHost(htonl(INADDR_ANY), htons(ctx->cb_port), 1,
                            ctx->cb_svc_name, &sc, 1, RXAFSCB_ExecuteRequest);
#else
    svc = rx_NewService(0, 1, ctx->cb_svc_name, &sc, 1, RXAFSCB_ExecuteRequest);
#endif
    /* stash context */
    rx_SetServiceSpecific(svc, ctx_key, ctx);

    if (!svc) {
	fprintf(stderr,"rx_NewServiceHost failed for callback service\n");
	exit(1);
    }

    /* XXX stash service so we can hijack its rx_socket when inititiating
     * RPC calls */
    ctx->svc = svc;

    /* release pkg mutex before entering rx processing loop */
    pthread_mutex_unlock(&rpc_test_params.mtx);

    rx_StartServer(1);

    printf("%s: init_callback_service_lwp: finished");

    return (NULL);

}        /* callback_service_lwp */

afs_int32 init_callback_service(rpc_test_request_ctx *ctx)
{
    pthread_t tid;
    pthread_attr_t tattr;
    afs_int32 code = 0;

    afs_uuid_create(&(ctx->cb_listen_addr.uuid));

#if !defined(RPC_TEST_GLOBAL_RX_INIT)
#if 0
    code = rx_InitHost(ctx->cb_listen_addr.addr_in[0],
                       (int) htons(ctx->cb_port));
#else
    code = rx_Init((int) htons(ctx->cb_port));
#endif
#endif /* RPC_TEST_GLOBAL_RX_INIT */

    assert(pthread_attr_init(&tattr) == 0);
    assert(pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED) == 0);
    assert(pthread_create(&tid, &tattr, init_callback_service_lwp, ctx) == 0);

    return (code);

}        /* init_callback_service */

afs_int32 init_fs_channel(rpc_test_request_ctx **octx, char *cb_if,
                          char *listen_addr_s, char *prefix, char *fs_addr_s,
                          afs_uint32 flags)
{
    char cmd[512];
    rpc_test_request_ctx *ctx;
    afs_int32 code = 0;
#ifdef AFS_NT40_ENV
    afs_int32 sslen = sizeof(struct sockaddr);
#endif

    ctx = *octx = (rpc_test_request_ctx *) malloc(sizeof(rpc_test_request_ctx));
    memset(ctx, 0, sizeof(rpc_test_request_ctx));

    /* initialize a local mutex */
    code = pthread_mutex_init(&ctx->mtx, &rpc_test_params.mtx_attrs);

    /* lock package before rx setup--which has global deps, atm */
    pthread_mutex_lock(&rpc_test_params.mtx);

    ctx->cno = rpc_test_params.next_cno++;
    ctx->flags = flags;

    /* afscbint (server) */
    sprintf(ctx->cb_svc_name, "cb_%d", ctx->cno);
    sprintf(ctx->cb_if_s, cb_if);
    sprintf(ctx->cb_listen_addr_s, listen_addr_s);
    sprintf(ctx->cb_prefix_s, prefix);
    sprintf(ctx->fs_addr_s, fs_addr_s);

#if defined(RPC_TEST_ADD_ADDRESSES)
#if defined(AFS_LINUX26_ENV)
    sprintf(cmd, "ip addr add %s/%s dev %s label %s", listen_addr_s, prefix,
            cb_if, cb_if);
    code = system(cmd);
#endif
#endif /* RPC_TEST_ADD_ADDRESSES */

    /* lock this */
    pthread_mutex_lock(&ctx->mtx);

    /* set up rx */
    ctx->cb_port = rpc_test_params.cb_next_port++;
    ctx->cb_listen_addr.numberOfInterfaces = 1;

#ifdef AFS_NT40_ENV
    code = WSAStringToAddressA(listen_addr_s, AF_INET, NULL,
              (struct sockaddr*) &(ctx->cb_listen_addr), &sslen);
#else
    code = inet_pton(AF_INET, listen_addr_s,
                     (void*) &(ctx->cb_listen_addr.addr_in[0]));
#endif

    code = init_callback_service(ctx /* LOCKED, && rpc_test_params->mtx LOCKED */);

    /* fsint (client) */

#ifdef AFS_NT40_ENV
    code = WSAStringToAddressA(fs_addr_s, AF_INET, NULL,
              (struct sockaddr*) &(ctx->fs_addr.addr_in[0]), &sslen);
#else
    code = inet_pton(AF_INET, fs_addr_s, (void*) &(ctx->fs_addr.addr_in[0]));
#endif
    ctx->sc = rxnull_NewClientSecurityObject();
    ctx->sc_index = RX_SECIDX_NULL;
    ctx->conn = rx_NewConnection(ctx->fs_addr.addr_in[0], (int) htons(fs_port),
                                 1, ctx->sc, ctx->sc_index);

    /* unlock this */
    pthread_mutex_unlock(&ctx->mtx);

out:
    return (code);

}        /* init_fs_channel */

/* XXX use the pkg lock to protect the state of rx_socket for
 * the duration of the call, switching it out for the stashed
 * rx_socket created by rx_NewService for this channel */
#define RXCALL_WITH_SOCK(code, ctx, call) \
    do { \
        osi_socket prev_rx_socket; \
        pthread_mutex_lock(&rpc_test_params.mtx); \
        prev_rx_socket = rx_socket; \
        rx_socket = ctx->svc->socket; \
        code = call; \
        rx_socket = prev_rx_socket; \
        pthread_mutex_unlock(&rpc_test_params.mtx); \
} while(0);

afs_int32
rpc_test_afs_fetch_status(rpc_test_request_ctx *ctx, AFSFid *fid,
                              AFSFetchStatus *outstatus)
{
    struct rx_call *tcall;
    struct AFSVolSync tsync;
    struct AFSCallBack tcb;
    afs_int32 code = 0;

    RXCALL_WITH_SOCK(code, ctx,
       (RXAFS_FetchStatus(ctx->conn, fid, outstatus, &tcb, &tsync)));

    return (code);

}        /* rpc_test_afs_fetch_status */

afs_int32
rpc_test_afs_store_status(rpc_test_request_ctx *ctx, AFSFid *fid,
                    AFSStoreStatus *instatus, AFSFetchStatus *outstatus)
{
    struct rx_call *tcall;
    struct AFSVolSync tsync;
    afs_int32 code = 0;

    RXCALL_WITH_SOCK(code, ctx,
       (RXAFS_StoreStatus(ctx->conn, fid, instatus, outstatus, &tsync)));

    return (code);

}        /* rpc_test_afs_fetch_status */

#if defined(AFS_BYTE_RANGE_FLOCKS)
afs_int32 rpc_test_afs_set_byterangelock(rpc_test_request_ctx *ctx,
    AFSByteRangeLock * lock)
{
    struct rx_call *tcall;
    afs_int32 code = 0;

    RXCALL_WITH_SOCK(code, ctx,
       (RXAFS_SetByteRangeLock(ctx->conn, lock)));

    return (code);

}        /* rpc_test_afs_set_byterangelock */

afs_int32 rpc_test_afs_release_byterangelock(rpc_test_request_ctx *ctx,
    AFSByteRangeLock * lock)
{
    struct rx_call *tcall;
    afs_int32 code = 0;

    RXCALL_WITH_SOCK(code, ctx,
       (RXAFS_ReleaseByteRangeLock(ctx->conn, lock)));

    return (code);

}        /* rpc_test_afs_release_byterangelock */

afs_int32 rpc_test_afs_upgrade_byterangelock(rpc_test_request_ctx *ctx,
    AFSByteRangeLock * lock)
{
    afs_int32 code = 0;

    /* TODO:  implement */

    return (code);

}        /* rpc_test_afs_upgrade_byterangelock */

afs_int32 rpc_test_afs_downgrade_byterangelock(rpc_test_request_ctx *ctx,
    AFSByteRangeLock * Lock)
{
    afs_int32 code = 0;

    /* TODO:  implement */

    return (code);

}        /* rpc_test_afs_downgrade_byterangelock */
#endif /* AFS_BYTE_RANGE_FLOCKS */

afs_int32
destroy_fs_channel(rpc_test_request_ctx *ctx)
{
    char cmd[512];
    afs_int32 code = 0;
#if defined(RPC_TEST_ADD_ADDRESSES)
#if defined(AFS_LINUX26_ENV)
    sprintf(cmd, "ip addr del %s/%s dev %s label %s", ctx->cb_listen_addr_s,
            ctx->cb_prefix_s, ctx->cb_if_s, ctx->cb_if_s);
    code = system(cmd);
#endif
#endif /* RPC_TEST_ADD_ADDRESSES */
    assert(ctx);
    free(ctx);
    return (code);

}        /* destroy_fs_channel */

void
rpc_test_PkgShutdown()
{
    afs_int32 code = 0;

}        /* rpc_test_PkgShutdown */
