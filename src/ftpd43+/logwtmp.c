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
 *
 */

#include <afs/param.h>
#include <sys/types.h>
#include <sys/file.h>
#ifdef	AFS_SUN5_ENV
#include <fcntl.h>
#endif
#include <sys/time.h>
#include <sys/stat.h>
#include <utmp.h>
#include <time.h>
#include <string.h>

#define	WTMPFILE	"/usr/adm/wtmp"
#define	WTMPFILEALT	"/var/adm/wtmp"

static int fd = 0;

#ifdef AFS_LINUX20_ENV
/* Need to conform to declaration in utmp.h */
void logwtmp(const char *line, const char *name, const char *host)
#else
logwtmp(line, name, host)
	char *line, *name, *host;
#endif
{
	struct utmp ut;
	struct stat buf;

	if (!fd && (fd = open(WTMPFILE, O_WRONLY|O_APPEND, 0)) < 0) {
	    if ((fd = open(WTMPFILEALT, O_WRONLY|O_APPEND, 0)) < 0)
		return;
	}

        bzero((char *)&ut, sizeof(ut));
	if (!fstat(fd, &buf)) {
		(void)strncpy(ut.ut_line, line, sizeof(ut.ut_line));
		(void)strncpy(ut.ut_name, name, sizeof(ut.ut_name));
#if !defined(AFS_SUN5_ENV)
		(void)strncpy(ut.ut_host, host, sizeof(ut.ut_host));
#endif /* !defined(AFS_SUN5_ENV) */
#ifdef AFS_AIX32_ENV
		(void)strncpy(ut.ut_id, line, sizeof(ut.ut_id));
		ut.ut_exit.e_termination = 0;
		ut.ut_exit.e_exit = 0;
#endif
#if	!defined(AFS_SUN_ENV) || defined(AFS_SUN5_ENV)
		ut.ut_type = ((name && strlen(name)) ? USER_PROCESS : DEAD_PROCESS);
		ut.ut_pid = getpid();
#endif
		(void)time(&ut.ut_time);
		if (write(fd, (char *)&ut, sizeof(struct utmp)) !=
		    sizeof(struct utmp))
			(void)ftruncate(fd, buf.st_size);
	}
}

