/*
 * Copyright (c) 1983, 1988 The Regents of the University of California.
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
"@(#) Copyright (c) 1983, 1988 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)rlogind.c	5.22.1.6 (Berkeley) 2/7/89";
#endif /* not lint */

/*
#ifdef MSG
#include "rlogind_msg.h"
#define MF_LIBC "libc.cat"
#define MS_LIBC 1
#define MSGSTR(n,s) NLgetamsg(MF_RLOGIND, MS_RLOGIND, n, s)
#else*/
#define MSGSTR(n,s) s
/*#endif*/


/*
 * remote login server:
 *	\0
 *	remuser\0
 *	locuser\0
 *	terminal_type/speed\0
 *	data
 *
 * Automatic login protocol is done here, using login -f upon success,
 * unless OLD_LOGIN is defined (then done in login, ala 4.2/4.3BSD).
 */

#include <afs/param.h>
#ifdef	AFS_SUN5_ENV
#define BSD_COMP
#include <sys/ioctl.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/file.h>
#ifdef	AFS_SUN5_ENV
#include <fcntl.h>
#endif

#include <netinet/in.h>

#include <errno.h>
#include <pwd.h>
#include <signal.h>
#ifdef _AIX
#include <utmp.h>
/* POSIX TERMIOS */
#include <termios.h>
#include <sys/ioctl.h>
#ifndef	AFS_AIX41_ENV
#include <sys/tty.h>
#endif
#include <sys/acl.h>
#include <sys/access.h>
#else /* _AIX */
#include <sgtty.h>
#endif /* _AIX */
#include <stdio.h>
#include <netdb.h>
#ifdef	AFS_AIX32_ENV
#include <sys/lockf.h>
#endif
#include <syslog.h>
#include <strings.h>

#ifdef	AFS_OSF_ENV
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#define TTYDEFCHARS
#include <sys/termios.h>
#define _PATH_LOGIN        "/usr/bin/login"
#endif

#if defined(AFS_HPUX_ENV)
#include <fcntl.h>
#include <sys/ptyio.h>
#endif /* defined(AFS_HPUX_ENV) */

#ifdef _AIX
#define DEF_UTMP	"Active tty entry not found in utmp file.\n"

#define NOLOGIN         "/etc/nologin"
#define BSHELL          "/bin/sh"

/*#define KAUTH*/

#ifdef KAUTH
int pass_tokens;
#include <afs/auth.h>
#include <afs/cellconfig.h>
#endif /* KAUTH */

struct utmp utmp;
#define UT_NAMESIZE     sizeof(utmp.ut_user)
#define	UTSIZ		(sizeof (struct utmp))

static void     loginlog();
#endif /* _AIX */

#ifndef TIOCPKT_WINDOW
#define TIOCPKT_WINDOW 0x80
#endif

char	*env[2];
#define	NMAX 30
char	lusername[NMAX+1], rusername[NMAX+1];
static	char term[64] = "TERM=";
#define	ENVSIZE	(sizeof("TERM=")-1)	/* skip null for concatenation */
int	keepalive = 1;
int	tracing = 0;
#ifdef	AFS_OSF_ENV
int	check_all = 0;
#endif

#define	SUPERUSER(pwd)	((pwd)->pw_uid == 0)

extern	int errno;
int	reapchild();
struct	passwd *getpwnam(), *pwd;
#ifdef	AFS_HPUX_ENV
#include <unistd.h>
struct  s_passwd  *s_pwd, *getspwnam();
struct stat s_pfile;
#endif
#if defined(AFS_AIX32_ENV) && (defined(NLS) || defined(KJI))
#include <locale.h>
#endif


#include "AFS_component_version_number.c"

main(argc, argv)
	int argc;
	char **argv;
{
	extern int opterr, optind, _check_rhosts_file;
	int ch;
	int on = 1, fromlen;
	struct sockaddr_in from;
#ifdef	AFS_AIX32_ENV
	struct sigaction sa;
	void trace_handler(int);

#if defined(NLS) || defined(KJI)
	setlocale(LC_ALL,"");
#endif
#endif
	openlog("rlogind", LOG_PID | LOG_CONS, LOG_AUTH);

#ifdef	KAUTH
	pass_tokens = 0;
#endif	
	opterr = 0;
	while ((ch = getopt(argc, argv, "ln")) != EOF)
		switch (ch) {
#ifdef	AFS_OSF_ENV
		case 'a':
		        check_all = 1;
			break;
#endif
		case 'l':
			_check_rhosts_file = 0;
			break;
		case 'n':
			keepalive = 0;
			break;
#ifdef 	AFS_AIX32_ENV
#ifdef KAUTH
		case 'v':
			pass_tokens = 1;
			break;
#endif /* KAUTH */
		case 's':
			tracing = 1;
			break;
#endif
		case '?':
		default:
#ifdef	AFS_AIX32_ENV
#ifdef KAUTH
			syslog(LOG_ERR, "usage: rlogind [-v] [-l] [-n] [-s]");
#else /* KAUTH */
			syslog(LOG_ERR, "usage: rlogind [-l] [-n] [-s]");
#endif /* KAUTH */
#else
#ifdef	AFS_OSF_ENV
			syslog(LOG_ERR, "usage: rlogind [-l] [-n] [-a] [-s]");
#else
			syslog(LOG_ERR, "usage: rlogind [-l] [-n]");
#endif
#endif
			break;
		}
	argc -= optind;
	argv += optind;

	fromlen = sizeof (from);
	if (getpeername(0, (struct sockaddr *) &from, &fromlen) < 0) {
		syslog(LOG_ERR, "Couldn't get peer name of remote host: %m");
		fatalperror("Can't get peer name of host");
	}
	if (keepalive &&
	    setsockopt(0, SOL_SOCKET, SO_KEEPALIVE, (char *) &on, sizeof (on)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
#ifdef	AFS_AIX32_ENV
	if (tracing &&
	    setsockopt(0, SOL_SOCKET, SO_DEBUG, (char *) &on, sizeof (on)) < 0)
		syslog(LOG_WARNING,MSGSTR(SETDEBUG,"setsockopt (SO_DEBUG): %m")); /*MSG*/

	/* set-up signal handler routines for SRC TRACE ON/OFF support */
	bzero((char *)&sa, sizeof(sa));
	sa.sa_mask.losigs = sigmask(SIGUSR2);
	sa.sa_handler = trace_handler;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGUSR1, &sa, (struct sigaction *)0);
	sa.sa_mask.losigs = sigmask(SIGUSR1);
	sa.sa_handler = trace_handler;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGUSR2, &sa, (struct sigaction *)0);
#endif
#ifdef	AFS_OSF_ENV
	on = IPTOS_LOWDELAY;
        if (setsockopt(0, IPPROTO_IP, IP_TOS, (char *)&on, sizeof(int)) < 0)
                syslog(LOG_WARNING, "setsockopt (IP_TOS): %m");
#endif
	doit(0, &from);
}

#ifdef	AFS_AIX32_ENV
/*
 * trace_handler - SRC TRACE ON/OFF signal handler
 */
void trace_handler(int sig)
{
	int	onoff;

	onoff = (sig == SIGUSR1) ? 1 : 0;
	if (setsockopt(0, SOL_SOCKET, SO_DEBUG, &onoff, sizeof (onoff)) < 0)
		syslog(LOG_WARNING,MSGSTR(SETDEBUG,"setsockopt (SO_DEBUG): %m")); /*MSG*/
}
#endif

int	child;
void	cleanup();
int	netf;
#ifdef	AFS_OSF_ENV
char    line[MAXPATHLEN];
#else
char	*line;
#endif
#ifdef _AIX
char	tbuf[MAXPATHLEN + 2];
struct	hostent *hp;
struct	hostent hostent;
#endif /* _AIX */

extern	char	*inet_ntoa();
#ifdef	AFS_HPUX_ENV
int netp;
char master[MAXPATHLEN], slave[MAXPATHLEN], tname[MAXPATHLEN];
#endif

#if	defined(TIOCSWINSZ) || defined(AFS_OSF_ENV)
struct winsize win = { 0, 0, 0, 0 };
#else /* ~TIOCWINSIZ */
/*
 * Window/terminal size structure.
 * This information is stored by the kernel
 * in order to provide a consistent interface,
 * but is not used by the kernel.
 *
 * Type must be "unsigned short" so that types.h not required.
 */
struct winsize {
        unsigned short  ws_row;                 /* rows, in characters */
        unsigned short  ws_col;                 /* columns, in characters */
        unsigned short  ws_xpixel;              /* horizontal size, pixels */
        unsigned short  ws_ypixel;              /* vertical size, pixels */
};
#endif /* ~TIOCWINSIZ */

doit(f, fromp)
	int f;
	struct sockaddr_in *fromp;
{
	int i, p, t, pid, on = 1;
#ifdef	AFS_OSF_ENV
	int master;
#endif
#ifdef	AFS_HPUX_ENV
	int t_;
	char *tn;
#endif
	char pline[16];
	
#ifndef OLD_LOGIN
	int authenticated = 0, hostok = 0;
	char remotehost[2 * MAXHOSTNAMELEN + 1];
#endif
#ifndef _AIX
	register struct hostent *hp;
	struct hostent hostent;
#endif
	char c, *cp;

#if defined(AFS_HPUX_ENV)
	int ptyStatusFlags;
#endif /* defined(AFS_HPUX_ENV) */
	
	alarm(60);
	read(f, &c, 1);
	if (c != 0)
		exit(1);

	alarm(0);
	fromp->sin_port = ntohs((u_short)fromp->sin_port);
	hp = gethostbyaddr((char *) &fromp->sin_addr, sizeof (struct in_addr),
		fromp->sin_family);
	if (hp == 0) {
		/*
		 * Only the name is used below.
		 */
		hp = &hostent;
		hp->h_name = inet_ntoa(fromp->sin_addr);
#ifndef OLD_LOGIN
		hostok++;
#endif
	}
#ifndef OLD_LOGIN
#ifdef	AFS_HPUX_ENV
	else if (errno != ECONNREFUSED) {
#else
#ifdef	AFS_OSF_ENV
	else if (check_all || local_domain(hp->h_name)) {
#else
	else if (local_domain(hp->h_name)) {
#endif
#endif
		/*
		 * If name returned by gethostbyaddr is in our domain,
		 * attempt to verify that we haven't been fooled by someone
		 * in a remote net; look up the name and check that this
		 * address corresponds to the name.
		 */
		strncpy(remotehost, hp->h_name, sizeof(remotehost) - 1);
		remotehost[sizeof(remotehost) - 1] = 0;
		hp = gethostbyname(remotehost);
		if (hp)
#if defined(BSD_42)
		    if (!bcmp(hp->h_addr, (caddr_t)&fromp->sin_addr,
			    sizeof(fromp->sin_addr))) {
#else /* BSD_42 */
		    for (; hp->h_addr_list[0]; hp->h_addr_list++)
			if (!bcmp(hp->h_addr_list[0], (caddr_t)&fromp->sin_addr,
			    sizeof(fromp->sin_addr))) {
#endif /* BSD_42 */
				hostok++;
#if !defined(BSD_42)
				break;
#endif
			}
	} else
		hostok++;
#endif

	if (fromp->sin_family != AF_INET ||
	    fromp->sin_port >= IPPORT_RESERVED ||
	    fromp->sin_port < IPPORT_RESERVED/2) {
		syslog(LOG_NOTICE, "Connection from %s on illegal port",
			inet_ntoa(fromp->sin_addr));
		fatal(f, "Permission denied");
	}
#ifdef IP_OPTIONS
      {
	u_char optbuf[BUFSIZ/3], *cp;
	char lbuf[BUFSIZ], *lp;
	int optsize = sizeof(optbuf), ipproto;
	struct protoent *ip;

	if ((ip = getprotobyname("ip")) != NULL)
		ipproto = ip->p_proto;
	else
		ipproto = IPPROTO_IP;
	if (getsockopt(0, ipproto, IP_OPTIONS, (char *)optbuf, &optsize) == 0 &&
	    optsize != 0) {
		lp = lbuf;
		for (cp = optbuf; optsize > 0; cp++, optsize--, lp += 3)
			sprintf(lp, " %2.2x", *cp);
		syslog(LOG_NOTICE,
		    "Connection received using IP options (ignored):%s", lbuf);
		if (setsockopt(0, ipproto, IP_OPTIONS,
		    (char *)NULL, optsize) != 0) {
			syslog(LOG_ERR, "setsockopt IP_OPTIONS NULL: %m");
			exit(1);
		}
	}
      }
#endif
	write(f, "", 1);
#ifndef OLD_LOGIN
	if (do_rlogin(hp->h_name) == 0) {
		if (hostok)
		    authenticated++;
		else
		    write(f, "rlogind: Host address mismatch.\r\n",
		     sizeof("rlogind: Host address mismatch.\r\n") - 1);
	}
#endif
#ifdef KAUTH
	if (pass_tokens) {
        if (intokens(0)) {
            fprintf(stderr,"%s: invalid remote authentication\n","login");
            exit(1);
        }
	}
#endif /* KAUTH */

#ifdef	AFS_OSF_ENV
	netf = f;

	pid = forkpty(&master, line, NULL, &win);
        if (pid < 0) {
                if (errno == ENOENT)
                        fatal(f, "Out of ptys", 0);
                else
                        fatal(f, "Forkpty", 1);
        }
	if (pid == 0) {
                if (f > 2)      /* f should always be 0, but... */
                        (void) close(f);
                setup_term(0);
                if (authenticated) {
                        execl(_PATH_LOGIN, "login", "-p",
                            "-h", hp->h_name, "-f", lusername, (char *)0);
                } else {
		    char *sp = lusername;
		    while (*sp == ' ') sp++;
		    if (!strncmp(sp, "-f", 2)) {
			syslog(LOG_ERR, "Can't use '-f' in username");
			exit(1);
		    } 
                    execl(_PATH_LOGIN, "login", "-p",
                            "-h", hp->h_name, lusername, (char *)0);
		    }
		fatalperror(2, _PATH_LOGIN);
                /*NOTREACHED*/
        }
		ioctl(f, FIONBIO, &on);
        ioctl(master, FIONBIO, &on);
        ioctl(master, TIOCPKT, &on);
        signal(SIGCHLD, cleanup);
        protocol(f, master);
        signal(SIGCHLD, SIG_IGN);
        cleanup();
#else
#if	defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV) && defined(notdef)
        /*
         * Find an unused pty.  A security hole exists if we
         * allow any other process to hold the slave side of the
         * pty open while we are setting up.  The other process
         * could read from the slave side at the time that in.rlogind
         * is forwarding the "rlogin authentication" string from the 
         * rlogin client through the pty to the login program.  If the
         * other process grabs this string, the user at the rlogin
         * client program can forge an authentication string.
         * This problem would not exist if the pty subsystem
         * would fail the open of the master side when there are
         * existing open instances of the slave side.
         * Until the pty driver is changed to work this way, we use
         * a side effect of one particular ioctl to test whether
         * any process has the slave open.
         */
 
        for (cp = "pqrstuvwxyzPQRST"; *cp; cp++) {
                struct stat stb;
                int pgrp;
 
                 /* make sure this "bank" of ptys exists */
                line = "/dev/ptyXX";
                line[strlen("/dev/pty")] = *cp;
                line[strlen("/dev/ptyp")] = '0';
                if (stat(line, &stb) < 0)
                        break;
                for (i = 0; i < 16; i++) {
                        line[strlen("/dev/ptyp")] = "0123456789abcdef"[i];
                        line[strlen("/dev/")] = 'p';
                        
                        /*
                         * Test to see if this pty is unused.  The open
                         * on the master side of the pty will fail if
                         * any other process has it open.
                         */
                        if ((p = open(line, O_RDWR | O_NOCTTY)) == -1)
                                continue;
 
                        /* 
                         * Lock the slave side so that no one else can 
                         * open it after this.
                         */
                        line[strlen("/dev/")] = 't';
                        if (chmod (line, 0600) == -1) {
                                (void) close (p);
                                continue;
                        }
 
                        /*
                         * XXX - Use a side effect of TIOCGPGRP on ptys
                         * to test whether any process is holding the
                         * slave side open.  May not work as we expect 
                         * in anything other than SunOS 4.1.
                         */
                        if ((ioctl(p, TIOCGPGRP, &pgrp) == -1) &&
                            (errno == EIO))
                                goto gotpty;
                        else {
                                (void) chmod(line, 0666);
                                (void) close(p);
                        }
                }
        }
	fatal(f, "Out of ptys");
	/*NOTREACHED*/
gotpty:
	(void) ioctl(p, TIOCSWINSZ, &win);
	netf = f;
	line[strlen("/dev/")] = 't';
	t = open(line, O_RDWR | O_NOCTTY);
	if (t < 0)
		fatalperror(f, line);
        {
	    /* These should be converted to termios ioctls. */
	    struct sgttyb b;
	    int zero = 0;  
        
	    if (gtty(t, &b) == -1)
		perror("gtty");  
	    b.sg_flags = RAW|ANYP; 
	    /* XXX - ispeed and ospeed must be non-zero */
	    b.sg_ispeed = B38400;
	    b.sg_ospeed = B38400;
	    if (stty(t, &b) == -1)
		perror("stty");
	    /*
	     * Turn off PASS8 mode, since "login" no longer does so.
	     */
	    if (ioctl(t, TIOCLSET, &zero) == -1)
		perror ("ioctl TIOCLSET"); 
	}
#else
#ifdef _AIX
	(void) setsid();
        if ((p = open("/dev/ptc", O_RDWR)) >= 0){
                line = ttyname(p);
	}
        else {
                fatal(f, MSGSTR(NOPTY, "Out of ptys")); /*MSG*/
        }

#else
#ifdef	AFS_HPUX_ENV
	if (getpty(&p, master, slave) != 0)
	    fatal(f, "Unable to allocate pty on remote host");
	/* Global copies for use by SIGCHLD handler */
	netf = f;
	netp = p;

	/* Slave becomes controlling terminal for this session. */
	t_ = open(slave, O_RDWR);
	if (t_ < 0) 
	    fatalperror(f, slave);
	/*  Remove any reference to the current control terminal */
	if (vhangup() < 0)
	    fatalperror(f, "rlogind: vhangup()");
	/* reopen the slave pseudo-terminal */
	t = open(slave, O_RDWR);
	if (t < 0)
	    fatalperror(f, slave);
	close(t_); 
	/* Get the line name for utmp accounting. */
	tn = ttyname(t);
	if (tn != NULL)
		strcpy(tname, tn);
	else
		strcpy(tname, slave);
#ifdef TIOCSWINSZ
	(void) ioctl(p, TIOCSWINSZ, &win);
#endif /* TIOCSWINSZ */
#ifndef OLD_LOGIN
	setup_term(t);
#endif /* ~OLD_LOGIN */

	/* Lock the pty so that the parent can then synchronize setting up
	 * the child's process group and controlling terminal with the
	 * login process.  The login process won't be able to read from
	 * the pty until the parent unlocks it.
	 */
	lockf(p, F_LOCK, 0);
#else
	for (c = 'p'; c <= 'z'; c++) {
		struct stat stb;
		line = "/dev/ptyXX";
		line[strlen("/dev/pty")] = c;
		line[strlen("/dev/ptyp")] = '0';
		if (stat(line, &stb) < 0)
			break;
		for (i = 0; i < 16; i++) {
			line[sizeof("/dev/ptyp") - 1] = "0123456789abcdef"[i];
			p = open(line, O_RDWR);
			if (p > 0)
				goto gotpty;
		}
	}
	fatal(f, "Out of ptys");
	/*NOTREACHED*/
gotpty:
#endif
#endif /* _AIX */

#ifdef AFS_SUN_ENV

	/* defect #2976 */

	strcpy(pline,line);
	pline[strlen("/dev/")]='t';
	chmod(pline,0620);

	if(close(p)<0)
		goto L1;
	if((p=open(line,O_RDWR))<0)
		fatal(f,"Bad open");

	L1:
#endif

#ifndef	AFS_HPUX_ENV
#ifdef	AFS_AIX32_ENV
#ifdef	TIOCSWINSZ
	(void) ioctl(p, TIOCSWINSZ, &win);
#else
#ifdef	TXSETWINSZ
	(void) ioctl(p, TXSETWINSZ, &win);
#endif	/* TXSETWINSZ */
#endif	/* TIOCSWINSZ */
#endif
#ifdef	TIOCSWINSZ
	(void) ioctl(p, TIOCSWINSZ, &win);
#endif
	netf = f;
#ifndef _AIX
	line[strlen("/dev/")] = 't';
	t = open(line, O_RDWR);
	if (t < 0)
		fatalperror(f, line);
#if !defined(ultrix)
	if (fchmod(t, 0))
		fatalperror(f, line);
	(void)signal(SIGHUP, SIG_IGN);
#if ! defined(AFS_HPUX_ENV) && ! defined(AFS_OSF_ENV)
	vhangup();
#endif /* !defined(AFS_HPUX_ENV) */
	(void)signal(SIGHUP, SIG_DFL);
#endif /* ultrix */
	t = open(line, O_RDWR);
	if (t < 0)
		fatalperror(f, line);
	setup_term(t);
#endif /* _AIX */
#endif	/* !AFS_SUN_ENV && !AFS_SUN5_ENV */
#ifdef DEBUG
	{
		int tt = open("/dev/tty", O_RDWR);
		if (tt > 0) {
			(void)ioctl(tt, TIOCNOTTY, 0);
			(void)close(tt);
		}
	}
#endif
#endif	/* !AFS_HPUX_ENV */
	pid = fork();
	if (pid < 0) {
		fatalperror(f, "");
	    }
	if (pid == 0) {
#if	defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV)  && defined(notdef)
	    int tt;

	    /* The child process needs to be the session leader
	     * and have the pty as its controlling tty
	     */
	    setpgrp(0, 0);
	    tt = open(line, O_RDWR);
	    if (tt < 0)
		fatalperror(f, line);
	    close(f), close(p), close(t);
	    if (tt != 0) dup2(tt, 0);
	    if (tt != 1) dup2(tt, 1);
	    if (tt != 2) dup2(tt, 2);
	    if (tt > 2) close(tt);
#ifdef	notdef
	    close(f), close(p);
	    dup2(t, 0), dup2(t, 1), dup2(t, 2);
	    close(t);
#endif
#else
#ifdef _AIX
		(void) setsid();
		t = open(line, O_RDWR);
		if (t < 0)
			fatalperror(f, line);
		if (acl_fset(t, NO_ACC, NO_ACC, NO_ACC))
			fatalperror(f, line);
		(void)signal(SIGHUP, SIG_IGN);
		/* frevoke(t); */
 
                {
                struct sigaction sa;
                bzero((char *)&sa, sizeof(sa));
                sa.sa_handler = SIG_DFL;
                sigaction(SIGQUIT, &sa, (struct sigaction *)0);
                sa.sa_handler = SIG_DFL;
                sigaction(SIGHUP, &sa, (struct sigaction *)0);
                }

		t = open(line, O_RDWR);
		if (t < 0)
			fatalperror(f, line);
		setup_term(t);
#endif /* _AIX */
#ifdef	AFS_OSF_ENV
		(void)setsid();
		(void)ioctl(t, TIOCSCTTY, 0);
#endif	/* AFS_OSF_ENV */
#ifdef	AFS_HPUX_ENV
		/* Wait for the parent to setup our process group and controlling tty. */
		lockf(p, F_LOCK, 0);
		lockf(p, F_ULOCK, 0);
#endif
		close(f), close(p);
		dup2(t, 0), dup2(t, 1), dup2(t, 2);
		close(t);
#endif	/* AFS_SUN_ENV */
#ifdef	AFS_AIX32_ENV
                /*
                 * Reset SIGUSR1 and SIGUSR2 to non-restartable so children
                 * of rlogind do not get clobbered by the way rlogind handles
                 * these signals.
                 */
                {
                struct sigaction sa;

                bzero((char *)&sa, sizeof(sa));
                sa.sa_mask.losigs = sigmask(SIGUSR2);
                sa.sa_handler = trace_handler;
                sigaction(SIGUSR1, &sa, (struct sigaction *)0);
                sa.sa_mask.losigs = sigmask(SIGUSR1);
                sa.sa_handler = trace_handler;
                sigaction(SIGUSR2, &sa, (struct sigaction *)0);
                }

		(void) loginlog(lusername, line, hp->h_name);
#endif
#ifdef OLD_LOGIN
		execl("/bin/login", "login", "-r", hp->h_name, 0);
#else /* OLD_LOGIN */
		if (authenticated) {
#ifdef AFS_AIX32_ENV
			execl("/bin/login.afs", "login", /*"-v",*/ "-p", "-h", hp->h_name, "-f", "--", lusername, 0);
#else
			execl("/bin/login", "login", "-p", "-h", hp->h_name, "-f", lusername, 0);
#endif
		} else {
		    char *sp = lusername;
		    while (*sp == ' ') sp++;
		    if (!strncmp(sp, "-f", 2)) {
			syslog(LOG_ERR, "Can't use '-f' in username");
			exit(1);
		    } 
#ifdef	AFS_AIX32_ENV
			execl("/bin/login.afs", "login", /*"-v",*/ "-p", "-h", hp->h_name, "--", lusername, 0);
#else
			execl("/bin/login", "login", "-p", "-h", hp->h_name, lusername, 0);
#endif
		    }
#endif /* OLD_LOGIN */
		fatalperror(2, "/bin/login");
		/*NOTREACHED*/
	    }
#ifdef	AFS_HPUX_ENV
	/* Set the child up as a process group leader.
	 * It must not be part of the parent's process group,
	 * because signals issued in the child's process group
	 * must not affect the parent.
	 */
	setpgid(pid, pid);

	/* Make the pty slave the controlling terminal for the child's process group. */
	tcsetpgrp(t, pid);
	/* Close our controlling terminal. */
	close(t);

	/* Let the child know that it is a process group leader and has a controlling tty.*/
	lockf(p, F_ULOCK, 0);
	ioctl(f, FIONBIO, &on);
	ioctl(p, FIONBIO, &on);
	ioctl(p, TIOCPKT, &on);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGCHLD, cleanup);
#ifdef AFS_HPUX102_ENV
	signal(SIGTERM, cleanup);
	signal(SIGPIPE, cleanup);
#else
	signal(SIGTERM, cleanup, sigmask(SIGCLD) | sigmask(SIGTERM) | sigmask(SIGPIPE));
	signal(SIGPIPE, cleanup, sigmask(SIGCLD) | sigmask(SIGTERM) | sigmask(SIGPIPE));
#endif
	setpgid(0, 0);
	protocol(f, p);
	signal(SIGCHLD, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	cleanup();
	return;
#endif
#ifndef	_AIX
	close(t);
#endif

	ioctl(f, FIONBIO, &on);

#if !defined(AFS_HPUX_ENV)
	ioctl(p, FIONBIO, &on);
#else /* !defined(AFS_HPUX_ENV) */
	/* HPUX does not support FIONBIO on ptys, so, we do it the hard way */
	ptyStatusFlags = fcntl(p, F_GETFL, 0);
        fcntl(p, F_SETFL, ptyStatusFlags | O_NDELAY);
#endif /* !defined(AFS_HPUX_ENV) */

#if !defined(AFS_SUN5_ENV)
	ioctl(p, TIOCPKT, &on);
	signal(SIGTSTP, SIG_IGN);
#if defined(AFS_HPUX_ENV)
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
#endif /* defined(AFS_HPUX_ENV) */
#endif /* !defined(AFS_SUN5_ENV) */
	signal(SIGCHLD, cleanup);
#ifndef	_AIX
#ifdef AFS_HPUX102_ENV
	setpgrp();
#else
	setpgrp(0, 0);
#endif
#endif
#ifdef	AFS_AIX32_ENV
	protocol(f, p,pid);
	(void)signal(SIGCHLD, SIG_IGN);
	cleanup(pid);
#else
	protocol(f, p);
	signal(SIGCHLD, SIG_IGN);
	cleanup();
#endif
#endif	/* AFS_OSF_ENV */
}

char	magic[2] = { 0377, 0377 };
char	oobdata[] = {TIOCPKT_WINDOW};

/*
 * Handle a "control" request (signaled by magic being present)
 * in the data stream.  For now, we are only willing to handle
 * window size changes.
 */
control(pty, cp, n)
	int pty;
	char *cp;
	int n;
{
	struct winsize w;

	if (n < 4+sizeof (w) || cp[2] != 's' || cp[3] != 's')
		return (0);
	oobdata[0] &= ~TIOCPKT_WINDOW;	/* we know he heard */
	bcopy(cp+4, (char *)&w, sizeof(w));
	w.ws_row = ntohs(w.ws_row);
	w.ws_col = ntohs(w.ws_col);
	w.ws_xpixel = ntohs(w.ws_xpixel);
	w.ws_ypixel = ntohs(w.ws_ypixel);
#ifdef	AFS_AIX32_ENV
#ifdef	TIOCSWINSZ
	(void) ioctl(pty, TIOCSWINSZ, &w);
#else
	(void) ioctl(pty, TXSETWINSZ, &w);
#endif	
#else
#ifdef	TIOCSWINSZ
	(void)ioctl(pty, TIOCSWINSZ, &w);
#endif
#endif
	return (4+sizeof (w));
}

/*
 * rlogin "protocol" machine.
 */
#ifdef	AFS_AIX32_ENV
protocol(f, p, pid)
	int f, p;
        pid_t pid;
#else
protocol(f, p)
	int f, p;
#endif
{
	char pibuf[1024], fibuf[1024], *pbp, *fbp;
	register pcc = 0, fcc = 0;
	int cc, nfd, pmask, fmask;
	char cntl;
	
	/*
	 * Must ignore SIGTTOU, otherwise we'll stop
	 * when we try and set slave pty's window shape
	 * (our controlling tty is the master pty).
	 */
	(void) signal(SIGTTOU, SIG_IGN);
#ifdef	AFS_OSF_ENV
	/* delay TIOCPKT_WINDOW oobdata, for backward compatibility */
	sleep(1);
#endif
	send(f, oobdata, 1, MSG_OOB);	/* indicate new rlogin */
	if (f > p)
		nfd = f + 1;
	else
		nfd = p + 1;
	fmask = 1 << f;
	pmask = 1 << p;
	for (;;) {
		int ibits, obits, ebits;

		ibits = 0;
		obits = 0;
		if (fcc)
			obits |= pmask;
		else
			ibits |= fmask;
		if (pcc >= 0)
			if (pcc)
				obits |= fmask;
			else
				ibits |= pmask;
		ebits = pmask;

		if (select(nfd, &ibits, obits ? &obits : (int *)NULL,
		    &ebits, 0) < 0) {
			if (errno == EINTR)
				continue;
#ifdef	AFS_AIX32_ENV
			cleanup(pid);
#endif
			fatalperror(f, "select");
		}
		if (ibits == 0 && obits == 0 && ebits == 0) {
			/* shouldn't happen... */
			sleep(5);
			continue;
		}

#if !defined(AFS_SUN5_ENV)
#define	pkcontrol(c)	((c)&(TIOCPKT_FLUSHWRITE|TIOCPKT_NOSTOP|TIOCPKT_DOSTOP))
		if (ebits & pmask) {
			cc = read(p, &cntl, 1);
			if (cc == 1 && pkcontrol(cntl)) {
				cntl |= oobdata[0];
				send(f, &cntl, 1, MSG_OOB);
				if (cntl & TIOCPKT_FLUSHWRITE) {
					pcc = 0;
					ibits &= ~pmask;
				}
			}
		}
#endif /* !defined(AFS_SUN5_ENV) */

		if (ibits & fmask) {
			fcc = read(f, fibuf, sizeof(fibuf));
			if (fcc < 0 && errno == EWOULDBLOCK)
				fcc = 0;
			else {
				register char *cp;
				int left, n;

				if (fcc <= 0)
					break;
				fbp = fibuf;

			top:
				for (cp = fibuf; cp < fibuf+fcc-1; cp++)
					if (cp[0] == magic[0] &&
					    cp[1] == magic[1]) {
						left = fcc - (cp-fibuf);
						n = control(p, cp, left);
						if (n) {
							left -= n;
							if (left > 0)
								bcopy(cp+n, cp, left);
							fcc -= n;
							goto top; /* n^2 */
						}
					}
				obits |= pmask;		/* try write */
			}
		}

		if ((obits & pmask) && fcc > 0) {
			cc = write(p, fbp, fcc);
			if (cc > 0) {
				fcc -= cc;
				fbp += cc;
			}
		}

		if (ibits & pmask) {
			pcc = read(p, pibuf, sizeof (pibuf));
			pbp = pibuf;
			if (pcc < 0 && errno == EWOULDBLOCK)
				pcc = 0;
			else if (pcc <= 0)
				break;
			else if (pibuf[0] == 0) {
				pbp++, pcc--;
				obits |= fmask;	/* try a write */
			} else {
#if !defined(AFS_SUN5_ENV)
				if (pkcontrol(pibuf[0])) {
					pibuf[0] |= oobdata[0];
					send(f, &pibuf[0], 1, MSG_OOB);
				}
#endif /* !defined(AFS_SUN5_ENV) */
				pcc = 0;
			}
		}

		if ((obits & fmask) && pcc > 0) {
			cc = write(f, pbp, pcc);
			if (cc < 0 && errno == EWOULDBLOCK) {
				/* also shouldn't happen */
				sleep(5);
				continue;
			}
			if (cc > 0) {
				pcc -= cc;
				pbp += cc;
			}
		}
	}
}

#ifdef	AFS_AIX32_ENV
void cleanup(pid_t pid)
#else
void cleanup()
#endif
{
	char *p;
#ifdef	AFS_HPUX_ENV
	char buf[BUFSIZ];
	int cc;

	fcntl(netp, F_SETFL, O_NDELAY);
	if ((cc = read(netp, buf, sizeof buf)) > 0) {
	    write(netf, buf, cc);
	}
	{
	    chmod(master,0666);
	    chown(master, 0, 0);
	    chmod(slave,0666);
	    chown(slave, 0, 0);
	}
#else
#ifdef	AFS_AIX32_ENV
	struct utmp cutmp;
	int found = 0, f, t;
	int rcode ;
	off_t offset;

	p = line + sizeof("/dev/") - 1;
	if (logout(p))
		logwtmp(p, "", "");
#ifdef _AIX
        (void) acl_set(line, R_ACC|W_ACC|X_ACC, R_ACC|W_ACC|X_ACC,
						R_ACC|W_ACC|X_ACC);
#else
	(void)chmod(line, 0666);
#endif /* _AIX */
	(void)chown(line, 0, 0);
	*p = 'p';
#ifdef _AIX
        (void) acl_set(line, R_ACC|W_ACC|X_ACC, R_ACC|W_ACC|X_ACC,
						R_ACC|W_ACC|X_ACC);
#else
	(void)chmod(line, 0666);
#endif /* _AIX */
#else
	p = line + sizeof("/dev/") - 1;
	if (logout(p))
		logwtmp(p, "", "");
	(void)chmod(line, 0666);
	(void)chown(line, 0, 0);
	*p = 'p';
	(void)chmod(line, 0666);
#endif
	(void)chown(line, 0, 0);
#endif
	shutdown(netf, 2);
	exit(1);
}

fatal(f, msg)
	int f;
	char *msg;
{
	char buf[BUFSIZ];

	buf[0] = '\01';		/* error indicator */
	(void) sprintf(buf + 1, "rlogind: %s.\r\n", msg);
	(void) write(f, buf, strlen(buf));

	exit(1);
}

fatalperror(f, msg)
	int f;
	char *msg;
{
	char buf[BUFSIZ];
	extern int sys_nerr;
	extern char *sys_errlist[];

	if ((unsigned)errno < sys_nerr)
		(void) sprintf(buf, "%s: %s", msg, sys_errlist[errno]);
	else
		(void) sprintf(buf, "%s: Error %d", msg, errno);
	
	fatal(f, buf);
}

#ifndef OLD_LOGIN
do_rlogin(host)
	char *host;
{

    int ru;
	getstr(rusername, sizeof(rusername), "remuser too long");
	getstr(lusername, sizeof(lusername), "locuser too long");
	getstr(term+ENVSIZE, sizeof(term)-ENVSIZE, "Terminal type too long");

	if (getuid())
		return(-1);
	pwd = getpwnam(lusername);
	if (pwd == NULL)
		return(-1);
#ifdef	AFS_HPUX_ENV
#define	SUPERUSER(pwd)	((pwd)->pw_uid == 0)
#define SECUREPASS		"/.secure/etc/passwd"
	/* If /.secure/etc/passwd file exists then make sure that user has
	 * a valid entry in both password files.
	 * Initialize s_pwd to point to pwd so check won't fail if the
	 * secure password file doesn't exists.
	 */
	s_pwd = (struct s_passwd *) pwd;
	if (stat(SECUREPASS, &s_pfile) == 0)
		s_pwd = getspwnam(lusername);
	if (s_pwd == NULL) {
	    return(-1);
	}
#endif

#ifdef _AIX
        if (*pwd->pw_shell == '\0')
                pwd->pw_shell = BSHELL;
	/* check for disabled logins before */
	/* authenticating based on .rhosts  */
	if (pwd->pw_uid)
		if (checknologin() == 0)
		    return(-1);
#endif /* _AIX */
    ru = ruserok(host, SUPERUSER(pwd), rusername, lusername);
    return (ru);
}


getstr(buf, cnt, errmsg)
	char *buf;
	int cnt;
	char *errmsg;
{
	char c;

	do {
		if (read(0, &c, 1) != 1)
			exit(1);
		if (--cnt < 0)
			fatal(1, errmsg);
		*buf++ = c;
	} while (c != 0);
}

extern	char **environ;

#ifdef _AIX
/* POSIX TERMIOS */
speed_t speeds(speed)
	char *speed;
{
	if (strcmp(speed, "38400") == 0) return(B38400);
	if (strcmp(speed, "19200") == 0) return(B19200);
	if (strcmp(speed, "9600") == 0) return(B9600);
	if (strcmp(speed, "4800") == 0) return(B4800);
	if (strcmp(speed, "2400") == 0) return(B2400);
	if (strcmp(speed, "1800") == 0) return(B1800);
	if (strcmp(speed, "1200") == 0) return(B1200);
	if (strcmp(speed, "600") == 0) return(B600);
	if (strcmp(speed, "300") == 0) return(B300);
	if (strcmp(speed, "200") == 0) return(B200);
	if (strcmp(speed, "150") == 0) return(B150);
	if (strcmp(speed, "134") == 0) return(B134);
	if (strcmp(speed, "110") == 0) return(B110);
	if (strcmp(speed, "75") == 0) return(B75);
	if (strcmp(speed, "50") == 0) return(B50);
	if (strcmp(speed, "0") == 0) return(B0);
	return(B0);
}
#else
char *speeds[] = {
	"0", "50", "75", "110", "134", "150", "200", "300", "600",
	"1200", "1800", "2400", "4800", "9600", "19200", "38400",
#ifdef	AFS_HPUX_ENV
	"EXTA", "EXTB"
#endif
};
#define	NSPEEDS	(sizeof(speeds) / sizeof(speeds[0]))
#endif

setup_term(fd)
	int fd;
{
#ifndef	AFS_OSF_ENV
	register char *cp = index(term, '/'), **cpp;
#endif
#ifdef	AFS_AIX32_ENV
#ifdef _AIX
	/* POSIX TERMIOS */
	struct termios termios;
#else /* _AIX */
	struct sgttyb sgttyb;
#endif /* _AIX */
	char *speed;
#ifdef	AFS_HPUX_ENV
        struct termio tp;
#endif

#ifdef _AIX
	/* POSIX TERMIOS */
	tcgetattr(fd, &termios);
#else /* _AIX */
#ifdef	AFS_HPUX_ENV
	ioctl(fd, TCGETA, &tp);
#else
	(void)ioctl(fd, TIOCGETP, &sgttyb);
#endif
#endif /* _AIX */
	if (cp) {
		*cp++ = '\0';
		speed = cp;
		cp = index(speed, '/');
		if (cp)
			*cp++ = '\0';
#ifdef _AIX
		/* POSIX TERMIOS */
		/* Setup PTY with some reasonable defaults */
		termios.c_cc[VINTR]  = CINTR;
		termios.c_cc[VQUIT]  = CQUIT;
		termios.c_cc[VERASE] = CERASE;
		termios.c_cc[VKILL]  = CKILL;
		termios.c_cc[VEOF]   = CEOF;
#if defined (NLS) || defined (KJI)
		/* For NLS environments, we need 8 bit data stream. */
		termios.c_iflag = IXON|BRKINT|IGNPAR|ICRNL;
		termios.c_cflag = PARENB|CS8|HUPCL|CREAD;    
#else
		termios.c_iflag = IXON|BRKINT|IGNPAR|ISTRIP|ICRNL;
		termios.c_cflag = PARENB|CS7|HUPCL|CREAD;    
#endif /* NLS */
		termios.c_oflag = OPOST|ONLCR|TAB3;
		termios.c_lflag = ISIG|ICANON|
				  ECHO|ECHOE|ECHOK|ECHOKE|ECHOCTL|
				  IEXTEN;
		cfsetispeed(&termios, speeds(speed));
		cfsetospeed(&termios, speeds(speed));
#else /* _AIX */
		for (cpp = speeds; cpp < &speeds[NSPEEDS]; cpp++)
		    if (strcmp(*cpp, speed) == 0) {
#ifdef	AFS_HPUX_ENV
			tp.c_cflag &= ~CBAUD;
			tp.c_cflag |= cpp - speeds;
#else
			sgttyb.sg_ispeed = sgttyb.sg_ospeed = cpp - speeds;
#endif
			break;
		    }
#endif /* _AIX */
	}
#ifdef _AIX
	/* POSIX TERMIOS */
	tcsetattr(fd, TCSANOW, &termios);
#else /* _AIX */
#ifdef	AFS_HPUX_ENV
	tp.c_iflag &= ~INPCK;
	tp.c_iflag |= ICRNL|IXON;
	tp.c_oflag |= OPOST|ONLCR|TAB3;
	tp.c_oflag &= ~ONLRET;
	tp.c_lflag |= (ECHO|ECHOE|ECHOK|ISIG|ICANON);
	tp.c_cflag &= ~PARENB;
	tp.c_cflag |= CS8;
	tp.c_cc[VMIN] = 1;
	tp.c_cc[VTIME] = 0;
	tp.c_cc[VEOF] = CEOF;
	ioctl(fd, TCSETAF, &tp);
#else
	sgttyb.sg_flags = ECHO|CRMOD|ANYP|XTABS;
	(void)ioctl(fd, TIOCSETP, &sgttyb);
#endif
#endif /* _AIX */

#else /* AFS_AIX32_ENV */

#ifdef	AFS_OSF_ENV
	register char *cp = index(term+ENVSIZE, '/');
	char *speed;
	struct termios tt;

        tcgetattr(fd, &tt);
        if (cp) {
                *cp++ = '\0';
                speed = cp;
                cp = index(speed, '/');
                if (cp)
                        *cp++ = '\0';
                cfsetspeed(&tt, atoi(speed));
        }

        tt.c_iflag = TTYDEF_IFLAG;
        tt.c_oflag = TTYDEF_OFLAG;
        tt.c_lflag = TTYDEF_LFLAG;
	bcopy(ttydefchars, tt.c_cc, sizeof(tt.c_cc));
        tcsetattr(fd, TCSAFLUSH, &tt);
#else
	struct sgttyb sgttyb;
	char *speed;

	(void)ioctl(fd, TIOCGETP, &sgttyb);
	if (cp) {
		*cp++ = '\0';
		speed = cp;
		cp = index(speed, '/');
		if (cp)
			*cp++ = '\0';
		for (cpp = speeds; cpp < &speeds[NSPEEDS]; cpp++)
		    if (strcmp(*cpp, speed) == 0) {
			sgttyb.sg_ispeed = sgttyb.sg_ospeed = cpp - speeds;
			break;
		    }
	}
	sgttyb.sg_flags = ECHO|CRMOD|ANYP|XTABS;
	(void)ioctl(fd, TIOCSETP, &sgttyb);
#endif
#endif /* AFS_AIX32_ENV */
	env[0] = term;
	env[1] = 0;
	environ = env;
}

/*
 * Check whether host h is in our local domain,
 * as determined by the part of the name following
 * the first '.' in its name and in ours.
 * If either name is unqualified (contains no '.'),
 * assume that the host is local, as it will be
 * interpreted as such.
 */
local_domain(h)
	char *h;
{
	char localhost[MAXHOSTNAMELEN];
	char *p1, *p2 = index(h, '.');
#ifdef	AFS_OSF_ENV
	char *topdomain();

	localhost[0] = 0;
#endif

	(void) gethostname(localhost, sizeof(localhost));
#ifdef	AFS_OSF_ENV
        p1 = topdomain(localhost);
        p2 = topdomain(h);
#else
	p1 = index(localhost, '.');
#endif
	if (p1 == NULL || p2 == NULL || !strcasecmp(p1, p2))
		return(1);
	return(0);
}
#endif /* OLD_LOGIN */


#ifdef	AFS_OSF_ENV
char *topdomain(h)
        char *h;
{
        register char *p;
        char *maybe = NULL;
        int dots = 0;

	for (p = h + strlen(h); p >= h; p--) {
                if (*p == '.') {
                        if (++dots == 2)
                                return (p);
                        maybe = p;
                }
        }
	return (maybe);
}
#endif


#ifdef _AIX
/*
 * NAME: getnonflatname()
 *                                                                    
 * FUNCTION: gets the name in /etc/passwd and replaces the
 *		flattened name that might have been passed in.
 *                                                                    
 * EXECUTION ENVIRONMENT: static
 *                                                                   
 * RETURNS: none.
 *
 */  

void
getnonflatname(char *user)
{
struct	passwd	*pw;				/* return from getpwent() */
char		currflat[UT_NAMESIZE + 1];	/* name we are looking for */
char		dataflat[UT_NAMESIZE + 1];	/* name from data base */
register int	siz;
register int	members = 0;

#ifndef _KJI
	setpwent ();

	while ((pw = getpwent ()) != NULL) {
		/* Map NLS characters in name to ascii */
		NLflatstr(user,currflat,UT_NAMESIZE + 1);
		NLflatstr(pw->pw_name,dataflat,UT_NAMESIZE + 1);
		if (strcmp (currflat, dataflat) == 0) {
			strcpy (user,pw->pw_name);	
			break;
		}
	}
	endpwent();
#endif
}


checknologin()
{
        register int fd;

        if ((fd = open(NOLOGIN, O_RDONLY, 0)) >= 0) {
		return(0);
		close(fd);
        }
	else return(-1);
}


/*
 * NAME: loginlog
 *                                                                    
 * FUNCTION: log the login
 *                                                                    
 * EXECUTION ENVIRONMENT: static
 *
 *		record login in utmp and wtmp files
 *                                                                   
 * RETURNS: void
 */  

static void
loginlog (user, tty, hostname)
char	*user;
char	*tty;
char	*hostname;
{
	char	*home;		/* user's home directory */
	char	*dev;		/* final portion of tty name [ with hft# ] */
	char	hush[PATH_MAX];	/* .hushlogin file */
	int	t;		/* index into the utmp file */
	int	found = 0;	/* utmp entry was found */
	int	f;		/* file descriptor */
	uid_t	uid;		/* the user ID */
	off_t	offset;		/* for keeping wtmp file un-corrupted */
	struct	utmp	utmp;	/* utmp structure being created */
	struct	utmp	outmp;	/* utmp entry from /etc/utmp file */
	struct	utmp	*up;	/* pointer to entry in utmp file */

	int rcode ;

	/*
	 * Initialize a utmp file entry to be filled in and written
	 * to UTMP_FILE and WTMP_FILE.
	 */

	memset (&utmp, 0, UTSIZ);
	up = &utmp;

	/*
	 * The last portion of the device name is placed in the utmp
	 * entry.  If the last portion is all digits it is assumed to
	 * be a multiplexed device, such as an hft, and the pointer
	 * is backed up to find the previous portion.
	 */

	if (dev = strrchr (tty, '/')) {
		if (! *(dev + strspn (dev, "/0123456789")))
			while (dev != tty && *--dev != '/')
				;

		if (*dev == '/')
			dev++;

		tty = dev;
	}

	if (tty)
		strncpy (up->ut_line, tty, sizeof up->ut_line);

	/*
	 * The remainder of the utmp entry is filled in.  The process
	 * id and tty line should already have been present, however,
	 * these are set here for completeness.
	 */

	strncpy (up->ut_user, user, sizeof up->ut_user);
	up->ut_pid = getpid ();
	up->ut_type = LOGIN_PROCESS;
	up->ut_time = time ((time_t *) 0);

	if (hostname)
		strncpy (up->ut_host, hostname, sizeof up->ut_host);

	/* Scan the utmp file. If an entry is found that	*/
	/* is the same as this tty, use this slot to create the */
	/* new entry. This slot has to be an empty slot. 	*/
	/* Re-using the same slot used by the tty (with an 	*/
	/* empty type) will avoid printing of empty slots on 	*/
	/* "who -a" output.					*/

	if ((f = open (UTMP_FILE, O_RDWR)) >= 0) {
		lseek(f,(off_t)0,SEEK_SET);
		rcode = lockf(f,F_LOCK,0);
		t = 0;
		while (read (f, (char *) &outmp, UTSIZ) == UTSIZ) {
			if ((!strcmp (outmp.ut_line, tty)) && 
				outmp.ut_type == EMPTY) {
				break;
			} else
				t++;
		}
		lseek (f, (off_t) (t * UTSIZ), 0);
		write (f, (char *) up, UTSIZ);
		lseek(f,(off_t)0,SEEK_SET);
		rcode = lockf(f,F_ULOCK,0);
		close (f);
	}

	/*
	 * The utmp entry is appended to the wtmp file to maintain a
	 * log of all login activity.
	 */
	if ((f = open (WTMP_FILE, O_WRONLY|O_APPEND)) >= 0) {
		offset = lseek (f, 0L, 2);
		if (offset % UTSIZ)
			write (f, (char *) up, UTSIZ - (offset % UTSIZ));

		write (f, (char *) up, UTSIZ);
		close (f);
	}

}

#ifdef	KAUTH
#define MAXLOCALTOKENS 4
#define HEADER_SIZE 18

/*
 * intkens:
 *
 * This routine accepts a token on the specified file handle;
 * The input format for a token is:
 *
 *   Field #    Contents         description
 *    (1)       Version #        unsigned integer (< 2^32)
 *    (2)       Length           unsigned integer (< 2^32)
 *    (3)       startTime        unsigned afs_int32 (< 2^32)
 *    (4)       endTime          unsigned afs_int32 (< 2^32)
 *    (5)       sessionKey       char[8]
 *    (6)       kvno             short (< 2^16)
 *    (7)       ticketLen        unsigned integer (< 2^32)
 *    (8)       ticket           char[MAXKTCTICKETLEN]
 *    (9)       AFS name         char[MAXKTCNAMELEN]
 *    (10)      Cell Name        char[MAXKTCREALMLEN]
 *
 * Each field is comma separated except (5) and (6), and (8) and (9);  the
 * ticket is variable length.  The * converted token is placed into the
 * token structure pointed to by the variable "token".  The first and second
 * fields are padded with spaces on the right so that they can be fixed length.
 * This is required so that the length can be read in before reading in the
 * whole packet.
 */  

extern int errno;

intokens(s)
int s;
  {
    char buf[512*MAXLOCALTOKENS], *bp;
    int length,readed,count;
    unsigned index, version;
	struct ktc_token token;
	struct ktc_principal tclient,tserver;

	if (setpag()) {
		perror("setpag");
		exit(1);
	}

/*
**	Read in the first two fields.
*/

	readed=0;
	errno=0;
	while (readed < HEADER_SIZE) {
    	count = read(s,(char*)((int)buf+readed),HEADER_SIZE-readed);
		if (count <= 0) {
			perror("intokens read");
			exit(1);
      	}
		readed=readed+count;
	}

	count=readed;
	bp=buf;

    /* (1) Version # */
    for(index = 0; (index+bp-buf) < count && bp[index] != ','; index++);

    if ((index+bp-buf) == count) {
		fprintf(stderr,"overran buffer while searching for version #\n");
		exit(1);
	}

	if (bp[index] != ',') {
		fprintf(stderr,"Didn't stop on a comma, searching for version #\n");
		exit(1);
	}

	bp[index] = '\0';
    
	sscanf(bp, "%u", &version);

	if(version != 2) {
		fprintf(stderr,"intokens: incompatible version encountered: %d\n",
					version);
		exit(1);
	}

	bp = bp + index + 1;

    /* (2) Length # */
    for(index = 0; (index+bp-buf) < count && bp[index] != ','; index++);

    if ((index+bp-buf) == count) {
		fprintf(stderr,"overran buffer while searching for length #\n");
		exit(1);
	}

	if (bp[index] != ',') {
		fprintf(stderr,"Didn't stop on a comma, searching for length\n");
		exit(1);
	}

	bp[index] = '\0';
    
	sscanf(bp, "%u", &length);

	bp = bp + index + 1;

	errno=0;
	while (readed < length) {
    	count = read(s,(char*)((int)buf+readed),length-readed);
		if (count <= 0) {
			perror("intokens read");
			exit(1);
      	}
		readed=readed+count;
	}

	count=readed;

/*
**	Terminate looping through the list of tokens when we hit a null byte.
*/
	while (*bp) {
	    /* (3) startTime */
    
		for (index=0; (index+bp-buf) < count && bp[index] != ','; index++);

		if ((index+bp-buf) == count) {
			fprintf(stderr,"overran buffer while searching for startTime #\n");
			exit(1);
		}

		if (bp[index] != ',') {
			fprintf(stderr,"Didn't stop on a comma, searching for startTime #\n");
			exit(1);
		}

		bp[index] = '\0';
    
		sscanf(bp, "%u", &token.startTime);

		/* (4) endTime */
    
		bp = bp + index + 1;

		for (index=0; (index+bp-buf) < count && bp[index] != ','; index++);

		if ((index+bp-buf) == count) {
			fprintf(stderr,"overran buffer while searching for endTime #\n");
			exit(1);
		}

		if (bp[index] != ',') {
			fprintf(stderr,"Didn't stop on a comma, searching for endTime #\n");
			exit(1);
		}

		bp[index] = '\0';
    
		sscanf(bp, "%u", &token.endTime);
    
		/* (5) sessionKey */

		bp = bp + index + 1;
		bcopy(bp, token.sessionKey.data, 8);
    
		/* (6) kvno */
    
		bp=bp+8;
		for (index=0; (index+bp-buf) < count && bp[index] != ','; index++);

		if ((index+bp-buf) == count) {
			fprintf(stderr,"overran buffer while searching for kvno\n");
			exit(1);
		}

		if (bp[index] != ',') {
			fprintf(stderr,"Didn't stop on a comma, searching for kvno\n");
			exit(1);
		}

		bp[index] = '\0';
    
		token.kvno=atoi(bp);
    
		/* (7) ticketLen */
    
		bp = bp + index + 1;
    
		for (index=0; (index+bp-buf) < count && bp[index] != ','; index++);

		if ((index+bp-buf) == count) {
			fprintf(stderr,"overran buffer while searching for ticketLen\n");
			exit(1);
		}

		if (bp[index] != ',') {
			fprintf(stderr,"Didn't stop on a comma, searching for ticketLen\n");
			exit(1);
		}

		bp[index] = '\0';

		sscanf(bp, "%u", &token.ticketLen);
   
		/* (8) ticketLen */
    
		bp = bp + index + 1;

		if ((index+bp-buf) + token.ticketLen > count) {
			fprintf(stderr,"overran buffer while copying ticket\n");
			exit(1);
		}

		bcopy(bp, token.ticket, token.ticketLen);

		bp = bp + token.ticketLen;

		/* (9) User name */

		for (index=0; (index+bp-buf) < count && bp[index] != ','; index++);

		if ((index+bp-buf) == count) {
			fprintf(stderr,"overran buffer while searching for Cell Name\n");
			exit(1);
		}

		bp[index]='\0';
		tclient.instance[0]='\0';
   		strncpy(tclient.name,bp,MAXKTCNAMELEN);

		/* (10) Cell name */

		bp = bp + index + 1;

		for (index=0; (index+bp-buf) < count && bp[index] != ','; index++);

		if ((index+bp-buf) == count) {
			fprintf(stderr,"overran buffer while searching for end\n");
			exit(1);
		}

		bp[index]='\0';
   		strncpy(tclient.cell,bp,MAXKTCREALMLEN);
		bp = bp + index + 1;

		strcpy(tserver.name,"afs");
		tserver.instance[0]='\0';
		strncpy(tserver.cell,tclient.cell,MAXKTCREALMLEN);

		if (ktc_SetToken(&tserver, &token, &tclient, 0)) {
			fprintf(stderr,
				"intokens: ktc_SetToken failed for %s\n",tclient.cell);
		}
	}
    return(0);
}
#endif /* KAUTH */
#endif /* _AIX */

#ifdef	AFS_HPUX_ENV
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>

# define	MAX_PTY_LEN	MAXPATHLEN+1
# define	CLONE_DRV	"/dev/ptym/clone"

/*
**	Ugh!  The following is gross, but there is no other simple way
**	to tell if a file is a master pty without looking at its major
**	device number.  Lets put it in ifdefs so it will fail to compile
**	on other architectures without modification.
*/
#ifdef __hp9000s300
#define PTY_MASTER_MAJOR_NUMBER 16
#endif
#ifdef __hp9000s800
#define PTY_MASTER_MAJOR_NUMBER 16
#endif

struct	stat	stb;
struct	stat	m_stbuf;	/* stat buffer for master pty */
struct	stat	s_stbuf;	/* stat buffer for slave pty */

/*
**	ptymdirs	--	directories to search for master ptys
**	ptysdirs	--	directories to search for slave ptys
**	ptymloc		--	full path to pty master
**	ptysloc		--	full path to pty slave
*/
char	*ptymdirs[] = { "/dev/ptym/", "/dev/", (char *) 0 } ;
char	*ptysdirs[] = { "/dev/pty/", "/dev/", (char *) 0 } ;
char	ptymloc[MAX_PTY_LEN];
char	ptysloc[MAX_PTY_LEN];

/*
**	oltrs	--	legal first char of pty name (letter) (old style)
**	onums	--	legal second char of pty name (number) for namespace 1
**              (old style)
**	ltrs	--	legal first char of pty name (letter) (new style)
**	nums	--	legal second and third char of pty name (number) (new style)
**	nums	--	legal second, third and fourth  char of pty name for names
**              space 3. 
*/
char	oltrs[] = "onmlkjihgfecbazyxwvutsrqp";
char	onums[] = "0123456789abcdef";
char	ltrs[] = "pqrstuvwxyzabcefghijklmno";
char	nums[] = "0123456789";

/*
**	getpty()	--	get a pty pair
**
**	Input	--	none
**	Output	--	zero if pty pair gotten; non-zero if not
**	[value parameter mfd]
**		  mfd	Master FD for the pty pair
**	[optional value parameters -- only set if != NULL]
**		mname	Master pty file name
**		sname	Slave pty file name
**
**	NOTE: This routine does not open the slave pty.  Therefore, if you
**	want to quickly find the slave pty to open, it is recommended that
**	you *not* pass sname as NULL.
*/

/*
**	Modified 3/28/89 by Peter Notess to search the new, expanded naming
**	convention.  The search is intended to proceed efficiently for both
**	large and small configurations, assuming that the ptys are created
**	in a pre-specified order.  This routine will do a last-ditch,
**	brute-force search of all files in the pty directories if the more
**	efficient search fails.  This is intended to make the search robust
**	in the case where the pty configuration is a non-standard one,
**	although it might take longer to find a pty in this case.  The
**	assumed order of creation is:
**		/dev/ptym/pty[p-z][0-f]
**		/dev/ptym/pty[a-ce-o][0-f]
**		/dev/ptym/pty[p-z][0-9][0-9]
**		/dev/ptym/pty[a-ce-o][0-9][0-9]
**	with /dev/ptym/pty[p-r][0-f] linked into /dev.  The search will
**	proceed in an order that leaves the ptys which also appear in /dev
**	searched last so that they remain available for other applications
**	as long as possible.
*/

/*
 * Modified by Glen A. Foster, September 23, 1986 to get around a problem
 * with 4.2BSD job control in the HP-UX (a.k.a. system V) kernel.  Before
 * this fix, getpty() used to find a master pty to use, then open the cor-
 * responding slave side, just to see if it was there (kind of a sanity
 * check), then close the slave side, fork(), and let the child re-open 
 * the slave side in order to get the proper controlling terminal.  This
 * was an excellent solution EXCEPT for the case when another process was
 * already associated with the same slave side before we (telnetd) were
 * exec()ed.  In that case, the controlling tty stuff gets all messed up,
 * and the solution is to NOT open the slave side in the parent (before the
 * fork()), but to let the child be the first to open it after its setpgrp()
 * call.  This works in all cases.  This stuff is black magic, really!
 *
 * This is necessary due to HP's implementation of 4.2BSD job control.
 */

/*
 * Modified by Byron Deadwiler, March 9, 1992 to add access to the clone
 * driver to get ptys faster.  Also added code to access ptys using
 * a new namespace (naming convention) that increases the number of
 * pty's that can be configured on a system.
 *
 *	Name-Space      It is a union of three name-spaces, the earlier two
 *			being supported for compatibility. The name comprises
 *			a generic name (/dev/pty/tty) followed by an alphabet
 *			followed by one to three numerals. The alphabet is one
 *			of the 25 in alpha[], which has 'p' thro' 'o' excluding
 *			'd'. The numeral is either hex or decimal.
 *	                ------------------------------------
 *	                | minor | name  |    Remarks       |
 *	                |----------------------------------|
 *                  |      0|  ttyp0|  Modulo 16 hex   |
 *	                |      :|      :|  representation. |
 *	                |     15|  ttypf|                  |
 *	                |     16|  ttyq0|                  |
 *	                |      :|      :|                  |
 *	                |    175|  ttyzf|                  |
 *	                |    176|  ttya0|                  |
 *	                |      :|      :|                  |
 *	                |    223|  ttycf|                  |
 *	                |    224|  ttye0|                  |
 *	                |      :|      :|                  |
 *	                |    399|  ttyof|  Total 400       |
 *	                |----------------------------------|
 *	                |    400| ttyp00|  Modulo hundred  |
 *	                |      :|      :|  decimal repr.   |
 *	                |    499| ttyp99|                  |
 *	                |    500| ttyq00|                  |
 *	                |      :|      :|                  |
 *	                |   1499| ttyz99|                  |
 *	                |   1500| ttya00|                  |
 *	                |      :|      :|                  |
 *	                |   1799| ttyc99|                  |
 *	                |   1800| ttye00|                  |
 *	                |      :|      :|                  |
 *	                |   2899| ttyo99|  Total 2500      |
 *	                |----------------------------------|
 *	                |   2900|ttyp000|  Modulo thousand |
 *	                |      :|      :|  decimal repr.   |
 *	                |   3899|ttyp999|                  |
 *	                |   4900|ttyq000|                  |
 *	                |      :|      :|                  |
 *	                |  12899|ttyz999|                  |
 *	                |  13900|ttya000|                  |
 *	                |      :|      :|                  |
 *	                |  16899|ttyc999|                  |
 *	                |  16900|ttye000|                  |
 *	                |      :|      :|                  |
 *	                |  27899|ttyo999|  Total 25000     |
 *	                |  27900|   	|     invalid      |
 *	                ------------------------------------
 */

/*
**	NOTE:	this routine should be put into a library somewhere, since
**	both rlogin and telnet need it!  also, other programs might want to
**	call it some day to get a pty pair ...
*/
getpty(mfd, mname, sname)
int	*mfd;
char	*mname, *sname;
{
    int loc, ltr, num, num2;
    register int mlen, slen;
	char *s, *s_path;

    if ((*mfd = open(CLONE_DRV, O_RDWR))  != -1) {
        s_path = ptsname(*mfd);
		strcpy(sname,s_path);
        (void) chmod (sname, 0622);
        /* get master path name */
        s = strrchr(sname, '/') + 2; /* Must succeed since slave_pty */
                                     /* begins with /dev/            */
        sprintf(mname,"%s%s%s",ptymdirs[0],"p",s);
        return 0;
    }
    

    for (loc=0; ptymdirs[loc] != (char *) 0; loc++) {
	if (stat(ptymdirs[loc], &stb))			/* no directory ... */
	    continue;					/*  so try next one */

	/*	first, try the 3rd name space ptyp000-ptyo999  */
	/*	generate the master pty path	*/
	if (namesp3(mfd,mname,sname,loc) == 0) {
             return 0;
	 }

	/*	second, try the 2nd name space ptyp00-ptyo99  */
	/*	generate the master pty path	*/
	(void) strcpy(ptymloc, ptymdirs[loc]);
	(void) strcat(ptymloc, "ptyLNN");
	mlen = strlen(ptymloc);

	/*	generate the slave pty path	*/
	(void) strcpy(ptysloc, ptysdirs[loc]);
	(void) strcat(ptysloc, "ttyLNN");
	slen = strlen(ptysloc);

	for (ltr=0; ltrs[ltr] != '\0'; ltr++) {
	    ptymloc[mlen - 3] = ltrs[ltr];
	    ptymloc[mlen - 2] = '0';
	    ptymloc[mlen - 1] = '0';
	    if (stat(ptymloc, &stb))			/* no ptyL00 ... */
		break;				/* go try old style names */

	    for (num=0; nums[num] != '\0'; num++)
	      for (num2=0; nums[num2] != '\0'; num2++) {
		ptymloc[mlen - 2] = nums[num];
		ptymloc[mlen - 1] = nums[num2];
		if ((*mfd=open(ptymloc,O_RDWR)) < 0)	/* no master	*/
		    continue;				/* try next num	*/
		
		ptysloc[slen - 3] = ltrs[ltr];
		ptysloc[slen - 2] = nums[num];
		ptysloc[slen - 1] = nums[num2];

		/*
		**	NOTE:	changed to only stat the slave device; see
		**	comments all over the place about job control ...
		*/
		if (fstat(*mfd, &m_stbuf) < 0 || stat(ptysloc, &s_stbuf) < 0) {
		    close(*mfd);
		    continue;
		}
		/*
		**	sanity check: are the minor numbers the same??
		*/
		if (minor(m_stbuf.st_rdev) != minor(s_stbuf.st_rdev)) {
		    close(*mfd);  
		    continue;				/* try next num	*/
		}

		/*	else we got both a master and a slave pty	*/
got_one:	(void) chmod (ptysloc, 0622);		/* not readable */
		if (mname != (char *) 0)
		    (void) strcpy(mname, ptymloc);
		if (sname != (char *) 0)
		    (void) strcpy(sname, ptysloc);
		return 0;				/* return OK	*/
	    }
	}

	/*	now, check old-style names	*/
	/*  the 1st name-space ptyp0-ptyof */
	/*	generate the master pty path	*/
	(void) strcpy(ptymloc, ptymdirs[loc]);
	(void) strcat(ptymloc, "ptyLN");
	mlen = strlen(ptymloc);

	/*	generate the slave pty path	*/
	(void) strcpy(ptysloc, ptysdirs[loc]);
	(void) strcat(ptysloc, "ttyLN");
	slen = strlen(ptysloc);

	for (ltr=0; oltrs[ltr] != '\0'; ltr++) {
	    ptymloc[mlen - 2] = oltrs[ltr];
	    ptymloc[mlen - 1] = '0';
	    if (stat(ptymloc, &stb))			/* no ptyL0 ... */
		continue;				/* try next ltr */

	    for (num=0; onums[num] != '\0'; num++) {
		ptymloc[mlen - 1] = onums[num];
		if ((*mfd=open(ptymloc,O_RDWR)) < 0)	/* no master	*/
		    continue;				/* try next num	*/
		
		ptysloc[slen - 2] = oltrs[ltr];
		ptysloc[slen - 1] = onums[num];

		/*
		**	NOTE:	changed to only stat the slave device; see
		**	comments all over the place about job control ...
		*/
		if (fstat(*mfd, &m_stbuf) < 0 || stat(ptysloc, &s_stbuf) < 0) {
		    close(*mfd);
		    continue;
		}
		/*
		**	sanity check: are the minor numbers the same??
		*/
		if (minor(m_stbuf.st_rdev) != minor(s_stbuf.st_rdev)) {
		    close(*mfd);  
		    continue;				/* try next num	*/
		}

		/*	else we got both a master and a slave pty	*/
		goto got_one;
	    }
	}
    }
    /* we failed in our search--we now try the slow brute-force method */
    for (loc=0; ptymdirs[loc] != (char *) 0; loc++) {
	DIR *dirp;
	struct dirent *dp;

	if ((dirp=opendir(ptymdirs[loc])) == NULL)	/* no directory ... */
	    continue;					/*  so try next one */

	(void) strcpy(ptymloc, ptymdirs[loc]);
	mlen = strlen(ptymloc);
	(void) strcpy(ptysloc, ptysdirs[loc]);
	slen = strlen(ptysloc);

	while ((dp=readdir(dirp)) != NULL) {
	    /* stat, open, go for it, else continue */
	    ptymloc[mlen] = '\0';
	    (void) strcat(ptymloc, dp->d_name);

	    if (stat(ptymloc, &m_stbuf) ||
	       (m_stbuf.st_mode & S_IFMT) != S_IFCHR ||
	       major(m_stbuf.st_rdev) != PTY_MASTER_MAJOR_NUMBER)
		continue;

	    if ((*mfd=open(ptymloc,O_RDWR)) < 0)	/* busy master	*/
	        continue;				/* try next entry */

	    ptysloc[slen] = '\0';       /* guess at corresponding slave name */
	    (void) strcat(ptysloc, dp->d_name);
	    if (ptysloc[slen] == 'p')
		ptysloc[slen] = 't';

	    if (stat(ptysloc, &s_stbuf) < 0 ||
		minor(m_stbuf.st_rdev) != minor(s_stbuf.st_rdev)) {
	        close(*mfd);
	        continue;
	    }
	    goto got_one;
	}

	closedir(dirp);
    }

    /*	we were not able to get the master/slave pty pair	*/
    return -1;		
}

namesp3(mfd,mname,sname,loc)
int     *mfd, loc;
char    *mname, *sname;
{
	int ltr, num, num2, num3;
    register int mlen, slen;

	/*	first, try the new naming convention	*/
	/*	generate the master pty path	*/
	(void) strcpy(ptymloc, ptymdirs[loc]);
	(void) strcat(ptymloc, "ptyLNNN");
	mlen = strlen(ptymloc);

	/*	generate the slave pty path	*/
	(void) strcpy(ptysloc, ptysdirs[loc]);
	(void) strcat(ptysloc, "ttyLNNN");
	slen = strlen(ptysloc);

	for (ltr=0; ltrs[ltr] != '\0'; ltr++) {
	    ptymloc[mlen - 4] = ltrs[ltr];
	    ptymloc[mlen - 3] = '0';
	    ptymloc[mlen - 2] = '0';
	    ptymloc[mlen - 1] = '0';
	    if (stat(ptymloc, &stb))			/* no ptyL00 ... */
		break;				/* go try old style names */

	    for (num=0; nums[num] != '\0'; num++)
	      for (num2=0; nums[num2] != '\0'; num2++) 
	        for (num3=0; nums[num3] != '\0'; num3++) {
		ptymloc[mlen - 3] = nums[num];
		ptymloc[mlen - 2] = nums[num2];
		ptymloc[mlen - 1] = nums[num3];
		if ((*mfd=open(ptymloc,O_RDWR)) < 0)	/* no master	*/
		    continue;				/* try next num	*/
		
		ptysloc[slen - 4] = ltrs[ltr];
		ptysloc[slen - 3] = nums[num];
		ptysloc[slen - 2] = nums[num2];
		ptysloc[slen - 1] = nums[num3];

		/*
		**	NOTE:	changed to only stat the slave device; see
		**	comments all over the place about job control ...
		*/
		if (fstat(*mfd, &m_stbuf) < 0 || stat(ptysloc, &s_stbuf) < 0) {
		    close(*mfd);
		    continue;
		}
		/*
		**	sanity check: are the minor numbers the same??
		*/
		if (minor(m_stbuf.st_rdev) != minor(s_stbuf.st_rdev)) {
		    close(*mfd);  
		    continue;				/* try next num	*/
		}

		/*	else we got both a master and a slave pty	*/
		(void) chmod (ptysloc, 0622);		/* not readable */
		if (mname != (char *) 0)
		    (void) strcpy(mname, ptymloc);
		if (sname != (char *) 0)
		    (void) strcpy(sname, ptysloc);
		return 0;				/* return OK	*/
	    }
	}
return 1;
}
#endif
