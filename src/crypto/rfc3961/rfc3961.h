
/* This header defines the public interface to a library which implements
 * RFC3961 crypto on top of an existing EVP layer. It is created using
 * selected bits of Heimdal's libkrb5.
 */

#ifndef RFC3961_RFC3961_H
#define RFC3961_RFC3961_H

typedef int krb5_error_code;
typedef int krb5_key_usage;
typedef struct _krb5_context * krb5_context;

typedef struct {
    size_t length;
    void *data;
} afs_heim_octet_string;

typedef afs_heim_octet_string krb5_data;

typedef struct {
  int keytype;
  afs_heim_octet_string keyvalue;
} krb5_keyblock;

typedef struct krb5_crypto_data *krb5_crypto;

#ifndef RFC3961_NO_ENUMS
typedef enum CKSUMTYPE {
  CKSUMTYPE_NONE = 0,
  CKSUMTYPE_CRC32 = 1,
  CKSUMTYPE_RSA_MD4 = 2,
  CKSUMTYPE_RSA_MD4_DES = 3,
  CKSUMTYPE_DES_MAC = 4,
  CKSUMTYPE_DES_MAC_K = 5,
  CKSUMTYPE_RSA_MD4_DES_K = 6,
  CKSUMTYPE_RSA_MD5 = 7,
  CKSUMTYPE_RSA_MD5_DES = 8,
  CKSUMTYPE_RSA_MD5_DES3 = 9,
  CKSUMTYPE_SHA1_OTHER = 10,
  CKSUMTYPE_HMAC_SHA1_DES3 = 12,
  CKSUMTYPE_SHA1 = 14,
  CKSUMTYPE_HMAC_SHA1_96_AES_128 = 15,
  CKSUMTYPE_HMAC_SHA1_96_AES_256 = 16,
  CKSUMTYPE_GSSAPI = 32771,
  CKSUMTYPE_HMAC_MD5 = -138,
  CKSUMTYPE_HMAC_MD5_ENC = -1138
} CKSUMTYPE;
#endif

#ifndef RFC3961_NO_CKSUM
typedef struct Checksum {
  CKSUMTYPE cksumtype;
  afs_heim_octet_string checksum;
} Checksum;

typedef int krb5_cksumtype;
#endif

#ifndef RFC3961_NO_ENUMS
typedef enum ENCTYPE {
  ETYPE_NULL = 0,
  ETYPE_DES_CBC_CRC = 1,
  ETYPE_DES_CBC_MD4 = 2,
  ETYPE_DES_CBC_MD5 = 3,
  ETYPE_DES3_CBC_MD5 = 5,
  ETYPE_OLD_DES3_CBC_SHA1 = 7,
  ETYPE_SIGN_DSA_GENERATE = 8,
  ETYPE_ENCRYPT_RSA_PRIV = 9,
  ETYPE_ENCRYPT_RSA_PUB = 10,
  ETYPE_DES3_CBC_SHA1 = 16,
  ETYPE_AES128_CTS_HMAC_SHA1_96 = 17,
  ETYPE_AES256_CTS_HMAC_SHA1_96 = 18,
  ETYPE_ARCFOUR_HMAC_MD5 = 23,
  ETYPE_ARCFOUR_HMAC_MD5_56 = 24,
  ETYPE_ENCTYPE_PK_CROSS = 48,
  ETYPE_ARCFOUR_MD4 = -128,
  ETYPE_ARCFOUR_HMAC_OLD = -133,
  ETYPE_ARCFOUR_HMAC_OLD_EXP = -135,
  ETYPE_DES_CBC_NONE = -4096,
  ETYPE_DES3_CBC_NONE = -4097,
  ETYPE_DES_CFB64_NONE = -4098,
  ETYPE_DES_PCBC_NONE = -4099,
  ETYPE_DIGEST_MD5_NONE = -4100,
  ETYPE_CRAM_MD5_NONE = -4101
} ENCTYPE;

enum {
  ENCTYPE_NULL		= ETYPE_NULL
};

typedef ENCTYPE krb5_enctype;

#else
typedef int krb5_enctype;
#endif

#define krb5_init_context oafs_h_krb5_init_context
#define krb5_free_context oafs_h_krb5_free_context
#define krb5_enctype_valid oafs_h_krb5_enctype_valid
#define krb5_crypto_init oafs_h_krb5_crypto_init
#define krb5_crypto_destroy oafs_h_krb5_crypto_destroy
#define krb5_encrypt oafs_h_krb5_encrypt
#define krb5_decrypt oafs_h_krb5_decrypt
#define krb5_enctype_keybits oafs_h_krb5_enctype_keybits
#define krb5_enctype_keysize oafs_h_krb5_enctype_keysize
#define krb5_data_free oafs_h_krb5_data_free
#define krb5_data_alloc oafs_h_krb5_data_alloc
#define krb5_keyblock_init oafs_h_krb5_keyblock_init
#define krb5_copy_keyblock oafs_h_krb5_copy_keyblock
#define krb5_copy_keyblock_contents oafs_h_krb5_copy_keyblock_contents
#define krb5_free_keyblock oafs_h_krb5_free_keyblock
#define krb5_free_keyblock_contents oafs_h_krb5_free_keyblock_contents
#define krb5_keyblock_zero oafs_h_krb5_keyblock_zero
#define krb5_keyblock_get_enctype oafs_h_krb5_keyblock_get_enctype

krb5_error_code krb5_init_context(krb5_context *context);

void krb5_free_context(krb5_context context);

krb5_error_code krb5_enctype_valid(krb5_context, krb5_enctype);

krb5_error_code krb5_crypto_init(krb5_context context,
		                 const krb5_keyblock *key,
				 krb5_enctype etype,
				 krb5_crypto *crypto);

krb5_error_code krb5_crypto_destroy(krb5_context context,
				    krb5_crypto crypto);

krb5_error_code krb5_encrypt(krb5_context context,
			     krb5_crypto crypto,
			     unsigned usage,
			     const void *data,
			     size_t len,
			     krb5_data *result);

krb5_error_code krb5_decrypt(krb5_context context,
			     krb5_crypto crypto,
			     unsigned usage,
			     void *data,
			     size_t len,
			     krb5_data *result);

krb5_error_code krb5_enctype_keybits(krb5_context context,
				     krb5_enctype type,
				     size_t *keybits);
krb5_error_code krb5_enctype_keysize(krb5_context context,
				     krb5_enctype type,
				     size_t *keysize);

void krb5_data_free(krb5_data *p);

krb5_error_code krb5_data_alloc(krb5_data *p, int len);

void krb5_free_keyblock_contents(krb5_context context,
				 krb5_keyblock *keyblock);

#define krb5_crypto_prf oafs_h_krb5_crypto_prf
#define krb5_crypto_prf_length oafs_h_krb5_crypto_prf_length
#define krb5_crypto_fx_cf2 oafs_h_krb5_crypto_fx_cf2
#define krb5_generate_random_block oafs_h_krb5_generate_random_block
#define krb5_random_to_key oafs_h_krb5_random_to_key
#define krb5_crypto_overhead oafs_h_krb5_crypto_overhead

krb5_error_code krb5_crypto_prf(krb5_context context,
				const krb5_crypto crypto,
				const krb5_data *input,
				krb5_data *output);

krb5_error_code krb5_crypto_prf_length(krb5_context context,
				       krb5_enctype type,
				       size_t *length);

krb5_error_code krb5_crypto_fx_cf2(krb5_context context,
				   const krb5_crypto crypto1,
				   const krb5_crypto crypto2,
				   krb5_data *pepper1,
				   krb5_data *pepper2,
				   krb5_enctype enctype,
				   krb5_keyblock *res);

void krb5_generate_random_block(void *buf, size_t len);

krb5_error_code krb5_random_to_key(krb5_context context,
				   krb5_enctype type,
				   const void *data,
				   size_t size,
				   krb5_keyblock *key);

size_t krb5_crypto_overhead (krb5_context context,
			     krb5_crypto crypto);

#ifndef RFC3961_NO_CKSUM
#define krb5_crypto_get_checksum_type oafs_h_krb5_crypto_get_checksum_type
#define krb5_checksumsize oafs_h_krb5_checksumsize
#define krb5_create_checksum oafs_h_krb5_create_checksum
#define krb5_verify_checksum oafs_h_krb5_verify_checksum
#define free_Checksum oafs_h_free_Checksum

krb5_error_code krb5_crypto_get_checksum_type (krb5_context context,
					       krb5_crypto crypto,
					       krb5_cksumtype *type);
krb5_error_code krb5_checksumsize (krb5_context context,
				   krb5_cksumtype type,
				   size_t *size);

krb5_error_code krb5_create_checksum (krb5_context context,
				      krb5_crypto crypto,
				      krb5_key_usage usage,
				      int type,
				      void *data,
				      size_t len,
				      Checksum *result);

krb5_error_code krb5_verify_checksum (krb5_context context,
				      krb5_crypto crypto,
				      krb5_key_usage usage,
				      void *data,
				      size_t len,
				      Checksum *cksum);


void free_Checksum(Checksum *data);
#endif

void krb5_keyblock_zero(krb5_keyblock *keyblock);
void krb5_free_keyblock_contents(krb5_context context,
			    krb5_keyblock *keyblock);
void krb5_free_keyblock(krb5_context context,
		   krb5_keyblock *keyblock);
krb5_error_code krb5_copy_keyblock_contents (krb5_context context,
			     const krb5_keyblock *inblock,
			     krb5_keyblock *to);
krb5_error_code krb5_copy_keyblock (krb5_context context,
		    const krb5_keyblock *inblock,
		    krb5_keyblock **to);
krb5_enctype krb5_keyblock_get_enctype(const krb5_keyblock *block);
krb5_error_code krb5_keyblock_init(krb5_context context,
		   krb5_enctype type,
		   const void *data,
		   size_t size,
		   krb5_keyblock *key);

#endif /* RFC3961_RFC3961_H */
