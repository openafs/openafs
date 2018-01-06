#include <rx_kmutex.h>
#define HEIMDAL_MUTEX afs_kmutex_t*
extern afs_kmutex_t hckernel_mutex;
#define HEIMDAL_MUTEX_INITIALIZER	&hckernel_mutex;
#define HEIMDAL_MUTEX_init(m)		MUTEX_INIT(*m,0,0,0)
#define HEIMDAL_MUTEX_lock(m)		MUTEX_ENTER(*m)
#define HEIMDAL_MUTEX_unlock(m)		MUTEX_EXIT(*m)
#define HEIMDAL_MUTEX_destroy(m)	MUTEX_DESTROY(*m)

/* Tell rand-fortuna we don't have userspace things. */
#define NO_RAND_UNIX_METHOD
#define NO_RAND_EGD_METHOD
