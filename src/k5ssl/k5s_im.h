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

struct _krb5_cc_ops {
    const char *prefix;
    krb5_error_code (*resolve)(krb5_context, krb5_ccache,
	const char *);
    krb5_error_code (*store_cred)(krb5_context context, krb5_ccache cache,
	krb5_creds *creds);
    krb5_error_code (*start_get)(krb5_context context,
	krb5_ccache cache, krb5_cc_cursor *cursor);
    krb5_error_code (*get_next)(krb5_context context,
	krb5_ccache cache, krb5_cc_cursor *cursor, krb5_creds *out_creds);
    krb5_error_code (*end_get)(krb5_context context,
	krb5_ccache cache, krb5_cc_cursor *cursor);
    krb5_error_code (*get_principal)(krb5_context context,
	krb5_ccache cache, krb5_principal *princ);
    krb5_error_code (*close)(krb5_context context, krb5_ccache cache);
    const char * (*get_name)(krb5_context context, krb5_ccache cache);
    krb5_error_code (*init)(krb5_context, krb5_ccache, krb5_principal);
};

struct _krb5_ccache {
    const krb5_cc_ops *ops;
    krb5_pointer data;
};

#define KRB5_CKSUMFLAG_NOT_COLL_PROOF 1
#define KRB5_CKSUMFLAG_DERIVE 2
#define KRB5_CKSUMFLAG_KEYED 4

struct krb5_cksumtypes {
    krb5_cksumtype cksumtype;
    int flags;
    krb5_enctype keyed_etype;
    const struct krb5_hash_provider {
	int (*hash_size)(unsigned int *output);
	int (*block_size)(unsigned int *output);
	int (*hash_init)(void **);
	int (*hash_setkey)(void *, krb5_context, const krb5_keyblock *, int,
	    unsigned char *);
	int (*hash_update)(void *state, const unsigned char *buf,
	    unsigned int len);
	int (*hash_digest)(void *state, unsigned char *output);
	int (*free_state)(void *state);
	int (*hash_setivec)(void *state, const unsigned char *buf,
	    unsigned int len);
    } *hash;
    int hash_trunc_size;
    char *names[4];
};
static const struct krb5_cksumtypes * find_cksumtype(krb5_cksumtype cksumtype);

struct krb5_enctypes {
    int enctype;
    const struct krb5_enc_provider {
	void (*block_size)(unsigned int *minlength, unsigned int *length);
	void (*key_size)(unsigned int *bytes, unsigned int *length);
	int (*make_key)(const krb5_data *, krb5_keyblock *, int);
	int (*encrypt)(const krb5_keyblock *, const krb5_data *ivec,
	    const krb5_data *input, krb5_data *output);
	int (*decrypt)(const krb5_keyblock *, const krb5_data *ivec,
	    const krb5_data *input, krb5_data *output);
    } *enc;
    const struct krb5_hash_provider *hash;
    int hash_trunc_size;
    int (*encrypt_len)(const struct krb5_enc_provider *,
	const struct krb5_hash_provider *,
	int,
	size_t, size_t *);
    int (*encrypt)(const struct krb5_enc_provider *,
	const struct krb5_hash_provider *,
	int,
	const krb5_keyblock *,
	krb5_keyusage,
	const krb5_data *,
	const krb5_data *,
	krb5_data *);
    int (*decrypt)(const struct krb5_enc_provider *,
	const struct krb5_hash_provider *,
	int,
	const krb5_keyblock *,
	krb5_keyusage,
	const krb5_data *,
	const krb5_data *,
	krb5_data *);
    char * names[4];
};

#define USE_TCP 3
#define TCP_ONLY 1
#define TCP_FIRST 2
#define USE_MASTER 4
krb5_error_code krb5i_sendto_kdc(krb5_context, const krb5_data *,
    const krb5_data *, krb5_data *, int *, struct timeval *);
krb5_error_code krb5i_locate_kdc(krb5_context, const krb5_data *,
    struct addrlist *, int);
krb5_error_code krb5i_locate_by_conf(krb5_context, const krb5_data *,
    struct addrlist *, int);
krb5_error_code krb5i_locate_by_dns(krb5_context, const krb5_data *,
    struct addrlist *, int);
int krb5i_config_lookup(krb5_profile_element **, const char *const*, int(*)(),
    void*);
int krb5i_add_name_value(krb5_profile_element **, const char *, const char *const*);

#if defined(KRB5_RC6_SUPPORT) || defined(KRB5_CAST_SUPPORT)
#define KRB5_EXTENDED_SALTS 1
#endif
