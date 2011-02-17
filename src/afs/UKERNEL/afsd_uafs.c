/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * User space client glue for the afsd cache manager interface
 */

#include <afsconfig.h>
#include <afs/param.h>

#ifdef UKERNEL

#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs_usrops.h"
#include "afs/afsutil.h"
#include "afs/afs_args.h"
#include "afsd/afsd.h"

extern int call_syscall(long, long, long, long, long, long);

void
afsd_mount_afs(const char *rn, const char *mountdir)
{
    uafs_setMountDir(mountdir);
    uafs_mount();
}

void
afsd_set_rx_rtpri(void)
{
    /* noop */
}
void
afsd_set_afsd_rtpri(void)
{
    /* noop */
}

int
afsd_check_mount(const char *rn, const char *mountdir)
{
    /* libuafs could provide a callback of some kind to let the user code
     * specify a "is this mount point valid?" function, but for now there's
     * no need for it. */
    return 0;
}

int
afsd_call_syscall(long param1, long param2, long param3, long param4,
                  long param5)
{
    return call_syscall(AFSCALL_CALL, param1, param2, param3, param4, param5);
}

int
afsd_fork(int wait, afsd_callback_func cbf, void *rock)
{
    int code;
    usr_thread_t tid;
    usr_thread_create(&tid, cbf, rock);
    if (wait) {
	code = usr_thread_join(tid, NULL);
    } else {
	code = usr_thread_detach(tid);
    }
    return code;
}

int
afsd_daemon(int nochdir, int noclose)
{
    /* noop */
    return 0;
}

#endif /* UKERNEL */
