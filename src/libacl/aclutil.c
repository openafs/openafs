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

#include "acl.h"
#include "prs_fs.h"

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

static int
ParseRights(int dfs, const char *rights_str, afs_uint32 *a_mask,
	    enum aclu_rights_type *rtypep, char *bad_char)
{
    int code;
    afs_int32 mode;
    char tc;
    char *tcp;                  /* to walk through the rights string  */
    char *arights = NULL;

    if (bad_char != NULL) {
	*bad_char = '\0';
    }

    arights = strdup(rights_str);
    if (arights == NULL) {
	code = ENOMEM;
	goto error;
    }

    /* set rights by default */
    *rtypep = ACLU_RTYPE_SET;

                                /* analyze last character of string   */
    tcp = arights + strlen(arights);
    if ( tcp-- > arights ) {    /* assure non-empty string            */
        if ( *tcp == '+' )
	    *rtypep = ACLU_RTYPE_RELADD;   /* '+' indicates more rights          */
        else if ( *tcp == '-' )
	    *rtypep = ACLU_RTYPE_RELDEL;   /* '-' indicates less rights          */
        else if ( *tcp == '=' )
	    *rtypep = ACLU_RTYPE_SET;      /* '=' also allows old behaviour      */
        else
            tcp++;              /* back to original null byte         */
        *tcp = '\0';            /* do not disturb old strcmp-s        */
    }

    if (dfs) {
	if (!strcmp(arights, "null")) {
	    *rtypep = ACLU_RTYPE_DENY;
	    mode = 0;
	    goto success;
	}
	if (!strcmp(arights, "read")) {
	    mode = DFS_READ | DFS_EXECUTE;
	    goto success;
	}
	if (!strcmp(arights, "write")) {
	    mode = DFS_READ | DFS_EXECUTE | DFS_INSERT | DFS_DELETE |
		DFS_WRITE;
	    goto success;
	}
	if (!strcmp(arights, "all")) {
	    mode = DFS_READ | DFS_EXECUTE | DFS_INSERT | DFS_DELETE |
		DFS_WRITE | DFS_CONTROL;
	    goto success;
	}
    } else {
	if (!strcmp(arights, "read")) {
	    mode = PRSFS_READ | PRSFS_LOOKUP;
	    goto success;
	}
	if (!strcmp(arights, "write")) {
	    mode = PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE |
		PRSFS_WRITE | PRSFS_LOCK;
	    goto success;
	}
	if (!strcmp(arights, "mail")) {
	    mode = PRSFS_INSERT | PRSFS_LOCK | PRSFS_LOOKUP;
	    goto success;
	}
	if (!strcmp(arights, "all")) {
	    mode = PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE |
		PRSFS_WRITE | PRSFS_LOCK | PRSFS_ADMINISTER;
	    goto success;
	}
    }
    if (!strcmp(arights, "none")) {
	*rtypep = ACLU_RTYPE_DESTROY;	/* Remove entire entry */
	mode = 0;
	goto success;
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
		if (bad_char != NULL) {
		    *bad_char = tc;
		}
		code = EINVAL;
		goto error;
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
		if (bad_char != NULL) {
		    *bad_char = tc;
		}
		code = EINVAL;
		goto error;
	    }
	}
    }

 success:
    *a_mask = mode;
    code = 0;

 error:
    free(arights);
    return code;
}

/**
 * Parse an ACL rights string into a bitmask.
 *
 * Translate a user-provided string of access rights (as would be given to the
 * 'fs setacl -acl' parameter) into the internal bitmask representation (a
 * combination of PRSFS_* bits). It handles abbreviations (e.g. rlidwka) and
 * shorthands (e.g. read). It also inspects the string for trailing modifiers
 * (+, -, =) to determine whether the rights should be added to, removed from,
 * or explicitly overwrite the existing ACL entry.
 *
 * For example, the string 'rl' would result in a mask of 'PRSFS_READ |
 * PRSFS_LOOKUP' and an rtypep of ACLU_RTYPE_SET.
 *
 * @param[in]   arights  null-terminated string representing the rights
 * @param[out]  a_mask   calculated rights bitmask (PRSFS_* bits)
 * @param[out]  rtypep   resolved ACL action (ACLU_RTYPE_*)
 * @param[out]  bad_char set to the first illegal character encountered (if any)
 *
 * @return status codes
 *   @retval 0       success
 *   @retval EINVAL  illegal character
 *   @retval ENOMEM  unable to allocate memory
 */
int
aclu_ParseRights(const char *arights, afs_uint32 *a_mask,
		 enum aclu_rights_type *rtypep, char *bad_char)
{
    return ParseRights(0, arights, a_mask, rtypep, bad_char);
}

/**
 * Parse an DFS ACL rights string into a bitmask.
 *
 * Has the same function as aclu_ParseRights(), but for DFS ACLs. See the
 * comment above aclu_ParseRights() for details on the supported functionality.
 *
 * @param[in]   arights  null-terminated string representing the rights
 * @param[out]  a_mask   calculated rights bitmask (DFS_* bits)
 * @param[out]  rtypep   resolved ACL action (ACLU_RTYPE_*)
 * @param[out]  bad_char set to the first illegal character encountered (if any)
 *
 * @return status codes
 *   @retval 0       success
 *   @retval EINVAL  illegal character
 *   @retval ENOMEM  unable to allocate memory
 */
int
aclu_ParseRightsDFS(const char *arights, afs_uint32 *a_mask,
		    enum aclu_rights_type *rtypep, char *bad_char)
{
    return ParseRights(1, arights, a_mask, rtypep, bad_char);
}

/**
 * Convert an ACL bitmask into a human-readable string.
 *
 * Translate an internal bitmask of access rights into a string representation.
 * For example, if the given arights is equal to PRSFS_LOOKUP | PRSFS_READ,
 * then this will be formatted into the string "rl".
 *
 * @param[in]  arights bitmask of PRSFS_* access rights (PRSFS_READ,
 *		       PRSFS_WRITE, etc.) to stringify
 * @param[out] strbuf  caller-provided output buffer
 *
 * @return a char* pointer to the given strbuf
 */
const char *
aclu_StringifyRights(afs_uint32 arights, struct aclu_rightsbuf *strbuf)
{
    char *cur = strbuf->sbuf;

    memset(strbuf, 0, sizeof(*strbuf));

    if (arights & PRSFS_READ)
	*cur++ = 'r';
    if (arights & PRSFS_LOOKUP)
	*cur++ = 'l';
    if (arights & PRSFS_INSERT)
	*cur++ = 'i';
    if (arights & PRSFS_DELETE)
	*cur++ = 'd';
    if (arights & PRSFS_WRITE)
	*cur++ = 'w';
    if (arights & PRSFS_LOCK)
	*cur++ = 'k';
    if (arights & PRSFS_ADMINISTER)
	*cur++ = 'a';
    if (arights & PRSFS_USR0)
	*cur++ = 'A';
    if (arights & PRSFS_USR1)
	*cur++ = 'B';
    if (arights & PRSFS_USR2)
	*cur++ = 'C';
    if (arights & PRSFS_USR3)
	*cur++ = 'D';
    if (arights & PRSFS_USR4)
	*cur++ = 'E';
    if (arights & PRSFS_USR5)
	*cur++ = 'F';
    if (arights & PRSFS_USR6)
	*cur++ = 'G';
    if (arights & PRSFS_USR7)
	*cur++ = 'H';

    return strbuf->sbuf;
}

/**
 * Same as aclu_StringifyRights(), but for DFS ACLs.
 */
const char *
aclu_StringifyRightsDFS(afs_uint32 arights, struct aclu_rightsbuf *strbuf)
{
    char *cur = strbuf->sbuf;

    memset(strbuf, 0, sizeof(*strbuf));

    if (arights & DFS_READ)
	*cur++ = 'r';
    else
	*cur++ = '-';
    if (arights & DFS_WRITE)
	*cur++ = 'w';
    else
	*cur++ = '-';
    if (arights & DFS_EXECUTE)
	*cur++ = 'x';
    else
	*cur++ = '-';
    if (arights & DFS_CONTROL)
	*cur++ = 'c';
    else
	*cur++ = '-';
    if (arights & DFS_INSERT)
	*cur++ = 'i';
    else
	*cur++ = '-';
    if (arights & DFS_DELETE)
	*cur++ = 'd';
    else
	*cur++ = '-';
    if (arights & (DFS_USRALL))
	*cur++ = '+';
    if (arights & DFS_USR0)
	*cur++ = 'A';
    if (arights & DFS_USR1)
	*cur++ = 'B';
    if (arights & DFS_USR2)
	*cur++ = 'C';
    if (arights & DFS_USR3)
	*cur++ = 'D';
    if (arights & DFS_USR4)
	*cur++ = 'E';
    if (arights & DFS_USR5)
	*cur++ = 'F';
    if (arights & DFS_USR6)
	*cur++ = 'G';
    if (arights & DFS_USR7)
	*cur++ = 'H';

    return strbuf->sbuf;
}

static void
FreeEntryList(struct aclu_AclEntry *alist)
{
    struct aclu_AclEntry *tp, *np;
    for (tp = alist; tp; tp = np) {
	np = tp->next;
	free(tp);
    }
}

/**
 * Free an aclu_Acl and all of its entries.
 *
 * If *a_acl is NULL, the function does nothing. On return, the caller's pointer
 * (*a_acl) is set to NULL.
 *
 * @param[inout] a_acl the ACL to free, set to NULL on return
 */
void
aclu_FreeAcl(struct aclu_Acl **a_acl)
{
    struct aclu_Acl *acl = *a_acl;
    if (acl == NULL) {
	return;
    }
    *a_acl = NULL;

    FreeEntryList(acl->pluslist);
    FreeEntryList(acl->minuslist);
    free(acl);
}

static const char *
SkipLine(const char *astr)
{
    while (*astr != '\0' && *astr != '\n')
	astr++;
    if (*astr == '\n')
	astr++;
    return astr;
}

/**
 * Create an empty aclu_Acl struct, using an ACL string to obtain DFS info.
 *
 * The only part of the input string that is parsed is the first line, since
 * that is the part containing DFS information, and so that bogus ACLs can be
 * recovered from. See aclu_ParseAcl() for information on expected ACL string
 * format.
 *
 * The caller is responsible for freeing the newly created Acl struct by
 * invoking aclu_FreeAcl().
 *
 * @param[in]  astr  ACL string to mimic the DFS status and cell from
 * @param[out] a_acl address of the resulting ACL
 *
 * @return status codes
 *   @retval 0      success
 *   @retval ENOMEM allocation failed, insufficient memory
 */
int
aclu_ParseEmptyAcl(const char *astr, struct aclu_Acl **a_acl)
{
    struct aclu_Acl *tp;
    int junk;

    tp = calloc(sizeof(*tp), 1);
    if (tp == NULL) {
	return ENOMEM;
    }

    tp->nplus = tp->nminus = 0;
    tp->pluslist = tp->minuslist = 0;
    tp->dfs = 0;
    sscanf(astr, "%d dfs:%d %1024s", &junk, &tp->dfs, tp->cell);

    *a_acl = tp;
    return 0;
}

/**
 * Create a new struct aclu_Acl from an ACL string.
 *
 * The expected format of the input string is the standard AFS ACL string
 * format, as used in the RPC RXAFS_FetchACL(). The first two lines are
 * formatted as follows:
 *
 * <nplus> [dfs:<type> <cell>]
 * <nminus>
 *
 * <nplus>, <type>, and <nminus> are decimal-integers, while <cell> is a string.
 * The dfs:<type> <cell> portion of the first line is only present for DFS ACLs,
 * AFS ACLs omit this part.
 *
 * The next nplus lines represent the positive entries, formatted as
 * <name> <rights>, where <rights> is the rights bitmask, formatted as a decimal
 * integer. Then, the next nminus lines represent the negative entries, in the
 * same format.
 *
 * The caller is responsible for freeing the newly created aclu_Acl struct by
 * invoking aclu_FreeAcl().
 *
 * @param[in]  astr  ACL string to construct the ACL from
 * @param[out] a_acl address of the resulting ACL
 *
 * @return status codes
 *   @retval 0      success
 *   @retval ENOMEM allocation failed, insufficient memory
 */
int
aclu_ParseAcl(const char *astr, struct aclu_Acl **a_acl)
{
    int nplus = 0, nminus = 0, i, trights = 0;
    int code;
    char tname[ACLU_MAXNAME + 1] = "";
    struct aclu_AclEntry *first, *last, *tl;
    struct aclu_Acl *ta;

    *a_acl = NULL;

    ta = calloc(sizeof(*ta), 1);
    if (ta == NULL) {
	code = ENOMEM;
	goto done;
    }

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
	if (tl == NULL) {
	    code = ENOMEM;
	    goto done;
	}
	if (!first) {
	    first = tl;
	    ta->pluslist = first;
	}
	strcpy(tl->name, tname);
	tl->rights = trights;
	tl->next = 0;
	if (last)
	    last->next = tl;
	last = tl;
    }

    last = 0;
    first = 0;
    for (i = 0; i < nminus; i++) {
	sscanf(astr, "%99s %d", tname, &trights);
	astr = SkipLine(astr);
	tl = calloc(sizeof(*tl), 1);
	if (tl == NULL) {
	    code = ENOMEM;
	    goto done;
	}
	if (!first) {
	    first = tl;
	    ta->minuslist = first;
	}
	strcpy(tl->name, tname);
	tl->rights = trights;
	tl->next = 0;
	if (last)
	    last->next = tl;
	last = tl;
    }

    *a_acl = ta;
    code = 0;

 done:
    if (code != 0) {
	aclu_FreeAcl(&ta);
    }
    return code;
}
