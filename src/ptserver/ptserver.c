/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *                      A general description of the supergroup changes
 *                      to ptserver:
 *
 *                      In AFS users can be members of groups. When you add
 *                      a user, u1, to a group, g1, you add the user id for u1
 *                      to an entries list in g1. This is a list of all the
 *                      members of g1.
 *                      You also add the id for g1 to an entries list in u1.
 *                      This is a list of all the groups this user is a
 *                      member of.
 *                      The list has room for 10 entries. If more are required,
 *                      a continuation record is created.
 *
 *                      With UMICH mods, u1 can be a group. When u1 is a group
 *                      a new field is required to list the groups this group
 *                      is a member of (since the entries field is used to
 *                      list it's members). This new field is supergroups and
 *                      has two entries. If more are required, a continuation
 *                      record is formed. 
 *                      There are two additional fields required, nextsg is
 *                      an address of the next continuation record for this
 *                      group, and countsg is the count for the number of 
 *                      groups this group is a member of.
 *                   
 *
 *
 *      09/18/95 jjm    Add mdw's changes to afs-3.3a Changes:
 *                      (1) Add the parameter -groupdepth or -depth to
 *                          define the maximum search depth for supergroups.
 *                          Define the variable depthsg to be the value of
 *                          the parameter. The default value is set to 5.
 *
 *                      (3) Make sure the sizes of the struct prentry and
 *                          struct prentryg are equal. If they aren't equal
 *                          the pt database will be corrupted.
 *                          The error is reported with an fprintf statement,
 *                          but this doesn't print when ptserver is started by
 *                          bos, so all you see is an error message in the
 *                          /usr/afs/logs/BosLog file. If you start the
 *                          ptserver without bos the fprintf will print
 *                          on stdout.
 *                          The program will terminate with a PT_EXIT(1).
 *
 *
 *                      Transarc does not currently use opcodes past 520, but
 *                      they *could* decide at any time to use more opcodes.
 *                      If they did, then one part of our local mods,
 *                      ListSupergroups, would break.  I've therefore
 *                      renumbered it to 530, and put logic in to enable the
 *                      old opcode to work (for now).
 *
 *      2/1/98 jjm      Add mdw's changes for bit mapping for supergroups
 *                      Overview:
 *                      Before fetching a supergroup, this version of ptserver
 *                      checks to see if it was marked "found" and "not a
 *                      member".  If and only if so, it doesn't fetch the group.
 *                      Since this should be the case with most groups, this
 *                      should save a significant amount of CPU in redundant
 *                      fetches of the same group.  After fetching the group,
 *                      it sets "found", and either sets or clears "not a
 *                      member", depending on if the group was a member of
 *                      other groups.  When it writes group entries to the
 *                      database, it clears the "found" flag.
 */

#if defined(SUPERGROUPS)
/*
 *  A general description of the supergroup changes
 *  to ptserver:
 *
 *  In AFS users can be members of groups. When you add a user, u1,
 *  to a group, g1, you add the user id for u1 to an entries list
 *  in g1. This is a list of all the members of g1.  You also add
 *  the id for g1 to an entries list in u1.  This is a list of all
 *  the groups this user is a member of.  The list has room for 10
 *  entries. If more are required, a continuation record is created.
 *
 *  With UMICH mods, u1 can be a group. When u1 is a group a new
 *  field is required to list the groups this group is a member of
 *  (since the entries field is used to list it's members). This
 *  new field is supergroups and has two entries. If more are
 *  required, a continuation record is formed.
 *
 *  There are two additional fields required, nextsg is an address
 *  of the next continuation record for this group, and countsg is
 *  the count for the number of groups this group is a member of.
 *
 *  Bit mapping support for supergroups:
 *
 *  Before fetching a supergroup, this version of ptserver checks to
 *  see if it was marked "found" and "not a member".  If and only if
 *  so, it doesn't fetch the group.  Since this should be the case
 *  with most groups, this should save a significant amount of CPU in
 *  redundant fetches of the same group.  After fetching the group, it
 *  sets "found", and either sets or clears "not a member", depending
 *  on if the group was a member of other groups.  When it writes
 *  group entries to the database, it clears the "found" flag.
 */
#endif

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/stds.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <WINNT/afsevent.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rx_globals.h>
#include <afs/cellconfig.h>
#ifdef AFS_RXK5
#include <afs/rxk5_utilafs.h>
#include <rx/rxk5.h>
#include <rx/rxk5errors.h>
#endif
#include <lock.h>
#include <ubik.h>
#include <afs/auth.h>
#include <afs/keys.h>
#include "ptserver.h"
#include "error_macros.h"
#include "afs/audit.h"
#include <afs/afsutil.h>
#include <afs/com_err.h>


/* make	all of these into a structure if you want */
struct prheader cheader;
struct ubik_dbase *dbase;
struct afsconf_dir *prdir;

#if defined(SUPERGROUPS)
extern afs_int32 depthsg;
#endif

int pr_realmNameLen;
char *pr_realmName;

int restricted = 0;
int rxMaxMTU = -1;
int rxBind = 0;

#define ADDRSPERSITE 16         /* Same global is in rx/rx_user.c */
afs_uint32 SHostAddrs[ADDRSPERSITE];

static struct afsconf_cell info;

extern int prp_group_default;
extern int prp_user_default;

#include "AFS_component_version_number.c"

int
prp_access_mask(s)
    char *s;
{
    int r;
    if (*s >= '0' && *s <= '9') {
	return strtol(s, NULL, 0);
    }
    r = 0;
    while (*s) switch(*s++)
    {
    case 'S':	r |= PRP_STATUS_ANY; break;
    case 's':	r |= PRP_STATUS_MEM; break;
    case 'O':	r |= PRP_OWNED_ANY; break;
    case 'M':	r |= PRP_MEMBER_ANY; break;
    case 'm':	r |= PRP_MEMBER_MEM; break;
    case 'A':	r |= PRP_ADD_ANY; break;
    case 'a':	r |= PRP_ADD_MEM; break;
    case 'r':	r |= PRP_REMOVE_MEM; break;
    }
    return r;
}

/* check whether caller is authorized to manage RX statistics */
int
pr_rxstat_userok(struct rx_call *call)
{
    return afsconf_SuperUser(prdir, call, NULL);
}

int
main(int argc, char **argv)
{
    register afs_int32 code;
    afs_int32 myHost;
    register struct hostent *th;
    char hostname[64];
    struct rx_service *tservice;

#if defined(AFS_RXK5)
#define MAX_SC_LEN 6
#else
#define MAX_SC_LEN 3
#endif	
    struct rx_securityClass *sc[MAX_SC_LEN];
    extern int RXSTATS_ExecuteRequest();
    extern int PR_ExecuteRequest();
    int lwps = 3;
    char clones[MAXHOSTSPERCELL];
    afs_uint32 host = htonl(INADDR_ANY);

    const char *pr_dbaseName;
    static char whoami[] = "ptserver";

    int a;
    char arg[100];

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
    sigaction(SIGABRT, &nsa, NULL);
    sigaction(SIGSEGV, &nsa, NULL);
#endif
#ifdef AFS_RXK5
    initialize_RXK5_error_table();
#endif
    osi_audit_init();
    osi_audit(PTS_StartEvent, 0, AUD_END);

    /* Initialize dirpaths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
#ifdef AFS_NT40_ENV
	ReportErrorEventAlt(AFSEVT_SVR_NO_INSTALL_DIR, 0, argv[0], 0);
#endif
	fprintf(stderr, "%s: Unable to obtain AFS server directory.\n",
		argv[0]);
	exit(2);
    }

    pr_dbaseName = AFSDIR_SERVER_PRDB_FILEPATH;

#if defined(SUPERGROUPS)
    /* make sure the structures for database records are the same size */
    if ((sizeof(struct prentry) != ENTRYSIZE)
	|| (sizeof(struct prentryg) != ENTRYSIZE)) {
	fprintf(stderr,
		"The structures for the database records are different"
		" sizes\n" "struct prentry = %d\n" "struct prentryg = %d\n"
		"ENTRYSIZE = %d\n", sizeof(struct prentry),
		sizeof(struct prentryg), ENTRYSIZE);
	PT_EXIT(1);
    }
#endif

    for (a = 1; a < argc; a++) {
	int alen;
	lcstring(arg, argv[a], sizeof(arg));
	alen = strlen(arg);
	if ((strncmp(arg, "-database", alen) == 0)
	    || (strncmp(arg, "-db", alen) == 0)) {
	    pr_dbaseName = argv[++a];	/* specify a database */
	} else if (strncmp(arg, "-p", alen) == 0) {
	    lwps = atoi(argv[++a]);
	    if (lwps > 16) {	/* maximum of 16 */
		printf("Warning: '-p %d' is too big; using %d instead\n",
		       lwps, 16);
		lwps = 16;
	    } else if (lwps < 3) {	/* minimum of 3 */
		printf("Warning: '-p %d' is too small; using %d instead\n",
		       lwps, 3);
		lwps = 3;
	    }
	} else if (strncmp(arg, "-rebuild", alen) == 0)	/* rebuildDB++ */
	    ;
#if defined(SUPERGROUPS)
	else if ((strncmp(arg, "-groupdepth", alen) == 0)
		 || (strncmp(arg, "-depth", alen) == 0)) {
	    depthsg = atoi(argv[++a]);	/* Max search depth for supergroups */
	}
#endif
	else if (strncmp(arg, "-default_access", alen) == 0) {
	    prp_user_default = prp_access_mask(argv[++a]);
	    prp_group_default = prp_access_mask(argv[++a]);
	}
	else if (strncmp(arg, "-restricted", alen) == 0) {
	    restricted = 1;
	}
	else if (strncmp(arg, "-rxbind", alen) == 0) {
	    rxBind = 1;
	}
	else if (strncmp(arg, "-enable_peer_stats", alen) == 0) {
	    rx_enablePeerRPCStats();
	} else if (strncmp(arg, "-enable_process_stats", alen) == 0) {
	    rx_enableProcessRPCStats();
	}
#ifndef AFS_NT40_ENV
	else if (strncmp(arg, "-syslog", alen) == 0) {
	    /* set syslog logging flag */
	    serverLogSyslog = 1;
	} else if (strncmp(arg, "-syslog=", MIN(8, alen)) == 0) {
	    serverLogSyslog = 1;
	    serverLogSyslogFacility = atoi(arg + 8);
	}
#endif
	else if (strncmp(arg, "-auditlog", alen) == 0) {
	    int tempfd, flags;
	    FILE *auditout;
	    char oldName[MAXPATHLEN];
	    char *fileName = argv[++a];

#ifndef AFS_NT40_ENV
	    struct stat statbuf;

	    if ((lstat(fileName, &statbuf) == 0) 
		&& (S_ISFIFO(statbuf.st_mode))) {
		flags = O_WRONLY | O_NONBLOCK;
	    } else 
#endif
	    {
		strcpy(oldName, fileName);
		strcat(oldName, ".old");
		renamefile(fileName, oldName);
		flags = O_WRONLY | O_TRUNC | O_CREAT;
	    }
	    tempfd = open(fileName, flags, 0666);
	    if (tempfd > -1) {
		auditout = fdopen(tempfd, "a");
		if (auditout) {
		    osi_audit_file(auditout);
		    osi_audit(PTS_StartEvent, 0, AUD_END);
		} else
		    printf("Warning: auditlog %s not writable, ignored.\n", fileName);
	    } else
		printf("Warning: auditlog %s not writable, ignored.\n", fileName);
	}
	else if (!strncmp(arg, "-rxmaxmtu", alen)) {
	    if ((a + 1) >= argc) {
		fprintf(stderr, "missing argument for -rxmaxmtu\n");
		PT_EXIT(1);
	    }
	    rxMaxMTU = atoi(argv[++a]);
	    if ((rxMaxMTU < RX_MIN_PACKET_SIZE) ||
		 (rxMaxMTU > RX_MAX_PACKET_DATA_SIZE)) {
		printf("rxMaxMTU %d% invalid; must be between %d-%d\n",
			rxMaxMTU, RX_MIN_PACKET_SIZE,
			RX_MAX_PACKET_DATA_SIZE);
		PT_EXIT(1);
	    }
	} 
	else if (*arg == '-') {
	    /* hack in help flag support */

#if defined(SUPERGROUPS)
#ifndef AFS_NT40_ENV
	    printf("Usage: ptserver [-database <db path>] "
		   "[-auditlog <log path>] "
		   "[-syslog[=FACILITY]] "
		   "[-p <number of processes>] [-rebuild] "
		   "[-groupdepth <depth>] "
		   "[-restricted] [-rxmaxmtu <bytes>] [-rxbind] "
		   "[-enable_peer_stats] [-enable_process_stats] "
		   "[-default_access default_user_access default_group_access] "
		   "[-help]\n");
#else /* AFS_NT40_ENV */
	    printf("Usage: ptserver [-database <db path>] "
		   "[-auditlog <log path>] "
		   "[-p <number of processes>] [-rebuild] [-rxbind] "
		   "[-default_access default_user_access default_group_access] "
		   "[-restricted] [-rxmaxmtu <bytes>] [-rxbind] "
		   "[-groupdepth <depth>] " "[-help]\n");
#endif
#else
#ifndef AFS_NT40_ENV
	    printf("Usage: ptserver [-database <db path>] "
		   "[-auditlog <log path>] "
		   "[-syslog[=FACILITY]] "
		   "[-p <number of processes>] [-rebuild] "
		   "[-enable_peer_stats] [-enable_process_stats] "
		   "[-default_access default_user_access default_group_access] "
		   "[-restricted] [-rxmaxmtu <bytes>] [-rxbind] "
		   "[-help]\n");
#else /* AFS_NT40_ENV */
	    printf("Usage: ptserver [-database <db path>] "
		   "[-auditlog <log path>] "
		   "[-default_access default_user_access default_group_access] "
		   "[-restricted] [-rxmaxmtu <bytes>] [-rxbind] "
		   "[-p <number of processes>] [-rebuild] " "[-help]\n");
#endif
#endif
	    fflush(stdout);

	    PT_EXIT(1);
	}
#if defined(SUPERGROUPS)
	else {
	    fprintf(stderr, "Unrecognized arg: '%s' ignored!\n", arg);
	}
#endif
    }

#ifndef AFS_NT40_ENV
    serverLogSyslogTag = "ptserver";
#endif
    OpenLog(AFSDIR_SERVER_PTLOG_FILEPATH);	/* set up logging */
    SetupLogSignals();

    prdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
    if (!prdir) {
	fprintf(stderr, "ptserver: can't open configuration directory.\n");
	PT_EXIT(1);
    }
    if (afsconf_GetNoAuthFlag(prdir))
	printf("ptserver: running unauthenticated\n");

#ifdef AFS_NT40_ENV
    /* initialize winsock */
    if (afs_winsockInit() < 0) {
	ReportErrorEventAlt(AFSEVT_SVR_WINSOCK_INIT_FAILED, 0, argv[0], 0);

	fprintf(stderr, "ptserver: couldn't initialize winsock. \n");
	PT_EXIT(1);
    }
#endif
    /* get this host */
    gethostname(hostname, sizeof(hostname));
    th = gethostbyname(hostname);
    if (!th) {
	fprintf(stderr, "ptserver: couldn't get address of this host.\n");
	PT_EXIT(1);
    }
    memcpy(&myHost, th->h_addr, sizeof(afs_int32));

    /* get list of servers */
    code =
	afsconf_GetExtendedCellInfo(prdir, NULL, "afsprot", &info, clones);
    if (code) {
	afs_com_err(whoami, code, "Couldn't get server list");
	PT_EXIT(2);
    }
    pr_realmName = info.name;
    pr_realmNameLen = strlen(pr_realmName);

    if (!have_afs_keyfile(prdir)
#if defined(AFS_RXK5)
	&& !have_afs_rxk5_keytab(prdir->name)
#endif
	    ) {
	fprintf(stderr, "ptserver: can't find any Kerberos key stores\n");
	ViceLog(0, ("ptserver: no Kerberos key store on startup."));
    } else {
	/* initialize ubik */
	ubik_CRXSecurityProc = afsconf_ClientAuth;
	ubik_CRXSecurityRock = (char *)prdir;
	ubik_SRXSecurityProc = afsconf_ServerAuth;
	ubik_SRXSecurityRock = (char *)prdir;
	ubik_CheckRXSecurityProc = afsconf_CheckAuth;
	ubik_CheckRXSecurityRock = (char *)prdir;
    }

    /* The max needed is when deleting an entry.  A full CoEntry deletion
     * required removal from 39 entries.  Each of which may refers to the entry
     * being deleted in one of its CoEntries.  If a CoEntry is freed its
     * predecessor CoEntry will be modified as well.  Any freed blocks also
     * modifies the database header.  Counting the entry being deleted and its
     * CoEntry this adds up to as much as 1+1+39*3 = 119.  If all these entries
     * and the header are in separate Ubik buffers then 120 buffers may be
     * required. */
    ubik_nBuffers = 120 + /*fudge */ 40;

    if (rxBind) {
	afs_int32 ccode;
#ifndef AFS_NT40_ENV
	if (AFSDIR_SERVER_NETRESTRICT_FILEPATH || 
	    AFSDIR_SERVER_NETINFO_FILEPATH) {
	    char reason[1024];
	    ccode = parseNetFiles(SHostAddrs, NULL, NULL,
					   ADDRSPERSITE, reason,
					   AFSDIR_SERVER_NETINFO_FILEPATH,
					   AFSDIR_SERVER_NETRESTRICT_FILEPATH);
	} else 
#endif
	{
	    ccode = rx_getAllAddr(SHostAddrs, ADDRSPERSITE);
	}
	if (ccode == 1) {
	    host = SHostAddrs[0];
	    rx_InitHost(host, htons(AFSCONF_PROTPORT));
	}
    }

    code =
	ubik_ServerInitByInfo(myHost, htons(AFSCONF_PROTPORT), &info, &clones,
			      pr_dbaseName, &dbase);
    if (code) {
	afs_com_err(whoami, code, "Ubik init failed");
	PT_EXIT(2);
    }
#if defined(SUPERGROUPS)
    pt_hook_write();
#endif

    memset(sc, 0, sizeof *sc);
    sc[0] = rxnull_NewServerSecurityObject();
#if defined(AFS_RXK5)
    if (have_afs_keyfile(prdir))
#endif
	sc[2] = rxkad_NewServerSecurityObject(0, prdir, afsconf_GetKey, NULL);
	
#if defined(AFS_RXK5)
    /* rxk5 */
    if(have_afs_rxk5_keytab(prdir->name)) {
	sc[5] = rxk5_NewServerSecurityObject(rxk5_auth,
	    get_afs_rxk5_keytab(prdir->name), 
	    rxk5_default_get_key, 0, 0);
    }
#endif

    /* Disable jumbograms */
    rx_SetNoJumbo();

    if (rxMaxMTU != -1) {
	rx_SetMaxMTU(rxMaxMTU);
    }

    tservice =
	rx_NewServiceHost(host, 0, PRSRV, "Protection Server", sc, MAX_SC_LEN,
		      PR_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	fprintf(stderr, "ptserver: Could not create new rx service.\n");
	PT_EXIT(3);
    }
    rx_SetMinProcs(tservice, 2);
    rx_SetMaxProcs(tservice, lwps);

    tservice =
	rx_NewServiceHost(host, 0, RX_STATS_SERVICE_ID, "rpcstats", sc, MAX_SC_LEN,
		      RXSTATS_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	fprintf(stderr, "ptserver: Could not create new rx service.\n");
	PT_EXIT(3);
    }
    rx_SetMinProcs(tservice, 2);
    rx_SetMaxProcs(tservice, 4);

    /* allow super users to manage RX statistics */
    rx_SetRxStatUserOk(pr_rxstat_userok);

    rx_StartServer(1);
    osi_audit(PTS_FinishEvent, -1, AUD_END);
    exit(0);
}
