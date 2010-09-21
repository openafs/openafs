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


#ifdef	AFS_AIX32_ENV
#include <signal.h>
#ifdef AFS_AIX51_ENV
#include <sys/cred.h>
#ifdef HAVE_SYS_PAG_H
#include <sys/pag.h>
#endif
#include <errno.h>
#endif
#endif
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#ifndef AFS_NT40_ENV
#include <unistd.h>
#endif
#include <string.h>
#include <pwd.h>
#ifdef AFS_KERBEROS_ENV
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include "rx/rx.h"
#include "sys_prototypes.h"
#include <afs/auth.h>

#include "AFS_component_version_number.c"

int
main(int argc, char *argv[])
{
    struct passwd *pwe;
    int uid, gid;
    char *shell = "/bin/sh";

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
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    gid = getgid();
    uid = getuid();
    pwe = getpwuid(uid);
    if (pwe == 0) {
	fprintf(stderr, "Intruder alert.\n");
    } else {
/*		shell = pwe->pw_shell; */
    }
    if (setpag() == -1) {
	perror("setpag");
    }
#ifdef AFS_KERBEROS_ENV
    ktc_newpag();
#endif
    (void)setuid(uid);
    (void)setgid(gid);
    argv[0] = shell;
    execvp(shell, argv);
    perror(shell);
    fprintf(stderr, "No shell\n");
    exit(1);
}
