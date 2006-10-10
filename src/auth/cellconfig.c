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

#include <afs/stds.h>
#include <afs/pthread_glock.h>
#ifdef UKERNEL
#include "afs/sysincludes.h"
#include "afsincludes.h"
#else /* UKERNEL */
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
#ifdef AFS_AFSDB_ENV
#include <arpa/nameser.h>
#ifdef HAVE_ARPA_NAMESER_COMPAT_H
#include <arpa/nameser_compat.h>
#endif
#include <resolv.h>
#endif /* AFS_AFSDB_ENV */
#endif /* AFS_NT40_ENV */
#include <afs/afsint.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
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
#endif /* UKERNEL */
#include <afs/afsutil.h>
#include "cellconfig.h"
#include "keys.h"
#ifdef AFS_NT40_ENV
#ifdef AFS_AFSDB_ENV
/* cm_dns.h depends on cellconfig.h */
#include <cm_dns.h>
#endif /* AFS_AFSDB_ENV */
#endif
static struct afsconf_servPair serviceTable[] = {
    {"afs", 7000,},
    {"afscb", 7001,},
    {"afsprot", 7002,},
    {"afsvldb", 7003,},
    {"afskauth", 7004,},
    {"afsvol", 7005,},
    {"afserror", 7006,},
    {"afsnanny", 7007,},
    {"afsupdate", 7008,},
    {"afsrmtsys", 7009,},
    {"afsres", 7010,},		/* residency database for MR-AFS */
    {"afsremio", 7011,},	/* remote I/O interface for MR-AFS */
    {0, 0}			/* insert new services before this spot */
};

/* Prototypes */
static afs_int32 afsconf_FindService(register const char *aname);
static int TrimLine(char *abuffer);
#ifdef AFS_NT40_ENV
static int IsClientConfigDirectory(const char *path);
static int GetCellNT(struct afsconf_dir *adir);
#endif
static int afsconf_Check(register struct afsconf_dir *adir);
static int afsconf_Touch(register struct afsconf_dir *adir);
static int GetCellUnix(struct afsconf_dir *adir);
static int afsconf_OpenInternal(register struct afsconf_dir *adir, char *cell,
				char clones[]);
static int ParseHostLine(char *aline, register struct sockaddr_in *addr,
			 char *aname, char *aclone);
static int ParseCellLine(register char *aline, register char *aname,
			 register char *alname);
static int afsconf_CloseInternal(register struct afsconf_dir *adir);
static int afsconf_Reopen(register struct afsconf_dir *adir);
static int SaveKeys(struct afsconf_dir *adir);

#ifndef T_AFSDB
#define T_AFSDB 18		/* per RFC1183 section 1 */
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
static afs_int32
afsconf_FindService(register const char *aname)
{
    /* lookup a service name */
    struct servent *ts;
    register struct afsconf_servPair *tsp;

#if     defined(AFS_OSF_ENV) 
    ts = getservbyname(aname, "");
#else
    ts = getservbyname(aname, NULL);
#endif
    if (ts) {
	/* we found it in /etc/services, so we use this value */
	return ts->s_port;	/* already in network byte order */
    }

    /* not found in /etc/services, see if it is one of ours */
    for (tsp = serviceTable;; tsp++) {
	if (tsp->name == NULL)
	    return -1;
	if (!strcmp(tsp->name, aname))
	    return htons(tsp->port);
    }
}

static int
TrimLine(char *abuffer)
{
    char tbuffer[256];
    register char *tp;
    register int tc;

    tp = abuffer;
    while ((tc = *tp)) {
	if (!isspace(tc))
	    break;
	tp++;
    }
    strcpy(tbuffer, tp);
    strcpy(abuffer, tbuffer);
    return 0;
}

#ifdef AFS_NT40_ENV
/*
 * IsClientConfigDirectory() -- determine if path matches well-known
 *     client configuration directory.
 */
static int
IsClientConfigDirectory(const char *path)
{
    const char *cdir = AFSDIR_CLIENT_ETC_DIRPATH;
    int i;

    for (i = 0; cdir[i] != '\0' && path[i] != '\0'; i++) {
	int cc = tolower(cdir[i]);
	int pc = tolower(path[i]);

	if (cc == '\\') {
	    cc = '/';
	}
	if (pc == '\\') {
	    pc = '/';
	}
	if (cc != pc) {
	    return 0;
	}
    }

    /* hit end of one or both; allow mismatch in existence of trailing slash */
    if (cdir[i] != '\0') {
	if ((cdir[i] != '\\' && cdir[i] != '/') || (cdir[i + 1] != '\0')) {
	    return 0;
	}
    }
    if (path[i] != '\0') {
	if ((path[i] != '\\' && path[i] != '/') || (path[i + 1] != '\0')) {
	    return 0;
	}
    }
    return 1;
}
#endif /* AFS_NT40_ENV */


static int
afsconf_Check(register struct afsconf_dir *adir)
{
    char tbuffer[256], *p;
    struct stat tstat;
    register afs_int32 code;

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
	    len = strlen(tbuffer);
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
afsconf_Touch(register struct afsconf_dir *adir)
{
    char tbuffer[256], *p;
#ifndef AFS_NT40_ENV
    struct timeval tvp[2];
#endif

    adir->timeRead = 0;		/* just in case */

#ifdef AFS_NT40_ENV
    /* NT client CellServDB has different file name than NT server or Unix */

    if (IsClientConfigDirectory(adir->name)) {
	if (!afssw_GetClientCellServDBDir(&p)) {
	    strcompose(tbuffer, sizeof(tbuffer), p, "/",
		       AFSDIR_CELLSERVDB_FILE_NTCLIENT, NULL);
	    free(p);
	} else {
	    int len = strlen(tbuffer);
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
afsconf_Open(register const char *adir)
{
    register struct afsconf_dir *tdir;
    register afs_int32 code;

    LOCK_GLOBAL_MUTEX;
    /* zero structure and fill in name; rest is done by internal routine */
    tdir = (struct afsconf_dir *)malloc(sizeof(struct afsconf_dir));
    memset(tdir, 0, sizeof(struct afsconf_dir));
    tdir->name = (char *)malloc(strlen(adir) + 1);
    strcpy(tdir->name, adir);

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
		if (fp == 0) {
		    free(tdir);
		    UNLOCK_GLOBAL_MUTEX;
		    return (struct afsconf_dir *)0;
		}
		fgets(afs_confdir, 128, fp);
		fclose(fp);
	    } else {
		char pathname[256];

		sprintf(pathname, "%s/%s", home_dir, ".AFSCONF");
		fp = fopen(pathname, "r");
		if (fp == 0) {
		    /* Our last chance is the "/.AFSCONF" file */
		    fp = fopen("/.AFSCONF", "r");
		    if (fp == 0) {
			free(tdir);
			UNLOCK_GLOBAL_MUTEX;
			return (struct afsconf_dir *)0;
		    }
		}
		fgets(afs_confdir, 128, fp);
		fclose(fp);
	    }
	    len = strlen(afs_confdir);
	    if (len == 0) {
		free(tdir);
		UNLOCK_GLOBAL_MUTEX;
		return (struct afsconf_dir *)0;
	    }
	    if (afs_confdir[len - 1] == '\n') {
		afs_confdir[len - 1] = 0;
	    }
	    afsconf_path = afs_confdir;
	}
	tdir->name = (char *)malloc(strlen(afsconf_path) + 1);
	strcpy(tdir->name, afsconf_path);
	code = afsconf_OpenInternal(tdir, 0, 0);
	if (code) {
	    free(tdir->name);
	    free(tdir);
	    UNLOCK_GLOBAL_MUTEX;
	    return (struct afsconf_dir *)0;
	}
    }
    UNLOCK_GLOBAL_MUTEX;
    return tdir;
}

static int
GetCellUnix(struct afsconf_dir *adir)
{
    char *rc;
    char tbuffer[256];
    char *p;
    afsconf_FILE *fp;
    
    strcompose(tbuffer, 256, adir->name, "/", AFSDIR_THISCELL_FILE, NULL);
    fp = fopen(tbuffer, "r");
    if (fp == 0) {
	return -1;
    }
    rc = fgets(tbuffer, 256, fp);
    fclose(fp);

    p = strchr(tbuffer, '\n');
    if (p)
	*p = '\0';

    adir->cellName = strdup(tbuffer);
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
#endif /* AFS_NT40_ENV */


static int
afsconf_OpenInternal(register struct afsconf_dir *adir, char *cell,
		     char clones[])
{
    afsconf_FILE *tf;
    register char *tp, *bp;
    register struct afsconf_entry *curEntry;
    struct afsconf_aliasentry *curAlias;
    register afs_int32 code;
    afs_int32 i;
    char tbuffer[256], tbuf1[256];
    struct stat tstat;

    /* figure out the cell name */
#ifdef AFS_NT40_ENV
    i = GetCellNT(adir);
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
	if (!afssw_GetClientCellServDBDir(&p)) {
	    strcompose(tbuffer, sizeof(tbuffer), p, "/",
		       AFSDIR_CELLSERVDB_FILE_NTCLIENT, NULL);
	    free(p);
	} else {
	    int len;
	    strncpy(tbuffer, adir->name, sizeof(tbuffer));
	    len = strlen(tbuffer);
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

    strcpy(tbuf1, tbuffer);
    tf = fopen(tbuffer, "r");
    if (!tf) {
	return -1;
    }
    while (1) {
	tp = fgets(tbuffer, sizeof(tbuffer), tf);
	if (!tp)
	    break;
	TrimLine(tbuffer);	/* remove white space */
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
	    if (linkedcell[0] != '\0') {
		curEntry->cellInfo.linkedCell =
		    (char *)malloc(strlen(linkedcell) + 1);
		strcpy(curEntry->cellInfo.linkedCell, linkedcell);
	    }
	} else {
	    /* new host in the current cell */
	    if (!curEntry) {
		afsconf_CloseInternal(adir);
		fclose(tf);
		return -1;
	    }
	    i = curEntry->cellInfo.numServers;
	    if (cell && !strcmp(cell, curEntry->cellInfo.name))
		code =
		    ParseHostLine(tbuffer, &curEntry->cellInfo.hostAddr[i],
				  curEntry->cellInfo.hostName[i], &clones[i]);
	    else
		code =
		    ParseHostLine(tbuffer, &curEntry->cellInfo.hostAddr[i],
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
	}
    }
    fclose(tf);			/* close the file now */

    /* end the last partially-completed cell */
    if (curEntry) {
	curEntry->next = adir->entries;
	adir->entries = curEntry;
    }

    /* Read in the alias list */
    strcompose(tbuffer, 256, adir->name, "/", AFSDIR_CELLALIAS_FILE, NULL);

    tf = fopen(tbuffer, "r");
    while (tf) {
	char *aliasPtr;

	tp = fgets(tbuffer, sizeof(tbuffer), tf);
	if (!tp)
	    break;
	TrimLine(tbuffer);	/* remove white space */

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

	strcpy(curAlias->aliasInfo.aliasName, aliasPtr);
	strcpy(curAlias->aliasInfo.realName, tbuffer);

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
ParseHostLine(char *aline, register struct sockaddr_in *addr, char *aname,
	      char *aclone)
{
    int c1, c2, c3, c4;
    register afs_int32 code;
    register char *tp;

    if (*aline == '[') {
	if (aclone)
	    *aclone = 1;
	/* FIXME: length of aname unknown here */
	code = sscanf(aline, "[%d.%d.%d.%d] #%s", &c1, &c2, &c3, &c4, aname);
    } else {
	if (aclone)
	    *aclone = 0;
	/* FIXME: length of aname unknown here */
	code = sscanf(aline, "%d.%d.%d.%d #%s", &c1, &c2, &c3, &c4, aname);
    }
    if (code != 5)
	return AFSCONF_SYNTAX;
    addr->sin_family = AF_INET;
    addr->sin_port = 0;
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
    addr->sin_len = sizeof(struct sockaddr_in);
#endif
    tp = (char *)&addr->sin_addr;
    *tp++ = c1;
    *tp++ = c2;
    *tp++ = c3;
    *tp++ = c4;
    return 0;
}

/* parse a line of the form
 * ">cellname [linkedcellname] [#comments]"
 * into the appropriate pieces.
 */
static int
ParseCellLine(register char *aline, register char *aname,
	      register char *alname)
{
    register int code;
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
		  int (*aproc) (struct afsconf_cell * cell, char *arock,
				struct afsconf_dir * dir), char *arock)
{
    register struct afsconf_entry *tde;
    register afs_int32 code;
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
				     char *arock, struct afsconf_dir * dir),
		       char *arock)
{
    register struct afsconf_aliasentry *tde;
    register afs_int32 code;
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

#ifdef AFS_AFSDB_ENV
#if !defined(AFS_NT40_ENV)
int
afsconf_GetAfsdbInfo(char *acellName, char *aservice,
		     struct afsconf_cell *acellInfo)
{
    afs_int32 code;
    int tservice, i, len;
    unsigned char answer[1024];
    unsigned char *p;
    char *dotcellname;
    int cellnamelength;
    char realCellName[256];
    char host[256];
    int server_num = 0;
    int minttl = 0;

    /* The resolver isn't always MT-safe.. Perhaps this ought to be
     * replaced with a more fine-grained lock just for the resolver
     * operations.
     */

    if ( ! strchr(acellName,'.') ) {
       cellnamelength=strlen(acellName);
       dotcellname=malloc(cellnamelength+2);
       memcpy(dotcellname,acellName,cellnamelength);
       dotcellname[cellnamelength]='.';
       dotcellname[cellnamelength+1]=0;
       LOCK_GLOBAL_MUTEX;
	    len = res_search(dotcellname, C_IN, T_AFSDB, answer, sizeof(answer));
       if ( len < 0 ) {
          len = res_search(acellName, C_IN, T_AFSDB, answer, sizeof(answer));
       }
       UNLOCK_GLOBAL_MUTEX;
       free(dotcellname);
    } else {
       LOCK_GLOBAL_MUTEX;
	    len = res_search(acellName, C_IN, T_AFSDB, answer, sizeof(answer));
       UNLOCK_GLOBAL_MUTEX;
    }
    if (len < 0)
	return AFSCONF_NOTFOUND;

    p = answer + sizeof(HEADER);	/* Skip header */
    code = dn_expand(answer, answer + len, p, host, sizeof(host));
    if (code < 0)
	return AFSCONF_NOTFOUND;

    p += code + QFIXEDSZ;	/* Skip name */

    while (p < answer + len) {
	int type, ttl, size;

	code = dn_expand(answer, answer + len, p, host, sizeof(host));
	if (code < 0)
	    return AFSCONF_NOTFOUND;

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
		strcpy(realCellName, host);
	    }

	    code = dn_expand(answer, answer + len, p + 2, host, sizeof(host));
	    if (code < 0)
		return AFSCONF_NOTFOUND;

	    if ((afsdb_type == 1) && (server_num < MAXHOSTSPERCELL) &&
		/* Do we want to get TTL data for the A record as well? */
		(he = gethostbyname(host))) {
		afs_int32 ipaddr;
		memcpy(&ipaddr, he->h_addr, he->h_length);
		acellInfo->hostAddr[server_num].sin_addr.s_addr = ipaddr;
		strncpy(acellInfo->hostName[server_num], host,
			sizeof(acellInfo->hostName[server_num]));
		server_num++;

		if (!minttl || ttl < minttl)
		    minttl = ttl;
	    }
	}

	p += size;
    }

    if (server_num == 0)	/* No AFSDB records */
	return AFSCONF_NOTFOUND;

    /* Convert the real cell name to lowercase */
    for (p = (unsigned char *)realCellName; *p; p++)
	*p = tolower(*p);

    strncpy(acellInfo->name, realCellName, sizeof(acellInfo->name));
    acellInfo->numServers = server_num;

    if (aservice) {
	tservice = afsconf_FindService(aservice);
	if (tservice < 0)
	    return AFSCONF_NOTFOUND;	/* service not found */
	for (i = 0; i < acellInfo->numServers; i++) {
	    acellInfo->hostAddr[i].sin_port = tservice;
	}
    }

    acellInfo->timeout = minttl ? (time(0) + minttl) : 0;

    return 0;
}
#else /* windows */
int
afsconf_GetAfsdbInfo(char *acellName, char *aservice,
		     struct afsconf_cell *acellInfo)
{
    register afs_int32 i;
    int tservice;
    struct afsconf_entry DNSce;
    afs_int32 cellHostAddrs[AFSMAXCELLHOSTS];
    char cellHostNames[AFSMAXCELLHOSTS][MAXHOSTCHARS];
    int numServers;
    int rc;
    int ttl;

    DNSce.cellInfo.numServers = 0;
    DNSce.next = NULL;
    rc = getAFSServer(acellName, cellHostAddrs, cellHostNames, &numServers,
		      &ttl);
    /* ignore the ttl here since this code is only called by transitory programs
     * like klog, etc. */
    if (rc < 0)
	return -1;
    if (numServers == 0)
	return -1;

    for (i = 0; i < numServers; i++) {
	memcpy(&acellInfo->hostAddr[i].sin_addr.s_addr, &cellHostAddrs[i],
	       sizeof(long));
	memcpy(acellInfo->hostName[i], cellHostNames[i], MAXHOSTCHARS);
	acellInfo->hostAddr[i].sin_family = AF_INET;

	/* sin_port supplied by connection code */
    }

    acellInfo->numServers = numServers;
    strcpy(acellInfo->name, acellName);
    if (aservice) {
	LOCK_GLOBAL_MUTEX;
	tservice = afsconf_FindService(aservice);
	UNLOCK_GLOBAL_MUTEX;
	if (tservice < 0) {
	    return AFSCONF_NOTFOUND;	/* service not found */
	}
	for (i = 0; i < acellInfo->numServers; i++) {
	    acellInfo->hostAddr[i].sin_port = tservice;
	}
    }
    acellInfo->linkedCell = NULL;	/* no linked cell */
    acellInfo->flags = 0;
    return 0;
}
#endif /* windows */
#endif /* AFS_AFSDB_ENV */

int
afsconf_GetCellInfo(struct afsconf_dir *adir, char *acellName, char *aservice,
		    struct afsconf_cell *acellInfo)
{
    register struct afsconf_entry *tce;
    struct afsconf_aliasentry *tcae;
    struct afsconf_entry *bestce;
    register afs_int32 i;
    int tservice;
    char *tcell;
    size_t cnLen;
    int ambig;
    char tbuffer[64];

    LOCK_GLOBAL_MUTEX;
    if (adir)
	afsconf_Check(adir);
    if (acellName) {
	tcell = acellName;
	cnLen = strlen(tcell) + 1;
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
	UNLOCK_GLOBAL_MUTEX;
	return 0;
    } else {
	UNLOCK_GLOBAL_MUTEX;
#ifdef AFS_AFSDB_ENV
	return afsconf_GetAfsdbInfo(tcell, aservice, acellInfo);
#else
	return AFSCONF_NOTFOUND;
#endif /* AFS_AFSDB_ENV */
    }
}

int
afsconf_GetLocalCell(register struct afsconf_dir *adir, char *aname,
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
afsconf_CloseInternal(register struct afsconf_dir *adir)
{
    register struct afsconf_entry *td, *nd;
    struct afsconf_aliasentry *ta, *na;
    register char *tname;

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
afsconf_Reopen(register struct afsconf_dir *adir)
{
    register afs_int32 code;
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
    register int fd;
    struct afsconf_keys *tstr;
    register afs_int32 code;

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
    register afs_int32 code;

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
afsconf_GetLatestKey(struct afsconf_dir * adir, afs_int32 * avno, char *akey)
{
    register int i;
    int maxa;
    register struct afsconf_key *tk;
    register afs_int32 best;
    struct afsconf_key *bestk;
    register afs_int32 code;

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
afsconf_GetKey(struct afsconf_dir *adir, afs_int32 avno, char *akey)
{
    register int i, maxa;
    register struct afsconf_key *tk;
    register afs_int32 code;

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
    register int fd;
    register afs_int32 i;
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
    register struct afsconf_keys *tk;
    register struct afsconf_key *tkey;
    register afs_int32 i;
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
    register struct afsconf_keys *tk;
    register struct afsconf_key *tkey;
    register int i;
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
