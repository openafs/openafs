/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_WINNT_AFSD_SMB_H
#define OPENAFS_WINNT_AFSD_SMB_H 1

/* #define DEBUG_SMB_REFCOUNT 1 */

#if _WIN32_WINNT < 0x0501
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#include <ntsecapi.h>

#include <cm_nls.h>

/* basic core protocol SMB structure */
#pragma pack(push, 1)
typedef struct smb {
    unsigned char id[4];
    unsigned char com;
    unsigned char rcls;
    unsigned char reh;
    unsigned char errLow;
    unsigned char errHigh;
    unsigned char reb;
    unsigned short flg2;
    unsigned short res[6];
    unsigned short tid;
    unsigned short pid;
    unsigned short uid;
    unsigned short mid;
    unsigned char wct;
    unsigned char vdata[1];
} smb_t;
#pragma pack(pop)

/* reb values */
#define SMB_FLAGS_SUPPORT_LOCKREAD         0x01
#define SMB_FLAGS_CLIENT_BUF_AVAIL         0x02
#define SMB_FLAGS_CASELESS_PATHNAMES       0x08
#define SMB_FLAGS_CANONICAL_PATHNAMES      0x10
#define SMB_FLAGS_REQUEST_OPLOCK           0x20
#define SMB_FLAGS_REQUEST_BATCH_OPLOCK     0x40
#define SMB_FLAGS_SERVER_TO_CLIENT         0x80

/* flg2 values */

#define SMB_FLAGS2_KNOWS_LONG_NAMES        0x0001
#define SMB_FLAGS2_KNOWS_EAS               0x0002
#define SMB_FLAGS2_SECURITY_SIGNATURE      0x0004
#define SMB_FLAGS2_RESERVED1               0x0008
#define SMB_FLAGS2_IS_LONG_NAME            0x0040
#define SMB_FLAGS2_EXT_SEC                 0x0800
#define SMB_FLAGS2_DFS_PATHNAMES           0x1000
#define SMB_FLAGS2_PAGING_IO               0x2000
#define SMB_FLAGS2_32BIT_STATUS            0x4000
#define SMB_FLAGS2_UNICODE                 0x8000

#define KNOWS_LONG_NAMES(inp) ((((smb_t *)inp)->flg2 & SMB_FLAGS2_KNOWS_LONG_NAMES)?1:0)
#define WANTS_DFS_PATHNAMES(inp) ((((smb_t *)inp)->flg2 & SMB_FLAGS2_DFS_PATHNAMES)?1:0)
#define WANTS_UNICODE(inp) ((((smb_t *)inp)->flg2 & SMB_FLAGS2_UNICODE)?1:0)

/* Information Levels */
#define SMB_INFO_STANDARD               1
#define SMB_INFO_QUERY_EA_SIZE          2
#define SMB_INFO_QUERY_EAS_FROM_LIST    3
#define SMB_INFO_QUERY_ALL_EAS          4
#define SMB_INFO_IS_NAME_VALID          6

#define SMB_QUERY_FILE_BASIC_INFO       0x101
#define SMB_QUERY_FILE_STANDARD_INFO    0x102
#define SMB_QUERY_FILE_EA_INFO          0x103
#define SMB_QUERY_FILE_NAME_INFO        0x104
#define SMB_QUERY_FILE_ALL_INFO         0x107
#define SMB_QUERY_FILE_ALT_NAME_INFO    0x108
#define SMB_QUERY_FILE_STREAM_INFO      0x109
#define SMB_QUERY_FILE_COMPRESSION_INFO 0x10B
#define SMB_QUERY_FILE_UNIX_BASIC       0x200
#define SMB_QUERY_FILE_UNIX_LINK        0x201
#define SMB_INFO_PASSTHROUGH           0x1000

#define SMB_SET_FILE_BASIC_INFO         0x101
#define SMB_SET_FILE_DISPOSITION_INFO   0x102
#define SMB_SET_FILE_ALLOCATION_INFO    0x103
#define SMB_SET_FILE_END_OF_FILE_INFO   0x104
#define SMB_SET_FILE_UNIX_BASIC         0x200
#define SMB_SET_FILE_UNIX_LINK          0x201
#define SMB_SET_FILE_UNIX_HLINK         0x203

#define SMB_INFO_ALLOCATION 		1
#define SMB_INFO_VOLUME			2
#define SMB_QUERY_FS_LABEL_INFO         0x101
#define SMB_QUERY_FS_VOLUME_INFO 	0x102
#define SMB_QUERY_FS_SIZE_INFO		0x103
#define SMB_QUERY_FS_DEVICE_INFO	0x104
#define SMB_QUERY_FS_ATTRIBUTE_INFO	0x105
#define SMB_QUERY_FS_QUOTA_INFO         0x106
#define SMB_QUERY_FS_CONTROL_INFO       0x107
#define SMB_INFO_UNIX			0x200
#define SMB_INFO_MACOS			0x301

#define SMB_FIND_FILE_DIRECTORY_INFO      0x101
#define SMB_FIND_FILE_FULL_DIRECTORY_INFO 0x102
#define SMB_FIND_FILE_NAMES_INFO	  0x103
#define SMB_FIND_FILE_BOTH_DIRECTORY_INFO 0x104

/* SMB_COM_TRANSACTION Named pipe operations */
#define SMB_TRANS_SET_NMPIPE_STATE	0x0001
#define SMB_TRANS_RAW_READ_NMPIPE	0x0011
#define SMB_TRANS_QUERY_NMPIPE_STATE	0x0021
#define SMB_TRANS_QUERY_NMPIPE_INFO	0x0022
#define SMB_TRANS_PEEK_NMPIPE		0x0023
#define SMB_TRANS_TRANSACT_NMPIPE	0x0026
#define SMB_TRANS_RAW_WRITE_NMPIPE	0x0031
#define SMB_TRANS_READ_NMPIPE		0x0036
#define SMB_TRANS_WRITE_NMPIPE		0x0037
#define SMB_TRANS_WAIT_NMPIPE		0x0053
#define SMB_TRANS_CALL_NMPIPE		0x0054

/* more defines */
#define SMB_NOPCODES		256	/* # of opcodes in the dispatch table */

/* threads per VC */
#define SMB_THREADSPERVC	4	/* threads per VC */

/* flags for functions */
#define SMB_FLAG_CREATE		1	/* create the structure if necessary */
#define SMB_FLAG_AFSLOGON       2       /* operating on behalf of afslogon.dll */

/* max # of bytes we'll receive in an incoming SMB message */
/* the maximum is 2^18-1 for NBT and 2^25-1 for Raw transport messages */
/* we will use something smaller but large enough to be efficient */
#define SMB_PACKETSIZE	32768 /* was 8400 */
/* raw mode is considered obsolete and cannot be used with message signing */
#define SMB_MAXRAWSIZE  65536
/* max STRING characters per packet per request */
#define SMB_STRINGBUFSIZE 4096

/* Negotiate protocol constants */
/* Security */
#define NEGOTIATE_SECURITY_USER_LEVEL               0x01
#define NEGOTIATE_SECURITY_CHALLENGE_RESPONSE       0x02
#define NEGOTIATE_SECURITY_SIGNATURES_ENABLED       0x04
#define NEGOTIATE_SECURITY_SIGNATURES_REQUIRED      0x08

/* Capabilities */
#define NTNEGOTIATE_CAPABILITY_RAWMODE			0x00000001L
#define NTNEGOTIATE_CAPABILITY_MPXMODE			0x00000002L
#define NTNEGOTIATE_CAPABILITY_UNICODE			0x00000004L
#define NTNEGOTIATE_CAPABILITY_LARGEFILES		0x00000008L
#define NTNEGOTIATE_CAPABILITY_NTSMB			0x00000010L
#define NTNEGOTIATE_CAPABILITY_RPCAPI			0x00000020L
#define NTNEGOTIATE_CAPABILITY_NTSTATUS			0x00000040L
#define NTNEGOTIATE_CAPABILITY_LEVEL_II_OPLOCKS	        0x00000080L
#define NTNEGOTIATE_CAPABILITY_LOCK_AND_READ		0x00000100L
#define NTNEGOTIATE_CAPABILITY_NTFIND			0x00000200L
#define NTNEGOTIATE_CAPABILITY_DFS			0x00001000L
#define NTNEGOTIATE_CAPABILITY_NT_INFO_PASSTHRU		0x00002000L
#define NTNEGOTIATE_CAPABILITY_LARGE_READX		0x00004000L
#define NTNEGOTIATE_CAPABILITY_LARGE_WRITEX		0x00008000L
#define NTNEGOTIATE_CAPABILITY_UNIX     		0x00800000L
#define NTNEGOTIATE_CAPABILITY_BULK_TRANSFER		0x20000000L
#define NTNEGOTIATE_CAPABILITY_COMPRESSED		0x40000000L
#define NTNEGOTIATE_CAPABILITY_EXTENDED_SECURITY	0x80000000L

#define NTSID_LOCAL_SYSTEM L"S-1-5-18"

/* a packet structure for receiving SMB messages; locked by smb_globalLock.
 * Most of the work involved is in handling chained requests and responses.
 *
 * When handling input, inWctp points to the current request's wct field (and
 * the other parameters and request data can be found from this field).  The
 * opcode, unfortunately, isn't available there, so is instead copied to the
 * packet's inCom field.  It is initially set to com, but each chained
 * operation sets it, also.
 * The function smb_AdvanceInput advances an input packet to the next request
 * in the chain.  The inCom field is set to 0xFF when there are no more
 * requests.  The inCount field is 0 if this is the first request, and
 * otherwise counts which request it is.
 *
 * When handling output, we also have to chain all of the responses together.
 * The function smb_GetResponsePacket will setup outWctp to point to the right
 * place.
 */
#define SMB_PACKETMAGIC	0x7436353	/* magic # for packets */
typedef struct smb_packet {
    char data[SMB_PACKETSIZE];
    struct smb_packet *nextp;	        /* in free list, or whatever */
    long magic;
    cm_space_t *spacep;		        /* use this for stripping last component */
    NCB *ncbp;			        /* use this for sending */
    struct smb_vc *vcp;
    unsigned long resumeCode;
    unsigned short inCount;
    unsigned short fid;		        /* for calls bundled with openAndX */
    unsigned char *wctp;
    unsigned char inCom;
    unsigned char oddByte;
    unsigned short ncb_length;
    unsigned char flags;
    cm_space_t *stringsp;               /* decoded strings from this packet */
} smb_packet_t;

/* smb_packet flags */
#define SMB_PACKETFLAG_NOSEND			1
#define SMB_PACKETFLAG_SUSPENDED		2

/* a structure for making Netbios calls; locked by smb_globalLock */
#define SMB_NCBMAGIC	0x2334344
typedef struct myncb {
    NCB ncb;			        /* ncb to use */
    struct myncb *nextp;		/* when on free list */
    long magic;
} smb_ncb_t;

/* structures representing environments from kernel / SMB network.
 * Most have their own locks, but the tree connection fields and
 * reference counts are locked by the smb_rctLock.  Those fields will
 * be marked in comments.
 */

/* one per virtual circuit */
typedef struct smb_vc {
    struct smb_vc *nextp;		/* not used */
    afs_uint32 magic;			/* a magic value to detect bad entries */
    afs_int32 refCount;		        /* the reference count */
    afs_uint32 flags;		        /* the flags, if any; locked by mx */
    osi_mutex_t mx;			/* the mutex */
    afs_uint32 vcID;		        /* VC id */
    unsigned short lsn;		        /* the NCB LSN associated with this */
    unsigned short uidCounter;	        /* session ID counter */
    unsigned short tidCounter;	        /* tree ID counter */
    unsigned short fidCounter;	        /* file handle ID counter */
    struct smb_tid *tidsp;		/* the first child in the tid list */
    struct smb_user *usersp;	        /* the first child in the user session list */
    struct smb_fid *fidsp;		/* the first child in the open file list */
    unsigned char errorCount;
    clientchar_t rname[17];
    int lana;
    char encKey[MSV1_0_CHALLENGE_LENGTH]; /* MSV1_0_CHALLENGE_LENGTH is 8 */
    void * secCtx;                      /* security context when negotiating SMB extended auth
                                         * valid when SMB_VCFLAG_AUTH_IN_PROGRESS is set
                                         */
    unsigned short session;		/* This is the Session Index associated with the NCBs */
} smb_vc_t;

#define SMB_VC_MAGIC ('S' | 'C'<<8 | 'A'<<16 | 'C'<<24)
					/* have we negotiated ... */
#define SMB_VCFLAG_USEV3	1	/* ... version 3 of the protocol */
#define SMB_VCFLAG_USECORE	2	/* ... the core protocol */
#define SMB_VCFLAG_USENT	4	/* ... NT LM 0.12 or beyond */
#define SMB_VCFLAG_STATUS32	8	/* use 32-bit NT status codes */
#define SMB_VCFLAG_REMOTECONN	0x10	/* bad: remote conns not allowed */
#define SMB_VCFLAG_ALREADYDEAD	0x20	/* do not get tokens from this vc */
#define SMB_VCFLAG_SESSX_RCVD	0x40	/* we received at least one session setups on this vc */
#define SMB_VCFLAG_AUTH_IN_PROGRESS 0x80 /* a SMB NT extended authentication is in progress */
#define SMB_VCFLAG_CLEAN_IN_PROGRESS 0x100
#define SMB_VCFLAG_USEUNICODE   0x200   /* une UNICODE for STRING fields (NTLM 0.12 or later) */

/* one per user session */
typedef struct smb_user {
    struct smb_user *nextp;		/* next sibling */
    afs_int32 refCount;		        /* ref count */
    afs_uint32 flags;		        /* flags; locked by mx */
    osi_mutex_t mx;
    unsigned short userID;		/* the session identifier */
    struct smb_vc *vcp;		        /* back ptr to virtual circuit */
    struct smb_username *unp;           /* user name struct */
    afs_uint32	deleteOk;		/* ok to del: locked by smb_rctLock */
} smb_user_t;

#define SMB_USERFLAG_DELETE	    1	/* delete struct when ref count zero */

typedef struct smb_username {
    struct smb_username *nextp;		/* next sibling */
    afs_int32 refCount;		        /* ref count */
    long flags;			        /* flags; locked by mx */
    osi_mutex_t mx;
    struct cm_user *userp;		/* CM user structure */
    clientchar_t *name;                 /* user name */
    clientchar_t *machine;              /* machine name */
    time_t last_logoff_t;		/* most recent logoff time */
} smb_username_t;

/* The SMB_USERNAMEFLAG_AFSLOGON is used to preserve the existence of an
 * smb_username_t even when the refCount is zero.  This is used to ensure
 * that tokens set to a username during the integrated logon process are
 * preserved until the SMB Session that will require the tokens is created.
 * The cm_IoctlSetTokens() function when executed from the Network Provider
 * connects to the AFS Client Service using the credentials of the machine
 * and not the user for whom the tokens are being configured. */
#define SMB_USERNAMEFLAG_AFSLOGON   1

/* The SMB_USERNAMEFLAG_LOGOFF is used to indicate that the user most
 * recently logged off at 'last_logoff_t'.  The smb_username_t should not
 * be deleted even if the refCount is zero before 'last_logoff_t' +
 * 'smb_LogoffTransferTimeout' if 'smb_LogoffTokenTransfer' is non-zero.
 * The smb_Daemon() thread is responsible for purging the expired objects */

#define SMB_USERNAMEFLAG_LOGOFF     2

/*
 * The SMB_USERNAMEFLAG_SID flag indicates that the name is not a username
 * but a SID string.
 */
#define SMB_USERNAMEFLAG_SID        4

#define SMB_MAX_USERNAME_LENGTH 256

/* one per tree-connect */
typedef struct smb_tid {
    struct smb_tid *nextp;		/* next sibling */
    afs_int32 refCount;
    afs_uint32 flags;			/* protected by mx */
    osi_mutex_t mx;			/* for non-tree-related stuff */
    unsigned short tid;		        /* the tid */
    struct smb_vc *vcp;		        /* back ptr */
    struct cm_user *userp;		/* user logged in at the
					 * tree connect level (base) */
    clientchar_t *pathname;             /* pathname derived from sharename */
    afs_uint32	deleteOk;		/* ok to del: locked by smb_rctLock */
} smb_tid_t;

#define SMB_TIDFLAG_IPC		1 	/* IPC$ */

/* one per process ID */
typedef struct smb_pid {
    struct smb_pid *nextp;		/* next sibling */
    afs_int32 refCount;
    long flags;
    osi_mutex_t mx;			/* for non-tree-related stuff */
    unsigned short pid;		        /* the pid */
    struct smb_tid *tidp;		/* back ptr */
} smb_pid_t;


/* Defined in smb_ioctl.h */
struct smb_ioctl;

/* Defined in smb_rpc.h */
struct smb_rpc;

/* one per file ID; these are really file descriptors */
typedef struct smb_fid {
    osi_queue_t q;
    afs_int32 refCount;
    afs_uint32 flags;			/* protected by mx */
    osi_mutex_t mx;			/* for non-tree-related stuff */
    unsigned short fid;		        /* the file ID */
    struct smb_vc *vcp;		        /* back ptr */
    struct cm_scache *scp;		/* scache of open file */
    struct cm_user *userp;              /* user that opened the file
                                           originally (used to close
                                           the file if session is
                                           terminated) */
    osi_hyper_t offset;			/* our file pointer */
    struct smb_ioctl *ioctlp;           /* ptr to ioctl structure */
					/* Under NT, we may need to know the
					 * parent directory and pathname used
					 * to open the file, either to delete
					 * the file on close, or to do a
					 * change notification */
    struct smb_rpc   *rpcp;	        /* ptr to RPC structure.  Used
					   to keep track of endpoint
					   that was opened for the
					   RPC. */
    struct cm_scache *NTopen_dscp;	/* parent directory (NT) */
    clientchar_t *NTopen_pathp;		/* path used in open (NT) */
    clientchar_t *NTopen_wholepathp;	/* entire path, not just last name */
    int curr_chunk;			/* chunk being read */
    int prev_chunk;			/* previous chunk read */
    int raw_writers;		        /* pending async raw writes */
    EVENT_HANDLE raw_write_event;	/* signal this when raw_writers zero */
    afs_uint32	deleteOk;		/* ok to del: locked by smb_rctLock */
} smb_fid_t;

#define SMB_FID_OPENREAD_LISTDIR	1	/* open for reading / listing directory */
#define SMB_FID_OPENWRITE		2	/* open for writing */
#define SMB_FID_CREATED                 4       /* a new file */
#define SMB_FID_IOCTL			8	/* a file descriptor for the
						 * magic ioctl file */
#define SMB_FID_OPENDELETE		0x10	/* open for deletion (NT) */
#define SMB_FID_DELONCLOSE		0x20	/* marked for deletion */

/*
 * Now some special flags to work around a bug in NT Client
 */
#define SMB_FID_LENGTHSETDONE		0x40	/* have done 0-length write */
#define SMB_FID_MTIMESETDONE		0x80	/* have set modtime via Tr2 */
#define SMB_FID_LOOKSLIKECOPY	(SMB_FID_LENGTHSETDONE | SMB_FID_MTIMESETDONE)
#define SMB_FID_NTOPEN			0x100	/* have dscp and pathp */
#define SMB_FID_SEQUENTIAL		0x200
#define SMB_FID_RANDOM			0x400
#define SMB_FID_EXECUTABLE              0x800

#define SMB_FID_SHARE_READ              0x1000
#define SMB_FID_SHARE_WRITE             0x2000

#define SMB_FID_RPC                     0x4000	/* open for MS RPC */
#define SMB_FID_MESSAGEMODEPIPE		0x8000	/* message mode pipe */
#define SMB_FID_BLOCKINGPIPE	       0x10000	/* blocking pipe */
#define SMB_FID_RPC_INCALL	       0x20000	/* in an RPC call */

#define SMB_FID_QLOCK_HIGH              0x7f000000
#define SMB_FID_QLOCK_LOW               0x00000000
#define SMB_FID_QLOCK_LENGTH            1
#define SMB_FID_QLOCK_PID               0

/*
 * SMB file attributes (16-bit)
 */
#define SMB_ATTR_READONLY       0x0001
#define SMB_ATTR_HIDDEN         0x0002 /* hidden file for the purpose of dir listings */
#define SMB_ATTR_SYSTEM         0x0004
#define SMB_ATTR_VOLUMEID       0x0008 /* obsolete */
#define SMB_ATTR_DIRECTORY      0x0010
#define SMB_ATTR_ARCHIVE        0x0020
#define SMB_ATTR_DEVICE         0x0040

/* the following are Extended File Attributes (32-bit) */
#define SMB_ATTR_NORMAL         0x0080 /* normal file. Only valid if used alone */
#define SMB_ATTR_TEMPORARY      0x0100
#define SMB_ATTR_SPARSE_FILE    0x0200 /* used with dfs links */
#define SMB_ATTR_REPARSE_POINT  0x0400
#define SMB_ATTR_COMPRESSED     0x0800 /* file or dir is compressed */
#define SMB_ATTR_OFFLINE        0x1000
#define SMB_ATTR_NOT_CONTENT_INDEXED 0x2000
#define SMB_ATTR_ENCRYPTED      0x4000
#define SMB_ATTR_POSIX_SEMANTICS	0x01000000
#define SMB_ATTR_BACKUP_SEMANTICS	0x02000000
#define SMB_ATTR_DELETE_ON_CLOSE	0x04000000
#define SMB_ATTR_SEQUENTIAL_SCAN	0x08000000
#define SMB_ATTR_RANDOM_ACCESS		0x10000000
#define SMB_ATTR_NO_BUFFERING		0x20000000
#define SMB_ATTR_WRITE_THROUGH		0x80000000

#define LOCKING_ANDX_SHARED_LOCK        0x01 /* Read-only lock */
#define LOCKING_ANDX_OPLOCK_RELEASE     0x02 /* Oplock break notification */
#define LOCKING_ANDX_CHANGE_LOCKTYPE    0x04 /* Change lock type */
#define LOCKING_ANDX_CANCEL_LOCK        0x08 /* Cancel outstanding request */
#define LOCKING_ANDX_LARGE_FILES        0x10 /* Large file locking format */

/* File type constants */
#define SMB_FILETYPE_DISK		0x0000
#define SMB_FILETYPE_BYTE_MODE_PIPE	0x0001
#define SMB_FILETYPE_MESSAGE_MODE_PIPE	0x0002
#define SMB_FILETYPE_PRINTER		0x0003
#define SMB_FILETYPE_UNKNOWN		0xffff

/* Device state constants */
#define SMB_DEVICESTATE_READASBYTESTREAM    0x0000
#define SMB_DEVICESTATE_READMSGFROMPIPE	    0x0100
#define SMB_DEVICESTATE_BYTESTREAMPIPE	    0x0000
#define SMB_DEVICESTATE_MESSAGEMODEPIPE	    0x0400
#define SMB_DEVICESTATE_PIPECLIENTEND	    0x0000
#define SMB_DEVICESTATE_PIPESERVEREND	    0x4000
#define SMB_DEVICESTATE_BLOCKING	    0x8000

/* for tracking in-progress directory searches */
typedef struct smb_dirSearch {
    osi_queue_t q;			/* queue of all outstanding cookies */
    osi_mutex_t mx;			/* just in case the caller screws up */
    afs_int32 refCount;		        /* reference count */
    long cookie;			/* value returned to the caller */
    struct cm_scache *scp;		/* vnode of the dir we're searching */
    time_t lastTime;			/* last time we used this (osi_Time) */
    long flags;			        /* flags (see below);
					 * locked by smb_globalLock */
    unsigned short attribute;	        /* search attribute
					 * (used for extended protocol) */
    clientchar_t tidPath[256];          /* tid path */
    clientchar_t relPath[1024];         /* relative path */
    clientchar_t mask[256];	        /* search mask for V3 */
} smb_dirSearch_t;

#define SMB_DIRSEARCH_DELETE	1	/* delete struct when ref count zero */
#define SMB_DIRSEARCH_HITEOF	2	/* perhaps useful for advisory later */
#define SMB_DIRSEARCH_SMALLID	4	/* cookie can only be 8 bits, not 16 */
#define SMB_DIRSEARCH_BULKST	8	/* get bulk stat info */

/* type for patching directory listings */
typedef struct smb_dirListPatch {
    osi_queue_t q;
    char *dptr;                         /* ptr to attr, time, data, sizel, sizeh */
    long flags;                         /* flags.  See below */
    cm_fid_t fid;
    cm_dirEntry_t *dep;                 /* temp */
} smb_dirListPatch_t;

/* dirListPatch Flags */
#define SMB_DIRLISTPATCH_DOTFILE        1
/* the file referenced is a dot file
 * Note: will not be set if smb_hideDotFiles is false
 */
#define SMB_DIRLISTPATCH_IOCTL          2

/* individual lock on a waiting lock request */
typedef struct smb_waitingLock {
    osi_queue_t      q;
    cm_key_t         key;
    LARGE_INTEGER    LOffset;
    LARGE_INTEGER    LLength;
    cm_file_lock_t * lockp;
    int              state;
} smb_waitingLock_t;

#define SMB_WAITINGLOCKSTATE_WAITING   0
#define SMB_WAITINGLOCKSTATE_DONE      1
#define SMB_WAITINGLOCKSTATE_ERROR     2
#define SMB_WAITINGLOCKSTATE_CANCELLED 3

/* waiting lock request */
typedef struct smb_waitingLockRequest {
    osi_queue_t   q;
    smb_vc_t *    vcp;
    cm_scache_t * scp;
    smb_packet_t *inp;
    smb_packet_t *outp;
    int           lockType;
    time_t        start_t;              /* osi_Time */
    afs_uint32    msTimeout;            /* msecs, 0xFFFFFFFF = wait forever */
    smb_waitingLock_t * locks;
} smb_waitingLockRequest_t;

extern smb_waitingLockRequest_t *smb_allWaitingLocks;

typedef long (smb_proc_t)(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

typedef struct smb_dispatch {
    smb_proc_t *procp;	                /* proc to call */
    int flags;		                /* flags describing function */
} smb_dispatch_t;

#define SMB_DISPATCHFLAG_CHAINED	1	/* this is an _AND_X function */
#define SMB_DISPATCHFLAG_NORESPONSE	2	/* don't send the response
						 * packet, typically because
						 * the response was already
						 * sent.
                                                 */
#define SMB_MAX_PATH                    260     /* max path length */

/* prototypes */

extern void smb_Init(osi_log_t *logp, int useV3,
	int nThreads
        , void *aMBfunc
  );

extern void smb_DosUTimeFromUnixTime(afs_uint32 *dosUTimep, time_t unixTime);

extern void smb_UnixTimeFromDosUTime(time_t *unixTimep, afs_uint32 dosUTime);

extern void CompensateForSmbClientLastWriteTimeBugs(afs_uint32 *dosTimep);

#ifdef DEBUG_SMB_REFCOUNT
extern smb_vc_t *smb_FindVCDbg(unsigned short lsn, int flags, int lana, char *, long);
#define smb_FindVC(a,b,c) smb_FindVCDbg(a,b,c,__FILE__,__LINE__);

extern void smb_HoldVCDbg(smb_vc_t *vcp, char *, long);
#define smb_HoldVC(a) smb_HoldVCDbg(a,__FILE__,__LINE__);

extern void smb_HoldVCNoLockDbg(smb_vc_t *vcp, char *, long);
#define smb_HoldVCNoLock(a) smb_HoldVCNoLockDbg(a,__FILE__,__LINE__);

extern void smb_ReleaseVCDbg(smb_vc_t *vcp, char *, long);
#define smb_ReleaseVC(a) smb_ReleaseVCDbg(a,__FILE__,__LINE__);

extern void smb_ReleaseVCNoLockDbg(smb_vc_t *vcp, char *, long);
#define smb_ReleaseVCNoLock(a) smb_ReleaseVCNoLockDbg(a,__FILE__,__LINE__);
#else
extern smb_vc_t *smb_FindVC(unsigned short lsn, int flags, int lana);

extern void smb_HoldVC(smb_vc_t *vcp);

extern void smb_HoldVCNoLock(smb_vc_t *vcp);

extern void smb_ReleaseVC(smb_vc_t *vcp);

extern void smb_ReleaseVCNoLock(smb_vc_t *vcp);
#endif

extern void smb_CleanupDeadVC(smb_vc_t *vcp);

extern void smb_MarkAllVCsDead(smb_vc_t *exclude_vcp);

#ifdef DEBUG_SMB_REFCOUNT
extern smb_tid_t *smb_FindTIDDbg(smb_vc_t *vcp, unsigned short tid, int flags, char *, long);
#define smb_FindTID(a,b,c) smb_FindTIDDbg(a,b,c,__FILE__,__LINE__);

extern void smb_HoldTIDNoLockDbg(smb_tid_t *tidp, char *, long);
#define smb_HoldTIDNoLock(a) smb_HoldTIDNoLockDbg(a,__FILE__,__LINE__);

extern void smb_ReleaseTIDDbg(smb_tid_t *tidp, afs_uint32 locked, char *, long);
#define smb_ReleaseTID(a,b) smb_ReleaseTIDDbg(a,b,__FILE__,__LINE__);
#else
extern smb_tid_t *smb_FindTID(smb_vc_t *vcp, unsigned short tid, int flags);

extern void smb_HoldTIDNoLock(smb_tid_t *tidp);

extern void smb_ReleaseTID(smb_tid_t *tidp, afs_uint32 locked);
#endif

extern smb_user_t *smb_FindUID(smb_vc_t *vcp, unsigned short uid, int flags);

extern afs_int32 smb_userIsLocalSystem(smb_user_t *userp);

extern smb_username_t *smb_FindUserByName(clientchar_t *usern, clientchar_t *machine, afs_uint32 flags);

extern cm_user_t *smb_FindCMUserByName(clientchar_t *usern, clientchar_t *machine, afs_uint32 flags);

extern cm_user_t *smb_FindCMUserBySID(clientchar_t *usern, clientchar_t *machine, afs_uint32 flags);

extern smb_user_t *smb_FindUserByNameThisSession(smb_vc_t *vcp, clientchar_t *usern);

extern void smb_ReleaseUsername(smb_username_t *unp);

extern void smb_HoldUIDNoLock(smb_user_t *uidp);

extern void smb_ReleaseUID(smb_user_t *uidp);

extern cm_user_t *smb_GetUserFromVCP(smb_vc_t *vcp, smb_packet_t *inp);

extern cm_user_t *smb_GetUserFromUID(smb_user_t *uidp);

extern long smb_LookupTIDPath(smb_vc_t *vcp, unsigned short tid, clientchar_t ** tidPathp);

#ifdef DEBUG_SMB_REFCOUNT
extern smb_fid_t *smb_FindFIDDbg(smb_vc_t *vcp, unsigned short fid, int flags, char *, long);
#define smb_FindFID(a,b,c) smb_FindFIDDbg(a,b,c,__FILE__,__LINE__);

extern smb_fid_t *smb_FindFIDByScacheDbg(smb_vc_t *vcp, cm_scache_t * scp, char *, long);
#define smb_FindFIDByScache(a,b) smb_FindFIDByScacheDbg(a,b,__FILE__,__LINE__);

extern void smb_HoldFIDNoLockDbg(smb_fid_t *fidp, char *, long);
#define smb_HoldFIDNoLock(a) smb_HoldFIDNoLockDbg(a,__FILE__,__LINE__);

extern void smb_ReleaseFIDDbg(smb_fid_t *fidp, char *, long);
#define smb_ReleaseFID(a) smb_ReleaseFIDDbg(a,__FILE__,__LINE__);
#else
extern smb_fid_t *smb_FindFID(smb_vc_t *vcp, unsigned short fid, int flags);

extern smb_fid_t *smb_FindFIDByScache(smb_vc_t *vcp, cm_scache_t * scp);

extern void smb_HoldFIDNoLock(smb_fid_t *fidp);

extern void smb_ReleaseFID(smb_fid_t *fidp);
#endif

extern long smb_CloseFID(smb_vc_t *vcp, smb_fid_t *fidp, cm_user_t *userp,
                         afs_uint32 dosTime);

extern int smb_FindShare(smb_vc_t *vcp, smb_user_t *uidp, clientchar_t *shareName, clientchar_t **pathNamep);

extern int smb_FindShareCSCPolicy(clientchar_t *shareName);

extern smb_dirSearch_t *smb_FindDirSearchNL(long cookie);

extern void smb_DeleteDirSearch(smb_dirSearch_t *dsp);

extern void smb_ReleaseDirSearch(smb_dirSearch_t *dsp);

extern smb_dirSearch_t *smb_FindDirSearch(long cookie);

extern smb_dirSearch_t *smb_NewDirSearch(int isV3);

extern smb_packet_t *smb_CopyPacket(smb_packet_t *packetp);

extern void smb_FreePacket(smb_packet_t *packetp);

extern unsigned char *smb_GetSMBData(smb_packet_t *smbp, int *nbytesp);

extern void smb_SetSMBDataLength(smb_packet_t *smbp, unsigned int dsize);

extern unsigned short smb_GetSMBParm(smb_packet_t *smbp, int parm);

extern unsigned char smb_GetSMBParmByte(smb_packet_t *smbp, int parm);

extern unsigned int smb_GetSMBParmLong(smb_packet_t *smbp, int parm);

extern unsigned int smb_GetSMBOffsetParm(smb_packet_t *smbp, int parm, int offset);

extern void smb_SetSMBParm(smb_packet_t *smbp, int slot, unsigned int parmValue);

extern void smb_SetSMBParmLong(smb_packet_t *smbp, int slot, unsigned int parmValue);

extern void smb_SetSMBParmDouble(smb_packet_t *smbp, int slot, char *parmValuep);

extern void smb_SetSMBParmByte(smb_packet_t *smbp, int slot, unsigned int parmValue);

extern void smb_StripLastComponent(clientchar_t *outPathp, clientchar_t **lastComponentp,
	clientchar_t *inPathp);

#define SMB_STRF_FORCEASCII (1<<0)
#define SMB_STRF_ANSIPATH   (1<<1)
#define SMB_STRF_IGNORENUL  (1<<2)
#define SMB_STRF_SRCNULTERM (1<<3)

extern clientchar_t *smb_ParseASCIIBlock(smb_packet_t * pktp, unsigned char *inp,
                                         char **chainpp, int flags);

extern clientchar_t *smb_ParseString(smb_packet_t * pktp, unsigned char * inp,
                                     char ** chainpp, int flags);

extern clientchar_t *smb_ParseStringBuf(const unsigned char * bufbase,
                                        cm_space_t ** stringspp,
                                        unsigned char *inp, size_t *pcb_max,
                                        char **chainpp, int flags);

extern clientchar_t *smb_ParseStringCb(smb_packet_t * pktp, unsigned char * inp,
                                       size_t cb, char ** chainpp, int flags);

extern clientchar_t *smb_ParseStringCch(smb_packet_t * pktp, unsigned char * inp,
                                        size_t cch, char ** chainpp, int flags);

extern unsigned char * smb_UnparseString(smb_packet_t * pktp, unsigned char * outp,
                                   clientchar_t * str,
                                   size_t * plen, int flags);

extern unsigned char *smb_ParseVblBlock(unsigned char *inp, char **chainpp,
                                  int *lengthp);

extern smb_packet_t *smb_GetResponsePacket(smb_vc_t *vcp, smb_packet_t *inp);

extern void smb_SendPacket(smb_vc_t *vcp, smb_packet_t *inp);

extern void smb_MapCoreError(long code, smb_vc_t *vcp, unsigned short *scodep,
	unsigned char *classp);

extern void smb_MapNTError(long code, unsigned long *NTStatusp);

extern void smb_MapWin32Error(long code, unsigned long *Win32Ep);

/* some globals, too */
extern char *smb_localNamep;

extern osi_log_t *smb_logp;

extern osi_rwlock_t smb_globalLock;

extern osi_rwlock_t smb_rctLock;

extern int smb_LogoffTokenTransfer;
extern time_t smb_LogoffTransferTimeout;

extern int smb_maxVCPerServer; /* max # of VCs per server */
extern int smb_maxMpxRequests; /* max # of mpx requests */

extern int smb_StoreAnsiFilenames;
extern int smb_hideDotFiles;
extern unsigned int smb_IsDotFile(clientchar_t *lastComp);
extern afs_uint32 smb_AsyncStore;
extern afs_uint32 smb_AsyncStoreSize;

/* the following are used for smb auth */
extern int smb_authType; /* Type of SMB authentication to be used. One from below. */

#define SMB_AUTH_NONE 0
#define SMB_AUTH_NTLM 1
#define SMB_AUTH_EXTENDED 2

extern HANDLE smb_lsaHandle; /* LSA handle obtained during smb_init if using SMB auth */
extern ULONG smb_lsaSecPackage; /* LSA security package id. Set during smb_init */
extern clientchar_t smb_ServerDomainName[];
extern int smb_ServerDomainNameLength;
extern clientchar_t smb_ServerOS[];
extern int smb_ServerOSLength;
extern clientchar_t smb_ServerLanManager[];
extern int smb_ServerLanManagerLength;
extern GUID smb_ServerGUID;
extern LSA_STRING smb_lsaLogonOrigin;
extern LONG smb_UseUnicode;
extern DWORD smb_monitorReqs;

/* used for getting a challenge for SMB auth */
typedef struct _MSV1_0_LM20_CHALLENGE_REQUEST {
    MSV1_0_PROTOCOL_MESSAGE_TYPE MessageType;
} MSV1_0_LM20_CHALLENGE_REQUEST, *PMSV1_0_LM20_CHALLENGE_REQUEST;

typedef struct _MSV1_0_LM20_CHALLENGE_RESPONSE {
    MSV1_0_PROTOCOL_MESSAGE_TYPE MessageType;
    UCHAR ChallengeToClient[MSV1_0_CHALLENGE_LENGTH];
} MSV1_0_LM20_CHALLENGE_RESPONSE, *PMSV1_0_LM20_CHALLENGE_RESPONSE;
/**/

extern long smb_AuthenticateUserLM(smb_vc_t *vcp, clientchar_t * accountName, clientchar_t * primaryDomain, char * ciPwd, unsigned ciPwdLength, char * csPwd, unsigned csPwdLength);

extern long smb_GetNormalizedUsername(clientchar_t * usern, const clientchar_t * accountName, const clientchar_t * domainName);

extern void smb_FormatResponsePacket(smb_vc_t *vcp, smb_packet_t *inp,
	smb_packet_t *op);

extern char *myCrt_Dispatch(int i);

extern char *myCrt_2Dispatch(int i);

extern char *myCrt_RapDispatch(int i);

extern char * myCrt_NmpipeDispatch(int i);

extern unsigned int smb_Attributes(cm_scache_t *scp);

extern int smb_ChainFID(int fid, smb_packet_t *inp);

extern unsigned char *smb_ParseDataBlock(unsigned char *inp, char **chainpp, int *lengthp);

extern unsigned char *smb_ParseVblBlock(unsigned char *inp, char **chainpp, int *lengthp);

extern int smb_SUser(cm_user_t *userp);

long smb_WriteData(smb_fid_t *fidp, osi_hyper_t *offsetp, afs_uint32 count, char *op,
	cm_user_t *userp, long *writtenp);

extern long smb_ReadData(smb_fid_t *fidp, osi_hyper_t *offsetp, afs_uint32 count,
	char *op, cm_user_t *userp, long *readp);

extern long smb_Rename(smb_vc_t *vcp, smb_packet_t *inp, clientchar_t *oldPathp, clientchar_t *newPathp, int attrs);

extern long smb_Link(smb_vc_t *vcp, smb_packet_t *inp, clientchar_t *oldPathp, clientchar_t *newPathp);

extern BOOL smb_IsLegalFilename(clientchar_t *filename);

extern char *smb_GetSharename(void);

extern DWORD smb_ServerExceptionFilter(void);

extern void smb_RestartListeners(int);
extern void smb_StopListeners(int);
extern void smb_StopListener(NCB *ncbp, int lana, int wait);
extern long smb_IsNetworkStarted(void);
extern void smb_LanAdapterChange(int);
extern void smb_SetLanAdapterChangeDetected(void);

extern void smb_InitReq(cm_req_t *reqp);

#define SMB_LISTENER_UNINITIALIZED -1
#define SMB_LISTENER_STOPPED 0
#define SMB_LISTENER_STARTED 1

/* include other include files */
#include "smb3.h"
#include "smb_ioctl.h"
#include "smb_iocons.h"
#include "smb_rpc.h"
#include "cm_vnodeops.h"

extern int smb_unixModeDefaultFile;
extern int smb_unixModeDefaultDir;
extern void smb_SetInitialModeBitsForFile(int smb_attr, cm_attr_t * attr);
extern void smb_SetInitialModeBitsForDir(int smb_attr, cm_attr_t * attr);

cm_user_t *smb_FindOrCreateUser(smb_vc_t *vcp, clientchar_t *usern);

int smb_DumpVCP(FILE *outputFile, char *cookie, int lock);

void smb_Shutdown(void);

#ifdef NOTSERVICE
extern void smb_LogPacket(smb_packet_t *packet);
#endif /* NOTSERVICE */

#ifndef MSV1_0_OPTION_ALLOW_BLANK_PASSWORD
#define MSV1_0_OPTION_ALLOW_BLANK_PASSWORD      0x1
#define MSV1_0_OPTION_DISABLE_ADMIN_LOCKOUT     0x2
#define MSV1_0_OPTION_DISABLE_FORCE_GUEST       0x4
#define MSV1_0_OPTION_TRY_CACHE_FIRST           0x10

typedef struct _MSV1_0_SETPROCESSOPTION_REQUEST {
    MSV1_0_PROTOCOL_MESSAGE_TYPE MessageType;
    ULONG ProcessOptions;
    BOOLEAN DisableOptions;
} MSV1_0_SETPROCESSOPTION_REQUEST, *PMSV1_0_SETPROCESSOPTION_REQUEST;
#endif

#endif /* whole file */
