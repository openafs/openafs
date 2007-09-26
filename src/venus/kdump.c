/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>		/* for malloc() */

/* Here be hacks. */
#ifdef AFS_LINUX24_ENV
#define __KERNEL__
#include <linux/string.h>
#define _STRING_H 1
#define _SYS_STATFS_H 1
#define _BITS_SIGCONTEXT_H 1
#undef USE_UCONTEXT
#endif

#ifdef AFS_LINUX26_ENV
/* For some reason, this doesn't get defined in linux/types.h
   if __KERNEL_STRICT_NAMES is defined. But the definition of
   struct inode uses it.
*/
#ifndef pgoff_t
#define pgoff_t unsigned long
#endif
#endif

#include <string.h>

#ifdef __linux__
#define _CFS_HEADER_
#define _AFFS_FS_I
#define _NFS_FS_I
#define _SYSV_FS_SB
#define _AFFS_FS_SB
#define _NFS_FS_SB
#define __LINUX_UFS_FS_SB_H
#define _SYSV_FS_I
#define _LINUX_CODA_FS_I
#define _LINUX_NTFS_FS_SB_H
#define _LINUX_NTFS_FS_I_H
#define _NCP_FS_SB
struct sysv_sb_info {
};
struct affs_sb_info {
};
struct ufs_sb_info {
};
struct nfs_sb_info {
};
struct nfs_inode_info {
};
struct sysv_inode_info {
};
struct coda_inode_info {
};
struct affs_inode_info {
};
struct nfs_lock_info {
};
struct ntfs_sb_info {
};
struct ntfs_inode_info {
};
struct ncp_sb_info {
};
#include <linux/types.h>
#define u32 unsigned int
#define s32 int
#define u16 unsigned short
#include <features.h>
#if __GLIBC_MINOR__ >= 2
#define _SYS_TYPES_H 1
#endif
#define __KERNEL__
#endif

/* This tells afs.h to pick up afs_args from the dest tree. */
#define KDUMP_KERNEL

/*
 * Need to include <netdb.h> before _KERNEL is defined since on IRIX 6.5
 * <netdb.h> includes <netinet/in.h>, which in turn declares inet_addr()
 * if _KERNEL is defined.  This declaration conflicts with that in
 * <arpa/inet.h>.
 */
#if	! defined(AFS_AIX_ENV)
#include <netdb.h>
#endif

/* For AFS_SGI61_ENV and a 64 bit OS, _KMEMUSER should be defined on the
 * compile line for kdump.o in the Makefile. This lets us pick up
 * app32_ptr_t from types.h when included from afs/param.h.
 */
#ifdef AFS_SGI62_ENV
#define _KERNEL 1
#endif

#ifndef	AFS_OSF_ENV
#include <sys/param.h>
#endif

#ifndef AFS_LINUX20_ENV
#include <nlist.h>
#endif

#ifdef AFS_HPUX_ENV
#include <a.out.h>
#endif

#include <afs/stds.h>
#include <sys/types.h>

#if defined(AFS_OSF_ENV)
#define	KERNEL
#define	UNIX_LOCKS
#define	_KERNEL	1
#ifdef	_KERN_LOCK_H_
#include FFFFF
#endif
#include <sys/time.h>
#include <kern/lock.h>
#include <sys/vnode.h>
#include <arch/alpha/pmap.h>

/*
 * beginning with DUX 4.0A, the system header files define the macros
 *
 * KSEG_TO_PHYS()
 * IS_KSEG_VA()
 * IS_SEG1_VA()
 *
 * to be calls to the kernel functions
 *
 * kseg_to_phys()
 * is_kseg_va()
 * is_seg1_va()
 * 
 * when _KERNEL is defined, and expressions otherwise.  Since need
 * to define _KERNEL, we redefine these kernel functions as macros
 * for the expressions that we would have gotten if _KERNEL had not
 * been defined.  Yes, this duplicates code from the header files, but
 * there's no simple way around it.
 */

#define kseg_to_phys(addr) ((vm_offset_t)(addr) - UNITY_BASE)
#define is_kseg_va(x)   (((unsigned long)(x) & SEG1_BASE) == UNITY_BASE)
#define is_seg1_va(x)   (((unsigned long)(x) & SEG1_BASE) == SEG1_BASE)

#undef	KERNEL
#undef  _KERNEL
#endif

#ifdef	AFS_SUN5_ENV /*XXXXX*/
#include <sys/t_lock.h>
struct vnode foo;
#ifdef	AFS_SUN54_ENV
#else
#ifdef	AFS_SUN52_ENV
typedef struct stat_mutex stat_mutex_t;
#define	kmutex_t		stat_mutex_t
#else
typedef struct adaptive_mutex2 adaptive_mutex2_t;
#define	kmutex_t	adaptive_mutex2_t
#endif
#endif
#endif

#ifdef AFS_SGI53_ENV
#define _KERNEL 1
#include <sys/sema.h>
#ifndef AFS_SGI62_ENV
#undef _KERNEL 1
#endif
#endif

#ifdef AFS_SGI62_ENV
#include <sys/fcntl.h>
#ifndef L_SET
#define L_SET 0
#endif
#endif

#include <sys/param.h>

#ifndef AFS_SGI64_ENV
#include <sys/user.h>
#endif

#ifndef AFS_LINUX20_ENV
#include <sys/socket.h>
#endif

#ifndef AFS_LINUX26_ENV
#include <sys/file.h>
#endif

/*
 * On SGIs, when _KERNEL is defined, <netinet/in.h> declares inet_addr()
 * in a way that conflicts with the declaration in <arpa/inet.h>.
 *
 * Here we bring in <netinet/in.h> without _KERNEL defined and restore
 * _KERNEL afterwards if needed.
 *
 * A better solution might be to straighten out which #includes are
 * sensitive to _KERNEL on SGIs....
 */
#if defined(AFS_SGI_ENV) && defined(_KERNEL)
# undef _KERNEL
# include <netinet/in.h>	/* struct in_addr */
# define _KERNEL 1
#else
# include <netinet/in.h>	/* struct in_addr */
#endif

#include <arpa/inet.h>		/* inet_ntoa() */

#if defined(AFS_SGI_ENV) || defined(AFS_OSF_ENV)
#ifdef       AFS_SGI_ENV
#include <sys/vnode.h>
#endif /* AFS_SGI_ENV */
#else
#if defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV)
#include <sys/vnode.h>
#include <sys/mount.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ffs/fs.h>
#else
#if !defined(AFS_OBSD_ENV)
#include "sys/vfs.h"
#endif
#ifdef AFS_LINUX20_ENV
#ifndef UIO_MAXIOV
#define UIO_MAXIOV 1		/* don't care */
#endif
#if __GLIBC_MINOR__ == 0
#include <iovec.h>
#endif
/*#define _TIME_H*/
/*#define _SYS_UIO_H */
#define _LINUX_SOCKET_H
#undef INT_MAX
#undef UINT_MAX
#undef LONG_MAX
#undef ULONG_MAX
#define _LINUX_TIME_H
#ifndef AFS_LINUX26_ENV
#define _LINUX_FCNTL_H
#endif
#ifdef AFS_IA64_LINUX24_ENV
#define flock64  flock
#endif /* AFS_IA64_LINUX24_ENV */
#ifdef AFS_S390_LINUX20_ENV
#define _S390_STATFS_H
#else
#ifdef AFS_SPARC64_LINUX20_ENV
#define _SPARC64_STATFS_H
#define _SPARC_STATFS_H
#else
#ifdef AFS_SPARC_LINUX20_ENV
#define _SPARC_STATFS_H
#else
#ifdef AFS_ALPHA_LINUX20_ENV
#define _ALPHA_STATFS_H
#else
#define _I386_STATFS_H
#endif /* AFS_ALPHA_LINUX20_ENV */
#endif /* AFS_SPARC_LINUX20_ENV */
#endif /* AFS_SPARC64_LINUX20_ENV */
#endif /* AFS_S390_LINUX20_ENV */
struct timezone {
    int a, b;
};
#if 0				/*ndef AFS_ALPHA_LINUX20_ENV */
typedef struct timeval {
    int tv_sec;
    int tv_usec;
} timeval_t;			/* Needed here since KERNEL defined. */
#endif /*AFS_ALPHA_LINUX20_ENV */
#if defined(WORDS_BIGENDIAN)
#define _LINUX_BYTEORDER_BIG_ENDIAN_H
#else
#define _LINUX_BYTEORDER_LITTLE_ENDIAN_H
#endif
/* Avoid problems with timer_t redefinition */
#ifndef timer_t
#define timer_t ktimer_t
#define timer_t_redefined
#endif
#ifdef AFS_LINUX26_ENV
/* For some reason, this doesn't get defined in linux/types.h
   if __KERNEL_STRICT_NAMES is defined. But the definition of
   struct inode uses it.
*/
#ifndef HAVE_SECTOR_T
/* got it from linux/types.h */
typedef unsigned long sector_t;
#endif /* HAVE_SECTOR_T */
#endif /* AFS_LINUX26_ENV */
#include <linux/version.h>
#include <linux/fs.h>
#include <osi_vfs.h>
#ifdef timer_t_redefined
#undef timer_t
#undef timer_t_redefined
#endif
#else /* AFS_LINUX20_ENV */
#ifdef AFS_HPUX110_ENV
#define  KERNEL
#define  _KERNEL 1
/* Declare following so sys/vnode.h will compile with KERNEL defined */
#define FILE FILe
typedef enum _spustate {	/* FROM /etc/conf/h/_types.h */
    SPUSTATE_NONE = 0,		/* must be 0 for proper initialization */
    SPUSTATE_IDLE,		/* spu is idle */
    SPUSTATE_USER,		/* spu is in user mode */
    SPUSTATE_SYSTEM,		/* spu is in system mode */
    SPUSTATE_UNKNOWN,		/* utility code for NEW_INTERVAL() */
    SPUSTATE_NOCHANGE		/* utility code for NEW_INTERVAL() */
} spustate_t;
#define k_off_t off_t
#include "sys/vnode.h"
#undef KERNEL
#undef _KERNEL
#else /* AFS_HPUX110_ENV */
#include "sys/vnode.h"
#endif /* else AFS_HPUX110_ENV */
#endif /* else AFS_LINUX20_ENV */
#ifdef	AFS_HPUX_ENV
#include "sys/inode.h"
#else
#ifndef	AFS_AIX_ENV
#ifdef	AFS_SUN5_ENV
#include "sys/fs/ufs_inode.h"
#else
#if !defined(AFS_LINUX20_ENV) && !defined(AFS_OBSD_ENV)
#include "ufs/inode.h"
#endif
#endif
#endif
#endif
#endif
#include <signal.h>
#endif

/* AFS includes */
#ifdef AFS_AIX41_ENV
/* This definition is in rx_machdep.h, currently only for AIX 41 */
#define RX_ENABLE_LOCKS
/* The following two defines are from rx_machdep.h and are used in rx_
 * structures.
 */
#define afs_kmutex_t int
#define afs_kcondvar_t int
#endif /* AFS_AIX41_ENV */


#ifdef AFS_SUN5_ENV

#define RX_ENABLE_LOCKS

/**
  * Removed redefinitions of afs_kmutex_t and afs_kcondvar_t and included
  * the system header files in which they are defined
  */
#include <sys/mutex.h>
#include <sys/condvar.h>
typedef kmutex_t afs_kmutex_t;
typedef kcondvar_t afs_kcondvar_t;
#endif /* AFS_SUN5_ENV */

#ifdef AFS_DUX40_ENV
#define RX_ENABLE_LOCKS
typedef struct {
    unsigned long lock;
    void *owner;
} afs_kmutex_t;
typedef int afs_kcondvar_t;
#endif /* AFS_DUX40_ENV */

#ifdef AFS_HPUX110_ENV
#define RX_ENABLE_LOCKS
typedef struct {
    void *s_lock;
    int count;
    long sa_fill1;
    void *wait_list;
    void *sa_fill2[2];
    int sa_fill2b[2];
    long sa_fill2c[3];
    int sa_fill2d[16];
    int order;
    int sa_fill3;
} afs_kmutex_t;
typedef char *afs_kcondvar_t;
#endif /* AFS_HPUX110_ENV */

#ifdef AFS_SGI65_ENV
#define RX_ENABLE_LOCKS 1
typedef struct {
    __psunsigned_t opaque1;
    void *opaque2;
} afs_kmutex_t;
typedef struct {
    __psunsigned_t opaque;
} afs_kcondvar_t;
#endif /* AFS_SGI65_ENV */

#ifdef AFS_LINUX20_ENV
#include <asm/atomic.h>
#include <asm/semaphore.h>
#define RX_ENABLE_LOCKS 1
typedef struct {
    struct semaphore opaque1;
    int opaque2;
} afs_kmutex_t;
typedef void *afs_kcondvar_t;
#endif /* AFS_LINUX20_ENV */

#ifdef AFS_OBSD_ENV
typedef struct {
    void *opaque;
} afs_kmutex_t;
#endif	/* AFS_OBSD_ENV */

#include <afs/exporter.h>
/*#include "afs/osi.h"*/

typedef struct {
    int tv_sec;
    int tv_usec;
} osi_timeval_t;		/* Needed here since KERNEL defined. */

/*#include "afs/volerrors.h"*/
#ifdef AFS_LINUX20_ENV
#define _SYS_TIME_H
#endif

#include <afs/afsint.h>
#include "vlserver/vldbint.h"
#include "afs/lock.h"

#define	KERNEL

#ifndef notdef
#define	AFS34
#define	AFS33
#define	AFS32a
#else
#define AFS32
#endif


#ifdef AFS_SGI61_ENV
extern off64_t lseek64();
#define KDUMP_SIZE_T size_t
#else /* AFS_SGI61_ENV */
#define KDUMP_SIZE_T int
#endif /* AFS_SGI61_ENV */

#include "afs/afs.h"		/* XXXX Getting it from the obj tree XXX */
#include "afs/afs_axscache.h"	/* XXXX Getting it from the obj tree XXX */
#include <afs/afs_stats.h>
#include <afs/nfsclient.h>

#include <afs/cmd.h>
#include <rx/rx.h>


#undef	KERNEL

#if defined(AFS_OSF_ENV) && !defined(v_count)
#define v_count         v_usecount
#endif

#ifdef	AFS_OSF_ENV
#define	KERNELBASE	0x80000000
#define	coreadj(x)	((int)x - KERNELBASE)
#endif

#if defined(AFS_SGI_ENV)
#define UNIX "/unix"
#else
#if	defined(AFS_HPUX100_ENV)
#define UNIX "/stand/vmunix"
#else
#ifdef	AFS_HPUX_ENV
#define UNIX  "/hp-ux"
#else
#ifdef	AFS_SUN5_ENV
#define UNIX  "/dev/ksyms"
#else
#define UNIX  "/vmunix"
#endif
#endif /* AFS_HPUX_ENV */
#endif /* AFS_HPUX100_ENV */
#endif /* AFS_SGI_ENV */

#if	defined(AFS_SUN5_ENV)
#define CORE "/dev/mem"
#else
#define CORE "/dev/kmem"
#endif

/* Forward declarations */
void print_Conns();
void print_cbHash();
void print_DindexTimes();
void print_DdvnextTbl();
void print_DdcnextTbl();
void print_DindexFlags();
void print_buffers();
void print_allocs();
void kread(int kmem, off_t loc, void *buf, KDUMP_SIZE_T len);
void print_exporter();
void print_nfsclient();
void print_unixuser();
void print_cell();
void print_server();
void print_conns();
void print_conn();
void print_volume();
void print_venusfid();
void print_vnode();
void print_vcache();
void print_dcache();
void print_bkg();
void print_vlru();
void print_dlru();
void print_callout();
void print_dnlc();
void print_global_locks();
void print_global_afs_resource();
void print_global_afs_cache();
void print_rxstats();
void print_rx();
void print_services();
#ifdef KDUMP_RX_LOCK
void print_peertable_lock();
void print_conntable_lock();
void print_calltable_lock();
#endif
void print_peertable();
void print_conntable();
void print_calltable();
void print_eventtable();
void print_upDownStats();
void print_cmperfstats();
void print_cmstats();




#ifndef AFS_KDUMP_LIB
extern struct cmd_syndesc *cmd_CreateSyntax();
#endif
int opencore();

#if	defined(AFS_HPUX_ENV) && defined(__LP64__)
#define afs_nlist nlist64
#define AFSNLIST(N, C) nlist64((N), (C))
#else /* defined(AFS_HPUX_ENV) && defined(__LP64__) */
#ifdef AFS_SGI61_ENV
#ifdef AFS_32BIT_KERNEL_ENV
#define afs_nlist nlist
#define AFSNLIST(N, C) nlist((N), (C))
#else
#define afs_nlist nlist64
#define AFSNLIST(N, C) nlist64((N), (C))
#endif /* AFS_32BIT_KERNEL_ENV */
#else /* AFS_SGI61_ENV */
#ifdef AFS_LINUX20_ENV
struct afs_nlist {
    char *n_name;
    unsigned long n_value;
};
#else /* AFS_LINUX20_ENV */
#define afs_nlist nlist
#endif /* AFS_LINUX20_ENV */
#define AFSNLIST(N, C) nlist((N), (C))
#endif /* AFS_SGI61_ENV */
#endif /* defined(AFS_HPUX_ENV) && defined(__LP64__) */

char *obj = UNIX, *core = CORE;
int kmem;

int Dcells = 0, Dusers = 0, Dservers = 0, Dconns = 0, Dvols = 0, Ddvols =
    0, mem = 0;
int Dvstats = 0, Ddstats = 0, Dnfs = 0, Dglobals = 0, Dstats = 0, Dlocks =
    0, Dall = 1;
int Dindextimes = 0, Dindexflags = 0, Dvnodes = 0, Dbuffers = 0, DCallbacks =
    0, Dallocs = 0, UserLevel = 0;
int DdvnextTbl = 0, DdcnextTbl = 0;
int Nconns = 0, Drxstats = 0, Drx = 0, Dbkg = 0, Dvlru = 0, Ddlru =
    0, Dcallout = 0;
int Ddnlc = 0;
int Dgcpags = 0;

#if	defined(AFS_SUN5_ENV)
#include <string.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/elf.h>
#include <libelf.h>
#include <sys/elf_M32.h>
#ifndef	AFS_SUN54_ENV
typedef ulong_t k_fltset_t;	/* XXXXXXXXXXX */
#endif /* !AFS_SUN54_ENV */
#include <sys/proc.h>
#include <sys/file.h>
#define	_NLIST_H		/* XXXXXXXXXXXXX */
#include <kvm.h>
kvm_t *kd;
#endif /* defined(AFS_SUN5_ENV) */

/* Pretty Printers - print real IP addresses and the like if running
 * in interpret_mode.
 */
int pretty = 1;

char *
PrintIPAddr(int addr)
{
    static char str[32];
    struct in_addr in_addr;

    if (pretty) {
	if (addr == 1) {
	    strcpy(str, "local");
	} else {
	    in_addr.s_addr = addr;
	    (void)strcpy(str, inet_ntoa(in_addr));
	}
    } else {
	(void)sprintf(str, "%x", addr);
    }
    return (char *)str;
}

#ifdef AFS_LINUX20_ENV
/* Find symbols in a live kernel. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AFS_LINUX26_ENV
#define KSYMS "/proc/kallsyms"
#else
#define KSYMS "/proc/ksyms"
#endif

/* symlist_t contains all the kernel symbols. Forcing a 64 byte array is
 * a bit wasteful, but simple.
 */
#define MAXNAMELEN 64
typedef struct symlist {
    char s_name[MAXNAMELEN];
#ifdef AFS_LINUX_64BIT_KERNEL
    unsigned long s_value;
#else
    int s_value;
#endif /* AFS_LINUX_64BIT_KERNEL */
} symlist_t;

#define KSYM_ALLOC_STEP 128
#define KSYM_ALLOC_BASE 1024
symlist_t *ksyms = NULL;
int nksyms = 0;
int availksyms = 0;

#define MAXLINE 1024

int
compare_strings(const void *a, const void *b)
{
    symlist_t *syma = (symlist_t *) a;
    symlist_t *symb = (symlist_t *) b;
    return strcmp(syma->s_name, symb->s_name);
}

/* Read in all the kernel symbols */
void
read_ksyms(void)
{
    FILE *fp;
    char line[MAXLINE];
    char *p, *q;

    if (ksyms)
	return;

    fp = fopen(KSYMS, "r");
    if (fp == NULL) {
	printf("Can't open %s, exiting.\n", KSYMS);
	exit(1);
    }

    availksyms = KSYM_ALLOC_BASE;
    ksyms = (symlist_t *) malloc(availksyms * sizeof(symlist_t));
    if (!ksyms) {
	printf("Can't malloc %d elements for symbol list.\n", availksyms);
	exit(1);
    }

    /* proc is organized as <addr> <name> <module> */
    while (fgets(line, MAXLINE, fp)) {
	if (nksyms >= availksyms) {
	    availksyms += KSYM_ALLOC_STEP;
	    ksyms =
		(symlist_t *) realloc(ksyms, availksyms * sizeof(symlist_t));
	    if (!ksyms) {
		printf("Failed to realloc %d symbols.\n", availksyms);
		exit(1);
	    }
	}
#ifdef AFS_LINUX_64BIT_KERNEL
	ksyms[nksyms].s_value = (unsigned long)strtoul(line, &p, 16);
#else
	ksyms[nksyms].s_value = (int)strtoul(line, &p, 16);
#endif /* AFS_LINUX_64BIT_KERNEL */
	p++;
#ifdef AFS_LINUX26_ENV
	/* Linux 2.6 /proc/kallsyms has a one-char symbol type
	   between address and name, so step over it and the following
	   blank.
	*/
	p += 2;
#endif
	q = strchr(p, '\t');
	if (q)
	    *q = '\0';
	if (strlen(p) >= MAXLINE) {
	    printf("Symbol '%s' too long, ignoring it.\n", p);
	    continue;
	}
	(void)strcpy(ksyms[nksyms].s_name, p);
	nksyms++;
    }

    /* Sort them in lexical order */
    qsort(ksyms, nksyms, sizeof(symlist_t), compare_strings);
}



/* find_symbol returns 0 if not found, otherwise value for symbol */
#ifdef AFS_LINUX_64BIT_KERNEL
unsigned long
#else
int
#endif /* AFS_LINUX_64BIT_KERNEL */
find_symbol(char *name)
{
    symlist_t *tmp;
    symlist_t entry;

    if (!ksyms)
	read_ksyms();

    (void)strcpy(entry.s_name, name);
    tmp =
	(symlist_t *) bsearch(&entry, ksyms, nksyms, sizeof(symlist_t),
			      compare_strings);

    return tmp ? tmp->s_value : 0;
}

/* nlist fills in values in list until a null name is found. */
int
nlist(void *notused, struct afs_nlist *nlp)
{
    for (; nlp->n_name && *nlp->n_name; nlp++)
	nlp->n_value = find_symbol(nlp->n_name);

    return 0;
}

#endif

#if	defined(AFS_SUN5_ENV)
#ifdef	_LP64
Elf64_Sym *tbl;
#else
Elf32_Sym *tbl;			/* symbol tbl */
#endif
char *tblp;			/* ptr to symbol tbl */
int scnt = 0;

#ifdef	_LP64
Elf64_Sym *
symsrch(s)
     char *s;
{
    Elf64_Sym *sp;
#else
Elf32_Sym *
symsrch(s)
     char *s;
{
    Elf32_Sym *sp;
#endif	/** _LP64 **/
    char *name;
    unsigned char type;

    for (sp = tbl; sp < &tbl[scnt]; sp++) {
#ifdef _LP64
	type = ELF64_ST_TYPE(sp->st_info);
#else
	type = ELF32_ST_TYPE(sp->st_info);
#endif	/** _LP64 **/
	if (((type == STB_LOCAL) || (type == STB_GLOBAL)
	     || (type == STB_WEAK))
	    && ((afs_uint32) sp->st_value >= 0x10000)) {
	    name = tblp + sp->st_name;
	    if (!strcmp(name, s))
		return (sp);
	}
    }
    return (0);
}

#endif /*defined(AFS_SUN5_ENV) */


#ifndef AFS_KDUMP_LIB
static
cmdproc(as, arock)
     register struct cmd_syndesc *as;
     afs_int32 arock;
{
    register afs_int32 code = 0;

    if (as->parms[0].items) {	/* -kobj */
	obj = as->parms[0].items->data;
    }
    if (as->parms[1].items) {	/* -kcore */
	core = as->parms[1].items->data;
    }
    if (as->parms[2].items) {	/* -cells */
	Dcells = 1, Dall = 0;
    }
    if (as->parms[3].items) {	/* -users */
	Dusers = 1, Dall = 0;
    }
    if (as->parms[4].items) {	/* -servers */
	Dservers = 1, Dall = 0;
    }
    if (as->parms[5].items) {	/* -conns */
	Dconns = 1, Dall = 0;
    }
    if (as->parms[6].items) {	/* -volumes */
	Dvols = 1, Dall = 0;
    }
    if (as->parms[7].items) {	/* -dvolumes */
	Ddvols = 1, Dall = 0;
    }
    if (as->parms[8].items) {	/* -vstats */
	Dvstats = 1, Dall = 0;
    }
    if (as->parms[9].items) {	/* -dstats */
	Ddstats = 1, Dall = 0;
    }
    if (as->parms[10].items) {	/* -nfstats */
	Dnfs = 1, Dall = 0;
    }
    if (as->parms[11].items) {	/* -globals */
	Dglobals = 1, Dall = 0;
    }
    if (as->parms[12].items) {	/* -stats */
	Dstats = 1, Dall = 0;
    }
    if (as->parms[13].items) {	/* -locks */
	Dlocks = 1, Dall = 0;
    }
    if (as->parms[14].items) {	/* -mem */
	mem = 1;
    }
    if (as->parms[15].items) {	/* -rxstats */
	Drxstats = 1, Dall = 0;
    }
    if (as->parms[16].items) {	/* -rx */
	Drx = 1, Dall = 0;
    }
    if (as->parms[17].items) {	/* -timestable */
	Dindextimes = 1, Dall = 0;
    }
    if (as->parms[18].items) {	/* -flagstable */
	Dindexflags = 1, Dall = 0;
    }
    if (as->parms[19].items) {	/* -cbhash */
	DCallbacks = 1, Dall = 0;
    }
    if (as->parms[20].items) {	/* -vnodes */
	Dvnodes = 1, Dall = 0;
    }
    if (as->parms[21].items) {	/* -buffers */
	Dbuffers = 1, Dall = 0;
    }
    if (as->parms[22].items) {	/* -allocedmem */
	Dallocs = 1, Dall = 0;
    }
    if (as->parms[23].items) {	/* -user */
	UserLevel = 1;
    }
    if (as->parms[24].items) {	/* -bkg */
	Dbkg = 1, Dall = 0;
    }
    if (as->parms[25].items) {	/* -vlru */
	Dvlru = 1, Dall = 0;
    }
    if (as->parms[26].items) {	/* -callout */
	Dcallout = 1, Dall = 0;
    }
    if (as->parms[27].items) {	/* -dnlc */
	Ddnlc = 1, Dall = 0;
    }
    if (as->parms[28].items) {	/* -dlru */
	Ddlru = 1, Dall = 0;
    }

    if (as->parms[29].items) {	/* -raw */
	pretty = 0;
    }

    if (as->parms[30].items) {	/* -gcpags */
	Dgcpags = 1, Dall = 0;
    }

    if (as->parms[31].items) {	/* -dhash */
	DdvnextTbl = 1, DdcnextTbl = 1, Dall = 0;
    }

    code = kdump();
    return code;
}

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char **argv;
{
    register struct cmd_syndesc *ts;
    register afs_int32 code;

#ifdef	AFS_AIX32_ENV
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
#endif

    ts = cmd_CreateSyntax(NULL, cmdproc, 0,
			  "Read internal cache manager structs");
    cmd_AddParm(ts, "-kobj", CMD_SINGLE, CMD_OPTIONAL,
		"kernel object (default /vmunix)");
    cmd_AddParm(ts, "-kcore", CMD_SINGLE, CMD_OPTIONAL,
		"kernel core image (default /dev/kmem)");
    cmd_AddParm(ts, "-cells", CMD_FLAG, CMD_OPTIONAL, "cell state");
    cmd_AddParm(ts, "-users", CMD_FLAG, CMD_OPTIONAL, "users state");
    cmd_AddParm(ts, "-servers", CMD_FLAG, CMD_OPTIONAL, "servers state");
    cmd_AddParm(ts, "-conns", CMD_FLAG, CMD_OPTIONAL, "conns state");
    cmd_AddParm(ts, "-volumes", CMD_FLAG, CMD_OPTIONAL,
		"incore volume state");
    cmd_AddParm(ts, "-dvolumes", CMD_FLAG, CMD_OPTIONAL, "disk volume state");
    cmd_AddParm(ts, "-vstats", CMD_FLAG, CMD_OPTIONAL, "stat file state");
    cmd_AddParm(ts, "-dstats", CMD_FLAG, CMD_OPTIONAL, "file data state");
    cmd_AddParm(ts, "-nfstats", CMD_FLAG, CMD_OPTIONAL,
		"nfs translator state");
    cmd_AddParm(ts, "-globals", CMD_FLAG, CMD_OPTIONAL,
		"general global state");
    cmd_AddParm(ts, "-stats", CMD_FLAG, CMD_OPTIONAL,
		"general cm performance state");
    cmd_AddParm(ts, "-locks", CMD_FLAG, CMD_OPTIONAL,
		"global cm related locks state");
    cmd_AddParm(ts, "-mem", CMD_FLAG, CMD_OPTIONAL,
		"core represents the physical mem (i.e. /dev/mem) and not virtual");
    cmd_AddParm(ts, "-rxstats", CMD_FLAG, CMD_OPTIONAL,
		"general rx statistics");
    cmd_AddParm(ts, "-rx", CMD_FLAG, CMD_OPTIONAL, "all info about rx");
    cmd_AddParm(ts, "-timestable", CMD_FLAG, CMD_OPTIONAL,
		"dcache LRU info table");
    cmd_AddParm(ts, "-flagstable", CMD_FLAG, CMD_OPTIONAL,
		"dcache flags info table");
    cmd_AddParm(ts, "-cbhash", CMD_FLAG, CMD_OPTIONAL,
		"vcache hashed by cbExpires");
    cmd_AddParm(ts, "-vnodes", CMD_FLAG, CMD_OPTIONAL, "afs vnodes");
    cmd_AddParm(ts, "-buffers", CMD_FLAG, CMD_OPTIONAL,
		"afs dir buffer cache");
    cmd_AddParm(ts, "-allocedmem", CMD_FLAG, CMD_OPTIONAL,
		"allocated memory");
    cmd_AddParm(ts, "-user", CMD_FLAG, CMD_OPTIONAL,
		"core is from a user-level program");
    cmd_AddParm(ts, "-bkg", CMD_FLAG, CMD_OPTIONAL, "background daemon info");
    cmd_AddParm(ts, "-vlru", CMD_FLAG, CMD_OPTIONAL, "vcache lru list");
    cmd_AddParm(ts, "-callout", CMD_FLAG, CMD_OPTIONAL,
		"callout info (aix only)");
    cmd_AddParm(ts, "-dnlc", CMD_FLAG, CMD_OPTIONAL,
		"DNLC table,freelist,trace");
    cmd_AddParm(ts, "-dlru", CMD_FLAG, CMD_OPTIONAL, "dcache lru list");


    cmd_AddParm(ts, "-raw", CMD_FLAG, CMD_OPTIONAL, "show raw values");
    cmd_AddParm(ts, "-gcpags", CMD_FLAG, CMD_OPTIONAL,
		"PAG garbage collection info");
    cmd_AddParm(ts, "-dhash", CMD_FLAG, CMD_OPTIONAL,
		"show dcache hash chains");

    code = cmd_Dispatch(argc, argv);
    return code;
}
#endif /* !AFS_KDUMP_LIB */

#ifdef	AFS_AIX_ENV
#ifndef AFS_KDUMP_LIB
Knlist(sp, cnt, size)
     struct afs_nlist *sp;
     int cnt, size;
{
    register int code;

    if (UserLevel)
	code = nlist(obj, sp);
    else
	code = knlist(sp, cnt, size);
    return code;
}
#endif /*AFS_KDUMP_LIB */
#endif

#if !defined(AFS_DARWIN_ENV) && !defined(AFS_FBSD_ENV)
int
findsym(char *sname, off_t * offset)
{
#if	defined(AFS_SUN5_ENV)
#ifdef	_LP64
    Elf64_Sym *ss_ans;
#else
    Elf32_Sym *ss_ans;
#endif
    ss_ans = symsrch(sname);
    if (!ss_ans) {
	printf("(WARNING) Couldn't find %s in %s. Proceeding..\n", sname,
	       obj);
	*offset = 0;
	return 0;
    }
    *offset = ss_ans->st_value;
    return 1;
#else /* defined(AFS_SUN5_ENV) */
#if	defined(AFS_AIX_ENV)
    if (!UserLevel) {
	struct afs_nlist nl;
	nl.n_name = sname;
	if (Knlist(&nl, 1, sizeof nl) == -1) {
	    printf("(WARNING) knlist: couldn't find %s. Proceeding...",
		   sname);
	    *offset = 0;
	    return 0;
	}
	*offset = nl.n_value;
	return 1;
    }
#endif /* defined(AFS_AIX_ENV) */
    {
	struct afs_nlist request[2];

	memset(request, 0, sizeof request);
	request[0].n_name = sname;
	if (AFSNLIST(obj, request) < 0) {
	    fprintf(stderr, "nlist(%s, %s) failure: %d (%s)\n", obj, sname,
		    errno, strerror(errno));
	    exit(1);
	}
#if	defined(AFS_OSF_ENV)
	if (mem) {
	    long X;

	    X = coreadj(request[0].n_value);
	    request[0].n_value = X;
	}
#endif /* defined(AFS_OSF_ENV) */

	*offset = request[0].n_value;
	if (!request[0].n_value) {
	    printf("(WARNING) Couldn't find %s in %s. Proceeding..\n", sname,
		   obj);
	    return 0;
	}
	return 1;
    }
#endif /* defined(AFS_SUN5_ENV) */
}
#endif

#define CBHTSIZE 128

kdump()
{
    int cell, cnt, cnt1;
#if defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV)
    printf("Kdump not supported\n");
#else
#ifndef AFS_KDUMP_LIB

    kmem = opencore(core);

#endif /* AFS_KDUMP_LIB */

#ifdef KDUMP_RX_LOCK
    /* Test to see if kernel is using RX_ENABLE_LOCKS in rx structs. */
#ifdef AFS_SGI53_ENV
#ifdef AFS_SGI64_ENV
    use_rx_lock = 1;		/* Always using fine gain locking. */
#else
    use_rx_lock = (sysmp(MP_NPROCS) > 1) ? 1 : 0;
#endif
#endif /* AFS_SGI53_ENV */
#endif /* KDUMP_RX_LOCK */

    if (Dcells || Dall) {
	print_cells(1);		/* Handle the afs_cells structures */
	print_cellaliases(1);
	print_cellnames(1);
    }

    if (Dusers || Dall) {
	print_users(1);		/* Handle the afs_users structs */
    }

    if (Dservers || Dall) {
	print_servers(1);	/* Handle the afs_servers structs */
    }

    if (Dconns) {
	print_Conns(1);		/* Handle the afs_servers structs */
    }

    if (Dvols || Dall) {
	print_volumes(1);	/* Handle the afs_volumes structs */
    }

    if (Ddvols || Dall) {
	printf
	    ("\n\nIGNORE reading the 'volumeinfo' file for now (NOT IMPORTANT)!\n");
    }

    if (DCallbacks || Dall) {
	print_cbHash(1);	/* Handle the cbHashT table of queued vcaches */
    }

    if (Dvstats || Dall || Dvnodes) {
	print_vcaches(1);	/* Handle the afs_vcaches structs */
    }

    if (Ddstats || Dall) {
	print_dcaches(1);
    }

    if (Dindextimes || Dall) {
	print_DindexTimes(1);
    }

    if (Dindexflags || Dall) {
	print_DindexFlags(1);
    }

    if (DdvnextTbl || Dall) {
	print_DdvnextTbl(1);
    }

    if (DdcnextTbl || Dall) {
	print_DdcnextTbl(1);
    }

    if (Dbuffers || Dall) {
	print_buffers(1);
    }

    if (Dnfs || Dall) {
	print_nfss(1);
    }

    if (Dstats || Dall) {
	off_t symoff;
	struct afs_CMStats afs_cmstats;
	struct afs_stats_CMPerf afs_cmperfstats;

	printf("\n\nPrinting count references to cm-related functions..\n\n");
	findsym("afs_cmstats", &symoff);
	kread(kmem, symoff, (char *)&afs_cmstats, sizeof afs_cmstats);
	print_cmstats(&afs_cmstats);
	printf("\n\nPrinting some cm struct performance stats..\n\n");
	findsym("afs_stats_cmperf", &symoff);
	kread(kmem, symoff, (char *)&afs_cmperfstats, sizeof afs_cmperfstats);
	print_cmperfstats(&afs_cmperfstats);

    }
    if (Dlocks || Dall) {
	print_global_locks(kmem);
    }
    if (Dglobals || Dall) {
	printf("\n\nPrinting Misc afs globals...\n");
	print_global_afs_resource(kmem);
	print_global_afs_cache(kmem);
    }
    if (Dbkg || Dall) {
	print_bkg(kmem);
    }
    if (Dvlru || Dall) {
	print_vlru(kmem);
    }
    if (Ddlru || Dall) {
	print_dlru(kmem);
    }
    if (Drxstats || Dall) {
	print_rxstats(kmem);
    }
    if (Drx || Dall) {
	print_rx(kmem);
    }
#ifndef AFS_KDUMP_LIB
    if (Dallocs || Dall) {
	print_allocs(1);
    }
#endif
    if (Dcallout || Dall) {
	print_callout(kmem);
    }
    if (Ddnlc || Dall) {
	print_dnlc(kmem);
    }
    if (Dgcpags || Dall) {
	print_gcpags(1);
    }
#endif
    return 0;
}

#if !defined(AFS_DARWIN_ENV) && !defined(AFS_FBSD_ENV)
int Sum_cellnames = 0, Sum_userstp = 0, Sum_volnames = 0, Sum_exps =
    0, Sum_nfssysnames = 0;
int Sum_vcachemvids = 0, Sum_vcachelinkData = 0, Sum_vcacheacc =
    0, Sum_vcachelocks = 0;
int Sum_cellaliases = 0, Sum_cellname_names = 0;

int
print_cells(pnt)
     int pnt;
{
    off_t symoff;
    struct cell *cells, cle, *clentry = &cle, *clep;
    long j = 0, cell;
    struct afs_q CellLRU, lru, *vu = &lru, *tq, *uq;
    u_long lru_addr;

    if (pnt)
	printf("\n\nPrinting Cells' LRU list...\n");
    findsym("CellLRU", &symoff);
    kread(kmem, symoff, (char *)&CellLRU, sizeof CellLRU);
    lru_addr = (u_long) symoff;
    for (tq = CellLRU.next; (u_long) tq != lru_addr; tq = uq) {
	clep = QTOC(tq);
	kread(kmem, (off_t) tq, (char *)vu, sizeof CellLRU);
	uq = vu->next;
	kread(kmem, (off_t) clep, (char *)clentry, sizeof *clentry);
	print_cell(kmem, clentry, clep, pnt);
	j++;
    }
    if (pnt)
	printf("... found %d 'afs_cells' entries\n", j);

    return j;
}

int
print_cellaliases(int pnt)
{
    off_t symoff;
    struct cell_alias *ca, cae;
    long j = 0;

    if (pnt)
	printf("\n\nPrinting cell_alias list...\n");
    findsym("afs_cellalias_head", &symoff);
    kread(kmem, symoff, (char *)&ca, sizeof ca);
    while (ca) {
	char alias[100], cell[100];

	kread(kmem, (off_t) ca, (char *)&cae, sizeof cae);
	kread(kmem, (off_t) cae.alias, alias, (KDUMP_SIZE_T) 40);
	alias[40] = '\0';
	Sum_cellaliases += strlen(alias) + 1;
	kread(kmem, (off_t) cae.cell, cell, (KDUMP_SIZE_T) 40);
	cell[40] = '\0';
	Sum_cellaliases += strlen(cell) + 1;
	if (pnt)
	    printf("%x: alias=%s cell=%s index=%d\n", ca, alias, cell,
		   cae.index);
	ca = cae.next;
	j++;
    }
    if (pnt)
	printf("... found %d 'cell_alias' entries\n", j);

    return j;
}

int
print_cellnames(int pnt)
{
    off_t symoff;
    struct cell_name *cn, cne;
    long j = 0;

    if (pnt)
	printf("\n\nPrinting cell_name list...\n");
    findsym("afs_cellname_head", &symoff);
    kread(kmem, symoff, (char *)&cn, sizeof cn);
    while (cn) {
	char cellname[100];

	kread(kmem, (off_t) cn, (char *)&cne, sizeof cne);
	kread(kmem, (off_t) cne.cellname, cellname, (KDUMP_SIZE_T) 40);
	cellname[40] = '\0';
	Sum_cellname_names += strlen(cellname) + 1;
	if (pnt)
	    printf("%x: cellnum=%d cellname=%s used=%d\n", cn, cne.cellnum,
		   cellname, cne.used);
	cn = cne.next;
	j++;
    }
    if (pnt)
	printf("... found %d 'cell_name' entries\n", j);

    return j;
}

int
print_users(pnt)
     int pnt;
{
    off_t symoff;
    struct unixuser *afs_users[NUSERS], ue, *uentry = &ue, *uep;
    int i, j;

    if (pnt)
	printf("\n\nPrinting 'afs_users' structures...\n");
    findsym("afs_users", &symoff);
    kread(kmem, symoff, (char *)afs_users, sizeof afs_users);
    for (i = 0, j = 0; i < NUSERS; i++) {
	for (uep = afs_users[i]; uep; uep = uentry->next, j++) {
	    kread(kmem, (off_t) uep, (char *)uentry, sizeof *uentry);
	    print_unixuser(kmem, uentry, uep, pnt);
	}
    }
    if (pnt)
	printf("... found %d 'afs_users' entries\n", j);
    return j;
}

struct server **serversFound = NULL;
afs_int32 NserversFound = 0;
#define SF_ALLOCATION_STEP 500

int
add_found_server(sep)
     struct server *sep;
{
    static afs_int32 NserversAllocated = 0;
    static afs_int32 failed = 0;

    if (failed)
	return -1;

    if (NserversFound >= NserversAllocated) {
	NserversAllocated += SF_ALLOCATION_STEP;
	if (!serversFound) {
	    serversFound =
		(struct server **)malloc(NserversAllocated *
					 sizeof(struct server *));
	} else {
	    serversFound =
		(struct server **)realloc((char *)serversFound,
					  NserversAllocated *
					  sizeof(struct server *));
	}
	if (!serversFound) {
	    printf("Can't allocate %lu bytes for list of found servers.\n",
		   NserversAllocated * sizeof(struct server *));
	    failed = 1;
	    NserversFound = 0;
	    return -1;
	}
    }
    serversFound[NserversFound++] = sep;
    return 0;
}

int
find_server(sep)
     struct server *sep;
{
    int i;

    for (i = 0; i < NserversFound; i++) {
	if (sep == serversFound[i])
	    return 1;
    }
    return 0;
}

int
print_servers(pnt)
     int pnt;
{
    off_t symoff;
    struct server *afs_servers[NSERVERS], se, *sentry = &se, *sep;
    struct srvAddr *afs_srvAddrs[NSERVERS], sa, *sap;
    afs_int32 i, nServers, nSrvAddrs, nSrvAddrStructs;
    afs_int32 afs_totalServers, afs_totalSrvAddrs;
    int failed = 0;
    int chainCount[NSERVERS];

    if (pnt) {
	memset((char *)chainCount, 0, sizeof(chainCount));
	printf("\n\nPrinting 'afs_servers' structures...\n");
    }
    findsym("afs_servers", &symoff);
    kread(kmem, symoff, (char *)afs_servers, NSERVERS * sizeof(long));
    for (i = 0, nServers = 0; i < NSERVERS; i++) {
	if (pnt)
	    printf(" --- Chain %d ---\n", i);
	for (sep = afs_servers[i]; sep; sep = sentry->next, nServers++) {
	    kread(kmem, (off_t) sep, (char *)sentry, sizeof *sentry);
	    if (pnt && !failed) {
		if (add_found_server(sep) < 0)
		    failed = 1;
	    }
	    if (pnt)
		chainCount[i]++;
	    if (Dconns || Dall || !pnt)
		print_server(kmem, sentry, sep, 1, pnt);
	    else
		print_server(kmem, sentry, sep, 0, pnt);
	}
    }
    if (pnt) {
	if (Dconns || Dall)
	    printf("... found %d 'afs_servers' entries (total conns = %d)\n",
		   nServers, Nconns);
	else
	    printf("... found %d 'afs_servers' entries\n", nServers);
	printf("Chain lengths:\n");
	for (i = 0; i < NSERVERS; i++) {
	    printf("%2d: %5d\n", i, chainCount[i]);
	}
    }
    Dconns = 0;


    /* Verify against afs_totalServers. */
    if (pnt) {
	memset((char *)chainCount, 0, sizeof(chainCount));
	if (findsym("afs_totalServers", &symoff)) {
	    kread(kmem, symoff, (char *)&afs_totalServers, sizeof(afs_int32));
	    if (afs_totalServers != nServers) {
		printf
		    ("ERROR: afs_totalServers = %d, differs from # of servers in hash table.\n",
		     afs_totalServers);
	    } else {
		printf("afs_totalServers = %d, matches hash chain count.\n",
		       afs_totalServers);
	    }
	}

	printf("\n\nPrinting 'afs_srvAddr' structures...\n");
	if (findsym("afs_srvAddrs", &symoff)) {
	    kread(kmem, symoff, (char *)afs_srvAddrs,
		  NSERVERS * sizeof(long));
	    nSrvAddrStructs = 0;
	    for (i = 0, nSrvAddrs = 0; i < NSERVERS; i++) {
		printf(" --- Chain %d ---\n", i);
		for (sap = afs_srvAddrs[i]; sap; sap = sa.next_bkt) {
		    kread(kmem, (off_t) sap, (char *)&sa, sizeof(sa));
		    printf
			("%lx: sa_ip=%s, sa_port=%d, sa_iprank=%d, sa_flags=%x, conns=%lx, server=%lx, nexth=%lx\n",
			 sap, PrintIPAddr(sa.sa_ip), sa.sa_portal,
			 sa.sa_iprank, sa.sa_flags, sa.conns, sa.server,
			 sa.next_bkt);
		    if (sap != (struct srvAddr *)sa.server) {
			/* only count ones not in a server struct. */
			nSrvAddrStructs++;
		    }
		    nSrvAddrs++;
		    chainCount[i]++;
		    if (!failed) {
			if (!find_server(sa.server)) {
			    kread(kmem, (off_t) sa.server, (char *)sentry,
				  sizeof *sentry);
			    printf
				("ERROR: Server missing from hash chain: server=%lx, server->next=%lx\n",
				 sa.server, sentry->next);
			    print_server(kmem, sentry, sa.server, 1, pnt);
			    printf
				("----------------------------------------------------\n");
			}
		    }

		}
	    }
	    printf
		("... found %d 'afs_srvAddr' entries, %d alloc'd (not in server struct)\n",
		 nSrvAddrs, nSrvAddrStructs);
	    printf("Chain lengths:\n");
	    for (i = 0; i < NSERVERS; i++) {
		printf("%2d: %5d\n", i, chainCount[i]);
	    }
	    if (findsym("afs_totalSrvAddrs", &symoff)) {
		kread(kmem, symoff, (char *)&afs_totalSrvAddrs,
		      sizeof(afs_int32));
		if (afs_totalSrvAddrs != nSrvAddrStructs) {
		    printf
			("ERROR: afs_totalSrvAddrs = %d, differs from number of alloc'd srvAddrs in hash table.\n",
			 afs_totalSrvAddrs);
		} else {
		    printf
			("afs_totalSrvAddrs = %d, matches alloc'd srvAddrs in hash chain count.\n",
			 afs_totalSrvAddrs);
		}
	    }
	}
    }
    return nServers;
}


void
print_Conns(pnt)
     int pnt;
{
    off_t symoff;
    struct server *afs_servers[NSERVERS], se, *sentry = &se, *sep;
    afs_int32 i, j;

    if (pnt)
	printf("\n\nPrinting all 'afs_conns' to  the servers...\n");
    findsym("afs_servers", &symoff);
    kread(kmem, symoff, (char *)afs_servers, sizeof afs_servers);
    for (i = 0, j = 0; i < NSERVERS; i++) {
	for (sep = afs_servers[i]; sep; sep = sentry->next, j++) {
	    kread(kmem, (off_t) sep, (char *)sentry, sizeof *sentry);
	    print_server(kmem, sentry, sep, 2, pnt);
	}
    }
    if (pnt)
	printf("... found %d 'afs_conns' entries\n", Nconns);
}


int
print_volumes(pnt)
     int pnt;
{
    off_t symoff;
    struct volume *afs_volumes[NVOLS], ve, *ventry = &ve, *vep;
    afs_int32 i, j;

    if (pnt)
	printf("\n\nPrinting 'afs_volumes' structures...\n");
    findsym("afs_volumes", &symoff);
    kread(kmem, symoff, (char *)afs_volumes, NVOLS * sizeof(long));
    for (i = 0, j = 0; i < NVOLS; i++) {
	for (vep = afs_volumes[i]; vep; vep = ventry->next, j++) {
	    kread(kmem, (off_t) vep, (char *)ventry, sizeof *ventry);
	    print_volume(kmem, ventry, vep, pnt);
	}
    }
    if (pnt)
	printf("... found %d 'afs_volumes' entries\n", j);
    return (j);
}

void
print_cbHash(pnt)
     int pnt;
{
    off_t symoff;
    struct afs_q cbHashT[CBHTSIZE];
    afs_int32 i, j;

    if (pnt)
	printf("\n\nPrinting 'cbHashT' table...\n");
    findsym("cbHashT", &symoff);
    kread(kmem, symoff, (char *)cbHashT, sizeof cbHashT);
    for (i = 0; i < CBHTSIZE; i++) {
	if (pnt)
	    printf("%lx: %x %x\n", (long)symoff + 8 * i, cbHashT[i].prev,
		   cbHashT[i].next);
    }
    if (pnt)
	printf("... that should be %d callback hash entries\n", i);
}

int
print_vcaches(pnt)
     int pnt;
{
    off_t symoff;
    struct vcache *afs_vhashTable[VCSIZE], Ve, *Ventry = &Ve, *Vep;
    afs_int32 i, j;

    if (pnt)
	printf("\n\nPrinting afs_vcaches structures...\n");
    if (pnt)
	printf("print_vcaches: sizeof(struct vcache) = %ld\n",
	       (long)sizeof(struct vcache));
    findsym("afs_vhashT", &symoff);
    kread(kmem, symoff, (char *)afs_vhashTable, sizeof afs_vhashTable);
    for (i = 0, j = 0; i < VCSIZE; i++) {
	if (pnt)
	    printf("Printing hash chain %d...\n", i);
	for (Vep = afs_vhashTable[i]; Vep; Vep = Ventry->hnext, j++) {
	    kread(kmem, (off_t) Vep, (char *)Ventry, sizeof *Ventry);
	    if (Dvstats || Dall || !pnt)
		print_vcache(kmem, Ventry, Vep, pnt);
	    if (Dvnodes || Dall)
		print_vnode(kmem, Ventry, Vep, pnt);
	}
    }
    if (pnt)
	printf("... found %d 'afs_vcaches' entries\n", j);
    return j;
}

int
print_dcaches(pnt)
     int pnt;
{
    off_t symoff;
    long table, *ptr;
    struct dcache dc, *dcp = &dc, *dp;
    afs_int32 i, j, count;
    struct afs_q dlru;

    /* Handle the afs_dcaches structs */
    if (pnt)
	printf("\n\nPrinting afs_dcache related structures...\n");
    findsym("afs_cacheFiles", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    findsym("afs_indexTable", &symoff);
    kread(kmem, symoff, (char *)&table, sizeof(long));
    ptr = (long *)malloc(count * sizeof(long));
    kread(kmem, table, (char *)ptr, count * sizeof(long));
    for (i = 0, j = 0; i < count; i++) {
	if (dp = (struct dcache *)ptr[i]) {
	    if (pnt)
		printf("afs_indexTable[%d] %x: ", i, dp);
	    kread(kmem, (off_t) dp, (char *)dcp, sizeof *dcp);
	    print_dcache(kmem, dcp, dp, pnt);
	    j++;
	}
    }
    if (pnt)
	printf("... found %d 'dcache' entries\n", j);
    findsym("afs_DLRU", &symoff);
    kread(kmem, symoff, (char *)&dlru, sizeof(struct afs_q));
    if (pnt)
	printf("DLRU next=0x%x, prev=0x%x\n", dlru.next, dlru.prev);
    free(ptr);

    return j;
}


void
print_DindexTimes(pnt)
     int pnt;
{
    off_t symoff;
    long table;
    afs_hyper_t *ptr;
    afs_int32 temp, *indexTime = &temp;
    afs_int32 i, j, count;

    /* Handle the afs_indexTimes array */
    if (pnt)
	printf("\n\nPrinting afs_indexTimes[]...\n");
    findsym("afs_cacheFiles", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    findsym("afs_indexTimes", &symoff);
    kread(kmem, symoff, (char *)&table, sizeof(long));
    ptr = (afs_hyper_t *) malloc(count * sizeof(afs_hyper_t));
    kread(kmem, table, (char *)ptr, count * sizeof(afs_hyper_t));
    for (i = 0, j = 0; i < count; i++) {
	if (pnt)
	    printf("afs_indexTimes[%d]\t%10d.%d\n", i, ptr[i].high,
		   ptr[i].low);
/*      if (dp = (struct dcache *)ptr[i]) {
	printf("afs_indexTable[%d] %lx: ", i, dp);
	kread(kmem, (off_t) dp, (char *)dcp, sizeof *dcp);
	print_dcache(kmem, dcp, dp);
	}
*/
	j++;
    }
    if (pnt)
	printf("afs_indexTimes has %d entries\n", j);
    free(ptr);
}


void
print_DdvnextTbl(pnt)
     int pnt;
{
    off_t symoff;
    long table;
    afs_int32 *ptr;
    afs_int32 temp, *indexTime = &temp;
    afs_int32 i, j, count;

    /* Handle the afs_dvnextTbl arrays */
    if (pnt)
	printf("\n\nPrinting afs_dvnextTbl[]...\n");
    findsym("afs_cacheFiles", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    findsym("afs_dvnextTbl", &symoff);
    kread(kmem, symoff, (char *)&table, sizeof(long));
    ptr = (afs_int32 *) malloc(count * sizeof(afs_int32));
    kread(kmem, table, (char *)ptr, count * sizeof(afs_int32));
    for (i = 0, j = 0; i < count; i++) {
	if (pnt)
	    printf("afs_dvnextTbl[%d]\t%d\n", i, ptr[i]);
	j++;
    }
    if (pnt)
	printf("afs_dvnextTbl has %d entries\n", j);
    free(ptr);
}


void
print_DdcnextTbl(pnt)
     int pnt;
{
    off_t symoff;
    long table;
    afs_int32 *ptr;
    afs_int32 temp, *indexTime = &temp;
    afs_int32 i, j, count;

    /* Handle the afs_dcnextTbl arrays */
    if (pnt)
	printf("\n\nPrinting afs_dcnextTbl[]...\n");
    findsym("afs_cacheFiles", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    findsym("afs_dcnextTbl", &symoff);
    kread(kmem, symoff, (char *)&table, sizeof(long));
    ptr = (afs_int32 *) malloc(count * sizeof(afs_int32));
    kread(kmem, table, (char *)ptr, count * sizeof(afs_int32));
    for (i = 0, j = 0; i < count; i++) {
	if (pnt)
	    printf("afs_dcnextTbl[%d]\t%d\n", i, ptr[i]);
	j++;
    }
    if (pnt)
	printf("afs_dcnextTbl has %d entries\n", j);
    free(ptr);
}


void
print_DindexFlags(pnt)
     int pnt;
{
    off_t symoff;
    afs_int32 count;
    long table;
    unsigned char *flags;
    afs_int32 temp, *indexTime = &temp;
    afs_int32 i, j;

    /* Handle the afs_indexFlags array */
    if (pnt)
	printf("\n\nPrinting afs_indexFlags[]...\n");
    findsym("afs_cacheFiles", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    findsym("afs_indexFlags", &symoff);
    kread(kmem, symoff, (char *)&table, sizeof(long));
    flags = (unsigned char *)malloc(count * sizeof(char));
    kread(kmem, table, flags, count * sizeof(char));
    for (i = 0, j = 0; i < count; i++) {
	if (pnt)
	    printf("afs_indexFlags[%d]\t%4u\n", i, flags[i]);
	j++;
    }
    if (pnt)
	printf("afs_indexFlags has %d entries\n", j);
    free(flags);
}


void
print_buffers(pnt)
     int pnt;
{
    off_t symoff;
    long table;
    afs_int32 count;
    unsigned char *buffers;
    struct buffer *bp;
    afs_int32 i, j;

    if (pnt)
	printf("\n\nPrinting 'buffers' table...\n");
    findsym("Buffers", &symoff);
    kread(kmem, symoff, (char *)&table, sizeof(long));
    findsym("nbuffers", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof(int));
    buffers = (unsigned char *)malloc(count * sizeof(struct buffer));
    kread(kmem, table, buffers, count * sizeof(struct buffer));
    bp = (struct buffer *)buffers;
    for (i = 0, j = 0; i < count; i++, bp++) {
#ifdef AFS_SGI62_ENV
	if (pnt)
	    printf
		("Buffer #%d:\tfid=%llu page=%d, accTime=%d,\n\tHash=%x, data=%x, lockers=%x, dirty=%d, hashI=%d\n",
		 i, bp->fid[0], bp->page, bp->accesstime, bp->hashNext,
		 bp->data, bp->lockers, bp->dirty, bp->hashIndex);
#else
	if (pnt)
	    printf
		("Buffer #%d:\tfid=%lu page=%d, accTime=%d,\n\tHash=%x, data=%x, lockers=%x, dirty=%d, hashI=%d\n",
		 i, bp->fid, bp->page, bp->accesstime, bp->hashNext,
		 bp->data, bp->lockers, bp->dirty, bp->hashIndex);
#endif
	j++;
    }
    if (pnt)
	printf("\n\t   ... that should be %d buffer entries\n", i);
}


int
print_nfss(pnt)
     int pnt;
{
    off_t symoff;
    struct afs_exporter *exp_entry, ex, *exp = &ex, *exp1;
    struct nfsclientpag *afs_nfspags[NNFSCLIENTS], e, *entry = &e, *ep;
    long i, j, cell;

    /* Handle the afs_exporter structures */
    if (pnt)
	printf("\n\nPrinting 'afs_exporters' link list...\n");
    findsym("root_exported", &symoff);
    kread(kmem, symoff, (char *)&cell, sizeof(long));
    for (exp1 = (struct afs_exporter *)cell, j = 0; exp1;
	 exp1 = exp->exp_next, j++) {
	kread(kmem, (off_t) exp1, (char *)exp, sizeof *exp);
	if (pnt)
	    printf("AFS_EXPORTER(%x): \n", exp1);
	print_exporter(kmem, exp, exp1, pnt);
	Sum_exps++;
    }
    if (pnt)
	printf("... found %d 'afs_exporters' entries\n", j);

    /* Handle the afs_nfsclientpags structs */
    if (pnt)
	printf("\n\nPrinting 'afs_nfsclientpags' structures...\n");
    if (!findsym("afs_nfspags", &symoff))
	return 0;
    kread(kmem, symoff, (char *)afs_nfspags, sizeof afs_nfspags);
    for (i = 0, j = 0; i < NNFSCLIENTS; i++) {
	for (ep = afs_nfspags[i]; ep; ep = entry->next, j++) {
	    kread(kmem, (off_t) ep, (char *)entry, sizeof *entry);
	    print_nfsclient(kmem, entry, ep, pnt);
	}
    }
    if (pnt)
	printf("... found %d 'afs_nfsclientpags' entries\n", j);
    return j;
}

#if defined(AFS_GLOBAL_SUNLOCK) && !defined(AFS_HPUX_ENV) && !defined(AFS_AIX41_ENV)
typedef struct event {
    struct event *next;		/* next in hash chain */
    char *event;		/* lwp event: an address */
    int refcount;		/* Is it in use? */
    kcondvar_t cond;		/* Currently associated condition variable */
    int seq;			/* Sequence number: this is incremented
				 * by wakeup calls; wait will not return until
				 * it changes */
} event_t;
#endif


#ifdef AFS_LINUX22_ENV
/* This is replicated from LINUX/osi_alloc.c */
#define MEM_SPACE sizeof(int)

#define KM_TYPE 1
#define VM_TYPE 2
struct osi_linux_mem {
    int mem_next;		/* types are or'd into low bits of next */
    char data[1];
};
#define MEMTYPE(A) ((A) & 0x3)
#define MEMADDR(A) ((struct osi_linux_mem*)((A) & (~0x3)))
#define PR_MEMTYPE(A) ((MEMTYPE(A) == KM_TYPE) ? "phys" : "virt")
void
print_alloced_memlist(void)
{
    off_t symoff;
    struct osi_linux_mem *memp, memlist, next;
    off_t next_addr;
    int count;
    int n = 0;

    findsym("afs_linux_memlist_size", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    findsym("afs_linux_memlist", &symoff);
    kread(kmem, symoff, (char *)&memp, sizeof memp);
    if (memp) {
#ifdef AFS_LINUX_64BIT_KERNEL
	kread(kmem, (unsigned long)memp, (char *)&next, sizeof next);
#else
	kread(kmem, (int)memp, (char *)&next, sizeof next);
#endif /* AFS_LINUX_64BIT_KERNEL */
    } else {
	memset(&next, 0, sizeof next);
    }
    printf("Allocated memory list: %d elements\n", count);
    printf("%20s %4s %10s\n", "Address", "Type", "Next");
    printf("%20lx %4s %10x\n", (long)((char *)memp) + MEM_SPACE,
	   PR_MEMTYPE(next.mem_next), next.mem_next);
    n = 1;
    while (next_addr = (off_t) MEMADDR(next.mem_next)) {
	n++;
	memlist = next;
	kread(kmem, next_addr, (char *)&next, sizeof next);
	printf("%20lx %4s %10x\n", (long)next_addr + MEM_SPACE,
	       PR_MEMTYPE(next.mem_next), next.mem_next);
    }
    printf("Found %d elements in allocated memory list, expected %d\n", n,
	   count);
}
#endif

void
print_allocs(pnt)
     int pnt;
{
    off_t symoff;
    long count, i, j, k, l, m, n, T = 0, tvs;
    struct afs_CMStats afs_cmstats;
    struct afs_stats_CMPerf afs_cmperfstats;

    findsym("afs_cmstats", &symoff);
    kread(kmem, symoff, (char *)&afs_cmstats, sizeof afs_cmstats);
    findsym("afs_stats_cmperf", &symoff);
    kread(kmem, symoff, (char *)&afs_cmperfstats, sizeof afs_cmperfstats);

    T += MAXSYSNAME;
    printf("\n\n%20s:\t%8d bytes\n", "Sysname area", MAXSYSNAME);

    Sum_cellnames = 0;
    i = print_cells(0);
    j = (i * sizeof(struct cell)) + Sum_cellnames;
    T += j;
    printf
	("%20s:\t%8d bytes\t[%d cells/%d bytes each + %d bytes for cell names]\n",
	 "Cell package", j, i, sizeof(struct cell), Sum_cellnames);

    Sum_cellaliases = 0;
    i = print_cellaliases(0);
    j = (i * sizeof(struct cell_alias)) + Sum_cellaliases;
    T += j;
    printf
	("%20s:\t%8d bytes\t[%d cell_aliases/%d bytes each + %d bytes for cell names]\n",
	 "Cell package", j, i, sizeof(struct cell_alias), Sum_cellaliases);

    Sum_cellname_names = 0;
    i = print_cellnames(0);
    j = (i * sizeof(struct cell_name)) + Sum_cellname_names;
    T += j;
    printf
	("%20s:\t%8d bytes\t[%d cell_names/%d bytes each + %d bytes for cell name strings]\n",
	 "Cell package", j, i, sizeof(struct cell_name), Sum_cellname_names);

    Sum_userstp = 0;
    i = print_users(0);
    j = (i * sizeof(struct unixuser)) + Sum_userstp;
    T += j;
    printf
	("%20s:\t%8d bytes\t[%d users/%d bytes each + %d bytes for secret tokens]\n",
	 "User package", j, i, sizeof(struct unixuser), Sum_userstp);

    i = print_servers(0);
    j = (i * sizeof(struct server));
    T += j;
    printf("%20s:\t%8d bytes\t[%d servers/%d bytes each]\n", "Server package",
	   j, i, sizeof(struct server));
    j = (Nconns * sizeof(struct conn));
    T += j;
    printf("%20s:\t%8d bytes\t[%d conns/%d bytes each]\n",
	   "Connection package", j, Nconns, sizeof(struct conn));

    i = (AFS_NCBRS * sizeof(struct afs_cbr)) * (j =
						afs_cmperfstats.
						CallBackAlloced);
    T += i;
    if (i)
	printf("%20s:\t%8d bytes\t[%d cbs/%d bytes each]\n",
	       "Server CB free pool", i, (j * AFS_NCBRS),
	       sizeof(struct afs_cbr));

    Sum_volnames = 0;
    i = print_volumes(0);
    j = (MAXVOLS * sizeof(struct volume)) + Sum_volnames;
    T += j;
    printf
	("%20s:\t%8d bytes\t[%d volumes/%d bytes each + %d bytes for volnames - %d active entries]\n",
	 "Volume package", j, MAXVOLS, sizeof(struct volume), Sum_volnames,
	 i);

    Sum_vcachemvids = Sum_vcachelinkData = Sum_vcacheacc = Sum_vcachelocks =
	0;
    tvs = i = print_vcaches(0);
    j = (i * sizeof(struct vcache));
/*    T += j;*/
/*    printf("%20s:\t%d bytes\t[%d vcaches/%d bytes each]\n", "Vcache package", j, i, sizeof(struct vcache));*/
#ifdef	AFS_AIX32_ENV
    i = (tvs + Sum_vcachemvids + Sum_vcachelinkData +
	 Sum_vcachelocks) * AFS_SMALLOCSIZ;
    printf
	("%20s:\t%8d bytes\t[%d act gnodes, %d mount pnts, %d symbolic links, %d unix locks]\n",
	 "[VC use of sml fp]*", i, tvs, Sum_vcachemvids, Sum_vcachelinkData,
	 Sum_vcachelocks);
#else
    i = (Sum_vcachemvids + Sum_vcachelinkData +
	 Sum_vcachelocks) * AFS_SMALLOCSIZ;
    printf
	("%20s:\t8%d bytes\t[%d mount pnts, %d symbolic links, %d unix locks]\n",
	 "[VC use of sml fp]*", i, Sum_vcachemvids, Sum_vcachelinkData,
	 Sum_vcachelocks);
#endif

#define	NAXSs (1000 / sizeof(struct axscache))
#ifdef	AFS32
    i = (NAXSs * sizeof(struct axscache));
    T += i;
    printf("%20s:\t%8d bytes\t[%d access used by vcaches/%d bytes each]\n",
	   "ACL List free pool", i, Sum_vcacheacc, sizeof(struct axscache));
#else
    {
	struct axscache *xp, xpe, *nxp = &xpe;

	findsym("afs_xaxscnt", &symoff);
	kread(kmem, symoff, (char *)&i, sizeof i);
	j = i * (NAXSs * sizeof(struct axscache));
	T += j;
	printf
	    ("%20s:\t%8d bytes\t[%d access used by vcaches/%d bytes each - %d blocks of %d]\n",
	     "ACL List free pool", j, Sum_vcacheacc, sizeof(struct axscache),
	     i, (NAXSs * sizeof(struct axscache)));
    }
#endif

#ifdef	AFS32
    i = print_dcaches(0);
    j = (i * sizeof(struct dcache));
    T += j;
    printf
	("%20s:\t%8d bytes\t[%d dcaches/%d bytes each - ONLY USED COUNTED]\n",
	 "Dcache package", j, i, sizeof(struct dcache));
#else
    findsym("afs_dcentries", &symoff);
    kread(kmem, symoff, (char *)&i, sizeof i);
    j = (i * sizeof(struct dcache));
    T += j;
    printf("%20s:\t%8d bytes\t[%d dcaches/%d bytes each]\n", "Dcache package",
	   j, i, sizeof(struct dcache));
#endif
    findsym("afs_cacheFiles", &symoff);
    kread(kmem, symoff, (char *)&i, sizeof i);
    findsym("afs_cacheStats", &symoff);
    kread(kmem, symoff, (char *)&j, sizeof j);

    k = (j * sizeof(struct vcache));
    printf
	("%20s:\t%8d bytes\t[%d free vcaches/%d bytes each - %d active entries]\n",
	 "Vcache free list", k, j, sizeof(struct vcache), tvs);
    printf("%20s:\t%8d bytes\t[%d entries/%d bytes each]\n",
	   "Dcache Index Table", i * 4, i, 4);
#ifndef	AFS32
    printf("%20s:\t%8d bytes\t[%d entries/%d bytes each]\n",
	   "Dcache Index Times", i * 8, i, 8);
#else
    printf("%20s:\t%8d bytes\t[%d entries/%d bytes each]\n",
	   "Dcache Index Times", i * 4, i, 4);
#endif
    printf("%20s:\t%8d bytes\t[%d entries/%d bytes each]\n",
	   "Dcache Index Flags", i, i, 1);
/*    printf("%20s:\t%8d bytes\t[%d entries/%d bytes each]\n", "Dcache free list", i, i, 1);*/
#ifndef	AFS32
    T += k + (i * 4) + (i * 8) + i;
#else
    T += k + (i * 4) + (i * 4) + i;
#endif

    i = (j = afs_cmperfstats.bufAlloced) * sizeof(struct buffer);
    T += i;
    printf("%20s:\t%8d bytes\t[%d entries/%d bytes each]\n", "Buffer package",
	   i, j, sizeof(struct buffer));
#if	!AFS_USEBUFFERS
#define	AFS_BUFFER_PAGESIZE 2048
    i = j * AFS_BUFFER_PAGESIZE;
    T += i;
    printf("%20s:\t%8d bytes\t[%d entries/%d bytes each]\n",
	   "Xtra Buffer pkg area", i, j, AFS_BUFFER_PAGESIZE);
#endif

    Sum_exps = 0;
    Sum_nfssysnames = 0;
    i = print_nfss(0);
    k = Sum_exps * sizeof(struct afs_exporter);
    T += k;
    if (k)
	printf("%20s:\t%8d bytes\t[%d entries/%d bytes each]\n",
	       "Xlator Exporter list", k, Sum_exps,
	       sizeof(struct afs_exporter));

    j = (i * sizeof(struct nfsclientpag)) + Sum_nfssysnames;
    T += j;
    if (j)
	printf
	    ("%20s:\t%8d bytes\t[%d entries/%d bytes each + %d for remote sysnames]\n",
	     "Xlator Nfs clnt pkg", j, i, sizeof(struct nfsclientpag),
	     Sum_nfssysnames);

    i = (j = afs_cmperfstats.LargeBlocksAlloced) * AFS_LRALLOCSIZ;
    T += i;
    printf
	("%20s:\t%8d bytes\t[%d entries/%d bytes each - %d active entries]\n",
	 "Large Free Pool", i, j, AFS_LRALLOCSIZ,
	 afs_cmperfstats.LargeBlocksActive);

    i = (j = afs_cmperfstats.SmallBlocksAlloced) * AFS_SMALLOCSIZ;
    T += i;
    printf
	("%20s:\t%8d bytes\t[%d entries/%d bytes each - %d active entries]\n",
	 "Small Free Pool", i, j, AFS_SMALLOCSIZ,
	 afs_cmperfstats.SmallBlocksActive);

#if defined(AFS_GLOBAL_SUNLOCK) && !defined(AFS_HPUX_ENV) && !defined(AFS_AIX41_ENV)
    findsym("afs_evhashcnt", &symoff);
    kread(kmem, symoff, (char *)&j, sizeof j);
    i = (j * sizeof(event_t));
    T += i;
    printf("%20s:\t%8d bytes\t[%d entries/%d bytes each]\n",
	   "afs glock Event Pool", i, j, sizeof(event_t));
/*    printf("XXXXXXX Count event queue allocs!!!! XXXXXX\n");*/

#endif
    i = j = 0;
    if (findsym("rxevent_nFree", &symoff))
	kread(kmem, symoff, (char *)&j, sizeof j);
    if (findsym("rxevent_nPosted", &symoff))
	kread(kmem, symoff, (char *)&i, sizeof i);
    k = (i + j) * sizeof(struct rxevent);
    if (k) {
	T += k;
	printf("%20s:\t%8d bytes\t[%d free, %d posted/%d bytes each]\n",
	       "Rx event pkg", k, j, i, sizeof(struct rxevent));
    } else {
	T += (k = 20 * sizeof(struct rxevent));
	printf
	    ("%20s:\t%8d bytes\t[%d entries/%d bytes each - THIS IS MIN ALLOC/NOT ACTUAL]\n",
	     "Rx event pkg", k, 20, sizeof(struct rxevent));
    }

    findsym("rx_nFreePackets", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
/*
    findsym("rx_initSendWindow", &symoff);
    kread(kmem, symoff, (char *) &i, sizeof i);
*/
    i = 0;
    findsym("rx_nPackets", &symoff);
    kread(kmem, symoff, (char *)&j, sizeof j);
    k = (j + i + 2) * sizeof(struct rx_packet);
    T += k;
    printf("%20s:\t%8d bytes\t[%d free packets/%d bytes each]\n",
	   "Rx packet freelist", k, count, sizeof(struct rx_packet));
#define rx_hashTableSize 256	/* XXX */
    i = (rx_hashTableSize * sizeof(struct rx_connection *));
    j = (rx_hashTableSize * sizeof(struct rx_peer *));
    k = i + j;
    T += k;
    printf("%20s:\t%8d bytes\t[%d entries/%d bytes each]\n",
	   "Rx conn/peer tables", k, rx_hashTableSize,
	   sizeof(struct rx_connection *));

    findsym("rxi_Alloccnt", &symoff);
    kread(kmem, symoff, (char *)&j, sizeof j);
    findsym("rxi_Allocsize", &symoff);
    kread(kmem, symoff, (char *)&i, sizeof i);
    T += i;
    printf("%20s:\t%8d bytes\t[%d outstanding allocs]\n", "RX misc allocs", i,
	   j);


    j = afs_cmperfstats.OutStandingMemUsage;
    printf("\n\n%20s:\t%8d bytes\n", "Mem used by afs", j);
    printf("%20s:\t%8d bytes\n", "Accounted-for mem", T);
    printf("%20s:\t%8d bytes\n", "Non acc'd-for mem", j - T);

    printf
	("\n\nNOTE:\n\tAll [...]* entries above aren't counted towards the total mem since they're redundant\n");

#ifdef AFS_LINUX22_ENV
    if (pnt)
	print_alloced_memlist();
#endif
}

#if	defined(sparc) && !defined(__linux__)
int
readmem(kmem, buf, vad, len)
     int kmem, len;
#ifdef AFS_SUN57_ENV
     uintptr_t vad;
#else
     int vad;
#endif		/** AFS_SUN57_ENV **/
     char *buf;
{
    int newlen;
    if ((newlen = kvm_kread(kd, vad, buf, len)) != len) {
	printf("Couldn't process dumpfile with supplied namelist %s\n", obj);
	exit(1);
    }
}
#endif

#ifdef	AFS_OSF_ENV
static
read_addr(int fd, unsigned long addr, unsigned long *val)
{
    if (lseek(fd, addr, SEEK_SET) == -1)
	return (0);
    if (read(fd, val, sizeof(long)) != sizeof(long))
	return (0);
    return (1);
}

static pt_entry_t *ptes = NULL;
static
addr_to_offset(unsigned long addr, unsigned long *ret, int fd)
{
    off_t symoff;
    pt_entry_t pte, *val;
    char *str, *ptr;

    if (IS_SEG1_VA(addr)) {
	if (ptes == NULL) {
	    int i, loc;
	    unsigned long loc1, loc2[2];
	    findsym("kernel_pmap", &symoff);
	    loc1 = coreadj(symoff);
	    /*printf("ptes=%lx -> %lx\n", symoff, loc1); */
	    if (lseek(fd, loc1, L_SET /*0 */ ) != loc1) {
		perror("lseek");
		exit(1);
	    }
	    if ((i = read(fd, (char *)&loc1, sizeof(long))) != sizeof(long)) {
		printf("Read of kerne_map failed\n");
		return;		/*exit(1); */
	    }
	    loc = loc1;
	    /*printf("loc1 %lx -> %lx\n",  loc1, loc); */
	    if (lseek(fd, loc, L_SET /*0 */ ) != loc) {
		perror("lseek");
		exit(1);
	    }
	    if ((i =
		 read(fd, (char *)loc2,
		      2 * sizeof(long))) != 2 * sizeof(long)) {
		printf("Read of kerne_map failed\n");
		return;		/*exit(1); */
	    }
	    ptes = (pt_entry_t *) loc2[1];
	    /*printf("ptes=%lx\n", ptes); */

	}
	if (!addr_to_offset
	    ((unsigned long)(ptes + LEVEL1_PT_OFFSET(addr)),
	     (unsigned long *)&val, fd))
	    return (0);
	if (!read_addr(fd, (unsigned long)val, (unsigned long *)&pte))
	    return (0);
	val = ((pt_entry_t *) PTETOPHYS(&pte)) + LEVEL2_PT_OFFSET(addr);
	if (!read_addr(fd, (unsigned long)val, (unsigned long *)&pte))
	    return (0);
	val = ((pt_entry_t *) PTETOPHYS(&pte)) + LEVEL3_PT_OFFSET(addr);
	if (!read_addr(fd, (unsigned long)val, (unsigned long *)&pte))
	    return (0);
	*ret = PTETOPHYS(&pte) + (addr & ((1 << PGSHIFT) - 1));
	return (1);
    } else if (IS_KSEG_VA(addr)) {
	*ret = KSEG_TO_PHYS(addr);
	return (1);
    } else {
	return (0);
    }
}
#endif

#ifndef AFS_KDUMP_LIB
void
kread(int kmem, off_t loc, void *buf, KDUMP_SIZE_T len)
{
    int i;

    memset(buf, 0, len);

#ifdef	AFS_OSF_ENV
    if (mem) {
	unsigned long ret;
	i = addr_to_offset(loc, &ret, kmem);
	if (i == 1)
	    loc = ret;
	else {
	    unsigned long loc1;
	    loc1 = coreadj(loc);
	    loc = loc1;
	}
    }
#else
#if	defined(sparc) && !defined(__linux__)
#ifndef AFS_SUN5_ENV
    if (mem) {
#endif
	readmem(kmem, buf, (off_t) loc, len);
	return;
#ifndef AFS_SUN5_ENV
    }
#endif
#endif
#endif
#if	! defined(AFS_SUN5_ENV)
#if defined(AFS_SGI61_ENV) && !defined(AFS_32BIT_KERNEL_ENV)
    if (lseek64(kmem, loc, L_SET /*0 */ ) != loc)
#else
    if (lseek(kmem, loc, L_SET /*0 */ ) != loc)
#endif
    {
	perror("lseek");
	exit(1);
    }
    if (loc == 0) 
	printf("WARNING: Read failed: loc=0\n"); 
    else
	if ((i = read(kmem, buf, len)) != len) {
	    printf("WARNING: Read failed: ");
	    if (sizeof(loc) > sizeof(long)) {
		printf("loc=%llx", loc);
	    } else {
		printf("loc=%lx", (long)loc);
	    }
	    printf(", buf=%lx, len=%ld, i=%d, errno=%d\n", (long)buf,
		   (long)len, i, errno);
	    return;			/*exit(1); */
	}
#endif
}
#endif /* AFS_KDUMP_LIB */

#ifdef	AFS_SUN5_ENV

/**
  * When examining the dump of a 64 bit kernel, we use this function to
  * read symbols. The function opencore() calls this or rdsymbols() using
  * the macro RDSYMBOLS
  */

rdsymbols()
{

    FILE *fp;
    Elf *efd;
    Elf_Scn *cn = NULL;
#ifdef	_LP64
    Elf64_Shdr *shdr;
    Elf64_Sym *stbl, *p1, *p2;
    Elf64_Shdr *(*elf_getshdr) (Elf_Scn *) = elf64_getshdr;
#else
    Elf32_Shdr *shdr;
    Elf32_Sym *stbl, *p1, *p2;
    Elf32_Shdr *(*elf_getshdr) (Elf_Scn *) = elf32_getshdr;
#endif
    Elf_Data *dp = NULL, *sdp = NULL;

    int nsyms, i, fd;

    if (!(fp = fopen(obj, "r"))) {
	printf("Can't open %s (%d)\n", core, errno);
	exit(1);
    }

    fd = fileno(fp);
    lseek(fd, 0L, 0);
    if ((efd = elf_begin(fd, ELF_C_READ, 0)) == NULL) {
	printf("Can't elf begin (%d)\n", errno);
	exit(1);
    }
    while (cn = elf_nextscn(efd, cn)) {
	if ((shdr = elf_getshdr(cn)) == NULL) {
	    elf_end(efd);
	    printf("Can't read section header (%d)\n", errno);
	    exit(1);
	}
	if (shdr->sh_type == SHT_SYMTAB)
	    break;
    }
    dp = elf_getdata(cn, dp);
    p1 = stbl = (void *)dp->d_buf;
    nsyms = dp->d_size / sizeof(*stbl);
    cn = elf_getscn(efd, shdr->sh_link);
    sdp = elf_getdata(cn, sdp);
    tblp = malloc(sdp->d_size);
    memcpy(tblp, sdp->d_buf, sdp->d_size);
    p2 = tbl = malloc(nsyms * sizeof(*stbl));
    for (i = 0, scnt = 0; i < nsyms; i++, p1++, p2++) {
	p2->st_name = p1->st_name;
	p2->st_value = p1->st_value;
	p2->st_size = p1->st_size;
	p2->st_info = p1->st_info;
	p2->st_shndx = p1->st_shndx;
	scnt++;
    }
    elf_end(efd);
    close(fd);
}

#endif		/** AFS_SUN5_ENV **/


int
opencore(core)
     char *core;
{
#ifdef AFS_KDUMP_LIB
    return 0;
#else /* AFS_KDUMP_LIB */
    int fd;

#if	defined(sparc) && !defined(__linux__)
#ifndef	AFS_SUN5_ENV
    if (mem) {
#endif

	if ((kd = kvm_open(obj, core, NULL, O_RDONLY, "crash")) == NULL) {
	    printf("Can't open kvm - core file %s\n", core);
	    exit(1);
	}
#ifndef AFS_SUN5_ENV
    } else
#endif
#ifdef	AFS_SUN5_ENV
	rdsymbols();
#endif
#endif /* sparc */

    {
	if ((fd = open(core, O_RDONLY)) < 0) {
	    perror(core);
	    exit(1);
	}
	return fd;
    }
#endif /* AFS_KDUMP_LIB */
}


void
print_exporter(kmem, exporter, ptr, pnt)
     int kmem, pnt;
     struct afs_exporter *exporter, *ptr;
{
    if (pnt) {
	printf("\tstates=%x, type=%x, *data=%lx\n", exporter->exp_states,
	       exporter->exp_type, exporter->exp_data);
	printf
	    ("\texp_stats (calls=%d, rejectedcalls=%d, nopag=%d, invalidpag=%d)\n",
	     exporter->exp_stats.calls, exporter->exp_stats.rejectedcalls,
	     exporter->exp_stats.nopag, exporter->exp_stats.invalidpag);
    }
}


void
print_nfsclient(kmem, ep, ptr, pnt)
     int kmem, pnt;
     struct nfsclientpag *ep, *ptr;
{
    char sysname[100];
	int count;

    if (pnt)
	printf("%lx: uid=%d, host=%x, pag=%x, lastt=%d, ref=%d count=%d\n",
	       ptr, ep->uid, ep->host, ep->pag,
	       ep->lastcall, ep->refCount, ep->sysnamecount);

	for(count = 0; count < ep->sysnamecount; count++){
		kread(kmem, (off_t) ep->sysname[count], sysname, (KDUMP_SIZE_T) 30);
		printf("   %lx: @sys[%d]=%s\n",
			ep->sysname[count], count, sysname);
		Sum_nfssysnames += MAXSYSNAME;
	}
}


#if	defined(AFS_SUN5_ENV)
pmutex(sp, mp)
     char *sp;
     kmutex_t *mp;
{
#ifdef	AFS_SUN54_ENV

#else
    struct stat_mutex *smp = (struct stat_mutex *)mp;

    printf("%s mutex: %x %x\n", sp, smp->m_stats_lock, smp->m_type);
#endif
}

#endif

void
print_unixuser(kmem, uep, ptr, pnt)
     int kmem, pnt;
     struct unixuser *uep, *ptr;
{
    Sum_userstp += uep->stLen;
    if (pnt) {
	printf
	    ("%lx: uid=x%x, cell=%x, vid=%d, refc=%d, states=%x, tokTime=%d, tikLen=%d\n",
	     ptr, uep->uid, uep->cell, uep->vid, uep->refCount, uep->states,
	     uep->tokenTime, uep->stLen);
	printf
	    ("\tstp=%lx, clearTok[Han=x%x, x<%x,%x,%x,%x,%x,%x,%x,%x>, vid=%d, Bt=%d, Et=%d], exporter=%lx\n",
	     uep->stp, uep->ct.AuthHandle, uep->ct.HandShakeKey[0],
	     uep->ct.HandShakeKey[1], uep->ct.HandShakeKey[2],
	     uep->ct.HandShakeKey[3], uep->ct.HandShakeKey[4],
	     uep->ct.HandShakeKey[5], uep->ct.HandShakeKey[6],
	     uep->ct.HandShakeKey[7], uep->ct.ViceId, uep->ct.BeginTimestamp,
	     uep->ct.EndTimestamp, uep->exporter);
    }
}

void
print_cell(kmem, clep, ptr, pnt)
     int kmem, pnt;
     struct cell *clep, *ptr;
{
    int i;
    char cellName[100];
    struct in_addr in;


    kread(kmem, (off_t) clep->cellName, cellName, (KDUMP_SIZE_T) 40);
    cellName[40] = 0;
    Sum_cellnames += strlen(cellName) + 1;
    if (pnt) {
	printf
	    ("%lx: cellname=%s, states=%x, cnum=%d, cindex=%d fsport=%d vlport=%d timeout=%d cnamep=%x\n",
	     ptr, cellName, clep->states, clep->cellNum, clep->cellIndex,
	     clep->fsport, clep->vlport, clep->timeout, clep->cnamep);
#ifdef	AFS33
	if (clep->lcellp)
	    printf("\tlinked cellp %lx\n", clep->lcellp);
#endif
	printf("\tCell's servers: ");
	for (i = 0; i < MAXCELLHOSTS; i++) {
	    if (pretty && (clep->cellHosts[i] == 0))
		break;
	    printf("[%lx] ", clep->cellHosts[i]);
	}
	printf("\n");
    }
}


void
print_server(kmem, sep, ptr, conns, pnt)
     int kmem, conns, pnt;
     struct server *sep, *ptr;
{
    struct srvAddr sa, *sap = &sa, *sap1;
    int j, mh = 0, cnt;

    if (conns != 2 && pnt) {
	printf
	    ("%lx: cell=%lx, addr=%lx, flags=0x%x, actTime=%x, lastDownS=%x, numDownIn=%d, sumofDownt=%d\n",
	     ptr, sep->cell, sep->addr, sep->flags, sep->activationTime,
	     sep->lastDowntimeStart, sep->numDowntimeIncidents,
	     sep->sumOfDowntimes);
	if (sep->flags & SRVR_MULTIHOMED) {
	    if (pnt) {
		printf
		    ("\tuuid=[%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x]  addr_uniquifier=%x\n",
		     sep->sr_uuid.time_low, sep->sr_uuid.time_mid,
		     sep->sr_uuid.time_hi_and_version,
		     sep->sr_uuid.clock_seq_hi_and_reserved,
		     sep->sr_uuid.clock_seq_low, sep->sr_uuid.node[0],
		     sep->sr_uuid.node[1], sep->sr_uuid.node[2],
		     sep->sr_uuid.node[3], sep->sr_uuid.node[4],
		     sep->sr_uuid.node[5], sep->sr_addr_uniquifier);
	    }
	    mh = 1;
	}
	for (j = 0, cnt = 1, sap1 = sep->addr; sap1;
	     sap1 = sap->next_sa, j++, cnt++) {
	    kread(kmem, (off_t) sap1, (char *)sap, sizeof(*sap));
	    if (pnt) {
		if (mh) {
		    printf
			("\t   #%d ip-addr(%lx): [sa_ip=%s, sa_port=%d, sa_iprank=%d, sa_flags=%x, conns=%lx, server=%lx, next_bkt=%lx]\n",
			 cnt, sap1, PrintIPAddr(sap->sa_ip), sap->sa_portal,
			 sap->sa_iprank, sap->sa_flags, sap->conns,
			 sap->server, sap->next_bkt);
		} else {
		    printf
			("\t[sa_ip=%s, sa_port=%d, sa_iprank=%d, sa_flags=%x, conns=%lx, server=%lx, nexth=%lx]\n",
			 PrintIPAddr(sap->sa_ip), sap->sa_portal,
			 sap->sa_iprank, sap->sa_flags, sap->conns,
			 sap->server, sap->next_bkt);
		}
	    }
	}
    }
    if (sep->cbrs && pnt) {
	struct afs_cbr cba, *cbsap = &cba, *cbsap1;

	printf(" Callbacks to be returned:\n");
	for (j = 0, cbsap1 = sep->cbrs; cbsap1; cbsap1 = cbsap->next, j++) {
	    kread(kmem, (off_t) cbsap1, (char *)cbsap, sizeof(*cbsap));
	    printf("     #%2d) %lx [v=%d, n=%d, u=%d]\n", j, cbsap1,
		   cbsap->fid.Volume, cbsap->fid.Vnode, cbsap->fid.Unique);
	}
    }
    if (conns) {
	for (j = 0, sap1 = sep->addr; sap1; sap1 = sap->next_sa, j++) {
	    kread(kmem, (off_t) sap1, (char *)sap, sizeof(*sap));
	    print_conns(kmem, sap1, sap->conns, conns, pnt);
	}
    } else if (pnt)
	printf("\n");
}


void
print_conns(kmem, srv, conns, Con, pnt)
     int kmem, Con, pnt;
     struct srvAddr *srv;
     struct conn *conns;
{
    struct conn *cep, ce, *centry = &ce;
    int i = 1;

    cep = (struct conn *)conns;
    if (pnt && Con != 2) {
	if (cep)
	    printf("\tRPC connections for server %lx:\n", srv);
	else
	    printf("\tNO RPC connections for server %x\n", srv);
    }
    for (; cep; cep = centry->next, Nconns++, i++) {
	if (pnt && Con != 2)
	    printf("\t   #%d> ", i);
	kread(kmem, (off_t) cep, (char *)centry, sizeof *centry);
	print_conn(kmem, centry, cep, pnt);
    }
}


void
print_conn(kmem, conns, ptr, pnt)
     int kmem, pnt;
     struct conn *conns, *ptr;
{
    if (!pnt)
	return;
    printf("%lx: user=%lx, rx=%lx, srvr=%lx, ref=%d, port=%d, forceC=%d\n",
	   ptr, conns->user, conns->id, conns->srvr, conns->refCount,
	   conns->port, conns->forceConnectFS);

}


void
print_volume(kmem, vep, ptr, pnt)
     int kmem, pnt;
     struct volume *vep, *ptr;
{
    int i;
    afs_int32 *loc;
    char Volname[100];



    loc = (afs_int32 *) & vep->lock;
    if (vep->name) {
	kread(kmem, (off_t) vep->name, Volname, (KDUMP_SIZE_T) 40);
	Sum_volnames += strlen(Volname) + 1;
    }
    if (!pnt)
	return;
    printf("%lx: cell=%x, vol=%d, name=%s, roVol=%d, backVol=%d\n", ptr,
	   vep->cell, vep->volume, (vep->name ? Volname : "nil"), vep->roVol,
	   vep->backVol);
#ifdef	AFS33
    printf
	("\trwVol=%d, AcTime=%d, copyDate=%d, expTime=%d, vtix=%d, refC=%d, states=%x\n",
	 vep->rwVol, vep->accessTime, vep->copyDate, vep->expireTime,
	 vep->vtix, vep->refCount, vep->states);
#else
    printf
	("\trwVol=%d, AcTime=%d, copyDate=%d, vtix=%d, refC=%d, states=%x\n",
	 vep->rwVol, vep->accessTime, vep->copyDate, vep->vtix, vep->refCount,
	 vep->states);
#endif
    printf("\tVolume's statuses: ");
    for (i = 0; i < MAXHOSTS && vep->serverHost[i]; i++)
	printf("[%d] ", vep->status[i]);
    printf("\n");

    printf("\tVolume's servers: ");
    for (i = 0; i < MAXHOSTS && vep->serverHost[i]; i++)
	printf("[%lx] ", vep->serverHost[i]);
    printf("\n");

    print_venusfid("\tdotdot", &vep->dotdot);
    printf("\n");

    print_venusfid("\tmtpnt", &vep->mtpoint);
    printf("\n");

#ifdef	AFS33
    if (vep->rootVnode)
	printf("\trootVnode = %d, rootUnique = %d\n", vep->rootVnode,
	       vep->rootUnique);
#endif
    printf("\tlock=0x%x\n", *loc);
}


void
print_venusfid(string, vid)
     char *string;
     struct VenusFid *vid;
{
    printf("%s(c=%x, v=%d, n=%d, u=%d)", string, vid->Cell, vid->Fid.Volume,
	   vid->Fid.Vnode, vid->Fid.Unique);
}


void
print_vnode(kmem, vep, ptr, pnt)
     int kmem, pnt;
     struct vnode *vep, *ptr;
{
#ifdef AFS_AIX_ENV
    struct gnode gnode;
    struct gnode *save_gnode;
#endif /* AFS_AIX_ENV */

    if (!pnt)
	return;
    printf("\n");
#ifdef AFS_AIX_ENV
    save_gnode = vep->v_gnode;
    kread(kmem, (off_t) save_gnode, (char *)&gnode, sizeof(struct gnode));
    vep->v_gnode = &gnode;
#endif /* AFS_AIX_ENV */

#ifdef	AFS_SUN5_ENV
    printf("%x: v_type=%d, v_flag=%d, v_count=%d, \n", ptr, vep->v_type,
	   vep->v_flag, vep->v_count);
    printf
	("\tv_v_stream=%x, v_pages=0x%x, v_mountdhere=%d, v_rdev=%d, v_vfsp=0x%x, v_filocks=0x%x\n",
	 vep->v_stream, vep->v_pages, vep->v_vfsmountedhere, vep->v_rdev,
	 vep->v_vfsp, vep->v_filocks);
    pmutex("\tVnode", &vep->v_lock);
    printf("\tCond v: 0x%x\n", vep->v_cv);
#endif
#ifdef AFS_AIX_ENV
    vep->v_gnode = save_gnode;
#endif /* AFS_AIX_ENV */
#ifdef AFS_SGI65_ENV
#if defined(AFS_32BIT_KERNEL_ENV)
    printf("%lx: v_mreg=0x%lx", ptr, vep->v_mreg);
#else
    printf("%llx: v_mreg=0x%llx", ptr, vep->v_mreg);
#endif
    printf(", v_mregb=0x%lx\n", vep->v_mregb);
#endif
#ifdef AFS_LINUX22_ENV
    /* Print out the stat cache and other inode info. */
    printf
	("\ti_ino=%d, i_mode=%x, i_nlink=%d, i_uid=%d, i_gid=%d, i_size=%d\n",
	 vep->i_ino, vep->i_mode, vep->i_nlink, vep->i_uid, vep->i_gid,
	 vep->i_size);
#ifdef AFS_LINUX24_ENV
    printf
	("\ti_atime=%u, i_mtime=%u, i_ctime=%u, i_version=%u, i_nrpages=%u\n",
	 vep->i_atime, vep->i_mtime, vep->i_ctime, vep->i_version,
	 vep->i_data.nrpages);
#else
    printf
	("\ti_atime=%u, i_mtime=%u, i_ctime=%u, i_version=%u, i_nrpages=%u\n",
	 vep->i_atime, vep->i_mtime, vep->i_ctime, vep->i_version,
	 vep->i_nrpages);
#endif
#ifdef AFS_LINUX26_ENV
    printf("\ti_op=0x%x, i_rdev=0x%x, i_sb=0x%x\n", vep->i_op,
	   vep->i_rdev, vep->i_sb);
#else /* AFS_LINUX26_ENV */
    printf("\ti_op=0x%x, i_dev=0x%x, i_rdev=0x%x, i_sb=0x%x\n", vep->i_op,
	   vep->i_dev, vep->i_rdev, vep->i_sb);
#endif /* AFS_LINUX26_ENV */
#ifdef AFS_LINUX24_ENV
#ifdef AFS_PARISC_LINUX24_ENV
    printf("\ti_sem: count=%d, wait=0x%x\n", vep->i_sem.count,
	   vep->i_sem.wait);
#else
    printf("\ti_sem: count=%d, sleepers=%d, wait=0x%x\n", vep->i_sem.count,
	   vep->i_sem.sleepers, vep->i_sem.wait);
#endif
#else
    printf("\ti_sem: count=%d, waking=%d, wait=0x%x\n", vep->i_sem.count,
	   vep->i_sem.waking, vep->i_sem.wait);
#endif
#ifdef AFS_LINUX26_ENV
    printf("\ti_hash=0x%x:0x%x, i_list=0x%x:0x%x, i_dentry=0x%x:0x%x\n",
	   vep->i_hash.pprev, vep->i_hash.next, vep->i_list.prev,
	   vep->i_list.next, vep->i_dentry.prev, vep->i_dentry.next);
#else /* AFS_LINUX26_ENV */
    printf("\ti_hash=0x%x:0x%x, i_list=0x%x:0x%x, i_dentry=0x%x:0x%x\n",
	   vep->i_hash.prev, vep->i_hash.next, vep->i_list.prev,
	   vep->i_list.next, vep->i_dentry.prev, vep->i_dentry.next);
#endif /* AFS_LINUX26_ENV */
#endif /* AFS_LINUX22_ENV */
}

void
print_vcache(kmem, vep, ptr, pnt)
     int kmem, pnt;
     struct vcache *vep, *ptr;
{
    long *loc, j = 0;
    char *cloc;
    struct VenusFid vid;
    struct axscache acc, *accp = &acc, *acp;
    struct SimpleLocks sl, *slcp = &sl, *slp;
    char linkchar;

    if (vep->mvid) {
	kread(kmem, (off_t) vep->mvid, (char *)&vid, sizeof(struct VenusFid));
	Sum_vcachemvids++;
    }
    if (vep->linkData)
	Sum_vcachelinkData++;
    loc = (long *)&vep->lock;

    if (pnt) {
	if (!Dvnodes)
	    printf("\n");
#ifdef	AFS33
	printf("%lx: refC=%d, pv=%d, pu=%d, flushDv=%d.%d, mapDV=%d.%d, ",
	       ptr, VREFCOUNT(vep), vep->parentVnode, vep->parentUnique,
	       vep->flushDV.high, vep->flushDV.low, vep->mapDV.high,
	       vep->mapDV.low);
#ifdef AFS_64BIT_CLIENT
	printf
	    ("truncPos=(0x%x, 0x%x),\n\tcallb=x%lx, cbE=%d, opens=%d, XoW=%d, ",
	     (int)(vep->truncPos >> 32), (int)(vep->truncPos & 0xffffffff),
	     vep->callback, vep->cbExpires, vep->opens, vep->execsOrWriters);
#else /* AFS_64BIT_CLIENT */
	printf("truncPos=%d,\n\tcallb=x%lx, cbE=%d, opens=%d, XoW=%d, ",
	       vep->truncPos, vep->callback, vep->cbExpires, vep->opens,
	       vep->execsOrWriters);
#endif /* AFS_64BIT_CLIENT */
	printf("flcnt=%d, mvstat=%d\n", vep->flockCount, vep->mvstat);
	printf("\tstates=x%x, ", vep->states);
#ifdef	AFS_SUN5_ENV
	printf("vstates=x%x, ", vep->vstates);
#endif /* AFS_SUN5_ENV */
	printf("dchint=%x, anyA=0x%x\n", vep->dchint, vep->anyAccess);
#ifdef AFS_64BIT_CLIENT
	printf
	    ("\tmstat[len=(0x%x, 0x%x), DV=%d.%d, Date=%d, Owner=%d, Group=%d, Mode=0%o, linkc=%d]\n",
	     (int)(vep->m.Length >> 32), (int)(vep->m.Length & 0xffffffff),
	     vep->m.DataVersion.high, vep->m.DataVersion.low, vep->m.Date,
	     vep->m.Owner, vep->m.Group, vep->m.Mode, vep->m.LinkCount);
#else /* AFS_64BIT_CLIENT */
	printf("\tquick[dc=%x, stamp=%x, f=%x, min=%d, len=%d]\n",
	       vep->quick.dc, vep->quick.stamp, vep->quick.f,
	       vep->quick.minLoc, vep->quick.len);
	printf
	    ("\tmstat[len=%d, DV=%d.%d, Date=%d, Owner=%d, Group=%d, Mode=0%o, linkc=%d]\n",
	     vep->m.Length, vep->m.DataVersion.high, vep->m.DataVersion.low,
	     vep->m.Date, vep->m.Owner, vep->m.Group, vep->m.Mode,
	     vep->m.LinkCount);
#endif /* AFS_64BIT_CLIENT */
#else /* AFS33 */
	printf
	    ("%x: refC=%d, pv=%d, pu=%d, flushDv=%d, mapDV=%d, truncPos=%d\n",
	     ptr, vep->vrefCount, vep->parentVnode, vep->parentUnique,
	     vep->flushDV, vep->mapDV, vep->truncPos);
	printf("\tcallb=x%x, cbE=%d, opens=%d, XoW=%d, flcnt=%d, mvstat=%d\n",
	       vep->callback, vep->cbExpires, vep->opens, vep->execsOrWriters,
	       vep->flockCount, vep->mvstat);
	printf("\tstates=x%x, dchint=%x, anyA=0x%x\n", vep->states,
	       vep->h1.dchint, vep->anyAccess);
	printf
	    ("\tmstat[len=%d, DV=%d, Date=%d, Owner=%d, Group=%d, Mode=%d, linkc=%d]\n",
	     vep->m.Length, vep->m.DataVersion, vep->m.Date, vep->m.Owner,
	     vep->m.Group, vep->m.Mode, vep->m.LinkCount);
#endif /* AFS33 */
#ifdef	AFS_AIX32_ENV
	loc = (afs_int32 *) & vep->pvmlock;
	printf("\tpvmlock=x%x, segid=%X, credp=%lx\n", *loc, vep->segid,
	       vep->credp);
#endif
	printf
	    ("\tlock [wait=%x excl=%x readers=%x #waiting=%x last_reader=%d writer=%d src=%d]\n",
	     vep->lock.wait_states, vep->lock.excl_locked,
	     vep->lock.readers_reading, vep->lock.num_waiting,
	     vep->lock.pid_last_reader, vep->lock.pid_writer,
	     vep->lock.src_indicator);
	print_venusfid("\tfid", &vep->fid);
	if (vep->mvid) {
	    printf(" ");
	    print_venusfid("mvid", &vid);
	}
	printf("\n");
    }
    if (vep->Access) {
	if (pnt)
	    printf("\tAccess Link list: %x\n", vep->Access);
	for (j = 0, acp = vep->Access; acp; acp = accp->next, j++) {
	    kread(kmem, (off_t) acp, (char *)accp, sizeof(*accp));
	    Sum_vcacheacc++;
	    if (pnt)
		printf("\t   %lx: %d) uid=0x%x, access=0x%x, next=%lx\n", acp,
		       j, accp->uid, accp->axess, accp->next);
	}
    }
    if (vep->slocks) {
	if (pnt)
	    printf("\tLocking Link list: %lx\n", vep->slocks);
    }
#ifdef	AFS33
    if (pnt)
	printf("\tCallbacks queue prev= %lx next= %lx\n", vep->callsort.prev,
	       vep->callsort.next);
#endif
    printf("\tvlruq.prev=%lx, vlruq.next=%lx\n", vep->vlruq.prev,
	   vep->vlruq.next);

    /* For defect 7733 - Print linkData field for symlinks */
    if (pnt) {
	if (vep->linkData) {
	    cloc = (char *)vep->linkData;
	    printf("\tSymlink information = '");
	    while (1) {
		kread(kmem, (off_t) cloc, &linkchar, (KDUMP_SIZE_T) 1);
		cloc++;
		if (linkchar == '\0') {
		    printf("'\n");
		    break;
		} else {
		    printf("%c", linkchar);
		}
	    }
	}
    }
#ifdef AFS_LINUX22_ENV
    printf("\tmapcnt=%d\n", vep->mapcnt);
#endif
}


void
print_dcache(kmem, dcp, dp, pnt)
     int kmem, pnt;
     struct dcache *dcp, *dp;
{
    if (!pnt)
	return;
    printf("%lx: ", dp);
    print_venusfid(" fid", &dcp->f.fid);
    printf("refcnt=%d, dflags=%x, mflags=%x, validPos=%d\n", dcp->refCount,
	   dcp->dflags, dcp->mflags, dcp->validPos);

#ifdef	AFS33
    printf("\tf.modtime=%d, f.versNo=%d.%d\n", dcp->f.modTime,
	   dcp->f.versionNo.high, dcp->f.versionNo.low);
#else
    printf("\tf.hvn=%d, f.hcn=%d, f.modtime=%d, f.versNo=%d\n",
	   dcp->f.hvNextp, dcp->f.hcNextp, dcp->f.modTime, dcp->f.versionNo);
#endif
#ifdef AFS_SGI62_ENV
    printf
	("\tf.chunk=%d, f.inode=%lld, f.chunkBytes=%d, f.states=%x",
	 dcp->f.chunk, dcp->f.inode, dcp->f.chunkBytes, dcp->f.states);
#else
    printf
	("\tf.chunk=%d, f.inode=%d, f.chunkBytes=%d, f.states=%x\n",
	 dcp->f.chunk, dcp->f.inode, dcp->f.chunkBytes, dcp->f.states);
#endif
    printf("\tlruq.prev=%lx, lruq.next=%lx, index=%d\n",
	   dcp->lruq.prev, dcp->lruq.next, dcp->index);
}

void
print_bkg(kmem)
     int kmem;
{
    off_t symoff;
    struct brequest afs_brs[NBRS], ue, *uentry = &ue, *uep;
    afs_int32 count, i, j;
    short scount;

    printf("\n\nPrinting some background daemon info...\n\n");
    findsym("afs_brsWaiters", &symoff);
    kread(kmem, symoff, (char *)&scount, sizeof scount);
    printf("Number of processes waiting for bkg daemon %d\n", scount);
    findsym("afs_brsDaemons", &symoff);
    kread(kmem, symoff, (char *)&scount, sizeof scount);
    printf("Number of free bkg daemons %d\n", scount);
    findsym("afs_brs", &symoff);
    kread(kmem, symoff, (char *)afs_brs, sizeof afs_brs);
    printf("Print the current bkg process table\n");
    for (i = 0, j = 0; i < NBRS; i++, j++) {
/*	kread(kmem, (off_t) afs_brs[i], (char *)uentry, sizeof *uentry);*/
	uentry = &afs_brs[i];
	if (uentry->refCount == 0)
	    break;
	printf
	    ("[%d] vcache=0x%lx, cred=0x%lx, code=%d, refCount=%d, opcode=%d, flags=%x [%lx, %lx, %lx, %lx]\n",
	     i, uentry->vc, uentry->cred, uentry->code, uentry->refCount,
	     uentry->opcode, uentry->flags, uentry->size_parm[0],
	     uentry->size_parm[1], uentry->ptr_parm[0], uentry->ptr_parm[1]);

    }
    printf("... found %d active 'afs_brs' entries\n", j);
}

void
print_vlru(kmem)
     int kmem;
{
    off_t symoff;
    struct vcache Ve, *Ventry = &Ve, *Vep, *tvc;
    struct afs_q VLRU, vlru, *vu = &vlru, *tq, *uq;
    u_long vlru_addr, l1, l2, l3;
    afs_int32 count, i, j = 0, maxvcount, vcount, nvnode;
    short scount;

    printf("\n\nPrinting vcache VLRU info (oldest first)...\n\n");
    findsym("afs_cacheStats", &symoff);
    kread(kmem, symoff, (char *)&maxvcount, sizeof maxvcount);
#ifdef	AFS_OSF_ENV
    findsym("afs_maxvcount", &symoff);
    kread(kmem, symoff, (char *)&maxvcount, sizeof maxvcount);
    findsym("afs_vcount", &symoff);
    kread(kmem, symoff, (char *)&vcount, sizeof vcount);
    findsym("max_vnodes", &symoff);
    kread(kmem, symoff, (char *)&nvnode, sizeof nvnode);
    printf("max number of vcache entries = %d\n", maxvcount);
    printf("number of vcaches in use = %d\n", vcount);
    printf("total number of system vnode entries = %d\n", nvnode);
#endif
    findsym("VLRU", &symoff);
    kread(kmem, symoff, (char *)&VLRU, sizeof VLRU);
    vlru_addr = (u_long) symoff;
    for (tq = VLRU.prev; (u_long) tq != vlru_addr; tq = uq) {
	tvc = QTOV(tq);
	kread(kmem, (off_t) tq, (char *)vu, sizeof VLRU);
	uq = vu->prev;
	kread(kmem, (off_t) tvc, (char *)Ventry, sizeof *Ventry);
	print_vcache(kmem, Ventry, tvc, 1);
	j++;
    }
    printf("... found %d active vcache entries in the VLRU\n", j);
}

void
print_dlru(kmem)
     int kmem;
{
    off_t symoff;
    struct dcache Ve, *Ventry = &Ve, *Vep, *tdc;
    struct afs_q DLRU, dlru, *vu = &dlru, *tq, *uq;
    u_long dlru_addr, l1, l2, l3;
    afs_int32 count, i, j = 0, maxvcount, vcount, nvnode;
    short scount;

    printf("\n\nPrinting vcache DLRU info...\n\n");
    findsym("afs_DLRU", &symoff);
    kread(kmem, symoff, (char *)&DLRU, sizeof DLRU);
    dlru_addr = (u_long) symoff;
    for (tq = DLRU.prev; (u_long) tq != dlru_addr; tq = uq) {
	tdc = (struct dcache *)tq;
	kread(kmem, (off_t) tq, (char *)vu, sizeof DLRU);
	uq = vu->prev;
	kread(kmem, (off_t) tdc, (char *)Ventry, sizeof *Ventry);
	print_dcache(kmem, Ventry, tdc, 1);
	j++;
    }
    printf("... found %d active dcache entries in the DLRU\n\n\n", j);

    findsym("afs_freeDSList", &symoff);
    kread(kmem, symoff, (char *)&dlru_addr, sizeof dlru_addr);
    printf("\tfreeDSList link list starts at 0x%x\n", dlru_addr);
    j = 0;
    for (tdc = (struct dcache *)dlru_addr; tdc;
	 tdc = (struct dcache *)Ventry->lruq.next) {
	kread(kmem, (off_t) tdc, (char *)Ventry, sizeof *Ventry);
	print_dcache(kmem, Ventry, tdc, 1);
	j++;
/*	printf("%3d) %x\n", j, tdc);*/
    }
    printf("... found %d dcache entries in the freeDSList\n", j);
}

int
print_gcpags(pnt)
     int pnt;
{
    off_t symoff;
    afs_int32 afs_gcpags;
    afs_int32 afs_gcpags_procsize;

    if (pnt)
	printf("\n\nPrinting GCPAGS structures...\n");

    findsym("afs_gcpags", &symoff);
    kread(kmem, symoff, (char *)&afs_gcpags, sizeof afs_gcpags);

    findsym("afs_gcpags_procsize", &symoff);
    kread(kmem, symoff, (char *)&afs_gcpags_procsize,
	  sizeof afs_gcpags_procsize);

    printf("afs_gcpags=%d\n", afs_gcpags);
    printf("afs_gcpags_procsize=%d\n", afs_gcpags_procsize);

    return 0;
}


#ifdef	AFS_AIX_ENV
#include <sys/syspest.h>	/* to define the assert and ASSERT macros       */
#include <sys/timer.h>		/* For the timer related defines                */
#include <sys/intr.h>		/* for the serialization defines                */
#include <sys/malloc.h>		/* for the parameters to xmalloc()              */

struct tos {
    struct tos *toprev;		/* previous tos in callout table */
    struct tos *tonext;		/* next tos in callout table    */
    struct trb *trb;		/* this timer request block     */
    afs_int32 type;
    long p1;
};

struct callo {
    int ncallo;			/* number of callout table elements     */
    struct tos *head;		/* callout table head element           */
};
#endif

void
print_callout(kmem)
     int kmem;
{
    off_t symoff;
#ifndef	AFS_AIX_ENV
    printf("\n\nCallout table doesn't exist for this system\n");
#else
    struct callo Co, *Coe = &Co, *Cop;
    struct tos To, *Toe = &To, *tos;
    struct trb Trb, *Trbe = &Trb, *trb;
    register int i = 0;


    printf("\n\nPrinting callout table info...\n\n");
    findsym("afs_callo", &symoff);
    kread(kmem, symoff, (char *)&Co, sizeof Co);
    printf("Number of callouts %d\n", Co.ncallo);
    if (Co.ncallo > 0) {
	printf("Count\tType\taddr\tfunc\tdata\n");
	for (tos = Co.head; tos != NULL; tos = Toe->tonext) {
	    i++;
	    kread(kmem, (off_t) tos, (char *)&To, sizeof To);
	    kread(kmem, (off_t) Toe->trb, (char *)&Trb, sizeof Trb);
	    printf("%d\t%d\t%x\t%x\t%x\n", i, Toe->type, Toe->p1, Trbe->tof,
		   Trbe->func_data);
	}
    }
#endif
}

void
print_dnlc(kmem)
     int kmem;
{
    struct nc *nameHash[256];

}


void
print_global_locks(kmem)
     int kmem;
{
    off_t symoff;
    afs_int32 count;
    int i;
    static struct {
	char *name;
    } locks[] = { {
    "afs_xvcache"}, {
    "afs_xdcache"}, {
    "afs_xserver"}, {
    "afs_xvcb"}, {
    "afs_xbrs"}, {
    "afs_xcell"}, {
    "afs_xconn"}, {
    "afs_xuser"}, {
    "afs_xvolume"},
#ifndef	AFS_AIX_ENV
    {
    "osi_fsplock"},
#endif
    {
    "osi_flplock"}, {
    "afs_xcbhash"}, {
    "afs_xinterface"}, {
    0},};


    printf("\n\nPrinting afs global locks...\n\n");
    for (i = 0; locks[i].name; i++) {
	findsym(locks[i].name, &symoff);
	kread(kmem, symoff, (char *)&count, sizeof count);
	printf("%s = 0x%x\n", locks[i].name, count);
    }
}


void
print_global_afs_resource(kmem)
     int kmem;
{
    off_t symoff;
    char sysname[100];
    afs_int32 count;
    long addr;

    printf("\n\nPrinting some general resource related globals...\n\n");
    findsym("afs_setTimeHost", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tafs_setTimeHost = 0x%x\n", count);
    findsym("afs_volCounter", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tafs_volCounter = 0x%x\n", count);
    findsym("afs_cellindex", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tafs_cellIndex = 0x%x\n", count);
    findsym("afs_marinerHost", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tafs_marinerHost = 0x%x\n", count);
    findsym("afs_sysname", &symoff);
    kread(kmem, symoff, (char *)&addr, sizeof addr);
#ifdef	AFS_HPUX_ENV
    printf("\tafs_sysname = %d\n", addr);
#else
    kread(kmem, (off_t) addr, sysname, (KDUMP_SIZE_T) 30);
    printf("\tafs_sysname = %s\n", sysname);
#endif
#ifdef AFS_SGI65_ENV
    findsym("afs_ipno", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tCPU BOARD = IP%d\n", count);
#endif
}


void
print_global_afs_cache(kmem)
     int kmem;
{
    off_t symoff;
    char sysname[100];
    afs_int32 count;
#ifdef AFS_SGI62_ENV
    ino64_t inode;
#endif
#ifndef	AFS32
    afs_hyper_t h;
#endif

    printf("\n\nPrinting some general cache related globals...\n\n");
    findsym("afs_mariner", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tafs_mariner = 0x%x\n", count);
#ifndef	AFS_OSF_ENV
    findsym("freeVCList", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tafs_freeVCList = 0x%x XXX\n", count);
#endif
    findsym("afs_freeDCList", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tfreeDCList = 0x%x\n", count);
    findsym("afs_freeDCCount", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tfreeDCCount = 0x%x (%d)\n", count, count);
    findsym("afs_discardDCList", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tdiscardDCList = 0x%x\n", count);
    findsym("afs_discardDCCount", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tdiscardDCCount = 0x%x (%d)\n", count, count);
    findsym("afs_freeDSList", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tfreeDSList= 0x%x XXXX\n", count);
#ifdef AFS_SGI62_ENV
    findsym("cacheInode", &symoff);
    kread(kmem, symoff, (char *)&inode, sizeof inode);
    printf("\tcacheInode = 0x%llx (%lld)\n", inode, inode);
    findsym("volumeInode", &symoff);
    kread(kmem, symoff, (char *)&inode, sizeof inode);
    printf("\tvolumeInode = 0x%llx (%lld)\n", inode, inode);
#else
    findsym("cacheInode", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tcacheInode = 0x%x (%d)\n", count, count);
    findsym("volumeInode", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tvolumeInode = 0x%x (%d)\n", count, count);
#endif
    findsym("cacheDiskType", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tcacheDiskType = 0x%x (%d)\n", count, count);
#ifndef	AFS32
    findsym("afs_indexCounter", &symoff);
    kread(kmem, symoff, (char *)&h, sizeof(struct afs_hyper_t));
    printf("\tafs_indexCounter = 0x%X.%X (%d.%d)\n", h.high, h.low, h.high,
	   h.low);
#endif
    findsym("afs_cacheFiles", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tafs_cacheFiles = 0x%x (%d)\n", count, count);
    findsym("afs_cacheBlocks", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tafs_cacheBlocks = 0x%x (%d)\n", count, count);
    findsym("afs_cacheStats", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tafs_cacheStats = 0x%x (%d)\n", count, count);
    findsym("afs_blocksUsed", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tafs_blocksUsed = 0x%x (%d)\n", count, count);
    findsym("afs_blocksDiscarded", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tafs_blocksDiscarded = 0x%x (%d)\n", count, count);
    findsym("afs_fsfragsize", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tafs_fsfragsize = 0x%x\n", count);
    findsym("afs_WaitForCacheDrain", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tafs_WaitForCacheDrain = 0x%x (%d)\n", count, count);
    findsym("afs_CacheTooFull", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\tafs_CacheTooFull = 0x%x (%d)\n", count, count);


    if (findsym("pagCounter", &symoff)) {
	kread(kmem, symoff, (char *)&count, sizeof count);
	printf("\tpagCounter = 0x%x (%d)\n", count, count);
    } else {
	printf("Ignoring pagCounter\n");
    }
}


void
print_rxstats(kmem)
     int kmem;
{
    off_t symoff;
    char sysname[100];
    afs_int32 count, i;
    struct rx_stats rx_stats;

    printf("\n\nPrinting some general RX stats...\n\n");
    findsym("rx_stats", &symoff);
    kread(kmem, symoff, (char *)&rx_stats, sizeof rx_stats);
    printf("\t\tpacketRequests = %d\n", rx_stats.packetRequests);
    printf("\t\tnoPackets[%d] = %d\n", RX_PACKET_CLASS_RECEIVE,
	   rx_stats.receivePktAllocFailures);
    printf("\t\tnoPackets[%d] = %d\n", RX_PACKET_CLASS_SEND,
	   rx_stats.sendPktAllocFailures);
    printf("\t\tnoPackets[%d] = %d\n", RX_PACKET_CLASS_SPECIAL,
	   rx_stats.specialPktAllocFailures);
    printf("\t\tnoPackets[%d] = %d\n", RX_PACKET_CLASS_RECV_CBUF,
	   rx_stats.receiveCbufPktAllocFailures);
    printf("\t\tnoPackets[%d] = %d\n", RX_PACKET_CLASS_SEND_CBUF,
	   rx_stats.sendCbufPktAllocFailures);
    printf("\t\tsocketGreedy = %d\n", rx_stats.socketGreedy);
    printf("\t\tbogusPacketOnRead = %d\n", rx_stats.bogusPacketOnRead);
    printf("\t\tbogusHost = %d\n", rx_stats.bogusHost);
    printf("\t\tnoPacketOnRead = %d\n", rx_stats.noPacketOnRead);
    printf("\t\tnoPacketBuffersOnRead = %d\n",
	   rx_stats.noPacketBuffersOnRead);
    printf("\t\tselects = %d\n", rx_stats.selects);
    printf("\t\tsendSelects = %d\n", rx_stats.sendSelects);
    for (i = 0; i < RX_N_PACKET_TYPES; i++)
	printf("\t\tpacketsRead[%d] = %d\n", i, rx_stats.packetsRead[i]);
    printf("\t\tdataPacketsRead = %d\n", rx_stats.dataPacketsRead);
    printf("\t\tackPacketsRead = %d\n", rx_stats.ackPacketsRead);
    printf("\t\tdupPacketsRead = %d\n", rx_stats.dupPacketsRead);
    printf("\t\tspuriousPacketsRead = %d\n", rx_stats.spuriousPacketsRead);
    for (i = 0; i < RX_N_PACKET_TYPES; i++)
	printf("\t\tpacketsSent[%d] = %d\n", i, rx_stats.packetsSent[i]);
    printf("\t\tackPacketsSent = %d\n", rx_stats.ackPacketsSent);
    printf("\t\tpingPacketsSent = %d\n", rx_stats.pingPacketsSent);
    printf("\t\tabortPacketsSent = %d\n", rx_stats.abortPacketsSent);
    printf("\t\tbusyPacketsSent = %d\n", rx_stats.busyPacketsSent);
    printf("\t\tdataPacketsSent = %d\n", rx_stats.dataPacketsSent);
    printf("\t\tdataPacketsReSent = %d\n", rx_stats.dataPacketsReSent);
    printf("\t\tdataPacketsPushed = %d\n", rx_stats.dataPacketsPushed);
    printf("\t\tignoreAckedPacket = %d\n", rx_stats.ignoreAckedPacket);
    printf("\t\ttotalRtt = %d sec, %d usec\n", rx_stats.totalRtt.sec,
	   rx_stats.totalRtt.usec);
    printf("\t\tminRtt = %d sec, %d usec\n", rx_stats.minRtt.sec,
	   rx_stats.minRtt.usec);
    printf("\t\tmaxRtt = %d sec, %d usec\n", rx_stats.maxRtt.sec,
	   rx_stats.maxRtt.usec);
    printf("\t\tnRttSamples = %d\n", rx_stats.nRttSamples);
    printf("\t\tnServerConns = %d\n", rx_stats.nServerConns);
    printf("\t\tnClientConns = %d\n", rx_stats.nClientConns);
    printf("\t\tnPeerStructs = %d\n", rx_stats.nPeerStructs);
    printf("\t\tnCallStructs = %d\n", rx_stats.nCallStructs);
    printf("\t\tnFreeCallStructs = %d\n", rx_stats.nFreeCallStructs);
    printf("\t\tnetSendFailures  = %d\n", rx_stats.netSendFailures);
    printf("\t\tfatalErrors      = %d\n", rx_stats.fatalErrors);
}


void
print_rx(kmem)
     int kmem;
{
    off_t symoff;
    char sysname[100], c;
    afs_int32 count, i, ar[100];
    short sm;
    struct rx_stats rx_stats;

    printf("\n\nPrinting some RX globals...\n\n");
    findsym("rx_extraQuota", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\trx_extraQuota = %d\n", count);
    findsym("rx_extraPackets", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\trx_extraPackets = %d\n", count);
    findsym("rx_stackSize", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\trx_stackSize = %d\n", count);
    findsym("rx_connDeadTime", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);

    printf("\trx_connDeadTime = %d\n", count);
    findsym("rx_idleConnectionTime", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);

    printf("\trx_idleConnectionTime = %d\n", count);

    findsym("rx_idlePeerTime", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\trx_idlePeerTime = %d\n", count);

    findsym("rx_initSendWindow", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\trx_initSendWindow = %d\n", count);

    findsym("rxi_nSendFrags", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);
    printf("\trxi_nSendFrags = %d\n", count);

    findsym("rx_nPackets", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);

    printf("\trx_nPackets = %d\n", count);
    findsym("rx_nFreePackets", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);

    printf("\trx_nFreePackets = %d\n", count);
    findsym("rx_socket", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);

    printf("\trx_socket = 0x%x\n", count);
    findsym("rx_port", &symoff);
    kread(kmem, symoff, (char *)&sm, sizeof sm);

    printf("\trx_Port = %d\n", sm);
    findsym("rx_packetQuota", &symoff);
    kread(kmem, symoff, (char *)ar, sizeof ar);

    for (i = 0; i < RX_N_PACKET_CLASSES; i++)
	printf("\trx_packetQuota[%d] = %d\n", i, ar[i]);
    findsym("rx_nextCid", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);

    printf("\trx_nextCid = 0x%x\n", count);
    findsym("rx_epoch", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);

    printf("\trx_epoch = 0u%u\n", count);
    findsym("rx_waitingForPackets", &symoff);
    kread(kmem, symoff, (char *)&c, sizeof(c));

    printf("\trx_waitingForPackets = %x\n", (int)c);
    findsym("rxi_nCalls", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);

    printf("\trxi_nCalls = %d\n", count);
    findsym("rxi_dataQuota", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);

    printf("\trxi_dataQuota = %d\n", count);
#ifdef	AFS_AIX_ENV
    if (findsym("rxi_Alloccnt", &symoff)) {
	kread(kmem, symoff, (char *)&count, sizeof count);
	printf("\trxi_Alloccnt = %d\n", count);
    }

    if (findsym("rxi_Allocsize", &symoff)) {
	kread(kmem, symoff, (char *)&count, sizeof count);
	printf("\trxi_Allocsize = %d\n", count);
    }
#endif
    findsym("rxi_availProcs", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);

    printf("\trxi_availProcs = %d\n", count);
    findsym("rxi_totalMin", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);

    printf("\trxi_totalMin = %d\n", count);
    findsym("rxi_minDeficit", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof count);

    printf("\trxi_minDeficit = %d\n", count);
    print_services(kmem);
#ifdef KDUMP_RX_LOCK
    if (use_rx_lock) {
	print_peertable_lock(kmem);
	print_conntable_lock(kmem);
	print_calltable_lock(kmem);
    } else {
	print_peertable(kmem);
	print_conntable(kmem);
	print_calltable(kmem);
    }
#else
    print_peertable(kmem);
    print_conntable(kmem);
    print_calltable(kmem);
#endif
    print_eventtable(kmem);
    print_rxstats(kmem);
}


void
print_services(kmem)
     afs_int32 kmem;
{
    off_t symoff;
    struct rx_service *rx_services[RX_MAX_SERVICES], se, *sentry = &se, *sep;
    char sysname[100];
    afs_int32 count, i, j;

    findsym("rx_services", &symoff);
    kread(kmem, symoff, (char *)rx_services, RX_MAX_SERVICES * sizeof(long));

    printf("\n\nPrinting all 'rx_services' structures...\n");
    for (i = 0, j = 0; i < RX_MAX_SERVICES; i++) {
	if (rx_services[i]) {
	    j++;
	    kread(kmem, (off_t) rx_services[i], (char *)sentry,
		  sizeof *sentry);
	    kread(kmem, (off_t) sentry->serviceName, sysname,
		  (KDUMP_SIZE_T) 40);
	    printf
		("\t%lx: serviceId=%d, port=%d, serviceName=%s, socket=0x%x\n",
		 rx_services[i], sentry->serviceId, sentry->servicePort,
		 sysname, sentry->socket);
	    printf
		("\t\tnSecObj=%d, nReqRunning=%d, maxProcs=%d, minProcs=%d, connDeadTime=%d, idleDeadTime=%d\n",
		 sentry->nSecurityObjects, sentry->nRequestsRunning,
		 sentry->maxProcs, sentry->minProcs, sentry->connDeadTime,
		 sentry->idleDeadTime);
	}
    }
    printf("... found %d 'rx_services' entries in the table\n", j);
}


#ifdef KDUMP_RX_LOCK
void
print_peertable_lock(kmem)
     afs_int32 kmem;
{
    off_t symoff;
    struct rx_peer_rx_lock *rx_peerTable[256], se, *sentry = &se, *sep;
    long count, i, j;

    findsym("rx_peerHashTable", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof(long));
    if (!count) {
	printf("No 'rx_peer' structures found.\n");
	return;
    }

    kread(kmem, count, (char *)rx_peerTable, 256 * sizeof(long));
    printf("\n\nPrinting all 'rx_peer' structures...\n");
    for (i = 0, j = 0; i < 256; i++) {
	for (sep = rx_peerTable[i]; sep; sep = sentry->next, j++) {
	    kread(kmem, (off_t) sep, (char *)sentry, sizeof *sentry);
	    printf("\t%lx: next=0x%lx, host=0x%x, ", sep, sentry->next,
		   sentry->host);
	    printf("ifMTU=%d, natMTU=%d, maxMTU=%d\n", sentry->ifMTU,
		   sentry->natMTU, sentry->maxMTU);
	    printf("\t\trtt=%d:%d, timeout(%d:%d), nSent=%d, reSends=%d\n",
		   sentry->rtt, sentry->rtt_dev, sentry->timeout.sec,
		   sentry->timeout.usec, sentry->nSent, sentry->reSends);
	    printf("\t\trefCount=%d, port=%d, idleWhen=0x%x\n",
		   sentry->refCount, sentry->port, sentry->idleWhen);
	    printf
		("\t\tCongestionQueue (0x%x:0x%x), inPacketSkew=0x%x, outPacketSkew=0x%x\n",
		 sentry->congestionQueue.prev, sentry->congestionQueue.next,
		 sentry->inPacketSkew, sentry->outPacketSkew);
	    printf("\t\tpeer_lock=%d\n", sentry->peer_lock);
	}
    }
    printf("... found %d 'rx_peer' entries in the table\n", j);
}

#endif /* KDUMP_RX_LOCK */
void
print_peertable(kmem)
     afs_int32 kmem;
{
    off_t symoff;
    struct rx_peer *rx_peerTable[256], se, *sentry = &se, *sep;
    long count, i, j;

    findsym("rx_peerHashTable", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof(long));

    kread(kmem, count, (char *)rx_peerTable, 256 * sizeof(long));
    printf("\n\nPrinting all 'rx_peer' structures...\n");
    for (i = 0, j = 0; i < 256; i++) {
	for (sep = rx_peerTable[i]; sep; sep = sentry->next, j++) {
	    kread(kmem, (off_t) sep, (char *)sentry, sizeof *sentry);
	    printf("\t%lx: next=0x%lx, host=0x%x, ", sep, sentry->next,
		   sentry->host);
	    printf("ifMTU=%d, natMTU=%d, maxMTU=%d\n", sentry->ifMTU,
		   sentry->natMTU, sentry->maxMTU);
	    printf("\t\trtt=%d:%d, timeout(%d:%d), nSent=%d, reSends=%d\n",
		   sentry->rtt, sentry->rtt_dev, sentry->timeout.sec,
		   sentry->timeout.usec, sentry->nSent, sentry->reSends);
	    printf("\t\trefCount=%d, port=%d, idleWhen=0x%x\n",
		   sentry->refCount, sentry->port, sentry->idleWhen);
	    printf
		("\t\tCongestionQueue (0x%x:0x%x), inPacketSkew=0x%x, outPacketSkew=0x%x\n",
		 sentry->congestionQueue.prev, sentry->congestionQueue.next,
		 sentry->inPacketSkew, sentry->outPacketSkew);
#ifdef RX_ENABLE_LOCKS
	    printf("\t\tpeer_lock=%d\n", sentry->peer_lock);
#endif /* RX_ENABLE_LOCKS */
	}
    }
    printf("... found %d 'rx_peer' entries in the table\n", j);
}


#ifdef KDUMP_RX_LOCK
void
print_conntable_lock(kmem)
     afs_int32 kmem;
{
    off_t symoff;
    struct rx_connection_rx_lock *rx_connTable[256], se, *sentry = &se;
    struct rx_connection_rx_lock *sep;
    long count, i, j;

    findsym("rx_connHashTable", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof(long));
    if (!count) {
	printf("No 'rx_connection' structures found.\n");
	return;
    }

    kread(kmem, count, (char *)rx_connTable, 256 * sizeof(long));
    printf("\n\nPrinting all 'rx_connection' structures...\n");
    for (i = 0, j = 0; i < 256; i++) {
	for (sep = rx_connTable[i]; sep; sep = sentry->next, j++) {
	    kread(kmem, (off_t) sep, (char *)sentry, sizeof *sentry);
	    printf
		("\t%lx: next=0x%lx, peer=0x%lx, epoch=0x%x, cid=0x%x, ackRate=%d\n",
		 sep, se.next, se.peer, se.epoch, se.cid, se.ackRate);
	    printf("\t\tcall[%lx=%d, %lx=%d, %lx=%d, %lx=%d]\n", se.call[0],
		   se.callNumber[0], se.call[1], se.callNumber[1], se.call[2],
		   se.callNumber[2], se.call[3], se.callNumber[3]);
	    printf
		("\t\ttimeout=%d, flags=0x%x, type=0x%x, serviceId=%d, service=0x%lx, refCount=%d\n",
		 se.timeout, se.flags, se.type, se.serviceId, se.service,
		 se.refCount);
	    printf
		("\t\tserial=%d, lastSerial=%d, secsUntilDead=%d, secsUntilPing=%d, secIndex=%d\n",
		 se.serial, se.lastSerial, se.secondsUntilDead,
		 se.secondsUntilPing, se.securityIndex);
	    printf
		("\t\terror=%d, secObject=0x%lx, secData=0x%lx, secHeaderSize=%d, secmaxTrailerSize=%d\n",
		 se.error, se.securityObject, se.securityData,
		 se.securityHeaderSize, se.securityMaxTrailerSize);
	    printf
		("\t\tchallEvent=0x%lx, lastSendTime=0x%x, maxSerial=%d, hardDeadTime=%d\n",
		 se.challengeEvent, se.lastSendTime, se.maxSerial,
		 se.hardDeadTime);
	    if (se.flags & RX_CONN_MAKECALL_WAITING)
		printf
		    ("\t\t***** Conn in RX_CONN_MAKECALL_WAITING state *****\n");
	    printf
		("\t\tcall_lock=%d, call_cv=%d, data_lock=%d, refCount=%d\n",
		 se.conn_call_lock, se.conn_call_cv, se.conn_data_lock,
		 se.refCount);
	}
    }
    printf("... found %d 'rx_connection' entries in the table\n", j);
}
#endif /* KDUMP_RX_LOCK */

void
print_conntable(kmem)
     afs_int32 kmem;
{
    off_t symoff;
    struct rx_connection *rx_connTable[256], se, *sentry = &se, *sep;
    long count, i, j;

    findsym("rx_connHashTable", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof(long));

    kread(kmem, count, (char *)rx_connTable, 256 * sizeof(long));
    printf("\n\nPrinting all 'rx_connection' structures...\n");
    for (i = 0, j = 0; i < 256; i++) {
	for (sep = rx_connTable[i]; sep; sep = sentry->next, j++) {
	    kread(kmem, (off_t) sep, (char *)sentry, sizeof *sentry);
	    printf
		("\t%lx: next=0x%lx, peer=0x%lx, epoch=0x%x, cid=0x%x, ackRate=%d\n",
		 sep, se.next, se.peer, se.epoch, se.cid, se.ackRate);
	    printf("\t\tcall[%x=%d, %x=%d, %x=%d, %x=%d]\n", se.call[0],
		   se.callNumber[0], se.call[1], se.callNumber[1], se.call[2],
		   se.callNumber[2], se.call[3], se.callNumber[3]);
	    printf
		("\t\ttimeout=%d, flags=0x%x, type=0x%x, serviceId=%d, service=0x%lx, refCount=%d\n",
		 se.timeout, se.flags, se.type, se.serviceId, se.service,
		 se.refCount);
	    printf
		("\t\tserial=%d, lastSerial=%d, secsUntilDead=%d, secsUntilPing=%d, secIndex=%d\n",
		 se.serial, se.lastSerial, se.secondsUntilDead,
		 se.secondsUntilPing, se.securityIndex);
	    printf
		("\t\terror=%d, secObject=0x%lx, secData=0x%lx, secHeaderSize=%d, secmaxTrailerSize=%d\n",
		 se.error, se.securityObject, se.securityData,
		 se.securityHeaderSize, se.securityMaxTrailerSize);
	    printf
		("\t\tchallEvent=0x%lx, lastSendTime=0x%x, maxSerial=%d, hardDeadTime=%d\n",
		 se.challengeEvent, se.lastSendTime, se.maxSerial,
		 se.hardDeadTime);
	    if (se.flags & RX_CONN_MAKECALL_WAITING)
		printf
		    ("\t\t***** Conn in RX_CONN_MAKECALL_WAITING state *****\n");
#ifdef RX_ENABLE_LOCKS
	    printf
		("\t\tcall_lock=%d, call_cv=%d, data_lock=%d, refCount=%d\n",
		 se.conn_call_lock, se.conn_call_cv, se.conn_data_lock,
		 se.refCount);
#endif /* RX_ENABLE_LOCKS */
	}
    }
    printf("... found %d 'rx_connection' entries in the table\n", j);
}


#ifdef KDUMP_RX_LOCK
void
print_calltable_lock(kmem)
     afs_int32 kmem;
{
    off_t symoff;
    struct rx_connection_rx_lock *rx_connTable[256], se;
    struct rx_connection_rx_lock *sentry = &se;
    struct rx_connection_rx_lock *sep;
    long count, i, j, k;

    findsym("rx_connHashTable", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof(long));
    if (!count) {
	printf("No 'rx_call' structures found.\n");
	return;
    }

    kread(kmem, count, (char *)rx_connTable, 256 * sizeof(long));
    printf("\n\nPrinting all active 'rx_call' structures...\n");
    for (i = 0, j = 0; i < 256; i++) {
	for (sep = rx_connTable[i]; sep; sep = se.next) {
	    kread(kmem, (off_t) sep, (char *)sentry, sizeof *sentry);
	    for (k = 0; k < RX_MAXCALLS; k++) {
		struct rx_call_rx_lock ce, *centry = &ce;
		struct rx_call_rx_lock *call = se.call[k];
		if (call) {
		    j++;
		    kread(kmem, (off_t) call, (char *)centry, sizeof *centry);
		    printf
			("\t%lx: conn=0x%lx, qiheader(0x%lx:0x%lx), tq(0x%lx:0x%lx), rq(0x%lx:0x%lx)\n",
			 call, centry->conn, centry->queue_item_header.prev,
			 centry->queue_item_header.next, centry->tq.prev,
			 centry->tq.next, centry->rq.prev, centry->rq.next);
		    printf
			("\t\t: curvec=%d, curpos=%d, nLeft=%d, nFree=%d, currPacket=0x%lx, callNumber=0x%x\n",
			 centry->curvec, centry->curpos, centry->nLeft,
			 centry->nFree, centry->currentPacket,
			 centry->callNumber);
		    printf
			("\t\t: channel=%d, state=0x%x, mode=0x%x, flags=0x%x, localStatus=0x%x, remStatus=0x%x\n",
			 centry->channel, centry->state, centry->mode,
			 centry->flags, centry->localStatus,
			 centry->remoteStatus);
		    printf
			("\t\t: error=%d, timeout=0x%x, rnext=0x%x, rprev=0x%x, rwind=0x%x, tfirst=0x%x, tnext=0x%x\n",
			 centry->error, centry->timeout, centry->rnext,
			 centry->rprev, centry->rwind, centry->tfirst,
			 centry->tnext);
		    printf
			("\t\t: twind=%d, resendEvent=0x%lx, timeoutEvent=0x%lx, keepAliveEvent=0x%lx, delayedAckEvent=0x%lx\n",
			 centry->twind, centry->resendEvent,
			 centry->timeoutEvent, centry->keepAliveEvent,
			 centry->delayedAckEvent);
		    printf
			("\t\t: lastSendTime=0x%x, lastReceiveTime=0x%x, lastAcked=0x%x, startTime=0x%x, startWait=0x%x\n",
			 centry->lastSendTime, centry->lastReceiveTime,
			 centry->lastAcked, centry->startTime,
			 centry->startWait);
		    if (centry->flags & RX_CALL_WAIT_PROC)
			printf
			    ("\t\t******** Call in RX_CALL_WAIT_PROC state **********\n");
		    if (centry->flags & RX_CALL_WAIT_WINDOW_ALLOC)
			printf
			    ("\t\t******** Call in RX_CALL_WAIT_WINDOW_ALLOC state **********\n");
		    if (centry->flags & RX_CALL_READER_WAIT)
			printf
			    ("\t\t******** Conn in RX_CALL_READER_WAIT state **********\n");
		    if (centry->flags & RX_CALL_WAIT_PACKETS)
			printf
			    ("\t\t******** Conn in RX_CALL_WAIT_PACKETS state **********\n");
		    printf
			("\t\t: lock=0x%x, cv_twind=0x%x, cv_rq=0x%x, refCount=%d\n",
			 centry->lock, centry->cv_twind, centry->cv_rq,
			 centry->refCount);
		    printf("\t\t: MTU=%d\n", centry->MTU);
		}
	    }
	}
    }
    printf("... found %d 'rx_call' entries in the table\n", j);
}
#endif /* KDUMP_RX_LOCK */

void
print_calltable(kmem)
     afs_int32 kmem;
{
    off_t symoff;
    struct rx_connection *rx_connTable[256], se, *sentry = &se, *sep;
    long count, i, j, k;

    findsym("rx_connHashTable", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof(long));

    kread(kmem, count, (char *)rx_connTable, 256 * sizeof(long));
    printf("\n\nPrinting all active 'rx_call' structures...\n");
    for (i = 0, j = 0; i < 256; i++) {
	for (sep = rx_connTable[i]; sep; sep = se.next) {
	    kread(kmem, (off_t) sep, (char *)sentry, sizeof *sentry);
	    for (k = 0; k < RX_MAXCALLS; k++) {
		struct rx_call ce, *centry = &ce, *call = se.call[k];
		if (call) {
		    j++;
		    kread(kmem, (off_t) call, (char *)centry, sizeof *centry);
		    printf
			("\t%lx: conn=0x%lx, qiheader(0x%lx:0x%lx), tq(0x%lx:0x%lx), rq(0x%lx:0x%lx)\n",
			 call, centry->conn, centry->queue_item_header.prev,
			 centry->queue_item_header.next, centry->tq.prev,
			 centry->tq.next, centry->rq.prev, centry->rq.next);
		    printf
			("\t\t: curvec=%d, curpos=%d, nLeft=%d, nFree=%d, currPacket=0x%lx, callNumber=0x%x\n",
			 centry->curvec, centry->curpos, centry->nLeft,
			 centry->nFree, centry->currentPacket,
			 centry->callNumber);
		    printf
			("\t\t: channel=%d, state=0x%x, mode=0x%x, flags=0x%x, localStatus=0x%x, remStatus=0x%x\n",
			 centry->channel, centry->state, centry->mode,
			 centry->flags, centry->localStatus,
			 centry->remoteStatus);
		    printf
			("\t\t: error=%d, timeout=0x%x, rnext=0x%x, rprev=0x%x, rwind=0x%x, tfirst=0x%x, tnext=0x%x\n",
			 centry->error, centry->timeout, centry->rnext,
			 centry->rprev, centry->rwind, centry->tfirst,
			 centry->tnext);
		    printf
			("\t\t: twind=%d, resendEvent=0x%lx, timeoutEvent=0x%lx, keepAliveEvent=0x%lx, delayedAckEvent=0x%lx\n",
			 centry->twind, centry->resendEvent,
			 centry->timeoutEvent, centry->keepAliveEvent,
			 centry->delayedAckEvent);
		    printf
			("\t\t: lastSendTime=0x%x, lastReceiveTime=0x%x, lastAcked=0x%x, startTime=0x%x, startWait=0x%x\n",
			 centry->lastSendTime, centry->lastReceiveTime,
			 centry->lastAcked, centry->startTime,
			 centry->startWait);
		    if (centry->flags & RX_CALL_WAIT_PROC)
			printf
			    ("\t\t******** Call in RX_CALL_WAIT_PROC state **********\n");
		    if (centry->flags & RX_CALL_WAIT_WINDOW_ALLOC)
			printf
			    ("\t\t******** Call in RX_CALL_WAIT_WINDOW_ALLOC state **********\n");
		    if (centry->flags & RX_CALL_READER_WAIT)
			printf
			    ("\t\t******** Conn in RX_CALL_READER_WAIT state **********\n");
		    if (centry->flags & RX_CALL_WAIT_PACKETS)
			printf
			    ("\t\t******** Conn in RX_CALL_WAIT_PACKETS state **********\n");
#ifdef RX_ENABLE_LOCKS
		    printf
			("\t\t: lock=0x%x, cv_twind=0x%x, cv_rq=0x%x, refCount=%d\n",
			 centry->lock, centry->cv_twind, centry->cv_rq,
			 centry->refCount);
#endif /* RX_ENABLE_LOCKS */
		    printf("\t\t: MTU=%d\n", centry->MTU);
		}
	    }
	}
    }
    printf("... found %d 'rx_call' entries in the table\n", j);
}

void
print_eventtable(kmem)
     afs_int32 kmem;
{
    off_t symoff;
    struct rx_queue epq;
    struct rx_queue evq;
    char *epend, *evend;
    afs_int32 count, i, j = 0, k = 0;

#if ! defined(AFS_HPUX_ENV) && ! defined(AFS_AIX_ENV)
    findsym("rxevent_nFree", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof(afs_int32));
    printf("\n\n\trxevent_nFree = %d\n", count);

    findsym("rxevent_nPosted", &symoff);
    kread(kmem, symoff, (char *)&count, sizeof(afs_int32));
    printf("\trxevent_nPosted = %d\n", count);
#endif
}

/*
 * print_upDownStats
 *
 * Print the up/downtime stats for the given class of server records
 * provided.
 */
void
print_upDownStats(a_upDownP)
     struct afs_stats_SrvUpDownInfo *a_upDownP;	/*Ptr to server up/down info */

{				/*print_upDownStats */

    /*
     * First, print the simple values.
     */
    printf("\t\t%10d numTtlRecords\n", a_upDownP->numTtlRecords);
    printf("\t\t%10d numUpRecords\n", a_upDownP->numUpRecords);
    printf("\t\t%10d numDownRecords\n", a_upDownP->numDownRecords);
    printf("\t\t%10d sumOfRecordAges\n", a_upDownP->sumOfRecordAges);
    printf("\t\t%10d ageOfYoungestRecord\n", a_upDownP->ageOfYoungestRecord);
    printf("\t\t%10d ageOfOldestRecord\n", a_upDownP->ageOfOldestRecord);
    printf("\t\t%10d numDowntimeIncidents\n",
	   a_upDownP->numDowntimeIncidents);
    printf("\t\t%10d numRecordsNeverDown\n", a_upDownP->numRecordsNeverDown);
    printf("\t\t%10d maxDowntimesInARecord\n",
	   a_upDownP->maxDowntimesInARecord);
    printf("\t\t%10d sumOfDowntimes\n", a_upDownP->sumOfDowntimes);
    printf("\t\t%10d shortestDowntime\n", a_upDownP->shortestDowntime);
    printf("\t\t%10d longestDowntime\n", a_upDownP->longestDowntime);

    /*
     * Now, print the array values.
     */
    printf("\t\tDowntime duration distribution:\n");
    printf("\t\t\t%8d: 0 min .. 10 min\n", a_upDownP->downDurations[0]);
    printf("\t\t\t%8d: 10 min .. 30 min\n", a_upDownP->downDurations[1]);
    printf("\t\t\t%8d: 30 min .. 1 hr\n", a_upDownP->downDurations[2]);
    printf("\t\t\t%8d: 1 hr .. 2 hr\n", a_upDownP->downDurations[3]);
    printf("\t\t\t%8d: 2 hr .. 4 hr\n", a_upDownP->downDurations[4]);
    printf("\t\t\t%8d: 4 hr .. 8 hr\n", a_upDownP->downDurations[5]);
    printf("\t\t\t%8d: > 8 hr\n", a_upDownP->downDurations[6]);

    printf("\t\tDowntime incident distribution:\n");
    printf("\t\t\t%8d: 0 times\n", a_upDownP->downIncidents[0]);
    printf("\t\t\t%8d: 1 time\n", a_upDownP->downDurations[1]);
    printf("\t\t\t%8d: 2 .. 5 times\n", a_upDownP->downDurations[2]);
    printf("\t\t\t%8d: 6 .. 10 times\n", a_upDownP->downDurations[3]);
    printf("\t\t\t%8d: 10 .. 50 times\n", a_upDownP->downDurations[4]);
    printf("\t\t\t%8d: > 50 times\n", a_upDownP->downDurations[5]);
}				/*print_upDownStats */


void
print_cmperfstats(perfP)
     struct afs_stats_CMPerf *perfP;
{
    struct afs_stats_SrvUpDownInfo *upDownP;	/*Ptr to server up/down info */

    printf("\t%10d numPerfCalls\n", perfP->numPerfCalls);
    printf("\t%10d epoch\n", perfP->epoch);
    printf("\t%10d numCellsVisible\n", perfP->numCellsVisible);
    printf("\t%10d numCellsContacted\n", perfP->numCellsContacted);
    printf("\t%10d dlocalAccesses\n", perfP->dlocalAccesses);
    printf("\t%10d vlocalAccesses\n", perfP->vlocalAccesses);
    printf("\t%10d dremoteAccesses\n", perfP->dremoteAccesses);
    printf("\t%10d vremoteAccesses\n", perfP->vremoteAccesses);
    printf("\t%10d cacheNumEntries\n", perfP->cacheNumEntries);
    printf("\t%10d cacheBlocksTotal\n", perfP->cacheBlocksTotal);
    printf("\t%10d cacheBlocksInUse\n", perfP->cacheBlocksInUse);
    printf("\t%10d cacheBlocksOrig\n", perfP->cacheBlocksOrig);
    printf("\t%10d cacheMaxDirtyChunks\n", perfP->cacheMaxDirtyChunks);
    printf("\t%10d cacheCurrDirtyChunks\n", perfP->cacheCurrDirtyChunks);
    printf("\t%10d dcacheHits\n", perfP->dcacheHits);
    printf("\t%10d vcacheHits\n", perfP->vcacheHits);
    printf("\t%10d dcacheMisses\n", perfP->dcacheMisses);
    printf("\t%10d vcacheMisses\n", perfP->vcacheMisses);
    printf("\t%10d cacheFlushes\n", perfP->cacheFlushes);
    printf("\t%10d cacheFilesReused\n", perfP->cacheFilesReused);
    printf("\t%10d vcacheXAllocs\n", perfP->vcacheXAllocs);
    printf("\t%10d dcacheXAllocs\n", perfP->dcacheXAllocs);

    printf("\t%10d bufAlloced\n", perfP->bufAlloced);
    printf("\t%10d bufHits\n", perfP->bufHits);
    printf("\t%10d bufMisses\n", perfP->bufMisses);
    printf("\t%10d bufFlushDirty\n", perfP->bufFlushDirty);

    printf("\t%10d LargeBlocksActive\n", perfP->LargeBlocksActive);
    printf("\t%10d LargeBlocksAlloced\n", perfP->LargeBlocksAlloced);
    printf("\t%10d SmallBlocksActive\n", perfP->SmallBlocksActive);
    printf("\t%10d SmallBlocksAlloced\n", perfP->SmallBlocksAlloced);
    printf("\t%10d MediumBlocksActive\n", perfP->MediumBlocksActive);
    printf("\t%10d MediumBlocksAlloced\n", perfP->MediumBlocksAlloced);
    printf("\t%10d OutStandingMemUsage\n", perfP->OutStandingMemUsage);
    printf("\t%10d OutStandingAllocs\n", perfP->OutStandingAllocs);
    printf("\t%10d CallBackAlloced\n", perfP->CallBackAlloced);
    printf("\t%10d CallBackFlushes\n", perfP->CallBackFlushes);
    printf("\t%10d CallBackLoops\n", perfP->cbloops);

    printf("\t%10d srvRecords\n", perfP->srvRecords);
    printf("\t%10d srvNumBuckets\n", perfP->srvNumBuckets);
    printf("\t%10d srvMaxChainLength\n", perfP->srvMaxChainLength);
    printf("\t%10d srvRecordsHWM\n", perfP->srvRecordsHWM);
    printf("\t%10d srvMaxChainLengthHWM\n", perfP->srvMaxChainLengthHWM);

    printf("\t%10d sysName_ID\n", perfP->sysName_ID);
    printf("\t%10d osi_Read_EFAULTS\n", perfP->osiread_efaults);

    printf("\tFile Server up/downtimes, same cell:\n");
    print_upDownStats(&(perfP->fs_UpDown[0]));

    printf("\tFile Server up/downtimes, diff cell:\n");
    print_upDownStats(&(perfP->fs_UpDown[1]));

    printf("\tVL Server up/downtimes, same cell:\n");
    print_upDownStats(&(perfP->vl_UpDown[0]));

    printf("\tVL Server up/downtimes, diff cell:\n");
    print_upDownStats(&(perfP->vl_UpDown[1]));
}


void
print_cmstats(cmp)
     struct afs_CMStats *cmp;
{
    printf("\t%10d afs_init\n", cmp->callInfo.C_afs_init);
    printf("\t%10d gop_rdwr\n", cmp->callInfo.C_gop_rdwr);
    printf("\t%10d aix_gnode_rele\n", cmp->callInfo.C_aix_gnode_rele);
    printf("\t%10d gettimeofday\n", cmp->callInfo.C_gettimeofday);
    printf("\t%10d m_cpytoc\n", cmp->callInfo.C_m_cpytoc);
    printf("\t%10d aix_vattr_null\n", cmp->callInfo.C_aix_vattr_null);
    printf("\t%10d afs_gn_frunc\n", cmp->callInfo.C_afs_gn_ftrunc);
    printf("\t%10d afs_gn_rdwr\n", cmp->callInfo.C_afs_gn_rdwr);
    printf("\t%10d afs_gn_ioctl\n", cmp->callInfo.C_afs_gn_ioctl);
    printf("\t%10d afs_gn_locktl\n", cmp->callInfo.C_afs_gn_lockctl);
    printf("\t%10d afs_gn_readlink\n", cmp->callInfo.C_afs_gn_readlink);
    printf("\t%10d afs_gn_readdir\n", cmp->callInfo.C_afs_gn_readdir);
    printf("\t%10d afs_gn_select\n", cmp->callInfo.C_afs_gn_select);
    printf("\t%10d afs_gn_strategy\n", cmp->callInfo.C_afs_gn_strategy);
    printf("\t%10d afs_gn_symlink\n", cmp->callInfo.C_afs_gn_symlink);
    printf("\t%10d afs_gn_revoke\n", cmp->callInfo.C_afs_gn_revoke);
    printf("\t%10d afs_gn_link\n", cmp->callInfo.C_afs_gn_link);
    printf("\t%10d afs_gn_mkdir\n", cmp->callInfo.C_afs_gn_mkdir);
    printf("\t%10d afs_gn_mknod\n", cmp->callInfo.C_afs_gn_mknod);
    printf("\t%10d afs_gn_remove\n", cmp->callInfo.C_afs_gn_remove);
    printf("\t%10d afs_gn_rename\n", cmp->callInfo.C_afs_gn_rename);
    printf("\t%10d afs_gn_rmdir\n", cmp->callInfo.C_afs_gn_rmdir);
    printf("\t%10d afs_gn_fid\n", cmp->callInfo.C_afs_gn_fid);
    printf("\t%10d afs_gn_lookup\n", cmp->callInfo.C_afs_gn_lookup);
    printf("\t%10d afs_gn_open\n", cmp->callInfo.C_afs_gn_open);
    printf("\t%10d afs_gn_create\n", cmp->callInfo.C_afs_gn_create);
    printf("\t%10d afs_gn_hold\n", cmp->callInfo.C_afs_gn_hold);
    printf("\t%10d afs_gn_rele\n", cmp->callInfo.C_afs_gn_rele);
    printf("\t%10d afs_gn_unmap\n", cmp->callInfo.C_afs_gn_unmap);
    printf("\t%10d afs_gn_access\n", cmp->callInfo.C_afs_gn_access);
    printf("\t%10d afs_gn_getattr\n", cmp->callInfo.C_afs_gn_getattr);
    printf("\t%10d afs_gn_setattr\n", cmp->callInfo.C_afs_gn_setattr);
    printf("\t%10d afs_gn_fclear\n", cmp->callInfo.C_afs_gn_fclear);
    printf("\t%10d afs_gn_fsync\n", cmp->callInfo.C_afs_gn_fsync);
    printf("\t%10d phash\n", cmp->callInfo.C_pHash);
    printf("\t%10d DInit\n", cmp->callInfo.C_DInit);
    printf("\t%10d DRead\n", cmp->callInfo.C_DRead);
    printf("\t%10d FixupBucket\n", cmp->callInfo.C_FixupBucket);
    printf("\t%10d afs_newslot\n", cmp->callInfo.C_afs_newslot);
    printf("\t%10d DRelease\n", cmp->callInfo.C_DRelease);
    printf("\t%10d DFlush\n", cmp->callInfo.C_DFlush);
    printf("\t%10d DFlushEntry\n", cmp->callInfo.C_DFlushEntry);
    printf("\t%10d DVOffset\n", cmp->callInfo.C_DVOffset);
    printf("\t%10d DZap\n", cmp->callInfo.C_DZap);
    printf("\t%10d DNew\n", cmp->callInfo.C_DNew);
    printf("\t%10d afs_RemoveVCB\n", cmp->callInfo.C_afs_RemoveVCB);
    printf("\t%10d afs_NewVCache\n", cmp->callInfo.C_afs_NewVCache);
    printf("\t%10d afs_FlushActiveVcaches\n",
	   cmp->callInfo.C_afs_FlushActiveVcaches);
    printf("\t%10d afs_VerifyVCache\n", cmp->callInfo.C_afs_VerifyVCache);
    printf("\t%10d afs_WriteVCache\n", cmp->callInfo.C_afs_WriteVCache);
    printf("\t%10d afs_GetVCache\n", cmp->callInfo.C_afs_GetVCache);
    printf("\t%10d afs_StuffVcache\n", cmp->callInfo.C_afs_StuffVcache);
    printf("\t%10d afs_FindVCache\n", cmp->callInfo.C_afs_FindVCache);
    printf("\t%10d afs_PutDCache\n", cmp->callInfo.C_afs_PutDCache);
    printf("\t%10d afs_PutVCache\n", cmp->callInfo.C_afs_PutVCache);
    printf("\t%10d CacheStoreProc\n", cmp->callInfo.C_CacheStoreProc);
    printf("\t%10d afs_FindDcache\n", cmp->callInfo.C_afs_FindDCache);
    printf("\t%10d afs_TryToSmush\n", cmp->callInfo.C_afs_TryToSmush);
    printf("\t%10d afs_AdjustSize\n", cmp->callInfo.C_afs_AdjustSize);
    printf("\t%10d afs_CheckSize\n", cmp->callInfo.C_afs_CheckSize);
    printf("\t%10d afs_StoreWarn\n", cmp->callInfo.C_afs_StoreWarn);
    printf("\t%10d CacheFetchProc\n", cmp->callInfo.C_CacheFetchProc);
    printf("\t%10d UFS_CacheStoreProc\n", cmp->callInfo.C_UFS_CacheStoreProc);
    printf("\t%10d UFS_CacheFetchProc\n", cmp->callInfo.C_UFS_CacheFetchProc);
    printf("\t%10d afs_GetDCache\n", cmp->callInfo.C_afs_GetDCache);
    printf("\t%10d afs_SimpleVStat\n", cmp->callInfo.C_afs_SimpleVStat);
    printf("\t%10d afs_ProcessFS\n", cmp->callInfo.C_afs_ProcessFS);
    printf("\t%10d afs_InitCacheInfo\n", cmp->callInfo.C_afs_InitCacheInfo);
    printf("\t%10d afs_InitVolumeInfo\n", cmp->callInfo.C_afs_InitVolumeInfo);
    printf("\t%10d afs_InitCacheFile\n", cmp->callInfo.C_afs_InitCacheFile);
    printf("\t%10d afs_CacheInit\n", cmp->callInfo.C_afs_CacheInit);
    printf("\t%10d afs_GetDSlot\n", cmp->callInfo.C_afs_GetDSlot);
    printf("\t%10d afs_WriteThroughDSlots\n",
	   cmp->callInfo.C_afs_WriteThroughDSlots);
    printf("\t%10d afs_MemGetDSlot\n", cmp->callInfo.C_afs_MemGetDSlot);
    printf("\t%10d afs_UFSGetDSlot\n", cmp->callInfo.C_afs_UFSGetDSlot);
    printf("\t%10d afs_StoreDCache\n", cmp->callInfo.C_afs_StoreDCache);
    printf("\t%10d afs_StoreMini\n", cmp->callInfo.C_afs_StoreMini);
    printf("\t%10d afs_StoreAllSegments\n",
	   cmp->callInfo.C_afs_StoreAllSegments);
    printf("\t%10d afs_InvalidateAllSegments\n",
	   cmp->callInfo.C_afs_InvalidateAllSegments);
    printf("\t%10d afs_TruncateAllSegments\n",
	   cmp->callInfo.C_afs_TruncateAllSegments);
    printf("\t%10d afs_CheckVolSync\n", cmp->callInfo.C_afs_CheckVolSync);
    printf("\t%10d afs_wakeup\n", cmp->callInfo.C_afs_wakeup);
    printf("\t%10d afs_CFileOpen\n", cmp->callInfo.C_afs_CFileOpen);
    printf("\t%10d afs_CFileTruncate\n", cmp->callInfo.C_afs_CFileTruncate);
    printf("\t%10d afs_GetDownD\n", cmp->callInfo.C_afs_GetDownD);
    printf("\t%10d afs_WriteDCache\n", cmp->callInfo.C_afs_WriteDCache);
    printf("\t%10d afs_FlushDCache\n", cmp->callInfo.C_afs_FlushDCache);
    printf("\t%10d afs_GetDownDSlot\n", cmp->callInfo.C_afs_GetDownDSlot);
    printf("\t%10d afs_FlushVCache\n", cmp->callInfo.C_afs_FlushVCache);
    printf("\t%10d afs_GetDownV\n", cmp->callInfo.C_afs_GetDownV);
    printf("\t%10d afs_QueueVCB\n", cmp->callInfo.C_afs_QueueVCB);
    printf("\t%10d afs_call\n", cmp->callInfo.C_afs_call);
    printf("\t%10d afs_syscall_call\n", cmp->callInfo.C_afs_syscall_call);
    printf("\t%10d afs_syscall_icreate\n",
	   cmp->callInfo.C_afs_syscall_icreate);
    printf("\t%10d afs_syscall_iopen\n", cmp->callInfo.C_afs_syscall_iopen);
    printf("\t%10d afs_syscall_iincdec\n",
	   cmp->callInfo.C_afs_syscall_iincdec);
    printf("\t%10d afs_syscall_ireadwrite\n",
	   cmp->callInfo.C_afs_syscall_ireadwrite);
    printf("\t%10d afs_syscall\n", cmp->callInfo.C_afs_syscall);
    printf("\t%10d lpioctl\n", cmp->callInfo.C_lpioctl);
    printf("\t%10d lsetpag\n", cmp->callInfo.C_lsetpag);
    printf("\t%10d afs_CheckInit\n", cmp->callInfo.C_afs_CheckInit);
    printf("\t%10d ClearCallback\n", cmp->callInfo.C_ClearCallBack);
    printf("\t%10d SRXAFSCB_GetCE\n", cmp->callInfo.C_SRXAFSCB_GetCE);
    printf("\t%10d SRXAFSCB_GetLock\n", cmp->callInfo.C_SRXAFSCB_GetLock);
    printf("\t%10d SRXAFSCB_CallBack\n", cmp->callInfo.C_SRXAFSCB_CallBack);
    printf("\t%10d SRXAFSCB_InitCallBackState\n",
	   cmp->callInfo.C_SRXAFSCB_InitCallBackState);
    printf("\t%10d SRXAFSCB_Probe\n", cmp->callInfo.C_SRXAFSCB_Probe);
    printf("\t%10d afs_Chunk\n", cmp->callInfo.C_afs_Chunk);
    printf("\t%10d afs_ChunkBase\n", cmp->callInfo.C_afs_ChunkBase);
    printf("\t%10d afs_ChunkOffset\n", cmp->callInfo.C_afs_ChunkOffset);
    printf("\t%10d afs_ChunkSize\n", cmp->callInfo.C_afs_ChunkSize);
    printf("\t%10d afs_ChunkToBase\n", cmp->callInfo.C_afs_ChunkToBase);
    printf("\t%10d afs_ChunkToSize\n", cmp->callInfo.C_afs_ChunkToSize);
    printf("\t%10d afs_SetChunkSize\n", cmp->callInfo.C_afs_SetChunkSize);
    printf("\t%10d afs_config\n", cmp->callInfo.C_afs_config);
    printf("\t%10d mem_freebytes\n", cmp->callInfo.C_mem_freebytes);
    printf("\t%10d mem_getbytes\n", cmp->callInfo.C_mem_getbytes);
    printf("\t%10d afs_Daemon\n", cmp->callInfo.C_afs_Daemon);
    printf("\t%10d afs_CheckRootVolume\n",
	   cmp->callInfo.C_afs_CheckRootVolume);
    printf("\t%10d BPath\n", cmp->callInfo.C_BPath);
    printf("\t%10d BPrefetch\n", cmp->callInfo.C_BPrefetch);
    printf("\t%10d BStore\n", cmp->callInfo.C_BStore);
    printf("\t%10d afs_BBusy\n", cmp->callInfo.C_afs_BBusy);
    printf("\t%10d afs_BQueue\n", cmp->callInfo.C_afs_BQueue);
    printf("\t%10d afs_BRelease\n", cmp->callInfo.C_afs_BRelease);
    printf("\t%10d afs_BackgroundDaemon\n",
	   cmp->callInfo.C_afs_BackgroundDaemon);
    printf("\t%10d exporter_add\n", cmp->callInfo.C_exporter_add);
    printf("\t%10d exporter_find\n", cmp->callInfo.C_exporter_find);
    printf("\t%10d afs_gfs_kalloc\n", cmp->callInfo.C_afs_gfs_kalloc);
    printf("\t%10d afs_gfs_kfree\n", cmp->callInfo.C_afs_gfs_kfree);
    printf("\t%10d gop_lookupname\n", cmp->callInfo.C_gop_lookupname);
    printf("\t%10d afs_uniqtime\n", cmp->callInfo.C_afs_uniqtime);
    printf("\t%10d gfs_vattr_null\n", cmp->callInfo.C_gfs_vattr_null);
    printf("\t%10d afs_lock\n", cmp->callInfo.C_afs_lock);
    printf("\t%10d afs_unlock\n", cmp->callInfo.C_afs_unlock);
    printf("\t%10d afs_update\n", cmp->callInfo.C_afs_update);
    printf("\t%10d afs_gclose\n", cmp->callInfo.C_afs_gclose);
    printf("\t%10d afs_gopen\n", cmp->callInfo.C_afs_gopen);
    printf("\t%10d afs_greadlink\n", cmp->callInfo.C_afs_greadlink);
    printf("\t%10d afs_select\n", cmp->callInfo.C_afs_select);
    printf("\t%10d afs_gbmap\n", cmp->callInfo.C_afs_gbmap);
    printf("\t%10d afs_getfsdata\n", cmp->callInfo.C_afs_getfsdata);
    printf("\t%10d afs_gsymlink\n", cmp->callInfo.C_afs_gsymlink);
    printf("\t%10d afs_namei\n", cmp->callInfo.C_afs_namei);
    printf("\t%10d afs_gmount\n", cmp->callInfo.C_afs_gmount);
    printf("\t%10d afs_gget\n", cmp->callInfo.C_afs_gget);
    printf("\t%10d afs_glink\n", cmp->callInfo.C_afs_glink);
    printf("\t%10d afs_gmkdir\n", cmp->callInfo.C_afs_gmkdir);
    printf("\t%10d afs_unlink\n", cmp->callInfo.C_afs_unlink);
    printf("\t%10d afs_grmdir\n", cmp->callInfo.C_afs_grmdir);
    printf("\t%10d afs_makenode\n", cmp->callInfo.C_afs_makenode);
    printf("\t%10d afs_grename\n", cmp->callInfo.C_afs_grename);
    printf("\t%10d afs_rele\n", cmp->callInfo.C_afs_rele);
    printf("\t%10d afs_syncgp\n", cmp->callInfo.C_afs_syncgp);
    printf("\t%10d afs_getval\n", cmp->callInfo.C_afs_getval);
    printf("\t%10d afs_trunc\n", cmp->callInfo.C_afs_trunc);
    printf("\t%10d afs_rwgp\n", cmp->callInfo.C_afs_rwgp);
    printf("\t%10d afs_stat\n", cmp->callInfo.C_afs_stat);
    printf("\t%10d afsc_link\n", cmp->callInfo.C_afsc_link);
    printf("\t%10d afs_vfs_mount\n", cmp->callInfo.C_afs_vfs_mount);
    printf("\t%10d afs_uniqtime\n", cmp->callInfo.C_afs_uniqtime);
    printf("\t%10d iopen\n", cmp->callInfo.C_iopen);
    printf("\t%10d idec\n", cmp->callInfo.C_idec);
    printf("\t%10d iinc\n", cmp->callInfo.C_iinc);
    printf("\t%10d ireadwrite\n", cmp->callInfo.C_ireadwrite);
    printf("\t%10d iread\n", cmp->callInfo.C_iread);
    printf("\t%10d iwrite\n", cmp->callInfo.C_iwrite);
    printf("\t%10d iforget\n", cmp->callInfo.C_iforget);
    printf("\t%10d icreate\n", cmp->callInfo.C_icreate);
    printf("\t%10d igetinode\n", cmp->callInfo.C_igetinode);
    printf("\t%10d osi_SleepR\n", cmp->callInfo.C_osi_SleepR);
    printf("\t%10d osi_SleepS\n", cmp->callInfo.C_osi_SleepS);
    printf("\t%10d osi_SleepW\n", cmp->callInfo.C_osi_SleepW);
    printf("\t%10d osi_Sleep\n", cmp->callInfo.C_osi_Sleep);
    printf("\t%10d afs_LookupMCE\n", cmp->callInfo.C_afs_LookupMCE);
    printf("\t%10d afs_MemReadBlk\n", cmp->callInfo.C_afs_MemReadBlk);
    printf("\t%10d afs_MemReadUIO\n", cmp->callInfo.C_afs_MemReadUIO);
    printf("\t%10d afs_MemWriteBlk\n", cmp->callInfo.C_afs_MemWriteBlk);
    printf("\t%10d afs_MemWriteUIO\n", cmp->callInfo.C_afs_MemWriteUIO);
    printf("\t%10d afs_MemCacheStoreProc\n",
	   cmp->callInfo.C_afs_MemCacheStoreProc);
    printf("\t%10d afs_MemCacheFetchProc\n",
	   cmp->callInfo.C_afs_MemCacheFetchProc);
    printf("\t%10d afs_MemCacheTruncate\n",
	   cmp->callInfo.C_afs_MemCacheTruncate);
    printf("\t%10d afs_MemCacheStoreProc\n",
	   cmp->callInfo.C_afs_MemCacheStoreProc);
    printf("\t%10d afs_GetNfsClientPag\n",
	   cmp->callInfo.C_afs_GetNfsClientPag);
    printf("\t%10d afs_FindNfsClientPag\n",
	   cmp->callInfo.C_afs_FindNfsClientPag);
    printf("\t%10d afs_PutNfsClientPag\n",
	   cmp->callInfo.C_afs_PutNfsClientPag);
    printf("\t%10d afs_nfsclient_reqhandler\n",
	   cmp->callInfo.C_afs_nfsclient_reqhandler);
    printf("\t%10d afs_nfsclient_GC\n", cmp->callInfo.C_afs_nfsclient_GC);
    printf("\t%10d afs_nfsclient_hold\n", cmp->callInfo.C_afs_nfsclient_hold);
    printf("\t%10d afs_nfsclient_stats\n",
	   cmp->callInfo.C_afs_nfsclient_stats);
    printf("\t%10d afs_nfsclient_sysname\n",
	   cmp->callInfo.C_afs_nfsclient_sysname);
    printf("\t%10d afs_rfs_dispatch\n", cmp->callInfo.C_afs_rfs_dispatch);
    printf("\t%10d afs_nfs2afscall\n", cmp->callInfo.C_Nfs2AfsCall);
    printf("\t%10d afs_sun_xuntext\n", cmp->callInfo.C_afs_sun_xuntext);
    printf("\t%10d osi_Active\n", cmp->callInfo.C_osi_Active);
    printf("\t%10d osi_FlushPages\n", cmp->callInfo.C_osi_FlushPages);
    printf("\t%10d osi_FlushText\n", cmp->callInfo.C_osi_FlushText);
    printf("\t%10d osi_CallProc\n", cmp->callInfo.C_osi_CallProc);
    printf("\t%10d osi_CancelProc\n", cmp->callInfo.C_osi_CancelProc);
    printf("\t%10d osi_Invisible\n", cmp->callInfo.C_osi_Invisible);
    printf("\t%10d osi_Time\n", cmp->callInfo.C_osi_Time);
    printf("\t%10d osi_Alloc\n", cmp->callInfo.C_osi_Alloc);
    printf("\t%10d osi_SetTime\n", cmp->callInfo.C_osi_SetTime);
    printf("\t%10d osi_Dump\n", cmp->callInfo.C_osi_Dump);
    printf("\t%10d osi_Free\n", cmp->callInfo.C_osi_Free);
    printf("\t%10d osi_UFSOpen\n", cmp->callInfo.C_osi_UFSOpen);
    printf("\t%10d osi_Close\n", cmp->callInfo.C_osi_Close);
    printf("\t%10d osi_Stat\n", cmp->callInfo.C_osi_Stat);
    printf("\t%10d osi_Truncate\n", cmp->callInfo.C_osi_Truncate);
    printf("\t%10d osi_Read\n", cmp->callInfo.C_osi_Read);
    printf("\t%10d osi_Write\n", cmp->callInfo.C_osi_Write);
    printf("\t%10d osi_MapStrategy\n", cmp->callInfo.C_osi_MapStrategy);
    printf("\t%10d osi_AllocLargeSpace\n",
	   cmp->callInfo.C_osi_AllocLargeSpace);
    printf("\t%10d osi_FreeLargeSpace\n", cmp->callInfo.C_osi_FreeLargeSpace);
    printf("\t%10d osi_AllocSmallSpace\n",
	   cmp->callInfo.C_osi_AllocSmallSpace);
    printf("\t%10d osi_FreeSmallSpace\n", cmp->callInfo.C_osi_FreeSmallSpace);
    printf("\t%10d osi_CloseToTheEdge\n", cmp->callInfo.C_osi_CloseToTheEdge);
    printf("\t%10d osi_xgreedy\n", cmp->callInfo.C_osi_xgreedy);
    printf("\t%10d osi_FreeSocket\n", cmp->callInfo.C_osi_FreeSocket);
    printf("\t%10d osi_NewSocket\n", cmp->callInfo.C_osi_NewSocket);
    printf("\t%10d osi_NetSend\n", cmp->callInfo.C_osi_NetSend);
    printf("\t%10d WaitHack\n", cmp->callInfo.C_WaitHack);
    printf("\t%10d osi_CancelWait\n", cmp->callInfo.C_osi_CancelWait);
    printf("\t%10d osi_Wakeup\n", cmp->callInfo.C_osi_Wakeup);
    printf("\t%10d osi_Wait\n", cmp->callInfo.C_osi_Wait);
    printf("\t%10d dirp_Read\n", cmp->callInfo.C_dirp_Read);
    printf("\t%10d dirp_Cpy\n", cmp->callInfo.C_dirp_Cpy);
    printf("\t%10d dirp_Eq\n", cmp->callInfo.C_dirp_Eq);
    printf("\t%10d dirp_Write\n", cmp->callInfo.C_dirp_Write);
    printf("\t%10d dirp_Zap\n", cmp->callInfo.C_dirp_Zap);
    printf("\t%10d afs_ioctl\n", cmp->callInfo.C_afs_ioctl);
    printf("\t%10d handleIoctl\n", cmp->callInfo.C_HandleIoctl);
    printf("\t%10d afs_xioctl\n", cmp->callInfo.C_afs_xioctl);
    printf("\t%10d afs_pioctl\n", cmp->callInfo.C_afs_pioctl);
    printf("\t%10d HandlePioctl\n", cmp->callInfo.C_HandlePioctl);
    printf("\t%10d PGetVolumeStatus\n", cmp->callInfo.C_PGetVolumeStatus);
    printf("\t%10d PSetVolumeStatus\n", cmp->callInfo.C_PSetVolumeStatus);
    printf("\t%10d PFlush\n", cmp->callInfo.C_PFlush);
    printf("\t%10d PFlushVolumeData\n", cmp->callInfo.C_PFlushVolumeData);
    printf("\t%10d PNewStatMount\n", cmp->callInfo.C_PNewStatMount);
    printf("\t%10d PGetTokens\n", cmp->callInfo.C_PGetTokens);
    printf("\t%10d PSetTokens\n", cmp->callInfo.C_PSetTokens);
    printf("\t%10d PUnlog\n", cmp->callInfo.C_PUnlog);
    printf("\t%10d PCheckServers\n", cmp->callInfo.C_PCheckServers);
    printf("\t%10d PCheckAuth\n", cmp->callInfo.C_PCheckAuth);
    printf("\t%10d PCheckVolNames\n", cmp->callInfo.C_PCheckVolNames);
    printf("\t%10d PFindVolume\n", cmp->callInfo.C_PFindVolume);
    printf("\t%10d Prefetch\n", cmp->callInfo.C_Prefetch);
    printf("\t%10d PGetCacheSize\n", cmp->callInfo.C_PGetCacheSize);
    printf("\t%10d PSetCacheSize\n", cmp->callInfo.C_PSetCacheSize);
    printf("\t%10d PSetSysName\n", cmp->callInfo.C_PSetSysName);
    printf("\t%10d PExportAfs\n", cmp->callInfo.C_PExportAfs);
    printf("\t%10d HandleClientContext\n",
	   cmp->callInfo.C_HandleClientContext);
    printf("\t%10d PViceAccess\n", cmp->callInfo.C_PViceAccess);
    printf("\t%10d PRemoveCallBack\n", cmp->callInfo.C_PRemoveCallBack);
    printf("\t%10d PRemoveMount\n", cmp->callInfo.C_PRemoveMount);
    printf("\t%10d PSetVolumeStatus\n", cmp->callInfo.C_PSetVolumeStatus);
    printf("\t%10d PListCells\n", cmp->callInfo.C_PListCells);
    printf("\t%10d PNewCell\n", cmp->callInfo.C_PNewCell);
    printf("\t%10d PGetUserCell\n", cmp->callInfo.C_PGetUserCell);
    printf("\t%10d PGetCellStatus\n", cmp->callInfo.C_PGetCellStatus);
    printf("\t%10d PSetCellStatus\n", cmp->callInfo.C_PSetCellStatus);
    printf("\t%10d PVenusLogging\n", cmp->callInfo.C_PVenusLogging);
    printf("\t%10d PGetAcl\n", cmp->callInfo.C_PGetAcl);
    printf("\t%10d PGetFID\n", cmp->callInfo.C_PGetFID);
    printf("\t%10d PSetAcl\n", cmp->callInfo.C_PSetAcl);
    printf("\t%10d PGetFileCell\n", cmp->callInfo.C_PGetFileCell);
    printf("\t%10d PGetWSCell\n", cmp->callInfo.C_PGetWSCell);
    printf("\t%10d PGetSPrefs\n", cmp->callInfo.C_PGetSPrefs);
    printf("\t%10d PSetSPrefs\n", cmp->callInfo.C_PSetSPrefs);
    printf("\t%10d afs_ResetAccessCache\n",
	   cmp->callInfo.C_afs_ResetAccessCache);
    printf("\t%10d afs_FindUser\n", cmp->callInfo.C_afs_FindUser);
    printf("\t%10d afs_GetUser\n", cmp->callInfo.C_afs_GetUser);
    printf("\t%10d afs_GCUserData\n", cmp->callInfo.C_afs_GCUserData);
    printf("\t%10d afs_PutUser\n", cmp->callInfo.C_afs_PutUser);
    printf("\t%10d afs_SetPrimary\n", cmp->callInfo.C_afs_SetPrimary);
    printf("\t%10d afs_ResetUserConns\n", cmp->callInfo.C_afs_ResetUserConns);
    printf("\t%10d afs_RemoveUserConns\n", cmp->callInfo.C_RemoveUserConns);
    printf("\t%10d afs_ResourceInit\n", cmp->callInfo.C_afs_ResourceInit);
    printf("\t%10d afs_GetCell\n", cmp->callInfo.C_afs_GetCell);
    printf("\t%10d afs_GetCellByIndex\n", cmp->callInfo.C_afs_GetCellByIndex);
    printf("\t%10d afs_GetCellByName\n", cmp->callInfo.C_afs_GetCellByName);
    printf("\t%10d afs_GetRealCellByIndex\n",
	   cmp->callInfo.C_afs_GetRealCellByIndex);
    printf("\t%10d afs_NewCell\n", cmp->callInfo.C_afs_NewCell);
    printf("\t%10d CheckVLDB\n", cmp->callInfo.C_CheckVLDB);
    printf("\t%10d afs_GetVolume\n", cmp->callInfo.C_afs_GetVolume);
    printf("\t%10d afs_PutVolume\n", cmp->callInfo.C_afs_PutVolume);
    printf("\t%10d afs_GetVolumeByName\n",
	   cmp->callInfo.C_afs_GetVolumeByName);
    printf("\t%10d afs_random\n", cmp->callInfo.C_afs_random);
    printf("\t%10d InstallVolumeEntry\n", cmp->callInfo.C_InstallVolumeEntry);
    printf("\t%10d InstallVolumeInfo\n", cmp->callInfo.C_InstallVolumeInfo);
    printf("\t%10d afs_ResetVolumeInfo\n",
	   cmp->callInfo.C_afs_ResetVolumeInfo);
    printf("\t%10d afs_FindServer\n", cmp->callInfo.C_afs_FindServer);
    printf("\t%10d afs_GetServer\n", cmp->callInfo.C_afs_GetServer);
    printf("\t%10d afs_SortServers\n", cmp->callInfo.C_afs_SortServers);
    printf("\t%10d afs_CheckServers\n", cmp->callInfo.C_afs_CheckServers);
    printf("\t%10d ServerDown\n", cmp->callInfo.C_ServerDown);
    printf("\t%10d afs_Conn\n", cmp->callInfo.C_afs_Conn);
    printf("\t%10d afs_PutConn\n", cmp->callInfo.C_afs_PutConn);
    printf("\t%10d afs_ConnByHost\n", cmp->callInfo.C_afs_ConnByHost);
    printf("\t%10d afs_ConnByMHosts\n", cmp->callInfo.C_afs_ConnByMHosts);
    printf("\t%10d afs_Analyze\n", cmp->callInfo.C_afs_Analyze);
    printf("\t%10d afs_CheckLocks\n", cmp->callInfo.C_afs_CheckLocks);
    printf("\t%10d CheckVLServer\n", cmp->callInfo.C_CheckVLServer);
    printf("\t%10d afs_CheckCacheResets\n",
	   cmp->callInfo.C_afs_CheckCacheResets);
    printf("\t%10d afs_CheckVolumeNames\n",
	   cmp->callInfo.C_afs_CheckVolumeNames);
    printf("\t%10d afs_CheckCode\n", cmp->callInfo.C_afs_CheckCode);
    printf("\t%10d afs_CopyError\n", cmp->callInfo.C_afs_CopyError);
    printf("\t%10d afs_FinalizeReq\n", cmp->callInfo.C_afs_FinalizeReq);
    printf("\t%10d afs_GetVolCache\n", cmp->callInfo.C_afs_GetVolCache);
    printf("\t%10d afs_GetVolSlot\n", cmp->callInfo.C_afs_GetVolSlot);
    printf("\t%10d afs_UFSGetVolSlot\n", cmp->callInfo.C_afs_UFSGetVolSlot);
    printf("\t%10d afs_MemGetVolSlot\n", cmp->callInfo.C_afs_MemGetVolSlot);
    printf("\t%10d afs_WriteVolCache\n", cmp->callInfo.C_afs_WriteVolCache);
    printf("\t%10d haveCallbacksfrom\n", cmp->callInfo.C_HaveCallBacksFrom);
    printf("\t%10d afs_getpage\n", cmp->callInfo.C_afs_getpage);
    printf("\t%10d afs_putpage\n", cmp->callInfo.C_afs_putpage);
    printf("\t%10d afs_nfsrdwr\n", cmp->callInfo.C_afs_nfsrdwr);
    printf("\t%10d afs_map\n", cmp->callInfo.C_afs_map);
    printf("\t%10d afs_cmp\n", cmp->callInfo.C_afs_cmp);
    printf("\t%10d afs_PageLeft\n", cmp->callInfo.C_afs_PageLeft);
    printf("\t%10d afs_mount\n", cmp->callInfo.C_afs_mount);
    printf("\t%10d afs_unmount\n", cmp->callInfo.C_afs_unmount);
    printf("\t%10d afs_root\n", cmp->callInfo.C_afs_root);
    printf("\t%10d afs_statfs\n", cmp->callInfo.C_afs_statfs);
    printf("\t%10d afs_sync\n", cmp->callInfo.C_afs_sync);
    printf("\t%10d afs_vget\n", cmp->callInfo.C_afs_vget);
    printf("\t%10d afs_index\n", cmp->callInfo.C_afs_index);
    printf("\t%10d afs_setpag\n", cmp->callInfo.C_afs_setpag);
    printf("\t%10d genpag\n", cmp->callInfo.C_genpag);
    printf("\t%10d getpag\n", cmp->callInfo.C_getpag);
    printf("\t%10d genpag\n", cmp->callInfo.C_genpag);
    printf("\t%10d afs_GetMariner\n", cmp->callInfo.C_afs_GetMariner);
    printf("\t%10d afs_AddMarinerName\n", cmp->callInfo.C_afs_AddMarinerName);
    printf("\t%10d afs_open\n", cmp->callInfo.C_afs_open);
    printf("\t%10d afs_close\n", cmp->callInfo.C_afs_close);
    printf("\t%10d afs_closex\n", cmp->callInfo.C_afs_closex);
    printf("\t%10d afs_write\n", cmp->callInfo.C_afs_write);
    printf("\t%10d afs_UFSwrite\n", cmp->callInfo.C_afs_UFSWrite);
    printf("\t%10d afs_Memwrite\n", cmp->callInfo.C_afs_MemWrite);
    printf("\t%10d afs_rdwr\n", cmp->callInfo.C_afs_rdwr);
    printf("\t%10d afs_read\n", cmp->callInfo.C_afs_read);
    printf("\t%10d afs_UFSread\n", cmp->callInfo.C_afs_UFSRead);
    printf("\t%10d afs_Memread\n", cmp->callInfo.C_afs_MemRead);
    printf("\t%10d afs_CopyOutAttrs\n", cmp->callInfo.C_afs_CopyOutAttrs);
    printf("\t%10d afs_access\n", cmp->callInfo.C_afs_access);
    printf("\t%10d afs_getattr\n", cmp->callInfo.C_afs_getattr);
    printf("\t%10d afs_setattr\n", cmp->callInfo.C_afs_setattr);
    printf("\t%10d afs_VAttrToAS\n", cmp->callInfo.C_afs_VAttrToAS);
    printf("\t%10d EvalMountPoint\n", cmp->callInfo.C_EvalMountPoint);
    printf("\t%10d afs_lookup\n", cmp->callInfo.C_afs_lookup);
    printf("\t%10d afs_create\n", cmp->callInfo.C_afs_create);
    printf("\t%10d afs_LocalHero\n", cmp->callInfo.C_afs_LocalHero);
    printf("\t%10d afs_remove\n", cmp->callInfo.C_afs_remove);
    printf("\t%10d afs_link\n", cmp->callInfo.C_afs_link);
    printf("\t%10d afs_rename\n", cmp->callInfo.C_afs_rename);
    printf("\t%10d afs_InitReq\n", cmp->callInfo.C_afs_InitReq);
    printf("\t%10d afs_mkdir\n", cmp->callInfo.C_afs_mkdir);
    printf("\t%10d afs_rmdir\n", cmp->callInfo.C_afs_rmdir);
    printf("\t%10d afs_readdir\n", cmp->callInfo.C_afs_readdir);
    printf("\t%10d afs_read1dir\n", cmp->callInfo.C_afs_read1dir);
    printf("\t%10d afs_readdir_move\n", cmp->callInfo.C_afs_readdir_move);
    printf("\t%10d afs_readdir_iter\n", cmp->callInfo.C_afs_readdir_iter);
    printf("\t%10d afs_symlink\n", cmp->callInfo.C_afs_symlink);
    printf("\t%10d afs_HandleLink\n", cmp->callInfo.C_afs_HandleLink);
    printf("\t%10d afs_MemHandleLink\n", cmp->callInfo.C_afs_MemHandleLink);
    printf("\t%10d afs_UFSHandleLink\n", cmp->callInfo.C_afs_UFSHandleLink);
    printf("\t%10d HandleFlock\n", cmp->callInfo.C_HandleFlock);
    printf("\t%10d afs_readlink\n", cmp->callInfo.C_afs_readlink);
    printf("\t%10d afs_fsync\n", cmp->callInfo.C_afs_fsync);
    printf("\t%10d afs_inactive\n", cmp->callInfo.C_afs_inactive);
    printf("\t%10d afs_ustrategy\n", cmp->callInfo.C_afs_ustrategy);
    printf("\t%10d afs_strategy\n", cmp->callInfo.C_afs_strategy);
    printf("\t%10d afs_bread\n", cmp->callInfo.C_afs_bread);
    printf("\t%10d afs_brelse\n", cmp->callInfo.C_afs_brelse);
    printf("\t%10d afs_bmap\n", cmp->callInfo.C_afs_bmap);
    printf("\t%10d afs_fid\n", cmp->callInfo.C_afs_fid);
    printf("\t%10d afs_FakeOpen\n", cmp->callInfo.C_afs_FakeOpen);
    printf("\t%10d afs_FakeClose\n", cmp->callInfo.C_afs_FakeClose);
    printf("\t%10d afs_StoreOnLastReference\n",
	   cmp->callInfo.C_afs_StoreOnLastReference);
    printf("\t%10d afs_AccessOK\n", cmp->callInfo.C_afs_AccessOK);
    printf("\t%10d afs_GetAccessBits\n", cmp->callInfo.C_afs_GetAccessBits);
    printf("\t%10d afsio_copy\n", cmp->callInfo.C_afsio_copy);
    printf("\t%10d afsio_trim\n", cmp->callInfo.C_afsio_trim);
    printf("\t%10d afsio_skip\n", cmp->callInfo.C_afsio_skip);
    printf("\t%10d afs_page_read\n", cmp->callInfo.C_afs_page_read);
    printf("\t%10d afs_page_write\n", cmp->callInfo.C_afs_page_write);
    printf("\t%10d afs_page_read\n", cmp->callInfo.C_afs_page_read);
    printf("\t%10d afs_get_groups_from_pag\n",
	   cmp->callInfo.C_afs_get_groups_from_pag);
    printf("\t%10d afs_get_pag_from_groups\n",
	   cmp->callInfo.C_afs_get_pag_from_groups);
    printf("\t%10d AddPag\n", cmp->callInfo.C_AddPag);
    printf("\t%10d PagInCred\n", cmp->callInfo.C_PagInCred);
    printf("\t%10d afs_getgroups\n", cmp->callInfo.C_afs_getgroups);
    printf("\t%10d afs_page_in\n", cmp->callInfo.C_afs_page_in);
    printf("\t%10d afs_page_out\n", cmp->callInfo.C_afs_page_out);
    printf("\t%10d afs_AdvanceFD\n", cmp->callInfo.C_afs_AdvanceFD);
    printf("\t%10d afs_lockf\n", cmp->callInfo.C_afs_lockf);
    printf("\t%10d afs_xsetgroups\n", cmp->callInfo.C_afs_xsetgroups);
    printf("\t%10d afs_nlinks\n", cmp->callInfo.C_afs_nlinks);
    printf("\t%10d afs_lockctl\n", cmp->callInfo.C_afs_lockctl);
    printf("\t%10d afs_xflock\n", cmp->callInfo.C_afs_xflock);
    printf("\t%10d PGetCPrefs\n", cmp->callInfo.C_PGetCPrefs);
    printf("\t%10d PSetCPrefs\n", cmp->callInfo.C_PSetCPrefs);
#ifdef	AFS_HPUX_ENV
    printf("\t%10d afs_pagein\n", cmp->callInfo.C_afs_pagein);
    printf("\t%10d afs_pageout\n", cmp->callInfo.C_afs_pageout);
    printf("\t%10d afs_hp_strategy\n", cmp->callInfo.C_afs_hp_strategy);
#endif
}

#endif
#if 0
#define OffsetOf(s,mem)	((long)(&(((s *)0)->mem)))
#define SizeOf(s,mem)	((long)sizeof(((s *)0)->mem))
#define values(s,mem)	OffsetOf(s,mem), SizeOf(s,mem)

print_struct_vcache_offsets()
{
    printf("struct vcache.v              offset = %ld, size = %ld\n",
	   values(struct vcache, v));
    printf("struct vcache.vlruq          offset = %ld, size = %ld\n",
	   values(struct vcache, vlruq));
    printf("struct vcache.nextfree       offset = %ld, size = %ld\n",
	   values(struct vcache, nextfree));
    printf("struct vcache.hnext          offset = %ld, size = %ld\n",
	   values(struct vcache, hnext));
    printf("struct vcache.fid            offset = %ld, size = %ld\n",
	   values(struct vcache, fid));
    printf("struct vcache.m              offset = %ld, size = %ld\n",
	   values(struct vcache, m));
    printf("struct vcache.lock           offset = %ld, size = %ld\n",
	   values(struct vcache, lock));
    printf("struct vcache.parentVnode    offset = %ld, size = %ld\n",
	   values(struct vcache, parentVnode));
    printf("struct vcache.parentUnique   offset = %ld, size = %ld\n",
	   values(struct vcache, parentUnique));
    printf("struct vcache.mvid           offset = %ld, size = %ld\n",
	   values(struct vcache, mvid));
    printf("struct vcache.linkData       offset = %ld, size = %ld\n",
	   values(struct vcache, linkData));
    printf("struct vcache.flushDV        offset = %ld, size = %ld\n",
	   values(struct vcache, flushDV));
    printf("struct vcache.mapDV          offset = %ld, size = %ld\n",
	   values(struct vcache, mapDV));
    printf("struct vcache.truncPos       offset = %ld, size = %ld\n",
	   values(struct vcache, truncPos));
    printf("struct vcache.callback       offset = %ld, size = %ld\n",
	   values(struct vcache, callback));
    printf("struct vcache.cbExpires      offset = %ld, size = %ld\n",
	   values(struct vcache, cbExpires));
    printf("struct vcache.callsort       offset = %ld, size = %ld\n",
	   values(struct vcache, callsort));
    printf("struct vcache.Access         offset = %ld, size = %ld\n",
	   values(struct vcache, Access));
    printf("struct vcache.anyAccess      offset = %ld, size = %ld\n",
	   values(struct vcache, anyAccess));
    printf("struct vcache.last_looker    offset = %ld, size = %ld\n",
	   values(struct vcache, last_looker));
    printf("struct vcache.activeV        offset = %ld, size = %ld\n",
	   values(struct vcache, activeV));
    printf("struct vcache.slocks         offset = %ld, size = %ld\n",
	   values(struct vcache, slocks));
    printf("struct vcache.opens          offset = %ld, size = %ld\n",
	   values(struct vcache, opens));
    printf("struct vcache.execsOrWriters offset = %ld, size = %ld\n",
	   values(struct vcache, execsOrWriters));
    printf("struct vcache.flockCount     offset = %ld, size = %ld\n",
	   values(struct vcache, flockCount));
    printf("struct vcache.mvstat         offset = %ld, size = %ld\n",
	   values(struct vcache, mvstat));
    printf("struct vcache.states         offset = %ld, size = %ld\n",
	   values(struct vcache, states));
    printf("struct vcache.quick          offset = %ld, size = %ld\n",
	   values(struct vcache, quick));
    printf("struct vcache.symhintstamp   offset = %ld, size = %ld\n",
	   values(struct vcache, symhintstamp));
    printf("struct vcache.h1             offset = %ld, size = %ld\n",
	   values(struct vcache, h1));
    printf("struct vcache.lastr          offset = %ld, size = %ld\n",
	   values(struct vcache, lastr));
    printf("struct vcache.vc_rwlockid    offset = %ld, size = %ld\n",
	   values(struct vcache, vc_rwlockid));
    printf("struct vcache.vc_locktrips   offset = %ld, size = %ld\n",
	   values(struct vcache, vc_locktrips));
    printf("struct vcache.vc_rwlock      offset = %ld, size = %ld\n",
	   values(struct vcache, vc_rwlock));
    printf("struct vcache.mapcnt         offset = %ld, size = %ld\n",
	   values(struct vcache, mapcnt));
    printf("struct vcache.cred           offset = %ld, size = %ld\n",
	   values(struct vcache, cred));
    printf("struct vcache.vc_bhv_desc    offset = %ld, size = %ld\n",
	   values(struct vcache, vc_bhv_desc));
    printf("struct vcache.vc_error       offset = %ld, size = %ld\n",
	   values(struct vcache, vc_error));
    printf("struct vcache.xlatordv       offset = %ld, size = %ld\n",
	   values(struct vcache, xlatordv));
    printf("struct vcache.uncred         offset = %ld, size = %ld\n",
	   values(struct vcache, uncred));
    printf("struct vcache.asynchrony     offset = %ld, size = %ld\n",
	   values(struct vcache, asynchrony));
}

print_struct_vnode_offsets()
{
    printf("struct vnode.v_list           offset = %ld, size = %ld\n",
	   values(struct vnode, v_list));
    printf("struct vnode.v_flag           offset = %ld, size = %ld\n",
	   values(struct vnode, v_flag));
    printf("struct vnode.v_count          offset = %ld, size = %ld\n",
	   values(struct vnode, v_count));
    printf("struct vnode.v_listid         offset = %ld, size = %ld\n",
	   values(struct vnode, v_listid));
    printf("struct vnode.v_intpcount      offset = %ld, size = %ld\n",
	   values(struct vnode, v_intpcount));
    printf("struct vnode.v_type           offset = %ld, size = %ld\n",
	   values(struct vnode, v_type));
    printf("struct vnode.v_rdev           offset = %ld, size = %ld\n",
	   values(struct vnode, v_rdev));
    printf("struct vnode.v_vfsmountedhere offset = %ld, size = %ld\n",
	   values(struct vnode, v_vfsmountedhere));
    printf("struct vnode.v_vfsp           offset = %ld, size = %ld\n",
	   values(struct vnode, v_vfsp));
    printf("struct vnode.v_stream         offset = %ld, size = %ld\n",
	   values(struct vnode, v_stream));
    printf("struct vnode.v_filocks        offset = %ld, size = %ld\n",
	   values(struct vnode, v_filocks));
    printf("struct vnode.v_filocksem      offset = %ld, size = %ld\n",
	   values(struct vnode, v_filocksem));
    printf("struct vnode.v_number         offset = %ld, size = %ld\n",
	   values(struct vnode, v_number));
    printf("struct vnode.v_bh             offset = %ld, size = %ld\n",
	   values(struct vnode, v_bh));
    printf("struct vnode.v_namecap        offset = %ld, size = %ld\n",
	   values(struct vnode, v_namecap));
    printf("struct vnode.v_hashp          offset = %ld, size = %ld\n",
	   values(struct vnode, v_hashp));
    printf("struct vnode.v_hashn          offset = %ld, size = %ld\n",
	   values(struct vnode, v_hashn));
    printf("struct vnode.v_mreg           offset = %ld, size = %ld\n",
	   values(struct vnode, v_mreg));
    printf("struct vnode.v_mregb          offset = %ld, size = %ld\n",
	   values(struct vnode, v_mregb));
    printf("struct vnode.v_pgcnt          offset = %ld, size = %ld\n",
	   values(struct vnode, v_pgcnt));
    printf("struct vnode.v_dpages         offset = %ld, size = %ld\n",
	   values(struct vnode, v_dpages));
    printf("struct vnode.v_dpages_gen     offset = %ld, size = %ld\n",
	   values(struct vnode, v_dpages_gen));
    printf("struct vnode.v_dbuf           offset = %ld, size = %ld\n",
	   values(struct vnode, v_dbuf));
    printf("struct vnode.v_buf            offset = %ld, size = %ld\n",
	   values(struct vnode, v_buf));
    printf("struct vnode.v_bufgen         offset = %ld, size = %ld\n",
	   values(struct vnode, v_bufgen));
    printf("struct vnode.v_traceix        offset = %ld, size = %ld\n",
	   values(struct vnode, v_traceix));
    printf("struct vnode.v_buf_lock       offset = %ld, size = %ld\n",
	   values(struct vnode, v_buf_lock));
    printf("struct vnode.v_pc             offset = %ld, size = %ld\n",
	   values(struct vnode, v_pc));
#ifdef VNODE_TRACING
    printf("struct vnode.v_trace          offset = %ld, size = %ld\n",
	   values(struct vnode, v_trace));
#endif
#ifdef CKPT
    printf("struct vnode.v_ckpt           offset = %ld, size = %ld\n",
	   values(struct vnode, v_ckpt));
#endif
}
#endif
