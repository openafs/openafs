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

#include <errno.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <afs/com_err.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <lwp.h>
#include <afs/butm.h>
#include "AFS_component_version_number.c"



static char *whoami = "TEST FAIL";

#define NULL (char *)0
#define PASS(str, err) printf("TEST: %s\n", str); \
                       if (code != err) \
                       { \
                          printf("FAILURE: expected %u; got %u\n", err, code); \
                          if (code) afs_com_err(whoami, code, ""); \
		       } \
                       else printf("PASSED; got %u\n", err); \
                       printf("\n");

#define PASSq(str, err) if (code != err) \
                        { \
			   printf("TEST: %s\n", str); \
                           printf("FAILURE: expected %u; got %u\n", err, code); \
                           if (code) afs_com_err(whoami, code, ""); \
			   printf("\n"); \
		        }
#define NOREWIND 0
#define REWIND   1

char tapeBlock[16384];
char *dataBlock;
long dataSize;

main(argc, argv)
     int argc;
     char *argv[];
{
    char *config = 0;
    char *tape = "testtape.0";
    long code;
    char **files;
    int nFiles;
    int i;
    int past;

    struct butm_tapeInfo tapeInfo;
    struct tapeConfig tapeConfig;

    struct butm_tapeLabel tapeLabelWrite, tapeLabelRead;

    /* -------------
     * General Setup 
     * ------------- */
    initialize_BUTM_error_table();

    tapeInfo.structVersion = BUTM_MAJORVERSION;

    strcpy(tapeConfig.device, "/dev/rmt0");
    tapeConfig.capacity = 100;
    tapeConfig.fileMarkSize = 16384;
    tapeConfig.portOffset = 0;
    tapeConfig.aixScsi = 0;

    goto start;

    /* ------------- */
    /* START TESTING */
    /* ------------- */

    /* butm_file_Instantiate tests */
    /* --------------------------- */
    code = butm_file_Instantiate(NULL, &tapeConfig);
    PASS("Bad info paramater", BUTM_BADARGUMENT)

	code = butm_file_Instantiate(&tapeInfo, NULL);
    PASS("Bad config parameter", BUTM_BADCONFIG);

    tapeInfo.structVersion = 0;
    code = butm_file_Instantiate(&tapeInfo, &tapeConfig);
    PASS("Bad version number", BUTM_OLDINTERFACE);
    tapeInfo.structVersion = BUTM_MAJORVERSION;

    tapeConfig.capacity = 0;
    code = butm_file_Instantiate(&tapeInfo, &tapeConfig);
    PASS("zero capacity tape", BUTM_BADCONFIG);
    tapeConfig.capacity = 100;

    tapeConfig.fileMarkSize = 0;
    code = butm_file_Instantiate(&tapeInfo, &tapeConfig);
    PASS("zero length filemark", BUTM_BADCONFIG);
    tapeConfig.fileMarkSize = 16384;

    strcpy(tapeConfig.device, "");
    code = butm_file_Instantiate(&tapeInfo, &tapeConfig);
    PASS("no tape device name", BUTM_BADCONFIG);
    strcpy(tapeConfig.device, "/dev/rmt0");

    /* file_Mount and file_Dismount tests */
    /* ---------------------------------- */

    strcpy(tapeConfig.device, "/dev/Bogus");
    code = butm_file_Instantiate(&tapeInfo, &tapeConfig);
    PASSq("file Instantiate", 0);
    strcpy(tapeConfig.device, "/dev/rmt0");

    code = tapeInfo.ops.mount(NULL, "TAPE_NAME");
    PASS("Null tapeInfo ptr for mount", BUTM_BADARGUMENT);

    code = tapeInfo.ops.dismount(NULL);
    PASS("Null info ptr for dismount", BUTM_BADARGUMENT);

    /* --------- */

    code = tapeInfo.ops.mount(&tapeInfo, NULL);
    PASS("NULL virtual tape name", BUTM_BADARGUMENT);

    code = tapeInfo.ops.mount(&tapeInfo, "-MORE_THAN_THIRTY_TWO_CHARACTERS-");
    PASS(">32 character tape name", BUTM_BADARGUMENT);

    code = tapeInfo.ops.mount(&tapeInfo, "TAPE_NAME");
    PASS("Bogus tape name", BUTM_MOUNTFAIL);

    /* --------- */

    code = butm_file_Instantiate(&tapeInfo, &tapeConfig);
    PASSq("file Instantiate", 0);

    code = tapeInfo.ops.mount(&tapeInfo, "TAPE_NAME");
    PASS("Open tape drive", 0);

    code = tapeInfo.ops.mount(&tapeInfo, "TAPE_NAME");
    PASS("Open tape drive 2nd time", BUTM_PARALLELMOUNTS);

    code = tapeInfo.ops.dismount(&tapeInfo);
    PASSq("Unmount the tape drive", 0);

    code = tapeInfo.ops.dismount(&tapeInfo);
    PASS("Unmount the tape drive which is not mounted", 0);

    /* file_writeLabel and file_readLabel tests */
    /* ---------------------------------------- */

    code = butm_file_Instantiate(&tapeInfo, &tapeConfig);
    PASSq("file Instantiate", 0);

    code = tapeInfo.ops.create(NULL, &tapeLabelWrite, REWIND);
    PASS("NULL info to Write label", BUTM_BADARGUMENT);

    code = tapeInfo.ops.readLabel(NULL, &tapeLabelWrite, REWIND);
    PASS("NULL info to Read Label", BUTM_BADARGUMENT);

    /* ---------- */

    code = tapeInfo.ops.create(&tapeInfo, &tapeLabelWrite, REWIND);
    PASS("Write label to unmounted tape", BUTM_NOMOUNT);

    code = tapeInfo.ops.readLabel(&tapeInfo, &tapeLabelRead, REWIND);
    PASS("Read label of unmounted tape", BUTM_NOMOUNT);

    /* ---------- */

    code = tapeInfo.ops.mount(&tapeInfo, "TAPE_NAME");
    PASSq("Mount tape", 0);

    memset(tapeLabelWrite, 0, sizeof(tapeLabelWrite));
    tapeLabelWrite.structVersion = CUR_TAPE_VERSION;
    tapeLabelWrite.creationTime = time(0);
    tapeLabelWrite.expirationDate = time(0);
    strcpy(tapeLabelWrite.name, "TAPE_LABEL_NAME");
    /* tapeLabelWrite.creator. */
    strcpy(tapeLabelWrite.cell, "CELLNAME");
    tapeLabelWrite.dumpid = 999;
    tapeLabelWrite.useCount = 8888;
    strcpy(tapeLabelWrite.comment, "THIS IS THE COMMENT FIELD");
    tapeLabelWrite.size = 77777;
    strcpy(tapeLabelWrite.dumpPath, "/FULL/WEEK3/DAY4/HOUR7");

    code = tapeInfo.ops.create(&tapeInfo, &tapeLabelWrite, REWIND);
    PASS("Write a label", 0);

    code = tapeInfo.ops.readLabel(&tapeInfo, &tapeLabelRead, REWIND);
    PASS("Read a label", 0);

    if (memcmp(&tapeLabelWrite, &tapeLabelRead, sizeof(tapeLabelWrite)))
	printf("FAILURE: Label Read is not same as label Written\n");
    else
	printf("PASSED: Label Read is same as label Written\n");

    code = tapeInfo.ops.dismount(&tapeInfo);
    PASSq("Unmount the tape drive", 0);

    /* ---------- */

    code = tapeInfo.ops.mount(&tapeInfo, "TAPE_NAME");
    PASSq("Mount tape", 0);

    code = tapeInfo.ops.create(&tapeInfo, NULL, REWIND);
    PASS("Write a NULL label", BUTM_BADARGUMENT);

    tapeLabelWrite.structVersion = 0;
    code = tapeInfo.ops.create(&tapeInfo, &tapeLabelWrite, REWIND);
    PASS("Write label with bad version in it", BUTM_OLDINTERFACE);
    tapeLabelWrite.structVersion = CUR_TAPE_VERSION;

    code = tapeInfo.ops.dismount(&tapeInfo);
    PASSq("Unmount the tape drive", 0);

    /* file_WriteFileBegin and file_ReadFileBegin tests */
    /* file_WriteFileEnd   and file_ReadFileEnd   tests */
    /* ------------------------------------------------ */

  start:
    code = butm_file_Instantiate(&tapeInfo, &tapeConfig);
    PASSq("file Instantiate", 0);

    code = tapeInfo.ops.writeFileBegin(NULL);
    PASS("Null info ptr for writeFileBegin", BUTM_BADARGUMENT);

    code = tapeInfo.ops.readFileBegin(NULL);
    PASS("Null info ptr for readFileBegin", BUTM_BADARGUMENT);

    code = tapeInfo.ops.writeFileEnd(NULL);
    PASS("Null info ptr for writeFileEnd", BUTM_BADARGUMENT);

    code = tapeInfo.ops.readFileEnd(NULL);
    PASS("Null info ptr for readFileEnd", BUTM_BADARGUMENT);

    /* ---------- */

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
    PASSq("Mount tape", 0);

    code = tapeInfo.ops.writeFileEnd(&tapeInfo);
    PASS("Write a fileEnd as first entry on tape", BUTM_BADOP);

    code = tapeInfo.ops.readFileEnd(&tapeInfo);
    PASS("Read a fileEnd as first entry on tape", BUTM_BADOP);

    code = tapeInfo.ops.dismount(&tapeInfo);
    PASSq("Unmount the tape drive", 0);

    /* ---------- */

    code = tapeInfo.ops.mount(&tapeInfo, "TAPE_NAME");
    PASSq("Mount tape", 0);

    code = tapeInfo.ops.writeFileBegin(&tapeInfo);
    PASS("Write a fileBegin", 0);

    code = tapeInfo.ops.writeFileEnd(&tapeInfo);
    PASS("Write a fileEnd", 0);

    code = tapeInfo.ops.dismount(&tapeInfo);
    PASSq("Unmount the tape drive", 0);

    /* ---------- */

    code = tapeInfo.ops.mount(&tapeInfo, "TAPE_NAME");
    PASSq("Mount tape", 0);

    code = tapeInfo.ops.readFileBegin(&tapeInfo);
    PASS("Read a fileBegin", 0);

    code = tapeInfo.ops.readFileEnd(&tapeInfo);
    PASS("Read a fileEnd", 0);

    code = tapeInfo.ops.dismount(&tapeInfo);
    PASSq("Unmount the tape drive", 0);

    /* file_WriteFileData and file_ReadFileData tests */
    /* ---------------------------------------------- */

    dataBlock = &tapeBlock[sizeof(struct blockMark)];
    dataSize = 16384 - sizeof(struct blockMark);


    code = butm_file_Instantiate(&tapeInfo, &tapeConfig);
    PASSq("file Instantiate", 0);

    code = tapeInfo.ops.writeFileData(NULL, dataBlock, dataSize);
    PASS("Null info ptr for writeFileData", BUTM_BADARGUMENT);

    code = tapeInfo.ops.readFileData(NULL, dataBlock, dataSize, nBytes);
    PASS("Null info ptr for readFileData", BUTM_BADARGUMENT);

    /* ---------- */

    code = tapeInfo.ops.writeFileData(&tapeInfo, NULL, dataSize);
    PASS("Null data ptr for writeFileData", BUTM_BADARGUMENT);

    code = tapeInfo.ops.readFileData(&tapeInfo, NULL, dataSize, nBytes);
    PASS("Null data ptr for readFileData", BUTM_BADARGUMENT);

    /* ---------- */

    code = tapeInfo.ops.writeFileData(&tapeInfo, dataBlock, -1);
    PASS("Neg. data size for writeFileData", BUTM_BADARGUMENT);

    code = tapeInfo.ops.readFileData(&tapeInfo, dataBlock, -1, nBytes);
    PASS("Neg. data size for readFileData", BUTM_BADARGUMENT);

    /* ---------- */

    code = tapeInfo.ops.writeFileData(&tapeInfo, dataBlock, dataSize + 1);
    PASS("Large data size for writeFileData", BUTM_BADARGUMENT);

    code =
	tapeInfo.ops.readFileData(&tapeInfo, dataBlock, dataSize + 1, nBytes);
    PASS("Large data size for readFileData", BUTM_BADARGUMENT);

    /* ---------- */

    code = tapeInfo.ops.readFileData(&tapeInfo, dataBlock, dataSize, NULL);
    PASS("Null nBytes ptr for readFileData", BUTM_BADARGUMENT);

    /* ---------- */

    code = tapeInfo.ops.writeFileData(&tapeInfo, dataBlock, dataSize);
    PASS("First write for WriteFileData", BUTM_BADOP);

    code = tapeInfo.ops.readFileData(&tapeInfo, dataBlock, dataSize, nBytes);
    PASS("First read for readFileData", BUTM_BADOP);

  end:return;
}
