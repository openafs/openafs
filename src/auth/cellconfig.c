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
#include <afs/pthread_glock.h>
#ifdef UKERNEL
#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
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
#endif
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif /* UKERNEL */
#include <afs/afsutil.h>
#include "cellconfig.h"
#include "keys.h"

static ParseHostLine();
static ParseCellLine();
static afsconf_OpenInternal();
static afsconf_CloseInternal();
static afsconf_Reopen();

static struct afsconf_servPair serviceTable [] = {
    "afs",	7000,
    "afscb",	7001,
    "afsprot",	7002,
    "afsvldb",	7003,
    "afskauth",	7004,
    "afsvol",	7005,
    "afserror",	7006,
    "afsnanny",	7007,
    "afsupdate",7008,
    "afsrmtsys",7009,
    0, 0		/* insert new services before this spot */
};

/*
 * Basic Rule: we touch "<AFSCONF_DIR>/CellServDB" every time we change anything, so
 * our code can tell if there is new info in the key files, the cell server db
 * files or any of the other files (and reopen the thing) if the date on
 * CellServDB changes.
 */

/* return port number in network byte order in the low 16 bits of a long; return -1 if not found */
static afs_int32 afsconf_FindService(aname)
register char *aname; {
    /* lookup a service name */
    struct servent *ts;
    register struct afsconf_servPair *tsp;
#if     defined(AFS_OSF_ENV) || defined(AFS_DEC_ENV)
    ts = getservbyname(aname, "");
#else
    ts = getservbyname(aname, (char *) 0);
#endif
    if (ts) {
	/* we found it in /etc/services, so we use this value */
	return ts->s_port;  /* already in network byte order */
    }
    /* not found in /etc/services, see if it is one of ours */
    for(tsp = serviceTable;; tsp++) {
	if (tsp->name == (char *) 0) return -1;
	if (!strcmp(tsp->name, aname)) return htons(tsp->port);
    }
}

static int TrimLine(abuffer)
char *abuffer; {
    char tbuffer[256];
    register char *tp;
    register int tc;

    tp = abuffer;
    while (tc = *tp) {
	if (!isspace(tc)) break;
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
static int IsClientConfigDirectory(const char *path)
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


static int afsconf_Check(adir)
register struct afsconf_dir *adir; {
    char tbuffer[256];
    struct stat tstat;
    register afs_int32 code;

#ifdef AFS_NT40_ENV
    /* NT client CellServDB has different file name than NT server or Unix */
    if (IsClientConfigDirectory(adir->name)) {
	strcompose(tbuffer, 256,
		   adir->name, "/", AFSDIR_CELLSERVDB_FILE_NTCLIENT, NULL);
    } else {
	strcompose(tbuffer, 256,
		   adir->name, "/", AFSDIR_CELLSERVDB_FILE, NULL);
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
static afsconf_Touch(adir)
register struct afsconf_dir *adir; {
    char tbuffer[256];
    struct timeval tvp[2];

#ifdef AFS_NT40_ENV
    /* NT client CellServDB has different file name than NT server or Unix */
    if (IsClientConfigDirectory(adir->name)) {
	strcompose(tbuffer, 256,
		   adir->name, "/", AFSDIR_CELLSERVDB_FILE_NTCLIENT, NULL);
    } else {
	strcompose(tbuffer, 256,
		   adir->name, "/", AFSDIR_CELLSERVDB_FILE, NULL);
    }
#else
    strcompose(tbuffer, 256, adir->name, "/", AFSDIR_CELLSERVDB_FILE, NULL);
#endif /* AFS_NT40_ENV */

    adir->timeRead = 0;	/* just in case */
#ifdef AFS_NT40_ENV
    return _utime(tbuffer, NULL);
#else
    gettimeofday(&tvp[0], NULL);
    tvp[1] = tvp[0];
    return utimes(tbuffer, tvp);
#endif  /* AFS_NT40_ENV */
}

struct afsconf_dir *afsconf_Open(adir)
register char *adir; {
    register struct afsconf_dir *tdir;
    register afs_int32 code;

    LOCK_GLOBAL_MUTEX
    /* zero structure and fill in name; rest is done by internal routine */
    tdir = (struct afsconf_dir *) malloc(sizeof(struct afsconf_dir));
    bzero(tdir, sizeof(struct afsconf_dir));
    tdir->name = (char *) malloc(strlen(adir)+1);
    strcpy(tdir->name, adir);

    code = afsconf_OpenInternal(tdir);
    if (code) {
	char *afsconf_path, *getenv(), afs_confdir[128];

	free(tdir->name);
	/* Check global place only when local Open failed for whatever reason */
	if (!(afsconf_path = getenv("AFSCONF"))) {
	    /* The "AFSCONF" environment (or contents of "/.AFSCONF") will be typically set to something like "/afs/<cell>/common/etc" where, by convention, the default files for "ThisCell" and "CellServDB" will reside; note that a major drawback is that a given afs client on that cell may NOT contain the same contents... */
	    char *home_dir;
	    FILE *fp;
	    int len;

	    if (!(home_dir = getenv("HOME"))) {
		/* Our last chance is the "/.AFSCONF" file */
		fp = fopen("/.AFSCONF", "r");
		if (fp == 0) {
		    free(tdir);
		    UNLOCK_GLOBAL_MUTEX
		    return (struct afsconf_dir *) 0;
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
			UNLOCK_GLOBAL_MUTEX
			return (struct afsconf_dir *) 0;
		    }
		    fgets(afs_confdir, 128, fp);
		    fclose(fp);
		}
		fgets(afs_confdir, 128, fp);
		fclose(fp);		
	    }
	    len = strlen(afs_confdir);
	    if (len == 0) {
		free(tdir);
		UNLOCK_GLOBAL_MUTEX
		return (struct afsconf_dir *) 0;
	    }
	    if (afs_confdir[len-1] == '\n') {
		afs_confdir[len-1] = 0;
	    }
	    afsconf_path = afs_confdir;
	}
	tdir->name = (char *) malloc(strlen(afsconf_path)+1);
	strcpy(tdir->name, afsconf_path);
	code = afsconf_OpenInternal(tdir);
	if (code) {
	    free(tdir->name);
	    free(tdir);
	    UNLOCK_GLOBAL_MUTEX
	    return (struct afsconf_dir *) 0;
	}
    }
    UNLOCK_GLOBAL_MUTEX
    return tdir;
}


static int GetCellUnix(struct afsconf_dir *adir)
{
    int rc;
    char tbuffer[256];
    FILE *tf;

    strcompose(tbuffer, 256, adir->name, "/", AFSDIR_THISCELL_FILE, NULL);
    tf = fopen(tbuffer, "r");
    if (tf) {
	rc = fscanf(tf, "%s", tbuffer);
	if (rc == 1) {
	    adir->cellName = (char *) malloc(strlen(tbuffer)+1);
	    strcpy(adir->cellName, tbuffer);
	}
	fclose(tf);
    }
    else {
	return -1;
    }
    return 0;
}


#ifdef AFS_NT40_ENV
static int GetCellNT(struct afsconf_dir *adir)
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


static int afsconf_OpenInternal(adir)
register struct afsconf_dir *adir; {
    FILE *tf;
    register char *tp, *bp;
    register struct afsconf_entry *curEntry;
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
    if (i) {
	return i;
    }

    /* now parse the individual lines */
    curEntry = 0;

#ifdef AFS_NT40_ENV
    /* NT client/server have a CellServDB that is the same format as Unix.
     * However, the NT client uses a different file name
     */
    if (IsClientConfigDirectory(adir->name)) {
	/* NT client config dir */
	strcompose(tbuffer, 256,
		   adir->name, "/", AFSDIR_CELLSERVDB_FILE_NTCLIENT, NULL);
    } else {
	/* NT server config dir */
	strcompose(tbuffer, 256,
		   adir->name, "/", AFSDIR_CELLSERVDB_FILE, NULL);
    }
#else
    strcompose(tbuffer, 256, adir->name, "/", AFSDIR_CELLSERVDB_FILE, NULL);
#endif  /* AFS_NT40_ENV */

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
	if (!tp) break;
	TrimLine(tbuffer);  /* remove white space */
	if (tbuffer[0] == 0 || tbuffer[0] == '\n') continue;   /* empty line */
	if (tbuffer[0] == '>') {
	    char linkedcell[MAXCELLCHARS];
	    /* start new cell item */
	    if (curEntry) {
		/* thread this guy on the list */
		curEntry->next = adir->entries;
		adir->entries = curEntry;
		curEntry = 0;
	    }
	    curEntry = (struct afsconf_entry *) malloc(sizeof(struct afsconf_entry));
	    bzero(curEntry, sizeof(struct afsconf_entry));
	    code = ParseCellLine(tbuffer, curEntry->cellInfo.name, linkedcell);
	    if (code) {
		afsconf_CloseInternal(adir);
		fclose(tf);
		return -1;
	    }
	    if (linkedcell[0] != '\0') {
		curEntry->cellInfo.linkedCell =
		    (char *) malloc(strlen(linkedcell) + 1);
		strcpy(curEntry->cellInfo.linkedCell, linkedcell);
	    }
	}
	else {
	    /* new host in the current cell */
	    if (!curEntry) {
		afsconf_CloseInternal(adir);
		fclose(tf);
		return -1;
	    }
	    i = curEntry->cellInfo.numServers;
	    code = ParseHostLine(tbuffer, (char *) &curEntry->cellInfo.hostAddr[i], curEntry->cellInfo.hostName[i]);
	    if (code) {
		if (code == AFSCONF_SYNTAX) {
		    for (bp=tbuffer; *bp != '\n'; bp++) {	/* Take out the <cr> from the buffer */
			if (!*bp) break;
		    }
		    *bp= '\0';
		    fprintf(stderr, "Can't properly parse host line \"%s\" in configuration file %s\n", tbuffer, tbuf1);
		}
		free(curEntry);
		fclose(tf);
		afsconf_CloseInternal(adir);
		return -1;
	    }
	    curEntry->cellInfo.numServers = ++i;
	}
    }
    fclose(tf);	/* close the file now */

    /* end the last partially-completed cell */
    if (curEntry) {
	curEntry->next = adir->entries;
	adir->entries = curEntry;
    }
    
    /* now read the fs keys, if possible */
    adir->keystr = (struct afsconf_keys *) 0;
    afsconf_IntGetKeys(adir);

    return 0;
}

/* parse a line of the form
 *"128.2.1.3	#hostname"
 * into the appropriate pieces.  
 */
static ParseHostLine(aline, addr, aname)
register struct sockaddr_in *addr;
char *aline, *aname; {
    int c1, c2, c3, c4;
    register afs_int32 code;
    register char *tp;

    code = sscanf(aline, "%d.%d.%d.%d #%s", &c1, &c2, &c3, &c4, aname);
    if (code != 5) return AFSCONF_SYNTAX;
    addr->sin_family = AF_INET;
    addr->sin_port = 0;
    tp = (char *) &addr->sin_addr;
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
static ParseCellLine(aline, aname, alname)
register char *aline, *aname, *alname; {
    register int code;
    code = sscanf(aline, ">%s %s", aname, alname);
    if (code == 1) *alname = '\0';
    if (code == 2) {
	if (*alname == '#') {
	    *alname = '\0';
	}
    }
    return (code > 0 ? 0 : AFSCONF_SYNTAX);
}

/* call aproc(entry, arock, adir) for all cells.  Proc must return 0, or we'll stop early and return the code it returns */
afsconf_CellApply(adir, aproc, arock)
struct afsconf_dir *adir;
int (*aproc)();
char *arock; {
    register struct afsconf_entry *tde;
    register afs_int32 code;
    LOCK_GLOBAL_MUTEX
    for(tde=adir->entries; tde; tde=tde->next) {
	code = (*aproc)(&tde->cellInfo, arock, adir);
	if (code) {
	    UNLOCK_GLOBAL_MUTEX
	    return code;
	}
    }
    UNLOCK_GLOBAL_MUTEX
    return 0;
}

afs_int32 afsconf_SawCell = 0;
afsconf_GetCellInfo(adir, acellName, aservice, acellInfo)
struct afsconf_dir *adir;
char *aservice;
char *acellName;
struct afsconf_cell *acellInfo; {
    register struct afsconf_entry *tce;
    struct afsconf_entry *bestce;
    register afs_int32 i;
    int tservice;
    char *tcell;
    int cnLen, ambig;
    char tbuffer[64];

    LOCK_GLOBAL_MUTEX
    if (adir) afsconf_Check(adir);
    if (acellName) {
	tcell = acellName;
	cnLen = strlen(tcell)+1;
	lcstring (tcell, tcell, cnLen);
	afsconf_SawCell = 1;                       /* will ignore the AFSCELL switch on future */
	                                           /* call to afsconf_GetLocalCell: like klog  */
    } else {
	i = afsconf_GetLocalCell(adir, tbuffer, sizeof(tbuffer));
	if (i) {
	    UNLOCK_GLOBAL_MUTEX
	    return i;
	}
	tcell = tbuffer;
    }
    cnLen = strlen(tcell);
    bestce = (struct afsconf_entry *) 0;
    ambig = 0;
    if (!adir) {
	UNLOCK_GLOBAL_MUTEX
	return 0;
    }
    for(tce=adir->entries;tce;tce=tce->next) {
	if (strcasecmp(tce->cellInfo.name, tcell) == 0) {
	    /* found our cell */
	    bestce = tce;
	    ambig = 0;
	    break;
	}
	if (strlen(tce->cellInfo.name) < cnLen)	continue;   /* clearly wrong */
	if (strncasecmp(tce->cellInfo.name, tcell, cnLen) == 0) {
	    if (bestce)	ambig =	1;  /* ambiguous unless we get exact match */
	    bestce = tce;
	}
    }
    if (!ambig && bestce) {
	*acellInfo = bestce->cellInfo;	/* structure assignment */
	if (aservice) {
	    tservice = afsconf_FindService(aservice);
	    if (tservice < 0) {
		UNLOCK_GLOBAL_MUTEX
		return AFSCONF_NOTFOUND;  /* service not found */
	    }
	    for(i=0;i<acellInfo->numServers;i++) {
		acellInfo->hostAddr[i].sin_port = tservice;
	    }
	}
	UNLOCK_GLOBAL_MUTEX
	return 0;
    }
    else {
	UNLOCK_GLOBAL_MUTEX
	return AFSCONF_NOTFOUND;
    }
}

afsconf_GetLocalCell(adir, aname, alen)
register struct afsconf_dir *adir;
char *aname;
afs_int32 alen; {
    static int  afsconf_showcell = 0;
    char        *afscell_path;
    char        *getenv();
    afs_int32        code = 0;

   LOCK_GLOBAL_MUTEX
   /*
    * If a cell switch was specified in a command, then it should override the 
    * AFSCELL variable.  If a cell was specified, then the afsconf_SawCell flag
    * is set and the cell name in the adir structure is used.
    * Read the AFSCELL var each time: in case it changes (unsetenv AFSCELL).
    */
   if ( !afsconf_SawCell && (afscell_path= getenv("AFSCELL")) ) {     
	if ( !afsconf_showcell ) {
	    fprintf(stderr, "Note: Operation is performed on cell %s\n", afscell_path);
	    afsconf_showcell = 1;
	}
	strncpy(aname, afscell_path, alen);
    } else {                                    
	afsconf_Check(adir);
	if (adir->cellName) {
	    strncpy(aname, adir->cellName, alen);
	}
	else code = AFSCONF_UNKNOWN;
    }

    UNLOCK_GLOBAL_MUTEX
    return(code);
}

afsconf_Close(adir)
struct afsconf_dir *adir; {
    LOCK_GLOBAL_MUTEX
    afsconf_CloseInternal(adir);
    if (adir->name) free(adir->name);
    free(adir);
    UNLOCK_GLOBAL_MUTEX
    return 0;
}

static int afsconf_CloseInternal(adir)
register struct afsconf_dir *adir; {
    register struct afsconf_entry *td, *nd;
    register char *tname;

    tname = adir->name;	/* remember name, since that's all we preserve */

    /* free everything we can find */
    if (adir->cellName) free(adir->cellName);
    for(td=adir->entries;td;td=nd) {
	nd = td->next;
	if (td->cellInfo.linkedCell)
	    free(td->cellInfo.linkedCell);
	free(td);
    }
    if (adir->keystr) free(adir->keystr);

    /* reinit */
    bzero(adir, sizeof(struct afsconf_dir));
    adir->name = tname;	    /* restore it */
    return 0;
}

static int afsconf_Reopen(adir)
register struct afsconf_dir *adir; {
    register afs_int32 code;
    code = afsconf_CloseInternal(adir);
    if (code) return code;
    code = afsconf_OpenInternal(adir);
    return code;
}

/* called during opening of config file */
afsconf_IntGetKeys(adir)
struct afsconf_dir *adir;
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

    LOCK_GLOBAL_MUTEX
    /* compute the key name and other setup */

    strcompose(tbuffer, 256, adir->name, "/", AFSDIR_KEY_FILE, NULL);
    tstr = (struct afsconf_keys *) malloc(sizeof (struct afsconf_keys));
    adir->keystr = tstr;

    /* read key file */
    fd = open(tbuffer, O_RDONLY);
    if (fd < 0) {
	tstr->nkeys = 0;
	UNLOCK_GLOBAL_MUTEX
	return 0;
    }
    code = read(fd, tstr, sizeof(struct afsconf_keys));
    close(fd);
    if (code < sizeof(afs_int32)) {
	tstr->nkeys = 0;
	UNLOCK_GLOBAL_MUTEX
	return 0;
    }

    /* convert key structure to host order */
    tstr->nkeys = ntohl(tstr->nkeys);
    for(fd=0;fd<tstr->nkeys;fd++)
	tstr->key[fd].kvno = ntohl(tstr->key[fd].kvno);

    UNLOCK_GLOBAL_MUTEX
    return 0;
}

/* get keys structure */
afsconf_GetKeys(adir, astr)
struct afsconf_dir *adir;
struct afsconf_keys *astr;
{
    LOCK_GLOBAL_MUTEX
    afsconf_Check(adir);
    bcopy(adir->keystr, astr, sizeof(struct afsconf_keys));
    UNLOCK_GLOBAL_MUTEX
    return 0;
}

/* get latest key */
afs_int32 afsconf_GetLatestKey(adir, avno, akey)
  IN struct afsconf_dir *adir;
  OUT afs_int32 *avno;
  OUT char *akey;
{
    register int i;
    int maxa;
    register struct afsconf_key *tk;
    register afs_int32 best;
    struct afsconf_key *bestk;
    
    LOCK_GLOBAL_MUTEX
    afsconf_Check(adir);
    maxa = adir->keystr->nkeys;

    best = -1;	    /* highest kvno we've seen yet */
    bestk = (struct afsconf_key	*) 0;	/* ptr to structure providing best */
    for(tk = adir->keystr->key,i=0;i<maxa;i++,tk++) {
	if (tk->kvno ==	999) continue;	/* skip bcrypt keys */
	if (tk->kvno > best) {
	    best = tk->kvno;
	    bestk = tk;
	}
    }
    if (bestk) {    /* found any  */
	if (akey) bcopy(bestk->key, akey, 8); /* copy out latest key */
	if (avno) *avno = bestk->kvno;	/* and kvno to caller */
	UNLOCK_GLOBAL_MUTEX
	return 0;
    }
    UNLOCK_GLOBAL_MUTEX
    return AFSCONF_NOTFOUND;	/* didn't find any keys */
}

/* get a particular key */
afsconf_GetKey(adir, avno, akey)
struct afsconf_dir *adir;
afs_int32 avno;
char *akey;
{
    register int i, maxa;
    register struct afsconf_key *tk;

    LOCK_GLOBAL_MUTEX
    afsconf_Check(adir);
    maxa = adir->keystr->nkeys;

    for(tk = adir->keystr->key,i=0;i<maxa;i++,tk++) {
	if (tk->kvno == avno) {
	    bcopy(tk->key, akey, 8);
	    UNLOCK_GLOBAL_MUTEX
	    return 0;
	}
    }

    UNLOCK_GLOBAL_MUTEX
    return AFSCONF_NOTFOUND;
}

/* save the key structure in the appropriate file */
static SaveKeys(adir)
struct afsconf_dir *adir;
{
    struct afsconf_keys tkeys;
    register int fd;
    register afs_int32 i;
    char tbuffer[256];

    bcopy(adir->keystr, &tkeys, sizeof(struct afsconf_keys));

    /* convert it to net byte order */
    for(i = 0; i<tkeys.nkeys; i++ )
	tkeys.key[i].kvno = htonl(tkeys.key[i].kvno);
    tkeys.nkeys = htonl(tkeys.nkeys);
    
    /* rewrite keys file */
    strcompose(tbuffer, 256, adir->name, "/", AFSDIR_KEY_FILE, NULL);
    fd = open(tbuffer, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return AFSCONF_FAILURE;
    i = write(fd, &tkeys, sizeof(tkeys));
    if (i != sizeof(tkeys)) {
	close(fd);
	return AFSCONF_FAILURE;
    }
    if (close(fd) < 0) return AFSCONF_FAILURE;
    return 0;
}

afsconf_AddKey(adir, akvno, akey, overwrite)
struct afsconf_dir *adir;
afs_int32 akvno, overwrite;
char akey[8];
{
    register struct afsconf_keys *tk;
    register struct afsconf_key *tkey;
    register afs_int32 i;
    int foundSlot;

    LOCK_GLOBAL_MUTEX
    tk = adir->keystr;
    
    if (akvno != 999) {
	if (akvno < 0 || akvno > 255) {
	    UNLOCK_GLOBAL_MUTEX
	    return ERANGE;
	}
    }
    foundSlot = 0;
    for(i=0, tkey = tk->key; i<tk->nkeys; i++, tkey++) {
	if (tkey->kvno == akvno) {
	    if (!overwrite) {
		UNLOCK_GLOBAL_MUTEX
		return AFSCONF_KEYINUSE;
	    }
	    foundSlot = 1;
	    break;
	}
    }
    if (!foundSlot) {
	if (tk->nkeys >= AFSCONF_MAXKEYS) {
	    UNLOCK_GLOBAL_MUTEX
	    return AFSCONF_FULL;
	}
	tkey = &tk->key[tk->nkeys++];
    }
    tkey->kvno = akvno;
    bcopy(akey, tkey->key, 8);
    i = SaveKeys(adir);
    afsconf_Touch(adir);
    UNLOCK_GLOBAL_MUTEX
    return i;
}

/* this proc works by sliding the other guys down, rather than using a funny
    kvno value, so that callers can count on getting a good key in key[0].
*/
afsconf_DeleteKey(adir, akvno)
struct afsconf_dir *adir;
afs_int32 akvno;
{
    register struct afsconf_keys *tk;
    register struct afsconf_key *tkey;
    register int i;
    int foundFlag = 0;

    LOCK_GLOBAL_MUTEX
    tk = adir->keystr;

    for(i=0, tkey = tk->key; i<tk->nkeys; i++, tkey++) {
	if (tkey->kvno == akvno) {
	    foundFlag = 1;
	    break;
	}
    }
    if (!foundFlag) {
	UNLOCK_GLOBAL_MUTEX
	return AFSCONF_NOTFOUND;
    }

    /* otherwise slide the others down.  i and tkey point at the guy to delete */
    for(;i<tk->nkeys-1; i++,tkey++) {
	tkey->kvno = (tkey+1)->kvno;
	bcopy((tkey+1)->key, tkey->key, 8);
    }
    tk->nkeys--;
    i = SaveKeys(adir);
    afsconf_Touch(adir);
    UNLOCK_GLOBAL_MUTEX
    return i;
}
