/* Taken from linux-2.6/arch/ppc64/boot/string.S 
 *
 * Copyright (C) Paul Mackerras 1997.
 *
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Flush the dcache and invalidate the icache for a range of addresses.
 *
 * flush_cache(addr, len)
 */
	.section        ".text"
	.align          2
	.globl          flush_cache
	.section        ".opd","aw"
	.align          3
flush_cache:
	.quad           .flush_cache,.TOC.@tocbase,0
	.previous
	.size           flush_cache,24
	.globl          .flush_cache
.flush_cache:
        addi    4,4,0x1f        /* len = (len + 0x1f) / 0x20 */
        rlwinm. 4,4,27,5,31
        mtctr   4
        beqlr
1:      dcbf    0,3
        icbi    0,3
        addi    3,3,0x20
        bdnz    1b
        sync
        isync
        blr
	.long           0
	.byte           0,12,0,0,0,0,0,0
	.type           .flush_cache,@function
	.size           .flush_cache,.-.flush_cache
