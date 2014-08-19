/* $Id$ */

#include <afsconfig.h>
#include <afs/param.h>


#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <io.h>
#else
#include <netinet/in.h>
#endif
#include <string.h>
#include <stdarg.h>

#include <lock.h>
#define UBIK_INTERNALS
#include <afs/stds.h>
#include <afs/cellconfig.h>
#include <ubik.h>
#include <rx/xdr.h>
#include "ptint.h"
#include "ptserver.h"

extern int dbase_fd;
struct ubik_dbase *dbase;

int
ubik_ServerInit(afs_uint32 myHost, short myPort, afs_uint32 serverList[],
                const char *pathName, struct ubik_dbase **dbase)
{
    return (0);
}

int
ubik_BeginTrans(struct ubik_dbase *dbase, afs_int32 transMode,
		struct ubik_trans **transPtr)
{
    static int init = 0;
    struct ubik_hdr thdr;

    if (!init) {
	memset(&thdr, 0, sizeof(thdr));
	thdr.version.epoch = htonl(2);
	thdr.version.counter = htonl(0);
	thdr.magic = htonl(UBIK_MAGIC);
	thdr.size = htons(HDRSIZE);
	lseek(dbase_fd, 0, 0);
	write(dbase_fd, &thdr, sizeof(thdr));
	fsync(dbase_fd);
	init = 1;
    }
    return (0);
}

int
ubik_BeginTransReadAny(struct ubik_dbase *dbase, afs_int32 transMode,
                       struct ubik_trans **transPtr)
{
    return (0);
}

int
ubik_AbortTrans(struct ubik_trans *transPtr)
{
    return (0);
}

int
ubik_EndTrans(struct ubik_trans *transPtr)
{
    return (0);
}

int
ubik_Tell(struct ubik_trans *transPtr, afs_int32 * fileid,
          afs_int32 * position)
{
    return (0);
}

int
ubik_Truncate(struct ubik_trans *transPtr, afs_int32 length)
{
    return (0);
}

int
ubik_SetLock(struct ubik_trans *atrans, afs_int32 apos, afs_int32 alen,
             int atype)
{
    return (0);
}

int
ubik_WaitVersion(struct ubik_dbase *adatabase,
                 struct ubik_version *aversion)
{
    return (0);
}

int
ubik_CheckCache(struct ubik_trans *atrans, ubik_updatecache_func cbf, void *rock)
{
    return (*cbf)(atrans, rock);
}

void
panic(char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);

    abort();
    printf("BACK FROM ABORT\n");	/* shouldn't come back from floating pt exception */
    exit(1);			/* never know, though */
}

int
ubik_GetVersion(struct ubik_trans *dummy, struct ubik_version *ver)
{
    memset(ver, 0, sizeof(struct ubik_version));
    return (0);
}


int
ubik_Seek(struct ubik_trans *tt, afs_int32 afd, afs_int32 pos)
{
    if (lseek(dbase_fd, pos + HDRSIZE, 0) < 0) {
	perror("ubik_Seek");
	return (-1);
    }
    return (0);
}

int
ubik_Write(struct ubik_trans *tt, void *buf, afs_int32 len)
{
    int status;

    status = write(dbase_fd, buf, len);
    if (status < len) {
	perror("ubik_Write");
	return (1);
    }
    return (0);
}

int
ubik_Read(struct ubik_trans *tt, void *buf, afs_int32 len)
{
    int status;

    status = read(dbase_fd, buf, len);
    if (status < 0) {
	perror("ubik_Read");
	return (1);
    }
    if (status < len)
	memset((char *)buf + status, 0, len - status);
    return (0);
}


/* Global declarations from ubik.c */
afs_int32 ubik_quorum = 0;
struct ubik_dbase *ubik_dbase = 0;
struct ubik_stats ubik_stats;
afs_uint32 ubik_host[UBIK_MAX_INTERFACE_ADDR];
afs_int32 ubik_epochTime = 0;
afs_int32 urecovery_state = 0;

struct rx_securityClass *ubik_sc[3];


/* Other declarations */

int
afsconf_GetNoAuthFlag(struct afsconf_dir *adir)
{
    return (1);
}


char *prdir = "/dev/null";
struct prheader cheader;
char *pr_realmName;
