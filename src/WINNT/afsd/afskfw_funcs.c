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


#undef  USE_KRB4
#ifndef _WIN64
#define USE_KRB524 1
#endif
#define USE_MS2MIT 1
#define USE_LEASH 1

#include "afskfw-int.h"
#include "afskfw.h"
#include <userenv.h>
#include <Sddl.h>
#include <Aclapi.h>

#include <osilog.h>
#include <afs/com_err.h>
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

#ifdef USE_LEASH
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
#endif 

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
DECL_FUNC_PTR(krb5_set_default_realm);
DECL_FUNC_PTR(krb5_free_default_realm);
DECL_FUNC_PTR(krb5_free_ticket);
DECL_FUNC_PTR(krb5_decode_ticket);
DECL_FUNC_PTR(krb5_get_host_realm);
DECL_FUNC_PTR(krb5_free_host_realm);
DECL_FUNC_PTR(krb5_free_addresses);
DECL_FUNC_PTR(krb5_c_random_make_octets);

// Added for rxk5
DECL_FUNC_PTR(krb5_copy_addresses);
DECL_FUNC_PTR(krb5_copy_principal);
DECL_FUNC_PTR(krb5_free_keyblock_contents);
DECL_FUNC_PTR(krb5_free_checksum_contents);
DECL_FUNC_PTR(krb5_c_block_size);
DECL_FUNC_PTR(krb5_c_make_checksum);
DECL_FUNC_PTR(krb5_c_verify_checksum);
DECL_FUNC_PTR(krb5_c_checksum_length);
DECL_FUNC_PTR(krb5_c_encrypt_length);
DECL_FUNC_PTR(krb5_c_encrypt);
DECL_FUNC_PTR(krb5_c_decrypt);
DECL_FUNC_PTR(krb5_c_make_random_key);
DECL_FUNC_PTR(krb5_kt_get_entry);
DECL_FUNC_PTR(krb5_kt_next_entry);
DECL_FUNC_PTR(krb5_kt_start_seq_get);
DECL_FUNC_PTR(krb5_kt_end_seq_get);
DECL_FUNC_PTR(krb5_kt_close);
DECL_FUNC_PTR(krb5_kt_resolve);

// Adding to loadfuncs-krb5.h
DECL_FUNC_PTR(krb5_free_keytab_entry_contents);
DECL_FUNC_PTR(krb5_c_is_keyed_cksum);
DECL_FUNC_PTR(krb5_c_is_coll_proof_cksum);
DECL_FUNC_PTR(krb5_c_valid_enctype);
DECL_FUNC_PTR(krb5_c_valid_cksumtype);

// Special rxk5
DECL_FUNC_PTR(krb5_server_decrypt_ticket_keyblock);
DECL_FUNC_PTR(krb5_encrypt_tkt_part);
DECL_FUNC_PTR(encode_krb5_ticket);

#ifdef USE_KRB524
// Krb524 functions
DECL_FUNC_PTR(krb524_init_ets);
DECL_FUNC_PTR(krb524_convert_creds_kdc);
#endif

#ifdef USE_KRB4
// krb4 functions
DECL_FUNC_PTR(krb_get_cred);
DECL_FUNC_PTR(tkt_string);
DECL_FUNC_PTR(krb_get_tf_realm);
DECL_FUNC_PTR(krb_mk_req);
#endif

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

#ifdef USE_LEASH
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
#endif

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
    MAKE_FUNC_INFO(krb5_set_default_realm),
    MAKE_FUNC_INFO(krb5_free_default_realm),
    MAKE_FUNC_INFO(krb5_free_ticket),
    MAKE_FUNC_INFO(krb5_decode_ticket),
    MAKE_FUNC_INFO(krb5_get_host_realm),
    MAKE_FUNC_INFO(krb5_free_host_realm),
    MAKE_FUNC_INFO(krb5_free_addresses),
    MAKE_FUNC_INFO(krb5_c_random_make_octets),
    MAKE_FUNC_INFO(krb5_copy_addresses),
    MAKE_FUNC_INFO(krb5_copy_principal),
    MAKE_FUNC_INFO(krb5_free_keyblock_contents),
    MAKE_FUNC_INFO(krb5_free_checksum_contents),
    MAKE_FUNC_INFO(krb5_c_block_size),
    MAKE_FUNC_INFO(krb5_c_make_checksum),
    MAKE_FUNC_INFO(krb5_c_verify_checksum),
    MAKE_FUNC_INFO(krb5_c_checksum_length),
    MAKE_FUNC_INFO(krb5_c_encrypt_length),
    MAKE_FUNC_INFO(krb5_c_encrypt),
    MAKE_FUNC_INFO(krb5_c_decrypt),
    MAKE_FUNC_INFO(krb5_c_make_random_key),
    MAKE_FUNC_INFO(krb5_kt_get_entry),
    MAKE_FUNC_INFO(krb5_kt_next_entry),
    MAKE_FUNC_INFO(krb5_kt_start_seq_get),
    MAKE_FUNC_INFO(krb5_kt_end_seq_get),
    MAKE_FUNC_INFO(krb5_kt_close),
    MAKE_FUNC_INFO(krb5_kt_resolve),
    MAKE_FUNC_INFO(krb5_free_keytab_entry_contents),
    MAKE_FUNC_INFO(krb5_c_is_keyed_cksum),
    MAKE_FUNC_INFO(krb5_c_is_coll_proof_cksum),
    MAKE_FUNC_INFO(krb5_c_valid_enctype),    
    MAKE_FUNC_INFO(krb5_c_valid_cksumtype),
    MAKE_FUNC_INFO(krb5_server_decrypt_ticket_keyblock),
    MAKE_FUNC_INFO(krb5_encrypt_tkt_part),
    MAKE_FUNC_INFO(encode_krb5_ticket),
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
#ifdef USE_KRB524
static HINSTANCE hKrb524 = 0;
#endif
#ifdef USE_MS2MIT
static HINSTANCE hSecur32 = 0;
#endif /* USE_MS2MIT */
static HINSTANCE hAdvApi32 = 0;
static HINSTANCE hService = 0;
static HINSTANCE hProfile = 0;
#ifdef USE_LEASH
static HINSTANCE hLeash = 0;
static HINSTANCE hLeashOpt = 0;
#endif
static HINSTANCE hCCAPI = 0;
static struct principal_ccache_data * princ_cc_data = NULL;
static struct cell_principal_map    * cell_princ_map = NULL;


static void
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

static int
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

void
KFW_initialize_funcs(void)
{
    static int inited = 0;

    if ( !inited ) {
        char mutexName[MAX_PATH];
        HANDLE hMutex = NULL;

        sprintf(mutexName, "AFS KFW Init Funcs pid=%d", getpid());
        
        hMutex = CreateMutex( NULL, TRUE, mutexName );
        if ( GetLastError() == ERROR_ALREADY_EXISTS ) {
            if ( WaitForSingleObject( hMutex, INFINITE ) != WAIT_OBJECT_0 ) {
                return;
            }
        }
        if ( !inited ) {
            inited = 1;
            LoadFuncs(KRB5_DLL, k5_fi, &hKrb5, 0, 1, 1 /* 0 */, 1 /* 0 */);
#ifdef USE_KRB4
            LoadFuncs(KRB4_DLL, k4_fi, &hKrb4, 0, 1, 0, 0);
#endif /* USE_KRB4 */
            LoadFuncs(SERVICE_DLL, service_fi, &hService, 0, 1, 0, 0);
#ifdef USE_MS2MIT
            LoadFuncs(SECUR32_DLL, lsa_fi, &hSecur32, 0, 1, 1, 1);
#endif /* USE_MS2MIT */
#ifdef USE_KRB524
            LoadFuncs(KRB524_DLL, k524_fi, &hKrb524, 0, 1, 1, 1);
#endif
            LoadFuncs(PROFILE_DLL, profile_fi, &hProfile, 0, 1, 0, 0);
#ifdef USE_LEASH
            LoadFuncs(LEASH_DLL, leash_fi, &hLeash, 0, 1, 0, 0);
            LoadFuncs(LEASH_DLL, leash_opt_fi, &hLeashOpt, 0, 1, 0, 0);
#endif
            LoadFuncs(CCAPI_DLL, ccapi_fi, &hCCAPI, 0, 1, 0, 0);

        }
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }
}

int
KFW_available_funcs(void)
{
    if ( hKrb5 && hComErr && hService && 
#ifdef USE_MS2MIT
         hSecur32 && 
#endif /* USE_MS2MIT */
#ifdef USE_KRB524
         hKrb524 &&
#endif
#ifdef USE_LEASH
         hLeash &&
#endif
         hProfile && hCCAPI )
        return TRUE;
    return FALSE;
}

int
KFW_funcs_cc_initialize_loaded(void)
{
	if(pcc_initialize)
		return 1;
	else
		return 0;
}

void
KFW_cleanup_funcs(void)
{
#ifdef USE_LEASH
    if (hLeashOpt)
        FreeLibrary(hLeashOpt);
    if (hLeash)
        FreeLibrary(hLeash);
#endif
#ifdef USE_KRB524
    if (hKrb524)
        FreeLibrary(hKrb524);
#endif
    if (hCCAPI)
        FreeLibrary(hCCAPI);
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

BOOLEAN 
WINAPI
DllMain(
    IN HINSTANCE hDllHandle, 
    IN DWORD     nReason,    
    IN LPVOID    Reserved)
{
	/* try to initialize on load */

    BOOLEAN result = TRUE;
	
    switch (nReason) {
        case DLL_PROCESS_ATTACH:
			KFW_initialize_funcs();
            break;
        case DLL_PROCESS_DETACH:
			KFW_cleanup_funcs();
            break;
    }
	return result;
}

/* Stub functions */

/* leash */

DWORD afskfw_Leash_get_default_lifetime()
{
	return pLeash_get_default_lifetime();
}

DWORD afskfw_Leash_get_default_forwardable()
{
	return pLeash_get_default_forwardable();
}

DWORD afskfw_Leash_get_default_renew_till()
{
	return pLeash_get_default_renew_till();
}

DWORD afskfw_Leash_get_default_noaddresses()
{
	return pLeash_get_default_noaddresses();
}

DWORD afskfw_Leash_get_default_proxiable()
{
	return pLeash_get_default_proxiable();
}

DWORD afskfw_Leash_get_default_publicip()
{
	return pLeash_get_default_publicip();
}

DWORD afskfw_Leash_get_default_use_krb4()
{
	return pLeash_get_default_use_krb4();
}

DWORD afskfw_Leash_get_default_life_min()
{
	return pLeash_get_default_life_min();
}

DWORD afskfw_Leash_get_default_life_max()
{
	return pLeash_get_default_life_max();
}

DWORD afskfw_Leash_get_default_renew_min()
{
	return pLeash_get_default_renew_min();
}

DWORD afskfw_Leash_get_default_renew_max()
{
	return pLeash_get_default_renew_max();
}

DWORD afskfw_Leash_get_default_renewable()
{
	return pLeash_get_default_renewable();
}

DWORD afskfw_Leash_get_default_mslsa_import()
{
	return pLeash_get_default_mslsa_import();
}

/* krb5 */

krb5_error_code 
afskfw_krb5_change_password
	(krb5_context context, krb5_creds *creds, char *newpw,
			int *result_code, krb5_data *result_code_string,
			krb5_data *result_string)
{
	return pkrb5_change_password
	(context, creds, newpw, result_code, result_code_string,
			result_string);
}

void 
afskfw_krb5_get_init_creds_opt_init
(krb5_get_init_creds_opt *opt)
{
	pkrb5_get_init_creds_opt_init(opt);
}

void 
afskfw_krb5_get_init_creds_opt_set_tkt_life
(krb5_get_init_creds_opt *opt,
		krb5_deltat tkt_life)
{
	pkrb5_get_init_creds_opt_set_tkt_life
	(opt, tkt_life);
}

void 
afskfw_krb5_get_init_creds_opt_set_renew_life
(krb5_get_init_creds_opt *opt,
		krb5_deltat renew_life)
{
	pkrb5_get_init_creds_opt_set_renew_life
	(opt, renew_life);
}

void 
afskfw_krb5_get_init_creds_opt_set_forwardable
(krb5_get_init_creds_opt *opt,
		int forwardable)
{
	pkrb5_get_init_creds_opt_set_forwardable
	(opt, forwardable);
}

void 
afskfw_krb5_get_init_creds_opt_set_proxiable
(krb5_get_init_creds_opt *opt,
		int proxiable)
{
	pkrb5_get_init_creds_opt_set_proxiable
	(opt, proxiable);
}

void 
afskfw_krb5_get_init_creds_opt_set_address_list
(krb5_get_init_creds_opt *opt,
		krb5_address **addresses)
{
	pkrb5_get_init_creds_opt_set_address_list
	(opt, addresses);
}
		
krb5_error_code 
afskfw_krb5_get_init_creds_password
(krb5_context context,
		krb5_creds *creds,
		krb5_principal client,
		char *password,
		krb5_prompter_fct prompter,
		void *data,
		krb5_deltat start_time,
		char *in_tkt_service,
		krb5_get_init_creds_opt *k5_gic_options)
{
	return pkrb5_get_init_creds_password
	(context, creds, client, password, prompter,
		data, start_time, in_tkt_service, k5_gic_options);
}

krb5_error_code  
afskfw_krb5_build_principal_ext
(krb5_context context,  krb5_principal * princ,
			 unsigned int rlen, const char * realm, ...)
{
	krb5_error_code code;
	
	va_list rest_args;
	va_start(rest_args, realm);
	code = pkrb5_build_principal_ext
		(context, princ, rlen, realm, rest_args);
	va_end(rest_args);
	
	return code;
}

const char * 
afskfw_krb5_cc_get_name (krb5_context context, krb5_ccache cache)
{
	return pkrb5_cc_get_name(context, cache);
}

krb5_error_code  
afskfw_krb5_cc_resolve
	(krb5_context context, const char *name, krb5_ccache *cache)
{
	return pkrb5_cc_resolve
	(context, name, cache);
}
		
const char *  
afskfw_krb5_cc_default_name
	(krb5_context context)
{
	return pkrb5_cc_default_name
	(context);
}
	
krb5_error_code  
afskfw_krb5_cc_set_default_name
	(krb5_context context, const char *ccname)
{
	return pkrb5_cc_set_default_name
	(context, ccname);
}

krb5_error_code  
afskfw_krb5_cc_default
	(krb5_context context, krb5_ccache *ccache)
{
	return pkrb5_cc_default
	(context, ccache);
}

krb5_error_code 
afskfw_krb5_cc_initialize(krb5_context context, krb5_ccache cache,
		   krb5_principal principal)
{
	return pkrb5_cc_initialize(context, cache, principal);
}

krb5_error_code 
afskfw_krb5_cc_destroy (krb5_context context, krb5_ccache cache)
{
	return pkrb5_cc_destroy(context, cache);
}

krb5_error_code 
afskfw_krb5_cc_close (krb5_context context, krb5_ccache cache)
{
	return pkrb5_cc_close(context, cache);
}

krb5_error_code 
afskfw_krb5_cc_store_cred (krb5_context context, krb5_ccache cache,
		    krb5_creds *creds)
{
	return pkrb5_cc_store_cred(context, cache, creds);
}

krb5_error_code 
afskfw_krb5_cc_copy_creds(krb5_context context, krb5_ccache incc, 
                          krb5_ccache outcc)
{
    return pkrb5_cc_copy_creds(context, incc, outcc);
}

krb5_error_code 
afskfw_krb5_cc_retrieve_cred (krb5_context context, krb5_ccache cache,
		       krb5_flags flags, krb5_creds *mcreds,
		       krb5_creds *creds)
{
	return pkrb5_cc_retrieve_cred(context, cache, flags, mcreds,
		       creds);
}

krb5_error_code 
afskfw_krb5_cc_get_principal (krb5_context context, krb5_ccache cache,
		       krb5_principal *principal)
{
	return pkrb5_cc_get_principal(context, cache, principal);
}

krb5_error_code 
afskfw_krb5_cc_start_seq_get (krb5_context context, krb5_ccache cache,
		       krb5_cc_cursor *cursor)
{
	return pkrb5_cc_start_seq_get(context, cache, cursor);
}

krb5_error_code 
afskfw_krb5_cc_next_cred (krb5_context context, krb5_ccache cache,
		   krb5_cc_cursor *cursor, krb5_creds *creds)
{
	return pkrb5_cc_next_cred(context, cache, cursor, creds);
}

krb5_error_code 
afskfw_krb5_cc_end_seq_get (krb5_context context, krb5_ccache cache,
		     krb5_cc_cursor *cursor)
{
	return pkrb5_cc_end_seq_get(context, cache, cursor);
}

krb5_error_code 
afskfw_krb5_cc_remove_cred (krb5_context context, krb5_ccache cache, krb5_flags flags,
		     krb5_creds *creds)
{
	return pkrb5_cc_remove_cred(context, cache, flags, creds);
}

krb5_error_code 
afskfw_krb5_cc_set_flags (krb5_context context, krb5_ccache cache, krb5_flags flags)
{
	return pkrb5_cc_set_flags(context, cache, flags);
}

const char * 
afskfw_krb5_cc_get_type (krb5_context context, krb5_ccache cache)
{
	return pkrb5_cc_get_type(context, cache);
}

void 
afskfw_krb5_free_context
	(krb5_context context)
{
	pkrb5_free_context(context);
}

void  
afskfw_krb5_free_cred_contents
	(krb5_context context, krb5_creds * creds)
{
	pkrb5_free_cred_contents(context, creds);
}

void  
afskfw_krb5_free_principal
	(krb5_context context, krb5_principal princ)
{
	pkrb5_free_principal(context, princ);
}

krb5_error_code  
afskfw_krb5_get_in_tkt_with_password
	(krb5_context context, krb5_flags options,
			      krb5_address *const *addrs, krb5_enctype *ktypes,
			      krb5_preauthtype *pre_auth_types,
			      const char *password, krb5_ccache ccache,
			      krb5_creds *creds, krb5_kdc_rep **ret_as_reply)
{
	return pkrb5_get_in_tkt_with_password(context, options,
			      addrs, ktypes, pre_auth_types, password,
				  ccache, creds, ret_as_reply);
}

krb5_error_code  
afskfw_krb5_init_context
	(krb5_context * context)
{
	return pkrb5_init_context(context);
}

krb5_error_code  
afskfw_krb5_parse_name
	(krb5_context context,
		const char * name,
		krb5_principal * princ)
{
	return pkrb5_parse_name(context, name, princ);
}		

krb5_error_code  
afskfw_krb5_timeofday
	(krb5_context context, krb5_timestamp * timestamp)
{
	return pkrb5_timeofday(context, timestamp);
}

krb5_error_code 
afskfw_krb5_timestamp_to_sfstring
	(krb5_timestamp timestamp, char *buffer, size_t buflen, char *pad)
{
	return pkrb5_timestamp_to_sfstring
	(timestamp, buffer, buflen, pad);
}

krb5_error_code  
afskfw_krb5_unparse_name
	(krb5_context context,
		krb5_const_principal principal,
		char ** name)
{
	return pkrb5_unparse_name(context, principal, name);
}

krb5_error_code 
afskfw_krb5_get_credentials(krb5_context context, krb5_flags options,
		     krb5_ccache ccache, krb5_creds *in_creds,
		     krb5_creds **out_creds)
{
	return pkrb5_get_credentials(context,options,
		     ccache, in_creds, out_creds);
}

krb5_error_code 
afskfw_krb5_mk_req(krb5_context context, krb5_auth_context *auth_context,
	    krb5_flags ap_req_options, char *service, char *hostname,
	    krb5_data *in_data, krb5_ccache ccache, krb5_data *outbuf)
{
	return pkrb5_mk_req(context, auth_context, ap_req_options, 
		service, hostname, in_data, ccache, outbuf);
}

krb5_error_code 
afskfw_krb5_sname_to_principal(krb5_context context, const char *hostname, const char *sname, 
	krb5_int32 type, krb5_principal *ret_princ)
{
	return pkrb5_sname_to_principal(context, hostname, sname, 
		type, ret_princ);
}

krb5_error_code 
afskfw_krb5_get_credentials_renew(krb5_context context, krb5_flags options,
			   krb5_ccache ccache, krb5_creds *in_creds,
			   krb5_creds **out_creds)
{
	return pkrb5_get_credentials_renew(context, options, ccache,
			in_creds, out_creds);
}

void  
afskfw_krb5_free_data
	(krb5_context context, krb5_data * data)
{
	pkrb5_free_data
	(context, data);
}
	
void  
afskfw_krb5_free_data_contents
	(krb5_context context, krb5_data * data)
{
	pkrb5_free_data_contents
	(context, data);
}

void  
afskfw_krb5_free_unparsed_name
	(krb5_context context, char * name)
{
	pkrb5_free_unparsed_name(context, name);
}

krb5_error_code  
afskfw_krb5_os_localaddr
	(krb5_context context,
		krb5_address *** addresses)
{
	return pkrb5_os_localaddr
	(context, addresses);
}

krb5_error_code  
afskfw_krb5_copy_keyblock_contents
	(krb5_context context,
		const krb5_keyblock * from,
		krb5_keyblock * to)
{
	return pkrb5_copy_keyblock_contents(context, from, to);
}

krb5_error_code 
afskfw_krb5_copy_data(krb5_context context, const krb5_data *indata, krb5_data **outdata)
{
	return pkrb5_copy_data(context, indata, outdata);
}

void  
afskfw_krb5_free_creds
	(krb5_context context, krb5_creds *creds)
{
	pkrb5_free_creds(context, creds);
}

krb5_error_code 
afskfw_krb5_build_principal(krb5_context context,  krb5_principal * princ, 
		     unsigned int rlen,
		     const char * realm, ...)
{
	krb5_error_code code;
	
	va_list rest_args;
	va_start(rest_args, realm);
	code = pkrb5_build_principal
		(context, princ, rlen, realm, rest_args);
	va_end(rest_args);
	
	return code;
}

krb5_error_code 
afskfw_krb5_get_renewed_creds(krb5_context context, krb5_creds *creds, krb5_principal client,
	krb5_ccache ccache, char *in_tkt_service)
{
	return pkrb5_get_renewed_creds(context, creds, client, ccache, in_tkt_service);
}

krb5_error_code 
afskfw_krb5_get_default_config_files(char ***pfilenames)
{
	return pkrb5_get_default_config_files(pfilenames);
}

void  
afskfw_krb5_free_config_files
	(char **filenames)
{
	pkrb5_free_config_files
	(filenames);
}

krb5_error_code 
afskfw_krb5_get_default_realm(krb5_context context, char **lrealm)
{
	return pkrb5_get_default_realm(context, lrealm);
}

krb5_error_code
afskfw_krb5_set_default_realm
	(krb5_context context, const char * lrealm)
{
    return pkrb5_set_default_realm(context, lrealm);
}

void 
afskfw_krb5_free_default_realm(krb5_context context, char *lrealm)
{
	pkrb5_free_default_realm(context, lrealm);
}

void 
afskfw_krb5_free_ticket
	(krb5_context context, krb5_ticket * ticket)
{
	pkrb5_free_ticket
	(context, ticket);
}

krb5_error_code 
afskfw_krb5_decode_ticket
(const krb5_data *code,
		krb5_ticket **rep)
{
	return pkrb5_decode_ticket
		(code, rep);
}

krb5_error_code 
afskfw_krb5_get_host_realm(krb5_context context, const char *host, char ***realmsp)
{
	return pkrb5_get_host_realm(context, host, realmsp);
}

krb5_error_code 
afskfw_krb5_free_host_realm(krb5_context context, char *const *realmlist)
{
	return pkrb5_free_host_realm(context, realmlist);
}

void  
afskfw_krb5_free_addresses(krb5_context context, krb5_address ** addresses)
{
	 pkrb5_free_addresses
	(context, addresses);
}

krb5_error_code 
afskfw_krb5_c_random_make_octets
    (krb5_context context, krb5_data *data)
{
	return pkrb5_c_random_make_octets
    (context, data);
}

/* Added for rxk5 */

krb5_error_code 
afskfw_krb5_copy_addresses(krb5_context context, krb5_address *const *inaddr, krb5_address ***outaddr)
{
	return pkrb5_copy_addresses(context, inaddr, outaddr);
}

krb5_error_code 
afskfw_krb5_copy_principal(krb5_context context, krb5_const_principal inprinc, krb5_principal *outprinc)
{
	return pkrb5_copy_principal(context, inprinc, outprinc);
}

void  
afskfw_krb5_free_keyblock_contents
	(krb5_context context, krb5_keyblock * keyblock)
{
	pkrb5_free_keyblock_contents
	(context, keyblock);
}

void  
afskfw_krb5_free_checksum_contents
	(krb5_context context, krb5_checksum * cksum)
{
	pkrb5_free_checksum_contents
	(context, cksum);
}

krb5_error_code 
afskfw_krb5_c_block_size
    (krb5_context context, krb5_enctype enctype,
		    size_t *blocksize)
{
	return pkrb5_c_block_size
    (context, enctype, blocksize);
}

krb5_error_code 
afskfw_krb5_c_make_checksum
    (krb5_context context, krb5_cksumtype cksumtype,
		    const krb5_keyblock *key, krb5_keyusage usage,
		    const krb5_data *input, krb5_checksum *cksum)
{
	return pkrb5_c_make_checksum
    (context, cksumtype, key, usage, input, cksum);
}

krb5_error_code 
afskfw_krb5_c_verify_checksum
    (krb5_context context, 
		    const krb5_keyblock *key, krb5_keyusage usage,
		    const krb5_data *data,
		    const krb5_checksum *cksum,
		    krb5_boolean *valid)
{
	return pkrb5_c_verify_checksum
    (context, key, usage, data, cksum, valid);
}
 
krb5_error_code 
afskfw_krb5_c_checksum_length
    (krb5_context context, krb5_cksumtype cksumtype,
		    size_t *length)
{
	return pkrb5_c_checksum_length
    (context, cksumtype, length);
}

krb5_error_code 
afskfw_krb5_c_encrypt_length
    (krb5_context context, krb5_enctype enctype,
		    size_t inputlen, size_t *length)
{
	return pkrb5_c_encrypt_length
    (context, enctype, inputlen, length);
}

krb5_error_code 
afskfw_krb5_c_encrypt
    (krb5_context context, const krb5_keyblock *key,
		    krb5_keyusage usage, const krb5_data *cipher_state,
		    const krb5_data *input, krb5_enc_data *output)
{
	return pkrb5_c_encrypt
    (context, key, usage, cipher_state, input, output);
}

krb5_error_code 
afskfw_krb5_c_decrypt
    (krb5_context context, const krb5_keyblock *key,
		    krb5_keyusage usage, const krb5_data *cipher_state,
		    const krb5_enc_data *input, krb5_data *output)
{
	return pkrb5_c_decrypt
    (context, key, usage, cipher_state, input, output);
}

krb5_error_code 
afskfw_krb5_c_make_random_key
    (krb5_context context, krb5_enctype enctype,
		    krb5_keyblock *k5_random_key)
{
	return pkrb5_c_make_random_key
    (context, enctype, k5_random_key);
}

krb5_error_code 
afskfw_krb5_kt_get_entry(krb5_context context, krb5_keytab keytab,
		  krb5_const_principal principal, krb5_kvno vno,
		  krb5_enctype enctype, krb5_keytab_entry *entry)
{
	return pkrb5_kt_get_entry(context, keytab, principal, vno,
		  enctype, entry);
}
		  
krb5_error_code 
afskfw_krb5_kt_next_entry(krb5_context context, krb5_keytab keytab,
		   krb5_keytab_entry *entry, krb5_kt_cursor *cursor)
{
	return pkrb5_kt_next_entry(context, keytab,
		   entry, cursor);
}

krb5_error_code 
afskfw_krb5_kt_start_seq_get(krb5_context context, krb5_keytab keytab,
		      krb5_kt_cursor *cursor)
{
	return pkrb5_kt_start_seq_get(context, keytab, cursor);
}
			  		   
krb5_error_code 
afskfw_krb5_kt_end_seq_get(krb5_context context, krb5_keytab keytab,
		    krb5_kt_cursor *cursor)
{
	return pkrb5_kt_end_seq_get(context, keytab, cursor);
}
			
krb5_error_code 
afskfw_krb5_kt_close(krb5_context context, krb5_keytab keytab)
{
	return pkrb5_kt_close(context, keytab);	
}		

krb5_error_code 
afskfw_krb5_kt_resolve (krb5_context context, const char *name, krb5_keytab *ktid)
{
	return pkrb5_kt_resolve(context, name, ktid);
}

krb5_error_code  
afskfw_krb5_free_keytab_entry_contents
	(krb5_context context,
		krb5_keytab_entry * entry)
{
	return pkrb5_free_keytab_entry_contents
	(context, entry);
}

krb5_boolean  
afskfw_krb5_c_is_keyed_cksum
	(krb5_cksumtype ctype)
{
	return pkrb5_c_is_keyed_cksum
	(ctype);
}
		
krb5_boolean  
afskfw_krb5_c_is_coll_proof_cksum
	(krb5_cksumtype ctype)
{
	return pkrb5_c_is_coll_proof_cksum
	(ctype);
}

krb5_boolean afskfw_krb5_c_valid_enctype
    (krb5_enctype ktype)
{
    return pkrb5_c_valid_enctype
        (ktype);
}
	
krb5_boolean  
afskfw_krb5_c_valid_cksumtype
	(krb5_cksumtype ctype)
{
	return pkrb5_c_valid_cksumtype
	(ctype);
}

/* special rxk5 */

krb5_error_code 
afskfw_krb5_server_decrypt_ticket_keyblock(krb5_context context,
    krb5_keyblock *key, krb5_ticket *ticket) 
{
	return pkrb5_server_decrypt_ticket_keyblock(context, key, ticket);
}

krb5_error_code  
afskfw_krb5_encrypt_tkt_part
	(krb5_context context,
		const krb5_keyblock * keyblock,
		krb5_ticket * ticket)
{
	return pkrb5_encrypt_tkt_part(context, keyblock, ticket);
}

krb5_error_code  
afskfw_encode_krb5_ticket
	(const krb5_ticket *rep, krb5_data **code)
{
	return pencode_krb5_ticket(rep, code);

}

void afskfw_krb524_init_ets
	(krb5_context context)
{
	pkrb524_init_ets
	(context);
}

int  
afskfw_krb524_convert_creds_kdc
	(krb5_context context, krb5_creds *v5creds,
	 struct credentials *v4creds)
{
	return pkrb524_convert_creds_kdc
	(context, v5creds, v4creds);
}

int 
afskfw_krb_get_cred
(char *service, char *instance, char *realm, CREDENTIALS *c)
{
	return pkrb_get_cred
(service, instance, realm, c);
}

char *
afskfw_tkt_string()
{
	return ptkt_string();
}

int FAR
afskfw_krb_get_tf_realm(char* ticket_file, char* realm)
{
	return pkrb_get_tf_realm(ticket_file, realm);
}

int PASCAL 
afskfw_krb_mk_req(KTEXT authent, char *service, char *instance,
	char *realm, long checksum)
{
	return pkrb_mk_req(authent, service, instance,
		realm, checksum);
}

long  
afskfw_profile_init
	(const_profile_filespec_t *files, profile_t *ret_profile)
{
	return pprofile_init
	(files, ret_profile);
}

void  
afskfw_profile_release
	(profile_t profile)
{
	pprofile_release
	(profile);
}

long  
afskfw_profile_get_subsection_names
	(profile_t profile, const char **names, char ***ret_names)
{
	return pprofile_get_subsection_names
	(profile, names, ret_names);
}	

void  
afskfw_profile_free_list
	(char **list)
{
	pprofile_free_list
	(list);
}
					
long  
afskfw_profile_get_string
	(profile_t profile, const char *name, const char *subname, 
			const char *subsubname, const char *def_val,
			char **ret_string)
{
	return pprofile_get_string
	(profile, name, subname, 
			subsubname, def_val,
			ret_string);	
}
	
	
void  
afskfw_profile_release_string 
	(char *str)
{
	pprofile_release_string(str);
}

NTSTATUS NTAPI
afskfw_LsaConnectUntrusted (PHANDLE ph)
{
	return pLsaConnectUntrusted(ph);
}


NTSTATUS NTAPI
afskfw_LsaLookupAuthenticationPackage(HANDLE h, PLSA_STRING lstr, PULONG pl)
{
	return pLsaLookupAuthenticationPackage(h, lstr, pl);
}

NTSTATUS NTAPI
afskfw_LsaCallAuthenticationPackage
(HANDLE h, ULONG l1, PVOID pv1, ULONG l2, PVOID * ppv1, PULONG pul1, PNTSTATUS ps)
{
	return pLsaCallAuthenticationPackage
	(h, l1, pv1, l2, ppv1, pul1, ps);
}

NTSTATUS NTAPI
afskfw_LsaFreeReturnBuffer
(PVOID pv1)
{
	return pLsaFreeReturnBuffer(pv1);
}

ULONG NTAPI
afskfw_LsaGetLogonSessionData
    (PLUID plu1, PSECURITY_LOGON_SESSION_DATA* data)
{
	return pLsaGetLogonSessionData
    (plu1, data);
}

/* service functions */

BOOL 
afskfw_CloseServiceHandle(SC_HANDLE hSCObject)
{
    return pCloseServiceHandle(hSCObject);
}

SC_HANDLE 
afskfw_OpenSCManagerA(LPCTSTR lpMachineName, LPCTSTR lpDatabaseName,
                         DWORD dwDesiredAccess)
{
    return pOpenSCManagerA(lpMachineName, lpDatabaseName,
                           dwDesiredAccess);
}

SC_HANDLE 
afskfw_OpenServiceA(SC_HANDLE hSCManager, LPCTSTR lpServiceName, 
                    DWORD dwDesiredAccess)
{
    return pOpenServiceA(hSCManager, lpServiceName, 
                        dwDesiredAccess);
}

BOOL 
afskfw_QueryServiceStatus(SC_HANDLE hService, 
                          LPSERVICE_STATUS lpServiceStatus)
{
    return pQueryServiceStatus(hService, lpServiceStatus);
}

CCACHE_API CALLCONV_C
afskfw_cc_initialize
(apiCB** cc_ctx, cc_int32 api_version, cc_int32*  api_supported, 
    const char** vendor)
{
    return pcc_initialize(cc_ctx, api_version, api_supported, vendor);
}

CCACHE_API CALLCONV_C
afskfw_cc_shutdown(apiCB** cc_ctx)
{
    return pcc_shutdown(cc_ctx);
}

CCACHE_API CALLCONV_C
afskfw_cc_get_NC_info(apiCB* cc_ctx, struct _infoNC*** ppNCi)
{
    return pcc_get_NC_info(cc_ctx, ppNCi);
}

CCACHE_API CALLCONV_C
afskfw_cc_free_NC_info(apiCB* cc_ctx, struct _infoNC*** ppNCi)
{
    return pcc_free_NC_info(cc_ctx, ppNCi);
}

ULONG 
afskfw_LsaNtStatusToWinError(NTSTATUS Status)
{
    return pLsaNtStatusToWinError(Status);
}
