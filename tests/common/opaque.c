/*
 * Copyright (c) 2019 Sine Nomine Associates
 * All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <roken.h>

#include <rx/rx_opaque.h>

#include <tests/tap/basic.h>
#include "common.h"

int
is_opaque_v(struct rx_opaque *left, struct rx_opaque *right,
	    const char *fmt, va_list ap)
{
    struct rx_opaque_stringbuf strbuf;
    int success = 0;

    /*
     * Be careful about NULL; rx_opaque_cmp() may segfault if it's given an
     * opaque with a NULL val. If 'val' is NULL for either arg, don't call
     * rx_opaque_cmp(), and only consider the args equal if the bare values in
     * the struct are identical (via memcmp()). So, if both args are {2,NULL}
     * they are considered equal, but {2,NULL} and {3,NULL} are not equal.
     */
    if (left->val == NULL || right->val == NULL) {
	if (memcmp(left, right, sizeof(*left)) == 0) {
	    success = 1;
	}
    } else if (rx_opaque_cmp(left, right) == 0) {
	success = 1;
    }

    if (!success) {
	diag("left:");
	diag(" %s", rx_opaque_stringify(left, &strbuf));
	diag("right:");
	diag(" %s", rx_opaque_stringify(right, &strbuf));
    }

    return okv(success, fmt, ap);
}

int
is_opaque(struct rx_opaque *left, struct rx_opaque *right,
	  const char *fmt, ...)
{
    va_list ap;
    int success;

    va_start(ap, fmt);
    success = is_opaque_v(left, right, fmt, ap);
    va_end(ap);

    return success;
}
