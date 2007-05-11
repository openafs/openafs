/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2007 Sine Nomine Associates
 */

/*
 * ALL RIGHTS RESERVED
 */

#include <osi/osi.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <afs/stds.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <netdb.h>
#include <errno.h>
#include <sys/ioctl.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <afs/vice.h>
#include <afs/cmd.h>
#include <afs/auth.h>
#include <afs/afsutil.h>


extern struct cmd_syndesc *cmd_CreateSyntax();

/*
Modifications:

29 Oct 1992 Patch GetTokens to print something reasonable when there are no tokens.

*/

/* this is a structure used to communicate with the afs cache mgr, but is
 * otherwise irrelevant, or worse.
 */
struct ClearToken {
    afs_int32 AuthHandle;
    char HandShakeKey[8];
    afs_int32 ViceId;
    afs_int32 BeginTimestamp;
    afs_int32 EndTimestamp;
};


static
SetSysname(ahost, auid, sysname)
     afs_int32 ahost;
     afs_int32 auid;
     char *sysname;
{
    afs_int32 code;
    afs_int32 pheader[6];
    char space[1200], *tp;
    struct ViceIoctl blob;
    afs_int32 setp = 1;

    /* otherwise we've got the token, now prepare to build the pioctl args */
    pheader[0] = ahost;
    pheader[1] = auid;
    pheader[2] = 0;		/* group low */
    pheader[3] = 0;		/* group high */
    pheader[4] = 38 /*VIOC_AFS_SYSNAME */ ;	/* sysname pioctl index */
    pheader[5] = 1;		/* NFS protocol exporter # */

    /* copy stuff in */
    memcpy(space, pheader, sizeof(pheader));
    tp = space + sizeof(pheader);

    /* finally setup the pioctl call's parameters */
    blob.in_size = sizeof(pheader);
    blob.in = space;
    blob.out_size = 0;
    blob.out = NULL;
    memcpy(tp, &setp, sizeof(afs_int32));
    tp += sizeof(afs_int32);
    strcpy(tp, sysname);
    blob.in_size += sizeof(afs_int32) + strlen(sysname) + 1;
    tp += strlen(sysname);
    *(tp++) = '\0';
    code = pioctl(NULL, _VICEIOCTL(99), &blob, 0);
    if (code) {
	code = errno;
    }
    return code;
}


static
GetTokens(ahost, auid)
     afs_int32 ahost;
     afs_int32 auid;
{
    struct ViceIoctl iob;
    afs_int32 pheader[6];
    char tbuffer[1024];
    register afs_int32 code;
    int index, newIndex;
    char *stp;			/* secret token ptr */
    struct ClearToken ct;
    register char *tp;
    afs_int32 temp, gotit = 0;
    int maxLen;			/* biggest ticket we can copy */
    int tktLen;			/* server ticket length */
    time_t tokenExpireTime;
    char UserName[16];
    struct ktc_token token;
    struct ktc_principal clientName;
    time_t current_time;
    char *expireString;

    current_time = time(0);

    /* otherwise we've got the token, now prepare to build the pioctl args */
    pheader[0] = ahost;
    pheader[1] = auid;
    pheader[2] = 0;		/* group low */
    pheader[3] = 0;		/* group high */
    pheader[4] = 8;		/* gettoken pioctl index */
    pheader[5] = 1;		/* NFS protocol exporter # */

    for (index = 0; index < 200; index++) {	/* sanity check in case pioctl fails */
	code = ktc_ListTokens(index, &newIndex, &clientName);
	if (code) {
	    if (code == KTC_NOENT) {
		/* all done */
		if (!gotit)
		    printf("knfs: there are no tokens here.\n");
		code = 0;
	    }
	    break;		/* done, but failed */
	}
	if (strcmp(clientName.name, "afs") != 0)
	    continue;		/* wrong ticket service */

	/* copy stuff in */
	memcpy(tbuffer, pheader, sizeof(pheader));
	tp = tbuffer + sizeof(pheader);
	memcpy(tp, &index, sizeof(afs_int32));
	tp += sizeof(afs_int32);
	iob.in = tbuffer;
	iob.in_size = sizeof(afs_int32) + sizeof(pheader);
	iob.out = tbuffer;
	iob.out_size = sizeof(tbuffer);
	code = pioctl(NULL, _VICEIOCTL(99), &iob, 0);
	if (code < 0 && errno == EDOM)
	    return KTC_NOENT;
	else if (code == 0) {
	    /* check to see if this is the right cell/realm */
	    tp = tbuffer;
	    memcpy(&temp, tp, sizeof(afs_int32));	/* get size of secret token */
	    tktLen = temp;	/* remember size of ticket */
	    tp += sizeof(afs_int32);
	    stp = tp;		/* remember where ticket is, for later */
	    tp += temp;		/* skip ticket for now */
	    memcpy(&temp, tp, sizeof(afs_int32));	/* get size of clear token */
	    if (temp != sizeof(struct ClearToken))
		return KTC_ERROR;
	    tp += sizeof(afs_int32);	/* skip length */
	    memcpy(&ct, tp, temp);	/* copy token for later use */
	    tp += temp;		/* skip clear token itself */
	    tp += sizeof(afs_int32);	/* skip primary flag */
	    /* tp now points to the cell name */
	    if (strcmp(tp, clientName.cell) == 0) {
		/* closing in now, we've got the right cell */
		gotit = 1;
		maxLen =
		    sizeof(token) - sizeof(struct ktc_token) +
		    MAXKTCTICKETLEN;
		if (maxLen < tktLen)
		    return KTC_TOOBIG;
		memcpy(token.ticket, stp, tktLen);
		token.startTime = ct.BeginTimestamp;
		token.endTime = ct.EndTimestamp;
		if (ct.AuthHandle == -1)
		    ct.AuthHandle = 999;
		token.kvno = ct.AuthHandle;
		memcpy(&token.sessionKey, ct.HandShakeKey,
		       sizeof(struct ktc_encryptionKey));
		token.ticketLen = tktLen;
		if ((token.kvno == 999) ||	/* old style bcrypt ticket */
		    (ct.BeginTimestamp &&	/* new w/ prserver lookup */
		     (((ct.EndTimestamp - ct.BeginTimestamp) & 1) == 1))) {
		    sprintf(clientName.name, "AFS ID %d", ct.ViceId);
		    clientName.instance[0] = 0;
		} else {
		    sprintf(clientName.name, "Unix UID %d", ct.ViceId);
		    clientName.instance[0] = 0;
		}
		strcpy(clientName.cell, tp);

		tokenExpireTime = token.endTime;
		strcpy(UserName, clientName.name);
		if (clientName.instance[0] != 0) {
		    strcat(UserName, ".");
		    strcat(UserName, clientName.instance);
		}
		if (UserName[0] == 0)
		    printf("Tokens");
		else if (strncmp(UserName, "AFS ID", 6) == 0) {
		    printf("User's (%s) tokens", UserName);
		} else if (strncmp(UserName, "Unix UID", 8) == 0) {
		    printf("Tokens");
		} else
		    printf("User %s's tokens", UserName);
		printf(" for %s%s%s@%s ", clientName.name,
		       clientName.instance[0] ? "." : "", clientName.instance,
		       clientName.cell);
		if (tokenExpireTime <= current_time)
		    printf("[>> Expired <<]\n");
		else {
		    expireString = ctime(&tokenExpireTime);
		    expireString += 4;	/*Move past the day of week */
		    expireString[12] = '\0';
		    printf("[Expires %s]\n", expireString);
		}

	    }
	}
    }
    return code;
}


static
NFSUnlog(ahost, auid)
     afs_int32 ahost;
     afs_int32 auid;
{
    afs_int32 code;
    afs_int32 pheader[6];
    char space[1200];
    struct ViceIoctl blob;

    /* otherwise we've got the token, now prepare to build the pioctl args */
    pheader[0] = ahost;
    pheader[1] = auid;
    pheader[2] = 0;		/* group low */
    pheader[3] = 0;		/* group high */
    pheader[4] = 9;		/* unlog pioctl index */
    pheader[5] = 1;		/* NFS protocol exporter # */

    /* copy stuff in */
    memcpy(space, pheader, sizeof(pheader));

    /* finally setup the pioctl call's parameters */
    blob.in_size = sizeof(pheader);
    blob.in = space;
    blob.out_size = 0;
    blob.out = NULL;
    code = pioctl(NULL, _VICEIOCTL(99), &blob, 0);
    if (code) {
	code = errno;
    }
    return code;
}

/* Copy the AFS service token into the kernel for a particular host and user */
static
NFSCopyToken(ahost, auid)
     afs_int32 ahost;
     afs_int32 auid;
{
    struct ktc_principal client, server;
    struct ktc_token theTicket;
    afs_int32 code;
    afs_int32 pheader[6];
    char space[1200];
    struct ClearToken ct;
    afs_int32 index, newIndex;
    afs_int32 temp;		/* for bcopy */
    char *tp;
    struct ViceIoctl blob;

    for (index = 0;; index = newIndex) {
	code = ktc_ListTokens(index, &newIndex, &server);
	if (code) {
	    if (code == KTC_NOENT) {
		/* all done */
		code = 0;
	    }
	    break;		/* done, but failed */
	}
	if (strcmp(server.name, "afs") != 0)
	    continue;		/* wrong ticket service */
	code = ktc_GetToken(&server, &theTicket, sizeof(theTicket), &client);
	if (code)
	    return code;

	/* otherwise we've got the token, now prepare to build the pioctl args */
	pheader[0] = ahost;
	pheader[1] = auid;
	pheader[2] = 0;		/* group low */
	pheader[3] = 0;		/* group high */
	pheader[4] = 3;		/* set token pioctl index */
	pheader[5] = 1;		/* NFS protocol exporter # */

	/* copy in the header */
	memcpy(space, pheader, sizeof(pheader));
	tp = space + sizeof(pheader);
	/* copy in the size of the encrypted part */
	memcpy(tp, &theTicket.ticketLen, sizeof(afs_int32));
	tp += sizeof(afs_int32);
	/* copy in the ticket itself */
	memcpy(tp, theTicket.ticket, theTicket.ticketLen);
	tp += theTicket.ticketLen;
	/* copy in "clear token"'s size */
	temp = sizeof(struct ClearToken);
	memcpy(tp, &temp, sizeof(afs_int32));
	tp += sizeof(afs_int32);
	/* create the clear token and copy *it* in */
	ct.AuthHandle = theTicket.kvno;	/* where we hide the key version # */
	memcpy(ct.HandShakeKey, &theTicket.sessionKey,
	       sizeof(ct.HandShakeKey));

	ct.ViceId = auid;
	ct.BeginTimestamp = theTicket.startTime;
	ct.EndTimestamp = theTicket.endTime;
	memcpy(tp, &ct, sizeof(ct));
	tp += sizeof(ct);
	/* copy in obsolete primary flag */
	temp = 0;
	memcpy(tp, &temp, sizeof(afs_int32));
	tp += sizeof(afs_int32);
	/* copy in cell name, null terminated */
	strcpy(tp, server.cell);
	tp += strlen(server.cell) + 1;

	/* finally setup the pioctl call's parameters */
	blob.in_size = tp - space;
	blob.in = space;
	blob.out_size = 0;
	blob.out = NULL;
	code = pioctl(NULL, _VICEIOCTL(99), &blob, 0);
	if (code) {
	    code = errno;
	    break;
	}
    }
    return code;
}

static
cmdproc(as, arock)
     register struct cmd_syndesc *as;
     afs_int32 arock;
{
    register struct hostent *the;
    char *tp, *sysname = 0;
    afs_int32 uid, addr;
    register afs_int32 code;

    the = (struct hostent *)
	hostutil_GetHostByName(tp = as->parms[0].items->data);
    if (!the) {
	printf("knfs: unknown host '%s'.\n", tp);
	return -1;
    }
    memcpy(&addr, the->h_addr, sizeof(afs_int32));
    uid = -1;
    if (as->parms[1].items) {
	code = util_GetInt32(tp = as->parms[1].items->data, &uid);
	if (code) {
	    printf("knfs: can't parse '%s' as a number (UID)\n", tp);
	    return code;
	}
    } else
	uid = -1;		/* means wildcard: match any user on this host */

    /*
     * If not "-id" is passed then we use the getuid() id, unless it's root
     * that is doing it in which case we only authenticate as "system:anyuser"
     * as it's appropriate for root. (The cm handles conversions from 0 to
     * "afs_nobody"!)
     */
    if (uid == -1) {
	uid = getuid();
    }

    if (as->parms[2].items) {
	sysname = as->parms[2].items->data;
    }

    if (as->parms[4].items) {
	/* tokens specified */
	code = GetTokens(addr, uid);
	if (code) {
	    if (code == ENOEXEC)
		printf
		    ("knfs: Translator in 'passwd sync' mode; remote uid must be the same as local uid\n");
	    else
		printf("knfs: failed to get tokens for uid %d (code %d)\n",
		       uid, code);
	}
	return code;
    }

    /* finally, parsing is done, make the call */
    if (as->parms[3].items) {
	/* unlog specified */
	code = NFSUnlog(addr, uid);
	if (code) {
	    if (code == ENOEXEC)
		printf
		    ("knfs: Translator in 'passwd sync' mode; remote uid must be the same as local uid\n");
	    else
		printf("knfs: failed to unlog (code %d)\n", code);
	}
    } else {
	code = NFSCopyToken(addr, uid);
	if (code) {
	    if (code == ENOEXEC)
		printf
		    ("knfs: Translator in 'passwd sync' mode; remote uid must be the same as local uid\n");
	    else
		printf("knfs: failed to copy tokens (code %d)\n", code);
	}
	if (sysname) {
	    code = SetSysname(addr, uid, sysname);
	    if (code) {
		printf("knfs: failed to set client's @sys to %s (code %d)\n",
		       sysname, code);
	    }
	}
    }
    return code;
}

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char **argv;
{
    register struct cmd_syndesc *ts;
    register afs_int32 code;

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

    osi_AssertOK(osi_PkgInit(osi_ProgramType_EphemeralUtility,
			     osi_NULL));

    ts = cmd_CreateSyntax(NULL, cmdproc, 0, "copy tickets for NFS");
    cmd_AddParm(ts, "-host", CMD_SINGLE, CMD_REQUIRED, "host name");
    cmd_AddParm(ts, "-id", CMD_SINGLE, CMD_OPTIONAL, "user ID (decimal)");
    cmd_AddParm(ts, "-sysname", CMD_SINGLE, CMD_OPTIONAL,
		"host's '@sys' value");
    cmd_AddParm(ts, "-unlog", CMD_FLAG, CMD_OPTIONAL, "unlog remote user");
    cmd_AddParm(ts, "-tokens", CMD_FLAG, CMD_OPTIONAL,
		"display all tokens for remote [host,id]");

    code = cmd_Dispatch(argc, argv);

    osi_AssertOK(osi_PkgShutdown());

    return code;
}
