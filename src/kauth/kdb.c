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

#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#ifndef AFS_NT40_ENV
#include <strings.h>
#endif
#include <afs/cmd.h>
#include "kauth.h"
#include "kalog.h"
#include <afs/afsutil.h>

#ifdef AUTH_DBM_LOG

char *dbmfile;

static int
cmdproc(register struct cmd_syndesc *as, void * arock)
{
    DBM *kdb;
    datum key, data;
    kalog_elt rdata;
    register afs_int32 code = 0, cnt = 0;
    register struct cmd_item *ti;

    if (as->parms[0].items) {
	dbmfile = as->parms[0].items->data;
    }
    kdb = dbm_open(dbmfile, O_RDONLY, KALOG_DB_MODE);
    if (!kdb) {
	perror(dbmfile);
	exit(1);
    }
    if (!(ti = as->parms[1].items)) {
	printf("Printing all entries found in %s\n", dbmfile);
	for (key = dbm_firstkey(kdb); key.dptr;
	     key = afs_dbm_nextkey(kdb, key), cnt++) {
            if (as->parms[2].items) {
		data = dbm_fetch(kdb, key);
		if (!data.dptr) {
		    fprintf(stderr, "%s: no entry exists\n", ti->data);
		    continue;
		}
		if (data.dsize != sizeof(kalog_elt)) {
		    fprintf(stderr, "%s: data came out corrupt\n", ti->data);
		    continue;
		}
		memcpy(&rdata, data.dptr, sizeof(kalog_elt));
		if (! as->parms[3].items) {
		    char *hostName;
		    hostName = hostutil_GetNameByINet(rdata.host);
		    printf("%s: last operation from host %s at %s", key.dptr, 
			   hostName, ctime(&rdata.last_use));
		} else {
		    char *hostIP;
		    hostIP = afs_inet_ntoa(rdata.host);
		    printf("%s: last operation from host %s at %s", key.dptr, 
			   hostIP, ctime(&rdata.last_use));
		}
            } else {
		printf("\t%s\n", key.dptr);
            }
	}
	printf("%d entries were found\n", cnt);
    } else {
	for (; ti; ti = ti->next) {
	    key.dsize = strlen(ti->data) + 1;
	    key.dptr = ti->data;
	    data = dbm_fetch(kdb, key);
	    if (!data.dptr) {
		fprintf(stderr, "%s: no entry exists\n", ti->data);
		continue;
	    }
	    if (data.dsize != sizeof(kalog_elt)) {
		fprintf(stderr, "%s: data came out corrupt\n", ti->data);
		continue;
	    }
	    memcpy(&rdata, data.dptr, sizeof(kalog_elt));
	    printf("%s: last operation from host %x at %s", ti->data,
		   rdata.host, ctime(&rdata.last_use));
	}
    }
    dbm_close(kdb);
    return 0;
}


#include "AFS_component_version_number.c"

int
main(int argc, char **argv)
{
    register struct cmd_syndesc *ts;
    register afs_int32 code;
    char dbmfile_help[AFSDIR_PATH_MAX];

    sprintf(dbmfile_help, "dbmfile to use (default %s)",
	    AFSDIR_SERVER_KALOGDB_FILEPATH);
    dbmfile = AFSDIR_SERVER_KALOGDB_FILEPATH;
    ts = cmd_CreateSyntax(NULL, cmdproc, NULL, "Dump contents of dbm database");
    cmd_AddParm(ts, "-dbmfile", CMD_SINGLE, CMD_OPTIONAL, dbmfile_help);
    cmd_AddParm(ts, "-key", CMD_SINGLE, CMD_OPTIONAL,
		"extract entries that match specified key");
    cmd_AddParm(ts, "-long", CMD_FLAG, CMD_OPTIONAL, "print long info for each entry");
    cmd_AddParm(ts, "-numeric", CMD_FLAG, CMD_OPTIONAL, "addresses only");
    code = cmd_Dispatch(argc, argv);
    return code;
}
#else

#include "AFS_component_version_number.c"

int
main(void)
{
    printf("kdb not supported\n");
    return 1;
}
#endif
