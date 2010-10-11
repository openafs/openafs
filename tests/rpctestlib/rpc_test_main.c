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

const char *prog = "lockharness";

#ifndef AFS_PTHREAD_ENV
#error compilation without pthreads is not supported
#endif


int
main(int argc, char **argv)
{
    int i, code;
    rpc_test_request_ctx *c1, *c2;
    AFSFetchStatus outstatus;

    printf("%s: start\n", prog);

    code = rpc_test_PkgInit();

    /* replace with appropriate test addresses */
    code = init_fs_channel(&c1, "eth0", "10.1.1.213", "24",
                           "10.1.1.81", /* fs */ RPC_TEST_REQ_CTX_FLAG_NONE);
    code = init_fs_channel(&c2, "eth0", "10.1.1.213", "24",
                           "10.1.1.81" /* fs */, RPC_TEST_REQ_CTX_FLAG_XCB);

    printf("%s: channels initialized\n", prog);

    i = 0;
repeat:
    {
        /* XXXX fid members should be...dynamic */
        AFSFid fid = { 536870915, 12, 4016};
        code = rpc_test_afs_fetch_status(c1, &fid, &outstatus);
        printf("%s: c1 call returned %d\n", prog, code);
    }

    {
        /* XXXX fid members should be...dynamic */
        AFSFid fid = { 536870915, 12, 4016};
        code = rpc_test_afs_fetch_status(c2, &fid, &outstatus);
        printf("%s: c2 call returned %d\n", prog, code);
    }

    /* force bcb at c1 */
    {
        AFSFid fid = { 536870915, 12, 4016};
        AFSStoreStatus instatus;
        instatus.Mask = 0;
        instatus.SegSize = 0;
        instatus.Owner = outstatus.Owner;
        instatus.Group = outstatus.Group;
        instatus.UnixModeBits = outstatus.UnixModeBits;
        instatus.ClientModTime = time(NULL);
        code = rpc_test_afs_store_status(c2, &fid, &instatus, &outstatus);
        printf("%s: c2 store status returned %d\n", prog, code);
    }

    /* force bcb at c2 */
    {
        AFSFid fid = { 536870915, 12, 4016};
        AFSStoreStatus instatus;
        instatus.Mask = 0;
        instatus.SegSize = 0;
        instatus.Owner = outstatus.Owner;
        instatus.Group = outstatus.Group;
        instatus.UnixModeBits = outstatus.UnixModeBits;
        instatus.ClientModTime = time(NULL);
        code = rpc_test_afs_store_status(c1, &fid, &instatus, &outstatus);
        printf("%s: c1 store status returned %d\n", prog, code);
    }
    i++;
    if (i < 10)
        goto repeat;

#if defined(AFS_BYTE_RANGE_FLOCKS)
    /* set a lock on c1 */
    {
        AFSFid fid = { 536870915, 12, 4016};
        AFSByteRangeLock lock;
        memset(&lock, 0, sizeof(AFSByteRangeLock));
        lock.Fid = fid;
        lock.Type = LockWrite;
        lock.Flags = 0;
        lock.Owner = 1001;
        lock.Uniq = 1;
        lock.Offset = 50;
        lock.Length = 100;
        code = rpc_test_afs_set_byterangelock(c1, &lock);
        printf("%s: c1 set lock returned %d\n", prog, code);
    }

    /* try to set the same lock in c2 (should FAIL) */
    {
        AFSFid fid = { 536870915, 12, 4016};
        AFSByteRangeLock lock;
        memset(&lock, 0, sizeof(AFSByteRangeLock));
        lock.Fid = fid;
        lock.Type = LockWrite;
        lock.Flags = 0;
        lock.Owner = 1002;
        lock.Uniq = 2;
        lock.Offset = 50;
        lock.Length = 100;
        code = rpc_test_afs_set_byterangelock(c2, &lock);
        printf("%s: c2 set lock returned %d\n", prog, code);
    }

    /* release lock on c1 */
    {
        AFSFid fid = { 536870915, 12, 4016};
        AFSByteRangeLock lock;
        memset(&lock, 0, sizeof(AFSByteRangeLock));
        lock.Fid = fid;
        lock.Type = LockWrite;
        lock.Flags = 0;
        lock.Owner = 1001;
        lock.Uniq = 1;
        lock.Offset = 50;
        lock.Length = 100;
        code = rpc_test_afs_release_byterangelock(c1, &lock);
        printf("%s: c1 set lock returned %d\n", prog, code);
    }
#endif /* AFS_BYTE_RANGE_FLOCKS */

    printf("%s: wait %d sec for processing\n", prog, 5);
    sleep(5);

    code = destroy_fs_channel(c1);
    code = destroy_fs_channel(c2);

    rpc_test_PkgShutdown();

    printf("%s: finish\n", prog);

    exit (0);

}        /* main */
