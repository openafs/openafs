#ifndef lint
#endif

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)rexecd.c	5.4 (Berkeley) 5/9/86";
#endif /* not lint */

#include <afs/param.h>
#include <afs/kautils.h>	/* for UserAuthGeneral */
#include <sys/types.h>
#ifdef	AFS_SUN5_ENV
#define BSD_COMP
#endif
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <netdb.h>
#ifdef	AFS_SUN5_ENV
#include <shadow.h>
#endif
#if defined(AFS_AIX_ENV)
#include <sys/syslog.h>
#include <usersec.h>
#endif

extern	errno;
struct	passwd *getpwnam();
char	*crypt(), *rindex(), *strncat();
#if	!defined(AFS_AIX_ENV) && !defined(AFS_HPUX_ENV) && !defined(AFS_OSF_ENV) && !defined(AFS_SUN5_ENV)
char *sprintf();
#endif
/*VARARGS1*/
int	error();

#include "AFS_component_version_number.c"

#ifdef	AFS_OSF_ENV
#include <sia.h>
SIAENTITY *entity=NULL;
int oargc;
char **oargv;
#endif

/*
 * remote execute server:
 *	username\0
 *	password\0
 *	command\0
 *	data
 */
/*ARGSUSED*/
main(argc, argv)
	int argc;
	char **argv;
{
	struct sockaddr_in from;
	int fromlen;

	fromlen = sizeof (from);
	if (getpeername(0, (struct sockaddr *)&from, &fromlen) < 0) {
		fprintf(stderr, "%s: ", argv[0]);
		perror("getpeername");
		exit(1);
	}
#ifdef	AFS_OSF_ENV
	oargc = argc;
	oargv = argv;
#endif
	doit(0, &from);
}

char	env_user[20] = "USER=";
char	env_home[64] = "HOME=";
char	env_shell[64] = "SHELL=";
char    env_logname[32] = "LOGNAME=";
char    env_password_expires[64] = "PASSWORD_EXPIRES=";
afs_int32 password_expires = -1;
char pwd_expires_str[10];
/* make sure env_password_expires is always the last item in envinit array */
#if	defined(AFS_OSF_ENV) || defined(AFS_AIX_ENV)
char	time_zone[20] = "TZ=";
char	*envinit[] =  {env_home, env_shell, "PATH=:/usr/ucb:/bin:/usr/bin:/usr/sbin", env_user, time_zone, env_logname, env_password_expires, 0};
#else
char	*envinit[] =  {env_home, env_shell, "PATH=:/usr/ucb:/bin:/usr/bin", env_user, env_logname, env_password_expires, 0};
#endif
#define PATHENV ":/usr/ucb:/bin:/usr/bin:/usr/bin/X11"
extern char	**environ;

struct	sockaddr_in asin = { AF_INET };

doit(f, fromp)
	int f;
	struct sockaddr_in *fromp;
{
	char cmdbuf[NCARGS+1], *cp, *namep;
	char user[16], pass[16];
	struct passwd *pwd;
	char *password;
	char **penvlist;
	int s;
	short port;
	int pv[2], pid, cc;
	fd_set ready, readfrom;
	char buf[BUFSIZ], sig;
	int one = 1;
	int afsauthok = 1;
#ifdef	AFS_SUN5_ENV
	struct spwd *shpwd;
#endif

	(void) signal(SIGINT, SIG_DFL);
	(void) signal(SIGQUIT, SIG_DFL);
	(void) signal(SIGTERM, SIG_DFL);
#ifdef DEBUG
	{ int t = open("/dev/tty", 2);
	  if (t >= 0) {
		ioctl(t, TIOCNOTTY, (char *)0);
		(void) close(t);
	  }
	}
#endif
	dup2(f, 0);
	dup2(f, 1);
	dup2(f, 2);
	(void) alarm(60);
	port = 0;
	for (;;) {
		char c;
		if (read(f, &c, 1) != 1)
			exit(1);
		if (c == 0)
			break;
		port = port * 10 + c - '0';
	}
	(void) alarm(0);
	if (port != 0) {
		s = socket(AF_INET, SOCK_STREAM, 0);
		if (s < 0)
			exit(1);
		if (bind(s, (struct sockaddr *) &asin, sizeof (asin)) < 0)
			exit(1);
		(void) alarm(60);
		fromp->sin_port = htons((u_short)port);
		if (connect(s, (struct sockaddr *) fromp, sizeof (*fromp)) < 0)
			exit(1);
		(void) alarm(0);
	}
	getstr(user, sizeof(user), "username");
	getstr(pass, sizeof(pass), "password");
	getstr(cmdbuf, sizeof(cmdbuf), "command");
	setpwent();
	pwd = getpwnam(user);
#ifdef	AFS_SUN5_ENV
	(void) setspent();	/* Shadow password file */
	shpwd = getspnam(user);
	if (pwd == NULL || shpwd == NULL) {
#else
	if (pwd == NULL) {
#endif
		error("Login Incorrect..\n");
		exit(1);
	}
	endpwent();
#ifdef	AFS_SUN5_ENV
	endspent();
	password = shpwd->sp_pwdp;
#else
	password = pwd->pw_passwd;
#endif
	if (*password != '\0') {
	    char *reason;

	    setpag();	/* Also set a pag */
	    /*
	     * If it's called as "root" we don't bother with afs authentication...
	     */
	    if (strcmp(user, "root")) {
		/* changed from ka_UserAuthenticate *
		 * to get password_expires information */
                if(ka_UserAuthenticateGeneral(
                           KA_USERAUTH_VERSION + KA_USERAUTH_DOSETPAG,
                           user, /* kerberos name */
                           (char *)0, /* instance */
                           (char *)0, /* realm */
                            pass, /* password */
                            0, /* default lifetime */
                            &password_expires,
                            0, /* spare 2 */
                            &reason /* error string */
                            )) {

			afsauthok = 0;
		}
	    } else
		afsauthok = 0;
	    /*
	      if (strcmp(user, "root") &&
	      strcmp(password, NO_VICE_AUTH_PWD)) {
	      rc = U_Authenticate(user, pass, &cToken, &sToken);
	      } else
	      rc = AUTH_FAILED;
	      */
	    namep = crypt(pass, password);
	    if (strcmp(namep, password) && !afsauthok) {
		error("Password Incorrect..\n");
		exit(1);
	    }
	}
	if (chdir(pwd->pw_dir) < 0) {
		error("No remote directory.\n");
		exit(1);
	}
	(void) write(2, "\0", 1);
	if (port) {
		(void) pipe(pv);
		pid = fork();
		if (pid == (int)-1)  {
			error("Try again.\n");
			exit(1);
		}
		if (pid) {
			(void) close(0); (void) close(1); (void) close(2);
			(void) close(f); (void) close(pv[1]);
			FD_ZERO(&readfrom);
			FD_SET(s, &readfrom);
			FD_SET(pv[0], &readfrom);
			ioctl(pv[1], FIONBIO, (char *)&one);
			/* should set s nbio! */
			for (;;) {
				int maxfd;
				maxfd = -1;
				if (FD_ISSET(s, &readfrom) && maxfd < s)
					maxfd = s;
				if (FD_ISSET(pv[0], &readfrom) && maxfd < pv[0])
					maxfd = pv[0];
				if (maxfd == -1)
					break;
				ready = readfrom;
				(void) select(maxfd+1, &ready, (fd_set *)0,
				    (fd_set *)0, (struct timeval *)0);
				if (FD_ISSET(s, &ready)) {
					if (read(s, &sig, 1) <= 0)
						FD_CLR(s, &readfrom);
					else
						killpg(pid, sig);
				}
				if (FD_ISSET(pv[0], &ready)) {
					cc = read(pv[0], buf, sizeof (buf));
					if (cc <= 0) {
						shutdown(s, 1+1);
						FD_CLR(pv[0], &readfrom);
					} else
						(void) write(s, buf, cc);
				}
			}
			exit(0);
		}
		setpgid(0, getpid());
		(void) close(s); (void)close(pv[0]);
		dup2(pv[1], 2);
	}
	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = "/bin/sh";
	if (f > 2)
		(void) close(f);

#ifdef AFS_AIX_ENV
        /* For 7403 - try setting ulimit here */
        /*
         * Need to set user's ulimit, else system default
         */
        if (setpcred(pwd->pw_name, NULL) < 0) {
              error("Can't set user credentials.\n");
	      exit(1);
        }
#endif /* AFS_AIX_ENV */

	/*
	 * For both setgid() and setuid() we should have checked for errors but we ignore them since
	 * they may fail in afs...
	 */
	(void) setgid((gid_t)pwd->pw_gid);
	initgroups(pwd->pw_name, pwd->pw_gid);
	(void) setuid((uid_t)pwd->pw_uid);
#ifndef	AFS_AIX_ENV
	environ = envinit;
	strncat(env_home, pwd->pw_dir, sizeof(env_home)-6);
	strncat(env_shell, pwd->pw_shell, sizeof(env_shell)-7);
	strncat(env_user, pwd->pw_name, sizeof(env_user)-6);
	strncat(env_logname, pwd->pw_name, sizeof(env_logname)-9);
	/* Determine if password expiration is used
	 * if not, take out env_password_expires string from envinit */
	if ((password_expires >= 0)  && (password_expires < 255)) {
		sprintf(env_password_expires,"%s%d", env_password_expires, password_expires);
	} else {
		/* taking out PASSWORD_EXPIRES env var from array */
#if	defined(AFS_OSF_ENV)
		envinit[6] = 0;
#else
		envinit[5] = 0;
#endif /* AFS_OSF_ENV */
	}
#else
	penvlist = getpenv(PENV_USR); 
	environ = penvlist +1 ;
	addenvvar("HOME", pwd->pw_dir);
	addenvvar("SHELL", pwd->pw_shell);
	addenvvar("PATH", PATHENV);
	addenvvar("LOGNAME", pwd->pw_name);
	addenvvar("USER", pwd->pw_name);
	/* Determine if password expiration is used
         * if not, take out env_password_expires string from envinit */
        if ((password_expires >= 0)  && (password_expires < 255)) {
		sprintf(pwd_expires_str,"%d", password_expires);
		addenvvar("PASSWORD_EXPIRES", pwd_expires_str);
	} 
#endif
	cp = rindex(pwd->pw_shell, '/');
	if (cp)
		cp++;
	else
		cp = pwd->pw_shell;
	execl(pwd->pw_shell, cp, "-c", cmdbuf, 0);
	perror(pwd->pw_shell);
	exit(1);
}

#ifdef	AFS_AIX_ENV
addenvvar(tag,value)
  char *tag,*value;
{
    char    *penv;
    unsigned int len = strlen(tag)+1;       /* allow for '=' */
 
    if (!tag) {
	errno = EINVAL;
	perror("rexecd, addenvvar");
	exit(1);
    }
    if (value) len+= strlen(value);
    penv = malloc(len+1);
    strcpy(penv,tag);
    strcat(penv,"=");
    if (value) strcat(penv,value);
    if (putenv(penv)<0) {
	perror("rexecd, putenv");
	exit(1);
    }
    return;
}
#endif


/*VARARGS1*/
error(fmt, a1, a2, a3)
	char *fmt;
	int a1, a2, a3;
{
	char buf[BUFSIZ];

	buf[0] = 1;
	(void) sprintf(buf+1, fmt, a1, a2, a3);
	(void) write(2, buf, strlen(buf));
}

getstr(buf, cnt, err)
	char *buf;
	int cnt;
	char *err;
{
	char c;

	do {
		if (read(0, &c, 1) != 1)
			exit(1);
		*buf++ = c;
		if (--cnt == 0) {
			error("%s too long\n", (int)err,0,0);
			exit(1);
		}
	} while (c != 0);
}
