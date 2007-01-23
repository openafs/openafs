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

#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <WINNT/afsevent.h>
#endif
#include <ctype.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <rx/rx.h>
#include <rx/xdr.h>
#include <afs/cellconfig.h>
#include <afs/afsutil.h>
#include "ptclient.h"
#include "pterror.h"

int verbose = 0;
void skip();

void
report_error(afs_int32 code, char *name, char *gname)
{
    if (code == 0) {
	if (verbose)
	    printf("  added %s to %s.\n", name, gname);
    } else if (code == PRIDEXIST) {
	if (verbose)
	    printf("  user %s already on group %s\n", name, gname);
    } else {
	fprintf(stderr, "Couldn't add %s to %s!\n", name, gname);
	fprintf(stderr, "%s (%d).\n", pr_ErrorMsg(code), code);
    }
}

#include "AFS_component_version_number.c"

int
main(int argc, char **argv)
{
    register afs_int32 code;
    char name[PR_MAXNAMELEN];
    char gname[PR_MAXNAMELEN];
    char owner[PR_MAXNAMELEN];
    afs_int32 id;
    char buf[3000];
    FILE *fp;
    char *ptr;
    char *tmp;
    char *cellname;
    namelist lnames;
    afs_int32 i;
    afs_int32 fail = 0;

    if (argc < 2) {
	fprintf(stderr, "Usage: readgroup [-v] [-c cellname] groupfile.\n");
	exit(0);
    }
    cellname = 0;
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-v"))
	    verbose = 1;
	else {
	    if (!strcmp(argv[i], "-c")) {
		cellname = (char *)malloc(100);
		strncpy(cellname, argv[++i], 100);
	    } else
		strncpy(buf, argv[i], 150);
	}
    }
    code = pr_Initialize(2, AFSDIR_SERVER_ETC_DIRPATH, cellname);
    free(cellname);
    if (code) {
	fprintf(stderr, "pr_Initialize failed .. exiting.\n");
	fprintf(stderr, "%s (%d).\n", pr_ErrorMsg(code), code);
	exit(1);
    }

    if ((fp = fopen(buf, "r")) == NULL) {
	fprintf(stderr, "Couldn't open %s.\n", argv[1]);
	exit(1);
    }

    while ((tmp = fgets(buf, 3000, fp)) != NULL) {
	/* group file lines must either have the name of a group or a tab or blank space at beginning */
	if (buf[0] == '\n')
	    break;
	if (buf[0] != ' ' && buf[0] != '\t') {
	    /* grab the group name */
	    memset(gname, 0, PR_MAXNAMELEN);
	    memset(owner, 0, PR_MAXNAMELEN);
	    sscanf(buf, "%s %d", gname, &id);
	    tmp = buf;
	    skip(&tmp);
	    skip(&tmp);
	    stolower(gname);
	    ptr = strchr(gname, ':');
	    strncpy(owner, gname, ptr - gname);
	    if (strcmp(owner, "system") == 0)
		strncpy(owner, "system:administrators", PR_MAXNAMELEN);
	    fail = 0;
	    if (verbose)
		printf("Group is %s, owner is %s, id is %d.\n", gname, owner,
		       id);
	    code = pr_CreateGroup(gname, owner, &id);
	    if (code != 0) {
		if (code != PRIDEXIST) {	/* already exists */
		    fprintf(stderr, "Failed to create group %s with id %d!\n",
			    gname, id);
		    fprintf(stderr, "%s (%d).\n", pr_ErrorMsg(code), code);
		}
		if (code != PREXIST && code != PRIDEXIST) {	/* we won't add users if it's not there */
		    fail = 1;
		}
	    }
	    if (!fail) {
		/* read members out of buf and add to the group */
		memset(name, 0, PR_MAXNAMELEN);
		while (sscanf(tmp, "%s", name) != EOF) {
		    if (strchr(name, ':') == NULL) {
			/* then it's not a group */
			code = pr_AddToGroup(name, gname);
			report_error(code, name, gname);
		    } else {
			/* add the members of a group to the group */
			if (verbose)
			    printf("Adding %s to %s.\n",
				   lnames.namelist_val[i], gname);
			code = pr_ListMembers(name, &lnames);
			if (code) {
			    fprintf(stderr,
				    "Couldn't get the members for %s to add to %s.\n",
				    name, gname);
			    fprintf(stderr, "%s (%d).\n", pr_ErrorMsg(code),
				    code);
			}
			for (i = 0; i < lnames.namelist_len; i++) {
			    code =
				pr_AddToGroup(lnames.namelist_val[i], gname);
			    report_error(code, lnames.namelist_val[i], gname);
			}
			if (lnames.namelist_val)
			    free(lnames.namelist_val);
		    }
		    memset(name, 0, PR_MAXNAMELEN);
		    skip(&tmp);
		}
	    }
	} else {		/* must have more names to add */
	    /* if we couldn't create the group, and it wasn't already there, don't try to add more users */
	    if (fail)
		continue;
	    /* read members out of buf and add to the group */
	    memset(name, 0, PR_MAXNAMELEN);
	    tmp = buf;
	    tmp++;
	    while (sscanf(tmp, "%s", name) != EOF) {
		if (strchr(name, ':') == NULL) {
		    /* then it's not a group */
		    code = pr_AddToGroup(name, gname);
		    report_error(code, name, gname);
		} else {
		    /* add the members of a group to the group */
		    code = pr_ListMembers(name, &lnames);
		    if (code) {
			fprintf(stderr,
				"Couldn't get the members for %s to add to %s.\n",
				name, gname);
			fprintf(stderr, "%s (%d).\n", pr_ErrorMsg(code),
				code);
		    }
		    for (i = 0; i < lnames.namelist_len; i++) {
			if (verbose)
			    printf("Adding %s to %s.\n",
				   lnames.namelist_val[i], gname);
			code = pr_AddToGroup(lnames.namelist_val[i], gname);
			report_error(code, lnames.namelist_val[i], gname);
		    }
		    if (lnames.namelist_val)
			free(lnames.namelist_val);
		}
		memset(name, 0, PR_MAXNAMELEN);
		skip(&tmp);
	    }
	}
    }
}

void
skip(char **s)
{
    while (**s != ' ' && **s != '\t' && **s != '\0')
	(*s)++;
    while (**s == ' ' || **s == '\t')
	(*s)++;
}
