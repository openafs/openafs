/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#else
#include <sys/param.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <afs/afs_assert.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/afsint.h>
#include <afs/nfs.h>
#include <afs/errors.h>
#include <lock.h>
#include <lwp.h>
#include <afs/ihandle.h>
#include <afs/vnode.h>
#include <afs/volume.h>
#include <afs/partition.h>
#include "dump.h"
#include <afs/daemon_com.h>
#include <afs/fssync.h>
#include <afs/acl.h>
#include <afs/com_err.h>
#include <afs/vol_prototypes.h>
#include "volser.h"
#include "volint.h"
#include "dumpstuff.h"

#ifndef AFS_NT40_ENV
#ifdef O_LARGEFILE
#define afs_stat	stat64
#define afs_fstat	fstat64
#else /* !O_LARGEFILE */
#define afs_stat	stat
#define afs_fstat	fstat
#endif /* !O_LARGEFILE */
#endif /* !AFS_NT40_ENV */

/*@printflike@*/ extern void Log(const char *format, ...);

extern int DoLogging;
extern int DoPreserveVolumeStats;


/* Forward Declarations */
static int DumpDumpHeader(struct iod *iodp, Volume * vp,
			  afs_int32 fromtime);
static int DumpPartial(struct iod *iodp, Volume * vp,
		       afs_int32 fromtime, int dumpAllDirs);
static int DumpVnodeIndex(struct iod *iodp, Volume * vp,
			  VnodeClass class, afs_int32 fromtime,
			  int forcedump);
static int DumpVnode(struct iod *iodp, struct VnodeDiskObject *v,
		     int volid, int vnodeNumber, int dumpEverything);
static int ReadDumpHeader(struct iod *iodp, struct DumpHeader *hp);
static int ReadVnodes(struct iod *iodp, Volume * vp, int incremental,
		      afs_foff_t * Lbuf, afs_int32 s1, afs_foff_t * Sbuf,
		      afs_int32 s2, afs_int32 delo);
static afs_fsize_t volser_WriteFile(int vn, struct iod *iodp,
				    FdHandle_t * handleP, int tag,
				    Error * status);

static int SizeDumpDumpHeader(struct iod *iodp, Volume * vp,
			      afs_int32 fromtime,
			      struct volintSize *size);
static int SizeDumpPartial(struct iod *iodp, Volume * vp,
			   afs_int32 fromtime, int dumpAllDirs,
			   struct volintSize *size);
static int SizeDumpVnodeIndex(struct iod *iodp, Volume * vp,
			      VnodeClass class, afs_int32 fromtime,
			      int forcedump,
			      struct volintSize *size);
static int SizeDumpVnode(struct iod *iodp, struct VnodeDiskObject *v,
			 int volid, int vnodeNumber, int dumpEverything,
			 struct volintSize *size);

#define MAX_SECTIONS    3
#define MIN_TLV_TAG     5
#define MAX_TLV_TAG     0x60
#define MAX_STANDARD_TAG 0x7a
static afs_uint32 oldtags[MAX_SECTIONS][16];
int oldtagsInited = 0;

static void
RegisterTag(afs_int32 section, unsigned char tag)
{
    afs_uint32 off = tag >> 5;
    afs_uint32 mask = 1 << (tag & 0x1f);
    oldtags[section][off] |= mask;
}

static void
initNonStandardTags(void)
{
    RegisterTag(0, 'n');                /* volume name */
    RegisterTag(0, 't');                /* fromtime, V_backupDate */
    RegisterTag(1, 'A');                /* V_accessDate */
    RegisterTag(1, 'C');                /* V_creationDate */
    RegisterTag(1, 'D');                /* V_dayUseDate */
    RegisterTag(1, 'E');                /* V_expirationDate */
    RegisterTag(1, 'M');                /* nullstring (motd) */
    RegisterTag(1, 'U');                /* V_updateDate */
    RegisterTag(1, 'W');                /* V_weekUse */
    RegisterTag(1, 'Z');                /* V_dayUse */
    RegisterTag(1, 'O');                /* V_offlineMessage */
    RegisterTag(1, 'b');                /* V_blessed */
    RegisterTag(1, 'n');                /* V_name */
    RegisterTag(1, 's');                /* V_inService */
    RegisterTag(1, 't');                /* V_type */
    RegisterTag(2, 'A');                /* VVnodeDiskACL */
    RegisterTag(2, 'b');                /* modeBits */
    RegisterTag(2, 'f');                /* small file */
    RegisterTag(2, 'h');                /* large file */
    RegisterTag(2, 'l');                /* linkcount */
    RegisterTag(2, 't');                /* type */
    oldtagsInited = 1;
}

static void
iod_Init(struct iod *iodp, struct rx_call *call)
{
    iodp->call = call;
    iodp->haveOldChar = 0;
    iodp->ncalls = 1;
    iodp->calls = (struct rx_call **)0;
}

static void
iod_InitMulti(struct iod *iodp, struct rx_call **calls, int ncalls,
	      int *codes)
{

    iodp->calls = calls;
    iodp->haveOldChar = 0;
    iodp->ncalls = ncalls;
    iodp->codes = codes;
    iodp->call = (struct rx_call *)0;
}

/* N.B. iod_Read doesn't check for oldchar (see previous comment) */
#define iod_Read(iodp, buf, nbytes) rx_Read((iodp)->call, buf, nbytes)

/* For the single dump case, it's ok to just return the "bytes written"
 * that rx_Write returns, since all the callers of iod_Write abort when
 * the returned value is less than they expect.  For the multi dump case,
 * I don't think we want half the replicas to go bad just because one
 * connection timed out, but if they all time out, then we should give up.
 */
static int
iod_Write(struct iod *iodp, char *buf, int nbytes)
{
    int code, i;
    int one_success = 0;

    osi_Assert((iodp->call && iodp->ncalls == 1 && !iodp->calls)
	   || (!iodp->call && iodp->ncalls >= 1 && iodp->calls));

    if (iodp->call) {
	code = rx_Write(iodp->call, buf, nbytes);
	return code;
    }

    for (i = 0; i < iodp->ncalls; i++) {
	if (iodp->calls[i] && !iodp->codes[i]) {
	    code = rx_Write(iodp->calls[i], buf, nbytes);
	    if (code != nbytes) {	/* everything gets merged into a single error */
		iodp->codes[i] = VOLSERDUMPERROR;	/* but that's exactly what the */
	    } /* standard dump does, anyways */
	    else {
		one_success = TRUE;
	    }
	}
    }				/* for all calls */

    if (one_success)
	return nbytes;
    else
	return 0;
}

static void
iod_ungetc(struct iod *iodp, int achar)
{
    iodp->oldChar = achar;
    iodp->haveOldChar = 1;
}

static int
iod_getc(struct iod *iodp)
{
    unsigned char t;

    if (iodp->haveOldChar) {
	iodp->haveOldChar = 0;
	return iodp->oldChar;
    }
    if (iod_Read(iodp, (char *) &t, 1) == 1)
	return t;
    return EOF;
}

static int
ReadShort(struct iod *iodp, unsigned short *sp)
{
    int b1, b0;
    b1 = iod_getc(iodp);
    if (b1 == EOF)
	return 0;
    b0 = iod_getc(iodp);
    if (b0 == EOF)
	return 0;
    *sp = (b1 << 8) | b0;
    return 1;
}

static int
ReadInt32(struct iod *iodp, afs_uint32 * lp)
{
    afs_uint32 b3, b2, b1, b0;
    b3 = iod_getc(iodp);
    if (b3 == EOF)
	return 0;
    b2 = iod_getc(iodp);
    if (b2 == EOF)
	return 0;
    b1 = iod_getc(iodp);
    if (b1 == EOF)
	return 0;
    b0 = iod_getc(iodp);
    if (b0 == EOF)
	return 0;
    *lp = (((((b3 << 8) | b2) << 8) | b1) << 8) | b0;
    return 1;
}

static void
ReadString(struct iod *iodp, char *to, int maxa)
{
    int c;

    *to = '\0';
    if (maxa == 0)
	return;

    while (maxa--) {
	if ((*to++ = c = iod_getc(iodp)) == 0 || c == EOF)
	    break;
    }
    if (to[-1]) {
	while ((c = iod_getc(iodp)) && c != EOF);
	to[-1] = '\0';
    }
}

static int
ReadByteString(struct iod *iodp, byte * to, int size)
{
    int nbytes = 0;
    int c;

    while (size-- > 0 && (c = iod_getc(iodp)) != EOF) {
	*to++ = c;
	nbytes++;
    }
    return nbytes;
}

/*
 * returns 1 on success and 0 otherwise
 */
static afs_int32
ReadStandardTagLen(struct iod *iodp, unsigned char tag, afs_int32 section,
                        afs_size_t *length)
{
    afs_int32 code, i;
    afs_uint32 off = tag >> 5;
    afs_uint32 mask = 1 << (tag & 0x1f);
    int len;
    unsigned char buf[8], *p;

    if (!oldtagsInited)
        initNonStandardTags();

    if (tag < MIN_TLV_TAG
      || tag > MAX_STANDARD_TAG
      || section >= MAX_SECTIONS
      || (oldtags[section][ off] & mask)) {
        Log("Trying to use ReadStandardTag with tag 0x%02x for section %d, aborting\n", tag, section);
        return 0;
    }
    if (tag <= MAX_TLV_TAG) {
        len = iod_getc(iodp);
	if (len == EOF)
	    return VOLSERDUMPERROR;
	else if (len < 128)
            *length = len;
        else {
            len &= 0x7f;
            if ((code = iod_Read(iodp, (char *)buf, len)) != len)
                return VOLSERDUMPERROR;
            *length = 0;
            p = (unsigned char *)&buf;
            for (i=0; i<len; i++) {
                *length = ((*length) << 8) | *p++;
            }
        }
    } else {
        if (tag < MAX_STANDARD_TAG)
            *length = 4;
    }
    return 1;
}

static char skipbuf[256];

static afs_int32
SkipData(struct iod *iodp, afs_size_t length)
{
    while (length > 256) {
        if (iod_Read(iodp, (char *)&skipbuf, 256) != 256)
            return 0;
        length -= 256;
    }
    if (iod_Read(iodp, (char *)&skipbuf, length) != length)
        return 0;
    return 1;
}

static char *secname[3] = {"ReadDumpHeader", "ReadVolumeHeader", "ReadVnodes"};

static int
HandleUnknownTag(struct iod *iodp, int tag, afs_int32 section,
		afs_int32 critical)
{
    afs_size_t taglen = 0;
    afs_uint32 trash;

    if (critical) {
        Log("%s: unknown critical tag x%02x, aborting\n",
                secname[section], tag);
        return 0;
    }
    Log("%s: unknown tag x%02x found, skipping\n", secname[section], tag);
    if (tag >= 0x06 && tag <= 0x60) {
        if (!ReadStandardTagLen(iodp, tag, 1, &taglen)) {
            Log("%s: error reading length field for tag x%02x, aborting\n",
                secname[section], tag);
            return 0;
        }
        if (!SkipData(iodp, taglen)) {
            Log("%s: error skipping %llu bytes for tag x%02x, aborting\n",
                secname[section], taglen, tag);
            return 0;
        }
        return 1;
    }
    if (tag >= 0x61 && tag <= 0x7a) {
        if (!ReadInt32(iodp, &trash)) {
            Log("%s: error skipping int32 for tag x%02x, aborting\n",
                secname[section], tag);
            return 0;
        }
        return 1;
    }
    if (tag >= 0x7b && tag < 0x80)      /* dataless tag */
        return 1;
    Log("%s: unknown invalid tag x%02x, aborting\n", secname[section], tag);
    return 0;
}

static int
ReadVolumeHeader(struct iod *iodp, VolumeDiskData * vol)
{
    int tag;
    afs_uint32 trash;
    afs_int32 critical = 0;
    memset(vol, 0, sizeof(*vol));
    while ((tag = iod_getc(iodp)) > D_MAX && tag != EOF) {
        if (critical)
            critical--;
	switch (tag) {
	case 'i':
	    if (!ReadInt32(iodp, &vol->id))
		return VOLSERREAD_DUMPERROR;
	    break;
	case 'v':
	    if (!ReadInt32(iodp, &trash))
		return VOLSERREAD_DUMPERROR;
	    break;
	case 'n':
	    ReadString(iodp, vol->name, sizeof(vol->name));
	    /*this means the name of the retsored volume could be possibly different. In conjunction with SAFSVolSignalRestore */
	    break;
	case 's':
	    vol->inService = iod_getc(iodp);
	    break;
	case 'b':
	    vol->blessed = iod_getc(iodp);
	    break;
	case 'u':
	    if (!ReadInt32(iodp, &vol->uniquifier))
		return VOLSERREAD_DUMPERROR;
	    break;
	case 't':
	    vol->type = iod_getc(iodp);
	    break;
	case 'p':
	    if (!ReadInt32(iodp, &vol->parentId))
		return VOLSERREAD_DUMPERROR;
	    break;
	case 'c':
	    if (!ReadInt32(iodp, &vol->cloneId))
		return VOLSERREAD_DUMPERROR;
	    break;
	case 'q':
	    if (!ReadInt32(iodp, (afs_uint32 *) & vol->maxquota))
		return VOLSERREAD_DUMPERROR;
	    break;
	case 'm':
	    if (!ReadInt32(iodp, (afs_uint32 *) & vol->minquota))
		return VOLSERREAD_DUMPERROR;
	    break;
	case 'd':
	    if (!ReadInt32(iodp, (afs_uint32 *) & vol->diskused))
		return VOLSERREAD_DUMPERROR;	/* Bogus:  should calculate this */
	    break;
	case 'f':
	    if (!ReadInt32(iodp, (afs_uint32 *) & vol->filecount))
		return VOLSERREAD_DUMPERROR;
	    break;
	case 'a':
	    if (!ReadInt32(iodp, &vol->accountNumber))
		return VOLSERREAD_DUMPERROR;
	    break;
	case 'o':
	    if (!ReadInt32(iodp, &vol->owner))
		return VOLSERREAD_DUMPERROR;
	    break;
	case 'C':
	    if (!ReadInt32(iodp, &vol->creationDate))
		return VOLSERREAD_DUMPERROR;
	    break;
	case 'A':
	    if (!ReadInt32(iodp, &vol->accessDate))
		return VOLSERREAD_DUMPERROR;
	    break;
	case 'U':
	    if (!ReadInt32(iodp, &vol->updateDate))
		return VOLSERREAD_DUMPERROR;
	    break;
	case 'E':
	    if (!ReadInt32(iodp, &vol->expirationDate))
		return VOLSERREAD_DUMPERROR;
	    break;
	case 'B':
	    if (!ReadInt32(iodp, &vol->backupDate))
		return VOLSERREAD_DUMPERROR;
	    break;
	case 'O':
	    ReadString(iodp, vol->offlineMessage,
		       sizeof(vol->offlineMessage));
	    break;
	case 'M':
	    /*
	     * Detailed volume statistics are never stored in dumps,
	     * so we just restore either the null string if this volume
	     * had already been set to store statistics, or the old motd
	     * contents otherwise.  It doesn't matter, since this field
	     * will soon get initialized anyway.
	     */
	    ReadString(iodp, (char *)(vol->stat_reads), VMSGSIZE);
	    break;
	case 'W':{
		unsigned short length;
		int i;
		afs_uint32 data;
		if (!ReadShort(iodp, &length))
		    return VOLSERREAD_DUMPERROR;
		for (i = 0; i < length; i++) {
		    if (!ReadInt32(iodp, &data))
			return VOLSERREAD_DUMPERROR;
		    if (i < sizeof(vol->weekUse) / sizeof(vol->weekUse[0]))
			vol->weekUse[i] = data;
		}
		break;
	    }
	case 'D':
	    if (!ReadInt32(iodp, &vol->dayUseDate))
		return VOLSERREAD_DUMPERROR;
	    break;
	case 'Z':
	    if (!ReadInt32(iodp, (afs_uint32 *) & vol->dayUse))
		return VOLSERREAD_DUMPERROR;
	    break;
	case 'V':
	    if (!ReadInt32(iodp, (afs_uint32 *) &trash/*volUpdateCounter*/))
		return VOLSERREAD_DUMPERROR;
	    break;
        case 0x7e:
            critical = 2;
            break;
        default:
            if (!HandleUnknownTag(iodp, tag, 1, critical))
                return VOLSERREAD_DUMPERROR;
	}
    }
    iod_ungetc(iodp, tag);
    return 0;
}

static int
DumpTag(struct iod *iodp, int tag)
{
    char p;

    p = tag;
    return ((iod_Write(iodp, &p, 1) == 1) ? 0 : VOLSERDUMPERROR);

}

static int
DumpByte(struct iod *iodp, char tag, byte value)
{
    char tbuffer[2];
    byte *p = (unsigned char *)tbuffer;
    *p++ = tag;
    *p = value;
    return ((iod_Write(iodp, tbuffer, 2) == 2) ? 0 : VOLSERDUMPERROR);
}

#define putint32(p, v)  *p++ = v>>24, *p++ = v>>16, *p++ = v>>8, *p++ = v
#define putshort(p, v) *p++ = v>>8, *p++ = v

static int
DumpDouble(struct iod *iodp, char tag, afs_uint32 value1,
	   afs_uint32 value2)
{
    char tbuffer[9];
    byte *p = (unsigned char *)tbuffer;
    *p++ = tag;
    putint32(p, value1);
    putint32(p, value2);
    return ((iod_Write(iodp, tbuffer, 9) == 9) ? 0 : VOLSERDUMPERROR);
}

static int
DumpInt32(struct iod *iodp, char tag, afs_uint32 value)
{
    char tbuffer[5];
    byte *p = (unsigned char *)tbuffer;
    *p++ = tag;
    putint32(p, value);
    return ((iod_Write(iodp, tbuffer, 5) == 5) ? 0 : VOLSERDUMPERROR);
}

static int
DumpArrayInt32(struct iod *iodp, char tag,
	       afs_uint32 * array, int nelem)
{
    char tbuffer[4];
    afs_uint32 v;
    int code = 0;
    byte *p = (unsigned char *)tbuffer;
    *p++ = tag;
    putshort(p, nelem);
    code = iod_Write(iodp, tbuffer, 3);
    if (code != 3)
	return VOLSERDUMPERROR;
    while (nelem--) {
	p = (unsigned char *)tbuffer;
	v = *array++;		/*this was register */

	putint32(p, v);
	code = iod_Write(iodp, tbuffer, 4);
	if (code != 4)
	    return VOLSERDUMPERROR;
    }
    return 0;
}

static int
DumpShort(struct iod *iodp, char tag, unsigned int value)
{
    char tbuffer[3];
    byte *p = (unsigned char *)tbuffer;
    *p++ = tag;
    *p++ = value >> 8;
    *p = value;
    return ((iod_Write(iodp, tbuffer, 3) == 3) ? 0 : VOLSERDUMPERROR);
}

static int
DumpBool(struct iod *iodp, char tag, unsigned int value)
{
    char tbuffer[2];
    byte *p = (unsigned char *)tbuffer;
    *p++ = tag;
    *p = value;
    return ((iod_Write(iodp, tbuffer, 2) == 2) ? 0 : VOLSERDUMPERROR);
}

static int
DumpString(struct iod *iodp, char tag, char *s)
{
    int n;
    int code = 0;
    code = iod_Write(iodp, &tag, 1);
    if (code != 1)
	return VOLSERDUMPERROR;
    n = strlen(s) + 1;
    code = iod_Write(iodp, s, n);
    if (code != n)
	return VOLSERDUMPERROR;
    return 0;
}

static int
DumpByteString(struct iod *iodp, char tag, byte * bs,
	       int nbytes)
{
    int code = 0;

    code = iod_Write(iodp, &tag, 1);
    if (code != 1)
	return VOLSERDUMPERROR;
    code = iod_Write(iodp, (char *)bs, nbytes);
    if (code != nbytes)
	return VOLSERDUMPERROR;
    return 0;
}

static afs_int32
DumpStandardTag(struct iod *iodp, char tag, afs_uint32 section)
{
    afs_int32 code;
    afs_uint32 off = tag >> 5;
    afs_uint32 mask = 1 << (tag & 0x1f);

    if (!oldtagsInited)
        initNonStandardTags();

    if (tag < MIN_TLV_TAG
      || tag > MAX_STANDARD_TAG
      || section >= MAX_SECTIONS
      || (oldtags[section][ off] & mask)) {
        Log("Trying to use DumpStandardTag with tag 0x%02x for section %d, aborting\n", tag, section);
        return VOLSERDUMPERROR;
    }
    code = iod_Write(iodp, &tag, 1);
    if (code != 1)
	return VOLSERDUMPERROR;
    return 0;
}

AFS_UNUSED
static afs_int32
DumpStandardTagLen(struct iod *iodp, char tag, afs_uint32 section,
                        afs_size_t length)
{
    char buf[10];
    char *p;
    afs_int32 code, len;

    if (tag < MIN_TLV_TAG || tag > MAX_TLV_TAG) {
        Log("Trying to use DumpStandardTagLen with tag 0x%02x for section %d, aborting\n", tag, section);
        return VOLSERDUMPERROR;
    }
    code = DumpStandardTag(iodp, tag, section);
    if (code)
        return code;
    p = &buf[9];
    if (length < 128) { /* byte after tag contains length */
        *p-- = length;
        len = 1;
    } else {            /* byte after tag contains length of length field | 0x80 */
        for (len=0; length; length=length >> 8) {
            *p-- = length;
            len++;
        }
        *p-- = len + 128;
        len += 1;
    }
    p++;
    code = iod_Write(iodp, p, len);
    if (code != len)
        return VOLSERDUMPERROR;
    return 0;
}

static int
DumpFile(struct iod *iodp, int vnode, FdHandle_t * handleP)
{
    int code = 0, error = 0;
    afs_int32 pad = 0;
    afs_foff_t offset = 0;
    afs_sfsize_t nbytes, howBig;
    ssize_t n;
    size_t howMany;
    afs_foff_t howFar = 0;
    byte *p;
    afs_uint32 hi, lo;
    afs_ino_str_t stmp;
#ifndef AFS_NT40_ENV
    struct afs_stat status;
#endif
#ifdef	AFS_AIX_ENV
#include <sys/statfs.h>
#if defined(AFS_AIX52_ENV)
    struct statfs64 tstatfs;
#else /* !AFS_AIX52_ENV */
    struct statfs tstatfs;
#endif /* !AFS_AIX52_ENV */
    int statfs_code;
#endif

#ifdef AFS_NT40_ENV
    howBig = _filelength(handleP->fd_fd);
    howMany = 4096;

#else
    afs_fstat(handleP->fd_fd, &status);
    howBig = status.st_size;

#ifdef	AFS_AIX_ENV
    /* Unfortunately in AIX valuable fields such as st_blksize are
     * gone from the stat structure.
     */
#if defined(AFS_AIX52_ENV)
    statfs_code = fstatfs64(handleP->fd_fd, &tstatfs);
#else /* !AFS_AIX52_ENV */
    statfs_code = fstatfs(handleP->fd_fd, &tstatfs);
#endif /* !AFS_AIX52_ENV */
    if (statfs_code != 0) {
        Log("DumpFile: fstatfs returned error code %d on descriptor %d\n", errno, handleP->fd_fd);
	return VOLSERDUMPERROR;
    }
    howMany = tstatfs.f_bsize;
#else
    howMany = status.st_blksize;
#endif /* AFS_AIX_ENV */
#endif /* AFS_NT40_ENV */


    SplitInt64(howBig, hi, lo);
    if (hi == 0L) {
	code = DumpInt32(iodp, 'f', lo);
    } else {
	code = DumpDouble(iodp, 'h', hi, lo);
    }
    if (code) {
	return VOLSERDUMPERROR;
    }

    p = malloc(howMany);
    if (!p) {
	Log("1 Volser: DumpFile: not enough memory to allocate %u bytes\n", (unsigned)howMany);
	return VOLSERDUMPERROR;
    }

    for (nbytes = howBig; (nbytes && !error); nbytes -= howMany) {
	if (nbytes < howMany)
	    howMany = nbytes;

	/* Read the data */
	n = FDH_PREAD(handleP, p, howMany, howFar);
	howFar += n;

	/* If read any good data and we null padded previously, log the
	 * amount that we had null padded.
	 */
	if ((n > 0) && pad) {
	    Log("1 Volser: DumpFile: Null padding file %d bytes at offset %lld\n", pad, (long long)offset);
	    pad = 0;
	}

	/* If didn't read enough data, null padd the rest of the buffer. This
	 * can happen if, for instance, the media has some bad spots. We don't
	 * want to quit the dump, so we start null padding.
	 */
	if (n < howMany) {
	    /* Record the read error */
	    if (n < 0) {
		n = 0;
		Log("1 Volser: DumpFile: Error reading inode %s for vnode %d: %s\n", PrintInode(stmp, handleP->fd_ih->ih_ino), vnode, afs_error_message(errno));
	    } else if (!pad) {
		Log("1 Volser: DumpFile: Error reading inode %s for vnode %d\n", PrintInode(stmp, handleP->fd_ih->ih_ino), vnode);
	    }

	    /* Pad the rest of the buffer with zeros. Remember offset we started
	     * padding. Keep total tally of padding.
	     */
	    memset(p + n, 0, howMany - n);
	    if (!pad)
		offset = (howBig - nbytes) + n;
	    pad += (howMany - n);

	    /* Now seek over the data we could not get. An error here means we
	     * can't do the next read.
	     */
	    howFar = (size_t)((howBig - nbytes) + howMany);
	}

	/* Now write the data out */
	if (iod_Write(iodp, (char *)p, howMany) != howMany)
	    error = VOLSERDUMPERROR;
#ifndef AFS_PTHREAD_ENV
	IOMGR_Poll();
#endif
    }

    if (pad) {			/* Any padding we hadn't reported yet */
	Log("1 Volser: DumpFile: Null padding file: %d bytes at offset %lld\n",
	    pad, (long long)offset);
    }

    free(p);
    return error;
}

static int
DumpVolumeHeader(struct iod *iodp, Volume * vp)
{
    int code = 0;
    static char nullString[1] = "";	/*The ``contents'' of motd */

    if (!code)
	code = DumpTag(iodp, D_VOLUMEHEADER);
    if (!code) {
	code = DumpInt32(iodp, 'i', V_id(vp));
    }
    if (!code)
	code = DumpInt32(iodp, 'v', V_stamp(vp).version);
    if (!code)
	code = DumpString(iodp, 'n', V_name(vp));
    if (!code)
	code = DumpBool(iodp, 's', V_inService(vp));
    if (!code)
	code = DumpBool(iodp, 'b', V_blessed(vp));
    if (!code)
	code = DumpInt32(iodp, 'u', V_uniquifier(vp));
    if (!code)
	code = DumpByte(iodp, 't', (byte) V_type(vp));
    if (!code) {
	code = DumpInt32(iodp, 'p', V_parentId(vp));
    }
    if (!code)
	code = DumpInt32(iodp, 'c', V_cloneId(vp));
    if (!code)
	code = DumpInt32(iodp, 'q', V_maxquota(vp));
    if (!code)
	code = DumpInt32(iodp, 'm', V_minquota(vp));
    if (!code)
	code = DumpInt32(iodp, 'd', V_diskused(vp));
    if (!code)
	code = DumpInt32(iodp, 'f', V_filecount(vp));
    if (!code)
	code = DumpInt32(iodp, 'a', V_accountNumber(vp));
    if (!code)
	code = DumpInt32(iodp, 'o', V_owner(vp));
    if (!code)
	code = DumpInt32(iodp, 'C', V_creationDate(vp));	/* Rw volume creation date */
    if (!code)
	code = DumpInt32(iodp, 'A', V_accessDate(vp));
    if (!code)
	code = DumpInt32(iodp, 'U', V_updateDate(vp));
    if (!code)
	code = DumpInt32(iodp, 'E', V_expirationDate(vp));
    if (!code)
	code = DumpInt32(iodp, 'B', V_backupDate(vp));	/* Rw volume backup clone date */
    if (!code)
	code = DumpString(iodp, 'O', V_offlineMessage(vp));
    /*
     * We do NOT dump the detailed volume statistics residing in the old
     * motd field, since we cannot tell from the info in a dump whether
     * statistics data has been put there.  Instead, we dump a null string,
     * just as if that was what the motd contained.
     */
    if (!code)
	code = DumpString(iodp, 'M', nullString);
    if (!code)
	code =
	    DumpArrayInt32(iodp, 'W', (afs_uint32 *) V_weekUse(vp),
			   sizeof(V_weekUse(vp)) / sizeof(V_weekUse(vp)[0]));
    if (!code)
	code = DumpInt32(iodp, 'D', V_dayUseDate(vp));
    if (!code)
	code = DumpInt32(iodp, 'Z', V_dayUse(vp));
    return code;
}

static int
DumpEnd(struct iod *iodp)
{
    return (DumpInt32(iodp, D_DUMPEND, DUMPENDMAGIC));
}

/* Guts of the dump code */

/* Dump a whole volume */
int
DumpVolume(struct rx_call *call, Volume * vp,
	   afs_int32 fromtime, int dumpAllDirs)
{
    struct iod iod;
    int code = 0;
    struct iod *iodp = &iod;
    iod_Init(iodp, call);

    if (!code)
	code = DumpDumpHeader(iodp, vp, fromtime);

    if (!code)
	code = DumpPartial(iodp, vp, fromtime, dumpAllDirs);

/* hack follows.  Errors should be handled quite differently in this version of dump than they used to be.*/
    if (rx_Error(iodp->call)) {
	Log("1 Volser: DumpVolume: Rx call failed during dump, error %d\n",
	    rx_Error(iodp->call));
	return VOLSERDUMPERROR;
    }
    if (!code)
	code = DumpEnd(iodp);

    return code;
}

/* Dump a volume to multiple places*/
int
DumpVolMulti(struct rx_call **calls, int ncalls, Volume * vp,
	     afs_int32 fromtime, int dumpAllDirs, int *codes)
{
    struct iod iod;
    int code = 0;
    iod_InitMulti(&iod, calls, ncalls, codes);

    if (!code)
	code = DumpDumpHeader(&iod, vp, fromtime);
    if (!code)
	code = DumpPartial(&iod, vp, fromtime, dumpAllDirs);
    if (!code)
	code = DumpEnd(&iod);
    return code;
}

/* A partial dump (no dump header) */
static int
DumpPartial(struct iod *iodp, Volume * vp,
	    afs_int32 fromtime, int dumpAllDirs)
{
    int code = 0;
    if (!code)
	code = DumpVolumeHeader(iodp, vp);
    if (!code)
	code = DumpVnodeIndex(iodp, vp, vLarge, fromtime, dumpAllDirs);
    if (!code)
	code = DumpVnodeIndex(iodp, vp, vSmall, fromtime, 0);
    return code;
}

static int
DumpVnodeIndex(struct iod *iodp, Volume * vp, VnodeClass class,
	       afs_int32 fromtime, int forcedump)
{
    int code = 0;
    struct VnodeClassInfo *vcp = &VnodeClassInfo[class];
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    StreamHandle_t *file;
    FdHandle_t *fdP;
    afs_sfsize_t size, nVnodes;
    int flag;
    int vnodeIndex;

    fdP = IH_OPEN(vp->vnodeIndex[class].handle);
    osi_Assert(fdP != NULL);
    file = FDH_FDOPEN(fdP, "r+");
    osi_Assert(file != NULL);
    size = OS_SIZE(fdP->fd_fd);
    osi_Assert(size != -1);
    nVnodes = (size / vcp->diskSize) - 1;
    if (nVnodes > 0) {
	osi_Assert((nVnodes + 1) * vcp->diskSize == size);
	osi_Assert(STREAM_ASEEK(file, vcp->diskSize) == 0);
    } else
	nVnodes = 0;
    for (vnodeIndex = 0;
	 nVnodes && STREAM_READ(vnode, vcp->diskSize, 1, file) == 1 && !code;
	 nVnodes--, vnodeIndex++) {
	flag = forcedump || (vnode->serverModifyTime >= fromtime);
	/* Note:  the >= test is very important since some old volumes may not have
	 * a serverModifyTime.  For an epoch dump, this results in 0>=0 test, which
	 * does dump the file! */
	if (!code)
	    code =
		DumpVnode(iodp, vnode, V_id(vp),
			  bitNumberToVnodeNumber(vnodeIndex, class), flag);
#ifndef AFS_PTHREAD_ENV
	if (!flag)
	    IOMGR_Poll();	/* if we dont' xfr data, but scan instead, could lose conn */
#endif
    }
    STREAM_CLOSE(file);
    FDH_CLOSE(fdP);
    return code;
}

static int
DumpDumpHeader(struct iod *iodp, Volume * vp,
	       afs_int32 fromtime)
{
    int code = 0;
    int UseLatestReadOnlyClone = 1;
    afs_int32 dumpTimes[2];
    iodp->device = vp->device;
    iodp->parentId = V_parentId(vp);
    iodp->dumpPartition = vp->partition;
    if (!code)
	code = DumpDouble(iodp, D_DUMPHEADER, DUMPBEGINMAGIC, DUMPVERSION);
    if (!code)
	code =
	    DumpInt32(iodp, 'v',
		      UseLatestReadOnlyClone ? V_id(vp) : V_parentId(vp));
    if (!code)
	code = DumpString(iodp, 'n', V_name(vp));
    dumpTimes[0] = fromtime;
    switch (V_type(vp)) {
    case readwriteVolume:
	dumpTimes[1] = V_updateDate(vp);	/* until last update */
	break;
    case readonlyVolume:
	dumpTimes[1] = V_creationDate(vp);	/* until clone was updated */
	break;
    case backupVolume:
	/* until backup was made */
	dumpTimes[1] = V_backupDate(vp) != 0 ? V_backupDate(vp) :
					       V_creationDate(vp);
	break;
    default:
	code = EINVAL;
    }
    if (!code)
	code = DumpArrayInt32(iodp, 't', (afs_uint32 *) dumpTimes, 2);
    return code;
}

static int
DumpVnode(struct iod *iodp, struct VnodeDiskObject *v, int volid,
	  int vnodeNumber, int dumpEverything)
{
    int code = 0;
    IHandle_t *ihP;
    FdHandle_t *fdP;

    if (!v || v->type == vNull)
	return code;
    if (!code)
	code = DumpDouble(iodp, D_VNODE, vnodeNumber, v->uniquifier);
    if (!dumpEverything)
	return code;
    if (!code)
	code = DumpByte(iodp, 't', (byte) v->type);
    if (!code)
	code = DumpShort(iodp, 'l', v->linkCount);	/* May not need this */
    if (!code)
	code = DumpInt32(iodp, 'v', v->dataVersion);
    if (!code)
	code = DumpInt32(iodp, 'm', v->unixModifyTime);
    if (!code)
	code = DumpInt32(iodp, 'a', v->author);
    if (!code)
	code = DumpInt32(iodp, 'o', v->owner);
    if (!code && v->group)
	code = DumpInt32(iodp, 'g', v->group);	/* default group is 0 */
    if (!code)
	code = DumpShort(iodp, 'b', v->modeBits);
    if (!code)
	code = DumpInt32(iodp, 'p', v->parent);
    if (!code)
	code = DumpInt32(iodp, 's', v->serverModifyTime);
    if (v->type == vDirectory) {
	code = acl_HtonACL(VVnodeDiskACL(v));
	if (code) {
	    Log("DumpVnode: Skipping invalid acl vnode %u (volume %i)\n",
		 vnodeNumber, volid);
	}
	if (!code)
	    code =
		DumpByteString(iodp, 'A', (byte *) VVnodeDiskACL(v),
			       VAclDiskSize(v));
    }
    if (VNDISK_GET_INO(v)) {
	afs_sfsize_t indexlen, disklen;
	IH_INIT(ihP, iodp->device, iodp->parentId, VNDISK_GET_INO(v));
	fdP = IH_OPEN(ihP);
	if (fdP == NULL) {
	    Log("1 Volser: DumpVnode: dump: Unable to open inode %llu for vnode %u (volume %i); not dumped, error %d\n", (afs_uintmax_t) VNDISK_GET_INO(v), vnodeNumber, volid, errno);
	    IH_RELEASE(ihP);
	    return VOLSERREAD_DUMPERROR;
	}
	VNDISK_GET_LEN(indexlen, v);
	disklen = FDH_SIZE(fdP);
	if (indexlen != disklen) {
	    FDH_REALLYCLOSE(fdP);
	    IH_RELEASE(ihP);
	    Log("DumpVnode: volume %lu vnode %lu has inconsistent length "
	        "(index %lu disk %lu); aborting dump\n",
	        (unsigned long)volid, (unsigned long)vnodeNumber,
	        (unsigned long)indexlen, (unsigned long)disklen);
	    return VOLSERREAD_DUMPERROR;
	}
	code = DumpFile(iodp, vnodeNumber, fdP);
	FDH_CLOSE(fdP);
	IH_RELEASE(ihP);
    }
    return code;
}


int
ProcessIndex(Volume * vp, VnodeClass class, afs_foff_t ** Bufp, int *sizep,
	     int del)
{
    int i, nVnodes, code;
    afs_foff_t offset;
    afs_foff_t *Buf;
    int cnt = 0;
    afs_sfsize_t size;
    StreamHandle_t *afile;
    FdHandle_t *fdP;
    struct VnodeClassInfo *vcp = &VnodeClassInfo[class];
    char buf[SIZEOF_LARGEDISKVNODE], zero[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;

    memset(zero, 0, sizeof(zero));	/* zero out our proto-vnode */
    fdP = IH_OPEN(vp->vnodeIndex[class].handle);
    if (fdP == NULL)
	return -1;
    afile = FDH_FDOPEN(fdP, "r+");
    if (del) {
	int cnt1 = 0;
	Buf = *Bufp;
	for (i = 0; i < *sizep; i++) {
	    if (Buf[i]) {
		cnt++;
		STREAM_ASEEK(afile, Buf[i]);
		code = STREAM_READ(vnode, vcp->diskSize, 1, afile);
		if (code == 1) {
		    if (vnode->type != vNull && VNDISK_GET_INO(vnode)) {
			cnt1++;
			if (DoLogging) {
			    Log("RestoreVolume %u Cleanup: Removing old vnode=%u inode=%llu size=unknown\n",
                     V_id(vp), bitNumberToVnodeNumber(i, class),
                     (afs_uintmax_t) VNDISK_GET_INO(vnode));
			}
			IH_DEC(V_linkHandle(vp), VNDISK_GET_INO(vnode),
			       V_parentId(vp));
			DOPOLL;
		    }
		    STREAM_ASEEK(afile, Buf[i]);
		    (void)STREAM_WRITE(zero, vcp->diskSize, 1, afile);	/* Zero it out */
		}
		Buf[i] = 0;
	    }
	}
	if (DoLogging) {
	    Log("RestoreVolume Cleanup: Removed %d inodes for volume %d\n",
		cnt1, V_id(vp));
	}
	STREAM_FLUSH(afile);	/* ensure 0s are on the disk */
	OS_SYNC(afile->str_fd);
    } else {
	size = OS_SIZE(fdP->fd_fd);
	osi_Assert(size != -1);
	nVnodes =
	    (size <=
	     vcp->diskSize ? 0 : size - vcp->diskSize) >> vcp->logSize;
	if (nVnodes > 0) {
	    Buf = malloc(nVnodes * sizeof(afs_foff_t));
	    if (Buf == NULL) {
		STREAM_CLOSE(afile);
		FDH_CLOSE(fdP);
		return -1;
	    }
	    memset(Buf, 0, nVnodes * sizeof(afs_foff_t));
	    STREAM_ASEEK(afile, offset = vcp->diskSize);
	    while (1) {
		code = STREAM_READ(vnode, vcp->diskSize, 1, afile);
		if (code != 1) {
		    break;
		}
		if (vnode->type != vNull && VNDISK_GET_INO(vnode)) {
		    Buf[(offset >> vcp->logSize) - 1] = offset;
		    cnt++;
		}
		offset += vcp->diskSize;
	    }
	    *Bufp = Buf;
	    *sizep = nVnodes;
	}
    }
    STREAM_CLOSE(afile);
    FDH_CLOSE(fdP);
    return 0;
}


int
RestoreVolume(struct rx_call *call, Volume * avp, int incremental,
	      struct restoreCookie *cookie)
{
    VolumeDiskData vol;
    struct DumpHeader header;
    afs_uint32 endMagic;
    Error error = 0, vupdate;
    Volume *vp;
    struct iod iod;
    struct iod *iodp = &iod;
    afs_foff_t *b1 = NULL, *b2 = NULL;
    int s1 = 0, s2 = 0, delo = 0, tdelo;
    int tag;
    VolumeDiskData saved_header;

    iod_Init(iodp, call);

    vp = avp;

    if (DoPreserveVolumeStats) {
	CopyVolumeStats(&V_disk(vp), &saved_header);
    }

    if (!ReadDumpHeader(iodp, &header)) {
	Log("1 Volser: RestoreVolume: Error reading header file for dump; aborted\n");
	return VOLSERREAD_DUMPERROR;
    }
    if (iod_getc(iodp) != D_VOLUMEHEADER) {
	Log("1 Volser: RestoreVolume: Volume header missing from dump; not restored\n");
	return VOLSERREAD_DUMPERROR;
    }
    if (ReadVolumeHeader(iodp, &vol) == VOLSERREAD_DUMPERROR)
	return VOLSERREAD_DUMPERROR;

    if (!delo)
	delo = ProcessIndex(vp, vLarge, &b1, &s1, 0);
    if (!delo)
	delo = ProcessIndex(vp, vSmall, &b2, &s2, 0);
    if (delo < 0) {
	Log("1 Volser: RestoreVolume: ProcessIndex failed; not restored\n");
	error = VOLSERREAD_DUMPERROR;
	goto out;
    }

    strncpy(vol.name, cookie->name, VOLSER_OLDMAXVOLNAME);
    vol.type = cookie->type;
    vol.cloneId = cookie->clone;
    vol.parentId = cookie->parent;

    V_needsSalvaged(vp) = 0;

    tdelo = delo;
    while (1) {
	if (ReadVnodes(iodp, vp, 0, b1, s1, b2, s2, tdelo)) {
	    error = VOLSERREAD_DUMPERROR;
	    goto clean;
	}
	tag = iod_getc(iodp);
	if (tag != D_VOLUMEHEADER)
	    break;

	if (ReadVolumeHeader(iodp, &vol) == VOLSERREAD_DUMPERROR) {
	    error = VOLSERREAD_DUMPERROR;
	    goto out;
	}
    }
    if (tag != D_DUMPEND || !ReadInt32(iodp, &endMagic)
	|| endMagic != DUMPENDMAGIC) {
	Log("1 Volser: RestoreVolume: End of dump not found; restore aborted\n");
	error = VOLSERREAD_DUMPERROR;
	goto clean;
    }


    if (iod_getc(iodp) != EOF) {
	Log("1 Volser: RestoreVolume: Unrecognized postamble in dump; restore aborted\n");
	error = VOLSERREAD_DUMPERROR;
	goto clean;
    }

    if (!delo) {
	delo = ProcessIndex(vp, vLarge, &b1, &s1, 1);
	if (!delo)
	    delo = ProcessIndex(vp, vSmall, &b2, &s2, 1);
	if (delo < 0) {
	    error = VOLSERREAD_DUMPERROR;
	    goto clean;
	}
    }

  clean:
    if (DoPreserveVolumeStats) {
	CopyVolumeStats(&saved_header, &vol);
    } else {
	ClearVolumeStats(&vol);
    }
    if (V_needsSalvaged(vp)) {
	/* needsSalvaged may have been set while we tried to write volume data.
	 * prevent it from getting overwritten. */
	vol.needsSalvaged = V_needsSalvaged(vp);
    }
    CopyVolumeHeader(&vol, &V_disk(vp));
    V_destroyMe(vp) = 0;
    VUpdateVolume(&vupdate, vp);
    if (vupdate) {
	Log("1 Volser: RestoreVolume: Unable to rewrite volume header; restore aborted\n");
	error = VOLSERREAD_DUMPERROR;
	goto out;
    }
  out:
    /* Free the malloced space above */
    if (b1)
	free((char *)b1);
    if (b2)
	free((char *)b2);
    return error;
}

static int
ReadVnodes(struct iod *iodp, Volume * vp, int incremental,
	   afs_foff_t * Lbuf, afs_int32 s1, afs_foff_t * Sbuf, afs_int32 s2,
	   afs_int32 delo)
{
    afs_int32 vnodeNumber;
    char buf[SIZEOF_LARGEDISKVNODE];
    int tag;
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    struct VnodeDiskObject oldvnode;
    int idx;
    VnodeClass class;
    struct VnodeClassInfo *vcp;
    IHandle_t *tmpH;
    FdHandle_t *fdP;
    Inode nearInode AFS_UNUSED;
    afs_int32 critical = 0;
    int nbytes;

    tag = iod_getc(iodp);
    V_pref(vp, nearInode);
    while (tag == D_VNODE) {
	int haveStuff = 0;
	int saw_f = 0;
	memset(buf, 0, sizeof(buf));
	if (!ReadInt32(iodp, (afs_uint32 *) & vnodeNumber))
	    break;

	if (!ReadInt32(iodp, &vnode->uniquifier))
	    return VOLSERREAD_DUMPERROR;

	while ((tag = iod_getc(iodp)) > D_MAX && tag != EOF) {
	    haveStuff = 1;
            if (critical)
                critical--;
	    switch (tag) {
	    case 't':
		vnode->type = (VnodeType) iod_getc(iodp);
		break;
	    case 'l':
		{
		    unsigned short tlc;
		    if (!ReadShort(iodp, &tlc))
			return VOLSERREAD_DUMPERROR;
		    vnode->linkCount = (signed int)tlc;
		}
		break;
	    case 'v':
		if (!ReadInt32(iodp, &vnode->dataVersion))
		    return VOLSERREAD_DUMPERROR;
		break;
	    case 'm':
		if (!ReadInt32(iodp, &vnode->unixModifyTime))
		    return VOLSERREAD_DUMPERROR;
		break;
	    case 's':
		if (!ReadInt32(iodp, &vnode->serverModifyTime))
		    return VOLSERREAD_DUMPERROR;
		break;
	    case 'a':
		if (!ReadInt32(iodp, &vnode->author))
		    return VOLSERREAD_DUMPERROR;
		break;
	    case 'o':
		if (!ReadInt32(iodp, &vnode->owner))
		    return VOLSERREAD_DUMPERROR;
		break;
	    case 'g':
		if (!ReadInt32(iodp, (afs_uint32 *) & vnode->group))
		    return VOLSERREAD_DUMPERROR;
		break;
	    case 'b':{
		    unsigned short modeBits;
		    if (!ReadShort(iodp, &modeBits))
			return VOLSERREAD_DUMPERROR;
		    vnode->modeBits = (unsigned int)modeBits;
		    break;
		}
	    case 'p':
		if (!ReadInt32(iodp, &vnode->parent))
		    return VOLSERREAD_DUMPERROR;
		break;
	    case 'A':
		nbytes = ReadByteString(iodp, (byte *) VVnodeDiskACL(vnode),
			       VAclDiskSize(vnode));
		if (nbytes != VAclDiskSize(vnode)) {
		    Log("ReadVnodes: could not read acl for vnode %lu in dump.\n",
			 (unsigned long)vnodeNumber);
		    return VOLSERREAD_DUMPERROR;
		}
		if (acl_NtohACL(VVnodeDiskACL(vnode)) != 0) {
		    Log("ReadVnodes: invalid acl for vnode %lu in dump.\n",
			 (unsigned long)vnodeNumber);
		    return VOLSERREAD_DUMPERROR;
		}
		break;
	    case 'h':
	    case 'f':{
		    Inode ino;
		    Error error;
		    afs_fsize_t vnodeLength;

		    if (saw_f) {
			Log("Volser: ReadVnodes: warning: ignoring duplicate "
			    "file entries for vnode %lu in dump\n",
			    (unsigned long)vnodeNumber);
			volser_WriteFile(vnodeNumber, iodp, NULL, tag, &error);
			break;
		    }
		    saw_f = 1;

		    tmpH =
			IH_CREATE_INIT(V_linkHandle(vp), V_device(vp),
				  VPartitionPath(V_partition(vp)), nearInode,
				  V_parentId(vp), vnodeNumber,
				  vnode->uniquifier, vnode->dataVersion);
		    if (!tmpH) {
			Log("1 Volser: ReadVnodes: IH_CREATE: %s - restore aborted\n",
                            afs_error_message(errno));
			V_needsSalvaged(vp) = 1;
			return VOLSERREAD_DUMPERROR;
		    }
		    ino = tmpH->ih_ino;
		    nearInode = ino;
		    VNDISK_SET_INO(vnode, ino);
		    fdP = IH_OPEN(tmpH);
		    if (fdP == NULL) {
			Log("1 Volser: ReadVnodes: IH_OPEN: %s - restore aborted\n",
                            afs_error_message(errno));
			IH_RELEASE(tmpH);
			V_needsSalvaged(vp) = 1;
			return VOLSERREAD_DUMPERROR;
		    }
		    vnodeLength =
			volser_WriteFile(vnodeNumber, iodp, fdP, tag, &error);
		    VNDISK_SET_LEN(vnode, vnodeLength);
		    FDH_REALLYCLOSE(fdP);
		    IH_RELEASE(tmpH);
		    if (error) {
			Log("1 Volser: ReadVnodes: IDEC inode %llu\n",
			    (afs_uintmax_t) ino);
			IH_DEC(V_linkHandle(vp), ino, V_parentId(vp));
			V_needsSalvaged(vp) = 1;
			return VOLSERREAD_DUMPERROR;
		    }
		    break;
		}
            case 0x7e:
                critical = 2;
                break;
            default:
                if (!HandleUnknownTag(iodp, tag, 2, critical))
                    return VOLSERREAD_DUMPERROR;
	    }
	}

	class = vnodeIdToClass(vnodeNumber);
	vcp = &VnodeClassInfo[class];

	/* Mark this vnode as in this dump - so we don't delete it later */
	if (!delo) {
	    idx = (vnodeIndexOffset(vcp, vnodeNumber) >> vcp->logSize) - 1;
	    if (class == vLarge) {
		if (Lbuf && (idx < s1))
		    Lbuf[idx] = 0;
	    } else {
		if (Sbuf && (idx < s2))
		    Sbuf[idx] = 0;
	    }
	}

	if (haveStuff) {
	    FdHandle_t *fdP = IH_OPEN(vp->vnodeIndex[class].handle);
	    if (fdP == NULL) {
		Log("1 Volser: ReadVnodes: Error opening vnode index: %s; restore aborted\n",
		    afs_error_message(errno));
		V_needsSalvaged(vp) = 1;
		return VOLSERREAD_DUMPERROR;
	    }
	    if (FDH_PREAD(fdP, &oldvnode, sizeof(oldvnode), vnodeIndexOffset(vcp, vnodeNumber)) ==
		sizeof(oldvnode)) {
		if (oldvnode.type != vNull && VNDISK_GET_INO(&oldvnode)) {
		    IH_DEC(V_linkHandle(vp), VNDISK_GET_INO(&oldvnode),
			   V_parentId(vp));
		}
	    }
	    vnode->vnodeMagic = vcp->magic;
	    if (FDH_PWRITE(fdP, vnode, vcp->diskSize, vnodeIndexOffset(vcp, vnodeNumber)) != vcp->diskSize) {
		Log("1 Volser: ReadVnodes: Error writing vnode index: %s; restore aborted\n",
		    afs_error_message(errno));
		FDH_REALLYCLOSE(fdP);
		V_needsSalvaged(vp) = 1;
		return VOLSERREAD_DUMPERROR;
	    }
	    FDH_CLOSE(fdP);
	}
    }
    iod_ungetc(iodp, tag);

    return 0;
}


/* called with disk file only.  Note that we don't have to worry about rx_Read
 * needing to read an ungetc'd character, since the ReadInt32 will have read
 * it instead.
 *
 * if handleP == NULL, don't write the file anywhere; just read and discard
 * the file contents
 */
static afs_fsize_t
volser_WriteFile(int vn, struct iod *iodp, FdHandle_t * handleP, int tag,
		 Error * status)
{
    afs_int32 code;
    ssize_t nBytes;
    afs_fsize_t filesize;
    afs_fsize_t written = 0;
    size_t size = 8192;
    afs_fsize_t nbytes;
    unsigned char *p;


    *status = 0;
#ifdef AFS_64BIT_ENV
    {
	afs_uint32 filesize_high = 0L, filesize_low = 0L;
	if (tag == 'h') {
	    if (!ReadInt32(iodp, &filesize_high)) {
		*status = 1;
		return 0;
	    }
	}
	if (!ReadInt32(iodp, &filesize_low)) {
	    *status = 1;
	    return 0;
	}
	FillInt64(filesize, filesize_high, filesize_low);
    }
#else /* !AFS_64BIT_ENV */
    if (!ReadInt32(iodp, &filesize)) {
	*status = 1;
	return (0);
    }
#endif /* !AFS_64BIT_ENV */
    p = (unsigned char *)malloc(size);
    if (p == NULL) {
	*status = 2;
	return (0);
    }
    for (nbytes = filesize; nbytes; nbytes -= size) {
	if (nbytes < size)
	    size = nbytes;

	if ((code = iod_Read(iodp, (char *) p, size)) != size) {
	    Log("1 Volser: WriteFile: Error reading dump file %d size=%llu nbytes=%u (%d of %u): %s; restore aborted\n", vn, (afs_uintmax_t) filesize, nbytes, code, (unsigned)size, afs_error_message(errno));
	    *status = 3;
	    break;
	}
	if (handleP) {
	    nBytes = FDH_PWRITE(handleP, p, size, written);
	    if (nBytes > 0)
		written += nBytes;
	    if (nBytes != size) {
		Log("1 Volser: WriteFile: Error writing (%u) bytes to vnode %d; %s; restore aborted\n", (int)(nBytes & 0xffffffff), vn, afs_error_message(errno));
		*status = 4;
		break;
	    }
	}
    }
    free(p);
    return (written);
}

static int
ReadDumpHeader(struct iod *iodp, struct DumpHeader *hp)
{
    int tag;
    afs_uint32 beginMagic;
    afs_int32 critical = 0;
    if (iod_getc(iodp) != D_DUMPHEADER || !ReadInt32(iodp, &beginMagic)
	|| !ReadInt32(iodp, (afs_uint32 *) & hp->version)
	|| beginMagic != DUMPBEGINMAGIC)
	return 0;
    hp->volumeId = 0;
    hp->nDumpTimes = 0;
    while ((tag = iod_getc(iodp)) > D_MAX) {
	unsigned short arrayLength;
	int i;
	if (critical)
	    critical--;
	switch (tag) {
	case 'v':
	    if (!ReadInt32(iodp, &hp->volumeId))
		return 0;
	    break;
	case 'n':
	    ReadString(iodp, hp->volumeName, sizeof(hp->volumeName));
	    break;
	case 't':
	    if (!ReadShort(iodp, &arrayLength))
		return 0;
	    hp->nDumpTimes = (arrayLength >> 1);
	    for (i = 0; i < hp->nDumpTimes; i++)
		if (!ReadInt32(iodp, (afs_uint32 *) & hp->dumpTimes[i].from)
		    || !ReadInt32(iodp, (afs_uint32 *) & hp->dumpTimes[i].to))
		    return 0;
	    break;
        case 0x7e:
            critical = 2;
            break;
        default:
            if (!HandleUnknownTag(iodp, tag, 0, critical))
                return VOLSERREAD_DUMPERROR;
	}
    }
    if (!hp->volumeId || !hp->nDumpTimes) {
	return 0;
    }
    iod_ungetc(iodp, tag);
    return 1;
}


/* ----- Below are the calls that calculate dump size ----- */

static int
SizeDumpVolumeHeader(struct iod *iodp, Volume * vp,
		     struct volintSize *v_size)
{
    int code = 0;
    static char nullString[1] = "";	/*The ``contents'' of motd */
    afs_uint64 addvar;

/*     if (!code) code = DumpTag(iodp, D_VOLUMEHEADER); */
    FillInt64(addvar,0, 1);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) {code = DumpInt32(iodp, 'i',V_id(vp));} */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'v',V_stamp(vp).version); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpString(iodp, 'n',V_name(vp)); */
    FillInt64(addvar,0, (2 + strlen(V_name(vp))));
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpBool(iodp, 's',V_inService(vp)); */
    FillInt64(addvar,0, 2);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpBool(iodp, 'b',V_blessed(vp)); */
    FillInt64(addvar,0, 2);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'u',V_uniquifier(vp)); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpByte(iodp, 't',(byte)V_type(vp)); */
    FillInt64(addvar,0, 2);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code){ code = DumpInt32(iodp, 'p',V_parentId(vp));} */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'c',V_cloneId(vp)); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'q',V_maxquota(vp)); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'm',V_minquota(vp)); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'd',V_diskused(vp)); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'f',V_filecount(vp)); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'a', V_accountNumber(vp)); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'o', V_owner(vp)); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'C',V_creationDate(vp));	/\* Rw volume creation date *\/ */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'A',V_accessDate(vp)); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'U',V_updateDate(vp)); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'E',V_expirationDate(vp)); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'B',V_backupDate(vp));		/\* Rw volume backup clone date *\/ */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpString(iodp, 'O',V_offlineMessage(vp)); */
    FillInt64(addvar,0, (2 + strlen(V_offlineMessage(vp))));
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     /\* */
/*      * We do NOT dump the detailed volume statistics residing in the old */
/*      * motd field, since we cannot tell from the info in a dump whether */
/*      * statistics data has been put there.  Instead, we dump a null string, */
/*      * just as if that was what the motd contained. */
/*      *\/ */
/*     if (!code) code = DumpString(iodp, 'M', nullString); */
    FillInt64(addvar,0, (2 + strlen(nullString)));
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpArrayInt32(iodp, 'W', (afs_uint32 *)V_weekUse(vp), sizeof(V_weekUse(vp))/sizeof(V_weekUse(vp)[0])); */
    FillInt64(addvar,0, (3 + 4 * (sizeof(V_weekUse(vp)) / sizeof(V_weekUse(vp)[0]))));
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'D', V_dayUseDate(vp)); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'Z', V_dayUse(vp)); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
    return code;
}

static int
SizeDumpEnd(struct iod *iodp, struct volintSize *v_size)
{
    int code = 0;
    afs_uint64 addvar;
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
    return code;
}

int
SizeDumpVolume(struct rx_call *call, Volume * vp,
	       afs_int32 fromtime, int dumpAllDirs,
	       struct volintSize *v_size)
{
    int code = 0;
    struct iod *iodp = (struct iod *)0;
/*    iod_Init(iodp, call); */

    if (!code)
	code = SizeDumpDumpHeader(iodp, vp, fromtime, v_size);
    if (!code)
	code = SizeDumpPartial(iodp, vp, fromtime, dumpAllDirs, v_size);
    if (!code)
	code = SizeDumpEnd(iodp, v_size);

    return code;
}

static int
SizeDumpDumpHeader(struct iod *iodp, Volume * vp,
		   afs_int32 fromtime, struct volintSize *v_size)
{
    int code = 0;
/*    int UseLatestReadOnlyClone = 1; */
/*    afs_int32 dumpTimes[2]; */
    afs_uint64 addvar;
/*    iodp->device = vp->device; */
/*    iodp->parentId = V_parentId(vp); */
/*    iodp->dumpPartition = vp->partition; */

    ZeroInt64(v_size->dump_size);	/* initialize the size */
/*     if (!code) code = DumpDouble(iodp, D_DUMPHEADER, DUMPBEGINMAGIC, DUMPVERSION); */
    FillInt64(addvar,0, 9);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'v', UseLatestReadOnlyClone? V_id(vp): V_parentId(vp)); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpString(iodp, 'n',V_name(vp)); */
    FillInt64(addvar,0, (2 + strlen(V_name(vp))));
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     dumpTimes[0] = fromtime; */
/*     dumpTimes[1] = V_backupDate(vp);	/\* Until the time the clone was made *\/ */
/*     if (!code) code = DumpArrayInt32(iodp, 't', (afs_uint32 *)dumpTimes, 2); */
    FillInt64(addvar,0, (3 + 4 * 2));
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
    return code;
}

static int
SizeDumpVnode(struct iod *iodp, struct VnodeDiskObject *v, int volid,
	      int vnodeNumber, int dumpEverything,
	      struct volintSize *v_size)
{
    int code = 0;
    afs_uint64 addvar;

    if (!v || v->type == vNull)
	return code;
/*     if (!code) code = DumpDouble(iodp, D_VNODE, vnodeNumber, v->uniquifier); */
    FillInt64(addvar,0, 9);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
    if (!dumpEverything)
	return code;
/*     if (!code)  code = DumpByte(iodp, 't',(byte)v->type); */
    FillInt64(addvar,0, 2);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpShort(iodp, 'l', v->linkCount); /\* May not need this *\/ */
    FillInt64(addvar,0, 3);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'v', v->dataVersion); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'm', v->unixModifyTime); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'a', v->author); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'o', v->owner); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code && v->group) code = DumpInt32(iodp, 'g', v->group);	/\* default group is 0 *\/ */
    if (v->group) {
	FillInt64(addvar,0, 5);
	AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
    }
/*     if (!code) code = DumpShort(iodp, 'b', v->modeBits); */
    FillInt64(addvar,0, 3);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 'p', v->parent); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
/*     if (!code) code = DumpInt32(iodp, 's', v->serverModifyTime); */
    FillInt64(addvar,0, 5);
    AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
    if (v->type == vDirectory) {
/*	acl_HtonACL(VVnodeDiskACL(v)); */
/* 	if (!code) code = DumpByteString(iodp, 'A', (byte *) VVnodeDiskACL(v), VAclDiskSize(v)); */
	FillInt64(addvar,0, (1 + VAclDiskSize(v)));
	AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
    }

    if (VNDISK_GET_INO(v)) {
	VNDISK_GET_LEN(addvar, v);
	if (v->vn_length_hi)
	    addvar += 9;
	else
	    addvar += 5;
	AddUInt64(v_size->dump_size, addvar, &v_size->dump_size);
    }
    return code;
}

/* A partial dump (no dump header) */
static int
SizeDumpPartial(struct iod *iodp, Volume * vp,
		afs_int32 fromtime, int dumpAllDirs,
		struct volintSize *v_size)
{
    int code = 0;
    if (!code)
	code = SizeDumpVolumeHeader(iodp, vp, v_size);
    if (!code)
	code =
	    SizeDumpVnodeIndex(iodp, vp, vLarge, fromtime, dumpAllDirs,
			       v_size);
    if (!code)
	code = SizeDumpVnodeIndex(iodp, vp, vSmall, fromtime, 0, v_size);
    return code;
}

static int
SizeDumpVnodeIndex(struct iod *iodp, Volume * vp, VnodeClass class,
		   afs_int32 fromtime, int forcedump,
		   struct volintSize *v_size)
{
    int code = 0;
    struct VnodeClassInfo *vcp = &VnodeClassInfo[class];
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    StreamHandle_t *file;
    FdHandle_t *fdP;
    afs_sfsize_t size, nVnodes;
    int flag;
    int vnodeIndex;

    fdP = IH_OPEN(vp->vnodeIndex[class].handle);
    osi_Assert(fdP != NULL);
    file = FDH_FDOPEN(fdP, "r+");
    osi_Assert(file != NULL);
    size = OS_SIZE(fdP->fd_fd);
    osi_Assert(size != -1);
    nVnodes = (size / vcp->diskSize) - 1;
    if (nVnodes > 0) {
	osi_Assert((nVnodes + 1) * vcp->diskSize == size);
	osi_Assert(STREAM_ASEEK(file, vcp->diskSize) == 0);
    } else
	nVnodes = 0;
    for (vnodeIndex = 0;
	 nVnodes && STREAM_READ(vnode, vcp->diskSize, 1, file) == 1 && !code;
	 nVnodes--, vnodeIndex++) {
	flag = forcedump || (vnode->serverModifyTime >= fromtime);
	/* Note:  the >= test is very important since some old volumes may not have
	 * a serverModifyTime.  For an epoch dump, this results in 0>=0 test, which
	 * does dump the file! */
	if (!code)
	    code =
		SizeDumpVnode(iodp, vnode, V_id(vp),
			      bitNumberToVnodeNumber(vnodeIndex, class), flag,
			      v_size);
    }
    STREAM_CLOSE(file);
    FDH_CLOSE(fdP);
    return code;
}
