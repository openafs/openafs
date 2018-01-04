#ifdef AFS_PTHREAD_ENV
#include <pthread.h>

#define HEIMDAL_MUTEX pthread_mutex_t
#define HEIMDAL_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define HEIMDAL_MUTEX_init(m) pthread_mutex_init(m, NULL)
#define HEIMDAL_MUTEX_lock(m) pthread_mutex_lock(m)
#define HEIMDAL_MUTEX_unlock(m) pthread_mutex_unlock(m)
#define HEIMDAL_MUTEX_destroy(m) pthread_mutex_destroy(m)
#else
/* The one location in hcrypto which uses mutexes is the PRNG
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

