/*
* Copyright (c) 2004, 2005, 2006, 2007 Secure Endpoints Inc.
* Copyright (c) 2003 SkyRope, LLC
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 * 
 * - Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice, 
 *   this list of conditions and the following disclaimer in the documentation 
 *   and/or other materials provided with the distribution.
 * - Neither the name of Skyrope, LLC nor the names of its contributors may be 
 *   used to endorse or promote products derived from this software without 
 *   specific prior written permission from Skyrope, LLC.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this code are derived from portions of the MIT
 * Leash Ticket Manager and LoadFuncs utilities.  For these portions the
 * following copyright applies.
 *
 * Copyright (c) 2003,2004 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 */

#ifndef AFSKRB5_INT_H
#define AFSKRB5_INT_H

#include <windows.h>
#ifdef USE_MS2MIT
#define SECURITY_WIN32
#include <security.h>
#if _WIN32_WINNT < 0x0501
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#include <ntsecapi.h>
#endif /* USE_MS2MIT */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>

#ifdef USE_MS2MIT
#include <loadfuncs-lsa.h>
#endif /* USE_MS2MIT */

#include <afs/stds.h>
#include <krb5.h>

#include <rxkad.h>

/* Defined in the KRBV4W32 version of krb.h but not the Kerberos V version */
/* Required for some of the loadfuncs headers */
typedef struct ktext far *KTEXT;
typedef struct ktext far *KTEXT_FP;
#include <KerberosIV/krb.h>

/* AFS has its own version of com_err.h */
typedef afs_int32 errcode_t;

#include <loadfuncs-com_err.h>
#include <loadfuncs-krb5.h>
#include <loadfuncs-profile.h>
#include <loadfuncs-krb.h>
#include <loadfuncs-krb524.h>
#include <loadfuncs-leash.h>

// service definitions
#define SERVICE_DLL   "advapi32.dll"
typedef SC_HANDLE (WINAPI *FP_OpenSCManagerA)(char *, char *, DWORD);
typedef SC_HANDLE (WINAPI *FP_OpenServiceA)(SC_HANDLE, char *, DWORD);
typedef BOOL (WINAPI *FP_QueryServiceStatus)(SC_HANDLE, LPSERVICE_STATUS);
typedef BOOL (WINAPI *FP_CloseServiceHandle)(SC_HANDLE);

#define KRB5_DEFAULT_LIFE            60*60*10 /* 10 hours */
#define LSA_CCNAME                   "MSLSA:"

#ifndef KTC_ERROR
#define KTC_ERROR      11862784L
#define KTC_TOOBIG     11862785L
#define KTC_INVAL      11862786L
#define KTC_NOENT      11862787L
#define KTC_PIOCTLFAIL 11862788L
#define KTC_NOPIOCTL   11862789L
#define KTC_NOCELL     11862790L
#define KTC_NOCM       11862791L
#endif

/* User Query data structures and functions */

struct textField {
    char * buf;                       /* Destination buffer address */
    int    len;                       /* Destination buffer length */
    char * label;                     /* Label for this field */
    char * def;                       /* Default response for this field */
    int    echo;                      /* 0 = no, 1 = yes, 2 = asterisks */
};

#define ID_TEXT       150
#define ID_MID_TEXT   300

struct principal_ccache_data {
    struct principal_ccache_data * next;
    char * principal;
    char * ccache_name;
    int    from_lsa;
    int    expired;
    int    expiration_time;
    int    renew;
};

struct cell_principal_map {
    struct cell_principal_map * next;
    char * cell;
    char * principal;
    int    active;
};

/* In order to avoid including the private CCAPI headers */
typedef int cc_int32;

#define CC_API_VER_1 1
#define CC_API_VER_2 2

#define CCACHE_API cc_int32

/*
** The Official Error Codes
*/
#define CC_NOERROR           0
#define CC_BADNAME           1
#define CC_NOTFOUND          2
#define CC_END               3
#define CC_IO                4
#define CC_WRITE             5
#define CC_NOMEM             6
#define CC_FORMAT            7
#define CC_LOCKED            8
#define CC_BAD_API_VERSION   9
#define CC_NO_EXIST          10
#define CC_NOT_SUPP          11
#define CC_BAD_PARM          12
#define CC_ERR_CACHE_ATTACH  13
#define CC_ERR_CACHE_RELEASE 14
#define CC_ERR_CACHE_FULL    15
#define CC_ERR_CRED_VERSION  16

enum {
    CC_CRED_VUNKNOWN = 0,       // For validation
    CC_CRED_V4 = 1,
    CC_CRED_V5 = 2,
    CC_CRED_VMAX = 3            // For validation
};

typedef struct opaque_dll_control_block_type* apiCB;
typedef struct _infoNC {
    char*     name;
    char*     principal;
    cc_int32  vers;
} infoNC;

TYPEDEF_FUNC(
CCACHE_API,
CALLCONV_C,
cc_initialize,
    (
    apiCB** cc_ctx,           // <  DLL's primary control structure.
                              //    returned here, passed everywhere else
    cc_int32 api_version,     // >  ver supported by caller (use CC_API_VER_1)
    cc_int32*  api_supported, // <  if ~NULL, max ver supported by DLL
    const char** vendor       // <  if ~NULL, vendor name in read only C string
    )
);

TYPEDEF_FUNC(
CCACHE_API,
CALLCONV_C,
cc_shutdown,
    (
    apiCB** cc_ctx            // <> DLL's primary control structure. NULL after
    )
);

TYPEDEF_FUNC(
CCACHE_API,
CALLCONV_C,
cc_get_NC_info,
    (
    apiCB* cc_ctx,          // >  DLL's primary control structure
    struct _infoNC*** ppNCi // <  (NULL before call) null terminated,
                            //    list of a structs (free via cc_free_infoNC())
    )
);

TYPEDEF_FUNC(
CCACHE_API,
CALLCONV_C,
cc_free_NC_info,
    (
    apiCB* cc_ctx,
    struct _infoNC*** ppNCi // <  free list of structs returned by
                            //    cc_get_cache_names().  set to NULL on return
    )
);

#ifdef _WIN64
#define CCAPI_DLL   "krbcc64.dll"
#else
#define CCAPI_DLL   "krbcc32.dll"
#endif

/* Function Prototypes */
DWORD GetServiceStatus(LPSTR, LPSTR, DWORD *);
void KFW_AFS_error(LONG, LPCSTR);

void UnloadFuncs(FUNC_INFO [], HINSTANCE);
int  LoadFuncs(const char*, FUNC_INFO [], HINSTANCE*, int*, int, int, int);
int  KFW_get_ccache(krb5_context, krb5_principal, krb5_ccache *);
int  KFW_error(krb5_error_code, LPCSTR, int, krb5_context *, krb5_ccache *);
int  KFW_kinit(krb5_context, krb5_ccache, HWND, char *, char *, krb5_deltat,
                DWORD, DWORD, krb5_deltat, DWORD, DWORD);
int  KFW_renew(krb5_context, krb5_ccache);
int  KFW_destroy(krb5_context, krb5_ccache);
BOOL KFW_ms2mit(krb5_context, krb5_ccache, BOOL);
int  KFW_AFS_unlog(void);
int  KFW_AFS_klog(krb5_context, krb5_ccache, char*, char*, char*, int, char*);
void KFW_import_ccache_data(void);
BOOL MSLSA_IsKerberosLogon();
char *afs_realm_of_cell(krb5_context, struct afsconf_cell *);
#endif /* AFSKFW_INT_H */
