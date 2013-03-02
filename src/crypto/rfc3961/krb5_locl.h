/* This is a shim header that's included by crypto.c, and turns it into
 * something that we can actually build on its own.
 */

#ifdef KERNEL

#include "config.h"

#else

#include <roken.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <pthread.h>

#endif

#include <hcrypto/evp.h>
#include <hcrypto/sha.h>

#include "rfc3961.h"

#ifndef KERNEL
# define HEIMDAL_MUTEX pthread_mutex_t
# define HEIMDAL_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
# define HEIMDAL_MUTEX_init(m) pthread_mutex_init(m, NULL)
# define HEIMDAL_MUTEX_lock(m) pthread_mutex_lock(m)
# define HEIMDAL_MUTEX_unlock(m) pthread_mutex_unlock(m)
# define HEIMDAL_MUTEX_destroy(m) pthread_mutex_destroy(m)
#endif

#define HEIMDAL_SMALLER 1
#define HEIM_CRYPTO_NO_TRIPLE_DES
#define HEIM_CRYPTO_NO_ARCFOUR
#define HEIM_CRYPTO_NO_PK

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
  heim_octet_string cipher;
} EncryptedData;

typedef enum krb5_salttype {
    KRB5_PW_SALT = 3,
    KRB5_AFS3_SALT = 10
} krb5_salttype;

typedef enum krb5_keytype {
    KEYTYPE_NULL        = 0,
    KEYTYPE_DES         = 1,
    KEYTYPE_DES3        = 7,
    KEYTYPE_AES128      = 17,
    KEYTYPE_AES256      = 18,
    KEYTYPE_ARCFOUR     = 23,
    KEYTYPE_ARCFOUR_56  = 24
} krb5_keytype;

#define KRB5_ENCTYPE_NULL KEYTYPE_NULL
#define KRB5_ENCTYPE_OLD_DES3_CBC_SHA1 KEYTYPE_DES3
#define KRB5_ENCTYPE_AES128_CTS_HMAC_SHA1_96 KEYTYPE_AES128
#define KRB5_ENCTYPE_AES256_CTS_HMAC_SHA1_96 KEYTYPE_AES256

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
krb5_error_code krb5_enctype_keysize(krb5_context context,
				     krb5_enctype type,
				     size_t *keysize);
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
int ct_memcmp(const void *p1, const void *p2, size_t len);
krb5_error_code krb5_enctype_to_string(krb5_context context,
				       krb5_enctype etype,
				       char **string);


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

/* These are bodges - we don't implement these encryption types, but
 * crypto.c contains hard coded references to them, and to these funcs.
 *
 * They will never actually be called ...
 */
static_inline krb5_error_code
_krb5_usage2arcfour(krb5_context context, unsigned *usage) {
   return -1;
}

static_inline void
_krb5_DES3_random_to_key (krb5_context context,
			  krb5_keyblock *key,
			  const void *rand,
			  size_t size) {
   return;
}

#define _krb5_AES_salt NULL
