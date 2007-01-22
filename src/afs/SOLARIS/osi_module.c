/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2006-2007 Sine Nomine Associates
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics stuff */
#include "h/modctl.h"
#include "h/syscall.h"
#include <sys/kobj.h>
#include <osi/osi_includes.h>


/*
 * Solaris kernel module initialization routines
 */


int afs_sinited = 0;

extern Afs_syscall();

extern struct sysent sysent[];
#if defined(AFS_SUN57_64BIT_ENV)
extern struct sysent sysent32[];
#endif


#if !defined(LIBKTRACE)
int (*ufs_iallocp) ();
void (*ufs_iupdatp) ();
int (*ufs_igetp) ();
void (*ufs_itimes_nolockp) ();

int (*afs_orig_ioctl) (), (*afs_orig_ioctl32) ();
int (*afs_orig_setgroups) (), (*afs_orig_setgroups32) ();

#ifndef AFS_SUN510_ENV
struct ill_s *ill_g_headp = 0;
#endif

#if !defined(AFS_NONFSTRANS)
int (*nfs_rfsdisptab_v2) ();
int (*nfs_rfsdisptab_v3) ();
int (*nfs_acldisptab_v2) ();
int (*nfs_acldisptab_v3) ();

int (*nfs_checkauth) ();
#endif /* !AFS_NONFSTRANS */

extern struct vfs *afs_globalVFS;
extern int afsfstype;

extern struct fs_operation_def afs_vnodeops_template[];
#ifdef AFS_SUN510_ENV
extern struct fs_operation_def afs_vfsops_template[];
extern struct vfsops *afs_vfsopsp;
#else
extern struct vfsops Afs_vfsops;
#endif /* !AFS_SUN510_ENV */
#endif /* !LIBKTRACE */

static void *
do_mod_lookup(const char * mod, const char * sym)
{
    void * ptr;

    ptr = (void *)modlookup(mod, sym);
    if (ptr == NULL) {
        osi_Msg_console("modlookup failed for symbol '%s' in module '%s'\n",
			sym, mod);
    }

    return ptr;
}

#if !defined(LIBKTRACE)
#ifdef AFS_SUN510_ENV
afsinit(int fstype, char *dummy)
#else
afsinit(struct vfssw *vfsswp, int fstype)
#endif
{
    extern int afs_xioctl();
    extern int afs_xsetgroups();

    afs_orig_setgroups = sysent[SYS_setgroups].sy_callc;
    afs_orig_ioctl = sysent[SYS_ioctl].sy_call;
    sysent[SYS_setgroups].sy_callc = afs_xsetgroups;
    sysent[SYS_ioctl].sy_call = afs_xioctl;

#if defined(AFS_SUN57_64BIT_ENV)
    afs_orig_setgroups32 = sysent32[SYS_setgroups].sy_callc;
    afs_orig_ioctl32 = sysent32[SYS_ioctl].sy_call;
    sysent32[SYS_setgroups].sy_callc = afs_xsetgroups;
    sysent32[SYS_ioctl].sy_call = afs_xioctl;
#endif /* AFS_SUN57_64BIT_ENV */

#ifdef AFS_SUN510_ENV
    vfs_setfsops(fstype, afs_vfsops_template, &afs_vfsopsp);
    afsfstype = fstype;
    vn_make_ops("afs", afs_vnodeops_template, &afs_ops);
#else /* !AFS_SUN510_ENV */
    vfsswp->vsw_vfsops = &Afs_vfsops;
    afsfstype = fstype;
#endif /* !AFS_SUN510_ENV */


#if !defined(AFS_NONFSTRANS)
    nfs_rfsdisptab_v2 = (int (*)()) do_mod_lookup("nfssrv", "rfsdisptab_v2");
    if (nfs_rfsdisptab_v2 != NULL) {
	nfs_acldisptab_v2 = (int (*)()) do_mod_lookup("nfssrv", "acldisptab_v2");
	if (nfs_acldisptab_v2 != NULL) {
	    afs_xlatorinit_v2(nfs_rfsdisptab_v2, nfs_acldisptab_v2);
	}
    }
    nfs_rfsdisptab_v3 = (int (*)()) do_mod_lookup("nfssrv", "rfsdisptab_v3");
    if (nfs_rfsdisptab_v3 != NULL) {
	nfs_acldisptab_v3 = (int (*)()) do_mod_lookup("nfssrv", "acldisptab_v3");
	if (nfs_acldisptab_v3 != NULL) {
	    afs_xlatorinit_v3(nfs_rfsdisptab_v3, nfs_acldisptab_v3);
	}
    }

    nfs_checkauth = (int (*)()) do_mod_lookup("nfssrv", "checkauth");
#endif /* !AFS_NONFSTRANS */

    ufs_iallocp = (int (*)()) do_mod_lookup("ufs", "ufs_ialloc");
    ufs_iupdatp = (void (*)()) do_mod_lookup("ufs", "ufs_iupdat");
    ufs_igetp = (int (*)()) do_mod_lookup("ufs", "ufs_iget");
    ufs_itimes_nolockp = (void (*)()) do_mod_lookup("ufs", "ufs_itimes_nolock");

    if (!ufs_iallocp || !ufs_iupdatp || !ufs_itimes_nolockp || !ufs_igetp) {
	afs_warn("AFS to UFS mapping cannot be fully initialised\n");
    }

#if !defined(AFS_SUN510_ENV)
    ill_g_headp = (struct ill_s *) do_mod_lookup("ip", "ill_g_head");
#endif /* !AFS_SUN510_ENV */

    afs_sinited = 1;
    return 0;

}

#ifdef AFS_SUN510_ENV
static struct vfsdef_v3 afs_vfsdef = {
    VFSDEF_VERSION,
    "afs",
    afsinit,
    0
};
#else
static struct vfssw afs_vfw = {
    "afs",
    afsinit,
    &Afs_vfsops,
    0
};
#endif

extern struct mod_ops mod_fsops;

static struct modlfs afsmodlfs = {
    &mod_fsops,
    "afs filesystem",
#ifdef AFS_SUN510_ENV
    &afs_vfsdef
#else
    &afs_vfw
#endif
};
#endif /* !LIBKTRACE */


static struct sysent afssysent = {
    6,
    0,
    Afs_syscall
};

extern struct mod_ops mod_syscallops;

static struct modlsys afsmodlsys = {
    &mod_syscallops,
    "afs syscall interface",
    &afssysent
};

/** The two structures afssysent32 and afsmodlsys32 are being added
  * for supporting 32 bit syscalls. In Solaris 7 there are two system
  * tables viz. sysent ans sysent32. 32 bit applications use sysent32.
  * Since most of our user space binaries are going to be 32 bit
  * we need to attach to sysent32 also. Note that the entry into AFS
  * land still happens through Afs_syscall irrespective of whether we
  * land here from sysent or sysent32
  */

#if defined(AFS_SUN57_64BIT_ENV)
extern struct mod_ops mod_syscallops32;

static struct modlsys afsmodlsys32 = {
    &mod_syscallops32,
    "afs syscall interface(32 bit)",
    &afssysent
};
#endif


static struct modlinkage afs_modlinkage = {
    MODREV_1,
    (void *)&afsmodlsys,
#ifdef AFS_SUN57_64BIT_ENV
    (void *)&afsmodlsys32,
#endif
#if !defined(LIBKTRACE)
    (void *)&afsmodlfs,
#else
    NULL,
#endif
    NULL
};


/* inter-module dependencies */
#if !defined(LIBKTRACE)
char _depends_on[] = "drv/ip drv/udp strmod/rpcmod";
#else
char _depends_on[] = "";
#endif

#ifndef	SYSBINDFILE
#define	SYSBINDFILE	"/etc/name_to_sysnum"
#endif /* SYSBINDFILE */

/** This is the function that modload calls when loading the afs kernel
  * extensions. The solaris modload program searches for the _init
  * function in a module and calls it when modloading
  */

_init()
{
    char *sysn, *mod_getsysname();
    int code;
    extern char *sysbind;
    extern struct bind *sb_hashtab[];
    struct modctl *mp = 0;

    if (afs_sinited)
	return EBUSY;

#if !defined(LIBKTRACE)
    if ((!(mp = mod_find_by_filename("fs", "ufs"))
	 && !(mp = mod_find_by_filename(NULL, "/kernel/fs/ufs"))
	 && !(mp = mod_find_by_filename(NULL, "sys/ufs"))) || (mp
							       && !mp->
							       mod_installed))
    {
	printf
	    ("ufs module must be loaded before loading afs; use modload /kernel/fs/ufs\n");
	return (ENOSYS);
    }

#ifndef	AFS_NONFSTRANS
    if ((!(mp = mod_find_by_filename("misc", "nfssrv"))
	 && !(mp = mod_find_by_filename(NULL, NFSSRV))
	 && !(mp = mod_find_by_filename(NULL, NFSSRV_V9))) || (mp
							       && !mp->
							       mod_installed))
    {
	printf
	    ("misc/nfssrv module must be loaded before loading afs with nfs-xlator\n");
	return (ENOSYS);
    }
#endif /* !AFS_NONFSTRANS */
#endif /* !LIBKTRACE */

#if !defined(AFS_SUN58_ENV)
    /* 
     * Re-read the /etc/name_to_sysnum file to make sure afs isn't added after
     * reboot.  Ideally we would like to call modctl_read_sysbinding_file() but
     * unfortunately in Solaris 2.2 it became a local function so we have to do
     * the read_binding_file() direct call with the appropriate text file and
     * system call hashtable.  make_syscallname actually copies "afs" to the
     * proper slot entry and we also actually have to properly initialize the
     * global sysent[AFS_SYSCALL] entry!
     */
    read_binding_file(SYSBINDFILE, sb_hashtab);
    make_syscallname("afs", AFS_SYSCALL);

    if (sysent[AFS_SYSCALL].sy_call == nosys) {
	if ((sysn = mod_getsysname(AFS_SYSCALL)) != NULL) {
	    sysent[AFS_SYSCALL].sy_lock =
		(krwlock_t *) kobj_zalloc(sizeof(krwlock_t), KM_SLEEP);
	    rw_init(sysent[AFS_SYSCALL].sy_lock, "afs_syscall",
#ifdef AFS_SUN57_ENV
		    RW_DEFAULT, NULL);
#else /* !AFS_SUN57_ENV */
		    RW_DEFAULT, DEFAULT_WT);
#endif /* AFS_SUN57_ENV */
	}
    }
#endif /* !AFS_SUN58_ENV */

    osi_Init();			/* initialize global lock, etc */

    code = mod_install(&afs_modlinkage);
    return code;
}

_info(modp)
     struct modinfo *modp;
{
    int code;

    code = mod_info(&afs_modlinkage, modp);
    return code;
}

_fini()
{
    int code;

#if !defined(LIBKTRACE)
    if (afs_globalVFS)
	return EBUSY;

    if (afs_sinited) {
	sysent[SYS_setgroups].sy_callc = afs_orig_setgroups;
	sysent[SYS_ioctl].sy_call = afs_orig_ioctl;
#if defined(AFS_SUN57_64BIT_ENV)
	sysent32[SYS_setgroups].sy_callc = afs_orig_setgroups32;
	sysent32[SYS_ioctl].sy_call = afs_orig_ioctl32;
#endif
    }
#endif /* !LIBKTRACE */

    code = mod_remove(&afs_modlinkage);
    return code;
}
