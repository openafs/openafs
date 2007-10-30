/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/budb_client.h>
#include <afs/afsutil.h>

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif /* HAVE_STDIO_H */

#include <string.h>

/*
 * Represents a host in the config database.
 */
struct bc_hostEntry {
    struct bc_hostEntry *next;	/*Ptr to next record */
    char *name;			/*Stringname for host */
    struct sockaddr_in addr;	/*Corresponding sockaddr */
    afs_int32 portOffset;	/*Port=standardPort+portOffset-allows multiple TC on a host */
};

/*
 * Global backup program configuration information.
 */
struct bc_config {
    char *path;			/*Root directory for config info */
    struct bc_hostEntry *dbaseHosts;	/*Hosts providing the backup database service */
    struct bc_hostEntry *tapeHosts;	/*Hosts providing the tape drives */
    struct bc_volumeSet *vset;	/*List of all volume sets */
    struct bc_dumpSchedule *dsched;	/*Dump schedule list */
    udbClientTextT configText[TB_NUM];	/* configuration text handles */
    char tmpTextFileNames[TB_NUM][AFSDIR_PATH_MAX];	/* names of temp files created to store config text recd from buserver */
};

/*
  * Central status information relating to per-opcode routine information.
  */
struct bc_opstatus {
    int isCmdLineOpcode;	/*Is this the opcode being done for the command line? */
};

/*
 * Representation of a Volume Set, namely the specification for a group of
 * related volumes. Each volume set has a name and a list of volume
 * descriptions, one for each line in the volumeset configuration file.
 */
#define VSFLAG_TEMPORARY 1	/* Volume set is temporary */

struct bc_volumeSet {
    struct bc_volumeSet *next;	/*Ptr to next volume set record */
    char *name;			/*Volume set name */
    afs_int32 flags;		/* flags */
    struct bc_volumeEntry *ventries;	/*List of component volume entries */
};

/*
 * Represents the name of a volume specifier in a volume set.
 */
struct bc_volumeEntry {
    struct bc_volumeEntry *next;	/*Ptr to next record in list */
    char *serverName;		/*Host name for volume spec */
    struct sockaddr_in server;	/*Host sockaddr for volume spec */
    char *partname;		/*Partition pattern name */
    afs_int32 partition;	/*Partition number for volume spec */
    char *name;			/*Volume pattern name */
};

/*
 * Represents an individual volume to be dumped, not a collection.
 */
struct bc_volumeDump {
    struct bc_volumeDump *next;	/*Ptr to next record */
    afs_int32 vid;		/*Volume id, or 0 if not known */
    struct bc_volumeEntry *entry;	/*Back pointer: information about server (obs?) */
    char *name;			/*Individual volume name */
    afs_int32 volType;		/*Volume type */
    afs_int32 date;		/*From date (for full, incremental or whatever) */
    afs_int32 cloneDate;	/* time of this volume's snapshot */
    afs_int32 partition;	/* partition containing this volume */
    struct sockaddr_in server;	/* server to obtain data from */
};

/*
 * Represents a dump schedule node, representing one type of dump (e.g. the
 * safe5 daily incremental)
 */
struct bc_dumpSchedule {
    struct bc_dumpSchedule *next;	/*Ptr to next record */
    char *name;			/*Dump sched name */
    char *vsname;		/*Volume set name to dump */
    afs_int32 period;		/*Period in minutes */
    afs_int32 periodType;	/*Qualifier on above, for exceptions like 'monthly' */
    char *parentName;		/*Parent dump schedule name-unused PA */
    struct bc_dumpSchedule *parent;	/*These are built at run-time */
    struct bc_dumpSchedule *firstChild;
    struct bc_dumpSchedule *nextSibling;
    afs_int32 level;		/*Level of the dump (do we need this?) */
    afs_int32 expDate;		/* expiration date */
    afs_int32 expType;		/* absolute or relative expiration */
};

/*
 * Private: represents a queued dump/restore item.
 */
struct bc_dumpTask {
    int (*callProc) ();
    struct bc_config *config;
    struct bc_volumeDump *volumes;
    char *dumpName;		/*Dump name we're doing */
    char *volSetName;		/*Volume set we're dumping */
    afs_int32 flags;
    afs_int32 dumpID;		/*Dump ID we're running */
    int oldFlag;		/* if a restore, whether we're doing it to the same vol#s */
    struct sockaddr_in destServer;	/* destination server for restore command */
    afs_int32 destPartition;	/* destination partition for restore command */
    afs_int32 fromDate;		/* date from which to do a restore command */
    afs_int32 parentDumpID;	/* parent dump ID */
    afs_int32 dumpLevel;	/* dump level being performed */
    char *newExt;		/* new volume extension if any, if oldFlag is false */
    afs_int32 bytesTransferred;	/* How many bytes sent */
    afs_int32 volumeBeingDumped;	/* the volume being processed */
    afs_int32 *portOffset;	/* used to derive the ports of the TCs */
    afs_int32 portCount;	/* number of points in the portOffset array */
    afs_int32 expDate;		/* for dumps - expiration date */
    afs_int32 expType;		/* for dumps - abs. or relative expiration */
    int doAppend;		/* for dumps - append this dump to the dump set */
    int dontExecute;		/* dont execute the dump or restore */
};

#define	BC_DI_INUSE	1	/* entry not used */

#define	DBHOSTFILE	"dbasehosts"
#define	TAPEHOSTFILE	"tapehosts"
#define	DSNAME		"dumpschedule"
#define	VSNAME		"volumeset"

#define	BC_MAXSIMDUMPS	    64
#define BC_MAXPORTS	    128	/* max number of port offsets for volrestore */
/* debugging support */
#define	dprintf(x)
