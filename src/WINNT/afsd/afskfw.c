/*
 * Copyright (c) 2004, 2005, 2006 Secure Endpoints Inc.
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


#define USE_MS2MIT
#undef  USE_KRB4
#include "afskfw-int.h"
#include "afskfw.h"
#include <userenv.h>
#include <Sddl.h>
#include <Aclapi.h>

#include <osilog.h>
#include <afs/ptserver.h>
#include <afs/ptuser.h>
#include <rx/rxkad.h>

#include <WINNT\afsreg.h>

/*
 * TIMING _____________________________________________________________________
 *
 */

#define cminREMIND_TEST      1    // test every minute for expired creds
#define cminREMIND_WARN      15   // warn if creds expire in 15 minutes
#define cminRENEW            20   // renew creds when there are 20 minutes remaining
#define cminMINLIFE          30   // minimum life of Kerberos creds

#define c100ns1SECOND        (LONGLONG)10000000
#define cmsec1SECOND         1000
#define cmsec1MINUTE         60000
#define csec1MINUTE          60

/* Function Pointer Declarations for Delayed Loading */
// CCAPI
DECL_FUNC_PTR(cc_initialize);
DECL_FUNC_PTR(cc_shutdown);
DECL_FUNC_PTR(cc_get_NC_info);
DECL_FUNC_PTR(cc_free_NC_info);

// leash functions
DECL_FUNC_PTR(Leash_get_default_lifetime);
DECL_FUNC_PTR(Leash_get_default_forwardable);
DECL_FUNC_PTR(Leash_get_default_renew_till);
DECL_FUNC_PTR(Leash_get_default_noaddresses);
DECL_FUNC_PTR(Leash_get_default_proxiable);
DECL_FUNC_PTR(Leash_get_default_publicip);
DECL_FUNC_PTR(Leash_get_default_use_krb4);
DECL_FUNC_PTR(Leash_get_default_life_min);
DECL_FUNC_PTR(Leash_get_default_life_max);
DECL_FUNC_PTR(Leash_get_default_renew_min);
DECL_FUNC_PTR(Leash_get_default_renew_max);
DECL_FUNC_PTR(Leash_get_default_renewable);
DECL_FUNC_PTR(Leash_get_default_mslsa_import);

// krb5 functions
DECL_FUNC_PTR(krb5_change_password);
DECL_FUNC_PTR(krb5_get_init_creds_opt_init);
DECL_FUNC_PTR(krb5_get_init_creds_opt_set_tkt_life);
DECL_FUNC_PTR(krb5_get_init_creds_opt_set_renew_life);
DECL_FUNC_PTR(krb5_get_init_creds_opt_set_forwardable);
DECL_FUNC_PTR(krb5_get_init_creds_opt_set_proxiable);
DECL_FUNC_PTR(krb5_get_init_creds_opt_set_address_list);
DECL_FUNC_PTR(krb5_get_init_creds_password);
DECL_FUNC_PTR(krb5_build_principal_ext);
DECL_FUNC_PTR(krb5_cc_get_name);
DECL_FUNC_PTR(krb5_cc_resolve);
DECL_FUNC_PTR(krb5_cc_default);
DECL_FUNC_PTR(krb5_cc_default_name);
DECL_FUNC_PTR(krb5_cc_set_default_name);
DECL_FUNC_PTR(krb5_cc_initialize);
DECL_FUNC_PTR(krb5_cc_destroy);
DECL_FUNC_PTR(krb5_cc_close);
DECL_FUNC_PTR(krb5_cc_store_cred);
DECL_FUNC_PTR(krb5_cc_copy_creds);
DECL_FUNC_PTR(krb5_cc_retrieve_cred);
DECL_FUNC_PTR(krb5_cc_get_principal);
DECL_FUNC_PTR(krb5_cc_start_seq_get);
DECL_FUNC_PTR(krb5_cc_next_cred);
DECL_FUNC_PTR(krb5_cc_end_seq_get);
DECL_FUNC_PTR(krb5_cc_remove_cred);
DECL_FUNC_PTR(krb5_cc_set_flags);
DECL_FUNC_PTR(krb5_cc_get_type);
DECL_FUNC_PTR(krb5_free_context);
DECL_FUNC_PTR(krb5_free_cred_contents);
DECL_FUNC_PTR(krb5_free_principal);
DECL_FUNC_PTR(krb5_get_in_tkt_with_password);
DECL_FUNC_PTR(krb5_init_context);
DECL_FUNC_PTR(krb5_parse_name);
DECL_FUNC_PTR(krb5_timeofday);
DECL_FUNC_PTR(krb5_timestamp_to_sfstring);
DECL_FUNC_PTR(krb5_unparse_name);
DECL_FUNC_PTR(krb5_get_credentials);
DECL_FUNC_PTR(krb5_mk_req);
DECL_FUNC_PTR(krb5_sname_to_principal);
DECL_FUNC_PTR(krb5_get_credentials_renew);
DECL_FUNC_PTR(krb5_free_data);
DECL_FUNC_PTR(krb5_free_data_contents);
DECL_FUNC_PTR(krb5_free_unparsed_name);
DECL_FUNC_PTR(krb5_os_localaddr);
DECL_FUNC_PTR(krb5_copy_keyblock_contents);
DECL_FUNC_PTR(krb5_copy_data);
DECL_FUNC_PTR(krb5_free_creds);
DECL_FUNC_PTR(krb5_build_principal);
DECL_FUNC_PTR(krb5_get_renewed_creds);
DECL_FUNC_PTR(krb5_get_default_config_files);
DECL_FUNC_PTR(krb5_free_config_files);
DECL_FUNC_PTR(krb5_get_default_realm);
DECL_FUNC_PTR(krb5_free_default_realm);
DECL_FUNC_PTR(krb5_free_ticket);
DECL_FUNC_PTR(krb5_decode_ticket);
DECL_FUNC_PTR(krb5_get_host_realm);
DECL_FUNC_PTR(krb5_free_host_realm);
DECL_FUNC_PTR(krb5_free_addresses);
DECL_FUNC_PTR(krb5_c_random_make_octets);

// Krb524 functions
DECL_FUNC_PTR(krb524_init_ets);
DECL_FUNC_PTR(krb524_convert_creds_kdc);

// krb4 functions
DECL_FUNC_PTR(krb_get_cred);
DECL_FUNC_PTR(tkt_string);
DECL_FUNC_PTR(krb_get_tf_realm);
DECL_FUNC_PTR(krb_mk_req);

// ComErr functions
DECL_FUNC_PTR(com_err);
DECL_FUNC_PTR(error_message);

// Profile functions
DECL_FUNC_PTR(profile_init);
DECL_FUNC_PTR(profile_release);
DECL_FUNC_PTR(profile_get_subsection_names);
DECL_FUNC_PTR(profile_free_list);
DECL_FUNC_PTR(profile_get_string);
DECL_FUNC_PTR(profile_release_string);

// Service functions
DECL_FUNC_PTR(OpenSCManagerA);
DECL_FUNC_PTR(OpenServiceA);
DECL_FUNC_PTR(QueryServiceStatus);
DECL_FUNC_PTR(CloseServiceHandle);
#ifdef USE_MS2MIT
DECL_FUNC_PTR(LsaNtStatusToWinError);
#endif /* USE_MS2MIT */

#ifdef USE_MS2MIT
// LSA Functions
DECL_FUNC_PTR(LsaConnectUntrusted);
DECL_FUNC_PTR(LsaLookupAuthenticationPackage);
DECL_FUNC_PTR(LsaCallAuthenticationPackage);
DECL_FUNC_PTR(LsaFreeReturnBuffer);
DECL_FUNC_PTR(LsaGetLogonSessionData);
#endif /* USE_MS2MIT */

// CCAPI
FUNC_INFO ccapi_fi[] = {
    MAKE_FUNC_INFO(cc_initialize),
    MAKE_FUNC_INFO(cc_shutdown),
    MAKE_FUNC_INFO(cc_get_NC_info),
    MAKE_FUNC_INFO(cc_free_NC_info),
    END_FUNC_INFO
};

FUNC_INFO leash_fi[] = {
    MAKE_FUNC_INFO(Leash_get_default_lifetime),
    MAKE_FUNC_INFO(Leash_get_default_renew_till),
    MAKE_FUNC_INFO(Leash_get_default_forwardable),
    MAKE_FUNC_INFO(Leash_get_default_noaddresses),
    MAKE_FUNC_INFO(Leash_get_default_proxiable),
    MAKE_FUNC_INFO(Leash_get_default_publicip),
    MAKE_FUNC_INFO(Leash_get_default_use_krb4),
    MAKE_FUNC_INFO(Leash_get_default_life_min),
    MAKE_FUNC_INFO(Leash_get_default_life_max),
    MAKE_FUNC_INFO(Leash_get_default_renew_min),
    MAKE_FUNC_INFO(Leash_get_default_renew_max),
    MAKE_FUNC_INFO(Leash_get_default_renewable),
    END_FUNC_INFO
};

FUNC_INFO leash_opt_fi[] = {
    MAKE_FUNC_INFO(Leash_get_default_mslsa_import),
    END_FUNC_INFO
};

FUNC_INFO k5_fi[] = {
    MAKE_FUNC_INFO(krb5_change_password),
    MAKE_FUNC_INFO(krb5_get_init_creds_opt_init),
    MAKE_FUNC_INFO(krb5_get_init_creds_opt_set_tkt_life),
    MAKE_FUNC_INFO(krb5_get_init_creds_opt_set_renew_life),
    MAKE_FUNC_INFO(krb5_get_init_creds_opt_set_forwardable),
    MAKE_FUNC_INFO(krb5_get_init_creds_opt_set_proxiable),
    MAKE_FUNC_INFO(krb5_get_init_creds_opt_set_address_list),
    MAKE_FUNC_INFO(krb5_get_init_creds_password),
    MAKE_FUNC_INFO(krb5_build_principal_ext),
    MAKE_FUNC_INFO(krb5_cc_get_name),
    MAKE_FUNC_INFO(krb5_cc_resolve),
    MAKE_FUNC_INFO(krb5_cc_default),
    MAKE_FUNC_INFO(krb5_cc_default_name),
    MAKE_FUNC_INFO(krb5_cc_set_default_name),
    MAKE_FUNC_INFO(krb5_cc_initialize),
    MAKE_FUNC_INFO(krb5_cc_destroy),
    MAKE_FUNC_INFO(krb5_cc_close),
    MAKE_FUNC_INFO(krb5_cc_copy_creds),
    MAKE_FUNC_INFO(krb5_cc_store_cred),
    MAKE_FUNC_INFO(krb5_cc_retrieve_cred),
    MAKE_FUNC_INFO(krb5_cc_get_principal),
    MAKE_FUNC_INFO(krb5_cc_start_seq_get),
    MAKE_FUNC_INFO(krb5_cc_next_cred),
    MAKE_FUNC_INFO(krb5_cc_end_seq_get),
    MAKE_FUNC_INFO(krb5_cc_remove_cred),
    MAKE_FUNC_INFO(krb5_cc_set_flags),
    MAKE_FUNC_INFO(krb5_cc_get_type),
    MAKE_FUNC_INFO(krb5_free_context),
    MAKE_FUNC_INFO(krb5_free_cred_contents),
    MAKE_FUNC_INFO(krb5_free_principal),
    MAKE_FUNC_INFO(krb5_get_in_tkt_with_password),
    MAKE_FUNC_INFO(krb5_init_context),
    MAKE_FUNC_INFO(krb5_parse_name),
    MAKE_FUNC_INFO(krb5_timeofday),
    MAKE_FUNC_INFO(krb5_timestamp_to_sfstring),
    MAKE_FUNC_INFO(krb5_unparse_name),
    MAKE_FUNC_INFO(krb5_get_credentials),
    MAKE_FUNC_INFO(krb5_mk_req),
    MAKE_FUNC_INFO(krb5_sname_to_principal),
    MAKE_FUNC_INFO(krb5_get_credentials_renew),
    MAKE_FUNC_INFO(krb5_free_data),
    MAKE_FUNC_INFO(krb5_free_data_contents),
    MAKE_FUNC_INFO(krb5_free_unparsed_name),
    MAKE_FUNC_INFO(krb5_os_localaddr),
    MAKE_FUNC_INFO(krb5_copy_keyblock_contents),
    MAKE_FUNC_INFO(krb5_copy_data),
    MAKE_FUNC_INFO(krb5_free_creds),
    MAKE_FUNC_INFO(krb5_build_principal),
    MAKE_FUNC_INFO(krb5_get_renewed_creds),
    MAKE_FUNC_INFO(krb5_free_addresses),
    MAKE_FUNC_INFO(krb5_get_default_config_files),
    MAKE_FUNC_INFO(krb5_free_config_files),
    MAKE_FUNC_INFO(krb5_get_default_realm),
    MAKE_FUNC_INFO(krb5_free_default_realm),
    MAKE_FUNC_INFO(krb5_free_ticket),
    MAKE_FUNC_INFO(krb5_decode_ticket),
    MAKE_FUNC_INFO(krb5_get_host_realm),
    MAKE_FUNC_INFO(krb5_free_host_realm),
    MAKE_FUNC_INFO(krb5_free_addresses),
    MAKE_FUNC_INFO(krb5_c_random_make_octets),
    END_FUNC_INFO
};

#ifdef USE_KRB4
FUNC_INFO k4_fi[] = {
    MAKE_FUNC_INFO(krb_get_cred),
    MAKE_FUNC_INFO(krb_get_tf_realm),
    MAKE_FUNC_INFO(krb_mk_req),
    MAKE_FUNC_INFO(tkt_string),
    END_FUNC_INFO
};
#endif

FUNC_INFO k524_fi[] = {
    MAKE_FUNC_INFO(krb524_init_ets),
    MAKE_FUNC_INFO(krb524_convert_creds_kdc),
    END_FUNC_INFO
};

FUNC_INFO profile_fi[] = {
        MAKE_FUNC_INFO(profile_init),
        MAKE_FUNC_INFO(profile_release),
        MAKE_FUNC_INFO(profile_get_subsection_names),
        MAKE_FUNC_INFO(profile_free_list),
        MAKE_FUNC_INFO(profile_get_string),
        MAKE_FUNC_INFO(profile_release_string),
        END_FUNC_INFO
};

FUNC_INFO ce_fi[] = {
    MAKE_FUNC_INFO(com_err),
    MAKE_FUNC_INFO(error_message),
    END_FUNC_INFO
};

FUNC_INFO service_fi[] = {
    MAKE_FUNC_INFO(OpenSCManagerA),
    MAKE_FUNC_INFO(OpenServiceA),
    MAKE_FUNC_INFO(QueryServiceStatus),
    MAKE_FUNC_INFO(CloseServiceHandle),
#ifdef USE_MS2MIT
    MAKE_FUNC_INFO(LsaNtStatusToWinError),
#endif /* USE_MS2MIT */
    END_FUNC_INFO
};

#ifdef USE_MS2MIT
FUNC_INFO lsa_fi[] = {
    MAKE_FUNC_INFO(LsaConnectUntrusted),
    MAKE_FUNC_INFO(LsaLookupAuthenticationPackage),
    MAKE_FUNC_INFO(LsaCallAuthenticationPackage),
    MAKE_FUNC_INFO(LsaFreeReturnBuffer),
    MAKE_FUNC_INFO(LsaGetLogonSessionData),
    END_FUNC_INFO
};
#endif /* USE_MS2MIT */

/* Static Prototypes */
char *afs_realm_of_cell(krb5_context, struct afsconf_cell *);
static long get_cellconfig_callback(void *, struct sockaddr_in *, char *);
int KFW_AFS_get_cellconfig(char *, struct afsconf_cell *, char *);
static krb5_error_code KRB5_CALLCONV KRB5_prompter( krb5_context context,
           void *data, const char *name, const char *banner, int num_prompts,
           krb5_prompt prompts[]);


/* Static Declarations */
static int                inited = 0;
static int                mid_cnt = 0;
static struct textField * mid_tb = NULL;
static HINSTANCE hKrb5 = 0;
#ifdef USE_KRB4
static HINSTANCE hKrb4 = 0;
#endif /* USE_KRB4 */
static HINSTANCE hKrb524 = 0;
#ifdef USE_MS2MIT
static HINSTANCE hSecur32 = 0;
#endif /* USE_MS2MIT */
static HINSTANCE hAdvApi32 = 0;
static HINSTANCE hComErr = 0;
static HINSTANCE hService = 0;
static HINSTANCE hProfile = 0;
static HINSTANCE hLeash = 0;
static HINSTANCE hLeashOpt = 0;
static HINSTANCE hCCAPI = 0;
static struct principal_ccache_data * princ_cc_data = NULL;
static struct cell_principal_map    * cell_princ_map = NULL;

void
KFW_initialize(void)
{
    static int inited = 0;

    if ( !inited ) {
        char mutexName[MAX_PATH];
        HANDLE hMutex = NULL;

        sprintf(mutexName, "AFS KFW Init pid=%d", getpid());
        
        hMutex = CreateMutex( NULL, TRUE, mutexName );
        if ( GetLastError() == ERROR_ALREADY_EXISTS ) {
            if ( WaitForSingleObject( hMutex, INFINITE ) != WAIT_OBJECT_0 ) {
                return;
            }
        }
        if ( !inited ) {
            inited = 1;
            LoadFuncs(KRB5_DLL, k5_fi, &hKrb5, 0, 1, 0, 0);
#ifdef USE_KRB4
            LoadFuncs(KRB4_DLL, k4_fi, &hKrb4, 0, 1, 0, 0);
#endif /* USE_KRB4 */
            LoadFuncs(COMERR_DLL, ce_fi, &hComErr, 0, 0, 1, 0);
            LoadFuncs(SERVICE_DLL, service_fi, &hService, 0, 1, 0, 0);
#ifdef USE_MS2MIT
            LoadFuncs(SECUR32_DLL, lsa_fi, &hSecur32, 0, 1, 1, 1);
#endif /* USE_MS2MIT */
            LoadFuncs(KRB524_DLL, k524_fi, &hKrb524, 0, 1, 1, 1);
            LoadFuncs(PROFILE_DLL, profile_fi, &hProfile, 0, 1, 0, 0);
            LoadFuncs(LEASH_DLL, leash_fi, &hLeash, 0, 1, 0, 0);
            LoadFuncs(CCAPI_DLL, ccapi_fi, &hCCAPI, 0, 1, 0, 0);
            LoadFuncs(LEASH_DLL, leash_opt_fi, &hLeashOpt, 0, 1, 0, 0);

            if ( KFW_is_available() ) {
                char rootcell[MAXCELLCHARS+1];
#ifdef USE_MS2MIT
                KFW_import_windows_lsa();
#endif /* USE_MS2MIT */
                KFW_import_ccache_data();
                KFW_AFS_renew_expiring_tokens();

                /* WIN32 NOTE: no way to get max chars */
                if (!cm_GetRootCellName(rootcell))
                    KFW_AFS_renew_token_for_cell(rootcell);
            }
        }
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }
}

void
KFW_cleanup(void)
{
    if (hLeashOpt)
        FreeLibrary(hLeashOpt);
    if (hCCAPI)
        FreeLibrary(hCCAPI);
    if (hLeash)
        FreeLibrary(hLeash);
    if (hKrb524)
        FreeLibrary(hKrb524);
#ifdef USE_MS2MIT
    if (hSecur32)
        FreeLibrary(hSecur32);
#endif /* USE_MS2MIT */
    if (hService)
        FreeLibrary(hService);
    if (hComErr)
        FreeLibrary(hComErr);
    if (hProfile)
        FreeLibrary(hProfile);
#ifdef USE_KRB4
    if (hKrb4)
        FreeLibrary(hKrb4);
#endif /* USE_KRB4 */
    if (hKrb5)
        FreeLibrary(hKrb5);
}

int
KFW_use_krb524(void)
{
    HKEY parmKey;
    DWORD code, len;
    DWORD use524 = 0;

    code = RegOpenKeyEx(HKEY_CURRENT_USER, AFSREG_USER_OPENAFS_SUBKEY,
                         0, KEY_QUERY_VALUE, &parmKey);
    if (code == ERROR_SUCCESS) {
        len = sizeof(use524);
        code = RegQueryValueEx(parmKey, "Use524", NULL, NULL,
                                (BYTE *) &use524, &len);
        RegCloseKey(parmKey);
    }
    if (code != ERROR_SUCCESS) {
        code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_OPENAFS_SUBKEY,
                             0, KEY_QUERY_VALUE, &parmKey);
        if (code == ERROR_SUCCESS) {
            len = sizeof(use524);
            code = RegQueryValueEx(parmKey, "Use524", NULL, NULL,
                                    (BYTE *) &use524, &len);
            RegCloseKey (parmKey);
        }
    }
    return use524;
}

int 
KFW_is_available(void)
{
    HKEY parmKey;
    DWORD code, len;
    DWORD enableKFW = 1;

    code = RegOpenKeyEx(HKEY_CURRENT_USER, AFSREG_USER_OPENAFS_SUBKEY,
                         0, KEY_QUERY_VALUE, &parmKey);
    if (code == ERROR_SUCCESS) {
        len = sizeof(enableKFW);
        code = RegQueryValueEx(parmKey, "EnableKFW", NULL, NULL,
                                (BYTE *) &enableKFW, &len);
        RegCloseKey (parmKey);
    }
    
    if (code != ERROR_SUCCESS) {
        code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_OPENAFS_SUBKEY,
                             0, KEY_QUERY_VALUE, &parmKey);
        if (code == ERROR_SUCCESS) {
            len = sizeof(enableKFW);
            code = RegQueryValueEx(parmKey, "EnableKFW", NULL, NULL,
                                    (BYTE *) &enableKFW, &len);
            RegCloseKey (parmKey);
        }
    } 

    if ( !enableKFW )
        return FALSE;

    KFW_initialize();
    if ( hKrb5 && hComErr && hService && 
#ifdef USE_MS2MIT
         hSecur32 && 
#endif /* USE_MS2MIT */
         hKrb524 &&
         hProfile && hLeash && hCCAPI )
        return TRUE;
    return FALSE;
}

int 
KRB5_error(krb5_error_code rc, LPCSTR FailedFunctionName, 
                 int FreeContextFlag, krb5_context * ctx, 
                 krb5_ccache * cache)
{
    char message[256];
    const char *errText;
    int krb5Error = ((int)(rc & 255));  
    
    /*
    switch (krb5Error)
    {
        // Wrong password
        case 31:
        case 8:
            return;
    }
    */
        
    errText = perror_message(rc);   
    _snprintf(message, sizeof(message), 
              "%s\n(Kerberos error %ld)\n\n%s failed", 
              errText, 
              krb5Error, 
              FailedFunctionName);

    if ( IsDebuggerPresent() )
        OutputDebugString(message);

    MessageBox(NULL, message, "Kerberos Five", MB_OK | MB_ICONERROR | 
               MB_TASKMODAL | 
               MB_SETFOREGROUND);
    if (FreeContextFlag == 1)
    {
        if (ctx && *ctx != NULL)
        {
            if (cache && *cache != NULL) {
                pkrb5_cc_close(*ctx, *cache);
				*cache = NULL;
			}
	
            pkrb5_free_context(*ctx);
			*ctx = NULL;
        }
    }

    return rc;
}

void
KFW_AFS_update_princ_ccache_data(krb5_context ctx, krb5_ccache cc, int lsa)
{
    struct principal_ccache_data * next = princ_cc_data;
    krb5_principal principal = 0;
    char * pname = NULL;
    const char * ccname = NULL;
    krb5_error_code code = 0;
    krb5_error_code cc_code = 0;
    krb5_cc_cursor cur;
    krb5_creds creds;
    krb5_flags flags=0;
    krb5_timestamp now;

    if (ctx == 0 || cc == 0)
        return;

    code = pkrb5_cc_get_principal(ctx, cc, &principal);
    if ( code ) return;

    code = pkrb5_unparse_name(ctx, principal, &pname);
    if ( code ) goto cleanup;

    ccname = pkrb5_cc_get_name(ctx, cc);
    if (!ccname) goto cleanup;

    // Search the existing list to see if we have a match 
    if ( next ) {
        for ( ; next ; next = next->next ) {
            if ( !strcmp(next->principal,pname) && !strcmp(next->ccache_name, ccname) )
                break;
        }
    } 

    // If not, match add a new node to the beginning of the list and assign init it
    if ( !next ) {
        next = (struct principal_ccache_data *) malloc(sizeof(struct principal_ccache_data));
        next->next = princ_cc_data;
        princ_cc_data = next;
        next->principal = _strdup(pname);
        next->ccache_name = _strdup(ccname);
        next->from_lsa = lsa;
        next->expired = 1;
        next->expiration_time = 0;
        next->renew = 0;
    }

    flags = 0;  // turn off OPENCLOSE mode
    code = pkrb5_cc_set_flags(ctx, cc, flags);
    if ( code ) goto cleanup;

    code = pkrb5_timeofday(ctx, &now);

    cc_code = pkrb5_cc_start_seq_get(ctx, cc, &cur);

    while (!(cc_code = pkrb5_cc_next_cred(ctx, cc, &cur, &creds))) {
        if ( creds.ticket_flags & TKT_FLG_INITIAL ) {
            int valid;
            // we found the ticket we are looking for
            // check validity of timestamp 
            // We add a 5 minutes fudge factor to compensate for potential
            // clock skew errors between the KDC and client OS

            valid = ((creds.times.starttime > 0) &&
                     now >= (creds.times.starttime - 300) &&
                     now < (creds.times.endtime + 300) &&
                     !(creds.ticket_flags & TKT_FLG_INVALID));

            if ( next->from_lsa) {
                next->expired = 0;
                next->expiration_time = creds.times.endtime;
                next->renew = 1;
            } else if ( valid ) {
                next->expired = 0;
                next->expiration_time = creds.times.endtime;
                next->renew = (creds.times.renew_till > creds.times.endtime) && 
                               (creds.ticket_flags & TKT_FLG_RENEWABLE);
            } else {
                next->expired = 1;
                next->expiration_time = 0;
                next->renew = 0;
            }

            pkrb5_free_cred_contents(ctx, &creds);
            cc_code = KRB5_CC_END;
            break;
        }
        pkrb5_free_cred_contents(ctx, &creds);
    }

    if (cc_code == KRB5_CC_END) {
        code = pkrb5_cc_end_seq_get(ctx, cc, &cur);
        if (code) goto cleanup;
    }

  cleanup:
    flags = KRB5_TC_OPENCLOSE;  //turn on OPENCLOSE
    code = pkrb5_cc_set_flags(ctx, cc, flags);

    if ( pname )
        pkrb5_free_unparsed_name(ctx,pname);
    if ( principal )
        pkrb5_free_principal(ctx,principal);
}   

int
KFW_AFS_find_ccache_for_principal(krb5_context ctx, char * principal, char **ccache, int valid_only)
{
    struct principal_ccache_data * next = princ_cc_data;
    char * response = NULL;

    if ( !principal || !ccache )
        return 0;

    while ( next ) {
        if ( (!valid_only || !next->expired) && !strcmp(next->principal,principal) ) {
            if (response) {
                // we always want to prefer the MS Kerberos LSA cache or
                // the cache afscreds created specifically for the principal
                // if the current entry is either one, drop the previous find
                if ( next->from_lsa || !strcmp(next->ccache_name,principal) )
                    free(response);
            }
            response = _strdup(next->ccache_name);
            // MS Kerberos LSA is our best option so use it and quit
            if ( next->from_lsa )   
                break;
        }
        next = next->next;
    }

    if ( response ) {
        *ccache = response;
        return 1;
    }
    return 0;
}

void 
KFW_AFS_delete_princ_ccache_data(krb5_context ctx, char * pname, char * ccname)
{
    struct principal_ccache_data ** next = &princ_cc_data;

    if ( !pname && !ccname )
        return;

    while ( (*next) ) {
        if ( !strcmp((*next)->principal,pname) || 
             !strcmp((*next)->ccache_name,ccname) ) {
            void * temp;
            free((*next)->principal);
            free((*next)->ccache_name);
            temp = (*next);
            (*next) = (*next)->next;
            free(temp);
        }
    }
}

void 
KFW_AFS_update_cell_princ_map(krb5_context ctx, char * cell, char *pname, int active)
{
    struct cell_principal_map * next = cell_princ_map;

    // Search the existing list to see if we have a match 
    if ( next ) {
        for ( ; next ; next = next->next ) {
            if ( !strcmp(next->cell, cell) ) {
                if ( !strcmp(next->principal,pname) ) {
                    next->active = active;
					break;
                } else {
                    // OpenAFS currently has a restriction of one active token per cell
                    // Therefore, whenever we update the table with a new active cell we
                    // must mark all of the other principal to cell entries as inactive.
                    if (active)
                        next->active = 0;
                }
            }
        }
    } 

    // If not, match add a new node to the beginning of the list and assign init it
    if ( !next ) {
        next = (struct cell_principal_map *) malloc(sizeof(struct cell_principal_map));
        next->next = cell_princ_map;
        cell_princ_map = next;
        next->principal = _strdup(pname);
        next->cell = _strdup(cell);
        next->active = active;
    }
}

void 
KFW_AFS_delete_cell_princ_maps(krb5_context ctx, char * pname, char * cell)
{
    struct cell_principal_map ** next = &cell_princ_map;

    if ( !pname && !cell )
        return;

    while ( (*next) ) {
        if ( !strcmp((*next)->principal,pname) || 
             !strcmp((*next)->cell,cell) ) {
            void * temp;
            free((*next)->principal);
            free((*next)->cell);
            temp = (*next);
            (*next) = (*next)->next;
            free(temp);
        }
    }
}

// Returns (if possible) a principal which has been known in 
// the past to have been used to obtain tokens for the specified
// cell.  
// TODO: Attempt to return one which has not yet expired by checking
// the principal/ccache data
int
KFW_AFS_find_principals_for_cell(krb5_context ctx, char * cell, char **principals[], int active_only)
{
    struct cell_principal_map * next_map = cell_princ_map;
    const char * princ = NULL;
    int count = 0, i;

    if ( !cell )
        return 0;

    while ( next_map ) {
        if ( (!active_only || next_map->active) && !strcmp(next_map->cell,cell) ) {
            count++;
        }
        next_map = next_map->next;
    }

    if ( !principals || !count )
        return count;

    *principals = (char **) malloc(sizeof(char *) * count);
    for ( next_map = cell_princ_map, i=0 ; next_map && i<count; next_map = next_map->next )
    {
        if ( (!active_only || next_map->active) && !strcmp(next_map->cell,cell) ) {
            (*principals)[i++] = _strdup(next_map->principal);
        }
    }
    return count;
}

int
KFW_AFS_find_cells_for_princ(krb5_context ctx, char * pname, char **cells[], int active_only)
{
    int     count = 0, i;
    struct cell_principal_map * next_map = cell_princ_map;
    const char * princ = NULL;
    
    if ( !pname )
        return 0;

    while ( next_map ) {
        if ( (!active_only || next_map->active) && !strcmp(next_map->principal,pname) ) {
            count++;
        }
        next_map = next_map->next;
    }

    if ( !cells )
        return count;

    *cells = (char **) malloc(sizeof(char *) * count);
    for ( next_map = cell_princ_map, i=0 ; next_map && i<count; next_map = next_map->next )
    {
        if ( (!active_only || next_map->active) && !strcmp(next_map->principal,pname) ) {
            (*cells)[i++] = _strdup(next_map->cell);
        }
    }
    return count;
}

/* Given a principal return an existing ccache or create one and return */
int
KFW_get_ccache(krb5_context alt_ctx, krb5_principal principal, krb5_ccache * cc)
{
    krb5_context ctx;
    char * pname = 0;
    char * ccname = 0;
    krb5_error_code code;

    if (!pkrb5_init_context)
        return 0;

    if ( alt_ctx ) {
        ctx = alt_ctx;
    } else {
        code = pkrb5_init_context(&ctx);
        if (code) goto cleanup;
    }

    if ( principal ) {
        code = pkrb5_unparse_name(ctx, principal, &pname);
        if (code) goto cleanup;

        if ( !KFW_AFS_find_ccache_for_principal(ctx,pname,&ccname,TRUE) &&
             !KFW_AFS_find_ccache_for_principal(ctx,pname,&ccname,FALSE)) {
            ccname = (char *)malloc(strlen(pname) + 5);
            sprintf(ccname,"API:%s",pname);
        }
        code = pkrb5_cc_resolve(ctx, ccname, cc);
    } else {
        code = pkrb5_cc_default(ctx, cc);
        if (code) goto cleanup;
    }

  cleanup:
    if (ccname)
        free(ccname);
    if (pname)
        pkrb5_free_unparsed_name(ctx,pname);
    if (ctx && (ctx != alt_ctx))
        pkrb5_free_context(ctx);
    return(code);
}

#ifdef USE_MS2MIT
// Import Microsoft Credentials into a new MIT ccache
void
KFW_import_windows_lsa(void)
{
    krb5_context ctx = 0;
    krb5_ccache  cc = 0;
    krb5_principal princ = 0;
    char * pname = NULL;
    krb5_data *  princ_realm;
    krb5_error_code code;
    char cell[128]="", realm[128]="", *def_realm = 0;
    int i;
    DWORD dwMsLsaImport;
         
    if (!pkrb5_init_context)
        return;

#ifdef COMMENT
    if ( !MSLSA_IsKerberosLogon() )
        return;
#endif

    code = pkrb5_init_context(&ctx);
    if (code) goto cleanup;

    code = pkrb5_cc_resolve(ctx, LSA_CCNAME, &cc);
    if (code) goto cleanup;

    KFW_AFS_update_princ_ccache_data(ctx, cc, TRUE);

    code = pkrb5_cc_get_principal(ctx, cc, &princ);
    if ( code ) goto cleanup;

    dwMsLsaImport = pLeash_get_default_mslsa_import ? pLeash_get_default_mslsa_import() : 1;
    switch ( dwMsLsaImport ) {
    case 0: /* do not import */
        goto cleanup;
    case 1: /* always import */
        break;
    case 2: { /* matching realm */
        char ms_realm[128] = "", *r;
        int i;

        for ( r=ms_realm, i=0; i<krb5_princ_realm(ctx, princ)->length; r++, i++ ) {
            *r = krb5_princ_realm(ctx, princ)->data[i];
        }
        *r = '\0';

        if (code = pkrb5_get_default_realm(ctx, &def_realm))
            goto cleanup;

        if (strcmp(def_realm, ms_realm))
            goto cleanup;
        break;
    }
    default:
        break;
    }

    code = pkrb5_unparse_name(ctx,princ,&pname);
    if ( code ) goto cleanup;

    princ_realm = krb5_princ_realm(ctx, princ);
    for ( i=0; i<princ_realm->length; i++ ) {
        realm[i] = princ_realm->data[i];
        cell[i] = tolower(princ_realm->data[i]);
    }
    cell[i] = '\0';
    realm[i] = '\0';

    code = KFW_AFS_klog(ctx, cc, "afs", cell, realm, pLeash_get_default_lifetime(),NULL);
    if ( IsDebuggerPresent() ) {
        char message[256];
        sprintf(message,"KFW_AFS_klog() returns: %d\n",code);
        OutputDebugString(message);
    }
    if ( code ) goto cleanup;

    KFW_AFS_update_cell_princ_map(ctx, cell, pname, TRUE);

  cleanup:
    if (pname)
        pkrb5_free_unparsed_name(ctx,pname);
    if (princ)
        pkrb5_free_principal(ctx,princ);
    if (def_realm)
        pkrb5_free_default_realm(ctx, def_realm);
    if (cc)
        pkrb5_cc_close(ctx,cc);
    if (ctx)
        pkrb5_free_context(ctx);
}
#endif /* USE_MS2MIT */

// If there are existing MIT credentials, copy them to a new
// ccache named after the principal

// Enumerate all existing MIT ccaches and construct entries
// in the principal_ccache table

// Enumerate all existing AFS Tokens and construct entries
// in the cell_principal table
void
KFW_import_ccache_data(void)
{
    krb5_context ctx = 0;
    krb5_ccache  cc = 0;
    krb5_principal principal = 0;
    krb5_creds creds;
    krb5_error_code code;
    krb5_error_code cc_code;
    krb5_cc_cursor cur;
    apiCB * cc_ctx = 0;
    struct _infoNC ** pNCi = NULL;
    int i, j, flags;

    if ( !pcc_initialize )
        return;

    if ( IsDebuggerPresent() )
        OutputDebugString("KFW_import_ccache_data()\n");

    code = pcc_initialize(&cc_ctx, CC_API_VER_2, NULL, NULL);
    if (code) goto cleanup;

    code = pcc_get_NC_info(cc_ctx, &pNCi);
    if (code) goto cleanup;

    code = pkrb5_init_context(&ctx);
    if (code) goto cleanup;

    for ( i=0; pNCi[i]; i++ ) {
        if ( pNCi[i]->vers != CC_CRED_V5 )
            continue;
        if ( IsDebuggerPresent() ) {
            OutputDebugString("Principal: ");
            OutputDebugString(pNCi[i]->principal);
            OutputDebugString(" in ccache ");
            OutputDebugString(pNCi[i]->name);
            OutputDebugString("\n");
        }
        if ( strcmp(pNCi[i]->name,pNCi[i]->principal)
             && strcmp(pNCi[i]->name,LSA_CCNAME) 
             ) {
            int found = 0;
            for ( j=0; pNCi[j]; j++ ) {
                if (!strcmp(pNCi[j]->name,pNCi[i]->principal)) {
                    found = 1;
                    break;
                }
            }
            
            code = pkrb5_cc_resolve(ctx, pNCi[i]->principal, &cc);
            if (code) goto loop_cleanup;

            if (!found) {
                krb5_ccache oldcc = 0;

                if ( IsDebuggerPresent() )
                    OutputDebugString("copying ccache data to new ccache\n");

                code = pkrb5_parse_name(ctx, pNCi[i]->principal, &principal);
                if (code) goto loop_cleanup;
                code = pkrb5_cc_initialize(ctx, cc, principal);
                if (code) goto loop_cleanup;

                code = pkrb5_cc_resolve(ctx, pNCi[i]->name, &oldcc);
                if (code) goto loop_cleanup;
                code = pkrb5_cc_copy_creds(ctx,oldcc,cc);
                if (code) {
                    code = pkrb5_cc_close(ctx,cc);
                    cc = 0;
                    code = pkrb5_cc_close(ctx,oldcc);
                    oldcc = 0;
                    KRB5_error(code, "krb5_cc_copy_creds", 0, NULL, NULL);
                    continue;
                }
                code = pkrb5_cc_close(ctx,oldcc);
            }
        } else {
            code = pkrb5_cc_resolve(ctx, pNCi[i]->name, &cc);
            if (code) goto loop_cleanup;
        }

        flags = 0;  // turn off OPENCLOSE mode
        code = pkrb5_cc_set_flags(ctx, cc, flags);
        if ( code ) goto cleanup;

        KFW_AFS_update_princ_ccache_data(ctx, cc, !strcmp(pNCi[i]->name,LSA_CCNAME));

        cc_code = pkrb5_cc_start_seq_get(ctx, cc, &cur);

        while (!(cc_code = pkrb5_cc_next_cred(ctx, cc, &cur, &creds))) {
            krb5_data * sname = krb5_princ_name(ctx, creds.server);
            krb5_data * cell  = krb5_princ_component(ctx, creds.server, 1);
            krb5_data * realm = krb5_princ_realm(ctx, creds.server);
            if ( sname && cell && !strcmp("afs",sname->data) ) {
                struct ktc_principal    aserver;
                struct ktc_principal    aclient;
                struct ktc_token	atoken;
                int active = TRUE;

                if ( IsDebuggerPresent() )  {
                    OutputDebugString("Found AFS ticket: ");
                    OutputDebugString(sname->data);
                    if ( cell->data ) {
                        OutputDebugString("/");
                        OutputDebugString(cell->data);
                    }
                    OutputDebugString("@");
                    OutputDebugString(realm->data);
                    OutputDebugString("\n");
                }

                memset(&aserver, '\0', sizeof(aserver));
                strcpy(aserver.name, sname->data);
                strcpy(aserver.cell, cell->data);

                code = ktc_GetToken(&aserver, &atoken, sizeof(atoken), &aclient);
                if (!code) {
                    // Found a token in AFS Client Server which matches
                    char pname[128], *p, *q;
                    for ( p=pname, q=aclient.name; *q; p++, q++)
                        *p = *q;
                    for ( *p++ = '@', q=aclient.cell; *q; p++, q++)
                        *p = toupper(*q);
                    *p = '\0';

                    if ( IsDebuggerPresent() )  {
                        OutputDebugString("Found AFS token: ");
                        OutputDebugString(pname);
                        OutputDebugString("\n");
                    }

                    if ( strcmp(pname,pNCi[i]->principal)  )
                        active = FALSE;
                    KFW_AFS_update_cell_princ_map(ctx, cell->data, pNCi[i]->principal, active);
                } else {
                    // Attempt to import it
                    KFW_AFS_update_cell_princ_map(ctx, cell->data, pNCi[i]->principal, active);

                    if ( IsDebuggerPresent() )  {
                        OutputDebugString("Calling KFW_AFS_klog() to obtain token\n");
                    }

                    code = KFW_AFS_klog(ctx, cc, "afs", cell->data, realm->data, pLeash_get_default_lifetime(),NULL);
                    if ( IsDebuggerPresent() ) {
                        char message[256];
                        sprintf(message,"KFW_AFS_klog() returns: %d\n",code);
                        OutputDebugString(message);
                    }
                }
            } else if ( IsDebuggerPresent() ) {
                OutputDebugString("Found ticket: ");
                OutputDebugString(sname->data);
                if ( cell && cell->data ) {
                    OutputDebugString("/");
                    OutputDebugString(cell->data);
                }
                OutputDebugString("@");
                OutputDebugString(realm->data);
                OutputDebugString("\n");
            }
            pkrb5_free_cred_contents(ctx, &creds);
        }

        if (cc_code == KRB5_CC_END) {
            cc_code = pkrb5_cc_end_seq_get(ctx, cc, &cur);
            if (cc_code) goto loop_cleanup;
        }

      loop_cleanup:
        flags = KRB5_TC_OPENCLOSE;  //turn on OPENCLOSE
        code = pkrb5_cc_set_flags(ctx, cc, flags);
        if (cc) {
            pkrb5_cc_close(ctx,cc);
            cc = 0;
        }
        if (principal) {
            pkrb5_free_principal(ctx,principal);
            principal = 0;
        }
    }

  cleanup:
    if (ctx)
        pkrb5_free_context(ctx);
    if (pNCi)
        pcc_free_NC_info(cc_ctx, &pNCi);
    if (cc_ctx)
        pcc_shutdown(&cc_ctx);
}


int
KFW_AFS_get_cred( char * username, 
                  char * cell,
                  char * password,
                  int lifetime,
                  char * smbname,
                  char ** reasonP )
{
    krb5_context ctx = 0;
    krb5_ccache cc = 0;
    char * realm = 0, * userrealm = 0;
    krb5_principal principal = 0;
    char * pname = 0;
    krb5_error_code code;
    char local_cell[MAXCELLCHARS+1];
    char **cells = NULL;
    int  cell_count=0;
    struct afsconf_cell cellconfig;
    char * dot;


    if (!pkrb5_init_context)
        return 0;

    if ( IsDebuggerPresent() ) {
        OutputDebugString("KFW_AFS_get_cred for token ");
        OutputDebugString(username);
        OutputDebugString(" in cell ");
        OutputDebugString(cell);
        OutputDebugString("\n");
    }

    code = pkrb5_init_context(&ctx);
    if ( code ) goto cleanup;

    code = KFW_AFS_get_cellconfig( cell, (void*)&cellconfig, local_cell);
    if ( code ) goto cleanup;

    realm = afs_realm_of_cell(ctx, &cellconfig);  // do not free

    userrealm = strchr(username,'@');
    if ( userrealm ) {
        pname = strdup(username);
        userrealm = strchr(pname, '@');
        *userrealm = '\0';

        /* handle kerberos iv notation */
        while ( dot = strchr(pname,'.') ) {
            *dot = '/';
        }
        *userrealm++ = '@';
    } else {
        pname = malloc(strlen(username) + strlen(realm) + 2);

        strcpy(pname, username);

        /* handle kerberos iv notation */
        while ( dot = strchr(pname,'.') ) {
            *dot = '/';
        }

        strcat(pname,"@");
        strcat(pname,realm);
    }
    if ( IsDebuggerPresent() ) {
        OutputDebugString("Realm: ");
        OutputDebugString(realm);
        OutputDebugString("\n");
    }

    code = pkrb5_parse_name(ctx, pname, &principal);
    if ( code ) goto cleanup;

    code = KFW_get_ccache(ctx, principal, &cc);
    if ( code ) goto cleanup;

    if ( lifetime == 0 )
        lifetime = pLeash_get_default_lifetime();

    if ( password && password[0] ) {
        code = KFW_kinit( ctx, cc, HWND_DESKTOP, 
                          pname, 
                          password,
                          lifetime,
                          pLeash_get_default_forwardable(),
                          pLeash_get_default_proxiable(),
                          pLeash_get_default_renewable() ? pLeash_get_default_renew_till() : 0,
                          pLeash_get_default_noaddresses(),
                          pLeash_get_default_publicip());
        if ( IsDebuggerPresent() ) {
            char message[256];
            sprintf(message,"KFW_kinit() returns: %d\n",code);
            OutputDebugString(message);
        }
        if ( code ) goto cleanup;

        KFW_AFS_update_princ_ccache_data(ctx, cc, FALSE);
    }

    code = KFW_AFS_klog(ctx, cc, "afs", cell, realm, lifetime, smbname);
    if ( IsDebuggerPresent() ) {
        char message[256];
        sprintf(message,"KFW_AFS_klog() returns: %d\n",code);
        OutputDebugString(message);
    }
    if ( code ) goto cleanup;

    KFW_AFS_update_cell_princ_map(ctx, cell, pname, TRUE);

    // Attempt to obtain new tokens for other cells supported by the same 
    // principal
    cell_count = KFW_AFS_find_cells_for_princ(ctx, pname, &cells, TRUE);
    if ( cell_count > 1 ) {
        while ( cell_count-- ) {
            if ( strcmp(cells[cell_count],cell) ) {
                if ( IsDebuggerPresent() ) {
                    char message[256];
                    sprintf(message,"found another cell for the same principal: %s\n",cell);
                    OutputDebugString(message);
                }
                code = KFW_AFS_get_cellconfig( cells[cell_count], (void*)&cellconfig, local_cell);
                if ( code ) continue;
    
                realm = afs_realm_of_cell(ctx, &cellconfig);  // do not free
                if ( IsDebuggerPresent() ) {
                    OutputDebugString("Realm: ");
                    OutputDebugString(realm);
                    OutputDebugString("\n");
                }
                
                code = KFW_AFS_klog(ctx, cc, "afs", cells[cell_count], realm, lifetime, smbname);
                if ( IsDebuggerPresent() ) {
                    char message[256];
                    sprintf(message,"KFW_AFS_klog() returns: %d\n",code);
                    OutputDebugString(message);
                }
            }
            free(cells[cell_count]);
        }
        free(cells);
    } else if ( cell_count == 1 ) {
        free(cells[0]);
        free(cells);
    }

  cleanup:
    if ( pname )
        free(pname);
    if ( cc )
        pkrb5_cc_close(ctx, cc);

    if ( code && reasonP ) {
        *reasonP = (char *)perror_message(code);
    }
    return(code);
}

int 
KFW_AFS_destroy_tickets_for_cell(char * cell)
{
    krb5_context		ctx = 0;
    krb5_error_code		code;
    int count;
    char ** principals = NULL;

    if (!pkrb5_init_context)
        return 0;

    if ( IsDebuggerPresent() ) {
        OutputDebugString("KFW_AFS_destroy_tickets_for_cell: ");
        OutputDebugString(cell);
        OutputDebugString("\n");
    }

    code = pkrb5_init_context(&ctx);
    if (code) ctx = 0;

    count = KFW_AFS_find_principals_for_cell(ctx, cell, &principals, FALSE);
    if ( count > 0 ) {
        krb5_principal      princ = 0;
        krb5_ccache			cc  = 0;

        while ( count-- ) {
            int cell_count = KFW_AFS_find_cells_for_princ(ctx, principals[count], NULL, TRUE);
            if ( cell_count > 1 ) {
                // TODO - What we really should do here is verify whether or not any of the
                // other cells which use this principal to obtain its credentials actually
                // have valid tokens or not.  If they are currently using these credentials
                // we will skip them.  For the time being we assume that if there is an active
                // map in the table that they are actively being used.
                goto loop_cleanup;
            }

            code = pkrb5_parse_name(ctx, principals[count], &princ);
            if (code) goto loop_cleanup;

            code = KFW_get_ccache(ctx, princ, &cc);
            if (code) goto loop_cleanup;

            code = pkrb5_cc_destroy(ctx, cc);
            if (!code) cc = 0;

          loop_cleanup:
            if ( cc ) {
                pkrb5_cc_close(ctx, cc);
                cc = 0;
            }
            if ( princ ) {
                pkrb5_free_principal(ctx, princ);
                princ = 0;
            }

            KFW_AFS_update_cell_princ_map(ctx, cell, principals[count], FALSE);
            free(principals[count]);
        }
        free(principals);
    }
    pkrb5_free_context(ctx);
    return 0;
}

int 
KFW_AFS_destroy_tickets_for_principal(char * user)
{
    krb5_context		ctx = 0;
    krb5_error_code		code;
    int count;
    char ** cells = NULL;
    krb5_principal      princ = 0;
    krb5_ccache			cc  = 0;

    if (!pkrb5_init_context)
        return 0;

    if ( IsDebuggerPresent() ) {
        OutputDebugString("KFW_AFS_destroy_tickets_for_user: ");
        OutputDebugString(user);
        OutputDebugString("\n");
    }

    code = pkrb5_init_context(&ctx);
    if (code) ctx = 0;

    code = pkrb5_parse_name(ctx, user, &princ);
    if (code) goto loop_cleanup;

    code = KFW_get_ccache(ctx, princ, &cc);
    if (code) goto loop_cleanup;

    code = pkrb5_cc_destroy(ctx, cc);
    if (!code) cc = 0;

  loop_cleanup:
    if ( cc ) {
        pkrb5_cc_close(ctx, cc);
        cc = 0;
    }
    if ( princ ) {
        pkrb5_free_principal(ctx, princ);
        princ = 0;
    }

    count = KFW_AFS_find_cells_for_princ(ctx, user, &cells, TRUE);
    if ( count >= 1 ) {
        while ( count-- ) {
            KFW_AFS_update_cell_princ_map(ctx, cells[count], user, FALSE);
            free(cells[count]);
        }
        free(cells);
    }

    pkrb5_free_context(ctx);
    return 0;
}

int
KFW_AFS_renew_expiring_tokens(void)
{
    krb5_error_code		        code = 0;
    krb5_context		        ctx = 0;
    krb5_ccache			        cc = 0;
    krb5_timestamp now;
    struct principal_ccache_data * pcc_next = princ_cc_data;
    int cell_count;
    char ** cells=NULL;
    const char * realm = NULL;
    char local_cell[MAXCELLCHARS+1]="";
    struct afsconf_cell cellconfig;

    if (!pkrb5_init_context)
        return 0;

    if ( pcc_next == NULL ) // nothing to do
        return 0;

    if ( IsDebuggerPresent() ) {
        OutputDebugString("KFW_AFS_renew_expiring_tokens\n");
    }

    code = pkrb5_init_context(&ctx);
    if (code) goto cleanup;

    code = pkrb5_timeofday(ctx, &now);
    if (code) goto cleanup; 

    for ( ; pcc_next ; pcc_next = pcc_next->next ) {
        if ( pcc_next->expired ) 
            continue;

        if ( now >= (pcc_next->expiration_time) ) {
            if ( !pcc_next->from_lsa ) {
                pcc_next->expired = 1;
                continue;
            }
        }

        if ( pcc_next->renew && now >= (pcc_next->expiration_time - cminRENEW * csec1MINUTE) ) {
            code = pkrb5_cc_resolve(ctx, pcc_next->ccache_name, &cc);
            if ( code ) 
				goto loop_cleanup;
            code = KFW_renew(ctx,cc);
#ifdef USE_MS2MIT
            if ( code && pcc_next->from_lsa)
                goto loop_cleanup;
#endif /* USE_MS2MIT */


            KFW_AFS_update_princ_ccache_data(ctx, cc, pcc_next->from_lsa);
            if (code) goto loop_cleanup;

            // Attempt to obtain new tokens for other cells supported by the same 
            // principal
            cell_count = KFW_AFS_find_cells_for_princ(ctx, pcc_next->principal, &cells, TRUE);
            if ( cell_count > 0 ) {
                while ( cell_count-- ) {
                    if ( IsDebuggerPresent() ) {
                        OutputDebugString("Cell: ");
                        OutputDebugString(cells[cell_count]);
                        OutputDebugString("\n");
                    }
                    code = KFW_AFS_get_cellconfig( cells[cell_count], (void*)&cellconfig, local_cell);
                    if ( code ) continue;
                    realm = afs_realm_of_cell(ctx, &cellconfig);  // do not free
                    if ( IsDebuggerPresent() ) {
                        OutputDebugString("Realm: ");
                        OutputDebugString(realm);
                        OutputDebugString("\n");
                    }
                    code = KFW_AFS_klog(ctx, cc, "afs", cells[cell_count], (char *)realm, 0, NULL);
                    if ( IsDebuggerPresent() ) {
                        char message[256];
                        sprintf(message,"KFW_AFS_klog() returns: %d\n",code);
                        OutputDebugString(message);
                    }
                    free(cells[cell_count]);
                }
                free(cells);
            }
        }

      loop_cleanup:
        if ( cc ) {
            pkrb5_cc_close(ctx,cc);
            cc = 0;
        }
    }

  cleanup:
    if ( cc )
        pkrb5_cc_close(ctx,cc);
    if ( ctx )
        pkrb5_free_context(ctx);

    return 0;
}


BOOL
KFW_AFS_renew_token_for_cell(char * cell)
{
    krb5_error_code		        code = 0;
    krb5_context		        ctx = 0;
    int count;
    char ** principals = NULL;

    if (!pkrb5_init_context)
        return 0;

    if ( IsDebuggerPresent() ) {
        OutputDebugString("KFW_AFS_renew_token_for_cell:");
        OutputDebugString(cell);
        OutputDebugString("\n");
    }

    code = pkrb5_init_context(&ctx);
    if (code) goto cleanup;

    count = KFW_AFS_find_principals_for_cell(ctx, cell, &principals, TRUE);
    if ( count == 0 ) {
        // We know we must have a credential somewhere since we are
        // trying to renew a token

        KFW_import_ccache_data();
        count = KFW_AFS_find_principals_for_cell(ctx, cell, &principals, TRUE);
    }
    if ( count > 0 ) {
        krb5_principal      princ = 0;
        krb5_principal      service = 0;
#ifdef COMMENT
        krb5_creds          mcreds, creds;
#endif /* COMMENT */
        krb5_ccache			cc  = 0;
        const char * realm = NULL;
        struct afsconf_cell cellconfig;
        char local_cell[MAXCELLCHARS+1];

        while ( count-- ) {
            code = pkrb5_parse_name(ctx, principals[count], &princ);
            if (code) goto loop_cleanup;

            code = KFW_get_ccache(ctx, princ, &cc);
            if (code) goto loop_cleanup;

            code = KFW_AFS_get_cellconfig( cell, (void*)&cellconfig, local_cell);
            if ( code ) goto loop_cleanup;

            realm = afs_realm_of_cell(ctx, &cellconfig);  // do not free
            if ( IsDebuggerPresent() ) {
                OutputDebugString("Realm: ");
                OutputDebugString(realm);
                OutputDebugString("\n");
            }

#ifdef COMMENT
            /* krb5_cc_remove_cred() is not implemented 
             * for a single cred 
             */
            code = pkrb5_build_principal(ctx, &service, strlen(realm),
                                          realm, "afs", cell, NULL);
            if (!code) {
                memset(&mcreds, 0, sizeof(krb5_creds));
                mcreds.client = princ;
                mcreds.server = service;

                code = pkrb5_cc_retrieve_cred(ctx, cc, 0, &mcreds, &creds);
                if (!code) {
                    if ( IsDebuggerPresent() ) {
                        char * cname, *sname;
                        pkrb5_unparse_name(ctx, creds.client, &cname);
                        pkrb5_unparse_name(ctx, creds.server, &sname);
                        OutputDebugString("Removing credential for client \"");
                        OutputDebugString(cname);
                        OutputDebugString("\" and service \"");
                        OutputDebugString(sname);
                        OutputDebugString("\"\n");
                        pkrb5_free_unparsed_name(ctx,cname);
                        pkrb5_free_unparsed_name(ctx,sname);
                    }

                    code = pkrb5_cc_remove_cred(ctx, cc, 0, &creds);
                    pkrb5_free_principal(ctx, creds.client);
                    pkrb5_free_principal(ctx, creds.server);
                }
            }
#endif /* COMMENT */

            code = KFW_AFS_klog(ctx, cc, "afs", cell, (char *)realm, 0,NULL);
            if ( IsDebuggerPresent() ) {
                char message[256];
                sprintf(message,"KFW_AFS_klog() returns: %d\n",code);
                OutputDebugString(message);
            }

          loop_cleanup:
            if (cc) {
                pkrb5_cc_close(ctx, cc);
                cc = 0;
            }
            if (princ) {
                pkrb5_free_principal(ctx, princ);
                princ = 0;
            }
            if (service) {
                pkrb5_free_principal(ctx, service);
                princ = 0;
            }

            KFW_AFS_update_cell_princ_map(ctx, cell, principals[count], code ? FALSE : TRUE);
            free(principals[count]);
        }
        free(principals);
    } else
        code = -1;      // we did not renew the tokens 

  cleanup:
    pkrb5_free_context(ctx);
    return (code ? FALSE : TRUE);

}

int
KFW_AFS_renew_tokens_for_all_cells(void)
{
    struct cell_principal_map * next = cell_princ_map;

    if ( IsDebuggerPresent() )
        OutputDebugString("KFW_AFS_renew_tokens_for_all()\n");

    if ( !next )
        return 0;

    for ( ; next ; next = next->next ) {
        if ( next->active )
            KFW_AFS_renew_token_for_cell(next->cell);
    }
    return 0;
}

int
KFW_renew(krb5_context alt_ctx, krb5_ccache alt_cc)
{
    krb5_error_code		        code = 0;
    krb5_context		        ctx = 0;
    krb5_ccache			        cc = 0;
    krb5_principal		        me = 0;
    krb5_principal              server = 0;
    krb5_creds			        my_creds;
    krb5_data                   *realm = 0;

    if (!pkrb5_init_context)
        return 0;

	memset(&my_creds, 0, sizeof(krb5_creds));

    if ( alt_ctx ) {
        ctx = alt_ctx;
    } else {
        code = pkrb5_init_context(&ctx);
        if (code) goto cleanup;
    }

    if ( alt_cc ) {
        cc = alt_cc;
    } else {
        code = pkrb5_cc_default(ctx, &cc);
        if (code) goto cleanup;
    }

    code = pkrb5_cc_get_principal(ctx, cc, &me);
    if (code) goto cleanup;

    realm = krb5_princ_realm(ctx, me);

    code = pkrb5_build_principal_ext(ctx, &server,
                                    realm->length,realm->data,
                                    KRB5_TGS_NAME_SIZE, KRB5_TGS_NAME,
                                    realm->length,realm->data,
                                    0);
    if ( code ) 
        goto cleanup;

    if ( IsDebuggerPresent() ) {
        char * cname, *sname;
        pkrb5_unparse_name(ctx, me, &cname);
        pkrb5_unparse_name(ctx, server, &sname);
        OutputDebugString("Renewing credential for client \"");
        OutputDebugString(cname);
        OutputDebugString("\" and service \"");
        OutputDebugString(sname);
        OutputDebugString("\"\n");
        pkrb5_free_unparsed_name(ctx,cname);
        pkrb5_free_unparsed_name(ctx,sname);
    }

    my_creds.client = me;
    my_creds.server = server;

    code = pkrb5_get_renewed_creds(ctx, &my_creds, me, cc, NULL);
    if (code) {
        if ( IsDebuggerPresent() ) {
            char message[256];
            sprintf(message,"krb5_get_renewed_creds() failed: %d\n",code);
            OutputDebugString(message);
        }
        goto cleanup;
    }

    code = pkrb5_cc_initialize(ctx, cc, me);
    if (code) {
        if ( IsDebuggerPresent() ) {
            char message[256];
            sprintf(message,"krb5_cc_initialize() failed: %d\n",code);
            OutputDebugString(message);
        }
        goto cleanup;
    }

    code = pkrb5_cc_store_cred(ctx, cc, &my_creds);
    if (code) {
        if ( IsDebuggerPresent() ) {
            char message[256];
            sprintf(message,"krb5_cc_store_cred() failed: %d\n",code);
            OutputDebugString(message);
        }
        goto cleanup;
    }

  cleanup:
    if (my_creds.client == me)
        my_creds.client = 0;
    if (my_creds.server == server)
        my_creds.server = 0;
    pkrb5_free_cred_contents(ctx, &my_creds);
    if (me)
        pkrb5_free_principal(ctx, me);
    if (server)
        pkrb5_free_principal(ctx, server);
    if (cc && (cc != alt_cc))
        pkrb5_cc_close(ctx, cc);
    if (ctx && (ctx != alt_ctx))
        pkrb5_free_context(ctx);
    return(code);
}

int
KFW_kinit( krb5_context alt_ctx,
            krb5_ccache  alt_cc,
            HWND hParent,
            char *principal_name,
            char *password,
            krb5_deltat lifetime,
            DWORD                       forwardable,
            DWORD                       proxiable,
            krb5_deltat                 renew_life,
            DWORD                       addressless,
            DWORD                       publicIP
            )
{
    krb5_error_code		        code = 0;
    krb5_context		        ctx = 0;
    krb5_ccache			        cc = 0;
    krb5_principal		        me = 0;
    char*                       name = 0;
    krb5_creds			        my_creds;
    krb5_get_init_creds_opt     options;
    krb5_address **             addrs = NULL;
    int                         i = 0, addr_count = 0;

    if (!pkrb5_init_context)
        return 0;

    pkrb5_get_init_creds_opt_init(&options);
    memset(&my_creds, 0, sizeof(my_creds));

    if (alt_ctx)
    {
        ctx = alt_ctx;
    }
    else
    {
        code = pkrb5_init_context(&ctx);
        if (code) goto cleanup;
    }

    if ( alt_cc ) {
        cc = alt_cc;
    } else {
        code = pkrb5_cc_default(ctx, &cc);  
        if (code) goto cleanup;
    }

    code = pkrb5_parse_name(ctx, principal_name, &me);
    if (code) 
	goto cleanup;

    code = pkrb5_unparse_name(ctx, me, &name);
    if (code) 
	goto cleanup;

    if (lifetime == 0)
        lifetime = pLeash_get_default_lifetime();
    lifetime *= 60;

    if (renew_life > 0)
	renew_life *= 60;

    if (lifetime)
        pkrb5_get_init_creds_opt_set_tkt_life(&options, lifetime);
	pkrb5_get_init_creds_opt_set_forwardable(&options,
                                                 forwardable ? 1 : 0);
	pkrb5_get_init_creds_opt_set_proxiable(&options,
                                               proxiable ? 1 : 0);
	pkrb5_get_init_creds_opt_set_renew_life(&options,
                                               renew_life);
    if (addressless)
        pkrb5_get_init_creds_opt_set_address_list(&options,NULL);
    else {
	if (publicIP)
        {
            // we are going to add the public IP address specified by the user
            // to the list provided by the operating system
            krb5_address ** local_addrs=NULL;
            DWORD           netIPAddr;

            pkrb5_os_localaddr(ctx, &local_addrs);
            while ( local_addrs[i++] );
            addr_count = i + 1;

            addrs = (krb5_address **) malloc((addr_count+1) * sizeof(krb5_address *));
            if ( !addrs ) {
                pkrb5_free_addresses(ctx, local_addrs);
                goto cleanup;
            }
            memset(addrs, 0, sizeof(krb5_address *) * (addr_count+1));
            i = 0;
            while ( local_addrs[i] ) {
                addrs[i] = (krb5_address *)malloc(sizeof(krb5_address));
                if (addrs[i] == NULL) {
                    pkrb5_free_addresses(ctx, local_addrs);
                    goto cleanup;
                }

                addrs[i]->magic = local_addrs[i]->magic;
                addrs[i]->addrtype = local_addrs[i]->addrtype;
                addrs[i]->length = local_addrs[i]->length;
                addrs[i]->contents = (unsigned char *)malloc(addrs[i]->length);
                if (!addrs[i]->contents) {
                    pkrb5_free_addresses(ctx, local_addrs);
                    goto cleanup;
                }

                memcpy(addrs[i]->contents,local_addrs[i]->contents,
                        local_addrs[i]->length);        /* safe */
                i++;
            }
            pkrb5_free_addresses(ctx, local_addrs);

            addrs[i] = (krb5_address *)malloc(sizeof(krb5_address));
            if (addrs[i] == NULL)
                goto cleanup;

            addrs[i]->magic = KV5M_ADDRESS;
            addrs[i]->addrtype = AF_INET;
            addrs[i]->length = 4;
            addrs[i]->contents = (unsigned char *)malloc(addrs[i]->length);
            if (!addrs[i]->contents)
                goto cleanup;

            netIPAddr = htonl(publicIP);
            memcpy(addrs[i]->contents,&netIPAddr,4);
        
            pkrb5_get_init_creds_opt_set_address_list(&options,addrs);

        }
    }

    code = pkrb5_get_init_creds_password(ctx, 
                                       &my_creds, 
                                       me,
                                       password, // password
                                       KRB5_prompter, // prompter
                                       hParent, // prompter data
                                       0, // start time
                                       0, // service name
                                       &options);
    if (code) 
	goto cleanup;

    code = pkrb5_cc_initialize(ctx, cc, me);
    if (code) 
	goto cleanup;

    code = pkrb5_cc_store_cred(ctx, cc, &my_creds);
    if (code) 
	goto cleanup;

 cleanup:
    if ( addrs ) {
        for ( i=0;i<addr_count;i++ ) {
            if ( addrs[i] ) {
                if ( addrs[i]->contents )
                    free(addrs[i]->contents);
                free(addrs[i]);
            }
        }
    }
    if (my_creds.client == me)
	my_creds.client = 0;
    pkrb5_free_cred_contents(ctx, &my_creds);
    if (name)
        pkrb5_free_unparsed_name(ctx, name);
    if (me)
        pkrb5_free_principal(ctx, me);
    if (cc && (cc != alt_cc))
        pkrb5_cc_close(ctx, cc);
    if (ctx && (ctx != alt_ctx))
        pkrb5_free_context(ctx);
    return(code);
}


int
KFW_kdestroy(krb5_context alt_ctx, krb5_ccache alt_cc)
{
    krb5_context		ctx;
    krb5_ccache			cc;
    krb5_error_code		code;

    if (!pkrb5_init_context)
        return 0;

    if (alt_ctx)
    {
        ctx = alt_ctx;
    }
    else
    {
        code = pkrb5_init_context(&ctx);
        if (code) goto cleanup;
    }

    if ( alt_cc ) {
        cc = alt_cc;
    } else {
        code = pkrb5_cc_default(ctx, &cc);  
        if (code) goto cleanup;
    }

    code = pkrb5_cc_destroy(ctx, cc);
    if ( !code ) cc = 0;

  cleanup:
    if (cc && (cc != alt_cc))
        pkrb5_cc_close(ctx, cc);
    if (ctx && (ctx != alt_ctx))
        pkrb5_free_context(ctx);

    return(code);
}


#ifdef USE_MS2MIT
static BOOL
GetSecurityLogonSessionData(PSECURITY_LOGON_SESSION_DATA * ppSessionData)
{
    NTSTATUS Status = 0;
    HANDLE  TokenHandle;
    TOKEN_STATISTICS Stats;
    DWORD   ReqLen;
    BOOL    Success;

    if (!ppSessionData)
        return FALSE;
    *ppSessionData = NULL;

    Success = OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY, &TokenHandle );
    if ( !Success )
        return FALSE;

    Success = GetTokenInformation( TokenHandle, TokenStatistics, &Stats, sizeof(TOKEN_STATISTICS), &ReqLen );
    CloseHandle( TokenHandle );
    if ( !Success )
        return FALSE;

    Status = pLsaGetLogonSessionData( &Stats.AuthenticationId, ppSessionData );
    if ( FAILED(Status) || !ppSessionData )
        return FALSE;

    return TRUE;
}

//
// MSLSA_IsKerberosLogon() does not validate whether or not there are valid tickets in the 
// cache.  It validates whether or not it is reasonable to assume that if we 
// attempted to retrieve valid tickets we could do so.  Microsoft does not 
// automatically renew expired tickets.  Therefore, the cache could contain
// expired or invalid tickets.  Microsoft also caches the user's password 
// and will use it to retrieve new TGTs if the cache is empty and tickets
// are requested.

static BOOL
MSLSA_IsKerberosLogon(VOID)
{
    PSECURITY_LOGON_SESSION_DATA pSessionData = NULL;
    BOOL    Success = FALSE;

    if ( GetSecurityLogonSessionData(&pSessionData) ) {
        if ( pSessionData->AuthenticationPackage.Buffer ) {
            WCHAR buffer[256];
            WCHAR *usBuffer;
            int usLength;

            Success = FALSE;
            usBuffer = (pSessionData->AuthenticationPackage).Buffer;
            usLength = (pSessionData->AuthenticationPackage).Length;
            if (usLength < 256)
            {
                lstrcpynW (buffer, usBuffer, usLength);
                lstrcatW (buffer,L"");
                if ( !lstrcmpW(L"Kerberos",buffer) )
                    Success = TRUE;
            }
        }
        pLsaFreeReturnBuffer(pSessionData);
    }
    return Success;
}
#endif /* USE_MS2MIT */

static BOOL CALLBACK 
MultiInputDialogProc( HWND hDialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    int i;

    switch ( message ) {
    case WM_INITDIALOG:
        if ( GetDlgCtrlID((HWND) wParam) != ID_MID_TEXT )
        {
            SetFocus(GetDlgItem( hDialog, ID_MID_TEXT));
            return FALSE;
        }
		for ( i=0; i < mid_cnt ; i++ ) {
			if (mid_tb[i].echo == 0)
				SendDlgItemMessage(hDialog, ID_MID_TEXT+i, EM_SETPASSWORDCHAR, 32, 0);
		    else if (mid_tb[i].echo == 2) 
				SendDlgItemMessage(hDialog, ID_MID_TEXT+i, EM_SETPASSWORDCHAR, '*', 0);
		}
        return TRUE;

    case WM_COMMAND:
        switch ( LOWORD(wParam) ) {
        case IDOK:
            for ( i=0; i < mid_cnt ; i++ ) {
                if ( !GetDlgItemText(hDialog, ID_MID_TEXT+i, mid_tb[i].buf, mid_tb[i].len) )
                    *mid_tb[i].buf = '\0';
            }
            /* fallthrough */
        case IDCANCEL:
            EndDialog(hDialog, LOWORD(wParam));
            return TRUE;
        }
    }
    return FALSE;
}

static LPWORD 
lpwAlign( LPWORD lpIn )
{
    ULONG_PTR ul;

    ul = (ULONG_PTR) lpIn;
    ul += 3;
    ul >>=2;
    ul <<=2;
    return (LPWORD) ul;;
}

/*
 * dialog widths are measured in 1/4 character widths
 * dialog height are measured in 1/8 character heights
 */

static LRESULT
MultiInputDialog( HINSTANCE hinst, HWND hwndOwner, 
                  char * ptext[], int numlines, int width, 
                  int tb_cnt, struct textField * tb)
{
    HGLOBAL hgbl;
    LPDLGTEMPLATE lpdt;
    LPDLGITEMTEMPLATE lpdit;
    LPWORD lpw;
    LPWSTR lpwsz;
    LRESULT ret;
    int nchar, i, pwid;

    hgbl = GlobalAlloc(GMEM_ZEROINIT, 4096);
    if (!hgbl)
        return -1;
 
    mid_cnt = tb_cnt;
    mid_tb = tb;

    lpdt = (LPDLGTEMPLATE)GlobalLock(hgbl);
 
    // Define a dialog box.
 
    lpdt->style = WS_POPUP | WS_BORDER | WS_SYSMENU
                   | DS_MODALFRAME | WS_CAPTION | DS_CENTER 
                   | DS_SETFOREGROUND | DS_3DLOOK
                   | DS_SETFONT | DS_FIXEDSYS | DS_NOFAILCREATE;
    lpdt->cdit = numlines + (2 * tb_cnt) + 2;  // number of controls
    lpdt->x  = 10;  
    lpdt->y  = 10;
    lpdt->cx = 20 + width * 4; 
    lpdt->cy = 20 + (numlines + tb_cnt + 4) * 14;

    lpw = (LPWORD) (lpdt + 1);
    *lpw++ = 0;   // no menu
    *lpw++ = 0;   // predefined dialog box class (by default)

    lpwsz = (LPWSTR) lpw;
    nchar = MultiByteToWideChar (CP_ACP, 0, "", -1, lpwsz, 128);
    lpw   += nchar;
    *lpw++ = 8;                        // font size (points)
    lpwsz = (LPWSTR) lpw;
    nchar = MultiByteToWideChar (CP_ACP, 0, "MS Shell Dlg", 
                                    -1, lpwsz, 128);
    lpw   += nchar;

    //-----------------------
    // Define an OK button.
    //-----------------------
    lpw = lpwAlign (lpw); // align DLGITEMTEMPLATE on DWORD boundary
    lpdit = (LPDLGITEMTEMPLATE) lpw;
    lpdit->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP | WS_BORDER;
    lpdit->dwExtendedStyle = 0;
    lpdit->x  = (lpdt->cx - 14)/4 - 20; 
    lpdit->y  = 10 + (numlines + tb_cnt + 2) * 14;
    lpdit->cx = 40; 
    lpdit->cy = 14;
    lpdit->id = IDOK;  // OK button identifier

    lpw = (LPWORD) (lpdit + 1);
    *lpw++ = 0xFFFF;
    *lpw++ = 0x0080;    // button class

    lpwsz = (LPWSTR) lpw;
    nchar = MultiByteToWideChar (CP_ACP, 0, "OK", -1, lpwsz, 50);
    lpw   += nchar;
    *lpw++ = 0;           // no creation data

    //-----------------------
    // Define an Cancel button.
    //-----------------------
    lpw = lpwAlign (lpw); // align DLGITEMTEMPLATE on DWORD boundary
    lpdit = (LPDLGITEMTEMPLATE) lpw;
    lpdit->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP | WS_BORDER;
    lpdit->dwExtendedStyle = 0;
    lpdit->x  = (lpdt->cx - 14)*3/4 - 20; 
    lpdit->y  = 10 + (numlines + tb_cnt + 2) * 14;
    lpdit->cx = 40; 
    lpdit->cy = 14;
    lpdit->id = IDCANCEL;  // CANCEL button identifier

    lpw = (LPWORD) (lpdit + 1);
    *lpw++ = 0xFFFF;
    *lpw++ = 0x0080;    // button class

    lpwsz = (LPWSTR) lpw;
    nchar = MultiByteToWideChar (CP_ACP, 0, "Cancel", -1, lpwsz, 50);
    lpw   += nchar;
    *lpw++ = 0;           // no creation data

    /* Add controls for preface data */
    for ( i=0; i<numlines; i++) {
        /*-----------------------
         * Define a static text control.
         *-----------------------*/
        lpw = lpwAlign (lpw); /* align DLGITEMTEMPLATE on DWORD boundary */
        lpdit = (LPDLGITEMTEMPLATE) lpw;
        lpdit->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
        lpdit->dwExtendedStyle = 0;
        lpdit->x  = 10; 
        lpdit->y  = 10 + i * 14;
        lpdit->cx = (short)strlen(ptext[i]) * 4 + 10; 
        lpdit->cy = 14;
        lpdit->id = ID_TEXT + i;  // text identifier

        lpw = (LPWORD) (lpdit + 1);
        *lpw++ = 0xFFFF;
        *lpw++ = 0x0082;                         // static class

        lpwsz = (LPWSTR) lpw;
        nchar = MultiByteToWideChar (CP_ACP, 0, ptext[i], 
                                         -1, lpwsz, 2*width);
        lpw   += nchar;
        *lpw++ = 0;           // no creation data
    }
    
    for ( i=0, pwid = 0; i<tb_cnt; i++) {
	int len = (int)strlen(tb[i].label);
        if ( pwid < len )
            pwid = len;
    }

    for ( i=0; i<tb_cnt; i++) {
        /* Prompt */
        /*-----------------------
         * Define a static text control.
         *-----------------------*/
        lpw = lpwAlign (lpw); /* align DLGITEMTEMPLATE on DWORD boundary */
        lpdit = (LPDLGITEMTEMPLATE) lpw;
        lpdit->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
        lpdit->dwExtendedStyle = 0;
        lpdit->x  = 10; 
        lpdit->y  = 10 + (numlines + i + 1) * 14;
        lpdit->cx = pwid * 4; 
        lpdit->cy = 14;
        lpdit->id = ID_TEXT + numlines + i;  // text identifier

        lpw = (LPWORD) (lpdit + 1);
        *lpw++ = 0xFFFF;
        *lpw++ = 0x0082;                         // static class

        lpwsz = (LPWSTR) lpw;
        nchar = MultiByteToWideChar (CP_ACP, 0, tb[i].label ? tb[i].label : "", 
                                     -1, lpwsz, 128);
        lpw   += nchar;
        *lpw++ = 0;           // no creation data

        /*-----------------------
         * Define an edit control.
         *-----------------------*/
        lpw = lpwAlign (lpw); /* align DLGITEMTEMPLATE on DWORD boundary */
        lpdit = (LPDLGITEMTEMPLATE) lpw;
        lpdit->style = WS_CHILD | WS_VISIBLE | ES_LEFT | WS_TABSTOP | WS_BORDER | (tb[i].echo == 1 ? 0L : ES_PASSWORD);
        lpdit->dwExtendedStyle = 0;
        lpdit->x  = 10 + (pwid + 1) * 4; 
        lpdit->y  = 10 + (numlines + i + 1) * 14;
        lpdit->cx = (width - (pwid + 1)) * 4; 
        lpdit->cy = 14;
        lpdit->id = ID_MID_TEXT + i;             // identifier

        lpw = (LPWORD) (lpdit + 1);
        *lpw++ = 0xFFFF;
        *lpw++ = 0x0081;                         // edit class

        lpwsz = (LPWSTR) lpw;
        nchar = MultiByteToWideChar (CP_ACP, 0, tb[i].def ? tb[i].def : "", 
                                     -1, lpwsz, 128);
        lpw   += nchar;
        *lpw++ = 0;           // no creation data
    }

    GlobalUnlock(hgbl); 
    ret = DialogBoxIndirect(hinst, (LPDLGTEMPLATE) hgbl, 
							hwndOwner, (DLGPROC) MultiInputDialogProc); 
    GlobalFree(hgbl); 

    switch ( ret ) {
    case 0:     /* Timeout */
        return -1;
    case IDOK:
        return 1;
    case IDCANCEL:
        return 0;
    default: {
        char buf[256];
        sprintf(buf,"DialogBoxIndirect() failed: %d",GetLastError());
        MessageBox(hwndOwner,
                    buf,
                    "GetLastError()",
                    MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
        return -1;
    }
    }
}

static int
multi_field_dialog(HWND hParent, char * preface, int n, struct textField tb[])
{
    HINSTANCE hInst = 0;
    int maxwidth = 0;
    int numlines = 0;
    int len;
    char * plines[16], *p = preface ? preface : "";
    int i;

    for ( i=0; i<16; i++ ) 
        plines[i] = NULL;

    while (*p && numlines < 16) {
        plines[numlines++] = p;
        for ( ;*p && *p != '\r' && *p != '\n'; p++ );
        if ( *p == '\r' && *(p+1) == '\n' ) {
            *p++ = '\0';
            p++;
        } else if ( *p == '\n' ) {
            *p++ = '\0';
        } 
        if ( strlen(plines[numlines-1]) > maxwidth )
            maxwidth = (int)strlen(plines[numlines-1]);
    }

    for ( i=0;i<n;i++ ) {
        len = (int)strlen(tb[i].label) + 1 + (tb[i].len > 40 ? 40 : tb[i].len);
        if ( maxwidth < len )
            maxwidth = len;
    }

    return(MultiInputDialog(hInst, hParent, plines, numlines, maxwidth, n, tb)?1:0);
}

static krb5_error_code KRB5_CALLCONV
KRB5_prompter( krb5_context context,
               void *data,
               const char *name,
               const char *banner,
               int num_prompts,
               krb5_prompt prompts[])
{
    krb5_error_code     errcode = 0;
    int                 i;
    struct textField * tb = NULL;
    int    len = 0, blen=0, nlen=0;
	HWND hParent = (HWND)data;

    if (name)
        nlen = (int)strlen(name)+2;

    if (banner)
        blen = (int)strlen(banner)+2;

    tb = (struct textField *) malloc(sizeof(struct textField) * num_prompts);
    if ( tb != NULL ) {
        int ok;
        memset(tb,0,sizeof(struct textField) * num_prompts);
        for ( i=0; i < num_prompts; i++ ) {
            tb[i].buf = prompts[i].reply->data;
            tb[i].len = prompts[i].reply->length;
            tb[i].label = prompts[i].prompt;
            tb[i].def = NULL;
            tb[i].echo = (prompts[i].hidden ? 2 : 1);
        }   

        ok = multi_field_dialog(hParent,(char *)banner,num_prompts,tb);
        if ( ok ) {
            for ( i=0; i < num_prompts; i++ )
                prompts[i].reply->length = (unsigned int)strlen(prompts[i].reply->data);
        } else
            errcode = -2;
    }

    if ( tb )
        free(tb);
    if (errcode) {
        for (i = 0; i < num_prompts; i++) {
            memset(prompts[i].reply->data, 0, prompts[i].reply->length);
        }
    }
    return errcode;
}

BOOL
KFW_AFS_wait_for_service_start(void)
{
    char    HostName[64];
    DWORD   CurrentState;

    CurrentState = SERVICE_START_PENDING;
    memset(HostName, '\0', sizeof(HostName));
    gethostname(HostName, sizeof(HostName));

    while (CurrentState != SERVICE_RUNNING || CurrentState != SERVICE_STOPPED)
    {
        if (GetServiceStatus(HostName, TRANSARCAFSDAEMON, &CurrentState) != NOERROR)
            return(0);
        if ( IsDebuggerPresent() ) {
            switch ( CurrentState ) {
            case SERVICE_STOPPED:
                OutputDebugString("SERVICE_STOPPED\n");
                break;
            case SERVICE_START_PENDING:
                OutputDebugString("SERVICE_START_PENDING\n");
                break;
            case SERVICE_STOP_PENDING:
                OutputDebugString("SERVICE_STOP_PENDING\n");
                break;
            case SERVICE_RUNNING:
                OutputDebugString("SERVICE_RUNNING\n");
                break;
            case SERVICE_CONTINUE_PENDING:
                OutputDebugString("SERVICE_CONTINUE_PENDING\n");
                break;
            case SERVICE_PAUSE_PENDING:
                OutputDebugString("SERVICE_PAUSE_PENDING\n");
                break;
            case SERVICE_PAUSED:
                OutputDebugString("SERVICE_PAUSED\n");
                break;
            default:
                OutputDebugString("UNKNOWN Service State\n");
            }
        }
        if (CurrentState == SERVICE_STOPPED)
            return(0);
        if (CurrentState == SERVICE_RUNNING)
            return(1);
        Sleep(500);
    }
    return(0);
}


int
KFW_AFS_unlog(void)
{
    long	rc;
    char    HostName[64];
    DWORD   CurrentState;

    CurrentState = 0;
    memset(HostName, '\0', sizeof(HostName));
    gethostname(HostName, sizeof(HostName));
    if (GetServiceStatus(HostName, TRANSARCAFSDAEMON, &CurrentState) != NOERROR)
        return(0);
    if (CurrentState != SERVICE_RUNNING)
        return(0);

    rc = ktc_ForgetAllTokens();

    return(0);
}


#define ALLOW_REGISTER 1
static int
ViceIDToUsername(char *username, 
                 char *realm_of_user, 
                 char *realm_of_cell,
                 char * cell_to_use,
                 struct ktc_principal *aclient, 
                 struct ktc_principal *aserver, 
                 struct ktc_token *atoken)
{
    static char lastcell[MAXCELLCHARS+1] = { 0 };
    static char confname[512] = { 0 };
#ifdef AFS_ID_TO_NAME
    char username_copy[BUFSIZ];
#endif /* AFS_ID_TO_NAME */
    long viceId = ANONYMOUSID;		/* AFS uid of user */
    int  status = 0;
#ifdef ALLOW_REGISTER
    afs_int32 id;
#endif /* ALLOW_REGISTER */

    if (confname[0] == '\0') {
        strncpy(confname, AFSDIR_CLIENT_ETC_DIRPATH, sizeof(confname));
        confname[sizeof(confname) - 2] = '\0';
    }

    strcpy(lastcell, aserver->cell);

    if (!pr_Initialize (0, confname, aserver->cell)) {
        char sname[PR_MAXNAMELEN];
        strncpy(sname, username, PR_MAXNAMELEN);
        sname[PR_MAXNAMELEN-1] = '\0';    
        status = pr_SNameToId (sname, &viceId);
	pr_End();
    }

    /*
     * This is a crock, but it is Transarc's crock, so
     * we have to play along in order to get the
     * functionality.  The way the afs id is stored is
     * as a string in the username field of the token.
     * Contrary to what you may think by looking at
     * the code for tokens, this hack (AFS ID %d) will
     * not work if you change %d to something else.
     */

    /*
     * This code is taken from cklog -- it lets people
     * automatically register with the ptserver in foreign cells
     */

#ifdef ALLOW_REGISTER
    if (status == 0) {
        if (viceId != ANONYMOUSID) {
#else /* ALLOW_REGISTER */
            if ((status == 0) && (viceId != ANONYMOUSID))
#endif /* ALLOW_REGISTER */
            {
#ifdef AFS_ID_TO_NAME
                strncpy(username_copy, username, BUFSIZ);
                snprintf (username, BUFSIZ, "%s (AFS ID %d)", username_copy, (int) viceId);
#endif /* AFS_ID_TO_NAME */
            }
#ifdef ALLOW_REGISTER
        } else if (strcmp(realm_of_user, realm_of_cell) != 0) {
            id = 0;
            strncpy(aclient->name, username, MAXKTCNAMELEN - 1);
            strcpy(aclient->instance, "");
            strncpy(aclient->cell, realm_of_user, MAXKTCREALMLEN - 1);
            if (status = ktc_SetToken(aserver, atoken, aclient, 0))
                return status;
            if (status = pr_Initialize(1L, confname, aserver->cell))
                return status;
            status = pr_CreateUser(username, &id);
	    pr_End();
	    if (status)
		return status;
#ifdef AFS_ID_TO_NAME
            strncpy(username_copy, username, BUFSIZ);
            snprintf (username, BUFSIZ, "%s (AFS ID %d)", username_copy, (int) viceId);
#endif /* AFS_ID_TO_NAME */
        }
    }
#endif /* ALLOW_REGISTER */
    return status;
}


int
KFW_AFS_klog(
    krb5_context alt_ctx,
    krb5_ccache  alt_cc,
    char *service,
    char *cell,
    char *realm,
    int  lifetime,  	/* unused parameter */
    char *smbname
    )
{
    long	rc = 0;
    CREDENTIALS	creds;
#ifdef USE_KRB4
    KTEXT_ST    ticket;
#endif /* USE_KRB4 */
    struct ktc_principal	aserver;
    struct ktc_principal	aclient;
    char	realm_of_user[REALM_SZ]; /* Kerberos realm of user */
    char	realm_of_cell[REALM_SZ]; /* Kerberos realm of cell */
    char	local_cell[MAXCELLCHARS+1];
    char	Dmycell[MAXCELLCHARS+1];
    struct ktc_token	atoken;
    struct ktc_token	btoken;
    struct afsconf_cell	ak_cellconfig; /* General information about the cell */
    char	RealmName[128];
    char	CellName[128];
    char	ServiceName[128];
    DWORD       CurrentState;
    char        HostName[64];
    BOOL        try_krb5 = 0;
    krb5_context  ctx = 0;
    krb5_ccache   cc = 0;
    krb5_creds increds;
    krb5_creds * k5creds = 0;
    krb5_error_code code;
    krb5_principal client_principal = 0;
    krb5_data * k5data;
    int i, retry = 0;

    CurrentState = 0;
    memset(HostName, '\0', sizeof(HostName));
    gethostname(HostName, sizeof(HostName));
    if (GetServiceStatus(HostName, TRANSARCAFSDAEMON, &CurrentState) != NOERROR) {
        if ( IsDebuggerPresent() )
            OutputDebugString("Unable to retrieve AFSD Service Status\n");
        return(-1);
    }
    if (CurrentState != SERVICE_RUNNING) {
        if ( IsDebuggerPresent() )
            OutputDebugString("AFSD Service NOT RUNNING\n");
        return(-2);
    }

    if (!pkrb5_init_context)
        return 0;

    memset(RealmName, '\0', sizeof(RealmName));
    memset(CellName, '\0', sizeof(CellName));
    memset(ServiceName, '\0', sizeof(ServiceName));
    memset(realm_of_user, '\0', sizeof(realm_of_user));
    memset(realm_of_cell, '\0', sizeof(realm_of_cell));
    if (cell && cell[0])
        strcpy(Dmycell, cell);
    else
        memset(Dmycell, '\0', sizeof(Dmycell));

    // NULL or empty cell returns information on local cell
    if (rc = KFW_AFS_get_cellconfig(Dmycell, &ak_cellconfig, local_cell))
    {
        // KFW_AFS_error(rc, "get_cellconfig()");
        return(rc);
    }

    if ( alt_ctx ) {
        ctx = alt_ctx;
    } else {
        code = pkrb5_init_context(&ctx);
        if (code) goto cleanup;
    }

    if ( alt_cc ) {
        cc = alt_cc;
    } else {
        code = pkrb5_cc_default(ctx, &cc);
        if (code) goto skip_krb5_init;
    }

    memset((char *)&increds, 0, sizeof(increds));

    code = pkrb5_cc_get_principal(ctx, cc, &client_principal);
    if (code) {
        if ( code == KRB5_CC_NOTFOUND && IsDebuggerPresent() ) 
        {
            OutputDebugString("Principal Not Found for ccache\n");
        }
        goto skip_krb5_init;
    }

    /* look for client principals which cannot be distinguished 
     * from Kerberos 4 multi-component principal names
     */
    k5data = krb5_princ_component(ctx,client_principal,0);
    for ( i=0; i<k5data->length; i++ ) {
        if ( k5data->data[i] == '.' )
            break;
    }
    if (i != k5data->length)
    {
        OutputDebugString("Illegal Principal name contains dot in first component\n");
        rc = KRB5KRB_ERR_GENERIC;
        goto cleanup;
    }

    i = krb5_princ_realm(ctx, client_principal)->length;
    if (i > REALM_SZ-1) 
        i = REALM_SZ-1;
    strncpy(realm_of_user,krb5_princ_realm(ctx, client_principal)->data,i);
    realm_of_user[i] = 0;
    try_krb5 = 1;

  skip_krb5_init:
#ifdef USE_KRB4
    if ( !try_krb5 || !realm_of_user[0] ) {
        if ((rc = (*pkrb_get_tf_realm)((*ptkt_string)(), realm_of_user)) != KSUCCESS)
        {
            goto cleanup;
        }       
    }
#else
    if (!try_krb5)
        goto cleanup;
#endif
    strcpy(realm_of_cell, afs_realm_of_cell(ctx, &ak_cellconfig));

    if (strlen(service) == 0)
        strcpy(ServiceName, "afs");
    else
        strcpy(ServiceName, service);

    if (strlen(cell) == 0)
        strcpy(CellName, local_cell);
    else
        strcpy(CellName, cell);

    if (strlen(realm) == 0)
        strcpy(RealmName, realm_of_cell);
    else
        strcpy(RealmName, realm);

    memset(&creds, '\0', sizeof(creds));

    if ( try_krb5 ) {
        int len;

        /* First try service/cell@REALM */
        if (code = pkrb5_build_principal(ctx, &increds.server,
                                          (int)strlen(RealmName),
                                          RealmName,
                                          ServiceName,
                                          CellName,
                                          0)) 
        {
            goto cleanup;
        }

        increds.client = client_principal;
        increds.times.endtime = 0;
        /* Ask for DES since that is what V4 understands */
        increds.keyblock.enctype = ENCTYPE_DES_CBC_CRC;


        if ( IsDebuggerPresent() ) {
            char * cname, *sname;
            pkrb5_unparse_name(ctx, increds.client, &cname);
            pkrb5_unparse_name(ctx, increds.server, &sname);
            OutputDebugString("Getting tickets for \"");
            OutputDebugString(cname);
            OutputDebugString("\" and service \"");
            OutputDebugString(sname);
            OutputDebugString("\"\n");
            pkrb5_free_unparsed_name(ctx,cname);
            pkrb5_free_unparsed_name(ctx,sname);
        }

        code = pkrb5_get_credentials(ctx, 0, cc, &increds, &k5creds);
        if (code == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN ||
             code == KRB5KRB_ERR_GENERIC /* heimdal */ ||
             code == KRB5KRB_AP_ERR_MSG_TYPE) {
            /* Or service@REALM */
            pkrb5_free_principal(ctx,increds.server);
            increds.server = 0;
            code = pkrb5_build_principal(ctx, &increds.server,
                                          (int)strlen(RealmName),
                                          RealmName,
                                          ServiceName,
                                          0);

            if ( IsDebuggerPresent() ) {
                char * cname, *sname;
                pkrb5_unparse_name(ctx, increds.client, &cname);
                pkrb5_unparse_name(ctx, increds.server, &sname);
                OutputDebugString("krb5_get_credentials() returned Service Principal Unknown\n");
                OutputDebugString("Trying again: getting tickets for \"");
                OutputDebugString(cname);
                OutputDebugString("\" and service \"");
                OutputDebugString(sname);
                OutputDebugString("\"\n");
                pkrb5_free_unparsed_name(ctx,cname);
                pkrb5_free_unparsed_name(ctx,sname);
            }

            if (!code)
                code = pkrb5_get_credentials(ctx, 0, cc, &increds, &k5creds);
        }

        if ((code == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN ||
              code == KRB5KRB_ERR_GENERIC /* heimdal */ ||
              code == KRB5KRB_AP_ERR_MSG_TYPE) &&
             strcmp(RealmName, realm_of_cell)) {
            /* Or service/cell@REALM_OF_CELL */
            strcpy(RealmName, realm_of_cell);
            pkrb5_free_principal(ctx,increds.server);
            increds.server = 0;
            code = pkrb5_build_principal(ctx, &increds.server,
                                         (int)strlen(RealmName),
                                         RealmName,
                                         ServiceName,
                                         CellName,
                                         0);

            if ( IsDebuggerPresent() ) {
                char * cname, *sname;
                pkrb5_unparse_name(ctx, increds.client, &cname);
                pkrb5_unparse_name(ctx, increds.server, &sname);
                OutputDebugString("krb5_get_credentials() returned Service Principal Unknown\n");
                OutputDebugString("Trying again: getting tickets for \"");
                OutputDebugString(cname);
                OutputDebugString("\" and service \"");
                OutputDebugString(sname);
                OutputDebugString("\"\n");
                pkrb5_free_unparsed_name(ctx,cname);
                pkrb5_free_unparsed_name(ctx,sname);
            }

            if (!code)
                code = pkrb5_get_credentials(ctx, 0, cc, &increds, &k5creds);

        
            if (code == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN ||
                 code == KRB5KRB_ERR_GENERIC /* heimdal */ ||
				 code == KRB5KRB_AP_ERR_MSG_TYPE) {
                /* Or service@REALM_OF_CELL */
                pkrb5_free_principal(ctx,increds.server);
                increds.server = 0;
                code = pkrb5_build_principal(ctx, &increds.server,
                                              (int)strlen(RealmName),
                                              RealmName,
                                              ServiceName,
                                              0);

                if ( IsDebuggerPresent() ) {
                    char * cname, *sname;
                    pkrb5_unparse_name(ctx, increds.client, &cname);
                    pkrb5_unparse_name(ctx, increds.server, &sname);
                    OutputDebugString("krb5_get_credentials() returned Service Principal Unknown\n");
                    OutputDebugString("Trying again: getting tickets for \"");
                    OutputDebugString(cname);
                    OutputDebugString("\" and service \"");
                    OutputDebugString(sname);
                    OutputDebugString("\"\n");
                    pkrb5_free_unparsed_name(ctx,cname);
                    pkrb5_free_unparsed_name(ctx,sname);
                }

                if (!code)
                    code = pkrb5_get_credentials(ctx, 0, cc, &increds, &k5creds);
            }
        }

        if (code) {
            if ( IsDebuggerPresent() ) {
                char message[256];
                sprintf(message,"krb5_get_credentials returns: %d\n",code);
                OutputDebugString(message);
            }
            try_krb5 = 0;
            goto use_krb4;
        }

        /* This code inserts the entire K5 ticket into the token
         * No need to perform a krb524 translation which is 
         * commented out in the code below
         */
        if (KFW_use_krb524() ||
            k5creds->ticket.length > MAXKTCTICKETLEN)
            goto try_krb524d;

        memset(&aserver, '\0', sizeof(aserver));
        strncpy(aserver.name, ServiceName, MAXKTCNAMELEN - 1);
        strncpy(aserver.cell, CellName, MAXKTCREALMLEN - 1);

        memset(&atoken, '\0', sizeof(atoken));
        atoken.kvno = RXKAD_TKT_TYPE_KERBEROS_V5;
        atoken.startTime = k5creds->times.starttime;
        atoken.endTime = k5creds->times.endtime;
        memcpy(&atoken.sessionKey, k5creds->keyblock.contents, k5creds->keyblock.length);
        atoken.ticketLen = k5creds->ticket.length;
        memcpy(atoken.ticket, k5creds->ticket.data, atoken.ticketLen);

      retry_gettoken5:
        rc = ktc_GetToken(&aserver, &btoken, sizeof(btoken), &aclient);
        if (rc != 0 && rc != KTC_NOENT && rc != KTC_NOCELL) {
            if ( rc == KTC_NOCM && retry < 20 ) {
                Sleep(500);
                retry++;
                goto retry_gettoken5;
            }
            goto try_krb524d;
        }

        if (atoken.kvno == btoken.kvno &&
             atoken.ticketLen == btoken.ticketLen &&
             !memcmp(&atoken.sessionKey, &btoken.sessionKey, sizeof(atoken.sessionKey)) &&
             !memcmp(atoken.ticket, btoken.ticket, atoken.ticketLen)) 
        {
            /* Success - Nothing to do */
            goto cleanup;
        }

        // * Reset the "aclient" structure before we call ktc_SetToken.
        // * This structure was first set by the ktc_GetToken call when
        // * we were comparing whether identical tokens already existed.

        len = min(k5creds->client->data[0].length,MAXKTCNAMELEN - 1);
        strncpy(aclient.name, k5creds->client->data[0].data, len);
        aclient.name[len] = '\0';

        if ( k5creds->client->length > 1 ) {
            char * p;
            strcat(aclient.name, ".");
            p = aclient.name + strlen(aclient.name);
            len = min(k5creds->client->data[1].length,MAXKTCNAMELEN - (int)strlen(aclient.name) - 1);
            strncpy(p, k5creds->client->data[1].data, len);
            p[len] = '\0';
        }
        aclient.instance[0] = '\0';

        strcpy(aclient.cell, realm_of_cell);

        len = min(k5creds->client->realm.length,(int)strlen(realm_of_cell));
        /* For Khimaira, always append the realm name */
        if ( 1 /* strncmp(realm_of_cell, k5creds->client->realm.data, len) */ ) {
            char * p;
            strcat(aclient.name, "@");
            p = aclient.name + strlen(aclient.name);
            len = min(k5creds->client->realm.length,MAXKTCNAMELEN - (int)strlen(aclient.name) - 1);
            strncpy(p, k5creds->client->realm.data, len);
            p[len] = '\0';
        }

	GetEnvironmentVariable(DO_NOT_REGISTER_VARNAME, NULL, 0);
	if (GetLastError() == ERROR_ENVVAR_NOT_FOUND)
	    ViceIDToUsername(aclient.name, realm_of_user, realm_of_cell, CellName, 
			     &aclient, &aserver, &atoken);

        if ( smbname ) {
            strncpy(aclient.smbname, smbname, sizeof(aclient.smbname));
            aclient.smbname[sizeof(aclient.smbname)-1] = '\0';
        } else {
            aclient.smbname[0] = '\0';
        }

        rc = ktc_SetToken(&aserver, &atoken, &aclient, (aclient.smbname[0]?AFS_SETTOK_LOGON:0));
        if (!rc)
            goto cleanup;   /* We have successfully inserted the token */

      try_krb524d:
        /* Otherwise, the ticket could have been too large so try to
         * convert using the krb524d running with the KDC 
         */
        code = pkrb524_convert_creds_kdc(ctx, k5creds, &creds);
        pkrb5_free_creds(ctx, k5creds);
        if (code) {
            if ( IsDebuggerPresent() ) {
                char message[256];
                sprintf(message,"krb524_convert_creds_kdc returns: %d\n",code);
                OutputDebugString(message);
            }
            try_krb5 = 0;
            goto use_krb4;
        }
    } else {
      use_krb4:
#ifdef USE_KRB4
        code = (*pkrb_get_cred)(ServiceName, CellName, RealmName, &creds);
        if (code == NO_TKT_FIL) {
            // if the problem is that we have no krb4 tickets
            // do not attempt to continue
            goto cleanup;
        }
        if (code != KSUCCESS)
            code = (*pkrb_get_cred)(ServiceName, "", RealmName, &creds);

        if (code != KSUCCESS)
        {
            if ((code = (*pkrb_mk_req)(&ticket, ServiceName, CellName, RealmName, 0)) == KSUCCESS)
            {
                if ((code = (*pkrb_get_cred)(ServiceName, CellName, RealmName, &creds)) != KSUCCESS)
                {
                    goto cleanup;
                }
            }
            else if ((code = (*pkrb_mk_req)(&ticket, ServiceName, "", RealmName, 0)) == KSUCCESS)
            {
                if ((code = (*pkrb_get_cred)(ServiceName, "", RealmName, &creds)) != KSUCCESS)
                {
                    goto cleanup;
                }
            }
            else
            {
                goto cleanup;
            }
        }
#else
        goto cleanup;
#endif
    }

    memset(&aserver, '\0', sizeof(aserver));
    strncpy(aserver.name, ServiceName, MAXKTCNAMELEN - 1);
    strncpy(aserver.cell, CellName, MAXKTCREALMLEN - 1);

    memset(&atoken, '\0', sizeof(atoken));
    atoken.kvno = creds.kvno;
    atoken.startTime = creds.issue_date;
    atoken.endTime = creds.issue_date + life_to_time(0,creds.lifetime);
    memcpy(&atoken.sessionKey, creds.session, 8);
    atoken.ticketLen = creds.ticket_st.length;
    memcpy(atoken.ticket, creds.ticket_st.dat, atoken.ticketLen);

  retry_gettoken:
    rc = ktc_GetToken(&aserver, &btoken, sizeof(btoken), &aclient);
    if (rc != 0 && rc != KTC_NOENT && rc != KTC_NOCELL) {
        if ( rc == KTC_NOCM && retry < 20 ) {
            Sleep(500);
            retry++;
            goto retry_gettoken;
        }
        KFW_AFS_error(rc, "ktc_GetToken()");
        code = rc;
        goto cleanup;
    }

    if (atoken.kvno == btoken.kvno &&
        atoken.ticketLen == btoken.ticketLen &&
        !memcmp(&atoken.sessionKey, &btoken.sessionKey, sizeof(atoken.sessionKey)) &&
        !memcmp(atoken.ticket, btoken.ticket, atoken.ticketLen)) 
    {
        goto cleanup;
    }

    // * Reset the "aclient" structure before we call ktc_SetToken.
    // * This structure was first set by the ktc_GetToken call when
    // * we were comparing whether identical tokens already existed.

    strncpy(aclient.name, creds.pname, MAXKTCNAMELEN - 1);
    if (creds.pinst[0])
    {
        strncat(aclient.name, ".", MAXKTCNAMELEN - 1);
        strncat(aclient.name, creds.pinst, MAXKTCNAMELEN - 1);
    }
    strcpy(aclient.instance, "");

    strncat(aclient.name, "@", MAXKTCNAMELEN - 1);
    strncat(aclient.name, creds.realm, MAXKTCREALMLEN - 1);
    aclient.name[MAXKTCREALMLEN-1] = '\0';

    strcpy(aclient.cell, CellName);

    GetEnvironmentVariable(DO_NOT_REGISTER_VARNAME, NULL, 0);
    if (GetLastError() == ERROR_ENVVAR_NOT_FOUND)
	ViceIDToUsername(aclient.name, realm_of_user, realm_of_cell, CellName, 
			 &aclient, &aserver, &atoken);

    if ( smbname ) {
        strncpy(aclient.smbname, smbname, sizeof(aclient.smbname));
        aclient.smbname[sizeof(aclient.smbname)-1] = '\0';
    } else {
        aclient.smbname[0] = '\0';
    }

    if (rc = ktc_SetToken(&aserver, &atoken, &aclient, (aclient.smbname[0]?AFS_SETTOK_LOGON:0)))
    {
        KFW_AFS_error(rc, "ktc_SetToken()");
        code = rc;
        goto cleanup;
    }

  cleanup:
    if (client_principal)
        pkrb5_free_principal(ctx,client_principal);
    /* increds.client == client_principal */
    if (increds.server)
        pkrb5_free_principal(ctx,increds.server);
    if (cc && (cc != alt_cc))
        pkrb5_cc_close(ctx, cc);
    if (ctx && (ctx != alt_ctx))
        pkrb5_free_context(ctx);

    return(rc? rc : code);
}

/**************************************/
/* afs_realm_of_cell():               */
/**************************************/
static char *
afs_realm_of_cell(krb5_context ctx, struct afsconf_cell *cellconfig)
{
    static char krbrlm[REALM_SZ+1]="";
    char ** realmlist=NULL;
    krb5_error_code r;

    if (!cellconfig)
        return 0;

    r = pkrb5_get_host_realm(ctx, cellconfig->hostName[0], &realmlist);
    if ( !r && realmlist && realmlist[0] ) {
        strcpy(krbrlm, realmlist[0]);
        pkrb5_free_host_realm(ctx, realmlist);
    }

    if ( !krbrlm[0] )
    {
        char *s = krbrlm;
        char *t = cellconfig->name;
        int c;

        while (c = *t++)
        {
            if (islower(c)) c=toupper(c);
            *s++ = c;
        }
        *s++ = 0;
    }
    return(krbrlm);
}

/**************************************/
/* KFW_AFS_get_cellconfig():          */
/**************************************/
int 
KFW_AFS_get_cellconfig(char *cell, struct afsconf_cell *cellconfig, char *local_cell)
{
    int	rc;
    char newcell[MAXCELLCHARS+1];

    local_cell[0] = (char)0;
    memset(cellconfig, 0, sizeof(*cellconfig));

    /* WIN32: cm_GetRootCellName(local_cell) - NOTE: no way to get max chars */
    if (rc = cm_GetRootCellName(local_cell))
    {
        return(rc);
    }

    if (strlen(cell) == 0)
        strcpy(cell, local_cell);

    /* WIN32: cm_SearchCellFile(cell, pcallback, pdata) */
    strcpy(cellconfig->name, cell);

    rc = cm_SearchCellFile(cell, newcell, get_cellconfig_callback, (void*)cellconfig);
#ifdef AFS_AFSDB_ENV
    if (rc != 0) {
        int ttl;
        rc = cm_SearchCellByDNS(cell, newcell, &ttl, get_cellconfig_callback, (void*)cellconfig);
    }
#endif
    return rc;
}

/**************************************/
/* get_cellconfig_callback():         */
/**************************************/
static long 
get_cellconfig_callback(void *cellconfig, struct sockaddr_in *addrp, char *namep)
{
    struct afsconf_cell *cc = (struct afsconf_cell *)cellconfig;

    cc->hostAddr[cc->numServers] = *addrp;
    strcpy(cc->hostName[cc->numServers], namep);
    cc->numServers++;
    return(0);
}


/**************************************/
/* KFW_AFS_error():                  */
/**************************************/
void
KFW_AFS_error(LONG rc, LPCSTR FailedFunctionName)
{
    char message[256];
    const char *errText; 

    // Using AFS defines as error messages for now, until Transarc 
    // gets back to me with "string" translations of each of these 
    // const. defines. 
    if (rc == KTC_ERROR)
      errText = "KTC_ERROR";
    else if (rc == KTC_TOOBIG)
      errText = "KTC_TOOBIG";
    else if (rc == KTC_INVAL)
      errText = "KTC_INVAL";
    else if (rc == KTC_NOENT)
      errText = "KTC_NOENT";
    else if (rc == KTC_PIOCTLFAIL)
      errText = "KTC_PIOCTLFAIL";
    else if (rc == KTC_NOPIOCTL)
      errText = "KTC_NOPIOCTL";
    else if (rc == KTC_NOCELL)
      errText = "KTC_NOCELL";
    else if (rc == KTC_NOCM)
      errText = "KTC_NOCM: The service, Transarc AFS Daemon, most likely is not started!";
    else
      errText = "Unknown error!";

    sprintf(message, "%s (0x%x)\n(%s failed)", errText, rc, FailedFunctionName);

    if ( IsDebuggerPresent() ) {
        OutputDebugString(message);
        OutputDebugString("\n");
    }
    MessageBox(NULL, message, "AFS", MB_OK | MB_ICONERROR | MB_TASKMODAL | MB_SETFOREGROUND);
    return;
}

static DWORD 
GetServiceStatus(
    LPSTR lpszMachineName, 
    LPSTR lpszServiceName,
    DWORD *lpdwCurrentState) 
{ 
    DWORD           hr               = NOERROR; 
    SC_HANDLE       schSCManager     = NULL; 
    SC_HANDLE       schService       = NULL; 
    DWORD           fdwDesiredAccess = 0; 
    SERVICE_STATUS  ssServiceStatus  = {0}; 
    BOOL            fRet             = FALSE; 

    *lpdwCurrentState = 0; 
 
    fdwDesiredAccess = GENERIC_READ; 
 
    schSCManager = OpenSCManager(lpszMachineName,  
                                 NULL,
                                 fdwDesiredAccess); 
 
    if(schSCManager == NULL) 
    { 
        hr = GetLastError();
        goto cleanup; 
    } 
 
    schService = OpenService(schSCManager,
                             lpszServiceName,
                             fdwDesiredAccess); 
 
    if(schService == NULL) 
    { 
        hr = GetLastError();
        goto cleanup; 
    } 
 
    fRet = QueryServiceStatus(schService,
                              &ssServiceStatus); 
 
    if(fRet == FALSE) 
    { 
        hr = GetLastError(); 
        goto cleanup; 
    } 
 
    *lpdwCurrentState = ssServiceStatus.dwCurrentState; 
 
cleanup: 
 
    CloseServiceHandle(schService); 
    CloseServiceHandle(schSCManager); 
 
    return(hr); 
} 

void
UnloadFuncs(
    FUNC_INFO fi[], 
    HINSTANCE h
    )
{
    int n;
    if (fi)
        for (n = 0; fi[n].func_ptr_var; n++)
            *(fi[n].func_ptr_var) = 0;
    if (h) FreeLibrary(h);
}

int
LoadFuncs(
    const char* dll_name, 
    FUNC_INFO fi[], 
    HINSTANCE* ph,  // [out, optional] - DLL handle
    int* pindex,    // [out, optional] - index of last func loaded (-1 if none)
    int cleanup,    // cleanup function pointers and unload on error
    int go_on,      // continue loading even if some functions cannot be loaded
    int silent      // do not pop-up a system dialog if DLL cannot be loaded
    )
{
    HINSTANCE h;
    int i, n, last_i;
    int error = 0;
    UINT em;

    if (ph) *ph = 0;
    if (pindex) *pindex = -1;

    for (n = 0; fi[n].func_ptr_var; n++)
	*(fi[n].func_ptr_var) = 0;

    if (silent)
	em = SetErrorMode(SEM_FAILCRITICALERRORS);
    h = LoadLibrary(dll_name);
    if (silent)
        SetErrorMode(em);

    if (!h)
        return 0;

    last_i = -1;
    for (i = 0; (go_on || !error) && (i < n); i++)
    {
	void* p = (void*)GetProcAddress(h, fi[i].func_name);
	if (!p)
	    error = 1;
        else
        {
            last_i = i;
	    *(fi[i].func_ptr_var) = p;
        }
    }
    if (pindex) *pindex = last_i;
    if (error && cleanup && !go_on) {
	for (i = 0; i < n; i++) {
	    *(fi[i].func_ptr_var) = 0;
	}
	FreeLibrary(h);
	return 0;
    }
    if (ph) *ph = h;
    if (error) return 0;
    return 1;
}

BOOL KFW_probe_kdc(struct afsconf_cell * cellconfig)
{
    krb5_context ctx = 0;
    krb5_ccache cc = 0;
    krb5_error_code code;
    krb5_data pwdata;
    const char * realm = 0;
    krb5_principal principal = 0;
    char * pname = 0;
    char   password[PROBE_PASSWORD_LEN+1];
    BOOL serverReachable = 0;

    if (!pkrb5_init_context)
        return 0;

    code = pkrb5_init_context(&ctx);
    if (code) goto cleanup;


    realm = afs_realm_of_cell(ctx, cellconfig);  // do not free

    code = pkrb5_build_principal(ctx, &principal, (int)strlen(realm),
                                  realm, PROBE_USERNAME, NULL, NULL);
    if ( code ) goto cleanup;

    code = KFW_get_ccache(ctx, principal, &cc);
    if ( code ) goto cleanup;

    code = pkrb5_unparse_name(ctx, principal, &pname);
    if ( code ) goto cleanup;

    pwdata.data = password;
    pwdata.length = PROBE_PASSWORD_LEN;
    code = pkrb5_c_random_make_octets(ctx, &pwdata);
    if (code) {
        int i;
        for ( i=0 ; i<PROBE_PASSWORD_LEN ; i++ )
            password[i] = 'x';
    }
    password[PROBE_PASSWORD_LEN] = '\0';

    code = KFW_kinit(NULL, NULL, HWND_DESKTOP, 
                      pname, 
                      password,
                      5,
                      0,
                      0,
                      0,
                      1,
                      0);
    switch ( code ) {
    case KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN:
    case KRB5KDC_ERR_CLIENT_REVOKED:
    case KRB5KDC_ERR_CLIENT_NOTYET:
    case KRB5KDC_ERR_PREAUTH_FAILED:
    case KRB5KDC_ERR_PREAUTH_REQUIRED:
    case KRB5KDC_ERR_PADATA_TYPE_NOSUPP:
        serverReachable = TRUE;
        break;
    default:
        serverReachable = FALSE;
    }

  cleanup:
    if ( pname )
        pkrb5_free_unparsed_name(ctx,pname);
    if ( principal )
        pkrb5_free_principal(ctx,principal);
    if (cc)
        pkrb5_cc_close(ctx,cc);
    if (ctx)
        pkrb5_free_context(ctx);

    return serverReachable;
}

BOOL
KFW_AFS_get_lsa_principal(char * szUser, DWORD *dwSize)
{
    krb5_context   ctx = 0;
    krb5_error_code code;
    krb5_ccache mslsa_ccache=0;
    krb5_principal princ = 0;
    char * pname = 0;
    BOOL success = 0;

    if (!KFW_is_available())
        return FALSE;

    if (code = pkrb5_init_context(&ctx))
        goto cleanup;

    if (code = pkrb5_cc_resolve(ctx, "MSLSA:", &mslsa_ccache))
        goto cleanup;

    if (code = pkrb5_cc_get_principal(ctx, mslsa_ccache, &princ))
        goto cleanup;

    if (code = pkrb5_unparse_name(ctx, princ, &pname))
        goto cleanup;

    if ( strlen(pname) < *dwSize ) {
        strncpy(szUser, pname, *dwSize);
        szUser[*dwSize-1] = '\0';
        success = 1;
    }
    *dwSize = (DWORD)strlen(pname);

  cleanup:
    if (pname)
        pkrb5_free_unparsed_name(ctx, pname);

    if (princ)
        pkrb5_free_principal(ctx, princ);

    if (mslsa_ccache)
        pkrb5_cc_close(ctx, mslsa_ccache);

    if (ctx)
        pkrb5_free_context(ctx);
    return success;
}

int 
KFW_AFS_set_file_cache_dacl(char *filename, HANDLE hUserToken)
{
    // SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_SID_AUTHORITY;
    PSID pSystemSID = NULL;
    DWORD SystemSIDlength = 0, UserSIDlength = 0;
    PACL ccacheACL = NULL;
    DWORD ccacheACLlength = 0;
    PTOKEN_USER pTokenUser = NULL;
    DWORD retLen;
    DWORD gle;
    int ret = 0;  

    if (!filename) {
	return 1;
    }

    /* Get System SID */
    if (!ConvertStringSidToSid("S-1-5-18", &pSystemSID)) {
	ret = 1;
	goto cleanup;
    }

    /* Create ACL */
    SystemSIDlength = GetLengthSid(pSystemSID);
    ccacheACLlength = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE)
        + SystemSIDlength - sizeof(DWORD);

    if (hUserToken) {
	if (!GetTokenInformation(hUserToken, TokenUser, NULL, 0, &retLen))
	{
	    if ( GetLastError() == ERROR_INSUFFICIENT_BUFFER ) {
		pTokenUser = (PTOKEN_USER) LocalAlloc(LPTR, retLen);

		GetTokenInformation(hUserToken, TokenUser, pTokenUser, retLen, &retLen);
	    }		 
	}

	if (pTokenUser) {
	    UserSIDlength = GetLengthSid(pTokenUser->User.Sid);

	    ccacheACLlength += sizeof(ACCESS_ALLOWED_ACE) + UserSIDlength 
		- sizeof(DWORD);
	}
    }

    ccacheACL = (PACL) LocalAlloc(LPTR, ccacheACLlength);
    if (!ccacheACL) {
 	ret = 1;
 	goto cleanup;
     }
    InitializeAcl(ccacheACL, ccacheACLlength, ACL_REVISION);
    AddAccessAllowedAceEx(ccacheACL, ACL_REVISION, 0,
                         STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL,
                         pSystemSID);
    if (pTokenUser) {
	AddAccessAllowedAceEx(ccacheACL, ACL_REVISION, 0,
			     STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL,
			     pTokenUser->User.Sid);
	if (!SetNamedSecurityInfo( filename, SE_FILE_OBJECT,
				   DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
				   NULL,
				   NULL, 
				   ccacheACL,
				   NULL)) {
 	    gle = GetLastError();
 	    if (gle != ERROR_NO_TOKEN)
		ret = 1;
	}
	if (!SetNamedSecurityInfo( filename, SE_FILE_OBJECT,
				   OWNER_SECURITY_INFORMATION,
				   pTokenUser->User.Sid,
				   NULL, 
				   NULL,
				   NULL)) {
 	    gle = GetLastError();
 	    if (gle != ERROR_NO_TOKEN)
		ret = 1;
	}
    } else {
	if (!SetNamedSecurityInfo( filename, SE_FILE_OBJECT,
				   DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
				   NULL,
				   NULL, 
				   ccacheACL,
				   NULL)) {
 	    gle = GetLastError();
 	    if (gle != ERROR_NO_TOKEN)
		ret = 1;
	}
    }

  cleanup:
    if (pSystemSID)
	LocalFree(pSystemSID);
    if (pTokenUser)
	LocalFree(pTokenUser);
    if (ccacheACL)
	LocalFree(ccacheACL);
    return ret;
}

int 
KFW_AFS_obtain_user_temp_directory(HANDLE hUserToken, char *newfilename, int size)
{
    int  retval = 0;
    DWORD dwSize = size-1;	/* leave room for nul */
    DWORD dwLen  = 0;
 
    if (!hUserToken || !newfilename || size <= 0)
 	return;
 
     *newfilename = '\0';
 
     dwLen = ExpandEnvironmentStringsForUser(hUserToken, "%TEMP%", newfilename, dwSize);
     if ( !dwLen || dwLen > dwSize )
 	dwLen = ExpandEnvironmentStringsForUser(hUserToken, "%TMP%", newfilename, dwSize);
     if ( !dwLen || dwLen > dwSize )
 	return 1;
 
     newfilename[dwSize] = '\0';
    return 0;
}

void
KFW_AFS_copy_cache_to_system_file(char * user, char * szLogonId)
{
    char filename[MAX_PATH] = "";
    DWORD count;
    char cachename[MAX_PATH + 8] = "FILE:";
    krb5_context		ctx = 0;
    krb5_error_code		code;
    krb5_principal              princ = 0;
    krb5_ccache			cc  = 0;
    krb5_ccache                 ncc = 0;

    if (!pkrb5_init_context || !user || !szLogonId)
        return;

    count = GetEnvironmentVariable("TEMP", filename, sizeof(filename));
    if ( count > sizeof(filename) || count == 0 ) {
        GetWindowsDirectory(filename, sizeof(filename));
    }

    if ( strlen(filename) + strlen(szLogonId) + 2 > sizeof(filename) )
        return;

    strcat(filename, "\\");
    strcat(filename, szLogonId);    

    strcat(cachename, filename);

    DeleteFile(filename);

    code = pkrb5_init_context(&ctx);
    if (code) ctx = 0;

    code = pkrb5_parse_name(ctx, user, &princ);
    if (code) goto cleanup;

    code = KFW_get_ccache(ctx, princ, &cc);
    if (code) goto cleanup;

    code = pkrb5_cc_resolve(ctx, cachename, &ncc);
    if (code) goto cleanup;

    code = pkrb5_cc_initialize(ctx, ncc, princ);
    if (code) goto cleanup;

    code = KFW_AFS_set_file_cache_dacl(filename, NULL);
    if (code) goto cleanup;

    code = pkrb5_cc_copy_creds(ctx,cc,ncc);

  cleanup:
    if ( cc ) {
        pkrb5_cc_close(ctx, cc);
        cc = 0;
    }
    if ( ncc ) {
        pkrb5_cc_close(ctx, ncc);
        ncc = 0;
    }
    if ( princ ) {
        pkrb5_free_principal(ctx, princ);
        princ = 0;
    }

    if (ctx)
        pkrb5_free_context(ctx);
}

int
KFW_AFS_copy_file_cache_to_default_cache(char * filename)
{
    char cachename[MAX_PATH + 8] = "FILE:";
    krb5_context		ctx = 0;
    krb5_error_code		code;
    krb5_principal              princ = 0;
    krb5_ccache			cc  = 0;
    krb5_ccache                 ncc = 0;
    int retval = 1;

    if (!pkrb5_init_context || !filename)
        return 1;

    if ( strlen(filename) + sizeof("FILE:") > sizeof(cachename) )
        return 1;

    strcat(cachename, filename);

    code = pkrb5_init_context(&ctx);
    if (code) ctx = 0;

    code = pkrb5_cc_resolve(ctx, cachename, &cc);
    if (code) goto cleanup;
    
    code = pkrb5_cc_get_principal(ctx, cc, &princ);

    code = pkrb5_cc_default(ctx, &ncc);
    if (!code) {
        code = pkrb5_cc_initialize(ctx, ncc, princ);

        if (!code)
            code = pkrb5_cc_copy_creds(ctx,cc,ncc);
    }
    if ( ncc ) {
        pkrb5_cc_close(ctx, ncc);
        ncc = 0;
    }

    retval=0;   /* success */

  cleanup:
    if ( cc ) {
        pkrb5_cc_close(ctx, cc);
        cc = 0;
    }

    DeleteFile(filename);

    if ( princ ) {
        pkrb5_free_principal(ctx, princ);
        princ = 0;
    }

    if (ctx)
        pkrb5_free_context(ctx);

    return 0;
}

/* We are including this 

/* Ticket lifetime.  This defines the table used to lookup lifetime for the
   fixed part of rande of the one byte lifetime field.  Values less than 0x80
   are intrpreted as the number of 5 minute intervals.  Values from 0x80 to
   0xBF should be looked up in this table.  The value of 0x80 is the same using
   both methods: 10 and two-thirds hours .  The lifetime of 0xBF is 30 days.
   The intervening values of have a fixed ratio of roughly 1.06914.  The value
   oxFF is defined to mean a ticket has no expiration time.  This should be
   used advisedly since individual servers may impose defacto upperbounds on
   ticket lifetimes. */

#define TKTLIFENUMFIXED 64
#define TKTLIFEMINFIXED 0x80
#define TKTLIFEMAXFIXED 0xBF
#define TKTLIFENOEXPIRE 0xFF
#define MAXTKTLIFETIME	(30*24*3600)	/* 30 days */

static const int tkt_lifetimes[TKTLIFENUMFIXED] = {
    38400,			/* 10.67 hours, 0.44 days */
    41055,			/* 11.40 hours, 0.48 days */
    43894,			/* 12.19 hours, 0.51 days */
    46929,			/* 13.04 hours, 0.54 days */
    50174,			/* 13.94 hours, 0.58 days */
    53643,			/* 14.90 hours, 0.62 days */
    57352,			/* 15.93 hours, 0.66 days */
    61318,			/* 17.03 hours, 0.71 days */
    65558,			/* 18.21 hours, 0.76 days */
    70091,			/* 19.47 hours, 0.81 days */
    74937,			/* 20.82 hours, 0.87 days */
    80119,			/* 22.26 hours, 0.93 days */
    85658,			/* 23.79 hours, 0.99 days */
    91581,			/* 25.44 hours, 1.06 days */
    97914,			/* 27.20 hours, 1.13 days */
    104684,			/* 29.08 hours, 1.21 days */
    111922,			/* 31.09 hours, 1.30 days */
    119661,			/* 33.24 hours, 1.38 days */
    127935,			/* 35.54 hours, 1.48 days */
    136781,			/* 37.99 hours, 1.58 days */
    146239,			/* 40.62 hours, 1.69 days */
    156350,			/* 43.43 hours, 1.81 days */
    167161,			/* 46.43 hours, 1.93 days */
    178720,			/* 49.64 hours, 2.07 days */
    191077,			/* 53.08 hours, 2.21 days */
    204289,			/* 56.75 hours, 2.36 days */
    218415,			/* 60.67 hours, 2.53 days */
    233517,			/* 64.87 hours, 2.70 days */
    249664,			/* 69.35 hours, 2.89 days */
    266926,			/* 74.15 hours, 3.09 days */
    285383,			/* 79.27 hours, 3.30 days */
    305116,			/* 84.75 hours, 3.53 days */
    326213,			/* 90.61 hours, 3.78 days */
    348769,			/* 96.88 hours, 4.04 days */
    372885,			/* 103.58 hours, 4.32 days */
    398668,			/* 110.74 hours, 4.61 days */
    426234,			/* 118.40 hours, 4.93 days */
    455705,			/* 126.58 hours, 5.27 days */
    487215,			/* 135.34 hours, 5.64 days */
    520904,			/* 144.70 hours, 6.03 days */
    556921,			/* 154.70 hours, 6.45 days */
    595430,			/* 165.40 hours, 6.89 days */
    636601,			/* 176.83 hours, 7.37 days */
    680618,			/* 189.06 hours, 7.88 days */
    727680,			/* 202.13 hours, 8.42 days */
    777995,			/* 216.11 hours, 9.00 days */
    831789,			/* 231.05 hours, 9.63 days */
    889303,			/* 247.03 hours, 10.29 days */
    950794,			/* 264.11 hours, 11.00 days */
    1016537,			/* 282.37 hours, 11.77 days */
    1086825,			/* 301.90 hours, 12.58 days */
    1161973,			/* 322.77 hours, 13.45 days */
    1242318,			/* 345.09 hours, 14.38 days */
    1328218,			/* 368.95 hours, 15.37 days */
    1420057,			/* 394.46 hours, 16.44 days */
    1518247,			/* 421.74 hours, 17.57 days */
    1623226,			/* 450.90 hours, 18.79 days */
    1735464,			/* 482.07 hours, 20.09 days */
    1855462,			/* 515.41 hours, 21.48 days */
    1983758,			/* 551.04 hours, 22.96 days */
    2120925,			/* 589.15 hours, 24.55 days */
    2267576,			/* 629.88 hours, 26.25 days */
    2424367,			/* 673.44 hours, 28.06 days */
    2592000
};				/* 720.00 hours, 30.00 days */

/* life_to_time - takes a start time and a Kerberos standard lifetime char and
 * returns the corresponding end time.  There are four simple cases to be
 * handled.  The first is a life of 0xff, meaning no expiration, and results in
 * an end time of 0xffffffff.  The second is when life is less than the values
 * covered by the table.  In this case, the end time is the start time plus the
 * number of 5 minute intervals specified by life.  The third case returns
 * start plus the MAXTKTLIFETIME if life is greater than TKTLIFEMAXFIXED.  The
 * last case, uses the life value (minus TKTLIFEMINFIXED) as an index into the
 * table to extract the lifetime in seconds, which is added to start to produce
 * the end time. */

afs_uint32
life_to_time(afs_uint32 start, unsigned char life)
{
    int realLife;

    if (life == TKTLIFENOEXPIRE)
	return NEVERDATE;
    if (life < TKTLIFEMINFIXED)
	return start + life * 5 * 60;
    if (life > TKTLIFEMAXFIXED)
	return start + MAXTKTLIFETIME;
    realLife = tkt_lifetimes[life - TKTLIFEMINFIXED];
    return start + realLife;
}

/* time_to_life - takes start and end times for the ticket and returns a
 * Kerberos standard lifetime char possibily using the tkt_lifetimes table for
 * lifetimes above 127*5minutes.  First, the special case of (end ==
 * 0xffffffff) is handled to mean no expiration.  Then negative lifetimes and
 * those greater than the maximum ticket lifetime are rejected.  Then lifetimes
 * less than the first table entry are handled by rounding the requested
 * lifetime *up* to the next 5 minute interval.  The final step is to search
 * the table for the smallest entry *greater than or equal* to the requested
 * entry.  The actual code is prepared to handle the case where the table is
 * unordered but that it an unnecessary frill. */

static unsigned char
time_to_life(afs_uint32 start, afs_uint32 end)
{
    int lifetime = end - start;
    int best, best_i;
    int i;

    if (end == NEVERDATE)
	return TKTLIFENOEXPIRE;
    if ((lifetime > MAXKTCTICKETLIFETIME) || (lifetime <= 0))
	return 0;
    if (lifetime < tkt_lifetimes[0])
	return (lifetime + 5 * 60 - 1) / (5 * 60);
    best_i = -1;
    best = MAXKTCTICKETLIFETIME;
    for (i = 0; i < TKTLIFENUMFIXED; i++)
	if (tkt_lifetimes[i] >= lifetime) {
	    int diff = tkt_lifetimes[i] - lifetime;
	    if (diff < best) {
		best = diff;
		best_i = i;
	    }
	}
    if (best_i < 0)
	return 0;
    return best_i + TKTLIFEMINFIXED;
}

