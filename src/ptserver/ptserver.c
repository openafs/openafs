/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#include <sys/types.h>
#include <stdio.h>
#ifdef AFS_NT40_ENV 
#include <winsock2.h>
#include <WINNT/afsevent.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#endif
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rx_globals.h>
#include <lock.h>
#include <ubik.h>
#include <afs/cellconfig.h>
#include <afs/auth.h>
#include <afs/keys.h>
#include "ptserver.h"
#include "error_macros.h"
#include "afs/audit.h"
#include <afs/afsutil.h>


/* make	all of these into a structure if you want */
struct prheader cheader;
struct ubik_dbase *dbase;
struct afsconf_dir *prdir;

extern afs_int32 ubik_lastYesTime;
extern afs_int32 ubik_nBuffers;

extern int afsconf_ServerAuth();
extern int afsconf_CheckAuth();

int   pr_realmNameLen;
char *pr_realmName;

#include "AFS_component_version_number.c"

/* check whether caller is authorized to manage RX statistics */
int pr_rxstat_userok(call)
    struct rx_call *call;
{
    return afsconf_SuperUser(prdir, call, (char *)0);
}

void main (argc, argv)
  int argc;
  char **argv;
{
    register afs_int32 code;
    afs_int32 myHost;
    register struct hostent *th;
    char hostname[64];
    struct rx_service *tservice;
    struct rx_securityClass *sc[3];
    extern struct rx_securityClass *rxnull_NewServerSecurityObject();
    extern struct rx_securityClass *rxkad_NewServerSecurityObject();
    extern int RXSTATS_ExecuteRequest();
    extern int PR_ExecuteRequest();
#if 0
    struct ktc_encryptionKey tkey;
#endif
    struct afsconf_cell info;
    int kerberosKeys;			/* set if found some keys */
    afs_int32 i,j;
    int lwps = 3;
    char clones[MAXHOSTSPERCELL];

    const char *pr_dbaseName;
    char *whoami = "ptserver";

    int   a;
    char  arg[100];
    
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
    osi_audit (PTS_StartEvent, 0, AUD_END);

    /* Initialize dirpaths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
#ifdef AFS_NT40_ENV
	ReportErrorEventAlt(AFSEVT_SVR_NO_INSTALL_DIR, 0, argv[0],0);
#endif
	fprintf(stderr,"%s: Unable to obtain AFS server directory.\n", argv[0]);
	exit(2);
    }

    pr_dbaseName = AFSDIR_SERVER_PRDB_FILEPATH;

    for (a=1; a<argc; a++) {
	int alen;
	lcstring (arg, argv[a], sizeof(arg));
	alen = strlen (arg);
	if ((strncmp (arg, "-database", alen) == 0) ||
	    (strncmp (arg, "-db", alen) == 0)) {
	    pr_dbaseName = argv[++a];	/* specify a database */
	}
	else if (strncmp(arg, "-p", alen) == 0) {
	   lwps = atoi(argv[++a]);
	   if (lwps > 16) {       /* maximum of 16 */
	      printf("Warning: '-p %d' is too big; using %d instead\n",
		     lwps, 16);
	      lwps = 16;
	   } else if (lwps < 3) { /* minimum of 3 */
	      printf("Warning: '-p %d' is too small; using %d instead\n",
		     lwps, 3);
	      lwps = 3;
	   }
	}
	else if (strncmp (arg, "-rebuild", alen) == 0) /* rebuildDB++ */ ;
	else if (strncmp (arg, "-enable_peer_stats", alen) == 0) {
	    rx_enablePeerRPCStats();
	}
	else if (strncmp (arg, "-enable_process_stats", alen) == 0) {
	    rx_enableProcessRPCStats();
	}
#ifndef AFS_NT40_ENV
	else if (strncmp(arg, "-syslog", alen)==0) {
	    /* set syslog logging flag */
	    serverLogSyslog = 1;
	} 
	else if (strncmp(arg, "-syslog=", MIN(8,alen))==0) {
	    serverLogSyslog = 1;
	    serverLogSyslogFacility = atoi(arg+8);
	}
#endif
	else if (*arg == '-') {
	  usage:

		/* hack in help flag support */

#ifndef AFS_NT40_ENV
	    	printf ("Usage: ptserver [-database <db path>] "
			"[-syslog[=FACILITY]] "
			"[-p <number of processes>] [-rebuild] "
			"[-enable_peer_stats] [-enable_process_stats] "
			"[-help]\n");
#else
	    	printf ("Usage: ptserver [-database <db path>] "
			"[-p <number of processes>] [-rebuild] "
			"[-help]\n");
#endif
		fflush(stdout);

	    PT_EXIT(1);
	}
    }

    OpenLog(AFSDIR_SERVER_PTLOG_FILEPATH);     /* set up logging */
    SetupLogSignals();
 
    prdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
    if (!prdir) {
	fprintf (stderr, "ptserver: can't open configuration directory.\n");
	PT_EXIT(1);
    }
    if (afsconf_GetNoAuthFlag(prdir))
	printf ("ptserver: running unauthenticated\n");

#ifdef AFS_NT40_ENV 
    /* initialize winsock */
    if (afs_winsockInit()<0) {
      ReportErrorEventAlt(AFSEVT_SVR_WINSOCK_INIT_FAILED, 0,
			  argv[0],0);
      
      fprintf(stderr, "ptserver: couldn't initialize winsock. \n");
      PT_EXIT(1);
    }
#endif
    /* get this host */
    gethostname(hostname,sizeof(hostname));
    th = gethostbyname(hostname);
    if (!th) {
	fprintf (stderr, "ptserver: couldn't get address of this host.\n");
	PT_EXIT(1);
    }
    bcopy(th->h_addr,&myHost,sizeof(afs_int32));
        
    /* get list of servers */
    code = afsconf_GetExtendedCellInfo(prdir,(char *)0,"afsprot",
                       &info, &clones);
    if (code) {
	com_err (whoami, code, "Couldn't get server list");
	PT_EXIT(2);
    }
    pr_realmName = info.name;
    pr_realmNameLen = strlen (pr_realmName);

#if 0
    /* get keys */
    code = afsconf_GetKey(prdir,999,&tkey);
    if (code) {
	com_err (whoami, code, "couldn't get bcrypt keys from key file, ignoring.");
    }
#endif
    {   afs_int32 kvno;			/* see if there is a KeyFile here */
	struct ktc_encryptionKey key;
	code = afsconf_GetLatestKey (prdir, &kvno, &key);
	kerberosKeys = (code == 0);
	if (!kerberosKeys)
	    printf ("ptserver: can't find any Kerberos keys, code = %d, ignoring\n", code);
    }
    if (kerberosKeys) {
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
    ubik_nBuffers = 120 + /*fudge*/40;
    code = ubik_ServerInitByInfo(myHost, htons(AFSCONF_PROTPORT), &info,
                           &clones, pr_dbaseName, &dbase);
    if (code) {
	com_err (whoami, code, "Ubik init failed");
	PT_EXIT(2);
    }
    sc[0] = rxnull_NewServerSecurityObject();
    sc[1] = 0;
    if (kerberosKeys) {
	sc[2] = rxkad_NewServerSecurityObject
	    (0, prdir, afsconf_GetKey, (char *)0);
    }
    else sc[2] = sc[0];

    /* These two lines disallow jumbograms */
    rx_maxReceiveSize = OLD_MAX_PACKET_SIZE;
    rxi_nSendFrags = rxi_nRecvFrags = 1;

    tservice = rx_NewService(0,PRSRV,"Protection Server",sc,3,PR_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	fprintf (stderr, "ptserver: Could not create new rx service.\n");
	PT_EXIT(3);
    }
    rx_SetMinProcs(tservice,2);
    rx_SetMaxProcs(tservice,lwps);

    tservice = rx_NewService(0,RX_STATS_SERVICE_ID,"rpcstats",sc,3,RXSTATS_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	fprintf (stderr, "ptserver: Could not create new rx service.\n");
	PT_EXIT(3);
    }
    rx_SetMinProcs(tservice,2);
    rx_SetMaxProcs(tservice,4);

    /* allow super users to manage RX statistics */
    rx_SetRxStatUserOk(pr_rxstat_userok);

    rx_StartServer(1);
    osi_audit (PTS_FinishEvent, -1, AUD_END);
}
