/*
 * Copyright (c) 1983 The Regents of the University of California.
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

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)rsh.c	5.7 (Berkeley) 9/20/88";
#endif /* not lint */

#include <afs/param.h>
#include <unistd.h>		/* select() prototype */
#include <sys/types.h>		/* fd_set on older platforms */
#include <sys/time.h>		/* struct timeval, select() prototype */
#ifndef FD_SET
# include <sys/select.h>	/* fd_set on newer platforms */
#endif
#include <sys/socket.h>
#ifdef	AFS_SUN5_ENV
#define BSD_COMP
#endif
#include <sys/ioctl.h>
#include <sys/file.h>

#include <netinet/in.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <pwd.h>
#include <netdb.h>

#ifdef	AFS_AIX32_ENV
/*#ifdef MSG
#include "rsh_msg.h" 
#define MSGSTR(n,s) NLgetamsg(MF_RSH, MS_RSH, n, s) 
#else*/
#define MSGSTR(n,s) s
/*#endif*/

#ifdef KAUTH
#include <afs/auth.h>
#include <afs/cellconfig.h>
#endif /* KAUTH */
#endif

#ifdef AFS_HPUX_ENV
#include <sys/stat.h>
extern char **environ;
char **vp;
#define RSHNAME "remsh"
#else
#define RSHNAME "rsh"
#endif /* AFS_HPUX_ENV */

/*
 * rsh - remote shell
 */
/* VARARGS */
int	error();

int	errno;
int	options;
int	rfd2;
int	nflag;
void	sendsig();

#define	mask(s)	(1 << ((s) - 1))
#if defined(AFS_AIX32_ENV) && (defined(NLS) || defined(KJI))
#include <locale.h>
#endif

#include "AFS_component_version_number.c"

/*
 * rlogin directory
 *
 * We don't really need a definition for AFS_SGI_ENV since SGI's own rsh
 * does the right thing.
 *
 * In some older platforms, such as SunOS 4.x, rlogin lives in /usr/ucb.
 * But for all our currently supported platforms for AFS 3.5, it's in /usr/bin.
 */
#ifdef	AFS_SGI_ENV
#define _PATH_RLOGIN	"/usr/bsd/rlogin"
#else
#define _PATH_RLOGIN	"/usr/bin/rlogin"
#endif

main(argc, argv0)
	int argc;
	char **argv0;
{
	int rem, pid;
	char *host, *cp, **ap, buf[BUFSIZ], *args, **argv = argv0, *user = 0;
	register int cc;
	int asrsh = 0;
	struct passwd *pwd;
	fd_set readfrom, ready;
	int one = 1;
	struct servent *sp;

	sigset_t oset;
	sigset_t sigBlock;
	int someSignals[100];

#ifdef	AFS_HPUX_ENV
	int fd;
	struct stat stat_buf;
	char *clientname;
#endif
#if defined(AFS_AIX32_ENV)
	struct sigaction ign_act, save_old_act;
#ifdef KAUTH
	int pass_tokens;
#endif /* KAUTH */

#if defined(AFS_AIX32_ENV) && (defined(NLS) || defined(KJI))
	setlocale(LC_ALL,"");
#endif

#ifdef	notdef
    /*
     * If we're being called as a non-afs version of the program, and if AFS extensions
     * have been loaded, run the AFS version  of the program.
     */
    check_and_run_afs_vers(argv);
#endif
	bzero(&ign_act, sizeof(ign_act));
	ign_act.sa_handler=SIG_IGN;
#endif
	host = strrchr(argv[0], '/');
	if (host)
		host++;
	else
		host = argv[0];
	argv++, --argc;
#ifdef	AFS_HPUX_ENV
	/* if invoked as something other than remsh or rsh, use the 
	 * invocation name as the host name to connect to (clever).
	 */
	if (!strcmp(host, "remsh") || !strcmp(host, "rsh")) {
	    clientname = host;
	    host = *argv++, --argc;
	    asrsh = 1;
	} else {
	    clientname = "remsh";
	}
#else
	if (!strcmp(host, RSHNAME)) {
	    if (argc == 0)
		goto usage;
	    if (*argv[0] != '-') {
		host = *argv++, --argc;
		asrsh = 1;
	    } else
		host = 0;
	}
#endif
#ifdef KAUTH
	pass_tokens=(int)getenv("KAUTH");
	if (pass_tokens) {
		pass_tokens=(!strcmp((char*)pass_tokens,"afs"));
	}
#endif /* KAUTH */
#ifdef	AFS_HPUX_ENV
	/* make sure file descriptors 0, 1, and 2 are open */
	for (fd = 0; fd <= 2 ; fd++) {
	    if (fstat(fd, &stat_buf) != 0) {
		if (open("/dev/null", O_RDWR) < 0) {
		    fprintf(stderr, "%s: ", clientname);
		    perror("open:");
		    exit(1);
		}
	    }
	}
	/* save a copy of original environment in case we exec rlogin */
	{ int vecsize = 0, envsize = 0, i = 0;
	  vp = environ;
	  while (vp != (char **) NULL && *vp != (char *) NULL ) {
	      vecsize++;
	      envsize += strlen(*vp) + 1;
	      vp++;
	  }
	  vp = (char **)malloc((vecsize + 1) * sizeof (char *));
	  cp = malloc(envsize);
	  while (i < vecsize) {
	      vp[i] = cp;
	      strcpy(vp[i], environ[i]);
	      while (*cp != (char) NULL) cp++;
	      cp++;
	      i++;
	  }
       }
	
	/* clear timers, close open files, and wipe out environment */
	cleanenv(&environ, "LANG", "LANGOPTS", "NLSPATH", "LOCALDOMAIN", "HOSTALIASES", 0);
#endif
another:
	if (argc > 0 && !strcmp(*argv, "-l")) {
		argv++, argc--;
		if (argc > 0)
			user = *argv++, argc--;
		goto another;
	}
	if (argc > 0 && !strcmp(*argv, "-n")) {
		argv++, argc--;
		nflag++;
#ifdef	AFS_SUN_ENV
		(void) close(0);
		(void) open("/dev/null", 0);
#endif
		goto another;
	}
	if (argc > 0 && !strcmp(*argv, "-d")) {
		argv++, argc--;
		options |= SO_DEBUG;
		goto another;
	}
#ifdef KAUTH
	if (argc > 0 && !strcmp(*argv, "-v")) {
		argv++, argc--;
		pass_tokens=1;
		goto another;
	}
	if (argc > 0 && !strcmp(*argv, "-V")) {
		argv++, argc--;
		pass_tokens=0;
		goto another;
	}
#endif /* KAUTH */
	/*
	 * Ignore the -L, -w, -e and -8 flags to allow aliases with rlogin
	 * to work
	 *
	 * There must be a better way to do this! -jmb
	 */
	if (argc > 0 && !strncmp(*argv, "-L", 2)) {
		argv++, argc--;
		goto another;
	}
	if (argc > 0 && !strncmp(*argv, "-w", 2)) {
		argv++, argc--;
		goto another;
	}
	if (argc > 0 && !strncmp(*argv, "-e", 2)) {
		argv++, argc--;
		goto another;
	}
	if (argc > 0 && !strncmp(*argv, "-8", 2)) {
		argv++, argc--;
		goto another;
	}
#ifdef	AFS_HPUX_ENV
	if (argc > 0 && !strncmp(*argv, "-7", 2)) {
		argv++, argc--;
		goto another;
	}
#endif
	if (host == 0) {
	    if (argc == 0)
		goto usage;
	    host = *argv++, --argc;
	    asrsh = 1;
	}
	if (argv[0] == 0) {
		if (asrsh)
			*argv0 = "rlogin";

#ifdef AFS_HPUX_ENV
		execve(_PATH_RLOGIN, argv0, vp);
#else
		execv(_PATH_RLOGIN, argv0);
#endif
		perror(_PATH_RLOGIN);
		exit(1);
	}
	pwd = getpwuid(getuid());
	if (pwd == 0) {
		fprintf(stderr, "who are you?\n");
		exit(1);
	}
	cc = 0;
	for (ap = argv; *ap; ap++)
		cc += strlen(*ap) + 1;
	cp = args = malloc(cc);
	for (ap = argv; *ap; ap++) {
		(void) strcpy(cp, *ap);
		while (*cp)
			cp++;
		if (ap[1])
			*cp++ = ' ';
	}
	sp = getservbyname("shell", "tcp");
	if (sp == 0) {
		fprintf(stderr, "%s: shell/tcp: unknown service\n", RSHNAME);
		exit(1);
	}
        rem = rcmd(&host, sp->s_port, pwd->pw_name,
#ifdef	AFS_AIX32_ENV
	    user ? user : pwd->pw_name, args, &rfd2,  /* long timeout? */ 1);
#else
	    user ? user : pwd->pw_name, args, &rfd2);
#endif
        if (rem < 0)
                exit(1);
	if (rfd2 < 0) {
		fprintf(stderr, "%s: can't establish stderr\n", RSHNAME);
		exit(2);
	}
	if (options & SO_DEBUG) {
		if (setsockopt(rem, SOL_SOCKET, SO_DEBUG,
			       (char *) &one, sizeof (one)) < 0)
			perror("setsockopt (stdin)");
		if (setsockopt(rfd2, SOL_SOCKET, SO_DEBUG,
			       (char *) &one, sizeof (one)) < 0)
			perror("setsockopt (stderr)");
	}
	(void) setuid(getuid());

	bzero((char *)someSignals, sizeof(someSignals));
#ifdef	AFS_HPUX_ENV
	someSignals[0] = mask(SIGINT)|mask(SIGQUIT)|mask(SIGTERM)|mask(SIGHUP);
#else
	someSignals[0] = mask(SIGINT)|mask(SIGQUIT)|mask(SIGTERM);
#endif
	sigBlock = *((sigset_t *)someSignals);
	sigprocmask(SIG_BLOCK, &sigBlock, &oset);
#ifdef	AFS_AIX32_ENV
	(void) sigaction(SIGINT, &ign_act, &save_old_act);
	if (save_old_act.sa_handler != SIG_IGN)
		(void) signal(SIGINT, sendsig);
	(void) sigaction(SIGQUIT, &ign_act, &save_old_act);
	if (save_old_act.sa_handler != SIG_IGN)
		(void) signal(SIGQUIT, sendsig);
	(void) sigaction(SIGTERM, &ign_act, &save_old_act);
	if (save_old_act.sa_handler != SIG_IGN)
		(void) signal(SIGTERM, sendsig); 
#else
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, sendsig);
	if (signal(SIGQUIT, SIG_IGN) != SIG_IGN)
		signal(SIGQUIT, sendsig);
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
		signal(SIGTERM, sendsig);
#ifdef	AFS_HPUX_ENV
	if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
		signal(SIGHUP, sendsig);
	/* ignore the death of my child -- banish zombies! */
        signal(SIGCLD, SIG_IGN);
#endif
#endif
	if (nflag == 0) {
		pid = fork();
		if (pid < 0) {
			perror("fork");
			exit(1);
		}
	}
	ioctl(rfd2, FIONBIO, &one);
	ioctl(rem, FIONBIO, &one);
        if (nflag == 0 && pid == 0) {
		char *bp; int wc;
		fd_set rembits;
		(void) close(rfd2);
	reread:
		errno = 0;
		cc = read(0, buf, sizeof buf);
		if (cc <= 0)
			goto done;
		bp = buf;
	rewrite:
		FD_ZERO(&rembits);
		FD_SET(rem, &rembits);
		if (select(rem+1, 0, &rembits, 0, 0) < 0) {
			if (errno != EINTR) {
				perror("select");
				exit(1);
			}
			goto rewrite;
		}
		if (!FD_ISSET(rem, &rembits))
			goto rewrite;
		wc = write(rem, bp, cc);
		if (wc < 0) {
			if (errno == EWOULDBLOCK)
				goto rewrite;
			goto done;
		}
		cc -= wc; bp += wc;
		if (cc == 0)
			goto reread;
		goto rewrite;
	done:
		(void) shutdown(rem, 1);
		exit(0);
	}
	sigprocmask(SIG_SETMASK, &oset, (sigset_t *)0);
	FD_ZERO(&readfrom);
	FD_SET(rfd2, &readfrom);
	FD_SET(rem, &readfrom);
	for (;;) {
		int maxfd;
		maxfd = -1;
		if (FD_ISSET(rfd2, &readfrom) && maxfd < rfd2)
			maxfd = rfd2;
		if (FD_ISSET(rem, &readfrom) && maxfd < rem)
			maxfd = rem;
		if (maxfd == -1)
			break;
		ready = readfrom;
		if (select(maxfd+1, &ready, 0, 0, 0) < 0) {
			if (errno != EINTR) {
				perror("select");
				exit(1);
			}
			continue;
		}
		if (FD_ISSET(rfd2, &ready)) {
			errno = 0;
			cc = read(rfd2, buf, sizeof buf);
			if (cc <= 0) {
				if (errno != EWOULDBLOCK)
					FD_CLR(rfd2, &readfrom);
			} else
				(void) write(2, buf, cc);
		}
		if (FD_ISSET(rem, &ready)) {
			errno = 0;
			cc = read(rem, buf, sizeof buf);
			if (cc <= 0) {
				if (errno != EWOULDBLOCK)
					FD_CLR(rem, &readfrom);
			} else
				(void) write(1, buf, cc);
		}
        }
	if (nflag == 0)
		(void) kill(pid, SIGKILL);
	exit(0);
usage:
	fprintf(stderr,
	    "usage: %s host [ -l login ] [ -n ] command\n", RSHNAME);
	exit(1);
}

void sendsig(signo)
	char signo;
{

	(void) write(rfd2, &signo, 1);
}
