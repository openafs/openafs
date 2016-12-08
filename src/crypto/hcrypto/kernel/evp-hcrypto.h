#define HCRYPTO_DEF_PROVIDER hckernel

const EVP_CIPHER *EVP_hckernel_aes_128_cbc(void);
const EVP_CIPHER *EVP_hckernel_aes_256_cbc(void);
const EVP_MD *EVP_hckernel_sha1(void);

/* Stubs */
const EVP_MD *EVP_hckernel_sha256(void);
const EVP_MD *EVP_hckernel_sha384(void);
const EVP_MD *EVP_hckernel_sha512(void);
const EVP_MD *EVP_hckernel_md5(void);
const EVP_MD *EVP_hckernel_md4(void);
const EVP_MD *EVP_hckernel_md2(void);
const EVP_CIPHER *EVP_hckernel_rc2_cbc(void);
const EVP_CIPHER *EVP_hckernel_rc2_40_cbc(void);
const EVP_CIPHER *EVP_hckernel_rc2_64_cbc(void);
const EVP_CIPHER *EVP_hckernel_rc4(void);
const EVP_CIPHER *EVP_hckernel_rc4_40(void);
const EVP_CIPHER *EVP_hckernel_des_cbc(void);
const EVP_CIPHER *EVP_hckernel_des_ede3_cbc(void);
const EVP_CIPHER *EVP_hckernel_aes_192_cbc(void);
const EVP_CIPHER *EVP_hckernel_aes_128_cfb8(void);
const EVP_CIPHER *EVP_hckernel_aes_192_cfb8(void);
const EVP_CIPHER *EVP_hckernel_aes_256_cfb8(void);
const EVP_CIPHER *EVP_hckernel_camellia_128_cbc(void);
const EVP_CIPHER *EVP_hckernel_camellia_192_cbc(void);
const EVP_CIPHER *EVP_hckernel_camellia_256_cbc(void);
