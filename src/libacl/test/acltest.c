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


#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <stdio.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <ptclient.h>
#include "acl.h"
#include "prs_fs.h"


struct acl_accessList *aclstore[20];
char *externalstore[20];

int
PRights(arights)
     long arights;
{
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
}

long
Convert(arights)
     char *arights;
{
    int i, len;
    long mode;
    char tc;
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
    if (!strcmp(arights, "none"))
	return 0;
    len = strlen(arights);
    mode = 0;
    for (i = 0; i < len; i++) {
	tc = *arights++;
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
	    printf("Bogus rights character '%c'.\n", tc);
	    exit(1);
	}
    }
    return mode;
}



main()
{
    long code;

    char op[3];
    char name[64];
    char rights[10];
    long which;
    long n, p;
    long realrights;
    long i, j;
    char *ptr;
    char *tptr;
    long size;
    idlist ids;
    namelist names;
    prlist cps;

    struct acl_accessList *alist;
    char foo[200];

    code = pr_Initialize(0, "/usr/afs/etc", 0);
    if (code) {
	fprintf(stderr, "Couldn't initialize wrt to protection server.\n");
	exit(1);
    }
    for (i = 0; i < 20; i++) {
	externalstore[i] = NULL;
	aclstore[i] = NULL;
    }

    printf("acl> ");
    while (1) {
	scanf("%s", op);
	if (!strcmp(op, "q"))
	    exit(2);
	else if (!strcmp(op, "ex")) {
	    scanf("%d", &which);
	    if (aclstore[which] == NULL) {
		printf("No internal acl in %d.\n", which);
		printf("acl> ");
		continue;
	    }
	    if (externalstore[which] != NULL) {
		code = acl_FreeExternalACL(&externalstore[which]);
		if (code) {
		    printf("Couldn't free current ACL.\n");
		    printf("acl> ");
		    continue;
		}
	    }
	    code = acl_Externalize(aclstore[which], &externalstore[which]);
	    if (code)
		printf("Couldn't externalize -- code is %d.\n", code);
	} else if (!strcmp(op, "in")) {
	    scanf("%d", &which);
	    if (externalstore[which] == NULL) {
		printf("No external acl in %d.\n", which);
		printf("acl> ");
		continue;
	    }
	    if (aclstore[which] != NULL) {
		code = acl_FreeACL(&aclstore[which]);
		if (code) {
		    printf("Couldn't free current ACL.\n");
		    printf("acl> ");
		    continue;
		}
	    }
	    code = acl_Internalize(externalstore[which], &aclstore[which]);
	    if (code)
		printf("Couldn't internalize. Code is %d\n", code);
	} else if (!strcmp(op, "sa")) {
	    scanf("%d %s %s", &which, name, rights);
	    realrights = (long)Convert(rights);
	    if (externalstore[which] != NULL) {
		/* we're adding to access list */
		size = strlen(externalstore[which]);
		ptr = (char *)malloc(size);
		sscanf(externalstore[which], "%d\n%d\n", &p, &n);
		strncpy(ptr, externalstore[which], size);
		p++;
		free(externalstore[which]);
		code = acl_NewExternalACL((p + n), &externalstore[which]);
		if (code) {
		    printf("Couldn't allocate external list.\n");
		    exit(2);
		}
		sprintf(externalstore[which], "%d", p);
		tptr = externalstore[which] + 1;
		ptr++;
		sprintf(tptr, "%s", ptr);
		ptr = externalstore[which] + size;
		sprintf(ptr, "%s\t%d\n", name, realrights);
	    } else {
		/* new external list */
		code = acl_NewExternalACL(1, &externalstore[which]);
		if (code) {
		    printf("Couldn't allocate external list.\n");
		    exit(2);
		}
		p = 1;
		n = 0;
		sprintf(externalstore[which], "%d\n%d\n%s\t%d\n", p, n, name,
			realrights);
	    }
	} else if (!strcmp(op, "la")) {
	    scanf("%d", &which);
	    if (externalstore[which] == NULL) {
		printf("No acl in %d.\n", which);
		printf("acl> ");
		continue;
	    }
	    ptr = externalstore[which];
	    sscanf(ptr, "%d\n%d\n", &p, &n);
	    skip(&ptr);
	    skip(&ptr);
	    for (i = 0; i < p; i++) {
		sscanf(ptr, "%s\t%d\n", name, &realrights);
		printf("%s\t", name);
		PRights(realrights);
		printf("\n");
		skip(&ptr);
	    }
	    if (n > 0) {
		printf("Negative rights: \n");
		for (i = 0; i < n; i++) {
		    scanf(ptr, "%s\t%d\n", name, &realrights);
		    printf("%s\t", name);
		    PRights(realrights);
		    printf("\n");
		}
	    }
	} else if (!strcmp(op, "cr")) {
	    scanf("%s %d", name, &which);
	    if (aclstore[which] == NULL) {
		printf("No acl in %d.\n", which);
		printf("acl> ");
		continue;
	    }
	    names.namelist_len = 1;
	    names.namelist_val = (prname *) malloc(strlen(name) + 1);
	    strncpy(names.namelist_val, name, PR_MAXNAMELEN);
	    code = pr_NameToId(&names, &ids);
	    if (code) {
		printf("Couldn't translate %s\n", name);
		printf("acl> ");
		continue;
	    }
	    code = pr_GetCPS(*ids.idlist_val, &cps);
	    if (code) {
		printf("Couldn't get cps\n");
		printf("acl> ");
		continue;
	    }
	    code = acl_CheckRights(aclstore[which], &cps, &realrights);
	    if (code) {
		printf("Couldn't check rights\n");
		printf("acl> ");
		continue;
	    }
	    printf("Rights for %s on %d are:\n", name, which);
	    PRights(realrights);
	    printf("\n");
	} else
	    printf("Unknown op!\n");
	printf("acl> ");
    }
}

skip(s)
     char **s;
{
    while (**s != '\n' && **s != '\0')
	(*s)++;
    if (**s == '\n')
	(*s)++;
}
