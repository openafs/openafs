#ifndef __LOADFUNCS_KRB524_H__
#define __LOADFUNCS_KRB524_H__

#include "loadfuncs.h"
#include <krb5.h>
#include <krb.h>

#define KRB524_DLL      "krb524.dll"

TYPEDEF_FUNC(
    int,
    KRB5_CALLCONV_C,
    krb524_init_ets,
    (krb5_context context)
    );
TYPEDEF_FUNC(
    int,
    KRB5_CALLCONV_C,
    krb524_convert_creds_kdc,
    (krb5_context context, krb5_creds *v5creds,
                                CREDENTIALS *v4creds)
    );
#endif /* __LOADFUNCS_KRB524_H__ */
