/*
 * Header file for common error description library.
 *
 * Copyright 1988, Student Information Processing Board of the
 * Massachusetts Institute of Technology.
 *
 * For copyright and distribution info, see the documentation supplied
 * with this package.
 */

#ifndef __COM_ERR_H

#include <stdarg.h>

void com_err (const char *, afs_int32, const char *, ...);
char const *error_table_name(afs_int32);
char const *error_message (afs_int32);
void (*set_com_err_hook (void (*) (const char *, afs_int32, const char *, va_list)))
    (const char *, afs_int32, const char *, va_list);
void (*reset_com_err_hook ()) (const char *, afs_int32, const char *, va_list);

#define __COM_ERR_H
#endif /* ! defined(__COM_ERR_H) */
