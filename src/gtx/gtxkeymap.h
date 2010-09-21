/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __GATOR_KEYMAP_H_
#define __GATOR_KEYMAP_H_

/* number of entries max per keymap */
#define KEYMAP_NENTRIES		256

/* types of keymaps */
#define KEYMAP_EMPTY		0	/* nothing in this slot */
#define KEYMAP_PROC		1	/* procedure in this slot */
#define KEYMAP_SUBMAP		2	/* submap in this slot */

/* one per entry */
struct keymap_entry {
    char type;			/* type, e.g. submap, etc */
    char pad[3];		/* padding */
    char *name;			/* descriptive name of function, if function */
    union {			/* value (proc, submap, etc) */
	int (*proc) (void *, void *);
	struct keymap_map *submap;
	void *generic;
    } u;
    void *rock;			/* rock to use */
};

struct keymap_map {
    short refcount;		/* reference count */
    char pad[2];		/* padding to afs_int32 boundary */
    struct keymap_entry entries[KEYMAP_NENTRIES];
};

struct keymap_state {
    struct keymap_map *initMap;
    struct keymap_map *currentMap;
};

extern struct keymap_map *keymap_Create(void);
extern int keymap_BindToString(struct keymap_map *, char *,
			       int (*aproc)(void *, void *), char *, void *);
extern int keymap_Delete(struct keymap_map *);
extern int keymap_InitState(struct keymap_state *, struct keymap_map *);
extern int keymap_ProcessKey(struct keymap_state *, int, void *);
extern int keymap_ResetState(struct keymap_state *);

extern char *gtx_CopyString(char *);


#endif /* define for file */
