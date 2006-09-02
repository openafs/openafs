/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * This module (residing in lib/afs/librmtsys.a) implements the client side of
 * the rpc version (via rx) of non-standard system calls. Currently only rpc
 * calls of setpag, and pioctl are supported.
 */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/sys/rmtsysc.c,v 1.11.2.1 2006/07/31 15:33:46 shadow Exp $");

#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <afs/vice.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/file.h>
#endif
#include <sys/stat.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <rx/xdr.h>
#include "rmtsys.h"


#define	NOPAG	    0xffffffff	/* Also defined in afs/afs.h */
static afs_int32 hostAddr = 0;
static int hostAddrLookup = 0;
char *afs_server = 0, server_name[128];
static afs_int32 SetClientCreds();

/* Picks up the name of the remote afs client host where the rmtsys 
 * daemon resides. Since the clients may be diskless and/or readonly
 * ones we felt it's better to rely on an shell environment
 * (AFSSERVER) for the host name first. If that is not set, the
 * $HOME/.AFSSERVER file is checked, otherwise the "/.AFSSERVER" is
 * used.
 */
afs_int32
GetAfsServerAddr(char *syscall)
{
    register struct hostent *th;
    char *getenv();

    if (hostAddrLookup) {
	/* Take advantage of caching and assume that the remote host
	 * address won't change during a single program's invocation.
	 */
	return hostAddr;
    }
    hostAddrLookup = 1;

    if (!(afs_server = getenv("AFSSERVER"))) {
	char *home_dir;
	FILE *fp;
	int len;

	if (!(home_dir = getenv("HOME"))) {
	    /* Our last chance is the "/.AFSSERVER" file */
	    fp = fopen("/.AFSSERVER", "r");
	    if (fp == 0) {
		return 0;
	    }
	    fgets(server_name, 128, fp);
	    fclose(fp);
	} else {
	    char pathname[256];

	    sprintf(pathname, "%s/%s", home_dir, ".AFSSERVER");
	    fp = fopen(pathname, "r");
	    if (fp == 0) {
		/* Our last chance is the "/.AFSSERVER" file */
		fp = fopen("/.AFSSERVER", "r");
		if (fp == 0) {
		    return 0;
		}
	    }
	    fgets(server_name, 128, fp);
	    fclose(fp);
	}
	len = strlen(server_name);
	if (len == 0) {
	    return 0;
	}
	if (server_name[len - 1] == '\n') {
	    server_name[len - 1] = 0;
	}
	afs_server = server_name;
    }
    th = gethostbyname(afs_server);
    if (!th) {
	printf("host %s not found; %s call aborted\n", afs_server, syscall);
	return 0;
    }
    memcpy(&hostAddr, th->h_addr, sizeof(hostAddr));
    return hostAddr;
}


/* Does the actual RX connection to the afs server */
struct rx_connection *
rx_connection(afs_int32 * errorcode, char *syscall)
{
    struct rx_connection *conn;
    struct rx_securityClass *null_securityObject;
    afs_int32 host;

    if (!(host = GetAfsServerAddr(syscall))) {
	*errorcode = -1;
	return (struct rx_connection *)0;
    }
    *errorcode = rx_Init(0);
    if (*errorcode) {
	printf("Rx initialize failed \n");
	return (struct rx_connection *)0;
    }
    null_securityObject = rxnull_NewClientSecurityObject();
    conn =
	rx_NewConnection(host, htons(AFSCONF_RMTSYSPORT), RMTSYS_SERVICEID,
			 null_securityObject, 0);
    if (!conn) {
	printf("Unable to make a new connection\n");
	*errorcode = -1;
	return (struct rx_connection *)0;
    }
    return conn;
}


/* WARNING: The calling program (i.e. klog) MUST be suid-root since we need to
 * do a setgroups(2) call with the new pag.... */
#ifdef AFS_DUX40_ENV
#pragma weak setpag = afs_setpag
int
afs_setpag(void)
#else
int
setpag(void)
#endif
{
    struct rx_connection *conn;
    clientcred creds;
    afs_int32 errorcode, errornumber, newpag, ngroups, j, groups[NGROUPS_MAX];

    if (!(conn = rx_connection(&errorcode, "setpag"))) {
	/* Remote call can't be performed for some reason.
	 * Try the local 'setpag' system call ... */
	errorcode = lsetpag();
	return errorcode;
    }
    ngroups = SetClientCreds(&creds, groups);
    errorcode = RMTSYS_SetPag(conn, &creds, &newpag, &errornumber);
    if (errornumber) {
	errno = errornumber;
	errorcode = -1;
	printf("Warning: Remote setpag to %s has failed (err=%d)...\n",
	       afs_server, errno);
    }
    if (errorcode) {
	return errorcode;
    }
    if (afs_get_pag_from_groups(groups[0], groups[1]) == NOPAG) {
	/* we will have to shift grouplist to make room for pag */
	if (ngroups + 2 > NGROUPS_MAX) {
	    /* this is what the real setpag returns */
	    errno = E2BIG;
	    return -1;
	}
	for (j = ngroups - 1; j >= 0; j--) {
	    groups[j + 2] = groups[j];
	}
	ngroups += 2;
    }
    afs_get_groups_from_pag(newpag, &groups[0], &groups[1]);
    if (setgroups(ngroups, groups) == -1) {
	return -1;
    }
#ifdef	AFS_HPUX_ENV
    errorcode = setuid(getuid());
#else
    errorcode = setreuid(-1, getuid());
#endif /* AFS_HPUX_ENV */
    return errorcode;
}


/* Remote pioctl(2) client routine */
#ifdef AFS_DUX40_ENV
#pragma weak pioctl = afs_pioctl
int
afs_pioctl(char *path, afs_int32 cmd, struct ViceIoctl *data,
	   afs_int32 follow)
#else
int
pioctl(char *path, afs_int32 cmd, struct ViceIoctl *data, afs_int32 follow)
#endif
{
    struct rx_connection *conn;
    clientcred creds;
    afs_int32 errorcode, errornumber, ins = data->in_size;
    afs_uint32 groups[NGROUPS_MAX];
    rmtbulk InData, OutData;
    char pathname[256], *pathp = pathname, *inbuffer;
#if 0/*ndef HAVE_GETCWD*/	/* XXX enable when autoconf happens */
    extern char *getwd();
#define getcwd(x,y) getwd(x)
#endif
    if (!(conn = rx_connection(&errorcode, "pioctl"))) {
	/* Remote call can't be performed for some reason.
	 * Try the local 'pioctl' system call ... */
	errorcode = lpioctl(path, cmd, data, follow);
	return errorcode;
    }
    (void)SetClientCreds(&creds, groups);
#ifdef	AFS_OSF_ENV
    if (!ins)
	ins = 1;
#endif
    if (!(inbuffer = (char *)malloc(ins)))
	return (-1);		/* helpless here */
    if (data->in_size)
	memcpy(inbuffer, data->in, data->in_size);
    InData.rmtbulk_len = data->in_size;
    InData.rmtbulk_val = inbuffer;
    inparam_conversion(cmd, InData.rmtbulk_val, 0);
    OutData.rmtbulk_len = data->out_size;
    OutData.rmtbulk_val = data->out;
    /* We always need to pass absolute pathnames to the remote pioctl since we
     * lose the current directory value when doing an rpc call. Below we
     * prepend the current absolute path directory, if the name is relative */
    if (path) {
	if (*path != '/') {
	    /* assuming relative path name */
	    if (getcwd(pathname, 256) == NULL) {
		free(inbuffer);
		printf("getwd failed; exiting\n");
		exit(1);
	    }
	    strcpy(pathname + strlen(pathname), "/");
	    strcat(pathname, path);
	} else {
	    strcpy(pathname, path);
	}
    } else {
	/* Special meaning for a "NULL" pathname since xdr_string hates nil
	 * pointers, at least on non-RTS; of course the proper solution would
	 * be to change the interface declartion. */
	strcpy(pathname, NIL_PATHP);
    }
    errorcode =
	RMTSYS_Pioctl(conn, &creds, pathp, cmd, follow, &InData, &OutData,
		      &errornumber);
    if (errornumber) {
	errno = errornumber;
	errorcode = -1;		/* Necessary since errorcode is 0 on
				 * standard remote pioctl errors */
	if (errno != EDOM && errno != EACCES)
	    printf("Warning: Remote pioctl to %s has failed (err=%d)...\n",
		   afs_server, errno);
    }
    if (!errorcode) {
	/* Do the conversions back to the host order; store the results back
	 * on the same buffer */
	outparam_conversion(cmd, OutData.rmtbulk_val, 1);
    }
    free(inbuffer);
    return errorcode;
}


int
afs_get_pag_from_groups(afs_uint32 g0, afs_uint32 g1)
{
    afs_uint32 h, l, result;

    g0 -= 0x3f00;
    g1 -= 0x3f00;
    if (g0 < 0xc000 && g1 < 0xc000) {
	l = ((g0 & 0x3fff) << 14) | (g1 & 0x3fff);
	h = (g0 >> 14);
	h = (g1 >> 14) + h + h + h;
	result = ((h << 28) | l);
	/* Additional testing */
	if (((result >> 24) & 0xff) == 'A')
	    return result;
	else
	    return NOPAG;
    }
    return NOPAG;
}


afs_get_groups_from_pag(afs_uint32 pag, afs_uint32 * g0p, afs_uint32 * g1p)
{
    unsigned short g0, g1;

    pag &= 0x7fffffff;
    g0 = 0x3fff & (pag >> 14);
    g1 = 0x3fff & pag;
    g0 |= ((pag >> 28) / 3) << 14;
    g1 |= ((pag >> 28) % 3) << 14;
    *g0p = g0 + 0x3f00;
    *g1p = g1 + 0x3f00;
}


static afs_int32
SetClientCreds(struct clientcred *creds, afs_int32 * groups)
{
    afs_int32 ngroups;

    creds->uid = getuid();
    groups[0] = groups[1] = 0;
    ngroups = getgroups(NGROUPS_MAX, groups);
    creds->group0 = groups[0];
    creds->group1 = groups[1];
    return ngroups;
}
