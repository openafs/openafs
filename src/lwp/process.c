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

#include <roken.h>

#include <assert.h>

#include "lwp.h"

#if defined(AFS_S390_LINUX_ENV)
extern int PRE_Block;		/* used in lwp.c and process.s */
#else
extern char PRE_Block;		/* used in lwp.c and process.s */
#endif

#if defined(USE_UCONTEXT) && defined(HAVE_UCONTEXT_H)

# if defined(AFS_LINUX_ENV) || defined(AFS_XBSD_ENV)
#  define AFS_UCONTEXT_NOSTACK
# endif

afs_int32
savecontext(void (*ep) (void), struct lwp_context *savearea, char *newsp)
{
# ifdef AFS_UCONTEXT_NOSTACK
    /* getcontext does not export stack info */
    int stackvar;
# endif

    PRE_Block = 1;

    savearea->state = 0;
    getcontext(&savearea->ucontext);
# ifdef AFS_UCONTEXT_NOSTACK
    savearea->topstack = (char *)&stackvar;
# else
    savearea->topstack = savearea->ucontext.uc_stack.ss_sp;
# endif
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
returnto(struct lwp_context *savearea)
{
    PRE_Block = 0;

    savearea->state = 2;
    setcontext(&savearea->ucontext);
}

#else

/*
 * Magic stack pointer
 */
#if	defined(AFS_SGI_ENV)
# ifdef _BSD_COMPAT
#  define LWP_SP 34
# else
#  define LWP_SP JB_SP
# endif
#elif	defined(AFS_HPUX_ENV)
#define	LWP_SP	1
#elif	defined(AFS_LINUX_ENV)
#if defined(AFS_PPC_LINUX_ENV) || defined(AFS_PPC64_LINUX_ENV)
#define LWP_SP 0
#elif   defined(AFS_I386_LINUX_ENV)
#define LWP_SP 4
#elif   defined(AFS_S390_LINUX_ENV)
#define LWP_SP 9
#define LWP_FP 5
#elif   defined(AFS_SPARC_LINUX_ENV)
#define LWP_SP 0
#define LWP_FP 1
#elif   defined(AFS_SPARC64_LINUX_ENV) && defined(AFS_32BIT_USR_ENV)
#define LWP_SP 0
#define LWP_FP 1
#elif defined(AFS_ALPHA_LINUX_ENV)
#define LWP_SP 8
#define LWP_FP 7
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
#ifdef AFS_SGI_ENV
#if (_MIPS_ISA == _MIPS_ISA_MIPS3 || _MIPS_ISA == _MIPS_ISA_MIPS4)
typedef __uint64_t jmp_buf_type;
#endif
#elif defined(AFS_ARM64_DARWIN_ENV)
typedef __uint64_t jmp_buf_type;
#else
#if defined(AFS_ALPHA_LINUX_ENV) || defined(AFS_PPC64_LINUX_ENV)
typedef long jmp_buf_type;
#else
typedef int jmp_buf_type;
#endif /*AFS_ALPHA_LINUX_ENV */
#endif /*SGI*/

    static jmp_buf jmp_tmp;
static void (*EP) (void);
static int rc;
static jmp_buf_type *jmpBuffer;

/** Starting with Glibc 2.4 pointers in jmp_buf are mangled (XOR) for "protection".
  * On Sparc ucontext functions are not implemented.
  */
#define ptr_mangle(x) (x)
#ifdef AFS_LINUX_ENV

#ifdef __GLIBC__
#if (__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ > 3)

#if defined(AFS_SPARC64_LINUX_ENV) || defined(AFS_SPARC_LINUX_ENV)
/* technically we should use POINTER_GUARD
 * ( == offsetof (tcbhead_t, pointer_guard) )
 * instead of 0x18
 */
#undef ptr_mangle
static int ptr_mangle(int p)
{
    register char *tls_ptr __asm__("%g7");
    return p ^ *(int*)(tls_ptr + 0x18);
}
#else
#error need ptr_mangle support or use UCONTEXT
#endif

#endif
#endif
#endif


afs_int32
savecontext(void (*ep)(void), struct lwp_context *savearea, char *sp)
{
    int code;

    PRE_Block = 1;
    EP = ep;

    code = setjmp(savearea->setjmp_buffer);
    jmpBuffer = (jmp_buf_type *) savearea->setjmp_buffer;
    savearea->topstack = (char *) ptr_mangle(jmpBuffer[LWP_SP]);

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
		jmpBuffer[LWP_SP] = ptr_mangle((jmp_buf_type) sp);
#if defined(AFS_S390_LINUX_ENV) || defined(AFS_SPARC_LINUX_ENV) || (defined(AFS_SPARC64_LINUX_ENV) && defined(AFS_32BIT_USR_ENV))
		jmpBuffer[LWP_FP] = ptr_mangle((jmp_buf_type) sp);
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

void
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
    return;
}

#endif
