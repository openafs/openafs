/* Copyright (C) 1998 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include <afs/param.h>
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

static cmdproc(
  register struct cmd_syndesc *as,
  afs_int32 arock)
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
	    printf("\t%s\n", key.dptr);
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
	    bcopy(data.dptr, &rdata, sizeof(kalog_elt));
	    printf("%s: last operation from host %x at %s", ti->data, rdata.host, 
		   ctime(&rdata.last_use));
	}
    }
    dbm_close(kdb);
}


#include "AFS_component_version_number.c"

int main(
  int argc,
  char **argv)
{
    register struct cmd_syndesc *ts;
    register afs_int32 code;
    char dbmfile_help[AFSDIR_PATH_MAX];

    sprintf(dbmfile_help, "dbmfile to use (default %s)",  AFSDIR_SERVER_KALOGDB_FILEPATH);
    dbmfile = AFSDIR_SERVER_KALOGDB_FILEPATH;
    ts = cmd_CreateSyntax((char *) 0, cmdproc, 0, "Dump contents of dbm database");
    cmd_AddParm(ts, "-dbmfile", CMD_SINGLE, CMD_OPTIONAL, dbmfile_help);
    cmd_AddParm(ts, "-key", CMD_SINGLE, CMD_OPTIONAL, "extract entries that match specified key");
    code = cmd_Dispatch(argc, argv);
    return code;
}
#else

#include "AFS_component_version_number.c"

int main(void)
{
    printf("kdb not supported\n");
}
#endif
