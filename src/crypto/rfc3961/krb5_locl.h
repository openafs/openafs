/* This is a shim header that's included by crypto.c, and turns it into
 * something that we can actually build on its own.
 */

#ifdef KERNEL

#include "config.h"
#include <roken.h>

#else
#include <afsconfig.h>
#include <afs/stds.h>
#include <roken.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#ifndef AFS_NT40_ENV
#include <sys/param.h>
#include <inttypes.h>
#include <sys/errno.h>
#endif
#include <sys/types.h>

#endif

#include <hcrypto/evp.h>
#include <hcrypto/des.h>
#include <hcrypto/rc4.h>
#include <hcrypto/sha.h>
#include <hcrypto/md5.h>

#include "rfc3961.h"

#ifndef KERNEL
#ifdef AFS_PTHREAD_ENV
#include <pthread.h>
# define HEIMDAL_MUTEX pthread_mutex_t
# define HEIMDAL_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
# define HEIMDAL_MUTEX_init(m) pthread_mutex_init(m, NULL)
# define HEIMDAL_MUTEX_lock(m) pthread_mutex_lock(m)
# define HEIMDAL_MUTEX_unlock(m) pthread_mutex_unlock(m)
# define HEIMDAL_MUTEX_destroy(m) pthread_mutex_destroy(m)
#else
/* The one location in this library which uses mutexes is the PRNG
 * code. As this code takes no locks, never yields, and does no
 * I/O through the LWP IO Manager, it cannot be pre-empted, so
 * it is safe to simply remove the locks in this case
 */
#define HEIMDAL_MUTEX int
#define HEIMDAL_MUTEX_INITIALIZER 0
#define HEIMDAL_MUTEX_init(m) do { (void)(m); } while(0)
#define HEIMDAL_MUTEX_lock(m) do { (void)(m); } while(0)
#define HEIMDAL_MUTEX_unlock(m) do { (void)(m); } while(0)
#define HEIMDAL_MUTEX_destroy(m) do { (void)(m); } while(0)
#endif
#endif

#define HEIMDAL_SMALLER 1
#define HEIM_CRYPTO_NO_TRIPLE_DES
#define HEIM_CRYPTO_NO_ARCFOUR
#define HEIM_CRYPTO_NO_PK

#define NO_RAND_EGD_METHOD
#define NO_RANDFILE

#define ALLOC(X, N) (X) = calloc((N), sizeof(*(X)))

#ifndef max
#define max(a,b) (((a)>(b))?(a):(b))
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

typedef int krb5_boolean;
typedef ssize_t krb5_ssize_t;

#define KRB5_KU_AS_REP_ENC_PART 3
#define KRB5_KU_USAGE_SEAL 22
#define KRB5_KU_USAGE_SIGN 23
#define KRB5_KU_USAGE_SEQ 24

#define TRUE 1
#define FALSE 0

/* From the ASN.1 */

typedef struct EncryptedData {
  int etype;
  int *kvno;
  afs_heim_octet_string cipher;
} EncryptedData;

typedef enum krb5_salttype {
    KRB5_PW_SALT = 3,
    KRB5_AFS3_SALT = 10
} krb5_salttype;

/*
 * Ew, gross!
 *
 * crypto.c from heimdal has hard-coded references to the symbol
 * KEYTYPE_ARCFOUR and the enum krb5_keytype.  This enum is effectively
 * deprecated, with comments like "Deprecated: keytypes doesn't exists, they
 * are really enctypes" appearing near public APIs that handle krb5_keytypes.
 *
 * In particular, the internal "type" information about the struct
 * _krb5_key_type is just a krb5_enctype, and the accessor to retrieve the
 * alleged krb5_keytype value just returns this "type" (of type krb5_enctype)
 * with an "XXX" comment but no cast.  Since krb5_keytype is otherwise unused
 * for OpenAFS and we are not constrained to provide ABI compatible functions
 * that expose the krb5_keytype type in the way that Heimdal is, just alias
 * the deprecated krb5_keytype type to the underlying krb5_enctype to silence
 * compiler warnings about implicit conversion between different enumeration
 * types.
 *
 * The actual enum values are used in one place in the file, to check whether
 * a checksum is an arcfour checksum, so provide an anonymous enum to alias
 * that one consumer as well.
 */

typedef krb5_enctype krb5_keytype;

enum {
  KEYTYPE_ARCFOUR = ETYPE_ARCFOUR_HMAC_MD5
};

#define KRB5_ENCTYPE_NULL ETYPE_NULL
#define KRB5_ENCTYPE_OLD_DES3_CBC_SHA1 ETYPE_OLD_DES3_CBC_SHA1
#define KRB5_ENCTYPE_AES128_CTS_HMAC_SHA1_96 ETYPE_AES128_CTS_HMAC_SHA1_96
#define KRB5_ENCTYPE_AES256_CTS_HMAC_SHA1_96 ETYPE_AES256_CTS_HMAC_SHA1_96
#define KRB5_ENCTYPE_ARCFOUR_HMAC_MD5 ETYPE_ARCFOUR_HMAC_MD5

typedef struct krb5_salt {
    krb5_salttype salttype;
    krb5_data saltvalue;
} krb5_salt;

typedef struct krb5_crypto_iov {
    unsigned int flags;
    /* ignored */
#define KRB5_CRYPTO_TYPE_EMPTY          0
    /* OUT krb5_crypto_length(KRB5_CRYPTO_TYPE_HEADER) */
#define KRB5_CRYPTO_TYPE_HEADER         1
    /* IN and OUT */
#define KRB5_CRYPTO_TYPE_DATA           2
    /* IN */
#define KRB5_CRYPTO_TYPE_SIGN_ONLY      3
   /* (only for encryption) OUT krb5_crypto_length(KRB5_CRYPTO_TYPE_TRAILER) */
#define KRB5_CRYPTO_TYPE_PADDING        4
   /* OUT krb5_crypto_length(KRB5_CRYPTO_TYPE_TRAILER) */
#define KRB5_CRYPTO_TYPE_TRAILER        5
   /* OUT krb5_crypto_length(KRB5_CRYPTO_TYPE_CHECKSUM) */
#define KRB5_CRYPTO_TYPE_CHECKSUM       6
    krb5_data data;
} krb5_crypto_iov;

#define ETYPE_NULL 0

#define KRB5_LIB_FUNCTION
#define KRB5_LIB_CALL

/* Error codes */
#define KRB5_BAD_MSIZE -1765328194
#define KRB5_BAD_KEYSIZE -1765328195
#define KRB5_PROG_SUMTYPE_NOSUPP -1765328231
#define KRB5_PROG_KEYTYPE_NOSUPP -1765328233
#define KRB5_PROG_ETYPE_NOSUPP -1765328234
#define HEIM_ERR_SALTTYPE_NOSUPP -1980176638
#define KRB5KRB_AP_ERR_BAD_INTEGRITY -1765328353

#define KRB5_CRYPTO_INTERNAL 1

/* Currently, we just disable localised error strings. We'll get the error
 * numbers out, but no meaningful text */
#define N_(X, Y) X

/* rename internal symbols, to reduce conflicts with external kerberos
   libraries */
#define krb5_abortx _oafs_h_krb5_abortx
#define krb5_set_error_message _oafs_h_krb5_set_error_message
#define copy_EncryptionKey _oafs_h_copy_EncryptionKey
#define der_copy_octet_string _oafs_h_der_copy_octet_string
#define _krb5_HMAC_MD5_checksum _oafs_h__krb5_HMAC_MD5_checksum
#define _krb5_usage2arcfour _oafs_h__krb5_usage2arcfour
#define _krb5_SP_HMAC_SHA1_checksum _oafs_h__krb5_SP_HMAC_SHA1_checksum
#define _krb5_derive_key _oafs_h__krb5_derive_key
#define _krb5_find_checksum _oafs_h__krb5_find_checksum
#define _krb5_find_enctype _oafs_h__krb5_find_enctype
#define _krb5_free_key_data _oafs_h__krb5_free_key_data
#define _krb5_internal_hmac _oafs_h__krb5_internal_hmac
#define krb5_allow_weak_crypto _oafs_h_krb5_allow_weak_crypto
#define krb5_checksum_disable _oafs_h_krb5_checksum_disable
#define krb5_checksum_is_collision_proof _oafs_h_krb5_checksum_is_collision_proof
#define krb5_checksum_is_keyed _oafs_h_krb5_checksum_is_keyed
#define _krb5_checksum_hmac_md5 _oafs_h__krb5_checksum_hmac_md5
#define _krb5_checksum_hmac_sha1_des3 _oafs_h__krb5_checksum_hmac_sha1_des3
#define _krb5_checksum_rsa_md5 _oafs_h__krb5_checksum_rsa_md5
#define _krb5_checksum_sha1 _oafs_h__krb5_checksum_sha1
#define _krb5_checksum_sha1_des3 _oafs_h__krb5_checksum_sha1_des3
#define krb5_cksumtype_to_enctype _oafs_h_krb5_cksumtype_to_enctype
#define krb5_cksumtype_valid _oafs_h_krb5_cksumtype_valid
#define krb5_create_checksum_iov _oafs_h_krb5_create_checksum_iov
#define krb5_crypto_getblocksize _oafs_h_krb5_crypto_getblocksize
#define krb5_crypto_getconfoundersize _oafs_h_krb5_crypto_getconfoundersize
#define krb5_crypto_getenctype _oafs_h_krb5_crypto_getenctype
#define krb5_crypto_getpadsize _oafs_h_krb5_crypto_getpadsize
#define krb5_crypto_length _oafs_h_krb5_crypto_length
#define krb5_crypto_length_iov _oafs_h_krb5_crypto_length_iov
#define krb5_decrypt_EncryptedData _oafs_h_krb5_decrypt_EncryptedData
#define krb5_decrypt_iov_ivec _oafs_h_krb5_decrypt_iov_ivec
#define krb5_decrypt_ivec _oafs_h_krb5_decrypt_ivec
#define krb5_derive_key _oafs_h_krb5_derive_key
#define krb5_encrypt_EncryptedData _oafs_h_krb5_encrypt_EncryptedData
#define krb5_encrypt_iov_ivec _oafs_h_krb5_encrypt_iov_ivec
#define krb5_encrypt_ivec _oafs_h_krb5_encrypt_ivec
#define _krb5_enctype_des3_cbc_none _oafs_h__krb5_enctype_des3_cbc_none
#define _krb5_enctype_des3_cbc_sha1 _oafs_h__krb5_enctype_des3_cbc_sha1
#define _krb5_enctype_arcfour_hmac_md5 _oafs_h__krb5_enctype_arcfour_hmac_md5
#define krb5_enctype_disable _oafs_h_krb5_enctype_disable
#define krb5_enctype_enable _oafs_h_krb5_enctype_enable
#define krb5_enctype_to_keytype _oafs_h_krb5_enctype_to_keytype
#define krb5_enctype_to_string _oafs_h_krb5_enctype_to_string
#define krb5_generate_random_keyblock _oafs_h_krb5_generate_random_keyblock
#define krb5_get_wrapped_length _oafs_h_krb5_get_wrapped_length
#define krb5_hmac _oafs_h_krb5_hmac
#define krb5_is_enctype_weak _oafs_h_krb5_is_enctype_weak
#define krb5_string_to_enctype _oafs_h_krb5_string_to_enctype
#define krb5_verify_checksum_iov _oafs_h_krb5_verify_checksum_iov
#define _krb5_DES3_random_to_key _oafs_h__krb5_DES3_random_to_key
#define _krb5_xor _oafs_h__krb5_xor
#define _krb5_evp_cleanup _oafs_h__krb5_evp_cleanup
#define _krb5_evp_encrypt _oafs_h__krb5_evp_encrypt
#define _krb5_evp_encrypt_cts _oafs_h__krb5_evp_encrypt_cts
#define _krb5_evp_schedule _oafs_h__krb5_evp_schedule
#define krb5_copy_data _oafs_h_krb5_copy_data
#define krb5_data_cmp _oafs_h_krb5_data_cmp
#define krb5_data_copy _oafs_h_krb5_data_copy
#define krb5_data_ct_cmp _oafs_h_krb5_data_ct_cmp
#define krb5_data_realloc _oafs_h_krb5_data_realloc
#define krb5_data_zero _oafs_h_krb5_data_zero
#define krb5_free_data _oafs_h_krb5_free_data
#define _krb5_n_fold _oafs_h__krb5_n_fold
#define _krb5_get_int _oafs_h__krb5_get_int
#define _krb5_put_int _oafs_h__krb5_put_int


/* These have to be real functions, because IRIX doesn't seem to support
 * variadic macros */
void krb5_set_error_message(krb5_context, krb5_error_code, const char *, ...);
krb5_error_code krb5_abortx(krb5_context, const char *, ...);

#define krb5_clear_error_message(ctx)

static_inline krb5_error_code
krb5_enomem(krb5_context context)
{
    return ENOMEM;
}


/* Local prototypes. These are functions that we aren't admitting to in the
 * public API */
krb5_error_code _krb5_n_fold(const void *str, size_t len, void *, size_t);
krb5_error_code krb5_derive_key(krb5_context context, const krb5_keyblock *key,
				krb5_enctype etype, const void *constant,
				size_t constant_len,
				krb5_keyblock **derived_key);
krb5_ssize_t _krb5_put_int(void *buffer, unsigned long value, size_t size);
void krb5_data_zero(krb5_data *p);
krb5_error_code krb5_data_copy(krb5_data *p, const void *data, size_t len);
void krb5_free_data(krb5_context context, krb5_data *p);
krb5_error_code krb5_copy_keyblock(krb5_context,
				   const krb5_keyblock *,
				   krb5_keyblock **);
void krb5_free_keyblock(krb5_context, krb5_keyblock *);
int krb5_data_ct_cmp(const krb5_data *, const krb5_data *);
int der_copy_octet_string(const krb5_data *, krb5_data *);
int copy_EncryptionKey(const krb5_keyblock *, krb5_keyblock *);
krb5_error_code krb5_enctype_to_string(krb5_context context,
				       krb5_enctype etype,
				       char **string);

/*
 * Unused prototypes from heimdal/krb5.  These are functions that are not used
 * outside of their compilation unit at all, but we may need to declare them to
 * avoid compiler warnings.
 */
struct _krb5_key_data;
struct _krb5_encryption_type;
/* heimdal/krb5/crypto.c */
KRB5_LIB_FUNCTION krb5_error_code _krb5_derive_key(krb5_context context,
						   struct _krb5_encryption_type *et,
						   struct _krb5_key_data *key,
						   const void *constant,
						   size_t len);
KRB5_LIB_FUNCTION krb5_error_code krb5_allow_weak_crypto(krb5_context context,
							 krb5_boolean enable);
KRB5_LIB_FUNCTION krb5_error_code krb5_checksum_disable(krb5_context context,
							krb5_cksumtype type);
KRB5_LIB_FUNCTION krb5_boolean krb5_checksum_is_keyed(krb5_context context,
						      krb5_cksumtype type);
KRB5_LIB_FUNCTION krb5_error_code krb5_cksumtype_valid(krb5_context context,
						       krb5_cksumtype ctype);
KRB5_LIB_FUNCTION krb5_error_code krb5_create_checksum_iov(krb5_context context,
							   krb5_crypto crypto,
							   unsigned usage,
							   krb5_crypto_iov *data,
							   unsigned int num_data,
							   krb5_cksumtype *type);
KRB5_LIB_FUNCTION krb5_error_code krb5_crypto_getblocksize(krb5_context context,
							   krb5_crypto crypto,
							   size_t *blocksize);
KRB5_LIB_FUNCTION krb5_error_code krb5_crypto_getenctype(krb5_context context,
							 krb5_crypto crypto,
							 krb5_enctype *enctype);
KRB5_LIB_FUNCTION krb5_error_code krb5_crypto_getpadsize(krb5_context context,
							 krb5_crypto crypto,
							 size_t *padsize);
KRB5_LIB_FUNCTION krb5_error_code krb5_crypto_length(krb5_context context,
						     krb5_crypto crypto,
						     int type,
						     size_t *len);
KRB5_LIB_FUNCTION krb5_error_code krb5_crypto_length_iov(krb5_context context,
							 krb5_crypto crypto,
							 krb5_crypto_iov *data,
							 unsigned int num_data);
KRB5_LIB_FUNCTION krb5_error_code krb5_decrypt_iov_ivec(krb5_context context,
							krb5_crypto crypto,
							unsigned usage,
							krb5_crypto_iov *data,
							unsigned int num_data,
							void *ivec);
KRB5_LIB_FUNCTION krb5_error_code krb5_decrypt_ivec(krb5_context context,
						    krb5_crypto crypto,
						    unsigned usage,
						    void *data,
						    size_t len,
						    krb5_data *result,
						    void *ivec);
KRB5_LIB_FUNCTION krb5_error_code krb5_encrypt_iov_ivec(krb5_context context,
							krb5_crypto crypto,
							unsigned usage,
							krb5_crypto_iov *data,
							int num_data,
							void *ivec);
KRB5_LIB_FUNCTION krb5_error_code krb5_encrypt_ivec(krb5_context context,
						    krb5_crypto crypto,
						    unsigned usage,
						    const void *data,
						    size_t len,
						    krb5_data *result,
						    void *ivec);
KRB5_LIB_FUNCTION krb5_error_code krb5_enctype_disable(krb5_context context,
						       krb5_enctype enctype);
KRB5_LIB_FUNCTION krb5_error_code krb5_enctype_enable(krb5_context context,
						      krb5_enctype enctype);
KRB5_LIB_FUNCTION krb5_error_code krb5_enctype_to_keytype(krb5_context context,
							  krb5_enctype etype,
							  krb5_keytype *keytype);
KRB5_LIB_FUNCTION size_t krb5_get_wrapped_length (krb5_context context,
						  krb5_crypto  crypto,
						  size_t       data_len);
KRB5_LIB_FUNCTION krb5_error_code krb5_hmac(krb5_context context,
					    krb5_cksumtype cktype,
					    const void *data,
					    size_t len,
					    unsigned usage,
					    krb5_keyblock *key,
					    Checksum *result);
KRB5_LIB_FUNCTION krb5_boolean krb5_is_enctype_weak(krb5_context context,
						    krb5_enctype enctype);
KRB5_LIB_FUNCTION krb5_error_code krb5_string_to_enctype(krb5_context context,
							 const char *string,
							 krb5_enctype *etype);
KRB5_LIB_FUNCTION krb5_error_code krb5_verify_checksum_iov(krb5_context context,
							   krb5_crypto crypto,
							   unsigned usage,
							   krb5_crypto_iov *data,
							   unsigned int num_data,
							   krb5_cksumtype *type);
KRB5_LIB_FUNCTION krb5_error_code krb5_generate_random_keyblock(krb5_context context,
								krb5_enctype type,
								krb5_keyblock *key);
KRB5_LIB_FUNCTION krb5_boolean krb5_checksum_is_collision_proof(krb5_context context,
								krb5_cksumtype type);

KRB5_LIB_FUNCTION krb5_error_code krb5_cksumtype_to_enctype(krb5_context context,
							    krb5_cksumtype ctype,
							    krb5_enctype *etype);
KRB5_LIB_FUNCTION krb5_error_code krb5_encrypt_EncryptedData(krb5_context context,
							     krb5_crypto crypto,
							     unsigned usage,
							     void *data,
							     size_t len,
							     int kvno,
							     EncryptedData *result);
KRB5_LIB_FUNCTION krb5_error_code krb5_crypto_getconfoundersize(krb5_context context,
								krb5_crypto crypto,
								size_t *confoundersize);

KRB5_LIB_FUNCTION krb5_error_code krb5_decrypt_EncryptedData(krb5_context context,
							     krb5_crypto crypto,
							     unsigned usage,
							     const EncryptedData *e,
							     krb5_data *result);
/* heimdal/krb5/data.c */
KRB5_LIB_FUNCTION krb5_error_code krb5_data_realloc(krb5_data *p, int len);
KRB5_LIB_FUNCTION krb5_error_code krb5_copy_data(krb5_context context,
						 const krb5_data *indata,
						 krb5_data **outdata);
KRB5_LIB_FUNCTION int krb5_data_cmp(const krb5_data *data1, const krb5_data *data2);
/* heimdal/krb5/store-int.c */
KRB5_LIB_FUNCTION krb5_ssize_t _krb5_get_int(void *buffer, unsigned long *value, size_t size);

#include "crypto.h"

struct _krb5_checksum_type * _krb5_find_checksum (krb5_cksumtype);
struct _krb5_encryption_type * _krb5_find_enctype (krb5_enctype);
void _krb5_free_key_data (krb5_context, struct _krb5_key_data *,
			  struct _krb5_encryption_type *);
void _krb5_evp_cleanup (krb5_context, struct _krb5_key_data *);

krb5_error_code _krb5_evp_encrypt (krb5_context, struct _krb5_key_data *,
				   void *, size_t, krb5_boolean, int,
				   void *);
krb5_error_code _krb5_evp_encrypt_cts (krb5_context, struct _krb5_key_data *,
				       void *,size_t, krb5_boolean,
				       int, void *);
void _krb5_evp_schedule (krb5_context, struct _krb5_key_type *,
			 struct _krb5_key_data *);
krb5_error_code _krb5_SP_HMAC_SHA1_checksum (krb5_context,
					     struct _krb5_key_data *,
					     const void *,
					     size_t, unsigned, Checksum *);

void _krb5_xor(DES_cblock *key, const unsigned char *b);

krb5_error_code _krb5_internal_hmac(krb5_context context,
                                    struct _krb5_checksum_type *cm,
                                    const void *data,
                                    size_t len,
                                    unsigned usage,
                                    struct _krb5_key_data *keyblock,
                                    Checksum *result);

#ifdef KERNEL
/*
 * Ew, gross!
 * crypto.c contains hard-coded references to these, so even though we don't
 * implement these enctypes in the kernel, we need to have stubs present in
 * order to link a kernel module.  In userspace, we do implement these enctypes,
 * and the real functions are provided by the heimdal source files.
 */
static_inline krb5_error_code
_krb5_usage2arcfour(krb5_context context, unsigned *usage) {
    return -1;
}

static_inline void
_krb5_DES3_random_to_key(krb5_context context, krb5_keyblock *key,
			 const void *rand, size_t size) {
    return;
}
#else	/* KERNEL */
void
_krb5_DES3_random_to_key (krb5_context context,
			  krb5_keyblock *key,
			  const void *rand,
			  size_t size);

krb5_error_code _krb5_usage2arcfour(krb5_context context, unsigned *usage);
#endif	/* KERNEL */

#define _krb5_AES_salt NULL
#define _krb5_arcfour_salt NULL
#define _krb5_des3_salt NULL
#define _krb5_des3_salt_derived NULL
#define _krb5_des_salt NULL
