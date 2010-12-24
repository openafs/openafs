/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_WINNT_AFSD_SMB3_H
#define OPENAFS_WINNT_AFSD_SMB3_H 1

typedef struct smb_tran2Packet {
	osi_queue_t q;			/* queue of all packets */
        int com;			/* Trans or Trans2 (0x25 or 0x32) */
        int totalData;			/* total # of expected data bytes */
        int totalParms;			/* total # of expected parm bytes */
	int oldTotalParms;		/* initial estimate of parm bytes */
        int curData;			/* current # of received data bytes */
        int curParms;			/* current # of received parm bytes */
        int maxReturnData;		/* max # of returned data bytes */
        int maxReturnParms;		/* max # of returned parm bytes */
    int setupCount;			/* setup count field */
        int opcode;			/* subopcode we're handling */
        long flags;			/* flags */
        smb_vc_t *vcp;			/* virtual circuit we're dealing with */
        unsigned short tid;		/* tid to match */
        unsigned short mid;		/* mid to match */
        unsigned short pid;		/* pid to remember */
        unsigned short uid;		/* uid to remember */
	unsigned short res[6];		/* contains PidHigh */
    unsigned int error_code;		/* CM error code for the packet */
        unsigned short *parmsp;		/* parms */
        unsigned char *datap;		/* data bytes */
    int pipeCommand;			/* named pipe command code */
    int pipeParam;			/* pipe parameter, if there is one */
    clientchar_t *name;			/* contents of Name
					   field. Only used for Named
					   pipes */
        cm_space_t * stringsp;          /* decoded strings */
} smb_tran2Packet_t;

/* for flags field */
#define SMB_TRAN2PFLAG_ALLOC	1
#define SMB_TRAN2PFLAG_USEUNICODE  2

typedef struct smb_tran2Dispatch {
	long (*procp)(smb_vc_t *, smb_tran2Packet_t *, smb_packet_t *);
        long flags;
} smb_tran2Dispatch_t;

/* Data Structures that are written to or read from the wire directly
 * must be byte aligned (no padding).
 */
#pragma pack(push, 1)
typedef struct smb_tran2QFSInfo {
    union {
        struct {
            unsigned long FSID;			/* file system ID */
            unsigned long sectorsPerAllocUnit;
            unsigned long totalAllocUnits;	/* on the disk */
            unsigned long availAllocUnits;	/* free blocks */
            unsigned short bytesPerSector;	/* bytes per sector */
        } allocInfo;
        struct {
            unsigned long vsn;			/* volume serial number */
            char vnCount;			/* count of chars in label, incl null */
            char /* STRING */ label[24];        /* pad out with nulls */
        } volumeInfo;
        struct {
            FILETIME      vct;		/* volume creation time */
            unsigned long vsn;	        /* volume serial number */
            unsigned long vnCount;	/* length of volume label in bytes */
            char res[2];		/* reserved */
            char /* STRING */ label[20];	/* volume label */
        } FSvolumeInfo;
        struct {
            LARGE_INTEGER totalAllocUnits;	/* on the disk */
            LARGE_INTEGER availAllocUnits;	/* free blocks */
            unsigned long sectorsPerAllocUnit;
            unsigned long bytesPerSector;	/* bytes per sector */
        } FSsizeInfo;
        struct {
            unsigned long devType;		/* device type */
            unsigned long characteristics;
        } FSdeviceInfo;
        struct {
            unsigned long attributes;
            unsigned long maxCompLength;	/* max path component length */
            unsigned long FSnameLength;		/* length of file system name */
            unsigned char /* STRING */ FSname[24]; /* File system name */
        } FSattributeInfo;
    } u;
} smb_tran2QFSInfo_t;

typedef struct {
    union {
	struct {
	    unsigned long  creationDateTime;	/* SMB_DATE / SMB_TIME */
	    unsigned long  lastAccessDateTime;	/* SMB_DATE / SMB_TIME */
	    unsigned long  lastWriteDateTime;	/* SMB_DATE / SMB_TIME */
	    unsigned long  dataSize;
	    unsigned long  allocationSize;
	    unsigned short attributes;
	    unsigned long  eaSize;
	} QPstandardInfo;
	struct {
	    unsigned long  creationDateTime;	/* SMB_DATE / SMB_TIME */
	    unsigned long  lastAccessDateTime;	/* SMB_DATE / SMB_TIME */
	    unsigned long  lastWriteDateTime;	/* SMB_DATE / SMB_TIME */
	    unsigned long  dataSize;
	    unsigned long  allocationSize;
	    unsigned short attributes;
	    unsigned long  eaSize;
	} QPeaSizeInfo;
	struct {
	    unsigned short maxDataCount;
	    unsigned short eaErrorOffset;
	    unsigned long  listLength;
	    unsigned char  eaList[128];
	} QPeasFromListInfo;
	struct {
	    unsigned short maxDataCount;
	    unsigned short eaErrorOffset;
	    unsigned long  listLength;
	    unsigned char  eaList[128];
	} QPallEasInfo;
	struct {
	    FILETIME       creationTime;
	    FILETIME       lastAccessTime;
	    FILETIME       lastWriteTime;
	    FILETIME       changeTime;
	    unsigned long  attributes;
	    unsigned long  reserved;
	} QPfileBasicInfo;
	struct {
	    LARGE_INTEGER  allocationSize;
	    LARGE_INTEGER  endOfFile;
	    unsigned long  numberOfLinks;
	    unsigned char  deletePending;
	    unsigned char  directory;
	    unsigned short reserved;
	} QPfileStandardInfo;
	struct {
	    unsigned long  eaSize;
	} QPfileEaInfo;
	struct {
	    unsigned long  fileNameLength;
	    unsigned char  fileName[512]; /* STRING */
	} QPfileNameInfo;
	struct {
	    FILETIME       creationTime;
	    FILETIME       lastAccessTime;
	    FILETIME       lastWriteTime;
	    FILETIME       changeTime;
	    unsigned long  attributes;
	    LARGE_INTEGER  allocationSize;
	    LARGE_INTEGER  endOfFile;
	    unsigned long  numberOfLinks;
	    unsigned char  deletePending;
	    unsigned char  directory;
	    LARGE_INTEGER  indexNumber;
	    unsigned long  eaSize;
	    unsigned long  accessFlags;
	    LARGE_INTEGER  indexNumber2;
	    LARGE_INTEGER  currentByteOffset;
	    unsigned long  mode;
	    unsigned long  alignmentRequirement;
	    unsigned long  fileNameLength;
	    unsigned char  fileName[512]; /* STRING */
	} QPfileAllInfo;
	struct {
	    unsigned long  fileNameLength;
	    unsigned char  fileName[512]; /* STRING */
	} QPfileAltNameInfo;
	struct {
	    unsigned long  nextEntryOffset;
	    unsigned long  streamNameLength;
	    LARGE_INTEGER  streamSize;
	    LARGE_INTEGER  streamAllocationSize;
	    unsigned char  fileName[512]; /* STRING */
	} QPfileStreamInfo;
	struct {
	    LARGE_INTEGER  compressedFileSize;
	    unsigned short compressionFormat;
	    unsigned char  compressionUnitShift;
	    unsigned char  chuckShift;
	    unsigned char  clusterShift;
	    unsigned char  reserved[3];
	} QPfileCompressionInfo;
    } u;
} smb_tran2QPathInfo_t;

typedef struct {
    union {
	struct {
	    FILETIME 	   creationTime;
	    FILETIME       lastAccessTime;
	    FILETIME	   lastWriteTime;
	    FILETIME	   lastChangeTime;
	    unsigned long  attributes;
	} QFbasicInfo;
	struct {
	    LARGE_INTEGER  allocationSize;
	    LARGE_INTEGER  endOfFile;
	    unsigned long  numberOfLinks;
	    unsigned char  deletePending;
	    unsigned char  directory;
	} QFstandardInfo;
	struct {
	    unsigned long  eaSize;
	} QFeaInfo;
	struct {
	    unsigned long  fileNameLength;
	    unsigned char  fileName[512];
	} QFfileNameInfo;
	struct {
	    unsigned long  nextEntryOffset;
	    unsigned long  streamNameLength;
	    LARGE_INTEGER  streamSize;
	    LARGE_INTEGER  streamAllocationSize;
	    unsigned char  fileName[512]; /* STRING */
	} QFfileStreamInfo;
        struct {
            LARGE_INTEGER  creationTime;
	    LARGE_INTEGER  lastAccessTime;
	    LARGE_INTEGER  lastWriteTime;
	    LARGE_INTEGER  lastChangeTime;
            LARGE_INTEGER  allocationSize;
            LARGE_INTEGER  endOfFile;
	    unsigned long  attributes;
        } QFnetworkOpenInfo;
    } u;
} smb_tran2QFileInfo_t;

typedef struct {
    unsigned long  creationDateTime;	/* SMB_DATE / SMB_TIME */
    unsigned long  lastAccessDateTime;	/* SMB_DATE / SMB_TIME */
    unsigned long  lastWriteDateTime;	/* SMB_DATE / SMB_TIME */
    unsigned long  dataSize;
    unsigned long  allocationSize;
    unsigned short attributes;
} smb_V3FileAttrsShort;

typedef struct {
    FILETIME       creationTime;
    FILETIME       lastAccessTime;
    FILETIME       lastWriteTime;
    FILETIME       lastChangeTime;
    LARGE_INTEGER  endOfFile;
    LARGE_INTEGER  allocationSize;
    unsigned long  extFileAttributes;
} smb_V3FileAttrsLong;

typedef struct {
    union {
	struct {
            smb_V3FileAttrsShort fileAttrs;
	    unsigned char  fileNameLength;
            /* STRING fileName */
	} FstandardInfo;

	struct {
            smb_V3FileAttrsShort fileAttrs;
	    unsigned long  eaSize;
            unsigned char  fileNameLength;
            /* STRING fileName */
	} FeaSizeInfo, FeasFromListInfo;

        struct {
            unsigned long  nextEntryOffset;
            unsigned long  fileIndex;
            smb_V3FileAttrsLong fileAttrs;
            unsigned long  fileNameLength;
            /* STRING fileName */
        } FfileDirectoryInfo;

        struct {
            unsigned long  nextEntryOffset;
            unsigned long  fileIndex;
            smb_V3FileAttrsLong fileAttrs;
            unsigned long  fileNameLength;
            unsigned long  eaSize;
            /* STRING fileName */
        } FfileFullDirectoryInfo;

        struct {
            unsigned long  nextEntryOffset;
            unsigned long  fileIndex;
            smb_V3FileAttrsLong fileAttrs;
            unsigned long  fileNameLength;
            unsigned long  eaSize;
            unsigned char  shortNameLength;
            unsigned char  reserved;
            wchar_t        shortName[12];
            /* STRING fileName */
        } FfileBothDirectoryInfo;

        struct {
            unsigned long  nextEntryOffset;
            unsigned long  fileIndex;
            unsigned long  fileNameLength;
            /* STRING fileName */
        } FfileNamesInfo;
    } u;
} smb_tran2Find_t;

#pragma pack(pop)

/* more than enough opcodes for today, anyway */
#define SMB_TRAN2_NOPCODES		20

extern smb_tran2Dispatch_t smb_tran2DispatchTable[SMB_TRAN2_NOPCODES];

#define SMB_RAP_NOPCODES	64

extern smb_tran2Dispatch_t smb_rapDispatchTable[SMB_RAP_NOPCODES];

extern smb_tran2Packet_t *smb_GetTran2ResponsePacket(smb_vc_t *vcp,
						     smb_tran2Packet_t *inp, smb_packet_t *outp,
						     int totalParms, int totalData);

extern void smb_FreeTran2Packet(smb_tran2Packet_t *t2p);

extern void smb_SendTran2Packet(smb_vc_t *vcp, smb_tran2Packet_t *t2p, smb_packet_t *tp);

extern long smb_ReceiveV3SessionSetupX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3TreeConnectX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3Trans(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3Tran2A(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveRAPNetShareEnum(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *op);

extern long smb_ReceiveRAPNetShareGetInfo(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *op);

extern long smb_ReceiveRAPNetWkstaGetInfo(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *op);

extern long smb_ReceiveRAPNetServerGetInfo(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *op);

extern long smb_ReceiveTran2Open(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2FindFirst(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2SearchDir(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2FindNext(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2QFSInfo(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2SetFSInfo(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2QFSInfoFid(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2QPathInfo(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2SetPathInfo(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2QFileInfo(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2SetFileInfo(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2FSCTL(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2IOCTL(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2FindNotifyFirst(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2FindNotifyNext(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2CreateDirectory(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2SessionSetup(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2GetDFSReferral(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2ReportDFSInconsistency(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveV3FindClose(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3FindNotifyClose(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3UserLogoffX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3OpenX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3LockingX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3GetAttributes(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3ReadX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3WriteX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3SetAttributes(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveNTCreateX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveNTTransact(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern void smb_NotifyChange(DWORD action, DWORD notifyFilter,
	cm_scache_t *dscp, clientchar_t *filename, clientchar_t *otherFilename,
	BOOL isDirectParent);

extern long smb_ReceiveNTCancel(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveNTRename(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern unsigned long smb_ExtAttributes(cm_scache_t *scp);

extern void smb3_Init();

/* SMB auth related functions */
extern void smb_NegotiateExtendedSecurity(void ** secBlob, int * secBlobLength);

/* Some of the FILE_NOTIFY_CHANGE values are undefined in winnt.h */
#define FILE_NOTIFY_CHANGE_EA           0x00000080
#define FILE_NOTIFY_CHANGE_STREAM_NAME  0x00000200
#define FILE_NOTIFY_CHANGE_STREAM_SIZE  0x00000400
#define FILE_NOTIFY_CHANGE_STREAM_WRITE 0x00000800

/* NT Create Device Status bit flags */
#define NO_REPARSETAG 0x0004
#define NO_SUBSTREAMS 0x0002
#define NO_EAS        0x0001

extern afs_uint32 smb_GetLogonSID(HANDLE hToken, PSID *ppsid);
extern afs_uint32 smb_GetUserSID(HANDLE hToken, PSID *ppsid);
extern void smb_FreeSID (PSID psid);

#endif /*  OPENAFS_WINNT_AFSD_SMB3_H */
