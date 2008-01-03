/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
	Information Technology Center
	Carnegie-Mellon University
*/


#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <string.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/ptclient.h>
#include "acl.h"

#ifdef AFS_PTHREAD_ENV
#include <assert.h>
#include <pthread.h>
pthread_mutex_t acl_list_mutex;
#endif /* AFS_PTHREAD_ENV */

static int AddToList();
static int GetFromList();

struct freeListEntry {
    struct freeListEntry *next;
    int size;
    char body[1];
};

struct freeListEntry *freeList;

/*todo: for sorting acls - make sure they work with new groups lists 10/5*/
static int
CmpPlus(a, b)
     struct acl_accessEntry *a, *b;
{
    if (a->id < b->id)
	return (-1);
    if (a->id == b->id)
	return (0);
    return (1);
}

static int
CmpMinus(a, b)
     struct acl_accessEntry *a, *b;
{
    if (a->id > b->id)
	return (-1);
    if (a->id == b->id)
	return (0);
    return (1);
}

static int
CmpInt(x, y)
     afs_int32 x, y;
{
    if (x < y)
	return (-1);
    if (x == y)
	return (0);
    return (1);
}


int
acl_NewACL(int nEntries, struct acl_accessList **acl)
{
    /* Creates an access list capable of holding at least nEntries entries.
     * Returns 0 on success; aborts if we run out of memory. */

    int t;
    struct freeListEntry *e;

    t = sizeof(struct acl_accessList) + (nEntries -
					 1) * sizeof(struct acl_accessEntry);
    if (GetFromList(&freeList, &e, t) < 0) {
	e = (struct freeListEntry *)malloc(t + sizeof(int) +
					   sizeof(struct freeListEntry *));
	if (e == NULL) {
	    perror("acl_NewACL: malloc() failed");
	    abort();
	}
	e->size = t;
	*acl = (struct acl_accessList *)(e->body);
    } else
	*acl = (struct acl_accessList *)(e->body);

    (*acl)->size = t;		/* May be less than actual size of storage */
    (*acl)->version = ACL_ACLVERSION;
    (*acl)->total = nEntries;
    (*acl)->positive = (*acl)->negative = 0;
    return (0);
}


int
acl_FreeACL(struct acl_accessList **acl)
{
    /* Releases the access list defined by acl.  Returns 0 always. */
    struct freeListEntry *x;

    x = (struct freeListEntry *)
	((char *)*acl - sizeof(struct freeListEntry *) - sizeof(int));
    *acl = NULL;
    return (AddToList(&freeList, x));
}

int
acl_NewExternalACL(int nEntries, char **r)
{
    /* Puts an external acl big enough to hold nEntries in r.  Returns 0 on success, aborts if insufficient memory. */

    int t;
    struct freeListEntry *e;

    t = 20 + (nEntries) * (PR_MAXNAMELEN + 20);
    /* Conservative estimate: enough space in each entry for longest 
     * name plus decimal 2**32 (for largest rights mask) plus some formatting */

    if (GetFromList(&freeList, &e, t)) {
	e = (struct freeListEntry *)malloc(t + sizeof(int) +
					   sizeof(struct freeListEntry *));
	if (e == NULL) {
	    perror("acl_NewExternalACL(): malloc() failed");
	    abort();
	}
	e->size = t;
    }

    *r = e->body;
    sprintf(*r, "0\n0\n");
    return (0);
}

int
acl_FreeExternalACL(char **r)
{
    /* Releases the external access list defined by r.  Returns 0 always.  */

    struct freeListEntry *x;

    x = (struct freeListEntry *)
	((char *)*r - sizeof(struct freeListEntry *) - sizeof(int));
    *r = NULL;
    return (AddToList(&freeList, x));
}


int
acl_Externalize(struct acl_accessList *acl, char **elist)
{
    /* Converts the access list defined by acl into the external access list in elist.  Non-translatable id's are converted to their ASCII string representations.  Returns 0 on success, -1 if number of entries exceeds ACL_MAXENTRIES, or a failure code from the protection server if the problem occured there. */

    register int i;
    register int j;
    register code;
    register char *nextc;
    idlist lids;
    namelist lnames;

    if (acl->total > ACL_MAXENTRIES)
	return (-1);
    acl_NewExternalACL(acl->total, elist);
    nextc = *elist;
    lids.idlist_val =
	(afs_int32 *) malloc(ACL_MAXENTRIES * sizeof(afs_int32));
    memset(lids.idlist_val, 0, ACL_MAXENTRIES * sizeof(afs_int32));
    lids.idlist_len = acl->total;
    lnames.namelist_len = 0;
    lnames.namelist_val = (prname *) 0;
    sprintf(nextc, "%d\n%d\n", acl->positive, acl->negative);
    nextc += strlen(nextc);
    for (i = 0; i < acl->positive; i++)
	lids.idlist_val[i] = acl->entries[i].id;
    j = i;
    for (i = acl->total - 1; i >= acl->total - acl->negative; i--, j++)
	lids.idlist_val[j] = acl->entries[i].id;
    code = pr_IdToName(&lids, &lnames);
    if (code != 0) {
	if (lids.idlist_val)
	    free(lids.idlist_val);
	if (lnames.namelist_val)
	    free(lnames.namelist_val);
	return code;
    }
    for (i = 0; i < acl->positive; i++) {
	sprintf(nextc, "%s", lnames.namelist_val[i]);
	nextc += strlen(nextc);
	sprintf(nextc, "\t%d\n", acl->entries[i].rights);
	nextc += strlen(nextc);
    }
    j = i;
    for (i = acl->total - 1; i >= acl->total - acl->negative; i--, j++) {
	sprintf(nextc, "%s", lnames.namelist_val[j]);
	nextc += strlen(nextc);
	sprintf(nextc, "\t%d\n", acl->entries[i].rights);
	nextc += strlen(nextc);
    }
    if (lids.idlist_val)
	free(lids.idlist_val);
    if (lnames.namelist_val)
	free(lnames.namelist_val);
    return (0);
}


int
acl_Internalize(char *elist, struct acl_accessList **acl)
{
    /* Converts the external access list elist into the access list acl.  Returns 0 on success, -1 if ANY name is not translatable, or if the number of entries exceeds al_maxExtEntries. */
    register int i;
    register int j;
    register char *nextc;
    register afs_int32 code;
    int p, n;
    namelist lnames;
    idlist lids;

    if (sscanf(elist, "%d\n%d\n", &p, &n) != 2)
	return -1;
    if (p + n > ACL_MAXENTRIES)
	return (-1);
    acl_NewACL(p + n, acl);
    (*acl)->total = p + n;
    (*acl)->positive = p;
    (*acl)->negative = n;
    if ((*acl)->total == 0) {
	/* Empty acl entry; simply return success */
	return 0;
    }
    lnames.namelist_len = (*acl)->total;
    lnames.namelist_val =
	(prname *) malloc(lnames.namelist_len * PR_MAXNAMELEN);
    if (lnames.namelist_val == 0) {
	return -1;
    }
    nextc = elist;
    while (*nextc && *nextc != '\n')
	nextc++;
    nextc++;
    while (*nextc && *nextc != '\n')
	nextc++;
    nextc++;			/* now at the beginning of the entry list */
    for (i = 0; i < (*acl)->positive; i++) {
	int k;
	if (sscanf(nextc, "%s\t%d\n", lnames.namelist_val[i], &k) != 2) {
	    free(lnames.namelist_val);
	    return (-1);
	}
	(*acl)->entries[i].rights = k;
	nextc = strchr(nextc, '\n');
	nextc++;		/* 1 + index can cast ptr to integer */
    }
    j = i;
    for (i = (*acl)->total - 1; i >= (*acl)->total - (*acl)->negative;
	 i--, j++) {
	if (sscanf
	    (nextc, "%s\t%d\n", lnames.namelist_val[j],
	     &((*acl)->entries[j].rights)) != 2) {
	    free(lnames.namelist_val);
	    return (-1);
	}
	nextc = strchr(nextc, '\n');
	nextc++;
    }
    lids.idlist_len = 0;
    lids.idlist_val = 0;

    code = pr_NameToId(&lnames, &lids);
    if (code) {
	free(lnames.namelist_val);
	if (lids.idlist_val)
	    free(lids.idlist_val);
	return -1;
    }
    for (i = 0; i < (*acl)->positive; i++) {
	if (lids.idlist_val[i] == ANONYMOUSID) {
	    free(lnames.namelist_val);
	    if (lids.idlist_val)
		free(lids.idlist_val);
	    return -1;
	}
	(*acl)->entries[i].id = lids.idlist_val[i];
    }
    j = i;
    for (i = (*acl)->total - 1; i >= (*acl)->total - (*acl)->negative; i--) {
	if (lids.idlist_val[i] == ANONYMOUSID) {
	    free(lnames.namelist_val);
	    if (lids.idlist_val)
		free(lids.idlist_val);
	    return -1;
	}
	(*acl)->entries[i].id = lids.idlist_val[i];
    }
    /* sort for easier lookup */
    qsort(&((*acl)->entries[0]), (*acl)->positive,
	  sizeof(struct acl_accessEntry), CmpPlus);
    qsort(&((*acl)->entries[(*acl)->total - (*acl)->negative]),
	  (*acl)->negative, sizeof(struct acl_accessEntry), CmpMinus);
    free(lnames.namelist_val);
    if (lids.idlist_val)
	free(lids.idlist_val);
    return (0);
}


int
acl_CheckRights(struct acl_accessList *acl, prlist *groups, int *rights)
{
    /* Returns the rights given by acl to groups */

    int temprights;		/* positive rights accumulated so far */
    int negrights;		/* negative rights accumulated so far */
    int a;			/* index into next entry in acl */
    int c;			/* index into next entry in CPS */

    /* more sanity checks */
    if (acl->total > ACL_MAXENTRIES)
	return 1;
    if (acl->total < 0)
	return 1;
    if (acl->size > 192)
	return 1;		/* 192 is the room in a 256 byte vnode reserved for the ACL */

    if (acl->total <= 0 || groups->prlist_len <= 0) {
	*rights = 0;
	return (0);
    }
    if (groups->prlist_val[groups->prlist_len - 1] == SYSADMINID) {
	*rights = -1;
	return 0;
    }

    /* Each iteration eats up exactly one entry from either acl or groups.
     * Duplicate Entries in access list ==> accumulated rights are obtained.
     * Duplicate Entries in groups ==> irrelevant */
    temprights = 0;
    c = a = 0;
    while ((a < acl->positive) && (c < groups->prlist_len))
	switch (CmpInt(acl->entries[a].id, groups->prlist_val[c])) {
	case -1:
	    a += 1;
	    break;

	case 0:
	    temprights |= acl->entries[a].rights;
	    a += 1;
	    break;

	case 1:
	    c += 1;
	    break;

	default:
	    printf("CmpInt() returned bogus value. Aborting ...\n");
	    abort();
	}
    negrights = 0;
    c = 0;
    a = acl->total - 1;
    while ((c < groups->prlist_len) && (a > acl->total - acl->negative - 1))
	switch (CmpInt(acl->entries[a].id, groups->prlist_val[c])) {
	case -1:
	    a -= 1;
	    break;
	case 0:
	    negrights |= acl->entries[a].rights;
	    a -= 1;
	    break;
	case 1:
	    c += 1;
	    break;
	}
    *rights = temprights & (~negrights);
    return (0);
}

int
acl_Initialize(char *version)
{
    /* I'm sure we need to do some initialization, I'm just not quite sure what yet! */
    if (strcmp(version, ACL_VERSION) != 0) {
	fprintf(stderr, "Wrong version of acl package!\n");
	fprintf(stderr, "This is version %s, file server passed in %s.\n",
		ACL_VERSION, version);
    }
#ifdef AFS_PTHREAD_ENV
    assert(pthread_mutex_init(&acl_list_mutex, NULL) == 0);
#endif /* AFS_PTHREAD_ENV */
    return 0;
}

int
acl_IsAMember(afs_int32 aid, prlist *cps)
{
    afs_int32 i;

    for (i = 0; i < cps->prlist_len; i++)
	if (cps->prlist_val[i] == aid)
	    return 1;
    return 0;
}


static int
AddToList(pflist, elem)
     struct freeListEntry **pflist;
     struct freeListEntry *elem;
{
    /* Adds elem to the freelist flist;  returns 0 */
#ifdef AFS_PTHREAD_ENV
    assert(pthread_mutex_lock(&acl_list_mutex) == 0);
#endif /* AFS_PTHREAD_ENV */
    elem->next = *pflist;
    *pflist = elem;
#ifdef AFS_PTHREAD_ENV
    assert(pthread_mutex_unlock(&acl_list_mutex) == 0);
#endif /* AFS_PTHREAD_ENV */
    return 0;
}

static int
GetFromList(pflist, elem, minsize)
     struct freeListEntry **pflist;
     struct freeListEntry **elem;
     afs_int32 minsize;
{
    /* Looks for an element whose body is at least minsize bytes in the freelist flist.  If found, unlinks it, puts its address in elem, and returns 0, else returns -1.  A trivial first-fit algorithm is used. */

    struct freeListEntry *y, *z;

#ifdef AFS_PTHREAD_ENV
    assert(pthread_mutex_lock(&acl_list_mutex) == 0);
#endif /* AFS_PTHREAD_ENV */
    if (*pflist == NULL) {
#ifdef AFS_PTHREAD_ENV
	assert(pthread_mutex_unlock(&acl_list_mutex) == 0);
#endif /* AFS_PTHREAD_ENV */
	return -1;
    }
    for (y = *pflist, z = NULL; y != NULL; z = y, y = y->next) {
	if (y->size >= minsize) {
	    *elem = y;
	    if (z == NULL) {	/* pulling off the head */
		*pflist = y->next;
#ifdef AFS_PTHREAD_ENV
		assert(pthread_mutex_unlock(&acl_list_mutex) == 0);
#endif /* AFS_PTHREAD_ENV */
		return 0;
	    }
	    z->next = y->next;
#ifdef AFS_PTHREAD_ENV
	    assert(pthread_mutex_unlock(&acl_list_mutex) == 0);
#endif /* AFS_PTHREAD_ENV */
	    return 0;
	}
    }
#ifdef AFS_PTHREAD_ENV
    assert(pthread_mutex_unlock(&acl_list_mutex) == 0);
#endif /* AFS_PTHREAD_ENV */
    return -1;
}
