/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * ALL RIGHTS RESERVED
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <sys/types.h>
#include <afs/cmd.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <afs/afsutil.h>
#include <afs/budb.h>
#include <afs/bubasics.h>
#include <afs/volser.h>
#include "bc.h"

/* protos */

static char * TapeName(char *);
static char * DumpName(afs_int32 adumpID);
static FILE * OpenDump(afs_int32 , char * );
FILE * OpenTape(char * , char * );
static afs_int32 ScanForChildren(afs_int32 );
static afs_int32 DeleteDump(afs_int32 );
char * tailCompPtr(char *);
afs_int32 ScanDumpHdr(FILE *, char *, char *, afs_int32 *, afs_int32 *,
  afs_int32 *, afs_int32 *);
afs_int32 ScanTapeVolume(FILE *, char *, afs_int32 *, char *, afs_int32 *, afs_int32 *,
   afs_int32 *, afs_int32 *);
afs_int32 ScanVolClone(FILE *, char *, afs_int32 *);


/* basic format of a tape file is a file, whose name is "T<tapename>.db", and
 * which contains the fields
 * (afs_int32) dumpID, (afs_int32) tape-sequence-within-dump, (afs_int32) damage_flag
 * all as space-separated integers.
 */

/* The format of a dump file is:
 * a file whose name is "D<dump#>.db"
 * and whose contents are a header line:
 * (string) dumpName, (long) parent-id, (long) incTime, (long) dumpEndTime, (long) level
 * and a bunch of bcdb_volumeEntries with this format:
 * (string) volume name, (long) volume ID, (string) tape name, (long) position-on-tape,
 *     (long) sequence-in-volume-dump, (long) is-this-the-last-vol-frag, (long) incTime
 * again, all space-separated.
 * Note that dumpEndTime is stored and returned in the dump creation time field.
 */

/* return the tape file name corresponding to a particular tape */

static char * TapeName(char *atapeName)
{
    static char tbuffer[AFSDIR_PATH_MAX];

    /* construct the backup dir path */
    strcpy(tbuffer, AFSDIR_SERVER_BACKUP_DIRPATH);
    strcat(tbuffer, "/T");
    strcat(tbuffer + 1, atapeName);
    strcat(tbuffer, ".db");
    return tbuffer;
}

/* return the dump file name corresponding to a particular dump ID */

static char * DumpName(afs_int32 adumpID)
{
    static char tbuffer[AFSDIR_PATH_MAX];
    char buf[AFSDIR_PATH_MAX];

    /* construct the backup dir path */
    strcpy(buf, AFSDIR_SERVER_BACKUP_DIRPATH);
    strcat(buf, "/D%d.db");
    sprintf(tbuffer, buf, adumpID);
    return tbuffer;
}

static FILE * OpenDump(afs_int32 adumpID, char * awrite)
{
    char *tp;
    FILE *tfile;

    tp = DumpName(adumpID);
    tfile = fopen(tp, awrite);
    return tfile;
}

/* OpenTape
 * notes:
 * 	non-static for recoverDB
 */

FILE * OpenTape(char * atapeName, char * awrite)
{
    char *tp;
    FILE *tfile;
    tp = TapeName(atapeName);
    tfile = fopen(tp, awrite);
    return tfile;
}

/* scan for, and delete, all dumps whose parent dump ID is aparentID */

static afs_int32 ScanForChildren(afs_int32 aparentID)
{
    DIR *tdir;
    struct dirent *tde;
    afs_int32 dumpID, parent;
    FILE *tfile;
    afs_int32 code;
    afs_int32 j2, j3, j4;
    char dname[256];
    char dumpName[1024];

    tdir = opendir(AFSDIR_SERVER_BACKUP_DIRPATH);
    if (!tdir)
	return -1;

    for (tde = readdir(tdir); tde; tde = readdir(tdir)) {
	code = sscanf(tde->d_name, "D%ld.db", (long int *) &dumpID);
	if (code != 1)
	    continue;

	tfile = OpenDump(dumpID, "r");
	if (!tfile)
	    continue;		/* shouldn't happen, but should continue anyway */

	code = ScanDumpHdr(tfile, dname, dumpName, &parent, &j2, &j3, &j4);
	fclose(tfile);
	if (code) {
	    printf("backup:dsstub: bad dump header for dump %d\n", dumpID);
	    continue;
	}

	/* if this guy's parent is the ID we're scanning for, delete it */
	if (aparentID == parent) {
	    code = DeleteDump(dumpID);
	    if (code)
		printf("backup:dsstub: failed to delete child dump %d\n",
		       dumpID);
	}
    }
    closedir(tdir);
    return 0;
}

static afs_int32 DeleteDump(afs_int32 adumpID)
{
    char *tp;
    afs_int32 code;
    tp = DumpName(adumpID);
    code = unlink(tp);
    if (code)
	return code;
    code = ScanForChildren(adumpID);
    return code;
}

#if 0
static afs_int32 DeleteTape(char * atapeName)
{
    char *tp;
    afs_int32 code;
    tp = TapeName(atapeName);
    code = unlink(tp);
    return code;
}
#endif

/* tailCompPtr
 *	name is a pathname style name, determine trailing name and return
 *	pointer to it
 */

char *
tailCompPtr(char *pathNamePtr)
{
    char *ptr;
    ptr = strrchr(pathNamePtr, '/');
    if (ptr == 0) {
	/* this should never happen */
	printf("tailCompPtr: could not find / in name(%s)\n", pathNamePtr);
	return (pathNamePtr);
    } else
	ptr++;			/* skip the / */
    return (ptr);
}

/* ScanDumpHdr
 *	scan a dump header out of a dump file, leaving the file ptr set after
 *	the header.
 * entry:
 *	afile - ptr to file, for reading.
 *	various - ptrs for return values
 * exit:
 *	aname - string of form volume_set.dump_level
 *	dumpName - pathname of dump schedule node
 *	aparent - id of parent
 *	aincTime
 *	acreateTime - time at which dump was created
 *	alevel - level of dump (0 = full, 1+ are incrementals)
 */
afs_int32
ScanDumpHdr(FILE *afile, char *aname, char *dumpName, afs_int32 *aparent, afs_int32 *aincTime, afs_int32 *acreateTime, afs_int32 *alevel)
{
    char tbuffer[256];
    char *tp;
    afs_int32 dbmagic, dbversion;
    afs_int32 code;

    tp = fgets(tbuffer, sizeof(tbuffer), afile);
    if (!tp)
	return -1;
    code =
	sscanf(tbuffer, "%d %d %s %s %ld %ld %ld %ld", &dbmagic, &dbversion,
	       aname, dumpName, (long int *) aparent, (long int *) aincTime,
	       (long int *) acreateTime, (long int *) alevel);
    if (code != 8)
	return -1;

    /* now check the magic and version numbers */
    if ((dbmagic != BC_DUMPDB_MAGIC) || (dbversion != BC_DUMPDB_VERSION))
	return (-1);

    return 0;
}

#if 0
/* scan a tape header out of a tape file, leaving the file ptr positioned just past the header */
static afs_int32 ScanTapeHdr(FILE *afile, afs_int32 *adumpID, afs_int32 *aseq, afs_int32 *adamage)
{
    char tbuffer[256];
    char *tp;
    afs_int32 code;

    tp = fgets(tbuffer, sizeof(tbuffer), afile);
    if (!tp)
	return -1;
    code = sscanf(tbuffer, "%ld %ld %ld", (long int *)adumpID,
		  (long int *)aseq, (long int *)adamage);
    if (code != 3)
	return -1;
    return 0;
}
#endif

/* ScanTapeVolume
 *	scan a tape volume record from a dump file, leaving the file ptr
 *	positioned past the just-scanned record.
 * exit:
 *	0 - success
 *	1 - EOF
 *	-1 for error
 */

afs_int32 ScanTapeVolume(FILE *afile, char *avolName, afs_int32 *avolID, char *atapeName, afs_int32 *apos, afs_int32 *aseq, afs_int32 *alastp, afs_int32 *cloneTime)
{
    char tbuffer[256];
    afs_int32 code;
    char *tp;

    tp = fgets(tbuffer, sizeof(tbuffer), afile);
    if (!tp) {			/* something went wrong, or eof hit */
	if (ferror(afile))
	    return -1;		/* error occurred */
	else
	    return 1;		/* eof */
    }
    code =
	sscanf(tbuffer, "%s %ld %s %ld %ld %ld %ld", avolName,
	       (long int *) avolID, atapeName, (long int *)apos,
	       (long int *) aseq, (long int *) alastp,
	       (long int *) cloneTime);
    if (code != 7)
	return -1;		/* bad input line */
    return 0;
}

/* ScanVolClone
 *	Search the dump for the volume with name volName, and return it's
 *	clone time.
 * exit:
 *	0 - clonetime set.
 *	-1 - volume with volName not found
 */

afs_int32 ScanVolClone(FILE *tdump, char *volName, afs_int32 *cloneTime)
{
    char avolName[256], atapeName[256];
    afs_int32 retval, avolID, apos, aseq, alastp;

    retval =
	ScanTapeVolume(tdump, &avolName[0], &avolID, &atapeName[0], &apos,
		       &aseq, &alastp, cloneTime);
    while (retval == 0) {
	if (strcmp(avolName, volName) == 0)
	    return (0);
	retval =
	    ScanTapeVolume(tdump, &avolName[0], &avolID, &atapeName[0], &apos,
			   &aseq, &alastp, cloneTime);
    }
    return (-1);
}

#if 0
/* seek a dump file (after a header scan has been done) to position apos */
static int SeekDump(FILE *afile, afs_int32 apos)
{
    afs_int32 i;
    char *tp;
    char tbuffer[256];

    /* now skip to appropriate position */
    for (i = 0; i < apos; i++) {
	tp = fgets(tbuffer, sizeof(tbuffer), afile);
	if (!tp) {
	    fclose(afile);
	    return -1;
	}
    }
    return 0;
}
#endif
