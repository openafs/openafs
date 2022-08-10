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

#include <roken.h>

#include <afs/com_err.h>
#include <lwp.h>
#include <afs/butm.h>
#include "butm_prototypes.h"
#include "AFS_component_version_number.c"


int isafile = 0;
int debugLevel = 1;

static char *whoami = "TEST FAIL";

#define PASS(str, err) printf("TEST: %s\n", str); \
                       if (code != err) \
                       { \
                          printf("FAILURE: expected %lu; got %lu\n", err, code); \
                          if (code) afs_com_err(whoami, code, "Failed test %s", str); \
		       } \
                       else printf("PASSED; got %lu\n", err); \
                       printf("\n");

#define PASSq(str, err) if (code != err) \
                        { \
			   printf("TEST: %s\n", str); \
                           printf("FAILURE: expected %lu; got %lu\n", err, code); \
                           if (code) afs_com_err(whoami, code, "Failed test %s", str); \
			   printf("\n"); \
		        }
#define NOREWIND 0
#define REWIND   1

extern char tapeBlock[];
char *dataBlock;
long dataSize;

int
main(int argc, char *argv[])
{
    long code;
    int nBytes = 0;
    PROCESS parent_pid;

    struct butm_tapeInfo tapeInfo;
    struct tapeConfig tapeConfig;

    /* -------------
     * General Setup
     * ------------- */
    initialize_BUTM_error_table();

    tapeInfo.structVersion = BUTM_MAJORVERSION;

    strcpy(tapeConfig.device, "/dev/rmt0");
    tapeConfig.capacity = 100;
    tapeConfig.fileMarkSize = 16384;
    tapeConfig.portOffset = 0;

    code = LWP_InitializeProcessSupport(1, &parent_pid);
    if (code) {
	afs_com_err(whoami, code, "Can't init LWP lib");
	exit(1);
    }
    code = IOMGR_Initialize();
    if (code) {
	afs_com_err(whoami, code, "Can't init LWP IOMGR lib");
	exit(1);
    }

    /* ------------- */
    /* START TESTING */
    /* ------------- */

    code = butm_file_Instantiate(&tapeInfo, &tapeConfig);
    PASSq("file Instantiate", 0L);

    code = tapeInfo.ops.writeFileBegin(&tapeInfo);
    PASS("Tape not mounted for writeFileBegin", BUTM_NOMOUNT);

    code = tapeInfo.ops.readFileBegin(&tapeInfo);
    PASS("Null info ptr for writeFileBegin", BUTM_NOMOUNT);

    code = tapeInfo.ops.writeFileEnd(&tapeInfo);
    PASS("Tape not mounted for writeFileEnd", BUTM_NOMOUNT);

    code = tapeInfo.ops.readFileEnd(&tapeInfo);
    PASS("Null info ptr for writeFileEnd", BUTM_NOMOUNT);

    /* ---------- */

    code = tapeInfo.ops.mount(&tapeInfo, "TAPE_NAME");
    PASSq("Mount tape", 0L);

    code = tapeInfo.ops.writeFileEnd(&tapeInfo);
    PASS("Write a fileEnd as first entry on tape", BUTM_BADOP);

    code = tapeInfo.ops.readFileEnd(&tapeInfo);
    PASS("Read a fileEnd as first entry on tape", BUTM_BADOP);

    code = tapeInfo.ops.dismount(&tapeInfo);
    PASSq("Unmount the tape drive", 0L);

    /* ---------- */

    code = tapeInfo.ops.mount(&tapeInfo, "TAPE_NAME");
    PASSq("Mount tape", 0L);

    code = tapeInfo.ops.writeFileBegin(&tapeInfo);
    PASS("Write a fileBegin", 0L);

    code = tapeInfo.ops.writeFileEnd(&tapeInfo);
    PASS("Write a fileEnd", 0L);

    code = tapeInfo.ops.dismount(&tapeInfo);
    PASSq("Unmount the tape drive", 0L);

    /* ---------- */

    code = tapeInfo.ops.mount(&tapeInfo, "TAPE_NAME");
    PASSq("Mount tape", 0L);

    code = tapeInfo.ops.readFileBegin(&tapeInfo);
    PASS("Read a fileBegin", 0L);

    code = tapeInfo.ops.readFileEnd(&tapeInfo);
    PASS("Read a fileEnd", 0L);

    code = tapeInfo.ops.dismount(&tapeInfo);
    PASSq("Unmount the tape drive", 0L);

    /* file_WriteFileData and file_ReadFileData tests */
    /* ---------------------------------------------- */

    dataBlock = &tapeBlock[sizeof(struct blockMark)];
    dataSize = 16384 - sizeof(struct blockMark);


    code = butm_file_Instantiate(&tapeInfo, &tapeConfig);
    PASSq("file Instantiate", 0L);

    /* ---------- */

    code = tapeInfo.ops.writeFileData(&tapeInfo, NULL, dataSize, 0);
    PASS("Null data ptr for writeFileData", BUTM_BADARGUMENT);

    code = tapeInfo.ops.readFileData(&tapeInfo, NULL, dataSize, &nBytes);
    PASS("Null data ptr for readFileData", BUTM_BADARGUMENT);

    /* ---------- */

    code = tapeInfo.ops.writeFileData(&tapeInfo, dataBlock, -1, 0);
    PASS("Neg. data size for writeFileData", BUTM_BADARGUMENT);

    code = tapeInfo.ops.readFileData(&tapeInfo, dataBlock, -1, &nBytes);
    PASS("Neg. data size for readFileData", BUTM_BADARGUMENT);

    /* ---------- */

    code = tapeInfo.ops.writeFileData(&tapeInfo, dataBlock, dataSize + 1, 0);
    PASS("Large data size for writeFileData", BUTM_BADARGUMENT);

    code =
	tapeInfo.ops.readFileData(&tapeInfo, dataBlock, dataSize + 1, &nBytes);
    PASS("Large data size for readFileData", BUTM_BADARGUMENT);

    /* ---------- */

    code = tapeInfo.ops.readFileData(&tapeInfo, dataBlock, dataSize, NULL);
    PASS("Null nBytes ptr for readFileData", BUTM_BADARGUMENT);

    /* ---------- */

    code = tapeInfo.ops.writeFileData(&tapeInfo, dataBlock, dataSize, 0);
    PASS("First write for WriteFileData", BUTM_BADOP);

    code = tapeInfo.ops.readFileData(&tapeInfo, dataBlock, dataSize, &nBytes);
    PASS("First read for readFileData", BUTM_BADOP);

    return 0;
}
