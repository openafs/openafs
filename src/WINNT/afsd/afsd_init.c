/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>
#include <afs/afs_args.h>

#include <windows.h>
#include <string.h>
#include <nb30.h>
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>

#include <osi.h>
#include "afsd.h"
#include <rx\rx.h>
#include <rx\rx_null.h>

#include <WINNT/syscfg.h>

#include "smb.h"
#include "cm_rpc.h"

extern int RXAFSCB_ExecuteRequest();
extern int RXSTATS_ExecuteRequest();

extern afs_int32 cryptall;

char AFSConfigKeyName[] =
	"SYSTEM\\CurrentControlSet\\Services\\TransarcAFSDaemon\\Parameters";

osi_log_t *afsd_logp;

char cm_rootVolumeName[64];
DWORD cm_rootVolumeNameLen;
cm_volume_t *cm_rootVolumep = NULL;
cm_cell_t *cm_rootCellp = NULL;
cm_fid_t cm_rootFid;
cm_scache_t *cm_rootSCachep;
char cm_mountRoot[1024];
DWORD cm_mountRootLen;
int cm_logChunkSize;
int cm_chunkSize;

int smb_UseV3;

int LANadapter;

int numBkgD;
int numSvThreads;

int traceOnPanic = 0;

int logReady = 0;

char cm_HostName[200];
long cm_HostAddr;

char cm_CachePath[200];
DWORD cm_CachePathLen;

BOOL isGateway = FALSE;

BOOL reportSessionStartups = FALSE;

cm_initparams_v1 cm_initParams;

/*
 * AFSD Initialization Log
 *
 * This is distinct from the regular debug logging facility.
 * Log items go directly to a file, not to an array in memory, so that even
 * if AFSD crashes, the log can be inspected.
 */

HANDLE afsi_file;

void
afsi_start()
{
	char wd[100];
	char t[100], u[100];
	int zilch;
	int code;

	afsi_file = INVALID_HANDLE_VALUE;
	code = GetWindowsDirectory(wd, sizeof(wd));
	if (code == 0) return;
	strcat(wd, "\\afsd_init.log");
	GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, t, sizeof(t));
	afsi_file = CreateFile(wd, GENERIC_WRITE, FILE_SHARE_READ, NULL,
				CREATE_ALWAYS, FILE_FLAG_WRITE_THROUGH, NULL);
	GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, u, sizeof(u));
	strcat(t, ": Create log file\n");
	strcat(u, ": Created log file\n");
	WriteFile(afsi_file, t, strlen(t), &zilch, NULL);
	WriteFile(afsi_file, u, strlen(u), &zilch, NULL);
}

void
afsi_log(char *pattern, ...)
{
	char s[100], t[100], u[100];
	int zilch;
	va_list ap;
	va_start(ap, pattern);

	vsprintf(s, pattern, ap);
	GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, t, sizeof(t));
	sprintf(u, "%s: %s\n", t, s);
	if (afsi_file != INVALID_HANDLE_VALUE)
		WriteFile(afsi_file, u, strlen(u), &zilch, NULL);
}

/*
 * Standard AFSD trace
 */

void afsd_ForceTrace(BOOL flush)
{
	HANDLE handle;
	int len;
	char buf[100];

	if (!logReady) return;

	len = GetTempPath(99, buf);
	strcpy(&buf[len], "/afsd.log");
	handle = CreateFile(buf, GENERIC_WRITE, FILE_SHARE_READ,
			    NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		logReady = 0;
		osi_panic("Cannot create log file", __FILE__, __LINE__);
	}
	osi_LogPrint(afsd_logp, handle);
	if (flush)
		FlushFileBuffers(handle);
	CloseHandle(handle);
}

/*
 * AFSD Initialization
 */

int afsd_InitCM(char **reasonP)
{
	osi_uid_t debugID;
	long cacheBlocks;
	long cacheSize;
	long logChunkSize;
	long stats;
	long traceBufSize;
	long ltt, ltto;
	char rootCellName[256];
	struct rx_service *serverp;
	static struct rx_securityClass *nullServerSecurityClassp;
	struct hostent *thp;
	char *msgBuf;
	char buf[200];
	HKEY parmKey;
	DWORD dummyLen;
	long code;
	WSADATA WSAjunk;

	WSAStartup(0x0101, &WSAjunk);

	/* setup osidebug server at RPC slot 1000 */
	osi_LongToUID(1000, &debugID);
	code = osi_InitDebug(&debugID);
	afsi_log("osi_InitDebug code %d", code);
//	osi_LockTypeSetDefault("stat");	/* comment this out for speed *
	if (code != 0) {
		*reasonP = "unknown error";
		return -1;
	}

	/* who are we ? */
	gethostname(cm_HostName, sizeof(cm_HostName));
	afsi_log("gethostname %s", cm_HostName);
	thp = gethostbyname(cm_HostName);
	memcpy(&cm_HostAddr, thp->h_addr_list[0], 4);

	/* seed random number generator */
	srand(ntohl(cm_HostAddr));

	/* Look up configuration parameters in Registry */

	code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSConfigKeyName,
				0, KEY_QUERY_VALUE, &parmKey);
	if (code != ERROR_SUCCESS) {
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM
				| FORMAT_MESSAGE_ALLOCATE_BUFFER,
			      NULL, code, 0, (LPTSTR)&msgBuf, 0, NULL);
		sprintf(buf,
			"Failure in configuration while opening Registry: %s",
			msgBuf);
		osi_panic(buf, __FILE__, __LINE__);
	}

	dummyLen = sizeof(LANadapter);
	code = RegQueryValueEx(parmKey, "LANadapter", NULL, NULL,
				(BYTE *) &LANadapter, &dummyLen);
	if (code == ERROR_SUCCESS)
		afsi_log("LAN adapter number %d", LANadapter);
	else {
		LANadapter = -1;
		afsi_log("Default LAN adapter number");
	}

	dummyLen = sizeof(cacheSize);
	code = RegQueryValueEx(parmKey, "CacheSize", NULL, NULL,
				(BYTE *) &cacheSize, &dummyLen);
	if (code == ERROR_SUCCESS)
		afsi_log("Cache size %d", cacheSize);
	else {
		cacheSize = CM_CONFIGDEFAULT_CACHESIZE;
		afsi_log("Default cache size %d", cacheSize);
	}

	dummyLen = sizeof(logChunkSize);
	code = RegQueryValueEx(parmKey, "ChunkSize", NULL, NULL,
				(BYTE *) &logChunkSize, &dummyLen);
	if (code == ERROR_SUCCESS) {
		if (logChunkSize < 12 || logChunkSize > 30) {
			afsi_log("Invalid chunk size %d, using default",
				 logChunkSize);
			logChunkSize = CM_CONFIGDEFAULT_CHUNKSIZE;
		}
		afsi_log("Chunk size %d", logChunkSize);
	} else {
		logChunkSize = CM_CONFIGDEFAULT_CHUNKSIZE;
		afsi_log("Default chunk size %d", logChunkSize);
	}
	cm_logChunkSize = logChunkSize;
	cm_chunkSize = 1 << logChunkSize;

	dummyLen = sizeof(numBkgD);
	code = RegQueryValueEx(parmKey, "Daemons", NULL, NULL,
				(BYTE *) &numBkgD, &dummyLen);
	if (code == ERROR_SUCCESS)
		afsi_log("%d background daemons", numBkgD);
	else {
		numBkgD = CM_CONFIGDEFAULT_DAEMONS;
		afsi_log("Defaulting to %d background daemons", numBkgD);
	}

	dummyLen = sizeof(numSvThreads);
	code = RegQueryValueEx(parmKey, "ServerThreads", NULL, NULL,
				(BYTE *) &numSvThreads, &dummyLen);
	if (code == ERROR_SUCCESS)
		afsi_log("%d server threads", numSvThreads);
	else {
		numSvThreads = CM_CONFIGDEFAULT_SVTHREADS;
		afsi_log("Defaulting to %d server threads", numSvThreads);
	}

	dummyLen = sizeof(stats);
	code = RegQueryValueEx(parmKey, "Stats", NULL, NULL,
				(BYTE *) &stats, &dummyLen);
	if (code == ERROR_SUCCESS)
		afsi_log("Status cache size %d", stats);
	else {
		stats = CM_CONFIGDEFAULT_STATS;
		afsi_log("Default status cache size %d", stats);
	}

	dummyLen = sizeof(ltt);
	code = RegQueryValueEx(parmKey, "LogoffTokenTransfer", NULL, NULL,
				(BYTE *) &ltt, &dummyLen);
	if (code == ERROR_SUCCESS)
		afsi_log("Logoff token transfer %s",  (ltt ? "on" : "off"));
	else {
		ltt = 1;
		afsi_log("Logoff token transfer on by default");
	}
        smb_LogoffTokenTransfer = ltt;

	if (ltt) {
		dummyLen = sizeof(ltto);
		code = RegQueryValueEx(parmKey, "LogoffTokenTransferTimeout",
					NULL, NULL, (BYTE *) &ltto, &dummyLen);
		if (code == ERROR_SUCCESS)
			afsi_log("Logoff token tranfer timeout %d seconds",
				 ltto);
		else {
			ltto = 10;
			afsi_log("Default logoff token transfer timeout 10 seconds");
		}
	}
        smb_LogoffTransferTimeout = ltto;

	dummyLen = sizeof(cm_rootVolumeName);
	code = RegQueryValueEx(parmKey, "RootVolume", NULL, NULL,
				cm_rootVolumeName, &dummyLen);
	if (code == ERROR_SUCCESS)
		afsi_log("Root volume %s", cm_rootVolumeName);
	else {
		strcpy(cm_rootVolumeName, "root.afs");
		afsi_log("Default root volume name root.afs");
	}

	cm_mountRootLen = sizeof(cm_mountRoot);
	code = RegQueryValueEx(parmKey, "Mountroot", NULL, NULL,
				cm_mountRoot, &cm_mountRootLen);
	if (code == ERROR_SUCCESS)
		afsi_log("Mount root %s", cm_mountRoot);
	else {
		strcpy(cm_mountRoot, "/afs");
		cm_mountRootLen = 4;
		/* Don't log */
	}

	dummyLen = sizeof(cm_CachePath);
	code = RegQueryValueEx(parmKey, "CachePath", NULL, NULL,
				cm_CachePath, &dummyLen);
	if (code == ERROR_SUCCESS)
		afsi_log("Cache path %s", cm_CachePath);
	else {
		GetWindowsDirectory(cm_CachePath, sizeof(cm_CachePath));
		cm_CachePath[2] = 0;	/* get drive letter only */
		strcat(cm_CachePath, "\\AFSCache");
		afsi_log("Default cache path %s", cm_CachePath);
	}

	dummyLen = sizeof(traceOnPanic);
	code = RegQueryValueEx(parmKey, "TrapOnPanic", NULL, NULL,
				(BYTE *) &traceOnPanic, &dummyLen);
	if (code == ERROR_SUCCESS)
		afsi_log("Set to %s on panic",
			 traceOnPanic ? "trap" : "not trap");
	else {
		traceOnPanic = 0;
		/* Don't log */
	}

	dummyLen = sizeof(isGateway);
	code = RegQueryValueEx(parmKey, "IsGateway", NULL, NULL,
				(BYTE *) &isGateway, &dummyLen);
	if (code == ERROR_SUCCESS)
		afsi_log("Set for %s service",
			 isGateway ? "gateway" : "stand-alone");
	else {
		isGateway = 0;
		/* Don't log */
	}

	dummyLen = sizeof(reportSessionStartups);
	code = RegQueryValueEx(parmKey, "ReportSessionStartups", NULL, NULL,
				(BYTE *) &reportSessionStartups, &dummyLen);
	if (code == ERROR_SUCCESS)
		afsi_log("Session startups %s be recorded in the Event Log",
			 reportSessionStartups ? "will" : "will not");
	else {
		reportSessionStartups = 0;
		/* Don't log */
	}

	dummyLen = sizeof(traceBufSize);
	code = RegQueryValueEx(parmKey, "TraceBufferSize", NULL, NULL,
				(BYTE *) &traceBufSize, &dummyLen);
	if (code == ERROR_SUCCESS)
		afsi_log("Trace Buffer size %d", traceBufSize);
	else {
		traceBufSize = CM_CONFIGDEFAULT_TRACEBUFSIZE;
		afsi_log("Default trace buffer size %d", traceBufSize);
	}

	dummyLen = sizeof(cm_sysName);
	code = RegQueryValueEx(parmKey, "SysName", NULL, NULL,
				cm_sysName, &dummyLen);
	if (code == ERROR_SUCCESS)
		afsi_log("Sys name %s", cm_sysName);
	else {
		strcat(cm_sysName, "i386_nt40");
		afsi_log("Default sys name %s", cm_sysName);
	}

	dummyLen = sizeof(cryptall);
	code = RegQueryValueEx(parmKey, "SecurityLevel", NULL, NULL,
				(BYTE *) &cryptall, &dummyLen);
	if (code == ERROR_SUCCESS)
		afsi_log("SecurityLevel is %s", cryptall?"crypt":"clear");
	else {
		cryptall = rxkad_clear;
		afsi_log("Default SecurityLevel is clear");
	}

	RegCloseKey (parmKey);

	/* setup early variables */
	/* These both used to be configurable. */
	smb_UseV3 = 1;
        buf_bufferSize = CM_CONFIGDEFAULT_BLOCKSIZE;

	/* turn from 1024 byte units into memory blocks */
        cacheBlocks = (cacheSize * 1024) / buf_bufferSize;
        
	/* setup and enable debug log */
	afsd_logp = osi_LogCreate("afsd", traceBufSize);
	afsi_log("osi_LogCreate log addr %x", afsd_logp);
        osi_LogEnable(afsd_logp);
	logReady = 1;

	/* get network related info */
	cm_noIPAddr = CM_MAXINTERFACE_ADDR;
	code = syscfg_GetIFInfo(&cm_noIPAddr,
				cm_IPAddr, cm_SubnetMask,
				cm_NetMtu, cm_NetFlags);

	if ( (cm_noIPAddr <= 0) || (code <= 0 ) )
	    afsi_log("syscfg_GetIFInfo error code %d", code);
	else
	    afsi_log("First Network address %x SubnetMask %x",
		     cm_IPAddr[0], cm_SubnetMask[0]);

	/*
	 * Save client configuration for GetCacheConfig requests
	 */
	cm_initParams.nChunkFiles = 0;
	cm_initParams.nStatCaches = stats;
	cm_initParams.nDataCaches = 0;
	cm_initParams.nVolumeCaches = 0;
	cm_initParams.firstChunkSize = cm_chunkSize;
	cm_initParams.otherChunkSize = cm_chunkSize;
	cm_initParams.cacheSize = cacheSize;
	cm_initParams.setTime = 0;
	cm_initParams.memCache = 0;

	/* initialize RX, and tell it to listen to port 7001, which is used for
         * callback RPC messages.
         */
	code = rx_Init(htons(7001));
	afsi_log("rx_Init code %x", code);
	if (code != 0) {
		*reasonP = "afsd: failed to init rx client on port 7001";
		return -1;
	}

	/* Initialize the RPC server for session keys */
	RpcInit();

	/* create an unauthenticated service #1 for callbacks */
	nullServerSecurityClassp = rxnull_NewServerSecurityObject();
        serverp = rx_NewService(0, 1, "AFS", &nullServerSecurityClassp, 1,
        	RXAFSCB_ExecuteRequest);
	afsi_log("rx_NewService addr %x", serverp);
	if (serverp == NULL) {
		*reasonP = "unknown error";
		return -1;
	}

	nullServerSecurityClassp = rxnull_NewServerSecurityObject();
        serverp = rx_NewService(0, RX_STATS_SERVICE_ID, "rpcstats",
		&nullServerSecurityClassp, 1, RXSTATS_ExecuteRequest);
	afsi_log("rx_NewService addr %x", serverp);
	if (serverp == NULL) {
		*reasonP = "unknown error";
		return -1;
	}
        
        /* start server threads, *not* donating this one to the pool */
        rx_StartServer(0);
	afsi_log("rx_StartServer");

	/* init user daemon, and other packages */
	cm_InitUser();

	cm_InitACLCache(2*stats);

	cm_InitConn();

        cm_InitCell();
        
        cm_InitServer();
        
        cm_InitVolume();
        
        cm_InitIoctl();
        
        smb_InitIoctl();
        
        cm_InitCallback();
        
        cm_InitSCache(stats);
        
        code = cm_InitDCache(0, cacheBlocks);
	afsi_log("cm_InitDCache code %x", code);
	if (code != 0) {
		*reasonP = "error initializing cache";
		return -1;
	}

	code = cm_GetRootCellName(rootCellName);
	afsi_log("cm_GetRootCellName code %d rcn %s", code,
		 (code ? "<none>" : rootCellName));
	if (code != 0) {
        	*reasonP = "can't find root cell name in afsd.ini";
		return -1;
	}

	cm_rootCellp = cm_GetCell(rootCellName, CM_FLAG_CREATE);
	afsi_log("cm_GetCell addr %x", cm_rootCellp);
	if (cm_rootCellp == NULL) {
		*reasonP = "can't find root cell in afsdcell.ini";
		return -1;
	}

	return 0;
}

int afsd_InitDaemons(char **reasonP)
{
	long code;
	cm_req_t req;

	cm_InitReq(&req);

	/* this should really be in an init daemon from here on down */

	code = cm_GetVolumeByName(cm_rootCellp, cm_rootVolumeName, cm_rootUserp,		&req, CM_FLAG_CREATE, &cm_rootVolumep);
	afsi_log("cm_GetVolumeByName code %x root vol %x", code,
		 (code ? 0xffffffff : cm_rootVolumep));
	if (code != 0) {
		*reasonP = "can't find root volume in root cell";
		return -1;
        }

        /* compute the root fid */
	cm_rootFid.cell = cm_rootCellp->cellID;
        cm_rootFid.volume = cm_GetROVolumeID(cm_rootVolumep);
        cm_rootFid.vnode = 1;
        cm_rootFid.unique = 1;
        
        code = cm_GetSCache(&cm_rootFid, &cm_rootSCachep, cm_rootUserp, &req);
	afsi_log("cm_GetSCache code %x scache %x", code,
		 (code ? 0xffffffff : cm_rootSCachep));
	if (code != 0) {
		*reasonP = "unknown error";
		return -1;
	}

	cm_InitDaemon(numBkgD);
	afsi_log("cm_InitDaemon");

	return 0;
}

int afsd_InitSMB(char **reasonP, void *aMBfunc)
{
	char hostName[200];
	char *ctemp;

	/* Do this last so that we don't handle requests before init is done.
         * Here we initialize the SMB listener.
         */
	strcpy(hostName, cm_HostName);
        ctemp = strchr(hostName, '.');	/* turn ntdfs.* into ntdfs */
        if (ctemp) *ctemp = 0;
        hostName[11] = 0;	/* ensure that even after adding the -A, we
				 * leave one byte free for the netbios server
				 * type.
                                 */
        strcat(hostName, "-AFS");
        _strupr(hostName);
	smb_Init(afsd_logp, hostName, smb_UseV3, LANadapter, numSvThreads,
		 aMBfunc);
	afsi_log("smb_Init");

	return 0;
}
