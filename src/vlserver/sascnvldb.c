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
#include <errno.h>
#include <stdio.h>
#include <sys/file.h>
#include "cnvldb.h"		/* CHANGEME! */
#include <netinet/in.h>
#include <afs/venus.h>
#include <afs/cmd.h>

#ifdef notdef
#include <afs/vice.h>
#undef VIRTUE
#undef VICE
#include "afs/prs_fs.h"
#include <afs/afsint.h>
#include <afs/cellconfig.h>
#include <ubik.h>
#endif
#include <strings.h>

#define MAXSIZE 2048		/* most I'll get back from PIOCTL */


static char pn[] = "cnvldb";
static char tempname[] = "XXnewvldb";
static char space[MAXSIZE];
static int MaxServers[2] = { 30, 254 };	/* max server # permitted in this version */

#ifdef notdef			/* postpone this... */
static int
saferead(fd, addr, osize)
     int fd, osize;
     char *addr;
{
    int rc, size;
    char *ptr;

    if (size == EOF)
	return (EOF);

    ptr = addr;
    rc = osize = size;

    while (rc != EOF) {
	rc = read(fd, ptr, size)) {
	    if (rc == size)
		return osize;
	    else {
		if (errno != EINTR) {
		    perror(pn);
		    exit(-1);
		}
	    }
	}
    }


    static int
      saferead(fd, addr, osize)
    int fd, osize;
    char *addr;
    {
	int rc;

	rc = read(fd, addr, osize);
	if (rc != osize && rc != EOF) {
	    perror(pn);
	    exit(-1);
	}
	return (rc);
    }
#else
#define saferead(fd,addr,siz) read((fd),(addr),(siz))
#endif

static char tspace[1024];	/* chdir can't handle anything bigger, anyway */
/* return a static pointer to a buffer */
static char *
Parent(apath)
     char *apath;
{
    register char *tp;
    strcpy(tspace, apath);
    tp = strrchr(tspace, '/');
    if (tp) {
	*tp = 0;
    } else
	strcpy(tspace, ".");
    return tspace;
}

#ifdef notdef

/* this function returns TRUE (1) if the file is in AFS, otherwise false (0) */
static int
InAFS(apath)
     register char *apath;
{
    struct ViceIoctl blob;
    register afs_int32 code;

    blob.in_size = 0;
    blob.out_size = MAXSIZE;
    blob.out = space;

    code = pioctl(apath, VIOC_FILE_CELL_NAME, &blob, 1);
    if (code) {
	if ((errno == EINVAL) || (errno == ENOENT))
	    return 0;
    }
    return 1;
}

QuickPrintStatus(status, name)
     struct VolumeStatus *status;
     char *name;
{
    double QuotaUsed = 0.0;
    double PartUsed = 0.0;
    int WARN = 0;

    if (status->MaxQuota != 0) {
	QuotaUsed =
	    ((((double)status->BlocksInUse) / status->MaxQuota) * 100.0);
    } else {
	/* no limit */
    }
    PartUsed =
	(100.0 -
	 ((((double)status->PartBlocksAvail) / status->PartMaxBlocks) *
	  100.0));
}


static int
ListQuotaCmd(register struct cmd_syndesc *as, void *arock)
{
    register afs_int32 code;
    struct ViceIoctl blob;
    register struct cmd_item *ti;
    struct VolumeStatus *status;
    char *name;

    for (ti = as->parms[0].items; ti; ti = ti->next) {
	/* once per file */
	blob.out_size = MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	code = pioctl(ti->data, VIOCGETVOLSTAT, &blob, 1);
	if (code) {
	    Die(code, ti->data);
	    continue;
	}
	status = (struct VolumeStatus *)space;
	name = (char *)status + sizeof(*status);
	QuickPrintStatus(status, name);
    }
    return 0;
}
#endif /* notdef */

int gc = 1, fromvers = 1, tovers = 2;
char *pathname = NULL, *defaultpath = "/usr/afs/db/vl.DB0";

usage()
{
    fprintf(stderr, "usage: %s ", pn);
    fprintf(stderr, "[-name <pathname>] [-help]\n");
}

getargs(argc, argv)
     int argc;
     char **argv;
{
    int pos, i;
    pos = 0;

    for (i = 1; i < argc; i++) {
	if (!argv[i])
	    break;
	else if (*(argv[i]) != '-') {	/* positional params */
	    if (!pathname)
		pathname = argv[i];
	    else {
		fprintf(stderr, "%s: Too many parameters!\n");
		usage();
		exit(-1);
	    }
	} else			/* keyword params */
	    switch (argv[i][1]) {

	    case 't':		/* -to */
		fprintf(stderr, "%s: can't specify version with this tool!\n",
			pn);
		exit(-1);
		break;

	    case 'f':		/* -from */
		fprintf(stderr, "%s: can't specify version with this tool!\n",
			pn);
		break;

	    case 'n':		/* -name */
		if (pathname) {
		    fprintf(stderr,
			    "%s: -name specified (or implied) twice!\n", pn);
		    exit(-1);
		}
		pathname = argv[++i];
		break;

	    case 'h':		/* -help */
		usage();
		exit(0);
		break;

	    case 'g':		/* -gc == No GC */
		gc = 0;
		break;

	    default:
		usage();
		exit(EINVAL);
	    }
    }

    if (!pathname)
	pathname = defaultpath;
}

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char **argv;
{
    register afs_int32 code;
    int old, new, rc;
    short uvers;
    char ubik[80];		/* space for some ubik header */

    union {
	struct vlheader_1 header1;
	struct vlheader_2 header2;
    } oldheader, newheader;	/* large enough for either */

    union {
	struct vlentry_1 entry1;
	struct vlentry_2 entry2;
    } vlentry;


    getargs(argc, argv);

    /* should stat() the old vldb, get its size, and see if there's */
    /* room for another.  It might be in AFS, so check the quota, too */
    if (!(old = open(pathname, O_RDONLY))) {
	perror(pn);
	exit(-1);
    }

    if (chdir(Parent(pathname))) {
	perror(pn);
	exit(-1);
    }

    if (!(new = open(tempname, O_WRONLY | O_CREAT | O_TRUNC, 0600))) {
	perror(pn);
	exit(-1);
    }

    if (fromvers == 0) {	/* not set */
	lseek(old, 64, L_SET);
	read(old, &fromvers, sizeof(int));
	fromvers = ntohl(fromvers);
	lseek(old, 0, L_SET);	/* go back to beginning */
    }

    /* skip the UBIK data */
    read(old, ubik, 64);
#ifdef notdef
    uvers = ntohs(uvers);
    uvers += 10;		/* hey, if you screw with the VLDB, you lose */
    uvers = htons(uvers);
#endif
    write(new, ubik, 64);

    readheader(old, fromvers, &oldheader);
    convert_header(new, fromvers, tovers, &oldheader, &newheader);
    while ((rc = read(old, &vlentry, sizeof(struct vlentry_1))) && rc != EOF) {
	convert_vlentry(new, fromvers, tovers, &oldheader, &newheader,
			&vlentry);
    }


    if (gc)
	rewrite_header(new, tovers, &newheader);

    close(old);
    if (fsync(new)) {
	perror(pn);
	exit(-1);
    }

    close(new);
    rename(tempname, pathname);
    exit(0);
}

readheader(fd, version, addr)
     int fd;
     int version;
     char *addr;
{
    if (version == 1) {
	read(fd, addr, sizeof(struct vlheader_2));	/* it's not a bug, it's SAS */
    } else if (version == 2) {
	read(fd, addr, sizeof(struct vlheader_2));
    } else
	return EINVAL;

    return 0;
}

/* SAS special */
convert_header(fd, fromv, tov, fromaddr, toaddr)
     int fd, fromv, tov;
     char *fromaddr, *toaddr;
{
    struct vlheader_2 *tvp1;
    struct vlheader_2 *tvp2;
    int i, j, diff;

    memcpy(toaddr, fromaddr, sizeof(struct vlheader_2));
    tvp2 = (struct vlheader_2 *)toaddr;
    tvp2->vital_header.vldbversion = htonl(2);

    write(fd, tvp2, sizeof(struct vlheader_2));

    /* for garbage-collecting... */
    if (gc)
	for (i = 0; i < 254; i++)
	    tvp2->IpMappedAddr[i] = 0;

    return 0;
}

static int
convert_vlentry(new, fromvers, tovers, oldheader, newheader, vlentry)
     int new, fromvers, tovers;
     struct vlheader_1 *oldheader, *newheader;	/* close enough */
     struct vlentry_1 *vlentry;	/* 1 and 2 are identical */
{
    int diff, i, s;

#ifndef DEBUG
    if (fromvers != tovers) {	/* only supports 1 and 2 currently */
#endif

	diff =
	    (tovers ==
	     1 ? sizeof(struct vlheader_1) : sizeof(struct vlheader_2))
	    - (fromvers ==
	       1 ? sizeof(struct vlheader_1) : sizeof(struct vlheader_2));

	for (i = 0; i < 3; i++)
	    vlentry->nextIdHash[i] =
		htonl(ntohl(vlentry->nextIdHash[i]) + diff);

	vlentry->nextNameHash = htonl(ntohl(vlentry->nextNameHash) + diff);

#ifndef DEBUG
    } else {
	;			/* no change, we're just in it for the GC */
    }
#endif

    for (i = 0; i < 8; i++) {
	s = vlentry->serverNumber[i];
	if (s != 255) {
	    if (s > 254) {
		fprintf(stderr,
			"%s: Too Many Servers (%d) for this version!\n", pn,
			s + 1);
		exit(-1);
	    } else {
		newheader->IpMappedAddr[s] = oldheader->IpMappedAddr[s];
	    }
	}
    }
    write(new, vlentry, sizeof(struct vlentry_2));

    return;
}

static int
rewrite_header(new, tovers, newheader)
     int new, tovers;
     char *newheader;
{
    int pos;

    pos = lseek(new, 64, L_SET);	/* leave room for ubik */

    if (pos == -1) {
	perror(pn);
	fprintf(stderr, "%s: no garbage collection\n", pn);
	return;
    } else if (pos != 64) {
	fprintf(stderr, "%s: Can't rewind: no garbage collection\n", pn);
	return;
    }

    if (tovers = 1) {
	write(new, newheader, sizeof(struct vlheader_1));
    } else {
	write(new, newheader, sizeof(struct vlheader_2));
    }

    return;
}
