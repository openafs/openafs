/*
 * (C) COPYRIGHT IBM CORPORATION 1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

/*===============================================================
 * Copyright (C) 1989 Transarc Corporation - All rights reserved 
 *===============================================================*/

#include <afs/param.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
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
#include <afs/assert.h>
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
#include <afs/fssync.h>
#include <afs/acl.h>
#include "volser.h"
#include "volint.h"

extern int DoLogging;

/* This iod stuff is a silly little package to emulate the old qi_in stuff, which
   emulated the stdio stuff.  There is a big assumption here, that the
   rx_Read will never be called directly, by a routine like readFile, when
   there is an old character that was pushed back with iod_ungetc.  This
   is really bletchy, and is here for compatibility only.  Eventually,
   we should define a volume format that doesn't require
   the pushing back of characters (i.e. characters should not double both
   as an end marker and a begin marker) */
struct iod {
    struct rx_call *call;	/* call to which to write, might be an array */
    int device;			/* dump device ID for volume */
    int parentId;		/* dump parent ID for volume */
    struct DiskPartition *dumpPartition; /* Dump partition. */
    struct rx_call **calls;      /* array of pointers to calls */
    int ncalls;                 /* how many calls/codes in array */
    int *codes;                 /* one return code for each call */
    char haveOldChar;		/* state for pushing back a character */
    char oldChar;
};


/* Forward Declarations */
static int DumpDumpHeader(register struct iod *iodp, register Volume *vp,
			  afs_int32 fromtime);
static int DumpPartial(register struct iod *iodp, register Volume *vp,
		       afs_int32 fromtime, int dumpAllDirs);
static int DumpVnodeIndex(register struct iod *iodp, Volume *vp,
			  VnodeClass class, afs_int32 fromtime, int forcedump);
static int DumpVnode(register struct iod *iodp, struct VnodeDiskObject *v,
		     int vnodeNumber, int dumpEverything);
static int ReadDumpHeader(register struct iod *iodp, struct DumpHeader *hp);
static int ReadVnodes(register struct iod *iodp, Volume *vp,
		      int incremental, afs_int32 *Lbuf, afs_int32 s1,
		      afs_int32 *Sbuf, afs_int32 s2, afs_int32 delo);
static bit32 volser_WriteFile(int vn, struct iod *iodp, FdHandle_t *handleP,
			      Error *status);


static void iod_Init(register struct iod *iodp, register struct rx_call *call)
{
    iodp->call = call;
    iodp->haveOldChar = 0;
    iodp->ncalls = 1;
    iodp->calls = (struct rx_call **) 0;
}

static void iod_InitMulti(struct iod *iodp, struct rx_call **calls, int ncalls,
		   int *codes)
{

  iodp->calls = calls;
  iodp->haveOldChar = 0;
  iodp->ncalls = ncalls;
  iodp->codes = codes;
  iodp->call = (struct rx_call *) 0;
}

/* N.B. iod_Read doesn't check for oldchar (see previous comment) */
#define iod_Read(iodp, buf, nbytes) rx_Read((iodp)->call, buf, nbytes)

/* For the single dump case, it's ok to just return the "bytes written"
 * that rx_Write returns, since all the callers of iod_Write abort when
 * the returned value is less than they expect.  For the multi dump case,
 * I don't think we want half the replicas to go bad just because one 
 * connection timed out, but if they all time out, then we should give up. 
 */
static int iod_Write(struct iod *iodp, char *buf, int nbytes)
{
  int code, i;
  int one_success = 0;

  assert ((iodp->call && iodp->ncalls == 1 && !iodp->calls) ||
	  (!iodp->call && iodp->ncalls >= 1 && iodp->calls));

  if (iodp->call) {
    code = rx_Write(iodp->call, buf, nbytes); 
    return code;     
  }

  for (i=0; i < iodp->ncalls; i++) {
    if (iodp->calls[i] && !iodp->codes[i]) {
      code = rx_Write(iodp->calls[i], buf, nbytes);
      if (code != nbytes) { /* everything gets merged into a single error */
      	iodp->codes[i] = VOLSERDUMPERROR;  /* but that's exactly what the */
      }       	 		           /* standard dump does, anyways */
      else {
      	one_success = TRUE;
      }
    }
  } /* for all calls */

if (one_success)
  return nbytes;
else return 0;
}

static void iod_ungetc(struct iod *iodp, int achar)
{
    iodp->oldChar = achar;
    iodp->haveOldChar = 1;
}

static int iod_getc(register struct iod *iodp)
{
    unsigned char t;
   
    if (iodp->haveOldChar) {
	iodp->haveOldChar = 0;
	return iodp->oldChar;
    }
    if (iod_Read(iodp, &t, 1) == 1) return t;
    return EOF;
}

static int ReadShort(register struct iod *iodp, register unsigned short *sp)
{
    register b1,b0;
    b1 = iod_getc(iodp);
    b0 = iod_getc(iodp);
    *sp = (b1<<8) | b0;
    return b0 != EOF;
}

static int ReadInt32(register struct iod *iodp, afs_uint32 *lp)
{
    afs_uint32 register b3,b2,b1,b0;
    b3 = iod_getc(iodp);
    b2 = iod_getc(iodp);
    b1 = iod_getc(iodp);
    b0 = iod_getc(iodp);
    *lp = (((((b3<<8)|b2)<<8)|b1)<<8)|b0;
    return b0 != EOF;
}

static void ReadString(register struct iod *iodp, register char *to,
		      register int maxa)
{
    register int c;
    while (maxa--) {
	if ((*to++ = iod_getc(iodp)) == 0)
	    break;
    }
    if (to[-1]) {
	while ((c = iod_getc(iodp)) && c != EOF);
	to[-1] = 0;
    }
}

static void ReadByteString(register struct iod *iodp, register byte *to,
		   register int size)
{
    while (size--)
	*to++ = iod_getc(iodp);
}

static int ReadVolumeHeader(register struct iod *iodp, VolumeDiskData *vol)
{
    register tag;
    afs_uint32 trash;
    bzero(vol, sizeof(*vol));
    while ((tag = iod_getc(iodp)) > D_MAX && tag != EOF) {
	switch (tag) {
	    case 'i':
		if(!ReadInt32(iodp, &vol->id)) return VOLSERREAD_DUMPERROR;
		break;
	    case 'v':
	        if(!ReadInt32(iodp, &trash)) return VOLSERREAD_DUMPERROR;
		break;
	    case 'n':
		 ReadString(iodp, vol->name, sizeof(vol->name));
		/*this means the name of the retsored volume could be possibly different. In conjunction with SAFSVolSignalRestore */
		break;
	    case 's':
		vol->inService = iod_getc(iodp );
		break;
	    case 'b':
		vol->blessed = iod_getc(iodp );
		break;
	    case 'u':
		if(!ReadInt32(iodp, &vol->uniquifier)) return VOLSERREAD_DUMPERROR;
		break;
	    case 't':
		vol->type = iod_getc(iodp );
		break;
	    case 'p':
	        if(!ReadInt32(iodp, &vol->parentId)) return VOLSERREAD_DUMPERROR;
		break;
	    case 'c':
	        if(!ReadInt32(iodp, &vol->cloneId)) return VOLSERREAD_DUMPERROR;
		break;
	    case 'q':
	        if(!ReadInt32(iodp, (afs_uint32 *)&vol->maxquota)) return VOLSERREAD_DUMPERROR;
		break;
	    case 'm':
		if(!ReadInt32(iodp, (afs_uint32 *)&vol->minquota)) return VOLSERREAD_DUMPERROR;
		break;
	    case 'd':
	        if(!ReadInt32(iodp, (afs_uint32 *)&vol->diskused)) return VOLSERREAD_DUMPERROR;	/* Bogus:  should calculate this */
		break;
	    case 'f':
		if(!ReadInt32(iodp, (afs_uint32 *)&vol->filecount)) return VOLSERREAD_DUMPERROR;
		break;
	    case 'a':
		if(!ReadInt32(iodp, &vol->accountNumber)) return VOLSERREAD_DUMPERROR;
		break;
	    case 'o':
	  	if(!ReadInt32(iodp, &vol->owner)) return VOLSERREAD_DUMPERROR;
		break;
	    case 'C':
		if(!ReadInt32(iodp, &vol->creationDate)) return VOLSERREAD_DUMPERROR;
		break;
	    case 'A':
		if(!ReadInt32(iodp, &vol->accessDate)) return VOLSERREAD_DUMPERROR;
		break;
	    case 'U':
	    	if(!ReadInt32(iodp, &vol->updateDate)) return VOLSERREAD_DUMPERROR;
		break;
	    case 'E':
	    	if(!ReadInt32(iodp, &vol->expirationDate)) return VOLSERREAD_DUMPERROR;
		break;
	    case 'B':
	    	if(!ReadInt32(iodp, &vol->backupDate)) return VOLSERREAD_DUMPERROR;
		break;
	    case 'O':
	    	ReadString(iodp, vol->offlineMessage, sizeof(vol->offlineMessage));
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
	    case 'W': {
		unsigned short length;
		int i;
    		afs_uint32 data;
	  	if(!ReadShort(iodp, &length)) return VOLSERREAD_DUMPERROR;
		for (i = 0; i<length; i++) {
		    if(!ReadInt32(iodp, &data)) return VOLSERREAD_DUMPERROR;
		    if (i < sizeof(vol->weekUse)/sizeof(vol->weekUse[0]))
			vol->weekUse[i] = data;
		}
		break;
	    }
	    case 'D':
		if(!ReadInt32(iodp, &vol->dayUseDate)) return VOLSERREAD_DUMPERROR;
		break;
	    case 'Z':
		if(!ReadInt32(iodp, (afs_uint32 *)&vol->dayUse)) return VOLSERREAD_DUMPERROR;
		break;
	}
    }
    iod_ungetc(iodp, tag);
    return 0;
}

static int DumpTag(register struct iod *iodp, register int tag)
{
    char p;
    
    p = tag;
    return((iod_Write(iodp, &p, 1) == 1)? 0 : VOLSERDUMPERROR);
    
}

static int DumpByte(register struct iod *iodp, char tag, byte value)
{
    char tbuffer[2];
    register byte *p = (unsigned char *) tbuffer;
    *p++ = tag;
    *p = value;
    return((iod_Write(iodp, tbuffer, 2) == 2)? 0: VOLSERDUMPERROR);
}

#define putint32(p, v)  *p++ = v>>24, *p++ = v>>16, *p++ = v>>8, *p++ = v
#define putshort(p, v) *p++ = v>>8, *p++ = v

static int DumpDouble(register struct iod *iodp, char tag,
		      register afs_uint32 value1, register afs_uint32 value2)
{
    char tbuffer[9];
    register byte *p = (unsigned char *) tbuffer;
    *p++ = tag;
    putint32(p, value1);
    putint32(p, value2);
    return((iod_Write(iodp, tbuffer, 9) == 9)? 0: VOLSERDUMPERROR);
}

static int DumpInt32(register struct iod *iodp, char tag,
		     register afs_uint32 value)
{
    char tbuffer[5];
    register byte *p = (unsigned char *) tbuffer;
    *p++ = tag;
    putint32(p, value);
    return((iod_Write(iodp, tbuffer, 5) == 5)? 0: VOLSERDUMPERROR);
}

static int DumpArrayInt32(register struct iod *iodp, char tag,
			  register afs_uint32 *array, register int nelem)
{
    char tbuffer[4];
    register afs_uint32 v ;
    int code = 0;
    register byte *p = (unsigned char *) tbuffer;
    *p++ = tag;
    putshort(p, nelem);
    code = iod_Write(iodp, tbuffer, 3);
    if (code != 3) return VOLSERDUMPERROR;
    while (nelem--) {
	p = (unsigned char *) tbuffer;
	v = *array++;/*this was register */
	
	putint32(p, v);
	code = iod_Write(iodp, tbuffer, 4);
	if(code != 4) return VOLSERDUMPERROR;
    }
    return 0;
}

static int DumpShort(register struct iod *iodp, char tag, unsigned int value)
{
    char tbuffer[3];
    register byte *p = (unsigned char *) tbuffer;
    *p++ = tag;
    *p++ = value>>8;
    *p = value;
    return((iod_Write(iodp, tbuffer, 3) == 3)? 0: VOLSERDUMPERROR);
}

static int DumpBool(register struct iod *iodp, char tag, unsigned int value)
{
    char tbuffer[2];
    register byte *p = (unsigned char *) tbuffer;
    *p++ = tag;
    *p = value;
    return((iod_Write(iodp, tbuffer, 2) == 2)? 0: VOLSERDUMPERROR);
}

static int DumpString(register struct iod *iodp, char tag, register char *s)
{
    register n;
    int code = 0;
    code = iod_Write(iodp, &tag, 1);
    if(code != 1) return VOLSERDUMPERROR;
    n = strlen(s)+1;
    code = iod_Write(iodp, s, n);
    if(code != n) return VOLSERDUMPERROR;
    return 0;
}

static int DumpByteString(register struct iod *iodp, char tag,
			  register byte *bs, register int nbytes)
{
    int code = 0;

    code = iod_Write(iodp, &tag, 1);
    if(code != 1) return VOLSERDUMPERROR;
    code = iod_Write(iodp, (char *)bs, nbytes);
    if(code != nbytes) return VOLSERDUMPERROR;
    return 0;
}
    
static int DumpFile(struct iod *iodp, char tag, int vnode, FdHandle_t *handleP)
{
    int   code = 0, lcode = 0, error = 0;
    afs_int32 pad = 0, offset;
    int   n, nbytes, howMany;
    byte  *p;
    struct stat status;
    int size;
#ifdef	AFS_AIX_ENV
#include <sys/statfs.h>
    struct statfs tstatfs;
#endif

#ifdef AFS_NT40_ENV
    howMany = 4096; /* Page size */
#else
    fstat(handleP->fd_fd, &status);
#ifdef	AFS_AIX_ENV
    /* Unfortunately in AIX valuable fields such as st_blksize are 
     * gone from the stat structure.
     */
    fstatfs(handleP->fd_fd, &tstatfs);
    howMany = tstatfs.f_bsize;
#else
    howMany = status.st_blksize;
#endif
#endif /* AFS_NT40_ENV */

    size = FDH_SIZE(handleP);
    code = DumpInt32(iodp, tag, size);
    if (code) {
       return VOLSERDUMPERROR;
    }

    p = (unsigned char *) malloc(howMany);
    if (!p) {
       Log("1 Volser: DumpFile: no memory");
       return VOLSERDUMPERROR;
    }

    for (nbytes = size; (nbytes && !error); nbytes -= howMany) {
	if (nbytes < howMany) 
	   howMany = nbytes;

	/* Read the data - unless we know we can't */
	n = (lcode ? 0 : FDH_READ(handleP, p, howMany));	

	/* If read any good data and we null padded previously, log the
	 * amount that we had null padded.
	 */
	if ((n > 0) && pad) {
	   Log("1 Volser: DumpFile: Null padding file %d bytes at offset %u\n",
	       pad, offset);
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
	      Log("1 Volser: DumpFile: Error %d reading inode %s for vnode %d\n", 
		  errno, PrintInode(NULL, handleP->fd_ih->ih_ino), vnode);
	   }
	   else if (!pad) {
	      Log("1 Volser: DumpFile: Error reading inode %s for vnode %d\n",
		  PrintInode(NULL, handleP->fd_ih->ih_ino), vnode);
	   }
	   
	   /* Pad the rest of the buffer with zeros. Remember offset we started 
	    * padding. Keep total tally of padding.
	    */
	   bzero(p+n, howMany-n);
	   if (!pad)
	      offset = (status.st_size - nbytes) + n;
	   pad += (howMany-n);
	   
	   /* Now seek over the data we could not get. An error here means we
	    * can't do the next read.
	    */
	   lcode = FDH_SEEK(handleP, ((size - nbytes) + howMany), SEEK_SET);
	   if (lcode != ((size - nbytes) + howMany)) {
	      if (lcode < 0) {
		 Log("1 Volser: DumpFile: Error %d seeking in inode %s for vnode %d\n",
		     errno, PrintInode(NULL, handleP->fd_ih->ih_ino), vnode);
	      }
	      else {
		 Log("1 Volser: DumpFile: Error seeking in inode %s for vnode %d\n",
		     PrintInode(NULL, handleP->fd_ih->ih_ino), vnode);
		 lcode = -1;
	      }
	   }
	   else {
	     lcode = 0;
	   }
	}

	/* Now write the data out */
	if (iod_Write(iodp, (char *)p, howMany) != howMany) 
	   error = VOLSERDUMPERROR;
	IOMGR_Poll();
    }

    if (pad) {                     /* Any padding we hadn't reported yet */
       Log("1 Volser: DumpFile: Null padding file: %d bytes at offset %u\n",
	   pad, offset);
    }

    free(p);
    return error;
}

static int DumpVolumeHeader(register struct iod *iodp, register Volume *vp)
{
    int code = 0;
    static char nullString[1] = "";  /*The ``contents'' of motd*/

    if (!code) code = DumpTag(iodp, D_VOLUMEHEADER);
    if (!code) {code = DumpInt32(iodp, 'i',V_id(vp));}
    if (!code) code = DumpInt32(iodp, 'v',V_stamp(vp).version);
    if (!code) code = DumpString(iodp, 'n',V_name(vp));
    if (!code) code = DumpBool(iodp, 's',V_inService(vp));
    if (!code) code = DumpBool(iodp, 'b',V_blessed(vp));
    if (!code) code = DumpInt32(iodp, 'u',V_uniquifier(vp));
    if (!code) code = DumpByte(iodp, 't',(byte)V_type(vp));
    if (!code){ code = DumpInt32(iodp, 'p',V_parentId(vp));}
    if (!code) code = DumpInt32(iodp, 'c',V_cloneId(vp));
    if (!code) code = DumpInt32(iodp, 'q',V_maxquota(vp));
    if (!code) code = DumpInt32(iodp, 'm',V_minquota(vp));
    if (!code) code = DumpInt32(iodp, 'd',V_diskused(vp));
    if (!code) code = DumpInt32(iodp, 'f',V_filecount(vp));
    if (!code) code = DumpInt32(iodp, 'a', V_accountNumber(vp));
    if (!code) code = DumpInt32(iodp, 'o', V_owner(vp));
    if (!code) code = DumpInt32(iodp, 'C',V_creationDate(vp));	/* Rw volume creation date */
    if (!code) code = DumpInt32(iodp, 'A',V_accessDate(vp));
    if (!code) code = DumpInt32(iodp, 'U',V_updateDate(vp));
    if (!code) code = DumpInt32(iodp, 'E',V_expirationDate(vp));
    if (!code) code = DumpInt32(iodp, 'B',V_backupDate(vp));		/* Rw volume backup clone date */
    if (!code) code = DumpString(iodp, 'O',V_offlineMessage(vp));
    /*
     * We do NOT dump the detailed volume statistics residing in the old
     * motd field, since we cannot tell from the info in a dump whether
     * statistics data has been put there.  Instead, we dump a null string,
     * just as if that was what the motd contained.
     */
    if (!code) code = DumpString(iodp, 'M', nullString);
    if (!code) code = DumpArrayInt32(iodp, 'W', (afs_uint32 *)V_weekUse(vp), sizeof(V_weekUse(vp))/sizeof(V_weekUse(vp)[0]));
    if (!code) code = DumpInt32(iodp, 'D', V_dayUseDate(vp));
    if (!code) code = DumpInt32(iodp, 'Z', V_dayUse(vp));
    return code;
}

static int DumpEnd(register struct iod *iodp)
{
    return(DumpInt32(iodp, D_DUMPEND, DUMPENDMAGIC));
}

/* Guts of the dump code */

/* Dump a whole volume */
int DumpVolume(register struct rx_call *call, register Volume *vp,
		      afs_int32 fromtime, int dumpAllDirs)
{
    struct iod iod;
    int code = 0;
    register struct iod *iodp = &iod;
    iod_Init(iodp, call);
    
    if (!code) code = DumpDumpHeader(iodp, vp, fromtime);
    
    if (!code) code = DumpPartial(iodp, vp, fromtime, dumpAllDirs);
    
/* hack follows.  Errors should be handled quite differently in this version of dump than they used to be.*/
    if (rx_Error(iodp->call)) {
	Log("1 Volser: DumpVolume: Rx call failed during dump, error %d\n", rx_Error(iodp->call));
	return VOLSERDUMPERROR;
    }
    if (!code) code = DumpEnd(iodp);
    
    return code;
}

/* Dump a volume to multiple places*/
int DumpVolMulti(struct rx_call **calls, int ncalls, Volume *vp,
		 afs_int32 fromtime, int dumpAllDirs, int *codes)
{
    struct iod iod;
    int code = 0;
    iod_InitMulti(&iod, calls, ncalls, codes);
    
    if (!code) code = DumpDumpHeader(&iod, vp, fromtime);
    if (!code) code = DumpPartial(&iod, vp, fromtime, dumpAllDirs);
    if (!code) code = DumpEnd(&iod);
    return code;
}

/* A partial dump (no dump header) */
static int DumpPartial(register struct iod *iodp, register Volume *vp,
		       afs_int32 fromtime, int dumpAllDirs)
{
    int code = 0;
    if (!code) code = DumpVolumeHeader(iodp, vp);
    if (!code) code = DumpVnodeIndex(iodp, vp, vLarge, fromtime, dumpAllDirs);
    if (!code) code = DumpVnodeIndex(iodp, vp, vSmall, fromtime, 0);
    return code;
}

static int DumpVnodeIndex(register struct iod *iodp, Volume *vp,
			  VnodeClass class, afs_int32 fromtime, int forcedump)
{
    register int code = 0;
    register struct VnodeClassInfo *vcp = &VnodeClassInfo[class];
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *) buf;
    StreamHandle_t *file;
    IHandle_t *handle;
    FdHandle_t *fdP;
    int size;
    int flag;
    register int vnodeIndex, nVnodes;

    fdP = IH_OPEN(vp->vnodeIndex[class].handle);
    assert(fdP != NULL);
    file = FDH_FDOPEN(fdP, "r+");
    assert(file != NULL);
    size = OS_SIZE(fdP->fd_fd);
    assert(size != -1);
    nVnodes = (size / vcp->diskSize) - 1;
    if (nVnodes > 0) {
	assert((nVnodes+1)*vcp->diskSize == size)
	assert(STREAM_SEEK(file, vcp->diskSize, 0) == 0)
    }
    else nVnodes = 0;
    for (vnodeIndex = 0; nVnodes && STREAM_READ(vnode, vcp->diskSize, 1, file) == 1 && !code;
      nVnodes--, vnodeIndex++) {
	flag = forcedump || (vnode->serverModifyTime >= fromtime);
	/* Note:  the >= test is very important since some old volumes may not have
  	   a serverModifyTime.  For an epoch dump, this results in 0>=0 test, which
	   does dump the file! */
	if (!code) code = DumpVnode(iodp, vnode, bitNumberToVnodeNumber(vnodeIndex, class), flag);
	if (!flag) IOMGR_Poll(); /* if we dont' xfr data, but scan instead, could lose conn */
    }
    STREAM_CLOSE(file);
    FDH_CLOSE(fdP);
    return code;
}

static int DumpDumpHeader(register struct iod *iodp, register Volume *vp,
			  afs_int32 fromtime)
{
    int code = 0;
    int UseLatestReadOnlyClone = 1;
    afs_int32 dumpTimes[2];
    iodp->device = vp->device;
    iodp->parentId = V_parentId(vp);
    iodp->dumpPartition = vp->partition;
    if (!code) code = DumpDouble(iodp, D_DUMPHEADER, DUMPBEGINMAGIC, DUMPVERSION);
    if (!code) code = DumpInt32(iodp, 'v', UseLatestReadOnlyClone? V_id(vp): V_parentId(vp));
    if (!code) code = DumpString(iodp, 'n',V_name(vp));
    dumpTimes[0] = fromtime;
    dumpTimes[1] = V_backupDate(vp);	/* Until the time the clone was made */
    if (!code) code = DumpArrayInt32(iodp, 't', (unsigned int *)dumpTimes, 2);
    return code;
}

static int DumpVnode(register struct iod *iodp, struct VnodeDiskObject *v,
		     int vnodeNumber, int dumpEverything)
{
    int code = 0;
    IHandle_t *ihP;
    FdHandle_t *fdP;

    if (!v || v->type == vNull)
	return code;
    if (!code) code = DumpDouble(iodp, D_VNODE, vnodeNumber, v->uniquifier);
    if (!dumpEverything)
        return code;
    if (!code) code = DumpByte(iodp, 't',(byte)v->type);
    if (!code) code = DumpShort(iodp, 'l', v->linkCount); /* May not need this */
    if (!code) code = DumpInt32(iodp, 'v', v->dataVersion);
    if (!code) code = DumpInt32(iodp, 'm', v->unixModifyTime);
    if (!code) code = DumpInt32(iodp, 'a', v->author);
    if (!code) code = DumpInt32(iodp, 'o', v->owner);
    if (!code && v->group) code = DumpInt32(iodp, 'g', v->group);	/* default group is 0 */
    if (!code) code = DumpShort(iodp, 'b', v->modeBits);
    if (!code) code = DumpInt32(iodp, 'p', v->parent);
    if (!code) code = DumpInt32(iodp, 's', v->serverModifyTime);
    if (v->type == vDirectory) {
	acl_HtonACL(VVnodeDiskACL(v));
	if (!code) code = DumpByteString(iodp, 'A', (byte *) VVnodeDiskACL(v), VAclDiskSize(v));
    }
    if (VNDISK_GET_INO(v)) {
	IH_INIT(ihP, iodp->device, iodp->parentId, VNDISK_GET_INO(v));
	fdP = IH_OPEN(ihP);
	if (fdP == NULL) {
	    Log("1 Volser: DumpVnode: dump: Unable to open inode %d for vnode %d; not dumped, error %d\n",
		VNDISK_GET_INO(v), vnodeNumber, errno);
	    IH_RELEASE(ihP);
	    return VOLSERREAD_DUMPERROR;
	}
	code = DumpFile(iodp, 'f', vnodeNumber, fdP);
	FDH_CLOSE(fdP);
	IH_RELEASE(ihP);
    }
    return code;
}


int ProcessIndex(Volume *vp, VnodeClass class, afs_int32 **Bufp, int *sizep,
		 int del)
{
    int i, nVnodes, offset, code, index=0;
    afs_int32 *Buf;
    int cnt=0;	
    int size;
    StreamHandle_t *afile;
    FdHandle_t *fdP;
    struct VnodeClassInfo *vcp = &VnodeClassInfo[class];
    char buf[SIZEOF_LARGEDISKVNODE], zero[SIZEOF_LARGEDISKVNODE];
    register struct VnodeDiskObject *vnode = (struct VnodeDiskObject *) buf;
    
    bzero(zero, sizeof(zero));	/* zero out our proto-vnode */
    fdP = IH_OPEN(vp->vnodeIndex[class].handle);
    if (fdP == NULL)
	return -1;
    afile = FDH_FDOPEN(fdP, "r+");
    if (del) {
	int cnt1=0;
	Buf = *Bufp;
	for (i = 0; i<*sizep; i++) {
	    if (Buf[i]) {
		cnt++;
		STREAM_SEEK(afile, Buf[i], 0);    
		code = STREAM_READ(vnode, vcp->diskSize, 1, afile);
		if (code == 1) {
		    if (vnode->type != vNull && VNDISK_GET_INO(vnode)) {
			cnt1++;
			if (DoLogging) {
			   Log("RestoreVolume %d Cleanup: Removing old vnode=%d inode=%d size=%d\n", 
			       V_id(vp), bitNumberToVnodeNumber(i,class),
			       VNDISK_GET_INO(vnode), vnode->length);
			}
			IH_DEC(V_linkHandle(vp), VNDISK_GET_INO(vnode),
			     V_parentId(vp));
			DOPOLL;
		    }
		    STREAM_SEEK(afile, Buf[i], 0);
		    (void ) STREAM_WRITE(zero, vcp->diskSize, 1, afile);	/* Zero it out */
		}
		Buf[i] = 0;
	    }
	}
	if (DoLogging) {
	   Log("RestoreVolume Cleanup: Removed %d inodes for volume %d\n",
	       cnt1, V_id(vp));
	}
	STREAM_FLUSH(afile);		/* ensure 0s are on the disk */
	OS_SYNC(afile->str_fd);
    } else {
	size = OS_SIZE(fdP->fd_fd);
	assert(size != -1)
	nVnodes = (size <= vcp->diskSize ? 0 :
		   size-vcp->diskSize) >> vcp->logSize;
	if (nVnodes > 0) {
	    Buf = (afs_int32 *) malloc(nVnodes * sizeof(afs_int32));
	    if (Buf == NULL) return 1;
	    bzero((char *)Buf, nVnodes * sizeof(afs_int32));
	    STREAM_SEEK(afile, offset = vcp->diskSize, 0);
	    while (1) {
		code = STREAM_READ(vnode, vcp->diskSize, 1, afile);
		if (code != 1) {
		    break;
		}
		if (vnode->type != vNull && VNDISK_GET_INO(vnode)) {
		    Buf[(offset >> vcp->logSize)-1] = offset;
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


int RestoreVolume(register struct rx_call *call, Volume *avp,
		  int incremental, struct restoreCookie *cookie)
{
    VolumeDiskData vol;
    struct DumpHeader header;
    afs_uint32 endMagic;
    Error error = 0, vupdate;
    register Volume *vp;
    struct iod iod;
    register struct iod *iodp = &iod;
    int *b1=0, *b2=0;
    int s1=0, s2=0, delo=0, tdelo;
    int tag;

    iod_Init(iodp, call);
    
    vp = avp;
    if (!ReadDumpHeader(iodp, &header)){
	Log("1 Volser: RestoreVolume: Error reading header file for dump; aborted\n");
	return VOLSERREAD_DUMPERROR;
    }
    if (iod_getc(iodp) != D_VOLUMEHEADER){
	Log("1 Volser: RestoreVolume: Volume header missing from dump; not restored\n");
	return VOLSERREAD_DUMPERROR;
    }
    if(ReadVolumeHeader(iodp, &vol) == VOLSERREAD_DUMPERROR) return VOLSERREAD_DUMPERROR;

    delo = ProcessIndex(vp, vLarge, &b1, &s1, 0);
    if (!delo) delo = ProcessIndex(vp, vSmall, &b2, &s2, 0);
    if (delo) {
       if (b1) free((char *)b1);
       if (b2) free((char *)b2);
       b1 = b2 = 0;
    }

    strncpy(vol.name,cookie->name,VOLSER_OLDMAXVOLNAME);
    vol.type = cookie->type;
    vol.cloneId = cookie->clone;
    vol.parentId = cookie->parent;
    

    tdelo = delo;
    while (1) {
       if (ReadVnodes(iodp, vp, 0, b1, s1, b2, s2, tdelo)) {
	  error =  VOLSERREAD_DUMPERROR;
	  goto clean;
       }
       tag = iod_getc(iodp);
       if (tag != D_VOLUMEHEADER)
	  break;
       if (ReadVolumeHeader(iodp, &vol) == VOLSERREAD_DUMPERROR) {
	  error = VOLSERREAD_DUMPERROR;
	  goto out;
       }
       tdelo = -1;
    }
    if (tag != D_DUMPEND || !ReadInt32(iodp, &endMagic) || endMagic != DUMPENDMAGIC){
       Log("1 Volser: RestoreVolume: End of dump not found; restore aborted\n");
       error =  VOLSERREAD_DUMPERROR;
       goto clean;
    }


    if (iod_getc(iodp) != EOF) {
	Log("1 Volser: RestoreVolume: Unrecognized postamble in dump; restore aborted\n");
	error = VOLSERREAD_DUMPERROR;
	goto clean;
    }

    if (!delo) {
	ProcessIndex(vp, vLarge, &b1, &s1, 1);
	ProcessIndex(vp, vSmall, &b2, &s2, 1);
    }

 clean:
    ClearVolumeStats(&vol);
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
    if (b1) free((char *)b1);
    if (b2) free((char *)b2);
    return error;
}

static int ReadVnodes(register struct iod *iodp, Volume *vp,
		      int incremental, afs_int32 *Lbuf, afs_int32 s1,
		      afs_int32 *Sbuf, afs_int32 s2, afs_int32 delo)
{
    afs_int32 vnodeNumber;
    char buf[SIZEOF_LARGEDISKVNODE];
    register tag;
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *) buf;
    struct VnodeDiskObject oldvnode;
    int idx;
    VnodeClass class;
    struct VnodeClassInfo *vcp;
    IHandle_t *tmpH;
    FdHandle_t *fdP;
    Inode	nearInode;

    tag = iod_getc(iodp);
    V_pref(vp, nearInode);
    while (tag == D_VNODE) {
        int haveStuff = 0;
	bzero(buf, sizeof (buf));
	if (!ReadInt32(iodp, (unsigned int *)&vnodeNumber))
	    break;

	ReadInt32(iodp, &vnode->uniquifier);
	while ((tag = iod_getc(iodp)) > D_MAX && tag != EOF) {
	    haveStuff = 1;
	    switch (tag) {
		case 't':
		    vnode->type = (VnodeType) iod_getc(iodp);
		    break;
		case 'l':
		    {
			unsigned short tlc;
			ReadShort(iodp, &tlc);
			vnode->linkCount = (signed int)tlc;
		    }
	   	    break;
		case 'v':
		    ReadInt32(iodp, &vnode->dataVersion);
		    break;
		case 'm':
		    ReadInt32(iodp, &vnode->unixModifyTime);
		    break;
		case 's':
		    ReadInt32(iodp, &vnode->serverModifyTime);
		    break;
		case 'a':
		    ReadInt32(iodp, &vnode->author);
		    break;
		case 'o':
		    ReadInt32(iodp, &vnode->owner);
		    break;
	        case 'g':
		    ReadInt32(iodp, (unsigned int *)&vnode->group);
		    break;
		case 'b': {
		    unsigned short modeBits;
		    ReadShort(iodp, &modeBits);
		    vnode->modeBits = (unsigned int)modeBits;
		    break;
		}
		case 'p':
		    ReadInt32(iodp, &vnode->parent);
		    break;
		case 'A':
		    ReadByteString(iodp, (byte *)VVnodeDiskACL(vnode),
				   VAclDiskSize(vnode));
		    acl_NtohACL(VVnodeDiskACL(vnode));
		    break;
		case 'f': {
		    Inode ino;
		    Error error;

		    ino = IH_CREATE(V_linkHandle(vp),
				    V_device(vp),
				    VPartitionPath(V_partition(vp)),
				    nearInode, V_parentId(vp), vnodeNumber,
				    vnode->uniquifier,
				    vnode->dataVersion);
		    if (!VALID_INO(ino)) {
			perror("unable to allocate inode");
			Log("1 Volser: ReadVnodes: Restore aborted\n");
			return VOLSERREAD_DUMPERROR;
		    }
		    nearInode = ino;
		    VNDISK_SET_INO(vnode, ino);
		    IH_INIT(tmpH, vp->device, V_parentId(vp), ino);
		    fdP = IH_OPEN(tmpH);
		    if (fdP == NULL) {
			IH_RELEASE(tmpH);
			return VOLSERREAD_DUMPERROR;
		    }
		    vnode->length = volser_WriteFile(vnodeNumber, iodp, fdP,
						     &error);
		    FDH_REALLYCLOSE(fdP);
		    IH_RELEASE(tmpH);
		    if (error) {
                        Log("1 Volser: ReadVnodes: IDEC inode %d\n", ino);
			IH_DEC(V_linkHandle(vp), ino, V_parentId(vp));
			return VOLSERREAD_DUMPERROR;
		    }
		    break;
		}
	    }
	}

	class = vnodeIdToClass(vnodeNumber);
	vcp   = &VnodeClassInfo[class];

	/* Mark this vnode as in this dump - so we don't delete it later */
	if (!delo) {
	   idx = (vnodeIndexOffset(vcp,vnodeNumber) >> vcp->logSize) - 1;
	   if (class == vLarge) {
	      if (Lbuf && (idx < s1)) Lbuf[idx] = 0;
	   } else {
	      if (Sbuf && (idx < s2)) Sbuf[idx] = 0;
	   }
	}

	if (haveStuff) {
	    FdHandle_t *fdP = IH_OPEN(vp->vnodeIndex[class].handle);
	    if (fdP == NULL) {
		Log("1 Volser: ReadVnodes: Error opening vnode index; restore aborted\n");
		return VOLSERREAD_DUMPERROR;
	    }
	    if (FDH_SEEK(fdP, vnodeIndexOffset(vcp, vnodeNumber), SEEK_SET) < 0) {
		Log("1 Volser: ReadVnodes: Error seeking into vnode index; restore aborted\n");
		FDH_REALLYCLOSE(fdP);
		return VOLSERREAD_DUMPERROR;
	    }
	    if (FDH_READ(fdP, &oldvnode, sizeof(oldvnode)) == sizeof(oldvnode)) {
		if (oldvnode.type != vNull && VNDISK_GET_INO(&oldvnode)) {
		    IH_DEC(V_linkHandle(vp), VNDISK_GET_INO(&oldvnode),
			 V_parentId(vp));
		}
	    }
	    vnode->vnodeMagic = vcp->magic;
	    if (FDH_SEEK(fdP, vnodeIndexOffset(vcp, vnodeNumber), SEEK_SET) < 0) {
		Log("1 Volser: ReadVnodes: Error seeking into vnode index; restore aborted\n");
		FDH_REALLYCLOSE(fdP);
		return VOLSERREAD_DUMPERROR;
	    }
	    if (FDH_WRITE(fdP, vnode, vcp->diskSize) != vcp->diskSize) {
		Log("1 Volser: ReadVnodes: Error writing vnode index; restore aborted\n");
		FDH_REALLYCLOSE(fdP);
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
 */
static bit32 volser_WriteFile(int vn, struct iod *iodp, FdHandle_t *handleP,
			      Error *status)
{
    afs_int32 code;
    afs_uint32 filesize;
    bit32 written=0;
    register afs_uint32 size = 8192;
    register afs_uint32 nbytes;
    unsigned char *p;


    *status = 0;
    if (!ReadInt32(iodp, &filesize)) {
        *status = 1;
	return(0);
    }
    p = (unsigned char *) malloc(size);
    if (p == NULL) {
        *status = 2;
        return(0);
    }
    for (nbytes = filesize; nbytes; nbytes -= size) {
	if (nbytes < size)
	    size = nbytes;
	
	if ((code = iod_Read(iodp, p, size)) != size) {
	    Log("1 Volser: WriteFile: Error reading dump file %d size=%d nbytes=%d (%d of %d); restore aborted\n", vn, filesize, nbytes, code, size);
	    *status = 3;
	    break;
	}
	code = FDH_WRITE(handleP, p, size);
	if (code > 0) written += code;
	if (code != size) {
	    Log("1 Volser: WriteFile: Error creating file in volume; restore aborted\n");
	    *status = 4;
	    break;
	}
    }
    free(p);
    return(written);
}

static int ReadDumpHeader(register struct iod *iodp, struct DumpHeader *hp)
{
    register tag;
    afs_uint32 beginMagic;
    if (iod_getc(iodp) != D_DUMPHEADER || !ReadInt32(iodp, &beginMagic)
       || !ReadInt32(iodp, (unsigned int *)&hp->version)
       || beginMagic != DUMPBEGINMAGIC
       ) return 0;
    hp->volumeId = 0;
    hp->nDumpTimes = 0;
    while ((tag = iod_getc(iodp)) > D_MAX) {
        unsigned short arrayLength;
 	register int i;
	switch(tag) {
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
		for (i = 0; i<hp->nDumpTimes; i++)
		    if (!ReadInt32(iodp, (unsigned int *)&hp->dumpTimes[i].from) || !ReadInt32(iodp, (unsigned int *)&hp->dumpTimes[i].to))
		        return 0;
		break;
	}
    }
    if (!hp->volumeId || !hp->nDumpTimes) {
	return 0;
    }
    iod_ungetc(iodp, tag);
    return 1;
}
