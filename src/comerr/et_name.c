/*
 * Copyright 1987 by MIT Student Information Processing Board
 *
 * For copyright info, see mit-sipb-cr.h.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/afsutil.h>

RCSID
    ("$Header: /cvs/openafs/src/comerr/et_name.c,v 1.5.2.1 2007/04/10 18:43:42 shadow Exp $");

#include "error_table.h"
#include "mit-sipb-cr.h"
#include "internal.h"

#ifndef	lint
static const char copyright[] =
    "Copyright 1987,1988 by Student Information Processing Board, Massachusetts Institute of Technology";
#endif

static const char char_set[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_";

static char buf[6];

const char *
afs_error_table_name(afs_int32 num)
{
    int ch;
    int i;
    char *p;

    /* num = aa aaa abb bbb bcc ccc cdd ddd d?? ??? ??? */
    p = buf;
    num >>= ERRCODE_RANGE;
    /* num = ?? ??? ??? aaa aaa bbb bbb ccc ccc ddd ddd */
    num &= 077777777;
    /* num = 00 000 000 aaa aaa bbb bbb ccc ccc ddd ddd */
    for (i = 4; i >= 0; i--) {
	ch = (num >> BITS_PER_CHAR * i) & ((1 << BITS_PER_CHAR) - 1);
	if (ch != 0)
	    *p++ = char_set[ch - 1];
    }
    *p = '\0';
    return (lcstring(buf, buf, sizeof(buf)));
}
