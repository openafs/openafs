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


#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <security/pam_appl.h>
#include <security/pam_modules.h>

#include "afs_message.h"
#include "afs_util.h"

extern int
pam_sm_open_session(pam_handle_t * pamh, int flags, int argc,
		    const char **argv)
{
    return PAM_SUCCESS;
}


#define REMAINLIFETIME 300

extern int
pam_sm_close_session(pam_handle_t * pamh, int flags, int argc,
		     const char **argv)
{
    int i;
    int logmask = LOG_UPTO(LOG_INFO);
    int origmask;
    int remain = 0;
    int remainlifetime = REMAINLIFETIME;
    int no_unlog = 0;

    openlog(pam_afs_ident, LOG_CONS | LOG_PID, LOG_AUTH);
    origmask = setlogmask(logmask);

    /*
     * Parse the user options.  Log an error for any unknown options.
     */
    for (i = 0; i < argc; i++) {
	if (strcasecmp(argv[i], "debug") == 0) {
	    logmask |= LOG_MASK(LOG_DEBUG);
	    (void)setlogmask(logmask);
	} else if (strcasecmp(argv[i], "remain") == 0) {
	    remain = 1;
	} else if (strcasecmp(argv[i], "remainlifetime") == 0) {
	    i++;
	    remain = 1;
	    remainlifetime = (int)strtol(argv[i], (char **)NULL, 10);
	    if (remainlifetime == 0)
		if ((errno == EINVAL) || (errno == ERANGE)) {
		    remainlifetime = REMAINLIFETIME;
		    pam_afs_syslog(LOG_ERR, PAMAFS_REMAINLIFETIME, argv[i],
				   REMAINLIFETIME);
		} else {
		    no_unlog = 0;
		    remain = 0;
		}
	} else if (strcmp(argv[i], "no_unlog") == 0) {
	    no_unlog = 1;
	} else {
	    pam_afs_syslog(LOG_ERR, PAMAFS_UNKNOWNOPT, argv[i]);
	}
    }

    if (logmask && LOG_MASK(LOG_DEBUG))
	syslog(LOG_DEBUG,
	       "pam_afs_session_close: remain: %d, remainlifetime: %d, no_unlog: %d",
	       remain, remainlifetime, no_unlog);
    if (remain && !no_unlog) {
	switch (fork()) {
	case -1:		/* error */
	    return (PAM_SESSION_ERR);
	case 0:		/* child */
#ifdef AFS_LINUX20_ENV
	    setpgrp();
#endif
	    setsid();
	    for (i = 0; i < 64; i++)
		close(i);
	    sleep(remainlifetime);
	    ktc_ForgetAllTokens();
	    pam_afs_syslog(LOG_INFO, PAMAFS_SESSIONCLOSED2);
	    exit(0);
	default:		/* parent */
	    pam_afs_syslog(LOG_INFO, PAMAFS_SESSIONCLOSED1);
	    return (PAM_SUCCESS);
	}
    }
    if (!no_unlog && ktc_ForgetAllTokens())
	return PAM_SESSION_ERR;
    if (logmask && LOG_MASK(LOG_DEBUG))
	syslog(LOG_DEBUG, "pam_afs_session_close: Session closed");
    return PAM_SUCCESS;
}
