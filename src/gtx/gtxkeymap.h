#ifndef __GATOR_KEYMAP_H_
#define __GATOR_KEYMAP_H_

/* number of entries max per keymap */
#define KEYMAP_NENTRIES		256

/* types of keymaps */
#define KEYMAP_EMPTY		0		/* nothing in this slot */
#define KEYMAP_PROC		1		/* procedure in this slot */
#define KEYMAP_SUBMAP		2		/* submap in this slot */

/* one per entry */
struct keymap_entry {
    char type;				/* type, e.g. submap, etc */
    char pad[3];			/* padding */
    char *name;				/* descriptive name of function, if function */
    union {				/* value (proc, submap, etc) */
	int (*proc)();
	struct keymap_map *submap;
	char *generic;
    } u;
    char *rock;				/* rock to use */
};

struct keymap_map {
    short refcount;	/* reference count */
    char pad[2];	/* padding to afs_int32 boundary */
    struct keymap_entry entries[KEYMAP_NENTRIES];
};

struct keymap_state {
    struct keymap_map *initMap;
    struct keymap_map *currentMap;
};

extern struct keymap_map *keymap_Create();
extern int keymap_BindToString();
extern int keymap_Delete();
extern int keymap_InitState();
extern int keymap_ProcessState();
extern int keymap_ResetState();

#endif	/* define for file */
