/*
 * HA HA HA HA!!!! THIS IS A COM_ERR SUBSTITUTE!!!! HA HA HA HA!!!!
 */

#ifndef _DCNS_MIT_COM_ERR_H
#define _DCNS_MIT_COM_ERR_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include <stdarg.h>

#if !defined(WINDOWS) && !defined(_WIN32)
#define FAR
#define LPSTR char *
#endif

typedef LPSTR (*err_func)(int, long);

#ifdef WIN16
#define COMEXP __far __export
int COMEXP com_err_export (LPSTR, long, LPSTR, ...);
LPSTR COMEXP error_message_export (long);
LPSTR COMEXP error_table_name_export(long num);
extern int (*com_err) (LPSTR, long, LPSTR, ...);
int mbprintf (LPSTR, LPSTR, ...);
LPSTR (*error_message) (long);
int (*set_com_err_hook (int (*) (LPSTR, long, LPSTR, va_list)))
    (LPSTR, long, LPSTR, va_list);
extern int (*reset_com_err_hook ()) (LPSTR, long, LPSTR, va_list);

// extern LPSTR (*error_table_name)(long num);
LPSTR (*error_table_name)(long num);

#else
#define COMEXP
int com_err (LPSTR, long, LPSTR, ...);
int mbprintf (LPSTR, LPSTR, ...);
LPSTR error_message (long);
int (*set_com_err_hook (int (*) (LPSTR, long, LPSTR, va_list)))
    (LPSTR, long, LPSTR, va_list);
int (*reset_com_err_hook ()) (LPSTR, long, LPSTR, va_list);
LPSTR error_table_name(long num);

#endif



#if defined(__cplusplus)
}
#endif

#endif // _DCNS_MIT_COM_ERR_H

