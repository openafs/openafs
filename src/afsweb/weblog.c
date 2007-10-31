/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements the weblog binary which links with the AFS libraries and acts
 * as the server for authenticating users for apache access to AFS. Code
 * structure is based on klog.c. The communication with clients is done 
 * via pipes whose file descriptors are passed as command line arguments
 * thus making it necessary for a common parent to start this process and 
 * the processes that will communicate with it for them to inherit the 
 * pipes. Also passed as a command line argument is a Silent flag (like klog)
 * and a cache expiration flag which allows cache expiration times for
 * tokens to be set for testing purposes
 */


/* These two needed for rxgen output to work */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/stds.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <rx/xdr.h>

#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif

#include <lock.h>
#include <ubik.h>
#include <stdio.h>
#include <pwd.h>
#include <signal.h>

#include <afs/com_err.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/cmd.h>

#include "weblog_errors.h"

#ifndef MAX
#define MAX(A,B)		((A)>(B)?(A):(B))
#endif /* !MAX */
#ifndef MIN
#define MIN(A,B)		((A)<(B)?(A):(B))
#endif /* !MAX */

#include "apache_afs_utils.h"

#define MAXBUFF 1024

/* For caching */
#include "apache_afs_cache.h"

/* the actual function that does all the work! */
int CommandProc();

static int zero_argc;
static char **zero_argv;

/* pipes used for communicating with web server */
/* these are passed as command line args - defaults to stdin/stdout */

static int readPipe;
static int writePipe;

/* 
 * now I know why this was necessary! - it's a hokie thing - 
 * the call to ka_UserAuthenticateGeneral doesn't compile otherwise
 */
int
osi_audit()
{
    return 0;
}


main(int argc, char **argv)
{
    struct cmd_syndesc *ts;
    afs_int32 code;

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

/* 
 * we ignore SIGPIPE so that EPIPE is returned if there is no one reading
 * data being written to the pipe 
 */

#ifdef AIX
    /* TODO - for AIX? */
#else
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
#endif

    zero_argc = argc;
    zero_argv = argv;

    ts = cmd_CreateSyntax(NULL, CommandProc, NULL,
			  "obtain Kerberos authentication for web servers");

/* define the command line arguments */
#define aREADPIPE 0
#define aWRITEPIPE 1
#define aCACHEEXPIRATION 2
#define aTOKENEXPIRATION 3
#define aSILENT 4

    cmd_AddParm(ts, "-readPipe", CMD_SINGLE, CMD_OPTIONAL, "inPipefd");
    cmd_AddParm(ts, "-writePipe", CMD_SINGLE, CMD_OPTIONAL, "outPipefd");
    cmd_AddParm(ts, "-cacheExpiration", CMD_SINGLE, CMD_OPTIONAL,
		"local cache expiration times for tokens");
    cmd_AddParm(ts, "-tokenExpiration", CMD_SINGLE, CMD_OPTIONAL,
		"cache manager expiration time for tokens");
    cmd_AddParm(ts, "-silent", CMD_FLAG, CMD_OPTIONAL, "silent operation");

    code = cmd_Dispatch(argc, argv);

    WEBLOGEXIT(code);
}


/*
 * send a buffer over the pipe 
 */
static int
sendToken(int len, void *buf)
{
    if (buf == NULL) {
	WEBLOGEXIT(NULLARGSERROR);
    }
    if (write(writePipe, buf, len) != len) {
#ifdef DEBUG
	perror("weblog: write to pipe error");
#endif
	return -1;
    }
    return 0;
}


/*
 * read the incoming buffer from the pipe
 */

static int
readFromClient(char *buf)
{
    int n;
    if (buf == NULL) {
	WEBLOGEXIT(NULLARGSERROR);
    }
    n = read(readPipe, buf, MAXBUFF);
    if (n < 0) {
#ifdef DEBUG
	perror("weblog: pipe read error");
#endif
	return -1;
    }
    if (n == 0) {
#ifdef DEBUG
	perror("weblog: zero bytes read from pipe");
#endif
	return -1;
    }
    return n;
}

/* 
 * copies the string spereated by the sep into retbuf and returns the position
 * from the beginning of the string that this string ends at so you can call 
 * it againword seperated by the sep character and give that value as th start 
 * parameter - used to parse incoming buffer from clients over the pipe
 */
/* 
 * NOTE - the space seperated credentials failed for passwds with spaces, thus
 * we use newline for seperators instead
 */
static int
getNullSepWord(char *buf, char sep, char *retBuf, int start)
{
    int ret = 0;
    int len = strlen(buf) - start;

    /* error checks */
    if ((buf == NULL) || (retBuf == NULL) || (start < 0)) {
	fprintf(stderr, "weblog (getWordSep):NULL args\n");
	return -1;
    }

    while ((buf[start] != sep) && (ret <= len)) {
	retBuf[ret] = buf[start];
	ret++;
	start++;
    }
    retBuf[ret] = '\0';
    return (ret + 1);
}


/* 
 * parses the NEWLINE seperated buffer giving the username, passwd and cell
 * coming over the pipe from the clients and sets the variables accordingly
 */
static int
parseBuf(char *buf, char *user, char *pass, char *cell, char *type)
{
    char *temp;
    int start = 0, ret = 0;

    if ((buf == NULL) || (user == NULL) || (pass == NULL) || (cell == NULL)
	|| (type == NULL)) {
#ifdef DEBUG
	fprintf(stderr, "afs_Authenticator:parseBuf-an arg was NULL\n");
#endif
	return -1;
    }
    if ((ret = getNullSepWord(buf, '\n', type, start)) < 0) {
	return -1;
    }
    start += ret;
    if ((ret = getNullSepWord(buf, '\n', user, start)) < 0) {
	return -1;
    }
    start += ret;
    if ((ret = getNullSepWord(buf, '\n', cell, start)) < 0) {
	return -1;
    }
    start += ret;
    if ((ret = getNullSepWord(buf, '\n', pass, start)) < 0) {
	return -1;
    }
    return 0;
}


/*
 * Discard any authentication information held in trust by the Cache Manager
 * for the calling process and all other processes in the same PAG
 */
static int
unlog()
{
    return do_pioctl(NULL, 0, NULL, 0, VIOCUNPAG, NULL, 0);
}


/* we can obtain a PAG by calling this system call */
static int
makeNewPAG()
{
    return do_setpag();
}


/*
 * The main procedure that waits in an infinite loop for data to
 * arrive through a pipe from the httpds, authenticates the user and
 * returns a token (or a failure message) over the pipe
 */
static int
CommandProc(struct cmd_syndesc *as, void *arock)
{
    char name[MAXKTCNAMELEN];
    char cell[MAXKTCREALMLEN];
    char passwd[BUFSIZ];
/* All the constant sizes for these arrays are taken from the code for klog */

    char cksum[SHA_HASH_BYTES];	/* for sha checksum for caching */
    afs_int32 expires = 0;	/* for cache expiration */
    afs_int32 cacheExpiration = 0;	/* configurable cmd line parameter */
    afs_int32 testExpires = 0;	/* cacheExpiration + current time */

    int authtype = 0;		/* AFS or AFS-DFS Authentication */
    int code;
    int shutdown = 0;		/* on getting shutdown from the pipe we GO */
    int gotToken = 0;		/* did we get a token from the cache manager */
    afs_int32 i = 0;		/* for getting primary token held by CM */
    int dosetpag = 1;		/* not used */
    Date lifetime;		/* requested ticket lifetime */
    char tbuffer[MAXBUFF];	/* for pioctl ops + pipe transfers */
    static char rn[] = "weblog";	/* Routine name */
    static int Silent = 0;	/* Don't want error messages */
    afs_int32 password_expires = -1;
    char *reason;		/* string describing errors */
    char type[10];		/* authentication type AFS or DFS */

    /* blow away command line arguments */
    for (i = 1; i < zero_argc; i++)
	memset(zero_argv[i], 0, strlen(zero_argv[i]));
    zero_argc = 0;

    /* first determine quiet flag based on -silent switch */
    Silent = (as->parms[aSILENT].items ? 1 : 0);

    code = ka_Init(0);
    if (code) {
	if (!Silent) {
	    fprintf(stderr, "%s:ka_Init FAILED\n", rn);
	}
	WEBLOGEXIT(KAERROR);
    }

    /* Parse our arguments. */
    if (as->parms[aREADPIPE].items)
	/* there is a file descriptor instead of stdin */
	readPipe = atoi(as->parms[aREADPIPE].items->data);
    else
	readPipe = stdin;

    if (as->parms[aWRITEPIPE].items)
	/* there is a file descriptor instead of stdout */
	writePipe = atoi(as->parms[aWRITEPIPE].items->data);
    else
	writePipe = stdout;

    if (as->parms[aCACHEEXPIRATION].items)
	/* set configurable cache expiration time */
	cacheExpiration = atoi(as->parms[aCACHEEXPIRATION].items->data);

    if (as->parms[aTOKENEXPIRATION].items)
	/* set configurable token lifetime */
	lifetime = atoi(as->parms[aTOKENEXPIRATION].items->data);

    /*
     * Initialize the cache for tokens
     */
    token_cache_init();

    /* 
     * discard any tokens held for this PAG - 
     * should we create a seperate PAG for weblog first? makeNewPAG does that
     */
#ifdef DEBUG
    fprintf(stderr, "%s:Before MAKENEWPAG\n", rn);
    printGroups();
    fprintf(stderr, "\nWEBLOG: before PAG:\t");
    printPAG();
#endif
    if (makeNewPAG()) {
	fprintf(stderr, "WEBLOG: MakeNewPAG failed\n");
    }
#ifdef DEBUG
    fprintf(stderr, "weblog:AFTER do_setpag,PAG:\t");
    printPAG();
    fprintf(stderr, "%s:After MAKENEWPAG\n", rn);
    printGroups();
#endif

    if (unlog()) {
#ifdef DEBUG
	fprintf(stderr, "WEBLOG: UNLOG FAILED\n");
#endif
    }

    while (!shutdown) {
	gotToken = 0;
	code = readFromClient(tbuffer);
	if (code > 0) {
	    tbuffer[code] = '\0';
	    code = parseBuf(tbuffer, name, passwd, cell, type);
	    if (code) {
		fprintf(stderr, "weblog: parseBuf FAILED\n");
		WEBLOGEXIT(PARSEERROR);
	    }
	} else {
#ifdef DEBUG
	    fprintf(stderr, "%s: readFromClient FAILED:%d...exiting\n", rn,
		    code);
#endif
	    WEBLOGEXIT(PIPEREADERROR);
	}

	if (strcasecmp(type, "AFS") == 0) {
	    authtype = 1;
	}
	else {
	    authtype = 0;
	}

	if (!authtype) {
	    reason = (char *)malloc(sizeof(tbuffer));
	    sprintf(reason, "weblog: Unknown Authentication type:%s.", type);
	    goto reply_failure;
	}

	memset((void *)&tbuffer, 0, sizeof(tbuffer));

	/* look up local cache */
	weblog_login_checksum(name, cell, passwd, cksum);
	code = weblog_login_lookup(name, cell, cksum, &tbuffer[0]);

	if (!code) {		/* local cache lookup failed */
	    /* authenticate user */

#ifdef DEBUG
	    fprintf(stderr, "WEBLOG GROUPCHECK BEFORE KA_AUTH\n");
	    printGroups();
#endif

	    if (authtype == 1) {
		code =
		    ka_UserAuthenticateGeneral(KA_USERAUTH_VERSION + 0, name,
					       NULL, cell, passwd, lifetime,
					       &password_expires, 0, &reason);
	    }

	    if (code) {
#ifdef DEBUG
		if (!Silent) {
		    fprintf(stderr,
			    "weblog:Unable to authenticate to AFS because "
			    "%s\n", reason);
		}
#endif
		goto reply_failure;
	    } else {
#ifdef DEBUG
		fprintf(stderr,
			"WEBLOG:After ka_UserAuthenticateGeneral GroupCheck\n");
		printGroups();
#endif
		/* get just the ONE token for this PAG from cache manager */
		i = 0;
		memcpy((void *)&tbuffer[0], (void *)&i, sizeof(afs_int32));
		code =
		    do_pioctl(tbuffer, sizeof(afs_int32), tbuffer,
			      sizeof(tbuffer), VIOCGETTOK, NULL, 0);
		if (code) {
		    fprintf(stderr, "weblog: failed to get token:%d\n", code);
		    strcpy(reason,
			   "FAILED TO GET TOKEN FROM CACHE MANAGER\n");
		} else {
		    gotToken = 1;

#ifdef DEBUG
		    hexDump(tbuffer, sizeof(tbuffer));
		    parseToken(tbuffer);
#endif /* _DEBUG */

		    /* put the token in local cache with the expiration date/time */
		    expires = getExpiration(tbuffer);
		    if (expires < 0) {
#ifdef DEBUG
			fprintf(stderr, "Error getting expiration time\n");
#endif
		    } else {
			weblog_login_checksum(name, cell, passwd, cksum);
			if (cacheExpiration == 0) {
			    weblog_login_store(name, cell, cksum, &tbuffer[0],
					       sizeof(tbuffer), expires);
			} else {
			    testExpires = cacheExpiration + time(NULL);
			    weblog_login_store(name, cell, cksum, &tbuffer[0],
					       sizeof(tbuffer), MIN(expires,
								    testExpires));
			}
		    }
		}
	    }
	} else {
	    /* cache lookup succesful */
#ifdef DEBUG
	    fprintf(stderr, "WEBLOG: Cache lookup succesful\n");
#endif
	    gotToken = 1;
	}

	/* prepare the reply buffer with this token */
	if (!gotToken) {

	  reply_failure:
	    /* respond with a reason why authentication failed */
	    sprintf(tbuffer, "FAILURE:%s", reason);
	}

	/* send response to client */
	code = sendToken(sizeof(tbuffer), tbuffer);
	if (code) {
#ifdef DEBUG
	    fprintf(stderr, "sendToken FAILED\n");
#endif
	    WEBLOGEXIT(PIPESENDERROR);
	}
	/* unlog after every request unconditionally */
	if (unlog()) {
#ifdef DEBUG
	    fprintf(stderr, "WEBLOG: UNLOG FAILED\n");
#endif
	}
    }				/* end - while */
    return 0;
}
