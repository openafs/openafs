/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* process.c - manage lwp context switches be means of setjmp/longjmp. */

#include <stdio.h>
#include <assert.h>
#include "lwp.h"

#ifdef  AFS_OSF_ENV
extern int PRE_Block;              /* used in lwp.c and process.s */
#else
extern char PRE_Block;             /* used in lwp.c and process.s */
#endif

/*
 * Magic stack pointer
 */
#if	defined(AFS_SGI64_ENV)
# ifdef _BSD_COMPAT
#  define LWP_SP 34
# else
#  define LWP_SP JB_SP
# endif
#elif	defined(AFS_HPUX_ENV)
#define	LWP_SP	1
#elif	defined(AFS_LINUX20_ENV)
#define LWP_SP 4
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
typedef int jmp_buf_type;
#endif

static jmp_buf	jmp_tmp;
static char	(*EP)();
static int  	rc;
static jmp_buf_type  *jmpBuffer;
static int 	code;

savecontext(ep, savearea, sp)
char	(*ep)();
struct lwp_context *savearea;
char*	sp;
{
	int code;

	PRE_Block = 1;
	EP = ep;

	code =  setjmp(savearea->setjmp_buffer);
	jmpBuffer = (jmp_buf_type *)savearea->setjmp_buffer;
	savearea->topstack = (char*)jmpBuffer[LWP_SP];

#if	defined(DEBUG)
	{
	    int i, *ptr = (int*)savearea->setjmp_buffer;
	    printf("savecontext\n");
	    for ( i=0; i < 5; i++)
		printf("(%d) 0x%x   ",i, ptr[i]);
	    printf("\n");
	    for ( i=5; i < 10; i++)
		printf("(%d) 0x%x   ",i, ptr[i]);
	    printf("\n");
	}
#endif
	switch ( code )
	{
		case 0: if ( !sp )
			(*EP)();
		   	else
		   	{
				rc = setjmp(jmp_tmp);
				switch ( rc )
				{
				case 0: jmpBuffer = (jmp_buf_type *)jmp_tmp;
					jmpBuffer[LWP_SP] = (jmp_buf_type)sp; 
				   	longjmp(jmp_tmp,1);
				   	break;
				case 1: (*EP)();
					assert(0); /* never returns */
				   	break;
				default:
				   	perror("Error in setjmp1\n");
				   	exit(2);
				}
		  	}
		  	break; 
               case 2: /* restoring frame */
		  	break;
	
	       default:
		  	perror("Error in setjmp2 : restoring\n");
		  	exit(3);
	}
}

returnto(savearea)
struct lwp_context *savearea;
{
#if	defined(DEBUG)
	int i, *ptr = savearea->setjmp_buffer;

	printf("Returning to \n");
	for ( i=0; i < 5; i++)
		printf("(%d) 0x%x   ",i, ptr[i]);
	printf("\n");
	for ( i=5; i < 10; i++)
		printf("(%d) 0x%x   ",i, ptr[i]);
	printf("\n");
#endif
	PRE_Block = 0;
	longjmp(savearea->setjmp_buffer, 2);
}

