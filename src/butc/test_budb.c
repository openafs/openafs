/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <sys/types.h>
#include <netinet/in.h>

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/budb_client.h>
#include <afs/budb.h>
#include <rx/rx_globals.h>

extern struct udbHandleS udbHandle;

#define ERROR(code) {err = code; printf("   - Got error %d.\n", err); goto error;}

int
connect_buserver()
{
    int code, err = 0;
    char cellName[64];
    struct ubik_client *cstruct;
    int version;

    /*
     * Setup RX connection and establish connection with the buserver
     */
    code = rx_Init(htons(0));
    if (code) {
	printf("Error in rx_Init call\n");
	ERROR(code);
    }

    rx_SetRxDeadTime(60);

    /* 
     * Connect to buserver 
     */
    cellName[0] = '\0';
    code = udbClientInit(0, 0, cellName);
    if (code) {
	printf("Error in udbClientInit call\n");
	ERROR(code);
    }

    /* Get the versin */
    code = ubik_BUDB_T_GetVersion(udbHandle.uh_client, 0, &version);
    if (code) {
	printf("Error in ubik_Call to BUDB_T_GetVersion\n");
	ERROR(code);
    }
    printf("BUDB Server Version = %d\n", version);

  error:
    return err;
}

int
verifyDb()
{
    int code, err = 0;
    afs_int32 status, orphans, host;

    code =
	ubik_BUDB_DbVerify(udbHandle.uh_client, 0, &status, &orphans,
		  &host);
    if (code) {
	printf("Error in ubik_Call to BUDB_DbVerify\n");
	ERROR(code);
    }

    if (!status)
	printf("DATABASE OK     ");
    else
	printf("DATABASE NOT OK ");
    printf("- orphans %d; host %d.%d.%d.%d\n", orphans,
	   ((host & 0xFF000000) >> 24), ((host & 0xFF0000) >> 16),
	   ((host & 0xFF00) >> 8), (host & 0xFF));

  error:
    return err;
}

int
deleteDump(dumpID)
     afs_int32 dumpID;
{
    int code;
    budb_dumpsList dumps;

    dumps.budb_dumpsList_len = 0;
    dumps.budb_dumpsList_val = 0;

    code = bcdb_deleteDump(dumpID, 0, 0, &dumps);
    return code;
}

#define NPASS     50
#define NDUMPS    4
#define NTAPES    2
#define NVOLUMES 10

int
main()
{
    int code, err = 0;
    int pass = 0;
    int d, t, v;
    afs_int32 newTape;
    struct budb_dumpEntry dumpEntry[NDUMPS];
    struct budb_tapeEntry tapeEntry[NTAPES];
    struct budb_volumeEntry volumeEntry[NVOLUMES];

    code = connect_buserver();
    if (code) {
	printf("Error in connect_buserver call\n");
	ERROR(code);
    }

    for (pass = 0; pass < NPASS; pass++) {
	printf("PASS %d\n", pass + 1);


	for (d = 0; d < NDUMPS; d++) {
	    /* ************************ */
	    /* Create and finish a dump */
	    /* ************************ */
	    dumpEntry[d].id = 0;
	    dumpEntry[d].parent = 0;
	    dumpEntry[d].level = 0;
	    dumpEntry[d].flags = 0;
	    strcpy(dumpEntry[d].volumeSetName, "TestVolSetName");
	    strcpy(dumpEntry[d].dumpPath, "/TestDumpPath");
	    strcpy(dumpEntry[d].name, "TestVolSetName.TestDumpPath");
	    dumpEntry[d].created = 0;
	    dumpEntry[d].incTime = 0;
	    dumpEntry[d].nVolumes = 0;
	    dumpEntry[d].tapes.id = 0;
	    strcpy(dumpEntry[d].tapes.tapeServer, "");
	    strcpy(dumpEntry[d].tapes.format,
		   "TestVolSetName.TestDumpPath.%d");
	    dumpEntry[d].tapes.maxTapes = 1;
	    dumpEntry[d].tapes.a = 0;
	    dumpEntry[d].tapes.b = 1;
	    strcpy(dumpEntry[d].dumper.name, "admin");
	    strcpy(dumpEntry[d].dumper.instance, "");
	    strcpy(dumpEntry[d].dumper.cell, "");
	    dumpEntry[d].initialDumpID = 0;
	    if (d == 1)
		dumpEntry[d].initialDumpID = dumpEntry[0].id;
	    dumpEntry[d].appendedDumpID = 0;

	    code = bcdb_CreateDump(&dumpEntry[d]);
	    if (code) {
		printf("Error in bcdb_CreateDump call\n");
		ERROR(code);
	    }
	    printf("\nCreated dump %s (DumpID %u)\n", dumpEntry[d].name,
		   dumpEntry[d].id);

	    for (t = 0; t < NTAPES; t++) {
		/* ************************ */
		/* Create and finish a tape */
		/* ************************ */
		sprintf(tapeEntry[t].name, "TestVolSetName.TestDumpPath.%d",
			t + 1);
		tapeEntry[t].flags = 0;
		tapeEntry[t].written = 0;
		tapeEntry[t].expires = 0;	/* date tape expires */
		tapeEntry[t].nMBytes = 0;
		tapeEntry[t].nBytes = 0;
		tapeEntry[t].nFiles = 0;
		tapeEntry[t].nVolumes = 0;
		tapeEntry[t].seq = (t + 1);	/* Tape in sequence */
		tapeEntry[t].tapeid = 0;
		tapeEntry[t].useCount = 999;	/* Number of time tape is used */
		tapeEntry[t].useKBytes = 0;
		tapeEntry[t].dump = dumpEntry[d].id;

		bcdb_UseTape(&tapeEntry[t], &newTape);
		if (code) {
		    printf("Error in bcdb_UseTape call\n");
		    ERROR(code);
		}
		printf("   Created tape %s (%u)\n", tapeEntry[t].name,
		       tapeEntry[t].dump);

		for (v = 0; v < NVOLUMES; v++) {
		    /* ************************* */
		    /* Create a volume           */
		    /* ************************* */
		    sprintf(volumeEntry[v].name, "TestVolumeName.%d",
			    (t * NVOLUMES) + (v + 1));
		    volumeEntry[v].flags =
			(BUDB_VOL_FIRSTFRAG | BUDB_VOL_LASTFRAG);
		    volumeEntry[v].id = 1234567890;	/* volume id */
		    strcpy(volumeEntry[v].server, "");
		    volumeEntry[v].partition = 0;
		    volumeEntry[v].tapeSeq = 0;
		    volumeEntry[v].position = v + 2;	/* positin on tape */
		    volumeEntry[v].clone = 0;	/* clone date */
		    volumeEntry[v].incTime = 0;
		    volumeEntry[v].startByte = 0;
		    volumeEntry[v].nBytes = (v + 1) * 100000;	/* vary size of volume */
		    volumeEntry[v].seq = 1;	/* The first fragment */
		    volumeEntry[v].dump = dumpEntry[d].id;	/* the dump id */
		    strcpy(volumeEntry[v].tape, tapeEntry[t].name);	/* the tape name */

		    code = bcdb_AddVolume(&volumeEntry[v]);
		    if (code) {
			printf("Error in bcdb_AddVolume call\n");
			ERROR(code);
		    }
		    printf("      Added volume %s\n", volumeEntry[v].name);
		}

		tapeEntry[t].nFiles = 77777;
		tapeEntry[t].useKBytes = 88888;
		code = bcdb_FinishTape(&tapeEntry[t]);
		if (code) {
		    printf("Error in bcdb_FinishTape call\n");
		    ERROR(code);
		}
		printf("   Finished tape %s (%u)\n", tapeEntry[t].name,
		       tapeEntry[t].dump);
	    }

	    code = bcdb_FinishDump(&dumpEntry[d]);
	    if (code) {
		printf("Error in bcdb_FinishDump call\n");
		ERROR(code);
	    }
	    printf("Finished dump %s (DumpID %u)\n", dumpEntry[d].name,
		   dumpEntry[d].id);

	    code = verifyDb();
	    if (code) {
		printf("Error in verifyDb call\n");
		ERROR(code);
	    }
	}

	/* ********************************************** */
	/* Delete one of the dumps - only if not appended */
	/* ********************************************** */
	if (!dumpEntry[(pass % NDUMPS)].initialDumpID) {
	    code = deleteDump(dumpEntry[(pass % NDUMPS)].id);
	    if (code) {
		printf("Error in deleteDump call\n");
		ERROR(code);
	    }
	    printf("Deleted DumpID %u\n", dumpEntry[(pass % NDUMPS)].id);
	}

	code = verifyDb();
	if (code) {
	    printf("Error in verifyDb call\n");
	    ERROR(code);
	}
    }

  error:
    return err;
}
