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

typedef struct smb_tran2QFSInfo {
    union {
#pragma pack(push, 2)
        struct {
            long FSID;			/* file system ID */
            long sectorsPerAllocUnit;
            long totalAllocUnits;		/* on the disk */
            long availAllocUnits;		/* free blocks */
            unsigned short bytesPerSector;	/* bytes per sector */
        } allocInfo;
#pragma pack(pop)
        struct {
            long vsn;	        /* volume serial number */
            char vnCount;	/* count of chars in label, incl null */
            char label[12];	/* pad out with nulls */
        } volumeInfo;
        struct {
            FILETIME vct;	/* volume creation time */
            long vsn;	        /* volume serial number */
            long vnCount;	/* length of volume label in bytes */
            char res[2];	/* reserved */
            char label[10];	/* volume label */
        } FSvolumeInfo;
        struct {
            osi_hyper_t totalAllocUnits;	/* on the disk */
            osi_hyper_t availAllocUnits;	/* free blocks */
            long sectorsPerAllocUnit;
            long bytesPerSector;		/* bytes per sector */
        } FSsizeInfo;
        struct {
            long devType;	/* device type */
            long characteristics;
        } FSdeviceInfo;
        struct {
            long attributes;
            long maxCompLength;	/* max path component length */
            long FSnameLength;	/* length of file system name */
            char FSname[12];
        } FSattributeInfo;
    } u;
} smb_tran2QFSInfo_t;

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

#ifdef DJGPP
#define DELETE (0x00010000)
#define READ_CONTROL (0x00020000)
#define SYNCHRONIZE (0x00100000)
#define FILE_WRITE_ATTRIBUTES ( 0x0100 )
#define FILE_GENERIC_READ (0x00120089)
#define FILE_GENERIC_WRITE (0x00120116)
#define FILE_GENERIC_EXECUTE (0x001200a0)
#endif /* DJGPP */

#endif /*  __SMB3_H_ENV__ */
