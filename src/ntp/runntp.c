/* Copyright (C) 1991, 1998 Transarc Corporation - All rights reserved */

#include <afs/param.h>
#include <afs/stds.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdio.h>
#include <afs/com_err.h>
#include <afs/cellconfig.h>
#include <afs/afsutil.h>


extern int errno;

char *whoami;

static afs_int32 lastTime = 0;

static afs_int32 GetCellInfo (cellinfoP)
  struct afsconf_cell *cellinfoP;
{
    struct afsconf_dir *conf;
    char lcell[MAXHOSTCHARS];
    afs_int32 code;

    conf = afsconf_Open (AFSDIR_SERVER_ETC_DIRPATH);
    if (conf == 0) return AFSCONF_NOTFOUND;
    code = afsconf_GetLocalCell (conf, lcell, sizeof(lcell));
    if (code) return code;
    code = afsconf_GetCellInfo (conf, lcell, 0, cellinfoP);
    lastTime = conf->timeRead;
    afsconf_Close(conf);
    return code;
}

/* Check the date of the afsconf stuff and return 0 if its date is the
   same as the last call the GetCellInfo. */

static int IsCellInfoNew ()
{   struct afsconf_dir *conf;
    int new;

    /* not using cellinfo, so always return OK */
    if (lastTime == 0) return 0;

    conf = afsconf_Open (AFSDIR_SERVER_ETC_DIRPATH);
    if (conf == 0) return 1;		/* something's wrong */
    new = conf->timeRead != lastTime;
    afsconf_Close(conf);
    return (new);
}

int pid = 0;				/* process id of ntpd */

void
terminate ()
{
    if (pid) kill (pid, 9);	        /* kill off ntpd */
    exit (0);
}

#include "AFS_component_version_number.c"

main (argc, argv)
  int   argc;
  char *argv[];
{
    afs_int32 code;
    char  *filename;
    FILE *f;				/* ntp.conf output file */
    int   a;
    int   i;
    int   local = 0;			/* just use machine's local clock */
    int   precision = 0;		/* precision specification */
    int   stratum = 12;			/* stratum for local clock */
    char *logfile;			/* file for ntpd output streams */
    char  *ntpdPath;            	/* pathname of ntpd executable */
    int   nHostArgs = 0;		/* number of explicit hosts */
    char *explicitHosts[10];		/* hosts names from arglist */

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
    whoami = argv[0];
    logfile = 0;

    /* Initialize dirpaths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
	fprintf(stderr,"%s: Unable to obtain AFS server directory.\n", argv[0]);
	exit(2);
    }

    ntpdPath = AFSDIR_SERVER_NTPD_FILEPATH;
    for (a=1; a<argc; a++) {
	if (strcmp (argv[a], "-localclock") == 0) local = 1;
	else if (strcmp (argv[a], "-precision") == 0) {
	    if (++a >= argc) goto usage;
	    precision = atoi(argv[a]);	/* next arg is (negative) precision */
	}
	else if (strcmp (argv[a], "-logfile") == 0) {
	    if (++a >= argc) goto usage;
	    logfile = argv[a];		/* next arg is pathname for stdout */
	    f = fopen(logfile, "a");
	    if (f == NULL) {
		com_err (whoami, errno, "Can't open %s as logfile", logfile);
		goto usage;
	    }
	    fclose (f);
	}
	else if (strcmp (argv[a], "-ntpdpath") == 0) {
	    if (++a >= argc) goto usage;
	    ntpdPath = argv[a];	/* next arg is pathname of ntpd */
	    f = fopen(ntpdPath, "r");
	    if (f == NULL) {
		com_err (whoami, errno,
			 "Can't find ntp daemon at %s", ntpdPath);
		goto usage;
	    }
	    fclose (f);
	}
	else if (argv[a][0] != '-') { /* must be a hostname */
	    if (nHostArgs >= sizeof(explicitHosts)/sizeof(explicitHosts[0])) {
		com_err (whoami, 0, "Too many hosts specified");
		goto usage;
	    } else explicitHosts[nHostArgs++] = argv[a];
	}
	else {
	  usage:
	    fprintf (stdout, "Usage: %s [-localclock] [-precision <small-negative-integer>] [-logfile <filename for ntpd's stdout/stderr] [-ntpdpath <pathname of ntpd executable (/usr/afs/bin/ntpd)>] [<host>*] [-help]\n", whoami);
	    exit (-1);
	}
    }

    /* setup to write to output file */
    filename = "/tmp/ntp.conf";
    f = fopen (filename, "w");
    if (f == 0) {
	com_err (whoami, errno, "can't create output file %s", filename);
	exit (4);
    }

#if defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV)
    /*
     * NOTE: Ntpd does not know how to set kernel variables on Solaris
     * systems, and it does not seem to be necessary on Solaris 2.6 and
     * later.
     */

    /* first, for SUNs set tickadj and dosynctodr */
    fprintf (f, "setdosynctodr Y\nsettickadj Y\n");
#endif

    /* specify precision */
    if (precision) {
	fprintf (f, "precision %d\n", precision);
    }

    /* then dump the appropriate type of host list */

    if (local) { /* use the machines local clock */
	fprintf (f, "peer\t/dev/null\tLOCL\t%d\t%d\tlocal\n",
		 stratum, precision);
    }
    if (nHostArgs) { /* use the explicitly provided list */
	for (i=0; i<nHostArgs; i++) {
	    fprintf (f, "peer\t%s\n", explicitHosts[i]);
	}
    }
    if (!(local || nHostArgs)) { /* just use CellServDB info instead */
	struct afsconf_cell cellinfo;

	code = GetCellInfo (&cellinfo);
	if (code) {
	    com_err (whoami, 0, "can't get local cell info");
	    exit (1);
	}
	for (i=0; i<cellinfo.numServers; i++) {
	    fprintf (f, "peer\t%s\t#%s\n",
		     inet_ntoa(cellinfo.hostAddr[i].sin_addr),
		     cellinfo.hostName[i]);
	}
    }	

    /* all done with ntp.conf file */
    if (fclose (f) == EOF) {
	com_err (whoami, errno, "can't close output file %s", filename);
	exit (5);
    }

    /* handle bozo kills right */

    {	struct sigaction sa;
	bzero((char *)&sa, sizeof(sa));
	sa.sa_handler = terminate;
	code = sigaction (SIGTERM, &sa, NULL);
	if (code) {
	    com_err (whoami, errno, "can't set up handler for SIGTERM");
	    exit (9);
	}
    }

    /* now start ntpd with proper arguments */

    pid = fork ();
    if (pid == -1) {
	com_err (whoami, errno, "forking for ntpd process");
	exit (6);
    }
    if (pid == 0) { /* this is child */
	char *argv[5];
	char *envp[1];
	afs_int32 now = time(0);
	extern char *ctime();
	char *nowString = ctime (&now);

	if (logfile == 0) logfile = "/dev/null";
	freopen (logfile, "a", stdout);
	freopen (logfile, "a", stderr);
	nowString[strlen(nowString)-1] = '\0'; /* punt the newline char */
	fprintf (stdout, "Starting %s at %s with output to logfile\n",
		 ntpdPath, nowString);
	fflush (stdout);

	argv[0] = "ntpd";
	argv[1] = "-f";		/* don't fork */
	argv[2] = "-c";		/* read our conf file */
	argv[3] = filename;
	argv[4] = 0;
	envp[0] = 0;
	code = execve (ntpdPath, argv, envp);
	if (code) com_err (whoami, errno, "execve of %s failed", ntpdPath);
	exit(errno);
    }
    else { /* this is parent */
	int  status;
	int  waitInterval = 0;
	do {
	    i = wait3 (&status, WNOHANG|WUNTRACED, 0);
	    if (i == -1) {
		(void) kill (pid, 9);	        /* try to kill off ntpd */
		com_err (whoami, errno, "wait3 ing");
		exit (7);
	    } else if (i == pid) {
		printf ("ntpd exited status = %x (code=%d, %ssignal=%d)\n",
			status, (status&0xffff)>>8,
			((status&0x80) ? "coredump, ":""), status&0x7f);
		exit (8);
	    } else if (i != 0) {
		printf ("strange return from wait3 %d\n", i);
		break;
	    }
	    sleep (waitInterval>60 ? 60 : waitInterval++);
	} while (!IsCellInfoNew());

	code = kill (pid, 9);	        /* kill off ntpd */
	if (code) com_err (whoami, errno, "trying to kill ntpd process");
	else printf ("Exiting due to change in cellinfo\n");
    }

    exit (0);
}
