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

#include <afs/stds.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#ifdef AFS_NT40_ENV
#include <windows.h>
#include <winsock2.h>
#include <io.h>
#include <afs/dirent.h>
#include <WINNT/afsreg.h>
#else
#include <unistd.h>
#include <dirent.h>
#include <math.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/param.h>
#endif /* AFS_NT40_ENV */

#include <pthread.h>

#include <afs/afs_Admin.h>
#include <afs/afs_AdminErrors.h>
#include <afs/afs_bosAdmin.h>
#include <afs/afs_clientAdmin.h>

#include <afs/dirpath.h>

#include "cfginternal.h"
#include "../adminutil/afs_AdminInternal.h"


/*
 * cfgutil_HostHandleValidate() -- validate a host configuration handle
 *
 * RETURN CODES: 1 success, 0 failure
 */
int
cfgutil_HostHandleValidate(const cfg_host_p cfg_host, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst = 0;

    if (cfg_host == NULL) {
	tst = ADMCFGHOSTHANDLENULL;

    } else if (cfg_host->begin_magic != BEGIN_MAGIC
	       || cfg_host->end_magic != END_MAGIC) {
	tst = ADMCFGHOSTHANDLEBADMAGIC;

    } else if (cfg_host->is_valid == 0) {
	tst = ADMCFGHOSTHANDLEINVALID;

    } else if (cfg_host->hostName == NULL) {
	tst = ADMCFGHOSTHANDLEHOSTNAMENULL;

    } else if (cfg_host->cellHandle == NULL) {
	tst = ADMCFGHOSTHANDLECELLHANDLENULL;

    } else if (cfg_host->cellName == NULL) {
	tst = ADMCFGHOSTHANDLECELLNAMENULL;
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfgutil_HostHandleBosInit() -- initialize bosserver handle in host
 *     configuration handle.
 * 
 * RETURN CODES: 1 success, 0 failure
 */
int
cfgutil_HostHandleBosInit(cfg_host_p cfg_host, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst = 0;

    if (pthread_mutex_lock(&cfg_host->mutex)) {
	tst = ADMMUTEXLOCK;
    } else {
	if (cfg_host->bosHandle == NULL) {
	    /* initialize bosserver handle for host */
	    void *bosHandle;
	    afs_status_t tst2;

	    if (bos_ServerOpen
		(cfg_host->cellHandle, cfg_host->hostName, &bosHandle,
		 &tst2)) {
		cfg_host->bosHandle = bosHandle;
	    } else {
		tst = tst2;
	    }
	}
	if (pthread_mutex_unlock(&cfg_host->mutex)) {
	    /* can only return one status; mutex failure is critical */
	    tst = ADMMUTEXUNLOCK;
	}
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfgutil_HostHandleCellNameCompatible() -- determine if specified cell name
 *     is compatible with the cell name in the given cfg handle.
 * 
 * RETURN CODES: 1 compatible, 0 not compatible
 */
int
cfgutil_HostHandleCellNameCompatible(const cfg_host_p cfg_host,
				     const char *cellName)
{
    int rc;

    if (*cfg_host->cellName == '\0') {
	/* null cell handle; any cell name compatible by definition */
	rc = 1;
    } else {
	/* standard cell handle; compare cell names */
	rc = (strcasecmp(cfg_host->cellName, cellName) == 0 ? 1 : 0);
    }
    return rc;
}


/*
 * cfgutil_HostNameGetFull() -- return fully qualified version of specified
 *     host name.
 *
 *     Note: fullHostName is presumed to be a buffer of size MAXHOSTCHARS.
 *
 * RETURN CODES: 1 success, 0 failure
 */
int
cfgutil_HostNameGetFull(const char *hostName, char *fullHostName,
			afs_status_p st)
{
    int rc = 1;
    afs_status_t tst = 0;

#ifdef AFS_NT40_ENV
    /* Note: gethostbyname() allocs hostent on a per-thread basis */
    struct hostent *hentryp;

    if ((hentryp = gethostbyname(hostName)) == NULL) {
	tst = ADMCFGCANTRESOLVEHOSTNAME;
    } else {
	size_t hostNameLen = strlen(hostName);
	char *fqName = hentryp->h_name;

	/* verify that canonical name is an expansion of name specified */
	if (strncasecmp(fqName, hostName, hostNameLen) != 0
	    || fqName[hostNameLen] != '.') {
	    /* canonical name not a direct expansion; consider aliases */
	    int i;

	    for (i = 0; hentryp->h_aliases[i] != NULL; i++) {
		char *aliasName = hentryp->h_aliases[i];
		if (strncasecmp(aliasName, hostName, hostNameLen) == 0
		    && aliasName[hostNameLen] == '.') {
		    /* found a direct exapansion of specified name */
		    fqName = aliasName;
		    break;
		}
	    }
	}

	if (strlen(fqName) > (MAXHOSTCHARS - 1)) {
	    tst = ADMCFGRESOLVEDHOSTNAMETOOLONG;
	} else {
	    strcpy(fullHostName, fqName);

	    /* lower-case name for consistency */
	    _strlwr(fullHostName);
	}
    }
#else
    /* function not yet implemented for Unix */
    tst = ADMCFGNOTSUPPORTED;
#endif /* AFS_NT40_ENV */

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfgutil_HostNameIsAlias() -- determine if specified host names
 *     are aliases (functionally equivalent).
 *
 * RETURN CODES: 1 success, 0 failure
 */
int
cfgutil_HostNameIsAlias(const char *hostName1, const char *hostName2,
			short *isAlias, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;
    int addrCount1, addrCount2;
    afs_int32 *addrList1 = NULL, *addrList2 = NULL;

    /* get all addrs for first host */

    if (!cfgutil_HostAddressFetchAll
	(hostName1, &addrCount1, &addrList1, &tst2)) {
	tst = tst2;
    }

    /* get all addrs for second host */

    if (tst == 0) {
	if (!cfgutil_HostAddressFetchAll
	    (hostName2, &addrCount2, &addrList2, &tst2)) {
	    tst = tst2;
		if (addrList1) {
			free(addrList1);
			addrList1 = NULL;
		}
	}
    }

    /* compare lists looking for a match */

    if (tst == 0) {
	int i, j;

	*isAlias = 0;

	for (i = 0; i < addrCount1; i++) {
	    for (j = 0; j < addrCount2; j++) {
		if (addrList1[i] == addrList2[j]) {
		    *isAlias = 1;
		    break;
		}
	    }
	}
		if (addrList1) {
			free(addrList1);
			addrList1 = NULL;
		}
		if (addrList2) {
			free(addrList2);
			addrList2 = NULL;
		}
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfgutil_HostNameIsLocal() -- determine if the specified host name is
 *     equivalent to (is an alias for) the standard host name for the
 *     machine on which this process is running.
 *
 * RETURN CODES: 1 success, 0 failure
 */
int
cfgutil_HostNameIsLocal(const char *hostName, short *isLocal, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;
    char localName[MAXHOSTCHARS];

    if (gethostname(localName, MAXHOSTCHARS)) {
	/* failed to lookup local name */
	tst = ADMCANTGETLOCALNAME;
    } else if (!cfgutil_HostNameIsAlias(hostName, localName, isLocal, &tst2)) {
	tst = tst2;
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfgutil_HostNameGetCellServDbAlias() -- Get alias for given host name
 *     as listed in the server CellServDB on the specified host.  If no
 *     alias is found then hostNameAlias is set to the empty string.
 *
 *     Note: hostNameAlias is presumed to be a buffer of size MAXHOSTCHARS.
 *
 * RETURN CODES: 1 success, 0 failure
 */
int
cfgutil_HostNameGetCellServDbAlias(const char *fsDbHost, const char *hostName,
				   char *hostNameAlias, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;
    void *cellHandle;
    void *bosHandle;

    if (!afsclient_NullCellOpen(&cellHandle, &tst2)) {
	tst = tst2;
    } else {
	if (!bos_ServerOpen(cellHandle, fsDbHost, &bosHandle, &tst2)) {
	    tst = tst2;
	} else {
	    void *dbIter;

	    if (!bos_HostGetBegin(bosHandle, &dbIter, &tst2)) {
		tst = tst2;
	    } else {
		short dbhostDone = 0;
		short dbhostFound = 0;

		while (!dbhostDone) {
		    short isAlias;

		    if (!bos_HostGetNext(dbIter, hostNameAlias, &tst2)) {
			/* no more entries (or failure) */
			if (tst2 != ADMITERATORDONE) {
			    tst = tst2;
			}
			dbhostDone = 1;

		    } else
			if (!cfgutil_HostNameIsAlias
			    (hostName, hostNameAlias, &isAlias, &tst2)) {
			tst = tst2;
			dbhostDone = 1;

		    } else if (isAlias) {
			dbhostFound = 1;
			dbhostDone = 1;
		    }
		}

		if (!dbhostFound) {
		    *hostNameAlias = '\0';
		}

		if (!bos_HostGetDone(dbIter, &tst2)) {
		    tst = tst2;
		}
	    }

	    if (!bos_ServerClose(bosHandle, &tst2)) {
		tst = tst2;
	    }
	}

	if (!afsclient_CellClose(cellHandle, &tst2)) {
	    tst = tst2;
	}
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfgutil_HostNameGetAddressString() -- Get IP address for specified host in
 *     in the canonical string form.
 *
 *     Returns pointer to a per-thread buffer; do not deallocate.
 */
int
cfgutil_HostNameGetAddressString(const char *hostName, const char **hostAddr,
				 afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;
    int addrCount;
    afs_int32 *addrList = NULL;

    /* get address list for host */

    if (!cfgutil_HostAddressFetchAll(hostName, &addrCount, &addrList, &tst2)) {
	tst = tst2;
    }

    /* convert first (canonical) address to string */

    if (tst == 0) {
	struct in_addr ina;
	char *inaString;

	ina.s_addr = htonl(addrList[0]);
	if ((inaString = inet_ntoa(ina)) == NULL) {
	    /* should never happen */
	    tst = ADMCFGCANTRESOLVEHOSTNAME;
	} else {
	    *hostAddr = inaString;
	}
		if (addrList) {
			free(addrList);
			addrList = NULL;
		}
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfgutil_HostAddressFetchAll() -- get allocated list of all known
 *     addresses for specified host.
 *
 *     Note: upon success, *addrList is an address array in host byte order.
 *
 * RETURN CODES: 1 success, 0 failure
 */
int
cfgutil_HostAddressFetchAll(const char *hostName, int *addrCount,
			    afs_int32 ** addrList, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst = 0;
    int aCount = 0;
    afs_int32 *aList = NULL;

#ifdef AFS_NT40_ENV
    /* Note: gethostbyname() allocs hostent on a per-thread basis */
    struct hostent *hentryp;

    if ((hentryp = gethostbyname(hostName)) == NULL) {
	tst = ADMCFGCANTRESOLVEHOSTNAME;
    } else {
	int i;

	/* assuming IPv4 addrs returned */
	for (i = 0; hentryp->h_addr_list[i] != NULL; i++);
	aCount = i;

	if ((aList =
	     (afs_int32 *) malloc(aCount * sizeof(afs_int32))) == NULL) {
	    tst = ADMNOMEM;
	} else {
	    for (i = 0; i < aCount; i++) {
		afs_int32 hostAddr;
		memcpy((void *)&hostAddr, (void *)hentryp->h_addr_list[i],
		       sizeof(afs_int32));
		aList[i] = ntohl(hostAddr);
	    }
	}
    }
#else
    /* function not yet implemented for Unix */
    tst = ADMCFGNOTSUPPORTED;
#endif /* AFS_NT40_ENV */

    if (tst == 0) {
	/* return results */
	*addrCount = aCount;
	*addrList = aList;
    } else {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfgutil_HostAddressIsValid() -- determine if address is a valid address
 *     for the named host.
 *
 *     Note: hostAddr must be specified in host byte order
 *
 * RETURN CODES: 1 success, 0 failure
 */
int
cfgutil_HostAddressIsValid(const char *hostName, int hostAddr, short *isValid,
			   afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;
    int addrCount;
    afs_int32 *addrList = NULL;

    *isValid = 0;

    /* get all addrs for host */

    if (!cfgutil_HostAddressFetchAll(hostName, &addrCount, &addrList, &tst2)) {
	tst = tst2;
    }

    /* search list looking for match */

    if (tst == 0) {
	int i;

	for (i = 0; i < addrCount; i++) {
	    if (addrList[i] == hostAddr) {
		*isValid = 1;
		break;
	    }
	}
		if (addrList) {
			free(addrList);
			addrList = NULL;
		}
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfgutil_CleanDirectory() -- remove all files from specified directory;
 *     function is NOT recursive.
 *
 * RETURN CODES: 1 success, 0 failure
 */
int
cfgutil_CleanDirectory(const char *dirName, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst = 0;

    DIR *dirp;
    struct dirent *entryp;
    char dirfile[MAXPATHLEN];

    if ((dirp = opendir(dirName)) == NULL) {
	/* cannot open directory */
	if (errno == EACCES) {
	    tst = ADMNOPRIV;
	}
	/* otherwise assume directory does not exist, which is OK */
    } else {
	while ((entryp = readdir(dirp)) != NULL) {
	    /* remove file (except "." and "..") */
	    if (strcmp(entryp->d_name, ".") && strcmp(entryp->d_name, "..")) {
		sprintf(dirfile, "%s/%s", dirName, entryp->d_name);
		if (unlink(dirfile)) {
		    /* remove failed */
		    if (errno == EACCES) {
			tst = ADMNOPRIV;
			break;
		    }
		    /* otherwise assume file removed by someone else or
		     * that file is a directory, which is OK.
		     */
		}
	    }
	}

	(void)closedir(dirp);
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfgutil_HostSetNoAuthFlag() -- set AFS server authentication flag on host
 *
 * RETURN CODES: 1 success, 0 failure (st indicates why)
 */
int
cfgutil_HostSetNoAuthFlag(const cfg_host_p cfg_host, short noAuth,
			  afs_status_p st)
{
    int rc = 1;
    afs_status_t tst = 0;

    /* remote configuration not yet supported in this function */

    if (!cfg_host->is_local) {
	tst = ADMCFGNOTSUPPORTED;
    }

    /* set mode; not using bosserver because may not have necessary creds */

    if (tst == 0) {
	if (noAuth) {
	    /* set no-authentication flag */
	    int fd = open(AFSDIR_SERVER_NOAUTH_FILEPATH,
			  O_CREAT | O_TRUNC | O_RDWR, 0666);

	    if (fd >= 0) {
		close(fd);
	    } else {
		if (errno == EACCES) {
		    tst = ADMNOPRIV;
		} else {
		    tst = ADMCFGHOSTSETNOAUTHFAILED;
		}
	    }
	} else {
	    /* clear no-authentication flag */
	    if (unlink(AFSDIR_SERVER_NOAUTH_FILEPATH)) {
		if (errno != ENOENT) {
		    if (errno == EACCES) {
			tst = ADMNOPRIV;
		    } else {
			tst = ADMCFGHOSTSETNOAUTHFAILED;
		    }
		}
	    }
	}
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfgutil_Sleep() -- put thread to sleep for specified number of seconds.
 */
void
cfgutil_Sleep(unsigned sec)
{
#ifdef AFS_NT40_ENV
    Sleep(sec * 1000);
#else
    time_t timeStart = time(NULL);
    struct timeval sleeptime;

    sleeptime.tv_sec = sec;
    sleeptime.tv_usec = 0;

    while (1) {
	if (select(0, 0, 0, 0, &sleeptime) == 0) {
	    /* timeout */
	    break;
	} else {
	    /* returned for reason other than timeout */
	    double cumSec = difftime(time(NULL), timeStart);
	    double remSec = (double)sec - cumSec;

	    if (remSec <= 0.0) {
		break;
	    }
	    sleeptime.tv_sec = ceil(remSec);
	}
    }
#endif
}



#ifdef AFS_NT40_ENV
/* Service control functions */

/* define generic service error codes */
#define CFGUTIL_SVC_NOPRIV      1	/* insufficient privilege */
#define CFGUTIL_SVC_BAD         2	/* service not properly configured */
#define CFGUTIL_SVC_NOTREADY    3	/* service not ready to accept command */
#define CFGUTIL_SVC_TIMEOUT     4	/* timed out waiting for stop/start */
#define CFGUTIL_SVC_STATUSUNK   5	/* service status cannot be determined */


/*
 * ServiceCodeXlate() -- translate generic code to service-specific code
 */
static afs_status_t
ServiceCodeXlate(LPCTSTR svcName, int code)
{
    afs_status_t tst = ADMCFGNOTSUPPORTED;

    if (!strcmp(svcName, AFSREG_CLT_SVC_NAME)) {
	/* AFS client (CM) service code required */
	switch (code) {
	case CFGUTIL_SVC_NOPRIV:
	    tst = ADMNOPRIV;
	    break;
	case CFGUTIL_SVC_BAD:
	    tst = ADMCFGCACHEMGRSERVICEBAD;
	    break;
	case CFGUTIL_SVC_NOTREADY:
	    tst = ADMCFGCACHEMGRSERVICENOTREADY;
	    break;
	case CFGUTIL_SVC_TIMEOUT:
	    tst = ADMCFGCACHEMGRSERVICETIMEOUT;
	    break;
	default:
	    tst = ADMCFGCACHEMGRSERVICESTATUSUNK;
	    break;
	}

    } else if (!strcmp(svcName, AFSREG_SVR_SVC_NAME)) {
	/* AFS BOS control service code required */
	switch (code) {
	case CFGUTIL_SVC_NOPRIV:
	    tst = ADMNOPRIV;
	    break;
	case CFGUTIL_SVC_BAD:
	    tst = ADMCFGBOSSERVERCTLSERVICEBAD;
	    break;
	case CFGUTIL_SVC_NOTREADY:
	    tst = ADMCFGBOSSERVERCTLSERVICENOTREADY;
	    break;
	case CFGUTIL_SVC_TIMEOUT:
	    tst = ADMCFGBOSSERVERCTLSERVICETIMEOUT;
	    break;
	default:
	    tst = ADMCFGBOSSERVERCTLSERVICESTATUSUNK;
	    break;
	}
    }
    return tst;
}


/*
 * cfgutil_WindowsServiceStart() -- Start a Windows service on local host.
 *
 *     The value of timeout specifies the maximum time, in seconds, to wait
 *     for the service to be in the SERVICE_RUNNING state.
 *
 *     If service was already started/starting then *wasRunning is true (1).
 *
 * RETURN CODES: 1 success, 0 failure (st indicates why)
 */
int
cfgutil_WindowsServiceStart(LPCTSTR svcName, DWORD svcArgc, LPCTSTR * svcArgv,
			    unsigned timeout, short *wasRunning,
			    afs_status_p st)
{
    int rc = 1;
    afs_status_t tst = 0;
    SC_HANDLE scmHandle, svcHandle;
    DWORD svcAccess = (SERVICE_START | SERVICE_QUERY_STATUS);

    *wasRunning = 0;

    if ((scmHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT)) == NULL
	|| (svcHandle = OpenService(scmHandle, svcName, svcAccess)) == NULL) {
	/* can't connect to SCM or can't open service */
	DWORD status = GetLastError();

	if (status == ERROR_ACCESS_DENIED) {
	    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_NOPRIV);
	} else if (status == ERROR_SERVICE_DOES_NOT_EXIST) {
	    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_BAD);
	} else {
	    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_STATUSUNK);
	}

	if (scmHandle != NULL) {
	    CloseServiceHandle(scmHandle);
	}

    } else {
	/* service configured; attempt to start */
	if (!StartService(svcHandle, svcArgc, svcArgv)) {
	    /* service start failed */
	    DWORD status = GetLastError();

	    if (status == ERROR_SERVICE_ALREADY_RUNNING) {
		*wasRunning = 1;
	    } else {
		if (status == ERROR_ACCESS_DENIED) {
		    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_NOPRIV);
		} else if (status == ERROR_SERVICE_DATABASE_LOCKED
			   || status == ERROR_SERVICE_DISABLED
			   || status == ERROR_SERVICE_REQUEST_TIMEOUT) {
		    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_NOTREADY);
		} else {
		    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_BAD);
		}
	    }
	}

	/* wait for service to be in SERVICE_RUNNING state */
	if (tst == 0 && timeout > 0) {
	    SERVICE_STATUS svcStatus;
	    time_t timeStart = time(NULL);

	    while (1) {
		if (!QueryServiceStatus(svcHandle, &svcStatus)) {
		    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_STATUSUNK);
		    break;
		} else if (svcStatus.dwCurrentState == SERVICE_RUNNING) {
		    break;
		} else if (difftime(time(NULL), timeStart) > timeout) {
		    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_TIMEOUT);
		    break;
		} else {
		    /* sleep a bit and check state again */
		    cfgutil_Sleep(5);
		}
	    }

	    if (tst == 0) {
		/* wait just a bit more because we're paranoid */
		cfgutil_Sleep(1);
	    }
	}

	CloseServiceHandle(svcHandle);
	CloseServiceHandle(scmHandle);
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfgutil_WindowsServiceStop() -- Stop a Windows service on local host.
 *
 *     The value of timeout specifies the maximum time, in seconds, to wait
 *     for the service to be in the SERVICE_STOPPED state.
 *
 *     If service was already stopped/stopping then *wasStopped is true (1).
 *
 * RETURN CODES: 1 success, 0 failure (st indicates why)
 */
int
cfgutil_WindowsServiceStop(LPCTSTR svcName, unsigned timeout,
			   short *wasStopped, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst = 0;
    SC_HANDLE scmHandle, svcHandle;
    DWORD svcAccess = (SERVICE_STOP | SERVICE_QUERY_STATUS);

    *wasStopped = 0;

    if ((scmHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT)) == NULL
	|| (svcHandle = OpenService(scmHandle, svcName, svcAccess)) == NULL) {
	/* can't connect to SCM or can't open service */
	DWORD status = GetLastError();

	if (status == ERROR_ACCESS_DENIED) {
	    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_NOPRIV);
	} else if (status == ERROR_SERVICE_DOES_NOT_EXIST) {
	    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_BAD);
	} else {
	    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_STATUSUNK);
	}

	if (scmHandle != NULL) {
	    CloseServiceHandle(scmHandle);
	}

    } else {
	/* service configured; attempt to stop */
	SERVICE_STATUS svcStatus;

	if (!ControlService(svcHandle, SERVICE_CONTROL_STOP, &svcStatus)) {
	    /* service stop failed */
	    DWORD status = GetLastError();

	    if (status == ERROR_SERVICE_NOT_ACTIVE) {
		*wasStopped = 1;
	    } else {
		if (status == ERROR_ACCESS_DENIED) {
		    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_NOPRIV);
		} else if (status == ERROR_INVALID_SERVICE_CONTROL
			   || status == ERROR_SERVICE_CANNOT_ACCEPT_CTRL
			   || status == ERROR_SERVICE_REQUEST_TIMEOUT) {
		    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_NOTREADY);
		} else {
		    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_BAD);
		}
	    }
	}

	/* wait for service to be in SERVICE_STOPPED state */
	if (tst == 0 && timeout > 0) {
	    time_t timeStart = time(NULL);

	    while (1) {
		if (!QueryServiceStatus(svcHandle, &svcStatus)) {
		    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_STATUSUNK);
		    break;
		} else if (svcStatus.dwCurrentState == SERVICE_STOPPED) {
		    break;
		} else if (difftime(time(NULL), timeStart) > timeout) {
		    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_TIMEOUT);
		    break;
		} else {
		    /* sleep a bit and check state again */
		    cfgutil_Sleep(5);
		}
	    }

	    if (tst == 0) {
		/* wait just a bit more because we're paranoid */
		cfgutil_Sleep(1);
	    }
	}

	CloseServiceHandle(svcHandle);
	CloseServiceHandle(scmHandle);
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfgutil_WindowsServiceQuery() -- Query Windows service on local host.
 * 
 * RETURN CODES: 1 success, 0 failure (st indicates why)
 */
int
cfgutil_WindowsServiceQuery(LPCTSTR svcName, DWORD * svcState,
			    afs_status_p st)
{
    int rc = 1;
    afs_status_t tst = 0;
    SC_HANDLE scmHandle, svcHandle;
    DWORD svcAccess = SERVICE_QUERY_STATUS;

    if ((scmHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT)) == NULL
	|| (svcHandle = OpenService(scmHandle, svcName, svcAccess)) == NULL) {
	/* can't connect to SCM or can't open service */
	DWORD status = GetLastError();

	if (status == ERROR_ACCESS_DENIED) {
	    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_NOPRIV);
	} else if (status == ERROR_SERVICE_DOES_NOT_EXIST) {
	    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_BAD);
	} else {
	    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_STATUSUNK);
	}

	if (scmHandle != NULL) {
	    CloseServiceHandle(scmHandle);
	}

    } else {
	/* service configured; determine service state */
	SERVICE_STATUS svcStatus;

	if (!QueryServiceStatus(svcHandle, &svcStatus)) {
	    tst = ServiceCodeXlate(svcName, CFGUTIL_SVC_STATUSUNK);
	} else {
	    *svcState = svcStatus.dwCurrentState;
	}

	CloseServiceHandle(svcHandle);
	CloseServiceHandle(scmHandle);
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

#endif /* AFS_NT40_ENV */
