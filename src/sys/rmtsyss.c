/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Daemon that implements remote procedure call service for non-vendor system
 * calls (currently setpag and pioctl). The AFS cache manager daemon, afsd,
 * currently fires up this module, when the "-rmtsys" flag is given.
 * This module resides in the lib/afs/librmtsys.a library.
 */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <sys/types.h>
#include <sys/ioctl.h>
#include <afs/vice.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdio.h>
#include <errno.h>
#include <rx/xdr.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
/*#include <afs/cellconfig.h>*/
#include "rmtsys.h"

extern RMTSYS_ExecuteRequest();

#define	NFS_EXPORTER	    1	/* To probably handle more later */
#define	PSETPAG		    110	/* Also defined in afs/afs_pioctl.c */
#define	PIOCTL_HEADER	    6	/* # of words prepended to special pioctl */
#define	PSetClientContext   99	/* Sets up caller's creds */
#define N_SECURITY_OBJECTS  1	/* No real security yet */

#define SETCLIENTCONTEXT(BLOB, host, uid, group0, group1, cmd, exportertype) { \
           (BLOB)[0] = (host); \
	   (BLOB)[1] = (uid); \
	   (BLOB)[2] = (group0); \
	   (BLOB)[3] = (group1); \
	   (BLOB)[4] = (cmd); \
           (BLOB)[5] = (exportertype); \
}


/* Main routine of the remote AFS system call server. The calling process will
 * never return; this is currently called from afsd (when "-rmtsys" is passed
 * as a parameter) */
rmtsysd()
{
/*  void catchsig(int); */
    struct rx_securityClass *(securityObjects[N_SECURITY_OBJECTS]);
    struct rx_service *service;

    /* 
     * Ignore SIGHUP signal since apparently is sent to the processes that
     * start up from /etc/rc for some systems like hpux and aix3.1... 
     */
    signal(SIGHUP, SIG_IGN);

    /* Initialize the rx-based RMTSYS server */
    if (rx_Init(htons(AFSCONF_RMTSYSPORT)) < 0)
	rmt_Quit("rx_init");
    securityObjects[0] = rxnull_NewServerSecurityObject();
    if (securityObjects[0] == (struct rx_securityClass *)0)
	rmt_Quit("rxnull_NewServerSecurityObject");
    service =
	rx_NewService(0, RMTSYS_SERVICEID, AFSCONF_RMTSYSSERVICE,
		      securityObjects, N_SECURITY_OBJECTS,
		      RMTSYS_ExecuteRequest);
    if (service == (struct rx_service *)0)
	rmt_Quit("rx_NewService");
    /* One may wish to tune some default RX params for better performance
     * at some point... */
    rx_SetMaxProcs(service, 2);
    rx_StartServer(1);		/* Donate this process to the server process pool */
    return 0; /* not reached */
}


/* Implements the remote setpag(2) call. Note that unlike the standard call,
 * here we also get back the new pag value; we need this so that the caller
 * can add it to its group list via setgroups() */
afs_int32
SRMTSYS_SetPag(call, creds, newpag, errornumber)
     struct rx_call *call;
     clientcred *creds;
     afs_int32 *newpag, *errornumber;
{
    afs_uint32 blob[PIOCTL_HEADER];
    struct ViceIoctl data;
    register afs_int32 error;

    *errornumber = 0;
    SETCLIENTCONTEXT(blob, rx_HostOf(call->conn->peer), creds->uid,
		     creds->group0, creds->group1, PSETPAG, NFS_EXPORTER);
    data.in = (caddr_t) blob;
    data.in_size = sizeof(blob);
    data.out = (caddr_t) blob;
    data.out_size = sizeof(blob);
    /* force local pioctl call */
    error = lpioctl(0, _VICEIOCTL(PSetClientContext), &data, 1);
    if (error) {
	if (errno == PSETPAG) {
	    *newpag = blob[0];	/* new pag value */
	} else
	    *errornumber = errno;
    }
    return 0;
}


/* Implements the remote pioctl(2) call */
afs_int32
SRMTSYS_Pioctl(call, creds, path, cmd, follow, InData, OutData, errornumber)
     struct rx_call *call;
     clientcred *creds;
     afs_int32 cmd, follow, *errornumber;
     char *path;
     rmtbulk *InData, *OutData;
{
    register afs_int32 error;
    struct ViceIoctl data;
    char *pathp = path;
    afs_uint32 blob[PIOCTL_HEADER];

    *errornumber = 0;
    SETCLIENTCONTEXT(blob, rx_HostOf(call->conn->peer), creds->uid,
		     creds->group0, creds->group1, cmd, NFS_EXPORTER);
    data.in =
	(char *)malloc(InData->rmtbulk_len +
		       PIOCTL_HEADER * sizeof(afs_int32));
    if (!data.in)
	return (-1);		/* helpless here */
    if (!strcmp(path, NIL_PATHP))
	pathp = (char *)0;	/* It meant to be NIL */
    memcpy(data.in, blob, sizeof(blob));
    inparam_conversion(cmd, InData->rmtbulk_val, 1);
    memcpy(data.in + sizeof(blob), InData->rmtbulk_val, InData->rmtbulk_len);
    data.in_size = InData->rmtbulk_len + PIOCTL_HEADER * sizeof(afs_int32);
    data.out = OutData->rmtbulk_val;
    data.out_size = OutData->rmtbulk_len;
    /* force local pioctl call */
    error = lpioctl(pathp, _VICEIOCTL(PSetClientContext), &data, follow);
    if (error) {
	*errornumber = errno;
    } else {
	/* Send the results back in network order */
	outparam_conversion(cmd, data.out, 0);
    }
    free(data.in);
    /* Note that we return success (i.e. 0) even when pioctl fails; that's
     * because the actual errno is passed back via 'errornumber' and this call
     * MUST return success error in order to get that OUT params back (YUCK!)
     */
    return (0);
}

rmt_Quit(msg, a, b)
     char *msg;
{
    fprintf(stderr, msg, a, b);
    exit(1);
}
