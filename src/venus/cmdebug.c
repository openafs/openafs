/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");


#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <rpc.h>
#else
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <sys/socket.h>
#include <netdb.h>
#endif
#include <stdio.h>
#include <string.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#include <afs/afscbint.h>
#include <afs/cmd.h>
#include <rx/rx.h>
#include <lock.h>
#include <afs/afs_args.h>
#include <afs/afsutil.h>

extern struct hostent *hostutil_GetHostByName();

static int print_ctime = 0;

static int
PrintCacheConfig(struct rx_connection *aconn)
{
    struct cacheConfig c;
    afs_uint32 srv_ver, conflen;
    int code;

    c.cacheConfig_len = 0;
    c.cacheConfig_val = NULL;
    code = RXAFSCB_GetCacheConfig(aconn, 1, &srv_ver, &conflen, &c);
    if (code) {
	printf("cmdebug: error checking cache config: %s\n",
	       afs_error_message(code));
	return 0;
    }

    if (srv_ver == AFS_CLIENT_RETRIEVAL_FIRST_EDITION) {
	struct cm_initparams_v1 *c1;

	if (c.cacheConfig_len != sizeof(*c1) / sizeof(afs_uint32)) {
	    printf("cmdebug: configuration data size mismatch (%d != %d)\n",
		   c.cacheConfig_len, sizeof(*c1) / sizeof(afs_uint32));
	    return 0;
	}

	c1 = (struct cm_initparams_v1 *)c.cacheConfig_val;
	printf("Chunk files:   %d\n", c1->nChunkFiles);
	printf("Stat caches:   %d\n", c1->nStatCaches);
	printf("Data caches:   %d\n", c1->nDataCaches);
	printf("Volume caches: %d\n", c1->nVolumeCaches);
	printf("Chunk size:    %d", c1->otherChunkSize);
	if (c1->firstChunkSize != c1->otherChunkSize)
	    printf(" (first: %d)", c1->firstChunkSize);
	printf("\n");
	printf("Cache size:    %d kB\n", c1->cacheSize);
	printf("Set time:      %s\n", c1->setTime ? "yes" : "no");
	printf("Cache type:    %s\n", c1->memCache ? "memory" : "disk");
    } else {
	printf("cmdebug: unsupported server version %d\n", srv_ver);
    }
}

#ifndef CAPABILITY_BITS
#define CAPABILITY_ERRORTRANS (1<<0)
#define CAPABILITY_BITS 1
#endif

static int
PrintInterfaces(struct rx_connection *aconn)
{
    Capabilities caps;
    struct interfaceAddr addr;
#ifdef AFS_NT40_ENV
    char * p;
#else
    char uuidstr[128];
#endif
    int i, code;

    caps.Capabilities_val = NULL;
    caps.Capabilities_len = 0;

    code = RXAFSCB_TellMeAboutYourself(aconn, &addr, &caps);
    if (code == RXGEN_OPCODE)
        code = RXAFSCB_WhoAreYou(aconn, &addr);
    if (code) {
	printf("cmdebug: error checking interfaces: %s\n",
	       afs_error_message(code));
	return 0;
    }

#ifdef AFS_NT40_ENV
    UuidToString((UUID *)&addr.uuid, &p);
    printf("UUID: %s\n",p);
    RpcStringFree(&p);
#else
    afsUUID_to_string(&addr.uuid, uuidstr, sizeof(uuidstr));
    printf("UUID: %s\n",uuidstr);
#endif

    printf("Host interfaces:\n");
    for (i = 0; i < addr.numberOfInterfaces; i++) {
	printf("%s", afs_inet_ntoa(htonl(addr.addr_in[i])));
	if (addr.subnetmask[i])
	    printf(", netmask %s", afs_inet_ntoa(htonl(addr.subnetmask[i])));
	if (addr.mtu[i])
	    printf(", MTU %d", addr.mtu[i]);
	printf("\n");
    }

    if (caps.Capabilities_val) {
        printf("Capabilities:\n");
        if (caps.Capabilities_val[0] & CAPABILITY_ERRORTRANS) {
            printf("Error Translation\n");  
        }
        printf("\n");
    }

    if (caps.Capabilities_val)
	free(caps.Capabilities_val);
    caps.Capabilities_val = NULL;
    caps.Capabilities_len = 0;

    return 0;
}

static int
IsLocked(register struct AFSDBLockDesc *alock)
{
    if (alock->waitStates || alock->exclLocked || alock->numWaiting
	|| alock->readersReading)
	return 1;
    return 0;
}

static int
PrintLock(register struct AFSDBLockDesc *alock)
{
    printf("(");
    if (alock->waitStates) {
	if (alock->waitStates & READ_LOCK)
	    printf("reader_waiting");
	if (alock->waitStates & WRITE_LOCK)
	    printf("writer_waiting");
	if (alock->waitStates & SHARED_LOCK)
	    printf("upgrade_waiting");
    } else
	printf("none_waiting");
    if (alock->exclLocked) {
	if (alock->exclLocked & WRITE_LOCK)
	    printf(", write_locked");
	if (alock->exclLocked & SHARED_LOCK)
	    printf(", upgrade_locked");
	printf("(pid:%d at:%d)", alock->pid_writer, alock->src_indicator);
    }
    if (alock->readersReading)
	printf(", %d read_locks(pid:%d)", alock->readersReading,
	       alock->pid_last_reader);
    if (alock->numWaiting)
	printf(", %d waiters", alock->numWaiting);
    printf(")");
    return 0;
}

static int
PrintLocks(register struct rx_connection *aconn, int aint32)
{
    register int i;
    struct AFSDBLock lock;
    afs_int32 code;

    for (i = 0; i < 1000; i++) {
	code = RXAFSCB_GetLock(aconn, i, &lock);
	if (code) {
	    if (code == 1)
		break;
	    /* otherwise we have an unrecognized error */
	    printf("cmdebug: error checking locks: %s\n",
		   afs_error_message(code));
	    return code;
	}
	/* here we have the lock information, so display it, perhaps */
	if (aint32 || IsLocked(&lock.lock)) {
	    printf("Lock %s status: ", lock.name);
	    PrintLock(&lock.lock);
	    printf("\n");
	}
    }
    return 0;
}

struct cell_cache {
    afs_int32 cellnum;
    char *cellname;
    struct cell_cache *next;
};

static char *
GetCellName(struct rx_connection *aconn, afs_int32 cellnum)
{
    static int no_getcellbynum;
    static struct cell_cache *cache;
    struct cell_cache *tcp;
    int code;
    char *cellname;
    serverList sl;

    if (no_getcellbynum)
	return NULL;

    for (tcp = cache; tcp; tcp = tcp->next)
	if (tcp->cellnum == cellnum)
	    return tcp->cellname;

    cellname = NULL;
    sl.serverList_len = 0;
    sl.serverList_val = NULL;
    code = RXAFSCB_GetCellByNum(aconn, cellnum, &cellname, &sl);
    if (code) {
	if (code == RXGEN_OPCODE)
	    no_getcellbynum = 1;
	return NULL;
    }

    if (sl.serverList_val)
	free(sl.serverList_val);
    tcp = malloc(sizeof(struct cell_cache));
    tcp->next = cache;
    tcp->cellnum = cellnum;
    tcp->cellname = cellname;
    cache = tcp;

    return cellname;
}

static int
PrintCacheEntries32(struct rx_connection *aconn, int aint32)
{
    register int i;
    register afs_int32 code;
    struct AFSDBCacheEntry centry;
    char *cellname;

    for (i = 0; i < 10000; i++) {
	code = RXAFSCB_GetCE(aconn, i, &centry);
	if (code) {
	    if (code == 1)
		break;
	    printf("cmdebug: failed to get cache entry %d (%s)\n", i,
		   afs_error_message(code));
	    return code;
	}

	if (centry.addr == 0) {
	    /* PS output */
	    printf("Proc %4d sleeping at %08x, pri %3d\n",
		   centry.netFid.Vnode, centry.netFid.Volume,
		   centry.netFid.Unique - 25);
	    continue;
	}

	if (aint32 == 0 && !IsLocked(&centry.lock) ||
            aint32 == 2 && centry.refCount == 0 ||
            aint32 == 4 && centry.callback == 0)
	    continue;

	/* otherwise print this entry */
	printf("** Cache entry @ 0x%08x for %d.%d.%d.%d", centry.addr,
	       centry.cell, centry.netFid.Volume, centry.netFid.Vnode,
	       centry.netFid.Unique);

	cellname = GetCellName(aconn, centry.cell);
	if (cellname)
	    printf(" [%s]\n", cellname);
	else
	    printf("\n");

	if (IsLocked(&centry.lock)) {
	    printf("    locks: ");
	    PrintLock(&centry.lock);
	    printf("\n");
	}
	printf("    %12d bytes  DV %12d  refcnt %5d\n", centry.Length,
	       centry.DataVersion, centry.refCount);
        if (print_ctime) {
            time_t t = centry.cbExpires;
            printf("    callback %08x\texpires %s\n", centry.callback,
                    ctime(&t));
        } else
            printf("    callback %08x\texpires %u\n", centry.callback,
                   centry.cbExpires);
	printf("    %d opens\t%d writers\n", centry.opens, centry.writers);

	/* now display states */
	printf("    ");
	if (centry.mvstat == 0)
	    printf("normal file");
	else if (centry.mvstat == 1)
	    printf("mount point");
	else if (centry.mvstat == 2)
	    printf("volume root");
	else if (centry.mvstat == 3)	/* windows */
	    printf("directory");
	else if (centry.mvstat == 4)	/* windows */
	    printf("symlink");
	else if (centry.mvstat == 5)	/* windows */
	    printf("microsoft dfs link");
	else if (centry.mvstat == 6)	/* windows */
	    printf("invalid link");
	else
	    printf("bogus mvstat %d", centry.mvstat);
	printf("\n    states (0x%x)", centry.states);
	if (centry.states & 1)
	    printf(", stat'd");
	if (centry.states & 2)
	    printf(", backup");
	if (centry.states & 4)
	    printf(", read-only");
	if (centry.states & 8)
	    printf(", mt pt valid");
	if (centry.states & 0x10)
	    printf(", pending core");
	if (centry.states & 0x40)
	    printf(", wait-for-store");
	if (centry.states & 0x80)
	    printf(", mapped");
	printf("\n");
    }
    return 0;
}

static int
PrintCacheEntries64(struct rx_connection *aconn, int aint32)
{
    register int i;
    register afs_int32 code;
    struct AFSDBCacheEntry64 centry;
    char *cellname;
    int ce64 = 0;

    for (i = 0; i < 10000; i++) {
	code = RXAFSCB_GetCE64(aconn, i, &centry);
	if (code) {
	    if (code == 1)
		break;
	    printf("cmdebug: failed to get cache entry %d (%s)\n", i,
		   afs_error_message(code));
	    return code;
	}

	if (centry.addr == 0) {
	    /* PS output */
	    printf("Proc %4d sleeping at %08x, pri %3d\n",
		   centry.netFid.Vnode, centry.netFid.Volume,
		   centry.netFid.Unique - 25);
	    continue;
	}

	if (aint32 == 0 && !IsLocked(&centry.lock) ||
            aint32 == 2 && centry.refCount == 0 ||
            aint32 == 4 && centry.callback == 0)
	    continue;

	/* otherwise print this entry */
	printf("** Cache entry @ 0x%08x for %d.%d.%d.%d", centry.addr,
	       centry.cell, centry.netFid.Volume, centry.netFid.Vnode,
	       centry.netFid.Unique);

	cellname = GetCellName(aconn, centry.cell);
	if (cellname)
	    printf(" [%s]\n", cellname);
	else
	    printf("\n");

	if (IsLocked(&centry.lock)) {
	    printf("    locks: ");
	    PrintLock(&centry.lock);
	    printf("\n");
	}
#ifdef AFS_64BIT_ENV
#ifdef AFS_NT40_ENV
	printf("    %12I64d bytes  DV %12d  refcnt %5d\n", centry.Length,
	       centry.DataVersion, centry.refCount);
#else
	printf("    %12llu bytes  DV %12d  refcnt %5d\n", centry.Length,
	       centry.DataVersion, centry.refCount);
#endif
#else
	printf("    %12d bytes  DV %12d  refcnt %5d\n", centry.Length,
	       centry.DataVersion, centry.refCount);
#endif
        if (print_ctime) {
            time_t t = centry.cbExpires;
            printf("    callback %08x\texpires %s\n", centry.callback,
                    ctime(&t));
        } else
            printf("    callback %08x\texpires %u\n", centry.callback,
                   centry.cbExpires);
	printf("    %d opens\t%d writers\n", centry.opens, centry.writers);

	/* now display states */
	printf("    ");
	if (centry.mvstat == 0)
	    printf("normal file");
	else if (centry.mvstat == 1)
	    printf("mount point");
	else if (centry.mvstat == 2)
	    printf("volume root");
	else if (centry.mvstat == 3)
	    printf("directory");
	else if (centry.mvstat == 4)
	    printf("symlink");
	else if (centry.mvstat == 5)
	    printf("microsoft dfs link");
	else if (centry.mvstat == 6)
	    printf("invalid link");
        else
	    printf("bogus mvstat %d", centry.mvstat);
	printf("\n    states (0x%x)", centry.states);
	if (centry.states & 1)
	    printf(", stat'd");
	if (centry.states & 2)
	    printf(", backup");
	if (centry.states & 4)
	    printf(", read-only");
	if (centry.states & 8)
	    printf(", mt pt valid");
	if (centry.states & 0x10)
	    printf(", pending core");
	if (centry.states & 0x40)
	    printf(", wait-for-store");
	if (centry.states & 0x80)
	    printf(", mapped");
	printf("\n");
    }
    return 0;
}

static int
PrintCacheEntries(struct rx_connection *aconn, int aint32)
{
    register afs_int32 code;
    struct AFSDBCacheEntry64 centry64;

    code = RXAFSCB_GetCE64(aconn, 0, &centry64);
    if (code != RXGEN_OPCODE)
	return PrintCacheEntries64(aconn, aint32);
    else
	return PrintCacheEntries32(aconn, aint32);
}

int
CommandProc(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *conn;
    register char *hostName;
    register struct hostent *thp;
    afs_int32 port;
    struct rx_securityClass *secobj;
    int int32p;
    afs_int32 addr;

    hostName = as->parms[0].items->data;
    if (as->parms[1].items)
	port = atoi(as->parms[1].items->data);
    else
	port = 7001;
    thp = hostutil_GetHostByName(hostName);
    if (!thp) {
	printf("cmdebug: can't resolve address for host %s.\n", hostName);
	exit(1);
    }
    memcpy(&addr, thp->h_addr, sizeof(afs_int32));
    secobj = rxnull_NewServerSecurityObject();
    conn = rx_NewConnection(addr, htons(port), 1, secobj, 0);
    if (!conn) {
	printf("cmdebug: failed to create connection for host %s\n",
	       hostName);
	exit(1);
    }
    if (as->parms[5].items) {
	/* -addrs */
	PrintInterfaces(conn);
	return 0;
    }
    if (as->parms[6].items) {
	/* -cache */
	PrintCacheConfig(conn);
	return 0;
    }

    if (as->parms[7].items)
        print_ctime = 1;

    if (as->parms[2].items)
        /* -long */
	int32p = 1;
    else if (as->parms[3].items)
        /* -refcounts */
        int32p = 2;
    else if (as->parms[4].items)
        /* -callbacks */
        int32p = 4;
    else
	int32p = 0;

    if (int32p == 0 || int32p == 1)
        PrintLocks(conn, int32p);
    if (int32p >= 0 || int32p <= 4)
        PrintCacheEntries(conn, int32p);
    return 0;
}

#ifndef AFS_NT40_ENV
#include "AFS_component_version_number.c"
#endif

int
main(int argc, char **argv)
{
    register struct cmd_syndesc *ts;

#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a 
     * crash (i.e. core is generated) we can include the user's data section 
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
#endif

#ifdef AFS_NT40_ENV
    if (afs_winsockInit() < 0) {
        printf("%s: Couldn't initialize winsock. Exiting...\n", argv[0]);
        return 1;
    }
#endif

    rx_Init(0);

    ts = cmd_CreateSyntax(NULL, CommandProc, NULL, "probe unik server");
    cmd_AddParm(ts, "-servers", CMD_SINGLE, CMD_REQUIRED, "server machine");
    cmd_AddParm(ts, "-port", CMD_SINGLE, CMD_OPTIONAL, "IP port");
    cmd_AddParm(ts, "-long", CMD_FLAG, CMD_OPTIONAL, "print all info");
    cmd_AddParm(ts, "-refcounts", CMD_FLAG, CMD_OPTIONAL, 
                 "print only cache entries with positive reference counts");
    cmd_AddParm(ts, "-callbacks", CMD_FLAG, CMD_OPTIONAL, 
                 "print only cache entries with callbacks");
    cmd_AddParm(ts, "-addrs", CMD_FLAG, CMD_OPTIONAL,
		"print only host interfaces");
    cmd_AddParm(ts, "-cache", CMD_FLAG, CMD_OPTIONAL,
		"print only cache configuration");
    cmd_AddParm(ts, "-ctime", CMD_FLAG, CMD_OPTIONAL, 
                "print human readable expiration time");

    cmd_Dispatch(argc, argv);
    exit(0);
}
