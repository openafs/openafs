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

#include <afs/procmgmt.h>

#include <roken.h>
#include <afs/opr.h>

#ifdef IGNORE_SOME_GCC_WARNINGS
# pragma GCC diagnostic warning "-Wimplicit-function-declaration"
#endif

#ifdef AFS_NT40_ENV
#include <WINNT/afsevent.h>
#endif

#include <ctype.h>

#include <rx/rx.h>
#include <rx/rx_globals.h>
#include <rx/rxkad.h>
#include <rx/xdr.h>

#include <afs/afsint.h>
#include <afs/prs_fs.h>
#include <afs/nfs.h>
#include <afs/vlserver.h>
#include <lwp.h>
#include <lock.h>
#include <afs/afsutil.h>
#include <afs/authcon.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <afs/volser.h>
#include <ubik.h>
#include <afs/audit.h>
#include <afs/com_err.h>
#include <afs/cmd.h>
#include <afs/tcdata.h>
#include <afs/bubasics.h>
#include <afs/budb_errs.h>
#include <afs/budb_client.h>
#include <afs/bucoord_prototypes.h>
#include <afs/butx.h>
#include <afs/kautils.h>
#include <afs/bc.h>

#include "error_macros.h"
#define XBSA_TCMAIN
#include "butc_xbsa.h"
#include "butc_prototypes.h"
#include "butc_internal.h"

#define N_SECURITY_OBJECTS 3
#define ERRCODE_RANGE 8		/* from error_table.h */

#define TE_PREFIX  "TE"
#define TL_PREFIX  "TL"
#define CFG_PREFIX "CFG"

extern struct ubik_client *cstruct;
FILE *logIO, *ErrorlogIO, *centralLogIO, *lastLogIO;
char lFile[AFSDIR_PATH_MAX];
char logFile[AFSDIR_PATH_MAX + 256];
char ErrorlogFile[AFSDIR_PATH_MAX + 256];
char lastLogFile[AFSDIR_PATH_MAX + 256];
char eFile[AFSDIR_PATH_MAX];
char tapeConfigFile[AFSDIR_PATH_MAX];
char pFile[AFSDIR_PATH_MAX];
int debugLevel = 0;
struct tapeConfig globalTapeConfig;
struct deviceSyncNode *deviceLatch;
char globalCellName[64];
char *whoami = "butc";

/* GLOBAL CONFIGURATION PARAMETERS */
int dump_namecheck;
int queryoperator;
int autoQuery;
int isafile;
int tapemounted;
char *opencallout;
char *closecallout;
char *restoretofile;
int forcemultiple;

int maxpass;
#define PASSESMIN  1
#define PASSESMAX  10
#define PASSESDFLT 2
afs_int32 groupId;
#define MINGROUPID 0x1
#define MAXGROUPID 0x7FFFFFFF
afs_int32 statusSize;
#define MINSTATUS  1
#define MAXSTATUS  0x7fffffff
afs_int32 BufferSize;		/* Size in B stored for data */
char *centralLogFile;
afs_int32 lastLog;		/* Log last pass info */
int rxBind = 0;
struct afsconf_dir *butc_confdir;
int allow_unauth = 0;

#define ADDRSPERSITE 16         /* Same global is in rx/rx_user.c */
afs_uint32 SHostAddrs[ADDRSPERSITE];

static afs_int32
SafeATOL(char *anum)
{
    afs_int32 total;
    int tc;

    total = 0;
    while ((tc = *anum)) {
	if (tc < '0' || tc > '9')
	    return -1;
	total *= 10;
	total += (tc - '0');
	anum++;
    }
    return total;
}

/* atocl
 *	Convert a string into an afs_int32.
 *	Returned afs_int32 is in Bytes, Kb, Mb, Gb, or Tb. Based on crunit char.
 *	This routine only converts unsigned float values.
 *      The returned value is a whole number.
 * entry:
 *	numstring - text string to be converted.
 *	crunit - value returned in 'B', 'K', 'M', 'G', 'T'.
 *               ' ' or NULL ==> 'B' (bytes).
 * exit:
 *	number - returned value in requested crunit - rounded
 *               to nearest whole number.
 * fn return value:
 *	 0 - conversion ok
 *	-1 - error in conversion
 * notes:
 *	should deal with signed numbers. Should signal error if no digits
 *	seen.
 */
int
atocl(char *numstring, char crunit, afs_int32 *number)
{
    float total;
    afs_int32 runits;
    char cunit;
    afs_int32 units;
    afs_int32 count;
    char rest[256];

    /* Determine which units to report in */
    switch (crunit) {
    case 't':
    case 'T':
	runits = 12;
	break;

    case 'g':
    case 'G':
	runits = 9;
	break;

    case 'm':
    case 'M':
	runits = 6;
	break;

    case 'k':
    case 'K':
	runits = 3;
	break;

    case 'b':
    case 'B':
    case ' ':
    case 0:
	runits = 0;
	break;

    default:
	return (-1);
    }

    count =
	sscanf(numstring, "%f%c%s", &total, &cunit, rest);
    if ((count > 2) || (count <= 0))
	return -1;
    if (count == 1)
	cunit = 'B';		/* bytes */

    switch (cunit) {
    case 't':
    case 'T':
	units = 12;
	break;

    case 'g':
    case 'G':
	units = 9;
	break;

    case 'm':
    case 'M':
	units = 6;
	break;

    case 'k':
    case 'K':
	units = 3;
	break;

    case 'b':
    case 'B':
    case ' ':
    case 0:
	units = 0;
	break;

    default:
	return (-1);
    }

    /* Go to correct unit */
    for (; units < runits; units += 3)
	total /= 1024.0;
    for (; units > runits; units -= 3)
	total *= 1024.0;

    total += 0.5;		/* Round up */
    if ((total >= 2147483648.0) || (total < 0))	/* Don't go over 2G */
	*number = 2147483647;
    else
	*number = total;

    return (0);
}

static int
stringNowReplace(char *logFile, char *deviceName)
{
    char *pos = 0;
    char storeDevice[256];
    int mvFlag = 0, devPrefLen;
#ifdef AFS_NT40_ENV
    char devPrefix[] = "\\\\.";
#else
    char devPrefix[] = "/dev";
#endif

    devPrefLen = strlen(devPrefix);
    strcpy(storeDevice, deviceName);
    if (strncmp(deviceName, devPrefix, devPrefLen) == 0) {
	deviceName += devPrefLen;
	mvFlag++;
    }
    while ((pos = strchr(deviceName, devPrefix[0])))	/* look for / or \ */
	*pos = '_';
    strcat(logFile, deviceName);
    /* now put back deviceName to the way it was */
    if (mvFlag)
	deviceName -= devPrefLen;

    strcpy(deviceName, storeDevice);

    return (0);
}


/* GetDeviceConfig
 *	get the configuration information for a particular tape device
 *	as specified by the portoffset
 * entry:
 *	filename - full pathname of file containing the tape device
 *		configuration information
 *	config - for return results
 *	portOffset - for which configuration is required
 * notes:
 *	logging not available when this routine is called
 *	caller return value checks
 * exit:
 *      0 => Found entry with same port, return info in config.
 *     -1 => Error encountered trying to read the file.
 *      1 => Desired entry does not exist or file does not exist.
 */

#define	LINESIZE	256
static afs_int32
GetDeviceConfig(char *filename, struct tapeConfig *config, afs_int32 portOffset)
{
    FILE *devFile = 0;
    char line[LINESIZE];
    char devName[LINESIZE], tcapacity[LINESIZE], tfmsize[LINESIZE],
	trest[LINESIZE];
    afs_int32 aport;
    afs_int32 capacity;
    afs_int32 fmSize;
    afs_int32 code = 0, count;

    /* Initialize the config struct */
    config->capacity = 0;
    config->fileMarkSize = 0;
    config->portOffset = portOffset;
    strcpy(config->device, "");

    devFile = fopen(filename, "r");
    if (!devFile) {
	if (errno == ENOENT)
	    ERROR_EXIT(1);
	fprintf(stderr, "Error %d: Can't open %s\n", errno, filename);
	ERROR_EXIT(-1);
    }

    while (fgets(line, LINESIZE - 1, devFile)) {
	count =
	    sscanf(line, "%s %s %s %u%s\n", tcapacity, tfmsize, devName,
		   &aport, trest);

	if (count == 4 || count == 5) {
	    if (atocl(tcapacity, 'K', &capacity)) {
		fprintf(stderr,
			"tapeconfig: Tape capacity parse error in: %s\n",
			line);
		ERROR_EXIT(-1);
	    }
	    if (atocl(tfmsize, 'B', &fmSize)) {
		fprintf(stderr,
			"tapeconfig: File-mark size parse error in: %s\n",
			line);
		ERROR_EXIT(-1);
	    }
	} else {
	    count = sscanf(line, "%s %u%s\n", devName, &aport, trest);
	    if (count == 2 || count == 3) {
		capacity = 0x7fffffff;
		fmSize = 0;
	    } else {
		fprintf(stderr, "tapeconfig: Parse error in: %s\n", line);
		ERROR_EXIT(-1);
	    }
	}

	if ((aport < 0) || (aport > BC_MAXPORTOFFSET)) {
	    fprintf(stderr, "tapeconfig: Port offset parse error in: %s\n",
		    line);
	    ERROR_EXIT(-1);
	}

	if (aport != portOffset)
	    continue;

	if (fmSize < 0) {
	    fprintf(stderr, "Invalid file mark size, %d, in: %s\n", fmSize,
		    line);
	    ERROR_EXIT(-1);
	}

	config->capacity = capacity;
	config->fileMarkSize = fmSize;
	config->portOffset = aport;
	strncpy(config->device, devName, 100);

	ERROR_EXIT(0);
    }

    /* fprintf(stderr, "Can't find tapeconfig entry for port offset %d\n", portOffset); */
    ERROR_EXIT(1);

  error_exit:
    if (devFile)
	fclose(devFile);
    return (code);
}

/* GetConfigParams
 */
static afs_int32
GetConfigParams(char *filename, afs_int32 port)
{
    char paramFile[AFSDIR_PATH_MAX + 257];
    FILE *devFile = 0;
    char line[LINESIZE], cmd[LINESIZE], value[LINESIZE];
    afs_int32 code = 0;
    int cnt;

    /* DEFAULT SETTINGS FOR GLOBAL PARAMETERS */
    dump_namecheck = 1;		/* check tape name on dumps */
    queryoperator = 1;		/* can question operator */
    autoQuery = 1;		/* prompt for first tape */
    isafile = 0;		/* Do not dump to a file */
    opencallout = NULL;		/* open  callout routine */
    closecallout = NULL;	/* close callout routine */
    tapemounted = 0;		/* tape is not mounted */
#ifdef xbsa
    BufferSize = (CONF_XBSA ? XBSADFLTBUFFER : BUTM_BLOCKSIZE);
    dumpRestAuthnLevel = rpc_c_protect_level_default;
    xbsaObjectOwner = NULL;	/* bsaObjectOwner */
    appObjectOwner = NULL;	/* appObjectOwner */
    adsmServerName = NULL;	/* TSM server name - same as ADSM */
    xbsaSecToken = NULL;	/* XBSA sercurity token */
    xbsalGName = NULL;		/* XBSA IGName */
#else
    BufferSize = BUTM_BLOCKSIZE;
#endif /*xbsa */
    centralLogFile = NULL;	/* Log for all butcs */
    centralLogIO = 0;		/* Log for all butcs */
    statusSize = 0;		/* size before status message */
    maxpass = PASSESDFLT;	/* dump passes */
    lastLog = 0;		/* separate log for last pass */
    lastLogIO = 0;		/* separate log for last pass */
    groupId = 0;		/* Group id for multiple dumps */

    /* Try opening the CFG_<port> file */
    snprintf(paramFile, sizeof(paramFile), "%s_%d", filename, port);
    devFile = fopen(paramFile, "r");
    if (devFile) {
	/* Set log names to TL_<port>, TL_<port>.lp and TE_<port> */
	snprintf(logFile, sizeof(logFile), "%s_%d", lFile, port);
	snprintf(lastLogFile, sizeof(lastLogFile), "%s_%d.lp", lFile, port);
	snprintf(ErrorlogFile, sizeof(ErrorlogFile), "%s_%d", eFile, port);
    } else if (CONF_XBSA) {
	/* If configured as XBSA, a configuration file CFG_<port> must exist */
	printf("Cannot open configuration file %s", paramFile);
	ERROR_EXIT(1);
    } else {
	/* Try the CFG_<device> name as the device file */
	strcpy(paramFile, filename);
	stringNowReplace(paramFile, globalTapeConfig.device);
	/* Set log names to TL_<device>, TL_<device> and TE_<device> */
	strcpy(logFile, lFile);
	stringNowReplace(logFile, globalTapeConfig.device);
	strcpy(lastLogFile, lFile);
	stringNowReplace(lastLogFile, globalTapeConfig.device);
	strcat(lastLogFile, ".lp");
	strcpy(ErrorlogFile, eFile);
	stringNowReplace(ErrorlogFile, globalTapeConfig.device);

	/* Now open the device file */
	devFile = fopen(paramFile, "r");
	if (!devFile)
	    ERROR_EXIT(0);	/* CFG file doesn't exist for non-XBSA and that's ok */
    }

    /* Read each line of the Configuration file */
    while (fgets(line, LINESIZE - 1, devFile)) {
	cnt = sscanf(line, "%s %s", cmd, value);
	if (cnt != 2) {
	    if (cnt > 0)
		printf("Bad line in %s: %s\n", paramFile, line);
	    continue;
	}

	for (cnt = 0; cnt < strlen(cmd); cnt++)
	    if (islower(cmd[cnt]))
		cmd[cnt] = toupper(cmd[cnt]);

	if (!strcmp(cmd, "NAME_CHECK")) {
	    if (CONF_XBSA) {
		printf
		    ("Warning: The %s parameter is ignored with a Backup Service\n",
		     cmd);
		continue;
	    }

	    for (cnt = 0; cnt < strlen(value); cnt++)
		if (islower(value[cnt]))
		    value[cnt] = toupper(value[cnt]);

	    if (!strcmp(value, "NO")) {
		printf("Dump tape name check is disabled\n");
		dump_namecheck = 0;
	    } else {
		printf("Dump tape name check is enabled\n");
		dump_namecheck = 1;
	    }
	}

	else if (!strcmp(cmd, "MOUNT")) {
	    if (CONF_XBSA) {
		printf
		    ("Warning: The %s parameter is ignored with a Backup Service\n",
		     cmd);
		continue;
	    }

	    opencallout = strdup(value);
	    printf("Tape mount callout routine is %s\n", opencallout);
	}

	else if (!strcmp(cmd, "UNMOUNT")) {
	    if (CONF_XBSA) {
		printf
		    ("Warning: The %s parameter is ignored with a Backup Service\n",
		     cmd);
		continue;
	    }

	    closecallout = strdup(value);
	    printf("Tape unmount callout routine is %s\n", closecallout);
	}

	else if (!strcmp(cmd, "ASK")) {
	    for (cnt = 0; cnt < strlen(value); cnt++)
		if (islower(value[cnt]))
		    value[cnt] = toupper(value[cnt]);

	    if (!strcmp(value, "NO")) {
		printf("Operator queries are disabled\n");
		queryoperator = 0;
	    } else {
		printf("Operator queries are enabled\n");
		queryoperator = 1;
	    }
	}

	else if (!strcmp(cmd, "FILE")) {
	    if (CONF_XBSA) {
		printf
		    ("Warning: The %s parameter is ignored with a Backup Service\n",
		     cmd);
		continue;
	    }

	    for (cnt = 0; cnt < strlen(value); cnt++)
		if (islower(value[cnt]))
		    value[cnt] = toupper(value[cnt]);

	    if (!strcmp(value, "YES")) {
		printf("Will dump to a file\n");
		isafile = 1;
	    } else {
		printf("Will not dump to a file\n");
		isafile = 0;
	    }
	}

	else if (!strcmp(cmd, "AUTOQUERY")) {
	    if (CONF_XBSA) {
		printf
		    ("Warning: The %s parameter is ignored with a Backup Service\n",
		     cmd);
		continue;
	    }

	    for (cnt = 0; cnt < strlen(value); cnt++)
		if (islower(value[cnt]))
		    value[cnt] = toupper(value[cnt]);

	    if (!strcmp(value, "NO")) {
		printf("Auto query is disabled\n");
		autoQuery = 0;
	    } else {
		printf("Auto query is enabled\n");
		autoQuery = 1;
	    }
	}

	else if (!strcmp(cmd, "BUFFERSIZE")) {
	    afs_int32 size;
	    afs_int32 tapeblocks;

	    if (!CONF_XBSA) {
		if (atocl(value, 'K', &size)) {
		    fprintf(stderr, "BUFFERSIZE parse error\n");
		    size = 0;
		}

		/* A tapeblock is 16KB. Determine # of tapeblocks. Then
		 * determine BufferSize needed for that many tapeblocks.
		 */
		tapeblocks = size / 16;
		if (tapeblocks <= 0)
		    tapeblocks = 1;
		printf("BUFFERSIZE is %u KBytes\n", (tapeblocks * 16));
		BufferSize = tapeblocks * BUTM_BLOCKSIZE;
	    } else {
#ifdef xbsa
		if (atocl(value, 'B', &size)) {
		    fprintf(stderr, "BUFFERSIZE parse error\n");
		    size = 0;
		}
		if (size < XBSAMINBUFFER)
		    size = XBSAMINBUFFER;
		if (size > XBSAMAXBUFFER)
		    size = XBSAMAXBUFFER;
		printf("XBSA buffer size is %u Bytes\n", size);
		BufferSize = size;
#endif
	    }
	}
#ifndef xbsa
	/* All the xbsa spacific parameters */
	else if (!strcmp(cmd, "TYPE") || !strcmp(cmd, "NODE")
		 || !strcmp(cmd, "SERVER") || !strcmp(cmd, "PASSWORD")
		 || !strcmp(cmd, "PASSFILE") || !strcmp(cmd, "MGMTCLASS")) {
	    printf("This binary does not have XBSA support\n");
	    return 1;
	}
#else
	else if (!strcmp(cmd, "TYPE")) {	/* required for XBSA */
	    if (!CONF_XBSA) {
		printf
		    ("Warning: The %s parameter is ignored with a tape drive\n",
		     cmd);
		continue;
	    }

	    for (cnt = 0; (size_t) cnt < strlen(value); cnt++)
		if (islower(value[cnt]))
		    value[cnt] = toupper(value[cnt]);

	    if (strcmp(value, "TSM") == 0) {
		xbsaType = XBSA_SERVER_TYPE_ADSM;	/* Known XBSA server type */
	    } else {
		printf("Configuration file error, %s %s is not recognized\n",
		       cmd, value);
		xbsaType = XBSA_SERVER_TYPE_UNKNOWN;
	    }
	    printf("XBSA type is %s\n",
		   ((xbsaType ==
		     XBSA_SERVER_TYPE_UNKNOWN) ? "Unknown" : value));
	}

	else if (!strcmp(cmd, "NODE")) {
	    if (!CONF_XBSA) {
		printf
		    ("Warning: The %s parameter is ignored with a tape drive\n",
		     cmd);
		continue;
	    }
	    xbsaObjectOwner = strdup(value);
	    printf("XBSA node is %s\n", xbsaObjectOwner);
	}

	else if (!strcmp(cmd, "SERVER")) {	/* required for XBSA */
	    if (!CONF_XBSA) {
		printf
		    ("Warning: The %s parameter is ignored with a tape drive\n",
		     cmd);
		continue;
	    }
	    adsmServerName = strdup(value);
	    printf("XBSA server is %s\n", adsmServerName);
	}

	else if (!strcmp(cmd, "PASSWORD")) {	/* This or PASSFILE required for XBSA */
	    if (!CONF_XBSA) {
		printf
		    ("Warning: The %s parameter is ignored with a tape drive\n",
		     cmd);
		continue;
	    }
	    if (xbsaSecToken) {
		printf
		    ("Warning: The %s parameter is ignored. Already read password\n",
		     cmd);
		continue;
	    }

	    xbsaSecToken = strdup(value);
	    printf("XBSA Password has been read\n");
	}

	else if (!strcmp(cmd, "PASSFILE")) {	/* This or PASSWORD required for XBSA */
	    FILE *pwdFile;
	    if (!CONF_XBSA) {
		printf
		    ("Warning: The %s parameter is ignored with a tape drive\n",
		     cmd);
		continue;
	    }
	    if (xbsaSecToken) {
		printf
		    ("Warning: The %s parameter is ignored. Already read password\n",
		     cmd);
		continue;
	    }

	    pwdFile = fopen(value, "r");
	    if (!pwdFile) {
		printf
		    ("Configuration file error, cannot open password file %s\n",
		     value);
		ERROR_EXIT(1);
	    }
	    xbsaSecToken = malloc(LINESIZE);
	    if (!fscanf(pwdFile, "%s", xbsaSecToken)) {
		printf
		    ("Configuration file error, cannot read password file %s\n",
		     value);
		ERROR_EXIT(1);
	    }
	    printf("XBSA password retrieved from password file\n");
	}

	else if (!strcmp(cmd, "MGMTCLASS")) {	/* XBSA */
	    if (!CONF_XBSA) {
		printf
		    ("Warning: The %s parameter is ignored with a tape drive\n",
		     cmd);
		continue;
	    }
	    xbsalGName = strdup(value);
	    printf("XBSA management class is %s\n", xbsalGName);
	}
#endif

	else if (!strcmp(cmd, "MAXPASS")) {
	    maxpass = SafeATOL(value);
	    if (maxpass < PASSESMIN)
		maxpass = PASSESMIN;
	    if (maxpass > PASSESMAX)
		maxpass = PASSESMAX;
	    printf("MAXPASS is %d\n", maxpass);
	}

	else if (!strcmp(cmd, "GROUPID")) {
	    groupId = SafeATOL(value);
	    if ((groupId < MINGROUPID) || (groupId > MAXGROUPID)) {
		printf("Configuration file error, %s %s is invalid\n", cmd,
		       value);
		ERROR_EXIT(1);
	    }
	    printf("Group Id is %d\n", groupId);
	}

	else if (!strcmp(cmd, "LASTLOG")) {
	    for (cnt = 0; (size_t) cnt < strlen(value); cnt++)
		if (islower(value[cnt]))
		    value[cnt] = toupper(value[cnt]);

	    lastLog = (strcmp(value, "YES") == 0);
	    printf("Will %sgenerate a last log\n", (lastLog ? "" : "not "));
	}

	else if (!strcmp(cmd, "CENTRALLOG")) {
	    centralLogFile = strdup(value);
	    printf("Central log file is %s\n", centralLogFile);
	}

	else if (!strcmp(cmd, "STATUS")) {
	    if (atocl(value, 'B', &statusSize)) {
		fprintf(stderr, "STATUS parse error\n");
		statusSize = 0;
	    }
	    if (statusSize < MINSTATUS)
		statusSize = MINSTATUS;
	    if (statusSize > MAXSTATUS)
		statusSize = MAXSTATUS;
	}

	else {
	    printf("Warning: Unrecognized configuration parameter: %s", line);
	}
    }				/*fgets */

    if (statusSize) {
	/* Statussize is in bytes and requires that BufferSize be set first */
	statusSize *= BufferSize;
	if (statusSize < 0)
	    statusSize = 0x7fffffff;	/*max size */
	printf("Status every %ld Bytes\n", afs_printable_int32_ld(statusSize));
    }

  error_exit:
    if (devFile)
	fclose(devFile);

    /* If the butc is configured as XBSA, check for required parameters */
#ifdef xbsa
    if (!code && CONF_XBSA) {
	if (xbsaType == XBSA_SERVER_TYPE_UNKNOWN) {
	    printf
		("Configuration file error, the TYPE parameter must be specified, or\n");
	    printf("an entry must exist in %s for port %d\n", tapeConfigFile,
		   port);
	    code = 1;
	}
	if (!adsmServerName) {
	    printf
		("Configuration file error, the SERVER parameter must be specified\n");
	    code = 1;
	}
	if (!xbsaSecToken) {
	    printf
		("Configuration file error, the PASSWORD or PASSFILE parameter must be specified\n");
	    code = 1;
	}
    }
#endif /*xbsa */
    return (code);
}

#ifdef xbsa
static void
xbsa_shutdown(int x)
{
    xbsa_Finalize(&butxInfo);
    exit(0);
}
#endif

static int
tc_IsLocalRealmMatch(void *rock, char *name, char *inst, char *cell)
{
    struct afsconf_dir *dir = (struct afsconf_dir *)rock;
    afs_int32 islocal = 0;	/* default to no */
    int code;

    code = afsconf_IsLocalRealmMatch(dir, &islocal, name, inst, cell);
    if (code) {
	TLog(0, "Failed local realm check; code=%d, name=%s, inst=%s, cell=%s\n",
		 code, name, inst, cell);
    }
    return islocal;
}

static int
WorkerBee(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code, numClasses;
    struct rx_securityClass *(nullObjects[1]), **secObjs, **allObjs;
    struct rx_service *service;
    time_t tokenExpires;
    char cellName[64];
    int localauth;
    /*process arguments */
    afs_int32 portOffset = 0;
#ifdef AFS_PTHREAD_ENV
    pthread_t dbWatcherPid;
    pthread_attr_t tattr;
    AFS_SIGSET_DECL;
#else
    PROCESS dbWatcherPid;
#endif
    char hoststr[16];
    afs_uint32 host = htonl(INADDR_ANY);
    char *auditFileName = NULL;
    char *auditInterface = NULL;

    debugLevel = 0;

    /*initialize the error tables */
    initialize_KA_error_table();
    initialize_RXK_error_table();
    initialize_KTC_error_table();
    initialize_ACFG_error_table();
    initialize_CMD_error_table();
    initialize_VL_error_table();
    initialize_BUTM_error_table();
    initialize_BUTC_error_table();
#ifdef xbsa
    initialize_BUTX_error_table();
#endif /*xbs */
    initialize_VOLS_error_table();
    initialize_BUDB_error_table();
    initialize_BUCD_error_table();

    if (as->parms[0].items) {
	portOffset = SafeATOL(as->parms[0].items->data);
	if (portOffset == -1) {
	    fprintf(stderr, "Illegal port offset '%s'\n",
		    as->parms[0].items->data);
	    exit(1);
	} else if (portOffset > BC_MAXPORTOFFSET) {
	    fprintf(stderr, "%u exceeds max port offset %u\n", portOffset,
		    BC_MAXPORTOFFSET);
	    exit(1);
	}
    }

    xbsaType = XBSA_SERVER_TYPE_NONE;	/* default */
    if (as->parms[3].items) {	/* -device */
	globalTapeConfig.capacity = 0x7fffffff;	/* 2T for max tape capacity */
	globalTapeConfig.fileMarkSize = 0;
	globalTapeConfig.portOffset = portOffset;
	strncpy(globalTapeConfig.device, as->parms[3].items->data, 100);
	xbsaType = XBSA_SERVER_TYPE_NONE;	/* Not XBSA */
    } else {
	/* Search for an entry in tapeconfig file */
	code = GetDeviceConfig(tapeConfigFile, &globalTapeConfig, portOffset);
	if (code == -1) {
	    fprintf(stderr, "Problem in reading config file %s\n",
		    tapeConfigFile);
	    exit(1);
	}
	/* Set xbsaType. If code == 1, no entry was found in the tapeconfig file so
	 * it's an XBSA server. Don't know if its ADSM or not so its unknown.
	 */
	xbsaType =
	    ((code == 1) ? XBSA_SERVER_TYPE_UNKNOWN : XBSA_SERVER_TYPE_NONE);
    }

    if (as->parms[6].items) {	/* -restoretofile */
	restoretofile = strdup(as->parms[6].items->data);
	printf("Restore to file '%s'\n", restoretofile);
    }

    /* Go and read the config file: CFG_<device> or CFG_<port>. We will also set
     * the exact xbsaType within the call (won't be unknown) - double check.
     */
    code = GetConfigParams(pFile, portOffset);
    if (code)
	exit(code);
#ifdef xbsa
    if (xbsaType == XBSA_SERVER_TYPE_UNKNOWN) {
	printf
	    ("\nConfiguration file error, the TYPE parameter must be specified, or\n");
	printf("an entry must exist in %s for port %d\n", tapeConfigFile,
	       portOffset);
	exit(1);
    }
#else
    /* Not compiled for XBSA code so we can't support it */
    if (CONF_XBSA) {
	printf("\nNo entry found in %s for port %d\n", tapeConfigFile,
	       portOffset);
	printf("This binary does not have XBSA support\n");
	exit(1);
    }
#endif

    /* Open the log files. The pathnames were set in GetConfigParams() */
    logIO = fopen(logFile, "a");
    if (!logIO) {
	fprintf(stderr, "Failed to open %s\n", logFile);
	exit(1);
    }
    ErrorlogIO = fopen(ErrorlogFile, "a");
    if (!ErrorlogIO) {
	fprintf(stderr, "Failed to open %s\n", ErrorlogFile);
	exit(1);
    }
    if (lastLog) {
	lastLogIO = fopen(lastLogFile, "a");
	if (!lastLogIO) {
	    fprintf(stderr, "Failed to open %s\n", lastLogFile);
	    exit(1);
	}
    }
    if (centralLogFile) {
	struct stat sbuf;
	afs_int32 statcode;
#ifndef AFS_NT40_ENV
	char *path;
#endif

	statcode = stat(centralLogFile, &sbuf);
	centralLogIO = fopen(centralLogFile, "a");
	if (!centralLogIO) {
	    fprintf(stderr, "Failed to open %s; error %d\n", centralLogFile,
		    errno);
	    exit(1);
	}
#ifndef AFS_NT40_ENV
	/* Make sure it is not in AFS, has to have been created first */
	path = malloc(AFSDIR_PATH_MAX);
	if (path == NULL || !realpath(centralLogFile, path)) {
	    fprintf(stderr,
		    "Warning: can't determine real path of '%s' (%d)\n",
		    centralLogFile, errno);
	} else {
	    if (strncmp(path, "/afs/", 5) == 0) {
		fprintf(stderr, "The central log '%s' should not be in AFS\n",
			centralLogFile);
		exit(1);
	    }
	}
	free(path);
#endif

	/* Write header if created it */
	if (statcode) {
	    char *h1 =
		"TASK   START DATE/TIME      END DATE/TIME        ELAPSED   VOLUMESET\n";
	    char *h2 =
		"-----  -------------------  -------------------  --------  ---------\n";
	    /* File didn't exist before so write the header lines */
	    fwrite(h1, strlen(h1), 1, centralLogIO);
	    fwrite(h2, strlen(h2), 1, centralLogIO);
	    fflush(centralLogIO);
	}
    }

    /* Open the configuration directory */
    butc_confdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
    if (butc_confdir == NULL) {
	TLog(0, "Failed to open server configuration directory");
	exit(1);
    }

    /* Start auditing */
    osi_audit_init();
    if (as->parms[9].items) {
	auditFileName = as->parms[9].items->data;
    }
    if (auditFileName != NULL)
	osi_audit_file(auditFileName);
    if (as->parms[10].items) {
	auditInterface = as->parms[10].items->data;
	if (osi_audit_interface(auditInterface)) {
	    TLog(0, "Invalid audit interface '%s'\n", auditInterface);
	    exit(1);
	}
    }
    osi_audit(TC_StartEvent, 0, AUD_END);
    osi_audit_set_user_check(butc_confdir, tc_IsLocalRealmMatch);

    if (as->parms[1].items) {
	debugLevel = SafeATOL(as->parms[1].items->data);
	if (debugLevel == -1) {
	    TLog(0, "Illegal debug level '%s'\n", as->parms[1].items->data);
	    exit(1);
	}
    }
#ifdef xbsa
    /* Setup XBSA library interface */
    if (CONF_XBSA) {
	afs_int32 rc;
	rc = xbsa_MountLibrary(&butxInfo, xbsaType);
	if (rc != XBSA_SUCCESS) {
	    TapeLog(0, 0, rc, 0, "Unable to mount the XBSA library\n");
	    return (1);
	}

	forcemultiple = (as->parms[7].items ? 1 : 0);/*-xbsaforcemultiple */
	if (forcemultiple)
	    printf("Force XBSA multiple server support\n");

	rc = InitToServer(0 /*taskid */ , &butxInfo, adsmServerName);
	if (rc != XBSA_SUCCESS)
	    return (1);
	(void)signal(SIGINT, xbsa_shutdown);
	(void)signal(SIGHUP, xbsa_shutdown);
    }
#endif /*xbsa */

    /* cell switch */
    if (as->parms[2].items)
	strncpy(cellName, as->parms[2].items->data, sizeof(cellName));
    else
	cellName[0] = '\0';

    if (as->parms[4].items)
	autoQuery = 0;

    localauth = (as->parms[5].items ? 1 : 0);
    rxBind = (as->parms[8].items ? 1 : 0);
    allow_unauth = (as->parms[11].items ? 1 : 0);

    if (!allow_unauth && !localauth) {
	const char *errstr = "Neither -localauth nor -allow_unauthenticated was provided; refusing to start in unintended insecure configuration\n";
	TLog(0, "%s", (char *)errstr);
	exit(1);
    }

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
        if (ccode == 1)
            host = SHostAddrs[0];
    }

    TLog(0, "butc binding rx to %s:%d\n",
         afs_inet_ntoa_r(host, hoststr), BC_TAPEPORT + portOffset);
    code = rx_InitHost(host, htons(BC_TAPEPORT + portOffset));
    if (code) {
	TapeLog(0, 0, code, 0, "rx init failed on port %u\n",
		BC_TAPEPORT + portOffset);
	exit(1);
    }
    rx_SetRxDeadTime(150);

    /* Establish connection with the vldb server */
    code = vldbClientInit(0, localauth, cellName, &cstruct, &tokenExpires);
    if (code) {
	TapeLog(0, 0, code, 0, "Can't access vldb\n");
	return code;
    }

    strcpy(globalCellName, cellName);

    /*initialize the dumpNode list */
    InitNodeList(portOffset);

    deviceLatch = malloc(sizeof(struct deviceSyncNode));
    Lock_Init(&(deviceLatch->lock));
    deviceLatch->flags = 0;

    /* initialize database support, volume support, and logs */

    /*
     * Create security objects for the Rx server functionality.  Historically
     * this was a single rxnull security object, since the tape controller was
     * run by an operator that had local access to the tape device and some
     * administrative privilege in the cell (to be able to perform volume-level
     * accesses), but on a machine that was not necessarily trusted to hold the
     * cell-wide key.
     *
     * Such a configuration is, of course, insecure because anyone can make
     * inbound RPCs and manipulate the database, including creating bogus
     * dumps and restoring them!  Additionally, in modern usage, butc is
     * frequently run with -localauth to authenticate its outbound connections
     * to the volservers and budb with the cell-wide key, in which case the
     * cell-wide key is present and could be used to authenticate incoming
     * connections as well.
     *
     * If -localauth is in use, create the full barrage of server security
     * objects, including rxkad, so that inbound connections can be verified
     * to only be made by authenticated clients.  Otherwise, only the rxnull
     * class is in use with a single server security object.  Note that butc
     * will refuse to start in this configuration unless the
     * "-allow_unauthenticated" flag is provided, indicating that the operator
     * has ensured that incoming connections are appropriately restricted by
     * firewall configuration or network topology.
     */

    if (allow_unauth) {
	nullObjects[RX_SECIDX_NULL] = rxnull_NewServerSecurityObject();
	if (!nullObjects[RX_SECIDX_NULL]) {
	    TLog(0, "rxnull_NewServerSecurityObject");
	    exit(1);
	}
	numClasses = 1;
	secObjs = nullObjects;
    } else {
	/* Must be -localauth, so the cell keys are available. */
	struct afsconf_bsso_info bsso;
	memset(&bsso, 0, sizeof(bsso));
	bsso.dir = butc_confdir;
	afsconf_BuildServerSecurityObjects_int(&bsso, &allObjs, &numClasses);
	secObjs = allObjs;
    }

    service =
	rx_NewServiceHost(host, 0, 1, "BUTC", secObjs, numClasses, TC_ExecuteRequest);
    if (!service) {
	TLog(0, "rx_NewService");
	exit(1);
    }
    rx_SetMaxProcs(service, 4);

    /* Establish connection to the backup database */
    code = udbClientInit(0, localauth, cellName);
    if (code) {
	TapeLog(0, 0, code, 0, "Can't access backup database\n");
	exit(1);
    }
    /* This call is here to verify that we are authentiated.
     * The call does nothing and will return BUDB_NOTPERMITTED
     * if we don't belong.
     */
    code = bcdb_deleteDump(0, 0, 0, 0);
    if (code == BUDB_NOTPERMITTED) {
	TapeLog(0, 0, code, 0, "Can't access backup database\n");
	exit(1);
    }

    initStatus();
#ifdef AFS_PTHREAD_ENV
    code = pthread_attr_init(&tattr);
    if (code) {
	TapeLog(0, 0, code, 0,
		"Can't pthread_attr_init database monitor task");
	exit(1);
    }
    code = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    if (code) {
	TapeLog(0, 0, code, 0,
		"Can't pthread_attr_setdetachstate database monitor task");
	exit(1);
    }
    AFS_SIGSET_CLEAR();
    code = pthread_create(&dbWatcherPid, &tattr, dbWatcher, (void *)2);
    AFS_SIGSET_RESTORE();
#else
    code =
	LWP_CreateProcess(dbWatcher, 20480, LWP_NORMAL_PRIORITY, (void *)2,
			  "dbWatcher", &dbWatcherPid);
#endif
    if (code) {
	TapeLog(0, 0, code, 0, "Can't create database monitor task");
	exit(1);
    }

    TLog(0, "Starting Tape Coordinator: Port offset %u   Debug level %u\n",
	 portOffset, debugLevel);
    TLog(0, "Token expires: %s\n", cTIME(&tokenExpires));

    rx_StartServer(1);		/* Donate this process to the server process pool */
    TLog(0, "Error: StartServer returned");
    exit(1);
}

#ifndef AFS_NT40_ENV
#include "AFS_component_version_number.c"
#endif

int
main(int argc, char **argv)
{
    struct cmd_syndesc *ts;
    struct cmd_item *ti;

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

    setlinebuf(stdout);

    ts = cmd_CreateSyntax(NULL, WorkerBee, NULL, 0, "tape coordinator");
    cmd_AddParm(ts, "-port", CMD_SINGLE, CMD_OPTIONAL, "port offset");
    cmd_AddParm(ts, "-debuglevel", CMD_SINGLE, CMD_OPTIONAL, "0 | 1 | 2");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    cmd_AddParm(ts, "-device", CMD_SINGLE, (CMD_OPTIONAL | CMD_HIDE),
		"tape device path");
    cmd_AddParm(ts, "-noautoquery", CMD_FLAG, CMD_OPTIONAL,
		"do not query operator for first tape");
    cmd_AddParm(ts, "-localauth", CMD_FLAG, CMD_OPTIONAL,
		"create tickets from KeyFile");
    cmd_AddParm(ts, "-restoretofile", CMD_SINGLE, (CMD_OPTIONAL | CMD_HIDE),
		"file to restore to");
    cmd_AddParm(ts, "-xbsaforcemultiple", CMD_FLAG, (CMD_OPTIONAL | CMD_HIDE),
		"Force multiple XBSA server support");
    cmd_AddParm(ts, "-rxbind", CMD_FLAG, CMD_OPTIONAL,
		"bind Rx socket");
    cmd_AddParm(ts, "-auditlog", CMD_SINGLE, CMD_OPTIONAL, "location of audit log");
    cmd_AddParm(ts, "-audit-interface", CMD_SINGLE, CMD_OPTIONAL,
		"interface to use for audit logging");
    cmd_AddParm(ts, "-allow_unauthenticated", CMD_FLAG, CMD_OPTIONAL,
		"allow unauthenticated inbound RPCs (requires firewalling)");

    /* Initialize dirpaths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
#ifdef AFS_NT40_ENV
	ReportErrorEventAlt(AFSEVT_SVR_NO_INSTALL_DIR, 0, argv[0], 0);
#endif
	fprintf(stderr, "Unable to obtain AFS server directory.\n");
	exit(2);
    }

    /* setup the file paths */
    strcompose(eFile, AFSDIR_PATH_MAX, AFSDIR_SERVER_BACKUP_DIRPATH, "/",
	       TE_PREFIX, (char *)NULL);
    strcompose(lFile, AFSDIR_PATH_MAX, AFSDIR_SERVER_BACKUP_DIRPATH, "/",
	       TL_PREFIX, (char *)NULL);
    strcompose(pFile, AFSDIR_PATH_MAX, AFSDIR_SERVER_BACKUP_DIRPATH, "/",
	       CFG_PREFIX, (char *)NULL);
    strcpy(tapeConfigFile, AFSDIR_SERVER_TAPECONFIG_FILEPATH);

    /* special case "no args" case since cmd_dispatch gives help message
     * instead
     */
    if (argc == 1) {
	ts = calloc(1, sizeof(struct cmd_syndesc));

	ti = malloc(sizeof(struct cmd_item));
	ti->next = 0;
	ti->data = "0";
	ts->parms[0].items = ti;
	ti = malloc(sizeof(struct cmd_item));
	ti->next = 0;
	ti->data = "0";
	ts->parms[1].items = ti;
	ts->parms[2].items = NULL;
	ts->parms[3].items = NULL;
	ts->parms[4].items = NULL;
	ts->parms[5].items = NULL;
	return WorkerBee(ts, NULL);
    } else
	return cmd_Dispatch(argc, argv);
}
