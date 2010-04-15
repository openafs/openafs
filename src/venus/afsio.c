/*
 * Copyright (c) 2007, Hartmut Reuter,
 * RZG, Max-Planck-Institut f. Plasmaphysik.
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <afs/stds.h>

#include <stdio.h>
#include <setjmp.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#ifdef AFS_NT40_ENV
#include <windows.h>
#include <winsock2.h>
#define _CRT_RAND_S
#include <stdlib.h>
#include <process.h>
#include <fcntl.h>
#include <io.h>
#include <afs/smb_iocons.h>
#include <afs/afsd.h>
#include <afs/cm_ioctl.h>
#include <afs/pioctl_nt.h>
#include <WINNT/syscfg.h>
#else
#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <pwd.h>
#include <afs/venus.h>
#include <sys/time.h>
#include <netdb.h>
#include <afs/afsint.h>
#define FSINT_COMMON_XG 1
#endif
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <afs/vice.h>
#include <afs/cmd.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/afsutil.h>
#include <rx/rx.h>
#include <rx/xdr.h>
#include <afs/afs_consts.h>
#include <afs/afscbint.h>
#include <afs/vldbint.h>
#include <afs/vlserver.h>
#include <afs/volser.h>
#include <afs/ptint.h>
#include <afs/dir.h>
#include <afs/nfs.h>
#include <afs/ihandle.h>
#include <afs/vnode.h>
#include <afs/com_err.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_DIRECT_H
#include <direct.h>
#endif
#ifdef AFS_DARWIN_ENV
#include <sys/malloc.h>
#else
#include <malloc.h>
#endif
#include <afs/errors.h>
#include <afs/sys_prototypes.h>
#include <rx/rx_prototypes.h>
#include <hcrypto/md5.h>

#ifdef O_LARGEFILE
#define afs_stat        stat64
#define afs_fstat       fstat64
#define afs_open        open64
#else /* !O_LARGEFILE */
#define afs_stat        stat
#define afs_fstat       fstat
#define afs_open        open
#endif /* !O_LARGEFILE */
#ifdef AFS_PTHREAD_ENV
#include <assert.h>
pthread_key_t uclient_key;
#endif

int readFile(struct cmd_syndesc *as, void *);
int writeFile(struct cmd_syndesc *as, void *);
struct rx_connection *FindRXConnection(afs_uint32 host, u_short port, u_short service, struct rx_securityClass *securityObject, int serviceSecurityIndex);
struct cellLookup * FindCell(char *cellName);

char pnp[255];
int rxInitDone = 0;
static int verbose = 0;		/* Set if -verbose option given */
static int CBServiceNeeded = 0;
static struct timeval starttime, opentime, readtime, writetime;
afs_uint64 xfered=0, oldxfered=0;
static struct timeval now;
struct timezone Timezone;
static float seconds, datarate, oldseconds;
extern int rxInitDone;
#ifdef AFS_NT40_ENV
static afs_int32 rx_mtu = -1;
#endif
afs_uint64 transid = 0;
afs_uint32 expires = 0;
afs_uint32 server_List[MAXHOSTSPERCELL];
char tmpstr[1024];
char tmpstr2[1024];
static struct ubik_client *uclient;
#define BUFFLEN 65536
#define WRITEBUFFLEN 1024*1024*64

afsUUID uuid;
MD5_CTX md5;
int md5sum = 0;

struct wbuf {
    struct wbuf *next;
    afs_uint32 offset;		/* offset inside the buffer */
    afs_uint32 buflen;		/* total length == BUFFLEN */
    afs_uint32 used;		/* bytes used inside buffer */
    char buf[BUFFLEN];
};

struct connectionLookup {
    afs_uint32 host;
    u_short port;
    struct rx_connection *conn;
};

struct cellLookup {
    struct cellLookup *next;
    struct afsconf_cell info;
    struct rx_securityClass *sc;
    afs_int32 scIndex;
};

struct dirLookup {
    struct dirLookup *next;
    struct dirLookup *prev;
    afs_int32 host;
    struct cellLookup *cell;
    AFSFid fid;
    char name[VL_MAXNAMELEN];
};

struct cellLookup *Cells = 0;
struct dirLookup  *Dirs = 0;
char cellFname[256];

#define MAX_HOSTS 256
static struct connectionLookup ConnLookup[MAX_HOSTS];
static int ConnLookupInitialized = 0;

struct FsCmdInputs PioctlInputs;
struct FsCmdOutputs PioctlOutputs;

void
printDatarate(void)
{
    seconds = (float)(now.tv_sec + now.tv_usec *.000001
	-opentime.tv_sec - opentime.tv_usec *.000001);
    if ((seconds - oldseconds) > 30.) {
	afs_int64 tmp;
	tmp = xfered - oldxfered;
        datarate = ((afs_uint32) (tmp >> 20)) / (seconds - oldseconds);
        fprintf(stderr,"%llu MB transferred, present date rate = %.03f MB/sec.\n",
		xfered >> 20, datarate);
	oldxfered = xfered;
	oldseconds = seconds;
    }
}

void
SetCellFname(char *name)
{
    struct afsconf_dir *tdir;

    strcpy((char *) &cellFname,"/afs/");
    if (name)
	strcat((char *) &cellFname, name);
    else {
	tdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);
	afsconf_GetLocalCell(tdir, &cellFname[5], MAXCELLCHARS);
    }
}

afs_int32
main (int argc, char **argv)
{
    afs_int32 code;
    struct cmd_syndesc *ts;

    strcpy(pnp, argv[0]);

#ifdef AFS_PTHREAD_ENV
    assert(pthread_key_create(&uclient_key, NULL) == 0);
#endif
    ts = cmd_CreateSyntax("read", readFile, CMD_REQUIRED,
			  "read a file from AFS");
    cmd_AddParm(ts, "-file", CMD_SINGLE, CMD_REQUIRED, "AFS-filename");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cellname");
    cmd_AddParm(ts, "-verbose", CMD_FLAG, CMD_OPTIONAL, (char *) 0);
    cmd_AddParm(ts, "-md5", CMD_FLAG, CMD_OPTIONAL, "calculate md5 checksum");

    ts = cmd_CreateSyntax("fidread", readFile, CMD_REQUIRED,
			  "read on a non AFS-client a file from AFS");
    cmd_IsAdministratorCommand(ts);
    cmd_AddParm(ts, "-fid", CMD_SINGLE, CMD_REQUIRED, "volume.vnode.uniquifier");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cellname");
    cmd_AddParm(ts, "-verbose", CMD_FLAG, CMD_OPTIONAL, (char *) 0);
    cmd_AddParm(ts, "-md5", CMD_FLAG, CMD_OPTIONAL, "calculate md5 checksum");

    ts = cmd_CreateSyntax("write", writeFile, CMD_REQUIRED,
			  "write a file into AFS");
    cmd_AddParm(ts, "-file", CMD_SINGLE, CMD_OPTIONAL, "AFS-filename");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cellname");
    cmd_AddParm(ts, "-verbose", CMD_FLAG, CMD_OPTIONAL, (char *) 0);
    cmd_AddParm(ts, "-md5", CMD_FLAG, CMD_OPTIONAL, "calculate md5 checksum");
    cmd_AddParm(ts, "-synthesize", CMD_SINGLE, CMD_OPTIONAL, "create data pattern of specified length instead reading from stdin");

    ts = cmd_CreateSyntax("fidwrite", writeFile, CMD_REQUIRED,
			  "write a file into AFS");
    cmd_IsAdministratorCommand(ts);
    cmd_AddParm(ts, "-vnode", CMD_SINGLE, CMD_REQUIRED, "volume.vnode.uniquifier");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cellname");
    cmd_AddParm(ts, "-verbose", CMD_FLAG, CMD_OPTIONAL, (char *) 0);
    cmd_AddParm(ts, "-md5", CMD_FLAG, CMD_OPTIONAL, "calculate md5 checksum");

    ts = cmd_CreateSyntax("append", writeFile, CMD_REQUIRED,
			  "append to a file in AFS");
    cmd_AddParm(ts, "-file", CMD_SINGLE, CMD_OPTIONAL, "AFS-filename");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cellname");
    cmd_AddParm(ts, "-verbose", CMD_FLAG, CMD_OPTIONAL, (char *) 0);

    ts = cmd_CreateSyntax("fidappend", writeFile, CMD_REQUIRED,
			  "append to a file in AFS");
    cmd_IsAdministratorCommand(ts);
    cmd_AddParm(ts, "-vnode", CMD_SINGLE, CMD_REQUIRED, "volume.vnode.uniquifier");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cellname");
    cmd_AddParm(ts, "-verbose", CMD_FLAG, CMD_OPTIONAL, (char *) 0);

    code = cmd_Dispatch(argc, argv);
    exit (0);
}

AFS_UNUSED
afs_int32
HandleLocalAuth(struct rx_securityClass **sc, afs_int32 *scIndex)
{
    static struct afsconf_dir *tdir = NULL;
    afs_int32 code;

    *sc = NULL;
    *scIndex = RX_SECIDX_NULL;

    tdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
    if (!tdir) {
        fprintf(stderr,"Could not open configuration directory: %s.\n",
		AFSDIR_SERVER_ETC_DIRPATH);
        return -1;
    }
    code = afsconf_ClientAuth(tdir, sc, scIndex);
    if (code) {
        fprintf(stderr,"afsconf_ClientAuth returned %d\n", code);
        return -1;
    }
    return 0;
}

afs_int32
AFS_Lookup(struct rx_connection *conn, AFSFid *dirfid, char *name,
	   AFSFid *outfid, AFSFetchStatus *outstatus, AFSFetchStatus
	   *dirstatus, AFSCallBack *callback, AFSVolSync *sync)
{
    afs_int32 code = VBUSY;
    while (code == VBUSY) {
	code = RXAFS_Lookup(conn, dirfid, name, outfid, outstatus, dirstatus,
			    callback, sync);
	if (code == VBUSY) {
	    fprintf(stderr, "waiting for busy AFS volume %u.\n",
		    dirfid->Volume);
#ifdef AFS_PTHREAD_ENV
	    sleep(10);
#else
	    IOMGR_Sleep(10);
#endif
	}
    }
    return code;
}

afs_int32
AFS_FetchStatus(struct rx_connection *conn, AFSFid *fid, AFSFetchStatus
		*Status, AFSCallBack *callback, AFSVolSync *sync)
{
    afs_int32 code = VBUSY;

    while (code == VBUSY) {
        code = RXAFS_FetchStatus(conn, fid, Status, callback, sync);
	if (code == VBUSY) {
	    fprintf(stderr, "waiting for busy AFS volume %u.\n",
		    fid->Volume);
#ifdef AFS_PTHREAD_ENV
	    sleep(10);
#else
	    IOMGR_Sleep(10);
#endif
	}
    }
    return code;
}

afs_int32
StartAFS_FetchData(struct rx_call *call, AFSFid *fid, afs_int32 pos,
		   afs_int32 len)
{
    afs_int32 code = VBUSY;
    while (code == VBUSY) {
	code = StartRXAFS_FetchData (call, fid, pos, len);
	if (code == VBUSY) {
	    fprintf(stderr, "waiting for busy AFS volume %u.\n",
		    fid->Volume);
#ifdef AFS_PTHREAD_ENV
	    sleep(10);
#else
	    IOMGR_Sleep(10);
#endif
	}
    }
    return code;
}

afs_int32
StartAFS_FetchData64(struct rx_call *call, AFSFid *fid, afs_int64 pos,
                     afs_int64 len)
{
    afs_int32 code = VBUSY;
    while (code == VBUSY) {
	code = StartRXAFS_FetchData64 (call, fid, pos, len);
	if (code == VBUSY) {
	    fprintf(stderr, "waiting for busy AFS volume %u.\n",
		    fid->Volume);
#ifdef AFS_PTHREAD_ENV
	    sleep(10);
#else
	    IOMGR_Sleep(10);
#endif
	}
    }
    return code;
}

afs_int32
StartAFS_StoreData(struct rx_call *call, AFSFid *fid, AFSStoreStatus *status,
		   afs_int32 pos, afs_int32 len, afs_int32 len2)
{
    afs_int32 code = VBUSY;
    while (code == VBUSY) {
	code = StartRXAFS_StoreData (call, fid, status, pos, len, len2);
	if (code == VBUSY) {
	    fprintf(stderr, "waiting for busy AFS volume %u.\n",
		    fid->Volume);
#ifdef AFS_PTHREAD_ENV
	    sleep(10);
#else
	    IOMGR_Sleep(10);
#endif
	}
    }
    return code;
}

afs_uint32
StartAFS_StoreData64(struct rx_call *call, AFSFid *fid, AFSStoreStatus *status,
		     afs_int64 pos, afs_int64 len, afs_int64 len2)
{
    afs_int32 code = VBUSY;
    while (code == VBUSY) {
	code = StartRXAFS_StoreData64 (call, fid, status, pos, len, len2);
	if (code == VBUSY) {
	    fprintf(stderr, "waiting for busy AFS volume %u.\n",
		    fid->Volume);
#ifdef AFS_PTHREAD_ENV
	    sleep(10);
#else
	    IOMGR_Sleep(10);
#endif
	}
    }
    return code;
}

afs_int32
SRXAFSCB_CallBack(struct rx_call *rxcall, AFSCBFids *Fids_Array,
		  AFSCBs *CallBack_Array)
{
    return 0;
}

afs_int32
SRXAFSCB_InitCallBackState(struct rx_call *rxcall)
{
    return 0;
}

afs_int32
SRXAFSCB_Probe(struct rx_call *rxcall)
{
    return 0;
}

afs_int32
SRXAFSCB_GetCE(struct rx_call *rxcall,
	       afs_int32 index,
	       AFSDBCacheEntry * ce)
{
    return(0);
}

afs_int32
SRXAFSCB_GetLock(struct rx_call *rxcall,
		 afs_int32 index,
		 AFSDBLock * lock)
{
    return(0);
}

afs_int32
SRXAFSCB_XStatsVersion(struct rx_call *rxcall,
		       afs_int32 * versionNumberP)
{
    return(0);
}

afs_int32
SRXAFSCB_GetXStats(struct rx_call *rxcall,
		   afs_int32 clientVersionNumber,
		   afs_int32 collectionNumber,
		   afs_int32 * srvVersionNumberP,
		   afs_int32 * timeP,
		   AFSCB_CollData * dataP)
{
    return(0);
}

afs_int32
SRXAFSCB_ProbeUuid(struct rx_call *a_call, afsUUID *a_uuid)
{
    if ( !afs_uuid_equal(&uuid, a_uuid) )
        return(1);
    else
        return(0);
}


afs_int32
SRXAFSCB_WhoAreYou(struct rx_call *a_call, struct interfaceAddr *addr)
{
    return SRXAFSCB_TellMeAboutYourself(a_call, addr, NULL);
}

afs_int32
SRXAFSCB_InitCallBackState2(struct rx_call *a_call, struct interfaceAddr *
			    addr)
{
    return RXGEN_OPCODE;
}

afs_int32
SRXAFSCB_InitCallBackState3(struct rx_call *a_call, afsUUID *a_uuid)
{
    return 0;
}

afs_int32
SRXAFSCB_GetCacheConfig(struct rx_call *a_call, afs_uint32 callerVersion,
			afs_uint32 *serverVersion, afs_uint32 *configCount,
			cacheConfig *config)
{
    return RXGEN_OPCODE;
}

afs_int32
SRXAFSCB_GetLocalCell(struct rx_call *a_call, char **a_name)
{
    return RXGEN_OPCODE;
}

afs_int32
SRXAFSCB_GetCellServDB(struct rx_call *a_call, afs_int32 a_index,
		       char **a_name, serverList *a_hosts)
{
    return RXGEN_OPCODE;
}

afs_int32
SRXAFSCB_GetServerPrefs(struct rx_call *a_call, afs_int32 a_index,
			afs_int32 *a_srvr_addr, afs_int32 *a_srvr_rank)
{
    return RXGEN_OPCODE;
}

afs_int32
SRXAFSCB_TellMeAboutYourself(struct rx_call *a_call, struct interfaceAddr *
			     addr, Capabilities *capabilities)
{
#ifdef AFS_NT40_ENV
    int code;
    int cm_noIPAddr;                        /* number of client network interfaces */
    int cm_IPAddr[CM_MAXINTERFACE_ADDR];    /* client's IP address in host order */
    int cm_SubnetMask[CM_MAXINTERFACE_ADDR];/* client's subnet mask in host order*/
    int cm_NetMtu[CM_MAXINTERFACE_ADDR];    /* client's MTU sizes */
    int cm_NetFlags[CM_MAXINTERFACE_ADDR];  /* network flags */
    int i;

    cm_noIPAddr = CM_MAXINTERFACE_ADDR;
    code = syscfg_GetIFInfo(&cm_noIPAddr,
                            cm_IPAddr, cm_SubnetMask,
                            cm_NetMtu, cm_NetFlags);
    if (code > 0) {
        /* return all network interface addresses */
        addr->numberOfInterfaces = cm_noIPAddr;
        for ( i=0; i < cm_noIPAddr; i++ ) {
            addr->addr_in[i] = cm_IPAddr[i];
            addr->subnetmask[i] = cm_SubnetMask[i];
            addr->mtu[i] = (rx_mtu == -1 || (rx_mtu != -1 && cm_NetMtu[i] < rx_mtu)) ?
                cm_NetMtu[i] : rx_mtu;
        }
    } else {
        addr->numberOfInterfaces = 0;
    }
#else
    addr->numberOfInterfaces = 0;
#ifdef notdef
    /* return all network interface addresses */
    addr->numberOfInterfaces = afs_cb_interface.numberOfInterfaces;
    for ( i=0; i < afs_cb_interface.numberOfInterfaces; i++) {
        addr->addr_in[i] = ntohl(afs_cb_interface.addr_in[i]);
        addr->subnetmask[i] = ntohl(afs_cb_interface.subnetmask[i]);
        addr->mtu[i] = ntohl(afs_cb_interface.mtu[i]);
    }
#endif
#endif

    addr->uuid = uuid;

    if (capabilities) {
        afs_uint32 *dataBuffP;
        afs_int32 dataBytes;

        dataBytes = 1 * sizeof(afs_uint32);
        dataBuffP = (afs_uint32 *) xdr_alloc(dataBytes);
        dataBuffP[0] = CLIENT_CAPABILITY_ERRORTRANS;
        capabilities->Capabilities_len = dataBytes / sizeof(afs_uint32);
        capabilities->Capabilities_val = dataBuffP;
    }
    return 0;
}

afs_int32
SRXAFSCB_GetCellByNum(struct rx_call *a_call, afs_int32 a_cellnum,
		      char **a_name, serverList *a_hosts)
{
    return RXGEN_OPCODE;
}

afs_int32
SRXAFSCB_GetCE64(struct rx_call *a_call, afs_int32 a_index,
		 struct AFSDBCacheEntry64 *a_result)
{
    return RXGEN_OPCODE;
}

void *
InitializeCBService_LWP(void *unused)
{
    struct rx_securityClass *CBsecobj;
    struct rx_service *CBService;

    afs_uuid_create(&uuid);

    CBsecobj = (struct rx_securityClass *)rxnull_NewServerSecurityObject();
    if (!CBsecobj) {
	fprintf(stderr,"rxnull_NewServerSecurityObject failed for callback service.\n");
	exit(1);
    }
    CBService = rx_NewService(0, 1, "afs", &CBsecobj, 1,
			      RXAFSCB_ExecuteRequest);
    if (!CBService) {
	fprintf(stderr,"rx_NewService failed for callback service.\n");
	exit(1);
    }
    rx_StartServer(1);
    return 0;
}


int
InitializeCBService(void)
{
#define RESTOOL_CBPORT 7102
#define MAX_PORT_TRIES 1000
#define LWP_STACK_SIZE	(16 * 1024)
    afs_int32 code;
#ifdef AFS_PTHREAD_ENV
    pthread_t CBservicePid;
    pthread_attr_t tattr;
#else
    PROCESS CBServiceLWP_ID, parentPid;
#endif
    int InitialCBPort;
    int CBPort;

#ifndef NO_AFS_CLIENT
    if (!CBServiceNeeded)
        return 0;
#endif
#ifndef AFS_PTHREAD_ENV
    code = LWP_InitializeProcessSupport(LWP_MAX_PRIORITY - 2, &parentPid);
    if (code != LWP_SUCCESS) {
	fprintf(stderr,"Unable to initialize LWP support, code %d\n",
		code);
	exit(1);
    }
#endif

#if defined(AFS_AIX_ENV) || defined(AFS_SUN_ENV) || defined(AFS_DEC_ENV) || defined(AFS_OSF_ENV) || defined(AFS_SGI_ENV)
    srandom(getpid());
    InitialCBPort = RESTOOL_CBPORT + random() % 1000;
#else /* AFS_AIX_ENV || AFS_SUN_ENV || AFS_OSF_ENV || AFS_SGI_ENV */
#if defined(AFS_HPUX_ENV)
    srand48(getpid());
    InitialCBPort = RESTOOL_CBPORT + lrand48() % 1000;
#else /* AFS_HPUX_ENV */
#if defined AFS_NT40_ENV
    srand(_getpid());
    InitialCBPort = RESTOOL_CBPORT + rand() % 1000;
#else /* AFS_NT40_ENV */
    srand(getpid());
    InitialCBPort = RESTOOL_CBPORT + rand() % 1000;
#endif /* AFS_NT40_ENV */
#endif /* AFS_HPUX_ENV */
#endif /* AFS_AIX_ENV || AFS_SUN_ENV || AFS_OSF_ENV || AFS_SGI_ENV */

    CBPort = InitialCBPort;
    do {
	code = rx_Init(htons(CBPort));
	if (code) {
	    if ((code == RX_ADDRINUSE) &&
		(CBPort < MAX_PORT_TRIES + InitialCBPort)) {
		CBPort++;
	    } else if (CBPort < MAX_PORT_TRIES + InitialCBPort) {
		fprintf(stderr, "rx_Init didn't succeed for callback service."
			" Tried port numbers %d through %d\n",
			InitialCBPort, CBPort);
		exit(1);
	    } else {
		fprintf(stderr,"Couldn't initialize callback service "
			"because too many users are running this program. "
			"Try again later.\n");
		exit(1);
	    }
	}
    } while(code);
#ifdef AFS_PTHREAD_ENV
    assert(pthread_attr_init(&tattr) == 0);
    assert(pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED) == 0);
    assert(pthread_create(
	       &CBservicePid, &tattr, InitializeCBService_LWP, 0)
	   == 0);
#else
    code = LWP_CreateProcess(InitializeCBService_LWP, LWP_STACK_SIZE,
			     LWP_MAX_PRIORITY - 2, (int *) 0, "CBService",
			     &CBServiceLWP_ID);
    if (code != LWP_SUCCESS) {
	fprintf(stderr,"Unable to create the callback service LWP, code %d\n",
		code);
	exit(1);
    }
#endif
    return 0;
}

int
ScanVnode(char *fname, char *cell)
{
    afs_int32 i, code = 0;

    SetCellFname(cell);
    i = sscanf(fname, "%u.%u.%u",
	       &PioctlInputs.fid.Volume,
	       &PioctlInputs.fid.Vnode,
	       &PioctlInputs.fid.Unique);
    if (i != 3) {
	PioctlInputs.fid.Volume = 0;
	PioctlInputs.fid.Vnode = 0;
	PioctlInputs.fid.Unique = 0;
        fprintf(stderr,"fs: invalid vnode triple: %s\n", fname);
        code = EINVAL;
    }
    /*
     * The following is used to handle the case of unknown uniquifier. We
     * just need a valid reference to the volume to direct the RPC to the
     * right fileserver. Therefore we take the root directory of the volume.
     */
    if (PioctlInputs.fid.Unique == 0) {
	PioctlInputs.int32s[0] = PioctlInputs.fid.Vnode;
	PioctlInputs.fid.Vnode = 1;
	PioctlInputs.fid.Unique = 1;
    }
    return code;
}

int
VLDBInit(int noAuthFlag, struct afsconf_cell *info)
{
    afs_int32 code;

    code = ugen_ClientInit(noAuthFlag, (char *) AFSDIR_CLIENT_ETC_DIRPATH,
                           info->name, 0, &uclient,
                           NULL, pnp, rxkad_clear,
                           VLDB_MAXSERVERS, AFSCONF_VLDBSERVICE, 50,
                           0, 0, USER_SERVICE_ID);
    rxInitDone = 1;
    return code;
}

afs_int32
get_vnode_hosts(char *fname, char **cellp, afs_int32 *hosts, AFSFid *Fid,
		int onlyRW)
{
    struct afsconf_dir *tdir;
    struct vldbentry vldbEntry;
    afs_int32 i, j, code, *h, len;
    struct afsconf_cell info;
    afs_int32 mask;

    tdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);
    if (!tdir) {
        fprintf(stderr,"Could not process files in configuration directory "
		"(%s).\n",AFSDIR_CLIENT_ETC_DIRPATH);
        return -1;
    }
    if (!*cellp) {
	len = MAXCELLCHARS;
	*cellp = (char *) malloc(MAXCELLCHARS);
	code = afsconf_GetLocalCell(tdir, *cellp, len);
	if (code) return code;
    }
    code = afsconf_GetCellInfo(tdir, *cellp, AFSCONF_VLDBSERVICE, &info);
    if (code) {
        fprintf(stderr,"fs: cell %s not in %s/CellServDB\n",
		*cellp, AFSDIR_CLIENT_ETC_DIRPATH);
        return code;
    }

    i = sscanf(fname, "%u.%u.%u", &Fid->Volume, &Fid->Vnode, &Fid->Unique);
    if (i != 3) {
	fprintf(stderr,"fs: invalid vnode triple: %s\n", fname);
	return 1;
    }
    code = VLDBInit(1, &info);
    if (code == 0) {
        code = ubik_VL_GetEntryByID(uclient, 0, Fid->Volume,
				    -1, &vldbEntry);
        if (code == VL_NOENT)
            fprintf(stderr,"fs: volume %u does not exist in this cell.\n",
		    Fid->Volume);
        if (code) return code;
    }
    h = hosts;
    mask = VLSF_RWVOL;
    if (!onlyRW) mask |= VLSF_RWVOL;
    for (i=0, j=0; j<vldbEntry.nServers; j++) {
        if (vldbEntry.serverFlags[j] & mask) {
            *h++ = ntohl(vldbEntry.serverNumber[j]);
            i++;
        }
    }
    for (; i<AFS_MAXHOSTS; i++) *h++ = 0;
    return 0;
}

/* get_file_cell()
 *     Determine which AFS cell file 'fn' lives in, the list of servers that
 *     offer it, and the FID.
 */
afs_int32
get_file_cell(char *fn, char **cellp, afs_int32 hosts[AFS_MAXHOSTS], AFSFid *Fid,
	      struct AFSFetchStatus *Status, afs_int32 create)
{
    afs_int32 code;
    char buf[256];
    struct ViceIoctl status;
    int j;
    afs_int32 *Tmpafs_int32;

    memset( Status, 0, sizeof(struct AFSFetchStatus));
    memset(buf, 0, sizeof(buf));
    status.in_size = 0;
    status.out_size = sizeof(buf);
    status.in = buf;
    status.out = buf;
    errno = 0;
    code = pioctl(fn, VIOC_FILE_CELL_NAME, &status, 0);
    if (code && create) {
	char *c;
	int fd;
	strcpy(buf,fn);
#ifdef AFS_NT40_ENV
        c = strrchr(buf,'\\');
#else
        c = strrchr(buf,'/');
#endif
	if (c) {
	    *c = 0;
	    code = pioctl(buf,VIOC_FILE_CELL_NAME, &status, 0);
	    if (!code) {
		fd = open(fn, O_CREAT, 0644);
		close(fd);
	    }
	    code = pioctl(fn, VIOC_FILE_CELL_NAME, &status, 0);
	}
    }
    if (code) {
	fprintf(stderr, "Unable to determine cell for %s\n", fn);
	if (errno) {
	    perror(fn);
	    if (errno == EINVAL)
		fprintf(stderr, "(File might not be in AFS)\n");
	} else
	    afs_com_err(pnp, code, (char *) 0);
    } else {
	*cellp = (char *) malloc(strlen(buf)+1);
	strcpy(*cellp, buf);
	SetCellFname(*cellp);
	memset(buf, 0, sizeof(buf));
	status.in = 0;
	status.in_size = 0;
	status.out = buf;
	status.out_size = sizeof(buf);
	code = pioctl(fn, VIOCWHEREIS, &status, 0);
	if (code) {
	    fprintf(stderr, "Unable to determine fileservers for %s\n", fn);
	    if (errno) {
		perror(fn);
	    }
	    else
	        afs_com_err(pnp, code, (char *) 0);
	} else {
	    Tmpafs_int32 = (afs_int32 *)buf;
	    for (j=0;j<AFS_MAXHOSTS;++j) {
		hosts[j] = Tmpafs_int32[j];
		if (!Tmpafs_int32[j])
		    break;
	    }
	}
	memset(buf, 0, sizeof(buf));
	status.in_size = 0;
	status.out_size = sizeof(buf);
	status.in = 0;
	status.out = buf;
	code = pioctl(fn, VIOCGETFID, &status, 0);
	if (code) {
	    fprintf(stderr, "Unable to determine FID for %s\n", fn);
	    if (errno) {
		perror(fn);
	    } else {
	        afs_com_err(pnp, code, (char *) 0);
	    }
	} else {
	    Tmpafs_int32 = (afs_int32 *)buf;
	    Fid->Volume = Tmpafs_int32[1];
	    Fid->Vnode = Tmpafs_int32[2];
	    Fid->Unique = Tmpafs_int32[3];
	}
    }
    return code;
}

int
DestroyConnections(void)
{
    int i;

    if (!ConnLookupInitialized) return 0;
    for (i = 0; i < MAX_HOSTS; i++) {
        if (!ConnLookup[i].conn) break;
	RXAFS_GiveUpAllCallBacks(ConnLookup[i].conn);
	rx_DestroyConnection(ConnLookup[i].conn);
    }
    if (!rxInitDone)
	rx_Finalize();
    return 0;
}


int
LogErrors (int level, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    return vfprintf(stderr, fmt, ap);
}

int
readFile(struct cmd_syndesc *as, void *unused)
{
    char *fname;
    char *cell = 0;
    afs_int32 code;
    afs_int32 hosts[AFS_MAXHOSTS];
    AFSFid Fid;
    int j;
    struct rx_connection *RXConn;
    struct cellLookup *cl;
    struct rx_call *tcall;
    struct AFSVolSync tsync;
    struct AFSFetchStatus OutStatus;
    struct AFSCallBack CallBack;
    afs_int64 Pos;
    afs_int32 len;
    afs_int64 length, Len;
    u_char vnode = 0;
    u_char first = 1;
    int bytes;
    int worstCode = 0;
    char *buf = 0;
    int bufflen = BUFFLEN;

#ifdef AFS_NT40_ENV
    /* stdout on Windows defaults to _O_TEXT mode */
    _setmode(1, _O_BINARY);
#endif

    if (as->name[0] == 'f')
	vnode = 1;
    if (as->parms[2].items)
	verbose = 1;
    if (as->parms[3].items) {
	md5sum = 1;
	MD5_Init(&md5);
    }

    CBServiceNeeded = 1;
    InitializeCBService();

    gettimeofday (&starttime, &Timezone);
    fname = as->parms[0].items->data;
    cell = 0;
    if (as->parms[1].items)
	cell = as->parms[1].items->data;
    if (vnode)
	code = get_vnode_hosts(fname, &cell, hosts, &Fid, 1);
    else
        code = get_file_cell(fname, &cell, hosts, &Fid, &OutStatus, 0);
    if (code) {
        fprintf(stderr,"File not found %s\n", fname);
        return code;
    }
    if (Fid.Vnode & 1) {
	fprintf(stderr,"%s is a directory, not a file\n", fname);
	return ENOENT;
    }
    cl = FindCell(cell);
    for (j=0;j<AFS_MAXHOSTS;++j) {
	int useHost;

        if (first && as->parms[6].items) {
	    afs_uint32 fields, ip1, ip2, ip3, ip4;
	    fields = sscanf(as->parms[6].items->data, "%d.%d.%d.%d",
			    &ip1, &ip2, &ip3, &ip4);
	    useHost = (ip1 << 24) + (ip2 << 16) + (ip3 << 8) + ip4;
	    j--;
        } else {
            if (!hosts[j])
                break;
	    useHost = hosts[j];
	}
	first = 0;
        RXConn = FindRXConnection(useHost, htons(AFSCONF_FILEPORT), 1,
				  cl->sc, cl->scIndex);
        if (!RXConn) {
            fprintf(stderr,"rx_NewConnection failed to server 0x%X\n",
                    useHost);
            continue;
        }
        code = AFS_FetchStatus(RXConn, &Fid, &OutStatus, &CallBack, &tsync);
        if (code) {
           fprintf(stderr,"RXAFS_FetchStatus failed to server 0x%X for"
                   " file %s, code was %d\n",
		   useHost, fname, code);
	   continue;
	}
        gettimeofday(&opentime, &Timezone);
	if (verbose) {
            seconds = (float)(opentime.tv_sec + opentime.tv_usec *.000001
		-starttime.tv_sec - starttime.tv_usec *.000001);
	    fprintf(stderr,"Startup to find the file took %.3f sec.\n",
		    seconds);
	}
	Len = OutStatus.Length_hi;
	Len <<= 32;
	Len += OutStatus.Length;
	ZeroInt64(Pos);
	{
	    afs_uint32 high, low;

            tcall = rx_NewCall(RXConn);
            code = StartAFS_FetchData64 (tcall, &Fid, Pos, Len);
            if (code == RXGEN_OPCODE) {
	        afs_int32 tmpPos,  tmpLen;
	        tmpPos = (afs_int32)Pos; tmpLen = (afs_int32)Len;
                code = StartAFS_FetchData (tcall, &Fid, tmpPos, tmpLen);
	        bytes = rx_Read(tcall, (char *)&low, sizeof(afs_int32));
		length = ntohl(low);
	        if (bytes != 4) code = -3;
            } else if (!code) {
                bytes = rx_Read(tcall, (char *)&high, 4);
	        length = ntohl(high);
		length <<= 32;
                bytes += rx_Read(tcall, (char *)&low, 4);
		length += ntohl(low);
	        if (bytes != 8) code = -3;
            }
            if (code) {
                if (code == RXGEN_OPCODE) {
                    fprintf(stderr, "File server for %s might not be running a"
			    " multi-resident AFS server\n",
			    fname);
		} else {
                    fprintf(stderr, "%s for %s ended with error code %d\n",
                            (char *) &as->name, fname, code);
		    exit(1);
	        }
            }
	    if (length > bufflen)
		len = bufflen;
	    else
		len = (afs_int32) length;
	    buf = (char *)malloc(len);
	    if (!buf) {
	        fprintf(stderr, "couldn't allocate buffer\n");
	        exit(1);
	    }
	    while (!code && NonZeroInt64(length)) {
	        if (length > bufflen)
		    len = bufflen;
	        else
		    len = (afs_int32) length;
	        bytes = rx_Read(tcall, (char *) buf, len);
	        if (bytes != len) {
		    code = -3;
	        }
		if (md5sum)
	    	    MD5_Update(&md5, buf, len);
	        if (!code)
		    write(1, buf, len);
	        length -= len;
		xfered += len;
		gettimeofday(&now, &Timezone);
	        if (verbose)
		    printDatarate();
	    }
	    worstCode = code;
	    code = EndRXAFS_FetchData (tcall, &OutStatus, &CallBack, &tsync);
	    rx_EndCall(tcall, 0);
	    if (!worstCode)
		worstCode = code;
	}
        break;
    }
    gettimeofday(&readtime, &Timezone);
    if (worstCode) {
	fprintf(stderr,"%s failed with code %d\n",
		(char *) &as->name, worstCode);
    } else {
        if (md5sum) {
	    afs_uint32 md5int[4];
	    char *p;
	    MD5_Final((char *) &md5int[0], &md5);
#ifdef AFS_NT40_ENV
            p = strrchr(fname,'\\');
#else
            p = strrchr(fname,'/');
#endif
            if (p)
                p++;
	    else
                p = fname;

	    fprintf(stderr, "%08x%08x%08x%08x  %s\n",
                    htonl(md5int[0]), htonl(md5int[1]),
		    htonl(md5int[2]), htonl(md5int[3]), p);
        }
	if(verbose) {
            seconds = (float)(readtime.tv_sec + readtime.tv_usec *.000001
		-opentime.tv_sec - opentime.tv_usec *.000001);
            fprintf(stderr,"Transfer of %llu bytes took %.3f sec.\n",
		    xfered, seconds);
            datarate = (xfered >> 20) / seconds;
            fprintf(stderr,"Total data rate = %.03f MB/sec. for read\n",
		    datarate);
	}
    }
    DestroyConnections();
    return worstCode;
}

int
writeFile(struct cmd_syndesc *as, void *unused)
{
    char *fname = NULL;
    char *cell = 0;
    afs_int32 code, localcode = 0;
    afs_int32 hosts[AFS_MAXHOSTS];
    afs_uint32 useHost;
    AFSFid Fid;
    struct rx_connection *RXConn;
    struct cellLookup *cl;
    struct rx_call *tcall;
    struct AFSVolSync tsync;
    struct AFSFetchStatus OutStatus;
    struct AFSStoreStatus InStatus;
    struct AFSCallBack CallBack;
    afs_int64 Pos;
    afs_int64 length, Len, synthlength = 0, offset = 0;
    u_char vnode = 0;
    afs_int64 bytes;
    int worstCode = 0;
    int append = 0;
    int synthesize = 0;
    afs_int32 byteswritten;
    struct wbuf *bufchain = 0;
    struct wbuf *previous, *tbuf;

#ifdef AFS_NT40_ENV
    /* stdin on Windows defaults to _O_TEXT mode */
    _setmode(0, _O_BINARY);
#endif

    if (as->name[0] == 'f') {
	vnode = 1;
        if (as->name[3] == 'a')
	    append = 1;
    } else
        if (as->name[0] == 'a')
	    append = 1;
    if (as->parms[2].items)
	verbose = 1;
    if (as->parms[3].items)
	md5sum = 1;
    if (as->parms[4].items) {
	code = util_GetInt64(as->parms[4].items->data, &synthlength);
	if (code) {
	    fprintf(stderr, "Invalid value for synthesize length %s\n",
		    as->parms[4].items->data);
	    return code;
	}
	synthesize = 1;
    }
    CBServiceNeeded = 1;
    InitializeCBService();

    if (as->parms[0].items)
	fname = as->parms[0].items->data;

    cell = 0;
    if (as->parms[1].items) cell = as->parms[1].items->data;
    if (vnode) {
	code = get_vnode_hosts(fname, &cell, hosts, &Fid, 1);
	if (code)
	    return code;
    } else
        code = get_file_cell(fname, &cell, hosts, &Fid, &OutStatus, append ? 0 : 1);
    if (code) {
        fprintf(stderr,"File or directory not found: %s\n",
                    fname);
        return code;
    }
    if (Fid.Vnode & 1) {
	fprintf(stderr,"%s is a directory, not a file\n", fname);
	return ENOENT;
    }
    if (!hosts[0]) {
	fprintf(stderr,"AFS file not found: %s\n", fname);
	return ENOENT;
    }
    cl = FindCell(cell);
    gettimeofday (&starttime, &Timezone);
    useHost = hosts[0];
    RXConn = FindRXConnection(useHost, htons(AFSCONF_FILEPORT), 1,
			      cl->sc, cl->scIndex);
    if (!RXConn) {
        fprintf(stderr,"rx_NewConnection failed to server 0x%X\n",
		hosts[0]);
        return -1;
    }
    code = AFS_FetchStatus(RXConn, &Fid, &OutStatus, &CallBack, &tsync);
    if (code) {
        fprintf(stderr,"RXAFS_FetchStatus failed to server 0x%X for file %s, code was%d\n",
                            useHost, fname, code);
       return -1;
    }
    if (!append && (OutStatus.Length || OutStatus.Length_hi)) {
        fprintf(stderr,"AFS file %s not empty, request aborted.\n", fname);
	DestroyConnections();
        return -5;
    }
    InStatus.Mask = AFS_SETMODE + AFS_FSYNC;
    InStatus.UnixModeBits = 0644;
    if (append) {
	Pos = OutStatus.Length_hi;
	Pos = (Pos << 32) | OutStatus.Length;
    } else
        Pos = 0;
    previous = (struct wbuf *)&bufchain;
    if (md5sum)
	MD5_Init(&md5);

    Len = 0;
    while (Len<WRITEBUFFLEN) {
	tbuf = (struct wbuf *)malloc(sizeof(struct wbuf));
	if (!tbuf) {
	    if (!bufchain) {
		fprintf(stderr, "Couldn't allocate buffer, aborting\n");
		exit(1);
	    }
	    break;
	}
	memset(tbuf, 0, sizeof(struct wbuf));
	tbuf->buflen = BUFFLEN;
	if (synthesize) {
	    afs_int64 ll, l = tbuf->buflen;
	    if (l > synthlength)
		l = synthlength;
	    for (ll = 0; ll < l; ll += 4096) {
                sprintf(&tbuf->buf[ll],"Offset (0x%x, 0x%x)\n",
			(unsigned int)((offset + ll) >> 32),
			(unsigned int)((offset + ll) & 0xffffffff));
	    }
	    offset += l;
	    synthlength -= l;
	    tbuf->used = (afs_int32)l;
	} else
	    tbuf->used = read(0, &tbuf->buf, tbuf->buflen);
	if (!tbuf->used) {
	    free(tbuf);
	    break;
	}
	if (md5sum)
	    MD5_Update(&md5, &tbuf->buf, tbuf->used);
	previous->next = tbuf;
	previous = tbuf;
	Len += tbuf->used;
    }
    gettimeofday(&opentime, &Timezone);
    if (verbose) {
        seconds = (float) (opentime.tv_sec + opentime.tv_usec *.000001
	    -starttime.tv_sec - starttime.tv_usec *.000001);
        fprintf(stderr,"Startup to find the file took %.3f sec.\n",
		seconds);
    }
    bytes = Len;
    while (!code && bytes) {
        afs_int32 code2;
        Len = bytes;
    restart:
        tcall = rx_NewCall(RXConn);
        code = StartAFS_StoreData64 (tcall, &Fid, &InStatus, Pos, Len, Pos+Len);
        if (code == RXGEN_OPCODE) {
	    afs_uint32 tmpLen, tmpPos;
	    tmpPos = (afs_int32) Pos;
	    tmpLen = (afs_int32) Len;
	    if (Pos+Len > 0x7fffffff) {
	        fprintf(stderr,"AFS fileserver does not support files >= 2 GB\n");
	        return EFBIG;
	    }
	    code = StartAFS_StoreData (tcall, &Fid, &InStatus, tmpPos, tmpLen,
				       tmpPos+tmpLen);
        }
        if (code) {
            fprintf(stderr, "StartRXAFS_StoreData had error code %d\n", code);
	    return code;
        }
        length = Len;
	tbuf = bufchain;
	if (Len) {
            for (tbuf= bufchain; tbuf; tbuf=tbuf->next) {
	        if (!tbuf->used)
		    break;
	        byteswritten = rx_Write(tcall, tbuf->buf, tbuf->used);
	        if (byteswritten != tbuf->used) {
	            fprintf(stderr,"Only %d instead of %" AFS_INT64_FMT " bytes transferred by rx_Write()\n", byteswritten, length);
	            fprintf(stderr, "At %" AFS_UINT64_FMT " bytes from the end\n", length);
	            code = -4;
		    break;
	        }
		xfered += tbuf->used;
		gettimeofday(&now, &Timezone);
	        if (verbose)
		    printDatarate();
	        length -= tbuf->used;
            }
	}
        worstCode = code;
        code = EndRXAFS_StoreData64 (tcall, &OutStatus, &tsync);
	if (code) {
	    fprintf(stderr, "EndRXAFS_StoreData64 returned %d\n", code);
            worstCode = code;
        }
        code2 = rx_Error(tcall);
	if (code2) {
	    fprintf(stderr, "rx_Error returned %d\n", code2);
            worstCode = code2;
        }
        code2 = rx_EndCall(tcall, localcode);
	if (code2) {
	    fprintf(stderr, "rx_EndCall returned %d\n", code2);
            worstCode = code2;
        }
	code = worstCode;
	if (code == 110) {
	    fprintf(stderr, "Waiting for busy volume\n");
	    sleep(10);
	    goto restart;
	}
	Pos += Len;
	bytes = 0;
	if (!code) {
            for (tbuf = bufchain; tbuf; tbuf=tbuf->next) {
	        tbuf->offset = 0;
		if (synthesize) {
	    	    afs_int64 ll, l = tbuf->buflen;
	    	    if (l > synthlength)
			l = synthlength;
	     	    for (ll = 0; ll < l; ll += 4096) {
			sprintf(&tbuf->buf[ll],"Offset (0x%x, 0x%x)\n",
				(unsigned int)((offset + ll) >> 32),
				(unsigned int)((offset + ll) & 0xffffffff));
	    	    }
	    	    offset += l;
	    	    synthlength -= l;
	    	    tbuf->used = (afs_int32) l;
		} else
	            tbuf->used = read(0, &tbuf->buf, tbuf->buflen);
	        if (!tbuf->used)
	            break;
		if (md5sum)
	    	    MD5_Update(&md5, &tbuf->buf, tbuf->used);
	        Len += tbuf->used;
		bytes += tbuf->used;
            }
        }
    }
    gettimeofday(&writetime, &Timezone);
    if (worstCode) {
	fprintf(stderr,"%s failed with code %d\n", as->name, worstCode);
    } else if(verbose) {
        seconds = (float) (writetime.tv_sec + writetime.tv_usec *.000001
	    -opentime.tv_sec - opentime.tv_usec *.000001);
        fprintf(stderr,"Transfer of %llu bytes took %.3f sec.\n",
		xfered, seconds);
        datarate = (xfered >> 20) / seconds;
        fprintf(stderr,"Total data rate = %.03f MB/sec. for write\n",
		datarate);
    }
    while (bufchain) {
	tbuf = bufchain;
	bufchain = tbuf->next;
	free(tbuf);
    }
    DestroyConnections();
    if (md5sum) {
	afs_uint32 md5int[4];
	char *p;
	MD5_Final((char *) &md5int[0], &md5);
#ifdef AFS_NT40_ENV
        p = strrchr(fname,'\\');
#else
        p = strrchr(fname,'/');
#endif
        if (p)
            p++;
	else
            p = fname;

        fprintf(stderr, "%08x%08x%08x%08x  %s\n",
		htonl(md5int[0]), htonl(md5int[1]),
		htonl(md5int[2]), htonl(md5int[3]), p);
    }
    return worstCode;
}

struct cellLookup *
FindCell(char *cellName)
{
    char name[MAXCELLCHARS];
    char *np;
    struct cellLookup *p, *p2;
    static struct afsconf_dir *tdir;
    time_t expires;
    afs_int32 len, code;

    if (cellName) {
	np = cellName;
    } else {
        if (!tdir)
	    tdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);
	len = MAXCELLCHARS;
	afsconf_GetLocalCell(tdir, name, len);
	np = (char *) &name;
    }
    SetCellFname(np);

    p2 = (struct cellLookup *) &Cells;
    for (p = Cells; p; p = p->next) {
	if (!strcmp((char *)&p->info.name, np)) {
#ifdef NO_AFS_CLIENT
	    if (!strcmp((char *)&lastcell, np))
	        code = VLDBInit(1, &p->info);
#endif
	    return p;
	}
	p2 = p;
    }
    p2->next = (struct cellLookup *) malloc(sizeof(struct cellLookup));
    p = p2->next;
    memset(p, 0, sizeof(struct cellLookup));
    p->next = (struct cellLookup *) 0;
    if (!tdir)
	tdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);
    if (afsconf_GetCellInfo(tdir, np, AFSCONF_VLDBSERVICE, &p->info)) {
	p2->next = (struct cellLookup *) 0;
	free(p);
	p = (struct cellLookup *) 0;
    } else {
#ifdef NO_AFS_CLIENT
	if (code = VLDBInit(1, &p->info))
            fprintf(stderr,"VLDBInit failed for cell %s\n", p->info.name);
#endif
	code = afsconf_ClientAuthToken(&p->info, 0, &p->sc, &p->scIndex, &expires);
	if (code) {
	    p->scIndex = RX_SECIDX_NULL;
            p->sc = rxnull_NewClientSecurityObject();
	}
    }

    if (p)
        return p;
    else
	return 0;
}

struct rx_connection *
FindRXConnection(afs_uint32 host, u_short port, u_short service,
		 struct rx_securityClass *securityObject,
		 int serviceSecurityIndex)
{
    int i;

    if (!ConnLookupInitialized) {
	memset(ConnLookup, 0, MAX_HOSTS * sizeof(struct connectionLookup));
	ConnLookupInitialized = 1;
    }

    for (i = 0; i < MAX_HOSTS; i++) {
        if ((ConnLookup[i].host == host) && (ConnLookup[i].port == port))
	    return ConnLookup[i].conn;
	if (!ConnLookup[i].conn)
	    break;
    }

    if (i >= MAX_HOSTS)
	return 0;

    ConnLookup[i].conn = rx_NewConnection(host, port, service, securityObject, serviceSecurityIndex);
    if (ConnLookup[i].conn) {
	ConnLookup[i].host = host;
	ConnLookup[i].port = port;
    }

    return ConnLookup[i].conn;
}
