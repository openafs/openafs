/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2007 Sine Nomine Associates
 */

#include <osi/osi.h>

RCSID
    ("$Header$");

#include <afs/stds.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <winsock2.h>
#include <WINNT/afsevent.h>
#include <sys/utime.h>
#include <afs/dirent.h>
#include <direct.h>
#include <process.h>
#include <io.h>
#include <afs/procmgmt.h>
#else
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <dirent.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <errno.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rxkad.h>
#include <afs/com_err.h>
#include <afs/cellconfig.h>
#include <afs/afsutil.h>
#include <afs/fileutil.h>
#ifdef	AFS_AIX_ENV
#include <sys/statfs.h>
#endif
#include "update.h"
#include "global.h"

char *whoami;
static int verbose;

/* prototypes */
static int GetFileFromUpServer(struct rx_connection *conn, char *filename,
			       short uid, short gid, afs_uint32 mode,
			       afs_int32 atime, afs_int32 mtime);
static int RenameNewFiles(struct filestr *modFiles);
static int PathsAreEquivalent(char *path1, char *path2);

afs_int32
GetServer(char *aname)
{
    register struct hostent *th;
    afs_int32 addr;

    th = gethostbyname(aname);
    if (!th) {
	printf("host %s not found \n", aname);
	exit(1);
    }
    memcpy(&addr, th->h_addr, sizeof(addr));
    return addr;
}


int
osi_audit()
{
/* this sucks but it works for now.
*/
    return 0;
}

#ifndef AFS_NT40_ENV
#include "AFS_component_version_number.c"
#endif

int
main(int argc, char **argv)
{
    struct rx_connection *conn;
    struct rx_call *call;
    struct afsconf_dir *cdir;
    afs_int32 scIndex;
    struct rx_securityClass *sc;

    short uid, gid;
    afs_uint32 u_uid, u_gid;	/*Unsigned long versions of the above */
    struct stat tstat;

    int code = 0;
    afs_uint32 mode;
    int error;
    char hostname[MAXSIZE];
    FILE *stream;
    afs_int32 time, length, atime;
    struct filestr *df;
    afs_int32 errcode;
    int retrytime;
    unsigned int interval;
    afs_int32 host;
    int a, cnt;
    rxkad_level level;

    char dirbuf[MAXSIZE], filename[MAXSIZE];
    struct filestr *dirname, *ModFiles, *okhostfiles;
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

    osi_AssertOK(osi_PkgInit(osi_ProgramType_Utility,
			     osi_NULL));

    whoami = argv[0];
#ifdef AFS_NT40_ENV
    /* dummy signal call to force afsprocmgmt.dll to load on NT */
    signal(SIGUSR1, SIG_DFL);

    /* initialize winsock */
    if (afs_winsockInit() < 0) {
	ReportErrorEventAlt(AFSEVT_SVR_WINSOCK_INIT_FAILED, 0, argv[0], 0);
	fprintf(stderr, "%s: Couldn't initialize winsock.\n", whoami);
	code = 1;
	goto error;
    }
#endif

    /* Initialize dirpaths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
#ifdef AFS_NT40_ENV
	ReportErrorEventAlt(AFSEVT_SVR_NO_INSTALL_DIR, 0, argv[0], 0);
#endif
	fprintf(stderr, "%s: Unable to obtain AFS server directory.\n",
		argv[0]);
	code = 2;
	goto error;
    }
    retrytime = 60;
    dirname = NULL;
    ModFiles = NULL;
    okhostfiles = NULL;

    verbose = 0;
    interval = TIMEOUT;
    level = rxkad_crypt;	/* safest default */
    strcpy(hostname, "");

    /* Note that IsArg only checks as many bytes as specified in the command line arg,
     * so that, for instance, -t still matches -time.
     */
    for (a = 1; a < argc; a++) {
	if (argv[a][0] == '-') {	/* parse options */
	    int arglen = strlen(argv[a]);
	    char arg[256];
	    lcstring(arg, argv[a], sizeof(arg));
#define IsArg(a) (strncmp (arg,a, arglen) == 0)
	    if (IsArg("-time"))
		interval = atol(argv[++a]);
	    else if (IsArg("-crypt"))
		level = rxkad_crypt;
	    else if (IsArg("-clear"))
		level = rxkad_clear;
	    else if (IsArg("-verbose"))
		verbose++;
	    else {
	      usage:
		printf
		    ("Usage: upclient <hostname> [-crypt] [-clear] [-t <retry time>] [-verbose]* <dir>+ [-help]\n");
		code = 1;
		goto error;
	    }
	} else if (strlen(hostname) == 0)
	    strcpy(hostname, argv[a]);
	else {
	    strcpy(filename, argv[a]);
	    FilepathNormalize(filename);
	    AddToList(&dirname, filename);
	}
    }
    if (level == -1)
	goto usage;
    if (strlen(hostname) == 0)
	goto usage;
    host = GetServer(hostname);
    if (interval < retrytime)
	retrytime = interval;
    if (dirname == 0)
	goto usage;

    errcode = rx_Init(0);
    if (errcode) {
	printf("Rx initialize failed \n");
	com_err(whoami, errcode, "calling Rx init");
	code = 1;
	goto error;
    }

    cdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
    if (cdir == 0) {
	fprintf(stderr, "Can't get server configuration info (%s)\n",
		AFSDIR_SERVER_ETC_DIRPATH);
	code = 1;
	goto error;
    }

    if (level == rxkad_crypt)
	errcode = afsconf_ClientAuthSecure(cdir, &sc, &scIndex);
    else if (level == rxkad_clear)
	errcode = afsconf_ClientAuth(cdir, &sc, &scIndex);
    else {
	printf("Unsupported security level %d\n", level);
	code = 1;
	goto error;
    }
    if (errcode) {
	com_err(whoami, errcode, "Couldn't get security obect for localAuth");
	code = 1;
	goto error;
    }

  again:
    conn =
	rx_NewConnection(host, htons(AFSCONF_UPDATEPORT), UPDATE_SERVICEID,
			 sc, scIndex);
    cnt = 0;
    while (1) {			/*keep doing it */
	char c, c1;
	for (df = dirname; df; df = df->next) {	/*for each directory do */
	    char *curDir;

	    if (verbose)
		printf("Checking dir %s\n", df->name);
	    /* initialize lists */
	    ZapList(&ModFiles);
	    ZapList(&okhostfiles);

	    /* construct local path from canonical (wire-format) path */
	    if ((errcode = ConstructLocalPath(df->name, "/", &curDir))) {
		com_err(whoami, errcode, "Unable to construct local path");
		code = errcode;
		goto error;
	    }

	    if (stat(curDir, &tstat) < 0) {
		/* try to make the dir */
#ifdef AFS_NT40_ENV
		if (mkdir(curDir) < 0) {
#else
		if (mkdir(curDir, 0700) < 0) {
#endif
		    com_err(whoami, errno, "can't create dir");
		    printf("upclient: Can't update dir %s\n", curDir);
		    code = -1;
		    goto error;
		}
	    } else {
		if ((tstat.st_mode & S_IFMT) != S_IFDIR) {
		    printf(" file %s is not a directory; aborting\n", curDir);
		    code = -1;
		    goto error;
		}
	    }
	    call = rx_NewCall(conn);

	    /* scratch pad file */
	    sprintf(dirbuf, "%s/upclient.%d", gettmpdir(), getpid());

	    errcode = FetchFile(call, df->name, dirbuf, 1);	/* get the names and relevant info about all the files in the directory df->name into file dirbuf */
	    error = rx_EndCall(call, 0);
	    if (error && !errcode) {
		printf("could not end rx call \n");
		com_err(whoami, error, "calling EndCall");
		goto fail;
	    }
	    if (errcode) {
		printf
		    ("warning: could not fetch the contents of directory %s \n",
		     df->name);
		com_err(whoami, errcode, "calling FetchFile");
		cnt++;
		goto fail;
	    }

	    stream = fopen(dirbuf, "r");
	    if (stream == NULL) {
		printf("fopen failed on %s \n", dirbuf);
		com_err(whoami, errno, "fopen");
		goto fail;
	    }
	    umask(00);

	    /* while there is more info about files in file dirbuf */
	    while (fscanf
		   (stream, "%c%[^\"]%c %u %u %u %u %u %u\n", &c, filename,
		    &c1, &time, &length, &mode, &u_uid, &u_gid,
		    &atime) != EOF) {
		uid = (short)u_uid;
		gid = (short)u_gid;
		AddToList(&okhostfiles, filename);
		/*record all the file names which exist on the remote
		 * sync site, to enable purging of redundant files */
		if (verbose >= 3)
		    printf("    checking %s\n", filename);
		if (!IsCompatible(filename, time, length)) {
		    /* if the file info has changed , record all the 
		     *changed files in the ModFiles array*/
		    if (verbose >= 2)
			printf("  getting %s\n", filename);
		    AddToList(&ModFiles, filename);

		    /* now get the file from the server. The received 
		     * file is created under the name filename.NEW */
		    errcode =
			GetFileFromUpServer(conn, filename, uid, gid, mode,
					    atime, time);
		    if (errcode == 1)	/* this file failed, but keep trying */
			goto fail_dirbuf;
		    if (errcode == -1) {	/* time to quit */
			fclose(stream);
			unlink(dirbuf);
			code = -1;
			goto error;
		    }
		}

	    }
	    fclose(stream);
	    unlink(dirbuf);

	    {			/*delete all the redundant files on the client */
		DIR *dirp;
		struct dirent *dp;
		char filename[MAXSIZE];

		dirp = opendir(curDir);
		if (dirp == 0) {
		    com_err(whoami, errno, "Can't open local dir %s", curDir);
		    goto fail;
		}

		while ((dp = readdir(dirp))) {
		    /* for all the files in the directory df->name do */
		    strcpy(filename, curDir);
		    strcat(filename, "/");
		    strcat(filename, dp->d_name);
		    /* if the file filename is redundant, delete it */
		    errcode = NotOnHost(filename, okhostfiles);
		    if (errcode == -1) {
			code = -1;
			goto error;
		    }
		    if (errcode == 1) {
			if (verbose >= 2)
			    printf("  flushing %s\n", filename);
			errcode = unlink(filename);
			if (errcode) {
			    printf("could not delete file %s \n", filename);
			    com_err(whoami, errno, "could not delete file %s",
				    filename);
			}
		    }
		}
		closedir(dirp);
	    }
	    /* Now, rename the .NEW files created by FetchFile */
	    if (RenameNewFiles(ModFiles)) {
		code = -1;
		goto error;
	    }

	    free(curDir);
	}			/* end for each dir loop */
	/*delete the file with info on files in directory df->name */
	IOMGR_Sleep(interval);
	continue;

      fail_dirbuf:
	fclose(stream);
	unlink(dirbuf);
      fail:
	IOMGR_Sleep(retrytime);
	if (cnt > 10) {
	    rx_DestroyConnection(conn);
	    goto again;
	}
	/* start the cycle again */

    }

 error:
    osi_AssertOK(osi_PkgShutdown());
    return code;
}

/* returns 1 if the file is upto date else returns 0*/
/*check the dir case more carefully */
int
IsCompatible(char *filename, afs_int32 time, afs_int32 length)	
{
    struct stat status;
    afs_int32 error;
    char *localname;

    /* construct a local path from canonical (wire-format) path */
    if ((error = ConstructLocalPath(filename, "/", &localname))) {
	com_err(whoami, error, "Unable to construct local path");
	return error;
    }

    error = stat(localname, &status);

    free(localname);

    if (error == -1)
	return 0;	/*a non-existent file, has to be fetched fresh */
    if ((status.st_mode & S_IFMT) == S_IFDIR
	|| ((status.st_mtime == time) && (status.st_size == length)))
	return (1);
    else
	return 0;
}

int
FetchFile(struct rx_call *call, char *remoteFile, char *localFile, int dirFlag)
{
    int fd = -1, error = 0;
    struct stat status;

    if (dirFlag) {
	if (StartUPDATE_FetchInfo(call, remoteFile))
	    return UPDATE_ERROR;
    } else {
	if (StartUPDATE_FetchFile(call, remoteFile))
	    return UPDATE_ERROR;
    }
    fd = open(localFile, O_CREAT | O_TRUNC | O_WRONLY, 0666);
    if (fd < 0) {
	printf("Could not create %s\n", localFile);
	com_err(whoami, errno, "Could not create %s", localFile);
	error = UPDATE_ERROR;
	return error;
    }
    if (fstat(fd, &status) < 0) {
	com_err(whoami, errno, "Could not stat %s", localFile);
	close(fd);
	printf("could not stast %s\n", localFile);
	return UPDATE_ERROR;
    }
    if (update_ReceiveFile(fd, call, &status))
	error = UPDATE_ERROR;

    close(fd);

    return error;
}



int
update_ReceiveFile(register int fd, register struct rx_call *call, register struct stat *status)
{
    register char *buffer = (char *)0;
    afs_int32 length;
    register int blockSize;
    afs_int32 error = 0, len;
#ifdef	AFS_AIX_ENV
    struct statfs tstatfs;
#endif

    len = rx_Read(call, &length, sizeof(afs_int32));
    length = ntohl(length);
    if (len != sizeof(afs_int32))
	return UPDATE_ERROR;
#ifdef	AFS_AIX_ENV
    /* Unfortunately in AIX valuable fields such as st_blksize are gone from the stat structure!! */
    fstatfs(fd, &tstatfs);
    blockSize = tstatfs.f_bsize;
#elif AFS_NT40_ENV
    blockSize = 4096;
#else
    blockSize = status->st_blksize;
#endif
    buffer = (char *)malloc(blockSize);
    if (!buffer) {
	printf("malloc failed\n");
	return UPDATE_ERROR;
    }
    while (!error && length) {
	register int nbytes = (length > blockSize ? blockSize : length);
	nbytes = rx_Read(call, buffer, nbytes);
	if (!nbytes)
	    error = UPDATE_ERROR;
	if (write(fd, buffer, nbytes) != nbytes) {
	    com_err(whoami, errno, "File system write failed!");
	    printf("File system write failed!\n");
	    error = UPDATE_ERROR;
	}
	length -= nbytes;
    }
    if (buffer)
	free(buffer);
    if (!error)
	fstat(fd, status);
    return error;
}


/*
 * PathsAreEquivalent() -- determine if paths are equivalent
 */
static int
PathsAreEquivalent(char *path1, char *path2)
{
    int areEq = 0;
    char pathNorm1[AFSDIR_PATH_MAX], pathNorm2[AFSDIR_PATH_MAX];

#ifdef AFS_NT40_ENV
    /* case-insensitive comparison of normalized, same-flavor (short) paths */
    DWORD status;

    status = GetShortPathName(path1, pathNorm1, AFSDIR_PATH_MAX);
    if (status == 0 || status > AFSDIR_PATH_MAX) {
	/* can't convert path to short version; just use long version */
	strcpy(pathNorm1, path1);
    }
    FilepathNormalize(pathNorm1);

    status = GetShortPathName(path2, pathNorm2, AFSDIR_PATH_MAX);
    if (status == 0 || status > AFSDIR_PATH_MAX) {
	/* can't convert path to short version; just use long version */
	strcpy(pathNorm2, path2);
    }
    FilepathNormalize(pathNorm2);

    if (_stricmp(pathNorm1, pathNorm2) == 0) {
	areEq = 1;
    }
#else
    /* case-sensitive comparison of normalized paths */
    strcpy(pathNorm1, path1);
    FilepathNormalize(pathNorm1);

    strcpy(pathNorm2, path2);
    FilepathNormalize(pathNorm2);

    if (strcmp(pathNorm1, pathNorm2) == 0) {
	areEq = 1;
    }
#endif /* AFS_NT40_ENV */
    return areEq;
}



/* returns 1 if filename does not exist on the host site (=> it should be
 * deleted on client site) else it returns 0 */

int
NotOnHost(char *filename, struct filestr *okhostfiles)
{
    int i, rc;
    struct stat status;
    struct filestr *tf;
    char *hostfile;

    stat(filename, &status);

    if ((status.st_mode & S_IFMT) == S_IFDIR)
	return 0;
    i = strlen(filename);
    if (!strcmp(&filename[i - 4], ".NEW"))
	return 0;

    for (tf = okhostfiles; tf; tf = tf->next) {
	/* construct local path from canonical (wire-format) path */
	if ((rc = ConstructLocalPath(tf->name, "/", &hostfile))) {
	    com_err(whoami, rc, "Unable to construct local path");
	    return -1;
	}
	if (PathsAreEquivalent(hostfile, filename)) {
	    free(hostfile);
	    return 0;
	}
	free(hostfile);
    }
    return 1;
}


/* RenameNewFiles() - renames all the newly copied files from the
 * server. Looks for files with .NEW extension and renames them
 */
static int
RenameNewFiles(struct filestr *modFiles)
{
    char newname[MAXSIZE];
    char *fname;
    int errcode = 0;
    struct filestr *tf;

    for (tf = modFiles; tf; tf = tf->next) {
	/* construct local path from canonical (wire-format) path */
	if ((errcode = ConstructLocalPath(tf->name, "/", &fname))) {
	    com_err(whoami, errcode, "Unable to construct local path");
	    return errcode;
	}
	strcpy(newname, fname);
	strcat(newname, ".NEW");
	if (verbose >= 2)
	    printf("  renaming %s\n", newname);
	errcode = renamefile(newname, fname);
	if (errcode) {
	    printf("could not rename %s to %s\n", newname, fname);
	    com_err(whoami, errno, "could not rename %s to %s", newname,
		    fname);
	}
	free(fname);
    }
    return errcode;
}



/* GetFileFromUpServer() - Makes the FetchFile() call and gets the
 * file from the upserver. 
 * Return Values:
 *   0 -  Alls well
 *   -1 - Serious error. Quit right away.
 *   1  - Error, but keep trying for the other files 
 * 
 * The file obtained is written to the localized version of the filename.NEW
 * and the uid, gid, file mode, access and modification times will be set to
 * the passed in values.
 */
static int
GetFileFromUpServer(struct rx_connection *conn,	/* handle for upserver */
		    char *filename,	/* name of file to be fetched */
		    short uid, short gid,	/* uid/gid for fetched file */
		    afs_uint32 mode,	/* file mode */
		    afs_int32 atime, afs_int32 mtime)
{				/* access/modification times */
    struct rx_call *call;
    afs_int32 errcode;
    char *lfile;
#ifdef AFS_NT40_ENV
    struct _utimbuf utbuf;
#else
    struct timeval tvp[2];
#endif
    char newfile[MAXSIZE];

    /* construct local path from canonical (wire-format) path */
    errcode = ConstructLocalPath(filename, "/", &lfile);
    if (errcode) {
	com_err(whoami, errcode, "Unable to construct local path");
	return -1;
    }
    strcpy(newfile, lfile);
    free(lfile);

    strcat(newfile, ".NEW");

    /* fetch filename into newfile from the host, since the current file
     * is outdated. the new versions of changed files is stored as
     * oldname.new */
    call = rx_NewCall(conn);
    errcode = FetchFile(call, filename, newfile, 0);
    errcode = rx_EndCall(call, errcode);

    if (errcode) {
	printf("failed to fetch file %s \n", filename);
	com_err(whoami, errcode, "fetching file");
	return 1;
    }

    /* now set the rest of the file status */
    errcode = chmod(newfile, mode);
    if (errcode) {
	printf("could not change protection on %s to %u\n", newfile,
	       (unsigned int)mode);
	com_err(whoami, errno, "could not change protection on %s to %u",
		newfile, mode);
	return 1;
    }
#ifdef AFS_NT40_ENV
    utbuf.actime = atime;
    utbuf.modtime = mtime;
    errcode = _utime(newfile, &utbuf);
#else
    errcode = chown(newfile, uid, gid);
    if (errcode) {
	printf("warning: could not change uid and gid on %s to %u and %u \n",
	       newfile, gid, uid);
	com_err(whoami, errno,
		"warning: could not change uid and gid on %s to %u and %u",
		newfile, gid, uid);
    }
    tvp[0].tv_sec = atime;
    tvp[0].tv_usec = 0;
    tvp[1].tv_sec = mtime;
    tvp[1].tv_usec = 0;
    errcode = utimes(newfile, tvp);
#endif /* NT40 */
    if (errcode) {
	printf("could not change access and modify times on %s to %u %u\n",
	       newfile, (unsigned int)atime, (unsigned int)mtime);
	com_err(whoami, errno,
		"could not change access and modify times on %s to %u %u",
		newfile, atime, mtime);
	return 1;
    }

    return 0;
}
