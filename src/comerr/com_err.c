/*
 * Copyright 1987, 1988 by MIT Student Information Processing Board.
 *
 * For copyright info, see mit-sipb-cr.h.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/comerr/com_err.c,v 1.5.2.1 2007/04/10 18:43:42 shadow Exp $");

#include "internal.h"
#include <stdio.h>
#include <stdarg.h>
#include "error_table.h"
#include "com_err.h"

static void
default_com_err_proc(const char *whoami, afs_int32 code, const char *fmt,
		     va_list args)
{
    if (whoami) {
	fputs(whoami, stderr);
	fputs(": ", stderr);
    }
    if (code) {
	fputs(afs_error_message(code), stderr);
	fputs(" ", stderr);
    }
    if (fmt) {
	vfprintf(stderr, fmt, args);
    }
    putc('\n', stderr);
    /* should do this only on a tty in raw mode */
    putc('\r', stderr);
    fflush(stderr);
}

typedef void (*errf) (const char *, afs_int32, const char *, va_list);

static errf com_err_hook = default_com_err_proc;

void
afs_com_err_va(const char *whoami, afs_int32 code, const char *fmt, va_list args)
{
    (*com_err_hook) (whoami, code, fmt, args);
}

void
afs_com_err(const char *whoami, afs_int32 code, const char *fmt, ...)
{
    va_list pvar;

    if (!com_err_hook)
	com_err_hook = default_com_err_proc;
    va_start(pvar, fmt);
    afs_com_err_va(whoami, code, fmt, pvar);
    va_end(pvar);
}

errf
afs_set_com_err_hook(errf new_proc)
{
    errf x = com_err_hook;
    if (new_proc)
	com_err_hook = new_proc;
    else
	com_err_hook = default_com_err_proc;
    return x;
}

errf
afs_reset_com_err_hook(void)
{
    errf x = com_err_hook;
    com_err_hook = default_com_err_proc;
    return x;
}
