/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *	Implementation of the ACL and quota-related operations used
 *	by the AFS user account facility.
 */

/*
 * --------------------- Required definitions ---------------------
 */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "uss_acl.h"
#include "uss_common.h"
#include "uss_fs.h"
#include <rx/xdr.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#undef VIRTUE
#undef VICE
#include <afs/afsint.h>
#include <afs/prs_fs.h>
#include <afs/com_err.h>

#define MAXNAME 100
#define	MAXSIZE	2048

#undef USS_ACL_DB

struct Acl {
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

static int PruneList();


/*------------------------------------------------------------------------
 * static foldcmp
 *
 * Description:
 *	String comparison with case-folding.
 *
 * Arguments:
 *	a_str1 : First string.
 *	a_str2 : Second string.
 *
 * Returns:
 *	0 if the two strings match,
 *	1 otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static
foldcmp(a_str1, a_str2)
     register char *a_str1;
     register char *a_str2;

{				/*foldcmp */

    register char t, u;

    while (1) {
	t = *a_str1++;
	u = *a_str2++;
	if (t >= 'A' && t <= 'Z')
	    t += 0x20;
	if (u >= 'A' && u <= 'Z')
	    u += 0x20;
	if (t != u)
	    return (1);		/*Difference */
	if (t == 0)
	    return (0);		/*Match */
    }
}				/*foldcmp */


/*------------------------------------------------------------------------
 * static Convert
 *
 * Description:
 *	Convert rights as expressed in a character string to an integer
 *	with the appropriate bits set.
 *
 * Arguments:
 *	a_rights : String version of access rights.
 *
 * Returns:
 *	Translated rights.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	Exits the program if a badly-formatted rights string is
 *	passed in.
 *------------------------------------------------------------------------*/

static afs_int32
Convert(a_rights)
     register char *a_rights;

{				/*Convert */

    register int i, len;
    afs_int32 mode;
    register char tc;

    if (!strcmp(a_rights, "read"))
	return (PRSFS_READ | PRSFS_LOOKUP);

    if (!strcmp(a_rights, "write"))
	return (PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE |
		PRSFS_WRITE | PRSFS_LOCK);

    if (!strcmp(a_rights, "mail"))
	return (PRSFS_INSERT | PRSFS_LOCK | PRSFS_LOOKUP);

    if (!strcmp(a_rights, "all"))
	return (PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE |
		PRSFS_WRITE | PRSFS_LOCK | PRSFS_ADMINISTER);

    if (!strcmp(a_rights, "none"))
	return (0);

    len = strlen(a_rights);
    mode = 0;
    for (i = 0; i < len; i++) {
	tc = *a_rights++;
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
	else {
	    printf("%s: Bogus rights character '%c'.\n", uss_whoami, tc);
	    exit(1);
	}
    }
    return (mode);

}				/*Convert */


/*------------------------------------------------------------------------
 * static FindList
 *
 * Description:
 *	Find the entry with the given name in the passed-in ACL.
 *
 * Arguments:
 *	a_alist : Internalized ACL to look through.
 *	a_name  : Name to find.
 *
 * Returns:
 *	Ptr to the ACL entry found, or null pointer if none.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static struct AclEntry *
FindList(a_alist, a_name)
     register struct AclEntry *a_alist;
     char *a_name;

{				/*FindList */

    while (a_alist) {
	if (!foldcmp(a_alist->name, a_name))
	    return (a_alist);
	a_alist = a_alist->next;
    }
    return (0);

}				/*FindList */


/*------------------------------------------------------------------------
 * static ChangeList
 *
 * Description:
 *	Set the rights for the named entry to the given value, creating
 *	an entry if one does not yet exist.
 *
 * Arguments:
 *	a_al     : Ptr to the internalized ACL.
 *	a_plus   : Positive or negative rights?
 *	a_name   : Name to look for.
 *	a_rights : Rights to set.
 *
 * Returns:
 *	0 if the two strings match,
 *	1 otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static void
ChangeList(a_al, a_plus, a_name, a_rights)
     struct Acl *a_al;
     afs_int32 a_plus;
     char *a_name;
     afs_int32 a_rights;

{				/*ChangeList */

    struct AclEntry *tlist;

    tlist = (a_plus ? a_al->pluslist : a_al->minuslist);
    tlist = FindList(tlist, a_name);
    if (tlist) {
	/*
	 * Found the item already in the list.
	 */
	tlist->rights = a_rights;
	if (a_plus)
	    a_al->nplus -= PruneList(&a_al->pluslist);
	else
	    a_al->nminus -= PruneList(&a_al->minuslist);
	return;
    }

    /*
     * Otherwise, we make a new item and plug in the new data.
     */
    tlist = (struct AclEntry *)malloc(sizeof(struct AclEntry));
    strcpy(tlist->name, a_name);
    tlist->rights = a_rights;
    if (a_plus) {
	tlist->next = a_al->pluslist;
	a_al->pluslist = tlist;
	a_al->nplus++;
	if (a_rights == 0)
	    a_al->nplus -= PruneList(&a_al->pluslist);
    } else {
	tlist->next = a_al->minuslist;
	a_al->minuslist = tlist;
	a_al->nminus++;
	if (a_rights == 0)
	    a_al->nminus -= PruneList(&a_al->minuslist);
    }
}				/*ChangeList */


/*------------------------------------------------------------------------
 * static PruneList
 *
 * Description:
 *	Remove all entries whose rights fields are set to zero.
 *
 * Arguments:
 *	a_aclPP : Ptr to the location of the ACL to purne.
 *
 * Returns:
 *	Number of entries pruned off.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
PruneList(a_aclPP)
     struct AclEntry **a_aclPP;

{				/*PruneList */

    struct AclEntry **lPP;
    struct AclEntry *tP, *nP;
    afs_int32 ctr;

    ctr = 0;
    lPP = a_aclPP;
    for (tP = *a_aclPP; tP; tP = nP) {
	if (tP->rights == 0) {
	    *lPP = tP->next;
	    nP = tP->next;
	    free(tP);
	    ctr++;
	} else {
	    nP = tP->next;
	    lPP = &tP->next;
	}
    }

    return (ctr);

}				/*PruneList */


/*------------------------------------------------------------------------
 * static SkipLine
 *
 * Description:
 *	Skip chars until we eat a newline.
 *
 * Arguments:
 *	a_str : String to process.
 *
 * Returns:
 *	Ptr to the first char past the newline.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static char *
SkipLine(a_str)
     register char *a_str;

{				/*SkipLine */

    while (*a_str != '\n')
	a_str++;
    a_str++;
    return (a_str);

}				/*SkipLine */


/*------------------------------------------------------------------------
 * static EmptyAcl
 *
 * Description:
 *	Create an empty internalized ACL.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	Ptr to an initialized and empty internalized ACL.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static struct Acl *
EmptyAcl()
{				/*EmptyAcl */

    register struct Acl *tp;

    tp = (struct Acl *)malloc(sizeof(struct Acl));
    tp->nplus = tp->nminus = 0;
    tp->pluslist = tp->minuslist = 0;
    return (tp);

}				/*EmptyAcl */


/*------------------------------------------------------------------------
 * static ParseAcl
 *
 * Description:
 *	Given an externalized ACL (i.e., in string format), convert it
 *	into its internalized form.
 *
 * Arguments:
 *	a_str : Ptr to Externalized ACL.
 *
 * Returns:
 *	Ptr to the equivalent internalized ACL.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static struct Acl *
ParseAcl(a_str)
     char *a_str;

{				/*ParseAcl */

    int nplus, nminus, i, trights;
    char tname[MAXNAME];
    struct AclEntry *first, *last, *tl;
    struct Acl *ta;

    /*
     * Pull out the number of positive & negative entries in the externalized
     * ACL.
     */
    sscanf(a_str, "%d", &nplus);
    a_str = SkipLine(a_str);
    sscanf(a_str, "%d", &nminus);
    a_str = SkipLine(a_str);

    /*
     * Allocate and initialize the first entry.
     */
    ta = (struct Acl *)malloc(sizeof(struct Acl));
    ta->nplus = nplus;
    ta->nminus = nminus;

    /*
     * Translate the positive entries.
     */
    last = 0;
    first = 0;
    for (i = 0; i < nplus; i++) {
	sscanf(a_str, "%100s %d", tname, &trights);
	a_str = SkipLine(a_str);
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

    /*
     * Translate the negative entries.
     */
    last = 0;
    first = 0;
    for (i = 0; i < nminus; i++) {
	sscanf(a_str, "%100s %d", tname, &trights);
	a_str = SkipLine(a_str);
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

    return (ta);

}				/*ParseAcl */


/*------------------------------------------------------------------------
 * static AclToString
 *
 * Description:
 *	Given an internalized ACL, convert it to its externalized form,
 *	namely a character string.
 *
 * Arguments:
 *	a_acl : Internalized ACL to externalize.
 *
 * Returns:
 *	Ptr to the externalized version.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static char *
AclToString(a_acl)
     struct Acl *a_acl;

{				/*AclToString */

    static char mydata[MAXSIZE];
    char tstring[MAXSIZE];
    struct AclEntry *tp;

    /*
     * Print out the number of positive & negative entries on one line.
     */
    sprintf(mydata, "%d\n%d\n", a_acl->nplus, a_acl->nminus);

    /*
     * Externalize the positive list.
     */
    for (tp = a_acl->pluslist; tp; tp = tp->next) {
	sprintf(tstring, "%s %d\n", tp->name, tp->rights);
	strcat(mydata, tstring);
    }

    /*
     * Externalize the negative list.
     */
    for (tp = a_acl->minuslist; tp; tp = tp->next) {
	sprintf(tstring, "%s %d\n", tp->name, tp->rights);
	strcat(mydata, tstring);
    }
    return (mydata);

}				/*AclToString */


/*------------------------------------------------------------------------
 * static uss_acl_SetAccess
 *
 * Environment:
 *	ACL comes in as a single string, prefixed with the pathname to
 *	which the ACL is applied.  The a_clear argument, if set, causes
 *	us to act like sac instead of sa.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_acl_SetAccess(a_access, a_clear, a_negative)
     char *a_access;
     int a_clear;
     int a_negative;

{				/*uss_acl_SetAccess */

    register afs_int32 code;
    static char rn[] = "uss_acl_SetAccess";
    struct Acl *ta;
    char *externalizedACL;
    int plusp;
    afs_int32 rights;
    char tmp_str[MAXSIZE];
    char path_field[MAXSIZE], user_field[64], rights_field[64], *tp;
    int overflow;

    plusp = !a_negative;

    /*
     * Pull out the pathname from our argument.
     */
    tp = uss_common_FieldCp(path_field, a_access, ' ', sizeof(path_field),
			    &overflow);
    if (overflow) {
	fprintf(stderr, "%s: * Pathname field too long (max is %d chars)\n",
		uss_whoami, sizeof(path_field));
	return (-1);
    }

    /*
     * Ask the Cache Manager to give us the externalized ACL for the
     * given directory.
     */
    code = uss_fs_GetACL(path_field, tmp_str, MAXSIZE);
    if (code) {
	afs_com_err(uss_whoami, code, "while getting access list for %s",
		path_field);
#ifdef USS_ACL_DB
	printf("%s: Error code from uss_fs_GetACL %d, errno %d\n", rn, code,
	       errno);
#endif /* USS_ACL_DB */
	return (code);
    }
#ifdef USS_ACL_DB
    else
	printf("%s: ACL for pathname '%s' is: '%s'\n", rn, path_field,
	       tmp_str);
#endif /* USS_ACL_DB */

    /*
     * Generate the proper internalized ACL.
     */
    if (a_clear)
	ta = EmptyAcl();
    else
	ta = ParseAcl(tmp_str);

    /*
     * For each given entry, pull out the information and alter the
     * internalized ACL to reflect it.
     */
    while (*tp != '\0') {
	tp = uss_common_FieldCp(user_field, tp, ' ', 64, &overflow);
	if (overflow) {
	    fprintf(stderr, "%s: * User field too long (max is 64 chars)\n",
		    uss_whoami);
	    return (-1);
	}
	if (*tp == '\0') {
	    printf("%s: Missing second half of user/access pair.\n",
		   uss_whoami);
	    return (1);
	}
	tp = uss_common_FieldCp(rights_field, tp, ' ', 64, &overflow);
	if (overflow) {
	    fprintf(stderr, "%s: * Rights field too long (max is 64 chars)\n",
		    uss_whoami);
	    return (-1);
	}
	rights = Convert(rights_field);
	ChangeList(ta, plusp, user_field, rights);
    }

    /*
     * Externalize the fully-processed internal ACL, then pass it back
     * to the Cache Manager.
     */
    externalizedACL = AclToString(ta);
    code =
	uss_fs_SetACL(path_field, externalizedACL,
		      strlen(externalizedACL) + 1);
    if (code) {
#ifdef USS_ACL_DB
	printf("%s: uss_fs_SetACL() failed, code is %d, errno is %d\n", rn,
	       code, errno);
#endif /* USS_ACL_DB */
	if (errno == EINVAL) {
	    printf("Can't set ACL for directory '%s' to '%s'\n", path_field,
		   externalizedACL);
	    printf("Invalid argument, possible reasons include:\n");
	    printf("\t1. File not in AFS, or\n");
	    printf("\t2. Too many users on the ACL, or\n");
	    printf("\t3. Non-existent user or group on ACL.\n");
	    return (code);
	} else {
	    afs_com_err(uss_whoami, code, "while setting the access list");
	    return (code);
	}
    }
    return (0);

}				/*uss_acl_SetAccess */


/*------------------------------------------------------------------------
 * static us_acl_SetDiskQuota
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_acl_SetDiskQuota(a_path, a_q)
     char *a_path;
     int a_q;

{				/*uss_acl_SetDiskQuota */

    register afs_int32 code;
    static char rn[] = "uss_acl_SetDiskQuota";
    uss_VolumeStatus_t *status;
    char *name, *motd, *offmsg;
    char *input;
    char tmp_str[MAXSIZE];

    if (uss_verbose)
	fprintf(stderr,
		"Setting disk quota on volume mounted at '%s' to %d blocks\n",
		a_path, a_q);

    status = (uss_VolumeStatus_t *) tmp_str;
    status->MinQuota = status->MaxQuota = -1;
    name = motd = offmsg = NULL;
    status->MaxQuota = a_q;

    input = (char *)status + sizeof(*status);
    *(input++) = '\0';
    *(input++) = '\0';
    *(input++) = '\0';

    code = uss_fs_SetVolStat(a_path, tmp_str, sizeof(*status) + 3);
    if (code) {
	afs_com_err(uss_whoami, code, "while setting disk quota");
#ifdef USS_ACL_DB
	printf("%s: uss_fs_SetVolStat() error code: %d, errno is %d\n", rn,
	       code, errno);
#endif /* USS_ACL_DB */
	return (code);
    }

    return (0);

}				/*uss_acl_SetDiskQuota */


/*-----------------------------------------------------------------------
 * EXPORTED uss_acl_CleanUp
 *
 * Environment:
 *	The uss_currentDir variable contains directories touched by the
 *	given operation.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_acl_CleanUp()
{				/*uss_acl_CleanUp */

    static char rn[] = "uss_acl_CleanUp";
    struct uss_subdir *t, *old_t = NULL;
    char tmp_str[uss_MAX_SIZE];

    /*
     * Restore the ACL for each directory created for our caller to its
     * original setting.  The uss_AccountCreator's access was (potentially)
     * amplified to full rights - any excess ability must now be removed.
     */
    for (t = uss_currentDir; t != NULL; t = t->previous) {
	sprintf(tmp_str, "%s %s", t->path, t->finalACL);
	if (uss_verbose)
	    fprintf(stderr, "Setting ACL: '%s'\n", tmp_str);
	if (!uss_DryRun)
	    uss_acl_SetAccess(tmp_str, 1, 0);
	else
	    fprintf(stderr, "\t[Dry run: uss_acl_SetAccess(%s) on '%s']\n",
		    tmp_str, t->path);
	free(t->path);
	if (old_t)
	    free(old_t);
	old_t = t;
    }				/*Remove caller from user directory ACL */

    if (old_t)
	free(old_t);

    /*
     * Return successfully.
     */
    return (0);

}				/*uss_acl_CleanUp */
