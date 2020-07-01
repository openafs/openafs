/* A trivial implementation of hcrypto's RAND interface for
 * kernel use */

#include <config.h>
#include <evp.h>
#include <evp-hcrypto.h>
#include <aes.h>
#include <sha.h>
#include <heim_threads.h>

/*
 * This mutex is used to synchronize hcrypto operations in the kernel.
 * We cheat and assume that all access into hcrypto comes through routines
 * in this file, so that we can ensure it is initialized before it is used.
 */
afs_kmutex_t hckernel_mutex;

/*
 * For these platforms, we use the fortuna RNG from heimdal (seeded from afsd
 * userspace on startup). Otherwise, we rely on osi_readRandom() as the source
 * of random data.
 */
#if defined(AFS_AIX_ENV) || defined(AFS_DFBSD_ENV) || defined(AFS_HPUX_ENV) || defined(AFS_SGI_ENV)
# define USE_FORTUNA
#endif

/* Called from osi_Init(); will only run once. */
void
init_hckernel_mutex(void)
{
    MUTEX_INIT(&hckernel_mutex, "hckernel", MUTEX_DEFAULT, 0);
}

void
RAND_seed(const void *indata, size_t size)
{
#ifdef USE_FORTUNA
    const RAND_METHOD *m = RAND_fortuna_method();
    m->seed(indata, size);
#endif
}

int
RAND_bytes(void *outdata, size_t size)
{
    if (size == 0)
	return 0;
#ifdef USE_FORTUNA
    const RAND_METHOD *m = RAND_fortuna_method();
    return m->bytes(outdata, size);
#else
    if (osi_readRandom(outdata, size))
	return 0;
#endif
    return 1;
}
