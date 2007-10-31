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
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif
#include <afs/budb_client.h>
#include <afs/cmd.h>
#include <afs/com_err.h>
#include <afs/bubasics.h>
#include "bc.h"
#include "error_macros.h"
#include <errno.h>

/* code to manage tape hosts
 * specific to the ubik database implementation
 */

afs_int32 bc_UpdateHosts();
extern struct bc_config *bc_globalConfig;
extern struct udbHandleS udbHandle;
extern char *whoami;

/* ------------------------------------
 * command level routines
 * ------------------------------------
 */


/* bc_AddHostCmd
 *	Add a host to the tape hosts
 */

int
bc_AddHostCmd(struct cmd_syndesc *as, void *arock)
{
    struct cmd_item *ti;
    udbClientTextP ctPtr;
    afs_int32 port = 0;
    register afs_int32 code = 0;

    ctPtr = &bc_globalConfig->configText[TB_TAPEHOSTS];
    code = bc_LockText(ctPtr);
    if (code)
	ERROR(code);

    code = bc_UpdateHosts();
    if (code) {
	afs_com_err(whoami, code, "; Can't retrieve tape hosts");
	return (code);
    }

    /* add tape hosts first */
    ti = as->parms[0].items;
    if (ti) {
	if (as->parms[1].items) {
	    port = getPortOffset(as->parms[1].items->data);
	    if (port < 0)
		ERROR(BC_BADARG);
	}
	printf("Adding host %s offset %u to tape list...", ti->data, port);
	fflush(stdout);
	code = bc_AddTapeHost(bc_globalConfig, ti->data, port);
	if (code) {
	    printf("failed\n");
	    fflush(stdout);
	    if (code == EEXIST)
		afs_com_err(whoami, 0, "Port offset already in tape database");
	    ERROR(code);
	}

	code = bc_SaveHosts();
	if (code) {
	    afs_com_err(whoami, code, "Cannot save tape hosts");
	    afs_com_err(whoami, 0,
		    "Changes are temporary - for this session only");
	    ERROR(code);
	}
    }

    /* done */
    printf("done\n");

  error_exit:
    if (ctPtr->lockHandle != 0)
	bc_UnlockText(ctPtr);
    return (code);
}

int
bc_DeleteHostCmd(struct cmd_syndesc *as, void *arock)
{
    struct cmd_item *ti;
    afs_int32 port = 0;
    udbClientTextP ctPtr;
    register afs_int32 code = 0;

    ctPtr = &bc_globalConfig->configText[TB_TAPEHOSTS];
    code = bc_LockText(ctPtr);
    if (code)
	ERROR(code);

    code = bc_UpdateHosts();
    if (code) {
	afs_com_err(whoami, code, "; Can't retrieve tape hosts");
	return (code);
    }

    /* delete tape hosts first */
    ti = as->parms[0].items;
    if (ti) {
	if (as->parms[1].items) {
	    port = bc_SafeATOI(as->parms[1].items->data);
	    if (port < 0)
		return (BC_BADARG);
	}

	printf("Deleting host %s offset %u to tape list...", ti->data, port);
	fflush(stdout);
	code = bc_DeleteTapeHost(bc_globalConfig, ti->data, port);
	if (code) {
	    if (code == ENOENT)
		printf("failed: no such host entry\n");
	    else
		printf("failed with code %d\n", code);
	    ERROR(code);
	}

	code = bc_SaveHosts();
	if (code) {
	    afs_com_err(whoami, code, "Cannot save tape hosts");
	    afs_com_err(whoami, 0,
		    "Changes are temporary - for this session only");
	    ERROR(code);
	}
    }

    /* done */
    printf("done\n");
    fflush(stdout);

  error_exit:
    if (ctPtr->lockHandle != 0)
	bc_UnlockText(ctPtr);
    return (code);
}


/* bc_ListHostsCmd
 *	list all tape hosts (from internally built tables)
 * parameters:
 *	ignored
 */

int
bc_ListHostsCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    register struct bc_hostEntry *tentry;

    code = bc_UpdateHosts();
    if (code) {
	afs_com_err(whoami, code, "; Can't retrieve tape hosts");
	return (code);
    }

    printf("Tape hosts:\n");
    for (tentry = bc_globalConfig->tapeHosts; tentry; tentry = tentry->next) {
	printf("    Host %s, port offset %u\n", tentry->name,
	       tentry->portOffset);
    }
    fflush(stdout);
    return 0;
}



/* ------------------------------------
 * support routines
 * ------------------------------------
 */

bc_ClearHosts()
{
    register struct bc_hostEntry *tentry, *temp;

    tentry = bc_globalConfig->tapeHosts;
    while (tentry != 0) {
	temp = tentry->next;
	free(tentry->name);
	free(tentry);
	tentry = temp;
    }

    bc_globalConfig->tapeHosts = (struct bc_hostEntry *)0;
    return (0);
}

/* bc_ParseHosts
 *	Open up the volume set configuration file as specified in our argument,
 *	then parse the file to set up our internal representation.
 * exit:
 *	0 on successful processing,
 *	-1 otherwise.
 */

int
bc_ParseHosts()
{
    char tbuffer[256];
    char hostName[256];
    afs_int32 port = 0;
    struct bc_hostEntry *tfirst, *tlast, *the;
    char *tp;
    struct hostent *th;

    udbClientTextP ctPtr;
    FILE *stream;

    /* initialize locally used variables */
    ctPtr = &bc_globalConfig->configText[TB_TAPEHOSTS];
    stream = ctPtr->textStream;

    if (ctPtr->textSize == 0)	/* nothing defined yet */
	return (0);

    if (stream == NULL)
	return (BC_INTERNALERROR);

    rewind(stream);

    /* now read the lines and build the structure list */
    tfirst = tlast = (struct bc_hostEntry *)0;

    while (1) {
	tp = fgets(tbuffer, sizeof(tbuffer), stream);
	if (!tp)
	    break;		/* end of file */

	sscanf(tbuffer, "%s %u", hostName, &port);
	th = gethostbyname(hostName);
	if (th == 0) {
	    afs_com_err(whoami, 0,
		    "can't get host info for %s from nameserver or /etc/hosts.",
		    hostName);
	}
	the = (struct bc_hostEntry *)malloc(sizeof(struct bc_hostEntry));
	if (the == (struct bc_hostEntry *)0)
	    return (BC_NOMEM);
	memset(the, 0, sizeof(struct bc_hostEntry));
	if (tlast) {
	    tlast->next = the;
	    tlast = the;
	} else {
	    tfirst = tlast = the;
	}
	the->next = (struct bc_hostEntry *)0;
	the->name = (char *)malloc(strlen(hostName) + 1);
	strcpy(the->name, hostName);
	the->portOffset = port;
	if (th) {
	    memcpy(&the->addr.sin_addr.s_addr, th->h_addr, 4);
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
	    the->addr.sin_len = sizeof(struct sockaddr_in);
#endif
	    the->addr.sin_family = AF_INET;
	    the->addr.sin_port = 0;
	}
    }

    bc_globalConfig->tapeHosts = tfirst;
    return (0);
}

/* bc_SaveHosts
 *	really two parts
 *	1) save the current host information to disk
 *	2) transmit to ubik server
 */

bc_SaveHosts()
{
    register afs_int32 code = 0;

    udbClientTextP ctPtr;
    register FILE *stream;
    struct bc_hostEntry *hePtr;

    extern struct bc_config *bc_globalConfig;

    ctPtr = &bc_globalConfig->configText[TB_TAPEHOSTS];
    stream = ctPtr->textStream;

    /* must be locked */
    if (ctPtr->lockHandle == 0)
	return (BC_INTERNALERROR);

    /* truncate the file */
    code = ftruncate(fileno(stream), 0);
    if (code)
	ERROR(errno);

    rewind(stream);

    hePtr = bc_globalConfig->tapeHosts;

    while (hePtr != 0) {
	fprintf(stream, "%s %u\n", hePtr->name, hePtr->portOffset);
	hePtr = hePtr->next;
    }

    if (ferror(stream))
	return (BC_INTERNALERROR);

    /* send to server */
    code = bcdb_SaveTextFile(ctPtr);
    if (code)
	ERROR(code);

    /* do this on bcdb_SaveTextFile */
    /* increment local version number */
    ctPtr->textVersion++;

    /* update locally stored file size */
    ctPtr->textSize = filesize(ctPtr->textStream);

  error_exit:
    return (code);
}

afs_int32
bc_UpdateHosts()
{
    struct udbHandleS *uhptr = &udbHandle;
    udbClientTextP ctPtr;
    afs_int32 code;
    int lock = 0;

    /* lock schedules and check validity */
    ctPtr = &bc_globalConfig->configText[TB_TAPEHOSTS];

    code = bc_CheckTextVersion(ctPtr);
    if (code != BC_VERSIONMISMATCH) {
	ERROR(code);		/* Version matches or some other error */
    }

    /* Must update the hosts */
    /* If we are not already locked, then lock it now */
    if (ctPtr->lockHandle == 0) {
	code = bc_LockText(ctPtr);
	if (code)
	    ERROR(code);
	lock = 1;
    }

    if (ctPtr->textVersion != -1) {
	afs_com_err(whoami, 0, "obsolete tapehosts - updating");
	bc_ClearHosts();
    }

    /* open a temp file to store the config text received from buserver *
     * The open file stream is stored in ctPtr->textStream */
    code =
	bc_openTextFile(ctPtr,
			&bc_globalConfig->tmpTextFileNames[TB_TAPEHOSTS][0]);
    if (code)
	ERROR(code);
    /* now get a fresh set of information from the database */
    code = bcdb_GetTextFile(ctPtr);
    if (code)
	ERROR(code);

    /* fetch the version number */
    code =
	ubik_BUDB_GetTextVersion(uhptr->uh_client, 0, ctPtr->textType,
		  &ctPtr->textVersion);
    if (code)
	ERROR(code);

    /* parse the file */
    code = bc_ParseHosts();
    if (code)
	ERROR(code);

  error_exit:
    if (lock && ctPtr->lockHandle)
	bc_UnlockText(ctPtr);
    return (code);
}
