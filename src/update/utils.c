/* Copyright (C) 1990 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include <afs/param.h>
#include <afs/stds.h>
#include <rx/rxkad.h>
#include "global.h"
#ifdef AFS_NT40_ENV
#include <afs/errmap_nt.h>
#include <afs/afsutil.h>
#include <WINNT/afssw.h>
#endif
#include <stdio.h>


AddToList(ah, aname)
struct filestr **ah;
char *aname; {
    register struct filestr *tf;
    tf = (struct filestr *) malloc(sizeof(struct filestr));
    tf->next = *ah;
    *ah = tf;
    tf->name = (char *) malloc(strlen(aname)+1);
    strcpy(tf->name, aname);
    return 0;
}

ZapList(ah)
struct filestr **ah; {
    register struct filestr *tf, *nf;
    for(tf = *ah; tf; tf=nf) {
	nf = tf->next;	    /* save before freeing */
	free(tf->name);
	free(tf);
    }
    *ah = (struct filestr *) 0;
    return 0;
}

/* StringToLevel - converts the name of an rxkad security level to a integer
 * suitable for calling rxkad_New*SecurityObject. */

rxkad_level StringToLevel (name)
  char *name;
{
    if (strcmp (name, "clear") == 0) return rxkad_clear;
    if (strcmp (name, "auth") == 0) return rxkad_auth;
    if (strcmp (name, "crypt") == 0) return rxkad_crypt;
    return -1;
}
