/*
 * Copyright (c) 1985, 1988 Regents of the University of California.
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
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * FTP server.
 */

#include <afs/param.h>
#ifdef	AFS_AIX32_ENV
#define	_CSECURITY 1		/* XXX */
#endif
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <dirent.h>
#ifdef _CSECURITY
/* Security New Stuff */
#include <sys/id.h>
#include <sys/priv.h>
#endif /* _CSECURITY */
#include <netinet/in.h>

#define	FTP_NAMES
#include "ftp.h"
#include <arpa/inet.h>
#ifndef AFS_HPUX_ENV
#include <arpa/telnet.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include <signal.h>
#include <pwd.h>
#include <setjmp.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#if defined(AIX)
#include <sys/syslog.h>
#else /* defined(AIX) */
#include <syslog.h>
#endif /* defined(AIX) */
#if     defined(AFS_SUN5_ENV) && !defined(_STDARG_H)
#include <varargs.h>
#endif
#if	defined(AFS_SUN53_ENV)
#include <shadow.h>
#include <security/ia_appl.h>
char *shadow_pass();
#endif /* AFS_SUN53_ENV */

#ifdef	AFS_AIX32_ENV
#include <usersec.h>
#include <userconf.h>

#ifdef _CSECURITY
#define tcpip_auditlog null
void
null()
{
    return;
}

#include "tcpip_audit.h"
#endif /* _CSECURITY */

#define MSGSTR(n,s) s

#ifdef _CSECURITY
int remote_addr;
char *audit_tail[TCPIP_MAX_TAIL_FIELDS];
uid_t saved_uid, effective_uid;
priv_t priv;

/* Restore privs and set the invoker back to uid saved_uid */
#define GET_PRIV(a)		\
        getpriv(PRIV_MAXIMUM, &priv,sizeof(priv_t)); \
        setpriv(PRIV_SET|PRIV_EFFECTIVE|PRIV_BEQUEATH, &priv,sizeof(priv_t)); \
        setuidx(ID_EFFECTIVE|ID_REAL|ID_SAVED, (a));

/* Drop privs and set the invoker to uid 100 */
#define DROP_PRIV(a) 		\
        priv.pv_priv[0] = 0; 	\
        priv.pv_priv[1] = 0; 	\
        setpriv(PRIV_SET|PRIV_EFFECTIVE|PRIV_BEQUEATH, &priv,sizeof(priv_t)); \
        setuidx(ID_EFFECTIVE|ID_REAL|ID_SAVED, (a));

#endif /* _CSECURITY */
#if defined(NLS) || defined(KJI)
#include <locale.h>
#endif

int tracing = 0;
#endif /* AFS_AIX32_ENV */

/*
 * File containing login names
 * NOT to be used on this machine.
 * Commonly used to disallow uucp.
 */
#define	FTPUSERS	"/etc/ftpusers"

#ifndef AFS_LINUX20_ENV
extern int errno;
extern char *sys_errlist[];
extern int sys_nerr;
#endif
extern char *crypt();
extern char version[];
extern char *home;		/* pointer to home directory for glob */
extern FILE *ftpd_popen(), *fopen(), *freopen();
extern int ftpd_pclose(), fclose();
extern char *getline();
extern char cbuf[];

struct sockaddr_in ctrl_addr;
struct sockaddr_in data_source;
struct sockaddr_in data_dest;
struct sockaddr_in his_addr;
struct sockaddr_in pasv_addr;

int data;
jmp_buf errcatch, urgcatch;
int logged_in;
struct passwd *pw;
int debug;
int timeout = 900;		/* timeout after 15 minutes of inactivity */
int maxtimeout = 7200;		/* don't allow idle time to be set beyond 2 hours */
int logging;
int guest;
int type;
int form;
int stru;			/* avoid C keyword */
int mode;
int usedefault = 1;		/* for data transfers */
int pdata = -1;			/* for passive mode */
int transflag;
off_t file_size;
off_t byte_count;
#if !defined(CMASK) || CMASK == 0
#undef CMASK
#define CMASK 027
#endif
int defumask = CMASK;		/* default umask value */
char tmpline[7];
char hostname[32];
char remotehost[32];

#ifndef	FD_SETSIZE
/* Yuck - we should have a AFS_BSD42 macro for these stuff */
typedef u_short uid_t;
typedef u_short gid_t;
#endif

/* disallow ftp PORT command to connect to a third host */
int allowPortAttack = 0;

/*
 * Timeout intervals for retrying connections
 * to hosts that don't accept PORT cmds.  This
 * is a kludge, but given the problems with TCP...
 */
#define	SWAITMAX	90	/* wait at most 90 seconds */
#define	SWAITINT	5	/* interval between retries */

int swaitmax = SWAITMAX;
int swaitint = SWAITINT;

void lostconn();
void myoob();
FILE *getdatasock(), *dataconn();

#ifdef	AFS_OSF_ENV
#include <sia.h>
#include <paths.h>
#define     _PATH_FTPUSERS  "/etc/ftpusers"
extern off_t restart_point;
int sia_argc;			/* SIA */
char **sia_argv;		/* SIA */
SIAENTITY *entity = NULL;	/* SIA */
#endif

#ifdef SETPROCTITLE
char **Argv = NULL;		/* pointer to argument vector */
char *LastArgv = NULL;		/* end of argv */
char proctitle[BUFSIZ];		/* initial part of title */
#endif /* SETPROCTITLE */

#if defined(AFS_HPUX_ENV)
/* HPUX uses a different call to set[res][gu]ids: */
#define seteuid(newEuid)	setresuid(-1, (newEuid), -1)
#define setegid(newEgid)	setresgid(-1, (newEgid), -1)
#endif /* defined(AFS_HPUX_ENV) */

#ifdef	AFS_AIX_ENV
#define setegid(newEgid)	setgid(newEgid)
#endif

main(argc, argv, envp)
     int argc;
     char *argv[];
     char **envp;
{
    int addrlen, on = 1;
    char *cp;
#ifdef	AFS_AIX32_ENV
    struct sigaction sa;
    void trace_handler();
    int pid;
#ifdef _CSECURITY
    char AllClass[] = "ALL\0";
#endif /* _CSECURITY */

#ifdef notdef
    /*
     *  If we're being called as a non-afs version of the program, and if AFS
     * extensions have been loaded, run the AFS version of the program.
     */
    check_and_run_afs_vers(argv);
#endif

#if defined(NLS) || defined(KJI)
    setlocale(LC_ALL, "");
#endif
    /* New Security Code */
    saved_uid = getuidx(ID_SAVED);
#ifdef MSG
    catd = NLcatopen(MF_FTPD, 0);
#endif
#ifdef _CSECURITY
    if ((auditproc(0, AUDIT_STATUS, AUDIT_SUSPEND, 0)) < 0) {
	syslog(LOG_ALERT, "ftpd: auditproc: %m");
	exit(1);
    }
    if (auditproc(0, AUDIT_EVENTS, AllClass, 5) < 0) {
	syslog(LOG_ALERT, "ftpd: auditproc: %m");
	exit(1);
    }
#endif /* _CSECURITY */
#endif /* AFS_AIX32_ENV */

#ifdef	AFS_OSF_ENV
    sia_argc = argc;		/* SIA */
    sia_argv = argv;		/* SIA */
#endif
    addrlen = sizeof(his_addr);
    if (getpeername(0, (struct sockaddr *)&his_addr, &addrlen) < 0) {
	syslog(LOG_ERR, "getpeername (%s): %m", argv[0]);
	exit(1);
    }
#ifdef _CSECURITY
    remote_addr = his_addr.sin_addr.s_addr;
#endif /* _CSECURITY */
    addrlen = sizeof(ctrl_addr);
    if (getsockname(0, (struct sockaddr *)&ctrl_addr, &addrlen) < 0) {
	syslog(LOG_ERR, "getsockname (%s): %m", argv[0]);
	exit(1);
    }
    data_source.sin_port = htons(ntohs(ctrl_addr.sin_port) - 1);
    debug = 0;
    openlog("ftpd", LOG_PID, LOG_DAEMON);
#ifdef SETPROCTITLE
    /*
     *  Save start and extent of argv for setproctitle.
     */
    Argv = argv;
    while (*envp)
	envp++;
    LastArgv = envp[-1] + strlen(envp[-1]);
#endif /* SETPROCTITLE */


    argc--, argv++;
    while (argc > 0 && *argv[0] == '-') {
	for (cp = &argv[0][1]; *cp; cp++)
	    switch (*cp) {
	    case 'd':
	    case 'v':
		debug = 1;
		break;

	    case 'l':
		logging = 1;
		break;

#ifdef	AFS_AIX32_ENV
	    case 's':
		tracing = 1;
		break;
#endif
	    case 't':
		timeout = atoi(++cp);
		if (maxtimeout < timeout)
		    maxtimeout = timeout;
		goto nextopt;

	    case 'T':
		maxtimeout = atoi(++cp);
		if (timeout > maxtimeout)
		    timeout = maxtimeout;
		goto nextopt;

	    case 'u':
		{
		    int val = 0;

		    while (*++cp && *cp >= '0' && *cp <= '9')
			val = val * 8 + *cp - '0';
		    if (*cp)
			fprintf(stderr, "ftpd: Bad value for -u\n");
		    else
			defumask = val;
		    goto nextopt;
		}

	    case 'z':		/* allow ftp PORT command to connect to a
				 ** third-party host */
		allowPortAttack = 1;
		break;

	    default:
		fprintf(stderr, "ftpd: Unknown flag -%c ignored.\n", *cp);
		break;
	    }
      nextopt:
	argc--, argv++;
    }
#ifdef	AFS_AIX32_ENV
    if (tracing
	&& setsockopt(0, SOL_SOCKET, SO_DEBUG, (char *)&on, sizeof(on)) < 0)
	syslog(LOG_ERR, MSGSTR(SOCKOPT, "setsockopt: %m"));

    /* set-up signal handler routines for SRC TRACE ON/OFF support */
    memset((char *)&sa, 0, sizeof(sa));
    sa.sa_mask.losigs = sigmask(SIGUSR2);
    sa.sa_handler = trace_handler;
    sigaction(SIGUSR1, &sa, NULL);
    sa.sa_mask.losigs = sigmask(SIGUSR1);
    sa.sa_handler = trace_handler;
    sigaction(SIGUSR2, &sa, NULL);
#endif /* AFS_AIX32_ENV */

#ifdef	AFS_OSF_ENV
    (void)freopen(_PATH_DEVNULL, "w", stderr);
#else
    (void)freopen("/dev/null", "w", stderr);
#endif
    (void)signal(SIGPIPE, lostconn);
    (void)signal(SIGCHLD, SIG_IGN);
    if ((int)signal(SIGURG, myoob) < 0)
	syslog(LOG_ERR, "signal: %m");

    /* handle urgent data inline */
    /* Sequent defines this, but it doesn't work */
#ifdef SO_OOBINLINE
    if (setsockopt(0, SOL_SOCKET, SO_OOBINLINE, (char *)&on, sizeof(on)) < 0)
	syslog(LOG_ERR, "setsockopt: %m");
#endif
#ifdef	AFS_AIX32_ENV
    pid = getpid();
    if (ioctl(fileno(stdin), SIOCSPGRP, (char *)&pid) == -1)
	syslog(LOG_ERR, MSGSTR(EIOCTL, "ioctl SIOCSPGRP: %m"));
#endif
#ifdef	F_SETOWN
    if (fcntl(fileno(stdin), F_SETOWN, getpid()) == -1)
	syslog(LOG_ERR, "fcntl F_SETOWN: %m");
#endif
    dolog(&his_addr);
    /*
     * Set up default state
     */
    data = -1;
    type = TYPE_A;
    form = FORM_N;
    stru = STRU_F;
    mode = MODE_S;
    tmpline[0] = '\0';
    (void)gethostname(hostname, sizeof(hostname));
    reply(220, "%s FTP server (%s) ready.", hostname, version);
    (void)setjmp(errcatch);
    for (;;)
	(void)yyparse();
    /* NOTREACHED */
}


#ifdef	AFS_AIX32_ENV
/*
 * trace_handler - SRC TRACE ON/OFF signal handler
 */
void
trace_handler(sig)
     int sig;
{
    int onoff;

    onoff = (sig == SIGUSR1) ? 1 : 0;
    if (setsockopt(0, SOL_SOCKET, SO_DEBUG, &onoff, sizeof(onoff)) < 0)
	syslog(LOG_ERR, MSGSTR(SOCKOPT, "setsockopt: %m"));
}
#endif


void
lostconn()
{

    if (debug)
	syslog(LOG_DEBUG, "lost connection");
#ifdef _CSECURITY
    GET_PRIV(saved_uid);
    CONNECTION_WRITE(remote_addr, "ftp/tcp", "close",
		     MSGSTR(LOSTCONN, "Lost connection"), -1);
    DROP_PRIV(effective_uid);
#endif /* _CSECURITY */
    dologout(-1);
}

static char ttyline[20];
#ifdef	AFS_OSF_ENV
extern void *malloc();
#else
extern char *malloc();
#endif
/*
 * Helper function for sgetpwnam().
 */
char *
sgetsave(s)
     char *s;
{
    char *new = malloc((unsigned)strlen(s) + 1);

    if (new == NULL) {
	perror_reply(421, "Local resource failure: malloc");
	dologout(1);
	/* NOTREACHED */
    }
    (void)strcpy(new, s);
    return (new);
}

/*
 * Save the result of a getpwnam.  Used for USER command, since
 * the data returned must not be clobbered by any other command
 * (e.g., globbing).
 */
struct passwd *
sgetpwnam(name)
     char *name;
{
    static struct passwd save;
    register struct passwd *p;
    struct passwd *getpwnam();
    char *sgetsave();

#ifdef	AFS_AIX32_ENV
    void getnonflatname();

    getnonflatname(name);
#endif
    if ((p = getpwnam(name)) == NULL)
	return (p);

    if (save.pw_name) {
	free(save.pw_name);
	free(save.pw_passwd);
	free(save.pw_gecos);
	free(save.pw_dir);
	free(save.pw_shell);
    }
    save = *p;
    save.pw_name = sgetsave(p->pw_name);
    save.pw_passwd = sgetsave(p->pw_passwd);
    save.pw_gecos = sgetsave(p->pw_gecos);
    save.pw_dir = sgetsave(p->pw_dir);
    save.pw_shell = sgetsave(p->pw_shell);
    return (&save);
}

#ifdef	AFS_AIX32_ENV
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
    struct passwd *pw;		/* return from getpwent() */
    char currflat[16];		/* name we are looking for */
    char dataflat[16];		/* name from data base */
    register int siz;
    register int members = 0;

#ifdef _KJI
    setpwent();
    while ((pw = getpwent()) != NULL) {
	/* Map NLS characters in name to ascii */
	NLflatstr(user, currflat, 16);
	NLflatstr(pw->pw_name, dataflat, 16);
	if (strcmp(currflat, dataflat) == 0) {
	    strcpy(user, pw->pw_name);
	    break;
	}
    }
    endpwent();
#endif
}
#endif

int login_attempts;		/* number of failed login attempts */
int askpasswd;			/* had user command, ask for passwd */

/*
 * USER command.
 * Sets global passwd pointer pw if named account exists
 * and is acceptable; sets askpasswd if a PASS command is
 * expected. If logged in previously, need to reset state.
 * If name is "ftp" or "anonymous" and ftp account exists,
 * set guest and pw, then just return.
 * If account doesn't exist, ask for passwd anyway.
 * Otherwise, check user requesting login privileges.
 * Disallow anyone who does not have a standard
 * shell returned by getusershell() (/etc/shells).
 * Disallow anyone mentioned in the file FTPUSERS
 * to allow people such as root and uucp to be avoided.
 */
user(name)
     char *name;
{
    register char *cp = NULL;
    FILE *fd;
    char *shell, *validshells = NULL;
    char line[BUFSIZ], *getusershell();


    if (logged_in) {
	if (guest) {
	    reply(530, "Can't change user from guest login.");
	    return;
	}
	end_login();
    }

    guest = 0;
    if (strcmp(name, "ftp") == 0 || strcmp(name, "anonymous") == 0) {
#ifdef	AFS_OSF_ENV
	if (checkuser("ftp") || checkuser("anonymous")) {
	    reply(530, "User %s access denied.", name);
	    if (logging)
		syslog(LOG_NOTICE, "FTP LOGIN REFUSED FROM %s, %s",
		       remotehost, name);
	} else
#endif
	if ((pw = sgetpwnam("ftp")) != NULL) {
	    guest = 1;
	    askpasswd = 1;
	    reply(331, "Guest login ok, send ident as password.");
	} else
	    reply(530, "User %s unknown.", name);
	return;
    }
    if (pw = sgetpwnam(name)) {
	if ((shell = pw->pw_shell) == NULL || *shell == 0)
	    shell = "/bin/sh";
#ifdef	AFS_AIX32_ENV
	if (getconfattr(SC_SYS_LOGIN, SC_SHELLS, &validshells, SEC_LIST)) {
	    reply(530, MSGSTR(DENIED, "User %s access denied."), name);
	    if (logging)
		syslog(LOG_NOTICE,
		       MSGSTR(GETCONFATTR,
			      "Getconfattr failed with error: %m"));
	    pw = (struct passwd *)NULL;
	    return;
	}
	while (*(cp = validshells) != NULL) {
	    if (strcmp(cp, shell) == 0)
		break;
	    else
		validshells += strlen(validshells) + 1;
	}
#else
	while ((cp = getusershell()) != NULL)
	    if (strcmp(cp, shell) == 0)
		break;
	endusershell();
#endif /* AFS_AIX32_ENV */
#ifdef	AFS_AIX32_ENV
	if (*cp == NULL) {
#else
#ifdef	AFS_OSF_ENV
	if (cp == NULL || checkuser(name)) {
#else
	if (cp == NULL) {
#endif
#endif
	    reply(530, "User %s access denied.", name);
	    if (logging)
		syslog(LOG_NOTICE, "FTP LOGIN REFUSED FROM %s, %s",
		       remotehost, name);
	    pw = (struct passwd *)NULL;
	    return;
	}
#ifndef	AFS_OSF_ENV
	if ((fd = fopen(FTPUSERS, "r")) != NULL) {
	    while (fgets(line, sizeof(line), fd) != NULL) {
		if ((cp = strchr(line, '\n')) != NULL)
		    *cp = '\0';
		if (strcmp(line, name) == 0) {
		    reply(530, "User %s access denied.", name);
		    if (logging)
			syslog(LOG_NOTICE, "FTP LOGIN REFUSED FROM %s, %s",
			       remotehost, name);
		    pw = (struct passwd *)NULL;
		    return;
		}
	    }
	}
	(void)fclose(fd);
#endif
    }
    reply(331, "Password required for %s.", name);
    askpasswd = 1;
    /*
     * Delay before reading passwd after first failed
     * attempt to slow down passwd-guessing programs.
     */
    if (login_attempts)
	sleep((unsigned)login_attempts);
}

#ifdef	AFS_OSF_ENV
/*
 * Check if a user is in the file _PATH_FTPUSERS
 */
checkuser(name)
     char *name;
{
    register FILE *fd;
    register char *p;
    char line[BUFSIZ];

    if ((fd = fopen(_PATH_FTPUSERS, "r")) != NULL) {
	while (fgets(line, sizeof(line), fd) != NULL) {
	    if ((p = strchr(line, '\n')) != NULL)
		*p = '\0';
	    if (line[0] == '#')
		continue;
	    if (strcmp(line, name) == 0)
		return (1);
	}
	(void)fclose(fd);
    }
    return (0);
}
#endif


/*
 * Terminate login as previous user, if any, resetting state;
 * used when USER command is given or login fails.
 */
end_login()
{

#ifdef	AFS_AIX32_ENV
/* New Security Code (void) seteuid(saved_uid); */
    GET_PRIV(saved_uid);
#else
    (void)seteuid((uid_t) 0);
#endif
    if (logged_in)
	logwtmp(ttyline, "", "");
    pw = NULL;
    logged_in = 0;
    guest = 0;
}

pass(passwd)
     char *passwd;
{
    char *xpasswd = 0, *salt;
    int afsauthok = 1;
    int limit;
#ifdef	AFS_OSF_ENV
    uid_t seuid;
#endif
#if	defined(AFS_SUN53_ENV)
    struct ia_status out;
    struct passwd *pwd;
    int val;
#endif
    if (logged_in || pw == NULL || askpasswd == 0) {
	reply(503, "Login with USER first.");
#ifdef _CSECURITY
	NET_ACCESS_WRITE(remote_addr, "File transfer", "",
			 MSGSTR(LOGINUSER, "Login with USER first."), -1);
#endif /* _CSECURITY */
	return;
    }
    askpasswd = 0;

    if (!guest) {		/* "ftp" is only account allowed no password */
	char *reason;
	int doafs = 0, co;

	if (pw == NULL)
	    salt = "xx";
	else
	    salt = pw->pw_passwd;
	setpag();		/* set a pag */
#ifdef	AFS_OSF_ENV
	/* SIA - sia session initialization */
	if ((co =
	     sia_ses_init(&entity, sia_argc, sia_argv, hostname, pw->pw_name,
			  (char *)NULL, FALSE, (char *)NULL)) != SIASUCCESS) {
	    reply(530, "Initialization failure");
	    return;
	}
	/* SIA - session authentication */
	if (!strcmp(pw->pw_passwd, "X")) {
	    doafs = 1;
	}

	if (!doafs
	    && (co = sia_ses_authent(NULL, passwd, entity)) != SIASUCCESS) {
	    if (!doafs && pw->pw_uid) {
		if (ka_UserAuthenticate
		    (pw->pw_name, /*inst */ "", /*realm */ 0,
		     passwd, /*sepag */ 1, &reason)) {
		    goto bad2;
		}
		goto good1;
	    }
	    syslog(LOG_ERR, "ses_authent failed=%d\n", co);
	  bad1:
	    reply(530, "Login incorrect. ");
	    pw = NULL;
	    if (logging)
		syslog(LOG_WARNING, "login %s from %s failed.", pw->pw_name,
		       remotehost);
	    if (login_attempts++ >= 5) {
		syslog(LOG_NOTICE, "repeated login failures from %s.",
		       remotehost);
		sia_ses_release(&entity);
		exit(0);
	    }
	    sia_ses_release(&entity);
	    return;
	}
	if (pw->pw_uid
	    && ka_UserAuthenticate(pw->pw_name, /*inst */ "", /*realm */ 0,
				   passwd, /*sepag */ 1, &reason)) {
	  bad2:
	    syslog(LOG_ERR, "Afs UserAuth failed\n");
	    xpasswd = crypt(passwd, salt);
	    afsauthok = 0;
	    /* The strcmp does not catch null passwords! */
	    if (*pw->pw_passwd == '\0' || strcmp(xpasswd, pw->pw_passwd)) {
		goto bad1;
	    }
	}

      good1:
	/* SIA - sia session establishment */
	if ((co = sia_ses_estab(NULL, entity)) != SIASUCCESS) {
	    syslog(LOG_ERR, "ses_estab failed=%d\n", co);
	    reply(530, "Login incorrect. ");
	    sia_ses_release(&entity);
	    return;
	}

	/* SIA - sia session launching */
	if ((co = sia_ses_launch(NULL, entity)) != SIASUCCESS) {
	    syslog(LOG_ERR, "ses_launch failed=%d\n", co);
	    reply(530, "Login incorrect. ");
	    sia_ses_release(&entity);
	    return;
	}
	/* SIA - get the password entry structure from entity structure */
	strncpy(pw->pw_name, entity->pwd->pw_name, sizeof(pw->pw_name));
	strncpy(pw->pw_passwd, entity->pwd->pw_passwd, sizeof(pw->pw_passwd));
	strncpy(pw->pw_gecos, entity->pwd->pw_gecos, sizeof(pw->pw_gecos));
	strncpy(pw->pw_dir, entity->pwd->pw_dir, sizeof(pw->pw_dir));
	strncpy(pw->pw_shell, entity->pwd->pw_shell, sizeof(pw->pw_shell));
    }
#else
	if (
#if	defined(AFS_AIX32_ENV) || defined(AFS_SUN53_ENV)
	       !pw->pw_uid ||
#else
#if defined(AFS_SUN5_ENV)
	       pw->pw_uid &&
#endif
#endif
	       ka_UserAuthenticate(pw->pw_name, /*inst */ "", /*realm */ 0,
				   passwd, /*sepag */ 1, &reason)) {
#if defined(AFS_SUN53_ENV)
	    pw->pw_passwd = sgetsave(shadow_pass(pw->pw_name));
	    salt = pw->pw_passwd;
#endif
	    if (passwd)
		xpasswd = crypt(passwd, salt);
	    afsauthok = 0;
	    /* The strcmp does not catch null passwords! */
	    if (pw == NULL || *pw->pw_passwd == '\0' || !xpasswd
		|| strcmp(xpasswd, pw->pw_passwd)) {
		reply(530, "Login incorrect");
		pw = NULL;
		if (login_attempts++ >= 5) {
		    syslog(LOG_NOTICE, "repeated login failures from %s",
			   remotehost);
		    exit(0);
		}
		return;
	    }
	}
    }

    (void)setegid((gid_t) pw->pw_gid);
    (void)initgroups(pw->pw_name, pw->pw_gid);

    /* open wtmp before chroot */
    (void)sprintf(ttyline, "%dftp", getpid());
    logwtmp(ttyline, pw->pw_name, remotehost);
#endif
    login_attempts = 0;		/* this time successful */
    logged_in = 1;

#ifdef	AFS_AIX32_ENV
    if (getuserattr(pw->pw_name, S_UFSIZE, &limit, SEC_INT) >= 0) {
	if (limit > 0)
	    ulimit(2, limit);
    }
#endif

    if (guest) {
#ifdef	AFS_OSF_ENV
	/* 
	 * SIA -ses_estab has set the euid to be that of
	 * guest.  Save this and then set the euid to be
	 * root.
	 */
	if ((seuid = geteuid()) < 0) {
	    reply(550, "Can't set guest privileges.");
	    goto bad;
	}
	if (seteuid((uid_t) 0) < 0) {
	    reply(550, "Can't set guest privileges.");
	    goto bad;
	}
#endif
	/*
	 * We MUST do a chdir() after the chroot. Otherwise
	 * the old current directory will be accessible as "."
	 * outside the new root!
	 */
	if (chroot(pw->pw_dir) < 0 || chdir("/") < 0) {
	    reply(550, "Can't set guest privileges.");
#ifdef	AFS_OSF_ENV
	    (void)seteuid(seuid);
#endif
	    goto bad;
	}
    } else {
#ifdef	AFS_OSF_ENV
	if (strcmp(pw->pw_dir, "/") == 0) {
#ifdef	notdef			/* ingore the complains for root users like "rootl" */
	    if (strcmp(pw->pw_name, "root"))
		lreply(230, "No directory! Logging in with home.");
#endif
	} else {
#endif

	    if (chdir(pw->pw_dir) < 0) {
		if (chdir("/") < 0) {
		    reply(530, "User %s: can't change directory to %s.",
			  pw->pw_name, pw->pw_dir);
		    goto bad;
		} else
		    lreply(230, "No directory! Logging in with home=/");
	    }
#ifdef	AFS_OSF_ENV
	}
#endif
    }


#ifdef	AFS_AIX32_ENV
/* New Security Code	if (seteuid((uid_t)pw->pw_uid) < 0) */
    effective_uid = pw->pw_uid;
    priv.pv_priv[0] = 0;
    priv.pv_priv[1] = 0;
    setpriv(PRIV_SET | PRIV_EFFECTIVE | PRIV_BEQUEATH, &priv, sizeof(priv_t));
    if (setuidx(ID_REAL | ID_EFFECTIVE | ID_SAVED, (uid_t) pw->pw_uid) < 0) {
	reply(550, MSGSTR(NOSEETUID, "Can't set uid."));
	goto bad;
    }
#else
#ifndef	AFS_OSF_ENV
    if (seteuid((uid_t) pw->pw_uid) < 0) {
	reply(550, "Can't set uid.");
	goto bad;
    }
#endif
#endif
    if (guest) {
	reply(230, "Guest login ok, access restrictions apply.");
#ifdef SETPROCTITLE
	sprintf(proctitle, "%s: anonymous/%.*s", remotehost,
		sizeof(proctitle) - sizeof(remotehost) -
		sizeof(": anonymous/"), passwd);
	setproctitle(proctitle);
#endif /* SETPROCTITLE */
	if (logging)
	    syslog(LOG_INFO, "ANONYMOUS FTP LOGIN FROM %s, %s", remotehost,
		   passwd);
    } else {
	if (afsauthok)
	    reply(230, "User %s logged in.", pw->pw_name);
	else
	    reply(230, "User %s logged in, no AFS authentication",
		  pw->pw_name);
#ifdef SETPROCTITLE
	sprintf(proctitle, "%s: %s", remotehost, pw->pw_name);
	setproctitle(proctitle);
#endif /* SETPROCTITLE */
	if (logging)
	    syslog(LOG_INFO, "FTP LOGIN FROM %s, %s", remotehost,
		   pw->pw_name);
    }
#ifdef _CSECURITY
    GET_PRIV(saved_uid);
    NET_ACCESS_WRITE(remote_addr, "File transfer", pw->pw_name, "", 0);
    DROP_PRIV(effective_uid);
#endif /* _CSECURITY */

    home = pw->pw_dir;		/* home dir for globbing */
    (void)umask(defumask);
#ifdef	AFS_OSF_ENV
    sia_ses_release(&entity);	/* SIA */
#endif
    return;
  bad:
    /* Forget all about it... */
#ifdef _CSECURITY
    NET_ACCESS_WRITE(remote_addr, "File transfer", "",
		     MSGSTR(LOGINERR, "Login incorrect."), -1);
#endif /* _CSECURITY */
    end_login();
#ifdef	AFS_OSF_ENV
    sia_ses_release(&entity);	/* SIA */
#endif
}

retrieve(cmd, name)
     char *cmd, *name;
{
    FILE *fin, *dout;
    struct stat st;
    int (*closefunc) ();
#ifdef _CSECURITY
    char *local, *remote;

    local = "";
    remote = name;
#endif /* _CSECURITY */
    if (cmd == 0) {
	fin = fopen(name, "r"), closefunc = fclose;
	st.st_size = 0;
    } else {
	char line[BUFSIZ];

	(void)sprintf(line, cmd, name), name = line;
	fin = ftpd_popen(line, "r"), closefunc = ftpd_pclose;
	st.st_size = -1;
	st.st_blksize = BUFSIZ;
    }
    if (fin == NULL) {
	if (errno != 0) {
	    perror_reply(550, name);
#ifdef _CSECURITY
	    GET_PRIV(saved_uid);
	    EXPORT_DATA_WRITE(remote_addr, local, remote, sys_errlist[errno],
			      errno);
	    DROP_PRIV(effective_uid);
	} else {
	    GET_PRIV(saved_uid);
	    EXPORT_DATA_WRITE(remote_addr, local, remote,
			      "Can't open remote file", -1);
	    DROP_PRIV(effective_uid);
#endif /* _CSECURITY */
	}
	return;
    }
    if (cmd == 0
	&& (fstat(fileno(fin), &st) < 0
	    || (st.st_mode & S_IFMT) != S_IFREG)) {
	reply(550, "%s: not a plain file.", name);
#ifdef _CSECURITY
	GET_PRIV(saved_uid);
	EXPORT_DATA_WRITE(remote_addr, local, remote,
			  "Remote file is not plain", -1);
	DROP_PRIV(effective_uid);
#endif /* _CSECURITY */
	goto done;
    }
#ifdef	AFS_OSF_ENV
    if (restart_point) {
	if (type == TYPE_A) {
	    register int i, n, c;

	    n = restart_point;
	    i = 0;
	    while (i++ < n) {
		if ((c = getc(fin)) == EOF) {
		    perror_reply(550, name);
		    goto done;
		}
		if (c == '\n')
		    i++;
	    }
	} else if (lseek(fileno(fin), restart_point, L_SET) < 0) {
	    perror_reply(550, name);
	    goto done;
	}
    }
#endif
    dout = dataconn(name, st.st_size, "w");
    if (dout == NULL) {
#ifdef _CSECURITY
	GET_PRIV(saved_uid);
	EXPORT_DATA_WRITE(remote_addr, local, remote,
			  "Data connection not established", -1);
	DROP_PRIV(effective_uid);
#endif /* _CSECURITY */
	goto done;
    }
    send_data(fin, dout, st.st_blksize);
#ifdef _CSECURITY
    GET_PRIV(saved_uid);
    EXPORT_DATA_WRITE(remote_addr, local, remote, "", 0);
    DROP_PRIV(effective_uid);
#endif /* _CSECURITY */
    (void)fclose(dout);
    data = -1;
    pdata = -1;
  done:
    (*closefunc) (fin);
}

store(name, mode, unique)
     char *name, *mode;
     int unique;
{
    FILE *fout, *din;
    struct stat st;
    int (*closefunc) ();
    char *gunique();
    int c;

#ifdef _CSECURITY
    char *remote;

    remote = "";
#endif /* _CSECURITY */

    if (unique && stat(name, &st) == 0 && (name = gunique(name)) == NULL)
	return;
#ifdef	AFS_OSF_ENV
    if (restart_point)
	mode = "r+w";
#endif
    fout = fopen(name, mode);
    closefunc = fclose;
    if (fout == NULL) {
	perror_reply(553, name);
#ifdef _CSECURITY
	GET_PRIV(saved_uid);
	IMPORT_DATA_WRITE(remote_addr, name, remote, sys_errlist[errno],
			  errno);
	DROP_PRIV(effective_uid);
#endif /* _CSECURITY */
	return;
    }
#ifdef	AFS_OSF_ENV
    if (restart_point) {
	if (type == TYPE_A) {
	    register int i, n, c;

	    n = restart_point;
	    i = 0;
	    while (i++ < n) {
		if ((c = getc(fout)) == EOF) {
		    perror_reply(550, name);
		    goto done;
		}
		if (c == '\n')
		    i++;
	    }
	    /*
	     * We must do this seek to "current" position
	     * because we are changing from reading to
	     * writing.
	     */
	    if (fseek(fout, 0L, L_INCR) < 0) {
		perror_reply(550, name);
		goto done;
	    }
	} else if (lseek(fileno(fout), restart_point, L_SET) < 0) {
	    perror_reply(550, name);
	    goto done;
	}
    }
#endif
    din = dataconn(name, (off_t) - 1, "r");
    if (din == NULL) {
#ifdef _CSECURITY
	GET_PRIV(saved_uid);
	IMPORT_DATA_WRITE(remote_addr, name, remote,
			  MSGSTR(NODATACONN, "Can't build data connection"),
			  -1);
	DROP_PRIV(effective_uid);
#endif /* _CSECURITY */
	goto done;
    }
    if (receive_data(din, fout) == 0) {
	/*
	 * This is AFS, so close() can return a meaningful error code
	 */
	c = (*closefunc) (fout);
	if (c) {
	    perror_reply(452, "Error writing file");
	    (void)fclose(din);
	    return;
	}
	fout = NULL;
	if (unique) {
	    reply(226, "Transfer complete (unique file name:%s).", name);
#ifdef _CSECURITY
	    GET_PRIV(saved_uid);
	    IMPORT_DATA_WRITE(remote_addr, name, remote, sys_errlist[errno],
			      errno);
	    DROP_PRIV(effective_uid);
#endif /* _CSECURITY */
	} else {
	    reply(226, "Transfer complete.");
#ifdef _CSECURITY
	    GET_PRIV(saved_uid);
	    IMPORT_DATA_WRITE(remote_addr, name, remote,
			      MSGSTR(TRANSOK, "Transfer complete."), 0);
	    DROP_PRIV(effective_uid);
#endif /* _CSECURITY */
	}
    }
    (void)fclose(din);
    data = -1;
    pdata = -1;
  done:
    if (fout)
	(*closefunc) (fout);
}

FILE *
getdatasock(mode)
     char *mode;
{
    int s, tries, on = 1;
    int bindretry = 0;

    if (data >= 0)
	return (fdopen(data, mode));
#ifdef	AFS_SUN5_ENV
    (void)seteuid(0);
#endif
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
#ifndef	AFS_SUN5_ENV
	(void)seteuid(pw->pw_uid);
#endif
	return (NULL);
    }
#ifdef	AFS_AIX32_ENV
/* New Security Code (void) seteuid(saved_uid); */
    GET_PRIV(saved_uid);
#else
#ifndef	AFS_SUN5_ENV
    (void)seteuid((uid_t) 0);
#endif

#endif
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
	goto bad;
    }
    /* anchor socket to avoid multi-homing problems */
    data_source.sin_family = AF_INET;
    data_source.sin_addr = ctrl_addr.sin_addr;
#if 	defined(AFS_AIX32_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_OSF_ENV)
    while (bind(s, (struct sockaddr *)&data_source, sizeof(data_source)) < 0) {
	if (bindretry > 5) {
	    goto bad;
	}
#if	defined(AFS_SUN5_ENV) || defined(AFS_OSF_ENV)
	if (errno == EADDRINUSE) {
	    sleep(1);
	} else
	    goto bad;
#endif
	bindretry++;
	sleep(bindretry);
    }
/* New Security Code (void) seteuid((uid_t)pw->pw_uid); */
#if	defined(AFS_SUN5_ENV) || defined(AFS_OSF_ENV)
    (void)seteuid((uid_t) pw->pw_uid);
#else
    DROP_PRIV(effective_uid);
#endif
#else
    if (bind(s, (struct sockaddr *)&data_source, sizeof(data_source)) < 0) {
	goto bad;
    }
    (void)seteuid((uid_t) pw->pw_uid);
#endif
    return (fdopen(s, mode));
  bad:
#ifdef	AFS_AIX32_ENV
/* New Security Code (void) seteuid((uid_t)pw->pw_uid); */
    DROP_PRIV(effective_uid);
#else
    (void)seteuid((uid_t) pw->pw_uid);
#endif
    (void)close(s);
    return (NULL);
}

FILE *
dataconn(name, size, mode)
     char *name;
     off_t size;
     char *mode;
{
    char sizebuf[32];
    FILE *file;
    int retry = 0;
#ifdef _CSECURITY
    char portbuf[8];
#endif /* _CSECURITY */

    file_size = size;
    byte_count = 0;
    if (size != (off_t) - 1)
	(void)sprintf(sizebuf, " (%ld bytes)", size);
    else
	(void)strcpy(sizebuf, "");
    if (pdata >= 0) {
	struct sockaddr_in from;
	int s, fromlen = sizeof(from);

	s = accept(pdata, (struct sockaddr *)&from, &fromlen);
	if (s < 0) {
	    reply(425, "Can't open data connection.");
	    (void)close(pdata);
	    pdata = -1;
#ifdef _CSECURITY
	    GET_PRIV(saved_uid);
	    CONNECTION_WRITE(remote_addr, "", "open",
			     MSGSTR(NOCONN, "Can't open data connection."),
			     errno);
	    DROP_PRIV(effective_uid);
#endif /* _CSECURITY */
	    return (NULL);
	}
	(void)close(pdata);
	pdata = s;
	reply(150, "Opening %s mode data connection for %s%s.",
	      type == TYPE_A ? "ASCII" : "BINARY", name, sizebuf);
#ifdef _CSECURITY
	sprintf(portbuf, "%d", ntohs(from.sin_port));
	GET_PRIV(saved_uid);
	CONNECTION_WRITE(from.sin_addr, portbuf, "open", "", 0);
	DROP_PRIV(effective_uid);
#endif /* _CSECURITY */
	return (fdopen(pdata, mode));
    }
    if (data >= 0) {
	reply(125, "Using existing data connection for %s%s.", name, sizebuf);
	usedefault = 1;
	return (fdopen(data, mode));
    }
    if (usedefault)
	data_dest = his_addr;
    usedefault = 1;
    file = getdatasock(mode);
    if (file == NULL) {
	reply(425, "Can't create data socket (%s,%d): %s.",
	      inet_ntoa(data_source.sin_addr), ntohs(data_source.sin_port),
	      errno < sys_nerr ? sys_errlist[errno] : "unknown error");
#ifdef _CSECURITY
	sprintf(portbuf, "%d", ntohs(data_source.sin_port));
	GET_PRIV(saved_uid);
	CONNECTION_WRITE(data_source.sin_addr, portbuf, "open",
			 "Can't create data socket", -1);
	DROP_PRIV(effective_uid);
#endif /* _CSECURITY */
	return (NULL);
    }
    data = fileno(file);
    while (connect(data, (struct sockaddr *)&data_dest, sizeof(data_dest)) <
	   0) {
	if (errno == EADDRINUSE && retry < swaitmax) {
	    sleep((unsigned)swaitint);
	    retry += swaitint;
	    continue;
	}
	perror_reply(425, "Can't build data connection");
	(void)fclose(file);
	data = -1;
#ifdef _CSECURITY
	sprintf(portbuf, "%d", ntohs(data_dest.sin_port));
	GET_PRIV(saved_uid);
	CONNECTION_WRITE(data_dest.sin_addr, portbuf, "open",
			 MSGSTR(NODATACONN, "Can't build data connection"),
			 errno);
	DROP_PRIV(effective_uid);
#endif /* _CSECURITY */
	return (NULL);
    }
    reply(150, "Opening %s mode data connection for %s%s.",
	  type == TYPE_A ? "ASCII" : "BINARY", name, sizebuf);
#ifdef _CSECURITY
    sprintf(portbuf, "%d", ntohs(data_dest.sin_port));
    GET_PRIV(saved_uid);
    CONNECTION_WRITE(data_dest.sin_addr, portbuf, "open", "", 0);
    DROP_PRIV(effective_uid);
#endif /* _CSECURITY */
    return (file);
}

/*
 * Tranfer the contents of "instr" to
 * "outstr" peer using the appropriate
 * encapsulation of the data subject
 * to Mode, Structure, and Type.
 *
 * NB: Form isn't handled.
 */
send_data(instr, outstr, blksize)
     FILE *instr, *outstr;
     off_t blksize;
{
    register int c, cnt;
    register char *buf;
    int netfd, filefd;

    transflag++;
    if (setjmp(urgcatch)) {
	transflag = 0;
	return;
    }
    switch (type) {

    case TYPE_A:
	while ((c = getc(instr)) != EOF) {
	    byte_count++;
	    if (c == '\n') {
		if (ferror(outstr))
		    goto data_err;
		(void)putc('\r', outstr);
	    }
	    (void)putc(c, outstr);
	    if (ferror(outstr))
		goto data_err;
	}
	fflush(outstr);
	transflag = 0;
	if (ferror(instr))
	    goto file_err;
	if (ferror(outstr))
	    goto data_err;
	reply(226, "Transfer complete.");
	return;

    case TYPE_I:
    case TYPE_L:
	if ((buf = malloc((u_int) blksize)) == NULL) {
	    transflag = 0;
	    perror_reply(451, "Local resource failure: malloc");
	    return;
	}
	netfd = fileno(outstr);
	filefd = fileno(instr);
	while ((cnt = read(filefd, buf, (u_int) blksize)) > 0
	       && write(netfd, buf, cnt) == cnt)
	    byte_count += cnt;
	transflag = 0;
	(void)free(buf);
	if (cnt != 0) {
	    if (cnt < 0)
		goto file_err;
	    goto data_err;
	}
	reply(226, "Transfer complete.");
	return;
    default:
	transflag = 0;
	reply(550, "Unimplemented TYPE %d in send_data", type);
	return;
    }

  data_err:
    transflag = 0;
    perror_reply(426, "Data connection");
    return;

  file_err:
    transflag = 0;
    perror_reply(551, "Error on input file");
}

/*
 * Transfer data from peer to
 * "outstr" using the appropriate
 * encapulation of the data subject
 * to Mode, Structure, and Type.
 *
 * N.B.: Form isn't handled.
 */
receive_data(instr, outstr)
     FILE *instr, *outstr;
{
    register int c;
    int cnt, d;
    char buf[BUFSIZ], *bufp;

    transflag++;
    if (setjmp(urgcatch)) {
	transflag = 0;
	return (-1);
    }
    switch (type) {

    case TYPE_I:
    case TYPE_L:
	errno = d = 0;
	while ((cnt = read(fileno(instr), buf, sizeof buf)) > 0) {
#ifdef	AFS_AIX32_ENV
	    byte_count += cnt;
	    for (bufp = buf; cnt > 0; cnt -= d, bufp += d) {
		if ((d = write(fileno(outstr), bufp, cnt)) <= 0)
		    goto file_err;
	    }
#else
	    if (write(fileno(outstr), buf, cnt) != cnt)
		goto file_err;
	    byte_count += cnt;
#endif
	}
	if (cnt < 0)
	    goto data_err;
	transflag = 0;
	return (0);

    case TYPE_E:
	reply(553, "TYPE E not implemented.");
	transflag = 0;
	return (-1);

    case TYPE_A:
	while ((c = getc(instr)) != EOF) {
	    byte_count++;
	    while (c == '\r') {
		if (ferror(outstr))
		    goto data_err;
		if ((c = getc(instr)) != '\n') {
		    (void)putc('\r', outstr);
		    if (c == '\0' || c == EOF)
			goto contin2;
		}
	    }
	    (void)putc(c, outstr);
	  contin2:;
	}
	fflush(outstr);
	if (ferror(instr))
	    goto data_err;
	if (ferror(outstr))
	    goto file_err;
	transflag = 0;
	return (0);
    default:
	reply(550, "Unimplemented TYPE %d in receive_data", type);
	transflag = 0;
	return (-1);
    }

  data_err:
    transflag = 0;
    perror_reply(426, "Data Connection");
    return (-1);

  file_err:
    transflag = 0;
    perror_reply(452, "Error writing file");
    return (-1);
}

statfilecmd(filename)
     char *filename;
{
    char line[BUFSIZ];
    FILE *fin;
    int c;

    (void)sprintf(line, "/bin/ls -lgA %s", filename);
    fin = ftpd_popen(line, "r");
    lreply(211, "status of %s:", filename);
    while ((c = getc(fin)) != EOF) {
	if (c == '\n') {
	    if (ferror(stdout)) {
		perror_reply(421, "control connection");
		(void)ftpd_pclose(fin);
		dologout(1);
		/* NOTREACHED */
	    }
	    if (ferror(fin)) {
		perror_reply(551, filename);
		(void)ftpd_pclose(fin);
		return;
	    }
	    (void)putc('\r', stdout);
	}
	(void)putc(c, stdout);
    }
    (void)ftpd_pclose(fin);
    reply(211, "End of Status");
}

statcmd()
{
    struct sockaddr_in *sin;
    u_char *a, *p;

    lreply(211, "%s FTP server status:", hostname, version);
    printf("     %s\r\n", version);
    printf("     Connected to %s", remotehost);
    if (isdigit(remotehost[0]))
	printf(" (%s)", inet_ntoa(his_addr.sin_addr));
    printf("\r\n");
    if (logged_in) {
	if (guest)
	    printf("     Logged in anonymously\r\n");
	else
	    printf("     Logged in as %s\r\n", pw->pw_name);
    } else if (askpasswd)
	printf("     Waiting for password\r\n");
    else
	printf("     Waiting for user name\r\n");
    printf("     TYPE: %s", typenames[type]);
    if (type == TYPE_A || type == TYPE_E)
	printf(", FORM: %s", formnames[form]);
    if (type == TYPE_L)
#if NBBY == 8
	printf(" %d", NBBY);
#else
	printf(" %d", bytesize);	/* need definition! */
#endif
    printf("; STRUcture: %s; transfer MODE: %s\r\n", strunames[stru],
	   modenames[mode]);
    if (data != -1)
	printf("     Data connection open\r\n");
    else if (pdata != -1) {
	printf("     in Passive mode");
	sin = &pasv_addr;
	goto printaddr;
    } else if (usedefault == 0) {
	printf("     PORT");
	sin = &data_dest;
      printaddr:
	a = (u_char *) & sin->sin_addr;
	p = (u_char *) & sin->sin_port;
#define UC(b) (((int) b) & 0xff)
	printf(" (%d,%d,%d,%d,%d,%d)\r\n", UC(a[0]), UC(a[1]), UC(a[2]),
	       UC(a[3]), UC(p[0]), UC(p[1]));
#undef UC
    } else
	printf("     No data connection\r\n");
    reply(211, "End of status");
}

fatal(s)
     char *s;
{
    reply(451, "Error in server: %s\n", s);
    reply(221, "Closing connection due to server error.");
#ifdef _CSECURITY
    GET_PRIV(saved_uid);
    CONNECTION_WRITE(remote_addr, "ftp/tcp", "close",
		     MSGSTR(CLOSECONN,
			    "Closing connection due to server error."), -1);
    DROP_PRIV(effective_uid);
#endif /* _CSECURITY */
    dologout(0);
    /* NOTREACHED */
}

/* VARARGS2 */
reply(n, fmt, p0, p1, p2, p3, p4, p5)
     int n;
     long p0, p1, p2, p3, p4, p5;
     char *fmt;
{
    printf("%d ", n);
    printf(fmt, p0, p1, p2, p3, p4, p5);
    printf("\r\n");
    (void)fflush(stdout);
    if (debug) {
	syslog(LOG_DEBUG, "<--- %d ", n);
	syslog(LOG_DEBUG, fmt, p0, p1, p2, p3, p4, p5);
    }
}

/* VARARGS2 */
lreply(n, fmt, p0, p1, p2, p3, p4, p5)
     int n;
     long p0, p1, p2, p3, p4, p5;
     char *fmt;
{
    printf("%d- ", n);
    printf(fmt, p0, p1, p2, p3, p4, p5);
    printf("\r\n");
    (void)fflush(stdout);
    if (debug) {
	syslog(LOG_DEBUG, "<--- %d- ", n);
	syslog(LOG_DEBUG, fmt, p0, p1, p2, p3, p4, p5);
    }
}

ack(s)
     char *s;
{
    reply(250, "%s command successful.", s);
}

nack(s)
     char *s;
{
    reply(502, "%s command not implemented.", s);
}

/* ARGSUSED */
yyerror(s)
     char *s;
{
    char *cp;

    if (cp = strchr(cbuf, '\n'))
	*cp = '\0';
    reply(500, "'%s': command not understood.", cbuf);
}

delete(name)
     char *name;
{
    struct stat st;

    if (stat(name, &st) < 0) {
	perror_reply(550, name);
	return;
    }
    if ((st.st_mode & S_IFMT) == S_IFDIR) {
	if (rmdir(name) < 0) {
	    perror_reply(550, name);
	    return;
	}
	goto done;
    }
    if (unlink(name) < 0) {
	perror_reply(550, name);
	return;
    }
  done:
    ack("DELE");
}

cwd(path)
     char *path;
{
    if (chdir(path) < 0)
	perror_reply(550, path);
    else
	ack("CWD");
}

makedir(name)
     char *name;
{
    if (mkdir(name, 0777) < 0)
	perror_reply(550, name);
    else
	reply(257, "MKD command successful.");
}

removedir(name)
     char *name;
{
    if (rmdir(name) < 0)
	perror_reply(550, name);
    else
	ack("RMD");
}

pwd()
{
    char path[MAXPATHLEN + 1];
#if 0/*ndef HAVE_GETCWD*/	/* XXX enable when autoconf happens */
    extern char *getwd();
#define getcwd(x,y) getwd(x)
#endif
    if (getcwd(path, MAXPATHLEN + 1) == (char *)NULL)
	reply(550, "%s.", path);
    else
	reply(257, "\"%s\" is current directory.", path);
}

char *
renamefrom(name)
     char *name;
{
    struct stat st;

    if (stat(name, &st) < 0) {
	perror_reply(550, name);
	return (NULL);
    }
    reply(350, "File exists, ready for destination name");
    return (name);
}

renamecmd(from, to)
     char *from, *to;
{
    if (rename(from, to) < 0)
	perror_reply(550, "rename");
    else
	ack("RNTO");
}

dolog(sin)
     struct sockaddr_in *sin;
{
    struct hostent *hp = gethostbyaddr((char *)&sin->sin_addr,
				       sizeof(struct in_addr), AF_INET);
    time_t t;

    if (hp)
	(void)strncpy(remotehost, hp->h_name, sizeof(remotehost));
    else
	(void)strncpy(remotehost, inet_ntoa(sin->sin_addr),
		      sizeof(remotehost));
#ifdef SETPROCTITLE
    sprintf(proctitle, "%s: connected", remotehost);
    setproctitle(proctitle);
#endif /* SETPROCTITLE */

    if (logging) {
	t = time((time_t *) 0);
	syslog(LOG_INFO, "connection from %s at %s", remotehost, ctime(&t));
    }
}

/*
 * Record logout in wtmp file
 * and exit with supplied status.
 */
dologout(status)
     int status;
{
    /*
     * Prevent reception of SIGURG from resulting in a resumption
     * back to the main program loop.
     */
    transflag = 0;

    if (logged_in) {
	(void)seteuid((uid_t) 0);
	logwtmp(ttyline, "", "");
    }
    /* beware of flushing buffers after a SIGPIPE */
    ktc_ForgetAllTokens();
    _exit(status);
}

void
myoob()
{
    char *cp;

    /* only process if transfer occurring */
    if (!transflag)
	return;
    cp = tmpline;
    if (getline(cp, 7, stdin) == NULL) {
	reply(221, "You could at least say goodbye.");
#ifdef _CSECURITY
	GET_PRIV(saved_uid);
	CONNECTION_WRITE(remote_addr, "ftp/tcp", "close",
			 MSGSTR(GOODBYE, "You could at least say goodbye."),
			 -1);
	DROP_PRIV(effective_uid);
#endif /* _CSECURITY */
	dologout(0);
    }
    upper(cp);
    if (strcmp(cp, "ABOR\r\n") == 0) {
	tmpline[0] = '\0';
	reply(426, "Transfer aborted. Data connection closed.");
	reply(226, "Abort successful");
	longjmp(urgcatch, 1);
    }
    if (strcmp(cp, "STAT\r\n") == 0) {
	if (file_size != (off_t) - 1)
	    reply(213, "Status: %lu of %lu bytes transferred", byte_count,
		  file_size);
	else
	    reply(213, "Status: %lu bytes transferred", byte_count);
    }
}

/*
 * Note: a response of 425 is not mentioned as a possible response to
 * 	the PASV command in RFC959. However, it has been blessed as
 * 	a legitimate response by Jon Postel in a telephone conversation
 *	with Rick Adams on 25 Jan 89.
 */
passive()
{
    int len;
    register char *p, *a;

    pdata = socket(AF_INET, SOCK_STREAM, 0);
    if (pdata < 0) {
	perror_reply(425, "Can't open passive connection");
	return;
    }
    pasv_addr = ctrl_addr;
    pasv_addr.sin_port = 0;
#ifdef	AFS_AIX32_ENV
/* New Security Code (void) seteuid((uid_t)0); */
    GET_PRIV(saved_uid);
    if (bind(pdata, (struct sockaddr *)&pasv_addr, sizeof(pasv_addr)) < 0) {
/* New Security Code (void) seteuid((uid_t)pw->pw_uid); */
	DROP_PRIV(effective_uid);
	goto pasv_error;
    }
/* New Security Code (void) seteuid((uid_t)pw->pw_uid); */
    DROP_PRIV(effective_uid);
#else
    (void)seteuid((uid_t) 0);
    if (bind(pdata, (struct sockaddr *)&pasv_addr, sizeof(pasv_addr)) < 0) {
	(void)seteuid((uid_t) pw->pw_uid);
	goto pasv_error;
    }
    (void)seteuid((uid_t) pw->pw_uid);
#endif
    len = sizeof(pasv_addr);
    if (getsockname(pdata, (struct sockaddr *)&pasv_addr, &len) < 0)
	goto pasv_error;
    if (listen(pdata, 1) < 0)
	goto pasv_error;
    a = (char *)&pasv_addr.sin_addr;
    p = (char *)&pasv_addr.sin_port;

#define UC(b) (((int) b) & 0xff)

    reply(227, "Entering Passive Mode (%d,%d,%d,%d,%d,%d)", UC(a[0]),
	  UC(a[1]), UC(a[2]), UC(a[3]), UC(p[0]), UC(p[1]));
    return;

  pasv_error:
    (void)close(pdata);
    pdata = -1;
    perror_reply(425, "Can't open passive connection");
    return;
}

/*
 * Generate unique name for file with basename "local".
 * The file named "local" is already known to exist.
 * Generates failure reply on error.
 */
char *
gunique(local)
     char *local;
{
    static char new[MAXPATHLEN];
    struct stat st;
    char *cp = strrchr(local, '/');
    int count = 0;

    if (cp)
	*cp = '\0';
    if (stat(cp ? local : ".", &st) < 0) {
	perror_reply(553, cp ? local : ".");
	return (NULL);
    }
    if (cp)
	*cp = '/';
    (void)strcpy(new, local);
    cp = new + strlen(new);
    *cp++ = '.';
    for (count = 1; count < 100; count++) {
	(void)sprintf(cp, "%d", count);
	if (stat(new, &st) < 0)
	    return (new);
    }
    reply(452, "Unique file name cannot be created.");
    return (NULL);
}

/*
 * Format and send reply containing system error number.
 */
perror_reply(code, string)
     int code;
     char *string;
{
    if (errno < sys_nerr)
	reply(code, "%s: %s.", string, sys_errlist[errno]);
    else
	reply(code, "%s: unknown error %d.", string, errno);
}

static char *onefile[] = {
    "",
    0
};

send_file_list(whichfiles)
     char *whichfiles;
{
    struct stat st;
    DIR *dirp = NULL;
    struct dirent *dir;
    FILE *dout = NULL;
    register char **dirlist, *dirname;
#ifndef AFS_LINUX22_ENV
    char *strpbrk();
#endif


    if (strpbrk(whichfiles, "~{[*?") != NULL) {
	extern char **glob(), *globerr;

	globerr = NULL;
	dirlist = glob(whichfiles);
	if (globerr != NULL) {
	    reply(550, globerr);
	    return;
	} else if (dirlist == NULL) {
	    errno = ENOENT;
	    perror_reply(550, whichfiles);
	    return;
	}
    } else {
	onefile[0] = whichfiles;
	dirlist = onefile;
    }

    if (setjmp(urgcatch)) {
	transflag = 0;
	return;
    }
    while (dirname = *dirlist++) {
	if (stat(dirname, &st) < 0) {
	    /*
	     * If user typed "ls -l", etc, and the client
	     * used NLST, do what the user meant.
	     */
	    if (dirname[0] == '-' && *dirlist == NULL && transflag == 0) {
		retrieve("/bin/ls %s", dirname);
		return;
	    }
	    perror_reply(550, whichfiles);
	    if (dout != NULL) {
		(void)fclose(dout);
		transflag = 0;
		data = -1;
		pdata = -1;
	    }
	    return;
	}

	if ((st.st_mode & S_IFMT) == S_IFREG) {
	    if (dout == NULL) {
		dout = dataconn(whichfiles, (off_t) - 1, "w");
		if (dout == NULL)
		    return;
		transflag++;
	    }
	    fprintf(dout, "%s\n", dirname);
	    byte_count += strlen(dirname) + 1;
	    continue;
	} else if ((st.st_mode & S_IFMT) != S_IFDIR)
	    continue;

	if ((dirp = opendir(dirname)) == NULL)
	    continue;

	while ((dir = readdir(dirp)) != NULL) {
	    char nbuf[MAXPATHLEN];

	    if (dir->d_name[0] == '.' && dir->d_name[1] == 0)
		continue;
	    if (dir->d_name[0] == '.' && dir->d_name[1] == '.'
		&& dir->d_name[2] == 0)
		continue;

	    sprintf(nbuf, "%s/%s", dirname, dir->d_name);

	    /*
	     * We have to do a stat to insure it's
	     * not a directory or special file.
	     */
	    if (stat(nbuf, &st) == 0 && (st.st_mode & S_IFMT) == S_IFREG) {
		if (dout == NULL) {
		    dout = dataconn(whichfiles, (off_t) - 1, "w");
		    if (dout == NULL)
			return;
		    transflag++;
		}
		if (nbuf[0] == '.' && nbuf[1] == '/')
		    fprintf(dout, "%s\n", &nbuf[2]);
		else
		    fprintf(dout, "%s\n", nbuf);
		byte_count += strlen(nbuf) + 1;
	    }
	}
	(void)closedir(dirp);
    }

    if (dout == NULL)
	reply(550, "No files found.");
    else if (ferror(dout) != 0)
	perror_reply(550, "Data connection");
    else
	reply(226, "Transfer complete.");

    transflag = 0;
    if (dout != NULL)
	(void)fclose(dout);
    data = -1;
    pdata = -1;
}

#ifdef SETPROCTITLE
/*
 * clobber argv so ps will show what we're doing.
 * (stolen from sendmail)
 * warning, since this is usually started from inetd.conf, it
 * often doesn't have much of an environment or arglist to overwrite.
 */

/*VARARGS1*/
setproctitle(fmt, a, b, c)
     char *fmt;
     long a, b, c;
{
    register char *p, *bp, ch;
    register int i;
    char buf[BUFSIZ];

    (void)sprintf(buf, fmt, a, b, c);

    /* make ps print our process name */
    p = Argv[0];
    *p++ = '-';

    i = strlen(buf);
    if (i > LastArgv - p - 2) {
	i = LastArgv - p - 2;
	buf[i] = '\0';
    }
    bp = buf;
    while (ch = *bp++)
	if (ch != '\n' && ch != '\r')
	    *p++ = ch;
    while (p < LastArgv)
	*p++ = ' ';
}
#endif /* SETPROCTITLE */

#if	defined(AFS_SUN53_ENV)
char *
shadow_pass(name)
     char *name;
{
    struct spwd *sp;
    static char *Nothing = "";
    int oldeuid;

    oldeuid = geteuid();
    if (oldeuid && oldeuid != -1)
	seteuid(0);
    if ((sp = getspnam(name)) == NULL) {
	if (oldeuid && oldeuid != -1)
	    seteuid(oldeuid);
	return Nothing;
    }
    if (oldeuid && oldeuid != -1)
	seteuid(oldeuid);
    return (sp->sp_pwdp);
}

#endif
