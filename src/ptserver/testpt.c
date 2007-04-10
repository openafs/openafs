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

#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <WINNT/afsevent.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <rx/rx.h>
#include <rx/xdr.h>
#include <afs/rxgen_consts.h>
#include <afs/cmd.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include "ptclient.h"
#include "pterror.h"
#include <afs/afsutil.h>
#include <afs/com_err.h>

static char *whoami = "testpr";
static struct afsconf_dir *conf;	/* cell info, set by MyBeforeProc */
static char conf_dir[100];
static char lcell[MAXCELLCHARS];

int
ListUsedIds(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    namelist lnames;
    idlist lids;
    int i, j;
    int group = 0;		/* check groups */
    int unused = 0;		/* print unused */
    int number = 100;		/* check 100 ids */
    afs_int32 startId = 1;
    afs_int32 maxId;
    int range;

    if (as->parms[0].items)
	startId = atoi(as->parms[0].items->data);
    if (as->parms[1].items)
	number = atoi(as->parms[1].items->data);
    if (as->parms[2].items)
	unused = 1;

    code = pr_Initialize(1, conf_dir, NULL);
    if (code) {
	afs_com_err(whoami, code, "initializing pruser");
	exit(1);
    }
    if (startId < 0) {
	group = 1;
	code = pr_ListMaxGroupId(&maxId);
	if (code) {
	  bad_max:
	    afs_com_err(whoami, code, "getting maximum id");
	    exit(2);
	}
	if (startId < maxId) {
	    fprintf(stderr, "Max group id is only %d.\n", maxId);
	    exit(3);
	}
    } else {
	code = pr_ListMaxUserId(&maxId);
	if (code)
	    goto bad_max;
	if (startId > maxId) {
	    fprintf(stderr, "Max user id is only %d.\n", maxId);
	    exit(3);
	}
    }
    range = abs(startId - maxId);
    if (range < 0)
	range = -range;
    range++;			/* number that can be printed */
    if (range < number) {
	fprintf(stderr, "Only %d ids to be checked.\n", range);
	number = range;
    }

    printf("Checking for %d %sused ids starting at %d.\n", number,
	   (unused ? "un" : ""), startId);
#define NUM 100
    lids.idlist_val = (afs_int32 *) malloc(sizeof(afs_int32) * NUM);
    lnames.namelist_len = 0;
    lnames.namelist_val = 0;
    while (number) {
	if (number < NUM)
	    i = number;
	else
	    i = NUM;
	for (j = 0; j < i; j++) {
	    lids.idlist_val[j] = startId;
	    if (group)
		startId--;
	    else
		startId++;
	}
	lids.idlist_len = i;
	code = pr_IdToName(&lids, &lnames);
	if (code) {
	    afs_com_err(whoami, code, "converting id to name");
	    exit(2);
	}
	for (j = 0; j < lnames.namelist_len; j++) {
	    if (lids.idlist_val[j] == atoi(lnames.namelist_val[j])) {
		if (unused)
		    printf("%s is free\n", lnames.namelist_val[j]);
	    } else {
		if (!unused)
		    printf("%s is id %d\n", lnames.namelist_val[j],
			   lids.idlist_val[j]);
	    }
	}
	number -= i;
    }
    if (lids.idlist_val)
	free(lids.idlist_val);
    if (lnames.namelist_val)
	free(lnames.namelist_val);
    return 0;
}

/* TestManyMembers - called with a number N.  Try creating N users and N groups
 * and put all the users on one of the groups and one of the users on all the
 * groups.  Also put many users on many groups.
 *
 * To keep track of this create an NxN matrix of membership and fill it in with
 * a function that looks like a quarter of a circle.  That makes the first
 * group contain every user and the first user be a member of every group. */

int verbose;
char callerName[PR_MAXNAMELEN];
afs_int32 callerId;
afs_int32 lastGroup;		/* id of last group created */
afs_int32 ownerUser;		/* first created user */
char ownerUserName[PR_MAXNAMELEN];	/*  " " " name */
int steepDropOff;		/* precentage decreate in GroupLimit */
char *createPrefix;		/* prefix for naming users&groups */
extern struct ubik_client *pruclient;	/* initialized by pr_Initialize */

/* These variables form the state if this test */
int number;			/* max number of members */
char *population;		/* matrix of memberships */
afs_int32 *users;		/* ids of users */
afs_int32 *groups;		/* ids of groups */
afs_int32 *groupOwners;		/* ids of owners of groups */

/* statistics */
int nUsers, nGroups, nAdds, nRems, nUDels, nGDels;

int
IdCmp(afs_int32 *a, afs_int32 *b)
{
    if (*a > *b) {
	return 1;
    } else if (*a == *b) {
	return 0;
    } else /* (*a < *b) */ {
	return -1;
    }
}

static int
sqr(int n)
{
    return n * n;
}

static int
GetGroupLimit(int N, int x)
{
    int y;
    double sqrt();

    if ((x >= N) || (x < 0)) {
	printf("GetGroupLimit: input value out of range %d (%d)\n", x, N);
	exit(10);
    }
    if (steepDropOff) {		/* Use exponential decrease */
	int i;
	y = N;
	for (i = 0; i < x; i++) {
	    y = (y * steepDropOff) / 100;	/* parameter is a percentage */
	    if (y == 0) {
		y = 1;		/* with a floor of 1 */
		break;
	    }
	}
    } else {			/* Use a circle's third quadrant */
	y = sqr(N - 1) - sqr(N - 1 - x);
	y = (int)(sqrt((double)y) + 0.5);	/* round off */
	y = N - y;
    }
    if ((y > N) || (y < 1)) {
	printf("filling value out of range: %d (%d) => %d\n", x, N, y);
	exit(11);
    }
    return y;
}

void
CreateUser(int u)
{
    afs_int32 code;
    char name[16];
    afs_int32 id;

    sprintf(name, "%s%d", createPrefix, u);
    id = 0;
    code = pr_CreateUser(name, &id);
    if (code) {
	if (code == PREXIST) {
	    code = pr_Delete(name);
	    if (code == 0) {
		nUDels++;
		code = pr_CreateUser(name, &id);
		if (code == 0) {
		    if (verbose)
			printf("RE-");
		    goto done;
		}
	    }
	}
	afs_com_err(whoami, code, "couldn't create %s", name);
	exit(12);
    }
  done:
    if (verbose)
	printf("Creating user %s (%di)\n", name, id);
    users[u] = id;
    nUsers++;

    if (ownerUser == 0) {
	ownerUser = id;
	strcpy(ownerUserName, name);
    }
}

void
CreateGroup(int g)
{
    afs_int32 code;
    char name[16];
    afs_int32 id = 0;
    afs_int32 flags = PRGRP;
    afs_int32 owner;
    char *ownerName;
    int ownerType;		/* type of ownership */
    static char *lastGroupPrefix;	/* prefix used for type==2 */

    /* At least 50 groups should be owned by another group to test long owner
     * chains during deletion.  Also let's create some long owners of owners
     * lists.  */
    ownerType = random() % 3;

    if (!ownerUser)
	ownerType = 0;
    if (!lastGroup)
	ownerType = 0;
    switch (ownerType) {
    case 0:
	owner = callerId;
	ownerName = callerName;
	break;
    case 1:
	owner = ownerUser;
	ownerName = ownerUserName;
	break;
    case 2:
	owner = lastGroup;
	ownerName = lastGroupPrefix;
	break;
    }

    sprintf(name, "%s:%s%d", ownerName, createPrefix, g);
    code = ubik_Call(PR_NewEntry, pruclient, 0, name, PRGRP, owner, &id);
    if (code) {
	if (code == PREXIST) {
	    code = pr_Delete(name);
	    if (code == 0) {
		nGDels++;
		code =
		    ubik_Call(PR_NewEntry, pruclient, 0, name, PRGRP, owner,
			      &id);
		if (code == 0) {
		    if (verbose)
			printf("RE-");
		    goto done;
		}
	    }
	}
	afs_com_err(whoami, code, "couldn't create %s w/ owner=%d", name, owner);
	exit(13);
    }
  done:
    if (verbose)
	printf("Creating group %s (%di)\n", name, id);
    groups[g] = id;
    groupOwners[g] = owner;
    nGroups++;
    if (!lastGroup || (ownerType == 2)) {
	lastGroup = id;
	lastGroupPrefix = ownerName;
    }
}

int
DeleteRandomId(afs_int32 *list)
{
    afs_int32 code;
    afs_int32 id;
    int j, k;
    int m;

    k = random();		/* random starting point */
    for (j = 0; j < number; j++) {	/* find an undeleted id */
	m = (k + j) % number;
	if (id = list[m]) {
	    code = ubik_Call(PR_Delete, pruclient, 0, id);
	    if (code) {
		afs_com_err(whoami, code, "Couldn't delete %di", id);
		exit(22);
	    }
	    list[m] = 0;
	    if (list == users)
		nUDels++;
	    else
		nGDels++;
	    return 0;
	}
    }
    return -1;			/* none left */
}

void
AddUser(int u, int g)
{
    afs_int32 code;
    afs_int32 ui, gi;

    if (users[u] == 0)		/* create if necessary */
	CreateUser(u);
    if (groups[g] == 0)		/* create group if necessary */
	CreateGroup(g);
    ui = users[u];
    gi = groups[g];
    code = ubik_Call(PR_AddToGroup, pruclient, 0, ui, gi);
    if (code) {
	afs_com_err(whoami, code, "couldn't add %d to %d", ui, gi);
	exit(14);
    }
    if (verbose)
	printf("Adding user (%di) to group (%di)\n", ui, gi);
    population[u * number + g]++;
    nAdds++;
}

void
RemUser(int u, int g)
{
    afs_int32 code;
    afs_int32 ui, gi;

    ui = users[u];
    gi = groups[g];
    code = ubik_Call(PR_RemoveFromGroup, pruclient, 0, ui, gi);
    if (code) {
	afs_com_err(whoami, code, "couldn't remove %d from %d", ui, gi);
	exit(14);
    }
    if (verbose)
	printf("Removing user (%di) from group (%di)\n", ui, gi);
    population[u * number + g]--;
    nRems++;
}

int
TestManyMembers(struct cmd_syndesc *as, char *arock)
{
    char *filled;		/* users filled up */
    char *cleaned;		/* users cleaned up */

    int nFilled, nCleaned;
    int u, g, i, j, n;
    int seed;			/* random number generator seed */

    afs_int32 *glist;		/* membership list */

    afs_int32 code;

    code = pr_Initialize(1, conf_dir, NULL);
    if (code) {
	afs_com_err(whoami, code, "initializing pruser");
	exit(1);
    }
    /* get name of person running command */
    {
	struct ktc_principal afs, user;
	struct ktc_token token;

	strcpy(afs.name, "afs");
	strcpy(afs.instance, "");
	code = afsconf_GetLocalCell(conf, afs.cell, sizeof(afs.cell));
	if (code)
	    exit(2);
	code = ktc_GetToken(&afs, &token, sizeof(token), &user);
	if (code) {
	    afs_com_err(whoami, code, "getting afs tokens");
	    exit(3);
	}
	if (strlen(user.instance) > 0) {
	    fprintf(stderr, "can't handle non-null instance %s.%s\n",
		    user.name, user.cell);
	    exit(4);
	}
	if (strncmp(user.name, "AFS ID ", 7) == 0) {
	    callerId = atoi(user.name + 7);
	    code = pr_SIdToName(callerId, callerName);
	    if (code) {
		afs_com_err(whoami, code, "call get name for id %d", callerId);
		exit(6);
	    }
	} else {
	    strcpy(callerName, user.name);
	    code = pr_SNameToId(callerName, &callerId);
	    if ((code == 0) && (callerId == ANONYMOUSID))
		code = PRNOENT;
	}
#if 0				/* don't create user */
	if (code == PRNOENT) {
	    callerId = 0;
	    code = pr_CreateUser(callerName, &callerId);
	    if (code) {
		afs_com_err(whoami, code, "can't create caller %s", callerName);
		exit(5);
	    }
	    printf("Creating caller %s (%di)\n", callerName, callerId);
	}
	/* else */
#endif
	if (code) {
	    afs_com_err(whoami, code, "can't find caller %s", callerName);
	    exit(6);
	} else
	    printf("Assuming caller is %s (%di)\n", callerName, callerId);
    }

    /* Parse arguments */
    if (as->parms[0].items)
	number = atoi(as->parms[0].items->data);
    if (as->parms[1].items) {
	steepDropOff = atoi(as->parms[1].items->data);
	if ((steepDropOff < 0) || (steepDropOff > 100)) {
	    fprintf(stderr,
		    "Illegal value for dropoff: %d, must be between 0 and 100, inclusive.\n",
		    steepDropOff);
	    exit(7);
	}
    } else
	steepDropOff = 0;	/* use quadratic dropoff */
    if (as->parms[2].items)
	createPrefix = as->parms[2].items->data;
    else
	createPrefix = "u";
    if (as->parms[3].items)
	verbose = 1;
    else
	verbose = 0;
    if (as->parms[4].items)
	seed = atoi(as->parms[4].items->data);
    else
	seed = 1;

    srandom(seed);

    users = (afs_int32 *) malloc(number * sizeof(afs_int32));
    groups = (afs_int32 *) malloc(number * sizeof(afs_int32));
    filled = (char *)malloc(number * sizeof(char));
    cleaned = (char *)malloc(number * sizeof(char));
    population = (char *)malloc(sqr(number) * sizeof(char));

    nFilled = 0;
    memset(filled, 0, number);
    nCleaned = 0;
    memset(cleaned, 0, number);
    memset(population, 0, sqr(number));
    memset(users, 0, number * sizeof(afs_int32));
    memset(groups, 0, number * sizeof(afs_int32));

    ownerUser = lastGroup = 0;
    groupOwners = (afs_int32 *) malloc(number * sizeof(afs_int32));
    nUsers = nGroups = nAdds = nRems = nUDels = nGDels = 0;

    while ((nFilled < number) || (nCleaned < number)) {
	/* pick a user at random, using  */
	u = random() % number;
	if (!filled[u]) {
	    n = GetGroupLimit(number, u);	/* get group limit for that user */
	    g = random() % (n + 1);	/* pick a random group */
	    if (g == n) {	/* in a few cases create any user */
		n = number;	/* in the whole range */
		g = random() % n;
	    }
	    for (i = 0; i < n; i++) {	/* rotate until unused one found */
		j = (g + i) % n;
		if (!population[u * number + j]) {
		    /* add this user/group membership */
		    AddUser(u, j);
		    goto added;
		}
	    }
	    filled[u]++;
	    nFilled++;
	  added:;
	}
	if (!cleaned[u]) {
	    int base;
	    if (filled[u]) {	/* only clean above GroupLimit */
		base = GetGroupLimit(number, u);
		n = number - base;
		if (n == 0)
		    goto iscleaned;
		g = random() % n;
	    } else {
		base = 0;
		n = number;	/* pick a group from the whole range */
		g = random() % 2 * n;	/* at random for removal */
		if (g >= n)
		    goto remed;	/* but half the time do nothing */
	    }
	    for (i = 0; i < n; i++) {	/* rotate until used one found */
		j = (g + i) % n + base;
		if (population[u * number + j]) {
		    /* remove this user/group membership */
		    RemUser(u, j);
		    goto remed;
		}
	    }
	    if (filled[u]) {	/* track finished ones */
	      iscleaned:
		cleaned[u]++;
		nCleaned++;
	    }
	  remed:;
	}
    }

    /* check the membership list of all users for correctness */
    printf("Starting check of memberships\n");
    glist = (afs_int32 *) malloc(number * sizeof(afs_int32));
    for (u = 0; u < number; u++) {
	afs_int32 ui = users[u];
	if (ui) {
	    int i;
	    int ng;		/* number groups */
	    int (*proc) ();	/* membership listing procedure */
	    int over;
	    prlist alist;

	    alist.prlist_len = 0;
	    alist.prlist_val = 0;
	    if (random() & 4)
		proc = PR_ListElements;
	    else
		proc = PR_GetCPS;
	    code = ubik_Call(proc, pruclient, 0, ui, &alist, &over);
	    if (code) {
		afs_com_err(whoami, code,
			"getting membership list of (%di) using %s", ui,
			((proc == PR_GetCPS) ? "GetCPR" : "ListElements"));
		exit(24);
	    }
	    if (over) {
		fprintf(stderr, "membership list for id %di too long\n", ui);
	    }
	    ng = 0;
	    for (i = 0; i < number; i++)
		if (population[u * number + i])
		    glist[ng++] = groups[i];
	    qsort(glist, ng, sizeof(afs_int32), IdCmp);
	    if (ng != (alist.prlist_len - ((proc == PR_GetCPS) ? 3 : 0))) {
		fprintf(stderr,
			"Membership list for %di of unexpected length: was %d but expected %d\n",
			ui, alist.prlist_len, ng);
		exit(20);
	    }
	    /* all the extra entries for the CPS should be at the end. */
	    code = 0;
	    for (i = 0; i < ng; i++)
		if (alist.prlist_val[i] != glist[i]) {
		    fprintf(stderr,
			    "membership for %di not correct: was %di but expected %di\n",
			    ui, alist.prlist_val[i], glist[i]);
		    code++;
		}
	    if (code)
		exit(21);
	    if (proc == PR_GetCPS) {
		if ((alist.prlist_val[i /* =ng */ ] != AUTHUSERID) ||
		    (alist.prlist_val[++i] != ANYUSERID)
		    || (alist.prlist_val[++i] != ui)) {
		    fprintf(stderr, "CPS doesn't have extra entries\n");
		    exit(27);
		}
	    }
	    if (alist.prlist_val)
		free(alist.prlist_val);

	    /* User 0 is a member of all groups all of which should also be on
	     * the owner list of the caller or the ownerUser, although there
	     * may also be others.  Check this. */
	    if (u == 0) {
		prlist callerList;
		prlist ownerList;
		prlist lastGroupList;
		int i, j, k, l;

		if (ng != number) {
		    fprintf(stderr, "User 0 not a member of all groups\n");
		    exit(26);
		}
#define GETOWNED(xlist,xid) \
  (xlist).prlist_val = 0; (xlist).prlist_len = 0; \
  code = ubik_Call (PR_ListOwned, pruclient, 0, (xid), &(xlist), &over); \
  if (code) { \
      afs_com_err (whoami, code, "getting owner list of (%di)", (xid)); \
      exit (23); } \
  if (over) \
      { fprintf (stderr, "membership of id %di too long\n", (xid)); }

		GETOWNED(callerList, callerId);
		GETOWNED(ownerList, ownerUser);

		/* look for every entry in glist, in all the owner lists */
		for (i = j = k = l = 0; i < number; i++) {
		    while ((j < callerList.prlist_len)
			   && (callerList.prlist_val[j] < glist[i]))
			j++;
		    while ((k < ownerList.prlist_len)
			   && (ownerList.prlist_val[k] < glist[i]))
			k++;
#define PRLISTCMP(l,i) \
  (((l).prlist_len == 0) || (glist[i] != (l).prlist_val[(i)]))
		    if (PRLISTCMP(callerList, j) && PRLISTCMP(ownerList, k)) {
			for (l = 0; l < number; l++) {
			    if (groups[l] == glist[i]) {
				if ((groupOwners[l] != callerId)
				    && (groupOwners[l] != ownerUser)) {
				    GETOWNED(lastGroupList, groupOwners[l]);
				    if ((lastGroupList.prlist_len != 1)
					|| (lastGroupList.prlist_val[0] !=
					    glist[i])) {
					fprintf(stderr,
						"Group (%di) not on any owner list\n",
						glist[i]);
					exit(25);
				    }
				}
				goto foundLast;
			    }
			}
			fprintf(stderr, "unexpected group %di\n", glist[i]);
		      foundLast:;
		    }
		}
		if (callerList.prlist_val)
		    free(callerList.prlist_val);
		if (ownerList.prlist_val)
		    free(ownerList.prlist_val);
		if (lastGroupList.prlist_val)
		    free(lastGroupList.prlist_val);
	    }
	}
    }

    /* cleanup by deleting all the users and groups */
    printf("Starting deletion of users and groups\n");
    for (i = 0; i < number; i++) {
	DeleteRandomId(users);
	DeleteRandomId(groups);
    }

    printf
	("Created/deleted %d/%d users and %d/%d groups; added %d and removed %d.\n",
	 nUsers, nUDels, nGroups, nGDels, nAdds, nRems);
    return 0;
}

/* from ka_ConvertBytes included here to avoid circularity */
/* Converts a byte string to ascii.  Return the number of unconverted bytes. */

static int
ka_ConvertBytes(char *ascii,		/* output buffer */
		int alen,		/* buffer length */
		char bs[],		/* byte string */
		int bl)			/* number of bytes */
{
    int i;
    unsigned char c;

    alen--;			/* make room for termination */
    for (i = 0; i < bl; i++) {
	c = bs[i];
	if (alen <= 0)
	    return bl - i;
	if (isalnum(c) || ispunct(c))
	    (*ascii++ = c), alen--;
	else {
	    if (alen <= 3)
		return bl - i;
	    *ascii++ = '\\';
	    *ascii++ = (c >> 6) + '0';
	    *ascii++ = (c >> 3 & 7) + '0';
	    *ascii++ = (c & 7) + '0';
	    alen -= 4;
	}
    }
    *ascii = 0;			/* terminate string */
    return 0;
}

/* This runs various tests on the server.  It creates, then deletes, a bunch of
 * users and groups, so it would be safest to run it on a test database.
 *
 * These are the things I check for:
 *   User names longer than PR_MAXNAMELEN - strlen(cellname).
 *   Group names longer than PR_MAXNAMELEN.
 *   User names containing all legal 8-bit ascii characters.  This excludes
 *     only ':', '@', and '\n'.
 *   Group names as above, but at least one colon is required, and the owner
 *     must be correct.
 */

int
TestPrServ(struct cmd_syndesc *as, char *arock)
{
    afs_int32 id;
    char name[PR_MAXNAMELEN + 1];
    char creator[PR_MAXNAMELEN];	/* our name */
    struct prcheckentry ent;
    afs_int32 code;
    int i, j;
    int maxLen = PR_MAXNAMELEN - 1 - strlen(lcell) - 1;

    code = pr_Initialize(1, conf_dir, NULL);
    if (code) {
	afs_com_err(whoami, code, "initializing pruser");
	exit(1);
    }

    for (i = 0; i < maxLen; i++)
	name[i] = 'a';
    name[i] = 'a';		/* too long a name... */
    name[i + 1] = 0;
    id = 0;
    code = pr_CreateUser(name, &id);
    if ((code != RXGEN_CC_MARSHAL) && (code != PRBADNAM)) {
	afs_com_err(whoami, code, "succeeded creating %s", name);
	exit(2);
    }
    name[i] = 0;
    id = 0;
    code = pr_CreateUser(name, &id);
    if (code == PREXIST) {
	fprintf(stderr, "group already exists, skipping\n");
	pr_SNameToId(name, &id);
    } else if (code) {
	afs_com_err(whoami, code, "failed creating %s", name);
	exit(3);
    }
    if ((code = pr_ListEntry(id, &ent))
	|| (code = pr_SIdToName(ent.creator, creator))) {
	afs_com_err(whoami, code, "getting creator's name");
	exit(5);
    }
    code = pr_DeleteByID(id);
    if (code) {
	afs_com_err(whoami, code, "deleting %s", name);
	exit(6);
    }
    /* now make sure the illegal chars are detected */
    {
	char *illegalChars;
	for (illegalChars = "@:\n"; *illegalChars; illegalChars++) {
	    name[10] = *illegalChars;
	    id = 0;
	    code = pr_CreateUser(name, &id);
	    if (code != PRBADNAM) {
		afs_com_err(whoami, code, "succeeded creating %s", name);
		exit(8);
	    }
	}
    }

    for (i = 1; i <= 255;) {	/* for all 8-bit ascii... */
	j = 0;			/* build a new name */
	while ((j < maxLen) && (i <= 255)) {
	    if (!((i == ':') || (i == '@') || (i == '\n')))
		name[j++] = i;
	    i++;
	}
	name[j] = 0;		/* terminate string */
	id = 0;
	code = pr_CreateUser(name, &id);
	if (code == PREXIST) {
	    fprintf(stderr, "user already exists, skipping\n");
	    pr_SNameToId(name, &id);
	} else if (code) {
	    char ascii[BUFSIZ];
	    ka_ConvertBytes(ascii, sizeof(ascii), name, strlen(name));
	    afs_com_err(whoami, code, "failed creating %s", ascii);
	    exit(4);
	}
	code = pr_DeleteByID(id);
	if (code) {
	    afs_com_err(whoami, code, "deleting %s", name);
	    exit(7);
	}
    }

    /* now check group names */
    strcpy(name, creator);
    strcat(name, ":abcdefghijklmnopqrstuvwxyz");
    name[0] = 1;		/* bash the owner name */
    id = 0;
    code = pr_CreateGroup(name, &id);
    if (code != PRNOENT) {	/* owner doesn't exist */
	afs_com_err(whoami, code, "succeeded creating %s", name);
	exit(9);
    }
    name[0] = creator[0];	/* fix owner */
    /* Make sure the illegal chars are detected */
    {
	char *illegalChars;
	for (illegalChars = ":@\n"; *illegalChars; illegalChars++) {
	    name[strlen(creator) + 10] = *illegalChars;
	    id = 0;
	    code = pr_CreateGroup(name, creator, &id);
	    if (code != PRBADNAM) {
		afs_com_err(whoami, code, "succeeded creating %s", name);
		exit(10);
	    }
	}
    }
    for (i = 1; i <= 255;) {	/* for all 8-bit ascii... */
	j = strlen(creator) + 1;	/* build a new name */
	while ((j < PR_MAXNAMELEN - 1) && (i <= 255)) {
	    if (!((i == ':') || (i == '@') || (i == '\n')))
		name[j++] = i;
	    i++;
	}
	name[j] = 0;		/* terminate string */
	id = 0;
	code = pr_CreateGroup(name, creator, &id);
	if (code == PREXIST) {
	    fprintf(stderr, "group already exists, skipping\n");
	    pr_SNameToId(name, &id);
	} else if (code) {
	    char ascii[BUFSIZ];
	    ka_ConvertBytes(ascii, sizeof(ascii), name, strlen(name));
	    afs_com_err(whoami, code, "failed creating %s", ascii);
	    exit(4);
	}
	code = pr_DeleteByID(id);
	if (code) {
	    afs_com_err(whoami, code, "deleting %s", name);
	    exit(7);
	}
    }

    printf("All OK\n");
    return 0;
}

static char tmp_conf_dir[128] = "";
static char tmp_conf_file[128] = "";
static char tmp_cell_file[128] = "";
static char tmp_noauth_file[128] = "";

static int
MyAfterProc(struct cmd_syndesc *as, char *arock)
{
    if (strlen(tmp_conf_file))
	unlink(tmp_conf_file);
    if (strlen(tmp_cell_file))
	unlink(tmp_cell_file);
    if (strlen(tmp_noauth_file))
	unlink(tmp_noauth_file);
    if (strlen(tmp_conf_dir))
	rmdir(tmp_conf_dir);
    return 0;
}

static int
MyBeforeProc(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    int i;
    char *cdir = 0;
    int noAuth = 0;
    struct cmd_item *serverList = 0;
    struct afsconf_dir *local_conf = 0;	/* so we can default stuff nicely */
    struct afsconf_cell cellinfo;

    if (as->parms[12].items) {	/* if conf dir specified */
	cdir = as->parms[12].items->data;
	if (as->parms[13].items || as->parms[14].items || as->parms[15].items) {
	    printf("Can't specify conf dir and other cell parameters\n");
	    return AFSCONF_SYNTAX;
	}
    }

    /* if we need to default cell name or cell servers, get local conf info */

    if (!(local_conf = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH))
	&& !(local_conf = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH))) {
	printf("** Can't local configuration!\n");
	return AFSCONF_NOCELL;
    }

    if (as->parms[13].items) {	/* if cell name specified */
	lcstring(lcell, as->parms[13].items->data, sizeof(lcell));
	code = afsconf_GetCellInfo(local_conf, lcell, 0, &cellinfo);
	if (code == 0)
	    strncpy(lcell, cellinfo.name, sizeof(lcell));
    } else {
	code = afsconf_GetLocalCell(local_conf, lcell, sizeof(lcell));
	if (code)
	    return code;
    }

    if (as->parms[14].items) {	/* noauth flag */
	noAuth = 1;
    }

    if (as->parms[15].items) {	/* servers list */
	serverList = as->parms[15].items;
	for (i = 0; serverList; i++, serverList = serverList->next) {
	    struct hostent *th;
	    if (i >= MAXHOSTSPERCELL)
		return AFSCONF_FULL;
	    strncpy(cellinfo.hostName[i], serverList->data, MAXHOSTCHARS);
	    th = gethostbyname(cellinfo.hostName[i]);
	    if (!th)
		return UBADHOST;
	    memcpy(&cellinfo.hostAddr[i].sin_addr, th->h_addr,
		   sizeof(afs_int32));
	    cellinfo.hostAddr[i].sin_family = AF_INET;
	    cellinfo.hostAddr[i].sin_port = 0;
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
	    cellinfo.hostAddr[i].sin_len = sizeof(struct sockaddr_in);
#endif
	}
	cellinfo.numServers = i;
	strcpy(cellinfo.name, lcell);
    } else {
	code = afsconf_GetCellInfo(local_conf, lcell, 0, &cellinfo);
	if (code)
	    return code;
    }

    if (local_conf)
	afsconf_Close(local_conf);

    if (cdir == 0) {
	FILE *f;

	sprintf(tmp_conf_dir, "%s/afsconf.%lu", gettmpdir(),
		(unsigned long)getpid());
	code = mkdir(tmp_conf_dir, 0777);
	if ((code < 0) && (errno != EEXIST)) {
	    afs_com_err(whoami, errno, "can't create temporary afsconf dir: %s",
		    cdir);
	    return errno;
	}

	strcompose(tmp_conf_file, 128, tmp_conf_dir, "/",
		   AFSDIR_CELLSERVDB_FILE, NULL);
	f = fopen(tmp_conf_file, "w");
	if (f == 0) {
	  cantcreate:
	    afs_com_err(whoami, errno, "can't create conf file %s",
		    tmp_conf_file);
	    return errno;
	}
	fprintf(f, ">%s\n", lcell);
	for (i = 0; i < cellinfo.numServers; i++) {
	    register unsigned char *tp =
		(unsigned char *)&cellinfo.hostAddr[i].sin_addr;
	    fprintf(f, "%d.%d.%d.%d\t#%s\n", tp[0], tp[1], tp[2], tp[3],
		    cellinfo.hostName[i]);
	}
	if (fclose(f) == EOF) {
	  cantclose:
	    afs_com_err(whoami, errno, "can't write to conf file %s",
		    tmp_conf_file);
	    return errno;
	}

	strcompose(tmp_cell_file, 128, tmp_conf_dir, "/",
		   AFSDIR_THISCELL_FILE, NULL);
	f = fopen(tmp_cell_file, "w");
	if (f == 0)
	    goto cantcreate;
	fprintf(f, "%s", lcell);
	if (fclose(f) == EOF)
	    goto cantclose;

	strcompose(tmp_noauth_file, 128, tmp_conf_dir, "/",
		   AFSDIR_NOAUTH_FILE, NULL);
	if (noAuth) {
	    code = creat(tmp_noauth_file, 0777);
	    if (code && (errno != EEXIST))
		return errno;
	} else {		/* make sure file doesn't exist */
	    code = unlink(tmp_noauth_file);
	    if (code && (errno != ENOENT))
		return errno;
	}
    }

    strncpy(conf_dir, tmp_conf_dir, sizeof(conf_dir));
    conf = afsconf_Open(conf_dir);
    if (conf == 0)
	return AFSCONF_NOTFOUND;
    return 0;
}

static void
add_std_args(register struct cmd_syndesc *ts)
{
    cmd_Seek(ts, 12);
    cmd_AddParm(ts, "-confdir", CMD_SINGLE, CMD_OPTIONAL,
		"AFS Conf dir pathname");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "Cell name");
    cmd_AddParm(ts, "-noauth", CMD_FLAG, CMD_OPTIONAL, "Don't authenticate");
    cmd_AddParm(ts, "-servers", CMD_LIST, CMD_OPTIONAL, "Server config");
}

int
osi_audit()
{
/* OK, this REALLY sucks bigtime, but I can't tell who is calling
 * afsconf_CheckAuth easily, and only *SERVERS* should be calling osi_audit
 * anyway.  It's gonna give somebody fits to debug, I know, I know.
 */
    return 0;
}

#include "AFS_component_version_number.c"

int 
main(int argc, char *argv[])
{
    afs_int32 code;
    struct cmd_syndesc *ts;	/* ptr to parsed command line syntax */

    whoami = argv[0];
    initialize_CMD_error_table();
    initialize_ACFG_error_table();
    initialize_KTC_error_table();
    initialize_U_error_table();
    initialize_PT_error_table();
    initialize_RXK_error_table();

#ifdef AFS_NT40_ENV
    /* initialize winsock */
    if (afs_winsockInit() < 0) {
	fprintf(stderr, "%s: couldn't initialize winsock. \n", whoami);
	exit(1);
    }
#endif

    cmd_SetBeforeProc(MyBeforeProc, NULL);
    cmd_SetAfterProc(MyAfterProc, NULL);

    ts = cmd_CreateSyntax("usedIds", ListUsedIds, 0,
			  "Find used (or unused) user (or group) ids");
    cmd_AddParm(ts, "-startId", CMD_SINGLE, CMD_OPTIONAL,
		"id to start checking");
    cmd_AddParm(ts, "-number", CMD_SINGLE, CMD_OPTIONAL,
		"number of ids to check");
    cmd_AddParm(ts, "-unused", CMD_FLAG, CMD_OPTIONAL, "print unused ids");
    add_std_args(ts);

    ts = cmd_CreateSyntax("initcmd", TestPrServ, 0, "test the prserver");
    add_std_args(ts);

    ts = cmd_CreateSyntax("testmanymembers", TestManyMembers, 0,
			  "test creating users and groups w/ many members");
    cmd_AddParm(ts, "-number", CMD_SINGLE, 0,
		"number of users/groups to create");
    cmd_AddParm(ts, "-dropoff", CMD_SINGLE, CMD_OPTIONAL,
		"precentage for exponential dropoff");
    cmd_AddParm(ts, "-prefix", CMD_SINGLE, CMD_OPTIONAL, "naming prefix");
    cmd_AddParm(ts, "-long", CMD_FLAG, CMD_OPTIONAL, "show progress");
    cmd_AddParm(ts, "-seed", CMD_SINGLE, CMD_OPTIONAL, "random number seed");
    add_std_args(ts);
    cmd_CreateAlias(ts, "mm");


    code = cmd_Dispatch(argc, argv);
    if (code)
	afs_com_err(whoami, code, "calling cmd_Dispatch");
    exit(code);
}
