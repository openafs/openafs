# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

# $Locker$
#
# misc.s -	miscellaneous assembler routines for the rs/6000
#

#
# get_toc -	return the current TOC
#
 	.csect .get_toc[PR]
	 .globl .get_toc[PR]
	mr	3,2		# get TOC
	br
        .align 2
	.tbtag 0x0,0xc,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0

 	.toc
	.csect  get_toc[DS]
	.globl  get_toc[DS]
	.long  .get_toc[PR]
	.long  TOC[t0]

#
# get_ret_addr -	return the caller's return address
#
	.csect .get_ret_addr[PR]
	.globl .get_ret_addr[PR]
	l	3, 0(1)		# caller's saved stack pointer
	l	3, 8(3)		# caller's saved link register
	br
	.align 2
	.tbtag 0x0,0xc,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0
 	.toc
        .csect  get_ret_addr[DS]
        .globl  get_ret_addr[DS]
	.long  .get_ret_addr[PR]
        .long  TOC[t0]
