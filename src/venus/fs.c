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

#include <roken.h>
#include <ctype.h>
#include <assert.h>

#include <afs/afs_consts.h>
#include <afs/afs_args.h>
#include <rx/xdr.h>
#include <afs/vice.h>
#include <afs/venus.h>
#include <afs/com_err.h>
#include <afs/afs_consts.h>

#undef VIRTUE
#undef VICE
#include "afs/prs_fs.h"
#include <afs/afsint.h>
#include <afs/cellconfig.h>
#include <ubik.h>
#include <rx/rxkad.h>
#include <rx/rx_globals.h>
#include <afs/vldbint.h>
#include <afs/volser.h>
#include <afs/vlserver.h>
#include <afs/cmd.h>
#include <afs/com_err.h>
#include <afs/ptclient.h>
#include <afs/ptuser.h>
#include <afs/afsutil.h>
#include <afs/sys_prototypes.h>

#define MAXNAME 100
#define MAXINSIZE 1300		/* pioctl complains if data is larger than this */
#define VMSGSIZE 128		/* size of msg buf in volume hdr */

static char space[AFS_PIOCTL_MAXSIZE];
static char tspace[1024];
static struct ubik_client *uclient;

static int GetClientAddrsCmd(struct cmd_syndesc *, void *);
static int SetClientAddrsCmd(struct cmd_syndesc *, void *);
static int FlushMountCmd(struct cmd_syndesc *, void *);
static int RxStatProcCmd(struct cmd_syndesc *, void *);
static int RxStatPeerCmd(struct cmd_syndesc *, void *);
static int GetFidCmd(struct cmd_syndesc *, void *);
static int UuidCmd(struct cmd_syndesc *, void *);

static char pn[] = "fs";
static int rxInitDone = 0;

struct AclEntry;
struct Acl;
static void ZapList(struct AclEntry *);
static int PruneList(struct AclEntry **, int);
static int CleanAcl(struct Acl *, char *);
static int SetVolCmd(struct cmd_syndesc *as, void *arock);
static int GetCellName(char *, char *, size_t);
static void Die(int, char *);

/*
 * Character to use between name and rights in printed representation for
 * DFS ACL's.
 */
#define DFS_SEPARATOR	' '

typedef char sec_rgy_name_t[1025];	/* A DCE definition */

struct Acl {
    int dfs;			/* Originally true if a dfs acl; now also the type
				 * of the acl (1, 2, or 3, corresponding to object,
				 * initial dir, or initial object). */
    sec_rgy_name_t cell;	/* DFS cell name */
    int nplus;
    int nminus;
    struct AclEntry *pluslist;
    struct AclEntry *minuslist;
};

struct AclEntry {
    struct AclEntry *next;
    char name[MAXNAME];
    afs_int32 rights;
};

struct vcxstat2 {
    afs_int32 callerAccess;
    afs_int32 cbExpires;
    afs_int32 anyAccess;
    char mvstat;
};

static void
ZapAcl(struct Acl *acl)
{
    if (!acl)
	return;
    ZapList(acl->pluslist);
    ZapList(acl->minuslist);
    free(acl);
}

static int
foldcmp(char *a, char *b)
{
    char t, u;
    while (1) {
	t = *a++;
	u = *b++;
	if (t >= 'A' && t <= 'Z')
	    t += 0x20;
	if (u >= 'A' && u <= 'Z')
	    u += 0x20;
	if (t != u)
	    return 1;
	if (t == 0)
	    return 0;
    }
}

/*
 * Mods for the AFS/DFS protocol translator.
 *
 * DFS rights. It's ugly to put these definitions here, but they
 * *cannot* change, because they're part of the wire protocol.
 * In any event, the protocol translator will guarantee these
 * assignments for AFS cache managers.
 */
#define DFS_READ          0x01
#define DFS_WRITE         0x02
#define DFS_EXECUTE       0x04
#define DFS_CONTROL       0x08
#define DFS_INSERT        0x10
#define DFS_DELETE        0x20

/* the application definable ones (backwards from AFS) */
#define DFS_USR0 0x80000000	/* "A" bit */
#define DFS_USR1 0x40000000	/* "B" bit */
#define DFS_USR2 0x20000000	/* "C" bit */
#define DFS_USR3 0x10000000	/* "D" bit */
#define DFS_USR4 0x08000000	/* "E" bit */
#define DFS_USR5 0x04000000	/* "F" bit */
#define DFS_USR6 0x02000000	/* "G" bit */
#define DFS_USR7 0x01000000	/* "H" bit */
#define DFS_USRALL	(DFS_USR0 | DFS_USR1 | DFS_USR2 | DFS_USR3 |\
			 DFS_USR4 | DFS_USR5 | DFS_USR6 | DFS_USR7)

/*
 * Offset of -id switch in command structure for various commands.
 * The -if switch is the next switch always.
 */
static int parm_setacl_id, parm_copyacl_id, parm_listacl_id;

/*
 * Determine whether either the -id or -if switches are present, and
 * return 0, 1 or 2, as appropriate. Abort if both switches are present.
 */
/*    int id;	Offset of -id switch; -if is next switch */
static int
getidf(struct cmd_syndesc *as, int id)
{
    int idf = 0;

    if (as->parms[id].items) {
	idf |= 1;
    }
    if (as->parms[id + 1].items) {
	idf |= 2;
    }
    if (idf == 3) {
	fprintf(stderr,
		"%s: you may specify either -id or -if, but not both switches\n",
		pn);
	exit(1);
    }
    return idf;
}

static int
PRights(afs_int32 arights, int dfs)
{
    if (!dfs) {
	if (arights & PRSFS_READ)
	    printf("r");
	if (arights & PRSFS_LOOKUP)
	    printf("l");
	if (arights & PRSFS_INSERT)
	    printf("i");
	if (arights & PRSFS_DELETE)
	    printf("d");
	if (arights & PRSFS_WRITE)
	    printf("w");
	if (arights & PRSFS_LOCK)
	    printf("k");
	if (arights & PRSFS_ADMINISTER)
	    printf("a");
	if (arights & PRSFS_USR0)
	    printf("A");
	if (arights & PRSFS_USR1)
	    printf("B");
	if (arights & PRSFS_USR2)
	    printf("C");
	if (arights & PRSFS_USR3)
	    printf("D");
	if (arights & PRSFS_USR4)
	    printf("E");
	if (arights & PRSFS_USR5)
	    printf("F");
	if (arights & PRSFS_USR6)
	    printf("G");
	if (arights & PRSFS_USR7)
	    printf("H");
    } else {
	if (arights & DFS_READ)
	    printf("r");
	else
	    printf("-");
	if (arights & DFS_WRITE)
	    printf("w");
	else
	    printf("-");
	if (arights & DFS_EXECUTE)
	    printf("x");
	else
	    printf("-");
	if (arights & DFS_CONTROL)
	    printf("c");
	else
	    printf("-");
	if (arights & DFS_INSERT)
	    printf("i");
	else
	    printf("-");
	if (arights & DFS_DELETE)
	    printf("d");
	else
	    printf("-");
	if (arights & (DFS_USRALL))
	    printf("+");
	if (arights & DFS_USR0)
	    printf("A");
	if (arights & DFS_USR1)
	    printf("B");
	if (arights & DFS_USR2)
	    printf("C");
	if (arights & DFS_USR3)
	    printf("D");
	if (arights & DFS_USR4)
	    printf("E");
	if (arights & DFS_USR5)
	    printf("F");
	if (arights & DFS_USR6)
	    printf("G");
	if (arights & DFS_USR7)
	    printf("H");
    }
    return 0;
}

/* this function returns TRUE (1) if the file is in AFS, otherwise false (0) */
static int
InAFS(char *apath)
{
    struct ViceIoctl blob;
    afs_int32 code;

    blob.in_size = 0;
    blob.out_size = AFS_PIOCTL_MAXSIZE;
    blob.out = space;

    code = pioctl(apath, VIOC_FILE_CELL_NAME, &blob, 1);
    if (code) {
	if ((errno == EINVAL) || (errno == ENOENT))
	    return 0;
    }
    return 1;
}

/* return a static pointer to a buffer */
static char *
Parent(char *apath)
{
    char *tp;
    strlcpy(tspace, apath, sizeof(tspace));
    tp = strrchr(tspace, '/');
    if (tp == (char *)tspace)
	tp++;
    else if (tp == NULL) {
	tp      = (char *)tspace;
	*(tp++) = '.';
    }
    *tp = '\0';
    return tspace;
}

                                /* added relative add resp. delete    */
                                /* (so old add really means to set)   */
enum rtype { add, destroy, deny, reladd, reldel };

static afs_int32
Convert(char *arights, int dfs, enum rtype *rtypep)
{
    afs_int32 mode;
    char tc;
    char *tcp;                  /* to walk through the rights string  */

    *rtypep = add;		/* add rights, by default */

                                /* analyze last character of string   */
    tcp = arights + strlen(arights);
    if ( tcp-- > arights ) {    /* assure non-empty string            */
        if ( *tcp == '+' )
            *rtypep = reladd;   /* '+' indicates more rights          */
        else if ( *tcp == '-' )
            *rtypep = reldel;   /* '-' indicates less rights          */
        else if ( *tcp == '=' )
            *rtypep = add;      /* '=' also allows old behaviour      */
        else
            tcp++;              /* back to original null byte         */
        *tcp = '\0';            /* do not disturb old strcmp-s        */
    }

    if (dfs) {
	if (!strcmp(arights, "null")) {
	    *rtypep = deny;
	    return 0;
	}
	if (!strcmp(arights, "read"))
	    return DFS_READ | DFS_EXECUTE;
	if (!strcmp(arights, "write"))
	    return DFS_READ | DFS_EXECUTE | DFS_INSERT | DFS_DELETE |
		DFS_WRITE;
	if (!strcmp(arights, "all"))
	    return DFS_READ | DFS_EXECUTE | DFS_INSERT | DFS_DELETE |
		DFS_WRITE | DFS_CONTROL;
    } else {
	if (!strcmp(arights, "read"))
	    return PRSFS_READ | PRSFS_LOOKUP;
	if (!strcmp(arights, "write"))
	    return PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE |
		PRSFS_WRITE | PRSFS_LOCK;
	if (!strcmp(arights, "mail"))
	    return PRSFS_INSERT | PRSFS_LOCK | PRSFS_LOOKUP;
	if (!strcmp(arights, "all"))
	    return PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE |
		PRSFS_WRITE | PRSFS_LOCK | PRSFS_ADMINISTER;
    }
    if (!strcmp(arights, "none")) {
	*rtypep = destroy;	/* Remove entire entry */
	return 0;
    }
    mode = 0;
    tcp = arights;
    while ((tc = *tcp++ )) {
	if (dfs) {
	    if (tc == '-')
		continue;
	    else if (tc == 'r')
		mode |= DFS_READ;
	    else if (tc == 'w')
		mode |= DFS_WRITE;
	    else if (tc == 'x')
		mode |= DFS_EXECUTE;
	    else if (tc == 'c')
		mode |= DFS_CONTROL;
	    else if (tc == 'i')
		mode |= DFS_INSERT;
	    else if (tc == 'd')
		mode |= DFS_DELETE;
	    else if (tc == 'A')
		mode |= DFS_USR0;
	    else if (tc == 'B')
		mode |= DFS_USR1;
	    else if (tc == 'C')
		mode |= DFS_USR2;
	    else if (tc == 'D')
		mode |= DFS_USR3;
	    else if (tc == 'E')
		mode |= DFS_USR4;
	    else if (tc == 'F')
		mode |= DFS_USR5;
	    else if (tc == 'G')
		mode |= DFS_USR6;
	    else if (tc == 'H')
		mode |= DFS_USR7;
	    else {
		fprintf(stderr, "%s: illegal DFS rights character '%c'.\n",
			pn, tc);
		exit(1);
	    }
	} else {
	    if (tc == 'r')
		mode |= PRSFS_READ;
	    else if (tc == 'l')
		mode |= PRSFS_LOOKUP;
	    else if (tc == 'i')
		mode |= PRSFS_INSERT;
	    else if (tc == 'd')
		mode |= PRSFS_DELETE;
	    else if (tc == 'w')
		mode |= PRSFS_WRITE;
	    else if (tc == 'k')
		mode |= PRSFS_LOCK;
	    else if (tc == 'a')
		mode |= PRSFS_ADMINISTER;
	    else if (tc == 'A')
		mode |= PRSFS_USR0;
	    else if (tc == 'B')
		mode |= PRSFS_USR1;
	    else if (tc == 'C')
		mode |= PRSFS_USR2;
	    else if (tc == 'D')
		mode |= PRSFS_USR3;
	    else if (tc == 'E')
		mode |= PRSFS_USR4;
	    else if (tc == 'F')
		mode |= PRSFS_USR5;
	    else if (tc == 'G')
		mode |= PRSFS_USR6;
	    else if (tc == 'H')
		mode |= PRSFS_USR7;
	    else {
		fprintf(stderr, "%s: illegal rights character '%c'.\n", pn,
			tc);
		exit(1);
	    }
	}
    }
    return mode;
}

static struct AclEntry *
FindList(struct AclEntry *alist, char *aname)
{
    while (alist) {
	if (!foldcmp(alist->name, aname))
	    return alist;
	alist = alist->next;
    }
    return 0;
}

/* if no parm specified in a particular slot, set parm to be "." instead */
static void
SetDotDefault(struct cmd_item **aitemp)
{
    struct cmd_item *ti;
    if (*aitemp)
	return;			/* already has value */
    /* otherwise, allocate an item representing "." */
    ti = malloc(sizeof(struct cmd_item));
    assert(ti);
    ti->next = (struct cmd_item *)0;
    ti->data = malloc(2);
    assert(ti->data);
    strcpy(ti->data, ".");
    *aitemp = ti;
}

static void
ChangeList(struct Acl *al, afs_int32 plus, char *aname, afs_int32 arights,
	   enum rtype *artypep)
{
    struct AclEntry *tlist;
    tlist = (plus ? al->pluslist : al->minuslist);
    tlist = FindList(tlist, aname);
    if (tlist) {
	/* Found the item already in the list.
	 * modify rights in case of reladd and reladd only,
	 * use standard - add, ie. set - otherwise
	 */
        if ( artypep == NULL )
            tlist->rights = arights;
        else if ( *artypep == reladd )
            tlist->rights |= arights;
        else if ( *artypep == reldel )
            tlist->rights &= ~arights;
        else
            tlist->rights = arights;

	if (plus)
	    al->nplus -= PruneList(&al->pluslist, al->dfs);
	else
	    al->nminus -= PruneList(&al->minuslist, al->dfs);
	return;
    }
    if ( artypep != NULL && *artypep == reldel )
        return;                 /* can't reduce non-existing rights   */

    /* Otherwise we make a new item and plug in the new data. */
    tlist = malloc(sizeof(struct AclEntry));
    assert(tlist);
    strcpy(tlist->name, aname);
    tlist->rights = arights;
    if (plus) {
	tlist->next = al->pluslist;
	al->pluslist = tlist;
	al->nplus++;
	if (arights == 0 || arights == -1)
	    al->nplus -= PruneList(&al->pluslist, al->dfs);
    } else {
	tlist->next = al->minuslist;
	al->minuslist = tlist;
	al->nminus++;
	if (arights == 0)
	    al->nminus -= PruneList(&al->minuslist, al->dfs);
    }
}

static void
ZapList(struct AclEntry *alist)
{
    struct AclEntry *tp, *np;
    for (tp = alist; tp; tp = np) {
	np = tp->next;
	free(tp);
    }
}

static int
PruneList(struct AclEntry **ae, int dfs)
{
    struct AclEntry **lp;
    struct AclEntry *te, *ne;
    afs_int32 ctr;
    ctr = 0;
    lp = ae;
    for (te = *ae; te; te = ne) {
	if ((!dfs && te->rights == 0) || te->rights == -1) {
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

static char *
SkipLine(char *astr)
{
    while (*astr != '\0' && *astr != '\n')
	astr++;
    if (*astr == '\n')
	astr++;
    return astr;
}

/*
 * Create an empty acl, taking into account whether the acl pointed
 * to by astr is an AFS or DFS acl. Only parse this minimally, so we
 * can recover from problems caused by bogus ACL's (in that case, always
 * assume that the acl is AFS: for DFS, the user can always resort to
 * acl_edit, but for AFS there may be no other way out).
 */
static struct Acl *
EmptyAcl(char *astr)
{
    struct Acl *tp;
    int junk;

    tp = calloc(sizeof(*tp), 1);
    assert(tp);
    tp->nplus = tp->nminus = 0;
    tp->pluslist = tp->minuslist = 0;
    tp->dfs = 0;
    sscanf(astr, "%d dfs:%d %1024s", &junk, &tp->dfs, tp->cell);
    return tp;
}

static struct Acl *
ParseAcl(char *astr)
{
    int nplus = 0, nminus = 0, i, trights = 0;
    char tname[MAXNAME + 1] = "";
    struct AclEntry *first, *last, *tl;
    struct Acl *ta;

    ta = calloc(sizeof(*ta), 1);
    assert(ta);
    ta->dfs = 0;
    sscanf(astr, "%d dfs:%d %1024s", &ta->nplus, &ta->dfs, ta->cell);
    astr = SkipLine(astr);
    sscanf(astr, "%d", &ta->nminus);
    astr = SkipLine(astr);

    nplus = ta->nplus;
    nminus = ta->nminus;

    last = 0;
    first = 0;
    for (i = 0; i < nplus; i++) {
	sscanf(astr, "%99s %d", tname, &trights);
	astr = SkipLine(astr);
	tl = calloc(sizeof(*tl), 1);
	assert(tl);
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
	sscanf(astr, "%99s %d", tname, &trights);
	astr = SkipLine(astr);
	tl = calloc(sizeof(*tl), 1);
	assert(tl);
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

static int
PrintStatus(VolumeStatus * status, char *name, char *offmsg)
{
    printf("Volume status for vid = %u named %s\n", status->Vid, name);
    if (*offmsg != 0)
	printf("Current offline message is %s\n", offmsg);
    printf("Current disk quota is ");
    if (status->MaxQuota != 0)
	printf("%d\n", status->MaxQuota);
    else
	printf("unlimited\n");
    printf("Current blocks used are %d\n", status->BlocksInUse);
    printf("The partition has %d blocks available out of %d\n\n",
	   status->PartBlocksAvail, status->PartMaxBlocks);
    return 0;
}

static const char power_letter[] = {
  'K',  /* kibi */
  'M',  /* mebi */
  'G',  /* gibi */
  'T',  /* tebi */
  'P',  /* pebi */
};

static void
HumanPrintSpace(afs_int32 int_space)
{
    int exponent = 0;
    int exponent_max = sizeof(power_letter) - 1;
    float space = int_space;

    while (space >= 1024 && exponent < exponent_max) {
	exponent++;
	space /= 1024;
    }
    printf("%9.1f%c", space, power_letter[exponent]);
}

static int
QuickPrintStatus(VolumeStatus * status, char *name, int human)
{
    double QuotaUsed = 0.0;
    double PartUsed = 0.0;
    int WARN = 0;
    printf("%-25.25s", name);

    if (status->MaxQuota != 0) {
	if (human) {
	    printf(" ");
	    HumanPrintSpace(status->MaxQuota);
	    printf(" ");
	    HumanPrintSpace(status->BlocksInUse);
	}
	else
	    printf(" %10d %10d", status->MaxQuota, status->BlocksInUse);
	QuotaUsed =
	    ((((double)status->BlocksInUse) / status->MaxQuota) * 100.0);
    } else {
	printf("   no limit ");
	if (human)
	    HumanPrintSpace(status->BlocksInUse);
	else
	    printf("%10d", status->BlocksInUse);
    }
    if (QuotaUsed > 90.0) {
	printf("%5.0f%%<<", QuotaUsed);
	WARN = 1;
    } else
	printf("%5.0f%%  ", QuotaUsed);
    PartUsed =
	(100.0 -
	 ((((double)status->PartBlocksAvail) / status->PartMaxBlocks) *
	  100.0));
    if (PartUsed > 97.0) {
	printf("%9.0f%%<<", PartUsed);
	WARN = 1;
    } else
	printf("%9.0f%%  ", PartUsed);
    if (WARN) {
	printf("  <<WARNING\n");
    } else
	printf("\n");
    return 0;
}

static int
QuickPrintSpace(VolumeStatus * status, char *name, int human)
{
    double PartUsed = 0.0;
    int WARN = 0;
    printf("%-25.25s", name);

    if (human) {
	HumanPrintSpace(status->PartMaxBlocks);
	HumanPrintSpace(status->PartMaxBlocks - status->PartBlocksAvail);
	HumanPrintSpace(status->PartBlocksAvail);
    }
    else
	printf("%10d%10d%10d", status->PartMaxBlocks,
               status->PartMaxBlocks - status->PartBlocksAvail,
               status->PartBlocksAvail);

    PartUsed =
	(100.0 -
	 ((((double)status->PartBlocksAvail) / status->PartMaxBlocks) *
	  100.0));
    if (PartUsed > 90.0) {
	printf(" %4.0f%%<<", PartUsed);
	WARN = 1;
    } else
	printf(" %4.0f%%  ", PartUsed);
    if (WARN) {
	printf("  <<WARNING\n");
    } else
	printf("\n");
    return 0;
}

static char *
AclToString(struct Acl *acl)
{
    static char mydata[AFS_PIOCTL_MAXSIZE + 24];
    char tstring[AFS_PIOCTL_MAXSIZE];
    char dfsstring[AFS_PIOCTL_MAXSIZE];
    struct AclEntry *tp;

    if (acl->dfs)
	snprintf(dfsstring, sizeof(dfsstring), " dfs:%d %s", acl->dfs, acl->cell);
    else
	dfsstring[0] = '\0';
    snprintf(mydata, sizeof(mydata), "%d%s\n%d\n", acl->nplus, dfsstring, acl->nminus);
    for (tp = acl->pluslist; tp; tp = tp->next) {
	snprintf(tstring, sizeof(tstring), "%s %d\n", tp->name, tp->rights);
	strlcat(mydata, tstring, sizeof(mydata));
    }
    for (tp = acl->minuslist; tp; tp = tp->next) {
	snprintf(tstring, sizeof(tstring), "%s %d\n", tp->name, tp->rights);
	strlcat(mydata, tstring, sizeof(mydata));
    }
    return mydata;
}

static int
SetACLCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct Acl *ta = 0;
    struct cmd_item *ti, *ui;
    int plusp;
    afs_int32 rights;
    int clear;
    int idf = getidf(as, parm_setacl_id);
    int error = 0;

    if (as->parms[2].items)
	clear = 1;
    else
	clear = 0;
    plusp = !(as->parms[3].items);
    for (ti = as->parms[0].items; ti; ti = ti->next) {
	blob.out_size = AFS_PIOCTL_MAXSIZE;
	blob.in_size = idf;
	blob.in = blob.out = space;
	code = pioctl(ti->data, VIOCGETAL, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
	    continue;
	}

	if (ta)
	    ZapAcl(ta);
	ta = ParseAcl(space);
	if (!plusp && ta->dfs) {
	    fprintf(stderr,
		    "%s: %s: you may not use the -negative switch with DFS acl's.\n%s",
		    pn, ti->data,
		    "(you may specify \"null\" to revoke all rights, however)\n");
	    error = 1;
	    continue;
	}

	if (ta)
	    ZapAcl(ta);
	if (clear)
	    ta = EmptyAcl(space);
	else
	    ta = ParseAcl(space);
	CleanAcl(ta, ti->data);
	for (ui = as->parms[1].items; ui; ui = ui->next->next) {
	    enum rtype rtype;
	    if (!ui->next) {
		fprintf(stderr,
			"%s: Missing second half of user/access pair.\n", pn);
		ZapAcl(ta);
		return 1;
	    }
	    rights = Convert(ui->next->data, ta->dfs, &rtype);
	    if (rtype == destroy && !ta->dfs) {
		struct AclEntry *tlist;

		tlist = (plusp ? ta->pluslist : ta->minuslist);
		if (!FindList(tlist, ui->data))
		    continue;
	    }
	    if (rtype == deny && !ta->dfs)
		plusp = 0;
	    if (rtype == destroy && ta->dfs)
		rights = -1;
	    ChangeList(ta, plusp, ui->data, rights, &rtype);
	}
	blob.in = AclToString(ta);
	blob.out_size = 0;
	blob.in_size = 1 + strlen(blob.in);
	code = pioctl(ti->data, VIOCSETAL, &blob, 1);
	if (code) {
	    if (errno == EINVAL) {
		if (ta->dfs) {
		    static char *fsenv = 0;
		    if (!fsenv) {
			fsenv = (char *)getenv("FS_EXPERT");
		    }
		    fprintf(stderr,
			    "%s: \"Invalid argument\" was returned when you tried to store a DFS access list.\n",
			    pn);
		    if (!fsenv) {
			fprintf(stderr,
				"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
				"\nPossible reasons for this include:\n\n",
				" -You may have specified an inappropriate combination of rights.\n",
				"  For example, some DFS-supported filesystems may not allow you to\n",
				"  drop the \"c\" right from \"user_obj\".\n\n",
				" -A mask_obj may be required (it is likely required by the underlying\n",
				"  filesystem if you try to set anything other than the basic \"user_obj\"\n",
				"  \"mask_obj\", or \"group_obj\" entries). Unlike acl_edit, the fs command\n",
				"  does not automatically create or update the mask_obj. Try setting\n",
				"  the rights \"mask_obj all\" with \"fs sa\" before adding any explicit\n",
				"  users or groups. You can do this with a single command, such as\n",
				"  \"fs sa mask_obj all user:somename read\"\n\n",
				" -A specified user or group may not exist.\n\n",
				" -You may have tried to delete \"user_obj\", \"group_obj\", or \"other_obj\".\n",
				"  This is probably not allowed by the underlying file system.\n\n",
				" -If you add a user or group to a DFS ACL, remember that it must be\n",
				"  fully specified as \"user:username\" or \"group:groupname\". In addition, there\n",
				"  may be local requirements on the format of the user or group name.\n",
				"  Check with your cell administrator.\n\n",
				" -Or numerous other possibilities. It would be great if we could be more\n",
				"  precise about the actual problem, but for various reasons, this is\n",
				"  impractical via this interface.  If you can't figure it out, you\n",
				"  might try logging into a DCE-equipped machine and use acl_edit (or\n",
				"  whatever is provided). You may get better results. Good luck!\n\n",
				" (You may inhibit this message by setting \"FS_EXPERT\" in your environment)\n");
		    }
		} else {
		    fprintf(stderr,
			    "%s: Invalid argument, possible reasons include:\n",
			    pn);
		    fprintf(stderr, "\t-File not in AFS\n");
		    fprintf(stderr,
			    "\t-Too many users on access control list\n");
		    fprintf(stderr,
			    "\t-Tried to add non-existent user to access control list\n");
		}
	    } else {
		Die(errno, ti->data);
	    }
	    error = 1;
	}
    }
    if (ta)
	ZapAcl(ta);
    return error;
}


static int
CopyACLCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct Acl *fa, *ta = 0;
    struct AclEntry *tp;
    struct cmd_item *ti;
    int clear;
    int idf = getidf(as, parm_copyacl_id);
    int error = 0;

    if (as->parms[2].items)
	clear = 1;
    else
	clear = 0;
    blob.out_size = AFS_PIOCTL_MAXSIZE;
    blob.in_size = idf;
    blob.in = blob.out = space;
    code = pioctl(as->parms[0].items->data, VIOCGETAL, &blob, 1);
    if (code) {
	Die(errno, as->parms[0].items->data);
	return 1;
    }
    fa = ParseAcl(space);
    CleanAcl(fa, as->parms[0].items->data);
    for (ti = as->parms[1].items; ti; ti = ti->next) {
	blob.out_size = AFS_PIOCTL_MAXSIZE;
	blob.in_size = idf;
	blob.in = blob.out = space;
	code = pioctl(ti->data, VIOCGETAL, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
	    continue;
	}

	if (ta)
	    ZapAcl(ta);
	if (clear)
	    ta = EmptyAcl(space);
	else
	    ta = ParseAcl(space);
	CleanAcl(ta, ti->data);
	if (ta->dfs != fa->dfs) {
	    fprintf(stderr,
		    "%s: incompatible file system types: acl not copied to %s; aborted\n",
		    pn, ti->data);
	    error = 1;
	    continue;
	}
	if (ta->dfs) {
	    if (!clear && strcmp(ta->cell, fa->cell) != 0) {
		fprintf(stderr,
			"%s: default DCE cell differs for file %s: use \"-clear\" switch; acl not merged\n",
			pn, ti->data);
		error = 1;
		continue;
	    }
	    strcpy(ta->cell, fa->cell);
	}
                                /* NULL rtype for standard handling   */
	for (tp = fa->pluslist; tp; tp = tp->next)
	    ChangeList(ta, 1, tp->name, tp->rights, NULL);
	for (tp = fa->minuslist; tp; tp = tp->next)
	    ChangeList(ta, 0, tp->name, tp->rights, NULL);
	blob.in = AclToString(ta);
	blob.out_size = 0;
	blob.in_size = 1 + strlen(blob.in);
	code = pioctl(ti->data, VIOCSETAL, &blob, 1);
	if (code) {
	    if (errno == EINVAL) {
		fprintf(stderr,
			"%s: Invalid argument, possible reasons include:\n",
			pn);
		fprintf(stderr, "\t-File not in AFS\n");
	    } else {
		Die(errno, ti->data);
	    }
	    error = 1;
	}
    }
    if (ta)
	ZapAcl(ta);
    ZapAcl(fa);
    return error;
}

/* pioctl() call to get the cellname of a pathname */
static afs_int32
GetCell(char *fname, char *cellname)
{
    afs_int32 code;
    struct ViceIoctl blob;

    blob.in_size = 0;
    blob.out_size = MAXCELLCHARS;
    blob.out = cellname;

    code = pioctl(fname, VIOC_FILE_CELL_NAME, &blob, 1);
    if (code != 0) {
	code = errno;
	fprintf(stderr, "%s: Failed to get cell for '%s'\n", pn, fname);
	Die(code, fname);
    }
    return code;
}

/* Check if a username is valid: If it contains only digits (or a
 * negative sign), then it might be bad. We then query the ptserver
 * to see.
 */
static int
BadName(char *aname, char *fname)
{
    afs_int32 tc, code, id;
    char *nm;
    char cell[MAXCELLCHARS];

    for (nm = aname; (tc = *nm); nm++) {
	/* all must be '-' or digit to be bad */
	if (tc != '-' && (tc < '0' || tc > '9'))
	    return 0;
    }

    /* Go to the PRDB and see if this all number username is valid */
    code = GetCell(fname, cell);
    if (code)
	return 0;

    pr_Initialize(1, AFSDIR_CLIENT_ETC_DIRPATH, cell);
    code = pr_SNameToId(aname, &id);
    pr_End();

    /* 1=>Not-valid; 0=>Valid */
    return ((!code && (id == ANONYMOUSID)) ? 1 : 0);
}


/* clean up an access control list of its bad entries; return 1 if we made
   any changes to the list, and 0 otherwise */
static int
CleanAcl(struct Acl *aa, char *fname)
{
    struct AclEntry *te, **le, *ne;
    int changes;

    /* Don't correct DFS ACL's for now */
    if (aa->dfs)
	return 0;

    /* prune out bad entries */
    changes = 0;		/* count deleted entries */
    le = &aa->pluslist;
    for (te = aa->pluslist; te; te = ne) {
	ne = te->next;
	if (BadName(te->name, fname)) {
	    /* zap this dude */
	    *le = te->next;
	    aa->nplus--;
	    free(te);
	    changes++;
	} else {
	    le = &te->next;
	}
    }
    le = &aa->minuslist;
    for (te = aa->minuslist; te; te = ne) {
	ne = te->next;
	if (BadName(te->name, fname)) {
	    /* zap this dude */
	    *le = te->next;
	    aa->nminus--;
	    free(te);
	    changes++;
	} else {
	    le = &te->next;
	}
    }
    return changes;
}


/* clean up an acl to not have bogus entries */
static int
CleanACLCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct Acl *ta = 0;
    struct ViceIoctl blob;
    int changes;
    struct cmd_item *ti;
    struct AclEntry *te;
    int error = 0;

    SetDotDefault(&as->parms[0].items);
    for (ti = as->parms[0].items; ti; ti = ti->next) {
	blob.out_size = AFS_PIOCTL_MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	code = pioctl(ti->data, VIOCGETAL, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
	    continue;
	}

	if (ta)
	    ZapAcl(ta);
	ta = ParseAcl(space);
	if (ta->dfs) {
	    fprintf(stderr,
		    "%s: cleanacl is not supported for DFS access lists.\n",
		    pn);
	    error = 1;
	    continue;
	}

	changes = CleanAcl(ta, ti->data);

	if (changes) {
	    /* now set the acl */
	    blob.in = AclToString(ta);
	    blob.in_size = strlen(blob.in) + 1;
	    blob.out_size = 0;
	    code = pioctl(ti->data, VIOCSETAL, &blob, 1);
	    if (code) {
		if (errno == EINVAL) {
		    fprintf(stderr,
			    "%s: Invalid argument, possible reasons include\n",
			    pn);
		    fprintf(stderr, "%s: File not in vice or\n", pn);
		    fprintf(stderr,
			    "%s: Too many users on access control list or\n",
			    pn);
		} else {
		    Die(errno, ti->data);
		}
		error = 1;
		continue;
	    }

	    /* now list the updated acl */
	    printf("Access list for %s is now\n", ti->data);
	    if (ta->nplus > 0) {
		if (!ta->dfs)
		    printf("Normal rights:\n");
		for (te = ta->pluslist; te; te = te->next) {
		    printf("  %s ", te->name);
		    PRights(te->rights, ta->dfs);
		    printf("\n");
		}
	    }
	    if (ta->nminus > 0) {
		printf("Negative rights:\n");
		for (te = ta->minuslist; te; te = te->next) {
		    printf("  %s ", te->name);
		    PRights(te->rights, ta->dfs);
		    printf("\n");
		}
	    }
	    if (ti->next)
		printf("\n");
	} else
	    printf("Access list for %s is fine.\n", ti->data);
    }
    if (ta)
	ZapAcl(ta);
    return error;
}

static int
ListACLCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct Acl *ta;
    struct ViceIoctl blob;
    struct AclEntry *te;
    struct cmd_item *ti;
    int idf = getidf(as, parm_listacl_id);
    int error = 0;

    SetDotDefault(&as->parms[0].items);
    for (ti = as->parms[0].items; ti; ti = ti->next) {
	blob.out_size = AFS_PIOCTL_MAXSIZE;
	blob.in_size = idf;
	blob.in = blob.out = space;
	code = pioctl(ti->data, VIOCGETAL, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
	    continue;
	}
	ta = ParseAcl(space);
        if (as->parms[3].items) { 			/* -cmd */
            printf("fs setacl -dir %s -acl ", ti->data);
            if (ta->nplus > 0) {
                for (te = ta->pluslist; te; te = te->next) {
                    printf("  %s ", te->name);
                    PRights(te->rights, ta->dfs);
                }
            }
            printf("\n");
            if (ta->nminus > 0) {
                printf("fs setacl -dir %s -acl ", ti->data);
                for (te = ta->minuslist; te; te = te->next) {
                    printf("  %s ", te->name);
                    PRights(te->rights, ta->dfs);
                }
                printf(" -negative\n");
            }
        } else {
	    switch (ta->dfs) {
	    case 0:
	        printf("Access list for %s is\n", ti->data);
	        break;
	    case 1:
	        printf("DFS access list for %s is\n", ti->data);
	        break;
	    case 2:
	        printf("DFS initial directory access list of %s is\n", ti->data);
	        break;
	    case 3:
	        printf("DFS initial file access list of %s is\n", ti->data);
	        break;
	    }
	    if (ta->dfs) {
	        printf("  Default cell = %s\n", ta->cell);
	    }
	    if (ta->nplus > 0) {
	        if (!ta->dfs)
		    printf("Normal rights:\n");
	        for (te = ta->pluslist; te; te = te->next) {
		    printf("  %s ", te->name);
		    PRights(te->rights, ta->dfs);
		    printf("\n");
	        }
	    }
	    if (ta->nminus > 0) {
	        printf("Negative rights:\n");
	        for (te = ta->minuslist; te; te = te->next) {
		    printf("  %s ", te->name);
		    PRights(te->rights, ta->dfs);
		    printf("\n");
	        }
	    }
	    if (ti->next)
	        printf("\n");
	}
	ZapAcl(ta);
    }
    return error;
}

static int
GetCallerAccess(struct cmd_syndesc *as, void *arock)
{
    struct cmd_item *ti;
    int error = 0;

    SetDotDefault(&as->parms[0].items);
    for (ti = as->parms[0].items; ti; ti = ti->next) {
        afs_int32 code;
        struct ViceIoctl blob;
        struct vcxstat2 stat;
        blob.out_size = sizeof(struct vcxstat2);
        blob.in_size = 0;
        blob.out = (void *)&stat;
        code = pioctl(ti->data, VIOC_GETVCXSTATUS2, &blob, 1);
        if (code) {
            Die(errno, ti->data);
            error = 1;
            continue;
        }
        printf("Callers access to %s is ", ti->data);
        PRights(stat.callerAccess, 0);
        printf("\n");
    }
    return error;
}

static int
FlushVolumeCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    int error = 0;

    SetDotDefault(&as->parms[0].items);
    for (ti = as->parms[0].items; ti; ti = ti->next) {
	blob.in_size = blob.out_size = 0;
	code = pioctl(ti->data, VIOC_FLUSHVOLUME, &blob, 0);
	if (code) {
	    fprintf(stderr, "Error flushing volume ");
	    perror(ti->data);
	    error = 1;
	    continue;
	}
    }
    return error;
}

static int
FlushAllVolumesCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    int error = 0;

    blob.in_size = 0;
    blob.out_size = AFS_PIOCTL_MAXSIZE;
    blob.out = space;

    code = pioctl(NULL, VIOC_FLUSHALL, &blob, 0);
    if (code) {
	fprintf(stderr, "Error flushing all volumes\n");
	error = 1;
    }
    return error;
}

/*
 * The Windows version of UuidCmd displays the UUID.
 * When the UNIX version is updated to do the same
 * be sure to replace the CMD_REQUIRED flag with
 * CMD_OPTIONAL in the cmd_AddParam(-generate) call
 */
static int
UuidCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;

    blob.in_size = 0;
    blob.out_size = 0;

    if (as->parms[0].items) {
        if (geteuid()) {
            fprintf (stderr, "Permission denied: requires root access.\n");
            return EACCES;
        }

        /* generate new UUID */
        code = pioctl(0, VIOC_NEWUUID, &blob, 1);

        if (code) {
            Die(errno, 0);
            return 1;
        }

        printf("New uuid generated.\n");
    } else {
        /* This will never execute */
        printf("Please add the '-generate' option to generate a new UUID.\n");
    }
    return 0;
}

#if defined(AFS_CACHE_BYPASS)
/*
 * Set cache-bypass threshold.  Files larger than this size will not be cached.
 * With a threshold of 0, the cache is always bypassed.  With a threshold of -1,
 * cache bypass is disabled.
 */

static int
BypassThresholdCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    afs_int32 threshold_i, threshold_o;
    char *tp;

    /* if new threshold supplied, then set and confirm, else,
     * get current threshold and print
     */

    if(as->parms[0].items) {
	int digit, ix, len;

	tp = as->parms[0].items->data;
	len = strlen(tp);

        if (!strcmp(tp,"-1")) {
            threshold_i = -1;
        } else {
            digit = 1;
            for(ix = 0; ix < len; ++ix) {
                if(!isdigit(tp[0])) {
                    digit = 0;
                    break;
                }
            }
            if (digit == 0) {
                fprintf(stderr, "fs bypassthreshold -size: %s must be an integer between -1 and 2^31\n", tp);
                return EINVAL;
            }
            threshold_i = atoi(tp);
            if(ix > 9 && threshold_i < 2147483647)
                threshold_i = 2147483647;
        }
        blob.in = (char *) &threshold_i;
        blob.in_size = sizeof(threshold_i);
    } else {
	blob.in = NULL;
	blob.in_size = 0;
    }

    blob.out = (char *) &threshold_o;
    blob.out_size = sizeof(threshold_o);
    code = pioctl(0, VIOC_SETBYPASS_THRESH, &blob, 1);
    if (code) {
	Die(errno, NULL);
	return 1;
    } else {
	printf("Cache bypass threshold %d", threshold_o);
	if(threshold_o ==  -1)
	    printf(" (disabled)");
	printf("\n");
    }

    return 0;
}

#endif

static int
FlushCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    int error = 0;

    for (ti = as->parms[0].items; ti; ti = ti->next) {
	blob.in_size = blob.out_size = 0;
	code = pioctl(ti->data, VIOCFLUSH, &blob, 0);
	if (code) {
	    if (errno == EMFILE) {
		fprintf(stderr, "%s: Can't flush active file %s\n", pn,
			ti->data);
	    } else {
		fprintf(stderr, "%s: Error flushing file ", pn);
		perror(ti->data);
	    }
	    error = 1;
	    continue;
	}
    }
    return error;
}

/* all this command does is repackage its args and call SetVolCmd */
static int
SetQuotaCmd(struct cmd_syndesc *as, void *arock)
{
    struct cmd_syndesc ts;

    /* copy useful stuff from our command slot; we may later have to reorder */
    memcpy(&ts, as, sizeof(ts));	/* copy whole thing */
    return SetVolCmd(&ts, arock);
}

static int
SetVolCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    struct VolumeStatus *status;
    char *offmsg, *input;
    int error = 0;

    SetDotDefault(&as->parms[0].items);
    for (ti = as->parms[0].items; ti; ti = ti->next) {
	/* once per file */
	blob.out_size = AFS_PIOCTL_MAXSIZE;
	blob.in_size = sizeof(*status) + 3;	/* for the three terminating nulls */
	blob.out = space;
	blob.in = space;
	status = (VolumeStatus *) space;
	status->MinQuota = status->MaxQuota = -1;
	offmsg = NULL;
	if (as->parms[1].items) {
	    code = util_GetHumanInt32(as->parms[1].items->data, &status->MaxQuota);
	    if (code) {
		fprintf(stderr, "%s: bad integer specified for quota.\n", pn);
		error = 1;
		continue;
	    }
	}
	if (as->parms[2].items)
	    offmsg = as->parms[2].items->data;
	input = (char *)status + sizeof(*status);
	*(input++) = '\0';	/* never set name: this call doesn't change vldb */
	if (offmsg) {
	    if (strlen(offmsg) >= VMSGSIZE) {
		fprintf(stderr,
			"%s: message must be shorter than %d characters\n",
			pn, VMSGSIZE);
		error = 1;
		continue;
	    }
	    strcpy(input, offmsg);
	    blob.in_size += strlen(offmsg);
	    input += strlen(offmsg) + 1;
	} else
	    *(input++) = '\0';
	*(input++) = '\0';	/* Pad for old style volume "motd" */
	code = pioctl(ti->data, VIOCSETVOLSTAT, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
	}
    }
    return error;
}

/*
 * Why is VenusFid declared in the kernel-only section of afs.h,
 * if it's the exported interface of the cache manager?
 */
struct VenusFid {
    afs_int32 Cell;
    AFSFid Fid;
};

static int
ExamineCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    struct VolumeStatus *status;
    char *name, *offmsg;
    int error = 0;

    SetDotDefault(&as->parms[0].items);
    for (ti = as->parms[0].items; ti; ti = ti->next) {
	struct VenusFid vfid;

	/* once per file */
	blob.out_size = AFS_PIOCTL_MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	code = pioctl(ti->data, VIOCGETVOLSTAT, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
	    continue;
	}
	status = (VolumeStatus *) space;
	name = (char *)status + sizeof(*status);
	offmsg = name + strlen(name) + 1;

	blob.out_size = sizeof(struct VenusFid);
	blob.out = (char *) &vfid;
	if (0 == pioctl(ti->data, VIOCGETFID, &blob, 1)) {
	    printf("File %s (%u.%u.%u) contained in volume %u\n",
		   ti->data, vfid.Fid.Volume, vfid.Fid.Vnode, vfid.Fid.Unique,
		   vfid.Fid.Volume);
	}

	PrintStatus(status, name, offmsg);
    }
    return error;
}

static int
ListQuotaCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    struct VolumeStatus *status;
    char *name;
    int error = 0;
    int human = 0;

    if (as->parms[1].items)
	human = 1;

    printf("%-25s%-11s%-11s%-7s%-11s\n", "Volume Name", "      Quota",
	   "       Used", " %Used", "  Partition");
    SetDotDefault(&as->parms[0].items);
    for (ti = as->parms[0].items; ti; ti = ti->next) {
	/* once per file */
	blob.out_size = AFS_PIOCTL_MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	code = pioctl(ti->data, VIOCGETVOLSTAT, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
	    continue;
	}
	status = (VolumeStatus *) space;
	name = (char *)status + sizeof(*status);
	QuickPrintStatus(status, name, human);
    }
    return error;
}

static int
WhereIsCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    int j;
    afs_int32 *hosts;
    char *tp;
    int error = 0;

    SetDotDefault(&as->parms[0].items);
    for (ti = as->parms[0].items; ti; ti = ti->next) {
	/* once per file */
	blob.out_size = AFS_PIOCTL_MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	memset(space, 0, sizeof(space));
	code = pioctl(ti->data, VIOCWHEREIS, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
	    continue;
	}
	hosts = (afs_int32 *) space;
	printf("File %s is on host%s ", ti->data,
	       (hosts[0] && !hosts[1]) ? "" : "s");
	for (j = 0; j < AFS_MAXHOSTS; j++) {
	    if (hosts[j] == 0)
		break;
	    tp = hostutil_GetNameByINet(hosts[j]);
	    printf("%s ", tp);
	}
	printf("\n");
    }
    return error;
}


static int
DiskFreeCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    char *name;
    struct VolumeStatus *status;
    int error = 0;
    int human = 0;

    if (as->parms[1].items)
	human = 1;

    printf("%-25s%10s%10s%10s%6s\n", "Volume Name",
	   human ? "total" : "kbytes", "used", "avail", "%used");
    SetDotDefault(&as->parms[0].items);
    for (ti = as->parms[0].items; ti; ti = ti->next) {
	/* once per file */
	blob.out_size = AFS_PIOCTL_MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	code = pioctl(ti->data, VIOCGETVOLSTAT, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
	    continue;
	}
	status = (VolumeStatus *) space;
	name = (char *)status + sizeof(*status);
	QuickPrintSpace(status, name, human);
    }
    return error;
}

static int
QuotaCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    double quotaPct;
    struct VolumeStatus *status;
    int error = 0;

    SetDotDefault(&as->parms[0].items);
    for (ti = as->parms[0].items; ti; ti = ti->next) {
	/* once per file */
	blob.out_size = AFS_PIOCTL_MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	code = pioctl(ti->data, VIOCGETVOLSTAT, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
	    continue;
	}
	status = (VolumeStatus *) space;
	if (status->MaxQuota)
	    quotaPct =
		((((double)status->BlocksInUse) / status->MaxQuota) * 100.0);
	else
	    quotaPct = 0.0;
	printf("%2.0f%% of quota used.\n", quotaPct);
    }
    return error;
}

static int
GetLastComponent(const char *data, char **outdir, char **outbase,
		 int *thru_symlink, int literal)
{
    char orig_name[MAXPATHLEN];	/*Original name, may be modified */
    char true_name[MAXPATHLEN];	/*``True'' dirname (e.g., symlink target) */
    char *lastSlash;
    struct stat statbuff;	/*Buffer for status info */
    int link_chars_read;	/*Num chars read in readlink() */
    char *dirname = NULL;
    char *basename = NULL;
    size_t len;

    *outbase = NULL;
    *outdir = NULL;
    memset(&statbuff, 0, sizeof(statbuff));

    if (thru_symlink)
	*thru_symlink = 0;

    snprintf(orig_name, sizeof(orig_name), "%s%s",
	     (data[0] == '/') ? "" : "./", data);

    if (!literal && lstat(orig_name, &statbuff) < 0) {
	/* if lstat fails, we should still try the pioctl, since it
	 * may work (for example, lstat will fail, but pioctl will
	 * work if the volume of offline (returning ENODEV). */
	statbuff.st_mode = S_IFDIR;	/* lie like pros */
    }

    /*
     * The lstat succeeded.  If the -literal flag wasn't specified and the given
     * file is a symlink, substitute the file name with the link name.
     */
    if (!literal && (statbuff.st_mode & S_IFMT) == S_IFLNK) {
	if (thru_symlink)
	     *thru_symlink = 1;

	/* Read name of resolved file (leave space for NULL!) */
	link_chars_read = readlink(orig_name, true_name, MAXPATHLEN-1);
	if (link_chars_read <= 0) {
	    fprintf(stderr,
		    "%s: Can't read target name for '%s' symbolic link!\n",
		    pn, orig_name);
	    goto out;
	}

	/* Add a trailing null to what was read, bump the length. */
	true_name[link_chars_read++] = 0;

	/*
	 * If the symlink is an absolute pathname, we're fine.  Otherwise, we
	 * have to create a full pathname using the original name and the
	 * relative symlink name.  Find the rightmost slash in the original
	 * name (we know there is one) and splice in the symlink value.
	 */
	if (true_name[0] != '/') {
	    lastSlash = strrchr(orig_name, '/');
	    strcpy(++lastSlash, true_name);
	    strcpy(true_name, orig_name);
	}
     } else {
	strcpy(true_name, orig_name);
     }

    /* Trim trailing slashes, if any. */
    len = strlen(true_name);
    while (len > 1 && true_name[len - 1] == '/') {
	true_name[len - 1 ] = '\0';
	len--;
    }

    /* Find rightmost slash, if any. */
    lastSlash = strrchr(true_name, '/');
    if (lastSlash == true_name) {
	dirname = strdup("/");
	basename = strdup(lastSlash+1);
    } else if (lastSlash != NULL) {
	/*
	 * Found it.  Designate everything before it as the parent directory,
	 * everything after it as the final component.
	 */
	*lastSlash = '\0';
	dirname = strdup(true_name);
	basename = strdup(lastSlash+1);
    } else {
	/*
	 * No slash appears in the given file name.  Set parent_dir to the current
	 * directory, and the last component as the given name.
	 */
	dirname = strdup(".");
	basename = strdup(true_name);
    }

    if (strcmp(basename, ".") == 0
	|| strcmp(basename, "..") == 0) {
	fprintf(stderr,
		"%s: you may not use '.' or '..' as the last component\n", pn);
	fprintf(stderr, "%s: of a name in this fs command.\n", pn);
	goto out;
    }

    *outdir = dirname;
    *outbase = basename;

    return 0;

out:
    if (dirname)
	free(dirname);
    if (basename)
	free(basename);
    return -1;
}


static int
ListMountCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    char *last_component;
    char *parent_dir;
    int thru_symlink = 0;
    int error = 0;

    for (ti = as->parms[0].items; ti; ti = ti->next) {
	if (GetLastComponent(ti->data, &parent_dir,
			     &last_component, &thru_symlink, 0) != 0) {
	    error = 1;
	    continue;
	}

	blob.in = last_component;
	blob.in_size = strlen(last_component) + 1;
	blob.out_size = AFS_PIOCTL_MAXSIZE;
	blob.out = space;
	memset(space, 0, AFS_PIOCTL_MAXSIZE);

	code = pioctl(parent_dir, VIOC_AFS_STAT_MT_PT, &blob, 1);
	free(last_component);

	if (code == 0) {
	    printf("'%s' is a %smount point for volume '%s'\n", ti->data,
		   (thru_symlink ? "symbolic link, leading to a " : ""),
		   space);
	} else {
	    if (errno == EINVAL) {
		fprintf(stderr, "'%s' is not a mount point.\n", ti->data);
	    } else {
		Die(errno, (ti->data ? ti->data : parent_dir));
	    }
	    error = 1;
	}
	free(parent_dir);
    }
    return error;
}

static int
MakeMountCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    char *cellName, *volName, *tmpName;
    struct afsconf_cell info;
    struct vldbentry vldbEntry;
    struct ViceIoctl blob;
    struct afsconf_dir *dir;

/*

defect #3069

    if (as->parms[5].items && !as->parms[2].items) {
	fprintf(stderr, "%s: must provide cell when creating cellular mount point.\n", pn);
	return 1;
    }
*/

    if (as->parms[2].items)	/* cell name specified */
	cellName = as->parms[2].items->data;
    else
	cellName = NULL;
    volName = as->parms[1].items->data;

    if (strlen(volName) >= 64) {
	fprintf(stderr,
		"%s: volume name too long (length must be < 64 characters)\n",
		pn);
	return 1;
    }

    /* Check for a cellname in the volume specification, and complain
     * if it doesn't match what was specified with -cell */
    if ((tmpName = strchr(volName, ':'))) {
	*tmpName = '\0';
	if (cellName) {
	    if (strcasecmp(cellName, volName)) {
		fprintf(stderr, "%s: cellnames do not match.\n", pn);
		return 1;
	    }
	}
	cellName = volName;
	volName = ++tmpName;
    }

    if (!InAFS(Parent(as->parms[0].items->data))) {
	fprintf(stderr,
		"%s: mount points must be created within the AFS file system\n",
		pn);
	return 1;
    }

    if (!cellName) {
	blob.in_size = 0;
	blob.out_size = AFS_PIOCTL_MAXSIZE;
	blob.out = space;
	code = pioctl(Parent(as->parms[0].items->data), VIOC_FILE_CELL_NAME,
	       	      &blob, 1);
	if (code) {
	   fprintf(stderr,
		   "%s: couldn't get cell name for file's parent\n", pn);
	   return 1;
	}
    }

    dir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);
    if (!dir) {
	fprintf(stderr,
		"Could not process files in configuration directory (%s).\n",
		AFSDIR_CLIENT_ETC_DIRPATH);
        return 1;
    }

    code = afsconf_GetCellInfo(dir, cellName ? cellName : space,
		               AFSCONF_VLDBSERVICE, &info);
    if (code) {
	fprintf(stderr,
		"%s: cell %s not in %s\n", pn, cellName ? cellName : space,
		AFSDIR_CLIENT_CELLSERVDB_FILEPATH);
	return 1;
    }

    if (!(as->parms[4].items)) {
	/* not fast, check which cell the mountpoint is being created in */
	code = ugen_ClientInitCell(dir, &info,
				   AFSCONF_SECOPTS_FALLBACK_NULL |
				   AFSCONF_SECOPTS_NOAUTH,
				   &uclient, VLDB_MAXSERVERS,
				   AFSCONF_VLDBSERVICE, 50);

	if (code == 0) {
	    /* make the check.  Don't complain if there are problems with init */
	    code =
		ubik_VL_GetEntryByNameO(uclient, 0, volName,
			  &vldbEntry);
	    if (code == VL_NOENT) {
		fprintf(stderr,
			"%s: warning, volume %s does not exist in cell %s.\n",
			pn, volName, cellName ? cellName : space);
	    }
	}
    }

    if (as->parms[3].items)	/* if -rw specified */
	strcpy(space, "%");
    else
	strcpy(space, "#");
    if (cellName) {
	/* cellular mount point, prepend cell prefix */
	strcat(space, info.name);
	strcat(space, ":");
    }
    strcat(space, volName);	/* append volume name */
    strcat(space, ".");		/* stupid convention; these end with a period */
    code = symlink(space, as->parms[0].items->data);
    if (code) {
	Die(errno, as->parms[0].items->data);
	return 1;
    }
    return 0;
}

/*
 * Delete AFS mount points.  Variables are used as follows:
 *      tbuffer: Set to point to the null-terminated directory name of the mount point
 *	    (or ``.'' if none is provided)
 *      tp: Set to point to the actual name of the mount point to nuke.
 */
static int
RemoveMountCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code = 0;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    char tbuffer[1024];
    char lsbuffer[1024];
    char *tp;
    int error = 0;

    for (ti = as->parms[0].items; ti; ti = ti->next) {
	/* once per file */
	tp = (char *)strrchr(ti->data, '/');
	if (tp) {
	    strncpy(tbuffer, ti->data, code = tp - ti->data);	/* the dir name */
	    tbuffer[code] = 0;
	    tp++;		/* skip the slash */
	} else {
	    strcpy(tbuffer, ".");
	    tp = ti->data;
	}
	blob.in = tp;
	blob.in_size = strlen(tp) + 1;
	blob.out = lsbuffer;
	blob.out_size = sizeof(lsbuffer);
	code = pioctl(tbuffer, VIOC_AFS_STAT_MT_PT, &blob, 1);
	if (code) {
	    if (errno == EINVAL) {
		fprintf(stderr, "%s: '%s' is not a mount point.\n", pn,
			ti->data);
	    } else {
		Die(errno, ti->data);
	    }
	    error = 1;
	    continue;		/* don't bother trying */
	}
	blob.out_size = 0;
	blob.in = tp;
	blob.in_size = strlen(tp) + 1;
	code = pioctl(tbuffer, VIOC_AFS_DELETE_MT_PT, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
	}
    }
    return error;
}

/*
*/

static int
CheckServersCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    afs_int32 j;
    afs_int32 temp;
    char *tp;
    struct chservinfo checkserv;

    memset(&checkserv, 0, sizeof(struct chservinfo));
    blob.in_size = sizeof(struct chservinfo);
    blob.in = (caddr_t) & checkserv;

    blob.out_size = AFS_PIOCTL_MAXSIZE;
    blob.out = space;
    memset(space, 0, sizeof(afs_int32));	/* so we assure zero when nothing is copied back */

    /* prepare flags for checkservers command */
    temp = 2;			/* default to checking local cell only */
    if (as->parms[2].items)
	temp |= 1;		/* set fast flag */
    if (as->parms[1].items)
	temp &= ~2;		/* turn off local cell check */

    checkserv.magic = 0x12345678;	/* XXX */
    checkserv.tflags = temp;

    /* now copy in optional cell name, if specified */
    if (as->parms[0].items) {
	code = GetCellName(as->parms[0].items->data, &checkserv.tbuffer[0],
			   sizeof(checkserv.tbuffer));
	if (code) {
	    return 1;
	}
	checkserv.tsize = strlen(checkserv.tbuffer) + 1;
    } else {
	strcpy(checkserv.tbuffer, "\0");
	checkserv.tsize = 0;
    }

    if (as->parms[3].items) {
	checkserv.tinterval = atol(as->parms[3].items->data);

	/* sanity check */
	if (checkserv.tinterval < 0) {
	    printf
		("Warning: The negative -interval is ignored; treated as an inquiry\n");
	    checkserv.tinterval = 0;
	} else if (checkserv.tinterval > 600) {
	    printf
		("Warning: The maximum -interval value is 10 mins (600 secs)\n");
	    checkserv.tinterval = 600;	/* 10 min max interval */
	}
    } else {
	checkserv.tinterval = -1;	/* don't change current interval */
    }

    code = pioctl(0, VIOCCKSERV, &blob, 1);
    if (code) {
	if ((errno == EACCES) && (checkserv.tinterval > 0)) {
	    printf("Must be root to change -interval\n");
	    return 1;
	}
	Die(errno, 0);
	return 1;
    }
    memcpy(&temp, space, sizeof(afs_int32));
    if (checkserv.tinterval >= 0) {
	if (checkserv.tinterval > 0)
	    printf
		("The new down server probe interval (%d secs) is now in effect (old interval was %d secs)\n",
		 checkserv.tinterval, temp);
	else
	    printf("The current down server probe interval is %d secs\n",
		   temp);
	return 0;
    }
    if (temp == 0) {
	printf("All servers are running.\n");
    } else {
	printf
	    ("These servers unavailable due to network or server problems: ");
	for (j = 0;; j++) {
	    memcpy(&temp, space + j * sizeof(afs_int32), sizeof(afs_int32));
	    if (temp == 0)
		break;
	    tp = hostutil_GetNameByINet(temp);
	    printf(" %s", tp);
	}
	printf(".\n");
	code = 1;		/* XXX */
    }
    return code;
}

static int
MessagesCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code = 0;
    struct ViceIoctl blob;
    struct gaginfo gagflags;
    struct cmd_item *show;

    memset(&gagflags, 0, sizeof(struct gaginfo));
    blob.in_size = sizeof(struct gaginfo);
    blob.in = (caddr_t) & gagflags;
    blob.out_size = AFS_PIOCTL_MAXSIZE;
    blob.out = space;
    memset(space, 0, sizeof(afs_int32));	/* so we assure zero when nothing is copied back */

    if ((show = as->parms[0].items)) {
	if (!strcasecmp(show->data, "user"))
	    gagflags.showflags |= GAGUSER;
	else if (!strcasecmp(show->data, "console"))
	    gagflags.showflags |= GAGCONSOLE;
	else if (!strcasecmp(show->data, "all"))
	    gagflags.showflags |= GAGCONSOLE | GAGUSER;
	else if (!strcasecmp(show->data, "none"))
	    /* do nothing */ ;
	else {
	    fprintf(stderr,
		    "unrecognized flag %s: must be in {user,console,all,none}\n",
		    show->data);
	    code = EINVAL;
	}
    }

    if (code)
	return 1;

    code = pioctl(0, VIOC_GAG, &blob, 1);
    if (code) {
	Die(errno, 0);
	return 1;
    }

    return 0;
}

static int
CheckVolumesCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;

    blob.in_size = 0;
    blob.out_size = 0;
    code = pioctl(0, VIOCCKBACK, &blob, 1);
    if (code) {
	Die(errno, 0);
	return 1;
    }

    printf("All volumeID/name mappings checked.\n");
    return 0;
}

static int
PreCacheCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    afs_int32 temp;

    if (!as->parms[0].items && !as->parms[1].items) {
	fprintf(stderr, "%s: syntax error in precache cmd.\n", pn);
	return 1;
    }
    if (as->parms[0].items) {
	code = util_GetInt32(as->parms[0].items->data, &temp);
	if (code) {
	    fprintf(stderr, "%s: bad integer specified for precache size.\n",
		    pn);
	    return 1;
	}
    } else
	temp = 0;
    blob.in = (char *)&temp;
    blob.in_size = sizeof(afs_int32);
    blob.out_size = 0;
    code = pioctl(0, VIOCPRECACHE, &blob, 1);
    if (code) {
	Die(errno, NULL);
	return 1;
    }

    printf("New precache size set.\n");
    return 0;
}

static int
SetCacheSizeCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    afs_int32 temp;

    if (!as->parms[0].items && !as->parms[1].items) {
	fprintf(stderr, "%s: syntax error in setcachesize cmd.\n", pn);
	return 1;
    }
    if (as->parms[0].items) {
	code = util_GetHumanInt32(as->parms[0].items->data, &temp);
	if (code) {
	    fprintf(stderr, "%s: bad integer specified for cache size.\n",
		    pn);
	    return 1;
	}
    } else
	temp = 0;
    blob.in = (char *)&temp;
    blob.in_size = sizeof(afs_int32);
    blob.out_size = 0;
    code = pioctl(0, VIOCSETCACHESIZE, &blob, 1);
    if (code) {
	if (errno == EROFS) {
	    printf
		("'fs setcache' not allowed on memory cache based cache managers.\n");
	} else {
	    Die(errno, NULL);
	}
	return 1;
    }

    printf("New cache size set.\n");
    return 0;
}

#define MAXGCSIZE	16
static int
GetCacheParmsCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code, filesUsed;
    struct ViceIoctl blob;
    afs_int32 parms[MAXGCSIZE];
    double percentFiles, percentBlocks;
    afs_int32 flags = 0;

    if (as->parms[0].items){ /* -files */
	flags = 1;
    } else if (as->parms[1].items){ /* -excessive */
	flags = 2;
    } else {
	flags = 0;
    }

    memset(parms, '\0', sizeof parms);	/* avoid Purify UMR error */
    if (flags){
	blob.in = (char *)&flags;
	blob.in_size = sizeof(afs_int32);
    } else {	/* be backward compatible */
	blob.in = NULL;
	blob.in_size = 0;
    }
    blob.out_size = sizeof(parms);
    blob.out = (char *)parms;
    code = pioctl(0, VIOCGETCACHEPARMS, &blob, 1);
    if (code) {
	Die(errno, NULL);
	return 1;
    }

    if (!flags){
	printf("AFS using %d of the cache's available %d 1K byte blocks.\n",
		parms[1], parms[0]);
	if (parms[1] > parms[0])
		printf("[Cache guideline temporarily deliberately exceeded; it will be adjusted down but you may wish to increase the cache size.]\n");
	return 0;
    }

    percentBlocks = ((double)parms[1]/parms[0]) * 100;
    printf("AFS using %5.0f%% of cache blocks (%d of %d 1k blocks)\n",
	   percentBlocks, parms[1], parms[0]);

    if (parms[2] == 0)
	return 0;

    filesUsed = parms[2] - parms[3];
    percentFiles = ((double)filesUsed/parms[2]) * 100;
    printf("          %5.0f%% of the cache files (%d of %d files)\n",
	    percentFiles, filesUsed, parms[2]);
    if (flags == 2){
	printf("	afs_cacheFiles: %10d\n", parms[2]);
	printf("	IFFree:         %10d\n", parms[3]);
	printf("	IFEverUsed:     %10d\n", parms[4]);
	printf("	IFDataMod:      %10d\n", parms[5]);
	printf("	IFDirtyPages:   %10d\n", parms[6]);
	printf("	IFAnyPages:     %10d\n", parms[7]);
	printf("	IFDiscarded:    %10d\n", parms[8]);
	printf("	DCentries:  %10d\n", parms[9]);
	printf("	  0k-   4K: %10d\n", parms[10]);
	printf("	  4k-  16k: %10d\n", parms[11]);
	printf("	 16k-  64k: %10d\n", parms[12]);
	printf("	 64k- 256k: %10d\n", parms[13]);
	printf("	256k-   1M: %10d\n", parms[14]);
	printf("	      >=1M: %10d\n", parms[15]);
    }

    if (percentBlocks > 90)
	printf("[cache size usage over 90%%, consider increasing cache size]\n");
    if (percentFiles > 90)
	printf("[cache file usage over 90%%, consider increasing '-files' argument to afsd]\n");

    return 0;
}

static int
ListCellsCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    afs_int32 i, j;
    char *tp;
    struct ViceIoctl blob;
    int resolve;

    resolve = !(as->parms[0].items);	/* -numeric */

    for (i = 0;; i++) {
	tp = space;
	memcpy(tp, &i, sizeof(afs_int32));
	blob.out_size = AFS_PIOCTL_MAXSIZE;
	blob.in_size = sizeof(afs_int32);
	blob.in = space;
	blob.out = space;
	code = pioctl(0, VIOCGETCELL, &blob, 1);
	if (code < 0) {
	    if (errno == EDOM)
		break;		/* done with the list */
	    Die(errno, 0);
	    return 1;
	}
	tp = space;
	printf("Cell %s on hosts", tp + AFS_MAXCELLHOSTS * sizeof(afs_int32));
	for (j = 0; j < AFS_MAXCELLHOSTS; j++) {
	    afs_int32 addr;
	    char *name, tbuffer[20];

	    memcpy(&addr, tp + j * sizeof(afs_int32), sizeof(afs_int32));
	    if (addr == 0)
		break;

	    if (resolve) {
		name = hostutil_GetNameByINet(addr);
	    } else {
		addr = ntohl(addr);
		snprintf(tbuffer, sizeof(tbuffer), "%d.%d.%d.%d", (addr >> 24) & 0xff,
			(addr >> 16) & 0xff, (addr >> 8) & 0xff, addr & 0xff);
		name = tbuffer;
	    }
	    printf(" %s", name);
	}
	printf(".\n");
    }
    return 0;
}

static int
ListAliasesCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code, i;
    char *tp, *aliasName, *realName;
    struct ViceIoctl blob;

    for (i = 0;; i++) {
	tp = space;
	memcpy(tp, &i, sizeof(afs_int32));
	blob.out_size = AFS_PIOCTL_MAXSIZE;
	blob.in_size = sizeof(afs_int32);
	blob.in = space;
	blob.out = space;
	code = pioctl(0, VIOC_GETALIAS, &blob, 1);
	if (code < 0) {
	    if (errno == EDOM)
		break;		/* done with the list */
	    Die(errno, 0);
	    return 1;
	}
	tp = space;
	aliasName = tp;
	tp += strlen(aliasName) + 1;
	realName = tp;
	printf("Alias %s for cell %s\n", aliasName, realName);
    }
    return 0;
}

static int
CallBackRxConnCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    afs_int32 hostAddr;
    struct hostent *thp;

    ti = as->parms[0].items;
    if (ti) {
        thp = hostutil_GetHostByName(ti->data);
	if (!thp) {
	    fprintf(stderr, "host %s not found in host table.\n", ti->data);
	    return 1;
	}
	else memcpy(&hostAddr, thp->h_addr, sizeof(afs_int32));
    } else {
        hostAddr = 0;   /* means don't set host */
    }

    /* now do operation */
    blob.in_size = sizeof(afs_int32);
    blob.out_size = sizeof(afs_int32);
    blob.in = (char *) &hostAddr;
    blob.out = (char *) &hostAddr;

    code = pioctl(0, VIOC_CBADDR, &blob, 1);
    if (code < 0) {
	Die(errno, 0);
	return 1;
    }
    return 0;
}

static int
NukeNFSCredsCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    afs_int32 hostAddr;
    struct hostent *thp;

    ti = as->parms[0].items;
    thp = hostutil_GetHostByName(ti->data);
    if (!thp) {
	fprintf(stderr, "host %s not found in host table.\n", ti->data);
	return 1;
    }
    else memcpy(&hostAddr, thp->h_addr, sizeof(afs_int32));

    /* now do operation */
    blob.in_size = sizeof(afs_int32);
    blob.out_size = sizeof(afs_int32);
    blob.in = (char *) &hostAddr;
    blob.out = (char *) &hostAddr;

    code = pioctl(0, VIOC_NFS_NUKE_CREDS, &blob, 1);
    if (code < 0) {
	Die(errno, 0);
	return 1;
    }
    return 0;
}

static int
NewCellCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code, linkedstate = 0, size = 0, *lp;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    char *tp, *cellname = 0;
    struct hostent *thp;
    afs_int32 fsport = 0, vlport = 0;
    afs_int32 scount;		/* Number of servers to pass in pioctl call */

    /* Yuck!
     * With the NEWCELL pioctl call, 3.4 clients take an array of
     * AFS_MAXHOSTS (13) servers while 3.5 clients take an array of
     * AFS_MAXCELLHOSTS (8) servers. To determine which we are talking to,
     * do a GETCELL pioctl and pass it a magic number. If an array of
     * 8 comes back, its a 3.5 client. If not, its a 3.4 client.
     * If we get back EDOM, there are no cells in the kernel yet,
     * and we'll assume a 3.5 client.
     */
    tp = space;
    lp = (afs_int32 *) tp;
    *lp++ = 0;			/* first cell entry */
    *lp = 0x12345678;		/* magic */
    blob.out_size = AFS_PIOCTL_MAXSIZE;
    blob.in_size = sizeof(afs_int32) + sizeof(afs_int32);
    blob.in = space;
    blob.out = space;
    code = pioctl(0, VIOCGETCELL, &blob, 1);
    if (code < 0 && errno != EDOM) {
	Die(errno, 0);
	return 1;
    }
    if (code < 1 && errno == EDOM) {
	scount = AFS_MAXHOSTS;
    } else {
	tp = space;
	cellname = tp + AFS_MAXCELLHOSTS * sizeof(afs_int32);
	scount = ((cellname[0] != '\0') ? AFS_MAXCELLHOSTS : AFS_MAXHOSTS);
    }

    /* Now setup and do the NEWCELL pioctl call */
    memset(space, 0, (scount + 1) * sizeof(afs_int32));
    tp = space;
    lp = (afs_int32 *) tp;
    *lp++ = 0x12345678;
    tp += sizeof(afs_int32);
    for (ti = as->parms[1].items; ti; ti = ti->next) {
	thp = hostutil_GetHostByName(ti->data);
	if (!thp) {
	    fprintf(stderr,
		    "%s: Host %s not found in host table, skipping it.\n", pn,
		    ti->data);
	} else {
	    memcpy(tp, thp->h_addr, sizeof(afs_int32));
	    tp += sizeof(afs_int32);
	}
    }
    if (as->parms[2].items) {
	/*
	 * Link the cell, for the purposes of volume location, to the specified
	 * cell.
	 */
	cellname = as->parms[2].items->data;
	linkedstate = 1;
    }
#ifdef FS_ENABLE_SERVER_DEBUG_PORTS
    if (as->parms[3].items) {
	code = util_GetInt32(as->parms[3].items->data, &vlport);
	if (code) {
	    fprintf(stderr,
		    "%s: bad integer specified for the fileserver port.\n",
		    pn);
	    return 1;
	}
    }
    if (as->parms[4].items) {
	code = util_GetInt32(as->parms[4].items->data, &fsport);
	if (code) {
	    fprintf(stderr,
		    "%s: bad integer specified for the vldb server port.\n",
		    pn);
	    return 1;
	}
    }
#endif
    tp = (char *)(space + (scount + 1) * sizeof(afs_int32));
    lp = (afs_int32 *) tp;
    *lp++ = fsport;
    *lp++ = vlport;
    *lp = linkedstate;
    strcpy(space + ((scount + 4) * sizeof(afs_int32)),
	   as->parms[0].items->data);
    size = ((scount + 4) * sizeof(afs_int32))
	+ strlen(as->parms[0].items->data)
	+ 1 /* for null */ ;
    tp = (char *)(space + size);
    if (linkedstate) {
	strcpy(tp, cellname);
	size += strlen(cellname) + 1;
    }
    blob.in_size = size;
    blob.in = space;
    blob.out_size = 0;
    code = pioctl(0, VIOCNEWCELL, &blob, 1);
    if (code < 0) {
	Die(errno, 0);
	return 1;
    }
    return 0;
}

static int
NewAliasCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    char *tp;
    char *aliasName, *realName;

    /* Now setup and do the NEWCELL pioctl call */
    aliasName = as->parms[0].items->data;
    realName = as->parms[1].items->data;
    tp = space;
    strcpy(tp, aliasName);
    tp += strlen(aliasName) + 1;
    strcpy(tp, realName);
    tp += strlen(realName) + 1;

    blob.in_size = tp - space;
    blob.in = space;
    blob.out_size = 0;
    blob.out = space;
    code = pioctl(0, VIOC_NEWALIAS, &blob, 1);
    if (code < 0) {
	if (errno == EEXIST) {
	    fprintf(stderr,
		    "%s: cell name `%s' in use by an existing cell.\n", pn,
		    aliasName);
	} else {
	    Die(errno, 0);
	}
	return 1;
    }
    return 0;
}

static int
WhichCellCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct cmd_item *ti;
    int error = 0;
    char cell[MAXCELLCHARS];

    SetDotDefault(&as->parms[0].items);
    for (ti = as->parms[0].items; ti; ti = ti->next) {
	code = GetCell(ti->data, cell);
	if (code) {
	    error = 1;
	    continue;
	}

	printf("File %s lives in cell '%s'\n", ti->data, cell);
    }
    return error;
}

static int
WSCellCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;

    blob.in_size = 0;
    blob.in = NULL;
    blob.out_size = AFS_PIOCTL_MAXSIZE;
    blob.out = space;

    code = pioctl(NULL, VIOC_GET_WS_CELL, &blob, 1);
    if (code) {
	Die(errno, NULL);
	return 1;
    }

    printf("This workstation belongs to cell '%s'\n", space);
    return 0;
}

/*
static PrimaryCellCmd(as)
    struct cmd_syndesc *as;
{
    fprintf(stderr, "This command is obsolete, as is the concept of a primary token.\n");
    return 0;
}
*/

static int
MonitorCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    afs_int32 hostAddr;
    struct hostent *thp;
    char *tp;
    int setp;

    ti = as->parms[0].items;
    setp = 1;
    if (ti) {
	/* set the host */
	if (!strcmp(ti->data, "off"))
	    hostAddr = 0xffffffff;
	else {
	    thp = hostutil_GetHostByName(ti->data);
	    if (!thp) {
		if (!strcmp(ti->data, "localhost")) {
		    fprintf(stderr,
			    "localhost not in host table, assuming 127.0.0.1\n");
		    hostAddr = htonl(0x7f000001);
		} else {
		    fprintf(stderr, "host %s not found in host table.\n",
			    ti->data);
		    return 1;
		}
	    } else
		memcpy(&hostAddr, thp->h_addr, sizeof(afs_int32));
	}
    } else {
	hostAddr = 0;		/* means don't set host */
	setp = 0;		/* aren't setting host */
    }

    /* now do operation */
    blob.in_size = sizeof(afs_int32);
    blob.out_size = sizeof(afs_int32);
    blob.in = (char *)&hostAddr;
    blob.out = (char *)&hostAddr;
    code = pioctl(0, VIOC_AFS_MARINER_HOST, &blob, 1);
    if (code) {
	Die(errno, 0);
	return 1;
    }
    if (setp) {
	printf("%s: new monitor host set.\n", pn);
    } else {
	/* now decode old address */
	if (hostAddr == 0xffffffff) {
	    printf("Cache monitoring is currently disabled.\n");
	} else {
	    tp = hostutil_GetNameByINet(hostAddr);
	    printf("Using host %s for monitor services.\n", tp);
	}
    }
    return 0;
}

static int
SysNameCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    char *input = space;
    afs_int32 setp = 0;

    ti = as->parms[0].items;
    blob.in = space;
    blob.out = space;
    blob.out_size = AFS_PIOCTL_MAXSIZE;
    blob.in_size = sizeof(afs_int32);
    input += sizeof(afs_int32);
    for (; ti; ti = ti->next) {
	setp++;
	blob.in_size += strlen(ti->data) + 1;
	if (blob.in_size > AFS_PIOCTL_MAXSIZE) {
	    fprintf(stderr, "%s: sysname%s too long.\n", pn,
		    setp > 1 ? "s" : "");
	    return 1;
	}
	strcpy(input, ti->data);
	input += strlen(ti->data);
	*(input++) = '\0';
    }
    memcpy(space, &setp, sizeof(afs_int32));
    code = pioctl(0, VIOC_AFS_SYSNAME, &blob, 1);
    if (code) {
	Die(errno, 0);
	return 1;
    }
    if (setp) {
	printf("%s: new sysname%s set.\n", pn, setp > 1 ? " list" : "");
	return 0;
    }
    input = space;
    memcpy(&setp, input, sizeof(afs_int32));
    input += sizeof(afs_int32);
    if (!setp) {
	fprintf(stderr, "No sysname name value was found\n");
	return 1;
    }
    printf("Current sysname%s is", setp > 1 ? " list" : "");
    for (; setp > 0; --setp) {
	printf(" \'%s\'", input);
	input += strlen(input) + 1;
    }
    printf("\n");
    return 0;
}

static char *exported_types[] = { "null", "nfs", "" };
static int
ExportAfsCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    int export = 0, type = 0, mode = 0, exportcall, pwsync =
	0, smounts = 0, clipags = 0, pagcb = 0;

    ti = as->parms[0].items;
    if (strcmp(ti->data, "nfs") == 0)
	type = 0x71;		/* NFS */
    else {
	fprintf(stderr,
		"Invalid exporter type, '%s', Only the 'nfs' exporter is currently supported\n",
		ti->data);
	return 1;
    }
    ti = as->parms[1].items;
    if (ti) {
	if (strcmp(ti->data, "on") == 0)
	    export = 3;
	else if (strcmp(ti->data, "off") == 0)
	    export = 2;
	else {
	    fprintf(stderr, "Illegal argument %s\n", ti->data);
	    return 1;
	}
    }
    if ((ti = as->parms[2].items)) {	/* -noconvert */
	if (strcmp(ti->data, "on") == 0)
	    mode = 2;
	else if (strcmp(ti->data, "off") == 0)
	    mode = 3;
	else {
	    fprintf(stderr, "Illegal argument %s\n", ti->data);
	    return 1;
	}
    }
    if ((ti = as->parms[3].items)) {	/* -uidcheck */
	if (strcmp(ti->data, "on") == 0)
	    pwsync = 3;
	else if (strcmp(ti->data, "off") == 0)
	    pwsync = 2;
	else {
	    fprintf(stderr, "Illegal argument %s\n", ti->data);
	    return 1;
	}
    }
    if ((ti = as->parms[4].items)) {	/* -submounts */
	if (strcmp(ti->data, "on") == 0)
	    smounts = 3;
	else if (strcmp(ti->data, "off") == 0)
	    smounts = 2;
	else {
	    fprintf(stderr, "Illegal argument %s\n", ti->data);
	    return 1;
	}
    }
    if ((ti = as->parms[5].items)) {	/* -clipags */
	if (strcmp(ti->data, "on") == 0)
	    clipags = 3;
	else if (strcmp(ti->data, "off") == 0)
	    clipags = 2;
	else {
	    fprintf(stderr, "Illegal argument %s\n", ti->data);
	    return 1;
	}
    }
    if ((ti = as->parms[6].items)) {	/* -pagcb */
	if (strcmp(ti->data, "on") == 0)
	    pagcb = 3;
	else if (strcmp(ti->data, "off") == 0)
	    pagcb = 2;
	else {
	    fprintf(stderr, "Illegal argument %s\n", ti->data);
	    return 1;
	}
    }
    exportcall =
	(type << 24) | (pagcb << 10) | (clipags << 8) |
	(mode << 6) | (pwsync << 4) | (smounts << 2) | export;
    type &= ~0x70;
    /* make the call */
    blob.in = (char *)&exportcall;
    blob.in_size = sizeof(afs_int32);
    blob.out = (char *)&exportcall;
    blob.out_size = sizeof(afs_int32);
    code = pioctl(0, VIOC_EXPORTAFS, &blob, 1);
    if (code) {
	if (errno == ENODEV) {
	    fprintf(stderr,
		    "Sorry, the %s-exporter type is currently not supported on this AFS client\n",
		    exported_types[type]);
	} else {
	    Die(errno, 0);
	}
	return 1;
    }

    if (exportcall & 1) {
	printf("'%s' translator is enabled with the following options:\n",
	       exported_types[type]);
	printf("\tRunning in %s mode\n",
	       (exportcall & 2 ? "strict unix" :
		"convert owner mode bits to world/other"));
	printf("\tRunning in %s mode\n",
	       (exportcall & 4 ? "strict 'passwd sync'" :
		"no 'passwd sync'"));
	printf("\t%s\n",
	       (exportcall & 8 ? "Allow mounts of /afs/.. subdirs" :
		"Only mounts to /afs allowed"));
	printf("\t%s\n",
	       (exportcall & 16 ? "Client-assigned PAG's are used" :
		"Client-assigned PAG's are not used"));
	printf("\t%s\n",
	       (exportcall & 32 ?
		"Callbacks are made to get creds from new clients" :
		"Callbacks are not made to get creds from new clients"));
    } else {
	printf("'%s' translator is disabled\n", exported_types[type]);
    }
    return 0;
}


static int
GetCellCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    char cellName[MAXCELLCHARS];
    struct cmd_item *ti;
    struct a {
	afs_int32 stat;
	afs_int32 junk;
    } args;
    int error = 0;

    memset(&args, '\0', sizeof args);	/* avoid Purify UMR error */
    for (ti = as->parms[0].items; ti; ti = ti->next) {
	/* once per cell */
	blob.out_size = sizeof(args);
	blob.out = (caddr_t) & args;
	code = GetCellName(ti->data, &cellName[0], sizeof(cellName));
	if (code) {
	    error = 1;
	    continue;
	}
	blob.in_size = 1 + strlen(cellName);
	blob.in = cellName;
	code = pioctl(0, VIOC_GETCELLSTATUS, &blob, 1);
	if (code) {
	    if (errno == ENOENT)
		fprintf(stderr, "%s: the cell named '%s' does not exist\n",
			pn, cellName);
	    else
		Die(errno, cellName);
	    error = 1;
	    continue;
	}
	printf("Cell %s status: ", cellName);
	if (args.stat & 2)
	    printf("no setuid allowed");
	else
	    printf("setuid allowed");
	if (args.stat & 4)
	    printf(", using old VLDB");
	printf("\n");
    }
    return error;
}

static int
SetCellCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    struct a {
	afs_int32 stat;
	afs_int32 junk;
	char cname[64];
    } args;
    int error = 0;

    /* Check arguments. */
    if (as->parms[1].items && as->parms[2].items) {
	fprintf(stderr, "Cannot specify both -suid and -nosuid.\n");
	return 1;
    }

    /* figure stuff to set */
    args.stat = 0;
    args.junk = 0;

    if (!as->parms[1].items)
	args.stat |= 2;		/* default to -nosuid */

    /* set stat for all listed cells */
    for (ti = as->parms[0].items; ti; ti = ti->next) {
	/* once per cell */
	code = GetCellName(ti->data, &args.cname[0], sizeof(args.cname));
	if (code) {
	    error = 1;
	    continue;
	}
	blob.in_size = sizeof(args);
	blob.in = (caddr_t) & args;
	blob.out_size = 0;
	blob.out = (caddr_t) 0;
	code = pioctl(0, VIOC_SETCELLSTATUS, &blob, 1);
	if (code) {
	    Die(errno, args.cname);	/* XXX added cell name to Die() call */
	    error = 1;
	}
    }
    return error;
}

static int
GetCellName(char *cellName, char *buf, size_t buf_size)
{
    struct afsconf_dir *tdir;
    int code;

    tdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);
    if (!tdir) {
	fprintf(stderr,
		"Could not process files in configuration directory (%s).\n",
		AFSDIR_CLIENT_ETC_DIRPATH);
	return -1;
    }

    code = afsconf_GetCellName(tdir, cellName, buf, buf_size);
    if (code) {
	fprintf(stderr, "%s: cell %s not in %s\n", pn, cellName,
		AFSDIR_CLIENT_CELLSERVDB_FILEPATH);
	return code;
    }

    return 0;
}

static struct ViceIoctl gblob;
static int debug = 0;
/*
 * here follow some routines in support of the setserverprefs and
 * getserverprefs commands.  They are:
 * SetPrefCmd  "top-level" routine
 * addServer   adds a server to the list of servers to be poked into the
 *             kernel.  Will poke the list into the kernel if it threatens
 *             to get too large.
 * pokeServers pokes the existing list of servers and ranks into the kernel
 * GetPrefCmd  reads the Cache Manager's current list of server ranks
 */

/*
 * returns -1 if error message printed,
 * 0 on success,
 * errno value if error and no error message printed
 */
static int
pokeServers(void)
{
    int code;

    code = pioctl(0, VIOC_SETSPREFS, &gblob, 1);
    if (code && (errno == EINVAL)) {
	struct setspref *ssp;
	ssp = (struct setspref *)gblob.in;
	if (!(ssp->flags & DBservers)) {
	    gblob.in = (void *)&(ssp->servers[0]);
	    gblob.in_size -= ((char *)&(ssp->servers[0])) - (char *)ssp;
	    code = pioctl(0, VIOC_SETSPREFS33, &gblob, 1);
	    return code ? errno : 0;
	}
	fprintf(stderr,
		"This cache manager does not support VL server preferences.\n");
	return -1;
    }

    return code ? errno : 0;
}

/*
 * returns -1 if error message printed,
 * 0 on success,
 * errno value if error and no error message printed
 */
static int
addServer(char *name, afs_int32 rank)
{
    int t, code;
    struct setspref *ssp;
    struct spref *sp;
    struct hostent *thostent;
    int error = 0;

#ifndef MAXUSHORT
#ifdef MAXSHORT
#define MAXUSHORT ((unsigned short) 2*MAXSHORT+1)	/* assumes two's complement binary system */
#else
#define MAXUSHORT ((unsigned short) ~0)
#endif
#endif

    thostent = hostutil_GetHostByName(name);
    if (!thostent) {
	fprintf(stderr, "%s: couldn't resolve name.\n", name);
	return -1;
    }

    ssp = (struct setspref *)(gblob.in);

    for (t = 0; thostent->h_addr_list[t]; t++) {
	if (gblob.in_size > MAXINSIZE - sizeof(struct spref)) {
	    code = pokeServers();
	    if (code)
		error = code;
	    ssp->num_servers = 0;
	}

	sp = (struct spref *)(gblob.in + gblob.in_size);
	memcpy(&(sp->server.s_addr), thostent->h_addr_list[t],
	       sizeof(afs_uint32));
	sp->rank = (rank > MAXUSHORT ? MAXUSHORT : rank);
	gblob.in_size += sizeof(struct spref);
	ssp->num_servers++;

	if (debug)
	    fprintf(stderr, "adding server %s, rank %d, ip addr 0x%lx\n",
		    name, sp->rank, (long unsigned int) sp->server.s_addr);
    }

    return error;
}


static int
SetPrefCmd(struct cmd_syndesc *as, void *arock)
{
    FILE *infd;
    afs_int32 code;
    struct cmd_item *ti;
    char name[80];
    int rank;
    struct setspref *ssp;
    int error = 0;		/* -1 means error message printed,
				 * >0 means errno value for unprinted message */

    ssp = (struct setspref *)space;
    ssp->flags = 0;
    ssp->num_servers = 0;
    gblob.in_size = ((char *)&(ssp->servers[0])) - (char *)ssp;
    gblob.in = space;
    gblob.out = space;
    gblob.out_size = AFS_PIOCTL_MAXSIZE;


    if (geteuid()) {
	fprintf(stderr, "Permission denied: requires root access.\n");
	return 1;
    }

    ti = as->parms[2].items;	/* -file */
    if (ti) {
	if (debug)
	    fprintf(stderr, "opening file %s\n", ti->data);
	if (!(infd = fopen(ti->data, "r"))) {
	    perror(ti->data);
	    error = -1;
	} else {
	    while (fscanf(infd, "%79s%d", name, &rank) != EOF) {
		code = addServer(name, (unsigned short)rank);
		if (code)
		    error = code;
	    }
	}
    }

    ti = as->parms[3].items;	/* -stdin */
    if (ti) {
	while (scanf("%79s%d", name, &rank) != EOF) {
	    code = addServer(name, (unsigned short)rank);
	    if (code)
		error = code;
	}
    }

    for (ti = as->parms[0].items; ti; ti = ti->next) {	/* list of servers, ranks */
	if (ti) {
	    if (!ti->next) {
		break;
	    }
	    code = addServer(ti->data, (unsigned short)atol(ti->next->data));
	    if (code)
		error = code;
	    if (debug)
		printf("set fs prefs %s %s\n", ti->data, ti->next->data);
	    ti = ti->next;
	}
    }
    code = pokeServers();
    if (code)
	error = code;
    if (debug)
	printf("now working on vlservers, code=%d\n", code);

    ssp = (struct setspref *)space;
    ssp->flags = DBservers;
    ssp->num_servers = 0;
    gblob.in_size = ((char *)&(ssp->servers[0])) - (char *)ssp;
    gblob.in = space;

    for (ti = as->parms[1].items; ti; ti = ti->next) {	/* list of dbservers, ranks */
	if (ti) {
	    if (!ti->next) {
		break;
	    }
	    code = addServer(ti->data, (unsigned short)atol(ti->next->data));
	    if (code)
		error = code;
	    if (debug)
		printf("set vl prefs %s %s\n", ti->data, ti->next->data);
	    ti = ti->next;
	}
    }

    if (as->parms[1].items) {
	if (debug)
	    printf("now poking vlservers\n");
	code = pokeServers();
	if (code)
	    error = code;
    }

    if (error > 0)
	Die(error, 0);

    return error ? 1 : 0;
}



static int
GetPrefCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct cmd_item *ti;
    char *name, tbuffer[20];
    afs_int32 addr;
    FILE *outfd;
    int resolve;
    int vlservers = 0;
    struct ViceIoctl blob;
    struct sprefrequest *in;
    struct sprefinfo *out;
    int i;

    ti = as->parms[0].items;	/* -file */
    if (ti) {
	if (debug)
	    fprintf(stderr, "opening file %s\n", ti->data);
	if (!(outfd = freopen(ti->data, "w", stdout))) {
	    perror(ti->data);
	    return 1;
	}
    }

    ti = as->parms[1].items;	/* -numeric */
    resolve = !(ti);
    ti = as->parms[2].items;	/* -vlservers */
    vlservers |= (ti ? DBservers : 0);
    /*  ti = as->parms[3].items;   -cell */

    in = (struct sprefrequest *)space;
    in->offset = 0;

    do {
	blob.in_size = sizeof(struct sprefrequest);
	blob.in = (char *)in;
	blob.out = space;
	blob.out_size = AFS_PIOCTL_MAXSIZE;

	in->num_servers =
	    (AFS_PIOCTL_MAXSIZE - 2 * sizeof(short)) / sizeof(struct spref);
	in->flags = vlservers;

	do {
	    code = pioctl(0, VIOC_GETSPREFS, &blob, 1);
	    if (code) {
		if ((errno != E2BIG) || (2 * blob.out_size > 0x7FFF)) {
		    perror("getserverprefs pioctl");
		    if (blob.out != space) {
			free(blob.out);
		    }
		    return 1;
		}
		blob.out_size *= 2;
		if (blob.out == space)
		    blob.out = malloc(blob.out_size);
		else
		    blob.out = realloc(blob.out, blob.out_size);
	    }
	} while (code != 0);

	out = (struct sprefinfo *)blob.out;

	for (i = 0; i < out->num_servers; i++) {
	    if (resolve) {
		name = hostutil_GetNameByINet(out->servers[i].server.s_addr);
	    } else {
		addr = ntohl(out->servers[i].server.s_addr);
		snprintf(tbuffer, sizeof(tbuffer), "%d.%d.%d.%d", (addr >> 24) & 0xff,
			(addr >> 16) & 0xff, (addr >> 8) & 0xff, addr & 0xff);
		name = tbuffer;
	    }
	    printf("%-50s %5u\n", name, out->servers[i].rank);
	}

	in->offset = out->next_offset;
	if (blob.out != space) {
	    free(blob.out);
	}
    } while (in->offset > 0);

    return 0;
}

static int
StoreBehindCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code = 0;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    struct sbstruct tsb, tsb2;
    int verbose = 0;
    afs_int32 allfiles;
    char *t;
    int error = 0;
    int async_default = -1;

    tsb.sb_thisfile = -1;
    ti = as->parms[0].items;	/* -kbytes */
    if (ti) {
	if (!as->parms[1].items) {
	    fprintf(stderr, "%s: you must specify -files with -kbytes.\n",
		    pn);
	    return 1;
	}
	tsb.sb_thisfile = strtol(ti->data, &t, 10) * 1024;
	if ((tsb.sb_thisfile < 0) || (t != ti->data + strlen(ti->data))) {
	    fprintf(stderr, "%s: %s must be 0 or a positive number.\n", pn,
		    ti->data);
	    return 1;
	}
    }

    allfiles = tsb.sb_default = -1;	/* Don't set allfiles yet */
    ti = as->parms[2].items;	/* -allfiles */
    if (ti) {
	allfiles = strtol(ti->data, &t, 10) * 1024;
	if ((allfiles < 0) || (t != ti->data + strlen(ti->data))) {
	    fprintf(stderr, "%s: %s must be 0 or a positive number.\n", pn,
		    ti->data);
	    return 1;
	}
    }

    /* -verbose or -file only or no options */
    if (as->parms[3].items || (as->parms[1].items && !as->parms[0].items)
	|| (!as->parms[0].items && !as->parms[1].items
	    && !as->parms[2].items))
	verbose = 1;

    blob.in = (char *)&tsb;
    blob.in_size = sizeof(struct sbstruct);

    /* once per -file */
    for (ti = as->parms[1].items; ti; ti = ti->next) {
	/* Do this solely to see if the file is there */

        blob.out = space;
	blob.out_size = AFS_PIOCTL_MAXSIZE;
	code = pioctl(ti->data, VIOCWHEREIS, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
	    continue;
	}

	memset(&tsb2, 0, sizeof(tsb2));
	blob.out = (char *)&tsb2;
	blob.out_size = sizeof(struct sbstruct);
	code = pioctl(ti->data, VIOC_STORBEHIND, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
	    continue;
	}

	if (blob.out_size == sizeof(tsb2)) {
	    async_default = tsb2.sb_default;

	    if (verbose) {
		if (tsb2.sb_thisfile == -1) {
		    fprintf(stdout, "Will store %s according to default.\n",
			    ti->data);
	    } else {
		    fprintf(stdout,
			    "Will store up to %d kbytes of %s asynchronously.\n",
			    (tsb2.sb_thisfile / 1024), ti->data);
		}
	    }
	}
    }

    /* If no files - make at least one pioctl call, or
     * set the allfiles default if we need to.
     */
    if (async_default < 0 || (allfiles != -1)) {
	tsb.sb_default = allfiles;
        memset(&tsb2, 0, sizeof(tsb2));
	blob.out = (char *)&tsb2;
	blob.out_size = sizeof(struct sbstruct);
	code = pioctl(0, VIOC_STORBEHIND, &blob, 1);
	if (code) {
	    Die(errno, ((allfiles == -1) ? 0 : "-allfiles"));
	    error = 1;

	} else if (blob.out_size == sizeof(tsb2)) {
	    async_default = tsb2.sb_default;
	}
    }

    /* Having no arguments also reports the default store asynchrony */
    if (async_default >= 0 && verbose) {
	fprintf(stdout, "Default store asynchrony is %d kbytes.\n",
		(async_default / 1024));
    }

    return error;
}


static int
SetCryptCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code = 0, flag;
    struct ViceIoctl blob;
    char *tp;

    tp = as->parms[0].items->data;
    if (strcmp(tp, "on") == 0)
	flag = 1;
    else if (strcmp(tp, "off") == 0)
	flag = 0;
    else {
	fprintf(stderr, "%s: %s must be \"on\" or \"off\".\n", pn, tp);
	return EINVAL;
    }

    blob.in = (char *)&flag;
    blob.in_size = sizeof(flag);
    blob.out_size = 0;
    code = pioctl(0, VIOC_SETRXKCRYPT, &blob, 1);
    if (code)
	Die(errno, NULL);
    return 0;
}


static int
GetCryptCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code = 0, flag;
    struct ViceIoctl blob;
    char *tp;

    blob.in = NULL;
    blob.in_size = 0;
    blob.out_size = sizeof(flag);
    blob.out = space;

    code = pioctl(0, VIOC_GETRXKCRYPT, &blob, 1);

    if (code)
	Die(errno, NULL);
    else {
	tp = space;
	memcpy(&flag, tp, sizeof(afs_int32));
	printf("Security level is currently ");
	if (flag == 1)
	    printf("crypt (data security).\n");
	else
	    printf("clear.\n");
    }
    return 0;
}

static char *modenames[] = {
    "offline",
    "online",
    "readonly",  /* Not currently supported */
    "fetchonly", /* Not currently supported */
    "partial",   /* Not currently supported */
    NULL
};

static char *policynames[] = {
    "client",
    "server",
    "closer",  /* Not currently supported. */
    "manual",  /* Not currently supported. */
    NULL
};

static int
DisconCmd(struct cmd_syndesc *as, void *arock)
{
    struct cmd_item *ti;
    char *modename;
    char *policyname;
    int modelen, policylen;
    afs_int32 mode, policy, code, unixuid = 0;
    struct ViceIoctl blob;

    blob.in = NULL;
    blob.in_size = 0;

    space[0] = space[1] = space[2] = space[3] = 0;

    ti = as->parms[0].items;
    if (ti) {
	modename = ti->data;
	modelen = strlen(modename);
	for (mode = 0; modenames[mode] != NULL; mode++)
	    if (!strncasecmp(modename, modenames[mode], modelen))
		break;
	if (modenames[mode] == NULL)
	    printf("Unknown discon mode \"%s\"\n", modename);
	else {
	    space[0] = mode + 1;
	}
    }
    ti = as->parms[1].items;
    if (ti) {
	policyname = ti->data;
	policylen = strlen(policyname);
	for (policy = 0; policynames[policy] != NULL; policy++)
	    if (!strncasecmp(policyname, policynames[policy], policylen))
		break;
	if (policynames[policy] == NULL)
	    printf("Unknown discon mode \"%s\"\n", policyname);
	else {
	    space[1] = policy + 1;
	}
    }

    if (as->parms[2].items) {
    	space[2] = 1;
    	printf("force on\n");
    }

    ti = as->parms[3].items;
    if (ti) {
	code = util_GetInt32(ti->data, &unixuid);
	if (code) {
	    fprintf(stderr, "%s: bad integer specified for uid.\n", pn);
            return 1;
        }
	space[3] = unixuid;
    } else
	space[3] = 0;

    blob.in = space;
    blob.in_size = 4 * sizeof(afs_int32);

    blob.out_size = sizeof(mode);
    blob.out = space;
    code = pioctl(0, VIOC_DISCON, &blob, 1);
    if (code)
	Die(errno, NULL);
    else {
	memcpy(&mode, space, sizeof mode);
	if (mode < sizeof modenames / sizeof (char *))
	    printf("Discon mode is now \"%s\"\n", modenames[mode]);
	else
	    printf("Unknown discon mode %d\n", mode);
    }

    return 0;
}

#include "AFS_component_version_number.c"

int
main(int argc, char **argv)
{
    afs_int32 code;
    struct cmd_syndesc *ts;

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
#endif

    /* try to find volume location information */
    ts = cmd_CreateSyntax("getclientaddrs", GetClientAddrsCmd, NULL, 0,
			  "get client network interface addresses");
    cmd_CreateAlias(ts, "gc");

    ts = cmd_CreateSyntax("setclientaddrs", SetClientAddrsCmd, NULL, 0,
			  "set client network interface addresses");
    cmd_AddParm(ts, "-address", CMD_LIST, CMD_OPTIONAL | CMD_EXPANDS,
		"client network interfaces");
    cmd_CreateAlias(ts, "sc");

    ts = cmd_CreateSyntax("setserverprefs", SetPrefCmd, NULL, 0,
			  "set server ranks");
    cmd_AddParm(ts, "-servers", CMD_LIST, CMD_OPTIONAL | CMD_EXPANDS,
		"fileserver names and ranks");
    cmd_AddParm(ts, "-vlservers", CMD_LIST, CMD_OPTIONAL | CMD_EXPANDS,
		"VL server names and ranks");
    cmd_AddParm(ts, "-file", CMD_SINGLE, CMD_OPTIONAL,
		"input from named file");
    cmd_AddParm(ts, "-stdin", CMD_FLAG, CMD_OPTIONAL, "input from stdin");
    cmd_CreateAlias(ts, "sp");

    ts = cmd_CreateSyntax("getserverprefs", GetPrefCmd, NULL, 0,
			  "get server ranks");
    cmd_AddParm(ts, "-file", CMD_SINGLE, CMD_OPTIONAL,
		"output to named file");
    cmd_AddParm(ts, "-numeric", CMD_FLAG, CMD_OPTIONAL, "addresses only");
    cmd_AddParm(ts, "-vlservers", CMD_FLAG, CMD_OPTIONAL, "VL servers");
/*    cmd_AddParm(ts, "-cell", CMD_FLAG, CMD_OPTIONAL, "cellname"); */
    cmd_CreateAlias(ts, "gp");

    ts = cmd_CreateSyntax("setacl", SetACLCmd, NULL, 0, "set access control list");
    cmd_AddParm(ts, "-dir", CMD_LIST, 0, "directory");
    cmd_AddParm(ts, "-acl", CMD_LIST, 0, "access list entries");
    cmd_AddParm(ts, "-clear", CMD_FLAG, CMD_OPTIONAL, "clear access list");
    cmd_AddParm(ts, "-negative", CMD_FLAG, CMD_OPTIONAL,
		"apply to negative rights");
    parm_setacl_id = ts->nParms;
    cmd_AddParm(ts, "-id", CMD_FLAG, CMD_OPTIONAL,
		"initial directory acl (DFS only)");
    cmd_AddParm(ts, "-if", CMD_FLAG, CMD_OPTIONAL,
		"initial file acl (DFS only)");
    cmd_CreateAlias(ts, "sa");

    ts = cmd_CreateSyntax("listacl", ListACLCmd, NULL, 0,
			  "list access control list");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    parm_listacl_id = ts->nParms;
    cmd_AddParm(ts, "-id", CMD_FLAG, CMD_OPTIONAL, "initial directory acl");
    cmd_AddParm(ts, "-if", CMD_FLAG, CMD_OPTIONAL, "initial file acl");
    cmd_AddParm(ts, "-cmd", CMD_FLAG, CMD_OPTIONAL, "output as 'fs setacl' command");
    cmd_CreateAlias(ts, "la");

    ts = cmd_CreateSyntax("getcalleraccess", GetCallerAccess, NULL, 0,
            "list callers access");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    cmd_CreateAlias(ts, "gca");

    ts = cmd_CreateSyntax("cleanacl", CleanACLCmd, NULL, 0,
			  "clean up access control list");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");

    ts = cmd_CreateSyntax("copyacl", CopyACLCmd, NULL, 0,
			  "copy access control list");
    cmd_AddParm(ts, "-fromdir", CMD_SINGLE, 0,
		"source directory (or DFS file)");
    cmd_AddParm(ts, "-todir", CMD_LIST, 0,
		"destination directory (or DFS file)");
    cmd_AddParm(ts, "-clear", CMD_FLAG, CMD_OPTIONAL,
		"first clear dest access list");
    parm_copyacl_id = ts->nParms;
    cmd_AddParm(ts, "-id", CMD_FLAG, CMD_OPTIONAL, "initial directory acl");
    cmd_AddParm(ts, "-if", CMD_FLAG, CMD_OPTIONAL, "initial file acl");

    cmd_CreateAlias(ts, "ca");

    ts = cmd_CreateSyntax("flush", FlushCmd, NULL, 0, "flush file from cache");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    ts = cmd_CreateSyntax("flushmount", FlushMountCmd, NULL, 0,
			  "flush mount symlink from cache");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");

    ts = cmd_CreateSyntax("setvol", SetVolCmd, NULL, 0, "set volume status");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    cmd_AddParm(ts, "-max", CMD_SINGLE, CMD_OPTIONAL,
		"disk space quota in 1K units");
    cmd_AddParm(ts, "-offlinemsg", CMD_SINGLE, CMD_OPTIONAL,
		"offline message");
    cmd_CreateAlias(ts, "sv");

    ts = cmd_CreateSyntax("messages", MessagesCmd, NULL, 0,
			  "control Cache Manager messages");
    cmd_AddParm(ts, "-show", CMD_SINGLE, CMD_OPTIONAL,
		"[user|console|all|none]");

    ts = cmd_CreateSyntax("examine", ExamineCmd, NULL, 0, "display file/volume status");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    cmd_CreateAlias(ts, "lv");
    cmd_CreateAlias(ts, "listvol");

    ts = cmd_CreateSyntax("listquota", ListQuotaCmd, NULL, 0, "list volume quota");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    cmd_AddParm(ts, "-human", CMD_FLAG, CMD_OPTIONAL, "human-readable listing");
    cmd_CreateAlias(ts, "lq");

    ts = cmd_CreateSyntax("diskfree", DiskFreeCmd, NULL, 0,
			  "show server disk space usage");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    cmd_AddParm(ts, "-human", CMD_FLAG, CMD_OPTIONAL, "human-readable listing");
    cmd_CreateAlias(ts, "df");

    ts = cmd_CreateSyntax("quota", QuotaCmd, NULL, 0, "show volume quota usage");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");

    ts = cmd_CreateSyntax("lsmount", ListMountCmd, NULL, 0, "list mount point");
    cmd_AddParm(ts, "-dir", CMD_LIST, 0, "directory");

    ts = cmd_CreateSyntax("mkmount", MakeMountCmd, NULL, 0, "make mount point");
    cmd_AddParm(ts, "-dir", CMD_SINGLE, 0, "directory");
    cmd_AddParm(ts, "-vol", CMD_SINGLE, 0, "volume name");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    cmd_AddParm(ts, "-rw", CMD_FLAG, CMD_OPTIONAL, "force r/w volume");
    cmd_AddParm(ts, "-fast", CMD_FLAG, CMD_OPTIONAL,
		"don't check name with VLDB");

#if defined(AFS_CACHE_BYPASS)
	ts = cmd_CreateSyntax("bypassthreshold", BypassThresholdCmd, NULL, 0,
		"get/set cache bypass file size threshold");
	cmd_AddParm(ts, "-size", CMD_SINGLE, CMD_OPTIONAL, "file size");
#endif

/*

defect 3069

    cmd_AddParm(ts, "-root", CMD_FLAG, CMD_OPTIONAL, "create cellular mount point");
*/


    ts = cmd_CreateSyntax("rmmount", RemoveMountCmd, NULL, 0, "remove mount point");
    cmd_AddParm(ts, "-dir", CMD_LIST, 0, "directory");

    ts = cmd_CreateSyntax("checkservers", CheckServersCmd, NULL, 0,
			  "check local cell's servers");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell to check");
    cmd_AddParm(ts, "-all", CMD_FLAG, CMD_OPTIONAL, "check all cells");
    cmd_AddParm(ts, "-fast", CMD_FLAG, CMD_OPTIONAL,
		"just list, don't check");
    cmd_AddParm(ts, "-interval", CMD_SINGLE, CMD_OPTIONAL,
		"seconds between probes");

    ts = cmd_CreateSyntax("checkvolumes", CheckVolumesCmd, NULL, 0,
			  "check volumeID/name mappings");
    cmd_CreateAlias(ts, "checkbackups");


    ts = cmd_CreateSyntax("setcachesize", SetCacheSizeCmd, NULL, 0,
			  "set cache size");
    cmd_AddParm(ts, "-blocks", CMD_SINGLE, CMD_OPTIONAL,
		"size in 1K byte blocks (0 => reset)");
    cmd_CreateAlias(ts, "cachesize");

    cmd_AddParm(ts, "-reset", CMD_FLAG, CMD_OPTIONAL,
		"reset size back to boot value");

    ts = cmd_CreateSyntax("getcacheparms", GetCacheParmsCmd, NULL, 0,
			  "get cache usage info");
    cmd_AddParm(ts, "-files", CMD_FLAG, CMD_OPTIONAL, "Show cach files used as well");
    cmd_AddParm(ts, "-excessive", CMD_FLAG, CMD_OPTIONAL, "excessively verbose cache stats");

    ts = cmd_CreateSyntax("listcells", ListCellsCmd, NULL, 0,
			  "list configured cells");
    cmd_AddParm(ts, "-numeric", CMD_FLAG, CMD_OPTIONAL, "addresses only");

    cmd_CreateSyntax("listaliases", ListAliasesCmd, NULL, 0,
		     "list configured cell aliases");

    ts = cmd_CreateSyntax("setquota", SetQuotaCmd, NULL, 0, "set volume quota");
    cmd_AddParm(ts, "-path", CMD_SINGLE, CMD_OPTIONAL, "dir/file path");
    cmd_AddParm(ts, "-max", CMD_SINGLE, 0, "max quota in kbytes");
    cmd_CreateAlias(ts, "sq");

    ts = cmd_CreateSyntax("newcell", NewCellCmd, NULL, 0, "configure new cell");
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "cell name");
    cmd_AddParm(ts, "-servers", CMD_LIST, CMD_REQUIRED, "primary servers");
    cmd_AddParm(ts, "-linkedcell", CMD_SINGLE, CMD_OPTIONAL,
		"linked cell name");

    ts = cmd_CreateSyntax("newalias", NewAliasCmd, NULL, 0,
			  "configure new cell alias");
    cmd_AddParm(ts, "-alias", CMD_SINGLE, 0, "alias name");
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "real name of cell");

#ifdef FS_ENABLE_SERVER_DEBUG_PORTS
/*
 * Turn this on only if you wish to be able to talk to a server which is listening
 * on alternative ports. This is not intended for general use and may not be
 * supported in the cache manager. It is not a way to run two servers at the
 * same host, since the cache manager cannot properly distinguish those two hosts.
 */
    cmd_AddParm(ts, "-fsport", CMD_SINGLE, CMD_OPTIONAL,
		"cell's fileserver port");
    cmd_AddParm(ts, "-vlport", CMD_SINGLE, CMD_OPTIONAL,
		"cell's vldb server port");
#endif

    ts = cmd_CreateSyntax("whichcell", WhichCellCmd, NULL, 0, "list file's cell");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");

    ts = cmd_CreateSyntax("whereis", WhereIsCmd, NULL, 0, "list file's location");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");

    cmd_CreateSyntax("wscell", WSCellCmd, NULL, 0, "list workstation's cell");

/*
    ts = cmd_CreateSyntax("primarycell", PrimaryCellCmd, NULL, "obsolete (listed primary cell)");
*/

    ts = cmd_CreateSyntax("monitor", MonitorCmd, NULL, CMD_HIDDEN, "set cache monitor host address");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_OPTIONAL,
		"host name or 'off'");
    cmd_CreateAlias(ts, "mariner");

    ts = cmd_CreateSyntax("getcellstatus", GetCellCmd, NULL, 0, "get cell status");
    cmd_AddParm(ts, "-cell", CMD_LIST, 0, "cell name");

    ts = cmd_CreateSyntax("setcell", SetCellCmd, NULL, 0, "set cell status");
    cmd_AddParm(ts, "-cell", CMD_LIST, 0, "cell name");
    cmd_AddParm(ts, "-suid", CMD_FLAG, CMD_OPTIONAL, "allow setuid programs");
    cmd_AddParm(ts, "-nosuid", CMD_FLAG, CMD_OPTIONAL,
		"disallow setuid programs");

    ts = cmd_CreateSyntax("flushvolume", FlushVolumeCmd, NULL, 0,
			  "flush all data in volume");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");

    cmd_CreateSyntax("flushall", FlushAllVolumesCmd, NULL, 0, "flush all data from the cache");

    ts = cmd_CreateSyntax("sysname", SysNameCmd, NULL, 0,
			  "get/set sysname (i.e. @sys) value");
    cmd_AddParm(ts, "-newsys", CMD_LIST, CMD_OPTIONAL, "new sysname");

    ts = cmd_CreateSyntax("exportafs", ExportAfsCmd, NULL, 0,
			  "enable/disable translators to AFS");
    cmd_AddParm(ts, "-type", CMD_SINGLE, 0, "exporter name");
    cmd_AddParm(ts, "-start", CMD_SINGLE, CMD_OPTIONAL,
		"start/stop translator (on | off)");
    cmd_AddParm(ts, "-convert", CMD_SINGLE, CMD_OPTIONAL,
		"convert from afs to unix mode (on | off)");
    cmd_AddParm(ts, "-uidcheck", CMD_SINGLE, CMD_OPTIONAL,
		"run on strict 'uid check' mode (on | off)");
    cmd_AddParm(ts, "-submounts", CMD_SINGLE, CMD_OPTIONAL,
		"allow nfs mounts to subdirs of /afs/.. (on  | off)");
    cmd_AddParm(ts, "-clipags", CMD_SINGLE, CMD_OPTIONAL,
		"enable use of client-assigned PAG's (on  | off)");
    cmd_AddParm(ts, "-pagcb", CMD_SINGLE, CMD_OPTIONAL,
		"enable callbacks to get creds from new clients (on  | off)");


    ts = cmd_CreateSyntax("storebehind", StoreBehindCmd, NULL, 0,
			  "store to server after file close");
    cmd_AddParm(ts, "-kbytes", CMD_SINGLE, CMD_OPTIONAL,
		"asynchrony for specified names");
    cmd_AddParm(ts, "-files", CMD_LIST, CMD_OPTIONAL, "specific pathnames");
    cmd_AddParm(ts, "-allfiles", CMD_SINGLE, CMD_OPTIONAL,
		"new default (KB)");
    cmd_AddParm(ts, "-verbose", CMD_FLAG, CMD_OPTIONAL, "show status");
    cmd_CreateAlias(ts, "sb");

    ts = cmd_CreateSyntax("setcrypt", SetCryptCmd, NULL, 0,
			  "set cache manager encryption flag");
    cmd_AddParm(ts, "-crypt", CMD_SINGLE, 0, "on or off");

    cmd_CreateSyntax("getcrypt", GetCryptCmd, NULL, 0,
			  "get cache manager encryption flag");

    ts = cmd_CreateSyntax("rxstatproc", RxStatProcCmd, NULL, 0,
			  "Manage per process RX statistics");
    cmd_AddParm(ts, "-enable", CMD_FLAG, CMD_OPTIONAL, "Enable RX stats");
    cmd_AddParm(ts, "-disable", CMD_FLAG, CMD_OPTIONAL, "Disable RX stats");
    cmd_AddParm(ts, "-clear", CMD_FLAG, CMD_OPTIONAL, "Clear RX stats");

    ts = cmd_CreateSyntax("rxstatpeer", RxStatPeerCmd, NULL, 0,
			  "Manage per peer RX statistics");
    cmd_AddParm(ts, "-enable", CMD_FLAG, CMD_OPTIONAL, "Enable RX stats");
    cmd_AddParm(ts, "-disable", CMD_FLAG, CMD_OPTIONAL, "Disable RX stats");
    cmd_AddParm(ts, "-clear", CMD_FLAG, CMD_OPTIONAL, "Clear RX stats");

    ts = cmd_CreateSyntax("setcbaddr", CallBackRxConnCmd, NULL, 0, "configure callback connection address");
    cmd_AddParm(ts, "-addr", CMD_SINGLE, CMD_OPTIONAL, "host name or address");

    /* try to find volume location information */
    ts = cmd_CreateSyntax("getfid", GetFidCmd, NULL, 0,
			  "get fid for file(s)");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    cmd_AddParm(ts, "-literal", CMD_FLAG, CMD_OPTIONAL,
		"literal evaluation of mountpoints and symlinks");

    ts = cmd_CreateSyntax("discon", DisconCmd, NULL, 0,
			  "disconnection mode");
    cmd_AddParm(ts, "-mode", CMD_SINGLE, CMD_REQUIRED, "offline | online");
    cmd_AddParm(ts, "-policy", CMD_SINGLE, CMD_OPTIONAL, "client | server");
    cmd_AddParm(ts, "-force", CMD_FLAG, CMD_OPTIONAL, "Force reconnection, despite any synchronization issues.");
    cmd_AddParm(ts, "-uid", CMD_SINGLE, CMD_OPTIONAL, "Numeric UID of user whose tokens to use at reconnect.");

    ts = cmd_CreateSyntax("nukenfscreds", NukeNFSCredsCmd, NULL, 0, "nuke credentials for NFS client");
    cmd_AddParm(ts, "-addr", CMD_SINGLE, 0, "host name or address");

    ts = cmd_CreateSyntax("uuid", UuidCmd, NULL, 0, "manage the UUID for the cache manager");
    cmd_AddParm(ts, "-generate", CMD_FLAG, CMD_REQUIRED, "generate a new UUID");

    ts = cmd_CreateSyntax("precache", PreCacheCmd, NULL, 0,
			  "set precache size");
    cmd_AddParm(ts, "-blocks", CMD_SINGLE, CMD_OPTIONAL,
		"size in 1K byte blocks (0 => disable)");

    code = cmd_Dispatch(argc, argv);
    if (rxInitDone)
	rx_Finalize();

    return code;
}

static void
Die(int errnum, char *filename)
{
    switch (errnum) {
    case EINVAL:
	if (filename)
	    fprintf(stderr,
		    "%s: Invalid argument; it is possible that %s is not in AFS.\n",
		    pn, filename);
	else
	    fprintf(stderr, "%s: Invalid argument.\n", pn);
	break;
    case ENOENT:
	if (filename)
	    fprintf(stderr, "%s: File '%s' doesn't exist\n", pn, filename);
	else
	    fprintf(stderr, "%s: no such file returned\n", pn);
	break;
    case EROFS:
	fprintf(stderr,
		"%s: You can not change a backup or readonly volume\n", pn);
	break;
    case EACCES:
    case EPERM:
	if (filename)
	    fprintf(stderr,
		    "%s: You don't have the required access rights on '%s'\n",
		    pn, filename);
	else
	    fprintf(stderr,
		    "%s: You do not have the required rights to do this operation\n",
		    pn);
	break;
    case ESRCH: /* hack: pioctls stole ESRCH for cell name not available errors. */
	fprintf(stderr, "%s: Cell name not recognized.\n", pn);
	break;
    default:
	if (filename)
	    fprintf(stderr, "%s:'%s'", pn, filename);
	else
	    fprintf(stderr, "%s", pn);
	fprintf(stderr, ": %s\n", afs_error_message(errnum));
	break;
    }
}

/* get clients interface addresses */
static int
GetClientAddrsCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct sprefrequest *in;
    struct sprefinfo *out;

    in = (struct sprefrequest *)space;
    in->offset = 0;

    do {
	blob.in_size = sizeof(struct sprefrequest);
	blob.in = (char *)in;
	blob.out = space;
	blob.out_size = AFS_PIOCTL_MAXSIZE;

	in->num_servers =
	    (AFS_PIOCTL_MAXSIZE - 2 * sizeof(short)) / sizeof(struct spref);
	/* returns addr in network byte order */
	code = pioctl(0, VIOC_GETCPREFS, &blob, 1);
	if (code) {
	    perror("getClientInterfaceAddr pioctl");
	    return 1;
	}

	{
	    int i;
	    out = (struct sprefinfo *)blob.out;
	    for (i = 0; i < out->num_servers; i++) {
		afs_int32 addr;
		char tbuffer[20];
		addr = ntohl(out->servers[i].server.s_addr);
		snprintf(tbuffer, sizeof(tbuffer), "%d.%d.%d.%d", (addr >> 24) & 0xff,
			(addr >> 16) & 0xff, (addr >> 8) & 0xff, addr & 0xff);
		printf("%-50s\n", tbuffer);
	    }
	    in->offset = out->next_offset;
	}
    } while (out->next_offset > 0);

    return 0;
}

static int
SetClientAddrsCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code, addr;
    struct cmd_item *ti;
    struct ViceIoctl blob;
    struct setspref *ssp;
    int sizeUsed = 0, i, flag;
    afs_uint32 existingAddr[1024];	/* existing addresses on this host */
    int existNu;
    int error = 0;

    ssp = (struct setspref *)space;
    ssp->num_servers = 0;
    blob.in = space;
    blob.out = space;
    blob.out_size = AFS_PIOCTL_MAXSIZE;

    if (geteuid()) {
	fprintf(stderr, "Permission denied: requires root access.\n");
	return 1;
    }

    /* extract all existing interface addresses */
    existNu = rx_getAllAddr(existingAddr, 1024);
    if (existNu < 0)
	return 1;

    sizeUsed = sizeof(struct setspref);	/* space used in ioctl buffer */
    for (ti = as->parms[0].items; ti; ti = ti->next) {
	if (sizeUsed >= sizeof(space)) {
	    fprintf(stderr, "No more space\n");
	    return 1;
	}
	addr = extractAddr(ti->data, 20);	/* network order */
	if ((addr == AFS_IPINVALID) || (addr == AFS_IPINVALIDIGNORE)) {
	    fprintf(stderr, "Error in specifying address: %s..ignoring\n",
		    ti->data);
	    error = 1;
	    continue;
	}
	/* see if it is an address that really exists */
	for (flag = 0, i = 0; i < existNu; i++)
	    if (existingAddr[i] == addr) {
		flag = 1;
		break;
	    }
	if (!flag) {		/* this is an nonexistent address */
	    fprintf(stderr, "Nonexistent address: 0x%08x..ignoring\n", addr);
	    error = 1;
	    continue;
	}
	/* copy all specified addr into ioctl buffer */
	(ssp->servers[ssp->num_servers]).server.s_addr = addr;
	printf("Adding 0x%08x\n", addr);
	ssp->num_servers++;
	sizeUsed += sizeof(struct spref);
    }
    if (ssp->num_servers < 1) {
	fprintf(stderr, "No addresses specified\n");
	return 1;
    }
    blob.in_size = sizeUsed - sizeof(struct spref);

    code = pioctl(0, VIOC_SETCPREFS, &blob, 1);	/* network order */
    if (code) {
	Die(errno, 0);
	error = 1;
    }

    return error;
}

static int
FlushMountCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    char *last_component;
    char *parent_dir;
    int error = 0;

    for (ti = as->parms[0].items; ti; ti = ti->next) {
	if (GetLastComponent(ti->data, &parent_dir,
			     &last_component, NULL, 0) != 0) {
	    error = 1;
	    continue;
	}

	blob.in = last_component;
	blob.in_size = strlen(last_component) + 1;
	blob.out_size = 0;

	code = pioctl(parent_dir, VIOC_AFS_FLUSHMOUNT, &blob, 1);

	free(last_component);

	if (code != 0) {
	    if (errno == EINVAL) {
		fprintf(stderr, "'%s' is not a mount point.\n", ti->data);
	    } else {
		Die(errno, (ti->data ? ti->data : parent_dir));
	    }
	    error = 1;
	}
	free(parent_dir);
    }
    return error;
}

static int
RxStatProcCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    afs_int32 flags = 0;
    struct ViceIoctl blob;

    if (as->parms[0].items) {	/* -enable */
	flags |= AFSCALL_RXSTATS_ENABLE;
    }
    if (as->parms[1].items) {	/* -disable */
	flags |= AFSCALL_RXSTATS_DISABLE;
    }
    if (as->parms[2].items) {	/* -clear */
	flags |= AFSCALL_RXSTATS_CLEAR;
    }
    if (flags == 0) {
	fprintf(stderr, "You must specify at least one argument\n");
	return 1;
    }

    blob.in = (char *)&flags;
    blob.in_size = sizeof(afs_int32);
    blob.out_size = 0;

    code = pioctl(NULL, VIOC_RXSTAT_PROC, &blob, 1);
    if (code != 0) {
	Die(errno, NULL);
	return 1;
    }

    return 0;
}

static int
RxStatPeerCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    afs_int32 flags = 0;
    struct ViceIoctl blob;

    if (as->parms[0].items) {	/* -enable */
	flags |= AFSCALL_RXSTATS_ENABLE;
    }
    if (as->parms[1].items) {	/* -disable */
	flags |= AFSCALL_RXSTATS_DISABLE;
    }
    if (as->parms[2].items) {	/* -clear */
	flags |= AFSCALL_RXSTATS_CLEAR;
    }
    if (flags == 0) {
	fprintf(stderr, "You must specify at least one argument\n");
	return 1;
    }

    blob.in = (char *)&flags;
    blob.in_size = sizeof(afs_int32);
    blob.out_size = 0;

    code = pioctl(NULL, VIOC_RXSTAT_PEER, &blob, 1);
    if (code != 0) {
	Die(errno, NULL);
	return 1;
    }

    return 0;
}

static int
GetFidCmd(struct cmd_syndesc *as, void *arock)
{
    struct ViceIoctl blob;
    struct cmd_item *ti;

    afs_int32 code;
    int error = 0, literal = 0;
    char cell[MAXCELLCHARS];

    SetDotDefault(&as->parms[0].items);
    if (as->parms[1].items) {
	/* -literal */
	literal = 1;
    }
    for (ti = as->parms[0].items; ti; ti = ti->next) {
        struct VenusFid vfid;
	char *parent_dir = NULL;
	char *last_component = NULL;

        blob.out_size = sizeof(struct VenusFid);
        blob.out = (char *) &vfid;
        blob.in_size = 0;

	if (literal) {
	    if (GetLastComponent(ti->data, &parent_dir,
				 &last_component, NULL, literal) != 0) {
		error = 1;
		goto next_item;
	    }

	    blob.in = last_component;
	    blob.in_size = strlen(last_component) + 1;
	    code = pioctl(parent_dir, VIOC_GETLITERALFID, &blob, 1);

	} else {
	    code = pioctl(ti->data, VIOCGETFID, &blob, 1);
	}
        if (code) {
            Die(errno,ti->data);
            error = 1;
	    goto next_item;
        }

	if (literal) {
	    code = GetCell(parent_dir, cell);
	} else {
	    code = GetCell(ti->data, cell);
	}
        if (code) {
	    strlcpy(cell, "unknown-cell", sizeof(cell));
        }

        printf("File %s (%u.%u.%u) located in cell %s\n",
               ti->data, vfid.Fid.Volume, vfid.Fid.Vnode, vfid.Fid.Unique,
               cell);

 next_item:
	free(parent_dir);
	free(last_component);
    }

    return error;
}

