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

/*
 * Routines to log kaserver activity
 *
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <afs/afsutil.h>
#include <string.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <assert.h>
#include <afs/afsutil.h>
#include "kauth.h"
#include "kalog.h"

extern afs_int32 verbose_track;

#ifdef AUTH_DBM_LOG

DBM *kalog_db;

kalog_Init()
{
    OpenLog(AFSDIR_SERVER_KALOGDB_FILEPATH);	/* set up logging */
    SetupLogSignals();
    kalog_db =
	dbm_open(AFSDIR_SERVER_KALOG_FILEPATH, O_WRONLY | O_CREAT,
		 KALOG_DB_MODE);
    if (!kalog_db)
	ViceLog(0, ("Cannot open dbm database - no DB logging possible\n"));
}

/* log a ticket usage */
kalog_log(principal, instance, sprincipal, sinstance, realm, hostaddr, type)
     char *principal, *instance, *sprincipal, *sinstance, *realm;
     int hostaddr, type;
{
    char keybuf[512];		/* not random! 63 . 63 , 63 . 63 max key */
    datum key, data;
    kalog_elt rdata;

    if (!kalog_db)
	return;
    if (*principal)
	strcpy(keybuf, principal);
    if (realm) {
	strcat(keybuf, "@");
	strcat(keybuf, realm);
    }
    if (*instance) {
	strcat(keybuf, ".");
	strcat(keybuf, instance);
    }

    /* unlike the name/instance, the services can come down as NULL */
    if (sprincipal && *sprincipal) {
	strcat(keybuf, ",");
	strcat(keybuf, sprincipal);
	if (sinstance && *sinstance) {
	    strcat(keybuf, ".");
	    strcat(keybuf, sinstance);
	}
    }
    switch (type) {
    case LOG_CRUSER:
	strcat(keybuf, ":cruser");
	break;
    case LOG_CHPASSWD:
	strcat(keybuf, ":chp");
	break;
    case LOG_AUTHENTICATE:
	strcat(keybuf, ":auth");
	break;
    case LOG_AUTHFAILED:
	strcat(keybuf, ":authnot");
	break;
    case LOG_SETFIELDS:
	strcat(keybuf, ":setf");
	break;
    case LOG_DELUSER:
	strcat(keybuf, ":delu");
	break;
    case LOG_UNLOCK:
	strcat(keybuf, ":unlok");
	break;
    case LOG_GETTICKET:
	strcat(keybuf, ":gtck");
	break;
    case LOG_TGTREQUEST:
	strcat(keybuf, ":tgtreq");
	break;
    default:
	break;
    }

    key.dptr = keybuf;
    key.dsize = strlen(keybuf) + 1;	/* store the key in a string w/ null */
    rdata.last_use = time((time_t *) 0);
    rdata.host = hostaddr;
    data.dptr = (char *)&rdata;
    data.dsize = sizeof(kalog_elt);

    dbm_store(kalog_db, key, data, DBM_REPLACE);

    ViceLog(verbose_track, ("%s from %x\n", keybuf, hostaddr));
}


#endif /* AUTH_DBM_LOG */


/* log a ticket usage to the text log */
void
ka_log(char *principal, char *instance, char *sprincipal, char *sinstance,
       char *realm, int hostaddr, int type)
{
    char logbuf[512];		/* not random! 63 . 63 , 63 . 63 max key */

    if (*principal)
	strcpy(logbuf, principal);
    if (realm) {
	strcat(logbuf, "@");
	strcat(logbuf, realm);
    }
    if (*instance) {
	strcat(logbuf, ".");
	strcat(logbuf, instance);
    }

    /* unlike the name/instance, the services can come down as NULL */
    if (sprincipal && *sprincipal) {
	strcat(logbuf, ",");
	strcat(logbuf, sprincipal);
	if (sinstance && *sinstance) {
	    strcat(logbuf, ".");
	    strcat(logbuf, sinstance);
	}
    }
    switch (type) {
    case LOG_CRUSER:
	strcat(logbuf, ":cruser");
	break;
    case LOG_CHPASSWD:
	strcat(logbuf, ":chp");
	break;
    case LOG_AUTHENTICATE:
	strcat(logbuf, ":auth");
	break;
    case LOG_AUTHFAILED:
	strcat(logbuf, ":authnot");
	break;
    case LOG_SETFIELDS:
	strcat(logbuf, ":setf");
	break;
    case LOG_DELUSER:
	strcat(logbuf, ":delu");
	break;
    case LOG_UNLOCK:
	strcat(logbuf, ":unlok");
	break;
    case LOG_GETTICKET:
	strcat(logbuf, ":gtck");
	break;
    case LOG_TGTREQUEST:
        strcat(logbuf, ":tgtreq");
        break;
    default:
	break;
    }

    ViceLog(verbose_track, ("%s from %x\n", logbuf, hostaddr));
}
