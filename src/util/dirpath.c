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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include "assert.h"
#include "afsutil.h"
#include "fileutil.h"
#ifdef AFS_PTHREAD_ENV
#include <pthread.h>
static pthread_once_t dirInit_once = PTHREAD_ONCE_INIT;
#endif
#ifdef AFS_NT40_ENV
#include <windows.h>
#include <WINNT\afssw.h>
#endif
#ifdef AFS_DARWIN_ENV
#include <unistd.h>
#endif

/* local vars */
/* static storage for path strings */
static char dirPathArray[AFSDIR_PATHSTRING_MAX][AFSDIR_PATH_MAX];

/* indicate if and how the dirpath module initialized. */
static int initFlag = 0;
static unsigned int initStatus = 0;


/* storage for dynamically-determined install dir (NT only; long and short) */
#ifdef AFS_NT40_ENV
static char ntServerInstallDirLong[AFSDIR_PATH_MAX];
static char ntServerInstallDirShort[AFSDIR_PATH_MAX];
static char ntClientConfigDirLong[AFSDIR_PATH_MAX];
static char ntClientConfigDirShort[AFSDIR_PATH_MAX];
#endif

/* storage for local afs server/client paths (top-level) */
static char afsSrvDirPath[AFSDIR_PATH_MAX];
static char afsClntDirPath[AFSDIR_PATH_MAX];

/* internal array init function */
static void initDirPathArray(void);

/* Additional macros for ease of use */
/* buf is expected to be atleast AFS_PATH_MAX bytes long */
#define AFSDIR_SERVER_DIRPATH(buf, dir)  \
            (void) strcompose(buf, AFSDIR_PATH_MAX, serverPrefix, dir, NULL)

#define AFSDIR_SERVER_FILEPATH(buf, dir, file)  \
            (void) strcompose(buf, AFSDIR_PATH_MAX, serverPrefix, dir, "/", file,  NULL)

#define AFSDIR_CLIENT_DIRPATH(buf, dir)  \
            (void) strcompose(buf, AFSDIR_PATH_MAX, clientPrefix, dir, NULL)

#define AFSDIR_CLIENT_FILEPATH(buf, dir, file)  \
            (void) strcompose(buf, AFSDIR_PATH_MAX,  clientPrefix, dir, "/", file,  NULL)


/* initAFSDirPath() -- External users call this function to initialize
 * the dirpath module and/or to determine the initialization status.
 */
unsigned int
initAFSDirPath(void)
{
    if (initFlag == 0) {	/* not yet init'ed, so initialize */
#ifdef AFS_PTHREAD_ENV
	pthread_once(&dirInit_once, initDirPathArray);
#else
	initDirPathArray();
#endif
    }
    return initStatus;
}


/* initDirPathArray() -- Initializes the afs dir paths for the 
 *     server and client installations.
 *
 *     For NT these are determined dynamically; for Unix they are static.
 *
 *     NT NOTE: If a particular component (client/server) is not installed
 *              then we may not be able to initialize the paths to anything
 *              meaningful.  In this case the paths are set to the local
 *              temp directory to avoid later reference to an uninitialized
 *              variable.  The initStatus flag is set to indicate which
 *              paths (client/server) initialized properly for callers of
 *              initAFSDirPath() who would like to know this information.
 */
static void
initDirPathArray(void)
{
    char *pathp;
    const char *clientPrefix = "";
    const char *serverPrefix = "";

#ifdef AFS_NT40_ENV
    char *buf;
    int status;

    /* get the afs server software installation dir from the registry */
    if (afssw_GetServerInstallDir(&buf)) {
	/* failed; server not installed; use temp directory */
	strcpy(ntServerInstallDirLong, gettmpdir());
    } else {
	strcpy(ntServerInstallDirLong, buf);
	free(buf);
	initStatus |= AFSDIR_SERVER_PATHS_OK;
    }
    FilepathNormalize(ntServerInstallDirLong);
    status =
	GetShortPathName(ntServerInstallDirLong, ntServerInstallDirShort,
			 AFSDIR_PATH_MAX);
    if (status == 0 || status > AFSDIR_PATH_MAX) {
	/* can't convert path to short version; just use long version */
	strcpy(ntServerInstallDirShort, ntServerInstallDirLong);
    }
    FilepathNormalize(ntServerInstallDirShort);

    /* get the afs client configuration directory (/usr/vice/etc equivalent) */
    if (afssw_GetClientInstallDir(&buf)) {
        /* failed */
        status = GetWindowsDirectory(ntClientConfigDirLong, AFSDIR_PATH_MAX);
        if (status == 0 || status > AFSDIR_PATH_MAX) {
            /* failed to get canonical Windows directory; use temp directory */
            strcpy(ntClientConfigDirLong, gettmpdir());
        } else {
            initStatus |= AFSDIR_CLIENT_PATHS_OK;
        }
    } else {
        strcpy(ntClientConfigDirLong, buf);
        free(buf);
        initStatus |= AFSDIR_CLIENT_PATHS_OK;
    }
    FilepathNormalize(ntClientConfigDirLong);

    status =
	GetShortPathName(ntClientConfigDirLong, ntClientConfigDirShort,
			 AFSDIR_PATH_MAX);
    if (status == 0 || status > AFSDIR_PATH_MAX) {
	/* can't convert path to short version; just use long version */
	strcpy(ntClientConfigDirShort, ntClientConfigDirLong);
    }
    FilepathNormalize(ntClientConfigDirShort);
    clientPrefix = ntClientConfigDirShort;

    /* setup the root server directory path (/usr/afs equivalent) */
    strcpy(afsSrvDirPath, ntServerInstallDirShort);
    strcat(afsSrvDirPath, AFSDIR_CANONICAL_SERVER_AFS_DIRPATH);

    /* there is no root client directory path (/usr/vice equivalent) */
    afsClntDirPath[0] = '\0';

    /* setup top level dirpath (/usr equivalent); valid for server ONLY */
    strcpy(dirPathArray[AFSDIR_USR_DIRPATH_ID], ntServerInstallDirShort);
    serverPrefix = ntServerInstallDirShort;
    strcat(dirPathArray[AFSDIR_USR_DIRPATH_ID], AFSDIR_CANONICAL_USR_DIRPATH);

#else /* AFS_NT40_ENV */
    /* setup the root server directory path */
    strcpy(afsSrvDirPath, AFSDIR_CANONICAL_SERVER_AFS_DIRPATH);

    /* setup the root client directory path */
#ifdef AFS_DARWIN_ENV
    if (access(AFSDIR_ALTERNATE_CLIENT_VICE_DIRPATH, F_OK) == 0)
	strcpy(afsClntDirPath, AFSDIR_ALTERNATE_CLIENT_VICE_DIRPATH);
    else
#endif
	strcpy(afsClntDirPath, AFSDIR_CANONICAL_CLIENT_VICE_DIRPATH);

    /* setup top level dirpath; valid for both server and client */
    strcpy(dirPathArray[AFSDIR_USR_DIRPATH_ID], AFSDIR_CANONICAL_USR_DIRPATH);

    initStatus |= (AFSDIR_CLIENT_PATHS_OK | AFSDIR_SERVER_PATHS_OK);
#endif /* AFS_NT40_ENV */

    /* now initialize various dir and file paths exported by dirpath module */

    /* server dir paths */
    strcpy(dirPathArray[AFSDIR_SERVER_AFS_DIRPATH_ID], afsSrvDirPath);

    pathp = dirPathArray[AFSDIR_SERVER_ETC_DIRPATH_ID];
    AFSDIR_SERVER_DIRPATH(pathp, AFSDIR_SERVER_ETC_DIR);

    pathp = dirPathArray[AFSDIR_SERVER_BIN_DIRPATH_ID];
    AFSDIR_SERVER_DIRPATH(pathp, AFSDIR_SERVER_BIN_DIR);

    pathp = dirPathArray[AFSDIR_SERVER_CORES_DIRPATH_ID];
    AFSDIR_SERVER_DIRPATH(pathp, AFSDIR_CORES_DIR);

    pathp = dirPathArray[AFSDIR_SERVER_DB_DIRPATH_ID];
    AFSDIR_SERVER_DIRPATH(pathp, AFSDIR_DB_DIR);

    pathp = dirPathArray[AFSDIR_SERVER_LOGS_DIRPATH_ID];
    AFSDIR_SERVER_DIRPATH(pathp, AFSDIR_LOGS_DIR);

    pathp = dirPathArray[AFSDIR_SERVER_LOCAL_DIRPATH_ID];
    AFSDIR_SERVER_DIRPATH(pathp, AFSDIR_LOCAL_DIR);

    pathp = dirPathArray[AFSDIR_SERVER_BACKUP_DIRPATH_ID];
    AFSDIR_SERVER_DIRPATH(pathp, AFSDIR_BACKUP_DIR);

    pathp = dirPathArray[AFSDIR_SERVER_MIGRATE_DIRPATH_ID];
    AFSDIR_SERVER_DIRPATH(pathp, AFSDIR_MIGR_DIR);

    pathp = dirPathArray[AFSDIR_SERVER_BIN_FILE_DIRPATH_ID];
    AFSDIR_SERVER_DIRPATH(pathp, AFSDIR_BIN_FILE_DIR);

    /* client dir path */
#ifdef AFS_NT40_ENV
    strcpy(dirPathArray[AFSDIR_CLIENT_VICE_DIRPATH_ID],
	   "/NoUsrViceDirectoryOnWindows");
    strcpy(dirPathArray[AFSDIR_CLIENT_ETC_DIRPATH_ID],
	   ntClientConfigDirShort);
#else
    strcpy(dirPathArray[AFSDIR_CLIENT_VICE_DIRPATH_ID], afsClntDirPath);

    pathp = dirPathArray[AFSDIR_CLIENT_ETC_DIRPATH_ID];
#ifdef AFS_DARWIN_ENV
    if (access(AFSDIR_ALTERNATE_CLIENT_ETC_DIR, F_OK) == 0)
	AFSDIR_CLIENT_DIRPATH(pathp, AFSDIR_ALTERNATE_CLIENT_ETC_DIR);
    else
#endif
	AFSDIR_CLIENT_DIRPATH(pathp, AFSDIR_CLIENT_ETC_DIR);
#endif /* AFS_NT40_ENV */

    /* server file paths */
    pathp = dirPathArray[AFSDIR_SERVER_THISCELL_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_SERVER_ETC_DIR,
			   AFSDIR_THISCELL_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_CELLSERVDB_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_SERVER_ETC_DIR,
			   AFSDIR_CELLSERVDB_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_NOAUTH_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOCAL_DIR, AFSDIR_NOAUTH_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_BUDBLOG_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOGS_DIR, AFSDIR_BUDBLOG_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_TAPECONFIG_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_BACKUP_DIR, AFSDIR_TAPECONFIG_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_KALOGDB_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOGS_DIR, AFSDIR_KALOGDB_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_KALOG_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOGS_DIR, AFSDIR_KALOG_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_KADB_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_DB_DIR, AFSDIR_KADB_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_NTPD_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_SERVER_BIN_DIR, AFSDIR_NTPD_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_PRDB_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_DB_DIR, AFSDIR_PRDB_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_PTLOG_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOGS_DIR, AFSDIR_PTLOG_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_KCONF_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_SERVER_ETC_DIR, AFSDIR_KCONF_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_VLDB_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_DB_DIR, AFSDIR_VLDB_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_VLOG_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOGS_DIR, AFSDIR_VLOG_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_CORELOG_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOGS_DIR, AFSDIR_CORE_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_SLVGLOG_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOGS_DIR, AFSDIR_SLVGLOG_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_SALVAGER_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_SERVER_BIN_DIR,
			   AFSDIR_SALVAGER_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_SLVGLOCK_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOCAL_DIR, AFSDIR_SLVGLOCK_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_KEY_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_SERVER_ETC_DIR, AFSDIR_KEY_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_ULIST_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_SERVER_ETC_DIR, AFSDIR_ULIST_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_BOZCONF_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_BOSCONFIG_DIR, AFSDIR_BOZCONF_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_BOZCONFNEW_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_BOSCONFIG_DIR,
			   AFSDIR_BOZCONFNEW_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_BOZLOG_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOGS_DIR, AFSDIR_BOZLOG_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_BOZINIT_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_BOSCONFIG_DIR, AFSDIR_BOZINIT_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_BOSVR_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_BOSSERVER_DIR, AFSDIR_BOSVR_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_VOLSERLOG_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOGS_DIR, AFSDIR_VOLSERLOG_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_ROOTVOL_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_SERVER_ETC_DIR, AFSDIR_ROOTVOL_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_HOSTDUMP_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOCAL_DIR, AFSDIR_HOSTDUMP_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_CLNTDUMP_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOCAL_DIR, AFSDIR_CLNTDUMP_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_CBKDUMP_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOCAL_DIR, AFSDIR_CBKDUMP_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_OLDSYSID_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOCAL_DIR, AFSDIR_OLDSYSID_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_SYSID_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOCAL_DIR, AFSDIR_SYSID_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_FILELOG_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOGS_DIR, AFSDIR_FILELOG_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_AUDIT_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOCAL_DIR, AFSDIR_AUDIT_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_NETINFO_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOCAL_DIR, AFSDIR_NETINFO_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_NETRESTRICT_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_LOCAL_DIR, AFSDIR_NETRESTRICT_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_WEIGHTING_CONSTANTS_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_MIGR_DIR,
			   AFSDIR_WEIGHTINGCONST_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_THRESHOLD_CONSTANTS_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_MIGR_DIR,
			   AFSDIR_THRESHOLDCONST_FILE);

    pathp = dirPathArray[AFSDIR_SERVER_MIGRATELOG_FILEPATH_ID];
    AFSDIR_SERVER_FILEPATH(pathp, AFSDIR_MIGR_DIR, AFSDIR_MIGRATE_LOGNAME);


    /* client file paths */
#ifdef AFS_NT40_ENV
    strcpy(dirPathArray[AFSDIR_CLIENT_THISCELL_FILEPATH_ID],
	   "/NoUsrViceEtcThisCellFileOnWindows");
    sprintf(dirPathArray[AFSDIR_CLIENT_CELLSERVDB_FILEPATH_ID], "%s/%s",
	    ntClientConfigDirShort, AFSDIR_CELLSERVDB_FILE_NTCLIENT);
    strcpy(dirPathArray[AFSDIR_CLIENT_CELLALIAS_FILEPATH_ID],
	   "/NoCellAliasOnWindows");
#else
    pathp = dirPathArray[AFSDIR_CLIENT_THISCELL_FILEPATH_ID];
    AFSDIR_CLIENT_FILEPATH(pathp, AFSDIR_CLIENT_ETC_DIR,
			   AFSDIR_THISCELL_FILE);

    pathp = dirPathArray[AFSDIR_CLIENT_CELLSERVDB_FILEPATH_ID];
    AFSDIR_CLIENT_FILEPATH(pathp, AFSDIR_CLIENT_ETC_DIR,
			   AFSDIR_CELLSERVDB_FILE);

    pathp = dirPathArray[AFSDIR_CLIENT_CELLALIAS_FILEPATH_ID];
    AFSDIR_CLIENT_FILEPATH(pathp, AFSDIR_CLIENT_ETC_DIR,
			   AFSDIR_CELLALIAS_FILE);
#endif /* AFS_NT40_ENV */

    pathp = dirPathArray[AFSDIR_CLIENT_NETINFO_FILEPATH_ID];
    AFSDIR_CLIENT_FILEPATH(pathp, AFSDIR_CLIENT_ETC_DIR, AFSDIR_NETINFO_FILE);

    pathp = dirPathArray[AFSDIR_CLIENT_NETRESTRICT_FILEPATH_ID];
    AFSDIR_CLIENT_FILEPATH(pathp, AFSDIR_CLIENT_ETC_DIR,
			   AFSDIR_NETRESTRICT_FILE);

    initFlag = 1;		/* finished dirpath initialization */
    return;
}

/* getDirPath - returns a const char pointer to the requested string
 * from the internal path array.
 * string_id - index into the path array 
 */
const char *
getDirPath(afsdir_id_t string_id)
{
    /* check if the array has been initialized */
    if (initFlag == 0) {	/* no it's not, so initialize */
#ifdef AFS_PTHREAD_ENV
	pthread_once(&dirInit_once, initDirPathArray);
#else
	initDirPathArray();
#endif
    }
    return (const char *)dirPathArray[string_id];
}

/*
 * LocalizePathHead() -- Make path relative to local part
 *
 * ConstructLocalPath takes a path  and a directory that path should
 * be considered relative to.   This  function checks the given path
 * for   a prefix  that represents a canonical path.  If such a prefix
 * is found,  the path is adjusted to remove the prefix and the path
 * is considered  relative to the local version of that path.
 */

/* The following array  maps cannonical parts to local parts.  It
 * might  seem reasonable to  simply construct an array in parallel to
 * dirpatharray  but it turns out you don't want translations for all
 local paths.
*/

struct canonmapping {
    const char *canonical;
    const char *local;
};
static struct canonmapping CanonicalTranslations[] = {
    {AFSDIR_CANONICAL_SERVER_ETC_DIRPATH, AFSDIR_SERVER_ETC_DIR},
    {AFSDIR_CANONICAL_SERVER_LOGS_DIRPATH, AFSDIR_LOGS_DIR},
    {AFSDIR_CANONICAL_SERVER_LOCAL_DIRPATH, AFSDIR_LOCAL_DIR},
    {AFSDIR_CANONICAL_SERVER_BIN_DIRPATH, AFSDIR_SERVER_BIN_DIR},
    {NULL, NULL}
};

static void
LocalizePathHead(const char **path, const char **relativeTo)
{
    struct canonmapping *current;
    for (current = CanonicalTranslations; current->local != NULL; current++) {
	size_t canonlength = strlen(current->canonical);
	if (strncmp(*path, current->canonical, canonlength) == 0) {
	    (*path) += canonlength;
	    if (**path == '/')
		(*path)++;
	    *relativeTo = current->local;
	    return;
	}
    }
}


#ifdef AFS_NT40_ENV
/* NT version of ConstructLocalPath() */

/*
 * ConstructLocalPath() --  Convert a canonical (wire-format) path to a fully
 *     specified local path.  Upon successful completion, *fullPathBufp is
 *     set to an allocated buffer containing the fully specified local path
 *     constructed from the cpath argument.
 *
 *     On NT, path construction proceeds as follows:
 *         1) If cpath is fully qualified (i.e., starts with 'X:/') then the
 *            path returned is equivalent to cpath.
 *         2) If cpath begins with a drive letter but is not fully qualified,
 *            i.e., it is drive relative, then the function fails with EINVAL.
 *         3) If cpath begins with '/' (or '\') then the path returned is the
 *            concatenation  AFS-server-install-dir + cpath after translating for localization.
 *	   4) Otherwise the path returned is the concatenation
 *            AFS-server-install-dir + relativeTo + cpath.
 *
 *     Leading whitespace in cpath is ignored; the constructed path is
 *     normalized (FilepathNormalize()).
 *
 * RETURN CODES: 0 if successful; errno code otherwise.
 */
int
ConstructLocalPath(const char *cpath, const char *relativeTo,
		   char **fullPathBufp)
{
    int status = 0;
    char *newPath = NULL;

    if (initFlag == 0) {	/* dirpath module not yet initialized */
#ifdef AFS_PTHREAD_ENV
	pthread_once(&dirInit_once, initDirPathArray);
#else
	initDirPathArray();
#endif
    }

    *fullPathBufp = NULL;

    while (isspace(*cpath)) {
	cpath++;
    }

    LocalizePathHead(&cpath, &relativeTo);
    if ((((*cpath >= 'a') && (*cpath <= 'z'))
	 || ((*cpath >= 'A') && (*cpath <= 'Z'))) && (*(cpath + 1) == ':')) {

	/* cpath has a leading drive letter */
	if ((*(cpath + 2) != '/') && (*(cpath + 2) != '\\')) {
	    /* drive letter relative path; this is not allowed */
	    status = EINVAL;
	} else {
	    /* fully qualified path; just make a copy */
	    newPath = (char *)malloc(strlen(cpath) + 1);
	    if (!newPath) {
		status = ENOMEM;
	    } else {
		(void)strcpy(newPath, cpath);
	    }
	}

    } else {
	/* cpath has NO leading drive letter; make relative to install dir */
	size_t pathSize = strlen(ntServerInstallDirShort) + 2;

	if ((*cpath == '/') || (*cpath == '\\')) {
	    /* construct path relative to install directory only */
	    pathSize += strlen(cpath);

	    newPath = (char *)malloc(pathSize);
	    if (!newPath) {
		status = ENOMEM;
	    } else {
		sprintf(newPath, "%s/%s", ntServerInstallDirShort, cpath);
	    }
	} else {
	    /* construct path relative to 'relativeTo' (and install dir) */
	    pathSize += strlen(relativeTo) + 1 + strlen(cpath);

	    newPath = (char *)malloc(pathSize);
	    if (!newPath) {
		status = ENOMEM;
	    } else {
		sprintf(newPath, "%s/%s/%s", ntServerInstallDirShort,
			relativeTo, cpath);
	    }
	}
    }

    if (status == 0) {
	FilepathNormalize(newPath);

	/* return buffer containing fully specified path */
	*fullPathBufp = newPath;
    }

    return status;
}

#else
/* Unix version of ConstructLocalPath() */

/*
 * ConstructLocalPath() --  Convert a canonical (wire-format) path to a fully
 *     specified local path.  Upon successful completion, *fullPathBufp is
 *     set to an allocated buffer containing the fully specified local path
 *     constructed from the cpath argument.
 *
 *     On Unix, path construction proceeds as follows:
 *         1) If cpath begins with '/' then the path returned is equivalent
 *            to cpath.
 *	   2) Otherwise the path returned is the concatenation
 *            relativeTo + cpath.
 *
 *     Leading whitespace in cpath is ignored; the constructed path is
 *     normalized (FilepathNormalize()).
 *
 * RETURN CODES: 0 if successful; errno code otherwise.
 */
int
ConstructLocalPath(const char *cpath, const char *relativeTo,
		   char **fullPathBufp)
{
    int status = 0;
    char *newPath = NULL;

    if (initFlag == 0) {	/* dirpath module not yet initialized */
#ifdef AFS_PTHREAD_ENV
	pthread_once(&dirInit_once, initDirPathArray);
#else
	initDirPathArray();
#endif
    }

    *fullPathBufp = NULL;

    while (isspace(*cpath)) {
	cpath++;
    }

    LocalizePathHead(&cpath, &relativeTo);
    if (*cpath == '/') {
	newPath = (char *)malloc(strlen(cpath) + 1);
	if (!newPath) {
	    status = ENOMEM;
	} else {
	    strcpy(newPath, cpath);
	}
    } else {
	newPath = (char *)malloc(strlen(relativeTo) + 1 + strlen(cpath) + 1);
	if (!newPath) {
	    status = ENOMEM;
	} else {
	    sprintf(newPath, "%s/%s", relativeTo, cpath);
	}
    }

    if (status == 0) {
	FilepathNormalize(newPath);

	/* return buffer containing fully specified path */
	*fullPathBufp = newPath;
    }

    return status;
}
#endif /* AFS_NT40_ENV */


/*
 * ConstructLocalBinPath() -- A convenience wrapper for ConstructLocalPath()
 *     that specifies the canonical AFS server binary directory as the relative
 *     directory.
 */
int
ConstructLocalBinPath(const char *cpath, char **fullPathBufp)
{
    return ConstructLocalPath(cpath, AFSDIR_SERVER_BIN_DIRPATH,
			      fullPathBufp);
}


/*
 * ConstructLocalLogPath() -- A convenience wrapper for ConstructLocalPath()
 *     that specifies the canonical AFS server logs directory as the relative
 *     directory.
 */
int
ConstructLocalLogPath(const char *cpath, char **fullPathBufp)
{
    return ConstructLocalPath(cpath, AFSDIR_SERVER_LOGS_DIRPATH,
			      fullPathBufp);
}
