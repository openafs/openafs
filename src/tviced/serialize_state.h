/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * demand attach fs
 * fileserver state serialization
 */

#ifndef _AFS_TVICED_SERIALIZE_STATE_H
#define _AFS_TVICED_SERIALIZE_STATE_H

#ifdef AFS_DEMAND_ATTACH_FS

#define FS_STATE_MAGIC 0x62FA841C
#define FS_STATE_VERSION 2

#define HOST_STATE_MAGIC 0x7B8C9DAE
#define HOST_STATE_VERSION 2

#define HOST_STATE_ENTRY_MAGIC 0xA8B9CADB

#define CALLBACK_STATE_MAGIC 0x89DE67BC
#define CALLBACK_STATE_VERSION 1

#define CALLBACK_STATE_TIMEOUT_MAGIC 0x99DD5511
#define CALLBACK_STATE_FEHASH_MAGIC 0x77BB33FF
#define CALLBACK_STATE_ENTRY_MAGIC 0x54637281

#define ACTIVE_VOLUME_STATE_MAGIC 0xAC7557CA
#define ACTIVE_VOLUME_STATE_VERSION 1

#define ACTIVE_VOLUME_STATE_AVEHASH_MAGIC 0xBADDF00D

#define HOST_STATE_VALID_WINDOW 1800 /* 30 minutes */

/*
 * on-disk structures
 */
struct disk_version_stamp {
    afs_uint32 magic;
    afs_uint32 version;
};

/* 1024 byte header structure */
struct fs_state_header {
    struct disk_version_stamp stamp;  /* version stamp */
    afs_uint32 timestamp;             /* timestamp of save */
    afs_uint32 sys_name;              /* sys name id for this machine */
    afsUUID server_uuid;              /* server's UUID */
    byte valid;                       /* whether header contents are valid */
    byte endianness;                  /* endianness sanity check (0 for LE, 1 for BE) */
    byte stats_detailed;              /* fs stats detailed sanity check */
    byte padding1[1];                 /* padding */
    afs_uint32 reserved1[23];         /* for expansion */
    afs_uint64 avol_offset;           /* offset of active volumes structure */
    afs_uint64 h_offset;              /* offset of host_state_header structure */
    afs_uint64 cb_offset;             /* offset of callback_state_header structure */
    afs_uint64 vlru_offset;           /* offset of vlru state structure */
    afs_uint32 reserved2[56];         /* for expansion */
    char server_version_string[128];  /* version string from AFS_component_version_number.c */
    afs_uint32 reserved3[128];        /* for expansion */
};

/*
 * host package serialization
 */

/* 256 byte header for the host state data */
struct host_state_header {
    struct disk_version_stamp stamp;  /* host state version stamp */
    afs_uint32 records;               /* number of stored host records */
    afs_uint32 index_max;             /* max index value encountered */
    afs_uint32 reserved[60];          /* for expansion */
};

/* 32 byte host entry header */
struct host_state_entry_header {
    afs_uint32 magic;         /* stamp */
    afs_uint32 len;           /* number of bytes in this record */
    afs_uint32 interfaces;    /* number of interfaces included in record */
    afs_uint32 hcps;          /* number of hcps entries in record */
    afs_uint32 reserved[4];
};

/* 36 byte host entry structure */
struct hostDiskEntry {
    afs_uint32 host;		/* IP address of host interface that is
				 * currently being used, in network
				 * byte order */
    afs_uint16 port;	        /* port address of host */
    afs_uint16 hostFlags;       /*  bit map */
    byte Console;		/* XXXX This host is a console */
    byte hcpsfailed;	        /* Retry the cps call next time */
    byte hcps_valid;            /* prlist_val not null */
#if FS_STATS_DETAILED
    byte InSameNetwork;	        /*Is host's addr in the same network as
				 * the File Server's? */
#else
    byte padding1[1];	        /* for padding */
#endif				/* FS_STATS_DETAILED */
    afs_uint32 hcps_len;        /* length of hcps */
    afs_uint32 LastCall;	/* time of last call from host */
    afs_uint32 ActiveCall;	/* time of any call but gettime */
    afs_uint32 cpsCall;		/* time of last cps call from this host */
    afs_uint32 cblist;		/* Call back list for this host */
    afs_uint32 index;           /* index for correlating w/ callback dumps */
};

/*
 * callback package serialization
 */

/* 512 byte header */
struct callback_state_header {
    struct disk_version_stamp stamp;    /* callback state version stamp */
    afs_uint32 nFEs;                    /* number of FileEntry records */
    afs_uint32 nCBs;                    /* number of CallBack records */
    afs_uint32 fe_max;                  /* max FileEntry index */
    afs_uint32 cb_max;                  /* max CallBack index */
    afs_int32 tfirst;                   /* first valid timeout */
    afs_uint32 reserved[115];           /* for expansion */
    afs_uint64 timeout_offset;          /* offset of timeout queue heads */
    afs_uint64 fehash_offset;           /* offset of file entry hash buckets */
    afs_uint64 fe_offset;               /* offset of first file entry */
};

/* 32 byte header */
struct callback_state_timeout_header {
    afs_uint32 magic;         /* magic number for timeout header */
    afs_uint32 len;           /* total length of header and timeout records */
    afs_uint32 records;       /* number of timeout records */
    afs_uint32 reserved[5];
};

/* 32 byte header */
struct callback_state_fehash_header {
    afs_uint32 magic;         /* magic number for fehash header */
    afs_uint32 len;           /* total length of header and fehash bucket heads */
    afs_uint32 records;       /* number of hash buckets */
    afs_uint32 reserved[5];
};

/* 32 byte header */
struct callback_state_entry_header {
    afs_uint32 magic;         /* magic number for FE entry */
    afs_uint32 len;           /* number of bytes in this record */
    afs_uint32 nCBs;          /* number of callbacks for this FE */
    afs_uint32 reserved[5];
};

struct FEDiskEntry {
    struct FileEntry fe;
    afs_uint32 index;
};

struct CBDiskEntry {
    struct CallBack cb;
    afs_uint32 index;
};

/*
 * active volumes state serialization
 *
 * these structures are meant to support
 * automated salvaging of active volumes
 * in the event of a fileserver crash
 */

/* 512 byte header */
struct active_volume_state_header {
    struct disk_version_stamp stamp;    /* callback state version stamp */
    afs_uint32 nAVEs;                   /* number of ActiveVolumeEntry records */
    afs_uint32 init_timestamp;          /* timestamp of AVE initialization */
    afs_uint32 update_timetamp;         /* timestamp of last AVE update */
    afs_uint32 reserved[119];           /* for expansion */
    afs_uint64 avehash_offset;          /* offset of active volume entry hash buckets */
    afs_uint64 ave_offset;              /* offset of first active volume entry */
};

/* 32 byte header */
struct active_volume_state_avehash_header {
    afs_uint32 magic;         /* magic number for avehash header */
    afs_uint32 len;           /* total length of header and avehash bucket heads */
    afs_uint32 records;       /* number of hash buckets */
    afs_uint32 reserved[5];
};

typedef afs_uint32 active_volume_state_avehash_entry;

/* active volume entry */
struct AVDiskEntry {
    afs_uint32 volume;
    afs_uint32 partition;
    afs_uint32 hash_next;
};


/*
 * dump runtime state
 */
struct idx_map_entry_t {
    afs_uint32 old_idx;                    /* host hash id from last runtime */
    afs_uint32 new_idx;                    /* host hash id for this runtime */
};


/* verification process sanity check constants
 *
 * make them fairly large so we don't get 
 * false positives 
 */
#define FS_STATE_H_MAX_UUID_HASH_CHAIN_LEN    100000     /* max elements in a host uuid-hash chain */
#define FS_STATE_H_MAX_ADDR_HASH_CHAIN_LEN    2000000    /* max elements in a host ipv4-hash chain */
#define FS_STATE_FE_MAX_HASH_CHAIN_LEN        100000     /* max elements in a FE fid-hash chain */
#define FS_STATE_FCB_MAX_LIST_LEN             100000     /* max elements in a per-FE CB list */
#define FS_STATE_HCB_MAX_LIST_LEN             100000     /* max elements in a per-host CB list */
#define FS_STATE_TCB_MAX_LIST_LEN             100000     /* max elements in a per-timeout CB list */


/*
 * main state serialization state structure
 */

struct fs_dump_state {
    enum {
	FS_STATE_DUMP_MODE,
	FS_STATE_LOAD_MODE
    } mode;
    struct {
	byte do_host_restore;              /* whether host restore should be done */
	byte some_steps_skipped;           /* whether some steps were skipped */
	byte warnings_generated;           /* whether any warnings were generated during restore */
    } flags;
    afs_fsize_t file_len;
    int fd;                                /* fd of the current dump file */
    int bail;                              /* non-zero if something went wrong */
    char * fn;                             /* name of the current dump file */
    struct {                               /* memory map of dump file */
	void * map;
	void * cursor;
	afs_foff_t offset;
	afs_fsize_t size;
    } mmap;
    struct fs_state_header * hdr;          /* main header */
    struct host_state_header * h_hdr;      /* header for host state data */
    struct callback_state_header * cb_hdr; /* header for callback state data */
    struct callback_state_timeout_header * cb_timeout_hdr;
    struct callback_state_fehash_header * cb_fehash_hdr;
    afs_uint64 eof_offset;                 /* current end of file offset */
    struct {
	int len;                           /* number of host entries in map */
	struct idx_map_entry_t * entries;
    } h_map;
    struct {
	int len;
	struct idx_map_entry_t * entries;
    } fe_map;
    struct {
	int len;
	struct idx_map_entry_t * entries;
    } cb_map;
};


/* prototypes */

/* serialize_state.c */
extern int fs_stateWrite(struct fs_dump_state * state,
			 void * buf, size_t len);
extern int fs_stateRead(struct fs_dump_state * state,
			void * buf, size_t len);
extern int fs_stateWriteV(struct fs_dump_state * state,
			  struct iovec * iov, int niov);
extern int fs_stateReadV(struct fs_dump_state * state,
			 struct iovec * iov, int niov);
extern int fs_stateSync(struct fs_dump_state * state);
extern int fs_stateWriteHeader(struct fs_dump_state * state,
			       afs_uint64 * offset,
			       void * hdr, size_t len);
extern int fs_stateReadHeader(struct fs_dump_state * state,
			      afs_uint64 * offset,
			      void * hdr, size_t len);
extern int fs_stateIncEOF(struct fs_dump_state * state,
			  afs_int32 len);
extern int fs_stateSeek(struct fs_dump_state * state,
			afs_uint64 * offset);

/* host.c */
extern int h_stateSave(struct fs_dump_state * state);
extern int h_stateRestore(struct fs_dump_state * state);
extern int h_stateRestoreIndices(struct fs_dump_state * state);
extern int h_stateVerify(struct fs_dump_state * state);
extern int h_OldToNew(struct fs_dump_state * state, afs_uint32 old, afs_uint32 * new);

/* callback.c */
extern int cb_stateSave(struct fs_dump_state * state);
extern int cb_stateRestore(struct fs_dump_state * state);
extern int cb_stateRestoreIndices(struct fs_dump_state * state);
extern int cb_stateVerify(struct fs_dump_state * state);
extern int cb_stateVerifyHCBList(struct fs_dump_state * state, struct host * host);
extern int fe_OldToNew(struct fs_dump_state * state, afs_uint32 old, afs_uint32 * new);
extern int cb_OldToNew(struct fs_dump_state * state, afs_uint32 old, afs_uint32 * new);

#endif /* AFS_DEMAND_ATTACH_FS */
#endif /* _AFS_TVICED_SERIALIZE_STATE_H */
