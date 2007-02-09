/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *	Implementation of the volume operations used by the AFS user
 *	account facility.
 */

/*
 * --------------------- Required definitions ---------------------
 */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "uss_vol.h"		/*Interface to this module */
#include "uss_common.h"		/*Common definitions */
#include "uss_procs.h"		/*Defs from procs module */
#include "uss_fs.h"		/*CacheManager ops */
#include <sys/stat.h>
#include <pwd.h>
#include <netdb.h>
#include <errno.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <afs/vlserver.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <rx/rx_globals.h>
#include <afs/volser.h>
#include <afs/volint.h>
#include <afs/keys.h>
#include <ubik.h>

extern struct rx_connection *UV_Bind();
extern int line;
extern int VL_GetEntryByID();
extern char *hostutil_GetNameByINet();


/*
 * ---------------------- Private definitions ---------------------
 */
#undef USS_VOL_DB
#undef USS_VOL_DB_SHOW_OVERRIDES


/*
 * --------------------- Exported definitions ---------------------
 */
/*
 * The Volume Server interface imports the Ubik connection
 * structure to use, expecting it to be named "cstruct".  This
 * is why we have two names here.  Thus, the UV_CreateVolume()
 * will work and we can avoid nasty little core dumps.
 */
struct ubik_client *uconn_vldbP;	/*Ubik connection struct */
struct ubik_client *cstruct;	/*Required name for above */

/*
 * ------------------------ Private globals -----------------------
 */
static int initDone = 0;	/*Module initialized? */
static int NoAuthFlag = 0;	/*Use -noauth? */
static struct rx_connection
 *serverconns[VLDB_MAXSERVERS];	/*Connection(s) to VLDB
				 * server(s) */


/*-----------------------------------------------------------------------
 * static InitThisModule
 *
 * Description:
 *	Set up this module, namely set up all the client state for
 *	dealing with the Volume Location Server(s), including
 *	network connections.
 *
 * Arguments:
 *	a_noAuthFlag : Do we need authentication?
 *	a_confDir    : Configuration directory to use.
 *	a_cellName   : Cell we want to talk to.
 *
 * Returns:
 *	0 if everything went fine, or
 *	lower-level error code otherwise.
 *
 * Environment:
 *	This routine will only be called once.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static afs_int32
InitThisModule(a_noAuthFlag, a_confDir, a_cellName)
     int a_noAuthFlag;
     char *a_confDir;
     char *a_cellName;

{				/*InitThisModule */

    static char rn[] = "uss_vol:InitThisModule";
    register afs_int32 code;	/*Return code */
    struct afsconf_dir *tdir;	/*Ptr to conf dir info */
    struct afsconf_cell info;	/*Info about chosen cell */
    struct ktc_principal sname;	/*Service name */
    struct ktc_token ttoken;	/*Service ticket */
    afs_int32 scIndex;		/*Chosen security index */
    struct rx_securityClass *sc;	/*Generated security object */
    afs_int32 i;		/*Loop index */

    /*
     * Only once, guys, will 'ya?
     */
    if (initDone) {
#ifdef USS_VOL_DB
	printf("[%s] Called multiple times!\n", rn);
#endif /* USS_VOL_DB */
	return (0);
    }

    /*
     * Set up our Rx environment.
     */
#ifdef USS_VOL_DB
    printf("[%s] Initializing Rx environment\n", rn);
#endif /* USS_VOL_DB */
    code = rx_Init(0);
    if (code) {
	fprintf(stderr, "%s:  Couldn't initialize Rx.\n", uss_whoami);
	return (code);
    }
    rx_SetRxDeadTime(50);

    /*
     * Find out all about our configuration.
     */
#ifdef USS_VOL_DB
    printf("[%s] Handling configuration info\n", rn);
#endif /* USS_VOL_DB */
    tdir = afsconf_Open(a_confDir);
    if (!tdir) {
	fprintf(stderr, "%s: Couldn't open configuration directory (%s).\n",
		uss_whoami, a_confDir);
	return (-1);
    }
    code = afsconf_GetCellInfo(tdir, a_cellName, AFSCONF_VLDBSERVICE, &info);
    if (code) {
	printf("%s: Can't find VLDB server(s) for cell %s\n", uss_whoami,
	       a_cellName);
	exit(1);
    }
#ifdef USS_VOL_DB
    printf("[%s] Getting tickets if needed\n", rn);
#endif /* USS_VOL_DB */
    if (!a_noAuthFlag) {
	/*
	 * We don't need tickets for unauthenticated connections.
	 */
	strcpy(sname.cell, info.name);
	sname.instance[0] = 0;
	strcpy(sname.name, "afs");
	code = ktc_GetToken(&sname, &ttoken, sizeof(ttoken), NULL);
	if (code) {
	    fprintf(stderr,
		    "%s: Couldn't get AFS tokens, running unauthenticated.\n",
		    uss_whoami);
	    scIndex = 0;
	} else {
	    /*
	     * We got a ticket, go for an authenticated connection.
	     */
	    if (ttoken.kvno >= 0 && ttoken.kvno <= 255)
		scIndex = 2;	/*Kerberos */
	    else {
		fprintf(stderr, "%s: Funny kvno (%d) in ticket, proceeding\n",
			uss_whoami, ttoken.kvno);
		scIndex = 2;
	    }
	}			/*Got a ticket */
    } /*Authentication desired */
    else
	scIndex = 0;

    /*
     * Generate the appropriate security object for the connection.
     */
#ifdef USS_VOL_DB
    printf("[%s] Generating Rx security object\n", rn);
#endif /* USS_VOL_DB */
    switch (scIndex) {
    case 0:
	sc = (struct rx_securityClass *)
	    rxnull_NewClientSecurityObject();
	break;

    case 1:
	break;

    case 2:
	sc = (struct rx_securityClass *)
	    rxkad_NewClientSecurityObject(rxkad_clear, &ttoken.sessionKey,
					  ttoken.kvno, ttoken.ticketLen,
					  ttoken.ticket);
	break;
    }

    /*
     * Tell UV module about default authentication.
     */
#ifdef USS_VOL_DB
    printf("[%s] Setting UV security: obj 0x%x, index %d\n", rn, sc, scIndex);
#endif /* USS_VOL_DB */
    UV_SetSecurity(sc, scIndex);
    if (info.numServers > VLDB_MAXSERVERS) {
	fprintf(stderr, "%s: info.numServers=%d (> VLDB_MAXSERVERS=%d)\n",
		uss_whoami, info.numServers, VLDB_MAXSERVERS);
	exit(1);
    }

    /*
     * Connect to each VLDB server for the chosen cell.
     */
    for (i = 0; i < info.numServers; i++) {
#ifdef USS_VOL_DB
	printf
	    ("[%s] Connecting to VLDB server 0x%x, port %d, service id %d\n",
	     rn, info.hostAddr[i].sin_addr.s_addr, info.hostAddr[i].sin_port,
	     USER_SERVICE_ID);
#endif /* USS_VOL_DB */
	serverconns[i] =
	    rx_NewConnection(info.hostAddr[i].sin_addr.s_addr,
			     info.hostAddr[i].sin_port, USER_SERVICE_ID, sc,
			     scIndex);
    }

    /*
     * Set up to execute Ubik transactions on the VLDB.
     */
#ifdef USS_VOL_DB
    printf("[%s] Initializing Ubik interface\n", rn);
#endif /* USS_VOL_DB */
    code = ubik_ClientInit(serverconns, &uconn_vldbP);
    if (code) {
	fprintf(stderr, "%s: Ubik client init failed.\n", uss_whoami);
	return (code);
    }
#ifdef USS_VOL_DB
    printf("[%s] VLDB ubik connection structure at 0x%x\n", rn, uconn_vldbP);
#endif /* USS_VOL_DB */

    /*
     * Place the ubik VLDB connection structure in its advertised
     * location.
     */
    cstruct = uconn_vldbP;

    /*
     * Success!
     */
    initDone = 1;
    return (0);

}				/*InitThisModule */


/*-----------------------------------------------------------------------
 * static HostIDToHostName
 *
 * Description:
 *	Given a host ID (in network byte order), figure out the
 *	corresponding host name.
 *
 * Arguments:
 *	a_hostID   : Host ID in network byte order.
 *	a_hostName : Ptr to host name buffer.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	This routine simply calls the hostutil_GetNameByINet()
 *	function exported by the utility library (util.a).
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

char *hostutil_GetNameByINet();

static void
HostIDToHostName(a_hostID, a_hostName)
     afs_int32 a_hostID;
     char *a_hostName;

{				/*HostIDToHostName */

    strcpy(a_hostName, hostutil_GetNameByINet(a_hostID));

}				/*HostIDToHostName */


/*-----------------------------------------------------------------------
 * static PartIDToPartName
 *
 * Description:
 *	Given a partition ID (in network byte order), figure out the
 *	corresponding partition name.
 *
 * Arguments:
 *	a_partID   : Partition ID in network byte order.
 *	a_partName : Ptr to partition name buffer.
 *
 * Returns:
 *	0 if everything went well, or
 *	-1 if the given partition ID couldn't be translated.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static afs_int32
PartIDToPartName(a_partID, a_partName)
     afs_int32 a_partID;
     char *a_partName;

{				/*PartIDToPartName */

    static char rn[] = "PartIDToPartName";

#ifdef USS_VOL_DB
    printf("[%s] Translating partition id %d to its name\n", rn, a_partID);
#endif /* USS_VOL_DB */

    if ((a_partID < 0) || (a_partID > VOLMAXPARTS))
	return (-1);

    if (a_partID < 26) {
	strcpy(a_partName, "/vicep");
	a_partName[6] = a_partID + 'a';
	a_partName[7] = '\0';
    } else {
	strcpy(a_partName, "/vicep");
	a_partID -= 26;
	a_partName[6] = 'a' + (a_partID / 26);
	a_partName[7] = 'a' + (a_partID % 26);
	a_partName[8] = '\0';
    }

#ifdef USS_VOL_DB
    printf("[%s] Translation for part ID %d is '%s'\n", rn, a_partID,
	   a_partName);
#endif /* USS_VOL_DB */
    return (0);

}				/*PartIDToPartName */


/*------------------------------------------------------------------------
 * EXPORTED uss_Vol_GetServer
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_vol_GetServer(a_name)
     char *a_name;

{				/*uss_vol_GetServer */

    register struct hostent *th;
    afs_int32 addr;
    char b1, b2, b3, b4;
    register afs_int32 code;

    code = sscanf(a_name, "%d.%d.%d.%d", &b1, &b2, &b3, &b4);
    if (code == 4) {
	/*
	 * Parsed as 128.2.9.4, or similar; return it in network
	 * byte order (128 in byte 0).
	 */
	addr =
	    (((afs_int32) b1) << 24) | (((afs_int32) b2) << 16) |
	    (((afs_int32) b3) << 8) | (afs_int32) b4;
	return htonl(addr);
    }

    th = gethostbyname(a_name);
    if (!th)
	return (0);
    memcpy(&addr, th->h_addr, sizeof(addr));
    return (addr);

}				/*uss_vol_GetServer */

/*------------------------------------------------------------------------
 * static GetVolumeType
 *
 * Description:
 *	Translate the given char string representing a volume type to the
 *	numeric representation.
 *
 * Arguments:
 *	a_type : Char string volume type.
 *
 * Returns:
 *	One of ROVOL, RWVOL, BACKVOL, or -1 on failure.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static afs_int32
GetVolumeType(a_type)
     char *a_type;

{				/*GetVolumeType */

    if (!strcmp(a_type, "ro"))
	return (ROVOL);
    else if (!strcmp(a_type, "rw"))
	return (RWVOL);
    else if (!strcmp(a_type, "bk"))
	return (BACKVOL);
    else
	return (-1);

}				/*GetVolumeType */


/*------------------------------------------------------------------------
 * EXPORTED uss_Vol_GetPartitionID
 *
 * Environment:
 *	It is assumed that partition names may begin with ``/vicep''.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_vol_GetPartitionID(a_name)
     char *a_name;

{				/*uss_vol_GetPartitionID */

    register char tc;
    char ascii[3];

    tc = *a_name;
    if (tc == 0)
	return (-1);

    /*
     * Numbers go straight through.
     */
    if (tc >= '0' && tc <= '9') {
	return (atoi(a_name));
    }

    /*
     * Otherwise, check for vicepa or /vicepa, or just plain "a"
     */
    ascii[2] = 0;
    if (strlen(a_name) <= 2) {
	strcpy(ascii, a_name);
    } else if (!strncmp(a_name, "/vicep", 6)) {
	strncpy(ascii, a_name + 6, 2);
    } else if (!strncmp(a_name, "vicep", 5)) {
	strncpy(ascii, a_name + 5, 2);
    } else
	return (-1);

    /*
     * Now, partitions are named /vicepa ... /vicepz, /vicepaa, /vicepab,
     * .../vicepzz, and are numbered from 0.  Do the appropriate conversion.
     */
    if (ascii[1] == 0) {
	/*
	 * Single-char name, 0..25
	 */
	if (ascii[0] < 'a' || ascii[0] > 'z')
	    return (-1);	/* wrongo */
	return (ascii[0] - 'a');
    } else {
	/*
	 * Two-char name, 26 .. <whatever>
	 */
	if (ascii[0] < 'a' || ascii[0] > 'z')
	    return (-1);
	if (ascii[1] < 'a' || ascii[1] > 'z')
	    return (-1);
	return ((ascii[0] - 'a') * 26 + (ascii[1] - 'a') + 26);
    }
}				/*uss_vol_GetPartitionID */


/*-----------------------------------------------------------------------
 * static CheckDoubleMount
 *
 * Description:
 *	Make sure we're not trying to mount a volume in the same place
 *	twice.
 *
 * Arguments:
 *	a_mp    : Mountpoint pathname to check.
 *	a_oldmp : Ptr to buffer into which the old value of the
 *		  mountpoint is placed (if any).
 *
 * Returns:
 *	0		  if the volume was not previously mounted.
 *	uss_procs_ANCIENT if there was already a mountpoint AND the
 *			  user was already recorded in the password
 *			  file.
 *	uss_procs_YOUNG	  if there was a mountpoint for the user taken
 *			  from the directory pool, yet the user was not
 *			  yet in the password file.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	May fill in the a_oldmp buffer with the value of the old
 *	mountpoint.
 *------------------------------------------------------------------------*/

static int
CheckDoubleMount(a_mp, a_oldmp)
     char *a_mp;
     char *a_oldmp;

{				/*CheckDoubleMount */

    static char rn[] = "uss_vol:CheckDoubleMount";
    int start, len, mlen, tlen;
    int i = 0;
    struct passwd *pws;
    struct stat stbuf;

    pws = getpwuid(atoi(uss_Uid));
    if (pws != NULL) {
	/*
	 * User exists in the password file, so they've been fully
	 * created and integrated.  Return the ``ancient'' mountpoint.
	 */
	strcpy(a_oldmp, pws->pw_dir);
	return (uss_procs_ANCIENT);
    }

    if (uss_NumGroups) {
	/*
	 * $AUTO used. Search among the possible directories.
	 */
	len = strlen(uss_Auto);
	mlen = strlen(a_mp);
	while (strncmp(&a_mp[i], uss_Auto, len)) {
	    a_oldmp[i] = a_mp[i];
	    if (++i > (mlen - len)) {
		i = -1;
		break;
	    }
	}
	if ((start = i) != -1) {
	    /*
	     * $AUTO used in mountpoint.
	     */
	    for (i = 0; i < uss_NumGroups; i++) {
		/*
		 * Copy in the base and tail components.
		 */
		tlen = strlen(uss_DirPool[i]);
		strncpy(&a_oldmp[start], uss_DirPool[i], tlen);
		strcpy(&a_oldmp[start + tlen], &a_mp[start + len]);
#ifdef USS_VOL_DB
		printf("%s: Checking '%s' for mount point\n", rn, a_oldmp);
#endif /* USS_VOL_DB */
		if (lstat(a_oldmp, &stbuf) == 0)	/*mp exists */
		    if (strcmp(a_oldmp, a_mp))
			/* and is different */
			/*
			 * The old mount point exists and is different
			 * from the current one, so return the fact
			 * that we have a ``young'' mountpoint.
			 */
			return (uss_procs_YOUNG);
	    }			/*Check each $AUTO directory */
	}
    }

    /*$AUTO has been used */
    /*
     * No luck finding the old mount point, so we just return that
     * this is the first time we've seen this volume.
     */
    return (0);

}				/*CheckDoubleMount */


/*------------------------------------------------------------------------
 * EXPORTED uss_vol_CreateVol
 *
 * Environment:
 *	Called from the code generated by the uss grammar.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_vol_CreateVol(a_volname, a_server, a_partition, a_quota, a_mpoint,
		  a_owner, a_acl)
     char *a_volname;
     char *a_server;
     char *a_partition;
     char *a_quota;
     char *a_mpoint;
     char *a_owner;
     char *a_acl;

{				/*uss_vol_CreateVol */

    static char rn[] = "uss_vol_CreateVol";	/*Routine name */
    afs_int32 pname;		/*Partition name */
    afs_int32 volid, code;	/*Volume ID, return code */
    afs_int32 saddr;		/*Socket info for server */
    int VolExistFlag = 0;	/*Does the volume exist? */
    int mpExistFlag = 0;	/*Does the mountpoint exist? */
    char *Oldmpoint = NULL;	/*Old mountpoint name, if any */
    char tmp_str[uss_MAX_SIZE];	/*Useful string buffer */
    int o;			/*Owner's user id */
    char userinput[64];		/*User's input */
    struct uss_subdir *new_dir;	/*Used to remember original ACL */

    /*
     * Don't do anything if there's already a problem.
     */
    if (uss_syntax_err)
	return (1);

#ifdef USS_VOL_DB
    fprintf(stderr, "%s:uss_vol_CreateVol params:\n", rn);
    fprintf(stderr,
	    "%s: volname '%s', server '%s', partition '%s', quota '%s', mpoint '%s', owner '%s', acl '%s'\n",
	    rn, a_volname, a_server, a_partition, a_quota, a_mpoint, a_owner,
	    a_acl);
#endif /* USS_VOL_DB */

    /*
     * All of the parameters passed in are taken from the template
     * file.  Override these values if the user has explicitly set
     * them, namely if the uss commons have non-null strings.
     */
    if (uss_Server[0] != '\0') {
#ifdef USS_VOL_DB_SHOW_OVERRIDES
	if (uss_verbose)
	    fprintf(stderr,
		    "%s: Overriding server field: template value is '%s', overridden to '%s'\n",
		    rn, a_server, uss_Server);
#endif /* USS_VOL_DB_SHOW_OVERRIDES */
	a_server = uss_Server;
    }

    if (uss_Partition[0] != '\0') {
#ifdef USS_VOL_DB_SHOW_OVERRIDES
	if (uss_verbose)
	    fprintf(stderr,
		    "%s: Overriding partition field: template value is '%s', overridden to '%s'\n",
		    rn, a_partition, uss_Partition);
#endif /* USS_VOL_DB_SHOW_OVERRIDES */
	a_partition = uss_Partition;
    }

    if (uss_MountPoint[0] != '\0') {
#ifdef USS_VOL_DB_SHOW_OVERRIDES
	if (uss_verbose)
	    fprintf(stderr,
		    "%s: overriding mountpoint field: template value is '%s', overridden to '%s'\n",
		    rn, a_mpoint, uss_MountPoint);
#endif /* USS_VOL_DB_SHOW_OVERRIDES */
	a_mpoint = uss_MountPoint;
    }
#ifdef USS_VOL_DB_SHOW_OVERRIDES
    printf("%s: Params after overrides:\n", uss_whoami);
    printf
	("%s: volname '%s', server '%s', partition '%s', quota '%s', mpoint '%s', owner '%s', acl '%s'\n",
	 uss_whoami, a_volname, a_server, a_partition, a_quota, a_mpoint,
	 a_owner, a_acl);
#endif /* USS_VOL_DB_SHOW_OVERRIDES */

    if (uss_verbose)
	fprintf(stderr,
		"Creating volume '%s' on server '%s', partition '%s'\n",
		a_volname, a_server, a_partition);

    saddr = uss_vol_GetServer(a_server);
    if (!saddr) {
	uss_procs_PrintErr(line,
			   "File server '%s' not found in config info\n",
			   a_server);
	return (1);
    }
    pname = uss_vol_GetPartitionID(a_partition);
    if (pname < 0) {
	uss_procs_PrintErr(line, "Couldn't interpret partition name '%s'\n",
			   a_partition);
	return (1);
    }

    /*
     * Make sure our VLDB connection(s) is/are set up before actually
     * trying to perform a volume creation creation.
     */
    if (!initDone) {
	code = InitThisModule(NoAuthFlag, uss_ConfDir, uss_Cell);
	if (code) {
	    com_err(uss_whoami, code,
		    "while inititializing VLDB connection(s)\n");
	    return (code);
	}
    }
    /*Initialize VLDB connection(s) */
    if (!uss_DryRun) {
#ifdef USS_VOL_DB
	printf("%s: Creating volume on srv 0x%x, part %d, vol name '%s'\n",
	       rn, saddr, pname, a_volname);
#endif /* USS_VOL_DB */
	code = UV_CreateVolume(saddr, pname, a_volname, &volid);
	if (code) {
	    if (code == VL_NAMEEXIST) {
		VolExistFlag = 1;
		fprintf(stderr,
			"%s: Warning; Volume '%s' exists, using existing one.\n",
			uss_whoami, a_volname);

		/*
		 * We should get the volid here if we want to use it, but
		 * we don't need it right now.  What we DO need, though, is
		 * to ask our caller if it's OK to overwrite the user's files
		 * if they're pre-existing.
		 */
		if (!uss_OverwriteThisOne) {
		    printf
			("Overwrite files in pre-existing '%s' volume? [y, n]: ",
			 a_volname);
		    scanf("%s", userinput);
		    if ((userinput[0] == 'y') || (userinput[0] == 'Y')) {
			printf("\t[Overwriting allowed]\n");
			uss_OverwriteThisOne = 1;
		    } else
			printf("\t[Overwriting not permitted]\n");
		}		/*Overwriting not previously allowed */
	    } /*Volume already existed */
	    else {
		uss_procs_PrintErr(line,
				   "Couldn't create volume '%s' [error %d]: %s\n",
				   a_volname, code, strerror(errno));
		return (1);
	    }			/*Failure was NOT because it already existed */
	}			/*UV_CreateVolume failed */
    } /*Not a dry run */
    else {
	fprintf(stderr,
		"\t[Dry run: Creating volume '%s' on '%s', partition '%s']\n",
		a_volname, a_server, a_partition);
    }				/*Not a dry run */

    /* OK, we want to make sure we don't double-mount the volume.
     * If the volume existed, it can be the case that it is
     * already mounted somewhere else (because of $AUTO or others).
     * Let's check for that.  Note: we never enter this portion of
     * the code if we're doing a dry run.
     */
    if (VolExistFlag) {
	if ((Oldmpoint = (char *)malloc(strlen(a_mpoint) + 50)) == NULL) {
	    fprintf(stderr, "%s: No more memory!\n", uss_whoami);
	    return (1);
	}

	mpExistFlag = CheckDoubleMount(a_mpoint, Oldmpoint);
	if (mpExistFlag == uss_procs_ANCIENT) {
	    fprintf(stderr,
		    "%s:\t*** WARNING ***; This user (%s) is already in passwd file (or equivalent). IGNORED.\n",
		    uss_whoami, uss_User);
	    free(Oldmpoint);
	    uss_syntax_err = 1;	/*I know, I know, it's not a SYNTAX error */
	    uss_ignoreFlag = 1;
	    return (0);
	}
	if (mpExistFlag == uss_procs_YOUNG) {
	    fprintf(stderr,
		    "%s: Warning; Volume '%s' is already mounted on %s.  Using the existing one.\n",
		    uss_whoami, a_volname, Oldmpoint);
	    a_mpoint = Oldmpoint;
	}
    }

    if (mpExistFlag == 0) {
	extern int local_Cell;
	/*
	 * Let's mount it.
	 */
	if (local_Cell)
	    sprintf(tmp_str, "#%s.", a_volname);
	else
	    sprintf(tmp_str, "#%s:%s.", uss_Cell, a_volname);
	if (!uss_DryRun) {
	    if (symlink(tmp_str, a_mpoint)) {
		if (errno == EEXIST) {
		    fprintf(stderr,
			    "%s: Warning: Mount point '%s' already exists.\n",
			    uss_whoami, a_mpoint);
		} else {
		    fprintf(stderr,
			    "%s: Can't mount volume '%s' on '%s': %s\n",
			    uss_whoami, a_volname, a_mpoint, strerror(errno));
		    if (Oldmpoint)
			free(Oldmpoint);
		    return (1);
		}		/*There was already something mounted there */
	    }			/*Mount failed */
	} /*Dry run */
	else {
	    fprintf(stderr, "\t[Dry run: Mounting '%s' at '%s']\n", tmp_str,
		    a_mpoint);
	}			/*Not a dry run */
    }

    /*Mount point didn't already exist */
    /*
     * Set the volume disk quota.
     */
    if (!uss_DryRun) {
	if (code = uss_acl_SetDiskQuota(a_mpoint, atoi(a_quota)))
	    return (code);
    } /*Dry run */
    else {
	fprintf(stderr,
		"\t[Dry run: Setting disk quota for '%s' to %s blocks]\n",
		a_mpoint, a_quota);
    }				/*Not a dry run */

    /*Copy mpoint into $MTPT for others to use */
    strcpy(uss_MountPoint, a_mpoint);

    o = uss_procs_GetOwner(a_owner);
    if (!uss_DryRun) {
	if (chown(a_mpoint, o, -1)) {
	    fprintf(stderr,
		    "%s: Can't chown() mountpoint '%s' to owner '%s' (uid %d): %s\n",
		    uss_whoami, a_mpoint, a_owner, o, strerror(errno));
	    if (Oldmpoint)
		free(Oldmpoint);
	    return (1);
	}			/*chown failed */
    } /*Dry run */
    else {
	fprintf(stderr,
		"\t[Dry run: chown() mountpoint '%s' to be owned by user]\n",
		a_mpoint);
    }				/*Not a dry run */

    /*
     * Set the ACL on the user's home directory so that, for the duration of
     * the account creation, only the uss_AccountCreator has any rights on the
     * files therein.  Make sure to clear this ACL to remove anyone that might
     * already be there due to volume creation defaults.  We will set this ACL
     * properly, as well as all ACLs of future subdirectories,as the very last
     * thing we do to the new account.
     */
    new_dir = (struct uss_subdir *)malloc(sizeof(struct uss_subdir));
    new_dir->previous = uss_currentDir;
    new_dir->path = (char *)malloc(strlen(a_mpoint) + 1);
    strcpy(new_dir->path, a_mpoint);
    new_dir->finalACL = (char *)malloc(strlen(a_acl) + 1);
    strcpy(new_dir->finalACL, a_acl);
    uss_currentDir = new_dir;
    sprintf(tmp_str, "%s %s all", a_mpoint, uss_AccountCreator);

    if (Oldmpoint)
	free(Oldmpoint);

    if (!uss_DryRun) {
	if (uss_verbose)
	    fprintf(stderr, "Setting ACL: '%s'\n", tmp_str);
	if (uss_acl_SetAccess(tmp_str, 1, 0))
	    return (1);
    } /*For real */
    else {
	fprintf(stderr, "\t[Dry run: uss_acl_SetAccess(%s)]\n", tmp_str);
    }
    return (0);
}				/*uss_vol_CreateVol */


/*------------------------------------------------------------------------
 * EXPORTED uss_vol_DeleteVol
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_vol_DeleteVol(a_volName, a_volID, a_servName, a_servID, a_partName,
		  a_partID)
     char *a_volName;
     afs_int32 a_volID;
     char *a_servName;
     afs_int32 a_servID;
     char *a_partName;
     afs_int32 a_partID;

{				/*uss_vol_DeleteVol */

    static char rn[] = "uss_vol_DeleteVol";	/*Routine name */
    register afs_int32 code;	/*Return code */

    /*
     * Make sure we've initialized our VLDB connection(s) before
     * proceeding apace.
     */
    if (!initDone) {
	code = InitThisModule(NoAuthFlag, uss_ConfDir, uss_Cell);
	if (code)
	    return (code);
    }

    if (!uss_DryRun) {
	/*
	 * Go for the deletion.
	 */
#ifdef USS_VOL_DB
	printf
	    ("%s: Deleting volume '%s' (ID %d) on FileServer '%s' (0x%x), partition '%s' (%d)\n",
	     rn, a_volName, a_volID, a_servName, a_servID, a_partName,
	     a_partID);
#endif /* USS_VOL_DB */

	code = UV_DeleteVolume(a_servID, a_partID, a_volID);
	if (code)
	    printf("%s: Can't delete volume '%s'\n", uss_whoami, a_volName);
    } else
	printf("\t[Dry run - volume '%s' NOT removed]\n", a_volName);

    return (code);

}				/*uss_vol_DeleteVol */


/*------------------------------------------------------------------------
 * static GetServerAndPart
 *
 * Description:
 *	Given a VLDB entry, return the server and partition IDs for
 *	the read/write volume.
 *
 * Arguments:
 *	a_vldbEntryP : Ptr to VLDB entry.
 *	a_servIDP    : Ptr to server ID to set.
 *	a_partIDP    : Ptr to partition ID to set.
 *
 * Returns:
 *	0 if everything went well, or
 *	-1 otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static afs_int32
GetServerAndPart(a_vldbEntryP, a_servIDP, a_partIDP)
     struct vldbentry *a_vldbEntryP;
     afs_int32 *a_servIDP;
     afs_int32 *a_partIDP;

{				/*GetServerAndPart */

    /*
     * It really looks like all we need to do is pull off the
     * first entry's info.
     */
    *a_servIDP = a_vldbEntryP->serverNumber[0];
    *a_partIDP = a_vldbEntryP->serverPartition[0];
    return (0);

}				/*GetServerAndPart */


/*------------------------------------------------------------------------
 * EXPORTED uss_vol_GetVolInfoFromMountPoint
 *
 * Environment:
 *	If the mountpoint path provided is not 
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_vol_GetVolInfoFromMountPoint(a_mountpoint)
     char *a_mountpoint;

{				/*uss_vol_GetVolInfoFromMountPoint */

    static char rn[] = "uss_vol_GetVolInfoFromMountPoint";
    register afs_int32 code;	/*Return code */
    uss_VolumeStatus_t *statusP;	/*Ptr to returned status */
    afs_int32 volID;		/*Volume ID */
    struct vldbentry vldbEntry;	/*VLDB entry for volume */
    afs_int32 serverID;		/*Addr of host FileServer */
    afs_int32 partID;		/*Volume's partition ID */

    /*
     * Set up to ask the CacheManager to give us all the info
     * it has about the given mountpoint.
     */
    code = uss_fs_GetVolStat(a_mountpoint, uss_fs_OutBuff, USS_FS_MAX_SIZE);
#ifdef USS_VOL_DB
    printf("[%s] Result of uss_fs_GetVolStat: code = %d, errno = %d\n", rn,
	   code, errno);
#endif /* USS_VOL_DB */
    if (code) {
	if (errno == EINVAL || errno == ENOENT || errno == ENODEV) {
	    /*
	     * We were given a mountpoint pathname that doesn't
	     * point to a volume, or a mountpoint that has already
	     * been deleted.  This means that there is no info
	     * to get from this pathname.  Zero out the volume,
	     * server & partition info and return successfully.
	     */
	    uss_Volume[0] = '\0';
	    uss_Server[0] = '\0';
	    uss_Partition[0] = '\0';
	    uss_VolumeID = 0;
	    uss_ServerID = 0;
	    uss_PartitionID = 0;
	    if (uss_verbose) {
		printf("%s: Warning: Mountpoint pathname '%s': ", uss_whoami,
		       a_mountpoint);
		if (errno == EINVAL)
		    printf("Volume not reachable\n");
		else if (errno == ENODEV)
		    printf("No such device\n");
		else
		    printf("Not found\n");
	    }
	    return (0);
	} else {
	    printf("%s: Can't get volume information from mountpoint '%s'\n",
		   uss_whoami, a_mountpoint);
	    return (code);
	}
    }

    /*Can't get volume status */
    /*
     * Pull out the volume name from the returned information and store
     * it in the common area.  It resides right after the basic volume
     * status structure.
     */
    statusP = (uss_VolumeStatus_t *) uss_fs_OutBuff;
    strcpy(uss_Volume, (((char *)statusP) + sizeof(*statusP)));
    volID = statusP->Vid;
    uss_VolumeID = volID;
    if (volID == 0) {
	printf("%s: Illegal volume ID: %d\n", uss_whoami, volID);
	return (-1);
    }

    /*
     * With the volume name in hand, find out where that volume
     * lives.  Make sure our VLDB stuff has been initialized first.
     */
    if (!initDone) {
	code = InitThisModule(NoAuthFlag, uss_ConfDir, uss_Cell);
	if (code)
	    return (code);
    }
    code = ubik_Call(VL_GetEntryByID, uconn_vldbP, 0, volID, -1, &vldbEntry);
    if (code) {
	printf("%s: Can't fetch VLDB entry for volume ID %d\n", uss_whoami,
	       volID);
	return (code);
    }

    /*
     * Translate the given VLDB entry from network to host format, then
     * checking on the volume's validity.  Specifically, it must be a
     * read/write volume and must only exist on one server.
     */
    MapHostToNetwork(&vldbEntry);
    if (vldbEntry.volumeId[RWVOL] != volID) {
	printf("%s: Volume '%s' (ID %d) is not a read/write volume!!\n",
	       uss_whoami, uss_Volume, volID);
	return (-1);
    }
    if (vldbEntry.nServers != 1) {
	printf("s: Volume '%s' (ID %d) exists on multiple servers!!\n",
	       uss_whoami, uss_Volume, volID);
	return (-1);
    }

    /*
     * Pull out the int32words containing the server and partition info
     * for the read/write volume.
     */
    code = GetServerAndPart(&vldbEntry, &serverID, &partID);
    if (code) {
	printf
	    ("%s: Can't get server/partition info from VLDB entry for volume '%s' (ID %d)\n",
	     uss_whoami, uss_Volume, volID);
	return (-1);
    }

    /*
     * Store the raw data, then translate the FileServer address to a
     * host name, and the partition ID to a partition name.
     */
    uss_ServerID = serverID;
    uss_PartitionID = partID;
    HostIDToHostName(serverID, uss_Server);
#ifdef USS_VOL_DB
    printf("[%s] Server ID 0x%x translated to '%s'\n", rn, serverID,
	   uss_Server);
#endif /* USS_VOL_DB */
    code = PartIDToPartName(partID, uss_Partition);
    if (code) {
	printf("%s: Error translating partition ID %d to partition name\n",
	       uss_whoami, partID);
	return (code);
    }

    /*
     * We got it, home boy.
     */
    return (0);

}				/*uss_vol_GetVolInfoFromMountPoint */
