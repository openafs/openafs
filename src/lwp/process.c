/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* process.c - manage lwp context switches be means of setjmp/longjmp. */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <assert.h>
#include "lwp.h"
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if defined(AFS_OSF_ENV) || defined(AFS_S390_LINUX20_ENV)
extern int PRE_Block;		/* used in lwp.c and process.s */
#else
extern char PRE_Block;		/* used in lwp.c and process.s */
#endif

#if defined(USE_UCONTEXT) && defined(HAVE_UCONTEXT_H)

afs_int32
savecontext(char (*ep) (), struct lwp_context *savearea, char *newsp)
{
#if defined(AFS_LINUX20_ENV)
    /* getcontext does not export stack info */
    int stackvar;
#endif

    PRE_Block = 1;

    savearea->state = 0;
    getcontext(&savearea->ucontext);
#if defined(AFS_LINUX20_ENV)
    savearea->topstack = &stackvar;
#else
    savearea->topstack = savearea->ucontext.uc_stack.ss_sp;
#endif
    switch (savearea->state) {
    case 0:
	if (newsp) {
	    ucontext_t thread;

	    getcontext(&thread);
	    thread.uc_stack.ss_sp =
		newsp - AFS_LWP_MINSTACKSIZE + sizeof(void *) +
		sizeof(void *);
	    thread.uc_stack.ss_size = AFS_LWP_MINSTACKSIZE - sizeof(void *);
	    makecontext(&thread, ep, 0);
	    setcontext(&thread);
	} else
	    (*ep) ();
	break;
    case 2:
	break;
    }
    return 0;
}

void
returnto(savearea)
     struct lwp_context *savearea;
{
    PRE_Block = 0;

    savearea->state = 2;
    setcontext(&savearea->ucontext);
}

#else

/*
 * Magic stack pointer
 */
#if	defined(AFS_SGI64_ENV)
# ifdef _BSD_COMPAT
#  define LWP_SP 34
# else
#  define LWP_SP JB_SP
# endif
#elif	defined(AFS_HPUX_ENV) || defined(AFS_PARISC_LINUX24_ENV)
#define	LWP_SP	1
#elif	defined(AFS_LINUX20_ENV)
#if defined(AFS_PPC_LINUX20_ENV) || defined(AFS_PPC64_LINUX20_ENV)
#define LWP_SP 0
#elif   defined(AFS_I386_LINUX20_ENV)
#define LWP_SP 4
#elif   defined(AFS_S390_LINUX20_ENV)
#define LWP_SP 9
#define LWP_FP 5
#elif   defined(AFS_SPARC_LINUX20_ENV)
#define LWP_SP 0
#define LWP_FP 1
#elif   defined(AFS_SPARC64_LINUX20_ENV) && defined(AFS_32BIT_USR_ENV)
#define LWP_SP 0
#define LWP_FP 1
#elif defined(AFS_ALPHA_LINUX20_ENV)
#define LWP_SP 8
#define LWP_FP 7
#elif defined(AFS_PARISC_LINUX24_ENV)
#define LWP_SP 19
#else
#error Unsupported linux LWP system type.
#endif
#elif   defined(AFS_X86_FBSD_ENV)
#define LWP_SP 4
#elif   defined(AFS_DARWIN_ENV)
#define LWP_SP 16
#else
Need offset to SP in jmp_buf for this platform.
#endif
/**
  * On SGIs the type of the elements of the array passed to setjmp
  * differs based on the ISA chosen. It is int for mips1 and mips2 and
  * __uint64_t for mips3 and mips4
  */
#ifdef AFS_SGI64_ENV
#if (_MIPS_ISA == _MIPS_ISA_MIPS3 || _MIPS_ISA == _MIPS_ISA_MIPS4)
typedef __uint64_t jmp_buf_type;
#endif
#else
#if defined(AFS_ALPHA_LINUX20_ENV) || defined(AFS_PPC64_LINUX20_ENV) 
typedef long jmp_buf_type;
#else
typedef int jmp_buf_type;
#endif /*AFS_ALPHA_LINUX20_ENV */
#endif /*SGI*/

    static jmp_buf jmp_tmp;
static char (*EP) ();
static int rc;
static jmp_buf_type *jmpBuffer;

afs_int32
savecontext(ep, savearea, sp)
     char (*ep) ();
     struct lwp_context *savearea;
     char *sp;
{
    int code;

    PRE_Block = 1;
    EP = ep;

    code = setjmp(savearea->setjmp_buffer);
    jmpBuffer = (jmp_buf_type *) savearea->setjmp_buffer;
    savearea->topstack = (char *)jmpBuffer[LWP_SP];

#if	defined(DEBUG)
    {
	int i, *ptr = (int *)savearea->setjmp_buffer;
	printf("savecontext\n");
	for (i = 0; i < 5; i++)
	    printf("(%d) 0x%x   ", i, ptr[i]);
	printf("\n");
	for (i = 5; i < 10; i++)
	    printf("(%d) 0x%x   ", i, ptr[i]);
	printf("\n");
    }
#endif
    switch (code) {
    case 0:
	if (!sp)
	    (*EP) ();
	else {
	    rc = setjmp(jmp_tmp);
	    switch (rc) {
	    case 0:
		jmpBuffer = (jmp_buf_type *) jmp_tmp;
		jmpBuffer[LWP_SP] = (jmp_buf_type) sp;
#if defined(AFS_S390_LINUX20_ENV) || defined(AFS_SPARC_LINUX20_ENV) || (defined(AFS_SPARC64_LINUX20_ENV) && defined(AFS_32BIT_USR_ENV))
		jmpBuffer[LWP_FP] = (jmp_buf_type) sp;
#endif
		longjmp(jmp_tmp, 1);
		break;
	    case 1:
		(*EP) ();
		assert(0);	/* never returns */
		break;
	    default:
		perror("Error in setjmp1\n");
		exit(2);
	    }
	}
	break;
    case 2:			/* restoring frame */
	break;

    default:
	perror("Error in setjmp2 : restoring\n");
	exit(3);
    }
    return 0;
}

afs_int32
returnto(struct lwp_context * savearea)
{
#if	defined(DEBUG)
    int i, *ptr = savearea->setjmp_buffer;

    printf("Returning to \n");
    for (i = 0; i < 5; i++)
	printf("(%d) 0x%x   ", i, ptr[i]);
    printf("\n");
    for (i = 5; i < 10; i++)
	printf("(%d) 0x%x   ", i, ptr[i]);
    printf("\n");
#endif
    PRE_Block = 0;
    longjmp(savearea->setjmp_buffer, 2);
    return 0;
}

#endif
