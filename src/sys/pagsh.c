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
    ("$Header: /cvs/openafs/src/sys/pagsh.c,v 1.9.2.2 2006/07/20 23:35:44 shadow Exp $");

#ifdef	AFS_AIX32_ENV
#include <signal.h>
#ifdef AFS_AIX51_ENV
#include <sys/cred.h>
#endif
#endif
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#ifndef AFS_NT40_ENV
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <pwd.h>
#ifdef AFS_KERBEROS_ENV
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "AFS_component_version_number.c"

extern afs_int32 setpag();

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


#ifdef AFS_KERBEROS_ENV
/* stolen from auth/ktc.c */

static afs_uint32
curpag(void)
{
#if defined(AFS_AIX51_ENV)
    afs_int32 pag;

    if (get_pag(PAG_AFS, &pag) < 0 || pag == 0)
        pag = -1;
    return pag;
#else
    afs_uint32 groups[NGROUPS_MAX];
    afs_uint32 g0, g1;
    afs_uint32 h, l, ret;

    if (getgroups(sizeof groups / sizeof groups[0], groups) < 2)
	return 0;

    g0 = groups[0] & 0xffff;
    g1 = groups[1] & 0xffff;
    g0 -= 0x3f00;
    g1 -= 0x3f00;
    if ((g0 < 0xc000) && (g1 < 0xc000)) {
	l = ((g0 & 0x3fff) << 14) | (g1 & 0x3fff);
	h = (g0 >> 14);
	h = (g1 >> 14) + h + h + h;
	ret = ((h << 28) | l);
	/* Additional testing */
	if (((ret >> 24) & 0xff) == 'A')
	    return ret;
	else
	    return -1;
    }
    return -1;
#endif
}

int
ktc_newpag(void)
{
    extern char **environ;

    afs_uint32 pag;
    struct stat sbuf;
    char fname[256], *prefix = "/ticket/";
    int numenv;
    char **newenv, **senv, **denv;

    if (stat("/ticket", &sbuf) == -1) {
	prefix = "/tmp/tkt";
    }

    pag = curpag() & 0xffffffff;
    if (pag == -1) {
	sprintf(fname, "%s%d", prefix, getuid());
    } else {
	sprintf(fname, "%sp%ld", prefix, pag);
    }
/*    ktc_set_tkt_string(fname); */

    for (senv = environ, numenv = 0; *senv; senv++)
	numenv++;
    newenv = (char **)malloc((numenv + 2) * sizeof(char *));

    for (senv = environ, denv = newenv; *senv; *senv++) {
	if (strncmp(*senv, "KRBTKFILE=", 10) != 0)
	    *denv++ = *senv;
    }

    *denv = malloc(10 + strlen(fname) + 1);
    strcpy(*denv, "KRBTKFILE=");
    strcat(*denv, fname);
    *++denv = 0;
    environ = newenv;
}

#endif
