/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * This code is used for application programs who want to transfer a
 * token from the local system to the remote system.
 */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/sgistuff/ta-rauth.c,v 1.1.2.2 2005/08/16 18:00:44 shadow Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <afs/auth.h>
#include <afs/cellconfig.h>

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <strings.h>
#if defined(AIX)
#include <sys/syslog.h>
#else /* defined(AIX) */
#include <syslog.h>
#endif /* defined(AIX) */
#include <errno.h>
#include <pwd.h>
#include <afs/afsutil.h>


#ifndef RAUTH_PORT
#define RAUTH_PORT (601)
#endif

 /* ta_rauth provides a single entry point into the remote */
 /* authentication scheme.  This allows us to simply pass the service */
 /* name; this routine will in turn obtain whatever remote */
 /* authentication information necessary and will negotiate with the */
 /* remote connection.  There are three possible return codes: */
 /*  (0) There is no remote authentication system; continue without */
 /*      any authentication. */
 /*  (1) Remote authentication was negotiated successfully */
 /*  (-1) Remote authentication failed (but did exist) */
 /*  (-2) The call could not complete due to internal failure */
 /*  (-3) The remote connection failed */
 /* Note that raddr is in *network* byte order! */

int ta_debug = 0;

int
ta_rauth(s, svc_name, raddr)
     int s;
     char *svc_name;
     struct in_addr raddr;
{
    char localName[64];
    int code;
    struct afsconf_dir *tdir;
    struct ktc_principal tserver;
    struct ktc_token token;
    struct sockaddr_in name;

    /* extract the token */

    tdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);
    if (!tdir) {
	if (ta_debug) {
	    syslog(LOG_ERR, "ta_rauth: afsconf_Open failed\n");
	}
	return (-2);
    }
    code = afsconf_GetLocalCell(tdir, localName, sizeof(localName));
    if (code) {
	if (ta_debug) {
	    syslog(LOG_ERR, "ta_rauth: afsconf_GetLocalCell failed\n");
	}
	return (-2);
    }
    afsconf_Close(tdir);

    strcpy(tserver.cell, localName);
    strcpy(tserver.name, "afs");

    code = ktc_GetToken(&tserver, &token, sizeof(token), NULL);
    if (code) {
	syslog(LOG_WARNING, "ta_rauth: no tokens available");
	return 0;		/* try port without authentication */
    }

    name.sin_family = AF_INET;
    name.sin_port = htons(RAUTH_PORT);
    name.sin_addr = raddr;
    if (connect(s, (struct sockaddr *)&name, sizeof(name)) == -1) {
	extern int errno;

	if (ta_debug) {
	    syslog(LOG_ERR,
		   "ta_rauth(%s): connect call to (%d:%d) failed=%d\n",
		   svc_name, raddr.s_addr, htons(RAUTH_PORT), errno);
	    perror("socket");
	}
	switch (errno) {
#ifdef AFS_AIX_ENV
	    /* On conn failure aix doesn't return any error! */
	case 0:
#endif
	case ECONNREFUSED:
	    return 0;
	case ETIMEDOUT:
	case ENETUNREACH:
	    return -3;
	default:
	    return -2;
	}
    }

    if (outtoken(s, &token, svc_name, localName) == 0) {
	char result;

	if (read(s, &result, 1) != 1) {
	    syslog(LOG_ERR, "Invalid return from remote authenticator\n");
	    exit(1);
	}
	if (result == '0')	/* remote authentication denied */
	    return -1;
	else			/* remote authentication allowed */
	    return 1;
    }

    return (-2);
}

/*
 * outtoken:
 *
 * This routine writes a token on the specified file handle;
 * The output format for a token is:
 *
 *   Field #    Contents         description
 *    (0)       Service requested char[]
 *    (1)       Version #        unsigned integer (< 2^32)
 *    (2)       startTime        unsigned afs_int32 (< 2^32)
 *    (3)       endTime          unsigned afs_int32 (< 2^32)
 *    (4)       sessionKey       char[8]
 *    (5)       kvno             short (< 2^16)
 *    (6)       ticketLen        unsigned integer (< 2^32)
 *    (7)       ticket           char[MAXKTCTICKETLEN]
 *
 *  All fields are comma separated except (4) and (5) because (4) is fixed
 *  length; since field (7) is variable length, it is presumed to
 *  begin after the ',' and to be ticketLen afs_int32.
 */
outtoken(s, token, svc, localName)
     int s;
     struct ktc_token *token;
     char *svc, *localName;
{
    char buf[1024], *bp;
    int count;

    /* (0) - (3) */
    sprintf(buf, "%s,%d,%s,%ld,%ld,", svc, 2, localName, token->startTime,
	    token->endTime);

    /* (4) sessionKey */
    bp = buf + strlen(buf);
    memcpy(bp, &token->sessionKey, 8);
    bp += 8;

    /* (5) - (6) */
    sprintf(bp, "%u,%u,", token->kvno, token->ticketLen);

    /* (7) ticket */
    bp += strlen(bp);
    memcpy(bp, token->ticket, token->ticketLen);
    bp += token->ticketLen;

    if ((count = write(s, buf, (int)(bp - buf))) == -1) {
	perror("outtoken write");
	exit(1);
    }
    if (ta_debug) {
	fprintf(stderr, "sent buffer %s\n", buf);
    }
    return 0;
}
