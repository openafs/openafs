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

#undef	IN
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>		/* for mtio.h */
#include <afs/cmd.h>
#include <afs/procmgmt.h>
#include <afs/usd.h>

/* structure for writing data to tape */
typedef struct tapeDataBuffer {
    struct tapeDataBuffer *tdb_next;
    char *tdb_buffer;
} tapeDataBufferT;
typedef tapeDataBufferT *tapeDataBufferP;

/* globals */
char *tapeDevice = 0;		/* device pathname */
afs_int32 eotEnabled = 1;

/* prototypes */
int fileMark(usd_handle_t hTape);
int fileMarkSize(char *tapeDevice);
static int tt_fileMarkSize(struct cmd_syndesc *as, void *arock);

#define ERROR(evalue)                                           \
        {                                                       \
            code = evalue;                                      \
            goto error_exit;                                    \
        }

#define MAXV	100

#ifndef AFS_NT40_ENV
#include "AFS_component_version_number.c"
#endif

void quitFms(int);

main(argc, argv)
     int argc;
     char **argv;
{
    struct sigaction intaction, oldaction;
    struct cmd_syndesc *cptr;

    memset((char *)&intaction, 0, sizeof(intaction));
    intaction.sa_handler = (int (*)())quitFms;

    sigaction(SIGINT, &intaction, &oldaction);

    cptr =
	cmd_CreateSyntax(NULL, tt_fileMarkSize, NULL,
			 "write a tape full of file marks");
    cmd_AddParm(cptr, "-tape", CMD_SINGLE, CMD_REQUIRED, "tape special file");

    cmd_Dispatch(argc, argv);
}

static int
tt_fileMarkSize(struct cmd_syndesc *as, void *arock)
{
    char *tapeDevice;

    tapeDevice = as->parms[0].items->data;

    fileMarkSize(tapeDevice);

    return 0;
}


fileMarkSize(tapeDevice)
     char *tapeDevice;
{
    afs_uint32 nFileMarks, nBlocks, nbfTape;
    double tpSize, fmSize;
    afs_uint32 bufferSize = 16384;
    usd_handle_t hTape;
    FILE *logFile;
    int count = 0;
    afs_uint32 countr;
    afs_int32 code = 0;

    afs_int32 rewindTape();

    code =
	usd_Open(tapeDevice, (USD_OPEN_RDWR | USD_OPEN_WLOCK), 0777, &hTape);
    if (code) {
	printf("Can't open tape device %s\n", tapeDevice);
	fflush(stdout);
	exit(1);
    }

    logFile = fopen("fms.log", "w+");
    if (logFile == NULL) {
	printf("Can't open log file\n");
	fflush(stdout);
	exit(1);
    }
    fprintf(logFile, "fms test started\n");
    fflush(logFile);

    code = rewindTape(hTape);
    if (code) {
	fprintf(logFile, "Can't rewind tape\n");
	fflush(logFile);
	ERROR(code);
    }

    /* measure capacity of tape */
    nbfTape = 0;
    countr = 0;
    while (1) {
	code = dataBlock(hTape, bufferSize);
	nbfTape++;
	count++;
	countr++;
	if (code)
	    break;

	if (count >= 5) {
	    count = 0;
	    printf("\rwrote block: %d", nbfTape);
	}

    }

    fprintf(logFile, "wrote %d blocks\n", nbfTape);
    fflush(logFile);
    printf("\rwrote %d blocks\n", nbfTape);
    printf("Finished data capacity test - rewinding\n");
    /* reset the tape device */
    code = USD_CLOSE(hTape);
    if (code) {
	fprintf(logFile, "Can't close tape device at end of pass 1\n");
	fflush(logFile);
	printf("Can't close tape device %s\n", tapeDevice);
	goto error_exit;
    }
    code =
	usd_Open(tapeDevice, (USD_OPEN_RDWR | USD_OPEN_WLOCK), 0777, &hTape);
    if (code) {
	fprintf(logFile, "Can't open tape device for pass 2\n");
	fflush(logFile);
	printf("Can't open tape device %s\n", tapeDevice);
	goto error_exit;
    }

    code = rewindTape(hTape);
    if (code) {
	fprintf(logFile, "Can't rewind tape\n");
	fflush(logFile);
	ERROR(code);
    }

    /* now measure file mark size */
    nFileMarks = 0;
    nBlocks = 0;
    count = 0;
    countr = 0;
    while (1) {
	code = dataBlock(hTape, bufferSize);
	nBlocks++;
	if (code)
	    break;
	code = fileMark(hTape);
	nFileMarks++;
	if (code)
	    break;
	count++;
	countr++;

	if (count >= 2) {
	    count = 0;
	    printf("\rwrote %d blocks, %d filemarks", nBlocks, nFileMarks);
	}

    }
    printf("\nFinished filemark test\n");
    tpSize = (double)nbfTape *(double)bufferSize;
    fmSize =
	(((double)nbfTape -
	  (double)nBlocks) * (double)bufferSize) / (double)nFileMarks;
    printf("Tape capacity is %.0f bytes\n", tpSize);
    printf("File marks are %.0f bytes\n", fmSize);
    fprintf(logFile, "Tape capacity is %.0f bytes\n", tpSize);
    fprintf(logFile, "File marks are %.0f bytes\n", fmSize);
    fflush(logFile);
    fclose(logFile);
  error_exit:
    USD_CLOSE(hTape);
    return (code);
}

void
quitFms(int sig)
{
    exit(0);
}


/* --------------------------
 * device handling routines
 * --------------------------
 */

/* rewindTape() - rewinds tape to beginning */
afs_int32
rewindTape(usd_handle_t hTape)
{
    usd_tapeop_t tapeop;
    int rcode;

    tapeop.tp_op = USDTAPE_REW;
    tapeop.tp_count = 1;
    rcode = USD_IOCTL(hTape, USD_IOCTL_TAPEOPERATION, (void *)&tapeop);
    return rcode;
}

/* write an EOF marker */
int
fileMark(usd_handle_t hTape)
{
    usd_tapeop_t tapeop;
    int rcode;

    tapeop.tp_op = USDTAPE_WEOF;
    tapeop.tp_count = 1;
    rcode = USD_IOCTL(hTape, USD_IOCTL_TAPEOPERATION, (void *)&tapeop);
    return rcode;
}

/* dataBlock
 *	write a block of data on tape
 * entry:
 * 	blocksize - size of block in bytes
 */

dataBlock(usd_handle_t hTape, afs_int32 reqSize)
{
    static char *dB_buffer = 0;
    static afs_int32 dB_buffersize = 0;
    static int dB_count = 0;
    int *ptr;
    afs_int32 code = 0, xferd;

    /* dbBuffersize is only valid when dB_buffer is non-zero */

    if ((dB_buffer != 0)
	&& (dB_buffersize != reqSize)
	) {
	free(dB_buffer);
	dB_buffer = 0;
    }

    if (dB_buffer == 0) {
	dB_buffer = (char *)malloc(reqSize);
	if (dB_buffer == 0)
	    ERROR(-1);
	dB_buffersize = reqSize;
	memset(dB_buffer, 0, dB_buffersize);
    }

    ptr = (int *)dB_buffer;
    *ptr = dB_count++;

    code = USD_WRITE(hTape, dB_buffer, dB_buffersize, &xferd);
    if (code || xferd != dB_buffersize)
	ERROR(-1);

  error_exit:
    return (code);
}
