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
    ("$Header: /cvs/openafs/src/update/utils.c,v 1.9 2003/07/15 23:17:07 shadow Exp $");

#include <afs/stds.h>
#include <rx/rxkad.h>
#include "global.h"
#ifdef AFS_NT40_ENV
#include <afs/errmap_nt.h>
#include <afs/afsutil.h>
#include <WINNT/afssw.h>
#endif
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>

int
AddToList(ah, aname)
     struct filestr **ah;
     char *aname;
{
    register struct filestr *tf;
    tf = (struct filestr *)malloc(sizeof(struct filestr));
    tf->next = *ah;
    *ah = tf;
    tf->name = (char *)malloc(strlen(aname) + 1);
    strcpy(tf->name, aname);
    return 0;
}

int
ZapList(ah)
     struct filestr **ah;
{
    register struct filestr *tf, *nf;
    for (tf = *ah; tf; tf = nf) {
	nf = tf->next;		/* save before freeing */
	free(tf->name);
	free(tf);
    }
    *ah = NULL;
    return 0;
}

/* StringToLevel - converts the name of an rxkad security level to a integer
 * suitable for calling rxkad_New*SecurityObject. */

rxkad_level
StringToLevel(name)
     char *name;
{
    if (strcmp(name, "clear") == 0)
	return rxkad_clear;
    if (strcmp(name, "auth") == 0)
	return rxkad_auth;
    if (strcmp(name, "crypt") == 0)
	return rxkad_crypt;
    return -1;
}
