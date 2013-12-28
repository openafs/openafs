/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#define	BOP_TIMEOUT(bnode)	((*(bnode)->ops->timeout)((bnode)))
#define	BOP_GETSTAT(bnode, a)	((*(bnode)->ops->getstat)((bnode),(a)))
#define	BOP_SETSTAT(bnode, a)	((*(bnode)->ops->setstat)((bnode),(a)))
#define	BOP_DELETE(bnode)	((*(bnode)->ops->delete)((bnode)))
#define	BOP_PROCEXIT(bnode, a)	((*(bnode)->ops->procexit)((bnode),(a)))
#define	BOP_GETSTRING(bnode, a, b)	((*(bnode)->ops->getstring)((bnode),(a), (b)))
#define	BOP_GETPARM(bnode, n, b, l)	((*(bnode)->ops->getparm)((bnode),(n),(b),(l)))
#define	BOP_RESTARTP(bnode)	((*(bnode)->ops->restartp)((bnode)))
#define BOP_HASCORE(bnode)	((*(bnode)->ops->hascore)((bnode)))
#define BOP_PROCSTARTED(bnode,p)	((*(bnode)->ops->procstarted)((bnode),(p)))

struct bnode_proc;

struct bnode_ops {
    struct bnode *(*create) ( char *, char *, char *, char *, char *, char *);
    int (*timeout) ( struct bnode * );
    int (*getstat) ( struct bnode *, afs_int32 * );
    int (*setstat) ( struct bnode *, afs_int32 );
    int (*delete) ( struct bnode * );
    int (*procexit) ( struct bnode *, struct bnode_proc * );
    int (*getstring) ( struct bnode *, char *abuffer, afs_int32 alen );
    int (*getparm) ( struct bnode *, afs_int32 aindex, char *abuffer,
		     afs_int32 alen);
    int (*restartp) ( struct bnode *);
    int (*hascore) ( struct bnode *);
    int (*procstarted) ( struct bnode *, struct bnode_proc * );
};

struct bnode_type {
    struct bnode_type *next;
    char *name;
    struct bnode_ops *ops;
};

struct bnode_token {
    struct bnode_token *next;
    char *key;
};

struct bnode {
    struct bnode *next;		/* next pointer in top-level's list */
    char *name;			/* instance name */
    afs_int32 nextTimeout;	/* next time this guy should be woken */
    afs_int32 period;		/* period between calls */
    afs_int32 rsTime;		/* time we started counting restarts */
    afs_int32 rsCount;		/* count of restarts since rsTime */
    struct bnode_type *type;	/* type object */
    struct bnode_ops *ops;	/* functions implementing bnode class */
    afs_int32 procStartTime;	/* last time a process was started */
    afs_int32 procStarts;	/* number of process starts */
    afs_int32 lastAnyExit;	/* last time a process exited, for any reason */
    afs_int32 lastErrorExit;	/* last time a process exited unexpectedly */
    afs_int32 errorCode;	/* last exit return code */
    afs_int32 errorSignal;	/* last proc terminating signal */
    char *lastErrorName;	/* name of proc that failed last */
    char *notifier;		/* notifier program to be executed on exits */
    short refCount;		/* reference count */
    short flags;		/* random flags */
    char goal;			/* 1=running or 0=not running */
    char fileGoal;		/* same, but to be stored in file */
    afs_int32 errorStopCount;	/* number of recent error stops */
    afs_int32 errorStopDelay;	/* seconds to wait before retrying start */
};

struct bnode_proc {
    struct bnode_proc *next;	/* next guy in top-level's list */
    struct bnode *bnode;	/* bnode creating this process */
    char *comLine;		/* command line used to start this process */
    char *coreName;		/* optional core file component name */
    afs_int32 pid;		/* pid if created */
    afs_int32 lastExit;		/* last termination code */
    afs_int32 lastSignal;	/* last signal that killed this guy */
    afs_int32 flags;		/* flags giving process state */
};

struct ezbnode {
    struct bnode b;
    afs_int32 zapTime;		/* time we sent a sigterm */
    char *command;
    struct bnode_proc *proc;
    afs_int32 lastStart;	/* time last started process */
    char waitingForShutdown;	/* have we started any shutdown procedure? */
    char running;		/* is process running? */
    char killSent;		/* have we tried sigkill signal? */
};

/* this struct is used to construct a list of dirpaths, along with
 * their recommended permissions
 */
struct bozo_bosEntryStats {
    const char *path;		/* pathname to check */
    int dir;			/* 1=>dir or 0=>file */
    int rootOwner;		/* 1=>root must own */
    int reqPerm;		/* required permissions */
    int proPerm;		/* prohibited permissions */
};
/* bnode flags */
#define	BNODE_NEEDTIMEOUT	    1	/* timeouts are active */
#define	BNODE_ACTIVE		    2	/* in generic lists */
#define	BNODE_WAIT		    4	/* someone waiting for status change */
#define	BNODE_DELETE		    8	/* delete this bnode asap */
#define	BNODE_ERRORSTOP		    0x10	/* stopped due to errors */

/* flags for bnode_proc */
#define	BPROC_STARTED		    1	/* ever started */
#define	BPROC_EXITED		    2	/* exited */

#define	NONOTIFIER		    "__NONOTIFIER__"

/* status values for bnodes, and goals */
#define	BSTAT_SHUTDOWN		    0	/* shutdown normally */
#define	BSTAT_NORMAL		    1	/* running normally */
#define	BSTAT_SHUTTINGDOWN	    2	/* normal --> shutdown */
#define	BSTAT_STARTINGUP	    3	/* shutdown --> normal */

/* exit values indicating that NT SCM integrator should restart bosserver */
#ifdef AFS_NT40_ENV
#define BOSEXIT_RESTART        0xA0
#define BOSEXIT_DORESTART(code)  (((code) & ~(0xF)) == BOSEXIT_RESTART)
#endif

/* max time to wait for fileserver shutdown */
#define	FSSDTIME	(30 * 60)	/* seconds */

/* calls back up to the generic bnode layer */
extern int bnode_SetTimeout(struct bnode *abnode, afs_int32 atimeout);
extern int bnode_Init(void);
extern int bnode_NewProc(struct bnode *abnode, char *aexecString, char *coreName, struct bnode_proc **aproc);
extern int bnode_InitBnode(struct bnode *abnode, struct bnode_ops *abnodeops, char *aname);
extern afs_int32 bnode_Create(char *atype, char *ainstance, struct bnode ** abp, char *ap1, char *ap2, char *ap3, char *ap4, char *ap5, char *notifier, int fileGoal, int rewritefile);
extern struct bnode *bnode_FindInstance(char *aname);
extern int bnode_WaitStatus(struct bnode *abnode, int astatus);
extern int bnode_SetStat(struct bnode *abnode, int agoal);
extern int bnode_CreatePidFile(struct bnode *abnode, struct bnode_proc *aproc, char *name);
extern int bnode_DestroyPidFile(struct bnode *abnode, struct bnode_proc *aproc);
extern int bnode_ResetErrorCount(struct bnode *abnode);
