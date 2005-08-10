#include <afsconfig.h>
#include <afs/param.h>

#include <des.h>
#include <des/stats.h>

#ifndef AFS_PTHREAD_ENV 
struct rxkad_stats rxkad_stats = { { 0 } }; 
#endif
