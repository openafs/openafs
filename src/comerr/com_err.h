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

extern void com_err(const char *, afs_int32, const char *, ...);
extern void com_err_va(const char *whoami, afs_int32 code, const char *fmt,
		       va_list args);
extern const char *error_table_name(afs_int32);
extern const char *error_message(afs_int32);
extern
void (*set_com_err_hook
      (void (*)(const char *, afs_int32, const char *, va_list)))
  (const char *, afs_int32, const char *, va_list);
extern void (*reset_com_err_hook(void)) (const char *, afs_int32,
					 const char *, va_list);

#define __COM_ERR_H
#endif /* ! defined(__COM_ERR_H) */
