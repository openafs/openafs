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
#include <afs/opr.h>

#include <rx/rx.h>
#include <rx/xdr.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/afsutil.h>
#include <afs/com_err.h>

#include "ptclient.h"
#include "ptuser.h"
#include "pterror.h"

struct ubik_client *pruclient = 0;
static afs_int32 lastLevel;	/* security level pruclient, if any */

static char *whoami = "libprot";


#define ID_HASH_SIZE 1024
#define ID_STACK_SIZE 1024

/**
 * Hash table chain of user and group ids.
 */
struct idchain {
    struct idchain *next;
    afs_int32 id;
};

/**
 * Hash table of user and group ids.
 */
struct idhash {
    afs_uint32 userEntries;         /**< number of user id entries hashed */
    afs_uint32 groupEntries;        /**< number of group id entries hashed */
    struct idchain *hash[ID_HASH_SIZE];
};

/**
 * Allocate a new id hash table.
 */
static afs_int32
AllocateIdHash(struct idhash **aidhash)
{
    struct idhash *idhash;

    idhash = calloc(1, sizeof(struct idhash));
    if (!idhash) {
       return ENOMEM;
    }
    *aidhash = idhash;
    return 0;
}

/**
 * Free the id hash.
 */
static void
FreeIdHash(struct idhash *idhash)
{
    int index;
    struct idchain *chain;
    struct idchain *next;

    for (index = 0; index < ID_HASH_SIZE; index++) {
       for (chain = idhash->hash[index]; chain; chain = next) {
           next = chain->next;
           free(chain);
       }
    }
    free(idhash);
}

/**
 * Indicate if group/user id is already hashed, and
 * if not insert it.
 *
 * @returns whether id is present
 *   @retval >0 id is already present in the hash
 *   @retval 0  id was not found and was inserted into the hash
 *   @retval <0 error encountered
 */
static afs_int32
FindId(struct idhash *idhash, afs_int32 id)
{
    afs_int32 index;
    struct idchain *chain;
    struct idchain *newChain;

    index = abs(id) % ID_HASH_SIZE;
    for (chain = idhash->hash[index]; chain; chain = chain->next) {
	if (chain->id == id) {
	    return 1;
	}
    }

    /* Insert this id but return not found. */
    newChain = malloc(sizeof(struct idchain));
    if (!newChain) {
	return ENOMEM;
    } else {
	newChain->id = id;
	newChain->next = idhash->hash[index];
	idhash->hash[index] = newChain;
	if (id < 0) {
	    idhash->groupEntries++;
	} else {
	    idhash->userEntries++;
	}
    }
    return 0;
}

/**
 * Create an idlist from the ids in the hash.
 */
static afs_int32
CreateIdList(struct idhash *idhash, idlist * alist, afs_int32 select)
{
    struct idchain *chain;
    afs_int32 entries = 0;
    int index;
    int i;

    if (select & PRGROUPS) {
	entries += idhash->groupEntries;
    }
    if (select & PRUSERS) {
	entries += idhash->userEntries;
    }
    if (entries == 0) {
	alist->idlist_len = 0;
	alist->idlist_val = NULL;
	return 0;
    }

    alist->idlist_len = entries;
    alist->idlist_val = malloc(sizeof(afs_int32) * entries);
    if (!alist->idlist_val) {
	return ENOMEM;
    }

    for (i = 0, index = 0; index < ID_HASH_SIZE; index++) {
	for (chain = idhash->hash[index]; chain; chain = chain->next) {
	    if (chain->id < 0) {
		if (select & PRGROUPS) {
		    alist->idlist_val[i++] = chain->id;
		}
	    } else {
		if (select & PRUSERS) {
		    alist->idlist_val[i++] = chain->id;
		}
	    }
	}
    }
    return 0;
}

afs_int32
pr_Initialize(IN afs_int32 secLevel, IN const char *confDir, IN char *cell)
{
    afs_int32 code;
    struct rx_connection *serverconns[MAXSERVERS];
    struct rx_securityClass *sc = NULL;
    static struct afsconf_dir *tdir = NULL;	/* only do this once */
    static char tconfDir[100] = "";
    static char tcell[64] = "";
    afs_int32 scIndex;
    afs_int32 secFlags;
    static struct afsconf_cell info;
    afs_int32 i;
    char cellstr[64];
    afs_int32 gottdir = 0;
    afs_int32 refresh = 0;

    initialize_PT_error_table();
    initialize_RXK_error_table();
    initialize_ACFG_error_table();
    initialize_KTC_error_table();

    if (!cell) {
        if (!tdir)
            tdir = afsconf_Open(confDir);
	if (!tdir) {
	    if (confDir && strcmp(confDir, ""))
		fprintf(stderr,
			"%s: Could not open configuration directory: %s.\n",
			whoami, confDir);
            else
		fprintf(stderr,
			"%s: No configuration directory specified.\n",
			whoami);
	    return -1;
	}
        gottdir = 1;

        code = afsconf_GetLocalCell(tdir, cellstr, sizeof(cellstr));
        if (code) {
            fprintf(stderr,
                     "libprot: Could not get local cell. [%d]\n", code);
            return code;
        }
        cell = cellstr;
    }

    if (tdir == NULL || strcmp(confDir, tconfDir) || strcmp(cell, tcell)) {
	/*
	 * force re-evaluation.  we either don't have an afsconf_dir,
         * the directory has changed or the cell has changed.
	 */
	if (tdir && !gottdir) {
	    afsconf_Close(tdir);
            tdir = NULL;
        }
	pruclient = NULL;
        refresh = 1;
    }

    if (refresh) {
	strncpy(tconfDir, confDir, sizeof(tconfDir));
        strncpy(tcell, cell, sizeof(tcell));

        if (!gottdir)
            tdir = afsconf_Open(confDir);
	if (!tdir) {
	    if (confDir && strcmp(confDir, ""))
		fprintf(stderr,
			"libprot: Could not open configuration directory: %s.\n",
			confDir);
            else
		fprintf(stderr,
			"libprot: No configuration directory specified.\n");
	    return -1;
	}

	code = afsconf_GetCellInfo(tdir, cell, "afsprot", &info);
	if (code) {
	    fprintf(stderr, "libprot: Could not locate cell %s in %s/%s\n",
		    cell, confDir, AFSDIR_CELLSERVDB_FILE);
	    return code;
	}
    }

    /* If we already have a client and it is at the security level we
     * want, don't get a new one. Unless the security level is 2 in
     * which case we will get one (and re-read the key file).
     */
    if (pruclient && (lastLevel == secLevel) && (secLevel != 2)) {
	return 0;
    }

    code = rx_Init(0);
    if (code) {
	fprintf(stderr, "libprot:  Could not initialize rx.\n");
	return code;
    }

    /* Most callers use secLevel==1, however, the fileserver uses secLevel==2
     * to force use of the KeyFile.  secLevel == 0 implies -noauth was
     * specified. */
    if (secLevel == 2) {
	/* If secLevel is two assume we're on a file server and use
	 * ClientAuthSecure if possible. */
	code = afsconf_ClientAuthSecure(tdir, &sc, &scIndex);
	if (code)
	    afs_com_err(whoami, code, "(calling client secure)\n");
    } else if (secLevel > 0) {
	secFlags = 0;
	if (secLevel > 1)
	    secFlags |= AFSCONF_SECOPTS_ALWAYSENCRYPT;

	code = afsconf_ClientAuthToken(&info, secFlags, &sc, &scIndex, NULL);
	if (code) {
	    afs_com_err(whoami, code, "(getting token)");
	    if (secLevel > 1)
		return code;
	}
    }

    if (sc == NULL) {
	sc = rxnull_NewClientSecurityObject();
        scIndex = RX_SECIDX_NULL;
    }

    if ((scIndex == RX_SECIDX_NULL) && (secLevel != 0))
	fprintf(stderr,
		"%s: Could not get afs tokens, running unauthenticated\n",
		whoami);

    memset(serverconns, 0, sizeof(serverconns));	/* terminate list!!! */
    for (i = 0; i < info.numServers; i++)
	serverconns[i] =
	    rx_NewConnection(info.hostAddr[i].sin_addr.s_addr,
			     info.hostAddr[i].sin_port, PRSRV, sc,
			     scIndex);

    code = ubik_ClientInit(serverconns, &pruclient);
    if (code) {
	afs_com_err(whoami, code, "ubik client init failed.");
	return code;
    }
    lastLevel = scIndex;

    code = rxs_Release(sc);
    return code;
}

int
pr_End(void)
{
    int code = 0;

    if (pruclient) {
	code = ubik_ClientDestroy(pruclient);
	pruclient = 0;
    }
    return code;
}

/*
 * Make sure that arg is a proper C string that fits in a prname.
 * If strnlen(arg, PR_MAXNAMELEN) == PR_MAXNAMELEN, then arg either
 * doesn't have a terminating NUL or is too long, and we can't tell
 * which one in the current API.  This code has always assumed that
 * the names presented to it are valid C strings, but for robustness
 * we can't depend on the server side guaranteeing that.  Unfortunately,
 * the wire protocol uses a vector[PR_MAXNAMELEN] of char, so XDR will
 * not automatically fix up strings generated by the server.
 *
 * The inequality is just belt-and-suspenders and should be impossible.
 */
static_inline int check_length(prname arg)
{
    if (strnlen(arg, PR_MAXNAMELEN) >= PR_MAXNAMELEN)
	return PRNAMETOOLONG;
    return 0;
}

int
pr_CreateUser(prname name, afs_int32 *id)
{
    afs_int32 code;

    code = check_length(name);
    if (code)
	return code;
    stolower(name);
    if (*id) {
	code = ubik_PR_INewEntry(pruclient, 0, name, *id, 0);
    } else {
	code = ubik_PR_NewEntry(pruclient, 0, name, 0, 0, id);
    }
    return code;
}

int
pr_CreateGroup(prname name, prname owner, afs_int32 *id)
{
    afs_int32 code;
    afs_int32 oid = 0;
    afs_int32 flags = 0;

    code = check_length(name);
    if (code)
	return code;
    /* pr_SNameToId will check owner's length. */
    stolower(name);
    if (owner) {
	code = pr_SNameToId(owner, &oid);
	if (code)
	    return code;
	if (oid == ANONYMOUSID)
	    return PRNOENT;
    }
    flags |= PRGRP;
    if (*id) {
	code = ubik_PR_INewEntry(pruclient, 0, name, *id, oid);
    } else {
	code = ubik_PR_NewEntry(pruclient, 0, name, flags, oid, id);
    }
    return code;
}

int
pr_Delete(prname name)
{
    afs_int32 code;
    afs_int32 id;

    /* pr_SNameToId both checks the length of name and lowercases it. */
    code = pr_SNameToId(name, &id);
    if (code)
	return code;
    if (id == ANONYMOUSID)
	return PRNOENT;
    code = ubik_PR_Delete(pruclient, 0, id);
    return code;
}

int
pr_DeleteByID(afs_int32 id)
{
    afs_int32 code;

    code = ubik_PR_Delete(pruclient, 0, id);
    return code;
}

int
pr_AddToGroup(prname user, prname group)
{
    afs_int32 code;
    namelist lnames;
    idlist lids;

    code = check_length(user);
    if (code)
	return code;
    code = check_length(group);
    if (code)
	return code;
    lnames.namelist_len = 2;
    lnames.namelist_val = calloc(2, PR_MAXNAMELEN);
    strncpy(lnames.namelist_val[0], user, PR_MAXNAMELEN);
    strncpy(lnames.namelist_val[1], group, PR_MAXNAMELEN);
    lids.idlist_val = 0;
    lids.idlist_len = 0;
    code = pr_NameToId(&lnames, &lids);
    if (code)
	goto done;
    /* if here, still could be missing an entry */
    if (lids.idlist_len != 2) {
	code = PRINTERNAL;
	goto done;
    }
    if (lids.idlist_val[0] == ANONYMOUSID
	|| lids.idlist_val[1] == ANONYMOUSID) {
	code = PRNOENT;
	goto done;
    }
    code =
	ubik_PR_AddToGroup(pruclient, 0, lids.idlist_val[0],
		  lids.idlist_val[1]);
  done:
    if (lnames.namelist_val)
	free(lnames.namelist_val);

    xdr_free((xdrproc_t) xdr_idlist, &lids);
    return code;
}

int
pr_RemoveUserFromGroup(prname user, prname group)
{
    afs_int32 code;
    namelist lnames;
    idlist lids;

    code = check_length(user);
    if (code)
	return code;
    code = check_length(group);
    if (code)
	return code;
    lnames.namelist_len = 2;
    lnames.namelist_val = calloc(2, PR_MAXNAMELEN);
    strncpy(lnames.namelist_val[0], user, PR_MAXNAMELEN);
    strncpy(lnames.namelist_val[1], group, PR_MAXNAMELEN);
    lids.idlist_val = 0;
    lids.idlist_len = 0;
    code = pr_NameToId(&lnames, &lids);
    if (code)
	goto done;

    if (lids.idlist_len != 2) {
	code = PRINTERNAL;
	goto done;
    }
    if (lids.idlist_val[0] == ANONYMOUSID
	|| lids.idlist_val[1] == ANONYMOUSID) {
	code = PRNOENT;
	goto done;
    }
    code =
	ubik_PR_RemoveFromGroup(pruclient, 0, lids.idlist_val[0],
		  lids.idlist_val[1]);
  done:
    if (lnames.namelist_val)
	free(lnames.namelist_val);

    xdr_free((xdrproc_t) xdr_idlist, &lids);

    return code;
}

int
pr_NameToId(namelist *names, idlist *ids)
{
    afs_int32 code;
    afs_int32 i;

    for (i = 0; i < names->namelist_len; i++) {
	code = check_length(names->namelist_val[i]);
	if (code)
	    return code;
	stolower(names->namelist_val[i]);
    }
    code = ubik_PR_NameToID(pruclient, 0, names, ids);
    if (code == 0 && ids->idlist_len != names->namelist_len) {
	code = PRINTERNAL;
    }
    return code;
}

int
pr_SNameToId(prname name, afs_int32 *id)
{
    namelist lnames;
    idlist lids;
    afs_int32 code;

    code = check_length(name);
    if (code)
	return code;
    lids.idlist_len = 0;
    lids.idlist_val = 0;
    lnames.namelist_len = 1;
    lnames.namelist_val = calloc(1, PR_MAXNAMELEN);
    stolower(name);
    strncpy(lnames.namelist_val[0], name, PR_MAXNAMELEN);
    code = ubik_PR_NameToID(pruclient, 0, &lnames, &lids);
    if (lids.idlist_val) {
	*id = *lids.idlist_val;
	xdr_free((xdrproc_t) xdr_idlist, &lids);
    } else if (code == 0) {
	code = PRINTERNAL;
    }
    if (lnames.namelist_val)
	free(lnames.namelist_val);
    return code;
}

/*
 * Like ubik_PR_IDToName, but enforces that the output prnames are
 * interpretable as C strings (i.e., NUL-terminated).
 */
int
string_PR_IDToName(struct ubik_client *client, afs_int32 flags,
		   idlist *ids, namelist *names)
{
    afs_int32 code;
    int i;

    code = ubik_PR_IDToName(client, flags, ids, names);
    if (code)
	return code;
    if (names->namelist_len != ids->idlist_len) {
	return PRINTERNAL;
    }
    for (i = 0; i < names->namelist_len; i++) {
	code = check_length(names->namelist_val[i]);
	if (code)
	    return code;
    }
    return code;
}


int
pr_IdToName(idlist *ids, namelist *names)
{
    return string_PR_IDToName(pruclient, 0, ids, names);
}

int
pr_SIdToName(afs_int32 id, prname name)
{
    namelist lnames;
    idlist lids;
    afs_int32 code;

    lids.idlist_len = 1;
    lids.idlist_val = malloc(sizeof(afs_int32));
    *lids.idlist_val = id;
    lnames.namelist_len = 0;
    lnames.namelist_val = 0;
    code = pr_IdToName(&lids, &lnames);
    if (lnames.namelist_val)
	strncpy(name, lnames.namelist_val[0], PR_MAXNAMELEN);
    else if (code == 0)
	code = PRINTERNAL;

    if (lids.idlist_val)
	free(lids.idlist_val);

    xdr_free((xdrproc_t) xdr_namelist, &lnames);

    return code;
}

int
pr_GetCPS(afs_int32 id, prlist *CPS)
{
    afs_int32 code;
    afs_int32 over;

    over = 0;
    code = ubik_PR_GetCPS(pruclient, 0, id, CPS, &over);
    if (code != PRSUCCESS)
	return code;
    if (over) {
	/* do something about this, probably make a new call */
	/* don't forget there's a hard limit in the interface */
	fprintf(stderr, "membership list for id %d exceeds display limit\n",
		id);
    }
    return 0;
}

int
pr_GetCPS2(afs_int32 id, afs_uint32 host, prlist *CPS)
{
    afs_int32 code;
    afs_int32 over;

    over = 0;
    code = ubik_PR_GetCPS2(pruclient, 0, id, host, CPS, &over);
    if (code != PRSUCCESS)
	return code;
    if (over) {
	/* do something about this, probably make a new call */
	/* don't forget there's a hard limit in the interface */
	fprintf(stderr, "membership list for id %d exceeds display limit\n",
		id);
    }
    return 0;
}

int
pr_GetHostCPS(afs_uint32 host, prlist *CPS)
{
    afs_int32 code;
    afs_int32 over;

    over = 0;
    code = ubik_PR_GetHostCPS(pruclient, 0, host, CPS, &over);
    if (code != PRSUCCESS)
	return code;
    if (over) {
	/* do something about this, probably make a new call */
	/* don't forget there's a hard limit in the interface */
	fprintf(stderr,
		"membership list for host id %d exceeds display limit\n",
		host);
    }
    return 0;
}

int
pr_ListMembers(prname group, namelist *lnames)
{
    afs_int32 code;
    afs_int32 gid;
    int i;

    memset(lnames, 0, sizeof(namelist));

    /* pr_SNameToId checks the length of group. */
    code = pr_SNameToId(group, &gid);
    if (code)
	return code;
    if (gid == ANONYMOUSID)
	return PRNOENT;
    code = pr_IDListMembers(gid, lnames);
    if (code)
	return code;
    for (i = 0; i < lnames->namelist_len; i++) {
	code = check_length(lnames->namelist_val[i]);
	if (code)
	    return code;
    }
    return code;
}

int
pr_ListOwned(afs_int32 oid, namelist *lnames, afs_int32 *moreP)
{
    afs_int32 code;
    prlist alist;
    idlist *lids;

    alist.prlist_len = 0;
    alist.prlist_val = 0;
    code = ubik_PR_ListOwned(pruclient, 0, oid, &alist, moreP);
    if (code)
	return code;
    if (*moreP == 1) {
	/* Remain backwards compatible when moreP was a T/F bit */
	fprintf(stderr, "membership list for id %d exceeds display limit\n",
		oid);
	*moreP = 0;
    }
    lids = (idlist *) &alist;
    code = pr_IdToName(lids, lnames);

    xdr_free((xdrproc_t) xdr_prlist, &alist);

    if (code)
	return code;

    return PRSUCCESS;
}

int
pr_IDListMembers(afs_int32 gid, namelist *lnames)
{
    afs_int32 code;
    prlist alist;
    idlist *lids;
    afs_int32 over;

    alist.prlist_len = 0;
    alist.prlist_val = 0;
    code = ubik_PR_ListElements(pruclient, 0, gid, &alist, &over);
    if (code)
	return code;
    if (over) {
	fprintf(stderr, "membership list for id %d exceeds display limit\n",
		gid);
    }
    lids = (idlist *) &alist;
    code = pr_IdToName(lids, lnames);

    xdr_free((xdrproc_t) xdr_prlist, &alist);

    if (code)
	return code;
    return PRSUCCESS;
}

int
pr_IDListExpandedMembers(afs_int32 aid, namelist * lnames)
{
    afs_int32 code;
    afs_int32 gid;
    idlist lids;
    prlist alist;
    afs_int32 over;
    struct idhash *members = NULL;
    afs_int32 *stack = NULL;
    afs_int32 maxstack = ID_STACK_SIZE;
    int n = 0;			/* number of ids stacked */
    int i;
    int firstpass = 1;

    code = AllocateIdHash(&members);
    if (code) {
	return code;
    }
    stack = malloc(sizeof(afs_int32) * maxstack);
    if (!stack) {
	code = ENOMEM;
	goto done;
    }

    stack[n++] = aid;
    while (n) {
	gid = stack[--n];	/* pop next group id */
	alist.prlist_len = 0;
	alist.prlist_val = NULL;
	if (firstpass || aid < 0) {
	    firstpass = 0;
	    code = ubik_PR_ListElements(pruclient, 0, gid, &alist, &over);
	} else {
	    code = ubik_PR_ListSuperGroups(pruclient, 0, gid, &alist, &over);
	    if (code == RXGEN_OPCODE) {
	        alist.prlist_len = 0;
		alist.prlist_val = NULL;
		code = 0; /* server does not support supergroups. */
	    }
	}
	if (code)
	    goto done;
	if (over) {
	    fprintf(stderr,
		    "membership list for id %d exceeds display limit\n", gid);
	}
	for (i = 0; i < alist.prlist_len; i++) {
	    afs_int32 found;
	    afs_int32 id;

	    id = alist.prlist_val[i];
	    found = FindId(members, id);
	    if (found < 0) {
		code = found;
		xdr_free((xdrproc_t) xdr_prlist, &alist);
		goto done;
	    }
	    if (found == 0 && id < 0) {
		if (n == maxstack) {	/* need more stack space */
		    afs_int32 *tmp;
		    maxstack += n;
		    tmp = realloc(stack, maxstack * sizeof(afs_int32));
		    if (!tmp) {
			code = ENOMEM;
			xdr_free((xdrproc_t) xdr_prlist, &alist);
			goto done;
		    }
		    stack = tmp;
		}
		stack[n++] = id;	/* push group id */
	    }
	}
	xdr_free((xdrproc_t) xdr_prlist, &alist);
    }

    code = CreateIdList(members, &lids, (aid < 0 ? PRUSERS : PRGROUPS));
    if (code) {
	goto done;
    } else if (lids.idlist_len == 0) {
	/* Avoid the RPC when there's nothing to look up. */
	lnames->namelist_len = 0;
	lnames->namelist_val = NULL;
	goto done;
    }
    code = pr_IdToName(&lids, lnames);
    free(lids.idlist_val);

  done:
    if (stack)
	free(stack);
    if (members)
	FreeIdHash(members);
    return code;
}

int
pr_ListEntry(afs_int32 id, struct prcheckentry *aentry)
{
    afs_int32 code;

    code = ubik_PR_ListEntry(pruclient, 0, id, aentry);
    if (code)
	return code;
    return check_length(aentry->name);
}

afs_int32
pr_ListEntries(int flag, afs_int32 startindex, afs_int32 *nentries, struct prlistentries **entries, afs_int32 *nextstartindex)
{
    afs_int32 code;
    int i;
    prentries bulkentries;

    *nentries = 0;
    *entries = NULL;
    *nextstartindex = -1;
    bulkentries.prentries_val = 0;
    bulkentries.prentries_len = 0;

    code =
	ubik_PR_ListEntries(pruclient, 0, flag, startindex,
		  &bulkentries, nextstartindex);
    if (code)
	return code;
    for (i = 0; i < bulkentries.prentries_len; i++) {
	/* XXX should we try to return all the other entries? */
	code = check_length(bulkentries.prentries_val[i].name);
	if (code)
	    goto out;
    }

out:
    if (code != 0) {
	xdr_free((xdrproc_t)xdr_prentries, &bulkentries);
    } else {
	*nentries = bulkentries.prentries_len;
	*entries = bulkentries.prentries_val;
    }
    return code;
}

int
pr_CheckEntryByName(prname name, afs_int32 *id, prname owner, prname creator)
{
    /* struct prcheckentry returns other things, which aren't useful to show at this time. */
    afs_int32 code;
    struct prcheckentry aentry;

    /* pr_SNameToId will check name's length. */
    code = pr_SNameToId(name, id);
    if (code)
	return code;
    if (*id == ANONYMOUSID)
	return PRNOENT;
    code = ubik_PR_ListEntry(pruclient, 0, *id, &aentry);
    if (code)
	return code;
    /* this should be done in one RPC, but I'm lazy. */
    code = pr_SIdToName(aentry.owner, owner);
    if (code)
	return code;
    code = pr_SIdToName(aentry.creator, creator);
    if (code)
	return code;
    return PRSUCCESS;
}

int
pr_CheckEntryById(prname name, afs_int32 id, prname owner, prname creator)
{
    /* struct prcheckentry returns other things, which aren't useful to show at this time. */
    afs_int32 code;
    struct prcheckentry aentry;

    /* XXX ListEntry RPC gives us the name back so should avoid extra RPC */
    code = pr_SIdToName(id, name);
    if (code)
	return code;
    if (id == ANONYMOUSID)
	return PRNOENT;
    code = ubik_PR_ListEntry(pruclient, 0, id, &aentry);
    if (code)
	return code;
    /* this should be done in one RPC, but I'm lazy. */
    code = pr_SIdToName(aentry.owner, owner);
    if (code)
	return code;
    code = pr_SIdToName(aentry.creator, creator);
    if (code)
	return code;
    return PRSUCCESS;
}

int
pr_ChangeEntry(prname oldname, prname newname, afs_int32 *newid, prname newowner)
{
    afs_int32 code;
    afs_int32 id;
    afs_int32 oid = 0;

    /* pr_SNameToId takes care of length checks for us. */
    code = pr_SNameToId(oldname, &id);
    if (code)
	return code;
    if (id == ANONYMOUSID)
	return PRNOENT;
    if (newowner && *newowner) {
	code = pr_SNameToId(newowner, &oid);
	if (code)
	    return code;
	if (oid == ANONYMOUSID)
	    return PRNOENT;
    }
    if (newid)
	code = ubik_PR_ChangeEntry(pruclient, 0, id, newname, oid, *newid);
    else
	code = ubik_PR_ChangeEntry(pruclient, 0, id, newname, oid, 0);
    return code;
}

int
pr_IsAMemberOf(prname uname, prname gname, afs_int32 *flag)
{
    afs_int32 code;
    namelist lnames;
    idlist lids;

    code = check_length(uname);
    if (code)
	return code;
    code = check_length(gname);
    if (code)
	return code;
    stolower(uname);
    stolower(gname);
    lnames.namelist_len = 2;
    lnames.namelist_val = malloc(2 * PR_MAXNAMELEN);
    strncpy(lnames.namelist_val[0], uname, PR_MAXNAMELEN);
    strncpy(lnames.namelist_val[1], gname, PR_MAXNAMELEN);
    lids.idlist_val = 0;
    lids.idlist_len = 0;
    code = pr_NameToId(&lnames, &lids);
    if (code) {
	if (lnames.namelist_val)
	    free(lnames.namelist_val);
	xdr_free((xdrproc_t) xdr_idlist, &lids);
	return code;
    }
    if (lids.idlist_len != 2) {
	free(lnames.namelist_val);
	xdr_free((xdrproc_t) xdr_idlist, &lids);
	return PRINTERNAL;
    }
    code =
	ubik_PR_IsAMemberOf(pruclient, 0, lids.idlist_val[0],
		  lids.idlist_val[1], flag);
    if (lnames.namelist_val)
	free(lnames.namelist_val);
    xdr_free((xdrproc_t) xdr_idlist, &lids);
    return code;
}

int
pr_ListMaxUserId(afs_int32 *mid)
{
    afs_int32 code;
    afs_int32 gid;
    code = ubik_PR_ListMax(pruclient, 0, mid, &gid);
    return code;
}

int
pr_SetMaxUserId(afs_int32 mid)
{
    afs_int32 code;
    afs_int32 flag = 0;
    code = ubik_PR_SetMax(pruclient, 0, mid, flag);
    return code;
}

int
pr_ListMaxGroupId(afs_int32 *mid)
{
    afs_int32 code;
    afs_int32 id;
    code = ubik_PR_ListMax(pruclient, 0, &id, mid);
    return code;
}

int
pr_SetMaxGroupId(afs_int32 mid)
{
    afs_int32 code;
    afs_int32 flag = 0;

    flag |= PRGRP;
    code = ubik_PR_SetMax(pruclient, 0, mid, flag);
    return code;
}

afs_int32
pr_SetFieldsEntry(afs_int32 id, afs_int32 mask, afs_int32 flags, afs_int32 ngroups, afs_int32 nusers)
{
    afs_int32 code;

    code =
	ubik_PR_SetFieldsEntry(pruclient, 0, id, mask, flags, ngroups,
		  nusers, 0, 0);
    return code;
}

int
pr_ListSuperGroups(afs_int32 gid, namelist * lnames)
{
    afs_int32 code;
    prlist alist;
    idlist *lids;
    afs_int32 over;

    alist.prlist_len = 0;
    alist.prlist_val = 0;
    code = ubik_PR_ListSuperGroups(pruclient, 0, gid, &alist, &over);
    if (code)
	return code;
    if (over) {
	fprintf(stderr, "supergroup list for id %d exceeds display limit\n",
		gid);
    }
    lids = (idlist *) & alist;
    code = pr_IdToName(lids, lnames);

    xdr_free((xdrproc_t) xdr_prlist, &alist);
    return code;
}
