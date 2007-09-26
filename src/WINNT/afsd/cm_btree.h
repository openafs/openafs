/* 
 * Copyright 2007 Secure Endpoints Inc.  
 * 
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Thanks to Jan Jannink for B+ tree algorithms.
 */

#ifndef BPLUSTREE_H
#define BPLUSTREE_H 1

/********	flag bits (5 of 16 used, 11 for magic value)	********/

/* bits set at node creation/split/merge */
#define isLEAF	        0x01
#define isROOT	        0x02
#define isDATA          0x04

/* bits set at key insertion/deletion */
#define isFULL	        0x08
#define FEWEST	        0x10
#define FLAGS_MASK	0xFF

/* identifies data as being a B+tree node */
#define BTREE_MAGIC	      0xBEE0BEE0
#define BTREE_MAGIC_MASK      0xFFFFFFFF


/*************************	constants	************************/

/* The maximum number of Entrys in one Node */
#define MAX_FANOUT	9

/* corresponds to a NULL node pointer value */
#define NONODE          NULL

/* special node slot values used in key search */
#define BTERROR	        -1
#define BTUPPER	        -2
#define BTLOWER	        -3


/************************* Data Types **********************************/
typedef struct node	*Nptr;

typedef struct key {
    char *name;
} keyT;


typedef struct dirdata {
    cm_fid_t    fid;
    char * longname;
} dataT;

typedef struct entry {
    keyT	key;
    Nptr	downNode;
} Entry;

typedef struct inner {
    short	pairs;
    Nptr	firstNode;
} Inner;


typedef struct leaf {
    short	pairs;
    Nptr	nextNode;
} Leaf;

typedef struct data {
    keyT        key;
    dataT	value;
    Nptr        next;
} Data;

typedef struct node {
    Nptr                pool;
    unsigned short      number;
    unsigned short	flags;
    unsigned long       magic;
    union {
        Entry	e[MAX_FANOUT];	/* allows access to entry array */
        Inner	i;
        Leaf	l;
        Data	d;
    } X;
} Node;


typedef int (*KeyCmp)(keyT, keyT, int);
#define EXACT_MATCH 0x01


typedef struct tree {
    unsigned int        flags;          /* tree flags */
    unsigned int	poolsize;	/* # of nodes allocated for tree */
    Nptr		root;		/* pointer to root node */
    Nptr		leaf;		/* pointer to first leaf node in B+tree */
    unsigned int	fanout;		/* # of pointers to other nodes */
    unsigned int	minfanout;	/* usually minfanout == ceil(fanout/2) */
    unsigned int	height;		/* nodes traversed from root to leaves */
    Nptr		pool;		/* list of all nodes */
    Nptr                empty;          /* list of empty nodes */
    keyT		theKey;		/*  the key value used in tree operations */
    dataT		theData;	/*  data used for insertions/deletions */
    union {			        /* nodes to change in insert and delete */
        Nptr	split;
        Nptr	merge;
    } branch;
    KeyCmp	keycmp;		        /* pointer to function comparing two keys */
    char message[1024];
} Tree;

#define TREE_FLAGS_MASK                 0x01
#define TREE_FLAG_UNIQUE_KEYS           0x01

/************************* B+ tree Public functions ****************/
Tree	*initBtree(unsigned int poolsz, unsigned int fan, KeyCmp keyCmp);
void	freeBtree(Tree *B);

#ifdef DEBUG_BTREE
void	showNode(Tree *B, const char * str, Nptr node);
void	showBtree(Tree *B);
void	listBtreeValues(Tree *B, Nptr start, int count);
void	listAllBtreeValues(Tree *B);
void    listBtreeNodes(Tree *B, const char * where, Nptr node);
void    findAllBtreeValues(Tree *B);
#endif

void	insert(Tree *B, keyT key, dataT data);
void	delete(Tree *B, keyT key);
Nptr	lookup(Tree *B, keyT key);

/******************* cache manager directory operations ***************/
int  cm_BPlusDirLookup(cm_dirOp_t * op, char *entry, cm_fid_t * cfid);
long cm_BPlusDirCreateEntry(cm_dirOp_t * op, char *entry, cm_fid_t * cfid);
int  cm_BPlusDirDeleteEntry(cm_dirOp_t * op, char *entry);
long cm_BPlusDirBuildTree(cm_scache_t *scp, cm_user_t *userp, cm_req_t* reqp);
void cm_BPlusDumpStats(void);
int cm_MemDumpBPlusStats(FILE *outputFile, char *cookie, int lock);

extern afs_uint32 bplus_free_tree;
extern afs_uint32 bplus_dv_error;
extern afs_uint64 bplus_free_time;

/************ Accessor Macros *****************************************/
			
/* low level definition of Nptr value usage */
#define nAdr(n) (n)->X

/* access keys and pointers in a node */
#define getkey(j, q) (nAdr(j).e[(q)].key)
#define getnode(j, q) (nAdr(j).e[(q)].downNode)
#define setkey(j, q, v) ((q > 0) ? nAdr(j).e[(q)].key.name = strdup((v).name) : NULL)
#define setnode(j, q, v) (nAdr(j).e[(q)].downNode = (v))

/* access tree flag values */
#define settreeflags(B,v) (B->flags |= (v & TREE_FLAGS_MASK))
#define gettreeflags(B)   (B->flags)
#define cleartreeflags(B) (B->flags = 0);

/* access node flag values */
#define setflag(j, v) ((j)->flags |= (v & FLAGS_MASK))
#define clrflag(j, v) ((j)->flags &= ~(v & FLAGS_MASK))
#define getflags(j) ((j)->flags & FLAGS_MASK)
#define getmagic(j)  ((j)->magic & BTREE_MAGIC_MASK)
#define clearflags(j) ((j)->flags = 0, (j)->magic = BTREE_MAGIC)


/* check that a node is in fact a node */
#define isnode(j) (((j) != NONODE) && (((j)->magic & BTREE_MAGIC_MASK) == BTREE_MAGIC) && !((j)->flags & isDATA))
#define isntnode(j) ((j) == NONODE)


/* test individual flag values */
#define isinternal(j) (((j)->flags & isLEAF) == 0)
#define isleaf(j) (((j)->flags & isLEAF) == isLEAF)
#define isdata(j) (((j)->flags & isDATA) == isDATA)
#ifndef DEBUG_BTREE
#define isroot(j) (((j)->flags & isROOT) == isROOT)
#define isfull(j) (((j)->flags & isFULL) == isFULL)
#define isfew(j) (((j)->flags & FEWEST) == FEWEST)
#else
#define isroot(j) _isRoot(B, j)
#define isfew(j) _isFew(B, j)
#define isfull(j) _isFull(B, j)
#endif

/* manage number of keys in a node */
#define numentries(j) (nAdr(j).i.pairs)
#define clearentries(j) (nAdr(j).i.pairs = 0)
#define incentries(j) (nAdr(j).i.pairs++)
#define decentries(j) (nAdr(j).i.pairs--)


/* manage first/last node pointers in internal nodes */
#define setfirstnode(j, v) (nAdr(j).i.firstNode = (v))
#define getfirstnode(j) (nAdr(j).i.firstNode)
#define getlastnode(j) (nAdr(j).e[nAdr(j).i.pairs].downNode)


/* manage pointers to next nodes in leaf nodes */
/* also used for free nodes list */
#define setnextnode(j, v) (nAdr(j).l.nextNode = (v))
#define getnextnode(j) (nAdr(j).l.nextNode)

/* manage pointers to all nodes list */
#define setallnode(j, v) ((j)->pool = (v))
#define getallnode(j) ((j)->pool)

/* manage access to data nodes */
#define getdata(j) (nAdr(j).d)
#define getdatavalue(j) (getdata(j).value)
#define getdatakey(j)   (getdata(j).key)
#define getdatanext(j)  (getdata(j).next)

/* shift/transfer entries for insertion/deletion */
#define clrentry(j, q) _clrentry(j,q)
#define pushentry(j, q, v) _pushentry(j, q, v)
#define pullentry(j, q, v) _pullentry(j, q, v)
#define xferentry(j, q, v, z) _xferentry(j, q, v, z)
#define setentry(j, q, v, z) _setentry(j, q, v, z)


/* access key and data values for B+tree methods */
/* pass values to getSlot(), descend...() */
#define getfunkey(B) ((B)->theKey)
#define getfundata(B) ((B)->theData)
#define setfunkey(B,v) ((B)->theKey = (v))
#define setfundata(B,v) ((B)->theData = (v))

/* define number of B+tree nodes for free node pool */
#define getpoolsize(B) ((B)->poolsize)
#define setpoolsize(B,v) ((B)->poolsize = (v))

/* locations from which tree access begins */
#define getroot(B) ((B)->root)
#define setroot(B,v) ((B)->root = (v))
#define getleaf(B) ((B)->leaf)
#define setleaf(B,v) ((B)->leaf = (v))

/* define max/min number of pointers per node */
#define getfanout(B) ((B)->fanout)
#define setfanout(B,v) ((B)->fanout = (v) - 1)
#define getminfanout(B,j) (isroot(j) ? (isleaf(j) ? 0 : 1) : (isleaf(j) ? (B)->fanout - (B)->minfanout: (B)->minfanout))
#define setminfanout(B,v) ((B)->minfanout = (v) - 1)

/* manage B+tree height */
#define inittreeheight(B) ((B)->height = 0)
#define inctreeheight(B) ((B)->height++)
#define dectreeheight(B) ((B)->height--)
#define gettreeheight(B) ((B)->height)

/* access pool of free nodes */
#define getfirstfreenode(B) ((B)->empty)
#define setfirstfreenode(B,v) ((B)->empty = (v))

/* access all node list */
#define getfirstallnode(B) ((B)->pool)
#define setfirstallnode(B,v) ((B)->pool = (v))

/* handle split/merge points during insert/delete */
#define getsplitpath(B) ((B)->branch.split)
#define setsplitpath(B,v) ((B)->branch.split = (v))
#define getmergepath(B) ((B)->branch.merge)
#define setmergepath(B,v) ((B)->branch.merge = (v))

/* exploit function to compare two B+tree keys */
#define comparekeys(B) (*(B)->keycmp)
#define setcomparekeys(B,v) ((B)->keycmp = (v))

/* location containing B+tree class variables */
#define setbplustree(B,v) ((B) = (Tree *)(v))

/* representation independent node numbering */
#define setnodenumber(B,v,q) ((v)->number = (q))
#define getnodenumber(B,v) ((v) != NONODE ? (v)->number : -1)

#endif /* BPLUSTREE_H */

