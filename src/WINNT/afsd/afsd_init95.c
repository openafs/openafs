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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>

#include <osi.h>
#include "afsd.h"
#include <rx/rx.h>
#include <rx/rx_null.h>
#include <afs/cmd.h>
#include <netdb.h>
#include "cm_rpc.h"

#define AFSDIR_CLIENT_ETC_DIRPATH "c:/"
#define AFSLOGFILE "afs.log"
#define CACHEINFOFILE "cache.info"

extern int RXAFSCB_ExecuteRequest();
extern int RXSTATS_ExecuteRequest();

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
int afs_diskCacheChunks;
char cm_cachePath[128];
int cm_diskCacheEnabled = 0;
#ifdef AFS_AFSDB_ENV
extern int cm_dnsEnabled;
#endif

#ifdef AFS_FREELANCE_CLIENT
extern int cm_freelanceEnabled;
char *cm_FakeRootDir;
#endif /* freelance */

int smb_UseV3;

int LANadapter;
int lanAdaptSet = 0;
int rootVolSet = 0;
int cacheSetTime = TRUE;
int afsd_verbose = 0;
int chunkSize;

int numBkgD;
int numSvThreads;

int traceOnPanic = 0;

int logReady = 0;

char cm_HostName[200];
long cm_HostAddr;

/*char cm_CachePath[200];*/
/*DWORD cm_CachePathLen;*/
char cm_CacheInfoPath[1024];
int cacheBlocks;
int sawCacheSize=0, sawDiskCacheSize=0, sawMountRoot=0;
int sawCacheBaseDir=0;
char cm_AFSLogFile[200];
int afsd_CloseSynch = 0;
int afs_shutdown = 0;
char cm_confDir[200];

BOOL isGateway = FALSE;
BOOL reportSessionStartups = FALSE;

int afsd_debug;
cm_initparams_v1 cm_initParams;


/*
 * AFSD Initialization Log
 *
 * This is distinct from the regular debug logging facility.
 * Log items go directly to a file, not to an array in memory, so that even
 * if AFSD crashes, the log can be inspected.
 */

FILE *afsi_file;

void
afsi_start()
{
	char wd[100];
	char t[100], u[100];
	int zilch;
	int code;
        time_t now;
        char *p;

	afsi_file = NULL;
	/*code = GetWindowsDirectory(wd, sizeof(wd));
          if (code == 0) return;*/
        strcpy (wd, "C:");
	strcat(wd, "\\afsd_init.log");
	/*GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, t, sizeof(t));*/
        time (&now);
        strcpy(t, asctime(localtime(&now)));
        /*afsi_file = CreateFile(wd, GENERIC_WRITE, FILE_SHARE_READ, NULL,
          CREATE_ALWAYS, FILE_FLAG_WRITE_THROUGH, NULL);*/
        /*afsi_file = open(wd, O_RDWR | O_CREAT | O_RSHARE);*/
        afsi_file = fopen(wd, "wt");
        setbuf(afsi_file, NULL);
        
	/*GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, u, sizeof(u));*/
        time (&now);
        strcpy(u, asctime(localtime(&now)));
        p = strchr(u, '\n'); if (p) *p = 0;
        p = strchr(u, '\r'); if (p) *p = 0;
	strcat(t, ": Create log file\n");
	strcat(u, ": Created log file\n");
	/*WriteFile(afsi_file, t, strlen(t), &zilch, NULL);
          WriteFile(afsi_file, u, strlen(u), &zilch, NULL);*/
        /*write(afsi_file, t, strlen(t));
          write(afsi_file, u, strlen(u));*/
        fputs(t, afsi_file);
        fputs(u, afsi_file);
}

void
afsi_log(char *pattern, ...)
{
	char s[100], t[100], u[100];
	int zilch;
        time_t now;
	va_list ap;
#ifndef DEBUG
        return;
#endif
	va_start(ap, pattern);

	vsprintf(s, pattern, ap);
	/*GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, t, sizeof(t));*/
        time(&now);
        strcpy(t, asctime(localtime(&now)));
	sprintf(u, "%s: %s\n", t, s);
	if (afsi_file != NULL)
          /* fputs(u, stderr); */
          fputs(u, afsi_file);
          /*write(afsi_file, u, strlen(u));*/
        /*WriteFile(afsi_file, u, strlen(u), &zilch, NULL);*/
}

/*
 * Standard AFSD trace
 */

void afsd_ForceTrace(BOOL flush)
{
	FILE *handle;
	int len;
	char buf[100];

	if (!logReady) return;

	/*len = GetTempPath(99, buf);*/
	/*strcpy(&buf[len], "/afsd.log");*/
	strcpy(buf, "c:/afsd.log");
	/*handle = CreateFile(buf, GENERIC_WRITE, FILE_SHARE_READ,
          NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);*/
        /*handle = open(buf, O_RDWR | O_CREAT | O_RSHARE);*/
        handle = fopen(buf, "wt");
	if (handle == NULL) {
		logReady = 0;
		osi_panic("Cannot create log file", __FILE__, __LINE__);
	}
	osi_LogPrint(afsd_logp, handle);
	if (flush)
          fflush(handle);
        /*FlushFileBuffers(handle);*/
	/*CloseHandle(handle);*/
        fclose(handle);
}

/*------------------------------------------------------------------------------
  * ParseCacheInfoFile
  *
  * Description:
  *	Open the file containing the description of the workstation's AFS cache
  *	and pull out its contents.  The format of this file is as follows:
  *
  *	    cm_mountRoot:cacheBaseDir:cacheBlocks
  *
  * Arguments:
  *	None.
  *
  * Returns:
  *	0 if everything went well,
  *	1 otherwise.
  *
  * Environment:
  *	Nothing interesting.
  *
  *  Side Effects:
  *	Sets globals.
  *---------------------------------------------------------------------------*/

int ParseCacheInfoFile()
{
    static char rn[]="ParseCacheInfoFile";	/*This routine's name*/
    FILE *cachefd;				/*Descriptor for cache info file*/
    int	parseResult;				/*Result of our fscanf()*/
    int32 tCacheBlocks;
    char tCacheBaseDir[1024], *tbd, tCacheMountDir[1024], *tmd;
    /*char cacheBaseDir[1024];  /* cache in mem, this is ignored */

    if (afsd_debug)
	printf("%s: Opening cache info file '%s'...\n",
	    rn, cm_CacheInfoPath);

    cachefd = fopen(cm_CacheInfoPath, "r");
    if (!cachefd) {
	printf("%s: Can't read cache info file '%s'\n",
	       rn, cm_CacheInfoPath);
        return(1);
    }

    /*
     * Parse the contents of the cache info file.  All chars up to the first
     * colon are the AFS mount directory, all chars to the next colon are the
     * full path name of the workstation cache directory and all remaining chars
     * represent the number of blocks in the cache.
     */
    tCacheMountDir[0] = tCacheBaseDir[0] = '\0';
    parseResult = fscanf(cachefd,
			  "%1024[^;];%1024[^;];%d",
			  tCacheMountDir, tCacheBaseDir, &tCacheBlocks);

    /*
     * Regardless of how the parse went, we close the cache info file.
     */
    fclose(cachefd);

    if (parseResult == EOF || parseResult < 3) {
	printf("%s: Format error in cache info file!\n",
	       rn);
	if (parseResult == EOF)
	    printf("\tEOF encountered before any field parsed.\n");
	else
	    printf("\t%d out of 3 fields successfully parsed.\n",
		   parseResult);

	printf("\tcm_mountRoot: '%s'\n\tcm_cachePath: '%s'\n\tcacheBlocks: %d\n",
	       cm_mountRoot, cm_cachePath, cacheBlocks);
	return(1);
    }

    for (tmd = tCacheMountDir; *tmd == '\n' || *tmd == ' ' || *tmd == '\t'; tmd++) ;
    for (tbd = tCacheBaseDir; *tbd == '\n' || *tbd == ' ' || *tbd == '\t'; tbd++) ;
    /* now copy in the fields not explicitly overridden by cmd args */
    if (!sawMountRoot) 
    {
	strcpy(cm_mountRoot, tmd);
        cm_mountRootLen = strlen(tmd);
    }
    if (!sawCacheBaseDir)
      strcpy(cm_cachePath, tbd);
    if  (!sawCacheSize)
	cacheBlocks = tCacheBlocks;

    if (afsd_debug) {
	printf("%s: Cache info file successfully parsed:\n",
	       rn);
	printf("\tcm_mountRoot: '%s'\n\tcm_cachePath: '%s'\n\tcacheBlocks: %d\n",
	       tmd, tbd, tCacheBlocks);
    }
    /*printf("cm_cachePath: %s\n", cm_cachePath);*/

    /*PartSizeOverflow(tbd, cacheBlocks);*/

    return(0);
}

/*
 * AFSD Initialization
 */

int afsd_InitCM(char **reasonP, struct cmd_syndesc *as, char *arock)
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
	DWORD dummyLen;
	long code;
        struct cmd_syndesc *ts;
        char *afsconf_path;
        long diskCacheSize;
	/*WSADATA WSAjunk;

          WSAStartup(0x0101, &WSAjunk);*/

#ifndef DJGPP
	/* setup osidebug server at RPC slot 1000 */
	osi_LongToUID(1000, &debugID);
	code = osi_InitDebug(&debugID);
	afsi_log("osi_InitDebug code %d", code);
//	osi_LockTypeSetDefault("stat");	/* comment this out for speed *
	if (code != 0) {
		*reasonP = "unknown error";
		return -1;
	}
#endif 

	/* who are we ? */
	gethostname(cm_HostName, sizeof(cm_HostName));
#ifdef DJGPP
	/* For some reason, we may be getting space-padded hostnames.
	   If so, we take out the padding so that we can append -AFS later. */
	{
	  char *space = strchr(cm_HostName,' ');
	  if (space) *space = '\0';
	}
#endif
	afsi_log("gethostname %s", cm_HostName);
	thp = gethostbyname(cm_HostName);
	memcpy(&cm_HostAddr, thp->h_addr_list[0], 4);

	/* seed random number generator */
	srand(ntohl(cm_HostAddr));

	/* Get configuration parameters from command line */

        /* call atoi on the appropriate parsed results */

        if (!as->parms[20].items) {
          /* -startup */
          fprintf(stderr, "Please do not run this program directly.  Use the AFS Client Windows loader\nto start AFS.\n");
          exit(1);
        }

        if (as->parms[0].items) {
          /* -lanadapt */
          LANadapter = atoi(as->parms[0].items->data);
          lanAdaptSet = 1;
          afsi_log("LAN adapter number %d", LANadapter);
        }
        else
        {
          LANadapter = -1;
          afsi_log("Default LAN adapter number");
        }
        
        if (as->parms[1].items) {
          /* -threads */
          numSvThreads = atoi(as->parms[1].items->data);
          afsi_log("%d server threads", numSvThreads);
        }
        else
        {
          numSvThreads = CM_CONFIGDEFAULT_SVTHREADS;
          afsi_log("Defaulting to %d server threads", numSvThreads);
        }
        
        if (as->parms[2].items) {
          /* -rootvol */
          strcpy(cm_rootVolumeName, as->parms[2].items->data);
          rootVolSet = 1;
          afsi_log("Root volume %s", cm_rootVolumeName);
        }
        else
        {
          strcpy(cm_rootVolumeName, "root.afs");
          afsi_log("Default root volume name root.afs");
        }
        
        if (as->parms[3].items) {
          /* -stat */
          stats = atoi(as->parms[3].items->data);
          afsi_log("Status cache size %d", stats);
        }
        else
        {
          stats = CM_CONFIGDEFAULT_STATS;
          afsi_log("Default status cache size %d", stats);
        }
        
        if (as->parms[4].items) {
          /* -memcache */
          /* no-op */
        }
        
        if (as->parms[5].items) {
          /* -cachedir */
          /* no-op; cache is in memory, not mapped file */
          strcpy(cm_cachePath, as->parms[5].items->data);
          sawCacheBaseDir = 1;
        }
        
        if (as->parms[6].items) {
          /* -mountdir */
          strcpy(cm_mountRoot, as->parms[6].items->data);
          cm_mountRootLen = strlen(cm_mountRoot);
          sawMountRoot = 1;
          afsi_log("Mount root %s", cm_mountRoot);
        }
        else
        {
          strcpy(cm_mountRoot, "/afs");
          cm_mountRootLen = 4;
          /* Don't log */
        }
        
        if (as->parms[7].items) {
          /* -daemons */
          numBkgD = atoi(as->parms[7].items->data);
          afsi_log("%d background daemons", numBkgD);
        }
        else
        {
          numBkgD = CM_CONFIGDEFAULT_DAEMONS;
          afsi_log("Defaulting to %d background daemons", numBkgD);
        }
        
        if (as->parms[8].items) {
          /* -nosettime */
          cacheSetTime = FALSE;
        }
        
        if (as->parms[9].items) {
          /* -verbose */
          afsd_verbose = 1;
        }
        
        if (as->parms[10].items) {
          /* -debug */
          afsd_debug = 1;
          afsd_verbose = 1;
        }
        
        if (as->parms[11].items) {
          /* -chunksize */
          chunkSize = atoi(as->parms[11].items->data);
          if (chunkSize < 12 || chunkSize > 30) {
            afsi_log("Invalid chunk size %d, using default",
                     logChunkSize);
	    logChunkSize = CM_CONFIGDEFAULT_CHUNKSIZE;
          }
        } else {
          logChunkSize = CM_CONFIGDEFAULT_CHUNKSIZE;
          afsi_log("Default chunk size %d", logChunkSize);
        }
        cm_logChunkSize = logChunkSize;
        cm_chunkSize = 1 << logChunkSize;
        
        if (as->parms[12].items) {
          /* -dcache */
          cacheSize = atoi(as->parms[12].items->data);
          afsi_log("Cache size %d", cacheSize);
          sawCacheSize = 1;
        }
        else
        {
          cacheSize = CM_CONFIGDEFAULT_CACHESIZE;
          afsi_log("Default cache size %d", cacheSize);
        }
        
        afsconf_path = getenv("AFSCONF");
        if (!afsconf_path)
          strcpy(cm_confDir, AFSDIR_CLIENT_ETC_DIRPATH);
        else
          strcpy(cm_confDir, afsconf_path);
        if (as->parms[13].items) {
          /* -confdir */
          strcpy(cm_confDir, as->parms[13].items->data);
        }

        sprintf(cm_CacheInfoPath,  "%s/%s", cm_confDir, CACHEINFOFILE);

        sprintf(cm_AFSLogFile,  "%s/%s", cm_confDir, AFSLOGFILE);
        if (as->parms[14].items) {
          /* -logfile */
          strcpy(cm_AFSLogFile, as->parms[14].items->data);
        }

        if (as->parms[15].items) {
          /* -waitclose */
          afsd_CloseSynch = 1;
        }

        if (as->parms[16].items) {
          /* -shutdown */
          afs_shutdown = 1;
          /* 
           * Cold shutdown is the default
           */
          printf("afsd: Shutting down all afs processes and afs state\n");
          /*call_syscall(AFSOP_SHUTDOWN, 1);*/
          exit(0);
        }

        if (as->parms[17].items) {
          /* -sysname */
          strcpy(cm_sysName, as->parms[17].items->data);
        }
        else
          strcpy(cm_sysName, "i386_win95");
        
        if (as->parms[18].items) {
          /* -gateway */
          isGateway = 1;
          afsi_log("Set for %s service",
                   isGateway ? "gateway" : "stand-alone");
        }
        else
          isGateway = 0;

        if (as->parms[19].items) {
          /* -tracebuf */
          traceBufSize = atoi(as->parms[19].items->data);
          afsi_log("Trace Buffer size %d", traceBufSize);
        }
        else
        {
          traceBufSize = CM_CONFIGDEFAULT_TRACEBUFSIZE;
          afsi_log("Default trace buffer size %d", traceBufSize);
	}
        
        if (as->parms[21].items) {
          /* -diskcache */
          diskCacheSize = atoi(as->parms[21].items->data);
          cm_diskCacheEnabled = 1;
          afsi_log("Disk cache size %d K", diskCacheSize);
          /*printf("Disk cache size %d K", diskCacheSize);*/
          sawDiskCacheSize = 1;
        }
        else
        {
          diskCacheSize = 50000; /*CM_CONFIGDEFAULT_DISKCACHESIZE;*/
          afsi_log("Default disk cache size %d", diskCacheSize);
        }

        if (as->parms[22].items) {
           /* -noafsdb */
           cm_dnsEnabled = 0;
        }

        if (as->parms[23].items) {
           /* -freelance */
           cm_freelanceEnabled = 1;
        }

        if (ParseCacheInfoFile()) {
          exit(1);
        }

	/* setup early variables */
	/* These both used to be configurable. */
	smb_UseV3 = 1;
        buf_blockSize = CM_CONFIGDEFAULT_BLOCKSIZE;

	/* turn from 1024 byte units into memory blocks */
        cacheBlocks = (cacheSize * 1024) / buf_blockSize;
        afs_diskCacheChunks = (diskCacheSize * 1024) / buf_blockSize;
        /*printf("afs_diskCacheChunks=%d\n", afs_diskCacheChunks);*/

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
        
	/* setup and enable debug log */
	afsd_logp = osi_LogCreate("afsd", traceBufSize);
	afsi_log("osi_LogCreate log addr %x", afsd_logp);
        osi_LogEnable(afsd_logp);
	logReady = 1;

#if 0
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
#endif

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
	/*RpcInit();*/

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

#ifdef AFS_AFSDB_ENV
	/* initialize dns lookup */
	if (cm_InitDNS(cm_dnsEnabled) == -1)
	  cm_dnsEnabled = 0;  /* init failed, so deactivate */
	afsi_log("cm_InitDNS %d", cm_dnsEnabled);
#endif

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
	if (code != 0 && !cm_freelanceEnabled) {
        	*reasonP = "can't find root cell name in ThisCell";
		return -1;
	}
	else if (cm_freelanceEnabled)
	  cm_rootCellp = NULL;
	
	if (code == 0 && !cm_freelanceEnabled) {
	  cm_rootCellp = cm_GetCell(rootCellName, CM_FLAG_CREATE);
	  afsi_log("cm_GetCell addr %x", cm_rootCellp);
	  if (cm_rootCellp == NULL) {
	    *reasonP = "can't find root cell in CellServDB";
	    return -1;
	  }
	}

#ifdef AFS_FREELANCE_CLIENT
	if (cm_freelanceEnabled)
	  cm_InitFreelance();
#endif

	return 0;
}

int afsd_InitDaemons(char **reasonP)
{
	long code;
	cm_req_t req;

	cm_InitReq(&req);

	/* this should really be in an init daemon from here on down */

	if (!cm_freelanceEnabled) { 
	  code = cm_GetVolumeByName(cm_rootCellp, cm_rootVolumeName, cm_rootUserp,		&req, CM_FLAG_CREATE, &cm_rootVolumep);
	  afsi_log("cm_GetVolumeByName code %x root vol %x", code,
		   (code ? 0xffffffff : cm_rootVolumep));
	  if (code != 0) {
	    *reasonP = "can't find root volume in root cell";
	    return -1;
	  }
	}

        /* compute the root fid */
	if (!cm_freelanceEnabled) {
	  cm_rootFid.cell = cm_rootCellp->cellID;
	  cm_rootFid.volume = cm_GetROVolumeID(cm_rootVolumep);
	  cm_rootFid.vnode = 1;
	  cm_rootFid.unique = 1;
	}
	else
	  cm_FakeRootFid(&cm_rootFid);
        
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

int afsd_InitSMB(char **reasonP)
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
        strupr(hostName);
	smb_Init(afsd_logp, hostName, smb_UseV3, LANadapter, numSvThreads);
	afsi_log("smb_Init");

	return 0;
}

