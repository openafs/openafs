/*
 * Header file for common error description library.
 *
 * Copyright 1988, Student Information Processing Board of the
 * Massachusetts Institute of Technology.
 *
 * For copyright and distribution info, see the documentation supplied
 * with this package.
 */

#ifndef __AFS_COM_ERR_H

#include <stdarg.h>

extern void afs_com_err(const char *, afs_int32, const char *, ...)
    AFS_ATTRIBUTE_FORMAT(__printf__, 3, 4);

extern void afs_com_err_va(const char *whoami, afs_int32 code, const char *fmt,
		       va_list args)
    AFS_ATTRIBUTE_FORMAT(__printf__, 3, 0);

extern const char *afs_error_table_name(afs_int32);
extern const char *afs_error_message(afs_int32);
extern const char *afs_error_message_localize(afs_int32 code, char *str, size_t len);
extern
void (*afs_set_com_err_hook
      (void (*)(const char *, afs_int32, const char *, va_list)))
  (const char *, afs_int32, const char *, va_list);
extern void (*afs_reset_com_err_hook(void)) (const char *, afs_int32,
					 const char *, va_list);

#define __AFS_COM_ERR_H
#ifdef AFS_OLD_COM_ERR
#define com_err                 afs_com_err
#define com_err_va              afs_com_err_va
#define error_table_name        afs_error_table_name
#define error_message           afs_error_message
#define set_com_err_hook        afs_set_com_err_hook
#define reset_com_err_hook      afs_reset_com_err_hook
#endif /* AFS_OLD_COM_ERR */
#endif /* ! defined(__AFS_COM_ERR_H) */
