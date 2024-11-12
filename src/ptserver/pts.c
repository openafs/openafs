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


#include <stdio.h>
#include <string.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#include <ctype.h>
#include <sys/types.h>
#include <errno.h>
#include <afs/cmd.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <WINNT/afsevent.h>
#include <WINNT/afsreg.h>
#else
#include <netinet/in.h>
#endif
#include <afs/cellconfig.h>
#include <rx/rx.h>
#include <rx/xdr.h>
#include "ptclient.h"
#include "ptuser.h"
#include "pterror.h"
#include "ptprototypes.h"
#include <afs/afsutil.h>
#include <afs/com_err.h>

#undef FOREIGN

char *whoami;
int force = 0;

static int finished;
static FILE *source;
extern struct ubik_client *pruclient;

struct sourcestack {
    struct sourcestack *s_next;
    FILE *s_file;
} *shead;

struct authstate {
    int sec;
    const char *confdir;
    char cell[MAXCELLCHARS];
};

static int CleanUp(struct cmd_syndesc *as, void *arock);

static int
pts_Interactive(struct cmd_syndesc *as, void *arock)
{
    source = stdin;
    finished = 0;
    return 0;
}

static int
pts_Quit(struct cmd_syndesc *as, void *arock)
{
    finished = 1;
    return 0;
}

static int
pts_Source(struct cmd_syndesc *as, void *arock)
{
    FILE *fd;
    struct sourcestack *sp;

    finished = 0;
    if (!as->parms[0].items) {
/* can this happen? */
	return 1;
    }
    fd = fopen(as->parms[0].items->data, "r");
    if (!fd) {
	perror(as->parms[0].items->data);
	return errno;
    }
    sp = (struct sourcestack *)malloc(sizeof *sp);
    if (!sp) {
	return errno ? errno : ENOMEM;
    } else {
	sp->s_next = shead;
	sp->s_file = source;
	shead = sp;
	source = fd;
    }
    return 0;
}

static int
pts_Sleep(struct cmd_syndesc *as, void *arock)
{
    int delay;
    if (!as->parms[0].items) {
/* can this happen? */
	return 1;
    }
    delay = atoi(as->parms[0].items->data);
#ifdef AFS_PTHREAD_ENV
    sleep(delay);
#else
    IOMGR_Sleep(delay);
#endif
    return 0;
}

static int
popsource(void)
{
    struct sourcestack *sp;
    if (!(sp = shead))
	return 0;
    if (source != stdin)
	fclose(source);
    source = sp->s_file;
    shead = sp->s_next;
    free((char *)sp);
    return 1;
}

int
osi_audit(void)
{
/* OK, this REALLY sucks bigtime, but I can't tell who is calling
 * afsconf_CheckAuth easily, and only *SERVERS* should be calling osi_audit
 * anyway.  It's gonna give somebody fits to debug, I know, I know.
 */
    return 0;
}

#ifdef AFS_NT40_ENV
static DWORD
win32_enableCrypt(void)
{
    HKEY parmKey;
    DWORD dummyLen;
    DWORD cryptall = 0;
    DWORD code;

    /* Look up configuration parameters in Registry */
    code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY,
                        0, (IsWow64()?KEY_WOW64_64KEY:0)|KEY_QUERY_VALUE, &parmKey);
    if (code != ERROR_SUCCESS) {
        dummyLen = sizeof(cryptall);
        RegQueryValueEx(parmKey, "SecurityLevel", NULL, NULL,
                        (BYTE *) &cryptall, &dummyLen);
    }
    RegCloseKey (parmKey);

    return cryptall;
}
#endif /* AFS_NT40_ENV */

static int
GetGlobals(struct cmd_syndesc *as, void *arock)
{
    struct authstate *state = (struct authstate *) arock;
    afs_int32 code;
    char *cell = NULL;
    afs_int32 sec;
    int changed = 0;
    const char* confdir;

    whoami = as->a0name;

    if (!strcmp(as->name, "help"))
	return 0;

    if (*state->cell) {
	cell = state->cell;
    }
    sec = state->sec;

    if (state->confdir == NULL) {
	changed = 1;
    }

    if (as->parms[16].items) {
	changed = 1;
	cell = as->parms[16].items->data;
    }
    if (as->parms[17].items) { /* -noauth */
	changed = 1;
	sec = 0;
    }
    if (as->parms[20].items) { /* -localauth */
	changed = 1;
	sec = 2;
    }
    if (as->parms[21].items) { /* -auth */
	changed = 1;
	sec = 1;
    }
    if (as->parms[22].items    /* -encrypt */
#ifdef AFS_NT40_ENV
        || win32_enableCrypt()
#endif /* AFS_NT40_ENV */
        ) {
	changed = 1;
	sec = 3;
    }
    if (as->parms[18].items || as->parms[20].items) { /* -test, -localauth */
	changed = 1;
	confdir = AFSDIR_SERVER_ETC_DIRPATH;
    } else {
	if (sec == 2)
	    confdir = AFSDIR_SERVER_ETC_DIRPATH;
	else
	    confdir = AFSDIR_CLIENT_ETC_DIRPATH;
    }
    if (changed) {
	CleanUp(as, arock);
	code = pr_Initialize(sec, confdir, cell);
    } else {
	code = 0;
    }
    if (code) {
	afs_com_err(whoami, code, "while initializing");
	return code;
    }
    state->sec = sec;
    state->confdir = confdir;
    if (cell && cell != state->cell)
        strncpy(state->cell, cell, MAXCELLCHARS-1);

    force = 0;
    if (as->parms[19].items)
	force = 1;

    return code;
}

static int
CleanUp(struct cmd_syndesc *as, void *arock)
{
    if (as && !strcmp(as->name, "help"))
	return 0;
    if (pruclient) {
	/* Need to shutdown the ubik_client & other connections */
	pr_End();
	rx_Finalize();
    }
    return 0;
}

static int
CreateGroup(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    afs_int32 id;
    char *owner;
    struct cmd_item *namei;
    struct cmd_item *idi;

    namei = as->parms[0].items;
    idi = as->parms[2].items;
    if (as->parms[1].items)
	owner = as->parms[1].items->data;
    else
	owner = NULL;

    while (namei) {
	if (idi) {
	    code = util_GetInt32(idi->data, &id);
	    if (code) {
		afs_com_err(whoami, code, "because group id was: '%s'",
			idi->data);
		return code;
	    }
	    if (id == 0) {
		printf("0 isn't a valid group id; aborting\n");
		return EINVAL;
	    }
	    if (id > 0) {
		code = PRBADARG;
		afs_com_err(whoami, code, "because group id %d was not negative",
			id);
		return code;
	    }

	    idi = idi->next;
	} else
	    id = 0;

	code = pr_CreateGroup(namei->data, owner, &id);
	if (code) {
	    if (owner || id)
		afs_com_err(whoami, code,
			"; unable to create group %s with id %d%s%s%s%s",
			namei->data, id, owner ? " owned by '" : "",
			owner ? owner : "", owner ? "'" : "",
			(force ? " (ignored)" : ""));
	    else
		afs_com_err(whoami, code, "; unable to create group %s %s",
			namei->data, (force ? "(ignored)" : ""));
	    if (!force)
		return code;
	}
	printf("group %s has id %d\n", namei->data, id);
	namei = namei->next;
    }
    return 0;
}

static int
CreateUser(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    afs_int32 id;
    struct cmd_item *namei;
    struct cmd_item *idi;

    namei = as->parms[0].items;
    idi = as->parms[1].items;

    while (namei) {
	if (idi) {
	    code = util_GetInt32(idi->data, &id);
	    if (code) {
		afs_com_err(whoami, code, "because id was: '%s'", idi->data);
		return code;
	    }
	    if (id == 0) {
		printf("0 isn't a valid user id; aborting\n");
		return EINVAL;
	    }
	    if (id < 0) {
		code = PRBADARG;
		afs_com_err(whoami, code, "because user id %d was not positive",
			id);
		return code;
	    }
	    idi = idi->next;
	} else
	    id = 0;

	code = pr_CreateUser(namei->data, &id);
	if (code) {
	    if (id)
		afs_com_err(whoami, code,
			"; unable to create user %s with id %d %s",
			namei->data, id, (force ? "(ignored)" : ""));
	    else
		afs_com_err(whoami, code, "; unable to create user %s %s",
			namei->data, (force ? "(ignored)" : ""));
	    if (!force)
		return code;
	}
	printf("User %s has id %d\n", namei->data, id);
	namei = namei->next;
    }
    return 0;
}


#ifdef notdef
static int
GetNameOrId(struct cmd_syndesc *as, struct idlist *lids, struct namelist *lnames)
{
    afs_int32 code = 0;
    int n = 0;
    struct cmd_item *i;
    int goodCount;

    if (!(as->parms[0].items || as->parms[1].items)) {
	afs_com_err(whoami, 0, "must specify either a name or an id.");
	return -1;
    }
    if (as->parms[0].items && as->parms[1].items) {
	afs_com_err(whoami, 0, "can't specify both a name and id.");
	return -1;
    }

    goodCount = 0;
    lids->idlist_len = 0;
    lids->idlist_val = 0;

    if (as->parms[0].items) {	/* name */
	struct namelist names;	/* local copy, if not ret. names */
	struct namelist *nl;

	names.namelist_val = 0;	/* so it gets freed later if needed */
	if (lnames)
	    nl = lnames;
	else
	    nl = &names;

	n = 0;			/* count names */
	for (i = as->parms[0].items; i; i = i->next)
	    n++;
	nl->namelist_val = (prname *) malloc(n * PR_MAXNAMELEN);
	nl->namelist_len = n;
	n = 0;
	for (i = as->parms[0].items; i; i = i->next)
	    strncpy(nl->namelist_val[n++], i->data, PR_MAXNAMELEN);

	code = pr_NameToId(nl, lids);
	if (code)
	    afs_com_err(whoami, code, "so couldn't look up names");
	else {
	    for (n = 0; n < lids->idlist_len; n++) {
		if ((lids->idlist_val[n] == ANONYMOUSID)) {
		    afs_com_err(whoami, PRNOENT, "so couldn't look up id for %s",
			    nl->namelist_val[n]);
		} else
		    goodCount++;
	    }
	    /* treat things as working if any of the lookups worked */
	    if (goodCount == 0)
		code = PRNOENT;
	}

	if (names.namelist_val)
	    free(names.namelist_val);
    } else if (as->parms[1].items) {	/* id */
	n = 0;
	for (i = as->parms[1].items; i; i = i->next)
	    n++;
	lids->idlist_val = (afs_int32 *) malloc(n * sizeof(afs_int32));
	lids->idlist_len = n;
	n = 0;
	for (i = as->parms[1].items; i; i = i->next) {
	    code = util_GetInt32(i->data, &lids->idlist_val[n]);
	    if (code)
		afs_com_err(whoami, code =
			PRNOENT, "because a bogus id '%s' was specified",
			i->data);
	    n++;
	}
	if (!code && lnames) {
	    lnames->namelist_val = 0;
	    lnames->namelist_len = 0;
	    code = pr_IdToName(lids, lnames);
	    if (code)
		afs_com_err(whoami, code, "translating ids");
	}
    }
    if (code) {
	if (lids->idlist_val)
	    free(lids->idlist_val);
	return -1;
    }
    return 0;
}
#endif


static int
GetNameOrId(struct cmd_syndesc *as, struct idlist *lids,
	    struct namelist *lnames)
{
    afs_int32 code = 0;
    int n = 0, nd = 0, nm = 0, id, x;
    struct cmd_item *i;
    struct namelist names, tnames;	/* local copy, if not ret. names */
    struct idlist ids, tids;	/* local copy, if not ret. ids */
    int goodCount = 0;

    for (i = as->parms[0].items; i; i = i->next)
	n++;
    lids->idlist_val = (afs_int32 *) malloc(n * sizeof(afs_int32));
    lids->idlist_len = n;
    ids.idlist_val = (afs_int32 *) malloc(n * sizeof(afs_int32));
    ids.idlist_len = n;
    names.namelist_val = (prname *) malloc(n * PR_MAXNAMELEN);
    names.namelist_len = n;
    if (lnames) {
	lnames->namelist_val = (prname *) malloc(n * PR_MAXNAMELEN);
	lnames->namelist_len = 0;
    }
    for (i = as->parms[0].items; i; i = i->next) {
	tnames.namelist_val = (prname *) malloc(PR_MAXNAMELEN);
	strncpy(tnames.namelist_val[0], i->data, PR_MAXNAMELEN);
	tnames.namelist_len = 1;
	tids.idlist_len = 0;
	tids.idlist_val = 0;
	code = pr_NameToId(&tnames, &tids);
	if ((!code && (tids.idlist_val[0] != 32766))
	    || (code = util_GetInt32(i->data, &id))) {
	    /* Assume it's a name instead */
	    strncpy(names.namelist_val[nm++], i->data, PR_MAXNAMELEN);
	} else {
	    ids.idlist_val[nd++] = id;
	}
	free(tnames.namelist_val);
    }
    names.namelist_len = nm;
    ids.idlist_len = nd;
    tids.idlist_len = nd = nm = 0;
    tids.idlist_val = 0;
    code = pr_NameToId(&names, &tids);
    if (code)
	afs_com_err(whoami, code, "so couldn't look up names");
    else {
	for (n = 0; n < tids.idlist_len; n++) {
	    if (tids.idlist_val[n] == ANONYMOUSID) {
		afs_com_err(whoami, PRNOENT, "so couldn't look up id for %s",
			names.namelist_val[n]);
	    } else
		goodCount++;
	    lids->idlist_val[nd] = tids.idlist_val[n];
	    if (lnames)
		strcpy(lnames->namelist_val[nd], names.namelist_val[n]);
	    nd++;
	}
    }
    for (x = 0; x < ids.idlist_len; x++) {
	lids->idlist_val[nd + x] = ids.idlist_val[x];
    }
    lids->idlist_len = nd + x;
    if (!code && lnames) {
	tnames.namelist_val = 0;
	tnames.namelist_len = 0;
	code = pr_IdToName(&ids, &tnames);
	if (code)
	    afs_com_err(whoami, code, "translating ids");
	else
	    goodCount++;
	if (lnames) {
	    for (x = 0; x < ids.idlist_len; x++)
		strcpy(lnames->namelist_val[nd + x], tnames.namelist_val[x]);
	    lnames->namelist_len = nd + x;
	}
    }
    /* treat things as working if any of the lookups worked */
    if (goodCount == 0)
	code = PRNOENT;
    if (code) {
	if (lids->idlist_val)
	    free(lids->idlist_val);
	return -1;
    }
    return 0;
}


static int
AddToGroup(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct cmd_item *u, *g;

    for (u = as->parms[0].items; u; u = u->next) {
	for (g = as->parms[1].items; g; g = g->next) {
	    code = pr_AddToGroup(u->data, g->data);
	    if (code) {
		afs_com_err(whoami, code,
			"; unable to add user %s to group %s %s", u->data,
			g->data, (force ? "(ignored)" : ""));
		if (!force)
		    return code;
	    }
	}
    }
    return 0;
}

static int
RemoveFromGroup(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct cmd_item *u, *g;

    for (u = as->parms[0].items; u; u = u->next) {
	for (g = as->parms[1].items; g; g = g->next) {
	    code = pr_RemoveUserFromGroup(u->data, g->data);
	    if (code) {
		afs_com_err(whoami, code,
			"; unable to remove user %s from group %s %s",
			u->data, g->data, (force ? "(ignored)" : ""));
		if (!force)
		    return code;
	    }
	}
    }
    return 0;
}

static int
ListMembership(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    idlist ids;
    namelist names;
    int i;
    namelist list;
    int j;

    if (GetNameOrId(as, &ids, &names))
	return PRBADARG;

    for (i = 0; i < ids.idlist_len; i++) {
	afs_int32 id = ids.idlist_val[i];
	char *name = names.namelist_val[i];

	if (id == ANONYMOUSID)
	    continue;		/* bad entry */

	list.namelist_val = 0;
	list.namelist_len = 0;
	if (as->parms[2].items) {	/* -expandgroups */
	    code = pr_IDListExpandedMembers(id, &list);
	    if (!code)
		printf("Expanded ");
	} else {
	    code = pr_IDListMembers(id, &list);
	}
	if (code) {
	    afs_com_err(whoami, code, "; unable to get membership of %s (id: %d)",
		    name, id);
	    continue;
	}
	if (id < 0)
	    printf("Members of %s (id: %d) are:\n", name, id);
	else
	    printf("Groups %s (id: %d) is a member of:\n", name, id);

	for (j = 0; j < list.namelist_len; j++)
	    printf("  %s\n", list.namelist_val[j]);
	if (list.namelist_val)
	    free(list.namelist_val);
	if (as->parms[1].items && id < 0) {	/* -supergroups */
	    list.namelist_val = 0;
	    list.namelist_len = 0;
	    code = pr_ListSuperGroups(ids.idlist_val[i], &list);
	    if (code == RXGEN_OPCODE) {
		continue; /* server does not support supergroups */
	    } else if (code != 0) {
		afs_com_err(whoami, code,
			    "; unable to get supergroups of %s (id: %d)",
			    name, id);
		continue;
	    }
	    printf("Groups %s (id: %d) is a member of:\n", name, id);
	    for (j = 0; j < list.namelist_len; j++)
		printf("  %s\n", list.namelist_val[j]);
	    if (list.namelist_val)
		free(list.namelist_val);
	}
    }
    if (ids.idlist_val)
	free(ids.idlist_val);
    if (names.namelist_val)
	free(names.namelist_val);
    return 0;
}

static int
Delete(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    idlist ids;
    namelist names;
    int i;

    if (GetNameOrId(as, &ids, &names))
	return PRBADARG;

    for (i = 0; i < ids.idlist_len; i++) {
	afs_int32 id = ids.idlist_val[i];
	char *name = names.namelist_val[i];

	if (id == ANONYMOUSID)
	    continue;

	code = pr_DeleteByID(id);
	if (code) {
	    afs_com_err(whoami, code, "deleting %s (id: %d) %s", name, id,
		    (force ? "(ignored)" : ""));
	    if (!force)
		return code;
	}
    }
    if (ids.idlist_val)
	free(ids.idlist_val);
    if (names.namelist_val)
	free(names.namelist_val);
    return 0;
}

/* access bit translation info */

char *flags_upcase = "SOMA ";	/* legal all access values */
char *flags_dncase = "s mar";	/* legal member acces values */
int flags_shift[5] = { 2, 1, 2, 2, 1 };	/* bits for each */

static int
CheckEntry(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    afs_int32 rcode = 1;
    int i, flag = 0, admin = 0;
    namelist lnames, names;
    idlist ids;
    idlist lids;
    struct prcheckentry aentry;

    if (GetNameOrId(as, &ids, &names))
	return PRBADARG;

    lids.idlist_len = 2;
    lids.idlist_val = (afs_int32 *) malloc(sizeof(afs_int32) * 2);
    lnames.namelist_len = 0;
    lnames.namelist_val = 0;

    for (i = 0; i < ids.idlist_len; i++) {
	afs_int32 id = ids.idlist_val[i];

	if (id == ANONYMOUSID)
	    continue;

	rcode = 0;
	code = pr_ListEntry(id, &aentry);
	if (code) {
	    rcode = code;
	    afs_com_err(whoami, code, "; unable to find entry for (id: %d)", id);
	    continue;
	}

	lids.idlist_val[0] = aentry.owner;
	lids.idlist_val[1] = aentry.creator;
	xdr_free((xdrproc_t) xdr_namelist, &lnames);
	code = pr_IdToName(&lids, &lnames);
	if (code) {
	    rcode = code;
	    afs_com_err(whoami, code,
		    "translating owner (%d) and creator (%d) ids",
		    aentry.owner, aentry.creator);
	    continue;
	}
	printf("Name: %s, id: %d, owner: %s, creator: %s,\n", aentry.name,
	       aentry.id, lnames.namelist_val[0], lnames.namelist_val[1]);
	printf("  membership: %d", aentry.count);
	{
	    char access[6];
	    afs_int32 flags = aentry.flags;
	    int j, s, new;
	    char c;
	    access[5] = 0;	/* null-terminate the string */
	    for (j = 4; j >= 0; j--) {
		s = flags_shift[j];
		if (s == 1)
		    new = flags & 1;
		else
		    new = flags & 3;
		if (new == 0)
		    c = '-';
		else if (new == 1) {
		    c = flags_dncase[j];
		    if (c == ' ')
			c = flags_upcase[j];
		} else if (new == 2)
		    c = flags_upcase[j];
		else
		    c = 'X';
		access[j] = c;
		flags >>= s;
	    }
	    printf(", flags: %s", access);
	}
	if (aentry.id == SYSADMINID)
	    admin = 1;
	else if (!pr_IsAMemberOf(aentry.name, "system:administrators", &flag)) {
	    if (flag)
		admin = 1;
	}
	if (admin)
	    printf(", group quota: unlimited");
	else
	    printf(", group quota: %d", aentry.ngroups);
#if FOREIGN
	printf(", foreign user quota=%d", aentry.nusers);
#endif
	printf(".\n");
    }

    if (lnames.namelist_val)
	free(lnames.namelist_val);
    if (lids.idlist_val)
	free(lids.idlist_val);
    if (ids.idlist_val)
	free(ids.idlist_val);

    return (rcode);
}

static int
ListEntries(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code = 0;
    afs_int32 flag, startindex, nentries, nextstartindex;
    struct prlistentries *entriesp = 0, *e;
    afs_int32 i;

    flag = PRUSERS;
    if (as->parms[1].items)
	flag = PRGROUPS;
    if (as->parms[0].items)
	flag |= PRUSERS;

    printf("Name                          ID  Owner Creator\n");
    for (startindex = 0; startindex != -1; startindex = nextstartindex) {
	code =
	    pr_ListEntries(flag, startindex, &nentries, &entriesp,
			   &nextstartindex);
	if (code) {
	    afs_com_err(whoami, code, "; unable to list entries");
	    if (entriesp)
		free(entriesp);
	    break;
	}

	/* Now display each of the entries we read */
	for (i = 0, e = entriesp; i < nentries; i++, e++) {
	    printf("%-25s %6d %6d %7d \n", e->name, e->id, e->owner,
		   e->creator);
	}
	if (entriesp)
	    free(entriesp);
    }
    return code;
}

static int
ChownGroup(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    char *name;
    char *owner;

    name = as->parms[0].items->data;
    owner = as->parms[1].items->data;
    code = pr_ChangeEntry(name, "", 0, owner);
    if (code)
	afs_com_err(whoami, code, "; unable to change owner of %s to %s", name,
		owner);
    return code;
}

static int
ChangeName(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    char *oldname;
    char *newname;

    oldname = as->parms[0].items->data;
    newname = as->parms[1].items->data;
    code = pr_ChangeEntry(oldname, newname, 0, "");
    if (code)
	afs_com_err(whoami, code, "; unable to change name of %s to %s", oldname,
		newname);
    return code;
}

static int
ListMax(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    afs_int32 maxUser, maxGroup;

    code = pr_ListMaxUserId(&maxUser);
    if (code)
	afs_com_err(whoami, code, "getting maximum user id");
    else {
	code = pr_ListMaxGroupId(&maxGroup);
	if (code)
	    afs_com_err(whoami, code, "getting maximum group id");
	else {
	    printf("Max user id is %d and max group id is %d.\n", maxUser,
		   maxGroup);
	}
    }
    return code;
}

static int
SetMaxCommand(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    afs_int32 maxid;

    code = 0;
    if (as->parms[1].items) {
	/* set user max */
	code = util_GetInt32(as->parms[1].items->data, &maxid);
	if (code) {
	    afs_com_err(whoami, code, "because id was: '%s'",
		    as->parms[1].items->data);
	} else {
	    code = pr_SetMaxUserId(maxid);
	    if (code)
		afs_com_err(whoami, code, "so couldn't set Max User Id to %d",
			maxid);
	}
    }
    if (as->parms[0].items) {
	/* set group max */
	code = util_GetInt32(as->parms[0].items->data, &maxid);
	if (code) {
	    afs_com_err(whoami, code, "because id was: '%s'",
		    as->parms[0].items->data);
	} else {
	    code = pr_SetMaxGroupId(maxid);
	    if (code)
		afs_com_err(whoami, code, "so couldn't set Max Group Id to %d",
			maxid);
	}
    }
    if (!as->parms[0].items && !as->parms[1].items) {
	code = PRBADARG;
	printf("Must specify at least one of group or user.\n");
    }
    return code;
}

static int
SetFields(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    idlist ids;
    namelist names;
    int i;
    afs_int32 mask, flags=0, ngroups, nusers;

    if (GetNameOrId(as, &ids, &names))
	return PRBADARG;

    mask = 0;
    nusers = 0;
    ngroups = 0;

    if (as->parms[1].items) {	/* privacy bits */
	char *access = as->parms[1].items->data;
	int new;

	if (strpbrk(access, "76543210") != 0) {	/* all octal digits */
	    sscanf(access, "%lo", (long unsigned int *) &flags);
	} else {		/* interpret flag bit names */
	    if (strlen(access) != 5) {
	      form_error:
		printf("Access bits must be of the form 'somar', not %s\n",
		       access);
		return PRBADARG;
	    }
	    if (strpbrk(access, "somar-") == 0)
		goto form_error;
	    flags = 0;
	    for (i = 0; i < 5; i++) {
		if (access[i] == flags_upcase[i])
		    new = 2;
		else if (access[i] == flags_dncase[i])
		    new = 1;
		else if (access[i] == '-')
		    new = 0;
		else {
		    printf
			("Access bits out of order or illegal:\n  must be a combination of letters from '%s' or '%s' or hyphen, not %s\n",
			 flags_upcase, flags_dncase, access);
		    return PRBADARG;
		}
		flags <<= flags_shift[i];
		if (flags_shift[i] == 1) {
		    if (new)
			flags |= 1;
		} else
		    flags |= new;
	    }
	}
	mask |= PR_SF_ALLBITS;
    }
    if (as->parms[2].items) {	/* limitgroups */
	code = util_GetInt32(as->parms[2].items->data, &ngroups);
	if (code) {
	    afs_com_err(whoami, code, "because ngroups was: '%s'",
		    as->parms[2].items->data);
	    return code;
	}
	mask |= PR_SF_NGROUPS;
    }
#if FOREIGN
    if (as->parms[3].items) {	/* limitgroups */
	code = util_GetInt32(as->parms[3].items->data, &nusers);
	if (code) {
	    afs_com_err(whoami, code, "because nusers was: '%s'",
		    as->parms[3].items->data);
	    return code;
	}
	mask |= PR_SF_NUSERS;
    }
#endif

    for (i = 0; i < ids.idlist_len; i++) {
	afs_int32 id = ids.idlist_val[i];
	char *name = names.namelist_val[i];
	if (id == ANONYMOUSID)
	    continue;
	code = pr_SetFieldsEntry(id, mask, flags, ngroups, nusers);
	if (code) {
	    afs_com_err(whoami, code, "; unable to set fields for %s (id: %d)",
		    name, id);
	    return code;
	}
    }
    if (ids.idlist_val)
	free(ids.idlist_val);
    if (names.namelist_val)
	free(names.namelist_val);
    return 0;
}

static int
ListOwned(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    idlist ids;
    namelist names;
    namelist list;
    int i, j;
    afs_int32 more;

    if (GetNameOrId(as, &ids, &names))
	return PRBADARG;

    for (i = 0; i < ids.idlist_len; i++) {
	afs_int32 oid = ids.idlist_val[i];
	char *name = names.namelist_val[i];

	if (oid == ANONYMOUSID)
	    continue;

	if (oid)
	    printf("Groups owned by %s (id: %d) are:\n", name, oid);
	else
	    printf("Orphaned groups are:\n");
	more = 0;
	do {
	    list.namelist_val = 0;
	    list.namelist_len = 0;
	    code = pr_ListOwned(oid, &list, &more);
	    if (code) {
		afs_com_err(whoami, code,
			"; unable to get owner list for %s (id: %d)", name,
			oid);
		break;
	    }

	    for (j = 0; j < list.namelist_len; j++)
		printf("  %s\n", list.namelist_val[j]);
	    if (list.namelist_val)
		free(list.namelist_val);
	} while (more);
    }

    if (ids.idlist_val)
	free(ids.idlist_val);
    if (names.namelist_val)
	free(names.namelist_val);
    return 0;
}

static void
add_std_args(struct cmd_syndesc *ts)
{
    char test_help[AFSDIR_PATH_MAX];

    sprintf(test_help, "use config file in %s", AFSDIR_SERVER_ETC_DIRPATH);

    cmd_Seek(ts, 16);
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    cmd_AddParm(ts, "-noauth", CMD_FLAG, CMD_OPTIONAL, "run unauthenticated");
    cmd_AddParm(ts, "-test", CMD_FLAG, CMD_OPTIONAL | CMD_HIDE, test_help);
    cmd_AddParm(ts, "-force", CMD_FLAG, CMD_OPTIONAL,
		"Continue oper despite reasonable errors");
    cmd_AddParm(ts, "-localauth", CMD_FLAG, CMD_OPTIONAL,
		"use local authentication");
    cmd_AddParm(ts, "-auth", CMD_FLAG, CMD_OPTIONAL,
		"use user's authentication (default)");
    cmd_AddParm(ts, "-encrypt", CMD_FLAG, CMD_OPTIONAL,
		"encrypt commands");
}

/*
static void add_NameOrId_args (ts)
  struct cmd_syndesc *ts;
{
    cmd_AddParm(ts,"-name",CMD_LIST,CMD_OPTIONAL,"user or group name");
    cmd_AddParm(ts,"-id",CMD_LIST,CMD_OPTIONAL,"user or group id");
}
*/

#include "AFS_component_version_number.c"

int
main(int argc, char **argv)
{
    afs_int32 code;
    struct cmd_syndesc *ts;

    char line[2048];
    char *cp, *lastp;
    int parsec;
    char *parsev[CMD_MAXPARMS];
    char *savec;
    struct authstate state;

#ifdef WIN32
    WSADATA WSAjunk;
#endif

#ifdef WIN32
    WSAStartup(0x0101, &WSAjunk);
#endif

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

    memset(&state, 0, sizeof(state));
    state.sec = 1; /* default is auth */

    ts = cmd_CreateSyntax("creategroup", CreateGroup, NULL,
			  "create a new group");
    cmd_AddParm(ts, "-name", CMD_LIST, 0, "group name");
    cmd_AddParm(ts, "-owner", CMD_SINGLE, CMD_OPTIONAL, "owner of the group");
    cmd_AddParm(ts, "-id", CMD_LIST, CMD_OPTIONAL,
		"id (negated) for the group");
    add_std_args(ts);
    cmd_CreateAlias(ts, "cg");

    ts = cmd_CreateSyntax("createuser", CreateUser, NULL, "create a new user");
    cmd_AddParm(ts, "-name", CMD_LIST, 0, "user name");
    cmd_AddParm(ts, "-id", CMD_LIST, CMD_OPTIONAL, "user id");
    add_std_args(ts);
    cmd_CreateAlias(ts, "cu");

    ts = cmd_CreateSyntax("adduser", AddToGroup, NULL, "add a user to a group");
    cmd_AddParm(ts, "-user", CMD_LIST, 0, "user name");
    cmd_AddParm(ts, "-group", CMD_LIST, 0, "group name");
    add_std_args(ts);

    ts = cmd_CreateSyntax("removeuser", RemoveFromGroup, NULL,
			  "remove a user from a group");
    cmd_AddParm(ts, "-user", CMD_LIST, 0, "user name");
    cmd_AddParm(ts, "-group", CMD_LIST, 0, "group name");
    add_std_args(ts);

    ts = cmd_CreateSyntax("membership", ListMembership, NULL,
			  "list membership of a user or group");
    cmd_AddParm(ts, "-nameorid", CMD_LIST, 0, "user or group name or id");
    cmd_AddParm(ts, "-supergroups", CMD_FLAG, CMD_OPTIONAL, "show supergroups");
    cmd_AddParm(ts, "-expandgroups", CMD_FLAG, CMD_OPTIONAL, "expand super and sub group membership");
    add_std_args(ts);
    cmd_CreateAlias(ts, "groups");

    ts = cmd_CreateSyntax("delete", Delete, NULL,
			  "delete a user or group from database");
    cmd_AddParm(ts, "-nameorid", CMD_LIST, 0, "user or group name or id");
    add_std_args(ts);

    ts = cmd_CreateSyntax("examine", CheckEntry, NULL, "examine an entry");
    cmd_AddParm(ts, "-nameorid", CMD_LIST, 0, "user or group name or id");
    add_std_args(ts);
    cmd_CreateAlias(ts, "check");

    ts = cmd_CreateSyntax("chown", ChownGroup, NULL,
			  "change ownership of a group");
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "group name");
    cmd_AddParm(ts, "-owner", CMD_SINGLE, 0, "new owner");
    add_std_args(ts);

    ts = cmd_CreateSyntax("rename", ChangeName, NULL, "rename user or group");
    cmd_AddParm(ts, "-oldname", CMD_SINGLE, 0, "old name");
    cmd_AddParm(ts, "-newname", CMD_SINGLE, 0, "new name");
    add_std_args(ts);
    cmd_CreateAlias(ts, "chname");

    ts = cmd_CreateSyntax("listmax", ListMax, NULL, "list max id");
    add_std_args(ts);

    ts = cmd_CreateSyntax("setmax", SetMaxCommand, NULL, "set max id");
    cmd_AddParm(ts, "-group", CMD_SINGLE, CMD_OPTIONAL, "group max");
    cmd_AddParm(ts, "-user", CMD_SINGLE, CMD_OPTIONAL, "user max");
    add_std_args(ts);

    ts = cmd_CreateSyntax("setfields", SetFields, NULL,
			  "set fields for an entry");
    cmd_AddParm(ts, "-nameorid", CMD_LIST, 0, "user or group name or id");
    cmd_AddParm(ts, "-access", CMD_SINGLE, CMD_OPTIONAL, "set privacy flags");
    cmd_AddParm(ts, "-groupquota", CMD_SINGLE, CMD_OPTIONAL,
		"set limit on group creation");
#if FOREIGN
    cmd_AddParm(ts, "-userquota", CMD_SINGLE, CMD_OPTIONAL,
		"set limit on foreign user creation");
#endif
    add_std_args(ts);

    ts = cmd_CreateSyntax("listowned", ListOwned, NULL,
			  "list groups owned by an entry or zero id gets orphaned groups");
    cmd_AddParm(ts, "-nameorid", CMD_LIST, 0, "user or group name or id");
    add_std_args(ts);

    ts = cmd_CreateSyntax("listentries", ListEntries, NULL,
			  "list users/groups in the protection database");
    cmd_AddParm(ts, "-users", CMD_FLAG, CMD_OPTIONAL, "list user entries");
    cmd_AddParm(ts, "-groups", CMD_FLAG, CMD_OPTIONAL, "list group entries");
    add_std_args(ts);

    ts = cmd_CreateSyntax("interactive", pts_Interactive, NULL,
			  "enter interactive mode");
    add_std_args(ts);
    cmd_CreateAlias(ts, "in");

    ts = cmd_CreateSyntax("quit", pts_Quit, NULL, "exit program");
    add_std_args(ts);

    ts = cmd_CreateSyntax("source", pts_Source, NULL, "read commands from file");
    cmd_AddParm(ts, "-file", CMD_SINGLE, 0, "filename");
    add_std_args(ts);

    ts = cmd_CreateSyntax("sleep", pts_Sleep, NULL, "pause for a bit");
    cmd_AddParm(ts, "-delay", CMD_SINGLE, 0, "seconds");
    add_std_args(ts);

    cmd_SetBeforeProc(GetGlobals, &state);

    finished = 1;
    source = NULL;
    if ((code = cmd_Dispatch(argc, argv))) {
	CleanUp(NULL, NULL);
	exit(1);
    }
    while (source && !finished) {
	if (isatty(fileno(source)))
	    fprintf(stderr, "pts> ");
	if (!fgets(line, sizeof line, source)) {
	    if (!popsource())
		break;
	    continue;
	}
	lastp = 0;
	for (cp = line; *cp; ++cp)
	    if (!isspace(*cp))
		lastp = 0;
	    else if (!lastp)
		lastp = cp;
	if (lastp)
	    *lastp = 0;
	if (!*line)
	    continue;
	code =
	    cmd_ParseLine(line, parsev, &parsec,
			  sizeof(parsev) / sizeof(*parsev));
	if (code) {
	    afs_com_err(whoami, code, "parsing line: <%s>", line);
	    exit(2);
	}
	savec = parsev[0];
	parsev[0] = argv[0];
	code = cmd_Dispatch(parsec, parsev);
	parsev[0] = savec;
	cmd_FreeArgv(parsev);
    }
    CleanUp(NULL, NULL);
    exit(0);
}
