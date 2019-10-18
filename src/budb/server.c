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
#include <afs/stds.h>

#include <roken.h>

#ifdef AFS_NT40_ENV
#include <WINNT/afsevent.h>
#endif
#include <afs/cmd.h>
#include <lwp.h>
#include <ubik.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rxkad.h>
#include <rx/rx_globals.h>
#include <afs/cellconfig.h>
#include <afs/bubasics.h>
#include <afs/afsutil.h>
#include <afs/com_err.h>
#include <afs/audit.h>

#include "budb_errs.h"
#include "database.h"
#include "error_macros.h"
#include "budb_internal.h"
#include "globals.h"

struct ubik_dbase *BU_dbase;
struct afsconf_dir *BU_conf;	/* for getting cell info */

int argHandler(struct cmd_syndesc *, void *);
int truncateDatabase(void);
int parseServerList(struct cmd_item *);

char lcell[MAXKTCREALMLEN];
afs_uint32 myHost = 0;
int helpOption;
static struct logOptions logopts;

/* server's global configuration information. This is exported to other
 * files/routines
 */

buServerConfT globalConf;
buServerConfP globalConfPtr = &globalConf;
char dbDir[AFSDIR_PATH_MAX], cellConfDir[AFSDIR_PATH_MAX];
/* debugging control */
int debugging = 0;

int rxBind = 0;
int lwps   = 3;

#define MINLWP  3
#define MAXLWP 16

#define ADDRSPERSITE 16         /* Same global is in rx/rx_user.c */
afs_uint32 SHostAddrs[ADDRSPERSITE];

/* check whether caller is authorized to manage RX statistics */
int
BU_rxstat_userok(struct rx_call *call)
{
    return afsconf_SuperUser(BU_conf, call, NULL);
}

/**
 * Return true if this name is a member of the local realm.
 */
int
BU_IsLocalRealmMatch(void *rock, char *name, char *inst, char *cell)
{
    struct afsconf_dir *dir = (struct afsconf_dir *)rock;
    afs_int32 islocal = 0;	/* default to no */
    int code;

    code = afsconf_IsLocalRealmMatch(dir, &islocal, name, inst, cell);
    if (code) {
	LogError(code, "Failed local realm check; name=%s, inst=%s, cell=%s\n",
		 name, inst, cell);
    }
    return islocal;
}

int
convert_cell_to_ubik(struct afsconf_cell *cellinfo, afs_uint32 *myHost,
		     afs_uint32 *serverList)
{
    int i;
    char hostname[64];
    struct hostent *th;

    /* get this host */
    gethostname(hostname, sizeof(hostname));
    th = gethostbyname(hostname);
    if (!th) {
	printf("prserver: couldn't get address of this host.\n");
	BUDB_EXIT(1);
    }
    memcpy(myHost, th->h_addr, sizeof(afs_uint32));

    for (i = 0; i < cellinfo->numServers; i++)
	/* omit my host from serverList */
	if (cellinfo->hostAddr[i].sin_addr.s_addr != *myHost)
	    *serverList++ = cellinfo->hostAddr[i].sin_addr.s_addr;

    *serverList = 0;		/* terminate list */
    return 0;
}

/* MyBeforeProc
 *      The whole purpose of MyBeforeProc is to detect
 *      if the -help option was not within the command line.
 *      If it were, this routine would never have been called.
 */
static int
MyBeforeProc(struct cmd_syndesc *as, void *arock)
{
    helpOption = 0;
    return 0;
}

/* initializeCommands
 *	initialize all the supported commands and their arguments
 */

void
initializeArgHandler(void)
{
    struct cmd_syndesc *cptr;

    cmd_SetBeforeProc(MyBeforeProc, NULL);

    cptr = cmd_CreateSyntax(NULL, argHandler, NULL, 0, "Backup database server");

    cmd_AddParm(cptr, "-database", CMD_SINGLE, CMD_OPTIONAL,
		"database directory");

    cmd_AddParm(cptr, "-cellservdb", CMD_SINGLE, CMD_OPTIONAL,
		"cell configuration directory");

    cmd_AddParm(cptr, "-resetdb", CMD_FLAG, CMD_OPTIONAL,
		"truncate the database");

    cmd_AddParm(cptr, "-noauth", CMD_FLAG, CMD_OPTIONAL,
		"run without authentication");

    cmd_AddParm(cptr, "-smallht", CMD_FLAG, CMD_OPTIONAL,
		"use small hash tables");

    cmd_AddParm(cptr, "-servers", CMD_LIST, CMD_OPTIONAL,
		"list of ubik database servers");

    cmd_AddParm(cptr, "-ubikbuffers", CMD_SINGLE, CMD_OPTIONAL,
		"the number of ubik buffers");

    cmd_AddParm(cptr, "-auditlog", CMD_SINGLE, CMD_OPTIONAL,
		"audit log path");

    cmd_AddParm(cptr, "-p", CMD_SINGLE, CMD_OPTIONAL,
		"number of processes");

    cmd_AddParm(cptr, "-rxbind", CMD_FLAG, CMD_OPTIONAL,
		"bind the Rx socket (primary interface only)");

    cmd_AddParm(cptr, "-audit-interface", CMD_SINGLE, CMD_OPTIONAL,
		"audit interface (file or sysvmq)");

    cmd_AddParm(cptr, "-transarc-logs", CMD_FLAG, CMD_OPTIONAL,
		"enable Transarc style logging");
}

int
argHandler(struct cmd_syndesc *as, void *arock)
{

    /* globalConfPtr provides the handle for the configuration information */

    /* database directory */
    if (as->parms[0].items != 0) {
	globalConfPtr->databaseDirectory = strdup(as->parms[0].items->data);
	if (globalConfPtr->databaseDirectory == 0)
	    BUDB_EXIT(-1);
    }

    /* -cellservdb, cell configuration directory */
    if (as->parms[1].items != 0) {
	globalConfPtr->cellConfigdir = strdup(as->parms[1].items->data);
	if (globalConfPtr->cellConfigdir == 0)
	    BUDB_EXIT(-1);

	globalConfPtr->debugFlags |= DF_RECHECKNOAUTH;
    }

    /* truncate the database */
    if (as->parms[2].items != 0)
	truncateDatabase();

    /* run without authentication */
    if (as->parms[3].items != 0)
	globalConfPtr->debugFlags |= DF_NOAUTH;

    /* use small hash tables */
    if (as->parms[4].items != 0)
	globalConfPtr->debugFlags |= DF_SMALLHT;

    /* user provided list of ubik database servers */
    if (as->parms[5].items != 0)
	parseServerList(as->parms[5].items);

    /* user provided the number of ubik buffers    */
    if (as->parms[6].items != 0)
	ubik_nBuffers = atoi(as->parms[6].items->data);
    else
	ubik_nBuffers = 0;

    /* param 7 (-auditlog) handled below */

    /* user provided the number of threads    */
    if (as->parms[8].items != 0) {
	lwps = atoi(as->parms[8].items->data);
	if (lwps > MAXLWP) {
	    printf ("Warning: '-p %d' is too big; using %d instead\n",
		lwps, MAXLWP);
	    lwps = MAXLWP;
	}
	if (lwps < MINLWP) {
	    printf ("Warning: '-p %d' is too small; using %d instead\n",
		lwps, MINLWP);
	    lwps = MINLWP;
	}
    }

    /* user provided rxbind option    */
    if (as->parms[9].items != 0) {
	rxBind = 1;
    }

    /* -audit-interface */
    if (as->parms[10].items != 0) {
	char *interface = as->parms[10].items->data;

	if (osi_audit_interface(interface)) {
	    printf("Invalid audit interface '%s'\n", interface);
	    BUDB_EXIT(-1);
	}
    }
    /* -transarc-logs */
    if (as->parms[11].items != 0) {
	logopts.lopt_rotateOnOpen = 1;
	logopts.lopt_rotateStyle = logRotate_old;
    }

    /* -auditlog */
    /* needs to be after -audit-interface, so we osi_audit_interface
     * before we osi_audit_file */
    if (as->parms[7].items != 0) {
	char *fileName = as->parms[7].items->data;

        osi_audit_file(fileName);
    }

    return 0;
}

/* --- */

int
parseServerList(struct cmd_item *itemPtr)
{
    struct cmd_item *save;
    char **serverArgs;
    char **ptr;
    afs_int32 nservers = 0;
    afs_int32 code = 0;

    save = itemPtr;

    /* compute number of servers in the list */
    while (itemPtr) {
	nservers++;
	itemPtr = itemPtr->next;
    }

    LogDebug(3, "%d servers\n", nservers);

    /* now can allocate the space for the server arguments */
    serverArgs = malloc((nservers + 2) * sizeof(char *));
    if (serverArgs == 0)
	ERROR(-1);

    ptr = serverArgs;
    *ptr++ = "";
    *ptr++ = "-servers";

    /* now go through and construct the list of servers */
    itemPtr = save;
    while (itemPtr) {
	*ptr++ = itemPtr->data;
	itemPtr = itemPtr->next;
    }

    code =
	ubik_ParseServerList(nservers + 2, serverArgs, &globalConfPtr->myHost,
			     globalConfPtr->serverList);
    if (code)
	ERROR(code);

    /* free space for the server args */
    free(serverArgs);

  error_exit:
    return (code);
}

/* truncateDatabase
 *	truncates just the database file.
 */

int
truncateDatabase(void)
{
    char *path;
    afs_int32 code = 0;
    int fd,r;

    r = asprintf(&path, "%s%s%s",
		 globalConfPtr->databaseDirectory,
		 globalConfPtr->databaseName,
		 globalConfPtr->databaseExtension);
    if (r < 0 || path == NULL)
	ERROR(-1);

    fd = open(path, O_RDWR, 0755);
    if (!fd) {
	code = errno;
    } else {
	if (ftruncate(fd, 0) != 0) {
	    code = errno;
	} else
	    close(fd);
    }

    free(path);

  error_exit:
    return (code);
}


/* --- */

#include "AFS_component_version_number.c"

int
main(int argc, char **argv)
{
    char *whoami = argv[0];
    char *dbNamePtr = 0;
    struct afsconf_cell cellinfo_s;
    struct afsconf_cell *cellinfo = NULL;
    time_t currentTime;
    afs_int32 code = 0;
    afs_uint32 host = ntohl(INADDR_ANY);
    int r;

    char  clones[MAXHOSTSPERCELL];

    struct rx_service *tservice;
    struct rx_securityClass **securityClasses;
    afs_int32 numClasses;

    extern int rx_stackSize;

#ifdef AFS_NT40_ENV
    /* initialize winsock */
    if (afs_winsockInit() < 0) {
	ReportErrorEventAlt(AFSEVT_SVR_WINSOCK_INIT_FAILED, 0, argv[0], 0);
	fprintf(stderr, "%s: Couldn't initialize winsock.\n", whoami);
	exit(1);
    }
#endif

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
    sigaction(SIGABRT, &nsa, NULL);
#endif

    memset(&cellinfo_s, 0, sizeof(cellinfo_s));
    memset(clones, 0, sizeof(clones));

    memset(&logopts, 0, sizeof(logopts));
    logopts.lopt_dest = logDest_file;
    logopts.lopt_filename = AFSDIR_SERVER_BUDBLOG_FILEPATH;

    osi_audit_init();
    osi_audit(BUDB_StartEvent, 0, AUD_END);

    initialize_BUDB_error_table();
    initializeArgHandler();

    /* Initialize dirpaths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
#ifdef AFS_NT40_ENV
	ReportErrorEventAlt(AFSEVT_SVR_NO_INSTALL_DIR, 0, argv[0], 0);
#endif
	afs_com_err(whoami, errno, "; Unable to obtain AFS server directory.");
	exit(2);
    }

    memset(globalConfPtr, 0, sizeof(*globalConfPtr));

    /* set default configuration values */
    strcpy(dbDir, AFSDIR_SERVER_DB_DIRPATH);
    strcat(dbDir, "/");
    globalConfPtr->databaseDirectory = dbDir;
    globalConfPtr->databaseName = DEFAULT_DBPREFIX;
    strcpy(cellConfDir, AFSDIR_SERVER_ETC_DIRPATH);
    globalConfPtr->cellConfigdir = cellConfDir;

    srandom(1);

#ifdef AFS_PTHREAD_ENV
    SetLogThreadNumProgram( rx_GetThreadNum );
#endif

    /* process the user supplied args */
    helpOption = 1;
    code = cmd_Dispatch(argc, argv);
    if (code)
	ERROR(code);

    /* exit if there was a help option */
    if (helpOption)
	BUDB_EXIT(0);

    /* open the log file */
    OpenLog(&logopts);

    /* open the cell's configuration directory */
    LogDebug(4, "opening %s\n", globalConfPtr->cellConfigdir);

    BU_conf = afsconf_Open(globalConfPtr->cellConfigdir);
    if (BU_conf == 0) {
	LogError(code, "Failed getting cell info\n");
	afs_com_err(whoami, code, "Failed getting cell info");
	ERROR(BUDB_NOCELLS);
    }

    if (afsconf_CountKeys(BU_conf) == 0) {
	LogError(0, "WARNING: No encryption keys found! "
		    "All authenticated accesses will fail. "
		    "Run akeyconvert or asetkey to import encryption keys.\n");
    }

    code = afsconf_GetLocalCell(BU_conf, lcell, sizeof(lcell));
    if (code) {
	LogError(0, "** Can't determine local cell name!\n");
	ERROR(code);
    }

    if (globalConfPtr->myHost == 0) {
	/* if user hasn't supplied a list of servers, extract server
	 * list from the cell's database
	 */

	cellinfo = &cellinfo_s;

	LogDebug(1, "Using server list from %s cell database.\n", lcell);

	code = afsconf_GetExtendedCellInfo (BU_conf, lcell, 0, cellinfo,
					    clones);
	if (code) {
	    LogError(0, "Can't read cell information\n");
	    ERROR(code);
	}

	code =
	    convert_cell_to_ubik(cellinfo, &globalConfPtr->myHost,
				 globalConfPtr->serverList);
	if (code)
	    ERROR(code);
    }

    /* initialize audit user check */
    osi_audit_set_user_check(BU_conf, BU_IsLocalRealmMatch);

    /* initialize ubik */
    ubik_SetClientSecurityProcs(afsconf_ClientAuth, afsconf_UpToDate, BU_conf);
    ubik_SetServerSecurityProcs(afsconf_BuildServerSecurityObjects,
				afsconf_CheckAuth, BU_conf);

    if (ubik_nBuffers == 0)
	ubik_nBuffers = 400;

    LogError(0, "Will allocate %d ubik buffers\n", ubik_nBuffers);

    r = asprintf(&dbNamePtr, "%s%s", globalConfPtr->databaseDirectory,
		 globalConfPtr->databaseName);
    if (r < 0 || dbNamePtr == 0)
	ERROR(-1);

    rx_SetRxDeadTime(60);	/* 60 seconds inactive before timeout */

    if (rxBind) {
	afs_int32 ccode;
        if (AFSDIR_SERVER_NETRESTRICT_FILEPATH ||
            AFSDIR_SERVER_NETINFO_FILEPATH) {
            char reason[1024];
            ccode = afsconf_ParseNetFiles(SHostAddrs, NULL, NULL,
                                          ADDRSPERSITE, reason,
                                          AFSDIR_SERVER_NETINFO_FILEPATH,
                                          AFSDIR_SERVER_NETRESTRICT_FILEPATH);
        } else
	{
            ccode = rx_getAllAddr(SHostAddrs, ADDRSPERSITE);
        }
        if (ccode == 1) {
            host = SHostAddrs[0];
	    rx_InitHost(host, htons(AFSCONF_BUDBPORT));
	}
    }

    /* Disable jumbograms */
    rx_SetNoJumbo();

    if (cellinfo) {
	code = ubik_ServerInitByInfo(globalConfPtr->myHost,
	                             htons(AFSCONF_BUDBPORT),
	                             cellinfo,
	                             clones,
	                             dbNamePtr,           /* name prefix */
	                             &BU_dbase);
    } else {
	code = ubik_ServerInit(globalConfPtr->myHost, htons(AFSCONF_BUDBPORT),
	                       globalConfPtr->serverList,
	                       dbNamePtr,  /* name prefix */
	                       &BU_dbase);
    }

    if (code) {
	LogError(code, "Ubik init failed\n");
	afs_com_err(whoami, code, "Ubik init failed");
	ERROR(code);
    }

    afsconf_BuildServerSecurityObjects(BU_conf, &securityClasses, &numClasses);

    tservice =
	rx_NewServiceHost(host, 0, BUDB_SERVICE, "BackupDatabase",
			  securityClasses, numClasses, BUDB_ExecuteRequest);

    if (tservice == (struct rx_service *)0) {
	LogError(0, "Could not create backup database rx service\n");
	printf("Could not create backup database rx service\n");
	BUDB_EXIT(3);
    }
    rx_SetMinProcs(tservice, 1);
    rx_SetMaxProcs(tservice, lwps);
    rx_SetStackSize(tservice, 10000);

    /* allow super users to manage RX statistics */
    rx_SetRxStatUserOk(BU_rxstat_userok);

    /* misc. initialization */

    /* database dump synchronization */
    memset(dumpSyncPtr, 0, sizeof(*dumpSyncPtr));
    Lock_Init(&dumpSyncPtr->ds_lock);

    rx_StartServer(0);		/* start handling requests */

    code = InitProcs();
    if (code)
	ERROR(code);


    currentTime = time(0);
    LogError(0, "Ready to process requests at %s\n", ctime(&currentTime));

    rx_ServerProc(NULL);		/* donate this LWP */

  error_exit:
    osi_audit(BUDB_FinishEvent, code, AUD_END);
    return (code);
}

void
consistencyCheckDb(void)
{
    /* do consistency checks on structure sizes */
    if ((sizeof(struct htBlock) > BLOCKSIZE)
	|| (sizeof(struct vfBlock) > BLOCKSIZE)
	|| (sizeof(struct viBlock) > BLOCKSIZE)
	|| (sizeof(struct dBlock) > BLOCKSIZE)
	|| (sizeof(struct tBlock) > BLOCKSIZE)
	) {
	fprintf(stderr, "Block layout error!\n");
	BUDB_EXIT(99);
    }
}

void
LogDebug(int level, char *fmt, ... )
{
    if (debugging >= level) {
	va_list ap;
	va_start(ap, fmt);
	vFSLog(fmt, ap);
	va_end(ap);
    }
}

void
Log(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vFSLog(fmt, ap);
    va_end(ap);
}

void
LogError(long code, char *fmt, ... )
{
    va_list ap;
    int len;
    char buffer[1024];

    va_start(ap, fmt);
    len = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);
    if (len >= 1024) {
	len = 1023;
	buffer[1023] = '\0';
    }
    /* Be consistent with (unintentional?) historic behavior. */
    if (code) {
	FSLog("%s: %s\n", afs_error_table_name(code), afs_error_message(code));
	WriteLogBuffer(buffer, len);
    } else {
	FSLog("%s", buffer);
    }
}
