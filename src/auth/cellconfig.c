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
#include <afs/pthread_glock.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <sys/utime.h>
#include <io.h>
#include <WINNT/afssw.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/file.h>
#include <sys/time.h>
#include <ctype.h>
#include <arpa/nameser.h>
#ifdef HAVE_ARPA_NAMESER_COMPAT_H
#include <arpa/nameser_compat.h>
#endif
#include <resolv.h>
#endif /* AFS_NT40_ENV */
#include <afs/afsint.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <rx/rxkad.h>
#include <rx/rx.h>

#include <afs/afsutil.h>
#include "cellconfig.h"
#include "keys.h"
#ifdef AFS_NT40_ENV
#include <cm.h>
#include <cm_config.h>
/* cm_dns.h depends on cellconfig.h */
#include <cm_nls.h>
#include <cm_dns.h>
#endif
#include <rx/rx.h>
#include <rx/rxkad.h>

struct afsconf_servPair {
    const char *name;
    const char *ianaName;
    int port;
};

static struct afsconf_servPair serviceTable[] = {
    {"afs", "afs3-fileserver", 7000,},
    {"afscb", "afs3-callback", 7001,},
    {"afsprot", "afs3-prserver", 7002,},
    {"afsvldb", "afs3-vlserver", 7003,},
    {"afskauth", "afs3-kaserver", 7004,},
    {"afsvol", "afs3-volserver", 7005,},
    {"afserror", "afs3-errors", 7006,},
    {"afsnanny", "afs3-bos", 7007,},
    {"afsupdate", "afs3-update", 7008,},
    {"afsrmtsys", "afs3-rmtsys", 7009,},
    {"afsres", NULL, 7010,},/* residency database for MR-AFS */
    {"afsremio", NULL, 7011,},	/* remote I/O interface for MR-AFS */
    {0, 0, 0}			/* insert new services before this spot */
};

/* Prototypes */
static int TrimLine(char *abuffer, int abufsize);
static int IsClientConfigDirectory(const char *path);
#ifdef AFS_NT40_ENV
static int GetCellNT(struct afsconf_dir *adir);
#endif
static int afsconf_Check(struct afsconf_dir *adir);
static int afsconf_Touch(struct afsconf_dir *adir);
static int GetCellUnix(struct afsconf_dir *adir);
static int afsconf_OpenInternal(struct afsconf_dir *adir, char *cell,
				char clones[]);
static int ParseHostLine(char *aline, struct sockaddr_in *addr,
			 char *aname, char *aclone);
static int ParseCellLine(char *aline, char *aname,
			 char *alname);
static int afsconf_CloseInternal(struct afsconf_dir *adir);
static int afsconf_Reopen(struct afsconf_dir *adir);
static int SaveKeys(struct afsconf_dir *adir);

#ifndef T_AFSDB
#define T_AFSDB 18		/* per RFC1183 section 1 */
#endif
#ifndef T_SRV
#define T_SRV 33		/* RFC2782 */
#endif

/*
 * Basic Rule: we touch "<AFSCONF_DIR>/CellServDB" every time we change anything, so
 * our code can tell if there is new info in the key files, the cell server db
 * files or any of the other files (and reopen the thing) if the date on
 * CellServDB changes.
 */

#if defined(AFS_SUN5_ENV) && !defined(__sparcv9)
/* Solaris through 10 in 32 bit mode will return EMFILE if fopen can't
   get an fd <= 255. We allow the fileserver to claim more fds than that.
   This has always been a problem since pr_Initialize would have the same
   issue, but hpr_Initialize makes it more likely that we would see this.
   Work around it. This is not generic. It's coded with the needs of
   afsconf_* in mind only.

   http://www.opensolaris.org/os/community/onnv/flag-days/pages/2006042001/
*/

#define BUFFER 4096

struct afsconf_iobuffer {
    int _file;
    char *buffer;
    char *ptr;
    char *endptr;
};

typedef struct afsconf_iobuffer afsconf_FILE;

static afsconf_FILE *
afsconf_fopen(const char *fname, const char *fmode)
{
    int fd;
    afsconf_FILE *iop;

    if ((fd = open(fname, O_RDONLY)) == -1) {
	return NULL;
    }

    iop = malloc(sizeof(struct afsconf_iobuffer));
    if (iop == NULL) {
	(void) close(fd);
	errno = ENOMEM;
	return NULL;
    }
    iop->_file = fd;
    iop->buffer = malloc(BUFFER);
    if (iop->buffer == NULL) {
	(void) close(fd);
	free((void *) iop);
	errno = ENOMEM;
	return NULL;
    }
    iop->ptr = iop->buffer;
    iop->endptr = iop->buffer;
    return iop;
}

static int
afsconf_fclose(afsconf_FILE *iop)
{
    if (iop == NULL) {
	return 0;
    }
    close(iop->_file);
    free((void *)iop->buffer);
    free((void *)iop);
    return 0;
}

static char *
afsconf_fgets(char *s, int n, afsconf_FILE *iop)
{
    char *p;

    p = s;
    for (;;) {
	char c;

	if (iop->ptr == iop->endptr) {
	    ssize_t len;

	    if ((len = read(iop->_file, (void *)iop->buffer, BUFFER)) == -1) {
		return NULL;
	    }
	    if (len == 0) {
		*p = 0;
		if (s == p) {
		    return NULL;
		}
		return s;
	    }
	    iop->ptr = iop->buffer;
	    iop->endptr = iop->buffer + len;
	}
	c = *iop->ptr++;
	*p++ = c;
	if ((p - s) == (n - 1)) {
	    *p = 0;
	    return s;
	}
	if (c == '\n') {
	    *p = 0;
	    return s;
	}
    }
}
#define fopen afsconf_fopen
#define fclose afsconf_fclose
#define fgets afsconf_fgets
#else
#define afsconf_FILE FILE
#endif /* AFS_SUN5_ENV && ! __sparcv9 */

/* return port number in network byte order in the low 16 bits of a long; return -1 if not found */
afs_int32
afsconf_FindService(const char *aname)
{
    /* lookup a service name */
    struct servent *ts;
    struct afsconf_servPair *tsp;

    if (aname == NULL || aname[0] == '\0')
	return -1;

#if     defined(AFS_OSF_ENV)
    ts = getservbyname(aname, "");
#else
    ts = (struct servent *) getservbyname(aname, NULL);
#endif
    if (ts) {
	/* we found it in /etc/services, so we use this value */
	return ts->s_port;	/* already in network byte order */
    }

    /* not found in /etc/services, see if it is one of ours */
    for (tsp = serviceTable; tsp->port; tsp++) {
	if ((tsp->name && (!strcmp(tsp->name, aname)))
	    || (tsp->ianaName && (!strcmp(tsp->ianaName, aname))))
	    return htons(tsp->port);
    }
    return -1;
}

const char *
afsconf_FindIANAName(const char *aname)
{
    /* lookup a service name */
    struct afsconf_servPair *tsp;

    if (aname == NULL || aname[0] == '\0')
        return NULL;

    /* see if it is one of ours */
    for (tsp = serviceTable; tsp->port; tsp++) {
	if ((tsp->name && (!strcmp(tsp->name, aname)))
	    || (tsp->ianaName && (!strcmp(tsp->ianaName, aname))))
	    return tsp->ianaName;
    }
    return NULL;
}

static int
TrimLine(char *abuffer, int abufsize)
{
    char tbuffer[256];
    char *tp;
    int tc;

    tp = abuffer;
    while ((tc = *tp)) {
	if (!isspace(tc))
	    break;
	tp++;
    }
    strlcpy(tbuffer, tp, sizeof tbuffer);
    strlcpy(abuffer, tbuffer, abufsize);
    return 0;
}

/*
 * IsClientConfigDirectory() -- determine if path matches well-known
 *     client configuration directory.
 */
#ifdef AFS_NT40_ENV
#define IS_SEP(x) ((x) == '\\' || (x) == '/')
#else /* AFS_NT40_ENV */
#define IS_SEP(x) ((x) == '/')
#endif /* AFS_NT40_ENV */
static int
IsClientConfigDirectory(const char *path)
{
    const char *cdir = AFSDIR_CLIENT_ETC_DIRPATH;
    int i, cc, pc;

    for (i = 0; cdir[i] != '\0' && path[i] != '\0'; i++) {
#ifdef AFS_NT40_ENV
	cc = tolower(cdir[i]);
	pc = tolower(path[i]);

	if (cc == '\\') {
	    cc = '/';
	}
	if (pc == '\\') {
	    pc = '/';
	}
#else /* AFS_NT40_ENV */
	cc = cdir[i];
	pc = path[i];
#endif /* AFS_NT40_ENV */
        if (cc != pc) {
	    return 0;
	}
    }

    /* hit end of one or both; allow mismatch in existence of trailing slash */
    if (cdir[i] != '\0') {
	if (!IS_SEP(cdir[i]) || (cdir[i + 1] != '\0')) {
	    return 0;
	}
    }
    if (path[i] != '\0') {
	if (!IS_SEP(path[i]) || (path[i + 1] != '\0')) {
	    return 0;
	}
    }
    return 1;
}


static int
afsconf_Check(struct afsconf_dir *adir)
{
    char tbuffer[256];
#ifdef AFS_NT40_ENV
    char *p;
#endif
    struct stat tstat;
    afs_int32 code;
    time_t now = time(0);

    if (adir->timeRead && (adir->timeCheck == now)) {
	return 0; /* stat no more than once a second */
    }
    adir->timeCheck = now;

#ifdef AFS_NT40_ENV
    /* NT client CellServDB has different file name than NT server or Unix */
    if (IsClientConfigDirectory(adir->name)) {
	if (!afssw_GetClientCellServDBDir(&p)) {
	    strcompose(tbuffer, sizeof(tbuffer), p, "/",
		       AFSDIR_CELLSERVDB_FILE_NTCLIENT, NULL);
	    free(p);
	} else {
	    int len;
	    strncpy(tbuffer, adir->name, sizeof(tbuffer));
	    len = (int)strlen(tbuffer);
	    if (tbuffer[len - 1] != '\\' && tbuffer[len - 1] != '/') {
		strncat(tbuffer, "\\", sizeof(tbuffer));
	    }
	    strncat(tbuffer, AFSDIR_CELLSERVDB_FILE_NTCLIENT,
		    sizeof(tbuffer));
	    tbuffer[sizeof(tbuffer) - 1] = '\0';
	}
    } else {
	strcompose(tbuffer, 256, adir->name, "/", AFSDIR_CELLSERVDB_FILE,
		   NULL);
    }
#else
    strcompose(tbuffer, 256, adir->name, "/", AFSDIR_CELLSERVDB_FILE, NULL);
#endif /* AFS_NT40_ENV */

    code = stat(tbuffer, &tstat);
    if (code < 0) {
	return code;
    }
    /* did file change? */
    if (tstat.st_mtime == adir->timeRead) {
	return 0;
    }
    /* otherwise file has changed, so reopen it */
    return afsconf_Reopen(adir);
}

/* set modtime on file */
static int
afsconf_Touch(struct afsconf_dir *adir)
{
    char tbuffer[256];
#ifndef AFS_NT40_ENV
    struct timeval tvp[2];
#else
    char *p;
#endif

    adir->timeRead = 0;		/* just in case */
    adir->timeCheck = 0;

#ifdef AFS_NT40_ENV
    /* NT client CellServDB has different file name than NT server or Unix */

    if (IsClientConfigDirectory(adir->name)) {
	if (!afssw_GetClientCellServDBDir(&p)) {
	    strcompose(tbuffer, sizeof(tbuffer), p, "/",
		       AFSDIR_CELLSERVDB_FILE_NTCLIENT, NULL);
	    free(p);
	} else {
	    int len = (int)strlen(tbuffer);
	    if (tbuffer[len - 1] != '\\' && tbuffer[len - 1] != '/') {
		strncat(tbuffer, "\\", sizeof(tbuffer));
	    }
	    strncat(tbuffer, AFSDIR_CELLSERVDB_FILE_NTCLIENT,
		    sizeof(tbuffer));
	    tbuffer[sizeof(tbuffer) - 1] = '\0';
	}
    } else {
	strcompose(tbuffer, 256, adir->name, "/", AFSDIR_CELLSERVDB_FILE,
		   NULL);
    }

    return _utime(tbuffer, NULL);

#else
    strcompose(tbuffer, 256, adir->name, "/", AFSDIR_CELLSERVDB_FILE, NULL);
    gettimeofday(&tvp[0], NULL);
    tvp[1] = tvp[0];
    return utimes(tbuffer, tvp);
#endif /* AFS_NT40_ENV */
}

struct afsconf_dir *
afsconf_Open(const char *adir)
{
    struct afsconf_dir *tdir;
    afs_int32 code;

    LOCK_GLOBAL_MUTEX;
    /* zero structure and fill in name; rest is done by internal routine */
    tdir = (struct afsconf_dir *)malloc(sizeof(struct afsconf_dir));
    memset(tdir, 0, sizeof(struct afsconf_dir));
    tdir->name = strdup(adir);

    code = afsconf_OpenInternal(tdir, 0, 0);
    if (code) {
	char *afsconf_path, afs_confdir[128];

	free(tdir->name);
	/* Check global place only when local Open failed for whatever reason */
	if (!(afsconf_path = getenv("AFSCONF"))) {
	    /* The "AFSCONF" environment (or contents of "/.AFSCONF") will be typically set to something like "/afs/<cell>/common/etc" where, by convention, the default files for "ThisCell" and "CellServDB" will reside; note that a major drawback is that a given afs client on that cell may NOT contain the same contents... */
	    char *home_dir;
	    afsconf_FILE *fp;
	    size_t len;

	    if (!(home_dir = getenv("HOME"))) {
		/* Our last chance is the "/.AFSCONF" file */
		fp = fopen("/.AFSCONF", "r");
		if (fp == 0)
		    goto fail;

		fgets(afs_confdir, 128, fp);
		fclose(fp);
	    } else {
		char *pathname = NULL;

		afs_asprintf(&pathname, "%s/%s", home_dir, ".AFSCONF");
		if (pathname == NULL)
		    goto fail;

		fp = fopen(pathname, "r");
		free(pathname);

		if (fp == 0) {
		    /* Our last chance is the "/.AFSCONF" file */
		    fp = fopen("/.AFSCONF", "r");
		    if (fp == 0)
			goto fail;
		}
		fgets(afs_confdir, 128, fp);
		fclose(fp);
	    }
	    len = strlen(afs_confdir);
	    if (len == 0)
		goto fail;

	    if (afs_confdir[len - 1] == '\n') {
		afs_confdir[len - 1] = 0;
	    }
	    afsconf_path = afs_confdir;
	}
	tdir->name = strdup(afsconf_path);
	code = afsconf_OpenInternal(tdir, 0, 0);
	if (code) {
	    free(tdir->name);
	    goto fail;
	}
    }
    UNLOCK_GLOBAL_MUTEX;
    return tdir;

fail:
    free(tdir);
    UNLOCK_GLOBAL_MUTEX;
    return NULL;
}

static int
GetCellUnix(struct afsconf_dir *adir)
{
    char *rc;
    char tbuffer[256];
    char *start, *p;
    afsconf_FILE *fp;

    strcompose(tbuffer, 256, adir->name, "/", AFSDIR_THISCELL_FILE, NULL);
    fp = fopen(tbuffer, "r");
    if (fp == 0) {
	return -1;
    }
    rc = fgets(tbuffer, 256, fp);
    fclose(fp);
    if (rc == NULL)
        return -1;

    start = tbuffer;
    while (*start != '\0' && isspace(*start))
        start++;
    p = start;
    while (*p != '\0' && !isspace(*p))
        p++;
    *p = '\0';
    if (*start == '\0')
        return -1;

    adir->cellName = strdup(start);
    return 0;
}


#ifdef AFS_NT40_ENV
static int
GetCellNT(struct afsconf_dir *adir)
{
    if (IsClientConfigDirectory(adir->name)) {
	/* NT client config dir; ThisCell is in registry (no file). */
	return afssw_GetClientCellName(&adir->cellName);
    } else {
	/* NT server config dir; works just like Unix */
	return GetCellUnix(adir);
    }
}

/* The following procedures and structs are used on Windows only
 * to enumerate the Cell information distributed within the
 * Windows registry.  (See src/WINNT/afsd/cm_config.c)
 */
typedef struct _cm_enumCellRegistry {
    afs_uint32 client;  /* non-zero if client query */
    struct afsconf_dir *adir;
} cm_enumCellRegistry_t;

static long
cm_serverConfigProc(void *rockp, struct sockaddr_in *addrp,
                    char *hostNamep, unsigned short rank)
{
    struct afsconf_cell *cellInfop = (struct afsconf_cell *)rockp;

    if (cellInfop->numServers == MAXHOSTSPERCELL)
        return 0;

    cellInfop->hostAddr[cellInfop->numServers] = *addrp;
    strncpy(cellInfop->hostName[cellInfop->numServers], hostNamep, MAXHOSTCHARS);
    cellInfop->hostName[cellInfop->numServers][MAXHOSTCHARS-1] = '\0';
    cellInfop->numServers++;

    return 0;
}

static long
cm_enumCellRegistryProc(void *rockp, char * cellNamep)
{
    long code;
    cm_enumCellRegistry_t *enump = (cm_enumCellRegistry_t *)rockp;
    char linkedName[256] = "";
    int timeout = 0;
    struct afsconf_entry *newEntry;


    newEntry = malloc(sizeof(struct afsconf_entry));
    if (newEntry == NULL)
        return ENOMEM;
    newEntry->cellInfo.numServers = 0;

    code = cm_SearchCellRegistry(enump->client, cellNamep, NULL, linkedName, cm_serverConfigProc, &newEntry->cellInfo);
    if (code == CM_ERROR_FORCE_DNS_LOOKUP)
        code = cm_SearchCellByDNS(cellNamep, NULL, &timeout, cm_serverConfigProc, &newEntry->cellInfo);

    if (code == 0) {
        strncpy(newEntry->cellInfo.name, cellNamep, MAXCELLCHARS);
        newEntry->cellInfo.name[MAXCELLCHARS-1];
        if (linkedName[0])
            newEntry->cellInfo.linkedCell = strdup(linkedName);
        else
            newEntry->cellInfo.linkedCell = NULL;
        newEntry->cellInfo.timeout = timeout;
        newEntry->cellInfo.flags = 0;

        newEntry->next = enump->adir->entries;
	enump->adir->entries = newEntry;
    } else {
        free(newEntry);
    }
    return code;
}
#endif /* AFS_NT40_ENV */


static int
afsconf_OpenInternal(struct afsconf_dir *adir, char *cell,
		     char clones[])
{
    afsconf_FILE *tf;
    char *tp, *bp;
    struct afsconf_entry *curEntry;
    struct afsconf_aliasentry *curAlias;
    afs_int32 code;
    afs_int32 i;
    char tbuffer[256], tbuf1[256];
    struct stat tstat;
#ifdef AFS_NT40_ENV
    cm_enumCellRegistry_t enumCellRegistry = {0, 0};
#endif /* AFS_NT40_ENV */

    /* figure out the local cell name */
#ifdef AFS_NT40_ENV
    i = GetCellNT(adir);
    enumCellRegistry.adir = adir;
#else
    i = GetCellUnix(adir);
#endif

#ifndef AFS_FREELANCE_CLIENT	/* no local cell not fatal in freelance */
    if (i) {
	return i;
    }
#endif

    /* now parse the individual lines */
    curEntry = 0;

#ifdef AFS_NT40_ENV
    /* NT client/server have a CellServDB that is the same format as Unix.
     * However, the NT client uses a different file name
     */
    if (IsClientConfigDirectory(adir->name)) {
	/* NT client config dir */
	char *p;

        enumCellRegistry.client = 1;

	if (!afssw_GetClientCellServDBDir(&p)) {
	    strcompose(tbuffer, sizeof(tbuffer), p, "/",
		       AFSDIR_CELLSERVDB_FILE_NTCLIENT, NULL);
	    free(p);
	} else {
	    int len;
	    strncpy(tbuffer, adir->name, sizeof(tbuffer));
	    len = (int)strlen(tbuffer);
	    if (tbuffer[len - 1] != '\\' && tbuffer[len - 1] != '/') {
		strncat(tbuffer, "\\", sizeof(tbuffer));
	    }
	    strncat(tbuffer, AFSDIR_CELLSERVDB_FILE_NTCLIENT,
		    sizeof(tbuffer));
	    tbuffer[sizeof(tbuffer) - 1] = '\0';
	}
    } else {
	/* NT server config dir */
	strcompose(tbuffer, 256, adir->name, "/", AFSDIR_CELLSERVDB_FILE,
		   NULL);
    }
#else
    strcompose(tbuffer, 256, adir->name, "/", AFSDIR_CELLSERVDB_FILE, NULL);
#endif /* AFS_NT40_ENV */

    if (!stat(tbuffer, &tstat)) {
	adir->timeRead = tstat.st_mtime;
    } else {
	adir->timeRead = 0;
    }

    strlcpy(tbuf1, tbuffer, sizeof tbuf1);
    tf = fopen(tbuffer, "r");
    if (!tf) {
	return -1;
    }

    /* The CellServDB file is now open.
     * The following code parses the contents of the
     * file and creates a list with the first cell entry
     * in the CellServDB file at the end of the list.
     *
     * No checking is performed for duplicates.
     * The side effects of this process are that duplicate
     * entries appended to the end of the CellServDB file
     * take precedence and are found in a shorter period
     * of time.
     */

    while (1) {
	tp = fgets(tbuffer, sizeof(tbuffer), tf);
	if (!tp)
	    break;
	TrimLine(tbuffer, sizeof tbuffer);	/* remove white space */
	if (tbuffer[0] == 0 || tbuffer[0] == '\n')
	    continue;		/* empty line */
	if (tbuffer[0] == '>') {
	    char linkedcell[MAXCELLCHARS];
	    /* start new cell item */
	    if (curEntry) {
		/* thread this guy on the list */
		curEntry->next = adir->entries;
		adir->entries = curEntry;
		curEntry = 0;
	    }
	    curEntry =
		(struct afsconf_entry *)malloc(sizeof(struct afsconf_entry));
	    memset(curEntry, 0, sizeof(struct afsconf_entry));
	    code =
		ParseCellLine(tbuffer, curEntry->cellInfo.name, linkedcell);
	    if (code) {
		afsconf_CloseInternal(adir);
		fclose(tf);
		free(curEntry);
		return -1;
	    }
	    if (linkedcell[0] != '\0')
		curEntry->cellInfo.linkedCell = strdup(linkedcell);
	} else {
	    /* new host in the current cell */
	    if (!curEntry) {
		afsconf_CloseInternal(adir);
		fclose(tf);
		return -1;
	    }
	    i = curEntry->cellInfo.numServers;
	    if (i < MAXHOSTSPERCELL) {
		if (cell && !strcmp(cell, curEntry->cellInfo.name))
		    code =
			ParseHostLine(tbuffer,
				      &curEntry->cellInfo.hostAddr[i],
				      curEntry->cellInfo.hostName[i],
				      &clones[i]);
		else
		    code =
			ParseHostLine(tbuffer,
				      &curEntry->cellInfo.hostAddr[i],
				      curEntry->cellInfo.hostName[i], 0);

		if (code) {
		    if (code == AFSCONF_SYNTAX) {
			for (bp = tbuffer; *bp != '\n'; bp++) {	/* Take out the <cr> from the buffer */
			    if (!*bp)
				break;
			}
			*bp = '\0';
			fprintf(stderr,
				"Can't properly parse host line \"%s\" in configuration file %s\n",
				tbuffer, tbuf1);
		    }
		    free(curEntry);
		    fclose(tf);
		    afsconf_CloseInternal(adir);
		    return -1;
		}
		curEntry->cellInfo.numServers = ++i;
	    } else {
		fprintf(stderr,
			"Too many hosts for cell %s in configuration file %s\n",
			curEntry->cellInfo.name, tbuf1);
	    }
	}
    }
    fclose(tf);			/* close the file now */

    /* end the last partially-completed cell */
    if (curEntry) {
	curEntry->next = adir->entries;
	adir->entries = curEntry;
    }

#ifdef AFS_NT40_ENV
     /*
      * Windows maintains a CellServDB list in the Registry
      * that supercedes the contents of the CellServDB file.
      * Prepending these entries to the head of the list
      * is sufficient to enforce the precedence.
      */
     cm_EnumerateCellRegistry( enumCellRegistry.client,
                               cm_enumCellRegistryProc,
                               &enumCellRegistry);
#endif /* AFS_NT40_ENV */

    /* Read in the alias list */
    strcompose(tbuffer, 256, adir->name, "/", AFSDIR_CELLALIAS_FILE, NULL);

    tf = fopen(tbuffer, "r");
    while (tf) {
	char *aliasPtr;

	tp = fgets(tbuffer, sizeof(tbuffer), tf);
	if (!tp)
	    break;
	TrimLine(tbuffer, sizeof tbuffer);	/* remove white space */

	if (tbuffer[0] == '\0' || tbuffer[0] == '\n' || tbuffer[0] == '#')
	    continue;		/* empty line */

	tp = tbuffer;
	while (tp[0] != '\0' && tp[0] != ' ' && tp[0] != '\t')
	    tp++;
	if (tp[0] == '\0')
	    continue;		/* invalid line */

	while (tp[0] != '\0' && (tp[0] == ' ' || tp[0] == '\t'))
	    0[tp++] = '\0';
	if (tp[0] == '\0')
	    continue;		/* invalid line */

	aliasPtr = tp;
	while (tp[0] != '\0' && tp[0] != ' ' && tp[0] != '\t' && tp[0] != '\r'
	       && tp[0] != '\n')
	    tp++;
	tp[0] = '\0';

	curAlias = malloc(sizeof(*curAlias));
	memset(curAlias, 0, sizeof(*curAlias));

	strlcpy(curAlias->aliasInfo.aliasName, aliasPtr, sizeof curAlias->aliasInfo.aliasName);
	strlcpy(curAlias->aliasInfo.realName, tbuffer, sizeof curAlias->aliasInfo.realName);

	curAlias->next = adir->alias_entries;
	adir->alias_entries = curAlias;
    }

    if (tf != NULL)
	fclose(tf);
    /* now read the fs keys, if possible */
    adir->keystr = (struct afsconf_keys *)0;
    afsconf_IntGetKeys(adir);

    return 0;
}

/* parse a line of the form
 *"128.2.1.3   #hostname" or
 *"[128.2.1.3]  #hostname" for clones
 * into the appropriate pieces.
 */
static int
ParseHostLine(char *aline, struct sockaddr_in *addr, char *aname,
	      char *aclone)
{
    int i;
    int c[4];
    afs_int32 code;
    char *tp;

    if (*aline == '[') {
	if (aclone)
	    *aclone = 1;
	/* FIXME: length of aname unknown here */
	code = sscanf(aline, "[%d.%d.%d.%d] #%s", &c[0], &c[1], &c[2], &c[3],
		      aname);
    } else {
	if (aclone)
	    *aclone = 0;
	/* FIXME: length of aname unknown here */
	code = sscanf(aline, "%d.%d.%d.%d #%s", &c[0], &c[1], &c[2], &c[3],
		      aname);
    }
    if (code != 5)
	return AFSCONF_SYNTAX;
    for(i = 0; i < 4; ++i) {
	if (c[i] < 0 || c[i] > 255) {
	    fprintf(stderr, "Illegal IP address %d.%d.%d.%d\n", c[0], c[1],
		    c[2], c[3]);
	    return AFSCONF_SYNTAX;
	}
    }
    addr->sin_family = AF_INET;
    addr->sin_port = 0;
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
    addr->sin_len = sizeof(struct sockaddr_in);
#endif
    tp = (char *)&addr->sin_addr;
    *tp++ = c[0];
    *tp++ = c[1];
    *tp++ = c[2];
    *tp++ = c[3];
    return 0;
}

/* parse a line of the form
 * ">cellname [linkedcellname] [#comments]"
 * into the appropriate pieces.
 */
static int
ParseCellLine(char *aline, char *aname,
	      char *alname)
{
    int code;
    /* FIXME: length of aname, alname unknown here */
    code = sscanf(aline, ">%s %s", aname, alname);
    if (code == 1)
	*alname = '\0';
    if (code == 2) {
	if (*alname == '#') {
	    *alname = '\0';
	}
    }
    return (code > 0 ? 0 : AFSCONF_SYNTAX);
}

/* call aproc(entry, arock, adir) for all cells.  Proc must return 0, or we'll stop early and return the code it returns */
int
afsconf_CellApply(struct afsconf_dir *adir,
		  int (*aproc) (struct afsconf_cell * cell, void *arock,
				struct afsconf_dir * dir), void *arock)
{
    struct afsconf_entry *tde;
    afs_int32 code;
    LOCK_GLOBAL_MUTEX;
    for (tde = adir->entries; tde; tde = tde->next) {
	code = (*aproc) (&tde->cellInfo, arock, adir);
	if (code) {
	    UNLOCK_GLOBAL_MUTEX;
	    return code;
	}
    }
    UNLOCK_GLOBAL_MUTEX;
    return 0;
}

/* call aproc(entry, arock, adir) for all cell aliases.
 * Proc must return 0, or we'll stop early and return the code it returns
 */
int
afsconf_CellAliasApply(struct afsconf_dir *adir,
		       int (*aproc) (struct afsconf_cellalias * alias,
				     void *arock, struct afsconf_dir * dir),
		       void *arock)
{
    struct afsconf_aliasentry *tde;
    afs_int32 code;
    LOCK_GLOBAL_MUTEX;
    for (tde = adir->alias_entries; tde; tde = tde->next) {
	code = (*aproc) (&tde->aliasInfo, arock, adir);
	if (code) {
	    UNLOCK_GLOBAL_MUTEX;
	    return code;
	}
    }
    UNLOCK_GLOBAL_MUTEX;
    return 0;
}

afs_int32 afsconf_SawCell = 0;

int
afsconf_GetExtendedCellInfo(struct afsconf_dir *adir, char *acellName,
			    char *aservice, struct afsconf_cell *acellInfo,
			    char clones[])
{
    afs_int32 code;
    char *cell;

    code = afsconf_GetCellInfo(adir, acellName, aservice, acellInfo);
    if (code)
	return code;

    if (acellName)
	cell = acellName;
    else
	cell = (char *)&acellInfo->name;

    code = afsconf_OpenInternal(adir, cell, clones);
    return code;
}

#if !defined(AFS_NT40_ENV)
int
afsconf_LookupServer(const char *service, const char *protocol,
		     const char *cellName, unsigned short afsdbPort,
		     int *cellHostAddrs, char cellHostNames[][MAXHOSTCHARS],
		     unsigned short ports[], unsigned short ipRanks[],
		     int *numServers, int *ttl, char **arealCellName)
{
    int code = 0;
    int len;
    unsigned char answer[4096];
    unsigned char *p;
    char *dotcellname;
    char *realCellName;
    int cellnamelength, fullnamelength;
    char host[256];
    int server_num = 0;
    int minttl = 0;
    int try_init = 0;
    int dnstype = 0;
    int pass = 0;
    char *IANAname = (char *) afsconf_FindIANAName(service);
    int tservice = afsconf_FindService(service);

    realCellName = NULL;

    *numServers = 0;
    *ttl = 0;
    if (tservice <= 0 || !IANAname)
	return AFSCONF_NOTFOUND;	/* service not found */

    if (strchr(cellName,'.'))
	pass += 2;

    cellnamelength=strlen(cellName); /* _ ._ . . \0 */
    fullnamelength=cellnamelength+strlen(protocol)+strlen(IANAname)+6;
    dotcellname=malloc(fullnamelength);
    if (!dotcellname)
	return AFSCONF_NOTFOUND;	/* service not found */

#ifdef HAVE_RES_RETRANSRETRY
    if ((_res.options & RES_INIT) == 0 && res_init() == -1)
      return (0);

    /*
     * Rx timeout is typically 56 seconds; limit user experience to
     * similar timeout
     */
    _res.retrans = 18;
    _res.retry = 3;
#endif

 retryafsdb:
    switch (pass) {
    case 0:
	dnstype = T_SRV;
	code = snprintf(dotcellname, fullnamelength, "_%s._%s.%s.",
		 IANAname, protocol, cellName);
	break;
    case 1:
	dnstype = T_AFSDB;
	code = snprintf(dotcellname, fullnamelength, "%s.",
		 cellName);
	break;
    case 2:
	dnstype = T_SRV;
	code = snprintf(dotcellname, fullnamelength, "_%s._%s.%s",
		 IANAname, protocol, cellName);
	break;
    case 3:
	dnstype = T_AFSDB;
	code = snprintf(dotcellname, fullnamelength, "%s",
		 cellName);
	break;
    }
    if ((code < 0) || (code >= fullnamelength))
	goto findservererror;
    LOCK_GLOBAL_MUTEX;
    len = res_search(dotcellname, C_IN, dnstype, answer, sizeof(answer));
    UNLOCK_GLOBAL_MUTEX;

    if (len < 0) {
	if (try_init < 1) {
	    try_init++;
	    res_init();
	    goto retryafsdb;
	}
	if (pass < 3) {
	    pass++;
	    goto retryafsdb;
	} else {
	    code = AFSCONF_NOTFOUND;
	    goto findservererror;
	}
    }

    p = answer + sizeof(HEADER);	/* Skip header */
    code = dn_expand(answer, answer + len, p, host, sizeof(host));
    if (code < 0) {
	code = AFSCONF_NOTFOUND;
	goto findservererror;
    }

    p += code + QFIXEDSZ;	/* Skip name */

    while (p < answer + len) {
	int type, ttl, size;

	code = dn_expand(answer, answer + len, p, host, sizeof(host));
	if (code < 0) {
	    code = AFSCONF_NOTFOUND;
	    goto findservererror;
	}

	p += code;		/* Skip the name */
	type = (p[0] << 8) | p[1];
	p += 4;			/* Skip type and class */
	ttl = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
	p += 4;			/* Skip the TTL */
	size = (p[0] << 8) | p[1];
	p += 2;			/* Skip the size */

	if (type == T_AFSDB) {
	    struct hostent *he;
	    short afsdb_type;

	    afsdb_type = (p[0] << 8) | p[1];
	    if (afsdb_type == 1) {
		/*
		 * We know this is an AFSDB record for our cell, of the
		 * right AFSDB type.  Write down the true cell name that
		 * the resolver gave us above.
		 */
		if (!realCellName)
		    realCellName = strdup(host);
	    }

	    code = dn_expand(answer, answer + len, p + 2, host, sizeof(host));
	    if (code < 0) {
		code = AFSCONF_NOTFOUND;
		goto findservererror;
	    }

	    if ((afsdb_type == 1) && (server_num < MAXHOSTSPERCELL) &&
		/* Do we want to get TTL data for the A record as well? */
		(he = gethostbyname(host))) {
		afs_int32 ipaddr;
		memcpy(&ipaddr, he->h_addr, he->h_length);
		cellHostAddrs[server_num] = ipaddr;
		ports[server_num] = afsdbPort;
		ipRanks[server_num] = 0;
		strncpy(cellHostNames[server_num], host,
			sizeof(cellHostNames[server_num]));
		server_num++;

		if (!minttl || ttl < minttl)
		    minttl = ttl;
	    }
	}
	if (type == T_SRV) {
	    struct hostent *he;
	    /* math here: _ is 1, _ ._ is 3, _ ._ . is 4. then the domain. */
	    if ((strncmp(host + 1, IANAname, strlen(IANAname)) == 0) &&
		(strncmp(host + strlen(IANAname) + 3, protocol,
			 strlen(protocol)) == 0)) {
		if (!realCellName)
		    realCellName = strdup(host + strlen(IANAname) +
					  strlen(protocol) + 4);
	    }

	    code = dn_expand(answer, answer + len, p + 6, host, sizeof(host));
	    if (code < 0) {
		code = AFSCONF_NOTFOUND;
		goto findservererror;
	    }

	    if ((server_num < MAXHOSTSPERCELL) &&
		/* Do we want to get TTL data for the A record as well? */
		(he = gethostbyname(host))) {
		afs_int32 ipaddr;
		memcpy(&ipaddr, he->h_addr, he->h_length);
		cellHostAddrs[server_num] = ipaddr;
		ipRanks[server_num] = (p[0] << 8) | p[1];
		ports[server_num] = htons((p[4] << 8) | p[5]);
		/* weight = (p[2] << 8) | p[3]; */
		strncpy(cellHostNames[server_num], host,
			sizeof(cellHostNames[server_num]));
		server_num++;

		if (!minttl || ttl < minttl)
		    minttl = ttl;
	    }
	}

	p += size;
    }

    if (server_num == 0) {	/* No AFSDB or SRV records */
	code = AFSCONF_NOTFOUND;
	goto findservererror;
    }

    if (realCellName) {
	/* Convert the real cell name to lowercase */
	for (p = (unsigned char *)realCellName; *p; p++)
	    *p = tolower(*p);
    }

    *numServers = server_num;
    *ttl = minttl ? (time(0) + minttl) : 0;

    if ( *numServers > 0 ) {
        code =  0;
	*arealCellName = realCellName;
    } else
        code = AFSCONF_NOTFOUND;

findservererror:
    if (code && realCellName)
	free(realCellName);
    free(dotcellname);
    return code;
}

int
afsconf_GetAfsdbInfo(char *acellName, char *aservice,
		     struct afsconf_cell *acellInfo)
{
    afs_int32 cellHostAddrs[AFSMAXCELLHOSTS];
    char cellHostNames[AFSMAXCELLHOSTS][MAXHOSTCHARS];
    unsigned short ipRanks[AFSMAXCELLHOSTS];
    unsigned short ports[AFSMAXCELLHOSTS];
    char *realCellName = NULL;
    int ttl, numServers, i;
    char *service = aservice;
    int code;
    unsigned short afsdbport;
    if (!service) {
	service = "afs3-vlserver";
	afsdbport = htons(7003);
    } else {
	service = aservice;
	afsdbport = afsconf_FindService(service);
    }
    code = afsconf_LookupServer((const char *)service, "udp",
				(const char *)acellName, afsdbport,
				cellHostAddrs, cellHostNames,
				ports, ipRanks, &numServers, &ttl,
				&realCellName);

    /* If we couldn't find an entry for the requested service
     * and that service happens to be the prservice or kaservice
     * then fallback to searching for afs3-vlserver and assigning
     * the port number here. */
    if (code < 0 && (afsdbport == htons(7002) || afsdbport == htons(7004))) {
        code = afsconf_LookupServer("afs3-vlserver", "udp",
                                    (const char *)acellName, afsdbport,
                                    cellHostAddrs, cellHostNames,
                                    ports, ipRanks, &numServers, &ttl,
                                    &realCellName);
        if (code >= 0) {
            for (i = 0; i < numServers; i++)
                ports[i] = afsdbport;
        }
    }
    if (code == 0) {
	acellInfo->timeout = ttl;
	acellInfo->numServers = numServers;
	for (i = 0; i < numServers; i++) {
	    memcpy(&acellInfo->hostAddr[i].sin_addr.s_addr, &cellHostAddrs[i],
		   sizeof(afs_int32));
	    memcpy(acellInfo->hostName[i], cellHostNames[i], MAXHOSTCHARS);
	    acellInfo->hostAddr[i].sin_family = AF_INET;
	    acellInfo->hostAddr[i].sin_port = ports[i];

	    if (realCellName) {
		strlcpy(acellInfo->name, realCellName,
			sizeof(acellInfo->name));
		free(realCellName);
		realCellName = NULL;
	    }
	}
	acellInfo->linkedCell = NULL;       /* no linked cell */
	acellInfo->flags = 0;
    }
    return code;
}
#else /* windows */
int
afsconf_GetAfsdbInfo(char *acellName, char *aservice,
		     struct afsconf_cell *acellInfo)
{
    afs_int32 i;
    int tservice = afsconf_FindService(aservice);   /* network byte order */
    const char *ianaName = afsconf_FindIANAName(aservice);
    struct afsconf_entry DNSce;
    afs_int32 cellHostAddrs[AFSMAXCELLHOSTS];
    char cellHostNames[AFSMAXCELLHOSTS][MAXHOSTCHARS];
    unsigned short ipRanks[AFSMAXCELLHOSTS];
    unsigned short ports[AFSMAXCELLHOSTS];          /* network byte order */
    int numServers;
    int rc;
    int ttl;

    if (tservice < 0) {
        if (aservice)
            return AFSCONF_NOTFOUND;
        else
            tservice = 0;       /* port will be assigned by caller */
    }

    if (ianaName == NULL)
        ianaName = "afs3-vlserver";

    DNSce.cellInfo.numServers = 0;
    DNSce.next = NULL;

    rc = getAFSServer(ianaName, "udp", acellName, tservice,
                      cellHostAddrs, cellHostNames, ports, ipRanks, &numServers,
		      &ttl);
    /* ignore the ttl here since this code is only called by transitory programs
     * like klog, etc. */

    /* If we couldn't find an entry for the requested service
     * and that service happens to be the prservice or kaservice
     * then fallback to searching for afs3-vlserver and assigning
     * the port number here. */
    if (rc < 0 && (tservice == htons(7002) || tservice == htons(7004))) {
        rc = getAFSServer("afs3-vlserver", "udp", acellName, tservice,
                           cellHostAddrs, cellHostNames, ports, ipRanks, &numServers,
                           &ttl);
        if (rc >= 0) {
            for (i = 0; i < numServers; i++)
                ports[i] = tservice;
        }
    }

    if (rc < 0 || numServers == 0)
	return -1;

    for (i = 0; i < numServers; i++) {
	memcpy(&acellInfo->hostAddr[i].sin_addr.s_addr, &cellHostAddrs[i],
	       sizeof(afs_uint32));
	memcpy(acellInfo->hostName[i], cellHostNames[i], MAXHOSTCHARS);
	acellInfo->hostAddr[i].sin_family = AF_INET;
        if (aservice)
            acellInfo->hostAddr[i].sin_port = ports[i];
        else
            acellInfo->hostAddr[i].sin_port = 0;
    }

    acellInfo->numServers = numServers;
    strlcpy(acellInfo->name, acellName, sizeof acellInfo->name);
    acellInfo->linkedCell = NULL;	/* no linked cell */
    acellInfo->flags = 0;
    return 0;
}
#endif /* windows */

int
afsconf_GetCellInfo(struct afsconf_dir *adir, char *acellName, char *aservice,
		    struct afsconf_cell *acellInfo)
{
    struct afsconf_entry *tce;
    struct afsconf_aliasentry *tcae;
    struct afsconf_entry *bestce;
    afs_int32 i;
    int tservice;
    char *tcell;
    int cnLen;
    int ambig;
    char tbuffer[64];

    LOCK_GLOBAL_MUTEX;
    if (adir)
	afsconf_Check(adir);
    if (acellName) {
	tcell = acellName;
	cnLen = (int)(strlen(tcell) + 1);
	lcstring(tcell, tcell, cnLen);
	afsconf_SawCell = 1;	/* will ignore the AFSCELL switch on future */
	/* call to afsconf_GetLocalCell: like klog  */
    } else {
	i = afsconf_GetLocalCell(adir, tbuffer, sizeof(tbuffer));
	if (i) {
	    UNLOCK_GLOBAL_MUTEX;
	    return i;
	}
	tcell = tbuffer;
    }
    cnLen = strlen(tcell);
    bestce = (struct afsconf_entry *)0;
    ambig = 0;
    if (!adir) {
	UNLOCK_GLOBAL_MUTEX;
	return 0;
    }

    /* Look through the list of aliases */
    for (tcae = adir->alias_entries; tcae; tcae = tcae->next) {
	if (strcasecmp(tcae->aliasInfo.aliasName, tcell) == 0) {
	    tcell = tcae->aliasInfo.realName;
	    break;
	}
    }

    for (tce = adir->entries; tce; tce = tce->next) {
	if (strcasecmp(tce->cellInfo.name, tcell) == 0) {
	    /* found our cell */
	    bestce = tce;
	    ambig = 0;
	    break;
	}
	if (strlen(tce->cellInfo.name) < cnLen)
	    continue;		/* clearly wrong */
	if (strncasecmp(tce->cellInfo.name, tcell, cnLen) == 0) {
	    if (bestce)
		ambig = 1;	/* ambiguous unless we get exact match */
	    bestce = tce;
	}
    }
    if (!ambig && bestce && bestce->cellInfo.numServers) {
	*acellInfo = bestce->cellInfo;	/* structure assignment */
	if (aservice) {
	    tservice = afsconf_FindService(aservice);
	    if (tservice < 0) {
		UNLOCK_GLOBAL_MUTEX;
		return AFSCONF_NOTFOUND;	/* service not found */
	    }
	    for (i = 0; i < acellInfo->numServers; i++) {
		acellInfo->hostAddr[i].sin_port = tservice;
	    }
	}
	acellInfo->timeout = 0;

        /*
         * Until we figure out how to separate out ubik server
         * queries from other server queries, only perform gethostbyname()
         * lookup on the specified hostnames for the client CellServDB files.
         */
        if (IsClientConfigDirectory(adir->name) &&
            !(acellInfo->flags & AFSCONF_CELL_FLAG_DNS_QUERIED)) {
            int j;
            short numServers=0;		                        /*Num active servers for the cell */
            struct sockaddr_in hostAddr[MAXHOSTSPERCELL];	/*IP addresses for cell's servers */
            char hostName[MAXHOSTSPERCELL][MAXHOSTCHARS];	/*Names for cell's servers */

            memset(&hostAddr, 0, sizeof(hostAddr));
            memset(&hostName, 0, sizeof(hostName));

            for ( j=0; j<acellInfo->numServers && numServers < MAXHOSTSPERCELL; j++ ) {
                struct hostent *he = gethostbyname(acellInfo->hostName[j]);
                int foundAddr = 0;

                if (he && he->h_addrtype == AF_INET) {
                    int i;
                    /* obtain all the valid address from the list */
                    for (i=0 ; he->h_addr_list[i] && numServers < MAXHOSTSPERCELL; i++) {
                        /* check to see if this is a new address; if so insert it into the list */
                        int k, dup;
                        for (k=0, dup=0; !dup && k < numServers; k++) {
			    afs_uint32 addr;
			    memcpy(&addr, he->h_addr_list[i], sizeof(addr));
                            if (hostAddr[k].sin_addr.s_addr == addr) {
                                dup = 1;
			    }
                        }
                        if (dup)
                            continue;

                        hostAddr[numServers].sin_family = AF_INET;
                        hostAddr[numServers].sin_port = acellInfo->hostAddr[0].sin_port;
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
                        hostAddr[numServers].sin_len = sizeof(struct sockaddr_in);
#endif
                        memcpy(&hostAddr[numServers].sin_addr.s_addr, he->h_addr_list[i], sizeof(afs_uint32));
                        strcpy(hostName[numServers], acellInfo->hostName[j]);
                        foundAddr = 1;
                        numServers++;
                    }
                }
                if (!foundAddr) {
                    hostAddr[numServers] = acellInfo->hostAddr[j];
                    strcpy(hostName[numServers], acellInfo->hostName[j]);
                    numServers++;
                }
            }

            for (i=0; i<numServers; i++) {
                acellInfo->hostAddr[i] = hostAddr[i];
                strcpy(acellInfo->hostName[i], hostName[i]);
            }
            acellInfo->numServers = numServers;
            acellInfo->flags |= AFSCONF_CELL_FLAG_DNS_QUERIED;
        }
	UNLOCK_GLOBAL_MUTEX;
	return 0;
    } else {
	UNLOCK_GLOBAL_MUTEX;
	return afsconf_GetAfsdbInfo(tcell, aservice, acellInfo);
    }
}

int
afsconf_GetLocalCell(struct afsconf_dir *adir, char *aname,
		     afs_int32 alen)
{
    static int afsconf_showcell = 0;
    char *afscell_path;
    afs_int32 code = 0;

    LOCK_GLOBAL_MUTEX;
    /*
     * If a cell switch was specified in a command, then it should override the
     * AFSCELL variable.  If a cell was specified, then the afsconf_SawCell flag
     * is set and the cell name in the adir structure is used.
     * Read the AFSCELL var each time: in case it changes (unsetenv AFSCELL).
     */
    if (!afsconf_SawCell && (afscell_path = getenv("AFSCELL"))) {
	if (!afsconf_showcell) {
	    fprintf(stderr, "Note: Operation is performed on cell %s\n",
		    afscell_path);
	    afsconf_showcell = 1;
	}
	strncpy(aname, afscell_path, alen);
    } else {
	afsconf_Check(adir);
	if (adir->cellName) {
	    strncpy(aname, adir->cellName, alen);
	} else
	    code = AFSCONF_UNKNOWN;
    }

    UNLOCK_GLOBAL_MUTEX;
    return (code);
}

int
afsconf_Close(struct afsconf_dir *adir)
{
    LOCK_GLOBAL_MUTEX;
    afsconf_CloseInternal(adir);
    if (adir->name)
	free(adir->name);
    free(adir);
    UNLOCK_GLOBAL_MUTEX;
    return 0;
}

static int
afsconf_CloseInternal(struct afsconf_dir *adir)
{
    struct afsconf_entry *td, *nd;
    struct afsconf_aliasentry *ta, *na;
    char *tname;

    tname = adir->name;		/* remember name, since that's all we preserve */

    /* free everything we can find */
    if (adir->cellName)
	free(adir->cellName);
    for (td = adir->entries; td; td = nd) {
	nd = td->next;
	if (td->cellInfo.linkedCell)
	    free(td->cellInfo.linkedCell);
	free(td);
    }
    for (ta = adir->alias_entries; ta; ta = na) {
	na = ta->next;
	free(ta);
    }
    if (adir->keystr)
	free(adir->keystr);

    /* reinit */
    memset(adir, 0, sizeof(struct afsconf_dir));
    adir->name = tname;		/* restore it */
    return 0;
}

static int
afsconf_Reopen(struct afsconf_dir *adir)
{
    afs_int32 code;
    code = afsconf_CloseInternal(adir);
    if (code)
	return code;
    code = afsconf_OpenInternal(adir, 0, 0);
    return code;
}

/* called during opening of config file */
int
afsconf_IntGetKeys(struct afsconf_dir *adir)
{
    char tbuffer[256];
    int fd;
    struct afsconf_keys *tstr;
    afs_int32 code;

#ifdef AFS_NT40_ENV
    /* NT client config dir has no KeyFile; don't risk attempting open
     * because there might be a random file of this name if dir is shared.
     */
    if (IsClientConfigDirectory(adir->name)) {
	adir->keystr = ((struct afsconf_keys *)
			malloc(sizeof(struct afsconf_keys)));
	adir->keystr->nkeys = 0;
	return 0;
    }
#endif /* AFS_NT40_ENV */

    LOCK_GLOBAL_MUTEX;
    /* compute the key name and other setup */
    strcompose(tbuffer, 256, adir->name, "/", AFSDIR_KEY_FILE, NULL);
    tstr = (struct afsconf_keys *)malloc(sizeof(struct afsconf_keys));
    adir->keystr = tstr;

    /* read key file */
    fd = open(tbuffer, O_RDONLY);
    if (fd < 0) {
	tstr->nkeys = 0;
	UNLOCK_GLOBAL_MUTEX;
	return 0;
    }
    code = read(fd, tstr, sizeof(struct afsconf_keys));
    close(fd);
    if (code < sizeof(afs_int32)) {
	tstr->nkeys = 0;
	UNLOCK_GLOBAL_MUTEX;
	return 0;
    }

    /* convert key structure to host order */
    tstr->nkeys = ntohl(tstr->nkeys);

    if (code < sizeof(afs_int32) + (tstr->nkeys*sizeof(struct afsconf_key))) {
	tstr->nkeys = 0;
	UNLOCK_GLOBAL_MUTEX;
	return 0;
    }

    for (fd = 0; fd < tstr->nkeys; fd++)
	tstr->key[fd].kvno = ntohl(tstr->key[fd].kvno);

    UNLOCK_GLOBAL_MUTEX;
    return 0;
}

/* get keys structure */
int
afsconf_GetKeys(struct afsconf_dir *adir, struct afsconf_keys *astr)
{
    afs_int32 code;

    LOCK_GLOBAL_MUTEX;
    code = afsconf_Check(adir);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return AFSCONF_FAILURE;
    }
    memcpy(astr, adir->keystr, sizeof(struct afsconf_keys));
    UNLOCK_GLOBAL_MUTEX;
    return 0;
}

/* get latest key */
afs_int32
afsconf_GetLatestKey(struct afsconf_dir * adir, afs_int32 * avno,
		     struct ktc_encryptionKey *akey)
{
    int i;
    int maxa;
    struct afsconf_key *tk;
    afs_int32 best;
    struct afsconf_key *bestk;
    afs_int32 code;

    LOCK_GLOBAL_MUTEX;
    code = afsconf_Check(adir);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return AFSCONF_FAILURE;
    }
    maxa = adir->keystr->nkeys;

    best = -1;			/* highest kvno we've seen yet */
    bestk = (struct afsconf_key *)0;	/* ptr to structure providing best */
    for (tk = adir->keystr->key, i = 0; i < maxa; i++, tk++) {
	if (tk->kvno == 999)
	    continue;		/* skip bcrypt keys */
	if (tk->kvno > best) {
	    best = tk->kvno;
	    bestk = tk;
	}
    }
    if (bestk) {		/* found any  */
	if (akey)
	    memcpy(akey, bestk->key, 8);	/* copy out latest key */
	if (avno)
	    *avno = bestk->kvno;	/* and kvno to caller */
	UNLOCK_GLOBAL_MUTEX;
	return 0;
    }
    UNLOCK_GLOBAL_MUTEX;
    return AFSCONF_NOTFOUND;	/* didn't find any keys */
}

/* get a particular key */
int
afsconf_GetKey(void *rock, int avno, struct ktc_encryptionKey *akey)
{
    struct afsconf_dir *adir = (struct afsconf_dir *) rock;
    int i, maxa;
    struct afsconf_key *tk;
    afs_int32 code;

    LOCK_GLOBAL_MUTEX;
    code = afsconf_Check(adir);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return AFSCONF_FAILURE;
    }
    maxa = adir->keystr->nkeys;

    for (tk = adir->keystr->key, i = 0; i < maxa; i++, tk++) {
	if (tk->kvno == avno) {
	    memcpy(akey, tk->key, 8);
	    UNLOCK_GLOBAL_MUTEX;
	    return 0;
	}
    }

    UNLOCK_GLOBAL_MUTEX;
    return AFSCONF_NOTFOUND;
}

/* save the key structure in the appropriate file */
static int
SaveKeys(struct afsconf_dir *adir)
{
    struct afsconf_keys tkeys;
    int fd;
    afs_int32 i;
    char tbuffer[256];

    memcpy(&tkeys, adir->keystr, sizeof(struct afsconf_keys));

    /* convert it to net byte order */
    for (i = 0; i < tkeys.nkeys; i++)
	tkeys.key[i].kvno = htonl(tkeys.key[i].kvno);
    tkeys.nkeys = htonl(tkeys.nkeys);

    /* rewrite keys file */
    strcompose(tbuffer, 256, adir->name, "/", AFSDIR_KEY_FILE, NULL);
    fd = open(tbuffer, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
	return AFSCONF_FAILURE;
    i = write(fd, &tkeys, sizeof(tkeys));
    if (i != sizeof(tkeys)) {
	close(fd);
	return AFSCONF_FAILURE;
    }
    if (close(fd) < 0)
	return AFSCONF_FAILURE;
    return 0;
}

int
afsconf_AddKey(struct afsconf_dir *adir, afs_int32 akvno, char akey[8],
	       afs_int32 overwrite)
{
    struct afsconf_keys *tk;
    struct afsconf_key *tkey;
    afs_int32 i;
    int foundSlot;

    LOCK_GLOBAL_MUTEX;
    tk = adir->keystr;

    if (akvno != 999) {
	if (akvno < 0 || akvno > 255) {
	    UNLOCK_GLOBAL_MUTEX;
	    return ERANGE;
	}
    }
    foundSlot = 0;
    for (i = 0, tkey = tk->key; i < tk->nkeys; i++, tkey++) {
	if (tkey->kvno == akvno) {
	    if (!overwrite) {
		UNLOCK_GLOBAL_MUTEX;
		return AFSCONF_KEYINUSE;
	    }
	    foundSlot = 1;
	    break;
	}
    }
    if (!foundSlot) {
	if (tk->nkeys >= AFSCONF_MAXKEYS) {
	    UNLOCK_GLOBAL_MUTEX;
	    return AFSCONF_FULL;
	}
	tkey = &tk->key[tk->nkeys++];
    }
    tkey->kvno = akvno;
    memcpy(tkey->key, akey, 8);
    i = SaveKeys(adir);
    afsconf_Touch(adir);
    UNLOCK_GLOBAL_MUTEX;
    return i;
}

/* this proc works by sliding the other guys down, rather than using a funny
    kvno value, so that callers can count on getting a good key in key[0].
*/
int
afsconf_DeleteKey(struct afsconf_dir *adir, afs_int32 akvno)
{
    struct afsconf_keys *tk;
    struct afsconf_key *tkey;
    int i;
    int foundFlag = 0;

    LOCK_GLOBAL_MUTEX;
    tk = adir->keystr;

    for (i = 0, tkey = tk->key; i < tk->nkeys; i++, tkey++) {
	if (tkey->kvno == akvno) {
	    foundFlag = 1;
	    break;
	}
    }
    if (!foundFlag) {
	UNLOCK_GLOBAL_MUTEX;
	return AFSCONF_NOTFOUND;
    }

    /* otherwise slide the others down.  i and tkey point at the guy to delete */
    for (; i < tk->nkeys - 1; i++, tkey++) {
	tkey->kvno = (tkey + 1)->kvno;
	memcpy(tkey->key, (tkey + 1)->key, 8);
    }
    tk->nkeys--;
    i = SaveKeys(adir);
    afsconf_Touch(adir);
    UNLOCK_GLOBAL_MUTEX;
    return i;
}
