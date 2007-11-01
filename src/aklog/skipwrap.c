
/*
 * Copyright (c) 2006
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the University of Michigan as to its fitness for any
 * purpose, and without warranty by the University of
 * Michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the University of Michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <stdio.h>
#include <aklog.h>
#include <krb5.h>

/* evil hack */
#define SEQUENCE 16
#define CONSTRUCTED 32
#define APPLICATION 64
#define CONTEXT_SPECIFIC 128
static int skip_get_number(char **pp, size_t *lp, int *np)
{
    unsigned l;
    int r, n, i;
    char *p;

    l = *lp;
    if (l < 1) {
#ifdef DEBUG
	fprintf(stderr, "skip_bad_number: missing number\n");
#endif
	return -1;
    }
    p = *pp;
    r = (unsigned char)*p;
    ++p; --l;
    if (r & 0x80) {
	n = (r&0x7f);
	if (l < n) {
#ifdef DEBUG
	    fprintf(stderr, "skip_bad_number: truncated number\n");
#endif
	    return -1;
	}
	r = 0;
	for (i = n; --i >= 0; ) {
	    r <<= 8;
	    r += (unsigned char)*p;
	    ++p; --l;
	}
    }
    *np = r;
    *pp = p;
    *lp = l;
    return 0;
}

int
afs_krb5_skip_ticket_wrapper(char *tix, size_t tixlen, char **enc, size_t *enclen)
{
    char *p = tix;
    size_t l = tixlen;
    int code;
    int num;

    if (l < 1) return -1;
    if (*p != (char) (CONSTRUCTED+APPLICATION+1)) return -1;
    ++p; --l;
    if ((code = skip_get_number(&p, &l, &num))) return code;
    if (l != num) return -1;
    if (l < 1) return -1;
    if (*p != (char)(CONSTRUCTED+SEQUENCE)) return -1;
    ++p; --l;
    if ((code = skip_get_number(&p, &l, &num))) return code;
    if (l != num) return -1;
    if (l < 1) return -1;
    if (*p != (char)(CONSTRUCTED+CONTEXT_SPECIFIC+0)) return -1;
    ++p; --l;
    if ((code = skip_get_number(&p, &l, &num))) return code;
    if (l < num) return -1;
    l -= num; p += num;
    if (l < 1) return -1;
    if (*p != (char)(CONSTRUCTED+CONTEXT_SPECIFIC+1)) return -1;
    ++p; --l;
    if ((code = skip_get_number(&p, &l, &num))) return code;
    if (l < num) return -1;
    l -= num; p += num;
    if (l < 1) return -1;
    if (*p != (char)(CONSTRUCTED+CONTEXT_SPECIFIC+2)) return -1;
    ++p; --l;
    if ((code = skip_get_number(&p, &l, &num))) return code;
    if (l < num) return -1;
    l -= num; p += num;
    if (l < 1) return -1;
    if (*p != (char)(CONSTRUCTED+CONTEXT_SPECIFIC+3)) return -1;
    ++p; --l;
    if ((code = skip_get_number(&p, &l, &num))) return code;
    if (l != num) return -1;
    *enc = p;
    *enclen = l;
    return 0;
}
