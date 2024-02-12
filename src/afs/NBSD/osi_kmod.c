/*
 * Copyright (c) 2011 Jonathan A. Kollasch
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include "afs/param.h"

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#if !defined(AFS_NBSD60_ENV)
#include <sys/lkm.h>
#endif
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>
#include <sys/syscallvar.h>

#include "afs/sysincludes.h"    /* Standard vendor system headers */
#include "afs/afsincludes.h"    /* Afs-based standard headers */

extern int afs3_syscall(struct lwp *, const void *, register_t *);
extern int Afs_xsetgroups(struct lwp *, const void *, register_t *);

#if !defined(AFS_NBSD60_ENV)
extern int sys_lkmnosys(struct lwp *, const void *, register_t *);
#endif

extern struct vfsops afs_vfsops;

static const struct sysent openafs_sysent = {
	sizeof(struct afs_sysargs)/sizeof(register_t),
	sizeof(struct afs_sysargs),
	0,
	afs3_syscall,
};

static struct sysent old_sysent;
static sy_call_t *old_setgroups;
static sy_call_t *old_ioctl;

MODULE(MODULE_CLASS_VFS, openafs, NULL);

#if defined(AFS_NBSD70_ENV)
#define SYS_NOSYSCALL sys_nomodule
#elif defined(AFS_NBSD60_ENV)
#define SYS_NOSYSCALL sys_nosys
#else
#define SYS_NOSYSCALL sys_lkmnosys
#endif

static int
openafs_modcmd(modcmd_t cmd, void *arg)
{
	struct sysent *se;
	int error;

#ifndef RUMP
	se = sysent;
#else
	se = emul_netbsd.e_sysent;
#endif

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = vfs_attach(&afs_vfsops);
		if (error != 0)
			break;
		old_sysent = se[AFS_SYSCALL];
		old_setgroups = se[SYS_setgroups].sy_call;
		old_ioctl = se[SYS_ioctl].sy_call;

#if defined(RUMP)
		if (true) {
#else
		if (old_sysent.sy_call == SYS_NOSYSCALL) {
#endif
#if defined(AFS_NBSD60_ENV)
			kernconfig_lock();
#endif
			se[AFS_SYSCALL] = openafs_sysent;
			se[SYS_setgroups].sy_call = Afs_xsetgroups;
			se[SYS_ioctl].sy_call = afs_xioctl;
#if defined(AFS_NBSD60_ENV)
			kernconfig_unlock();
#endif
		} else {
			error = EBUSY;
		}
		if (error != 0)
			break;
		break;
	case MODULE_CMD_FINI:
#if defined(AFS_NBSD60_ENV)
		kernconfig_lock();
#endif
		se[SYS_ioctl].sy_call = old_ioctl;
		se[SYS_setgroups].sy_call = old_setgroups;
		se[AFS_SYSCALL] = old_sysent;
#if defined(AFS_NBSD60_ENV)
		kernconfig_unlock();
#endif
		error = vfs_detach(&afs_vfsops);
		if (error != 0)
			break;
		break;
#if defined(AFS_NBSD70_ENV)
	case MODULE_CMD_AUTOUNLOAD:
		error = EBUSY;
		break;
#endif
	default:
		error = ENOTTY;
		break;
	}

	return error;
}
