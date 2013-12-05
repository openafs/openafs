/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* for input byte sequences */
customized struct ka_CBS {
	afs_int32 SeqLen;
	char *SeqBody;
};

/* for in/out byte sequences */
customized struct ka_BBS {
	afs_int32 MaxSeqLen;
	afs_int32 SeqLen;
	char *SeqBody;
};

const   MAXKAKVNO     = 127;	/* The key version number must fit in a byte */

/* flags: zero is an illegal value */
const   KAFNORMAL      = 0x001;	/* set for all user entries */
  /* if the normal is off then one of these two MUST be set */
const	KAFFREE	       = 0x002;	/* set if in free list */
const	KAFOLDKEYS     = 0x010;	/* if entry used to store old keys */
  /* otherwise one of these may be set to define the usage of the misc field */
const   KAFSPECIAL     = 0x100;	/* set if special AuthServer principal */
const   KAFASSOCROOT   = 0x200;	/* set if root of associate tree */
const   KAFASSOC       = 0x400;	/* set if entry is an associate */
  /* These bits define special propertied of normal users. */
const	KAFADMIN       = 0x004;	/* an administrator */
const	KAFNOTGS       = 0x008;	/* ! allow principal to get or use TGT */
const	KAFNOSEAL      = 0x020;	/* ! allow principal as server in GetTicket */
const	KAFNOCPW       = 0x040;	/* ! allow principal to change its own key */
const	KAFNEWASSOC    = 0x080;	/* allow user to create associates */

/* these flags are settable using SetFields */
%#define KAF_SETTABLE_FLAGS (KAFADMIN | KAFNOTGS | KAFNOSEAL | KAFNOCPW | KAFNEWASSOC)

/* This struction defines an encryption key that is bit level compatible with
 * DES and ktc_encryptionKey but which will have to be cast to the appropriate
 * type in calls. */

struct EncryptionKey {
    char key[8];
};

/* These structures are returned by server RPC interface routines.  To make
 * future revisions easy to accomodate they are assigned a major and minor
 * version number.  Major version changes will require recompilation because of
 * the structures have changed size.  Minor version changes will be more or
 * less upward compaitible. */
const KAMAJORVERSION = 5;		/* as of 890301 */
const KAMINORVERSION = 2;

%#ifndef NEVERDATE
%#define NEVERDATE 037777777777		/* a date that will never come */
%#endif
%#ifndef Date
%#define Date afs_uint32
%#endif
#define Date afs_uint32
/* We log to AuthLog and a dbm-based log file on most platforms.
 * On NT & some HPs  we only log to AuthLog
 * For HPs, AUTH_DBM_LOG is defined in their respective Makefiles for
 * platforms that can log using dbm.
 * On Linux, dbm is not part of the standard installation, and we can't
 * statically link it in. So, ignore it for now.
 */
%#if !defined(AFS_HPUX_ENV) && !defined(AFS_NT40_ENV) && !defined(AFS_LINUX20_ENV)
%#define AUTH_DBM_LOG
%#endif

#define MAXKANAMELEN 64			/* don't export: use MAXKTCNAMELEN */
typedef string kaname<MAXKANAMELEN>;

/* A structure for returning name and instance strings */
struct kaident {
    char name[MAXKANAMELEN];		/* user name */
    char instance[MAXKANAMELEN];	/* group name */
};

/* A structure for returning entry information */
struct kaentryinfo {
    afs_int32	   minor_version;	/* the minor version of this struct */
    afs_int32	   flags;		/* random flags */
    Date	   user_expiration;	/* user registration good till then */
    Date	   modification_time;	/* time of last update */
    struct kaident modification_user;	/* user name & inst last mod. entry */
    Date	   change_password_time;/* time user changed own password */
    afs_int32	   max_ticket_lifetime;	/* maximum lifetime for tickets */
    afs_int32	   key_version;		/* verson number of this key */
    EncryptionKey  key;			/* the key to use */
    afs_uint32  keyCheckSum;		/* crypto-cksum of key */
    afs_uint32  misc_auth_bytes;     /* expiry, plus more byte values */
    afs_int32	   reserved3;	       	/* NOT Spare - used to hold pwsums[0] */
    afs_int32	   reserved4;
};

/* These are (static) statistics kept in the database header */
/* WARNING: Changing the size of this structure affects the on-disk database
 * header, which will force it to be rebuilt. */
struct kasstats {
    afs_int32 minor_version;			/* the minor version of this struct */
    afs_int32 allocs;			/* total # of calls to AllocBlock */
    afs_int32 frees;				/* total # of calls to FreeBlock */
    afs_int32 cpws;				/* # of user change password cmds */
    afs_int32 reserved1;
    afs_int32 reserved2;
    afs_int32 reserved3;
    afs_int32 reserved4;
};

struct katimeval {
	afs_int32	tv_sec;		/* seconds */
	afs_int32	tv_usec;	/* and microseconds */
};
struct karpcstats {int requests; int aborts;};
#define declare_stat(n) struct karpcstats n

/* These are dynamic statistics kept in the each AuthServer process */
struct kadstats {
    afs_int32 minor_version;			/* the minor version of this struct */
    afs_int32 host;				/* host number */
    Date start_time;			/* time statistics were last cleared */
    /* statistics that can be calculated upon request */
#if (KAMAJORVERSION>5)
    struct katimeval utime;
    struct katimeval stime;
    int  dataSize;
    int  stackSize;
    int  pageFaults;
#endif
    afs_int32 hashTableUtilization;		/* utilization of non-empty hash table
					   entries in parts per 10,000 */
    /* count of requests and aborts for each RPC */
    declare_stat(Authenticate);
    declare_stat(ChangePassword);
    declare_stat(GetTicket);
    declare_stat(CreateUser);
    declare_stat(SetPassword);
    declare_stat(SetFields);
    declare_stat(DeleteUser);
    declare_stat(GetEntry);
    declare_stat(ListEntry);
    declare_stat(GetStats);
    declare_stat(GetPassword);
    declare_stat(GetRandomKey);
    declare_stat(Debug);
    declare_stat(UAuthenticate);
    declare_stat(UGetTicket);
    declare_stat(Unlock);
    declare_stat(LockStatus);
    afs_int32 string_checks;			/* errors detected in name.inst strs */
    afs_int32 reserved1;
    afs_int32 reserved2;
    afs_int32 reserved3;
    afs_int32 reserved4;
};

/* This returns information about the state of the server for debugging
   problems remotely. */

const KADEBUGKCINFOSIZE = 25;

struct ka_kcInfo {
    Date used;
    afs_int32 kvno;
    char primary;
    char keycksum;
    char principal[64];
};

struct ka_debugInfo {
    afs_int32 minorVersion;			/* the minor version of this struct */
    afs_int32 host;				/* host number */
    Date startTime;			/* time server was started */
#if (KAMAJORVERSION>5)
    Date now;				/* current server time */
#endif
    int  noAuth;			/* running with authentication off */
    /* activity */
    Date lastTrans;			/* time of last transation */
    char lastOperation[16];		/* name of last operation */
    char lastAuth[256];			/* last principal to authenticate */
    char lastUAuth[256];		/*  "  authenticate via UDP */
    char lastTGS[256];			/*  "  call ticket granting service */
    char lastUTGS[256];			/*  "  call TGS via UDP */
    char lastAdmin[256];		/*  "  call admin service */
    char lastTGSServer[256];		/*  last server a ticket was req for */
    char lastUTGSServer[256];		/*  "       "     " via UDP */
    Date nextAutoCPW;			/* time of next AutoCPW attempt */
    int	 updatesRemaining;		/* update necessary for next AutoCPW */
    /* db header stuff */
    Date dbHeaderRead;			/* time cheader was last read in */
    afs_int32 dbVersion;
    afs_int32 dbFreePtr;
    afs_int32 dbEofPtr;
    afs_int32 dbKvnoPtr;
    afs_int32 dbSpecialKeysVersion;
    /* these are of type struct Lock */
    afs_int32 cheader_lock;
    afs_int32 keycache_lock;

    /* key cache stuff */
    afs_int32 kcVersion;
    int	 kcSize;
    int	 kcUsed;
    struct ka_kcInfo kcInfo[KADEBUGKCINFOSIZE];

    afs_int32 reserved1;
    afs_int32 reserved2;
    afs_int32 reserved3;
    afs_int32 reserved4;
};

/* finally the procedural definitions */

package KAA_
prefix S
statindex 18

proc Authenticate_old(
  IN kaname name,
  IN kaname instance,
  IN Date start_time,
  IN Date end_time,
  IN struct ka_CBS *request,
  INOUT struct ka_BBS *answer
) = 1;

proc Authenticate(
  IN kaname name,
  IN kaname instance,
  IN Date start_time,
  IN Date end_time,
  IN struct ka_CBS *request,
  INOUT struct ka_BBS *answer
) = 21;

proc AuthenticateV2(
  IN kaname name,
  IN kaname instance,
  IN Date start_time,
  IN Date end_time,
  IN struct ka_CBS *request,
  INOUT struct ka_BBS *answer
) = 22;

proc ChangePassword(
  IN kaname name,
  IN kaname instance,
  IN struct ka_CBS *arequest,
  INOUT struct ka_BBS *oanswer
) = 2;

package KAT_
prefix S
statindex 19

proc GetTicket_old(
  IN afs_int32 kvno,
  IN kaname auth_domain,
  IN struct ka_CBS *aticket,
  IN kaname name,
  IN kaname instance,
  IN struct ka_CBS *atimes,
  INOUT struct ka_BBS *oanswer
) = 3;

proc GetTicket(
  IN afs_int32 kvno,
  IN kaname auth_domain,
  IN struct ka_CBS *aticket,
  IN kaname name,
  IN kaname instance,
  IN struct ka_CBS *atimes,
  INOUT struct ka_BBS *oanswer
) = 23;

package KAM_
prefix S
statindex 20

proc SetPassword(
  IN kaname name,
  IN kaname instance,
  IN afs_int32 kvno,
  IN EncryptionKey password
) = 4;

proc SetFields(
  IN kaname name,
  IN kaname instance,
  IN afs_int32 flags,
  IN Date user_expiration,
  IN afs_int32 max_ticket_lifetime,
  IN afs_int32 maxAssociates,
  IN afs_uint32 misc_auth_bytes,
  IN afs_int32 spare2
) = 5;

proc CreateUser(
  IN kaname name,
  IN kaname instance,
  IN EncryptionKey password
) = 6;

proc DeleteUser(
  IN kaname name,
  IN kaname instance
) = 7;

proc GetEntry(
  IN kaname name,
  IN kaname instance,
  IN afs_int32 major_version,
  OUT struct kaentryinfo *entry
) = 8;

proc ListEntry(
  IN afs_int32 previous_index,
  OUT afs_int32 *index,
  OUT afs_int32 *count,
  OUT kaident *name
) = 9;

proc GetStats(
  IN afs_int32 major_version,
  OUT afs_int32 *admin_accounts,
  OUT struct kasstats *statics,
  OUT struct kadstats *dynamics
) = 10;

proc Debug(
  IN afs_int32 major_version,
  IN int checkDB,
  OUT struct ka_debugInfo *info
) = 11;

proc GetPassword(
  IN kaname name,
  OUT EncryptionKey *password
) = 12;

proc GetRandomKey(
  OUT EncryptionKey *password
) = 13;

proc Unlock(
  IN kaname name,
  IN kaname instance,
  IN afs_int32 spare1,
  IN afs_int32 spare2,
  IN afs_int32 spare3,
  IN afs_int32 spare4
) = 14;

proc LockStatus(
  IN kaname name,
  IN kaname instance,
  OUT afs_int32 *lockeduntil,
  IN afs_int32 spare1,
  IN afs_int32 spare2,
  IN afs_int32 spare3,
  IN afs_int32 spare4
) = 15;
