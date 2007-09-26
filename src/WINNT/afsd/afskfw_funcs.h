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

#ifndef AFSKFW_FUNCS_H
#define AFSKFW_FUNCS_H

#define USE_MS2MIT
#undef  USE_KRB4

#include <windows.h>
#ifdef USE_MS2MIT
#define SECURITY_WIN32
#include <security.h>
#include <ntsecapi.h>
#endif /* USE_MS2MIT */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>


#include <afs/stds.h>
#include <krb5.h>

#ifdef AFSKFW_KRBIV
/* Defined in the KRBV4W32 version of krb.h but not the Kerberos V version */
/* Required for some of the loadfuncs headers */
typedef struct ktext far *KTEXT;
typedef struct ktext far *KTEXT_FP;
#include <KerberosIV/krb.h>
#endif

/* ms security api */
#define CC_API_VER_1 1
#define CC_API_VER_2 2

#define CCACHE_API cc_int32
#define CALLCONV_C KRB5_CALLCONV

typedef int cc_int32;

void
KFW_initialize_funcs(void);

int
KFW_available_funcs(void);

int
KFW_funcs_cc_initialize_loaded(void);

void
KFW_cleanup_funcs(void);

/* Stub functions */

/* leash */

DWORD afskfw_Leash_get_default_lifetime();
DWORD afskfw_Leash_get_default_forwardable();
DWORD afskfw_Leash_get_default_renew_till();
DWORD afskfw_Leash_get_default_noaddresses();
DWORD afskfw_Leash_get_default_proxiable();
DWORD afskfw_Leash_get_default_publicip();
DWORD afskfw_Leash_get_default_use_krb4();
DWORD afskfw_Leash_get_default_life_min();
DWORD afskfw_Leash_get_default_life_max();
DWORD afskfw_Leash_get_default_renew_min();
DWORD afskfw_Leash_get_default_renew_max();
DWORD afskfw_Leash_get_default_renewable();
DWORD afskfw_Leash_get_default_mslsa_import();

/* krb5 */

krb5_error_code 
afskfw_krb5_change_password
	(krb5_context context, krb5_creds *creds, char *newpw,
			int *result_code, krb5_data *result_code_string,
			krb5_data *result_string);
void 
afskfw_krb5_get_init_creds_opt_init
(krb5_get_init_creds_opt *opt);
void 
afskfw_krb5_get_init_creds_opt_set_tkt_life
(krb5_get_init_creds_opt *opt,
		krb5_deltat tkt_life);
void 
afskfw_krb5_get_init_creds_opt_set_renew_life
(krb5_get_init_creds_opt *opt,
		krb5_deltat renew_life);
void 
afskfw_krb5_get_init_creds_opt_set_forwardable
(krb5_get_init_creds_opt *opt,
		int forwardable);
void 
afskfw_krb5_get_init_creds_opt_set_proxiable
(krb5_get_init_creds_opt *opt,
		int proxiable);
void 
afskfw_krb5_get_init_creds_opt_set_address_list
(krb5_get_init_creds_opt *opt,
		krb5_address **addresses);		
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
		krb5_get_init_creds_opt *k5_gic_options);
krb5_error_code  
afskfw_krb5_build_principal_ext
(krb5_context context,  krb5_principal * princ,
			 unsigned int rlen, const char * realm, ...);
const char * 
afskfw_krb5_cc_get_name (krb5_context context, krb5_ccache cache);
krb5_error_code  
afskfw_krb5_cc_resolve
	(krb5_context context, const char *name, krb5_ccache *cache);		
const char *  
afskfw_krb5_cc_default_name
	(krb5_context context);	
krb5_error_code  
afskfw_krb5_cc_set_default_name
	(krb5_context context, const char *ccname);
krb5_error_code  
afskfw_krb5_cc_default
	(krb5_context context, krb5_ccache *ccache);
krb5_error_code 
afskfw_krb5_cc_initialize(krb5_context context, krb5_ccache cache,
		   krb5_principal principal);
krb5_error_code 
afskfw_krb5_cc_destroy (krb5_context context, krb5_ccache cache);
krb5_error_code 
afskfw_krb5_cc_close (krb5_context context, krb5_ccache cache);
krb5_error_code 
afskfw_krb5_cc_store_cred (krb5_context context, krb5_ccache cache,
		    krb5_creds *creds);
krb5_error_code 
afskfw_krb5_cc_copy_creds(krb5_context context, krb5_ccache incc, 
                          krb5_ccache outcc);
krb5_error_code 
afskfw_krb5_cc_retrieve_cred (krb5_context context, krb5_ccache cache,
		       krb5_flags flags, krb5_creds *mcreds,
		       krb5_creds *creds);
krb5_error_code 
afskfw_krb5_cc_get_principal (krb5_context context, krb5_ccache cache,
		       krb5_principal *principal);
krb5_error_code 
afskfw_krb5_cc_start_seq_get (krb5_context context, krb5_ccache cache,
		       krb5_cc_cursor *cursor);
krb5_error_code 
afskfw_krb5_cc_next_cred (krb5_context context, krb5_ccache cache,
		   krb5_cc_cursor *cursor, krb5_creds *creds);
krb5_error_code 
afskfw_krb5_cc_end_seq_get (krb5_context context, krb5_ccache cache,
		     krb5_cc_cursor *cursor);
krb5_error_code 
afskfw_krb5_cc_remove_cred (krb5_context context, krb5_ccache cache, krb5_flags flags,
		     krb5_creds *creds);
krb5_error_code 
afskfw_krb5_cc_set_flags (krb5_context context, krb5_ccache cache, krb5_flags flags);
const char * 
afskfw_krb5_cc_get_type (krb5_context context, krb5_ccache cache);
void  
afskfw_krb5_free_context
	(krb5_context context);
void  
afskfw_krb5_free_cred_contents
	(krb5_context context, krb5_creds * creds);
void  
afskfw_krb5_free_principal
	(krb5_context context, krb5_principal princ);
krb5_error_code  
afskfw_krb5_get_in_tkt_with_password
	(krb5_context context, krb5_flags options,
			      krb5_address *const *addrs, krb5_enctype *ktypes,
			      krb5_preauthtype *pre_auth_types,
			      const char *password, krb5_ccache ccache,
			      krb5_creds *creds, krb5_kdc_rep **ret_as_reply);
krb5_error_code  
afskfw_krb5_init_context
	(krb5_context * context);
krb5_error_code  
afskfw_krb5_parse_name
	(krb5_context context,
		const char * name,
		krb5_principal * princ);
krb5_error_code  
afskfw_krb5_timeofday
	(krb5_context context, krb5_timestamp * timestamp);
krb5_error_code 
afskfw_krb5_timestamp_to_sfstring
	(krb5_timestamp timestamp, char *buffer, size_t buflen, char *pad);
krb5_error_code  
afskfw_krb5_unparse_name
	(krb5_context context,
		krb5_const_principal principal,
		char ** name);
krb5_error_code 
afskfw_krb5_get_credentials(krb5_context context, krb5_flags options,
		     krb5_ccache ccache, krb5_creds *in_creds,
		     krb5_creds **out_creds);
krb5_error_code 
afsfkw_krb5_mk_req(krb5_context context, krb5_auth_context *auth_context,
	    krb5_flags ap_req_options, char *service, char *hostname,
	    krb5_data *in_data, krb5_ccache ccache, krb5_data *outbuf);
krb5_error_code 
afskfw_krb5_sname_to_principal(krb5_context context, const char *hostname, const char *sname, 
	krb5_int32 type, krb5_principal *ret_princ);
krb5_error_code 
afskfw_krb5_get_credentials_renew(krb5_context context, krb5_flags options,
			   krb5_ccache ccache, krb5_creds *in_creds,
			   krb5_creds **out_creds);
void  
afskfw_krb5_free_data
	(krb5_context context, krb5_data * data);	
void  
afskfw_krb5_free_data_contents
	(krb5_context context, krb5_data * data);
void  
afskfw_krb5_free_unparsed_name
	(krb5_context context, char * name);
krb5_error_code  
afskfw_krb5_os_localaddr
	(krb5_context context,
		krb5_address *** addresses);
krb5_error_code  
afskfw_krb5_copy_keyblock_contents
	(krb5_context context,
		const krb5_keyblock * from,
		krb5_keyblock * to);
krb5_error_code 
afskfw_krb5_copy_data(krb5_context context, const krb5_data *indata, krb5_data **outdata);
void  
afskfw_krb5_free_creds
	(krb5_context context, krb5_creds *creds);
krb5_error_code 
afskfw_krb5_build_principal(krb5_context context,  krb5_principal * princ, 
		     unsigned int rlen,
		     const char * realm, ...);
krb5_error_code 
afskfw_krb5_get_renewed_creds(krb5_context context, krb5_creds *creds, krb5_principal client,
	krb5_ccache ccache, char *in_tkt_service);
krb5_error_code 
afskfw_krb5_get_default_config_files(char ***pfilenames);
void  
afskfw_krb5_free_config_files
	(char **filenames);
krb5_error_code 
afskfw_krb5_get_default_realm(krb5_context context, char **lrealm);
krb5_error_code
afskfw_krb5_set_default_realm
	(krb5_context, const char * );
void 
afskfw_krb5_free_default_realm(krb5_context context, char *lrealm);
void 
afskfw_krb5_free_ticket
	(krb5_context context, krb5_ticket * ticket);
krb5_error_code 
afskfw_krb5_decode_ticket
(const krb5_data *code,
		krb5_ticket **rep);
krb5_error_code 
afskfw_krb5_get_host_realm(krb5_context context, const char *host, char ***realmsp);
krb5_error_code 
afskfw_krb5_free_host_realm(krb5_context context, char *const *realmlist);
void  
afskfw_krb5_free_addresses(krb5_context context, krb5_address ** addresses);
krb5_error_code 
afskfw_krb5_c_random_make_octets
    (krb5_context context, krb5_data *data);
	
/* Added for rxk5 */

krb5_error_code 
afskfw_krb5_copy_addresses(krb5_context context, krb5_address *const *inaddr, 
	krb5_address ***outaddr);
krb5_error_code 
afskfw_krb5_copy_principal(krb5_context context, krb5_const_principal inprinc, 
	krb5_principal *outprinc);
void  
afskfw_krb5_free_keyblock_contents
	(krb5_context context, krb5_keyblock * keyblock);
void  
afskfw_krb5_free_checksum_contents
	(krb5_context context, krb5_checksum * cksum);
krb5_error_code 
afskfw_krb5_c_block_size
    (krb5_context context, krb5_enctype enctype,
		    size_t *blocksize);
krb5_error_code 
afskfw_krb5_c_make_checksum
    (krb5_context context, krb5_cksumtype cksumtype,
		    const krb5_keyblock *key, krb5_keyusage usage,
		    const krb5_data *input, krb5_checksum *cksum);
krb5_error_code 
afskfw_krb5_c_verify_checksum
    (krb5_context context, 
		    const krb5_keyblock *key, krb5_keyusage usage,
		    const krb5_data *data,
		    const krb5_checksum *cksum,
		    krb5_boolean *valid); 
krb5_error_code 
afskfw_krb5_c_checksum_length
    (krb5_context context, krb5_cksumtype cksumtype,
		    size_t *length);
krb5_error_code 
afskfw_krb5_c_encrypt_length
    (krb5_context context, krb5_enctype enctype,
		    size_t inputlen, size_t *length);
krb5_error_code 
afskfw_krb5_c_encrypt
    (krb5_context context, const krb5_keyblock *key,
		    krb5_keyusage usage, const krb5_data *cipher_state,
		    const krb5_data *input, krb5_enc_data *output);
krb5_error_code 
afskfw_krb5_c_decrypt
    (krb5_context context, const krb5_keyblock *key,
		    krb5_keyusage usage, const krb5_data *cipher_state,
		    const krb5_enc_data *input, krb5_data *output);
krb5_error_code 
afskfw_krb5_c_make_random_key
    (krb5_context context, krb5_enctype enctype,
		    krb5_keyblock *k5_random_key);
krb5_error_code 
afskfw_krb5_kt_get_entry(krb5_context context, krb5_keytab keytab,
		  krb5_const_principal principal, krb5_kvno vno,
		  krb5_enctype enctype, krb5_keytab_entry *entry);		  
krb5_error_code 
afskfw_krb5_kt_next_entry(krb5_context context, krb5_keytab keytab,
		   krb5_keytab_entry *entry, krb5_kt_cursor *cursor);
krb5_error_code 
afskfw_krb5_kt_start_seq_get(krb5_context context, krb5_keytab keytab,
		      krb5_kt_cursor *cursor);			  		   
krb5_error_code 
afskfw_krb5_kt_end_seq_get(krb5_context context, krb5_keytab keytab,
		    krb5_kt_cursor *cursor);			
krb5_error_code 
afskfw_krb5_kt_close(krb5_context context, krb5_keytab keytab);
krb5_error_code 
afskfw_krb5_kt_resolve (krb5_context context, const char *name, 
	krb5_keytab *ktid);
krb5_error_code  
afskfw_krb5_free_keytab_entry_contents
	(krb5_context context,
		krb5_keytab_entry * entry);
krb5_boolean  
afskfw_krb5_c_is_keyed_cksum
	(krb5_cksumtype ctype);		
krb5_boolean  
afskfw_krb5_c_is_coll_proof_cksum
	(krb5_cksumtype ctype);	
krb5_boolean afskfw_krb5_c_valid_enctype
    (krb5_enctype ktype);
krb5_boolean  
afskfw_krb5_c_valid_cksumtype
	(krb5_cksumtype ctype);

/* special rxk5 */

krb5_error_code 
afskfw_krb5_server_decrypt_ticket_keyblock(krb5_context context,
    krb5_keyblock *key, krb5_ticket *ticket);
krb5_error_code  
afskfw_krb5_encrypt_tkt_part
	(krb5_context context,
		const krb5_keyblock * keyblock,
		krb5_ticket * ticket);
krb5_error_code  
afskfw_encode_krb5_ticket
	(const krb5_ticket *rep, krb5_data **code);
void afskfw_krb524_init_ets
	(krb5_context context);
int  
afskfw_krb524_convert_creds_kdc
	(krb5_context context, krb5_creds *v5creds,
	 struct credentials *v4creds);
	 
#ifdef AFSKFW_KRBIV	 
int 
afskfw_krb_get_cred
(char *service, char *instance, char *realm, CREDENTIALS *c);
char *
afskfw_tkt_string();
int FAR
afwkfw_krb_get_tf_realm(char* ticket_file, char* realm);
int PASCAL 
afskfw_krb_mk_req(KTEXT authent, char *service, char *instance,
	char *realm, long checksum);	
long  
afskfw_profile_init
	(const_profile_filespec_t *files, profile_t *ret_profile);
void  
afskfw_profile_release
	(profile_t profile);
long  
afskfw_profile_get_subsection_names
	(profile_t profile, const char **names, char ***ret_names);
void  
afskfw_profile_free_list
	(char **list);					
long  
afskfw_profile_get_string
	(profile_t profile, const char *name, const char *subname, 
			const char *subsubname, const char *def_val,
			char **ret_string);	
void  
afskfw_profile_release_string 
	(char *str);

#endif	/* AFSKFW_KRBIV */
	
#ifdef AFSKFW_LSA	
/* mslsa */
	
NTSTATUS NTAPI
afskfw_LsaConnectUntrusted (PHANDLE ph);
NTSTATUS NTAPI
afskfw_LsaLookupAuthenticationPackage(HANDLE h, PLSA_STRING lstr, PULONG pl);
NTSTATUS NTAPI
afskfw_LsaCallAuthenticationPackage
(HANDLE h, ULONG l1, PVOID pv1, ULONG l2, PVOID * ppv1, PULONG pul1, PNTSTATUS ps);
NTSTATUS NTAPI
afskfw_LsaFreeReturnBuffer
(PVOID pv1);

ULONG NTAPI
LsaGetLogonSessionData
    (PLUID plu1, PSECURITY_LOGON_SESSION_DATA* data);
	
ULONG 
afskfw_LsaNtStatusToWinError(NTSTATUS Status);	
	
#endif /* AFSKFW_LSA */

#ifdef AFSKFW_SVC
/* service functions */

BOOL 
afskfw_CloseServiceHandle(SC_HANDLE hSCObject);

SC_HANDLE 
afskfw_OpenSCManagerA(LPCTSTR lpMachineName, LPCTSTR lpDatabaseName,
                      DWORD dwDesiredAccess);

SC_HANDLE 
afskfw_OpenServiceA(SC_HANDLE hSCManager, LPCTSTR lpServiceName, 
                    DWORD dwDesiredAccess);

BOOL 
afskfw_QueryServiceStatus(SC_HANDLE hService, 
                          LPSERVICE_STATUS lpServiceStatus);
						  
#endif /* AFSKFW_SVC */

#ifdef AFSKFW_CC

CCACHE_API CALLCONV_C
afskfw_cc_initialize
(apiCB** cc_ctx, cc_int32 api_version, cc_int32*  api_supported, 
 const char** vendor);

CCACHE_API CALLCONV_C
afskfw_cc_shutdown(apiCB** cc_ctx);

CCACHE_API CALLCONV_C
afskfw_cc_get_NC_info(apiCB* cc_ctx, struct _infoNC*** ppNCi);

CCACHE_API CALLCONV_C
afskfw_cc_free_NC_info(apiCB* cc_ctx, struct _infoNC*** ppNCi);


#endif /* AFSKFW_CC */
	
#endif AFSKFW_FUNCS_H
