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
#include <roken.h>

#include <afs/opr.h>
#include <afs/stds.h>
#include <afs/afs_consts.h>

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <winioctl.h>
#include <winsock2.h>
#include <nb30.h>

#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <strsafe.h>
#include <afs/opr_assert.h>
#include <afs/ptserver.h>
#include <afs/ptuser.h>


#include "fs_acl.h"

static int BadName(char *aname, char *cellname);

void
ZapAcl (struct Acl *acl)
{
    if (!acl)
        return;

    ZapList(acl->pluslist);
    ZapList(acl->minuslist);
    free(acl);
}

void
ZapList (struct AclEntry *alist)
{
    struct AclEntry *tp, *np;
    for (tp = alist; tp; tp = np) {
        np = tp->next;
        free(tp);
    }
}

int
PruneList (struct AclEntry **ae, int dfs)
{
    struct AclEntry **lp;
    struct AclEntry *te, *ne;
    afs_int32 ctr;
    ctr = 0;
    lp = ae;
    for(te = *ae;te;te=ne) {
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
SkipLine (char *astr)
{
    while (*astr != '\0' && *astr !='\n')
        astr++;
    if (*astr == '\n')
	astr++;
    return astr;
}

struct AclEntry *
FindList (struct AclEntry *alist, char *aname)
{
    while (alist) {
        if (!strcasecmp(alist->name, aname))
            return alist;
        alist = alist->next;
    }
    return 0;
}

void
ChangeList (struct Acl *al, afs_int32 plus, char *aname, afs_int32 arights,
	    enum rtype *artypep)
{
    struct AclEntry *tlist;
    tlist = (plus ? al->pluslist : al->minuslist);
    tlist = FindList (tlist, aname);
    if (tlist) {
        /* Found the item already in the list. */
                                /* modify rights in case of reladd    */
                                /* and reladd only, use standard -    */
                                /* add, ie. set - otherwise           */
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
                                /* can't reduce non-existing rights   */
        return;

    /* Otherwise we make a new item and plug in the new data. */
    tlist = (struct AclEntry *) malloc(sizeof (struct AclEntry));
    assert(tlist);
    if( FAILED(StringCbCopy(tlist->name, sizeof(tlist->name), aname))) {
	fprintf (stderr, "name - not enough space");
        exit(1);
    }
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


/*
 * Create an empty acl, taking into account whether the acl pointed
 * to by astr is an AFS or DFS acl. Only parse this minimally, so we
 * can recover from problems caused by bogus ACL's (in that case, always
 * assume that the acl is AFS: for DFS, the user can always resort to
 * acl_edit, but for AFS there may be no other way out).
 */
struct Acl *
EmptyAcl(char *astr)
{
    struct Acl *tp;
    int junk;

    tp = (struct Acl *)calloc(sizeof(*tp), 1);
    assert(tp);
    tp->nplus = tp->nminus = 0;
    tp->pluslist = tp->minuslist = 0;
    tp->dfs = 0;
#if _MSC_VER < 1400
    if (astr == NULL || sscanf(astr, "%d dfs:%d %s", &junk, &tp->dfs, tp->cell) <= 0) {
        tp->dfs = 0;
        tp->cell[0] = '\0';
    }
#else
    if (astr == NULL || sscanf_s(astr, "%d dfs:%d %s", &junk, &tp->dfs, tp->cell, sizeof(tp->cell)) <= 0) {
        tp->dfs = 0;
        tp->cell[0] = '\0';
    }
#endif
    return tp;
}

/* clean up an access control list of its bad entries; return 1 if we made
   any changes to the list, and 0 otherwise */
int
CleanAcl(struct Acl *aa, char *cellname)
{
    struct AclEntry *te, **le, *ne;
    int changes;

    /* Don't correct DFS ACL's for now */
    if (aa->dfs)
	return 0;

    /* prune out bad entries */
    changes = 0;	    /* count deleted entries */
    le = &aa->pluslist;
    for(te = aa->pluslist; te; te=ne) {
	ne = te->next;
	if (BadName(te->name, cellname)) {
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
    for(te = aa->minuslist; te; te=ne) {
	ne = te->next;
	if (BadName(te->name, cellname)) {
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

struct Acl *
ParseAcl (char *astr, int astr_size)
{
    int nplus, nminus, i, trights = 0, ret;
    size_t len;
    char tname[ACL_MAXNAME + 1] = "";
    struct AclEntry *first, *next, *last, *tl;
    struct Acl *ta;

    ta = EmptyAcl(NULL);
    if( FAILED(StringCbLength(astr, astr_size, &len))) {
        fprintf (stderr, "StringCbLength failure on astr");
        exit(1);
    }
    if (astr == NULL || len == 0)
        return ta;

#if _MSC_VER < 1400
    ret = sscanf(astr, "%d dfs:%d %s", &ta->nplus, &ta->dfs, ta->cell);
#else
    ret = sscanf_s(astr, "%d dfs:%d %s", &ta->nplus, &ta->dfs, ta->cell, sizeof(ta->cell));
#endif
    if (ret <= 0) {
        free(ta);
        return NULL;
    }
    astr = SkipLine(astr);
#if _MSC_VER < 1400
    ret = sscanf(astr, "%d", &ta->nminus);
#else
    ret = sscanf_s(astr, "%d", &ta->nminus);
#endif
    if (ret <= 0) {
        free(ta);
        return NULL;
    }
    astr = SkipLine(astr);

    nplus = ta->nplus;
    nminus = ta->nminus;

    last = 0;
    first = 0;
    for(i=0;i<nplus;i++) {
#if _MSC_VER < 1400
        ret = sscanf(astr, "%100s %d", tname, &trights);
#else
        ret = sscanf_s(astr, "%100s %d", tname, sizeof(tname), &trights);
#endif
        if (ret <= 0)
            goto nplus_err;
        astr = SkipLine(astr);
	tl = (struct AclEntry *) calloc(sizeof(*tl), 1);
        if (tl == NULL)
            goto nplus_err;
        if (!first)
            first = tl;
        if( FAILED(StringCbCopy(tl->name, sizeof(tl->name), tname))) {
            fprintf (stderr, "name - not enough space");
            exit(1);
        }
        tl->rights = trights;
        tl->next = 0;
        if (last)
            last->next = tl;
        last = tl;
    }
    ta->pluslist = first;

    last = 0;
    first = 0;
    for(i=0;i<nminus;i++) {
#if _MSC_VER < 1400
        ret = sscanf(astr, "%100s %d", tname, &trights);
#else
        ret = sscanf_s(astr, "%100s %d", tname, sizeof(tname), &trights);
#endif
        if (ret <= 0)
            goto nminus_err;
        astr = SkipLine(astr);
	tl = (struct AclEntry *) calloc(sizeof(*tl), 1);
        if (tl == NULL)
            goto nminus_err;
        if (!first)
            first = tl;
        if( FAILED(StringCbCopy(tl->name, sizeof(tl->name), tname))) {
            fprintf (stderr, "name - not enough space");
            exit(1);
        }
        tl->rights = trights;
        tl->next = 0;
        if (last)
            last->next = tl;
        last = tl;
    }
    ta->minuslist = first;

    return ta;

  nminus_err:
    for (;first; first = next) {
        next = first->next;
        free(first);
    }
    first = ta->pluslist;

  nplus_err:
    for (;first; first = next) {
        next = first->next;
        free(first);
    }
    free(ta);
    return NULL;
}

char *
AclToString(struct Acl *acl)
{
    static char mydata[AFS_PIOCTL_MAXSIZE];
    char tstring[AFS_PIOCTL_MAXSIZE];
    char dfsstring[30];
    struct AclEntry *tp;

    if (acl->dfs) {
        if( FAILED(StringCbPrintf(dfsstring, sizeof(dfsstring), " dfs:%d %s", acl->dfs, acl->cell))) {
            fprintf (stderr, "dfsstring - cannot be populated");
            exit(1);
        }
    } else {
        dfsstring[0] = '\0';
    }
    if( FAILED(StringCbPrintf(mydata, sizeof(mydata), "%d%s\n%d\n", acl->nplus, dfsstring, acl->nminus))) {
        fprintf (stderr, "mydata - cannot be populated");
        exit(1);
    }
    for (tp = acl->pluslist;tp;tp=tp->next) {
        if( FAILED(StringCbPrintf(tstring, sizeof(tstring), "%s %d\n", tp->name, tp->rights))) {
            fprintf (stderr, "tstring - cannot be populated");
            exit(1);
        }
        if( FAILED(StringCbCat(mydata, sizeof(mydata), tstring))) {
            fprintf (stderr, "mydata - not enough space");
            exit(1);
        }
    }
    for (tp = acl->minuslist;tp;tp=tp->next) {
        if( FAILED(StringCbPrintf(tstring, sizeof(tstring), "%s %d\n", tp->name, tp->rights))) {
            fprintf (stderr, "tstring - cannot be populated");
            exit(1);
        }
        if( FAILED(StringCbCat(mydata, sizeof(mydata), tstring))) {
            fprintf (stderr, "mydata - not enough space");
            exit(1);
        }
    }
    return mydata;
}

/*
 * Check if a username is valid: If it contains only digits (or a
 * negative sign), then it might be bad.  If we know the cellname,
 * then query the ptserver to see if the entry is recognized.
 */
static int
BadName(char *aname, char *cellname)
{
    afs_int32 tc, code, id = 0;
    char *nm;

    for ( nm = aname; tc = *nm; nm++) {
	/* all must be '-' or digit to be bad */
	if (tc != '-' && (tc < '0' || tc > '9'))
            return 0;
    }

    if (cellname) {
        char confDir[257];

        /* Go to the PRDB and see if this all number username is valid */
        cm_GetConfigDir(confDir, sizeof(confDir));

        pr_Initialize(1, confDir, cellname);
        code = pr_SNameToId(aname, &id);
        pr_End();
    }

    /* 1=>Not-valid; 0=>Valid */
    return ((!code && (id == ANONYMOUSID)) ? 1 : 0);
}


