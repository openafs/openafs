#ifndef __LOADFUNCS_KRB_H__
#define __LOADFUNCS_KRB_H__

#include "loadfuncs.h"
#include <krb.h>

#define KRB4_DLL      "krbv4w32.dll"

#define krb_err_text(status) pget_krb_err_txt_entry(status)
#define CALLCONV_C

TYPEDEF_FUNC(
    char *,
    CALLCONV_C,
    tkt_string,
    ()
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    gettimeofday,
    (struct timeval *tv, struct timezone *tz)
    );
TYPEDEF_FUNC(
    int,
    PASCAL,
    krb_sendauth,
    (long, int, KTEXT, char *, char *, char *,
     unsigned long, MSG_DAT *, CREDENTIALS *, 
     Key_schedule *, struct sockaddr_in *, 
     struct sockaddr_in FAR *, char *)
    );
TYPEDEF_FUNC(
    int,
    PASCAL,
    krb_mk_req,
    (KTEXT, char *, char *, char *, long)
    );
TYPEDEF_FUNC(
    char *,
    PASCAL,
    krb_getrealm,
    (char *host)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    krb_get_tf_fullname,
    (char FAR *, char FAR *, char FAR *, char FAR *)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    krb_get_tf_realm,
    (char FAR *,char FAR *)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    tf_init,
    (char FAR*,int)
    );
TYPEDEF_FUNC(
    long,
    CALLCONV_C,
    LocalHostAddr,
    ()
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    tf_get_pname,
    (char FAR*)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    tf_get_pinst,
    (char FAR*)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    tf_get_cred,
    (CREDENTIALS FAR*)
    );
TYPEDEF_FUNC(
    void,
    CALLCONV_C,
    tf_close,
    (void)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    tf_save_cred,
    (char FAR*,char FAR*,char FAR*,C_Block,int,int,KTEXT,long)
    );
TYPEDEF_FUNC(
    BOOL,
    CALLCONV_C,
    k_isinst,
    (char FAR *s)
    );
TYPEDEF_FUNC(
    BOOL,
    CALLCONV_C,
    k_isrealm,
    (char FAR *s)
    );
TYPEDEF_FUNC(
    BOOL,
    CALLCONV_C,
    k_isname,
    (char FAR *s)
    );

TYPEDEF_FUNC(
    char **,
    CALLCONV_C,
    get_krb_err_txt,
    (void)
    );

/* Warning, unique to Windows! */
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    set_krb_debug,
    (int)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    set_krb_ap_req_debug,
    (int)
    );
TYPEDEF_FUNC(
    char *,
    PASCAL,
    get_krb_err_txt_entry,
    (int i)
    );
TYPEDEF_FUNC(
    char *,
    CALLCONV_C,
    krb_err_func,
    (int offset, long code)
    );
TYPEDEF_FUNC(
    int,
    PASCAL,
    k_decomp_ticket,
    (KTEXT, unsigned char *, char *, char *, char *, 
     unsigned long *, C_Block, int *, unsigned long *, 
     char *, char *, C_Block, Key_schedule)
    );
TYPEDEF_FUNC(
    int,
    PASCAL,
    krb_mk_req,
    (KTEXT,char *,char *,char *,long)
    );
TYPEDEF_FUNC(
    char *,
    PASCAL,
    krb_getrealm,
    (char *host)
    );
TYPEDEF_FUNC(
    char *,
    PASCAL,
    krb_realmofhost,
    (char *host)
    );
TYPEDEF_FUNC(
    char *,
    CALLCONV_C,
    krb_get_phost,
    (char *host)
    );

TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    kname_parse,
    (char *, char *, char *, char *)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    krb_get_pw_in_tkt,
    (char *, char *, char *, char *, char *, int, char *)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    dest_tkt,
    (void)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    krb_get_lrealm,
    (char *, int)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    krb_check_serv,
    (char*)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    krb_get_cred,
    (char *, char *, char *, CREDENTIALS *)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    send_to_kdc,
    (KTEXT, KTEXT, char *)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    create_ciph,
    (KTEXT, C_Block, char *, char *, char *, unsigned long, int,
     KTEXT, unsigned long, C_Block *)
    );
TYPEDEF_FUNC(
    char *,
    CALLCONV_C,
    krb_get_krbconf2,
    (char *, size_t *)
    );
TYPEDEF_FUNC(
    char *,
    CALLCONV_C,
    krb_get_krbrealm2,
    (char *, size_t *)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    krb_in_tkt,
    (char *, char *, char *)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    krb_save_credentials,
    (char *, char *, char *, C_Block, int, int, KTEXT, long)
    );

/* NOT IN krb.h HEADER: */

TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    k_gethostname,
    (char FAR*, int)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    krb_get_krbhst,
    (char *, char *, int)
    );
TYPEDEF_FUNC(
    void,
    CALLCONV_C,
    krb_set_tkt_string,
    (char *)
    );

/* Evil incarnate... */

TYPEDEF_FUNC(
    long,
    CALLCONV_C,
    kadm_change_your_password,
    (LPSTR principal, LPSTR old_password,
     LPSTR new_password, HANDLE FAR * hInfo_desc)
    );
TYPEDEF_FUNC(
    void,
    CALLCONV_C,
    initialize_krb_error_func,
    (void* func, HANDLE *__et_list)
    );
TYPEDEF_FUNC(
    void,
    CALLCONV_C,
    initialize_kadm_error_table,
    (HANDLE *__et_list)
    );
TYPEDEF_FUNC(
    LONG,
    CALLCONV_C,
    lsh_LoadKrb4LeashErrorTables,
    (HMODULE hLeashDll, INT useCallBackFunction)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    krb_life_to_time,
    (int start, int life)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    krb_time_to_life,
    (int start, int end)
    );
#endif /* __LOADFUNCS_KRB_H__ */
