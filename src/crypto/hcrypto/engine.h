#ifndef OPENAFS_HCRYPTO_ENGINE_H
#define OPENAFS_HCRYPTO_ENGINE_H

typedef struct hc_engine ENGINE;

#include <hcrypto/rand.h>


int     ENGINE_finish(const ENGINE *);
int     ENGINE_up_ref(const ENGINE *);
const RAND_METHOD *     ENGINE_get_RAND(const ENGINE *);

#endif
