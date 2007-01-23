/*
 * vi:set cin noet sw=4 tw=70:
 * Copyright 2004, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions of this code borrowed from arla under the following terms:
 * Copyright (c) 2003-2004 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL").
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Code to find the Linux syscall table */

#ifdef OSI_PROBE_STANDALONE
#define OSI_PROBE_DEBUG
#endif
#ifndef OSI_PROBE_STANDALONE
#include <afsconfig.h>
#include "afs/param.h"
#endif
#ifdef AFS_LINUX24_ENV
#include <linux/module.h> /* early to avoid printf->printk mapping */
#ifndef OSI_PROBE_STANDALONE
#include "afs/sysincludes.h"
#include "afsincludes.h"
#endif
#include <linux/version.h>
#ifdef CONFIG_H_EXISTS
#include <linux/config.h>
#endif
#include <linux/linkage.h>
#include <linux/init.h>
#include <linux/unistd.h>
#include <linux/mm.h>
#ifdef AFS_LINUX26_ENV
#include <scsi/scsi.h> /* for scsi_command_size */
#endif

#if defined(AFS_PPC64_LINUX26_ENV)
#include <asm/abs_addr.h>
#endif

#ifdef AFS_AMD64_LINUX20_ENV
#include <asm/ia32_unistd.h>
#endif

/* number of syscalls */
/* NB: on MIPS we care about the 4xxx range */
#ifndef NR_syscalls
#define NR_syscalls 222
#endif

/* lower bound of valid kernel text pointers */
#ifdef AFS_IA64_LINUX20_ENV
#define ktxt_lower_bound (((unsigned long)&kernel_thread )  & 0xfff00000L)
#elif defined(AFS_PPC64_LINUX20_ENV)
#define ktxt_lower_bound (KERNELBASE)
#else
#define ktxt_lower_bound (((unsigned long)&kernel_thread )  & ~0xfffffL)
#endif

/* On SPARC64 and S390X, sys_call_table contains 32-bit entries
 * even though pointers are 64 bit quantities.
 */
#if defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_S390X_LINUX24_ENV)
#define SYSCALLTYPE unsigned int
#define PROBETYPE int
#else
#define SYSCALLTYPE void *
#define PROBETYPE long
#endif

#if defined(AFS_S390X_LINUX20_ENV) && !defined(AFS_S390X_LINUX26_ENV) 
#define _SS(x) ((x) << 1)
#define _SX(x) ((x) &~ 1)
#else
#define _SS(x) (x)
#define _SX(x) (x)
#endif

/* Older Linux doesn't have __user. The sys_read prototype needs it. */
#ifndef __user
#define __user
#endif

/* Allow the user to specify sys_call_table addresses */
static unsigned long sys_call_table_addr[4] = { 0,0,0,0 };
#if defined(module_param_array) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
module_param_array(sys_call_table_addr, long, NULL, 0);
#else
MODULE_PARM(sys_call_table_addr, "1-4l");
#endif
MODULE_PARM_DESC(sys_call_table_addr, "Location of system call tables");

/* If this is set, we are more careful about avoiding duplicate matches */
static int probe_carefully = 1;
#ifdef module_param
module_param(probe_carefully, int, 0);
#else
MODULE_PARM(probe_carefully, "i");
#endif
MODULE_PARM_DESC(probe_carefully, "Probe for system call tables carefully");

static int probe_ignore_syscalls[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
#if defined(module_param_array) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
module_param_array(probe_ignore_syscalls, int, NULL, 0);
#else
MODULE_PARM(probe_ignore_syscalls, "1-8i");
#endif
MODULE_PARM_DESC(probe_ignore_syscalls, "Syscalls to ignore in table checks");

#ifdef OSI_PROBE_DEBUG
/* 
 * Debugging flags:
 * 0x0001 - General debugging
 * 0x0002 - detail - try
 * 0x0004 - detail - try_harder
 * 0x0008 - detail - check_table
 * 0x0010 - detail - check_harder
 * 0x0020 - detail - check_harder/zapped
 * 0x0040 - automatically ignore setgroups and afs_syscall
 */
static int probe_debug = 0x41;
#ifdef module_param
module_param(probe_debug, int, 0);
#else
MODULE_PARM(probe_debug, "i");
#endif
MODULE_PARM_DESC(probe_debug, "Debugging level");

static unsigned long probe_debug_addr[4] = { 0,0,0,0 };
#if defined(module_param_array) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
module_param_array(probe_debug_addr, long, NULL, 0);
#else
MODULE_PARM(probe_debug_addr, "1-4l");
#endif
MODULE_PARM_DESC(probe_debug_addr, "Debug range starting locations");

static unsigned long probe_debug_range = 0;
#ifdef module_param
module_param(probe_debug_range, long, 0);
#else
MODULE_PARM(probe_debug_range, "l");
#endif
MODULE_PARM_DESC(probe_debug_range, "Debug range length");

static unsigned long probe_debug_tag = 0;
#ifdef module_param
module_param(probe_debug_tag, long, 0);
#else
MODULE_PARM(probe_debug_tag, "l");
#endif
MODULE_PARM_DESC(probe_debug_tag, "Debugging output start tag");
#endif


/* Weak references are our friends.  They are supported by the in-kernel
 * linker in Linux 2.6 and by all versions of modutils back to 2.2pre1.
 * A weak reference not satisified by the kernel will have value zero.
 *
 * Unfortunately, weak references to functions don't work right on
 * IA64; specifically, if you actually try to make a call through
 * such a reference, and the symbol doesn't exist in the kernel, then
 * the module relocation code will oops.  A workaround for this is
 * probably possible, but the use of kallsyms_* is of limited value,
 * so I'm not bothing with the effort for now.
 * -- jhutz, 10-Feb-2005
 */
#ifdef OSI_PROBE_KALLSYMS
extern int kallsyms_symbol_to_address(char *name, unsigned long *token,
				      char **mod_name,
				      unsigned long *mod_start,
				      unsigned long *mod_end,
				      char **sec_name,
				      unsigned long *sec_start,
				      unsigned long *sec_end,
				      char **sym_name,
				      unsigned long *sym_start,
				      unsigned long *sym_end
				     ) __attribute__((weak));

extern int kallsyms_address_to_symbol(unsigned long address,
				      char **mod_name,
				      unsigned long *mod_start,
				      unsigned long *mod_end,
				      char **sec_name,
				      unsigned long *sec_start,
				      unsigned long *sec_end,
				      char **sym_name,
				      unsigned long *sym_start,
				      unsigned long *sym_end
				     ) __attribute__((weak));
#endif

extern SYSCALLTYPE sys_call_table[] __attribute__((weak));
extern SYSCALLTYPE ia32_sys_call_table[] __attribute__((weak));
extern SYSCALLTYPE sys_call_table32[] __attribute__((weak));
extern SYSCALLTYPE sys_call_table_emu[] __attribute__((weak));

extern asmlinkage ssize_t sys_read(unsigned int fd, char __user * buf, size_t count) __attribute__((weak));
extern asmlinkage long sys_close(unsigned int) __attribute__((weak));
#if defined(EXPORTED_SYS_CHDIR)
extern asmlinkage long sys_chdir(const char *) __attribute__((weak));
#endif
extern asmlinkage ssize_t sys_write(unsigned int, const char *, size_t) __attribute__((weak));
#ifdef AFS_LINUX26_ENV
extern asmlinkage long sys_wait4(pid_t, int *, int, struct rusage *) __attribute__((weak));
#else
extern asmlinkage long sys_wait4(pid_t, unsigned int *, int, struct rusage *) __attribute__((weak));
#endif
extern asmlinkage long sys_exit (int) __attribute__((weak));
#if defined(EXPORTED_SYS_OPEN)
extern asmlinkage long sys_open (const char *, int, int) __attribute__((weak));
#endif
extern asmlinkage long sys_ioctl(unsigned int, unsigned int, unsigned long) __attribute__((weak));
extern rwlock_t tasklist_lock __attribute__((weak));


/* Structures used to control probing.  We put all the details of which
 * symbols we're interested in, what syscall functions to look for, etc
 * into tables, so we can then have a single copy of the functions that
 * actually do the work.
 */
typedef struct {
    char *name;
    int NR1;
    void *fn1;
    int NR2;
    void *fn2;
    int NR3;
    void *fn3;
} tryctl;

typedef struct {
    char *symbol;                   /* symbol name */
    char *desc;                     /* description for messages */
    int offset;                     /* first syscall number in table */

    void *weak_answer;              /* weak symbol ref */
    void *parm_answer;              /* module parameter answer */
    void *debug_answer;             /* module parameter answer */
    unsigned long given_answer;     /* compiled-in answer, if any */

    tryctl *trylist;                /* array of combinations to try */

    unsigned long try_sect_sym;     /* symbol in section to try scanning */
    unsigned long try_base;         /* default base address for scan */
    unsigned long try_base_mask;    /* base address bits to force to zero */
    unsigned long try_length;       /* default length for scan */

    unsigned long alt_try_sect_sym;     /* symbol in section to try scanning */
    unsigned long alt_try_base;         /* default base address for scan */
    unsigned long alt_try_base_mask;    /* base address bits to force to zero */
    unsigned long alt_try_length;       /* default length for scan */

    int n_zapped_syscalls;          /* number of unimplemented system calls */
    int *zapped_syscalls;           /* list of unimplemented system calls */

    int n_unique_syscalls;          /* number of unique system calls */
    int *unique_syscalls;           /* list of unimplemented system calls */

    int verifyNR;                   /* syscall number to verify match */
    void *verify_fn;                /* syscall pointer to verify match */

    int debug_ignore_NR[4];         /* syscalls to ignore for debugging */
} probectl;



/********** Probing Configuration: sys_call_table **********/

/* syscall pairs/triplets to probe */
/* On PPC64 and SPARC64, we need to omit the ones that might match both tables */
static tryctl main_try[] = {
#if !defined(AFS_PPC64_LINUX20_ENV) && !defined(AFS_SPARC64_LINUX20_ENV)
#if defined(EXPORTED_SYS_CHDIR)
    { "scan: close+chdir+write", __NR_close, &sys_close, __NR_chdir, &sys_chdir, __NR_write, &sys_write },
#endif
#endif
    { "scan: close+wait4",       __NR_close, &sys_close, __NR_wait4, &sys_wait4, -1,         0          },
#if !defined(AFS_PPC64_LINUX20_ENV) && !defined(AFS_SPARC64_LINUX20_ENV)
#if defined(EXPORTED_SYS_CHDIR)
    { "scan: close+chdir",       __NR_close, &sys_close, __NR_chdir, &sys_chdir, -1,         0          },
#endif
#endif
    { "scan: close+ioctl",       __NR_close, &sys_close, __NR_ioctl, &sys_ioctl, -1,         0          },
#if defined(EXPORTED_SYS_OPEN)
    { "scan: exit+open",         __NR_exit,  &sys_exit,  __NR_open,  &sys_open,  -1,         0          },
#endif
    { 0 }
};

/* zapped syscalls for try_harder */
/* this list is based on the table in 'zapped_syscalls' */

static int main_zapped_syscalls[] = {
/* 
 * SPARC-Linux uses syscall number mappings chosen to be compatible
 * with SunOS.  So, it doesn't have any of the traditional calls or
 * the new STREAMS ones.  However, there are a number of syscalls
 * which are SunOS-specific (not implemented on Linux), i386-specific
 * (not implemented on SPARC-Linux), or implemented only on one of
 * sparc32 or sparc64.  Of course, there are no __NR macros for most
 * of these.
 * 
 * Note that the calls we list here are implemented by sys_nis_syscall,
 * not by sys_ni_syscall.  That means we have to exclude all of the
 * other entries, or we might get a sys_ni_syscall into the list and
 * the test would no longer work.
 */
#if defined(AFS_SPARC64_LINUX20_ENV)
    /* mmap2, fstat64, getmsg, putmsg, modify_ldt */
    56, 63, 151, 152, 218,
#elif defined(AFS_SPARC_LINUX20_ENV)
    /* memory_ordering, getmsg, putmsg, unimplemented, modify_ldt */
    52, 151, 152, 164, 218,
#else /* !AFS_SPARC_LINUX20_ENV */

/* 
 * These 7 syscalls are present in the syscall table on most "older"
 * platforms that use the traditional syscall number mappings.  They
 * are not implemented on any platform.
 */
#ifdef __NR_break
    __NR_break,
#endif
#ifdef __NR_stty
    __NR_stty,
#endif
#ifdef __NR_gtty
    __NR_gtty,
#endif
#ifdef __NR_ftime
    __NR_ftime,
#endif
#ifdef __NR_prof
    __NR_prof,
#endif
#ifdef __NR_lock
    __NR_lock,
#endif
#ifdef __NR_mpx
    __NR_mpx,
#endif
/* 
 * On s390 and arm (but not arm26), the seven traditional unimplemented
 * system calls are indeed present and unimplemented.  However, the
 * corresponding __NR macros are not defined, so the tests above fail.
 * Instead, we just have to know the numbers for these.
 */
#if defined(AFS_S390_LINUX20_ENV) || defined(AFS_S390X_LINUX20_ENV)
    /* break, stty, gtty, ftime, prof, lock, mpx */
    17, 31, 32, 35, 44, 53, 56,
#endif

/* 
 * Sadly, some newer platforms like IA64, amd64, and PA-RISC don't have
 * the traditional numbers, so the list above are not helpful.  They
 * do have entries for getpmsg/putpmsg, which are always unimplemented.
 */
#ifdef __NR_getpmsg
    __NR_getpmsg,
#endif
#ifdef __NR_putpmsg
    __NR_putpmsg,
#endif

/* 
 * The module-loading mechanism changed in Linux 2.6, and insmod's
 * loss is our gain: three new unimplemented system calls! 
 */
#if defined(AFS_LINUX26_ENV)
#ifdef __NR_
    __NR_create_module,
#endif
#ifdef __NR_query_module
    __NR_query_module,
#endif
#ifdef __NR_get_kernel_syms
    __NR_get_kernel_syms,
#endif
#endif /* AFS_LINUX26_ENV */

/* 
 * On IA64, the old module-loading calls are indeed present and
 * unimplemented, but the __NR macros are not defined.  Again,
 * we simply have to know their numbers.
 */
#ifdef AFS_IA64_LINUX26_ENV
    /* create_module, query_module, get_kernel_sysms */
    1132, 1136, 1135,
#endif

/* And the same deal for arm (not arm26), if we ever support that. */
#if 0
    /* create_module, query_module, get_kernel_sysms */
    127, 167, 130,
#endif

/*
 * Alpha-Linux uses syscall number mappings chosen to be compatible
 * with OSF/1.  So, it doesn't have any of the traditional calls or
 * the new STREAMS ones, but it does have several OSF/1-specific
 * syscalls which are not implemented on Linux.  These don't exist on
 * any other platform.
 */
#ifdef __NR_osf_syscall
    __NR_osf_syscall,
#endif
#ifdef __NR_osf_profil
    __NR_osf_profil,
#endif
#ifdef __NR_osf_reboot
    __NR_osf_reboot,
#endif
#ifdef __NR_osf_kmodcall
    __NR_osf_kmodcall,
#endif
#ifdef __NR_osf_old_vtrace
    __NR_osf_old_vtrace,
#endif

/*
 * On PPC64, we need a couple more entries to distinguish the two
 * tables, since the system call numbers are the same and the sets of
 * unimplemented calls are very similar.
 * mmap2 and fstat64 are implemented only for 32-bit calls
 */
#ifdef AFS_PPC64_LINUX20_ENV
    /* _mmap2, _fstat64 */
    192, 197,
#endif /* AFS_PPC64_LINUX20_ENV */

/* Similarly for S390X, with lcown16 and fstat64 */
#ifdef AFS_S390X_LINUX20_ENV
    /* lchown16, fstat64 */
    16, 197,
#endif
#endif /* !AFS_SPARC_LINUX20_ENV */
    0
};

/* unique syscalls for try_harder */
static int main_unique_syscalls[] = {
#if defined(AFS_SPARC64_LINUX24_ENV) || defined(AFS_SPARC_LINUX24_ENV)
    /* 
     * On SPARC, we need some additional unique calls to make sure
     * we don't match the SunOS-compatibility table.
     */
    __NR_sgetmask, __NR_ssetmask,
#endif
    __NR_exit, __NR_mount, __NR_read, __NR_write,
    __NR_open, __NR_close, __NR_unlink
};

/* probe control structure */
static probectl main_probe = {
    /* symbol name and description */
    "sys_call_table",
    "system call table",

    /* syscall number of first entry in table */
#ifdef AFS_IA64_LINUX20_ENV
    1024,
#else
    0,
#endif

    sys_call_table,               /* weak symbol ref */
    0, 0,                         /* module parameter answers */
#ifdef AFS_LINUX_sys_call_table
    AFS_LINUX_sys_call_table,     /* compiled-in answer, if any */
#else
    0,
#endif

    main_try,                     /* array of combinations to try */

    /* symbol in section to try scanning */
#if defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_S390_LINUX20_ENV) || defined(AFS_S390X_LINUX20_ENV)
    (unsigned long)&sys_close,
#elif defined(AFS_AMD64_LINUX20_ENV)
    /* On this platform, it's in a different section! */
    (unsigned long)&tasklist_lock,
#else
    (unsigned long)&init_mm,
#endif

    /* default base address for scan */
    /* base address bits to force to zero */
    /* default length for scan */
#if   defined(AFS_SPARC64_LINUX20_ENV)
    (unsigned long)(&sys_close),
    0xfffff,
    0x10000,
#elif   defined(AFS_S390_LINUX20_ENV) || defined(AFS_S390X_LINUX20_ENV)
    /* bleah; this is so suboptimal */
    (unsigned long)(&sys_close),
    0xfffff,
    0x20000,
#elif   defined(AFS_IA64_LINUX20_ENV)
    (unsigned long)(&init_mm),
    0x1fffff,
    0x30000,
#elif defined(AFS_AMD64_LINUX20_ENV)
    (unsigned long)(&tasklist_lock) - 0x30000,
    0,
    0x6000,
#elif defined(AFS_PPC64_LINUX26_ENV)
    (unsigned long)(&do_signal),
    0xfff,
    0x400,
#elif defined(AFS_PPC_LINUX20_ENV) || defined(AFS_PPC_LINUX20_ENV)
    (unsigned long)&init_mm,
    0xffff,
    16384,
#else
    (unsigned long)&init_mm,
    0,
    16384,
#endif

#ifdef AFS_LINUX26_ENV
    (unsigned long)scsi_command_size,
    (unsigned long)scsi_command_size,
    0x3ffff,
    0x30000,
#else
    0, 0, 0, 0,
#endif

    /* number and list of unimplemented system calls */
    ((sizeof(main_zapped_syscalls)/sizeof(main_zapped_syscalls[0])) - 1),
    main_zapped_syscalls,

    /* number and list of unique system calls */
    (sizeof(main_unique_syscalls)/sizeof(main_unique_syscalls[0])),
    main_unique_syscalls,

    /* syscall number and pointer to verify match */
    __NR_close, &sys_close,

    /* syscalls to ignore for debugging */
    {
#if   defined(AFS_ALPHA_LINUX20_ENV)
	338,
#elif defined(AFS_AMD64_LINUX20_ENV)
	183,
#elif defined(AFS_IA64_LINUX20_ENV)
	1141,
#elif defined(AFS_SPARC_LINUX20_ENV) || defined(AFS_SPARC64_LINUX20_ENV)
	227,
#else
	137,
#endif
	__NR_setgroups,
#ifdef __NR_setgroups32
	__NR_setgroups32,
#else
	-1,
#endif
	-1,
    }
};


/********** Probing Configuration: amd64 ia32_sys_call_table **********/
#if defined(AFS_AMD64_LINUX20_ENV)

/* syscall pairs/triplets to probe */
static tryctl ia32_try[] = {
#if defined(EXPORTED_SYS_CHDIR)
    { "scan: close+chdir+write", __NR_ia32_close, &sys_close, __NR_ia32_chdir, &sys_chdir,        __NR_ia32_write, &sys_write },
    { "scan: close+chdir",       __NR_ia32_close, &sys_close, __NR_ia32_chdir, &sys_chdir,        -1,              0          },
#endif
    { 0 }
};

/* zapped syscalls for try_harder */
static int ia32_zapped_syscalls[] = {
    __NR_ia32_break, __NR_ia32_stty, __NR_ia32_gtty, __NR_ia32_ftime,
    __NR_ia32_prof,  __NR_ia32_lock, __NR_ia32_mpx,
    0
};

/* unique syscalls for try_harder */
static int ia32_unique_syscalls[] = {
    __NR_ia32_exit, __NR_ia32_mount, __NR_ia32_read, __NR_ia32_write,
    __NR_ia32_open, __NR_ia32_close, __NR_ia32_unlink
};

/* probe control structure */
static probectl ia32_probe = {
    /* symbol name and description */
    "ia32_sys_call_table",
    "32-bit system call table",

    /* syscall number of first entry in table */
    0,

    ia32_sys_call_table,          /* weak symbol ref */
    0, 0,                         /* module parameter answers */
#ifdef AFS_LINUX_ia32_sys_call_table
    AFS_LINUX_ia32_sys_call_table,/* compiled-in answer, if any */
#else
    0,
#endif

    ia32_try,                     /* array of combinations to try */

    /* symbol in section to try scanning */
    (unsigned long)&init_mm,

    /* default base address for scan */
    /* base address bits to force to zero */
    /* default length for scan */
    (unsigned long)&init_mm,
    0,
    (0x180000 / sizeof(unsigned long *)),

#ifdef AFS_LINUX26_ENV
    (unsigned long)scsi_command_size,
    (unsigned long)scsi_command_size,
    0x3ffff,
    0x30000,
#else
    0, 0, 0, 0,
#endif


    /* number and list of unimplemented system calls */
    ((sizeof(ia32_zapped_syscalls)/sizeof(ia32_zapped_syscalls[0])) - 1),
    ia32_zapped_syscalls,

    /* number and list of unique system calls */
    (sizeof(ia32_unique_syscalls)/sizeof(ia32_unique_syscalls[0])),
    ia32_unique_syscalls,

    /* syscall number and pointer to verify match */
    __NR_ia32_close, &sys_close,

    /* syscalls to ignore for debugging */
    {
	137,
	__NR_ia32_setgroups,
	__NR_ia32_setgroups32,
	-1,
    }
};

static probectl *probe_list[] = {
    &main_probe, &ia32_probe
};


/********** Probing Configuration: IA64 **********/
#elif defined(AFS_IA64_LINUX20_ENV)
struct fptr {
    void *ip;
    unsigned long gp;
};

/* no 32-bit support on IA64 for now */
static probectl *probe_list[] = {
    &main_probe
};


/********** Probing Configuration: ppc64, sparc64 sys_call_table32 **********/
#elif defined(AFS_PPC64_LINUX20_ENV) || defined(AFS_SPARC64_LINUX20_ENV)
struct fptr {
    void *ip;
    unsigned long gp;
};

/* 
 * syscall pairs/triplets to probe
 * This has to be empty, because anything that would work will
 * also match the main table, and that's no good.
 */
static tryctl sct32_try[] = {
    { 0 }
};

/* zapped syscalls for try_harder */
static int sct32_zapped_syscalls[] = {
#ifdef AFS_PPC64_LINUX20_ENV
    /* These should be sufficient */
    __NR_break, __NR_stty, __NR_gtty, __NR_ftime,
    __NR_prof, __NR_lock, __NR_mpx,
#endif
#ifdef AFS_SPARC64_LINUX20_ENV
    /* memory_ordering, getmsg, putmsg, unimplemented, modify_ldt */
    52, 151, 152, 164, 218,
#endif
    0
};

/* unique syscalls for try_harder */
/* mmap2 and fstat64 are implemented only for 32-bit calls */
static int sct32_unique_syscalls[] = {
#ifdef AFS_PPC64_LINUX20_ENV
    /* _mmap2, _fstat64 */
    192, 197,
#endif
#ifdef AFS_SPARC64_LINUX24_ENV
    /* 
     * On SPARC, we need some additional unique calls to make sure
     * we don't match the SunOS-compatibility table.
     */
    __NR_sgetmask, __NR_ssetmask,
#endif
    __NR_exit, __NR_mount, __NR_read, __NR_write,
    __NR_open, __NR_close, __NR_unlink
};

/* probe control structure */
static probectl sct32_probe = {
    /* symbol name and description */
    "sys_call_table32",
    "32-bit system call table",

    /* syscall number of first entry in table */
    0,

    sys_call_table32,             /* weak symbol ref */
    0, 0,                         /* module parameter answers */
#ifdef AFS_LINUX_sys_call_table32
    AFS_LINUX_sys_call_table32,   /* compiled-in answer, if any */
#else
    0,
#endif

    sct32_try,                   /* array of combinations to try */

    /* symbol in section to try scanning */
#if defined(AFS_SPARC64_LINUX20_ENV)
    (unsigned long)&sys_close,
#else
    (unsigned long)&init_mm,
#endif

    /* default base address for scan */
    /* base address bits to force to zero */
    /* default length for scan */
#if   defined(AFS_SPARC64_LINUX20_ENV)
    (unsigned long)(&sys_close),
    0xfffff,
    0x10000,
#elif defined(AFS_PPC64_LINUX26_ENV)
    (unsigned long)(&do_signal),
    0xfff,
    0x400,
#else
    (unsigned long)&init_mm,
    0,
    16384,
#endif

#ifdef AFS_LINUX26_ENV
    (unsigned long)scsi_command_size,
    (unsigned long)scsi_command_size,
    0x3ffff,
    0x30000,
#else
    0, 0, 0, 0,
#endif

    /* number and list of unimplemented system calls */
    ((sizeof(sct32_zapped_syscalls)/sizeof(sct32_zapped_syscalls[0])) - 1),
    sct32_zapped_syscalls,

    /* number and list of unique system calls */
    (sizeof(sct32_unique_syscalls)/sizeof(sct32_unique_syscalls[0])),
    sct32_unique_syscalls,

    /* syscall number and pointer to verify match */
    __NR_close, &sys_close,

    /* syscalls to ignore for debugging */
    {
#if defined(AFS_SPARC64_LINUX20_ENV)
	227,
#else
	137,
#endif
	__NR_setgroups,
	-1,
	-1,
    }
};

static probectl *probe_list[] = {
    &main_probe, &sct32_probe
};


/********** Probing Configuration: s390x sys_call_table_emu **********/
/* We only actually need to do this on s390x_linux26 and later.
 * On earlier versions, the two tables were interleaved and so
 * have related base addresses.
 */
#elif defined(AFS_S390X_LINUX26_ENV)

/* syscall pairs/triplets to probe */
/* nothing worthwhile is exported, so this is empty */
static tryctl emu_try[] = {
    { 0 }
};

/* zapped syscalls for try_harder */
static int emu_zapped_syscalls[] = {
    /* break, stty, gtty, ftime, prof, lock, mpx */
    17, 31, 32, 35, 44, 53, 56,
    0
};

/* unique syscalls for try_harder */
static int emu_unique_syscalls[] = {
    /* lchown16, fstat64 */
    16, 197,
    __NR_exit, __NR_mount, __NR_read, __NR_write,
    __NR_open, __NR_close, __NR_unlink
};

/* probe control structure */
static probectl emu_probe = {
    /* symbol name and description */
    "sys_call_table_emu",
    "32-bit system call table",

    /* syscall number of first entry in table */
    0,

    sys_call_table_emu,           /* weak symbol ref */
    0, 0,                         /* module parameter answers */
#ifdef AFS_LINUX_sys_call_table_emu
    AFS_LINUX_sys_call_table_emu, /* compiled-in answer, if any */
#else
    0,
#endif

    emu_try,                      /* array of combinations to try */

    /* symbol in section to try scanning */
    (unsigned long)&sys_close,

    /* default base address for scan */
    /* base address bits to force to zero */
    /* default length for scan */
    (unsigned long)&sys_close,
    0xfffff,
    0x20000,

#ifdef AFS_LINUX26_ENV
    (unsigned long)scsi_command_size,
    (unsigned long)scsi_command_size,
    0x3ffff,
    0x30000,
#else
    0, 0, 0, 0,
#endif

    /* number and list of unimplemented system calls */
    ((sizeof(emu_zapped_syscalls)/sizeof(emu_zapped_syscalls[0])) - 1),
    emu_zapped_syscalls,

    /* number and list of unique system calls */
    (sizeof(emu_unique_syscalls)/sizeof(emu_unique_syscalls[0])),
    emu_unique_syscalls,

    /* syscall number and pointer to verify match */
    __NR_close, &sys_close,

    /* syscalls to ignore for debugging */
    {
	137,
	__NR_setgroups,
	-1,
	-1,
    }
};

static probectl *probe_list[] = {
    &main_probe, &emu_probe
};


/********** End of Probing Configuration **********/

#else /* no per-platform probe control, so use the default list */
static probectl *probe_list[] = {
    &main_probe
};
#endif

#define N_PROBE_LIST (sizeof(probe_list) / sizeof(*probe_list))
#define DEBUG_IN_RANGE(P,x) (!probe_debug_range ||                             \
  (P->debug_answer &&                                                          \
   (unsigned long)(x) >= (unsigned long)P->debug_answer &&                     \
   (unsigned long)(x) <  (unsigned long)P->debug_answer + probe_debug_range))



static int check_table(probectl *P, PROBETYPE *ptr)
{
    PROBETYPE *x;
    int i, j;

    for (x = ptr, i = 0; i < _SS(NR_syscalls); i++, x++) {
#ifdef OSI_PROBE_DEBUG
	if (probe_debug & 0x0040) {
	    for (j = 0; j < 4; j++) {
		if (_SS(P->debug_ignore_NR[j]) == _SX(i + P->offset)) break;
	    }
	    if (j < 4) continue;
	}
#endif
	for (j = 0; j < 8; j++) {
	    if (_SS(probe_ignore_syscalls[j]) == _SX(i) + P->offset) break;
	}
	if (j < 8) continue;
	if (*x <= ktxt_lower_bound) {
#ifdef OSI_PROBE_DEBUG
	    if ((probe_debug & 0x0008) && DEBUG_IN_RANGE(P,ptr))
		printk("<7>check 0x%lx -> %d [0x%lx]\n",
		       (unsigned long)ptr, i, (unsigned long)*x);
#endif
	    return i;
	}
    }
#ifdef OSI_PROBE_DEBUG
    if ((probe_debug & 0x0008) && DEBUG_IN_RANGE(P,ptr))
	printk("<7>check 0x%lx -> ok\n", (unsigned long)ptr);
#endif
    return -1;
}

static void *try(probectl *P, tryctl *T, PROBETYPE *aptr,
		 unsigned long datalen)
{
#ifdef OSI_PROBE_KALLSYMS
    char *mod_name, *sec_name, *sym_name;
    unsigned long mod_start, mod_end;
    unsigned long sec_start, sec_end;
    unsigned long sym_start, sym_end;
#endif
    unsigned long offset, ip1, ip2, ip3;
    int ret;
    PROBETYPE *ptr;

#if defined(AFS_IA64_LINUX20_ENV) || defined(AFS_PPC64_LINUX20_ENV)
    ip1 = T->fn1 ? (unsigned long)((struct fptr *)T->fn1)->ip : 0;
    ip2 = T->fn2 ? (unsigned long)((struct fptr *)T->fn2)->ip : 0;
    ip3 = T->fn3 ? (unsigned long)((struct fptr *)T->fn3)->ip : 0;
#else
    ip1 = (unsigned long)T->fn1;
    ip2 = (unsigned long)T->fn2;
    ip3 = (unsigned long)T->fn3;
#endif

#ifdef OSI_PROBE_DEBUG
    if (probe_debug & 0x0001)
	printk("<7>osi_probe: %s                      %s (%d->0x%lx, %d->0x%lx, %d->0x%lx)\n",
	       P->symbol, T->name, T->NR1, ip1, T->NR2, ip2, T->NR3, ip3);
#endif

    if (!ip1 || !ip2 || (T->NR3 >= 0 && !ip3))
	return 0;

    for (offset = 0; offset < datalen; offset++, aptr++) {
#if defined(AFS_PPC64_LINUX20_ENV)
	ptr = (PROBETYPE*)(*aptr);
	if ((unsigned long)ptr <= KERNELBASE) {
		continue;
	}
#else
	ptr = aptr;
#endif
	if ((unsigned long)ptr < init_mm.start_code ||
#if defined(AFS_AMD64_LINUX20_ENV)
		(unsigned long)ptr > init_mm.brk)
#else
		(unsigned long)ptr > init_mm.end_data)
#endif
	{
/*	     printk("address 0x%lx (from 0x%lx %d) is out of range in check_table. wtf?\n", (unsigned long)x, (unsigned long)ptr, i);*/
	     continue;
	}

	ret = check_table(P, ptr);
	if (ret >= 0) {
	    /* return value is number of entries to skip */
	    aptr    += ret;
	    offset += ret;
	    continue;
	}

#ifdef OSI_PROBE_DEBUG
	if ((probe_debug & 0x0002) && DEBUG_IN_RANGE(P,ptr))
	    printk("<7>try 0x%lx\n", (unsigned long)ptr);
#endif
	if (ptr[_SS(T->NR1 - P->offset)] != ip1)        continue;
	if (ptr[_SS(T->NR2 - P->offset)] != ip2)        continue;
	if (ip3 && ptr[_SS(T->NR3 - P->offset)] != ip3) continue;

#ifdef OSI_PROBE_DEBUG
	if (probe_debug & 0x0002)
	    printk("<7>try found 0x%lx\n", (unsigned long)ptr);
#endif
#ifdef OSI_PROBE_KALLSYMS
	if (kallsyms_address_to_symbol) {
	    ret = kallsyms_address_to_symbol((unsigned long)ptr,
					     &mod_name, &mod_start, &mod_end,
					     &sec_name, &sec_start, &sec_end,
					     &sym_name, &sym_start, &sym_end);
	    if (!ret || strcmp(sym_name, P->symbol)) continue;
	}
#endif
	/* XXX should we make sure there is only one match? */
	return (void *)ptr;
    }
    return 0;
}


static int check_harder(probectl *P, PROBETYPE *p)
{
    unsigned long ip1;
    int i, s;

    /* Check zapped syscalls */
    for (i = 1; i < P->n_zapped_syscalls; i++) {
	if (p[_SS(P->zapped_syscalls[i])] != p[_SS(P->zapped_syscalls[0])]) {
#ifdef OSI_PROBE_DEBUG
	    if ((probe_debug & 0x0020) && DEBUG_IN_RANGE(P,p))
		printk("<7>check_harder 0x%lx zapped failed i=%d\n", (unsigned long)p, i);
#endif
	    return 0;
	}
    }

    /* Check unique syscalls */
    for (i = 0; i < P->n_unique_syscalls; i++) {
	for (s = 0; s < NR_syscalls; s++) {
	    if (p[_SS(s)] == p[_SS(P->unique_syscalls[i])]
		&& s != P->unique_syscalls[i]) {
#ifdef OSI_PROBE_DEBUG
		if ((probe_debug & 0x0010) && DEBUG_IN_RANGE(P,p))
		    printk("<7>check_harder 0x%lx unique failed i=%d s=%d\n", (unsigned long)p, i, s);
#endif
		return 0;
	    }
	}
    }

#if defined(AFS_IA64_LINUX20_ENV) || defined(AFS_PPC64_LINUX20_ENV)
    ip1 = P->verify_fn ? (unsigned long)((struct fptr *)(P->verify_fn))->ip : 0;
#else
    ip1 = (unsigned long)(P->verify_fn);
#endif

    if (ip1 && p[_SS(P->verifyNR - P->offset)] != ip1) {
#ifdef OSI_PROBE_DEBUG
	if ((probe_debug & 0x0010) && DEBUG_IN_RANGE(P,p))
	    printk("<7>check_harder 0x%lx verify failed\n", (unsigned long)p);
#endif
	return 0;
    }

#ifdef OSI_PROBE_DEBUG
    if ((probe_debug & 0x0010) && DEBUG_IN_RANGE(P,p))
	printk("<7>check_harder 0x%lx success!\n", (unsigned long)p);
#endif
    return 1;
}

static void *try_harder(probectl *P, PROBETYPE *ptr, unsigned long datalen)
{
#ifdef OSI_PROBE_KALLSYMS
    char *mod_name, *sec_name, *sym_name;
    unsigned long mod_start, mod_end;
    unsigned long sec_start, sec_end;
    unsigned long sym_start, sym_end;
#endif
    unsigned long offset;
    void *match = 0;
    int ret;

#ifdef OSI_PROBE_DEBUG
    if (probe_debug & 0x0001)
	printk("<7>osi_probe: %s                      try_harder\n", P->symbol);
#endif
    for (offset = 0; offset < datalen; offset++, ptr++) {
	 if ((unsigned long)ptr < init_mm.start_code ||
#if defined(AFS_AMD64_LINUX20_ENV)
		(unsigned long)ptr > init_mm.brk)
#else
		(unsigned long)ptr > init_mm.end_data)
#endif
	{
/*	     printk("address 0x%lx (from 0x%lx %d) is out of range in check_table. wtf?\n", (unsigned long)x, (unsigned long)ptr, i);*/
	     continue;
	}
	ret = check_table(P, ptr);
        if (ret >= 0) {
            /* return value is number of entries to skip */
	    ptr    += ret;
	    offset += ret;
	    continue;
	}

#ifdef OSI_PROBE_DEBUG
	if ((probe_debug & 0x0004) && DEBUG_IN_RANGE(P,ptr))
	    printk("<7>try_harder 0x%lx\n", (unsigned long)ptr);
#endif
	if (!check_harder(P, ptr))
	    continue;

#ifdef OSI_PROBE_DEBUG
	if (probe_debug & 0x0004)
	    printk("<7>try_harder found 0x%lx\n", (unsigned long)ptr);
#endif

#ifdef OSI_PROBE_KALLSYMS
	if (kallsyms_address_to_symbol) {
	    ret = kallsyms_address_to_symbol((unsigned long)ptr,
					     &mod_name, &mod_start, &mod_end,
					     &sec_name, &sec_start, &sec_end,
					     &sym_name, &sym_start, &sym_end);
	    if (!ret || strcmp(sym_name, P->symbol)) continue;
	}
#endif

	if (match) {
#ifdef OSI_PROBE_DEBUG
	    if (probe_debug & 0x0005)
		printk("<7>%s: try_harder found multiple matches!\n", P->symbol);
#endif
	    return 0;
	}

	match = (void *)ptr;
	if (!probe_carefully)
	    break;
    }
    return match;
}


#ifdef OSI_PROBE_DEBUG
#define check_result(x,m) do {                                                             \
    if (probe_debug & 0x0001) {                                                              \
	printk("<7>osi_probe: %s = 0x%016lx %s\n", P->symbol, (unsigned long)(x), (m)); \
    }                                                                                      \
    if ((x)) {                                                                             \
	*method = (m);                                                                     \
        final_answer = (void *)(x);                                                        \
    }                                                                                      \
} while (0)
#else
#define check_result(x,m) do {  \
    if ((x)) {                  \
        *method = (m);          \
        return (void *)(x);     \
    }                           \
} while (0)
#endif
static void *scan_for_syscall_table(probectl *P, PROBETYPE *B, unsigned long L)
{
    tryctl *T;
    void *answer;
#if defined(AFS_S390_LINUX20_ENV) || defined(AFS_S390X_LINUX20_ENV)
    void *answer2;
#endif
#ifdef OSI_PROBE_DEBUG
    void *final_answer = 0;
#endif
#ifdef OSI_PROBE_DEBUG
    if (probe_debug & 0x0007)
	printk("<7>osi_probe: %s                      base=0x%lx, len=0x%lx\n",
	       P->symbol, (unsigned long)B, L);
    if (probe_debug & 0x0009) {
	printk("<7>osi_probe: %s                      ktxt_lower_bound=0x%lx\n",
	       P->symbol, ktxt_lower_bound);
	printk("<7>osi_probe: %s                      NR_syscalls=%d\n",
	       P->symbol, NR_syscalls);
    }
#endif

    for (T = P->trylist; T->name; T++) {
	answer = try(P, T, B, L);
#if defined(AFS_S390_LINUX20_ENV) || defined(AFS_S390X_LINUX20_ENV)
	answer2 = try(P, T, (PROBETYPE *)(2 + (void *)B), L);
#ifdef OSI_PROBE_DEBUG
	if (probe_debug & 0x0003) {
	    printk("<7>osi_probe: %s = 0x%016lx %s (even)\n",
		   P->symbol, (unsigned long)(answer), T->name);
	    printk("<7>osi_probe: %s = 0x%016lx %s (odd)\n",
		   P->symbol, (unsigned long)(answer2), T->name);
	}
#endif
	if (answer && answer2) answer = 0;
	else if (answer2) answer = answer2;
#endif
	if (answer)
	    return answer;
    }

    /* XXX more checks here */

    answer = try_harder(P, B, L);
#if defined(AFS_S390_LINUX20_ENV) || defined(AFS_S390X_LINUX20_ENV)
    answer2 = try_harder(P, (PROBETYPE *)(2 + (void *)B), L);
#ifdef OSI_PROBE_DEBUG
    if (probe_debug & 0x0005) {
	printk("<7>osi_probe: %s = 0x%016lx pattern scan (even)\n",
	       P->symbol, (unsigned long)(answer));
	printk("<7>osi_probe: %s = 0x%016lx pattern scan (odd)\n",
	       P->symbol, (unsigned long)(answer2));
    }
#endif
    if (answer && answer2) answer = 0;
    else if (answer2) answer = answer2;
#endif
    return answer;
}

static void *do_find_syscall_table(probectl *P, char **method)
{
#ifdef OSI_PROBE_KALLSYMS
    char *mod_name, *sec_name, *sym_name;
    unsigned long mod_start, mod_end;
    unsigned long sec_start, sec_end;
    unsigned long sym_start, sym_end;
    unsigned long token;
    int ret;
#endif
    PROBETYPE *B;
    unsigned long L;
    void *answer;
#ifdef OSI_PROBE_DEBUG
    void *final_answer = 0;
#endif

    *method = "not found";

    /* if it's exported, there's nothing to do */
    check_result(P->weak_answer, "exported");

    /* ask the kernel to do the name lookup, if it's willing */
#ifdef OSI_PROBE_KALLSYMS
    if (kallsyms_symbol_to_address) {
	token = 0;
        sym_start = 0;
	do {
	    ret = kallsyms_symbol_to_address(P->symbol, &token,
					     &mod_name, &mod_start, &mod_end,
					     &sec_name, &sec_start, &sec_end,
					     &sym_name, &sym_start, &sym_end);
	    if (ret && !strcmp(mod_name, "kernel") && sym_start)
		break;
	    sym_start = 0;
	} while (ret);
        check_result(sym_start, "kallsyms_symbol_to_address");
    }
#endif

    /* Maybe a little birdie told us */
    check_result(P->parm_answer,  "module parameter");
    check_result(P->given_answer, "compiled-in");

    /* OK, so we have to scan. */
    B = (PROBETYPE *)((P->try_base) & ~(P->try_base_mask));
    L = P->try_length;
    /* Now, see if the kernel will tell us something better than the default */
#ifdef OSI_PROBE_KALLSYMS
    if (kallsyms_address_to_symbol) {
	ret = kallsyms_address_to_symbol(P->try_sect_sym,
					 &mod_name, &mod_start, &mod_end,
					 &sec_name, &sec_start, &sec_end,
					 &sym_name, &sym_start, &sym_end);
	if (ret) {
	    B = (PROBETYPE *)sec_start;
	    L = (sec_end - sec_start) / sizeof(unsigned long);
	}
    }
#endif
   
    answer = scan_for_syscall_table(P, B, L);
    check_result(answer, "pattern scan");
    B = (PROBETYPE *)((P->alt_try_base) & ~(P->alt_try_base_mask));
    L = P->alt_try_length;
    /* Now, see if the kernel will tell us something better than the default */
#ifdef OSI_PROBE_KALLSYMS
    if (kallsyms_address_to_symbol && P->alt_try_sect_sym) {
	ret = kallsyms_address_to_symbol(P->alt_try_sect_sym,
					 &mod_name, &mod_start, &mod_end,
					 &sec_name, &sec_start, &sec_end,
					 &sym_name, &sym_start, &sym_end);
	if (ret) {
	    B = (PROBETYPE *)sec_start;
	    L = (sec_end - sec_start) / sizeof(unsigned long);
	}
    }
#endif
    if (B && L) {
	answer = scan_for_syscall_table(P, B, L);
	check_result(answer, "pattern scan");
    }
#ifdef OSI_PROBE_DEBUG
    return final_answer;
#else
    return 0;
#endif
}

#if defined(AFS_I386_LINUX26_ENV) || defined(AFS_AMD64_LINUX26_ENV)
static int check_writable(unsigned long address) 
{ 
    pgd_t *pgd = pgd_offset_k(address);
#ifdef PUD_SIZE
    pud_t *pud;
#endif
    pmd_t *pmd;
    pte_t *pte;

    if (pgd_none(*pgd))
	return 0;
#ifdef PUD_SIZE
    pud = pud_offset(pgd, address);
    if (pud_none(*pud))
	return 0;
    pmd = pmd_offset(pud, address);
#else
    pmd = pmd_offset(pgd, address);
#endif
    if (pmd_none(*pmd))
	return 0;
#ifndef CONFIG_UML
    if (pmd_large(*pmd))
	pte = (pte_t *)pmd;
    else
#endif
	pte = pte_offset_kernel(pmd, address);
    if (pte_none(*pte) || !pte_present(*pte) || !pte_write(*pte))
	return 0;
    return 1;
}
#endif

void *osi_find_syscall_table(int which)
{
    probectl *P;
    void *answer;
    char *method;

    if (which < 0 || which >= N_PROBE_LIST) {
	printk("error - afs_find_syscall_table called with invalid index!\n");
	return 0;
    }
    P = probe_list[which];
    if (which < 4) {
	P->parm_answer = (void *)sys_call_table_addr[which];
#ifdef OSI_PROBE_DEBUG
	P->debug_answer = (void *)probe_debug_addr[which];
#endif
    }
    answer = do_find_syscall_table(P, &method);
    if (!answer) {
	printk("Warning: failed to find address of %s\n", P->desc);
	printk("System call hooks will not be installed; proceeding anyway\n");
	return 0;
    }
    printk("Found %s at 0x%lx (%s)\n", P->desc, (unsigned long)answer, method);
#if defined(AFS_I386_LINUX26_ENV) || defined(AFS_AMD64_LINUX26_ENV)
    if (!check_writable((unsigned long)answer)) {
	printk("Address 0x%lx is not writable.\n", (unsigned long)answer);
	printk("System call hooks will not be installed; proceeding anyway\n");
	return 0;
    }
#endif
    return answer;
}


#ifdef OSI_PROBE_STANDALONE
int __init osi_probe_init(void)
{
    int i;

    if (!probe_debug_tag) probe_debug_tag = jiffies;
    printk("*** osi_probe %ld debug = 0x%04x ***\n",
	   probe_debug_tag, probe_debug);
    for (i = 0; i < N_PROBE_LIST; i++)
	(void)osi_find_syscall_table(i);
    return 0;
}

void osi_probe_exit(void) { }

module_init(osi_probe_init);
module_exit(osi_probe_exit);
#endif
#endif
