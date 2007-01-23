/* $Id$ */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <sys/types.h>
#include <netinet/in.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <lock.h>
#define UBIK_INTERNALS
#include <afs/stds.h>
#include <ubik.h>
#include <rx/xdr.h>
#include "ptint.h"
#include "ptserver.h"

extern int dbase_fd;
struct ubik_dbase *dbase;

int
ubik_ServerInit(afs_int32 myHost, short myPort, afs_int32 serverList[],
                char *pathName, struct ubik_dbase **dbase)
{
    return (0);
}

int
ubik_BeginTrans(register struct ubik_dbase *dbase, afs_int32 transMode, 
		struct ubik_trans **transPtr)
{
    static int init = 0;
    struct ubik_hdr thdr;

    if (!init) {
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
ubik_BeginTransReadAny(register struct ubik_dbase *dbase, afs_int32 transMode,
                       struct ubik_trans **transPtr)
{
    return (0);
}

int
ubik_AbortTrans(register struct ubik_trans *transPtr)
{
    return (0);
}

int
ubik_EndTrans(register struct ubik_trans *transPtr)
{
    return (0);
}

int
ubik_Tell(register struct ubik_trans *transPtr, afs_int32 * fileid,
          afs_int32 * position)
{
    return (0);
}

int
ubik_Truncate(register struct ubik_trans *transPtr, afs_int32 length)
{
    return (0);
}

long
ubik_SetLock(struct ubik_trans *atrans, afs_int32 apos, afs_int32 alen,
             int atype)
{
    return (0);
}

int
ubik_WaitVersion(register struct ubik_dbase *adatabase,
                 register struct ubik_version *aversion)
{
    return (0);
}

int
ubik_CacheUpdate(register struct ubik_trans *atrans)
{
    return (0);
}

int
panic(char *a, char *b, char *c, char *d)
{
    printf(a, b, c, d);
    abort();
    printf("BACK FROM ABORT\n");	/* shouldn't come back from floating pt exception */
    exit(1);			/* never know, though */
}

int
ubik_GetVersion(int dummy, struct ubik_version *ver)
{
    memset(ver, 0, sizeof(struct ubik_version));
    return (0);
}


int
ubik_Seek(struct ubik_trans *tt, long afd, long pos)
{
    if (lseek(dbase_fd, pos + HDRSIZE, 0) < 0) {
	perror("ubik_Seek");
	return (-1);
    }
    return (0);
}

int
ubik_Write(struct ubik_trans *tt, char *buf, long len)
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
ubik_Read(struct ubik_trans *tt, char *buf, long len)
{
    int status;

    status = read(dbase_fd, buf, len);
    if (status < 0) {
	perror("ubik_Read");
	return (1);
    }
    if (status < len)
	memset(&buf[status], 0, len - status);
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


struct afsconf_dir *prdir;
struct prheader cheader;
int pr_realmNameLen;
char *pr_realmName;
