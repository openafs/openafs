/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __SMB3_H_ENV__
#define __SMB3_H_ENV__ 1

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
        int opcode;			/* subopcode we're handling */
        long flags;			/* flags */
        smb_vc_t *vcp;			/* virtual circuit we're dealing with */
        unsigned short tid;		/* tid to match */
        unsigned short mid;		/* mid to match */
        unsigned short pid;		/* pid to remember */
        unsigned short uid;		/* uid to remember */
	unsigned short res[6];		/* contains PidHigh */
        unsigned short *parmsp;		/* parms */
        unsigned char *datap;		/* data bytes */
} smb_tran2Packet_t;

/* for flags field */
#define SMB_TRAN2PFLAG_ALLOC	1

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
            char label[12];			/* pad out with nulls */
        } volumeInfo;
        struct {
            FILETIME      vct;		/* volume creation time */
            unsigned long vsn;	        /* volume serial number */
            unsigned long vnCount;	/* length of volume label in bytes */
            char res[2];		/* reserved */
            char label[10];		/* volume label */
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
            unsigned char FSname[12];
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
	    unsigned char  fileName[512];
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
	    unsigned char  fileName[512];
	} QPfileAllInfo;
	struct {
	    unsigned long  fileNameLength;
	    unsigned char  fileName[512];
	} QPfileAltNameInfo;
	struct {
	    unsigned long  nextEntryOffset;
	    unsigned long  streamNameLength;
	    LARGE_INTEGER  streamSize;
	    LARGE_INTEGER  streamAllocationSize;
	    unsigned char  fileName[512];
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
    } u;
} smb_tran2QFileInfo_t;
#pragma pack(pop)

/* more than enough opcodes for today, anyway */
#define SMB_TRAN2_NOPCODES		20

extern smb_tran2Dispatch_t smb_tran2DispatchTable[SMB_TRAN2_NOPCODES];

#define SMB_RAP_NOPCODES	64

extern smb_tran2Dispatch_t smb_rapDispatchTable[SMB_RAP_NOPCODES];

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
	cm_scache_t *dscp, char *filename, char *otherFilename,
	BOOL isDirectParent);

extern long smb_ReceiveNTCancel(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveNTRename(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern int smb_V3MatchMask(char *namep, char *maskp, int flags);

extern void smb3_Init();
extern cm_user_t *smb_FindCMUserByName(char *usern, char *machine, afs_uint32 flags);

/* SMB auth related functions */
extern void smb_NegotiateExtendedSecurity(void ** secBlob, int * secBlobLength);

#endif /*  __SMB3_H_ENV__ */
