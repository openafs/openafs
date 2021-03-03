/*
 * Copyright (C) 2014 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This contains portable implementations of the BSD ffs() suite of functions,
 * which locate the first or last bit set in a bit string.
 */

#ifndef OPENAFS_OPR_FFS_H
#define OPENAFS_OPR_FFS_H

static_inline int
opr_ffs(int value)
{
    afs_int32 i;
    afs_uint32 tmp = value;

    if (tmp == 0)
	return 0;
    /* This loop must terminate because tmp is nonzero and thus has at least
     * one bit set. */
    for (i = 1;; ++i) {
	if (tmp & 1u)
	    return i;
	else
	    tmp >>= 1;
    }
    /* NOTREACHED */
}

static_inline int
opr_fls(int value)
{
    afs_int32 i;
    /* tmp must be unsigned to avoid undefined behavior. */
    afs_uint32 tmp = value;

    if (tmp == 0)
	return 0;
    /* This loop must terminate because tmp is nonzero and thus has at least
     * one bit set. */
    for (i = 32;; --i) {
	if (tmp & 0x80000000u)
	    return i;
	else
	    tmp <<= 1;
    }
    /* NOTREACHED */
}

#endif /* OPENAFS_OPR_FFS_H */
