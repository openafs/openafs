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

#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif
#include <errno.h>

#include "bc.h"

struct bc_config *bc_globalConfig;

static
TrimLine(abuffer, aport)
     afs_int32 *aport;
     register char *abuffer;
{
    register int tc;
    char garb[100];

    *aport = 0;
    sscanf(abuffer, "%s %u", garb, aport);
    while (tc = *abuffer) {
	if (tc == ' ') {
	    *abuffer = 0;
	    return 0;
	}
	abuffer++;
    }
    return 0;
}

FILE *
bc_open(aconfig, aname, aext, amode)
     register struct bc_config *aconfig;
     char *amode;
     char *aext;
     char *aname;
{
    register FILE *tf;
    char tpath[256];

    strcpy(tpath, aconfig->path);
    strcat(tpath, "/");
    strcat(tpath, aname);
    if (aext)
	strcat(tpath, aext);
    tf = fopen(tpath, amode);
    return tf;
}

bc_InitConfig(apath)
     char *apath;
{
    register struct bc_config *tb;

    /* initialize global config structure */
    tb = (struct bc_config *)malloc(sizeof(struct bc_config));
    if (!tb)
	return (BC_NOMEM);

    bc_globalConfig = tb;
    memset(tb, 0, sizeof(struct bc_config));
    tb->path = (char *)malloc(strlen(apath) + 1);
    if (!tb->path) {
	free(tb);
	return (BC_NOMEM);
    }

    strcpy(tb->path, apath);

    /* now read the important config files; no file means empty list during system init */

    return 0;
}

static
HostAdd(alist, aname, aport)
     struct bc_hostEntry **alist;
     afs_int32 aport;
     char *aname;
{
    register struct bc_hostEntry **tlast, *tentry;
    struct hostent *th;

    /* check that the host address is real */
    th = gethostbyname(aname);
    if (!th)
	return -1;

    /* check if this guy is already in the list */
    for (tentry = *alist; tentry; tentry = tentry->next)
	if (tentry->portOffset == aport)
	    return EEXIST;

    /* add this guy to the end of the list */
    tlast = alist;
    for (tentry = *tlast; tentry; tlast = &tentry->next, tentry = *tlast);

    /* tlast now points to the next pointer (or head pointer) we should overwrite */
    tentry = (struct bc_hostEntry *)malloc(sizeof(struct bc_hostEntry));
    memset(tentry, 0, sizeof(*tentry));
    tentry->name = (char *)malloc(strlen(aname) + 1);
    strcpy(tentry->name, aname);
    *tlast = tentry;
    tentry->next = (struct bc_hostEntry *)0;
    tentry->addr.sin_family = AF_INET;
    memcpy(&tentry->addr.sin_addr.s_addr, th->h_addr, sizeof(afs_int32));
    tentry->addr.sin_port = 0;
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
    tentry->addr.sin_len = sizeof(struct sockaddr_in);
#endif
    tentry->portOffset = aport;
    return 0;
}

static
HostDelete(alist, aname, aport)
     struct bc_hostEntry **alist;
     afs_int32 aport;
     char *aname;
{
    register struct bc_hostEntry **tlast, *tentry;

    /* find guy to zap */
    tlast = alist;
    for (tentry = *tlast; tentry; tlast = &tentry->next, tentry = *tlast)
	if (strcmp(tentry->name, aname) == 0 && (aport == tentry->portOffset))
	    break;
    if (!tentry)
	return ENOENT;		/* failed to find it */

    /* otherwise delete the entry from the list and free appropriate structures */
    *tlast = tentry->next;
    free(tentry->name);
    free(tentry);
    return 0;
}

bc_AddTapeHost(aconfig, aname, aport)
     struct bc_config *aconfig;
     afs_int32 aport;
     char *aname;
{
    register afs_int32 code;

    code = HostAdd(&aconfig->tapeHosts, aname, aport);

    return code;
}

bc_DeleteTapeHost(aconfig, aname, aport)
     struct bc_config *aconfig;
     afs_int32 aport;
     char *aname;
{
    register afs_int32 code;

    code = HostDelete(&aconfig->tapeHosts, aname, aport);

    return code;
}
