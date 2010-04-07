#include <pthread.h>

#define HEIMDAL_MUTEX pthread_mutex_t
#define HEIMDAL_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define HEIMDAL_MUTEX_init(m) pthread_mutex_init(m, NULL)
#define HEIMDAL_MUTEX_lock(m) pthread_mutex_lock(m)
#define HEIMDAL_MUTEX_unlock(m) pthread_mutex_unlock(m)
#define HEIMDAL_MUTEX_destroy(m) pthread_mutex_destroy(m)
