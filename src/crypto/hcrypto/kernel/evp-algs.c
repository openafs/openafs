/* A cut down set of hcrypto EVP ciphers for kernel use */

#include <config.h>
#include <evp.h>
#include <evp-hcrypto.h>
#include <aes.h>
#include <sha.h>

static int
aes_init(EVP_CIPHER_CTX *ctx,
	 const unsigned char * key,
	 const unsigned char * iv,
	 int encp)
{
    AES_KEY *k = ctx->cipher_data;
    if (ctx->encrypt)
	AES_set_encrypt_key(key, ctx->cipher->key_len * 8, k);
    else
	AES_set_decrypt_key(key, ctx->cipher->key_len * 8, k);
    return 1;
}

static int
aes_do_cipher(EVP_CIPHER_CTX *ctx,
	      unsigned char *out,
	      const unsigned char *in,
	      unsigned int size)
{
    AES_KEY *k = ctx->cipher_data;
    if (ctx->flags & EVP_CIPH_CFB8_MODE)
        AES_cfb8_encrypt(in, out, size, k, ctx->iv, ctx->encrypt);
    else
        AES_cbc_encrypt(in, out, size, k, ctx->iv, ctx->encrypt);
    return 1;
}

const EVP_CIPHER *
EVP_hckernel_aes_128_cbc(void)
{
    static const EVP_CIPHER aes_128_cbc = {
	0,
	16,
	16,
	16,
	EVP_CIPH_CBC_MODE,
	aes_init,
	aes_do_cipher,
	NULL,
	sizeof(AES_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };

    return &aes_128_cbc;
}

const EVP_CIPHER *
EVP_hckernel_aes_256_cbc(void)
{
    static const EVP_CIPHER aes_256_cbc = {
	0,
	16,
	32,
	16,
	EVP_CIPH_CBC_MODE,
	aes_init,
	aes_do_cipher,
	NULL,
	sizeof(AES_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &aes_256_cbc;
}

const EVP_MD *
EVP_hckernel_sha1(void)
{
    static const struct hc_evp_md sha1 = {
	20,
	64,
	sizeof(SHA_CTX),
	(hc_evp_md_init)SHA1_Init,
	(hc_evp_md_update)SHA1_Update,
	(hc_evp_md_final)SHA1_Final,
	NULL
    };
    return &sha1;
}

const EVP_MD *
EVP_hckernel_sha256(void) {
    return NULL;
}

const EVP_MD *
EVP_hckernel_sha384(void) {
    return NULL;
}

const EVP_MD *
EVP_hckernel_sha512(void) {
    return NULL;
}

const EVP_MD *
EVP_hckernel_md5(void) {
    return NULL;
}

const EVP_MD *
EVP_hckernel_md4(void) {
    return NULL;
}

const EVP_MD *
EVP_hckernel_md2(void) {
    return NULL;
}

const EVP_CIPHER *
EVP_hckernel_rc2_cbc(void) {
    return NULL;
}

const EVP_CIPHER *
EVP_hckernel_rc2_40_cbc(void) {
    return NULL;
}

const EVP_CIPHER *
EVP_hckernel_rc2_64_cbc(void) {
    return NULL;
}

const EVP_CIPHER *
EVP_hckernel_rc4(void) {
    return NULL;
}

const EVP_CIPHER *
EVP_hckernel_rc4_40(void) {
    return NULL;
}

const EVP_CIPHER *
EVP_hckernel_des_cbc(void) {
    return NULL;
}

const EVP_CIPHER *
EVP_hckernel_des_ede3_cbc(void) {
    return NULL;
}

const EVP_CIPHER *
EVP_hckernel_aes_192_cbc(void) {
    return NULL;
}

const EVP_CIPHER *
EVP_hckernel_aes_128_cfb8(void) {
    return NULL;
}

const EVP_CIPHER *
EVP_hckernel_aes_192_cfb8(void) {
    return NULL;
}

const EVP_CIPHER *
EVP_hckernel_aes_256_cfb8(void) {
    return NULL;
}

const EVP_CIPHER *
EVP_hckernel_camellia_128_cbc(void) {
    return NULL;
}

const EVP_CIPHER *
EVP_hckernel_camellia_192_cbc(void) {
    return NULL;
}

const EVP_CIPHER *
EVP_hckernel_camellia_256_cbc(void) {
    return NULL;
}

void
hcrypto_validate(void) {
    return;
}
