/* This file contains definitions for use by the Hesiod name service and
 * applications.
 *
 * @doc
 *
 * @module hesiod.h |
 * For copying and distribution information, see the file 
 * <gt> mit-copyright.h <lt>.
 *
 * Original version by Steve Dyer, IBM/Project Athena.
 * 
 */

/* Configuration information. */

#ifndef _HESIOD_
#define _HESIOD_

#if defined(_WINDOWS) || defined(_WIN32)
#include <windows.h>
#endif

#if defined(_WINDOWS) || defined(_WIN32)
#define HESIOD_CONF     "c:\\net\\tcp\\hesiod.cfg"
#else
#define HESIOD_CONF     "/etc/hesiod.conf"      /* Configuration file. */
#endif

#define DEF_RHS         ".Athena.MIT.EDU"       /* Defaults if HESIOD_CONF */
#define DEF_LHS         ".ns"                   /*    file is not present. */

/* @doc ERROR
/* Error codes. */
/*

@type HES_ER_UNINIT   | -1      uninitialized 
@type HES_ER_OK       | 0       no error 
@type HES_ER_NOTFOUND | 1       Hesiod name not found by server 
@type HES_ER_CONFIG   | 2       local problem (no config file?) 
@type HES_ER_NET      | 3        network problem

 */

#define HES_ER_UNINIT   -1      /* uninitialized */
#define HES_ER_OK       0       /* no error */
#define HES_ER_NOTFOUND 1       /* Hesiod name not found by server */
#define HES_ER_CONFIG   2       /* local problem (no config file?) */
#define HES_ER_NET      3       /* network problem */

/* Declaration of routines */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WINDOWS) || defined(_WIN32)

LPSTR WINAPI
hes_to_bind(
    LPSTR HesiodName, 
    LPSTR HesiodNameType
    );

LPSTR * WINAPI
hes_resolve(
    LPSTR HesiodName,
    LPSTR HesiodNameType
    );

int WINAPI
hes_error(
    void
    );

#else 
char *hes_to_bind(const char *name, const char *type);
char **hes_resolve(const char *name, const char *type);
int hes_error(void);
#endif /* WINDOWS */


/*
 * @doc
 *
 * @struct hes_postoffice | For use in getting post-office information.
 *
 * @field LPSTR   | po_type | The post office type, e.g. POP, IMAP 
 * @field LPSTR   | po_host | The post office host, e.g. PO10.MIT.EDU
 * @field LPSTR   | po_name | The account name on the post office, e.g. tom
 *
 */
#if defined(_WINDOWS) || defined(_WIN32)
struct hes_postoffice {
    LPSTR   po_type;
    LPSTR   po_host;
    LPSTR   po_name;
};
#else
struct hes_postoffice {
    char    *po_type;
    char    *po_host;
    char    *po_name;
};
#endif

/* Other routines */

#if defined(_WINDOWS) || defined(_WIN32)
struct hes_postoffice FAR * WINAPI hes_getmailhost(LPSTR user);
struct servent FAR * WINAPI hes_getservbyname(LPSTR name, 
                                                              LPSTR proto);
struct passwd FAR * WINAPI hes_getpwnam(LPSTR nam);
struct passwd FAR * WINAPI hes_getpwuid(int uid);
#else
struct hes_postoffice *hes_getmailhost();
struct servent *hes_getservbyname();
struct passwd *hes_getpwnam();
struct passwd *hes_getpwuid();
#endif

#ifdef __cplusplus
}
#endif

#endif /* _HESIOD_ */
