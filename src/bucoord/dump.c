/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * ALL RIGHTS RESERVED
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <sys/types.h>
#include <afs/cmd.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#endif
#include <lwp.h>
#include <rx/rx.h>
#include <afs/bubasics.h>
#include <afs/butc.h>
#include <afs/com_err.h>
#include <lock.h>		/* for checking TC_ABORTFAILED. PA */
#include <afs/tcdata.h>		/* for checking TC_ABORTFAILED. PA */
#include <afs/butc.h>

#include "bc.h"
#include "error_macros.h"

struct bc_dumpTask bc_dumpTasks[BC_MAXSIMDUMPS];

extern char *bc_CopyString();
extern void bc_HandleMisc();
extern char *whoami;
extern char *tailCompPtr();
extern statusP createStatusNode();
extern afs_int32 bc_jobNumber();

extern afs_int32 lastTaskCode;

#define HOSTADDR(sockaddr) (sockaddr)->sin_addr.s_addr

/* bc_Dumper
 *	called (indirectly) to make a dump
 * entry:
 *	aindex - index into dumpTask array, contains all the information 
 * 		 relevant to the dump
 */
bc_Dumper(aindex)
{
    struct rx_connection *tconn;
    register struct bc_volumeDump *tde;
    afs_int32 count, port;
    struct tc_dumpDesc *volDesc = 0;
    struct tc_dumpArray volArray;
    char *baseNamePtr;
    statusP statusPtr;

    struct tc_dumpInterface dumpInterface;
    struct tc_dumpInterface *tcdiPtr = &dumpInterface;
    register struct bc_dumpTask *dumpTaskPtr;

    register afs_int32 code = 0;

    dumpTaskPtr = &bc_dumpTasks[aindex];

    if (!dumpTaskPtr->portOffset || (dumpTaskPtr->portCount == 0))
	port = 0;
    else
	port = dumpTaskPtr->portOffset[0];

    code = ConnectButc(dumpTaskPtr->config, port, &tconn);
    if (code)
	return (code);

    /* count number of volumes to be dumped and
     * build array of volumes to be sent to backup system 
     */
    for (count = 0, tde = dumpTaskPtr->volumes; tde;
	 tde = tde->next, count++);
    volDesc =
	(struct tc_dumpDesc *)malloc(count * sizeof(struct tc_dumpDesc));
    if (!volDesc) {
	afs_com_err(whoami, BC_NOMEM, "");
	ERROR(BC_NOMEM);
    }

    for (count = 0, tde = dumpTaskPtr->volumes; tde; tde = tde->next, count++) {
	strcpy(volDesc[count].name, tde->name);
	volDesc[count].vid = tde->vid;
	volDesc[count].vtype = tde->volType;
	volDesc[count].partition = tde->partition;
	volDesc[count].hostAddr = HOSTADDR(&tde->server);	/* the internet address */
	volDesc[count].date = tde->date;
	volDesc[count].cloneDate = tde->cloneDate;	/* Not yet known */
    }

    volArray.tc_dumpArray_len = count;	/* element count */
    volArray.tc_dumpArray_val = volDesc;	/* and data */

    baseNamePtr = tailCompPtr(dumpTaskPtr->dumpName);

    /* setup the interface structure */
    memset(tcdiPtr, 0, sizeof(*tcdiPtr));

    /* general */
    strcpy(tcdiPtr->dumpPath, dumpTaskPtr->dumpName);
    strcpy(tcdiPtr->volumeSetName, dumpTaskPtr->volSetName);

    /* tapeset */
    strcpy(tcdiPtr->tapeSet.format, dumpTaskPtr->volSetName);
    strcat(tcdiPtr->tapeSet.format, ".");
    strcat(tcdiPtr->tapeSet.format, baseNamePtr);
    strcat(tcdiPtr->tapeSet.format, ".%d");
    tcdiPtr->tapeSet.a = 1;
    tcdiPtr->tapeSet.b = 1;
    tcdiPtr->tapeSet.maxTapes = 1000000000;
    tcdiPtr->tapeSet.expDate = dumpTaskPtr->expDate;	/* PA */
    tcdiPtr->tapeSet.expType = dumpTaskPtr->expType;

    /* construct dump set name */
    strcpy(tcdiPtr->dumpName, dumpTaskPtr->volSetName);
    strcat(tcdiPtr->dumpName, ".");
    strcat(tcdiPtr->dumpName, baseNamePtr);

    tcdiPtr->parentDumpId = dumpTaskPtr->parentDumpID;
    tcdiPtr->dumpLevel = dumpTaskPtr->dumpLevel;
    tcdiPtr->doAppend = dumpTaskPtr->doAppend;

    /* start the dump on the tape coordinator */
    printf("Starting dump\n");
    code = TC_PerformDump(tconn, tcdiPtr, &volArray, &dumpTaskPtr->dumpID);
    if (code) {
	afs_com_err(whoami, code, "; Failed to start dump");
	ERROR(code);
    }

    afs_com_err(whoami, 0, "Task %u: Dump (%s)", dumpTaskPtr->dumpID,
	    tcdiPtr->dumpName);

    /* create status monitor block */
    statusPtr = createStatusNode();
    lock_Status();
    statusPtr->taskId = dumpTaskPtr->dumpID;
    statusPtr->port = port;
    statusPtr->jobNumber = bc_jobNumber();
    statusPtr->volsTotal = volArray.tc_dumpArray_len;
    statusPtr->flags &= ~STARTING;
    sprintf(statusPtr->taskName, "Dump (%s.%s)", dumpTaskPtr->volSetName,
	    baseNamePtr);
    unlock_Status();

  error_exit:
    /* locally allocated resources */
    if (volDesc)
	free(volDesc);

    if (tconn)
	rx_DestroyConnection(tconn);

    return (code);
}

/* freeDumpTaskVolumeList
 *	free the list of volumes used for dumps
 */

freeDumpTaskVolumeList(vdptr)
     struct bc_volumeDump *vdptr;
{
    struct bc_volumeDump *nextVdPtr;

    while (vdptr != 0) {
	nextVdPtr = vdptr->next;

	if (vdptr->name)
	    free(vdptr->name);
	free(vdptr);

	vdptr = nextVdPtr;
    }
}

/* bc_DmpRstStart
 *     The other half of the dump/restore create process call. In bc_StartDmpRst, 
 *     we allocated a dumpTask entry. Here we do the task and then free the entry.
 */
bc_DmpRstStart(aindex)
     afs_int32 aindex;
{
    register struct bc_dumpTask *tdump;
    register afs_int32 code;

    tdump = &bc_dumpTasks[aindex];

    code = (tdump->callProc) (aindex);
    if (code)
	lastTaskCode = code;

    /* Cleanup allocated data structures */
    freeDumpTaskVolumeList(tdump->volumes);
    tdump->dumpID = 0;
    if (tdump->dumpName)
	free(tdump->dumpName);
    if (tdump->newExt)
	free(tdump->newExt);
    if (tdump->volSetName)
	free(tdump->volSetName);
    if (tdump->portOffset)
	free(tdump->portOffset);
    tdump->flags &= ~BC_DI_INUSE;

    return code;
}

/* bc_StartDmpRst
 *	function to start dump running. Packages the relevant information
 *	(from params) into any free dumpTask structure (globally allocated),
 *	and then invokes bc_DmpRstStart to do the work, passing it a single 
 *      parameter, the index into the dumpTask array.
 *
 * entry:
 *	aconfig - normally is bc_globalConfig
 *	aproc - bc_Dumper for doing dumps
 *		bc_Restorer for doing restores
 */

bc_StartDmpRst(aconfig, adname, avname, avolsToDump, adestServer,
	       adestPartition, afromDate, anewExt, aoldFlag, aparent, alevel,
	       aproc, ports, portCount, dsptr, append, dontExecute)
     struct bc_config *aconfig;
     char *adname;
     char *avname;
     struct bc_volumeDump *avolsToDump;
     struct sockaddr_in *adestServer;
     afs_int32 adestPartition;
     afs_int32 afromDate;
     char *anewExt;
     int aoldFlag;
     afs_int32 aparent, alevel;
     int (*aproc) ();
     afs_int32 *ports;
     afs_int32 portCount;
     struct bc_dumpSchedule *dsptr;
     int append, dontExecute;
{
    register int i;
    register afs_int32 code;
    char *junk;

    for (i = 0; i < BC_MAXSIMDUMPS; i++)
	if (!(bc_dumpTasks[i].flags & BC_DI_INUSE))
	    break;

    if (i >= BC_MAXSIMDUMPS) {
	afs_com_err(whoami, BC_NOTLOCKED,
		"All of the dump/restore slots are in use, try again later");
	return (BC_NOTLOCKED);
    }

    memset(&bc_dumpTasks[i], 0, sizeof(struct bc_dumpTask));
    bc_dumpTasks[i].callProc = aproc;
    bc_dumpTasks[i].config = aconfig;
    bc_dumpTasks[i].volumes = avolsToDump;
    bc_dumpTasks[i].flags = BC_DI_INUSE;
    bc_dumpTasks[i].dumpName = bc_CopyString(adname);
    bc_dumpTasks[i].volSetName = bc_CopyString(avname);
    bc_dumpTasks[i].newExt = bc_CopyString(anewExt);
    bc_dumpTasks[i].dumpLevel = alevel;
    bc_dumpTasks[i].parentDumpID = aparent;
    bc_dumpTasks[i].oldFlag = aoldFlag;
    bc_dumpTasks[i].fromDate = afromDate;
    bc_dumpTasks[i].destPartition = adestPartition;
    bc_dumpTasks[i].portOffset = ports;
    bc_dumpTasks[i].portCount = portCount;
    bc_dumpTasks[i].doAppend = append;
    bc_dumpTasks[i].dontExecute = dontExecute;

    if (dsptr) {
	/* This should be specified for dumps, but will be 0 for restores */
	bc_dumpTasks[i].expDate = dsptr->expDate;
	bc_dumpTasks[i].expType = dsptr->expType;
    }
    if (adestServer)
	memcpy(&bc_dumpTasks[i].destServer, adestServer,
	       sizeof(struct sockaddr_in));
    else
	memset(&bc_dumpTasks[i].destServer, 0, sizeof(struct sockaddr_in));

    code =
	LWP_CreateProcess(bc_DmpRstStart, 20480, LWP_NORMAL_PRIORITY,
			  (void *)i, "helper", &junk);
    if (code) {
	bc_HandleMisc(code);
	afs_com_err(whoami, code, "; Can't start thread");

	/* Cleanup allocated data structures */
	freeDumpTaskVolumeList(bc_dumpTasks[i].volumes);
	bc_dumpTasks[i].dumpID = 0;
	if (bc_dumpTasks[i].dumpName)
	    free(bc_dumpTasks[i].dumpName);
	if (bc_dumpTasks[i].newExt)
	    free(bc_dumpTasks[i].newExt);
	if (bc_dumpTasks[i].volSetName)
	    free(bc_dumpTasks[i].volSetName);
	if (bc_dumpTasks[i].portOffset)
	    free(bc_dumpTasks[i].portOffset);
	bc_dumpTasks[i].flags &= ~BC_DI_INUSE;
	return (code);
    }

    return 0;
}

#ifdef notdef
/* bc_FindDumpSlot
 * 	Returns the dump slot of the dump with dumpID
 * entry:
 *	dumpID - id to look for
 *	port - portoffset for tape coordinator
 * exit:
 *	0-n - i.e. 0 or positive number, is the dump slot
 *	-1 - failed to find dumpID
 */

afs_int32
bc_FindDumpSlot(dumpID, port)
     afs_int32 dumpID;
     afs_int32 port;
{
    int i;

    for (i = 0; i < BC_MAXSIMDUMPS; i++) {
	if ((bc_dumpTasks[i].flags & BC_DI_INUSE)
	    && (bc_dumpTasks[i].dumpID == dumpID)
	    && ((afs_int32) bc_dumpTasks[i].portOffset == port)) {
	    return (i);
	}
    }
    return (-1);
}
#endif

/* bc_LabelTape
 *	opens a connection to the tape coordinator and requests that it
 *	label a tape
 */

bc_LabelTape(afsname, pname, size, config, port)
     char *afsname, *pname;
     struct bc_config *config;
     afs_int32 port;
     afs_int32 size;
{
    struct rx_connection *tconn;
    afs_int32 code = 0;
    struct tc_tapeLabel label;
    statusP statusPtr;
    afs_uint32 taskId;

    code = ConnectButc(config, port, &tconn);
    if (code)
	return (code);

    memset(&label, 0, sizeof(label));
    if (afsname)
	strcpy(label.afsname, afsname);
    if (pname)
	strcpy(label.pname, (strcmp(pname, "") ? pname : "<NULL>"));
    label.size = size;

    code = TC_LabelTape(tconn, &label, &taskId);
    if (code) {
	afs_com_err(whoami, code, "; Failed to start labeltape");
	return (code);
    }

    /* create status monitor block */
    statusPtr = createStatusNode();
    lock_Status();
    statusPtr->taskId = taskId;
    statusPtr->port = port;
    statusPtr->jobNumber = bc_jobNumber();
    /* statusPtr->flags    |= SILENT; *//* don't report termination */
    statusPtr->flags &= ~STARTING;	/* ok to examine */

    sprintf(statusPtr->taskName, "Labeltape (%s)",
	    (pname ? pname : (afsname ? afsname : "<NULL>")));
    unlock_Status();

    return 0;
}

/* bc_ReadLabel
 *	open a connection to the tape coordinator and read the label on
 *	a tape
 */

bc_ReadLabel(config, port)
     struct bc_config *config;
     afs_int32 port;
{
    struct rx_connection *tconn;
    struct tc_tapeLabel label;
    afs_uint32 taskId;
    afs_int32 code = 0;
    char *tname = 0;

    code = ConnectButc(config, port, &tconn);
    if (code)
	return (code);

    memset(&label, 0, sizeof(label));
    code = TC_ReadLabel(tconn, &label, &taskId);
    if (code) {
	if (code == BUTM_NOLABEL) {
	    printf("Tape read was unlabelled\n");
	    return 0;
	}
	afs_com_err(whoami, code, "; Failed to start readlabel");
	return (code);
    }

    if (strcmp(label.pname, ""))
	tname = label.pname;
    else if (strcmp(label.afsname, ""))
	tname = label.afsname;

    if (!tname) {
	printf("Tape read was labelled : <NULL>  size : %u\n", label.size);
    } else if (!label.tapeId) {
	printf("Tape read was labelled : %s size : %lu Kbytes\n", tname,
	       label.size);
    } else {
	printf("Tape read was labelled : %s (%lu) size : %lu Kbytes\n", tname,
	       label.tapeId, label.size);
    }

    return 0;
}

bc_ScanDumps(config, dbAddFlag, port)
     struct bc_config *config;
     afs_int32 dbAddFlag;
     afs_int32 port;
{
    struct rx_connection *tconn;
    statusP statusPtr;
    afs_uint32 taskId;
    afs_int32 code = 0;

    code = ConnectButc(config, port, &tconn);
    if (code)
	return (code);

    code = TC_ScanDumps(tconn, dbAddFlag, &taskId);
    if (code) {
	afs_com_err(whoami, code, "; Failed to start scantape");
	return (code);
    }

    /* create status monitor block */
    statusPtr = createStatusNode();
    lock_Status();
    statusPtr->taskId = taskId;
    statusPtr->port = port;
    statusPtr->jobNumber = bc_jobNumber();
    statusPtr->flags &= ~STARTING;	/* ok to examine */
    sprintf(statusPtr->taskName, "Scantape");
    unlock_Status();

    return (0);
}

/*************/
/* utilities */
/*************/

/* get a connection to the tape controller */
afs_int32
bc_GetConn(aconfig, aport, tconn)
     struct bc_config *aconfig;
     afs_int32 aport;
     struct rx_connection **tconn;
{
    afs_int32 code = 0;
    afs_uint32 host;
    unsigned short port;
    static struct rx_securityClass *rxsc;
    struct bc_hostEntry *te;

    *tconn = (struct rx_connection *)0;

    /* use non-secure connections to butc */
    if (!rxsc)
	rxsc = rxnull_NewClientSecurityObject();
    if (!rxsc || !aconfig)
	return (-1);

    for (te = aconfig->tapeHosts; te; te = te->next) {
	if (te->portOffset == aport) {
	    /* found the right port */
	    host = te->addr.sin_addr.s_addr;
	    if (!host)
		return (BC_NOHOSTENTRY);	/* gethostbyname in bc_ParseHosts failed */

	    port = htons(BC_TAPEPORT + aport);

	    /* servers is 1; sec index is 0 */
	    *tconn = rx_NewConnection(host, port, 1, rxsc, 0);
	    return ((*tconn ? 0 : -1));
	}
    }
    return (BC_NOHOSTENTRY);
}

/* CheckTCVersion
 *	make sure we are talking to a compatible butc process.
 * exit: 
 *	0 - ok
 *	-1 - not compatible
 */

CheckTCVersion(tconn)
     struct rx_connection *tconn;
{
    struct tc_tcInfo tci;
    afs_int32 code;

    code = TC_TCInfo(tconn, &tci);
    if (code)
	return (code);

    if (tci.tcVersion != CUR_BUTC_VERSION)
	return (BC_VERSIONFAIL);

    return 0;
}

ConnectButc(config, port, tconn)
     struct bc_config *config;
     afs_int32 port;
     struct rx_connection **tconn;
{
    afs_int32 code;

    code = bc_GetConn(config, port, tconn);
    if (code) {
	afs_com_err(whoami, code,
		"; Can't connect to tape coordinator at port %d", port);
	return (code);
    }

    code = CheckTCVersion(*tconn);
    if (code) {
	rx_DestroyConnection(*tconn);

	if (code == BC_VERSIONFAIL)
	    afs_com_err(whoami, code,
		    "; Backup and butc are not the same version");
	else
	    afs_com_err(whoami, code,
		    "; Can't access tape coordinator at port %d", port);

	return (code);
    }

    return (0);
}
