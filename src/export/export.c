/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * export -	EXPORT kernel extension
 */

/* Unsafe: conflicts with _KERNEL inclusion of headers below */
/* #include <afs/param.h> */
/* #include <afsconfig.h> */

#define _KERNEL
#include "sys/types.h"
#include "sys/user.h"
#include "sys/conf.h"
#include "sys/errno.h"
#include "sys/device.h"
#include "sys/uio.h"
#include "sys/pri.h"
#include "sys/malloc.h"
#include "sym.h"
#include "export.h"

#undef RELOC

sym_t *toc_syms;		/* symbol table                 */
int toc_nsyms;			/* # of symbols                 */
caddr_t toc_strs;		/* string table                 */
int toc_size;			/* size of toc_syms             */

/*
 * export	-	entry point to EXPORT kernel extension
 *
 * Input:
 *	cmd	-	add/delete command
 *	uiop	-	uio vector describing any config params
 */
export(cmd, uiop)
     struct uio *uiop;
{
    int err, monster;
    static once;

    err = 0;
    monster = lockl(&kernel_lock, LOCK_SHORT);

    switch (cmd) {
    case CFG_INIT:		/* add EXPORT           */
	if (err = config(uiop))
	    export_cleanup();
	break;

    case CFG_TERM:		/* remove EXPORT        */
	if (err = export_cleanup())
	    break;
	break;

    default:
	err = EINVAL;
	break;
    }

    if (monster != LOCK_NEST)
	unlockl(&kernel_lock);

    return err;
}

/*
 * config -	process configuration data
 */
config(uiop)
     struct uio *uiop;
{
    struct k_conf conf;
    struct export_nl *np;
    sym_t *sym;
    int err;

    if (err = uiomove((char *)&conf, sizeof(conf), UIO_WRITE, uiop))
	return err;

    toc_nsyms = conf.nsyms;
    toc_size = conf.symt_sz + conf.str_sz;

    if (toc_nsyms * sizeof(sym_t) != conf.symt_sz || toc_size > (1024 * 1024))
#ifdef AFS_AIX51_ENV
	return EFBIG;
#else
	return EINVAL;
#endif

    toc_syms = (sym_t *) xmalloc(toc_size, 2, kernel_heap);

    if (!toc_syms)
	return ENOMEM;

    toc_strs = (char *)&toc_syms[toc_nsyms];

    /*
     * copy in the symbol table and the string table
     */
    if (err = copyin(conf.symtab, toc_syms, conf.symt_sz)
	|| (err = copyin(conf.strtab, toc_strs, conf.str_sz))) {
	xmfree(toc_syms, kernel_heap);
	toc_syms = 0;
	return err;
    }

    /*
     * `TOC' format in kernel has offsets relocated to point directly
     * into the string table.
     */
    for (sym = toc_syms; sym < &toc_syms[toc_nsyms]; ++sym)
#ifndef __XCOFF64__
	if (sym->n_zeroes == 0)
#endif
	    sym->n_nptr = sym->n_offset + toc_strs;

    return 0;
}

/*
 * export_cleanup -	cleanup EXPORT prior to removing kernel extension
 */
export_cleanup()
{

    /*
     * get rid of the symbol table
     */
    if (toc_syms) {
	xmfree(toc_syms, kernel_heap);
	toc_syms = 0;
	toc_size = 0;
	toc_strs = 0;
    }

    return 0;
}

/*
 * import_kfunc -	import a kernel function that was mistakenly left
 *			off the exports list
 *
 * NOTE:
 *	We are assuming that the functions we are importing from the
 *	kernel really are within the kernel.  If they are actually
 *	exported from some other kernel extension (but referenced in
 *	the /unix symbol table) we are in trouble.
 */
#ifdef __XCOFF64__
u_int64 *myg_toc;
#else
u_int32 *myg_toc;
#endif

import_kfunc(struct k_func * kfp)
{
    sym_t *sym;
    int i, pri;
#if 0
    static caddr_t *g_toc;
#endif

    if (!myg_toc) {
#ifdef __XCOFF64__
	sym = sym_lookup("ktoc", 0);
#else
	sym = sym_lookup("g_toc", 0);
#endif
	if (!sym) {
	    printf("\nimport: can't ascertain kernel's TOC\n");
	    return EINVAL;
	}
	myg_toc = sym->n_value;
    }

    sym = sym_lookup(kfp->name, 0);
    if (!sym) {
	printf("\nimport: function `%s' not found\n", kfp->name);
	return EINVAL;
    }

    kfp->fdesc[0] = sym->n_value;
    kfp->fdesc[1] = *myg_toc;
    kfp->fdesc[2] = 0;

#ifdef __XCOFF64__
    *(u_int64 **) kfp->fpp = kfp->fdesc;
#else
    *(u_int **) kfp->fpp = kfp->fdesc;
#endif

    return 0;
}

/*
 * import_kvar -	import a kernel variable that was mistakenly left
 *			off the exports list
 */
import_kvar(struct k_var * kvp, caddr_t * toc)
{
    sym_t *sym;
    int i, pri;
    label_t jmpbuf;

    switch (setjmpx(&jmpbuf)) {
    case 0:
	break;

    default:
	return EINVAL;
    }

    sym = sym_lookup(kvp->name, 0);
    if (!sym) {
	printf("\nimport: variable `%s' not found\n", kvp->name);
	longjmpx(EINVAL);
    }

    /*
     * look through the caller's TOC for the reference to his surrogate
     * variable.
     */
    while (*toc != kvp->varp)
	++toc;

    printf("import(%s): replacing my TOC at 0x%x: 0x%8x with 0x%8x\n",
	   kvp->name, toc, *toc, sym->n_value);

    /*
     * replace reference to surrogate with reference real
     */
    pri = i_disable(INTMAX);
    *toc = (caddr_t) sym->n_value;
    i_enable(pri);

    clrjmpx(&jmpbuf);

    return 0;
}


/*
 * Call vanilla syscalls
 */
#ifndef AFS_AIX51_ENV
osetgroups(ngroups, gidset)
     int ngroups;
     gid_t *gidset;
{
    int error;

    error = setgroups(ngroups, gidset);
    return (error);
}
#endif

#ifdef AFS_AIX51_ENV
#ifdef AFS_64BIT_KERNEL
okioctl(fdes, cmd, arg, ext, arg2, arg3)
#else /* AFS_64BIT_KERNEL */
okioctl32(fdes, cmd, arg, ext, arg2, arg3)
#endif /* AFS_64BIT_KERNEL */
     int fdes, cmd;
     caddr_t ext, arg, arg2, arg3;
#else
okioctl(fdes, cmd, arg, ext)
     int fdes, cmd, arg;
     caddr_t ext;
#endif
{
    int error;

#ifdef AFS_AIX51_ENV
#ifdef AFS_64BIT_KERNEL
    error = kioctl(fdes, cmd, arg, ext, arg2, arg3);
#else /* AFS_64BIT_KERNEL */
    error = kioctl32(fdes, cmd, arg, ext, arg2, arg3);
#endif /* AFS_64BIT_KERNEL */
#else
    error = kioctl(fdes, cmd, arg, ext);
#endif
    return (error);
}

#ifdef	notdef
ocore(signo, sigctx)
     char signo;
     struct sigcontext *sigctx;
{
    int error;
#include <sys/user.h>
    u.u_sigflags[signo] |= SA_FULLDUMP;	/* XXX */
    error = core(signo, sigctx);
    return (error);
}
#endif
