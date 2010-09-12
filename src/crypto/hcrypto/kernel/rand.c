/* A trivial implementation of hcrypto's RAND interface for
 * kernel use */

#include <config.h>
#include <evp.h>
#include <evp-hcrypto.h>
#include <aes.h>
#include <sha.h>

int
RAND_bytes(void *outdata, size_t size)
{
    if (size == 0)
	return 0;
    if (osi_readRandom(outdata, size))
	return 0;
    return 1;
}
