/* inetd.c */

/*
 * Copyright (c) 1983 Regents of the University of California.
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
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)inetd.c	5.14 (Berkeley) 1/23/89";
#endif /* not lint */

/*
 * Inetd - Internet super-server
 *
 * This program invokes all internet services as needed.
 * connection-oriented services are invoked each time a
 * connection is made, by creating a process.  This process
 * is passed the connection as file descriptor 0 and is
 * expected to do a getpeername to find out the source host
 * and port.
 *
 * Datagram oriented services are invoked when a datagram
 * arrives; a process is created and passed a pending message
 * on file descriptor 0.  Datagram servers may either connect
 * to their peer, freeing up the original socket for inetd
 * to receive further messages on, or ``take over the socket'',
 * processing all arriving datagrams and, eventually, timing
 * out.	 The first type of server is said to be ``multi-threaded'';
 * the second type of server ``single-threaded''. 
 *
 * Inetd uses a configuration file which is read at startup
 * and, possibly, at some later time in response to a hangup signal.
 * The configuration file is ``free format'' with fields given in the
 * order shown below.  Continuation lines for an entry must being with
 * a space or tab.  All fields must be present in each entry.
 *
 *	service name			must be in /etc/services
 *	socket type			stream/dgram/raw/rdm/seqpacket
 *	protocol			must be in /etc/protocols
 *	wait/nowait			single-threaded/multi-threaded
 *	user				user to run daemon as
 *	server program			full path name
 *	server program arguments	maximum of MAXARGS (5)
 *
 * Comment lines are indicated by a `#' in column 1.
 */
#include <afs/param.h>
#include <sys/param.h>
#include <sys/stat.h>
#ifdef AFS_SUN5_ENV
#define BSD_COMP
#endif
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/file.h>
#ifdef AFS_SUN5_ENV
#include <fcntl.h>
#endif
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/afsutil.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#ifdef AIX
#include <sys/syslog.h>
#else
#include <syslog.h>
#endif /* AIX */
#include <pwd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define	TOOMANY		40		/* don't start more than TOOMANY */
#define	CNT_INTVL	60		/* servers in CNT_INTVL sec. */
#define	RETRYTIME	(60*10)		/* retry after bind or server fail */

void	config(), reapchild(), retry();

int	debug = 0;
int	nsock, maxsock;
fd_set	allsock;
int	options;
int	timingout;
struct	servent *sp;

struct	servtab {
	char	*se_service;		/* name of service */
	int	se_socktype;		/* type of socket to use */
	char	*se_proto;		/* protocol used */
	short	se_wait;		/* single threaded server */
	short	se_checked;		/* looked at during merge */
	char	*se_user;		/* user name to run as */
	struct	biltin *se_bi;		/* if built-in, description */
	char	*se_server;		/* server program */
#define MAXARGV 5
	char	*se_argv[MAXARGV+1];	/* program arguments */
	int	se_fd;			/* open descriptor */
	struct	sockaddr_in se_ctrladdr;/* bound address */
	int	se_count;		/* number started since se_time */
	struct	timeval se_time;	/* start of se_count */
	struct	servtab *se_next;
} *servtab;

int echo_stream(), discard_stream(), machtime_stream();
int daytime_stream(), chargen_stream();
int echo_dg(), discard_dg(), machtime_dg(), daytime_dg(), chargen_dg();
int auth_stream(), auth_dg();

struct biltin {
	char	*bi_service;		/* internally provided service name */
	int	bi_socktype;		/* type of socket supported */
	short	bi_fork;		/* 1 if should fork before call */
	short	bi_wait;		/* 1 if should wait for child */
	int	(*bi_fn)();		/* function which performs it */
} biltins[] = {
	/* Echo received data */
	"echo",		SOCK_STREAM,	1, 0,	echo_stream,
	"echo",		SOCK_DGRAM,	0, 0,	echo_dg,

	/* Internet /dev/null */
	"discard",	SOCK_STREAM,	1, 0,	discard_stream,
	"discard",	SOCK_DGRAM,	0, 0,	discard_dg,

	/* Return 32 bit time since 1970 */
	"time",		SOCK_STREAM,	0, 0,	machtime_stream,
	"time",		SOCK_DGRAM,	0, 0,	machtime_dg,

	/* Return human-readable time */
	"daytime",	SOCK_STREAM,	0, 0,	daytime_stream,
	"daytime",	SOCK_DGRAM,	0, 0,	daytime_dg,

	/* Familiar character generator */
	"chargen",	SOCK_STREAM,	1, 0,	chargen_stream,
	"chargen",	SOCK_DGRAM,	0, 0,	chargen_dg,

	/* Remote authentication services */
	"ta-rauth",        SOCK_STREAM,    1, 0,   auth_stream,
	"ta-rauth",        SOCK_DGRAM,     0, 0,   auth_dg,
	0
};

#define NUMINT	(sizeof(intab) / sizeof(struct inent))
char	*CONFIG = "/etc/inetd.conf";
char	**Argv;
char 	*LastArg;
int	backlog = 10;	/* listen() queue length */

#include "AFS_component_version_number.c"

long allZeroes[100];
sigset_t sigNone;
sigset_t sigBlock;

int afs_didsetpag = 0;
main(argc, argv, envp)
	int argc;
	char *argv[], *envp[];
{
	extern char *optarg;
	extern int optind;
	register struct servtab *sep;
	register struct passwd *pwd;
	struct passwd *getpwnam();
	register int tmpint;
	struct sigaction sa;
	int ch, pid, dofork;
	char buf[50];
#if defined(AFS_HPUX_ENV)
	int	consoleFid;
	pid_t	newSessionID;
#endif /* defined(AFS_HPUX_ENV) */

	memset((char *)allZeroes, '\0', sizeof(allZeroes));
	bzero((char *)allZeroes, sizeof(allZeroes));

	sigNone = *((sigset_t *) allZeroes);
	allZeroes[0] = (1<<(SIGCHLD-1)) + (1<<(SIGHUP-1)) + (1<<(SIGALRM-1));
	sigBlock = *((sigset_t *) allZeroes);

	setpag();  /* disassociate with PAG of person starting inetd */

	Argv = argv;
	if (envp == 0 || *envp == 0)
		envp = argv;
	while (*envp)
		envp++;
	LastArg = envp[-1] + strlen(envp[-1]);

	while ((ch = getopt(argc, argv, "dl:")) != EOF)
		switch(ch) {
		case 'd':
			debug = 1;
			options |= SO_DEBUG;
			break;
		case 'l':
			/* undocumented option to set listen() queue length */
			backlog = atoi(optarg);
			break;
		case '?':
		default:
			fprintf(stderr, "usage: inetd [-d]");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		CONFIG = argv[0];
	if (debug == 0) {
#if	defined(AFS_OSF_ENV) && !defined(AFS_OSF32_ENV)
	        daemon(0,0);
#else
		if (fork()) {
			exit(0);
		}
		
#ifdef AFS_HPUX_ENV
		for (tmpint = 0; tmpint < getnumfds(); tmpint++)
#else
		for (tmpint = 0; tmpint < 10; tmpint++)
#endif
			(void) close(tmpint);
		(void) open("/", O_RDONLY);
		(void) dup2(0, 1);
		(void) dup2(0, 2);
#ifndef AFS_HPUX_ENV
		tmpint = open("/dev/tty", O_RDWR);
		if (tmpint > 0) {
			ioctl(tmpint, TIOCNOTTY, (char *)0);
			close(tmpint);
		}
#else
#ifdef	notdef
		/*
		 * the way to get rid of the controlling terminal in hp-ux, if we
		 * are not a process group leader
		 */
		newSessionID = setsid();
		if (newSessionID == -1) {
		  /*
		   * we are already a process group leader, & extensive experimentation
		   * indicates that (contrary to the documentation, once again), there
		   * doesn't seem to be any way to get rid of our control tty, other
		   * than the following ugliness:
		   */
		  if ( fork() )	exit(0);
		}
#endif
#endif
#ifdef	AFS_HPUX_ENV
		(void) setpgrp();
#else
		(void) setpgrp(0, 0);
#endif

#if !defined(AIX)
		(void) signal(SIGTSTP, SIG_IGN);
		(void) signal(SIGTTIN, SIG_IGN);
		(void) signal(SIGTTOU, SIG_IGN);
#endif /* !defined(AIX) */
#endif	/* AFS_OSF_ENV */
	}

	openlog("inetd", LOG_PID | LOG_NOWAIT, LOG_DAEMON);

	memset((char *)&sa, '\0', sizeof(sa));
	sa.sa_mask = sigBlock;
	sa.sa_handler = retry;
	sigaction(SIGALRM, &sa, (struct sigaction *)0);
	config();
	sa.sa_handler = config;
	sigaction(SIGHUP, &sa, (struct sigaction *)0);
	sa.sa_handler = reapchild;
	sigaction(SIGCHLD, &sa, (struct sigaction *)0);
	{
		/* space for daemons to overwrite environment for ps */
#define	DUMMYSIZE	100
		char dummy[DUMMYSIZE];

		(void)memset(dummy, 'x', sizeof(DUMMYSIZE) - 1);
		dummy[DUMMYSIZE - 1] = '\0';
		(void)setenv("inetd_dummy", dummy, 1);
	}

	for (;;) {
	    int n, ctrl;
	    fd_set readable;

	    if (nsock == 0) {
		(void) sigprocmask(SIG_BLOCK, &sigBlock, (sigset_t *)0);
		while (nsock == 0)
		    sigpause(0L);
		(void) sigprocmask(SIG_SETMASK, &sigNone, (sigset_t *)0);
	    }
	    readable = allsock;
	    if ((n = select(maxsock + 1, &readable, (fd_set *)0,
		(fd_set *)0, (struct timeval *)0)) <= 0) {
		    if (n < 0 && errno != EINTR)
			syslog(LOG_WARNING, "select: %m\n");
		    sleep(1);
		    continue;
	    }
	    for (sep = servtab; n && sep; sep = sep->se_next)
	    if ((sep->se_fd != -1) && FD_ISSET(sep->se_fd, &readable)) {
		n--;
		if (debug)
			fprintf(stderr, "someone wants %s\n", sep->se_service);
		if (!sep->se_wait && sep->se_socktype == SOCK_STREAM) {
			ctrl = accept(sep->se_fd, (struct sockaddr *)0,
			    (int *)0);
			if (debug)
				fprintf(stderr, "accept, ctrl %d\n", ctrl);
			if (ctrl < 0) {
				if (errno == EINTR)
					continue;
				syslog(LOG_WARNING, "accept: %m");
				continue;
			}
		} else
			ctrl = sep->se_fd;
		(void) sigprocmask(SIG_BLOCK, &sigBlock, (sigset_t *)0);
		pid = 0;
		dofork = (sep->se_bi == 0 || sep->se_bi->bi_fork);
		if (dofork) {
		    if(debug)
			fprintf(stderr,"forking\n");
                    if (sep->se_socktype == SOCK_DGRAM) {
			if (sep->se_count++ == 0)
			    (void)gettimeofday(&sep->se_time,
			        (struct timezone *)0);
			else if (sep->se_count >= TOOMANY) {
				struct timeval now;

				(void)gettimeofday(&now, (struct timezone *)0);
				if (now.tv_sec - sep->se_time.tv_sec >
				    CNT_INTVL) {
					sep->se_time = now;
					sep->se_count = 1;
				} else {
					syslog(LOG_ERR,
			"%s/%s server failing (looping), service terminated %d\n",
					    sep->se_service, sep->se_proto, sep->se_socktype);
					FD_CLR(sep->se_fd, &allsock);
					(void) close(sep->se_fd);
					sep->se_fd = -1;
					sep->se_count = 0;
					nsock--;
					sigprocmask(SIG_SETMASK, &sigNone,
						    (sigset_t *)0);
					if (!timingout) {
						timingout = 1;
						alarm(RETRYTIME);
					}
					continue;
				}
			}
		    }
		    pid = fork();
		}
		if (pid < 0) {
			if (!sep->se_wait && sep->se_socktype == SOCK_STREAM)
				close(ctrl);
			sigprocmask(SIG_SETMASK, &sigNone, (sigset_t *)0);
			sleep(1);
			continue;
		}
		if (pid && sep->se_wait) {
			sep->se_wait = pid;
			FD_CLR(sep->se_fd, &allsock);
			nsock--;
		}
		sigprocmask(SIG_SETMASK, &sigNone, (sigset_t *)0);
		if (pid == 0) {
			if (debug) {
#ifdef	AFS_OSF_ENV
			    setsid();
#else
				if (dofork &&
				    (tmpint = open("/dev/tty", O_RDWR)) > 0) {
#ifndef AFS_HPUX_ENV
					ioctl(tmpint, TIOCNOTTY, 0);
#endif
					close(tmpint);
				}
				(void) setpgrp(0, 0);
#if !defined(AIX)
				(void) signal(SIGTSTP, SIG_IGN);
				(void) signal(SIGTTIN, SIG_IGN);
				(void) signal(SIGTTOU, SIG_IGN);
#endif /* !defined(AIX) */
#endif
			}
			if (dofork) {
#ifdef	AFS_HPUX_ENV
			    /* make child session leader */
			    setsid();
			    resetsignals();
			    sigsetmask(0L);
			    for (tmpint = getnumfds(); --tmpint > 2; )
#else
			    for (tmpint = getdtablesize(); --tmpint > 2; )
#endif
				if (tmpint != ctrl)
				    close(tmpint);
			}
			afs_didsetpag = 0;
			if (sep->se_bi)
				(*sep->se_bi->bi_fn)(ctrl, sep);
			else {
#ifdef	AFS_HPUX_ENV
			         int pgid = -getpid();
#else
#ifndef	AFS_OSF_ENV
			        (void) setpgrp(0, 0);
#endif
#endif
				dup2(ctrl, 0);
				close(ctrl);
				dup2(0, 1);
				dup2(0, 2);
#ifdef	AFS_HPUX_ENV
				 /* make child socket process group leader */
				 ioctl(0, SIOCSPGRP, (char *)&pgid);
#endif
				if ((pwd = getpwnam(sep->se_user)) == NULL) {
				    fprintf(stderr,"getpwnam failed\n");
					syslog(LOG_ERR,	"getpwnam: %s: No such user",sep->se_user);
					if (sep->se_socktype != SOCK_STREAM)
						recv(0, buf, sizeof (buf), 0);
					_exit(1);
				}
				if (pwd->pw_uid) {
#ifdef	AFS_HPUX_ENV
				        (void) initgroups((uid_t)pwd->pw_name,(gid_t)pwd->pw_gid);
					(void) setgid((gid_t)pwd->pw_gid);
#else
					(void) setgid((gid_t)pwd->pw_gid);
					initgroups(pwd->pw_name, pwd->pw_gid);
#endif
					(void) setuid((uid_t)pwd->pw_uid);
				}
				if (!afs_didsetpag && (!strcmp(sep->se_service, "login") || 
						       !strcmp(sep->se_service, "shell"))) {
				    setpag(); /* to disassociate it from current group... */
				}
#ifdef	AFS_HPUX_ENV
#ifdef	notdef
				if (sep->se_argv[0] != NULL) {
				    if (!strcmp(sep->se_argv[0], "%A")) {
					char addrbuf[32];
					sprintf(addrbuf, "%s.%d", 
						inet_ntoa(his_addr.sin_addr.s_addr),
						ntohs(his_addr.sin_port));
					execl(sep->se_server,
					      rindex(sep->se_server, '/')+1,
					      sep->se_socktype == SOCK_DGRAM
					      ? (char *)0 : addrbuf, (char *)0);
				    } else
					execv(sep->se_server, sep->se_argv);
				} else
#endif
#endif
				execv(sep->se_server, sep->se_argv);
				if (debug)
					fprintf(stderr, "%d execl %s\n",
					    getpid(), sep->se_server);
				if (sep->se_socktype != SOCK_STREAM)
					recv(0, buf, sizeof (buf), 0);
				syslog(LOG_ERR, "execv %s: %m", sep->se_server);
				_exit(1);
			}
		}
		if (!sep->se_wait && sep->se_socktype == SOCK_STREAM)
			close(ctrl);
	    }
	}
}

void
reapchild()
{
	int status;
	int pid;
	register struct servtab *sep;

	for (;;) {
		pid = wait3(&status, WNOHANG, (struct rusage *)0);
		if (pid <= 0)
			break;
		if (debug)
			fprintf(stderr, "%d <reaped (status %d)\n", pid,
				status);
		for (sep = servtab; sep; sep = sep->se_next)
			if (sep->se_wait == pid) {
				if (status)
					syslog(LOG_WARNING,
					    "%s: exit status 0x%x",
					    sep->se_server, status);
				if (debug)
					fprintf(stderr, "restored %s, fd %d\n",
					    sep->se_service, sep->se_fd);
				FD_SET(sep->se_fd, &allsock);
				nsock++;
				sep->se_wait = 1;
			}
	}
}

void config()
{
	register struct servtab *sep, *cp, **sepp;
	struct servtab *getconfigent(), *enter();
	sigset_t oset;

	if (!setconfig()) {
		syslog(LOG_ERR, "%s: %m", CONFIG);
		return;
	}
	for (sep = servtab; sep; sep = sep->se_next)
		sep->se_checked = 0;
	while (cp = getconfigent()) {
#if 0
	        /* fix a bug on rt */
	        if(cp->se_service == 0 || *cp->se_service == '\0')
		        break;
#endif /* 0 */
		for (sep = servtab; sep; sep = sep->se_next)
			if (strcmp(sep->se_service, cp->se_service) == 0 &&
			    strcmp(sep->se_proto, cp->se_proto) == 0)
				break;
		if (sep != 0 ) {
			int i;

			sigprocmask(SIG_BLOCK, &sigBlock, &oset);
			if (cp->se_bi == 0)
				sep->se_wait = cp->se_wait;
#define SWAP(a, b) { char *c = a; a = b; b = c; }
			if (cp->se_user)
				SWAP(sep->se_user, cp->se_user);
			if (cp->se_server)
				SWAP(sep->se_server, cp->se_server);
			for (i = 0; i < MAXARGV; i++)
				SWAP(sep->se_argv[i], cp->se_argv[i]);
			sigprocmask(SIG_SETMASK, &oset, (sigset_t *)0);
			freeconfig(cp);
			if (debug)
				print_service("REDO", sep);
		} else {
			sep = enter(cp);
			if (debug)
				print_service("ADD ", sep);
		}
		sep->se_checked = 1;
		sp = getservbyname(sep->se_service, sep->se_proto);
		if (sp == 0) {
			syslog(LOG_ERR, "%s/%s: unknown service",
			    sep->se_service, sep->se_proto);
			continue;
		}
		if (sp->s_port != sep->se_ctrladdr.sin_port) {
			sep->se_ctrladdr.sin_port = sp->s_port;
			if (sep->se_fd != -1)
				(void) close(sep->se_fd);
			sep->se_fd = -1;
		}
		if (sep->se_fd == -1)
			setup(sep);
	}
	endconfig();
	/*
	 * Purge anything not looked at above.
	 */
	sigprocmask(SIG_BLOCK, &sigBlock, &oset);
	sepp = &servtab;
	while (sep = *sepp) {
		if (sep->se_checked) {
			sepp = &sep->se_next;
			continue;
		}
		*sepp = sep->se_next;
		if (sep->se_fd != -1) {
			FD_CLR(sep->se_fd, &allsock);
			nsock--;
			(void) close(sep->se_fd);
		}
		if (debug)
			print_service("FREE", sep);
		freeconfig(sep);
		free((char *)sep);
	}
	sigprocmask(SIG_SETMASK, &oset, (sigset_t *)0);
}

void
retry()
{
	register struct servtab *sep;

	timingout = 0;
	for (sep = servtab; sep; sep = sep->se_next)
		if (sep->se_fd == -1)
			setup(sep);
}

setup(sep)
	register struct servtab *sep;
{
	int on = 1;

	if ((sep->se_fd = socket(AF_INET, sep->se_socktype, 0)) < 0) {
		syslog(LOG_ERR, "%s/%s: socket: %m",
		    sep->se_service, sep->se_proto);
		return;
	}
#define	turnon(fd, opt) \
setsockopt(fd, SOL_SOCKET, opt, (char *)&on, sizeof (on))
	if (strcmp(sep->se_proto, "tcp") == 0 && (options & SO_DEBUG) &&
	    turnon(sep->se_fd, SO_DEBUG) < 0)
		syslog(LOG_ERR, "setsockopt (SO_DEBUG): %m");
	if (turnon(sep->se_fd, SO_REUSEADDR) < 0)
		syslog(LOG_ERR, "setsockopt (SO_REUSEADDR): %m");
#undef turnon
	if (bind(sep->se_fd, (struct sockaddr *) &sep->se_ctrladdr,
	    sizeof (sep->se_ctrladdr)) < 0) {
		syslog(LOG_ERR, "%s/%s: bind: %m",
		    sep->se_service, sep->se_proto);
		(void) close(sep->se_fd);
		sep->se_fd = -1;
		if (!timingout) {
			timingout = 1;
			alarm(RETRYTIME);
		}
		return;
	}
	if (sep->se_socktype == SOCK_STREAM)
		listen(sep->se_fd, backlog);
	FD_SET(sep->se_fd, &allsock);
	nsock++;
	if (sep->se_fd > maxsock)
		maxsock = sep->se_fd;
}

struct servtab *
enter(cp)
	struct servtab *cp;
{
	register struct servtab *sep;
	sigset_t oset;

	sep = (struct servtab *)malloc(sizeof (*sep));
	if (sep == (struct servtab *)0) {
		syslog(LOG_ERR, "Out of memory.");
		exit(-1);
	}
	*sep = *cp;
	sep->se_fd = -1;
	sigprocmask(SIG_BLOCK, &sigBlock, &oset);
	sep->se_next = servtab;
	servtab = sep;
	sigprocmask(SIG_SETMASK, &oset, (sigset_t *)0);
	return (sep);
}

FILE	*fconfig = NULL;
struct	servtab serv;
char	line[256];
char	*skip(), *nextline();

setconfig()
{

	if (fconfig != NULL) {
		fseek(fconfig, 0L, L_SET);
		return (1);
	}
	fconfig = fopen(CONFIG, "r");
	return (fconfig != NULL);
}

endconfig()
{
	if (fconfig) {
		(void) fclose(fconfig);
		fconfig = NULL;
	}
}

struct servtab *
getconfigent()
{
	register struct servtab *sep = &serv;
	int argc;
	char *cp, *arg, *copyofstr();

more:
	/* modified to skip blank lines... */
	while ((cp = nextline(fconfig)) && (*cp == '#' || (strlen(cp) < 2)))
		;
	if (cp == NULL)
		return ((struct servtab *)0);
	sep->se_service = copyofstr(skip(&cp));
	arg = skip(&cp);
	if (strcmp(arg, "stream") == 0)
		sep->se_socktype = SOCK_STREAM;
	else if (strcmp(arg, "dgram") == 0)
		sep->se_socktype = SOCK_DGRAM;
	else if (strcmp(arg, "rdm") == 0)
		sep->se_socktype = SOCK_RDM;
	else if (strcmp(arg, "seqpacket") == 0)
		sep->se_socktype = SOCK_SEQPACKET;
	else if (strcmp(arg, "raw") == 0)
		sep->se_socktype = SOCK_RAW;
	else
		sep->se_socktype = -1;
	sep->se_proto = copyofstr(skip(&cp));
	arg = skip(&cp);
	sep->se_wait = strcmp(arg, "wait") == 0;
	sep->se_user = copyofstr(skip(&cp));
	sep->se_server = copyofstr(skip(&cp));
	if (strcmp(sep->se_server, "internal") == 0) {
		register struct biltin *bi;

		for (bi = biltins; bi->bi_service; bi++)
			if (bi->bi_socktype == sep->se_socktype &&
			    strcmp(bi->bi_service, sep->se_service) == 0)
				break;
		if (bi->bi_service == 0) {
			syslog(LOG_ERR, "internal service %s unknown\n",
				sep->se_service);
			goto more;
		}
		sep->se_bi = bi;
		sep->se_wait = bi->bi_wait;
	} else
		sep->se_bi = NULL;
	argc = 0;
	for (arg = skip(&cp); cp; arg = skip(&cp))
		if (argc < MAXARGV)
			sep->se_argv[argc++] = copyofstr(arg);
	while (argc <= MAXARGV)
		sep->se_argv[argc++] = NULL;
	return (sep);
}

freeconfig(cp)
	register struct servtab *cp;
{
	int i;

	if (cp->se_service)
		free(cp->se_service);
	if (cp->se_proto)
		free(cp->se_proto);
	if (cp->se_user)
		free(cp->se_user);
	if (cp->se_server)
		free(cp->se_server);
	for (i = 0; i < MAXARGV; i++)
		if (cp->se_argv[i])
			free(cp->se_argv[i]);
}

char *
skip(cpp)
	char **cpp;
{
	register char *cp = *cpp;
	char *start;

again:
	while (*cp == ' ' || *cp == '\t')
		cp++;
	if (*cp == '\0') {
		char c;

		c = getc(fconfig);
		(void) ungetc(c, fconfig);
		if (c == ' ' || c == '\t')
			if (cp = nextline(fconfig))
				goto again;
		*cpp = (char *)0;
		return ((char *)0);
	}
	start = cp;
	while (*cp && *cp != ' ' && *cp != '\t')
		cp++;
	if (*cp != '\0')
		*cp++ = '\0';
	*cpp = cp;
	return (start);
}

char *
nextline(fd)
	FILE *fd;
{
	char *cp;

	if (fgets(line, sizeof (line), fd) == NULL)
		return ((char *)0);
	cp = strchr(line, '\n');
	if (cp)
		*cp = '\0';
	return (line);
}

char *
copyofstr(cp)
	const char *cp;
{
	char *new;

	if (cp == NULL)
		cp = "";
	new = malloc((unsigned)(strlen(cp) + 1));
	if (new == (char *)0) {
		syslog(LOG_ERR, "Out of memory.");
		exit(-1);
	}
	(void)strcpy(new, cp);
	return (new);
}

setproctitle(a, s)
	char *a;
	int s;
{
	int size;
	register char *cp;
	struct sockaddr_in sin;
	char buf[80];

	cp = Argv[0];
	size = sizeof(sin);
	if (getpeername(s, (struct sockaddr *) &sin, &size) == 0)
		(void) sprintf(buf, "-%s [%s]", a, inet_ntoa(sin.sin_addr)); 
	else
		(void) sprintf(buf, "-%s", a); 
	strncpy(cp, buf, LastArg - cp);
	cp += strlen(cp);
	while (cp < LastArg)
		*cp++ = ' ';
}

/*
 * Internet services provided internally by inetd:
 */

/* ARGSUSED */
echo_stream(s, sep)		/* Echo service -- echo data back */
	int s;
	struct servtab *sep;
{
	char buffer[BUFSIZ];
	int i;

	setproctitle(sep->se_service, s);
	while ((i = read(s, buffer, sizeof(buffer))) > 0 &&
	    write(s, buffer, i) > 0)
		;
	exit(0);
}

/* ARGSUSED */
echo_dg(s, sep)			/* Echo service -- echo data back */
	int s;
	struct servtab *sep;
{
	char buffer[BUFSIZ];
	int i, size;
	struct sockaddr sa;

	size = sizeof(sa);
	if ((i = recvfrom(s, buffer, sizeof(buffer), 0, &sa, &size)) < 0)
		return;
	(void) sendto(s, buffer, i, 0, &sa, sizeof(sa));
}

/* ARGSUSED */
discard_stream(s, sep)		/* Discard service -- ignore data */
	int s;
	struct servtab *sep;
{
	char buffer[BUFSIZ];

	setproctitle(sep->se_service, s);
	while (1) {
		while (read(s, buffer, sizeof(buffer)) > 0)
			;
		if (errno != EINTR)
			break;
	}
	exit(0);
}

/* ARGSUSED */
discard_dg(s, sep)		/* Discard service -- ignore data */
	int s;
	struct servtab *sep;
{
	char buffer[BUFSIZ];

	(void) read(s, buffer, sizeof(buffer));
}

#define LINESIZ 72
char ring[128];
char *endring;

initring()
{
	register int i;

	endring = ring;

	for (i = 0; i <= 128; ++i)
		if (isprint(i))
			*endring++ = i;
}

/* ARGSUSED */
chargen_stream(s, sep)		/* Character generator */
	int s;
	struct servtab *sep;
{
	register char *rs;
	int len;
	char text[LINESIZ+2];

	setproctitle(sep->se_service, s);

	if (!endring) {
		initring();
		rs = ring;
	}

	text[LINESIZ] = '\r';
	text[LINESIZ + 1] = '\n';
	for (rs = ring;;) {
		if ((len = endring - rs) >= LINESIZ)
			memcpy(text, rs, LINESIZ);
		else {
			memcpy(text, rs, len);
			memcpy(text + len, ring, LINESIZ - len);
		}
		if (++rs == endring)
			rs = ring;
		if (write(s, text, sizeof(text)) != sizeof(text))
			break;
	}
	exit(0);
}

/* ARGSUSED */
chargen_dg(s, sep)		/* Character generator */
	int s;
	struct servtab *sep;
{
	struct sockaddr sa;
	static char *rs;
	int len, size;
	char text[LINESIZ+2];

	if (endring == 0) {
		initring();
		rs = ring;
	}

	size = sizeof(sa);
	if (recvfrom(s, text, sizeof(text), 0, &sa, &size) < 0)
		return;

	if ((len = endring - rs) >= LINESIZ)
		memcpy(text, rs, LINESIZ);
	else {
		memcpy(text, rs, len);
		memcpy(text + len, ring, LINESIZ - len);
	}
	if (++rs == endring)
		rs = ring;
	text[LINESIZ] = '\r';
	text[LINESIZ + 1] = '\n';
	(void) sendto(s, text, sizeof(text), 0, &sa, sizeof(sa));
}

/*
 * Return a machine readable date and time, in the form of the
 * number of seconds since midnight, Jan 1, 1900.  Since gettimeofday
 * returns the number of seconds since midnight, Jan 1, 1970,
 * we must add 2208988800 seconds to this figure to make up for
 * some seventy years Bell Labs was asleep.
 */

afs_int32
machtime()
{
	struct timeval tv;

	if (gettimeofday(&tv, (struct timezone *)0) < 0) {
		fprintf(stderr, "Unable to get time of day\n");
		return (0L);
	}
	return (htonl((afs_int32)tv.tv_sec + 2208988800));
}

/* ARGSUSED */
machtime_stream(s, sep)
	int s;
	struct servtab *sep;
{
	afs_int32 result;

	result = machtime();
	(void) write(s, (char *) &result, sizeof(result));
}

/* ARGSUSED */
machtime_dg(s, sep)
	int s;
	struct servtab *sep;
{
	afs_int32 result;
	struct sockaddr sa;
	int size;

	size = sizeof(sa);
	if (recvfrom(s, (char *)&result, sizeof(result), 0, &sa, &size) < 0)
		return;
	result = machtime();
	(void) sendto(s, (char *) &result, sizeof(result), 0, &sa, sizeof(sa));
}

/* ARGSUSED */
daytime_stream(s, sep)		/* Return human-readable time of day */
	int s;
	struct servtab *sep;
{
	char buffer[256];
	time_t clock;

	clock = time((time_t *) 0);

	(void) sprintf(buffer, "%.24s\r\n", ctime(&clock));
	(void) write(s, buffer, strlen(buffer));
}

/* ARGSUSED */
daytime_dg(s, sep)		/* Return human-readable time of day */
	int s;
	struct servtab *sep;
{
	char buffer[256];
	time_t clock;
	struct sockaddr sa;
	int size;

	clock = time((time_t *) 0);

	size = sizeof(sa);
	if (recvfrom(s, buffer, sizeof(buffer), 0, &sa, &size) < 0)
		return;
	(void) sprintf(buffer, "%.24s\r\n", ctime(&clock));
	(void) sendto(s, buffer, strlen(buffer), 0, &sa, sizeof(sa));
}

/*
 * print_service:
 *	Dump relevant information to stderr
 */
print_service(action, sep)
	char *action;
	struct servtab *sep;
{
	fprintf(stderr,
	    "%s: %s proto=%s, wait=%d, user=%s builtin=%x server=%s\n",
	    action, sep->se_service, sep->se_proto,
	    sep->se_wait, sep->se_user, (int)sep->se_bi, sep->se_server);
}
/*
 * (C) Copyright 1989 Transarc Corporation.  All Rights Reserved.
 */

/*
 * This program is a front-end for the various remote access programs
 * (e.g. rsh, rcp, rlogin, ftp) to allow for weak remote authentication.
 * It will be used by a call to a well-known restricted tcp port;  if
 * there is a service on that port, we will attempt authentication.
 *
 * Note, this only affects the Kerberos portion of the authentication;
 * the program still requires its existing authentication (although it
 * seems reasonable to change this in the future.)
 *
 * The advantage to this scheme (rather than modifying each program to
 * incorporate this authentication scheme) is it allows us to modify
 * the authentication mechanism without requiring additonal code
 * changes to the other programs.
 *
 * Current format of authentication packet:
 *
 *   (1) User;
 *   (2) Service;
 *   (3) Token length (null terminated);
 *   (4) Token (not null terminated).
 *
 * The code to add/delete to the AFS ticket cache is taken from the
 * authentication library.
 */

/*
 * routine to do the actual authentication.  At this time, it is
 * very simple -- the remote end passes a token length and a token.
 *
 * This routine returns the name of the requested service.
 */

#define gettime(tl) do { struct timezone tzp; struct timeval tv; \
                         gettimeofday(&tv,&tzp); tl = tv.tv_sec; } while(0)
#define RAUTH_TOKENLIFE (60 * 60 * 2) /* 2 hours */

#define	NOPAG	0xffffffff
int get_pag_from_groups(g0, g1)
afs_uint32 g0, g1;
{
	afs_uint32 h, l, result;

	g0 -= 0x3f00;
	g1 -= 0x3f00;
	if (g0 < 0xc000 && g1 < 0xc000) {
		l = ((g0 & 0x3fff) << 14) | (g1 & 0x3fff);
		h = (g0 >> 14);
		h = (g1 >> 14) + h + h + h;
		result =  ((h << 28) | l);
		/* Additional testing */	
		if (((result >> 24) & 0xff) == 'A')
			return result;
		else
			return NOPAG;
	}
	return NOPAG;
}

auth_stream(s, sepent)
   int s;
   struct servtab *sepent;
  {
    char service[100], remoteName[64];
    struct sockaddr_in from;
    int fromlen;
    int code;
    struct afsconf_dir *tdir;
    struct ktc_principal tserver, tclient;
    struct ktc_token token;
    register struct servtab *sep;

    /*
     * First, obtain information on remote end of connection.
     */
    if(debug)
	fprintf(stderr,"auth_stream: entered\n");
#ifndef	AFS_SUN5_ENV
    if (getpeername(s, &from, &fromlen) < 0) {
	syslog(LOG_ERR, "getpeername failed");
	if(debug)
	    fprintf(stderr,"auth_stream: getpeername failed\n");
	exit(1);
      }
#endif
    if(intoken(s,&token,service,remoteName) != 0) {
	syslog(LOG_ERR,"invalid remote authentication");
	if(debug)
	    fprintf(stderr,"auth_stream: invalid remote authentication\n");
	exit(1);
      }
    /* lookup the name of the local cell */

    if(debug)
	fprintf(stderr,"auth_stream: look up local cell name\n");

    tdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);
    if (!tdir) {
	syslog(LOG_NOTICE, "Can't open dir %s\n", AFSDIR_CLIENT_ETC_DIRPATH);
	if(debug)
	    fprintf(stderr,"Can't open dir %s\n", AFSDIR_CLIENT_ETC_DIRPATH);
        exit(1);
      }
    /* done with configuration stuff now */
    afsconf_Close(tdir);
    /* set ticket in local cell */
    strcpy(tserver.cell, remoteName);
    strcpy(tserver.name, "afs");
    tclient = tserver;
    /* now, set the token */

    if(debug) {
	fprintf(stderr,
		"token information: service is %s\ntoken length is %d\n",
		service, token. ticketLen);
	fprintf(stderr,"token is %s\n",token.ticket);
	fprintf(stderr,"cell name is %s\n",remoteName);
      }
    setpag();
    afs_didsetpag = 1;
    code = ktc_SetToken(&tserver, &token, &tclient, 0);
    if (code) {
	write(s,"0",1); /* say "no" to other side */
	printf("Login incorrect.(%d)", code);
	syslog(LOG_ERR, "Invalid token from %s",
	       inet_ntoa(from.sin_addr));
	exit(1);
      }
    write(s, "1", 1); /* say "yes" to other side */
    
    if(debug)
	fprintf(stderr,"Finished authentication code\n");
    for (sep = servtab; sep; sep = sep->se_next)
	if(strcmp(sep->se_service,service) == 0) {
	    int dofork = (sep->se_bi == 0 || sep->se_bi->bi_fork);
	    int tmpint;

            if(dofork) {
		for(tmpint = getdtablesize(); --tmpint > 2; )
		    if(tmpint != s)
			close(tmpint);
	    }
	    if(sep->se_bi) {
		(*sep->se_bi->bi_fn)(s, sep);
	    } else {
		register struct passwd *pwd;
		struct passwd *getpwnam();
		char buf[BUFSIZ];
		    
		if ((pwd = getpwnam(sep->se_user)) == NULL) {
		    syslog(LOG_ERR,
			   "getpwnam: %s: No such user",
			   sep->se_user);
		    if (sep->se_socktype != SOCK_STREAM)
			recv(0, buf, sizeof (buf), 0);
		    abort();
		    _exit(1);
		  }
		if (pwd->pw_uid) {
		    (void) setgid((gid_t)pwd->pw_gid);
		    initgroups(pwd->pw_name, pwd->pw_gid);
		    (void) setuid((uid_t)pwd->pw_uid);
		  }
		dup2(s,0);
		close(s);
		dup2(0,1);
		if(debug)
		    fprintf(stderr,"going to exec program %s(errno = %d)\n",
			    sep->se_server,errno);
		errno = 0;
 		debug = 0;
		dup2(0,2);

		if (!afs_didsetpag && (!strcmp(sep->se_service, "login") || 
				       !strcmp(sep->se_service, "shell"))) {
		    setpag(); /* to disassociate it from current group... */
		}
		execv(sep->se_server, sep->se_argv);
		abort();
		if (sep->se_socktype != SOCK_STREAM)
		    recv(0, buf, sizeof (buf), 0);
		syslog(LOG_ERR, "execv %s: %m", sep->se_server);
		_exit(1);
	      }
        }
    fprintf(stderr,"service not available\n");
    syslog(LOG_ERR, "auth_stream: invalid service requested %s\n",service);
    exit(0);
  }
 
   
auth_dg()
  {
    syslog(LOG_NOTICE,
	   "datagram remote authentication requested, not supported"); 
  }
   
/*
 * intoken:
 *
 * This routine accepts a token on the specified file handle;
 * The input format for a token is:
 *
 *   Field #    Contents         description
 *    (0)       Service Name     char[]
 *    (1)       Version #        unsigned integer (< 2^32)
 *    (2)       cellName         char[]
 *    (3)       startTime        unsigned afs_int32 (< 2^32)
 *    (4)       endTime          unsigned afs_int32 (< 2^32)
 *    (5)       sessionKey       char[8]
 *    (6)       kvno             short (< 2^16)
 *    (7)       ticketLen        unsigned integer (< 2^32)
 *    (8)       ticket           char[MAXKTCTICKETLEN]
 *
 * Each field is comma separated;  the last is variable length.  The
 * converted token is placed into the token structure pointed to by
 * the variable "token".
 */  

intoken(s,token,svc, cell)
   int s;
   struct ktc_token *token;
   char *svc, *cell;
  {
    char buf[1024], *bp;
    int count;
    unsigned index, version;

    if(debug)
	fprintf(stderr,"intoken: entered\n");

    if((count = recv(s,buf,sizeof buf,0)) == -1) {
	if(debug) {
	    fprintf(stderr,"error on fd %d\n",s);
	    perror("intoken recv");
	  }
	return(-1);
      }

    /* (0) Service Name */
    for(index = 0; index < count && buf[index] != ','; index++)
	;

    if (index == count) {
	if(debug)
	    fprintf(stderr,"overran buffer while searching for svc name\n");
	return(-1);
      }

    if (buf[index] != ',') {
	if(debug)
	    fprintf(stderr,"Didn't stop on a comma, searching for svc name\n");
	return(-1);
      }

    buf[index] = '\0';

    strcpy(svc, buf);
    
    /* (1) Version # */

    bp = buf + index + 1;

    for(; index < count && buf[index] != ','; index++)
	;

    if (index == count) {
	if(debug)
	    fprintf(stderr,"overran buffer while searching for version #\n");
	return(-1);
      }

    if (buf[index] != ',') {
	if(debug)
	    fprintf(stderr,
		    "Didn't stop on a comma, searching for version #\n");
	return(-1);
      }

    buf[index] = '\0';
    
    sscanf(bp, "%u", &version);

    if(version > 2) {
	if(debug)
	    fprintf(stderr,"Incompatible (newer) version encountered: %d\n",
		    version);
	return(-1);
      }

    if(version > 1) {
	/* we didn't include cell name in prior versions */
	bp = buf + index + 1;
	
	for(index = 0; index < count && buf[index] != ','; index++)
	    ;
	
	if (index == count) {
	    if(debug)
		fprintf(stderr,"overran buffer while searching for cell\n");
	    return(-1);
	}
	
	if (buf[index] != ',') {
	    if(debug)
		fprintf(stderr,"Didn't stop on a comma, searching for cell\n");
	    return(-1);
	}
	buf[index] = '\0';
	strcpy(cell, bp);
    }

    /* (2) startTime */
    
    bp = buf + index + 1;

    for(; index < count && buf[index] != ','; index++)
	;

    if (index == count) {
	if(debug)
	    fprintf(stderr,
		    "overran buffer while searching for startTime #\n");
	exit(1);
      }

    if (buf[index] != ',') {
	if(debug)
	    fprintf(stderr,
		    "Didn't stop on a comma, searching for startTime #\n");
	return(-1);
      }

    buf[index] = '\0';
    
    sscanf(bp, "%u", &token->startTime);
    
    /* (3) endTime */
    
    bp = buf + index + 1;

    for(; index < count && buf[index] != ','; index++)
	;

    if (index == count) {
	if(debug)
	    fprintf(stderr,
		    "overran buffer while searching for endTime #\n");
	return(-1);
      }

    if (buf[index] != ',') {
	if(debug)
	    fprintf(stderr,
		    "Didn't stop on a comma, searching for endTime #\n");
	return(-1);
      }

    buf[index] = '\0';
    
    sscanf(bp, "%u", &token->endTime);
    
    /* (4) sessionKey */

    bp = buf + index + 1;
    memcpy(&token->sessionKey, bp, 8);
    
    /* (5) kvno */
    
    bp += 8;
    for(index += 9; index < count && buf[index] != ','; index++)
	;

    if (index == count) {
	if(debug)
	    fprintf(stderr,"overran buffer while searching for kvno\n");
	return(-1);
      }

    if (buf[index] != ',') {
	if(debug)
	    fprintf(stderr,"Didn't stop on a comma, searching for kvno\n");
	return(-1);
      }

    buf[index] = '\0';
    
    /* kvno is actually a short, so insist that it scan a short */
    
    sscanf(bp, "%hu", &token->kvno);

    /* (6) ticketLen */
    
    bp = buf + index + 1;
    
    for(; index < count && buf[index] != ','; index++)
	;

    if (index == count) {
	if(debug)
	    fprintf(stderr,"overran buffer while searching for ticketLen\n");
	return(-1);
      }

    if (buf[index] != ',') {
	if(debug)
	    fprintf(stderr,
		    "Didn't stop on a comma, searching for ticketLen\n");
	return(-1);
      }

    buf[index] = '\0';

    sscanf(bp, "%u", &token->ticketLen);
    
    /* (7) ticketLen */
    
    bp = buf + index + 1;

    if(index + token->ticketLen > count) {
	if(debug)
	    fprintf(stderr,"overran buffer while copying ticket\n");
	return(-1);
      }

    memcpy(token->ticket, bp, token->ticketLen);

    return 0;
  }

#ifdef	AFS_HPUX_ENV
resetsignals()
{ 
    struct sigaction act, oact;
    int sign;

    memset(&act, '\0', sizeof(act));
    act.sa_handler = SIG_DFL;
    for (sign = 1; sign < NSIG; sign++)
	sigaction(sign, &act, &oact);
}
#endif

