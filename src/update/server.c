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

#include <afs/procmgmt.h>
#include <roken.h>
#include <afs/opr.h>

#ifdef AFS_NT40_ENV
#include <WINNT/afsevent.h>
#endif

#ifdef	AFS_AIX_ENV
#include <sys/statfs.h>
#endif

#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rxkad.h>
#include <afs/authcon.h>
#include <afs/cellconfig.h>
#include <afs/afsutil.h>
#include <afs/fileutil.h>
#include <afs/com_err.h>

#include "update.h"
#include "global.h"

static int AddObject(char **expPath, char *dir);
static int PathInDirectory(char *dir, char *path);
int update_SendFile(int, struct rx_call *, struct stat *);
int update_SendDirInfo(char *, struct rx_call *, struct stat *,
		       char *origDir);

struct afsconf_dir *cdir;
int nDirs;
char *dirName[MAXENTRIES];
int dirLevel[MAXENTRIES];
char *whoami;

static int Quit(char *);

int rxBind = 0;

#define ADDRSPERSITE 16         /* Same global is in rx/rx_user.c */
afs_uint32 SHostAddrs[ADDRSPERSITE];

/* check whether caller is authorized to manage RX statistics */
int
update_rxstat_userok(struct rx_call *call)
{
    return afsconf_SuperUser(cdir, call, NULL);
}

/*
 * PathInDirectory() -- determine if path is in directory (or is directory)
 * Returns 1 if yes, 0 if no, -1 on error.
 */
static int
PathInDirectory(char *dir, char *path)
{
    int inDir = 0, code;
    size_t dirLen;
    char *dirNorm, *pathNorm;

#ifdef AFS_NT40_ENV
    /* case-insensitive comparison of normalized, same-flavor (short) paths */
    DWORD status;

    dirNorm = malloc(AFSDIR_PATH_MAX);
    if (dirNorm == NULL)
	return -1;
    status = GetShortPathName(dir, dirNorm, AFSDIR_PATH_MAX);
    if (status == 0 || status > AFSDIR_PATH_MAX) {
	/* can't convert path to short version; just use long version */
	free(dirNorm);
	dirNorm = strdup(dir);
	if (dirNorm == NULL)
	    return -1;
    }
    FilepathNormalize(dirNorm);

    pathNorm = malloc(AFSDIR_PATH_MAX);
    if (pathNorm == NULL) {
	code = -1;
	goto out;
    }
    status = GetShortPathName(path, pathNorm, AFSDIR_PATH_MAX);
    if (status == 0 || status > AFSDIR_PATH_MAX) {
	/* can't convert path to short version; just use long version */
	free(pathNorm);
	pathNorm = strdup(path);
	if (pathNorm == NULL) {
	    code = -1;
	    goto out;
	}
    }
    FilepathNormalize(pathNorm);

    dirLen = strlen(dirNorm);

    if (_strnicmp(dirNorm, pathNorm, dirLen) == 0) {
	/* substrings match; path must match dir or be subdirectory */
	if (pathNorm[dirLen] == '\0' || pathNorm[dirLen] == '/') {
	    inDir = 1;
	}
    }
#else
    /* case-sensitive comparison of normalized paths */
    dirNorm = strdup(dir);
    if (dirNorm == NULL)
	return -1;
    FilepathNormalize(dirNorm);

    pathNorm = strdup(path);
    if (pathNorm == NULL) {
	code = -1;
	goto out;
    }
    FilepathNormalize(pathNorm);

    dirLen = strlen(dirNorm);

    if (strncmp(dirNorm, pathNorm, dirLen) == 0) {
	/* substrings match; path must match dir or be subdirectory */
	if (pathNorm[dirLen] == '\0' || pathNorm[dirLen] == '/') {
	    inDir = 1;
	}
    }
#endif /* AFS_NT40_ENV */
    code = 0;
out:
    free(dirNorm);
    free(pathNorm);
    return (code != 0) ? code : inDir;
}

int
AuthOkay(struct rx_call *call, char *name)
{
    int i, r;
    rxkad_level level;
    afs_int32 code;
    int matches;

    /* Must be in 'UserList' to use */
    if (!afsconf_SuperUser(cdir, call, NULL))
	return 0;

    if (rx_SecurityClassOf(rx_ConnectionOf(call)) == RX_SECIDX_KAD) {
	code = rxkad_GetServerInfo(rx_ConnectionOf(call), &level, 0, 0, 0, 0, 0);
	if (code)
	    return 0;
    } else
	level = 0;

    matches = 0;
    for (i = 0; i < nDirs; i++) {
	r = PathInDirectory(dirName[i], name);
	if (r < 0)
	    return 0;
	if (r) {
	    if (dirLevel[i] > level)
		return 0;
	    matches++;
	    /* keep searching in case there's a more restrictive subtree
	     * specified later. */
	}
    }
    if (nDirs && !matches)
	return 0;		/* if dirs spec., name must match */
    return 1;			/* okay or no dirs */
}

int
osi_audit(void)
{
/* this sucks but it works for now.
*/
    return 0;
}

#ifndef AFS_NT40_ENV
#include "AFS_component_version_number.c"
#endif

int
main(int argc, char *argv[])
{
    struct rx_securityClass **securityClasses;
    afs_int32 numClasses;
    struct rx_service *service;
    char hoststr[16];
    afs_uint32 host = htonl(INADDR_ANY);

    int a = 0;
    rxkad_level level;
    rxkad_level newLevel;
    struct afsconf_bsso_info bsso;

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

    memset(&bsso, 0, sizeof(bsso));

    whoami = argv[0];

#ifdef AFS_NT40_ENV
    /* dummy signal call to force afsprocmgmt.dll to load on NT */
    signal(SIGUSR1, SIG_DFL);
#endif

    /* Initialize dirpaths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
#ifdef AFS_NT40_ENV
	ReportErrorEventAlt(AFSEVT_SVR_NO_INSTALL_DIR, 0, argv[0], 0);
#endif
	fprintf(stderr, "%s: Unable to obtain AFS server directory.\n",
		argv[0]);
	exit(2);
    }
    nDirs = 0;
    level = rxkad_clear;

    if (argc == 1)		/* no arguments */
	goto usage;

    /* support help flag */
    if (strcmp("-help", argv[1]) == 0)
	goto usage;
    if (strcmp("help", argv[1]) == 0)
	goto usage;

    for (a = 1; a < argc; a++) {
	if (argv[a][0] == '-') {	/* parse options */
	    if (strcmp(argv[a], "-rxbind") == 0) {
		rxBind = 1;
		continue;
	    } else {
		char arg[256];
		lcstring(arg, argv[a], sizeof(arg));
		newLevel = rxkad_StringToLevel(&argv[a][1]);
		if (newLevel != -1) {
		    level = newLevel;	/* set new level */
		    continue;
		}
	    }
	  usage:
	    Quit("Usage: upserver [<directory>+] [-crypt <directory>+] [-clear <directory>+] [-auth <directory>+] [-rxbind] [-help]\n");
	} else {
	    if (nDirs >= sizeof(dirName) / sizeof(dirName[0]))
		Quit("Too many dirs");
	    if (AddObject(&dirName[nDirs], argv[a])) {
		printf("%s: Unable to export dir %s. Skipping\n", whoami,
		       argv[a]);
		continue;
	    }
	    dirLevel[nDirs] = level;	/* remember current level */
	    nDirs++;
	}
    }

    if (nDirs == 0) {		/* Didn't find any directories to export */
	printf("%s: No directories to export. Quitting\n", whoami);
	exit(1);
    }

    cdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
    if (cdir == 0) {
	fprintf(stderr, "Can't get server configuration info (%s)\n",
		AFSDIR_SERVER_ETC_DIRPATH);
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

    /* Initialize Rx, telling it port number this server will use for its
     * single service */
    fprintf(stderr, "upserver binding rx to %s:%d\n",
            afs_inet_ntoa_r(host, hoststr), AFSCONF_UPDATEPORT);
    if (rx_InitHost(host, htons(AFSCONF_UPDATEPORT)) < 0)
	Quit("rx_init");

    bsso.dir = cdir;
    afsconf_BuildServerSecurityObjects_int(&bsso, &securityClasses, &numClasses);

    if (securityClasses[2] == NULL)
	Quit("rxkad_NewServerSecurityObject");

    /* Instantiate a single UPDATE service.  The rxgen-generated procedure
     * which is called to decode requests is passed in here
     * (UPDATE_ExecuteRequest). */
    service =
	rx_NewServiceHost(host, 0, UPDATE_SERVICEID, "UPDATE", securityClasses,
			  numClasses, UPDATE_ExecuteRequest);
    if (service == (struct rx_service *)0)
	Quit("rx_NewService");
    rx_SetMaxProcs(service, 2);

    /* allow super users to manage RX statistics */
    rx_SetRxStatUserOk(update_rxstat_userok);

    rx_StartServer(1);		/* Donate this process to the server process pool */
    Quit("StartServer returned?");
    return 0;
}

/* fetch the file name and send it to the remote requester specified by call */

int
UPDATE_FetchFile(struct rx_call *call, char *name)
{
    int fd = -1;
    int error = 0;
    struct stat status;
    char *reqObject;

    /* construct a local path from a canonical (wire-format) path */
    if ((error = ConstructLocalPath(name, "/", &reqObject))) {
	afs_com_err(whoami, error, "Unable to construct local path");
	return UPDATE_ERROR;
    }

    if (!AuthOkay(call, reqObject)) {
	error = UPDATE_ERROR;
    } else {
	fd = open(reqObject, O_RDONLY, 0);
	if (fd < 0 || fstat(fd, &status) < 0) {
	    printf("Failed to open %s\n", reqObject);
	    error = UPDATE_ERROR;
	}
	if (!error)
	    error = update_SendFile(fd, call, &status);
	if (fd >= 0)
	    close(fd);
    }
    free(reqObject);
    return error;
}

/* fetch dir info about directory name and send it to remote host associated
  with call. */
int
UPDATE_FetchInfo(struct rx_call *call, char *name)
{
    int error = 0;
    struct stat status;
    char *reqObject;

    /* construct a local path from a canonical (wire-format) path */
    if ((error = ConstructLocalPath(name, "/", &reqObject))) {
	afs_com_err(whoami, error, "Unable to construct local path");
	return UPDATE_ERROR;
    }

    if (!AuthOkay(call, reqObject)) {
	error = UPDATE_ERROR;
    } else {
	/* we only need to stat the obj, not open it. */
	if (stat(reqObject, &status) < 0) {
	    printf("Failed to open %s\n", reqObject);
	    error = UPDATE_ERROR;
	}
	if ((status.st_mode & S_IFMT) != S_IFDIR) {
	    printf(" file %s is not a directory \n", reqObject);
	    error = -1;
	}

	if (!error)
	    error = update_SendDirInfo(reqObject, call, &status, name);
    }
    free(reqObject);
    return error;
}

static int
Quit(char *msg)
{
    fprintf(stderr, "%s", msg);
    exit(1);
}

int
update_SendFile(int fd, struct rx_call *call, struct stat *status)
{
    char *buffer = (char *)0;
    int blockSize;
    afs_int32 length, tlen;
#ifdef	AFS_AIX_ENV
    struct statfs tstatfs;
#endif

    afs_int32 error = 0;
#ifdef	AFS_AIX_ENV
    /* Unfortunately in AIX valuable fields such as st_blksize are gone from the stat structure!! */
    fstatfs(fd, &tstatfs);
    blockSize = tstatfs.f_bsize;
#elif AFS_NT40_ENV
    blockSize = 4096;
#else
    blockSize = status->st_blksize;
#endif
    length = status->st_size;
    buffer = malloc(blockSize);
    if (!buffer) {
	printf("malloc failed\n");
	return UPDATE_ERROR;
    }
    tlen = htonl(length);
    rx_Write(call, (char *)&tlen, sizeof(afs_int32));	/* send length on fetch */
    while (!error && length) {
	int nbytes = (length > blockSize ? blockSize : length);
	nbytes = read(fd, buffer, nbytes);
	if (nbytes <= 0) {
	    fprintf(stderr, "File system read failed\n");
	    break;
	}
	if (rx_Write(call, buffer, nbytes) != nbytes)
	    break;
	length -= nbytes;
    }
    if (buffer)
	free(buffer);
    if (length)
	error = UPDATE_ERROR;
    return error;
}

/* Enumerate dir (name) and write dir entry info into temp file.
 */
int
update_SendDirInfo(char *name,		/* Name of dir to enumerate */
     struct rx_call *call,	/* rx call */
     struct stat *status,	/* stat struct for dir */
     char *origDir)		/* orig name of dir before being localized */
{
    DIR *dirp;
    struct dirent *dp;
    FILE *stream;
    struct stat tstatus;
    char filename[MAXFNSIZE], dirInfoFile[MAXFNSIZE];
    int fd, tfd, errcode, error, err;

    error = 0;
    dirp = opendir(name);
    sprintf(dirInfoFile, "%s/upserver.tmp", gettmpdir());
    stream = fopen(dirInfoFile, "w");
    if (!stream) {
	error = EIO;
    } else {
	while ((dp = readdir(dirp))) {
	    strcpy(filename, name);
	    strcat(filename, "/");
	    strcat(filename, dp->d_name);

	    tfd = open(filename, O_RDONLY, 0);
	    if (tfd < 0 || fstat(tfd, &tstatus) < 0) {
		printf("Failed to open %s\n", name);
		error = UPDATE_ERROR;
		goto fail;
	    }
	    if ((tstatus.st_mode & S_IFMT) != S_IFDIR) {	/* not a directory */
		char dirEntry[MAXFNSIZE];

		strcpy(dirEntry, origDir);
		strcat(dirEntry, "/");
		strcat(dirEntry, dp->d_name);
		err =
		    fprintf(stream, "\"%s\" %u %u %u %u %u %u\n", dirEntry,
			    (unsigned int)tstatus.st_mtime,
			    (unsigned int)tstatus.st_size, tstatus.st_mode,
			    tstatus.st_uid, tstatus.st_gid,
			    (unsigned int)tstatus.st_atime);
		if (err < 0)
		    error = EIO;
	    }
	    err = close(tfd);
	    if (err) {
		printf("could not close file %s \n", filename);
		error = UPDATE_ERROR;
		goto fail;
	    }
	}
    }
  fail:
    if (dirp)
	closedir(dirp);
    if (stream) {
	if (ferror(stream))
	    if (!error)
		error = UPDATE_ERROR;
	fclose(stream);
    }
    if (error == 0) {
	fd = open(dirInfoFile, O_RDONLY, 0);
	if (fd >= 0) {
	    fstat(fd, &tstatus);
	    errcode = update_SendFile(fd, call, &tstatus);
	    if (errcode)
		if (!error)
		    error = UPDATE_ERROR;
	    close(fd);
	}
    }
    unlink(dirInfoFile);
    return error;
}


/* AddObject() - Adds the object to the list of exported objects after
 *     converting to a local path.
 *
 * expPath : points to allocated storage in which the exportable path is
 *           passed back.
 * dir     : dir name passed in for export
 *
 */
static int
AddObject(char **expPath, char *dir)
{
    int error;
    struct stat statbuf;

    /* construct a local path from a canonical (wire-format) path */
    if ((error = ConstructLocalPath(dir, "/", expPath))) {
	afs_com_err(whoami, error, "Unable to construct local path");
	return error;
    }

    /* stat the object */
    error = stat(*expPath, &statbuf);
    if (error) {
	afs_com_err(whoami, error, ";Can't stat object.");
	return error;
    }
    /* now check if the object has an exportable (file/dir)  type */
    if (!(statbuf.st_mode & S_IFDIR)) {
	fprintf(stderr, "%s: Unacceptable object type for %s\n", whoami,
		*expPath);
	return -1;
    }

    return 0;
}
