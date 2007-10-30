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

#include <string.h>
#include <stdlib.h>

#include "gtxkeymap.h"

struct keymap_map *
keymap_Create()
{
    register struct keymap_map *tmap;

    tmap = (struct keymap_map *)malloc(sizeof(struct keymap_map));
    if (tmap != (struct keymap_map *)0)
	memset(tmap, 0, sizeof(*tmap));
    return (tmap);
}

/* make a copy of a string; generic utility */
char *
gtx_CopyString(aval)
     register char *aval;
{
    register char *tp;

    if (!aval)
	return NULL;		/* propagate null strings around */
    tp = (char *)malloc(strlen(aval) + 1);
    if (tp != NULL)
	strcpy(tp, aval);
    return (tp);
}

static int
BindIt(amap, aslot, atype, aproc, aname, arock)
     struct keymap_map *amap;
     char *arock;
     int aslot;
     int atype;
     char *aproc;
     char *aname;
{
    register char *tp;
    register struct keymap_entry *tentry;

    if (aslot < 0 || aslot >= KEYMAP_NENTRIES)
	return -1;
    tentry = &amap->entries[aslot];
    tentry->type = atype;
    if (tp = tentry->name)
	free(tp);
    if (atype == KEYMAP_EMPTY) {
	tentry->u.generic = NULL;
	tentry->name = NULL;
    } else {
	tentry->name = gtx_CopyString(aname);
	tentry->u.generic = aproc;
    }
    tentry->rock = arock;
    return 0;
}

keymap_BindToString(amap, astring, aproc, aname, arock)
     register struct keymap_map *amap;
     char *astring;
     char *arock;
     int (*aproc) ();
     char *aname;
{
    register char *cptr;
    register int tc;
    register afs_int32 code;
    struct keymap_map *tmap;

    cptr = astring;
    /* walk down string, building submaps if possible, until we get to function
     * at the end */
    while (tc = *cptr++) {
	/* see if we should do submap or final function */
	if (*cptr == 0) {	/* we're peeking: already skipped command char */
	    /* last character, do final function */
	    if (!aproc)		/* delete the entry */
		code = BindIt(amap, tc, KEYMAP_EMPTY, NULL, NULL, NULL);
	    else
		code =
		    BindIt(amap, tc, KEYMAP_PROC, (char *)aproc, aname,
			   arock);
	    if (code)
		return code;
	} else {
	    /* more characters after this; do submap */
	    if (amap->entries[tc].type != KEYMAP_SUBMAP) {
		tmap = keymap_Create();
		code =
		    BindIt(amap, tc, KEYMAP_SUBMAP, (char *)tmap, NULL, NULL);
	    } else {
		tmap = amap->entries[tc].u.submap;
		code = 0;
	    }
	    if (code)
		return code;
	    amap = tmap;	/* continue processing this map */
	}
    }				/* while loop */
    /* here when all characters are gone */
    return 0;
}

/* delete a keymap and all of its recursively-included maps */
keymap_Delete(amap)
     register struct keymap_map *amap;
{
    register int i;
    register struct keymap_entry *tentry;

    for (i = 0; i < KEYMAP_NENTRIES; i++) {
	tentry = &amap->entries[i];
	if (tentry->name)
	    free(tentry->name);
	if (tentry->type == KEYMAP_SUBMAP)
	    keymap_Delete(tentry->u.submap);
    }
    free(amap);
    return 0;
}

keymap_InitState(astate, amap)
     register struct keymap_state *astate;
     struct keymap_map *amap;
{
    memset(astate, 0, sizeof(*astate));
    astate->initMap = amap;
    astate->currentMap = amap;
    return 0;
}

keymap_ProcessKey(astate, akey, arock)
     register struct keymap_state *astate;
     char *arock;
     register int akey;
{
    register struct keymap_entry *tentry;
    register afs_int32 code;

    if (akey < 0 || akey >= KEYMAP_NENTRIES)
	return -1;
    tentry = &astate->currentMap->entries[akey];
    code = 0;
    switch (tentry->type) {
    case KEYMAP_EMPTY:
	keymap_ResetState(astate);
	return -1;
	/* break; */
	/* break commented out because of return above causing compiler warnings */

    case KEYMAP_SUBMAP:
	astate->currentMap = tentry->u.submap;
	break;

    case KEYMAP_PROC:
	code = (*tentry->u.proc) (arock, tentry->rock);
	keymap_ResetState(astate);
	break;
    }
    return code;
}

keymap_ResetState(astate)
     register struct keymap_state *astate;
{
    return keymap_InitState(astate, astate->initMap);
}
