/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#ifdef AFS_AIX_ENV
#include <sys/statfs.h>
#endif
#include <netdb.h>
#include <sys/errno.h>
#include <lock.h>
#include <netinet/in.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rx_globals.h>
#include <afs/nfs.h>
#include <afs/vlserver.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <ubik.h>
#include <afs/afsint.h>
#include <afs/cmd.h>
#include <rx/rxkad.h>
#include <afs/tcdata.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif

#define SERVERNAME "server1"

afs_int32 code = 0;
struct tc_tapeSet ttapeSet;
char tdumpSetName[TC_MAXNAMELEN];
tc_dumpArray tdumps;		/*defined by rxgen */
tc_restoreArray trestores;	/*defined by rxgen */
afs_int32 tdumpID;
struct tc_dumpStat tstatus;
int rxInitDone = 0;

struct rx_connection *
UV_Bind(aserver, port)
     afs_int32 aserver, port;
{
    register struct rx_connection *tc;
    struct rx_securityClass *uvclass;

    uvclass = rxnull_NewClientSecurityObject();
    tc = rx_NewConnection(aserver, htons(port), TCSERVICE_ID, uvclass, 0);
    return tc;
}


/* return host address in network byte order */
afs_int32
GetServer(aname)
     char *aname;
{
    register struct hostent *th;
    afs_int32 addr;
    char b1, b2, b3, b4;
    register afs_int32 code;

    code = sscanf(aname, "%d.%d.%d.%d", &b1, &b2, &b3, &b4);
    if (code == 4) {
	addr = (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;
	return htonl(addr);	/* convert to network order (128 in byte 0) */
    }
    th = gethostbyname(aname);
    if (!th)
	return 0;
    memcpy(&addr, th->h_addr, sizeof(addr));
    return addr;
}


static int
PerformDump(register struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *aconn;
    afs_int32 server;
    FILE *fopen(), *fp;
    struct tc_dumpDesc *ptr;
    int i;
    afs_int32 parentDumpID, dumpLevel;

    server = GetServer(SERVERNAME);
    if (!server) {
	printf("cant get server id \n");
	exit(1);
    }
    parentDumpID = 1;
    dumpLevel = 1;
    strcpy(tdumpSetName, "Test");
    ttapeSet.id = 1;
    ttapeSet.maxTapes = 10;
    fp = fopen("dumpScr", "r");
    fscanf(fp, "%u %u %u\n", &tdumps.tc_dumpArray_len, &ttapeSet.a,
	   &ttapeSet.b);
    strcpy(ttapeSet.format, "tapeName%u");
    strcpy(ttapeSet.tapeServer, "diskTapes");
    tdumps.tc_dumpArray_val =
	(struct tc_dumpDesc
	 *)(malloc(tdumps.tc_dumpArray_len * sizeof(struct tc_dumpDesc)));
    ptr = tdumps.tc_dumpArray_val;
    for (i = 0; i < tdumps.tc_dumpArray_len; i++) {
	fscanf(fp, "%s\n", ptr->name);
	fscanf(fp, "%s\n", ptr->hostAddr);
	fscanf(fp, "%u %u %u\n", &ptr->vid, &ptr->partition, &ptr->date);
	ptr++;
    }

    aconn = UV_Bind(server, TCPORT);
    code =
	TC_PerformDump(aconn, tdumpSetName, &ttapeSet, &tdumps, parentDumpID,
		       dumpLevel, &tdumpID);
    free(tdumps.tc_dumpArray_val);
    if (code) {
	printf("call to TC_PerformDump failed %u\n", code);
	exit(1);
    }
    printf("dumpid returned %u\n", tdumpID);

    return 0;
}

static int
PerformRestore(register struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *aconn;
    afs_int32 server;
    int i;
    FILE *fopen(), *fp;
    struct tc_restoreDesc *ptr;

    server = GetServer(SERVERNAME);
    if (!server) {
	printf("cant get server id \n");
	exit(1);
    }
    aconn = UV_Bind(server, TCPORT);
    strcpy(tdumpSetName, "");
    strcpy(tdumpSetName, "Test");
    fp = fopen("restoreScr", "r");
    fscanf(fp, "%u\n", &trestores.tc_restoreArray_len);
    trestores.tc_restoreArray_val =
	(struct tc_restoreDesc *)malloc(trestores.tc_restoreArray_len *
					sizeof(struct tc_restoreDesc));
    ptr = trestores.tc_restoreArray_val;
    for (i = 0; i < trestores.tc_restoreArray_len; i++) {
	fscanf(fp, "%s\n", ptr->oldName);
	fscanf(fp, "%s\n", ptr->newName);
	fscanf(fp, "%s\n", ptr->tapeName);
	fscanf(fp, "%s\n", ptr->hostAddr);
	fscanf(fp, "%u %u %u %u %d %u\n", &ptr->origVid, &ptr->vid,
	       &ptr->partition, &ptr->flags, &ptr->frag, &ptr->position);
	ptr++;

    }
    code = TC_PerformRestore(aconn, tdumpSetName, &trestores, &tdumpID);
    if (code) {
	printf("call to TC_PerformRestore failed %u\n", code);
	exit(1);
    }
    printf("dumpid returned %u\n", tdumpID);
    return 0;
}

static int
CheckDump(register struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *aconn;
    afs_int32 server;
    server = GetServer(SERVERNAME);
    if (!server) {
	printf("cant get server id \n");
	exit(1);
    }
    tdumpID = atol(as->parms[0].items->data);
    aconn = UV_Bind(server, TCPORT);
    code = TC_CheckDump(aconn, tdumpID, &tstatus);
    if (code) {
	printf("call to TC_CheckDump failed %u\n", code);
	exit(1);
    }
    return 0;
}

static int
AbortDump(register struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *aconn;
    afs_int32 server;
    server = GetServer(SERVERNAME);
    if (!server) {
	printf("cant get server id \n");
	exit(1);
    }
    tdumpID = atol(as->parms[0].items->data);
    aconn = UV_Bind(server, TCPORT);
    code = TC_AbortDump(aconn, tdumpID);
    if (code) {
	printf("call to TC_AbortDump failed %u\n", code);
	exit(1);
    }
    return 0;
}

static int
WaitForDump(register struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *aconn;
    afs_int32 server;
    server = GetServer(SERVERNAME);
    if (!server) {
	printf("cant get server id \n");
	exit(1);
    }
    tdumpID = atol(as->parms[0].items->data);
    aconn = UV_Bind(server, TCPORT);
    code = TC_WaitForDump(aconn, tdumpID);
    if (code) {
	printf("call to TC_WaitForDump failed %u\n", code);
	exit(1);
    }
    return 0;
}

static int
EndDump(register struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *aconn;
    afs_int32 server;
    server = GetServer(SERVERNAME);
    if (!server) {
	printf("cant get server id \n");
	exit(1);
    }
    tdumpID = atol(as->parms[0].items->data);
    aconn = UV_Bind(server, TCPORT);
    code = TC_EndDump(aconn, tdumpID);
    if (code) {
	printf("call to TC_EndDump failed %u\n", code);
	exit(1);
    }
    return 0;
}

static int
MyBeforeProc(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;

    code = rx_Init(0);
    if (code) {
	printf("Could not initialize rx.\n");
	return code;
    }
    rxInitDone = 1;
    rx_SetRxDeadTime(50);
    return 0;
}

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char **argv;
{
    register afs_int32 code;

    register struct cmd_syndesc *ts;

#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a 
     * crash (i.e. core is generated) we can include the user's data section 
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGABRT, &nsa, NULL);
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    cmd_SetBeforeProc(MyBeforeProc, NULL);

    ts = cmd_CreateSyntax("dump", PerformDump, NULL, "perform a dump");

    ts = cmd_CreateSyntax("restore", PerformRestore, NULL, "perform a restore");

    ts = cmd_CreateSyntax("check", CheckDump, NULL, "check a dump");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "dump id");

    ts = cmd_CreateSyntax("abort", AbortDump, NULL, "abort a dump");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "dump id");

    ts = cmd_CreateSyntax("wait", WaitForDump, NULL, "wait for a dump");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "dump id");

    ts = cmd_CreateSyntax("end", EndDump, NULL, "end a dump");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "dump id");

    code = cmd_Dispatch(argc, argv);
    if (rxInitDone)
	rx_Finalize();
    exit(code);
}
