/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_CFG_INTERNAL_H
#define OPENAFS_CFG_INTERNAL_H

/* Define types, macros, etc., internal to the configuration library */

typedef struct {
    int begin_magic;		/* begin and end magic help validate cfg handle */
    int is_valid;		/* true if cfg handle is valid */
    char *hostName;		/* name of host being configured */
    int is_local;		/* true if hostName specifies the local host */
    void *cellHandle;		/* cell handle establishing working cell */
    const char *cellName;	/* cell name in cell handle */
    pthread_mutex_t mutex;	/* protects bosHandle initialization */
    void *bosHandle;		/* handle for bosserver on host */
    int end_magic;
} cfg_host_t, *cfg_host_p;


/* Declare utility functions internal to the configuration library */

extern int
  cfgutil_HostHandleValidate(const cfg_host_p cfg_host, afs_status_p st);

extern int
  cfgutil_HostHandleBosInit(cfg_host_p cfg_host, afs_status_p st);

extern int
  cfgutil_HostHandleCellNameCompatible(const cfg_host_p cfg_host,
				       const char *cellName);

extern int
  cfgutil_HostNameGetFull(const char *hostName, char *fullHostName,
			  afs_status_p st);

extern int
  cfgutil_HostNameIsAlias(const char *hostName1, const char *hostName2,
			  short *isAlias, afs_status_p st);

extern int
  cfgutil_HostNameIsLocal(const char *hostName, short *isLocal,
			  afs_status_p st);

extern int
  cfgutil_HostNameGetCellServDbAlias(const char *fsDbHost,
				     const char *hostName,
				     char *hostNameAlias, afs_status_p st);

extern int
  cfgutil_HostNameGetAddressString(const char *hostName,
				   const char **hostAddr, afs_status_p st);

extern int
  cfgutil_HostAddressFetchAll(const char *hostName, int *addrCount,
			      afs_int32 ** addrList, afs_status_p st);

extern int
  cfgutil_HostAddressIsValid(const char *hostName, int hostAddr,
			     short *isValid, afs_status_p st);

extern int
  cfgutil_CleanDirectory(const char *dirName, afs_status_p st);

extern int
  cfgutil_HostSetNoAuthFlag(const cfg_host_p cfg_host, short noAuth,
			    afs_status_p st);

extern void
  cfgutil_Sleep(unsigned sec);


#ifdef AFS_NT40_ENV
/* Service control functions */

extern int
  cfgutil_WindowsServiceStart(LPCTSTR svcName, DWORD svcArgc,
			      LPCTSTR * svcArgv, unsigned timeout,
			      short *wasRunning, afs_status_p st);

extern int
  cfgutil_WindowsServiceStop(LPCTSTR svcName, unsigned timeout,
			     short *wasStopped, afs_status_p st);

extern int
  cfgutil_WindowsServiceQuery(LPCTSTR svcName, DWORD * svcState,
			      afs_status_p st);
#endif /* AFS_NT40_ENV */

#endif /* OPENAFS_CFG_INTERNAL_H */
