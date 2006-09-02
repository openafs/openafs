/*
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/login/Attic/util_logout.c,v 1.5 2003/07/15 23:15:44 shadow Exp $");

#include <sys/types.h>
#include <sys/file.h>
#ifdef	AFS_SUN5_ENV
#include <fcntl.h>
#define L_INCR SEEK_CUR
#endif
#include <sys/time.h>
#include <time.h>
#include <utmp.h>
#include <stdio.h>
#ifdef AIX
#include <fcntl.h>
#include <unistd.h>
#define L_INCR SEEK_CUR
#endif

#define	UTMPFILE	"/etc/utmp"

/* 0 on failure, 1 on success */

logout(line)
     register char *line;
{
    register FILE *fp;
    struct utmp ut;
    int rval;

    if (!(fp = fopen(UTMPFILE, "r+")))
	return (0);
    rval = 0;
    while (fread((char *)&ut, sizeof(struct utmp), 1, fp) == 1) {
	if (!ut.ut_name[0] || strncmp(ut.ut_line, line, sizeof(ut.ut_line)))
	    continue;
	memset(ut.ut_name, 0, sizeof(ut.ut_name));
#if	!defined(AIX) && !defined(AFS_SUN5_ENV)
	memset(ut.ut_host, 0, sizeof(ut.ut_host));
#endif /* AIX */
	(void)time(&ut.ut_time);
	(void)fseek(fp, (long)-sizeof(struct utmp), L_INCR);
	(void)fwrite((char *)&ut, sizeof(struct utmp), 1, fp);
	(void)fseek(fp, (long)0, L_INCR);
	rval = 1;
    }
    (void)fclose(fp);
    return (rval);
}
