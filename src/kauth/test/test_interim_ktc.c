/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Test ktc related calls as well as some file access stuff. */

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <sys/file.h>
#include <arpa/inet.h>
#include <afs/prs_fs.h>
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/stds.h>
#include <afs/com_err.h>
#include <afs/cellconfig.h>
#include <afs/auth.h>
#include "kautils.h"


extern int errno;

static char *whoami = "test_interim_ktc";
static verbose = 1;
#define CELLSTOKEEP 50
static char cell[MAXKTCREALMLEN];
static char cellNameList[CELLSTOKEEP][MAXKTCREALMLEN];
static int nCells = 0;
static int unusedCell;
static char tester[MAXKTCNAMELEN];
static char testerPassword[32];
static long testerId;
static char remoteTester[MAXKTCNAMELEN];
static char remoteTesterPassword[32];
static long remoteTesterId;

static void
PrintPrincipal(stream, p)
     FILE *stream;
     struct ktc_principal *p;
{
    if (!verbose)
	return;
    fprintf(stream, "%s", p->name);
    if (strlen(p->instance))
	fprintf(stream, ".%s", p->instance);
    if (strlen(p->cell) && (strcmp(p->cell, ka_LocalCell()) != 0))
	fprintf(stream, "@%s", p->cell);
}

static void
PrintAuthentication(stream, s, t, c)
     FILE *stream;
     struct ktc_principal *s, *c;
     struct ktc_token *t;
{
    long lifetime;
    unsigned long now = time(0);
    char bob[KA_TIMESTR_LEN];

    lifetime = (unsigned long)t->endTime - (unsigned long)t->startTime;

    fprintf(stream, "Ticket for '");
    PrintPrincipal(stream, s);
    fprintf(stream, "'\n");
    ka_timestr(t->endTime, bob, KA_TIMESTR_LEN);
    fprintf(stream, "  good for %d seconds till %s", lifetime, bob);

    /* Allow one second grace, so the even/odd lifetime doesn't appear as an
     * error.  If it is way off complain loudly. */
    if (now + KTC_TIME_UNCERTAINTY < t->startTime)
	fprintf(stream, " [FUTURE]\n");
    else if (now + 1 < t->startTime)
	fprintf(stream, " [ahead]\n");
    else if (now >= t->endTime + KTC_TIME_UNCERTAINTY)
	fprintf(stream, " [EXPIRED]\n");
    else if (now >= t->endTime)
	fprintf(stream, " [behind]\n");
    else
	fprintf(stream, "\n");
    fprintf(stream, "  for user '");
    PrintPrincipal(stream, c);
    fprintf(stream, "'\n");
    fprintf(stream, "  session key='");
    ka_PrintBytes(&t->sessionKey, sizeof(t->sessionKey));
    fprintf(stream, "'\n");
    fprintf(stream, "  kvno=%d, ticket is %d bytes long.\n", t->kvno,
	    t->ticketLen);
    if (lifetime & 1) {
	fprintf(stream, "from %d to %d is %d seconds\n", t->startTime,
		t->endTime, lifetime);
	fprintf(stream, "Lifetime is odd\n");
    }
}

static int
CheckUnixUID(server, token, client)
     struct ktc_principal *server;
     struct ktc_token *token;
     struct ktc_principal *client;
{
    long code;
    struct ktc_token ntoken;
    struct ktc_principal nclient;
    long lifetime;
    char name_buf[24];

    code = ktc_SetToken(server, token, client, 0);
    if (code) {
	com_err(whoami, code, "using SetToken to set vice id");
	return 1;
    }
    sprintf(name_buf, "Unix UID %d", getuid());

    code = ktc_GetToken(server, &ntoken, sizeof(ntoken), &nclient);
    if (code || (strcmp(nclient.name, name_buf) != 0)
	|| strlen(nclient.instance)) {
	fprintf(stderr, "GetToken returned bad client: '");
	PrintPrincipal(stderr, &nclient);
	fprintf(stderr, "'\n");
	com_err(whoami, code, "should have gotten '%s'", name_buf);
	return 1;
    }
    lifetime =
	(unsigned long)ntoken.endTime - (unsigned long)ntoken.startTime;
    if ((lifetime & 1) == 1) {
	com_err(whoami, code, "GetToken returned even lifetime (%d)",
		lifetime);
	return 1;
    }
    return 0;
}

static int
CheckAFSId(server, token, client)
     struct ktc_principal *server;
     struct ktc_token *token;
     struct ktc_principal *client;
{
    long code;
    struct ktc_token ntoken;
    struct ktc_principal nclient;
    long lifetime;
    char name_buf[24];
    long viceId;

    viceId = atoi((client->name) + 7);
    code = ktc_SetToken(server, token, client, 0);
    if (code) {
	com_err(whoami, code, "using SetToken to set vice id to %d", viceId);
	return 1;
    }
    code = ktc_GetToken(server, &ntoken, sizeof(ntoken), &nclient);
    if ((strncmp(nclient.name, "AFS ID ", 7) != 0)
	|| (strlen(nclient.name) < 8) || (atoi(nclient.name + 7) != viceId)
	|| strlen(nclient.instance)) {
	fprintf(stderr, "GetToken returned bad client: '");
	PrintPrincipal(stderr, &nclient);
	fprintf(stderr, "' should have gotten '");
	PrintPrincipal(stderr, client);
	fprintf(stderr, "'\n");
	com_err(whoami, code, "didn't preserve AFS ID");
	return 1;
    }
    lifetime =
	(unsigned long)ntoken.endTime - (unsigned long)ntoken.startTime;
    if ((lifetime & 1) == 0) {
	com_err(whoami, code, "GetToken returned even lifetime (%d)",
		lifetime);
	return 1;
    }
    return 0;
}

#include <afs/venus.h>

/* Stolen from auth/comktc.c: U_CellSetLocalTokens */
/* Try various Auth2 style pioctl calls with kvno == 999 */

static int
CheckAuth2(server)
     struct ktc_principal *server;
{
    long code;
    int i;
    struct ViceIoctl buffer;	/*pioctl() communication block */
    struct ktc_principal client;
    struct ktc_token token;
    long lifetime;

    struct EncryptedSecretToken {
	char data[56];
    };
    typedef struct EncryptedSecretToken EncryptedSecretToken;

    struct ClearToken {
	long AuthHandle;
	struct ktc_encryptionKey HandShakeKey;
	long ViceId;
	long BeginTimestamp;
	long EndTimestamp;
    };
    typedef struct ClearToken ClearToken;

    /* venus buffer for using the new interface to set/get venus tokens */
    typedef struct {
	int sTokenSize;		/*Size in bytes of secret token */
	EncryptedSecretToken stoken;	/*Secret token */
	int cTokenSize;		/*Size in bytes of clear token */
	ClearToken ctoken;	/*Clear token */
	int isPrimary;		/*Is this the primary ID? */
	char cellName[64];	/*Cell in which tokens are valid */
    } venusbuff;
    venusbuff inbuff;
    ClearToken cToken;
    EncryptedSecretToken sToken;
    char *cellID;
    int primaryFlag;

    cellID = server->cell;
    primaryFlag = 0;

    /* First build a plausible clear token (code from old auth(2) server */
    cToken.AuthHandle = -1;	/* not in use right now */
    for (i = 0; i < sizeof(struct ktc_encryptionKey); i++)
	cToken.HandShakeKey.data[i] = random() & 0xff;
    cToken.BeginTimestamp = 0;
    cToken.EndTimestamp = time(0) + 60 * 60 * 25;	/* valid for 25 hours */
    cToken.ViceId = 123;

    /* Then invent secret token */
    for (i = 0; i < sizeof(sToken); i++)
	sToken.data[i] = random() & 0xff;


    /*Copy in the sizes and bodies of the secret and clear tokens */
    inbuff.sTokenSize = sizeof(EncryptedSecretToken);
    memcpy((char *)&inbuff.stoken, &sToken, sizeof(EncryptedSecretToken));
    inbuff.cTokenSize = sizeof(ClearToken);
    memcpy((char *)&inbuff.ctoken, &cToken, sizeof(ClearToken));

    /* Copy in the Primary ID flag and the cell name */
#if DB_CELLS
    fprintf(stderr,
	    "U_CellSetLocalTokens: using isPrimary=%d, cellName='%s'\n",
	    primaryFlag, cellID);
#endif /* DB_CELLS */
    inbuff.isPrimary = primaryFlag;
    strcpy(inbuff.cellName, cellID);

    /* Place our inbuff in the standard PIOCTL buffer and go for it. */
    buffer.in = (char *)&inbuff;
    buffer.out = 0;
    buffer.in_size = sizeof(inbuff);
    buffer.out_size = 0;
    code = pioctl(0, _VICEIOCTL(3), &buffer, 1);
    if (code) {
	com_err(whoami, errno, "setting old-style token");
	return 1;
    }

    /* now get it back and see if it's OK */
    code = ktc_GetToken(server, &token, sizeof(token), &client);
    if ((strcmp(client.name, "AFS ID 123") != 0) || strlen(client.instance)
	|| (strcmp(client.cell, cellID) != 0)) {
	fprintf(stderr, "GetToken returned bad client: '");
	PrintPrincipal(stderr, &client);
	fprintf(stderr, "'\n");
	return 1;
    }
    if ((token.kvno != 999) || (token.startTime != 0)
	|| (token.endTime != cToken.EndTimestamp)
	|| (token.ticketLen != sizeof(sToken))
	||
	(memcmp
	 (&cToken.HandShakeKey, &token.sessionKey,
	  sizeof(struct ktc_encryptionKey)) != 0)
	|| (memcmp(&sToken, token.ticket, sizeof(sToken)) != 0)) {
	fprintf(stdout, "Auth2 token was bad\n");
	PrintAuthentication(stdout, server, &token, &client);
	return 1;
    }

    return 0;

}

/* Stolen from the "fs" command. */

#define MAXNAME 100
#define	MAXSIZE	2048

static void
ListCellsCmd()
{
    register long code;
    long i, j;
    char *tcp;
    long clear;
    char space[MAXSIZE];
    struct ViceIoctl blob;

    for (i = 0; i < 1000; i++) {
	char *cellname;
	blob.out_size = MAXSIZE;
	blob.in_size = sizeof(long);
	blob.in = space;
	blob.out = space;
	memcpy(space, &i, sizeof(long));
	code = pioctl(0, VIOCGETCELL, &blob, 1);
	if (code < 0) {
	    if (errno == EDOM)
		break;		/* done with the list */
	    else {
		com_err(whoami, code, "getting cell list");
		exit(1);
	    }
	}
	cellname = space + 8 * sizeof(long);
	if (verbose > 1) {
	    printf("Cell %s on hosts", cellname);
	    for (j = 0; j < 8; j++) {
		memcpy(&clear, space + j * sizeof(long), sizeof(long));
		if (clear == 0)
		    break;
#if SLOW
		tcp = hostutil_GetNameByINet(clear);
#else
		tcp = inet_ntoa(clear);
#endif
		printf(" %s", tcp);
	    }
	    printf(".\n");
	}
	if (nCells < CELLSTOKEEP) {
	    strncpy(cellNameList[nCells++], cellname, MAXKTCREALMLEN);
	}
    }
    return;
}

/* Stolen from the fs command */

struct Acl {
    int nplus;
    int nminus;
    struct AclEntry *pluslist;
    struct AclEntry *minuslist;
};

struct AclEntry {
    struct AclEntry *next;
    char name[MAXNAME];
    long rights;
};

char *
AclToString(acl)
     struct Acl *acl;
{
    static char mydata[MAXSIZE];
    char tstring[MAXSIZE];
    struct AclEntry *tp;
    sprintf(mydata, "%d\n%d\n", acl->nplus, acl->nminus);
    for (tp = acl->pluslist; tp; tp = tp->next) {
	sprintf(tstring, "%s %d\n", tp->name, tp->rights);
	strcat(mydata, tstring);
    }
    for (tp = acl->minuslist; tp; tp = tp->next) {
	sprintf(tstring, "%s %d\n", tp->name, tp->rights);
	strcat(mydata, tstring);
    }
    return mydata;
}

char *
SkipLine(astr)
     register char *astr;
{
    while (*astr != '\n')
	astr++;
    astr++;
    return astr;
}

struct Acl *
ParseAcl(astr)
     char *astr;
{
    int nplus, nminus, i, trights;
    char tname[MAXNAME];
    struct AclEntry *first, *last, *tl;
    struct Acl *ta;
    sscanf(astr, "%d", &nplus);
    astr = SkipLine(astr);
    sscanf(astr, "%d", &nminus);
    astr = SkipLine(astr);

    ta = (struct Acl *)malloc(sizeof(struct Acl));
    ta->nplus = nplus;
    ta->nminus = nminus;

    last = 0;
    first = 0;
    for (i = 0; i < nplus; i++) {
	sscanf(astr, "%100s %d", tname, &trights);
	astr = SkipLine(astr);
	tl = (struct AclEntry *)malloc(sizeof(struct AclEntry));
	if (!first)
	    first = tl;
	strcpy(tl->name, tname);
	tl->rights = trights;
	tl->next = 0;
	if (last)
	    last->next = tl;
	last = tl;
    }
    ta->pluslist = first;

    last = 0;
    first = 0;
    for (i = 0; i < nminus; i++) {
	sscanf(astr, "%100s %d", tname, &trights);
	astr = SkipLine(astr);
	tl = (struct AclEntry *)malloc(sizeof(struct AclEntry));
	if (!first)
	    first = tl;
	strcpy(tl->name, tname);
	tl->rights = trights;
	tl->next = 0;
	if (last)
	    last->next = tl;
	last = tl;
    }
    ta->minuslist = first;

    return ta;
}

ZapList(alist)
     struct AclEntry *alist;
{
    register struct AclEntry *tp, *np;
    for (tp = alist; tp; tp = np) {
	np = tp->next;
	free(tp);
    }
}

int
PruneList(ae)
     struct AclEntry **ae;
{
    struct AclEntry **lp;
    struct AclEntry *te, *ne;
    long ctr;
    ctr = 0;
    lp = ae;
    for (te = *ae; te; te = ne) {
	if (te->rights == 0) {
	    *lp = te->next;
	    ne = te->next;
	    free(te);
	    ctr++;
	} else {
	    ne = te->next;
	    lp = &te->next;
	}
    }
    return ctr;
}

static int
AddTester(pathname)
     char *pathname;
{
    register long code;
    struct ViceIoctl blob;
    struct Acl *al;
    char space[MAXSIZE];

    blob.out_size = MAXSIZE;
    blob.in_size = 0;
    blob.out = space;
    code = pioctl(pathname, VIOCGETAL, &blob, 1);
    if (code) {
	com_err(whoami, errno, "getting acl for %s", pathname);
	return 1;
    }
    if (verbose > 1)
	printf("old acl for %s is %s\n", pathname, space);
    al = ParseAcl(space);

    {
	struct AclEntry *tlist;
	struct AclEntry *ae;
	/* clean acl up a bit */
	ZapList(al->minuslist);
	al->minuslist = 0;
	al->nminus = 0;
	for (ae = al->pluslist; ae; ae = ae->next) {
	    if ((strcmp(ae->name, tester) == 0) ||
		/* punt useless entries (like system:anyuser) */
		!(ae->
		  rights & (PRSFS_INSERT | PRSFS_DELETE | PRSFS_WRITE |
			    PRSFS_LOCK | PRSFS_ADMINISTER)))
		ae->rights = 0;
	}
	al->nplus -= PruneList(&al->pluslist);

	tlist = (struct AclEntry *)malloc(sizeof(struct AclEntry));
	tlist->rights = 9;
	strcpy(tlist->name, tester);
	tlist->next = al->pluslist;
	al->pluslist = tlist;
	al->nplus++;
    }

    if (verbose > 1) {
	char tmp[100];
	printf("before:\n");
	sprintf(tmp, "fs la %s", pathname);
	system(tmp);
    }

    blob.in = AclToString(al);
    blob.out_size = 0;
    blob.in_size = 1 + strlen(blob.in);
    code = pioctl(pathname, VIOCSETAL, &blob, 1);
    if (code) {
	com_err(whoami, errno, "setting acl on %s to %s", pathname, blob.in);
	return 1;
    }
    if (verbose > 1) {
	char tmp[100];
	printf("after:\n");
	sprintf(tmp, "fs la %s", pathname);
	system(tmp);
    }
    return 0;
}

/* Free this cell for reuse */
static void
FreeUnusedCell()
{
    struct ktc_principal client, server;
    struct ktc_token token;
    long code;

    strcpy(server.name, "afs");
    strcpy(server.instance, "");
    strcpy(server.cell, cellNameList[unusedCell]);

    token.ticketLen = MINKTCTICKETLEN;
    token.endTime = 0;
    code = ktc_SetToken(&server, &token, &client, 0);
    if (code) {
	com_err(whoami, code, "freeing cell");
	exit(1);
    }
}

static int
TryAuthenticating(name, password, viceId, cell)
     char *name;
     char *password;
     long viceId;
     char *cell;
{
    long code;
    char *reason;
    struct ktc_principal server, client;
    struct ktc_token token;
    long lifetime;
    unsigned long now = time(0);

    code = ka_UserAuthenticate(name, "", cell, password, 0, &reason);
    if (code) {
	fprintf(stderr, "unable to authenticate as %s because %s\n", name,
		reason);
	return 1;
    }
    strcpy(server.name, "afs");
    strcpy(server.instance, "");
    strcpy(server.cell, cell);
    code = ktc_GetToken(&server, &token, sizeof(token), &client);
    if (code) {
	com_err(whoami, code, "so couldn't get %s's afs token in %s", name,
		cell);
	return code;
    }
    if (code = tkt_CheckTimes(token.startTime, token.endTime, now) != 2) {
	fprintf(stdout, "Bad times on afs ticket\n");
	PrintAuthentication(stdout, &server, &token, &client);
	return code;
    }

    lifetime = token.endTime - token.startTime;
    if ((lifetime & 1) == 0) {
	fprintf(stderr, "*** Old style KTC (in cell %s) ***\n", cell);
    } else {
	char tmp[24];
	sprintf(tmp, "AFS ID %d", viceId);
	if ((strcmp(client.name, tmp) != 0) || strlen(client.instance)) {
	    fprintf(stderr, "GetToken returned bad client: '");
	    PrintPrincipal(stderr, &client);
	    fprintf(stderr, "' should have gotten AFS ID %d for %s@%s\n",
		    viceId, name, cell);
	    return 1;
	}
    }
    if (verbose) {
	fprintf(stdout, "%s@%s using ", name, cell);
	PrintAuthentication(stdout, &server, &token, &client);
    }
    return 0;
}

static long
CheckAFSTickets()
{
    long code;
    struct ktc_principal client, server;
    struct ktc_token token;
    struct ktc_principal nclient;
    struct ktc_token ntoken;
    long lifetime;
    long exitCode = 0;
    char *reason;
    char *tdpath = "./tester_dir";
    char *tfpath = "./tester_dir/touch";
    int fd;
    unsigned long now = time(0);

    strcpy(server.name, "afs");
    strcpy(server.instance, "");
    strcpy(server.cell, ka_LocalCell());
    code = ktc_GetToken(&server, &token, sizeof(token), &client);
    if (code) {
	com_err(whoami, code, "so couldn't get afs token");
	return code;
    }

    code = mkdir(tdpath, 0777);
    if (code && (errno != EEXIST)) {
	com_err(whoami, errno, "making test dir %s", tdpath);
	return code;
    }
    fd = open(tfpath, O_WRONLY + O_CREAT + O_TRUNC, 0777);
    if (fd == -1) {
	com_err(whoami, errno, "making test file %s", tfpath);
	goto failed;
    }
    code = close(fd);
    if (code) {
	com_err(whoami, errno, "failed to close %s after create", tfpath);
	goto failed;
    }

    code = AddTester(tdpath);
    if (code)
	goto failed;

    setpag();

    if (TryAuthenticating(tester, testerPassword, testerId, server.cell))
	goto failed;
    code = ktc_GetToken(&server, &ntoken, sizeof(ntoken), &nclient);
    if (code) {
	com_err(whoami, code, "getting new local afs token");
	goto failed;
    }

    if (remoteTesterId) {	/* make sure remote cells work also */
	if (TryAuthenticating
	    (remoteTester, remoteTesterPassword, remoteTesterId, cell))
	    goto failed;
    }

    ktc_ForgetToken(&server);	/* switch to unauthenticated */

    /* check to see if remote ticket disappears also. */
    {
	struct ktc_principal remote;
	strcpy(remote.name, "afs");
	strcpy(remote.instance, "");
	strcpy(remote.cell, cell);
	code = ktc_GetToken(&remote, 0, 0, 0);
	if (code == KTC_NOENT)
	    fprintf(stdout, "*** Using interim KTC ***\n");
	else
	    fprintf(stdout, "Using kernel ticket cache\n");
    }

    code = open(tfpath, O_RDONLY, 0);	/* check for read access */
    if (!((code == -1) && ((errno == ENOENT) || (errno == EACCES)))) {
	com_err(whoami, errno, "didn't fail to open %s for read", tfpath);
	goto failed;
    }

    /* as tester we should have read but not write */
    code = ktc_SetToken(&server, &ntoken, &nclient, 0);
    if (code) {
	com_err(whoami, code, "restoring new local afs token");
	goto failed;
    }
    code = open(tfpath, O_RDWR + O_TRUNC, 0);
    if ((code != -1) || (errno != EACCES)) {
	com_err(whoami, errno, "didn't fail to open %s for write", tfpath);
	goto failed;
    }
    fd = open(tfpath, O_RDONLY, 0);
    if (fd == -1) {
	com_err(whoami, errno, "failed to open %s for read", tfpath);
	goto failed;
    }
    code = close(fd);
    if (code) {
	com_err(whoami, errno, "failed to close %s after open", tfpath);
	goto failed;
    }

  finish:
    /* go back to original privileges */
    code = ktc_SetToken(&server, &token, &client, 0);
    if (code) {
	com_err(whoami, code, "so couldn't set afs token in new pag");
	exit(1);
    }
    if (unlink(tfpath) || rmdir(tdpath)) {
	com_err(whoami, errno, "removing test dir %s", tdpath);
	return 1;
    }
    return exitCode;

  failed:
    exitCode = 1;
    goto finish;
}

main(argc, argv)
     int argc;
     char *argv[];
{
    int i;
    long code;
    struct ktc_principal client, server;
    struct ktc_token token;
    struct ktc_principal nclient, nnclient;
    struct ktc_token ntoken;
    long lifetime;
    long viceId;
    int printToken = 0;		/* just print afs @ remoteCell */

    srandom(1);

    /* Initialize com_err error code hacking */
    initialize_U_error_table();
    initialize_KA_error_table();
    initialize_RXK_error_table();
    initialize_KTC_error_table();
    initialize_ACFG_error_table();
    initialize_rx_error_table();

    /* set defaults */
    strcpy(cell, "");
    strcpy(tester, "tester");
    strcpy(testerPassword, "xxx");
    testerId = 1031;
    remoteTesterId = 0;		/* don't try this */

    /* parse arguments */
    i = 1;
    while (i < argc) {
	int arglen = strlen(argv[i]);
	char arg[256];
	lcstring(arg, argv[i], sizeof(arg));
#define IsArg(a) (strncmp (arg,a, arglen) == 0)
	if (IsArg("-quiet"))
	    verbose--;
	else if (IsArg("-verbose"))
	    verbose++;
	else if (IsArg("-printtoken"))
	    printToken++;
	else if (IsArg("-remotecell"))
	    strncpy(cell, argv[++i], sizeof(cell));
	else if (IsArg("-testid"))
	    testerId = atoi(argv[++i]);
	else if (IsArg("-tester"))
	    strncpy(tester, argv[++i], sizeof(tester));
	else if (IsArg("-testpassword"))
	    strncpy(testerPassword, argv[++i], sizeof(testerPassword));
	else if (IsArg("-remotetestid"))
	    remoteTesterId = atoi(argv[++i]);
	else if (IsArg("-remotetester"))
	    strncpy(remoteTester, argv[++i], sizeof(tester));
	else if (IsArg("-remotetestpassword"))
	    strncpy(remoteTesterPassword, argv[++i],
		    sizeof(remoteTesterPassword));
	else {
	    fprintf(stderr, "unexpected arg '%s'\n", arg);
	  usage:
	    fprintf(stderr,
		    "Usage is: '%s [-quiet] [-verbose] [-remotecell <cellname>] [-testid <AFS ID>] [-tester <name>] -testpassword <pass> [-remotetestid <AFS ID> -remotetester <name> -remotetestpassword <pass>]\n",
		    whoami);
	    exit(1);
	}
	i++;
    }

    /* get list of cells */
    ListCellsCmd();

    /* expand requested cell name */
    code = ka_CellConfig(AFSCONF_CLIENTNAME);
    if (code)
	com_err(whoami, code, "calling cell config");
    code = ka_ExpandCell(cell, cell, 0);
    if (code) {
	com_err(whoami, code, "expanding cell %s", cell);
	exit(1);
    }

    strcpy(server.name, "afs");
    strcpy(server.instance, "");
    strcpy(server.cell, cell);
    if (printToken) {
	code = ktc_GetToken(&server, &token, sizeof(token), &client);
	if (code) {
	    com_err(whoami, code, "so couldn't get afs token");
	    exit(1);
	}
	PrintAuthentication(stdout, &server, &token, &client);
    } else {			/* dummy up a token */
	token.startTime = time(0);
	token.endTime = token.startTime + 3600;
	token.kvno = 1;
	token.ticketLen = 48;
    }

    /* find a cell w/o tokens */
    for (i = 0; i < nCells; i++) {
	strcpy(server.cell, cellNameList[i]);
	code = ktc_GetToken(&server, &ntoken, sizeof(ntoken), &nclient);
	if ((code == KTC_NOENT) || ((code == 0) && (ntoken.endTime == 0)))
	    goto unused_cell_found;
    }
    fprintf(stderr, "All cells have tokens\n");
  unused_cell_found:
    unusedCell = i;
    if (verbose)
	printf("Using unused cell %s\n", cellNameList[unusedCell]);

    /* First check for various pathological cases */

    strcpy(server.cell, "foo.bar.baz");
    memcpy(&ntoken, &token, sizeof(ntoken));
    code = ktc_SetToken(&server, &ntoken, &client, 0);
    if (code != KTC_NOCELL) {
	com_err(whoami, code,
		"should have gotten bad pioctl error calling SetToken with bogus cell name");
	goto failed;
    }
    strcpy(server.cell, cellNameList[unusedCell]);

    ntoken.ticketLen = 0;
    code = ktc_SetToken(&server, &ntoken, &client, 0);
    if ((code != KTC_TOOBIG) && (code != KTC_PIOCTLFAIL)) {
	com_err(whoami, code,
		"should have gotten error calling SetToken with zero ticket length");
	goto failed;
    }
    ntoken.ticketLen = token.ticketLen;
    ntoken.endTime = 0;
    code = ktc_SetToken(&server, &ntoken, &client, 0);
    if (code) {
	com_err(whoami, code, "calling SetToken with zero expiration time");
	goto failed;
    }
    strcpy(nclient.name, "foo");
    strcpy(nclient.instance, "bar");
    strcpy(nclient.cell, "foo.bar.baz");
    code = ktc_SetToken(&server, &ntoken, &nclient, 0);
    if (code) {
	com_err(whoami, code, "calling SetToken with bogus client cell");
	goto failed;
    }
    memcpy(&ntoken, &token, sizeof(ntoken));
    if (token.kvno == 999)
	ntoken.kvno = 99;

    /* Now check out SetToken parsing of specially formed names */

    strcpy(nclient.instance, "");
    /* cell is uniformly ignored */
    viceId = 123;
    sprintf(nclient.name, "AFS ID %d", viceId);
    if (CheckAFSId(&server, &ntoken, &nclient))
	goto failed;
    ntoken.endTime--;
    if (CheckAFSId(&server, &ntoken, &nclient))
	goto failed;
    sprintf(nclient.name, "AFS ID 0%d", viceId);
    if (CheckAFSId(&server, &ntoken, &nclient))
	goto failed;
#if 1
    sprintf(nclient.name, "AFS ID %d", -44444);
    if (CheckAFSId(&server, &ntoken, &nclient))
	goto failed;
#endif
    sprintf(nclient.name, "AFS ID %d", 0x7fffffff);
    if (CheckAFSId(&server, &ntoken, &nclient))
	goto failed;

    sprintf(nclient.name, "AFS ID ");
    if (CheckUnixUID(&server, &ntoken, &nclient))
	goto failed;
    sprintf(nclient.name, "AFS ID 10x");
    if (CheckUnixUID(&server, &ntoken, &nclient))
	goto failed;
    sprintf(nclient.name, "foobar");
    if (CheckUnixUID(&server, &ntoken, &nclient))
	goto failed;
    ntoken.endTime--;
    if (CheckUnixUID(&server, &ntoken, &nclient))
	goto failed;

    /* make sure simulated auth2 tokens still does something reasonable */
    if (CheckAuth2(&server))
	goto failed;

    FreeUnusedCell();

    code = CheckAFSTickets();	/* returns in new PAG */
    if (code)
	exit(1);

    rx_Finalize();

    printf("All OK\n");
    exit(0);

  failed:
    FreeUnusedCell();
    exit(1);
}
