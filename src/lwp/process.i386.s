/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Sun 386i... I hope this does the right thing!!!
 * 
 * Written by Derek Atkins <warlord@MIT.EDU>
 * (debugging help by Chris Provenzano <proven@mit.edu>)
 * 11/1991
 *
 * "ojala que es correcto!"
 */

#define IGNORE_STDS_H	1
/*#ifndef AFS_DJGPP_ENV*/
#include <afs/param.h>
/*#endif /* AFS_DJGPP_ENV */

	.file "process.s"

	.data

	.text

/*
 * struct savearea {
 * 	char	*topstack;
 * }
 */

	.set 	topstack,0

/*
 * savecontext(f, area1, newsp)
 *	int (*f)(); struct savearea *area1; char *newsp;
 */

/* offsets, to make my life easier! */
	.set	f,8
	.set	area1,12
	.set	newsp,16


#if defined(AFS_DJGPP_ENV) || (defined(AFS_OBSD_ENV) && !defined (AFS_OBSD34_ENV))
#define PRE_Block	_PRE_Block
#define savecontext	_savecontext
#define abort		_abort
#define returnto	_returnto
#endif /* AFS_DJGPP_ENV */

.globl	PRE_Block
.globl	savecontext

	savecontext:
	pushl	%ebp			/* New Frame! */
	movl	%esp,%ebp
	pusha				/* Push all registers */
	movl	$1,PRE_Block		/* Pre-emption code */
	movl	area1(%ebp),%eax	/* eax = base of savearea */
	movl	%esp,(%eax)		/* area->topstack = esp */
	movl	newsp(%ebp),%eax	/* get new sp into eax */
	cmpl	$0,%eax
	je	L1			/* if new sp is 0 then dont change esp */
	movl	%eax,%esp		/* go ahead.  make my day! */
L1:
	jmp	*f(%ebp)			/* ebx = &f */

/* Shouldnt be here....*/

	call	abort

/*
 * returnto(area2)
 *	struct savearea *area2;
 */

/* stack offset */
	.set	area2,8

.globl	returnto

returnto:
	pushl	%ebp
	movl	%esp, %ebp		/* New frame, to get correct pointer */
	movl	area2(%ebp),%eax	/* eax = area2 */
	movl	(%eax),%esp		/* restore esp */
	popa 
	movl	$0,PRE_Block		/* clear it up... */
	popl	%ebp
	ret

/* I see, said the blind man, as he picked up his hammer and saw! */
	pushl	$1234
	call	abort
