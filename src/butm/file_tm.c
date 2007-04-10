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

RCSID
    ("$Header$");

#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdio.h>
#include <ctype.h>
#include <lwp.h>
#include <afs/com_err.h>
#include <afs/butm.h>
#include <afs/usd.h>
#include "error_macros.h"

#ifdef O_LARGEFILE
typedef off64_t osi_lloff_t;
#else /* O_LARGEFILE */
#ifdef AFS_HAVE_LLSEEK
typedef offset_t osi_lloff_t;
#else /* AFS_HAVE_LLSEEK */
typedef off_t osi_lloff_t;
#endif /* AFS_HAVE_LLSEEK */
#endif /* O_LARGEFILE */

extern int isafile;

#define FILE_MAGIC  1000000007	/* s/w file mark */
#define FILE_BEGIN  0		/* byte field in file mark */
#define FILE_FMEND    1		/* byte field in file mark */
#define FILE_EOD    -1		/* byte field in file mark */
#define TAPE_MAGIC  1100000009	/* tape label block */
#define BLOCK_MAGIC 1100000005	/* file data block */
#ifdef AFS_PTHREAD_ENV
#define POLL()
#define SLEEP(s) sleep(s)
#else
#define POLL()   IOMGR_Poll()
#define SLEEP(s) IOMGR_Sleep(s)
#endif

/* Notes: (PA)
 *
 * 1) filemarks and block marks have the magic,bytes fields reversed. This
 *	is undoubtedly a bug. Also note that the two structures have
 *	inconsistent types, overlaying int and afs_int32.
 * 2) When a volume is dumped, if the volume is locked, the dump will produce
 *	an anomalous tape format of the form:
 *		s/w file begin mark
 *		volume header
 *		s/w file end mark
 * 	The design of the original butm code means that this cannot be
 *	handled correctly. The code is modified so that ReadFileData
 *	returns BUTM_ENDVOLUME if it encounters the s/w filemark.
 */

/* data organization on tape:
 * all writes are in blocks of BUTM_BLOCKSIZE (= sizeof(blockMark) + BUTM_BLKSIZE)
 * blockMark contains a magic number and counts of real data bytes
 * written out in the block.
 *
 * each file is preceeded by a fileMark, which acts as the file 
 * delimiter. A file consists of contigous data blocks. TM does
 * understand or interrpet the data in data blocks. 
 *
 * The tape begins with a tape label and ends with EOF file markers
 * in succession (2 or 4 of them ).
 */


struct fileMark {		/* in network byte order */
    afs_int32 magic;
    afs_uint32 nBytes;
};

struct tapeLabel {
    afs_int32 magic;
    struct butm_tapeLabel label;
};

struct progress {
    usd_handle_t fid;		/* file id of simulated tape */
    afs_int32 mountId;		/* current mountId */
    afs_int32 reading;		/* read file operation in progress */
    afs_int32 writing;		/* write file operation in progress */
};

static struct configuration {
    char tapedir[64];		/* directory to create "tapes" */
    afs_int32 mountId;		/* for detecting simultaneous mounts */
    afs_uint32 tapeSize;	/* size of simulated tapes */
    afs_uint32 fileMarkSize;	/* size of file mark, bytes */
    afs_int32 portOffset;	/* port + portOffset is used by TC to listen */
} config;

static char *whoami = "file_tm";
char tapeBlock[BUTM_BLOCKSIZE];	/* Tape buffer for reads and writes */

#define BLOCK_LABEL      0	/* read/write a  tape label     */
#define BLOCK_FMBEGIN    1	/* read/write a  begin FileMark */
#define BLOCK_DATA       2	/* read/write a  data block     */
#define BLOCK_FMEND      3	/* read/write an end FileMark   */
#define BLOCK_EOD        4	/* read/write an EOD FileMark   */
#define BLOCK_EOF        5	/* tape block is a HW EOF mark (usually error) */
#define BLOCK_UNKNOWN    6	/* tape block is unknwon (error) */

#define WRITE_OP 1
#define READ_OP  0

#define READS  (((struct progress *)(info->tmRock))->reading)
#define WRITES (((struct progress *)(info->tmRock))->writing)

/* ----------------------------------------------------------------------
 * These routines use the usd library to perform tape operations.
 * ForkIoctl, ForkOpen, ForkClose, ForwardSpace, BackwardSpace, WriteEOF, 
 * PrepareAccess(nt), ShutdownAccess(nt)
 * 
 * Return Values: USD functions return 0 if successful or errno if failed.
 */
/* ------------------ USD Interface Functions Begin ------------------------ */

/*
 * On Unix, fork a child process to perform an IOCTL call. This avoids
 * blocking the entire process during a tape operation
 */

#ifndef AFS_NT40_ENV
/* Unix version of function */

static int
ForkIoctl(usd_handle_t fd, int op, int count)
{
    int rc;			/* return code from system calls */
    int i;			/* loop index */
    int pid;			/* process ID of child process */
    int status;			/* exit status of child process */
    int ioctl_rc;		/* return code from ioctl call */
    int pipefd[2];		/* pipe for child return status */
    int forkflag;		/* flag set when ready to fork */
    usd_tapeop_t tapeop;	/* tape operation specification */
    int unixfd;

#ifdef AFS_PTHREAD_ENV
    forkflag = 0;		/* No need to fork if using pthreads */
#else
    forkflag = 1;
#endif

    /* initialize tape command structure */
    tapeop.tp_op = op;
    tapeop.tp_count = count;

    /* create pipe for getting return code from child */
    if (forkflag) {
	rc = pipe(pipefd);
	if (rc < 0) {
	    printf("butm:  Can't open pipe for IOCTL process. Error %d\n",
		   errno);
	    forkflag = 0;
	}
    }

    if (forkflag) {
	pid = fork();
	if (pid < 0) {
	    close(pipefd[0]);
	    close(pipefd[1]);
	    printf("butm:  Can't fork IOCTL process. Error %d\n", errno);
	    forkflag = 0;

	}
    }

    if (!forkflag) {		/* problem starting child process */
	/* do the ioctl anyway, it will probably work */
	ioctl_rc = USD_IOCTL(fd, USD_IOCTL_TAPEOPERATION, (void *)&tapeop);
    } else if (pid == 0) {	/* child process */
	/* close all unneccessary file descriptors */
	/* note: as painful as it is, we have to reach under the covers of
	 *       the usd package to implement this functionality.
	 */
	unixfd = (int)(fd->handle);

	for (i = 3; i < _POSIX_OPEN_MAX; i++) {
	    if (i != unixfd && i != pipefd[1]) {
		close(i);
	    }
	}

	/* do the ioctl call */
	ioctl_rc = USD_IOCTL(fd, USD_IOCTL_TAPEOPERATION, (void *)&tapeop);

	/* send the return code back to the parent */
	write(pipefd[1], &ioctl_rc, sizeof(int));

	exit(0);
    } else {			/* parent process */

	close(pipefd[1]);
	POLL();
	/* read the result from the child process */
	rc = read(pipefd[0], &ioctl_rc, sizeof(int));
	if (rc != sizeof(int)) {
	    /* tape is now in unknown state */
	    printf("butm:  Can't determine IOCTL child status. Error %d\n",
		   errno);
	    ioctl_rc = EFAULT;
	}

	close(pipefd[0]);

	/* get the completion status from the child process */
	rc = waitpid(pid, &status, 0);
	while (rc < 0 && errno == EINTR) {
	    rc = waitpid(pid, &status, 0);
	}
	if (rc < 0) {
	    printf("butm:  Can't determine IOCTL child status. Error %d\n",
		   errno);
	} else if (status != 0) {
	    printf
		("butm: Unexpected IOCTL process status 0x%04x . Error %d\n",
		 status, errno);
	}
	SLEEP(1);
    }

    return (ioctl_rc);
}
#else
/* NT version of function */

static int
ForkIoctl(usd_handle_t fd, int op, int count)
{
    usd_tapeop_t tapeop;

    /* Issue requested tape control */
    tapeop.tp_op = op;
    tapeop.tp_count = count;

    return (USD_IOCTL(fd, USD_IOCTL_TAPEOPERATION, (void *)&tapeop));
}
#endif /* !AFS_NT40_ENV */


/*
 * On Unix, fork a child process to attempt to open the drive. We want to make
 * certain there is tape in the drive before trying to open the device
 * in the main process
 */

#ifndef AFS_NT40_ENV
/* Unix version of function. */

static int
ForkOpen(char *device)
{
    int rc;			/* return code from system calls */
    int i;			/* loop index */
    int pid;			/* process ID of child process */
    int status;			/* exit status of child process */
    int open_rc;		/* return code from open */
    int pipefd[2];		/* pipe for child return status */
    int forkflag;		/* flag set when ready to fork */
    usd_handle_t fd;		/* handle returned from open */

#ifdef AFS_PTHREAD_ENV
    forkflag = 0;		/* No need to fork if using pthreads */
#else
    forkflag = 1;
#endif

    /* create pipe for getting return code from child */
    if (forkflag) {
	rc = pipe(pipefd);
	if (rc < 0) {
	    printf("butm: Cannot create pipe for OPEN process. Error %d\n",
		   errno);
	    forkflag = 0;
	}
    }

    if (forkflag) {
	pid = fork();
	if (pid < 0) {
	    close(pipefd[0]);
	    close(pipefd[1]);
	    printf("butm: Cannot create child process for OPEN. Error %d\n",
		   errno);
	    forkflag = 0;
	}
    }

    if (!forkflag) {		/* problem starting child process */
	/*
	 *return success, the caller will discover any problems
	 * when it opens the device.
	 */
	open_rc = 0;
    } else if (pid == 0) {	/* child process */
	/* close all unneccessary file descriptors */
	for (i = 3; i < _POSIX_OPEN_MAX; i++) {
	    if (i != pipefd[1]) {
		close(i);
	    }
	}

	/* try the open */
	open_rc = usd_Open(device, USD_OPEN_RDONLY, 0, &fd);

	if (open_rc == 0) {
	    USD_CLOSE(fd);
	}

	/* send the return code back to the parent */
	write(pipefd[1], &open_rc, sizeof(open_rc));

	exit(0);
    } else {			/* parent process */

	close(pipefd[1]);

	POLL();
	/* read the result from the child process */
	rc = read(pipefd[0], &open_rc, sizeof(open_rc));
	if (rc != sizeof(open_rc)) {
	    /* this is not a problem since we will reopen the device anyway */
	    printf("butm: No response from  OPEN process. Error %d\n", errno);
	    open_rc = 0;
	}

	close(pipefd[0]);

	/* get the completion status from the child process */
	rc = waitpid(pid, &status, 0);
	while (rc < 0 && errno == EINTR) {
	    rc = waitpid(pid, &status, 0);
	}
	if (rc < 0) {
	    printf("butm: Cannot get status of OPEN process. Error %d\n",
		   errno);
	} else if (status != 0) {
	    printf
		("butm: Unexpected OPEN process exit status 0x%04x. Error %d \n",
		 status, errno);
	}
	SLEEP(1);
    }

    return (open_rc);
}
#else
/* NT version of function. */

static int
ForkOpen(char *device)
{
    return (0);
}
#endif /* AFS_NT40_ENV */

/*
 * On Unix, fork a child process to close the drive. If the drive rewinds
 * on close it could cause the process to block.
 */

#ifndef AFS_NT40_ENV
/* Unix version of function */

static int
ForkClose(usd_handle_t fd)
{
    int rc;			/* return code from system calls */
    int i;			/* loop index */
    int pid;			/* process ID of child process */
    int status;			/* exit status of child process */
    int close_rc, parent_close_rc;	/* return codes from close */
    int pipefd[2];		/* pipe for child return status */
    int ctlpipe[2];		/* pipe for message to child */
    int forkflag;		/* flag set when ready to fork */
    int unixfd;

#ifdef AFS_PTHREAD_ENV
    forkflag = 0;		/* No need to fork if using pthreads */
#else
    forkflag = 1;
#endif

    /* create pipe for getting return code from child */
    if (forkflag) {
	rc = pipe(pipefd);
	if (rc < 0) {
	    printf("butm: Cannot create pipe for CLOSE process. Error %d\n",
		   errno);
	    forkflag = 0;
	}
    }

    /* create pipe for notifying child when to close */
    if (forkflag) {
	rc = pipe(ctlpipe);
	if (rc < 0) {
	    close(pipefd[0]);
	    close(pipefd[1]);
	    printf("butm: Cannot create pipe for CLOSE  process. Error %d\n",
		   errno);
	    forkflag = 0;
	}
    }

    if (forkflag) {
	pid = fork();
	if (pid < 0) {
	    close(pipefd[0]);
	    close(pipefd[1]);
	    close(ctlpipe[0]);
	    close(ctlpipe[1]);
	    printf("butm: Cannot create CLOSE child process. Error %d\n",
		   errno);
	    forkflag = 0;
	}
    }

    if (!forkflag) {		/* problem starting child process */
	close_rc = USD_CLOSE(fd);
	parent_close_rc = close_rc;
    } else if (pid == 0) {	/* child process */
	/* close all unneccessary file descriptors */
	/* note: as painful as it is, we have to reach under the covers of
	 *       the usd package to implement this functionality.
	 */
	unixfd = (int)(fd->handle);

	for (i = 3; i < _POSIX_OPEN_MAX; i++) {
	    if (i != unixfd && i != ctlpipe[0] && i != pipefd[1]) {
		close(i);
	    }
	}

	/* the parent writes the control pipe after it closes the device */
	read(ctlpipe[0], &close_rc, sizeof(int));
	close(ctlpipe[0]);

	/* do the close */
	close_rc = USD_CLOSE(fd);

	/* send the return code back to the parent */
	write(pipefd[1], &close_rc, sizeof(int));

	exit(0);
    } else {			/* parent process */

	close(pipefd[1]);
	close(ctlpipe[0]);

	POLL();
	/*
	 * close the device, this should have no effect as long as the
	 * child has not closed
	 */

	parent_close_rc = USD_CLOSE(fd);

	/* notify the child to do its close */
	rc = write(ctlpipe[1], &close_rc, sizeof(int));	/* just send garbage */
	if (rc != sizeof(int)) {
	    printf("butm: Error communicating with CLOSE process. Error %d\n",
		   errno);
	}
	close(ctlpipe[1]);

	/* read the result from the child process */
	rc = read(pipefd[0], &close_rc, sizeof(int));
	if (rc != sizeof(int)) {
	    /* logging is enough, since we wrote a file mark the */
	    /* return code from the close doesn't really matter  */
	    printf("butm: No response from  CLOSE  process. Error %d\n",
		   errno);
	    close_rc = 0;
	}

	close(pipefd[0]);

	/* get the completion status from the child process */
	rc = waitpid(pid, &status, 0);
	while (rc < 0 && errno == EINTR) {
	    rc = waitpid(pid, &status, 0);
	}
	if (rc < 0) {
	    printf("butm: Cannot get status of CLOSE  process. Error %d\n",
		   errno);
	} else if (status != 0) {
	    printf
		("butm: Unexpected exit status 0x04x from CLOSE  process. Error %d\n",
		 status, errno);
	}

	/* if either process received an error, then return an error */
	if (parent_close_rc < 0) {
	    close_rc = parent_close_rc;
	}
	SLEEP(1);
    }

    return (close_rc);
}
#else
/* NT version of function */

static int
ForkClose(usd_handle_t fd)
{
    return (USD_CLOSE(fd));
}
#endif /* AFS_NT40_ENV */

/* Forward space file */
static int
ForwardSpace(usd_handle_t fid, int count)
{
    POLL();

    if (isafile) {
	return (0);
    } else {
	return (ForkIoctl(fid, USDTAPE_FSF, count));
    }
}

/* Backward space file */
static int
BackwardSpace(usd_handle_t fid, int count)
{
    POLL();

    if (isafile) {
	return (0);
    } else {
	return (ForkIoctl(fid, USDTAPE_BSF, count));
    }
}

/* write end of file mark */
static
WriteEOF(usd_handle_t fid, int count)
{
    POLL();

    if (isafile) {
	return (0);
    } else {
	return (ForkIoctl(fid, USDTAPE_WEOF, count));
    }
}

/* rewind tape */
static int
Rewind(usd_handle_t fid)
{
    if (isafile) {
	afs_hyper_t startOff, stopOff;
	hzero(startOff);

	return (USD_SEEK(fid, startOff, SEEK_SET, &stopOff));
    } else {
	return (ForkIoctl(fid, USDTAPE_REW, 0));
    };
}

/* prepare tape drive for access */
static int
PrepareAccess(usd_handle_t fid)
{
    int code = 0;
#ifdef AFS_NT40_ENV
    if (!isafile) {
	(void)ForkIoctl(fid, USDTAPE_PREPARE, 0);
    }
    /* NT won't rewind tape when it is opened */
    code = Rewind(fid);
#endif /* AFS_NT40_ENV */
    return code;
}

/* decommission tape drive after all accesses complete */
static int
ShutdownAccess(usd_handle_t fid)
{
#ifdef AFS_NT40_ENV
    if (!isafile) {
	(void)ForkIoctl(fid, USDTAPE_SHUTDOWN, 0);
    }
#endif /* AFS_NT40_ENV */
    return 0;
}

/* -------------------- USD Interface Functions End ----------------------- */

/* =====================================================================
 * Support routines
 * ===================================================================== */

/* incSize
 *	add the supplied no. of bytes to the byte count of information placed
 *	on the tape.
 * entry:
 *	dataSize - bytes used on the tape
 */

incSize(info, dataSize)
     struct butm_tapeInfo *info;
     afs_uint32 dataSize;
{
    info->nBytes += dataSize;
    info->kBytes += (info->nBytes / 1024);
    info->nBytes %= 1024;
}

/* incPosition
 *      IF YOU ADD/CHANGE THE ifdefs, BE SURE
 *      TO ALSO MAKE THE SAME CHANGES IN bu_utils/fms.c.
 *
 *	add the supplied no. of bytes to the byte count of data placed
 *	on the tape. Also check for reaching 2GB limit and reset the 
 *      pointer if necessary.  This allows us to use >2GB tapes.
 * entry:
 *      fid      - file id for the tape.
 *	dataSize - bytes used on the tape
 */

incPosition(info, fid, dataSize)
     struct butm_tapeInfo *info;
     usd_handle_t fid;
     afs_uint32 dataSize;
{
    /* Add this to the amount of data written to the tape */
    incSize(info, dataSize);

    info->posCount += dataSize;

    if (info->posCount >= 2147467264) {	/* 2GB - 16K */
	info->posCount = 0;
#if (defined(AFS_SUN_ENV) || defined(AFS_LINUX24_ENV))
	if (!isafile) {
        afs_hyper_t off;
	    hset64(off, 0, 0);
	    USD_IOCTL(fid, USD_IOCTL_SETSIZE, &off);
	}
#endif
    }
}

/*
 * This accounts for tape drives with a block size different from variable or 16K 
 * blocks and only reads that block size.
 */
afs_int32 TapeBlockSize;
afs_int32
readData(fid, data, totalSize, errorP)
     usd_handle_t fid;
     char *data;
     afs_int32 totalSize;
     afs_int32 *errorP;
{
    afs_int32 toread;		/* Number of bytes to read */
    afs_int32 rSize;		/* Total bytes read so far */
    afs_int32 tSize;		/* Temporary size */
    afs_int32 rc;		/* return code */

    toread = totalSize;		/* First, try to read all the data */

    rc = USD_READ(fid, &data[0], toread, &rSize);
    if (rc != 0) {
	*errorP = rc;
	return -1;
    }
    if (rSize == 0)		/* reached EOF */
	return rSize;

    if (rSize != TapeBlockSize) {	/* Tape block size has changed */
	TapeBlockSize = rSize;
	printf("Tape blocks read in %d Byte chunks.\n", TapeBlockSize);
    }

    /* Read the rest of the data in */
    while (rSize < totalSize) {
	toread =
	    ((totalSize - rSize) <
	     TapeBlockSize ? (totalSize - rSize) : TapeBlockSize);
	rc = USD_READ(fid, &data[rSize], toread, &tSize);
	if (rc)
	    *errorP = rc;
	else
	    rSize += tSize;
	if (tSize != toread)
	    break;
    }

    if (rSize > totalSize)
	printf("readData - Read > 16K data block - continuing.\n");

    return (rSize);
}

afs_int32
SeekFile(info, count)
     struct butm_tapeInfo *info;
     int count;
{
    afs_int32 code = 0;
    struct progress *p;
    afs_int32 error = 0;

    if (count == 0)
	ERROR_EXIT(0);
    p = (struct progress *)info->tmRock;

    if (isafile) {		/* no reason for seeking through a file */
	p->reading = p->writing = 0;
	return (0);
    }

    POLL();

    if (count > 0)
	error = ForwardSpace(p->fid, count);
    else
	error = BackwardSpace(p->fid, -count);

    POLL();

    if (error) {
	info->status |= BUTM_STATUS_SEEKERROR;
	ERROR_EXIT(BUTM_IOCTL);
    }

    info->position += count;
    incSize(info, (count * config.fileMarkSize));
    p = (struct progress *)info->tmRock;
    p->reading = p->writing = 0;

  error_exit:
    if (error)
	info->error = error;
    return (code);
}

/* Step to the next filemark if we are not at one already */
afs_int32
NextFile(info)
     struct butm_tapeInfo *info;
{
    afs_int32 code;

    if (!READS && !WRITES)
	return 0;

    code = SeekFile(info, 1);
    return (code);
}

static afs_int32
WriteTapeBlock(info, buffer, length, blockType)
     struct butm_tapeInfo *info;
     char *buffer;		/* assumed to be 16384 bytes with data in it */
     afs_int32 length;		/* amount data in buffer */
     afs_int32 blockType;
{
    afs_int32 code = 0, rc = 0;
    afs_int32 wsize;
    struct tapeLabel *label;
    struct fileMark *fmark;
    struct blockMark *bmark;
    struct progress *p;
    afs_int32 error = 0;

    p = (struct progress *)info->tmRock;

    if (blockType == BLOCK_DATA) {	/* Data Block */
	if (length == 0)
	    ERROR_EXIT(0);
	bmark = (struct blockMark *)buffer;
	memset(bmark, 0, sizeof(struct blockMark));
	bmark->magic = htonl(BLOCK_MAGIC);
	bmark->count = htonl(length);
    } else if (blockType == BLOCK_FMBEGIN) {	/* Filemark - begin */
	fmark = (struct fileMark *)buffer;
	fmark->magic = htonl(FILE_MAGIC);
	fmark->nBytes = htonl(FILE_BEGIN);
    } else if (blockType == BLOCK_FMEND) {	/* Filemark - end */
	fmark = (struct fileMark *)buffer;
	fmark->magic = htonl(FILE_MAGIC);
	fmark->nBytes = htonl(FILE_FMEND);
    } else if (blockType == BLOCK_LABEL) {	/* Label */
	label = (struct tapeLabel *)buffer;
	label->magic = htonl(TAPE_MAGIC);
    } else if (blockType == BLOCK_EOD) {	/* Filemark - EOD mark */
	fmark = (struct fileMark *)buffer;
	fmark->magic = htonl(FILE_MAGIC);
	fmark->nBytes = htonl(FILE_EOD);
    }

    /* Write the tape block */
    /* -------------------- */
    rc = USD_WRITE(p->fid, buffer, BUTM_BLOCKSIZE, &wsize);
    if ((rc == 0) && (wsize > 0)) {
	incPosition(info, p->fid, wsize);	/* record whats written */
	p->writing++;
    }

    if (wsize != BUTM_BLOCKSIZE) {
	info->status |= BUTM_STATUS_WRITEERROR;
	if (rc != 0) {
	    error = rc;
	    ERROR_EXIT(BUTM_IO);
	} else
	    ERROR_EXIT(BUTM_EOT);
    }
    if (isafile)
	info->position++;

    /* Write trailing EOF marker for some block types */
    /* ---------------------------------------------- */
    if ((blockType == BLOCK_FMEND) || (blockType == BLOCK_LABEL)
	|| (blockType == BLOCK_EOD)) {

	POLL();
	error = WriteEOF(p->fid, 1);
	POLL();
	if (error) {
	    info->status |= BUTM_STATUS_WRITEERROR;
	    ERROR_EXIT(BUTM_IOCTL);
	}

	incSize(info, config.fileMarkSize);
	if (!isafile)
	    info->position++;
	p->writing = 0;
    }

  error_exit:
    if (error)
	info->error = error;
    return (code);
}

static afs_int32
ReadTapeBlock(info, buffer, blockType)
     struct butm_tapeInfo *info;
     char *buffer;		/* assumed to be 16384 bytes */
     afs_int32 *blockType;
{
    afs_int32 code = 0;
    afs_int32 rsize, fmtype;
    struct tapeLabel *label;
    struct fileMark *fmark;
    struct blockMark *bmark;
    struct progress *p;

    *blockType = BLOCK_UNKNOWN;

    p = (struct progress *)info->tmRock;

    memset(buffer, 0, BUTM_BLOCKSIZE);
    label = (struct tapeLabel *)buffer;
    fmark = (struct fileMark *)buffer;
    bmark = (struct blockMark *)buffer;

    rsize = readData(p->fid, buffer, BUTM_BLOCKSIZE, &info->error);
    if (rsize > 0) {
	incPosition(info, p->fid, rsize);
	p->reading++;
    }

    if (rsize == 0) {		/* Read a HW EOF Marker? OK */
	*blockType = BLOCK_EOF;
	incSize(info, config.fileMarkSize);	/* Size of filemark */
	if (!isafile)
	    info->position++;	/* bump position */
	p->reading = 0;		/* No reads since EOF */
    }

    else if (rsize != BUTM_BLOCKSIZE) {	/* Didn't Read a full block */
	info->status |= BUTM_STATUS_READERROR;
	ERROR_EXIT((rsize < 0) ? BUTM_IO : BUTM_EOT);
    }

    else if (ntohl(bmark->magic) == BLOCK_MAGIC) {	/* Data block? */
	*blockType = BLOCK_DATA;
    }

    else if (ntohl(fmark->magic) == FILE_MAGIC) {	/* Read a filemark? */
	fmtype = ntohl(fmark->nBytes);

	if (fmtype == FILE_BEGIN) {	/* filemark begin */
	    *blockType = BLOCK_FMBEGIN;
	} else if (fmtype == FILE_FMEND) {	/* filemark end */
	    *blockType = BLOCK_FMEND;
	    code = SeekFile(info, 1);
	} else if (fmtype == FILE_EOD) {	/* EOD mark */
	    *blockType = BLOCK_EOD;
	    info->status |= BUTM_STATUS_EOD;
	    code = SeekFile(info, 1);
	}
    }

    else if (ntohl(label->magic) == TAPE_MAGIC) {	/* Read a tape label? */
	*blockType = BLOCK_LABEL;
	code = SeekFile(info, 1);
    }

    if (isafile)
	info->position++;

  error_exit:
    return (code);
}

/* check
 *	check version numbers and permissions in the info structure
 */

static afs_int32
check(info, write)
     struct butm_tapeInfo *info;
     int write;			/* write operation requested */
{
    struct progress *p;

    if (!info)
	return (BUTM_BADARGUMENT);

    /* Check version number in info structure */
    if (info->structVersion != BUTM_MAJORVERSION)
	return BUTM_OLDINTERFACE;

    /* Check if a tape is mounted */
    if (((p = (struct progress *)info->tmRock) == 0) || (p->fid == 0))
	return BUTM_NOMOUNT;

    /* If writing check if there is write access */
    if (write && (info->flags & BUTM_FLAGS_READONLY))
	return BUTM_READONLY;

    return 0;
}

static afs_int32
rewindFile(info)
     struct butm_tapeInfo *info;
{
    struct progress *p;
    afs_int32 code = 0;
    afs_int32 error;

    p = (struct progress *)info->tmRock;

    POLL();

    error = Rewind(p->fid);

    POLL();

    if (error) {
	info->status |= BUTM_STATUS_SEEKERROR;
	ERROR_EXIT(BUTM_IOCTL);
    }

    info->position = (isafile ? 0 : 1);
    info->kBytes = info->nBytes = 0;
    info->nFiles = info->nRecords = 0;
    p->reading = p->writing = 0;

  error_exit:
    if (error)
	info->error = error;
    return (code);
}

/* =====================================================================
 * butm routines
 * ===================================================================== */

static afs_int32
file_Mount(info, tape)
     struct butm_tapeInfo *info;
     char *tape;
{
    struct progress *p;
    char filename[64];
    usd_handle_t fid;
    int xflags;
    afs_int32 code = 0, error = 0, rc = 0;

    if (info->debug)
	printf("butm: Mount tape drive\n");

    POLL();
    info->error = 0;

    if (!info || !tape)
	ERROR_EXIT(BUTM_BADARGUMENT);
    if (info->structVersion != BUTM_MAJORVERSION)
	ERROR_EXIT(BUTM_OLDINTERFACE);
    if (info->tmRock)
	ERROR_EXIT(BUTM_PARALLELMOUNTS);
    if (strlen(tape) >= sizeof(info->name))
	ERROR_EXIT(BUTM_BADARGUMENT);

    strcpy(info->name, tape);

    strcpy(filename, config.tapedir);	/* the name of the tape device */
    info->position = (isafile ? 0 : 1);
    info->kBytes = info->nBytes = 0;
    info->nRecords = info->nFiles = 0;
    info->recordSize = 0;
    info->tapeSize = config.tapeSize;
    info->coefBytes = 1;
    info->coefRecords = 0;
    info->coefFiles = sizeof(struct fileMark);
    info->simultaneousTapes = 1;
    info->status = 0;
    info->error = 0;
    info->flags = BUTM_FLAGS_SEQUENTIAL;

    xflags = 0;
    if (isafile) {
	xflags |= USD_OPEN_CREATE;
    } else {
	/*
	 * try to open in a child process first so nothing will
	 * time out should the process block because the device
	 * isn't ready.
	 */

	if (ForkOpen(filename)) {
	    ERROR_EXIT(BUTM_MOUNTFAIL);
	}
    }

    /* Now go ahead and open the tape drive for real */
    rc = usd_Open(filename, (USD_OPEN_RDWR | USD_OPEN_WLOCK | xflags), 0777,
		  &fid);
    if (rc != 0) {		/* try for lesser access */
	rc = usd_Open(filename, (USD_OPEN_RDONLY | USD_OPEN_RLOCK), 0, &fid);

	if (rc) {
	    error = rc;
	    ERROR_EXIT(BUTM_MOUNTFAIL);
	}
	info->flags |= BUTM_FLAGS_READONLY;
    }

    (void)PrepareAccess(fid);	/* for NT */

    p = (struct progress *)malloc(sizeof(*p));
    info->tmRock = (char *)p;
    p->fid = fid;
    p->mountId = config.mountId = time(0);
    p->reading = p->writing = 0;

    TapeBlockSize = BUTM_BLOCKSIZE;	/* Initialize */

  error_exit:
    if (error)
	info->error = error;
    return (code);
}

static afs_int32
file_Dismount(info)
     struct butm_tapeInfo *info;
{
    struct progress *p;
    afs_int32 code = 0, error = 0;

    if (info->debug)
	printf("butm: Unmount tape drive\n");

    POLL();
    info->error = 0;

    code = check(info, READ_OP);
    if (code)
	ERROR_EXIT(code);

    p = (struct progress *)info->tmRock;

    (void)ShutdownAccess(p->fid);	/* for NT */

    /* close the device */
    if (error = ForkClose(p->fid)) {
	printf("butm: Tape close failed. Error %d\n", errno);
    }

    POLL();

    if (error) {
	code = BUTM_DISMOUNTFAIL;
	info->status |= BUTM_STATUS_TAPEERROR;
    }

    config.mountId = 0;
    info->tmRock = 0;		/* mark it as closed - even if error on close */
    if (p)
	free(p);

  error_exit:
    if (error)
	info->error = error;
    return (code);
}

/* file_WriteLabel
 *	write the header on a tape
 * entry:
 *	info - handle on tape unit
 *	label - label information. This label is not copied onto the tape.
 *		If supplied, various fields are copied from this label to
 *		the actual tape label written on the tape.
 */

static afs_int32
file_WriteLabel(info, label, rewind)
     struct butm_tapeInfo *info;
     struct butm_tapeLabel *label;
     afs_int32 rewind;
{
    afs_int32 code = 0;
    afs_int32 fcode;
    struct tapeLabel *tlabel;
    struct progress *p;
    afs_hyper_t off;		/* offset */

    if (info->debug)
	printf("butm: Write tape label\n");

    POLL();
    info->error = 0;

    code = check(info, WRITE_OP);
    if (code)
	ERROR_EXIT(code);
    if (!label)
	ERROR_EXIT(BUTM_BADARGUMENT);
    if (label->structVersion != CUR_TAPE_VERSION)
	ERROR_EXIT(BUTM_OLDINTERFACE);

    if (rewind) {		/* Not appending, so rewind */
	code = rewindFile(info);
	if (code)
	    ERROR_EXIT(code);

	if (isafile) {
	    p = (struct progress *)info->tmRock;
	    hset64(off, 0, 0);
	    code = USD_IOCTL(p->fid, USD_IOCTL_SETSIZE, &off);
	    if (code)
		ERROR_EXIT(BUTM_POSITION);
	}
    } else {
	if (READS || WRITES)
	    ERROR_EXIT(BUTM_BADOP);
    }

    /* Copy the label into the tape block
     * ---------------------------------- */
    memset(tapeBlock, 0, BUTM_BLOCKSIZE);

    if (!label->creationTime)
	label->creationTime = time(0);

    tlabel = (struct tapeLabel *)tapeBlock;
    memcpy(&tlabel->label, label, sizeof(struct butm_tapeLabel));
    tlabel->label.structVersion = htonl(CUR_TAPE_VERSION);
    tlabel->label.creationTime = htonl(tlabel->label.creationTime);
    tlabel->label.expirationDate = htonl(tlabel->label.expirationDate);
    tlabel->label.size = htonl(tlabel->label.size);
    tlabel->label.useCount = htonl(tlabel->label.useCount);
    tlabel->label.dumpid = htonl(tlabel->label.dumpid);

    /* 
     * write the tape label - For appends, the write may need to skip 
     * over 1 or 2 EOF marks that were written when tape was closed after 
     * the last dump. Plus, some AIX tape drives require we try forwarding
     * over the last EOF and take an error before we can write the new label.
     * ---------------------------------------------------------------------- */
    code = WriteTapeBlock(info, tapeBlock, BUTM_BLOCKSIZE, BLOCK_LABEL);
    if (!isafile && !rewind && (code == BUTM_IO))
	do {			/* do if write failed */
	    fcode = SeekFile(info, 1);	/* skip over the EOF */
	    if (fcode)
		break;		/* leave if error */

	    code =
		WriteTapeBlock(info, tapeBlock, BUTM_BLOCKSIZE, BLOCK_LABEL);
	    if (code != BUTM_IO)
		break;		/* continue if write failed */

	    fcode = SeekFile(info, 1);	/* skip over the EOF */
	    if (fcode) {	/* retry 1 write if couldn't skip */
		code =
		    WriteTapeBlock(info, tapeBlock, BUTM_BLOCKSIZE,
				   BLOCK_LABEL);
		break;
	    }

	    code =
		WriteTapeBlock(info, tapeBlock, BUTM_BLOCKSIZE, BLOCK_LABEL);
	    if (code != BUTM_IO)
		break;		/* continue if write failed */

	    fcode = SeekFile(info, 1);	/* skip over the EOF */
	    if (fcode) {	/* retry 1 write if couldn't skip */
		code =
		    WriteTapeBlock(info, tapeBlock, BUTM_BLOCKSIZE,
				   BLOCK_LABEL);
		break;
	    }
	    break;
	} while (0);

    /* clear the write error status a failed WriteTapeBlock may have produced */
    if (!code)
	info->status &= ~BUTM_STATUS_WRITEERROR;

  error_exit:
    return code;
}

static afs_int32
file_ReadLabel(info, label, rewind)
     struct butm_tapeInfo *info;
     struct butm_tapeLabel *label;
     afs_int32 rewind;
{
    struct tapeLabel *tlabel;
    afs_int32 code = 0;
    afs_int32 blockType;

    if (info->debug)
	printf("butm: Read tape label\n");

    POLL();
    info->error = 0;

    code = check(info, READ_OP);
    if (code)
	ERROR_EXIT(code);
    if (READS || WRITES)
	ERROR_EXIT(BUTM_BADOP);

    if (rewind) {
	code = rewindFile(info);
	if (code)
	    ERROR_EXIT(code);	/* status is set so return */
    }

    /*
     * When appended labels were written, either 1 or 2 EOF marks may
     * have had to be skipped.  When reading a label, these EOF marks 
     * must also be skipped. When an EOF is read, 0 bytes are returned
     * (refer to the write calls in file_WriteLabel routine).
     * ---------------------------------------------------------------- */
    code = ReadTapeBlock(info, tapeBlock, &blockType);
    if (!isafile && !rewind && (blockType == BLOCK_EOF))
	do {
	    code = ReadTapeBlock(info, tapeBlock, &blockType);
	    if (blockType != BLOCK_EOF)
		break;		/* didn't read an EOF */

	    code = ReadTapeBlock(info, tapeBlock, &blockType);
	} while (0);

    if (blockType == BLOCK_UNKNOWN)
	ERROR_EXIT(BUTM_NOLABEL);
    if (code)
	ERROR_EXIT(code);
    if (blockType != BLOCK_LABEL)
	ERROR_EXIT(BUTM_BADBLOCK);

    /* Copy label out
     * -------------- */
    if (label) {
	tlabel = (struct tapeLabel *)tapeBlock;
	memcpy(label, &tlabel->label, sizeof(struct butm_tapeLabel));
	label->structVersion = ntohl(label->structVersion);
	label->creationTime = ntohl(label->creationTime);
	label->expirationDate = ntohl(label->expirationDate);
	label->size = ntohl(label->size);
	label->dumpid = ntohl(label->dumpid);
	label->useCount = ntohl(label->useCount);

	info->tapeSize = label->size;	/* use size from label */
    }

  error_exit:
    return (code);
}

static afs_int32
file_WriteFileBegin(info)
     struct butm_tapeInfo *info;
{
    afs_int32 code = 0;
    afs_int32 error = 0;

    if (info->debug)
	printf("butm: Write filemark begin\n");

    POLL();

    code = check(info, WRITE_OP);
    if (code)
	ERROR_EXIT(code);

    code = WriteTapeBlock(info, tapeBlock, BUTM_BLOCKSIZE, BLOCK_FMBEGIN);
    if (code)
	ERROR_EXIT(code);

    info->nFiles++;

  error_exit:
    return (code);
}

static afs_int32
file_ReadFileBegin(info)
     struct butm_tapeInfo *info;
{
    afs_int32 code = 0;
    afs_int32 blockType;

    if (info->debug)
	printf("butm: Read filemark begin\n");

    POLL();
    info->error = 0;

    code = check(info, READ_OP);
    if (code)
	ERROR_EXIT(code);
    if (READS || WRITES)
	ERROR_EXIT(BUTM_BADOP);

    code = ReadTapeBlock(info, tapeBlock, &blockType);
    if (code)
	ERROR_EXIT(code);

    if (blockType != BLOCK_FMBEGIN) {
	if (blockType == BLOCK_EOD)
	    ERROR_EXIT(BUTM_EOD);	/* EODump label */
	if (blockType == BLOCK_LABEL)
	    ERROR_EXIT(BUTM_LABEL);	/* Tape label */
	ERROR_EXIT(BUTM_BADBLOCK);	/* Other */
    }

  error_exit:
    return (code);
}

/* Writes data out in block sizes of 16KB. Does destroy the data.
 * Assumes the data buffer has a space reserved at beginning for a blockMark.
 */
static afs_int32
file_WriteFileData(info, data, blocks, len)
     struct butm_tapeInfo *info;
     char *data;
     afs_int32 blocks;
     afs_int32 len;
{
    afs_int32 code = 0;
    int length;
    afs_int32 b;
    char *bstart;		/* Where block starts for a 16K block */
    char *dstart;		/* Where data  starts for a 16K block */

    if (info->debug)
	printf("butm: Write tape data - %u bytes\n", len);

    POLL();
    info->error = 0;

    code = check(info, WRITE_OP);
    if (code)
	ERROR_EXIT(code);
    if (!data || (len < 0))
	ERROR_EXIT(BUTM_BADARGUMENT);
    if (READS || !WRITES)
	ERROR_EXIT(BUTM_BADOP);

    b = 0;			/* start at block 0 */
    while (len > 0) {
	dstart = &data[b * BUTM_BLKSIZE];
	bstart = dstart - sizeof(struct blockMark);

	if (len < BUTM_BLKSIZE) {
	    memset(&dstart[len], 0, BUTM_BLKSIZE - len);
	    length = len;
	} else {
	    length = BUTM_BLKSIZE;
	}

	code = WriteTapeBlock(info, bstart, length, BLOCK_DATA);

	len -= length;

	/* If there are more blocks, step to next block */
	/* Otherwise, copy the data to beginning of last block */

	if (b < (blocks - 1))
	    b++;
	else if (len)
	    memcpy(&dstart[0], &dstart[BUTM_BLKSIZE], len);
    }

  error_exit:
    return (code);
}

/* file_ReadFileData
 *	Read a data block from tape.
 * entry:
 *	info - tape info structure, c.f. fid
 *	data - ptr to buffer for data
 *	len - size of data buffer
 * exit:
 *	nBytes - no. of data bytes read.
 */

static afs_int32
file_ReadFileData(info, data, len, nBytes)
     struct butm_tapeInfo *info;
     char *data;
     int len;
     int *nBytes;
{
    struct blockMark *bmark;
    afs_int32 code = 0;
    afs_int32 blockType;

    if (info->debug)
	printf("butm: Read tape data - %u bytes\n", len);

    POLL();
    info->error = 0;

    if (!nBytes)
	ERROR_EXIT(BUTM_BADARGUMENT);
    *nBytes = 0;

    code = check(info, READ_OP);
    if (code)
	ERROR_EXIT(code);
    if (!data || (len < 0) || (len > BUTM_BLKSIZE))
	ERROR_EXIT(BUTM_BADARGUMENT);
    if (!READS || WRITES)
	ERROR_EXIT(BUTM_BADOP);

    data -= sizeof(struct blockMark);
    code = ReadTapeBlock(info, data, &blockType);
    if (code)
	ERROR_EXIT(code);

    if (blockType != BLOCK_DATA) {
	if (blockType == BLOCK_EOF)
	    ERROR_EXIT(BUTM_EOF);
	if (blockType == BLOCK_FMEND)
	    ERROR_EXIT(BUTM_ENDVOLUME);
	ERROR_EXIT(BUTM_BADBLOCK);
    }

    bmark = (struct blockMark *)data;
    *nBytes = ntohl(bmark->count);	/* Size of data in buf */

  error_exit:
    return (code);
}

static afs_int32
file_WriteFileEnd(info)
     struct butm_tapeInfo *info;
{
    afs_int32 code = 0;

    if (info->debug)
	printf("butm: Write filemark end\n");

    POLL();
    info->error = 0;

    code = check(info, WRITE_OP);
    if (code)
	ERROR_EXIT(code);
    if (READS || !WRITES)
	ERROR_EXIT(BUTM_BADOP);

    code = WriteTapeBlock(info, tapeBlock, BUTM_BLOCKSIZE, BLOCK_FMEND);

  error_exit:
    return (code);
}

/* Read a SW filemark, verify that it is a SW filemark, then skip to the next 
 * HW filemark. If the read of the SW filemark shows it's an EOF, then 
 * ignore that the SW filemark is not there and return 0 (found the SW filemark
 * missing with some 3.1 dumps).
 */
static afs_int32
file_ReadFileEnd(info)
     struct butm_tapeInfo *info;
{
    afs_int32 code = 0;
    afs_int32 blockType;

    if (info->debug)
	printf("butm: Read filemark end\n");

    POLL();
    info->error = 0;

    code = check(info, READ_OP);
    if (code)
	ERROR_EXIT(code);
    if (!READS || WRITES)
	ERROR_EXIT(BUTM_BADOP);

    info->status &= ~BUTM_STATUS_EOF;

    code = ReadTapeBlock(info, tapeBlock, &blockType);
    if (code)
	ERROR_EXIT(code);

    if ((blockType != BLOCK_FMEND) && (blockType != BLOCK_EOF))
	ERROR_EXIT(BUTM_BADBLOCK);

  error_exit:
    return code;
}

/*
 * Write the end-of-dump marker.
 */
static afs_int32
file_WriteEODump(info)
     struct butm_tapeInfo *info;
{
    afs_int32 code = 0;

    if (info->debug)
	printf("butm: Write filemark EOD\n");

    POLL();
    info->error = 0;

    code = check(info, WRITE_OP);
    if (code)
	ERROR_EXIT(code);
    if (READS || WRITES)
	ERROR_EXIT(BUTM_BADOP);

    code = WriteTapeBlock(info, tapeBlock, BUTM_BLOCKSIZE, BLOCK_EOD);
    if (code)
	ERROR_EXIT(code);

    info->status |= BUTM_STATUS_EOD;

  error_exit:
    return (code);
}

static afs_int32
file_Seek(info, position)
     struct butm_tapeInfo *info;
     afs_int32 position;
{
    afs_int32 code = 0;
    afs_int32 w;
    osi_lloff_t posit;
    afs_uint32 c, d;
    struct progress *p;
    afs_hyper_t startOff, stopOff;	/* for normal file(non-tape)  seeks  */

    if (info->debug)
	printf("butm: Seek to the tape position %d\n", position);

    info->error = 0;

    code = check(info, READ_OP);
    if (code)
	ERROR_EXIT(code);

    if (isafile) {
	p = (struct progress *)info->tmRock;
	posit = (osi_lloff_t) position *(osi_lloff_t) BUTM_BLOCKSIZE;

	/* Not really necessary to do it this way, should be fixed */
#ifdef O_LARGEFILE
	c = (posit >> 32);
	d = (posit & 0xffffffff);
#else
	c = 0;
	d = posit;
#endif
	hset64(startOff, c, d);

	w = USD_SEEK(p->fid, startOff, SEEK_SET, &stopOff);
	if (w)
	    info->error = w;
	if (hcmp(startOff, stopOff) != 0)
	    ERROR_EXIT(BUTM_POSITION);

	p->reading = p->writing = 0;
	info->position = position;
    } else {
	/* Don't position backwards if we are in-between FMs */
	if ((READS || WRITES) && ((position - info->position) <= 0))
	    ERROR_EXIT(BUTM_BADOP);

	code = SeekFile(info, (position - info->position));
	if (code)
	    ERROR_EXIT(code);
    }

  error_exit:
    return (code);
}

/*
 * Seek to the EODump (end-of-dump) after the given position. This is 
 * the position after the EOF filemark immediately after the EODump mark. 
 * This is for tapes of version 4 or greater.
 */
static afs_int32
file_SeekEODump(info, position)
     struct butm_tapeInfo *info;
     afs_int32 position;
{
    afs_int32 code = 0;
    afs_int32 blockType;
    afs_int32 w;
    struct progress *p;
    afs_hyper_t startOff, stopOff;	/* file seek offsets */

    if (info->debug)
	printf("butm: Seek to end-of-dump\n");
    info->error = 0;

    code = check(info, READ_OP);
    if (code)
	ERROR_EXIT(code);
    if (READS || WRITES)
	ERROR_EXIT(BUTM_BADOP);

    if (isafile) {
	p = (struct progress *)info->tmRock;
	hset64(startOff, 0, 0);
	w = USD_SEEK(p->fid, startOff, SEEK_END, &stopOff);
	if (w) {
	    info->error = w;
	    ERROR_EXIT(BUTM_POSITION);
	}

	if (hgetlo(stopOff) % BUTM_BLOCKSIZE)
	    ERROR_EXIT(BUTM_POSITION);
	info->position = (hgetlo(stopOff) / BUTM_BLOCKSIZE);
    } else {
	/* Seek to the desired position */
	code = SeekFile(info, (position - info->position) + 1);
	if (code)
	    ERROR_EXIT(code);

	/*
	 * Search until the filemark is an EODump filemark.
	 * Skip over volumes only.
	 */
	while (1) {
	    code = ReadTapeBlock(info, tapeBlock, &blockType);
	    if (code)
		ERROR_EXIT(code);

	    if (blockType == BLOCK_EOD)
		break;
	    if (blockType != BLOCK_FMBEGIN)
		ERROR_EXIT(BUTM_BADBLOCK);

	    code = SeekFile(info, 1);	/* Step forward to next volume */
	    if (code)
		ERROR_EXIT(code);
	}
	code = 0;
    }

  error_exit:
    return (code);
}

static afs_int32
file_SetSize(info, size)
     struct butm_tapeInfo *info;
     afs_uint32 size;
{
    if (info->debug)
	printf("butm: Set size of tape\n");
    info->error = 0;

    if (size <= 0)
	info->tapeSize = config.tapeSize;
    else
	info->tapeSize = size;
    return 0;
}

static afs_int32
file_GetSize(info, size)
     struct butm_tapeInfo *info;
     afs_uint32 *size;
{
    if (info->debug)
	printf("butm: Get size of tape\n");
    info->error = 0;

    *size = info->tapeSize;
    return 0;
}

/* =====================================================================
 * Startup/configuration routines.
 * ===================================================================== */

static afs_int32
file_Configure(file)
     struct tapeConfig *file;
{
    if (!file) {
	afs_com_err(whoami, BUTM_BADCONFIG, "device not specified");
	return BUTM_BADCONFIG;
    }

    config.tapeSize = file->capacity;
    config.fileMarkSize = file->fileMarkSize;
    config.portOffset = file->portOffset;
    strcpy(config.tapedir, file->device);

    /* Tape must be large enough to at least fit a label */
    if (config.tapeSize <= 0) {
	afs_com_err(whoami, BUTM_BADCONFIG, "Tape size bogus: %d Kbytes",
		config.tapeSize);
	return BUTM_BADCONFIG;
    }

    if (strlen(config.tapedir) == 0) {
	afs_com_err(whoami, BUTM_BADCONFIG, "no tape device specified");
	return BUTM_BADCONFIG;
    }

    config.mountId = 0;
    return 0;
}

/* This procedure instantiates a tape module of type file_tm. */
afs_int32
butm_file_Instantiate(info, file)
     struct butm_tapeInfo *info;
     struct tapeConfig *file;
{
    extern int debugLevel;
    afs_int32 code = 0;

    if (debugLevel > 98)
	printf("butm: Instantiate butc\n");

    if (!info)
	ERROR_EXIT(BUTM_BADARGUMENT);
    if (info->structVersion != BUTM_MAJORVERSION)
	ERROR_EXIT(BUTM_OLDINTERFACE);

    memset(info, 0, sizeof(struct butm_tapeInfo));
    info->structVersion = BUTM_MAJORVERSION;
    info->ops.mount = file_Mount;
    info->ops.dismount = file_Dismount;
    info->ops.create = file_WriteLabel;
    info->ops.readLabel = file_ReadLabel;
    info->ops.seek = file_Seek;
    info->ops.seekEODump = file_SeekEODump;
    info->ops.readFileBegin = file_ReadFileBegin;
    info->ops.readFileData = file_ReadFileData;
    info->ops.readFileEnd = file_ReadFileEnd;
    info->ops.writeFileBegin = file_WriteFileBegin;
    info->ops.writeFileData = file_WriteFileData;
    info->ops.writeFileEnd = file_WriteFileEnd;
    info->ops.writeEOT = file_WriteEODump;
    info->ops.setSize = file_SetSize;
    info->ops.getSize = file_GetSize;
    info->debug = ((debugLevel > 98) ? 1 : 0);

    code = file_Configure(file);

  error_exit:
    return code;
}
