/*
 * Copyright (c) 2005, 2006
 * The Linux Box Corporation
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the Linux Box
 * Corporation is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * Linux Box Corporation is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the Linux Box Corporation as to its fitness for any
 * purpose, and without warranty by the Linux Box Corporation
 * of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the Linux Box Corporation shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */
 
#ifndef RXK5_NTFIXPROTOS_H
#define RXK5_NTFIXPROTOS_H

/* Preprocessor fixups which map krb5 function calls to indirect versions of
 * of these calls declared in afskfw.   In order for this to work, this header
 * must be included after krb5.h, and before any such calls are made. */

#ifdef AFS_NT40_ENV

#include <afs/afskfw_funcs.h>

/* these must match up with pkrb5* functions declared in dynimport.h */
#define krb5_change_password afskfw_krb5_change_password
#define krb5_get_init_creds_opt_init afskfw_krb5_get_init_creds_opt_init
#define krb5_get_init_creds_opt_set_tkt_life afskfw_krb5_get_init_creds_opt_set_tkt_life
#define krb5_get_init_creds_opt_set_renew_life afskfw_krb5_get_init_creds_opt_set_renew_life
#define krb5_get_init_creds_opt_set_forwardable afskfw_krb5_get_init_creds_opt_set_forwardable
#define krb5_get_init_creds_opt_set_proxiable afskfw_krb5_get_init_creds_opt_set_proxiable
#define krb5_get_init_creds_opt_set_renew_life afskfw_krb5_get_init_creds_opt_set_renew_life
#define krb5_get_init_creds_opt_set_address_list afskfw_krb5_get_init_creds_opt_set_address_list
#define krb5_get_init_creds_password afskfw_krb5_get_init_creds_password
#define krb5_get_prompt_types afskfw_krb5_get_prompt_types
#define krb5_build_principal_ext afskfw_krb5_build_principal_ext
#define krb5_cc_get_name afskfw_krb5_cc_get_name
#define krb5_cc_get_type afskfw_krb5_cc_get_type
#define krb5_cc_resolve afskfw_krb5_cc_resolve
#define krb5_cc_default afskfw_krb5_cc_default
#define krb5_cc_default_name afskfw_krb5_cc_default_name
#define krb5_cc_set_default_name afskfw_krb5_cc_set_default_name
#define krb5_cc_initialize afskfw_krb5_cc_initialize
#define krb5_cc_destroy afskfw_krb5_cc_destroy
#define krb5_cc_close afskfw_krb5_cc_close
#define krb5_cc_copy_creds afskfw_krb5_cc_copy_creds
#define krb5_cc_store_cred afskfw_krb5_cc_store_cred
#define krb5_cc_retrieve_cred afskfw_krb5_cc_retrieve_cred
#define krb5_cc_get_principal afskfw_krb5_cc_get_principal
#define krb5_cc_start_seq_get afskfw_krb5_cc_start_seq_get
#define krb5_cc_next_cred afskfw_krb5_cc_next_cred
#define krb5_cc_end_seq_get afskfw_krb5_cc_end_seq_get
#define krb5_cc_remove_cred afskfw_krb5_cc_remove_cred
#define krb5_cc_set_flags afskfw_krb5_cc_set_flags
#define krb5_cc_get_type afskfw_krb5_cc_get_type
#define krb5_free_context afskfw_krb5_free_context
#define krb5_free_cred_contents afskfw_krb5_free_cred_contents
#define krb5_free_principal afskfw_krb5_free_principal
#define krb5_get_in_tkt_with_password afskfw_krb5_get_in_tkt_with_password
#define krb5_init_context afskfw_krb5_init_context
#define krb5_parse_name afskfw_krb5_parse_name
#define krb5_timeofday afskfw_krb5_timeofday
#define krb5_timestamp_to_sfstring afskfw_krb5_timestamp_to_sfstring
#define krb5_unparse_name afskfw_krb5_unparse_name
#define krb5_get_credentials afskfw_krb5_get_credentials
#define krb5_mk_req afskfw_krb5_mk_req
#define krb5_sname_to_principal afskfw_krb5_sname_to_principal
#define krb5_get_credentials_renew afskfw_krb5_get_credentials_renew
#define krb5_free_data afskfw_krb5_free_data
#define krb5_free_data_contents afskfw_krb5_free_data_contents
#define krb5_get_realm_domain afskfw_krb5_get_realm_domain
#define krb5_free_unparsed_name afskfw_krb5_free_unparsed_name
#define krb5_os_localaddr afskfw_krb5_os_localaddr
#define krb5_copy_keyblock_contents afskfw_krb5_copy_keyblock_contents
#define krb5_copy_data afskfw_krb5_copy_data
#define krb5_free_creds afskfw_krb5_free_creds
#define krb5_build_principal afskfw_krb5_build_principal
#define krb5_get_renewed_creds afskfw_krb5_get_renewed_creds
#define krb5_free_addresses afskfw_krb5_free_addresses
#define krb5_get_default_config_files afskfw_krb5_get_default_config_files
#define krb5_free_config_files afskfw_krb5_free_config_files
#define krb5_get_default_realm afskfw_krb5_get_default_realm
#define krb5_set_default_realm afskfw_krb5_set_default_realm
#define krb5_free_ticket afskfw_krb5_free_ticket
#define krb5_decode_ticket afskfw_krb5_decode_ticket
#define krb5_get_host_realm afskfw_krb5_get_host_realm
#define krb5_free_host_realm afskfw_krb5_free_host_realm
#define krb5_c_random_make_octets afskfw_krb5_c_random_make_octets
#define krb5_free_default_realm afskfw_krb5_free_default_realm

/* Added for rxk5 */
#define krb5_copy_addresses afskfw_krb5_copy_addresses
#define krb5_copy_principal afskfw_krb5_copy_principal
#define krb5_free_keyblock_contents afskfw_krb5_free_keyblock_contents
#define krb5_free_checksum_contents afskfw_krb5_free_checksum_contents
#define krb5_c_block_size afskfw_krb5_c_block_size
#define krb5_c_make_checksum afskfw_krb5_c_make_checksum
#define krb5_c_verify_checksum afskfw_krb5_c_verify_checksum
#define krb5_c_checksum_length afskfw_krb5_c_checksum_length
#define krb5_c_is_keyed_cksum afskfw_krb5_c_is_keyed_cksum
#define krb5_c_is_coll_proof_cksum afskfw_krb5_c_is_coll_proof_cksum
#define krb5_c_valid_cksumtype afskfw_krb5_c_valid_cksumtype
#define krb5_c_valid_enctype afskfw_krb5_c_valid_enctype
#define krb5_c_encrypt_length afskfw_krb5_c_encrypt_length
#define krb5_c_encrypt afskfw_krb5_c_encrypt
#define krb5_c_decrypt afskfw_krb5_c_decrypt
#define krb5_c_make_random_key afskfw_krb5_c_make_random_key
#define krb5_kt_resolve afskfw_krb5_kt_resolve
#define krb5_kt_get_entry afskfw_krb5_kt_get_entry
#define krb5_kt_next_entry afskfw_krb5_kt_next_entry
#define krb5_kt_start_seq_get afskfw_krb5_kt_start_seq_get
#define krb5_kt_end_seq_get afskfw_krb5_kt_end_seq_get
#define krb5_kt_close afskfw_krb5_kt_close
#define krb5_free_keytab_entry_contents afskfw_krb5_free_keytab_entry_contents
#define krb5_encrypt_tkt_part afskfw_krb5_encrypt_tkt_part
#define encode_krb5_ticket afskfw_encode_krb5_ticket

/* Special */
#define krb5_server_decrypt_ticket_keyblock afskfw_krb5_server_decrypt_ticket_keyblock

#endif /* AFS_NT40_ENV */
#endif /* RXK5_NTFIXPROTOS_H */ 
