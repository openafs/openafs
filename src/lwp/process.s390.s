/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Linux for S/390 (31 bit)
 *
 * Written by Neale Ferguson <Neale.Ferguson@SoftwareAG-usa.com>
 *
 *  additional munging by Adam Thornton <adam@sinenomine.net>
 */

#define	IGNORE_STDS_H	1
#include <afs/param.h>

	.file   "process.s"

              .globl savecontext
              .type  savecontext,%function
      /*
       * savecontext(f, area1, newsp)
       *      int (*f)();    struct savearea *area1; char *newsp;
       * f     - r2
       * area1 - r3
       * newsp - r4
       */

       /*
        * struct savearea {
        *      char    *topstack;
        * }
        */

P_PRE:	                  .long   PRE_Block
P_ABORT:	              .long   abort

savecontext:
              stm     %r6,%r15,24(%r15)       /* Save our registers */
              lr      %r1,%r15
              ahi     %r15,-96                /* Move out of harm's way */
              st      %r1,0(%r15)
              bras    %r5,.L0                 /* Get A(A(PRE_Block)) */
              .long   PRE_Block
      .L0:
              l       %r5,0(%r5)              /* Get A(PRE_Block) */
              mvi     3(%r5),1                /* Set it */
              lr      %r6,%r3                 /* Get base of savearea */
              st      %r15,0(%r3)             /* Save stack pointer */
              ltr     %r4,%r4                 /* If new sp is 0 */
              jz      .L1                     /* ... don't change sp */
              lr      %r15,%r4                /* Set new stack pointer */
      .L1:
              br      %r2                     /* Call the routine */
              /* Can't get here....*/

              bras    %r5,.L2
              .long   abort
      .L2:
              l      %r5,0(%r5)
              balr    %r14,%r5

      .savecontext_end:
              .size   savecontext,.savecontext_end-savecontext

      /*
       * returnto(area2)
       *      struct savearea *area2;
       *
       * area2 - r2
       */
        .globl  returnto
        .type   returnto,%function
returnto:
        l       %r15,0(%r2)             /* New frame, to get correct pointer*/
        bras    %r5,.L3                         /* Get A(A(PRE_Block))
      */
                 .long          PRE_Block
      .L3:
              l       %r5,0(%r5)              /* Get A(PRE_Block) */
              /*xc      0(4,%r5),0(%r5)         /* Clear it */
	      mvi     3(%r5),0                /* Clear it */ 
	      l       %r15,0(%r15)
              lm      %r6,%r15,24(%r15)       /* Restore registers */
              br      %r14                    /* Return */

	      /* Can't happen */
              la      %r2,1234
              bras    %r5,.L4
                .long          abort
      .L4:
              l       %r5,0(%r5)
              basr    %r14,%r5
      .returnto_end:
              .size   returnto,.returnto_end-returnto

#if defined(__linux__) && defined(__ELF__)
        .section .note.GNU-stack,"",%progbits
#endif
