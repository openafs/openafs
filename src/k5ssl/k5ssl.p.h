/*
 * Copyright (c) 2005
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the University of Michigan as to its fitness for any
 * purpose, and without warranty by the University of
 * Michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the University of Michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#ifndef K5SSL_P_H
#define K5SSL_P_H

#define ENCTYPE_DES_CBC_CRC	1
#define ENCTYPE_DES_CBC_MD4	2
#define ENCTYPE_DES_CBC_MD5	3
/* ENCTYPE_DES_CBC_RAW	4 */
/* ENCTYPE_DES3_CBC_SHA 5 */
/* ENCTYPE_DES3_CBC_RAW 6 */
/* ENCTYPE_DES3_HMAC_SHA1 7 */
#define ENCTYPE_DES_HMAC_SHA1	8
#define ENCTYPE_DES3_CBC_SHA1	16
#define ENCTYPE_AES128_CTS_HMAC_SHA1_96	17
#define ENCTYPE_AES256_CTS_HMAC_SHA1_96	18
#define ENCTYPE_ARCFOUR_HMAC		23	/* MIT */
#define ENCTYPE_ARCFOUR_HMAC_MD5	23	/* HEIMDAL */
#define ENCTYPE_ARCFOUR_HMAC_EXP	24	/* MIT */
#define ENCTYPE_ARCFOUR_HMAC_56		24	/* HEIMDAL */
#define ENCTYPE_CAST_CBC_SHA	-2
#define ENCTYPE_RC6_CBC_SHA	-4

#define ENCTYPE_UNKNOWN		511

#define CKSUMTYPE_CRC32		1
#define CKSUMTYPE_RSA_MD4	2
#define CKSUMTYPE_RSA_MD4_DES	3
#define CKSUMTYPE_DESCBC	4	/* heimdal: CKSUMTYPE_DES_MAC */
/* CKSUMTYPE_DES_MAC_K	5 */
/* CKSUMTYPE_RSA_MD4_DES_K 6 */
#define CKSUMTYPE_RSA_MD5	7
#define CKSUMTYPE_RSA_MD5_DES	8
#define CKSUMTYPE_NIST_SHA	9
/* CKSUMTYPE_HMAC_SHA1	10 */
#define CKSUMTYPE_HMAC_SHA1_DES3 12
#define CKSUMTYPE_HMAC_SHA1_96_AES128 15
#define CKSUMTYPE_HMAC_SHA1_96_AES256 16
#define CKSUMTYPE_CASTCBC	-2
#define CKSUMTYPE_RC6CBC	-3
#define CKSUMTYPE_OLD_HMAC_SHA	-4	/* obselete, (was 10 once?) */
#define CKSUMTYPE_HMAC_MD5_ARCFOUR -138

typedef int krb5_enctype;
typedef int krb5_cksumtype;
typedef int krb5_keyusage;
typedef int krb5_boolean;
typedef int krb5_error_code;
typedef unsigned int krb5_kvno;
typedef int krb5_int32;
typedef unsigned int krb5_ui_4;
typedef unsigned int krb5_msgtype;
typedef int krb5_preauthtype;

typedef struct _krb5_context * krb5_context;
typedef struct _krb5_data {
	unsigned int length;
	unsigned char *data;
} krb5_data;
typedef struct _krb5_keyblock {
	krb5_enctype enctype;
	unsigned int length;
	unsigned char *contents;
} krb5_keyblock;

typedef struct _krb5_checksum {
	krb5_cksumtype checksum_type;
	unsigned int length;
	unsigned char *contents;
} krb5_checksum;

typedef struct _krb5_enc_data {
	krb5_enctype enctype;
	krb5_kvno kvno;
	krb5_data ciphertext;
} krb5_enc_data;

krb5_error_code krb5_c_make_checksum(krb5_context context,
	krb5_cksumtype cksumtype,
	const krb5_keyblock *key,
	krb5_keyusage usage,
	const krb5_data *input,
	krb5_checksum *cksum);
krb5_error_code krb5_c_verify_checksum(krb5_context context,
	const krb5_keyblock *key,
	krb5_keyusage usage,
	const krb5_data *data,
	krb5_checksum *cksum,
	krb5_boolean *valid);
krb5_boolean krb5_c_is_keyed_cksum(krb5_cksumtype cksumtype);
krb5_boolean krb5_c_is_coll_proof_cksum(krb5_cksumtype cksumtype);
krb5_boolean krb5_c_valid_cksumtype(krb5_cksumtype cksumtype);  
krb5_error_code krb5_c_checksum_length(krb5_context context,
	krb5_cksumtype cksumtype, size_t *length);
krb5_error_code krb5_init_context(krb5_context *);
krb5_error_code krb5_c_make_random_key(krb5_context,krb5_enctype,krb5_keyblock *);
krb5_error_code krb5_c_random_make_octets(krb5_context, krb5_data *);
krb5_error_code krb5_c_random_os_entropy(krb5_context, int, int *);
krb5_error_code krb5_c_random_add_entropy(krb5_context, unsigned int, const krb5_data *);
#define KRB5_C_RANDSOURCE_OLDAPI 0
#define KRB5_C_RANDSOURCE_OSRAND 1
#define KRB5_C_RANDSOURCE_TRUSTEDPARTY 2
#define KRB5_C_RANDSOURCE_TIMING 3
#define KRB5_C_RANDSOURCE_EXTERNAL_PROTOCOL 4

krb5_error_code krb5_c_block_size(krb5_context,krb5_enctype,size_t *blocksize);
krb5_error_code krb5_c_decrypt(krb5_context,const krb5_keyblock *,
	krb5_keyusage,const krb5_data *,const krb5_enc_data *,krb5_data*);
krb5_error_code krb5_c_encrypt(krb5_context,const krb5_keyblock *,
	krb5_keyusage,const krb5_data *,const krb5_data *,krb5_enc_data*);
krb5_error_code krb5_c_encrypt_length(krb5_context,krb5_enctype,
	size_t, size_t *);
krb5_boolean krb5_c_valid_enctype(krb5_enctype);
krb5_error_code krb5_c_keyed_checksum_types(krb5_context, krb5_enctype,
	unsigned int *, krb5_cksumtype **);
void krb5_free_cksumtypes(krb5_context, krb5_cksumtype *);

krb5_error_code krb5_string_to_enctype(char *, krb5_enctype *);
krb5_error_code krb5_string_to_cksumtype(char *, krb5_cksumtype *);
krb5_error_code krb5i_iterate_enctypes(int (*)(void *, krb5_enctype,
	char *const *,
	void (*)(unsigned int *, unsigned int *),
	void (*)(unsigned int *, unsigned int *)),
    void *);

#define KRB5_KEYUSAGE_AS_REQ_PA_ENC_TS	1
#define KRB5_KEYUSAGE_KDC_REP_TICKET	2
#define KRB5_KEYUSAGE_AS_REP_ENCPART	3
#define KRB5_KEYUSAGE_TGS_REQ_AD_SESSKEY	4
#define KRB5_KEYUSAGE_TGS_REQ_AUTH_CKSUM	6
#define KRB5_KEYUSAGE_TGS_REQ_AUTH		7
#define KRB5_KEYUSAGE_TGS_REP_ENCPART_SESSKEY	8
#define KRB5_KEYUSAGE_AP_REQ_AUTH_CKSUM		10
#define KRB5_KEYUSAGE_AP_REQ_AUTH		11

#define KRB5_NT_UNKNOWN		0
#define KRB5_NT_PRINCIPAL	1

#define KRB5_DOMAIN_X500_COMPRESS	1

#define KRB5_TGS_NAME	"krbtgt"
#define KRB5_TGS_NAME_SIZE	6
#define KRB5_REALM_BRANCH_CHAR	'.'

#define KRB5_GC_CACHED	2

#define KRB5_TC_MATCH_SRV_NAMEONLY	0x40
#define KRB5_TC_MATCH_KTYPE		0x100
#define KRB5_TC_SUPPORTED_KTYPES	0x200

#define KDC_OPT_VALIDATE	0x1
#define KDC_OPT_RENEW		0x2
#define KDC_OPT_ENC_TKT_IN_SKEY	0x8
#define KDC_OPT_RENEWABLE_OK	0x10
#define KDC_OPT_RENEWABLE	0x800000
#define KDC_OPT_POSTDATED	0x2000000
#define KDC_OPT_ALLOW_POSTDATE	0x4000000
#define KDC_OPT_PROXIABLE	0x10000000
#define KDC_OPT_FORWARDABLE	0x40000000
#define KDC_TKT_COMMON_MASK (KDC_OPT_RENEWABLE|KDC_OPT_ALLOW_POSTDATE| \
	KDC_OPT_PROXIABLE|KDC_OPT_FORWARDABLE)
#define KRB5_AS_REP	11
#define KRB5_TGS_REP	13
#define KRB5_ENC_AS_REP_PART	25
#define KRB5_ENC_TGS_REP_PART	26
#define KRB5_ERROR	30

#define	TKT_FLG_INITIAL			0x400000
#define	TKT_FLG_INVALID			0x1000000

#define KRB5_PADATA_PW_TYPE_AND_SALT	-1
#define KRB5_PADATA_NONE 0
#define KRB5_PADATA_AP_REQ	1
#define KRB5_PADATA_ENC_TIMESTAMP	2
#define KRB5_PADATA_PW_SALT	3
#define KRB5_PADATA_AFS3_SALT	10
#define KRB5_PADATA_ETYPE_INFO	11
#define KRB5_PADATA_ETYPE_INFO2	19

typedef void *krb5_pointer;
typedef krb5_pointer krb5_cc_cursor;
typedef int krb5_timestamp, krb5_flags;

typedef struct _krb5_principal_data {
	krb5_data realm, *data;
	int length, type;
} krb5_principal_data, *krb5_principal;
typedef const krb5_principal_data *krb5_const_principal;

typedef struct _krb5_ticket_times {
    krb5_timestamp authtime, starttime, endtime, renew_till;
} krb5_ticket_times;

typedef struct _krb5_address {
    unsigned short addrtype;
    unsigned int length;
    unsigned char *contents;
} krb5_address;

typedef struct _krb5_authdata {
    unsigned short ad_type;
    unsigned int length;
    unsigned char *contents;
} krb5_authdata;

typedef struct _krb5_salt {
    krb5_data s2kdata, s2kparams;
} krb5_salt;

typedef struct _krb5_creds {
    krb5_principal client, server;
    krb5_keyblock keyblock;
    krb5_ticket_times times;
    krb5_boolean is_skey;
    krb5_flags ticket_flags;
    krb5_address **addresses;
    krb5_data ticket, second_ticket;
    krb5_authdata **authdata;
} krb5_creds;

typedef struct _krb5_transited {
    int tr_type;
    krb5_data tr_contents;
} krb5_transited;

typedef struct _krb5_enc_tkt_part {
    krb5_flags flags;
    krb5_keyblock *session;
    krb5_principal client;
    krb5_transited transited;
    krb5_ticket_times times;
    krb5_address **caddrs;
    krb5_authdata **authorization_data;
} krb5_enc_tkt_part;

typedef struct _krb5_ticket {
	krb5_principal server;
	krb5_enc_data enc_part;
	krb5_enc_tkt_part *enc_part2;
} krb5_ticket;

typedef struct _krb5_pa_data {
    krb5_preauthtype pa_type;
    unsigned int length;
    unsigned char *contents;
} krb5_pa_data;

typedef struct _krb5_authenticator {
    krb5_principal client;
    krb5_checksum *checksum;
    krb5_int32 cusec, seq_number;
    krb5_timestamp ctime;
    krb5_keyblock *subkey;
    krb5_authdata **authorization_data;
} krb5_authenticator;

typedef struct _krb5_kdc_req {
    krb5_msgtype msg_type;
    krb5_pa_data **padata;
    krb5_flags kdc_options;
    krb5_principal client,server;
    krb5_timestamp from,till,rtime;
    krb5_int32 nonce;
    int nktypes;
    krb5_enctype *ktype;
    krb5_address **addresses;
    krb5_enc_data authorization_data;
    krb5_authdata **unenc_authdata;
    krb5_ticket **second_ticket;
} krb5_kdc_req;

typedef struct _krb5_last_req_entry {
    krb5_int32 lr_type;
    krb5_timestamp value;
} krb5_last_req_entry;

typedef struct _krb5_enc_kdc_rep_part {
    krb5_msgtype msg_type;
    krb5_keyblock *session;
    krb5_last_req_entry **last_req;
    krb5_int32 nonce;
    krb5_timestamp key_exp;
    krb5_flags flags;
    krb5_ticket_times times;
    krb5_principal server;
    krb5_address **caddrs;
} krb5_enc_kdc_rep_part;

typedef struct _krb5_kdc_rep {
    krb5_msgtype msg_type;
    krb5_pa_data **padata;
    krb5_principal client;
    krb5_ticket *ticket;
    krb5_enc_data enc_part;
    krb5_enc_kdc_rep_part *enc_part2;
} krb5_kdc_rep;

typedef struct _krb5_error {
    /* krb5_msgtype msg_type; */
    krb5_timestamp ctime, stime;
    krb5_int32 cusec, susec;
    krb5_ui_4 error;
    krb5_principal client, server;
    krb5_data text, e_data;
} krb5_error;

typedef struct _krb5_response {
    int message_type;
    krb5_data response;
    krb5_int32 expected_nonce;
    krb5_timestamp request_time;
} krb5_response;

typedef struct _krb5_ap_req {
    krb5_flags ap_options;
    krb5_ticket *ticket;
    krb5_enc_data authenticator;
} krb5_ap_req;

typedef struct _krb5_cc_ops krb5_cc_ops;
typedef struct _krb5_ccache *krb5_ccache;

extern const krb5_cc_ops krb5_cc_file_ops;

typedef struct _krb5_keytab_entry {
	krb5_principal principal;
	time_t timestamp;
	int vno;
	krb5_keyblock key;
} krb5_keytab_entry;
typedef struct _krb5_keytab * krb5_keytab;
typedef krb5_pointer krb5_kt_cursor;
typedef struct _krb5_kt_ops {
    const char *prefix;
    int (*resolve)(krb5_context, krb5_keytab, const char *);
    int (*start_seq_get)(krb5_context, krb5_keytab,
	krb5_kt_cursor *cursor);
    int (*get_next)(krb5_context, krb5_keytab,
	krb5_keytab_entry *, krb5_kt_cursor *);
    int (*end_seq_get)(krb5_context, krb5_keytab,
	krb5_kt_cursor *cursor);
    int (*close)(krb5_context, krb5_keytab);
} krb5_kt_ops;
extern const krb5_kt_ops krb5_ktf_ops;

extern struct krb5_kt_type {
    struct krb5_kt_type *next;
    struct _krb5_kt_ops *ops;
} *krb5i_kt_typelist;

krb5_error_code krb5_parse_name(krb5_context, const char *, krb5_principal *);
krb5_error_code krb5_unparse_name(krb5_context, krb5_const_principal, char **);
void krb5_free_principal(krb5_context, krb5_principal);
void krb5_free_addresses(krb5_context, krb5_address **);
void krb5_free_authdata(krb5_context, krb5_authdata **);
void krb5_free_cred_contents(krb5_context, krb5_creds *);
void krb5_free_keyblock_contents(krb5_context, krb5_keyblock *);
void krb5_free_keytab_entry_contents(krb5_context, krb5_keytab_entry *);
void krb5_free_keyblock(krb5_context, krb5_keyblock *);
void krb5_free_ticket(krb5_context, krb5_ticket *);
void krb5_free_enc_tkt_part(krb5_context, krb5_enc_tkt_part *);
void krb5_free_creds(krb5_context, krb5_creds *);
void krb5_free_checksum_contents(krb5_context, krb5_checksum *);
void krb5_free_context(krb5_context);

krb5_error_code krb5_copy_keyblock_contents(krb5_context,
    const krb5_keyblock *, krb5_keyblock *);

krb5_boolean krb5_principal_compare(krb5_context, krb5_const_principal,
    krb5_const_principal);
krb5_error_code krb5_get_tgs_ktypes(krb5_context, krb5_const_principal,
    krb5_enctype **);
krb5_error_code krb5_get_tkt_ktypes(krb5_context, krb5_const_principal,
    krb5_enctype **);
krb5_error_code krb5_kt_resolve(krb5_context, const char *, krb5_keytab *);
krb5_error_code krb5_kt_get_entry(krb5_context, krb5_keytab,
    krb5_const_principal, krb5_kvno, krb5_enctype, krb5_keytab_entry *);
krb5_error_code krb5_kt_start_seq_get(krb5_context, krb5_keytab,
    krb5_kt_cursor *);
krb5_error_code krb5_kt_next_entry(krb5_context, krb5_keytab,
    krb5_keytab_entry *, krb5_kt_cursor *);
krb5_error_code krb5_kt_end_seq_get(krb5_context, krb5_keytab,
    krb5_kt_cursor *);
krb5_error_code krb5_kt_close(krb5_context, krb5_keytab);
krb5_error_code krb5_kt_register(krb5_context, const krb5_kt_ops *);
krb5_error_code krb5_ktfile_wresolve(krb5_context, const char *, krb5_keytab *);
krb5_error_code krb5_kt_add_entry(krb5_context, krb5_keytab,
    krb5_keytab_entry *);

krb5_error_code krb5_cc_register(krb5_context, krb5_cc_ops *, krb5_boolean);
krb5_error_code krb5_cc_resolve(krb5_context, const char *, krb5_ccache *);
krb5_error_code krb5_cc_get_principal(krb5_context, krb5_ccache,
    krb5_principal *);
krb5_error_code krb5_cc_start_seq_get(krb5_context, krb5_ccache,
    krb5_cc_cursor *);
krb5_error_code krb5_cc_next_cred(krb5_context, krb5_ccache, krb5_cc_cursor *,
    krb5_creds *);
krb5_error_code krb5_cc_end_seq_get(krb5_context, krb5_ccache,
    krb5_cc_cursor *);
krb5_error_code krb5_cc_retrieve_cred(krb5_context, krb5_ccache,
    krb5_flags, krb5_creds *, krb5_creds *);
krb5_error_code krb5_cc_store_cred(krb5_context, krb5_ccache, krb5_creds *);
krb5_error_code krb5_cc_close (krb5_context, krb5_ccache);

krb5_error_code krb5_decode_ticket(const krb5_data *, krb5_ticket **);
krb5_error_code decode_krb5_enc_tkt_part(krb5_data *, krb5_enc_tkt_part **);
krb5_error_code krb5_get_credentials(krb5_context, const krb5_flags,
    krb5_ccache, krb5_creds *, krb5_creds **);
krb5_error_code krb5_decrypt_tkt_part(krb5_context, const krb5_keyblock *,
    krb5_ticket *);
krb5_error_code krb5_encrypt_tkt_part(krb5_context, const krb5_keyblock *,
    krb5_ticket *);
krb5_error_code krb5_get_default_realm(krb5_context, char **);
const char *krb5_cc_default_name(krb5_context);
krb5_error_code krb5_cc_default(krb5_context, krb5_ccache *);
krb5_error_code krb5_kt_default_name(krb5_context, char *, int);
krb5_error_code krb5_kt_default(krb5_context, krb5_keytab *);

#define PROFILE_GO_AWAY	2048
typedef struct _krb5_profile_element *krb5_profile, krb5_profile_element;
int krb5i_data_compare(const krb5_data *, const krb5_data *);
int krb5i_find_names(krb5_profile, const char *const *, char ***);
int krb5i_visit_nodes(krb5_profile, int (*f)(), void *);
void krb5i_free_profile(krb5_profile);
int krb5i_parse_profile(char *, krb5_profile *);
time_t krb5i_timegm();
krb5_error_code krb5i_config_get_strings(krb5_context, const char * const *,
    char ***);
int krb5i_issuid(void);
unsigned char * krb5i_nfold(unsigned char *in, int inlen,
    unsigned char *out, int outlen);

void krb5_free_realm_tree(krb5_context, krb5_principal *);
krb5_error_code krb5_walk_realm_tree(krb5_context, const krb5_data *,
    const krb5_data *, krb5_principal **, int);
krb5_error_code krb5i_check_transited_list(krb5_context, const krb5_transited *,
    const krb5_data *, const krb5_data *);
krb5_error_code krb5i_domain_x500_decode(krb5_context, const krb5_data *,
    const krb5_data *, const krb5_data *, krb5_data **, int *);

krb5_error_code krb5_get_cred_from_kdc(krb5_context, krb5_ccache,
    krb5_creds *, krb5_creds**, krb5_creds ***);
krb5_error_code krb5_get_cred_from_kdc_opt(krb5_context, krb5_ccache,
    krb5_creds *, krb5_creds**, krb5_creds ***, krb5_flags);
krb5_error_code krb5_get_cred_via_tkt(krb5_context, krb5_creds *,
    const krb5_flags, krb5_address * const *, krb5_creds *, krb5_creds **);
krb5_error_code krb5_get_clockskew(krb5_context, int *);
krb5_error_code krb5_get_kdc_req_checksum_type(krb5_context, krb5_cksumtype *);
krb5_error_code krb5_get_ap_req_checksum_type(krb5_context, krb5_cksumtype *);
krb5_error_code krb5_get_udp_preference_limit(krb5_context, int *);
krb5_error_code krb5_get_dns_fallback(krb5_context, char *, int *);
#if 0
krb5_error_code krb5_send_tgs(krb5_context, const krb5_flags,
    const krb5_ticket_times *, const krb5_enctype *, krb5_const_principal,
    krb5_address * const *, const krb5_authdata **, krb5_pa_data * const *,
    const krb5_data *, krb5_creds *, krb5_response *);
#endif
krb5_error_code krb5_copy_principal(krb5_context, krb5_const_principal,
    krb5_principal *);
krb5_error_code krb5_copy_addresses(krb5_context, krb5_address *const *,
    krb5_address ***);

struct addrlist;
void krb5i_free_addrlist(struct addrlist *);

void krb5_free_pa_data(krb5_context, krb5_pa_data **);
void krb5_free_last_req(krb5_context, krb5_last_req_entry **);
void krb5_free_enc_kdc_rep_part(krb5_context, krb5_enc_kdc_rep_part *);
void krb5_free_error(krb5_context, krb5_error *);
void krb5_free_kdc_rep(krb5_context, krb5_kdc_rep *);
int krb5_is_tgs_rep(krb5_data *);
int krb5_is_krb_error(krb5_data *);
krb5_error_code krb5_decode_kdc_rep(krb5_context,krb5_data *,
    const krb5_keyblock *,krb5_kdc_rep **);
krb5_error_code encode_krb5_ticket(const krb5_ticket *, krb5_data **);
krb5_error_code decode_krb5_error(const krb5_data *, krb5_error **);
#if 0
#define C const
#else
#define C /**/
#endif
krb5_error_code encode_krb5_authdata(C krb5_authdata **, krb5_data **);
krb5_error_code encode_krb5_authenticator(const krb5_authenticator *,
    krb5_data **);
krb5_error_code encode_krb5_ap_req(const krb5_ap_req *, krb5_data **);
krb5_error_code encode_krb5_tgs_req(krb5_kdc_req *, krb5_data **);
krb5_error_code encode_krb5_kdc_req_body(krb5_kdc_req *, krb5_data **);

krb5_error_code encode_krb5_enc_tkt_part(const krb5_enc_tkt_part *,
    krb5_data **);

krb5_error_code krb5_free_host_realm(krb5_context, char * const *);
krb5_error_code krb5_get_host_realm(krb5_context, const char *, char ***);
#ifdef va_end	/* cheezy and probably a bad idea... */
krb5_error_code krb5_build_principal_va(krb5_context, krb5_principal *,
    int, char const *, va_list);
#endif
krb5_error_code krb5_build_principal(krb5_context, krb5_principal *, int,
    char const *, ...);
krb5_error_code krb5_kt_read_service_key(krb5_context, krb5_pointer,
    krb5_principal, krb5_kvno, krb5_enctype, krb5_keyblock **);
krb5_error_code krb5_server_decrypt_ticket_keyblock(krb5_context,
    krb5_keyblock *, krb5_ticket *);

#ifdef ERR_LIB_NONE
void * krb5i_skip_tag(void **, const unsigned char **, long);
krb5_principal krb5i_d2i_PRINCIPAL_NAME (krb5_principal*,
    const unsigned char **, long);
krb5_ticket * krb5i_d2i_TICKET (krb5_ticket **, const unsigned char **, long);
krb5_enc_data * krb5i_d2i_ENC_DATA (krb5_enc_data **, const unsigned char **,
    long);
void krb5i_SEQ_AUTHDATA_free(krb5_authdata **);
krb5_authdata ** krb5i_d2i_SEQ_AUTHDATA (krb5_authdata***,
    const unsigned char **, long);
krb5_keyblock * krb5i_d2i_ENCRYPTION_KEY (krb5_keyblock **,
    const unsigned char **, long);
time_t krb5i_ASN1_KERBEROS_TIME_get(const ASN1_GENERALIZEDTIME *);
int krb5i_BIT_STRING_get(const ASN1_BIT_STRING *);
krb5_address ** krb5i_d2i_SEQ_HOSTADDRESS (krb5_address***,
    const unsigned char **, long);
int krb5i_i2d_ASN1_BIT_STRING(ASN1_BIT_STRING *, unsigned char **);
#endif

typedef krb5_int32 krb5_deltat;
#define KRB5_GET_INIT_CREDS_OPT_TKT_LIFE	1
#define KRB5_GET_INIT_CREDS_OPT_RENEW_LIFE	2
#define KRB5_GET_INIT_CREDS_OPT_FORWARDABLE	4
#define KRB5_GET_INIT_CREDS_OPT_PROXIABLE	8
#define KRB5_GET_INIT_CREDS_OPT_ETYPE_LIST	16
#define KRB5_GET_INIT_CREDS_OPT_ADDRESS_LIST	32
#define KRB5_GET_INIT_CREDS_OPT_PREAUTH_LIST	64
typedef struct _krb5_get_init_creds_opt {
    krb5_flags flags;
    krb5_deltat tkt_life, renew_life;
    int forwardable, proxiable;
    krb5_enctype *etype_list;
    int etype_list_length;
    krb5_address **address_list;
    krb5_preauthtype *preauth_list;
    int preauth_list_length;
    krb5_salt *salt;
} krb5_get_init_creds_opt;
#define KRB5_PROMPT_TYPE_PASSWORD	1
#define KRB5_PROMPT_TYPE_NEW_PASSWORD	2
#define KRB5_PROMPT_TYPE_NEW_PASSWORD_AGAIN 3
#define KRB5_PROMPT_TYPE_PREAUTH	4
typedef krb5_int32 krb5_prompt_type;
typedef struct _krb5_prompt {
    char *prompt;
    krb5_data *reply;
    int hidden;
    krb5_prompt_type type;
} krb5_prompt;
typedef krb5_error_code krb5_prompter_fct_(krb5_context, void *,
    const char *, const char *, int, krb5_prompt[]);
typedef krb5_prompter_fct_ *krb5_prompter_fct;
krb5_prompter_fct_ krb5_prompter_posix;

krb5_error_code krb5_string_to_deltat(char *, krb5_deltat *);
krb5_error_code krb5_os_localaddr(krb5_context, krb5_address ***);
krb5_error_code krb5_copy_authdata(krb5_context, krb5_authdata *const*,
    krb5_authdata ***);
krb5_error_code krb5_copy_creds(krb5_context, const krb5_creds *,
    krb5_creds **);
const char *krb5_cc_get_name(krb5_context, krb5_ccache);
krb5_error_code krb5_cc_initialize(krb5_context, krb5_ccache, krb5_principal);
krb5_error_code krb5_set_default_realm(krb5_context, const char *);
void krb5_get_init_creds_opt_init(krb5_get_init_creds_opt *);
void krb5_get_init_creds_opt_set_address_list(
    krb5_get_init_creds_opt *, krb5_address **);
void krb5_get_init_creds_opt_set_forwardable(
    krb5_get_init_creds_opt *, int);
void krb5_get_init_creds_opt_set_proxiable(
    krb5_get_init_creds_opt *, int);
void krb5_get_init_creds_opt_set_renew_life(
    krb5_get_init_creds_opt *, krb5_deltat);
void krb5_get_init_creds_opt_set_tkt_life(
    krb5_get_init_creds_opt *, krb5_deltat);
void krb5_get_init_creds_opt_set_etype_list(
    krb5_get_init_creds_opt *, krb5_enctype *, int);
krb5_error_code krb5_get_init_creds_password(krb5_context, krb5_creds *,
    krb5_principal, char *, krb5_prompter_fct, void *, krb5_deltat, char *,
    krb5_get_init_creds_opt *);
krb5_error_code krb5_get_init_creds_keytab(krb5_context, krb5_creds *,
    krb5_principal, krb5_keytab, krb5_deltat, char *,
    krb5_get_init_creds_opt *);
krb5_error_code krb5_get_kdc_default_options(krb5_context, int *);

krb5_error_code krb5_get_validated_creds(krb5_context, krb5_creds *,
    krb5_principal, krb5_ccache, char *);
krb5_error_code krb5_get_renewed_creds(krb5_context, krb5_creds *,
    krb5_principal, krb5_ccache, char *);
krb5_error_code krb5_principal2salt(krb5_context, krb5_const_principal,
    krb5_salt *);
void krb5_free_salt_contents(krb5_context, krb5_salt *);

typedef struct _krb5_etype_info_entry {
    krb5_enctype etype;
    unsigned int length;
    unsigned char *salt;
    krb5_data s2kparams;
} krb5_etype_info_entry, **krb5_etype_info;
void krb5_free_etype_info(krb5_context, krb5_etype_info);

/**********************************************/
#define krb5_princ_realm(context, princ)        (&(princ)->realm)
#define krb5_princ_size(context, princ) ((princ)->length)
#define krb5_princ_name(context, princ) ((princ)->data)
#define krb5_princ_type(context, princ) ((princ)->type)
#define krb5_princ_component(context, princ, i) ((princ)->data+(i))

typedef struct _krb5_pa_enc_ts {
    unsigned int patimestamp;
    int pausec;
} krb5_pa_enc_ts;

krb5_error_code encode_krb5_pa_enc_ts(const krb5_pa_enc_ts *, krb5_data **);
krb5_error_code encode_krb5_enc_data(const krb5_enc_data *, krb5_data **);

#endif /* K5SSL_P_H */
