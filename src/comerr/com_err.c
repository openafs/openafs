/*
 * Copyright 1987, 1988 by MIT Student Information Processing Board.
 *
 * For copyright info, see mit-sipb-cr.h.
 */

#include "internal.h"
#include <stdio.h>
#include <stdarg.h>
#include "error_table.h"


/*
 * Protect us from header version (externally visible) of com_err, so
 * we can survive in a <varargs.h> environment.  I think.
 */
#define com_err com_err_external
/* #include "com_err.h" */
char *error_message();
#undef com_err

extern char *error_message ();

static void
    default_com_err_proc (const char *whoami, afs_int32 code, const char *fmt, va_list args)
{
    if (whoami) {
	fputs(whoami, stderr);
	fputs(": ", stderr);
    }
    if (code) {
	fputs(error_message(code), stderr);
	fputs(" ", stderr);
    }
    if (fmt) {
        vfprintf (stderr, fmt, args);
    }
    putc('\n', stderr);
    /* should do this only on a tty in raw mode */
    putc('\r', stderr);
    fflush(stderr);
}

typedef void (*errf) (const char *, afs_int32, const char *, va_list);

static errf com_err_hook = default_com_err_proc;

void com_err_va (whoami, code, fmt, args)
    const char *whoami;
    afs_int32 code;
    const char *fmt;
    va_list args;
{
    (*com_err_hook) (whoami, code, fmt, args);
}

void com_err (const char *whoami,
	      afs_int32 code,
	      const char *fmt, ...)
{
    va_list pvar;

    if (!com_err_hook)
	com_err_hook = default_com_err_proc;
    va_start(pvar, fmt);
    com_err_va (whoami, code, fmt, pvar);
    va_end(pvar);
}

errf set_com_err_hook (new_proc)
    errf new_proc;
{
    errf x = com_err_hook;
    if (new_proc) com_err_hook = new_proc;
    else com_err_hook = default_com_err_proc;
    return x;
}

errf reset_com_err_hook () {
    errf x = com_err_hook;
    com_err_hook = default_com_err_proc;
    return x;
}
