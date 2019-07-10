
/* Conversion functions to go between the various different key types used
 * by rxkad and supporting libraries
 */

#ifndef OPENAFS_RXKAD_CONVERT_H
#define OPENAFS_RXKAD_CONVERT_H 1

#include <hcrypto/des.h>

struct ktc_encryptionKey;

static_inline DES_cblock *
ktc_to_cblock(struct ktc_encryptionKey *key) {
    return (DES_cblock *)key;
}

static_inline char *
ktc_to_charptr(struct ktc_encryptionKey *key) {
    return (char *)key;
}


static_inline DES_cblock *
ktc_to_cblockptr(struct ktc_encryptionKey *key) {
    return (DES_cblock *)key;
}

static_inline DES_cblock *
charptr_to_cblock(char *key) {
    return (DES_cblock *)key;
}

static_inline DES_cblock *
charptr_to_cblockptr(char *key) {
    return (DES_cblock *)key;
}

#endif
