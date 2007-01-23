/*
 *	CAST-128 in C
 *	Written by Steve Reid <sreid@sea-to-sky.net>
 *	100% Public Domain - no warranty
 *	Released 1997.10.11
 */

typedef unsigned char cast_u8;
typedef unsigned int cast_u32;

typedef struct {
    cast_u32 xkey[32];		/* Key, after expansion */
    int rounds;			/* Number of rounds to use, 12 or 16 */
} cast_key;

void cast_setkey(cast_key* key, const cast_u8* rawkey, int keybytes);
void cast_encrypt(const cast_key* key, const cast_u8* inblock, cast_u8* outblock);
void cast_decrypt(const cast_key* key, const cast_u8* inblock, cast_u8* outblock);

#define CAST_BLOCK_SIZE 8

void cast_cbc_encrypt(const unsigned char *, unsigned char *, unsigned int,
    const cast_key *, unsigned char *, const int);
void cast_cfb64_encrypt(const unsigned char *, unsigned char *, unsigned int,
    const cast_key *, unsigned char *, int *num, const int);
void cast_ecb_encrypt(const unsigned char *, unsigned char *, const cast_key *,
    int);
void cast_ofb64_encrypt(const unsigned char *, unsigned char *, unsigned int,
    const cast_key *, unsigned char *, int *);
