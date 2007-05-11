/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2007 Sine Nomine Associates
 */

#include <osi/osi.h>

RCSID
    ("$Header$");

#include <afs/cmd.h>
#include <lock.h>
#include <afs/tcdata.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include <afs/usd.h>

usd_handle_t fd;
usd_handle_t ofd;
int ofdIsOpen = 0;
afs_int32 nrestore, nskip;
int ask, printlabels, printheaders, verbose;
char filename[100];
char *outfile;

#define TAPE_MAGIC  1100000009	/* Defined in file_tm.c */
#define BLOCK_MAGIC 1100000005
#define FILE_MAGIC  1000000007
#define FILE_BEGIN     0
#define FILE_END       1
#define FILE_EOD      -1

struct tapeLabel {		/* also in file_tm.c */
    afs_int32 magic;
    struct butm_tapeLabel label;
};
struct fileMark {		/* also in file_tm.c */
    afs_int32 magic;
    afs_uint32 nBytes;
};

/* Read a tape block of size 16K */
afs_int32
readblock(buffer)
     char *buffer;
{
    static rerror = 0;
    u_int nread, total = 0;
    int rc, fmcount = 0;

    while (total < BUTM_BLOCKSIZE) {
	rc = USD_READ(fd, buffer + total, BUTM_BLOCKSIZE - total, &nread);
	if (rc != 0) {
	    return (rc);
	} else if ((nread == 0) && (total == 0)) {
	    if (verbose)
		fprintf(stderr, "Hardware file mark\n");
	    if (++fmcount > 3) {
		if (verbose)
		    fprintf(stderr,
			    "Greater than 3 hardware file marks in a row - done\n");
		return -1;
	    }
	} else if (nread == 0) {
	    fprintf(stderr, "Reached unexpected end of dump file\n");
	    return -1;
	} else {
	    total += nread;
	}
    }
    return 0;
}

printLabel(tapeLabelPtr)
     struct tapeLabel *tapeLabelPtr;
{
    tapeLabelPtr->label.dumpid = ntohl(tapeLabelPtr->label.dumpid);
    tapeLabelPtr->label.creationTime =
	ntohl(tapeLabelPtr->label.creationTime);
    tapeLabelPtr->label.expirationDate =
	ntohl(tapeLabelPtr->label.expirationDate);
    tapeLabelPtr->label.structVersion =
	ntohl(tapeLabelPtr->label.structVersion);
    tapeLabelPtr->label.useCount = ntohl(tapeLabelPtr->label.useCount);
    tapeLabelPtr->label.size = ntohl(tapeLabelPtr->label.size);

    fprintf(stderr, "\nDUMP       %u\n", tapeLabelPtr->label.dumpid);
    if (printlabels) {
	fprintf(stderr, "   AFS Tape  Name   : %s\n",
		tapeLabelPtr->label.AFSName);
	fprintf(stderr, "   Permanent Name   : %s\n",
		tapeLabelPtr->label.pName);
	fprintf(stderr, "   Dump Id          : %u\n",
		tapeLabelPtr->label.dumpid);
	fprintf(stderr, "   Dump Id Time     : %.24s\n",
		ctime(&(tapeLabelPtr->label.dumpid)));
	fprintf(stderr, "   Date Created     : %.24s\n",
		ctime(&(tapeLabelPtr->label.creationTime)));
	fprintf(stderr, "   Date Expires     : %.24s\n",
		ctime(&(tapeLabelPtr->label.expirationDate)));
	fprintf(stderr, "   Version Number   : %d\n",
		tapeLabelPtr->label.structVersion);
	fprintf(stderr, "   Tape Use Count   : %d\n",
		tapeLabelPtr->label.useCount);
	fprintf(stderr, "   Tape Size        : %u\n",
		tapeLabelPtr->label.size);
	fprintf(stderr, "   Comment          : %s\n",
		tapeLabelPtr->label.comment);
	fprintf(stderr, "   Dump Path        : %s\n",
		tapeLabelPtr->label.dumpPath);
	fprintf(stderr, "   Cell Name        : %s\n",
		tapeLabelPtr->label.cell);
	fprintf(stderr, "   Creator Name     : %s\n",
		tapeLabelPtr->label.creator.name);
	fprintf(stderr, "   Creator Instance : %s\n",
		tapeLabelPtr->label.creator.instance);
	fprintf(stderr, "   Creator Cell     : %s\n",
		tapeLabelPtr->label.creator.cell);
    }
}

printHeader(headerPtr, isvolheader)
     struct volumeHeader *headerPtr;
     afs_int32 *isvolheader;
{
    static volcount = 0;

    *isvolheader = 0;
    headerPtr->volumeID = ntohl(headerPtr->volumeID);
    headerPtr->server = ntohl(headerPtr->server);
    headerPtr->part = ntohl(headerPtr->part);
    headerPtr->from = ntohl(headerPtr->from);
    headerPtr->frag = ntohl(headerPtr->frag);
    headerPtr->magic = ntohl(headerPtr->magic);
    headerPtr->contd = ntohl(headerPtr->contd);
    headerPtr->dumpID = ntohl(headerPtr->dumpID);
    headerPtr->level = ntohl(headerPtr->level);
    headerPtr->parentID = ntohl(headerPtr->parentID);
    headerPtr->endTime = ntohl(headerPtr->endTime);
    headerPtr->versionflags = ntohl(headerPtr->versionflags);
    headerPtr->cloneDate = ntohl(headerPtr->cloneDate);

    if (headerPtr->magic == TC_VOLBEGINMAGIC) {
	*isvolheader = 1;
	if (verbose)
	    fprintf(stderr, "Volume header\n");
	fprintf(stderr,
		"VOLUME %3d %s (%u) - %s dump from %.24s till %.24s\n",
		++volcount, headerPtr->volumeName, headerPtr->volumeID,
		(headerPtr->level ? "Incr" : "Full"),
		((headerPtr->from) ? (char *)ctime(&headerPtr->from) : "0"),
		ctime(&(headerPtr->cloneDate)));
	if (printheaders) {
	    fprintf(stderr, "   Volume Name    = %s\n",
		    headerPtr->volumeName);
	    fprintf(stderr, "   Volume ID      = %u\n", headerPtr->volumeID);
	    fprintf(stderr, "   Clone Date     = %.24s\n",
		    ctime(&headerPtr->cloneDate));
	    fprintf(stderr, "   Vol Fragment   = %d\n", headerPtr->frag);
	    fprintf(stderr, "   Vol Continued  = 0x%x\n", headerPtr->contd);
	    fprintf(stderr, "   DumpSet Name   = %s\n",
		    headerPtr->dumpSetName);
	    fprintf(stderr, "   Dump ID        = %u\n", headerPtr->dumpID);
	    fprintf(stderr, "   Dump Level     = %d\n", headerPtr->level);
	    fprintf(stderr, "   Dump Since     = %.24s\n",
		    ctime(&headerPtr->from));
	    fprintf(stderr, "   parent Dump ID = %u\n", headerPtr->parentID);
	}
    } else if (headerPtr->magic == TC_VOLENDMAGIC) {
	if (verbose)
	    fprintf(stderr, "Volume Trailer\n");
    } else {
	fprintf(stderr, "Unrecognized Volume Header/Trailer\n");
    }
}

int
openOutFile(headerPtr)
     struct volumeHeader *headerPtr;
{
    afs_int32 len;
    int ch;
    int rc;
    int oflag;
    int skip, first;
    char rest[100];
    afs_hyper_t size;

    /* If we were asked to skip this volume, then skip it */
    if (nskip) {
	nskip--;
	return 0;
    }
    /* Skip if we are not to restore any */
    if (!nrestore)
	return 0;

    /* Get the volume name and strip off the BK or RO extension */
    if (outfile) {
	strcpy(filename, outfile);
    } else {
	strcpy(filename, headerPtr->volumeName);
	len = strlen(filename);
	if ((len > 7) && (strcmp(".backup", filename + len - 7) == 0)) {
	    filename[len - 7] = 0;
	} else if ((len > 9)
		   && (strcmp(".readonly", filename + len - 9) == 0)) {
	    filename[len - 9] = 0;
	}
    }

    if (ask) {
	first = 1;
	skip = 0;
	printf("Press return to retreive volume %s (%u) to file %s; " "s"
	       " to skip\n", headerPtr->volumeName, headerPtr->volumeID,
	       filename);
	do {
	    ch = getchar();
	    if ((first == 1) && (ch == 's'))
		skip = 1;
	    if ((first == 1) && (ch == 'q'))
		exit(0);
	    first = 0;
	} while (ch != '\n');
	if (skip) {
	    printf("Will not restore volume %s\n", headerPtr->volumeName,
		   headerPtr->volumeID);
	    return 0;
	}
    } else {
	printf("Retreive volume %s (%u) to file %s\n", headerPtr->volumeName,
	       headerPtr->volumeID, filename);
    }

    /* Should I append the date onto the end of the name? */

    /* Open the file to write to */
    if (headerPtr->contd == TC_VOLCONTD) {
	/* Continuation of dump */
	oflag = USD_OPEN_RDWR;
    } else {
	/* An all new dump */
	oflag = USD_OPEN_RDWR | USD_OPEN_CREATE;
    }
    rc = usd_Open(filename, oflag, 0664, &ofd);
    if (rc != 0) {
	fprintf(stderr, "Unable to open file %s. Skipping. Code = %d\n",
		filename, rc);
	nrestore--;
	return 0;
    }
    if (headerPtr->contd != TC_VOLCONTD) {
	hzero(size);
	rc = USD_IOCTL(ofd, USD_IOCTL_SETSIZE, &size);
	if (rc != 0) {
	    fprintf(stderr, "Unable to open file %s. Skipping. Code = %d\n",
		    filename, rc);
	    USD_CLOSE(ofd);
	    nrestore--;
	    return 0;
	}
    }
    ofdIsOpen = 1;
    return 0;
}

int
writeLastBlocks(char *lastblock, char *lastblock2)
{
    int rc;
    struct volumeHeader vh;
    char trailer[12];
    struct blockMark *bmark, *bmark2;
    char *data, *data2;
    int count, count2;
    int tlen, skip, pos;

    if (!ofdIsOpen)
	return 0;

    bmark = (struct blockMark *)lastblock;
    data = &lastblock[sizeof(struct blockMark)];
    count = ntohl(bmark->count);
    if (lastblock2) {
	bmark2 = (struct blockMark *)lastblock2;
	data2 = &lastblock2[sizeof(struct blockMark)];
	count2 = ntohl(bmark2->count);
    } else {
	count2 = 0;
    }

    /*
     * Strip off all but the last twelve bytes of the volume trailer
     */
    skip = sizeof(struct volumeHeader) - 12;
    if (count >= skip) {
	count = count - skip;
    } else if (count + count2 >= skip) {
	count2 = count2 - (skip - count);
	count = 0;
    } else {
	fprintf(stderr, "Failed to strip off volume trailer (1).\n");
	return 0;
    }

    /* volume trailer is somewhere in the last 12 bytes of the tape file.
     * The volume trailer may span tape blocks. */
    if (count >= 12) {
	tlen = 0;
	memcpy(trailer, data + (count - 12), 12);
    } else {
	tlen = 12 - count;
	memcpy(trailer, data2 + (count2 - tlen), tlen);
	if (count != 0) {
	    memcpy(trailer + tlen, data, count);
	}
    }

    for (pos = 0; pos <= 2; pos++) {
	if (strncmp(&trailer[pos], "H++NAME#", 8) == 0) {
	    break;
	}
	if (tlen > 0) {
	    tlen--;
	}
    }

    if (pos == 3) {
	fprintf(stderr, "Failed to strip off volume trailer (2).\n");
    } else {
	if (count2 - tlen > 0) {
	    rc = writeData(data2, count2 - tlen);
	}
	if ((tlen == 0) && (count > 12 - pos)) {
	    rc = writeData(data, count - (12 - pos));
	}
    }
    return 0;
}

int
closeOutFile()
{
    if (!ofdIsOpen)
	return 0;

    USD_CLOSE(ofd);
    ofdIsOpen = 0;

    /* Decrement the number of volumes to restore */
    nrestore--;
    return 0;
}

int
writeData(data, size)
     char *data;
     afs_int32 size;
{
    int rc;
    u_int nwritten;

    if (!ofdIsOpen)
	return 0;
    rc = USD_WRITE(ofd, data, (u_int) size, &nwritten);
    if (rc != 0) {
	fprintf(stderr, "Unable to write volume data to file. Code = %d\n",
		rc);
    }
    return 0;
}

WorkerBee(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{
    char *tapedev;
    struct tapeLabel *label;
    struct fileMark *fmark;
    afs_int32 fmtype;
    struct blockMark *bmark;
    afs_int32 isheader, isdatablock;
    char *data;
    char *tblock;
    afs_int32 code;
    struct volumeHeader *volheaderPtr;
    int eod = 1;
    int rc;
    char *nextblock;		/* We cycle through three tape blocks so we  */
    char *lastblock;		/* can trim off the volume trailer from the  */
    char *lastblock2;		/* end of each volume without having to back */
    char *tapeblock1;		/* up the output stream.                     */
    char *tapeblock2;
    char *tapeblock3;

    tapedev = as->parms[0].items->data;	/* -tape     */
    nrestore =
	(as->parms[1].items ? atol(as->parms[1].items->data) : 0x7fffffff);
    nskip = (as->parms[2].items ? atol(as->parms[2].items->data) : 0);
    if (as->parms[4].items)
	nskip = 0x7fffffff;	/* -scan     */
    outfile = (as->parms[3].items ? as->parms[3].items->data : 0);
    ask = (as->parms[5].items ? 0 : 1);	/* -noask    */
    printlabels = (as->parms[6].items ? 1 : 0);	/* -label    */
    printheaders = (as->parms[7].items ? 1 : 0);	/* -vheaders */
    verbose = (as->parms[8].items ? 1 : 0);	/* -verbose  */

    /* Open the tape device */
    rc = usd_Open(tapedev, USD_OPEN_RDONLY, 0, &fd);
    if (rc != 0) {
	printf("Failed to open tape device %s. Code = %d\n", tapedev, rc);
	exit(1);
    }

    /*
     * Initialize the tape block buffers
     */
    tapeblock1 = (char *)malloc(3 * 16384);
    if (tapeblock1 == NULL) {
	printf("Failed to allocate I/O buffers.\n");
	exit(1);
    }
    tapeblock2 = tapeblock1 + 16384;
    tapeblock3 = tapeblock2 + 16384;

    nextblock = tapeblock1;
    lastblock = NULL;
    lastblock2 = NULL;

    /* Read each tape block deciding what to do with it */
    do {			/* while ((nskip!=0) && (nrestore!=0)) */
	code = readblock(nextblock);
	if (code) {
	    if (!eod)
		fprintf(stderr, "Tape device read error: %d\n", code);
	    break;
	}
	isdatablock = 0;

	/* A data block can be either a volume header, volume trailer,
	 * or actual data from a dump.
	 */
	bmark = (struct blockMark *)nextblock;
	label = (struct tapeLabel *)nextblock;
	fmark = (struct fileMark *)nextblock;
	if (ntohl(bmark->magic) == BLOCK_MAGIC) {
	    if (verbose)
		printf("Data block\n");
	    isdatablock = 1;
	    isheader = 0;
	    data = &nextblock[sizeof(struct blockMark)];
	    if (strncmp(data, "H++NAME#", 8) == 0) {
		volheaderPtr = (struct volumeHeader *)data;
		printHeader(volheaderPtr, &isheader);
	    }
	    if (isheader) {
		code = openOutFile(volheaderPtr);
		nextblock = tapeblock1;
		lastblock = NULL;
		lastblock2 = NULL;
	    } else {
		if (lastblock2 != NULL) {
		    data = &lastblock2[sizeof(struct blockMark)];
		    bmark = (struct blockMark *)lastblock2;
		    code = writeData(data, ntohl(bmark->count));
		    tblock = lastblock2;
		} else if (lastblock != NULL) {
		    tblock = tapeblock2;
		} else {
		    tblock = tapeblock3;
		}
		lastblock2 = lastblock;
		lastblock = nextblock;
		nextblock = tblock;
	    }
	}

	/* Filemarks come in 3 forms: BEGIN, END, and EOD.
	 * There is no information stored in filemarks.
	 */
	else if (ntohl(fmark->magic) == FILE_MAGIC) {
	    fmtype = ntohl(fmark->nBytes);
	    if (fmtype == FILE_BEGIN) {
		if (verbose)
		    fprintf(stderr, "File mark volume begin\n");
	    } else if (fmtype == FILE_END) {
		if (verbose)
		    fprintf(stderr, "File mark volume end\n");
	    } else if (fmtype == FILE_EOD) {
		if (verbose)
		    fprintf(stderr, "File mark end-of-dump\n");
		eod = 1;
	    }
	}

	/* A dump label */
	else if (ntohl(label->magic) == TAPE_MAGIC) {
	    if (verbose)
		fprintf(stderr, "Dump label\n");
	    printLabel(label);
	    eod = 0;
	}

	else {
	    if (verbose) {
		fprintf(stderr, "Unrecognized tape block - end\n");
	    }
	}

	/* Anything other than a data block closes the out file.
	 * At this point nextblock contains the end of tape file mark,
	 * lastblock contains the last data block for the current volume,
	 * and lastblock2 contains the second to last block for the current
	 * volume. If the volume fits in a single block, lastblock2 will
	 * be NULL. Call writeLastBlocks to strip off the dump trailer before
	 * writing the last of the volume data to the dump file. The dump
	 * trailer may span block boundaries.
	 */
	if (!isdatablock && lastblock) {
	    writeLastBlocks(lastblock, lastblock2);
	    closeOutFile();
	    nextblock = tapeblock1;
	    lastblock = NULL;
	    lastblock2 = NULL;
	}
    } while ((nskip != 0) || (nrestore != 0));

    free(tapeblock1);

    return 0;
}

main(argc, argv)
     int argc;
     char **argv;
{
    int code;
    struct cmd_syndesc *ts;
    struct cmd_item *ti;

    osi_AssertOK(osi_PkgInit(osi_ProgramType_Utility,
			     osi_NULL));

    setlinebuf(stdout);

    ts = cmd_CreateSyntax(NULL, WorkerBee, NULL,
			  "Restore volumes from backup tape");
    cmd_AddParm(ts, "-tape", CMD_SINGLE, CMD_REQUIRED, "tape device");
    cmd_AddParm(ts, "-restore", CMD_SINGLE, CMD_OPTIONAL,
		"# volumes to restore");
    cmd_AddParm(ts, "-skip", CMD_SINGLE, CMD_OPTIONAL, "# volumes to skip");
    cmd_AddParm(ts, "-file", CMD_SINGLE, CMD_OPTIONAL, "filename");
    cmd_AddParm(ts, "-scan", CMD_FLAG, CMD_OPTIONAL, "Scan the tape");
    cmd_AddParm(ts, "-noask", CMD_FLAG, CMD_OPTIONAL,
		"Prompt for each volume");
    cmd_AddParm(ts, "-label", CMD_FLAG, CMD_OPTIONAL, "Display dump label");
    cmd_AddParm(ts, "-vheaders", CMD_FLAG, CMD_OPTIONAL,
		"Display volume headers");
    cmd_AddParm(ts, "-verbose", CMD_FLAG, CMD_OPTIONAL, "verbose");

    code = cmd_Dispatch(argc, argv);
    osi_AssertOK(osi_PkgShutdown());
    return code;
}
