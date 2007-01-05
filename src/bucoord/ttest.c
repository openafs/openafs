/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <lwp.h>
#include <rx/rx.h>
#include <rpc/types.h>
#include <afs/bubasics.h>
#include <afs/butc.h>
#include <afs/budb.h>

#include "bc.h"

extern TC_ExecuteRequest();

/* dump information */
static afs_int32 transID = 1000;	/* dump or restore transaction id */
static afs_int32 bytesDumped = 0;

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char **argv;
{
    register int i;
    register afs_int32 code;
    register struct rx_service *tservice;
    struct rx_securityClass *rxsc[1];

    for (i = 1; i < argc; i++) {
	/* parse args */
	if (*argv[i] == '-') {
	    /* switch */
	} else {
	    printf("ttest takes only switches (not '%s')\n", argv[i]);
	    exit(1);
	}
    }
    code = rx_Init(htons(BC_TAPEPORT));
    if (code) {
	printf("ttest: could not initialize rx, code %d.\n", code);
	exit(1);
    }
    rxsc[0] = rxnull_NewServerSecurityObject();
    tservice =
	rx_NewService(0, 1, "tape-controller", rxsc, 1, TC_ExecuteRequest);
    rx_SetMinProcs(tservice, 3);
    rx_SetMaxProcs(tservice, 5);
    rx_StartServer(1);		/* don't donate this process to the rpc pool; it has work to do */
    /* never returns */
    printf("RETURNED FROM STARTSERVER!\n");
    exit(1);
}

STC_LabelTape(acall)
     struct rx_call *acall;
{
    printf("Got a tape labelling call.\n");
    return 0;
}

STC_PerformDump(acall, adumpName, atapeSet, adumpArray, aparent, alevel,
		adumpID)
     struct rx_call *acall;
     char *adumpName;
     afs_int32 aparent, alevel;
     struct tc_tapeSet *atapeSet;
     struct tc_dumpArray *adumpArray;
     afs_int32 *adumpID;
{
    register int i;
    register struct tc_dumpDesc *tdescr;
    register afs_int32 code;
    struct sockaddr_in taddr;
    struct budb_dumpEntry tdentry;
    struct budb_volumeEntry tventry;
    struct budb_tapeEntry ttentry;
    afs_int32 new;

    printf("tape controller received request to start dump %s.\n", adumpName);
    *adumpID = ++transID;	/* send result back to caller */

    memset(&tdentry, 0, sizeof(tdentry));
    tdentry.created = time(0);
    strcpy(tdentry.name, atapeSet->format);
    strcat(tdentry.name, ".");
    strcat(tdentry.name, adumpName);
    tdentry.parent = aparent;
    tdentry.level = alevel;
    tdentry.incTime = 0;	/* useless? */
    tdentry.nVolumes = 1000000000;	/* bogus, but not important */
    tdentry.tapes.a = 1;	/* a*x+b is tape numbering scheme */
    tdentry.tapes.b = 0;
    tdentry.tapes.maxTapes = 1000000000;	/* don't care */
    strcpy(tdentry.tapes.format, atapeSet->format);	/* base name (e.g. sys) */
    strcat(tdentry.tapes.format, ".");
    strcat(tdentry.tapes.format, adumpName);	/* e.g. .daily */
    strcat(tdentry.tapes.format, ".%d");	/* so we get basename.0, basename.1, etc */
    code = bcdb_CreateDump(&tdentry);
    if (code) {
	printf("ttape: failed to create dump, code %d\n", code);
	return code;
    }
    printf("created dump %d\n", tdentry.id);

    /* start tape (preent all fits on one tape at first */
    memset(&ttentry, 0, sizeof(ttentry));
    sprintf(ttentry.name, tdentry.tapes.format, 1);
    ttentry.written = time(0);
    ttentry.dump = tdentry.id;	/* dump we're in */
    ttentry.seq = 0;
    ttentry.nVolumes = 0;	/* perhaps we'll adjust during dump */
    ttentry.flags = BUDB_TAPE_BEINGWRITTEN;	/* starting I/O */
    code = bcdb_UseTape(&ttentry, &new);
    if (code) {
	printf("ttape: failed to start tape %s, code %d\n", ttentry.name,
	       code);
	return code;
    }

    tdescr = adumpArray->tc_dumpArray_val;
    for (i = 0; i < adumpArray->tc_dumpArray_len; i++, tdescr++) {
	memcpy(&taddr, tdescr->hostID, sizeof(taddr));
	printf("dumping volid %s(%d) from host %08x since date %d\n",
	       tdescr->name, tdescr->vid, taddr.sin_addr.s_addr,
	       tdescr->date);
	memset(&tventry, 0, sizeof(tventry));
	strcpy(tventry.name, tdescr->name);
	tventry.clone = tdescr->date;
	tventry.seq = 0;	/* frag in volume */
	tventry.incTime = tdescr->date;	/* date from which this is an incremental? */
	tventry.id = tdescr->vid;
	tventry.dump = tdentry.id;
	strcpy(tventry.tape, ttentry.name);
	tventry.position = i;
	tventry.flags = (BUDB_VOL_LASTFRAG | BUDB_VOL_FIRSTFRAG);
	code = bcdb_AddVolume(&tventry);
	if (code) {
	    printf("failed to append volume entry for volume %d, code %d\n",
		   tdescr->vid, code);
	    return code;
	}
    }

    ttentry.flags = BUDB_TAPE_WRITTEN;
    code = bcdb_FinishTape(&ttentry);
    if (code) {
	printf("ttape: failed to finish tape, code %d\n", code);
	return code;
    }

    code = bcdb_FinishDump(&tdentry);
    if (code) {
	printf("ttest: failed to finish dump, code %d\n", code);
	return code;
    }
    bytesDumped = 0;
    return 0;
}

STC_PerformRestore(acall, aname, arestore, adumpID)
     struct rx_call *acall;
     char *aname;
     struct tc_restoreArray *arestore;
     afs_int32 *adumpID;
{
    register int i;
    register struct tc_restoreDesc *tdescr;
    struct sockaddr_in taddr;

    printf("tape controller received request to start restore %s.\n", aname);
    tdescr = arestore->tc_restoreArray_val;
    for (i = 0; i < arestore->tc_restoreArray_len; i++, tdescr++) {
	memcpy(&taddr, tdescr->hostID, sizeof(taddr));
	printf
	    ("restoring frag %d of volume %s from tape %s at position %d.\n    New name is '%s', new vid is %d, new host is %08x, new partition is %d\n",
	     tdescr->frag, tdescr->oldName, tdescr->tapeName,
	     tdescr->position, tdescr->newName, tdescr->vid,
	     taddr.sin_addr.s_addr, tdescr->partition);
    }
    *adumpID = ++transID;
    bytesDumped = 0;
    return 0;
}

/* check the status of a dump; the tape coordinator is assumed to sit on
    the status of completed dumps for a reasonable period (2 - 12 hours)
    so that they can be examined later */
STC_CheckDump(acall, adumpID, astatus)
     struct rx_call *acall;
     afs_int32 adumpID;
     struct tc_dumpStat *astatus;
{
    if (adumpID != transID)
	return 2;
    astatus->dumpID = adumpID;
    astatus->bytesDumped = (bytesDumped += 1470);
    astatus->flags = 0;
    if (bytesDumped > 2000)
	astatus->flags = TC_STAT_DONE;
    return 0;
}

STC_AbortDump(acall, adumpID)
     struct rx_call *acall;
     afs_int32 adumpID;
{
    return 0;
}

/* this call waits for a dump to complete; it ties up an LWP on the tape 
coordinator */
STC_WaitForDump()
{
    return 1;
}

STC_EndDump(acall, adumpID)
     struct rx_call *acall;
     afs_int32 adumpID;
{
    return 0;
}

STC_GetTMInfo(acall)
     struct rx_call *acall;
{
    return 0;
}
