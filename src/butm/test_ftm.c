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

#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#ifdef AFS_NT40_ENV
#include <io.h>
#else
#include <sys/time.h>
#include <netinet/in.h>
#endif
#include <afs/afsutil.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdio.h>
#include <ctype.h>
#include <lwp.h>
#include <afs/com_err.h>
#include <afs/butm.h>
#include "error_macros.h"

int isafile = 0, debugLevel = 1;

/* Format of the config file is now 
 * <capacity devicename portno isafile>
 * capacity - size of tape or file in bytes
 * devicename - tape device or file name to write data into
 * portno - tape coordinator port no. (irrelevant) 
 * isafile - Is this entry a file or a tape? 
 *
 * test_ftm goes through the lines in the config file and selects the
 * entry that has a port no. of 0 and tries to open the device.
 */

static char *whoami = "test_TM";


typedef struct TestInfo {
    char *tapeName;		/* tape name */
    struct tapeConfig *tc_Infop;	/* tape config info (size, devname etc..) */
    int nFiles;			/* no. of files to be dumped */
    char **files;		/* names of files to be dumped */
    char dumpDone;		/* event that signals the completion of
				 * the dump test thread.
				 */
    int appended;		/* do an appended test? */
} TestInfo;

struct BufferBlock {
    char mark[sizeof(struct blockMark)];
    char data[BUTM_BLKSIZE];
} *bufferBlock;

struct tapeConfig confInfo;
char dumpDone;
int PerformDumpTest(TestInfo * tip);

static
GetDeviceInfo(filename, config)
     char *filename;
     struct tapeConfig *config;
{
    FILE *devFile;
    char line[356];
    char devName[256];
    afs_int32 aport, capacity, notfound;
    afs_int32 portOffset;

    portOffset = 0;
    devFile = fopen(filename, "r");
    if (!devFile) {
	fprintf(stderr, "failed to open %s\n", filename);
	return -1;
    }
    notfound = 1;
    while (1) {
	if (fgets(line, 100, devFile) == NULL)
	    break;
	sscanf(line, "%u %s %u %u\n", &capacity, devName, &aport, &isafile);
	if (aport == portOffset) {
	    config->capacity = capacity;
	    config->portOffset = aport;
	    strncpy(config->device, devName, 100);
	    notfound = 0;
	    break;
	}

    }
    fclose(devFile);
    return notfound;
}

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char *argv[];
{
    char *config = 0;
    char *tape = "testtape.0", *parent_pid, *pid;
    afs_int32 code;
    char **files;
    int nFiles, i;
    TestInfo ti;

    whoami = argv[0];
    initialize_BUTM_error_table();
    initialize_rx_error_table();
    if (argc < 2)
	goto usage;

    files = (char **)malloc(argc * sizeof(char *));
    nFiles = 0;
    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
	    if (strncmp(argv[i], "-configuration", strlen(argv[i])) == 0)
		config = argv[++i];
	    else if (strncmp(argv[i], "-tapename", strlen(argv[i])) == 0)
		tape = argv[++i];
	    else {
		com_err(whoami, 0, "unexpected command argument: '%s'",
			argv[i]);
		goto usage;
	    }
	} else {
	    struct stat buf;
	    if (stat(argv[i], &buf)) {
		com_err(whoami, errno, "can't stat filename parameter %s\n",
			argv[i]);
	    } else {
		if ((buf.st_mode & S_IFREG) && (buf.st_mode & 0444))
		    files[nFiles++] = argv[i];
		else
		    printf("skipping non-file '%s'\n", argv[i]);
	    }
	}
    }
    if (nFiles == 0)
	goto usage;

    if ((strlen(tape) == 0) || (strlen(tape) >= BU_MAXTAPELEN)) {
	com_err(whoami, 0, "bogus tape name");
	goto usage;
    }
    code = GetDeviceInfo(config, &confInfo);
    if (code) {
	com_err(whoami, 0, "cant find tape config info");
	goto usage;
    }

    ti.tapeName = tape;
    ti.tc_Infop = &confInfo;
    ti.nFiles = nFiles;
    ti.files = files;
    ti.appended = 0;
    printf("%s: Beginning Dump Tests\n", whoami);
    code = LWP_InitializeProcessSupport(1, &parent_pid);
    if (code) {
	com_err(whoami, code, "Can't init LWP lib");
	exit(1);
    }
    code = IOMGR_Initialize();
    if (code) {
	com_err(whoami, code, "Can't init LWP IOMGR lib");
	exit(1);
    }
    /* Perform normal test */
    code =
	LWP_CreateProcess(PerformDumpTest, 100000, 0, (void *)&ti,
			  "BUTM Tester", &pid);
    if (code) {
	com_err(whoami, code, "libbutm.a: Normal Tests failed!. :-( ");
	exit(code);
    }
    LWP_WaitProcess(&ti.dumpDone);
    LWP_DestroyProcess(pid);

    /* Perform Appended Test, on tapes */
    if (!isafile) {
	ti.appended = 1;
	code =
	    LWP_CreateProcess(PerformDumpTest, 100000, 0, (void *)&ti,
			      "BUTM Tester", &pid);
	if (code) {
	    com_err(whoami, code, "libbutm.a: Appended Tests failed!. :-( ");
	    exit(code);
	}

	LWP_WaitProcess(&ti.dumpDone);
	LWP_DestroyProcess(pid);
    }

    IOMGR_Finalize();
    LWP_TerminateProcessSupport();

    exit(code);

  usage:
    printf
	("usage is: %s [-configuration <config file>] [-tapename <name>] <file>*\n",
	 whoami);
    exit(1);
}


int
PerformDumpTest(TestInfo * tip)
{				/* Dump Files into target tape/file */
    struct butm_tapeInfo info;
    struct butm_tapeLabel label;
    int i, past, code;
    struct timeval tp;

    bufferBlock = (struct BufferBlock *)malloc(sizeof(struct BufferBlock));

    info.structVersion = BUTM_MAJORVERSION;
    if (code = butm_file_Instantiate(&info, tip->tc_Infop)) {
	com_err(whoami, code, "instantiating file tape");
	ERROR_EXIT(2);
    }

    memset(&label, 0, sizeof(label));
    gettimeofday(&tp, 0);
    label.structVersion = CUR_TAPE_VERSION;
    label.creationTime = tp.tv_sec;
    label.size = info.tapeSize;

#define T_NAME "AFS 3.5"
#define T_INST ""
#define T_CELL "cellname"
#define T_REALM "cellname"
#define T_COMMENT "Testing the butm library.."
    strcpy(label.AFSName, tip->tapeName);
    strcpy(label.creator.name, T_NAME);
    strcpy(label.creator.instance, T_INST);
    strcpy(label.creator.cell, T_CELL);
    strcpy(label.cell, T_REALM);
    strcpy(label.comment, T_COMMENT);

    if (code = butm_Mount(&info, tip->tapeName)) {
	com_err(whoami, code, "setting up tape");
	ERROR_EXIT(2);
    }
    if (tip->appended) {	/* This is to be an appended dump */
	code = butm_SeekEODump(&info, tip->nFiles + 1);
	if (code) {
	    com_err(whoami, code,
		    "Can't append: Can't position to end of dump on tape\n");
	    ERROR_EXIT(code);
	}
    }
    if ((code = butm_Create(&info, &label, !tip->appended /*Rewind */ ))) {
	com_err(whoami, code, "Writing tape label");
	ERROR_EXIT(2);
    }


    for (i = 0; i < tip->nFiles; i++) {
	int len;
	int fid = open(tip->files[i], O_RDONLY, 0);
	if (fid < 0) {
	    com_err(whoami, errno, "opening file to write on tape");
	    ERROR_EXIT(3);
	}
	if (code = butm_WriteFileBegin(&info)) {
	    com_err(whoami, code, "beginning butm write file");
	    ERROR_EXIT(3);
	}
	while ((len = read(fid, bufferBlock->data, BUTM_BLKSIZE)) > 0) {
	    if (code = butm_WriteFileData(&info, bufferBlock->data, 1, len)) {
		com_err(whoami, code, "butm writing file data");
		ERROR_EXIT(3);
	    }
	}
	if (len < 0) {
	    com_err(whoami, errno, "reading file data");
	    ERROR_EXIT(3);
	}
	if (code = butm_WriteFileEnd(&info)) {
	    com_err(whoami, code, "ending butm write file");
	    ERROR_EXIT(3);
	}
	if (close(fid) < 0) {
	    com_err(whoami, errno, "closing file");
	    ERROR_EXIT(3);
	}
    }
    if ((code = butm_WriteEOT(&info)) || (code = butm_Dismount(&info))) {
	com_err(whoami, code, "finishing up tape");
	ERROR_EXIT(4);
    }

    /* now read the tape back in and make sure everything is OK */

    label.structVersion = BUTM_MAJORVERSION;
    if (code = butm_Mount(&info, tip->tapeName)) {
	com_err(whoami, code, "setting up tape");
	ERROR_EXIT(5);
    }
    if (tip->appended) {	/* This is to be an appended dump */
	code = butm_SeekEODump(&info, tip->nFiles + 1);
	if (code) {
	    com_err(whoami, code,
		    "Can't append: Can't position to end of dump on tape\n");
	    ERROR_EXIT(code);
	}
    }
    if (code = butm_ReadLabel(&info, &label, !tip->appended /*rewind */ )) {
	com_err(whoami, code, "reading tape label");
	ERROR_EXIT(5);
    }
    past = time(0) - label.creationTime;
    if ((past < 0) || (past > 5 * 60)) {
        time_t t = label.creationTime;
	printf("label creation time is long ago: %s\n", ctime(&t));
	ERROR_EXIT(5);
    }
    if (strcmp(label.AFSName, tip->tapeName) != 0) {
	printf("label tape name is bogus: %s, should be %s\n", label.AFSName,
	       tip->tapeName);
	ERROR_EXIT(5);
    }
    if ((strcmp(label.creator.name, T_NAME) != 0)
	|| (strcmp(label.creator.instance, T_INST) != 0)
	|| (strcmp(label.creator.cell, T_CELL) != 0)
	|| (strcmp(label.cell, T_REALM) != 0)
	|| (strcmp(label.comment, T_COMMENT) != 0)) {
	printf("label strings are bad: creator %s.%s@%s from realm %s (%s)\n",
	       label.creator.name, label.creator.instance, label.creator.cell,
	       label.cell, label.comment);
	ERROR_EXIT(5);
    }
    for (i = 0; i < sizeof(label.spare); i++)
	if (*(char *)label.spare) {
	    printf("Label's spare fields not zero\n");
	    ERROR_EXIT(5);
	}

    for (i = 0; i < tip->nFiles; i++) {
	char *tbuffer = bufferBlock->data;
	int tlen;
	char fbuffer[BUTM_BLKSIZE];
	int flen;
	int tprogress, fprogress;
	int fid;

	fid = open(tip->files[i], O_RDONLY, 0);
	if (fid < 0) {
	    com_err(whoami, errno, "Opening %dth file to compare with tape",
		    i + 1);
	    ERROR_EXIT(6);
	}
	if (code = butm_ReadFileBegin(&info)) {
	    com_err(whoami, code, "Beginning butm %dth read file", i + 1);
	    ERROR_EXIT(6);
	}

	tprogress = tlen = fprogress = flen = 0;
	while (1) {
	    memset(tbuffer, 0, BUTM_BLKSIZE);
	    code = butm_ReadFileData(&info, tbuffer, BUTM_BLKSIZE, &tlen);

	    if (code && code != BUTM_STATUS_EOF) {
		com_err(whoami, code, "Reading %dth tape data", i + 1);
		ERROR_EXIT(6);
	    }
	    memset(fbuffer, 0, BUTM_BLKSIZE);
	    flen = read(fid, fbuffer, sizeof(fbuffer));
	    if (flen < 0) {
		com_err(whoami, errno, "Reading %dth file data", i + 1);
		ERROR_EXIT(6);
	    }
	    if (code == BUTM_STATUS_EOF)
		break;
	    if ((tlen == 0) && (flen == 0))
		break;		/* correct termination */
	    if (flen != tlen) {
		printf("File length mismatch for %dth file (%s)\n", i,
		       tip->files[i]);
		ERROR_EXIT(6);
	    }
	    if (tbuffer[tprogress++] != fbuffer[fprogress++]) {
		printf("Data mismatch for %dth file (%s)\n", i + 1,
		       tip->files[i]);
		ERROR_EXIT(6);
	    }
	    if (tlen < BUTM_BLKSIZE)
		break;
	}

	if (code = butm_ReadFileEnd(&info)) {
	    com_err(whoami, code, "Ending butm %dth read file", i + 1);
	    ERROR_EXIT(7);
	}
	if (close(fid) < 0) {
	    com_err(whoami, errno, "Closing %dth file", i + 1);
	    ERROR_EXIT(7);
	}
    }

    if ((info.status & BUTM_STATUS_EOD) == 0) {
	code = butm_ReadFileBegin(&info);
	if (code && (code != BUTM_EOD)) {
	    com_err(whoami, code, "Should have encountered an 'End Of Tape'");
	    ERROR_EXIT(8);
	}
    }
    if (code = butm_Dismount(&info)) {
	com_err(whoami, code, "Finishing up tape");
	ERROR_EXIT(8);
    }

    if (tip->appended)
	printf("%d files Appended: All OK\n", tip->nFiles);
    else
	printf("%d files written: All OK\n", tip->nFiles);
    ERROR_EXIT(0);
  error_exit:
    LWP_SignalProcess(&tip->dumpDone);
    return (code);
}
