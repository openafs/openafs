/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#define	IGNORE_STDS_H
#include <afs/param.h>

#if defined(AFS_AIX32_ENV) && defined(RIOS)
	/*
	 * This is never be referenced, and is only here as placeholder;
	 * that's AIX, I'm afraid.
	 */
	.globl	.rios_dummy_sysc[PR]
	.csect	.rios_dummy_sysc[PR]
	t	4,1,1		# trap always
#ifdef	AFS_AIX41_ENV
	br			# just return
#else

	bcr	14, 0		# and return	
#endif
#endif
#ifdef __hp9000s800
        .CODE
dummysysc
        .PROC
        .CALLINFO FRAME=0
        .ENTRY
        STW     %r19,-28(0,%r30)
	LDIL	L%0xc0000004,%r1
	BLE	R%0xc0000004(%sr7,%r1)
	LDI	4,%r22	; Load the value here (in place of 4)
	OR,=	0,22,0
	B	$cerror
	LDW	-28(0,%r30),%r19
        BV,N    %r0(%r2)
	OR	0,0,0
        .EXIT
        .PROCEND

        .EXPORT dummysysc,ENTRY
	.IMPORT $cerror,CODE
        .END
#else
#if defined(AFS_SGI_ENV)
#include <regdef.h>
#include <sys/asm.h>

LEAF(afs_syscall)
	li	v0,AFS_SYSCALL
	syscall
	beq	a3,zero,9f
	la	t9, _cerror
	j	t9
9:	j	ra
.end	afs_syscall
#endif /* AFS_SGI_ENV */
#endif

#if defined(__linux__) && defined(__ELF__)
	.section .note.GNU-stack,"",%progbits
#endif
