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
    ("$Header: /cvs/openafs/src/login/Attic/util_login.c,v 1.4 2003/07/15 23:15:44 shadow Exp $");

#include <sys/types.h>
#include <sys/file.h>
#include <utmp.h>
#include <stdio.h>
#ifdef AIX
#include <fcntl.h>
#include <unistd.h>
#define L_SET SEEK_SET
#endif

#ifdef	AFS_SUN5_ENV
#include <fcntl.h>
#endif

#define	UTMPFILE	"/etc/utmp"
#ifdef AFS_HPUX_ENV
#define	WTMPFILE	"/etc/wtmp"
#else
#define	WTMPFILE	"/usr/adm/wtmp"
#endif

void
login(ut)
     struct utmp *ut;
{
    register int fd;
    int tty;
    off_t lseek();

    tty = ttyslot();
    if (tty > 0 && (fd = open(UTMPFILE, O_WRONLY, 0)) >= 0) {
	(void)lseek(fd, (long)(tty * sizeof(struct utmp)), L_SET);
	(void)write(fd, (char *)ut, sizeof(struct utmp));
	(void)close(fd);
    }
    if ((fd = open(WTMPFILE, O_WRONLY | O_APPEND, 0)) >= 0) {
	(void)write(fd, (char *)ut, sizeof(struct utmp));
	(void)close(fd);
    }
}
