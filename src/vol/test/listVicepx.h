#define         MAX_HASH_SIZE   100
#define         MAX_STACK_SIZE  1000

typedef struct vnodeName
{
        int     vnode;
	int	vunique;	/* uniquefier */
        char*   name;           /* name for this vnode */
} VnodeName;

typedef struct dirEnt
{
        int     vnode;          /* this directory's vnode */
	int 	inode;		/* this directory's inode */
        int     vnodeParent;    /* parent directory's vnode */
        int     numEntries;     /* number of enrtries in this directory */
        VnodeName*      vnodeName; /* the entries themselves */
       struct dirEntry*        next;   /* used by hash table */
} DirEnt;

