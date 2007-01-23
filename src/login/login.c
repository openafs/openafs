/*
 * Notes:
 *
 * This is an AFS-aware login.
 *
 * Machine/os specific code is surrounded by #ifdef <system>
 *
 * The Q_SETUID and Q_DOWARN #ifdefs are for IBM's AOS which doesn't
 * define them if NFS is configured. Like that matters anymore
 */

/*
 * Copyright (c) 1980, 1987, 1988 The Regents of the University of California.
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

/*
 * login [ name ]
 * login -h hostname	(for telnetd, etc.)
 * login -f name	(for pre-authenticated login: datakit, xterm, etc.)
 */

#if !defined(UT_NAMESIZE)
#define UT_NAMESIZE 8
#endif

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <afs/kautils.h>
#ifdef	AFS_SUN5_ENV
#include <sys/fs/ufs_quota.h>
#else
#if  !defined(AFS_HPUX_ENV) && !defined(AFS_AIX_ENV)
#if defined(AFS_SUN_ENV) || (defined(AFS_ATHENA_STDENV) && !defined(AFS_DEC_ENV)) || defined(AFS_OSF_ENV)
#include <ufs/quota.h>
#elif defined(AFS_FBSD_ENV)
#include <ufs/ufs/quota.h>
#else
#include <sys/quota.h>
#endif
#endif /* AIX */
#endif /* SUN5 */
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#ifdef	AFS_SUN5_ENV
#include <sys/termios.h>
#include <sys/ttychars.h>
/* hack to get it to build */
#define	CBRK	0377
#include <sys/ttydev.h>
#include <sys/ttold.h>
#include <sys/filio.h>
#endif
#ifdef AFS_FBSD_ENV
#define USE_OLD_TTY 1
#include <sys/ttydefaults.h>
#endif
#include <sys/ioctl.h>

#include <utmp.h>
#ifdef	AFS_SUN5_ENV
#include <utmpx.h>
#include <dirent.h>
#endif
#include <signal.h>
#if !defined(AIX) && !defined(AFS_HPUX_ENV) && !defined(AFS_AIX32_ENV) && !defined(AFS_FBSD_ENV)
#include <lastlog.h>
#endif
#include <errno.h>
#if defined(AIX)
#include <stand.h>
#undef B300
#undef B1200
#undef B9600
#include <sys/termio.h>
#include <sys/syslog.h>
#include <fcntl.h>
#else
#if !defined(AFS_HPUX_ENV) && !defined(AFS_SUN5_ENV)
#include <ttyent.h>
#endif
#if defined(AFS_HPUX_ENV)
#include <sgtty.h>
#include <sys/bsdtty.h>
#endif
#include <syslog.h>
#endif
#include <grp.h>
#include <pwd.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define	TTYGRPNAME	"tty"	/* name of group to own ttys */

#define	MOTDFILE	"/etc/motd"

#if defined(AFS_SUN5_ENV) || defined(AFS_HPUX100_ENV)
#ifndef NO_MAIL
#ifdef V4FS
#define MAILDIR         "/var/mail/"
#else
#define MAILDIR         "/usr/mail/"
#endif /* V4FS */
static char mail[30];
#endif /* NO_MAIL */
#else /* AFS_SUN5_ENV || AFS_HPUX100_ENV */
#define MAILDIR         "/usr/spool/mail"
#endif /* AFS_SUN5_ENV || AFS_HPUX100_ENV */

#define	NOLOGIN		"/etc/nologin"
#define	HUSHLOGIN	".hushlogin"
#define	LASTLOG		"/usr/adm/lastlog"
#define	BSHELL		"/bin/sh"
#define SECURETTY_FILE	"/etc/securetty"

/*
 * This bounds the time given to login.  Not a define so it can
 * be patched on machines where it's too small.
 */
int timeout = 300;

struct passwd *pwd, *getpwnam();
int failures;
char term[64], *hostname, *username, *tty;
#ifdef	AFS_OSF_ENV
char ousername[256];
#endif
struct sgttyb sgttyb;

#ifdef	AFS_SUN5_ENV
#include <deflt.h>
extern char *defread(), *strdup();
static void defaults();
static char *Pndefault = "/etc/default/login";
static char *Altshell = NULL;
static char *Console = NULL;
static int Idleweeks = -1;
static char *Passreq = NULL;
#define	DEFUMASK	022
static mode_t Umask = DEFUMASK;
static char *Def_tz = NULL;
static char *tmp_tz = NULL;
static char *Def_hertz = NULL;
#define	SET_FSIZ	2	/* ulimit() command arg */
static long Def_ulimit = 0;
#define	MAX_TIMEOUT	(15 * 60)
#define	DEF_TIMEOUT	5*60
static unsigned Def_timeout = DEF_TIMEOUT;
static char *Def_path = NULL;
static char *Def_supath = NULL;
#define	DEF_PATH	"/usr/bin:"	/* same as PATH */
#define	DEF_SUPATH	"/usr/sbin:/usr/bin"	/* same as ROOTPATH */
/*
 * Intervals to sleep after failed login
 */
#ifndef	SLEEPTIME
#	define	SLEEPTIME 4	/* sleeptime before login incorrect msg */
#endif
static int Sleeptime = SLEEPTIME;

#include <shadow.h>
/* This is for Solaris's shadow pass struc */
static struct spwd *spwd;
char *ttypmt = NULL;
char *d_tz = NULL;
char *d_dev = NULL;
static int authenticated_remote_user = 0;
static char rhost[64], rusername[10], lusername[10];
#endif
#ifdef	AFS_SUN_ENV
int usererr = -1;
#endif

/* For setting of TZ environment on HPUX */
#ifdef AFS_HPUX_ENV
#define MAX_TZ_LEN 256
static char TZ[MAX_TZ_LEN];
#endif /* AFS_HPUX_ENV */

#ifdef AFS_KERBEROS_ENV
extern char *ktc_tkt_string();
#endif

#if !defined(AIX) && !defined(AFS_HPUX_ENV)
struct tchars tc = {
/*	CINTR, CQUIT, CSTART, CSTOP, 000, 000 */
    CINTR, CQUIT, CSTART, CSTOP, CEOT, CBRK
};

struct ltchars ltc = {
    CSUSP, CDSUSP, CRPRNT, CFLUSH, CWERASE, CLNEXT
};
#endif

#ifdef	AFS_OSF_ENV
#include <sia.h>
#include <paths.h>
SIAENTITY *entity = NULL;

#ifdef	AFS_OSF20_ENV
#include <sys/security.h>
#endif
#include <prot.h>
#endif

#ifndef	NOPAG
#define	NOPAG	0xffffffff
#endif

static void
timedout(x)
{
    fprintf(stderr, "Login timed out after %d seconds\n", timeout);
    exit(0);
}

char *
cv2string(ttp, aval)
     register char *ttp;
     register unsigned long aval;
{
    register char *tp = ttp;
    register int i;
    int any = 0;

    *(--tp) = 0;
    while (aval != 0) {
	i = aval % 10;
	*(--tp) = '0' + i;
	aval /= 10;
	any = 1;
    }
    if (!any)
	*(--tp) = '0';
    return tp;
}

#include "AFS_component_version_number.c"


#ifdef	AFS_SUN5_ENV
struct utmpx utmp;
#else
struct utmp utmp;
#endif

#if	defined(AFS_SUN5_ENV) || defined(AFS_HPUX_ENV)
/* the following utmp field is used for user counting: */
#define how_to_count ut_exit.e_exit
char *ttyntail;
#define SUBLOGIN	"<!sublogin>"
int sublogin;
main(argc, argv, renvp)
     char **renvp;
#else
main(argc, argv)
#endif
     int argc;
     char **argv;
{
    extern int errno, optind;
    extern char *optarg, **environ;
    struct group *gr, *getgrnam();
    register int ch;
    register char *p;
    int ask, fflag, hflag, rflag, pflag, cnt, locl, zero = 0, dflag =
	0, rlog = 0;
    int quietlog, passwd_req, ioctlval;
    char *domain, *salt, *envinit[2], *ttyn = 0, *pp;
    char tbuf[MAXPATHLEN + 2];
    char *ttyname(), *stypeof(), *crypt(), *getpass();
    extern off_t lseek();
    int local, code = 0;
    char *lcell;		/* local cellname */
    char realm[MAXKTCREALMLEN];
    char buf1[256], **envp;
    afs_int32 j, pagval = NOPAG, ngroups, groups[NGROUPS_MAX];
    long password_expires = -1;
    extern char *AFSVersion;	/* generated version */
    int afsLoginFail = 0;
    char *reason;

#if defined(AFS_SUN5_ENV) || defined(AFS_HPUX_ENV)
    char retry_chdir = 0;
#endif /* AFS_SUN5_ENV || AFS_HPUX_ENV */

#ifdef	AFS_OSF_ENV
    char tname[sizeof(_PATH_TTY) + 10], *loginname = NULL;
    int oargc = argc;
    char **oargv = argv;

#else
#ifdef	AFS_SUN5_ENV

    defaults();			/* Set up Defaults and flags */
    if (Umask < 0 || ((mode_t) 0777) < Umask)	/* Set up default umask */
	Umask = DEFUMASK;
    (void)umask(Umask);

    if (Def_timeout > MAX_TIMEOUT)	/* Set up default timeouts and delays */
	Def_timeout = MAX_TIMEOUT;
    if (Sleeptime < 0 || Sleeptime > 5)
	Sleeptime = SLEEPTIME;
    (void)alarm(Def_timeout);
#else
    (void)alarm((u_int) timeout);
#endif
    (void)signal(SIGALRM, timedout);
#endif
    (void)signal(SIGQUIT, SIG_IGN);
    (void)signal(SIGINT, SIG_IGN);
#if !defined(AIX) && !defined(AFS_HPUX_ENV)
    (void)setpriority(PRIO_PROCESS, 0, 0);
#endif
#ifdef Q_SETUID
    (void)quota(Q_SETUID, 0, 0, 0);
#endif /* Q_SETUID */

    /*
     * -p is used by getty to tell login not to destroy the environment
     * -f is used to skip a second login authentication 
     * -h is used by other servers to pass the name of the remote
     *    host to login so that it may be placed in utmp and wtmp
     */
    (void)gethostname(tbuf, sizeof(tbuf));
    domain = strchr(tbuf, '.');

#ifdef	AFS_HPUX_ENV
    /*
     * Set flag to disable the pid check if you find that you are
     * a subsystem login.
     */
    sublogin = 0;
    if (*renvp && strcmp(*renvp, SUBLOGIN) == 0)
	sublogin = 1;
#endif
    fflag = hflag = pflag = rflag = 0;
    memset(&utmp, '\0', sizeof(utmp));
    passwd_req = 1;
#ifdef	AFS_SUN_ENV
#ifdef	AFS_SUN5_ENV
    while ((ch = getopt(argc, argv, "fh:prd")) != -1) {
#else
    while ((ch = getopt(argc, argv, "fh:pr")) != -1) {
#endif
#else
    while ((ch = getopt(argc, argv, "fh:p")) != EOF) {
#endif
/*printf("ch='%c'\n", ch);*/
	switch (ch) {
	case 'f':
	    fflag = 1;
	    break;
	case 'h':
#ifdef	AFS_SUN_ENV
	    if (hflag || rflag) {
		fprintf(stderr, "Only one of -r and -h allowed\n");
		exit(1);
	    }
#endif
	    if (getuid()) {
		fprintf(stderr, "login: -h for super-user only.\n");

		exit(1);
	    }
	    rlog = 1;
	    hflag = 1;
	    if (domain && (p = strchr(optarg, '.'))
		&& strcasecmp(p, domain) == 0)
		*p = 0;
	    hostname = optarg;
	    break;
#ifdef	AFS_SUN_ENV
	case 'r':
	    if (hflag || rflag) {
		fprintf(stderr, "Only one of -r and -h allowed\n");
		exit(1);
	    }
	    if (argv[2] == 0)
		exit(1);
#ifndef	AFS_SUN5_ENV
/*			fflag = 1;*/
#endif
	    rflag = 1;
#ifndef	AFS_SUN5_ENV
	    usererr = doremotelogin(argv[2]);
	    strncpy(utmp.ut_host, argv[2], sizeof(utmp.ut_host));
	    argc -= 2;
	    argv += 2;
#endif
	    continue;

	    break;
#ifdef	AFS_SUN5_ENV
	case 'd':		/* Ignode the '-d device' option for now!!! */
	    dflag = 0;		/* XXX */
	    d_dev = *++argv;
	    argc--;
	    break;
#endif
#endif
	case 'p':
	    pflag = 1;
	    break;
	case '?':
	default:
	    fprintf(stderr, "usage: login [-fp] [username]\n");
	    exit(1);
	}
    }
    argc -= optind;
    argv += optind;
#ifdef	AFS_SUN5_ENV
    if (rflag)
	strcpy(rhost, *argv++);
    if (hflag) {
	if (argv[0] && (*argv[0] != '-')) {
	    if ((!strncmp(*argv, "TERM=", strlen("TERM=")))) {
		strncpy(term, &((*argv)[strlen("TERM=")]), sizeof(term));
	    }
	}
	ask = 1;
    } else
#endif
#if	defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV)
    if (!rflag && *argv) {
#else
    if (*argv) {
#endif
#ifdef	AFS_OSF_ENV
	memset(ousername, '\0', sizeof(ousername));
	strcpy(ousername, *argv);
	loginname = ousername;
#else
	username = *argv;
#endif
	ask = 0;
    } else {
#ifdef	AFS_SUN5_ENV
	ttypmt = getenv("TTYPROMPT");
	if (ttypmt && (*ttypmt != '\0')) {
	    extern char **getargs();
	    /*
	     * if ttyprompt is set, there should be data on the stream already. 
	     */
	    if ((envp = getargs(buf1)) != (char **)NULL) {
		username = *envp;
	    }
	}
	ask = 0;
#else
#ifdef	AFS_SUN_ENV
	if (rflag)
	    ask = 0;
	else
#endif
	    ask = 1;
#endif
    }
    ioctlval = 0;
#if !defined(AIX) && !defined(AFS_HPUX_ENV) && !defined(AFS_OSF_ENV)
    (void)ioctl(0, TIOCLSET, &ioctlval);

    (void)ioctl(0, TIOCNXCL, 0);

    (void)fcntl(0, F_SETFL, ioctlval);
    (void)ioctl(0, TIOCGETP, &sgttyb);
#ifdef	AFS_SUN_ENV
    (void)ioctl(0, FIONBIO, &zero);
    (void)ioctl(0, FIOASYNC, &zero);
    (void)ioctl(0, TIOCLGET, &locl);
    /*
     * If talking to an rlogin process,
     * propagate the terminal type and
     * baud rate across the network.
     */
#ifdef	AFS_SUN5_ENV
    if (rflag) {
	doremotelogin();
#else
    if (rflag || rlog) {
	doremoteterm(term, &sgttyb);
#endif
    }
    locl &= LPASS8;
    locl |= LCRTBS | LCTLECH | LDECCTQ;
    if (sgttyb.sg_ospeed >= B1200)
	locl |= LCRTERA | LCRTKIL;
    (void)ioctl(0, TIOCLSET, &locl);
#endif
    sgttyb.sg_erase = CERASE;
    sgttyb.sg_kill = CKILL;
    (void)ioctl(0, TIOCSLTC, &ltc);
    (void)ioctl(0, TIOCSETC, &tc);
#ifdef	AFS_SUN5_ENV
    if (!rflag)
#endif
	(void)ioctl(0, TIOCSETP, &sgttyb);
#endif /* AIX  */
    for (cnt = getdtablesize(); cnt > 2; cnt--)
	close(cnt);
#ifdef	AFS_SUN52_ENV
    if (dflag) {
	ttyn = d_dev;
    } else
#endif
    if (ttyn == NULL) {
	ttyn = ttyname(0);
	if (ttyn == NULL || *ttyn == '\0') {
#ifdef	AFS_OSF_ENV
	    (void)sprintf(tname, "%s??", _PATH_TTY);
	    ttyn = tname;
#else
	    ttyn = "/dev/tty??";
#endif

	}
    }
#ifdef	AFS_HPUX_ENV
    ttyntail = ttyn + sizeof("/dev/") - 1;
#endif
    if (tty = strrchr(ttyn, '/'))
	++tty;
    else
	tty = ttyn;

    openlog("login", LOG_ODELAY, LOG_AUTH);

#ifdef	AFS_OSF_ENV
#define _PATH_HUSHLOGIN ".hushlogin"
#define _PATH_MAILDIR   "/var/spool/mail"
#ifndef _PATH_DEFPATH
#define _PATH_DEFPATH   "/usr/bin:."
#endif
    {
	int cnt;
	int authret = 0, doafs = 0;
	char shell[PATH_MAX + 1] = _PATH_BSHELL, *pass, user[40], ouser[40];
	int (*sia_collect) () = sia_collect_trm;
	int getloginp(), uid;
	struct passwd *pwd;

/*printf("loginname=%s,  ttyn=%s, fflag=%d\n", loginname, ttyn, fflag);*/
	quietlog = 0;
	if ((sia_ses_init
	     (&entity, oargc, oargv, hostname, loginname, ttyn, 1,
	      NULL)) == SIASUCCESS) {
	    static int useGetPass = 0;
	    int cnt1 = 0, first = 0;
	    char *prompt, *reason, pword[BUFSIZ];
	    /***** SIA SESSION AUTHENTICATION *****/
	    if (!fflag) {
		for (cnt = 5; cnt; cnt--) {
		    doafs = 0;
		    if (first++ || !entity->name) {
			clean_entity_pwd(entity);
			if (getloginp(entity) < 0) {
			    printf("Illegal login name\n");
			    doafs = -1;
			    break;
			}
		    }
		    strcpy(user, entity->name);
		    prompt = "Password:";
		    if ((pwd = getpwnam(user))) {
			if (!strcmp(pwd->pw_passwd, "X")) {
			    doafs = 1;
			}
		    }
		    if (useGetPass) {
			pass = getpass(prompt);
		    } else {
			code =
			    ka_UserReadPassword(prompt, pword, sizeof(pword),
						&reason);
			pass = pword;
			if (code) {
			    if (code != KANULLPASSWORD) {
				if (++cnt1 > 3)
				    useGetPass = 1;
			    }
			    fprintf(stderr,
				    "Unable to read password because %s\n",
				    reason);
			    continue;
			}
		    }

		    if (!pwd) {
			printf("Login incorrect\n");
			fflush(stdout);
			continue;
		    }

		    if (doafs
			||
			((authret =
			  sia_ses_authent(sia_collect, pass,
					  entity)) == SIASUCCESS)) {
			if (doafs) {
			    uid = pwd->pw_uid;
			    sia_make_entity_pwd(pwd, entity);
			} else {
			    uid = entity->pwd->pw_uid;
			    pwd = entity->pwd;
			}
		      doafs:
			authret = do_afs_auth(pwd, pass, doafs);
			if (!authret) {
			    strcpy(ouser, user);
			    printf("Login incorrect\n");
			    fflush(stdout);
			    continue;
			}

			printf("AFS (R) %s Login\n", AFSVersion);

			if (uid) {
			    groups[0] = groups[1] = 0;
			    ngroups = getgroups(NGROUPS, groups);
			    pagval =
				get_pag_from_groups(groups[0], groups[1]);
			}
			break;
		    } else if (authret & SIASTOP) {
			break;
		    } else if (authret == SIAFAIL) {
			/* why did it fail ? */
			struct ttyent *ptr;
			setttyent();
			/* 
			 ** if the "ptys" command does not exist in the
			 ** /etc/securettys file, then root login's are
			 ** allowed only from the console
			 */
			if (!pwd->pw_uid)	/* for root */
			    if (((ptr = getttynam("ptys")) == NULL)
				|| !(ptr->ty_status & TTY_SECURE)) {
				printf
				    ("root login refused on this terminal\n");
				continue;
			    }
		    }
		    if (doafs || (authret != SIASUCCESS)) {
			uid = pwd->pw_uid;
			sia_make_entity_pwd(pwd, entity);
			goto doafs;
		    }
		    printf("login incorrect\n");
		}
		if (cnt <= 0 || (doafs == -1)) {
		    sia_ses_release(&entity);
		    exit(1);
		}
	    }
	    /***** SIA SESSION ESTABLISHMENT *****/
	    if (sia_ses_estab(sia_collect, entity) == SIASUCCESS) {
		/*
		 * Display and update lastlog file entry.  -DAL003
		 */
		quietlog = access(_PATH_HUSHLOGIN, F_OK) == 0;
		if (!quietlog)
		    quietlog = !*entity->pwd->pw_passwd
			&& !usershell(entity->pwd->pw_shell);
		if (!entity->tty)
		    dolastlog(quietlog);
		/* END DAL003 */
#ifdef AFS_KERBEROS_ENV
		(void)chown(ktc_tkt_string(), pwd->pw_uid, pwd->pw_gid);
#endif /* AFS_KERBEROS_ENV */
		/***** SIA LAUNCHING SESSION *****/
		if (sia_ses_launch(sia_collect, entity) == SIASUCCESS) {
		    ngroups = getgroups(NGROUPS, groups);
		    if ((pagval != NOPAG)
			&& (afs_get_pag_from_groups(groups[0], groups[1])) ==
			NOPAG) {
			/* we will have to shift grouplist to make room for pag */
			if (ngroups + 2 > NGROUPS) {
			    perror("Too many groups");	/* XXX */
			    exit(3);
			}
			for (j = ngroups - 1; j >= 0; j--) {
			    groups[j + 2] = groups[j];
			}
			ngroups += 2;
			get_groups_from_pag(pagval, &groups[0], &groups[1]);
			seteuid(0);	/* Since the seteuid was set the user already */
			if (setgroups(ngroups, groups) == -1) {
			    perror("setgroups");
			    exit(3);
			}
			seteuid(entity->pwd->pw_uid);	/* Set euid back */
		    }
		    /****** Nothing left to fail *******/
		    if (setreuid(geteuid(), geteuid()) < 0) {
			perror("setreuid()");
			exit(3);
		    }
		    /****** set up environment   *******/
		    /* destroy environment unless user has requested preservation */
		    envinit[0] = envinit[1] = 0;
		    if (environ)
			for (cnt = 0; environ[cnt] != NULL; cnt++)
			    if (!strncmp(environ[cnt], "TERM=", 5)) {
				envinit[0] = environ[cnt];
				break;
			    }
		    if (!pflag)
			environ = envinit;
		    (void)setenv("HOME", entity->pwd->pw_dir, 1);
		    if (entity->pwd->pw_shell && *entity->pwd->pw_shell)
			strncpy(shell, entity->pwd->pw_shell, sizeof shell);
		    (void)setenv("SHELL", shell, 1);
		    if (term[0] == '\0')
			(void)strncpy(term, stypeof(tty), sizeof(term));
		    (void)setenv("TERM", term, 0);
		    (void)setenv("USER", entity->pwd->pw_name, 1);
		    (void)setenv("LOGNAME", entity->pwd->pw_name, 1);
		    (void)setenv("PATH", _PATH_DEFPATH, 0);
#ifdef AFS_KERBEROS_ENV
		    (void)setenv("KRBTKFILE", ktc_tkt_string(), 0);
#endif /* AFS_KERBEROS_ENV */
		    if (!quietlog) {
			struct stat st;

			motd();
			(void)sprintf(tbuf, "%s/%s", _PATH_MAILDIR,
				      entity->pwd->pw_name);
			if (stat(tbuf, &st) == 0 && st.st_size != 0)
			    (void)printf("You have %smail.\n",
					 (st.st_mtime >
					  st.st_atime) ? "new " : "");
		    }
		    /******* Setup default signals **********/
		    (void)signal(SIGALRM, SIG_DFL);
		    (void)signal(SIGQUIT, SIG_DFL);
		    (void)signal(SIGINT, SIG_DFL);
		    (void)signal(SIGTSTP, SIG_IGN);

		    tbuf[0] = '-';
		    (void)strcpy(tbuf + 1,
				 (p = strrchr(shell, '/')) ? p + 1 : shell);
		    sia_ses_release(&entity);
		    execlp(shell, tbuf, 0);
		    (void)printf("login: no shell: %s.\n", strerror(errno));
		    exit(0);
		}
		/***** SIA session launch failure *****/
	    }
	    /***** SIA session establishment failure *****/
	}
	if (entity != NULL) {
	    sia_ses_release(&entity);
	}
	syslog(LOG_ERR, " LOGIN FAILURE ");
	exit(1);
    }
#else /* AFS_OSF_ENV */
#ifdef AFS_HPUX_ENV
    {				/* This block sets TZ environment on HPUX */
	/* First set TZ environment here, so that
	 * LOGIN FAILURE will get correct time stamp.
	 * For logins from console, -p flag is not set,
	 * the TZ environment set here will be cleared later.
	 * We're gonna set it once again later after it's cleared.
	 */
	FILE *fp;
	char buf[MAX_TZ_LEN];
	char *env_p = NULL;
	int tz_incorrect = 0;

	/* set TZ only if tz_incorrect
	 * (*env_p==0 or env_p==0 ) and there is a correct /etc/TIMEZONE
	 * otherwise, just let it be default (US eastern on HP)
	 */
	env_p = getenv("TZ");
	if (!env_p)
	    tz_incorrect = 1;
	else if (!(*env_p))
	    tz_incorrect = 1;

	if (tz_incorrect) {
	    /* HP UX 10.0 or later has different file format */
#ifdef AFS_HPUX100_ENV
	    /* /etc/TIMEZONE file example (HP-UX 10 or later)
	     * TZ=EST5EDT
	     * export TZ
	     */
	    fp = fopen("/etc/TIMEZONE", "r");
	    if (fp) {
		if ((fgets(buf, sizeof(buf), fp)) != NULL) {
		    buf[strlen(buf) - 1] = 0;
		    if (!strncmp(buf, "TZ=", 3)) {
			strncpy(TZ, buf, MAX_TZ_LEN);
			putenv(TZ);
		    }
		}
		fclose(fp);
	    }
#else /* AFS_HPUX100_ENV */
	    /* /etc/src.sh file example (HP-UX 9 or earlier)
	     * SYSTEM_NAME=myname; export SYSTEM_NAME
	     * TZ=EST5EDT; export TZ
	     */
	    fp = fopen("/etc/src.sh", "r");
	    if (fp) {
		while ((fgets(buf, sizeof(buf), fp)) != NULL) {
		    buf[strlen(buf) - 1] = 0;
		    if (!strncmp(buf, "TZ=", 3)) {
			char *p = strchr(buf, ';');
			if (p)
			    *p = 0;
			strncpy(TZ, buf, MAX_TZ_LEN);
			putenv(TZ);
		    }
		}
		fclose(fp);
	    }
#endif /* AFS_HPUX100_ENV */
	} else {
	    *TZ = 0;
	}
    }
#endif /* AFS_HPUX_ENV */
    for (cnt = 0;; ask = 1) {
	ioctlval = 0;
#if !defined(AIX) && !defined(AFS_HPUX_ENV)
	(void)ioctl(0, TIOCSETD, &ioctlval);
#endif /* !AIX and !hpux */

	if (ask | !username) {
	    fflag = 0;
	    getloginname(1);
	}
	/*
	 * Note if trying multiple user names;
	 * log failures for previous user name,
	 * but don't bother logging one failure
	 * for nonexistent name (mistyped username).
	 */
	if (failures && strcmp(tbuf, username)) {
	    if (failures > (pwd ? 0 : 1))
		badlogin(tbuf);
	    failures = 0;
	}
	(void)strcpy(tbuf, username);
	salt = "xx";
#ifdef	AFS_SUN5_ENV
	if ((pwd = getpwnam(username)) && (spwd = getspnam(username))) {
	    initgroups(username, pwd->pw_gid);	/* XXX */
	    salt = spwd->sp_pwdp;
	}
#else /* if !SUN5 */
	if (pwd = getpwnam(username))
	    salt = pwd->pw_passwd;
#endif /* else !SUN5 */

	/* if user not super-user, check for disabled logins */
	if (pwd == NULL || pwd->pw_uid) {
	    checknologin();
	}

	/*
	 * Disallow automatic login to root; if not invoked by
	 * root, disallow if the uid's differ.
	 */
	if (fflag && pwd) {
	    uid_t uid = getuid();

	    /*
	     *  allow f flag only if user running login is
	     *  same as target user or superuser
	     */
	    fflag = (uid == 0) || (uid == pwd->pw_uid);
	    if (fflag)
		passwd_req = 0;
/*			passwd_req = pwd->pw_uid == 0 || (uid && uid != pwd->pw_uid);*/
	}

	/*
	 * If no pre-authentication and a password exists
	 * for this user, prompt for one and verify it.
	 */
	if (!passwd_req || (pwd && !*pwd->pw_passwd)) {
	    break;
	}
#if	defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV)
	if (!usererr)
	    break;
#endif
#if !defined(AIX) && !defined(AFS_HPUX_ENV)
	setpriority(PRIO_PROCESS, 0, -4);
#endif

	/*
	 * The basic model for logging in now, is that *if* there
	 * is a kerberos record for this individual user we will
	 * trust kerberos (provided the user really has an account
	 * locally.)  If there is no kerberos record (or the password
	 * were typed incorrectly.) we would attempt to authenticate
	 * against the local password file entry.  Naturally, if
	 * both fail we use existing failure code.
	 */
	if (pwd) {
	    char pword[BUFSIZ];
	    int cnt1 = 0;
	    static int useGetPass = 0;

	    if (useGetPass) {
		pp = getpass("Password:");
		memcpy(pword, pp, 8);
	    } else {
		code =
		    ka_UserReadPassword("Password:", pword, sizeof(pword),
					&reason);

		if (code) {
		    if (code != KANULLPASSWORD) {
			if (++cnt1 > 5)
			    useGetPass = 1;
		    }
		    fprintf(stderr, "Unable to read password because %s\n",
			    reason);
		    goto loginfailed;
		}
	    }
	    pp = pword;
	    if (ka_UserAuthenticateGeneral(KA_USERAUTH_VERSION + KA_USERAUTH_DOSETPAG, pwd->pw_name,	/* kerberos name */
					   NULL,	/* instance */
					   NULL,	/* realm */
					   pp,	/* password */
					   0,	/* default lifetime */
					   &password_expires, 0,	/* spare 2 */
					   &reason	/* error string */
		)) {

		afsLoginFail = 1;
		pword[8] = '\0';
	    } else {
		if (strcmp(pwd->pw_passwd, "*"))
		    break;	/* out of for loop */
#if !defined(AIX) && !defined(AFS_HPUX_ENV)
		(void)ioctl(0, TIOCHPCL, (struct sgttyb *)NULL);
#endif /* AIX */
		goto loginfailed;
	    }
	} else {
	    pp = getpass("Password:");
	}

	p = crypt(pp, salt);
	(void)memset(pp, '\0', strlen(pp));
#ifdef	AFS_SUN5_ENV
	if (spwd && !strcmp(p, spwd->sp_pwdp))
#else
	if (pwd && !strcmp(p, pwd->pw_passwd))
#endif
	{
	    /* Only print this if local authentication is successful */
	    if (afsLoginFail) {
		printf("Unable to authenticate to AFS because %s\n", reason);
		printf("proceeding with local authentication...\n");
	    }
	    afsLoginFail = 0;
	    break;
	}
	afsLoginFail = 0;

      loginfailed:
	printf("Login incorrect\n");
	failures++;
	/* we allow 10 tries, but after 3 we start backing off */
	if (++cnt > 3) {
	    if (cnt >= 6) {
		badlogin(username);
#if !defined(AIX) && !defined(AFS_HPUX_ENV)
		(void)ioctl(0, TIOCHPCL, (struct sgttyb *)NULL);
#endif /* AIX */
		(void)close(0);
		(void)close(1);
		(void)close(2);
		sleep(10);
		exit(1);
	    }
	    sleep((u_int) ((cnt - 3) * 5));
	}
    }
#if !defined(AIX) && !defined(AFS_HPUX_ENV)
    setpriority(PRIO_PROCESS, 0, 0);
#endif /* AIX */
#ifdef	AFS_SUN5_ENV
    /*  Exits if root login not on system console. */
    if (pwd->pw_uid == 0) {
	if ((Console != NULL) && (strcmp(ttyn, Console) != 0)) {
	    (void)printf("Not on system console\n");
	    exit(10);
	}
	if (Def_supath != NULL)
	    Def_path = Def_supath;
	else
	    Def_path = DEF_SUPATH;
    }
#endif /* AFS_SUN5_ENV */

    printf("AFS (R) %s Login\n", AFSVersion);	/* generated version */

    /* committed to login -- turn off timeout */
    (void)alarm((u_int) 0);

    /*
     * If valid so far and root is logging in, see if root logins on
     * this terminal are permitted.
     */
#if !defined(AIX) && !defined(AFS_SUN5_ENV)
#ifdef	AFS_HPUX_ENV
    /* This is the /etc/securetty feature.  We wanted to 1st prompt for a
     * password even if we know that this is not a securetty because
     * we don't want to give any clue if the potential intruder guesses
     * the correct password.
     */
    if ((pwd->pw_uid == 0) && !rootterm(ttyn + sizeof("/dev/") - 1)) {
#else
    if (pwd->pw_uid == 0 && !rootterm(tty)) {
#endif
	if (hostname)
	    syslog(LOG_NOTICE, "ROOT LOGIN REFUSED FROM %s", hostname);
	else
	    syslog(LOG_NOTICE, "ROOT LOGIN REFUSED ON %s", tty);
	printf("Login incorrect\n");

	/*
	 * Reset tty line.
	 */

	(void)chmod(ttyn, 0666);

	sleepexit(1);
    }
#endif /* !AIX & !SUN5 */
#ifdef Q_SETUID
    if (quota(Q_SETUID, pwd->pw_uid, 0, 0) < 0 && errno != EINVAL) {
	switch (errno) {
	case EUSERS:
	    fprintf(stderr,
		    "Too many users logged on already.\nTry again later.\n");
	    break;
	case EPROCLIM:
	    fprintf(stderr, "You have too many processes running.\n");
	    break;
	default:
	    perror("quota (Q_SETUID)");
	}

	/*
	 * Reset tty line.
	 */

	(void)chmod(ttyn, 0666);

	sleepexit(0);
    }
#endif /* Q_SETUID */

    /* For Solaris and HP, we might want to chdir later for remotely 
     * mounted home directory because home direcotry may be
     * protected now
     */
    if (chdir(pwd->pw_dir) < 0) {
#if !defined(AFS_SUN5_ENV) && !defined(AFS_HPUX_ENV)
	printf("No directory %s!\n", pwd->pw_dir);
#endif /* !defined(AFS_SUN5_ENV) && !defined(AFS_HPUX_ENV) */
	if (chdir("/")) {

	    /*
	     * Reset tty line.
	     */

	    (void)chmod(ttyn, 0666);

	    exit(0);
	}
#if !defined(AFS_SUN5_ENV) && !defined(AFS_HPUX_ENV)
	pwd->pw_dir = "/";
	printf("Logging in with home = \"/\".\n");
#else /* !defined(AFS_SUN5_ENV) && !defined(AFS_HPUX_ENV) */
	retry_chdir = 1;
#endif /* !defined(AFS_SUN5_ENV) && !defined(AFS_HPUX_ENV) */
    }

    /* nothing else left to fail -- really log in */
    {
#ifdef	AFS_SUN5_ENV
	long tlen;
	struct utmpx *utx, *getutxent(), *pututxline();
#endif
#if defined(AFS_HPUX_ENV)
	register struct utmp *u = NULL;
	extern pid_t getpid();
#endif

#ifdef	AFS_SUN5_ENV
	(void)time(&utmp.ut_tv.tv_sec);
#else
	(void)time(&utmp.ut_time);
#endif
	strncpy(utmp.ut_name, username, sizeof(utmp.ut_name));
#if !defined(AIX)		/*&& !defined(AFS_SUN5_ENV) */
	if (hostname)
	    strncpy(utmp.ut_host, hostname, sizeof(utmp.ut_host));
#endif
	strncpy(utmp.ut_line, tty, sizeof(utmp.ut_line));
#if defined(AFS_HPUX_ENV) || defined(AFS_SUN5_ENV)
	utmp.ut_type = USER_PROCESS;
	if (!strncmp(ttyn, "/dev/", strlen("/dev/"))) {
	    strncpy(utmp.ut_line, &(ttyn[strlen("/dev/")]),
		    sizeof(utmp.ut_line));
	}

	utmp.ut_pid = getpid();
	if ((!strncmp(tty, "tty", strlen("tty")))
	    || (!strncmp(tty, "pty", strlen("pty")))) {
	    strncpy(utmp.ut_id, &(tty[strlen("tty")]), sizeof(utmp.ut_id));
	}
#endif /* AFS_HPUX_ENV */
#ifdef	AFS_HPUX_ENV
	/*
	 * Find the entry for this pid (or line if we are a sublogin)
	 * in the utmp file.
	 */
	while ((u = getutent()) != NULL) {
	    if ((u->ut_type == INIT_PROCESS || u->ut_type == LOGIN_PROCESS
		 || u->ut_type == USER_PROCESS)
		&&
		((sublogin
		  && strncmp(u->ut_line, ttyntail, sizeof(u->ut_line)) == 0)
		 || u->ut_pid == utmp.ut_pid)) {

		/*
		 * Copy in the name of the tty minus the
		 * "/dev/", the id, and set the type of entry
		 * to USER_PROCESS.
		 */
		(void)strncpy(utmp.ut_line, ttyntail, sizeof(utmp.ut_line));
		utmp.ut_id[0] = u->ut_id[0];
		utmp.ut_id[1] = u->ut_id[1];
		utmp.ut_id[2] = u->ut_id[2];
		utmp.ut_id[3] = u->ut_id[3];
		utmp.ut_type = USER_PROCESS;
		utmp.how_to_count = add_count();

		/*
		 * Copy remote host information
		 */
		strncpy(utmp.ut_host, u->ut_host, sizeof(utmp.ut_host));
		utmp.ut_addr = u->ut_addr;

		/* Return the new updated utmp file entry. */
		pututline(&utmp);
		break;
	    }
	}
	endutent();		/* Close utmp file */
#endif
#ifdef	AFS_SUN5_ENV
	utmp.ut_syslen = (tlen =
			  strlen(utmp.ut_host)) ? min(tlen + 1,
						      sizeof(utmp.
							     ut_host)) : 0;
	strncpy(utmp.ut_user, username, sizeof(utmp.ut_user));
	while ((utx = getutxent()) != NULL) {
	    if ((utx->ut_type == INIT_PROCESS || utx->ut_type == LOGIN_PROCESS
		 || utx->ut_type == USER_PROCESS)
		&& (utx->ut_pid == utmp.ut_pid)) {
		strncpy(utmp.ut_line, (ttyn + sizeof("/dev/") - 1),
			sizeof(utmp.ut_line));
		utmp.ut_id[0] = utx->ut_id[0];
		utmp.ut_id[1] = utx->ut_id[1];
		utmp.ut_id[2] = utx->ut_id[2];
		utmp.ut_id[3] = utx->ut_id[3];
		utmp.ut_type = USER_PROCESS;
		pututxline(&utmp);
		break;
	    }
	}
	endutxent();		/* Close utmp file */
	if (!utx) {
	    printf
		("No utmpx entry.  You must exec 'login' from the lowest level 'sh'\n");
	    exit(1);
	} else
	    updwtmpx(WTMPX_FILE, &utmp);
#else
	login(&utmp);
#endif
    }

    quietlog = access(HUSHLOGIN, F_OK) == 0;
    dolastlog(quietlog);

#if !defined(AIX) && !defined(AFS_HPUX_ENV)
#ifdef	AFS_SUN_ENV
    if (!hflag && !rflag) {	/* XXX */
#else
    if (!hflag) {		/* XXX */
#endif
	static struct winsize win = { 0, 0, 0, 0 };

	(void)ioctl(0, TIOCSWINSZ, &win);
    }
#endif /* !AIX and !hpux */

#ifdef	AFS_SUN5_ENV
    /*
     * Set the user's ulimit
     */
    if (Def_ulimit > 0L && ulimit(SET_FSIZ, Def_ulimit) < 0L)
	(void)printf("Could not set ULIMIT to %ld\n", Def_ulimit);

#endif
#if	defined(AFS_SUN_ENV)
    /*
     * Set owner/group/permissions of framebuffer devices
     */
    (void)set_fb_attrs(ttyn, pwd->pw_uid, pwd->pw_gid);
#endif

    (void)chown(ttyn, pwd->pw_uid,
		(gr = getgrnam(TTYGRPNAME)) ? gr->gr_gid : pwd->pw_gid);


    (void)chmod(ttyn, 0620);
    (void)setgid(pwd->pw_gid);

#ifdef	AFS_HPUX_ENV
    if ((pwd->pw_gid < 0) || (pwd->pw_gid > MAXUID)) {
	printf("Bad group id.\n");
	exit(1);
    }
    if ((pwd->pw_uid < 0) || (pwd->pw_uid > MAXUID)) {
	printf("Bad user id.\n");
	exit(1);
    }
#endif

#ifdef AFS_KERBEROS_ENV
    (void)chown(ktc_tkt_string(), pwd->pw_uid, pwd->pw_gid);

#endif

    groups[0] = groups[1] = 0;
    ngroups = getgroups(NGROUPS_MAX, groups);
    pagval = get_pag_from_groups(groups[0], groups[1]);
    initgroups(username, pwd->pw_gid);
    ngroups = getgroups(NGROUPS_MAX, groups);
    if ((pagval != NOPAG)
	&& (afs_get_pag_from_groups(groups[0], groups[1])) == NOPAG) {
	/* we will have to shift grouplist to make room for pag */
	if (ngroups + 2 > NGROUPS_MAX)
	    return E2BIG;	/* XXX */
	for (j = ngroups - 1; j >= 0; j--) {
	    groups[j + 2] = groups[j];
	}
	ngroups += 2;
	get_groups_from_pag(pagval, &groups[0], &groups[1]);
	if (setgroups(ngroups, groups) == -1) {
	    perror("setgroups");
	    return -1;
	}
    }
#ifdef Q_DOWARN
    quota(Q_DOWARN, pwd->pw_uid, (dev_t) - 1, 0);
#endif /* Q_DOWARN */

    /* destroy environment unless user has requested preservation */
    if (!pflag) {
	envinit[0] = NULL;
	envinit[1] = NULL;
	environ = envinit;
    }
    (void)setuid(pwd->pw_uid);

#if defined(AFS_SUN5_ENV) || defined(AFS_HPUX_ENV)
    /* This is the time to retry chdir for Solaris and HP
     */
    if (retry_chdir && chdir(pwd->pw_dir) < 0) {
	printf("No directory %s!\n", pwd->pw_dir);
	if (chdir("/") < 0) {
	    /* reset tty line and exit */
	    (void)chmod(ttyn, 0666);
	    exit(0);
	}
	pwd->pw_dir = "/";
	printf("Logging with home = \"/\".\n");
    }
#endif /* AFS_SUN5_ENV || AFS_HPUX_ENV */

    if (*pwd->pw_shell == '\0')
	pwd->pw_shell = BSHELL;
#if !defined(AIX) && !defined(AFS_HPUX_ENV)
    /* turn on new line discipline for the csh */
    else if (!strcmp(pwd->pw_shell, "/bin/csh")) {
	ioctlval = NTTYDISC;
	(void)ioctl(0, TIOCSETD, &ioctlval);
    }
#endif /* AIX */
#ifdef	AFS_SUN5_ENV
    else {
	if (Altshell != NULL && strcmp(Altshell, "YES") == 0) {
	    (void)setenv("SHELL", pwd->pw_shell, 1);
	}
    }
    if (Def_tz != NULL) {
	tmp_tz = Def_tz;
    }
    Def_tz = getenv("TZ");
    if (Def_tz) {
	setenv("TZ", Def_tz, 1);
    } else {
	if (tmp_tz) {
	    Def_tz = tmp_tz;
	    setenv("TZ", Def_tz, 1);
	}
    }
    if (Def_hertz == NULL)
	(void)setenv("HZ", HZ, 1);
    else
	(void)setenv("HZ", Def_hertz, 1);

#endif

#ifdef AFS_HPUX_ENV
    /* if pflag is not set
     * the TZ environment was cleared moments ago */
    if (*TZ && !pflag) {
	setenv("TZ", &TZ[3], 1);
    }
#endif /* AFS_HPUX_ENV */

    (void)setenv("HOME", pwd->pw_dir, 1);
    (void)setenv("SHELL", pwd->pw_shell, 1);
    if (term[0] == '\0')
	strncpy(term, stypeof(tty), sizeof(term));
    (void)setenv("TERM", term, 0);
    (void)setenv("USER", pwd->pw_name, 1);
    (void)setenv("LOGNAME", pwd->pw_name, 1);
#ifdef	AFS_SUN5_ENV
    if (Def_path == NULL)
	(void)setenv("PATH", DEF_PATH, 0);
    else
	(void)setenv("PATH", Def_path, 0);
#else
    (void)setenv("PATH", "/usr/ucb:/bin:/usr/bin:", 0);
#endif

#if     defined(AFS_SUN5_ENV) || defined(AFS_HPUX100_ENV)
#ifndef NO_MAIL
    /* Sol, HP: set MAIL only if NO_MAIL is not set */
    sprintf(mail, "%s%s", MAILDIR, pwd->pw_name);
    (void)setenv("MAIL", mail, 0);
#endif /* NO_MAIL */
#endif /* AFS_SUN5_ENV || AFS_HPUX100_ENV */

#ifdef AFS_KERBEROS_ENV
    (void)setenv("KRBTKFILE", ktc_tkt_string(), 0);
#endif

    if (password_expires >= 0) {
	char sbuffer[100];
	(void)setenv("PASSWORD_EXPIRES",
		     cv2string(&sbuffer[100], password_expires), 1);
    }

    if (tty[sizeof("tty") - 1] == 'd')
	syslog(LOG_INFO, "DIALUP %s, %s", tty, pwd->pw_name);
    if (pwd->pw_uid == 0)
	if (hostname)
	    syslog(LOG_NOTICE, "ROOT LOGIN ON %s FROM %s", tty, hostname);
	else
	    syslog(LOG_NOTICE, "ROOT LOGIN ON %s", tty);

    if (!quietlog) {
	struct stat st;

#if	! defined(AFS_SUN5_ENV) && (! defined(AFS_HPUX_ENV))
	/* In Solaris the shell displays motd */
	/* On hp700s /etc/profile also does it so it's redundant here too */
	motd();
	/* Don't check email either, should be done in shell */
	(void)sprintf(tbuf, "%s/%s", MAILDIR, pwd->pw_name);
	if (stat(tbuf, &st) == 0 && st.st_size != 0)
	    printf("You have %smail.\n",
		   (st.st_mtime > st.st_atime) ? "new " : "");
#endif
    }

    (void)signal(SIGALRM, SIG_DFL);
    (void)signal(SIGQUIT, SIG_DFL);
    (void)signal(SIGINT, SIG_DFL);
#if !defined(AIX) && !defined(AFS_HPUX_ENV)
    (void)signal(SIGTSTP, SIG_IGN);
#endif /* AIX */

    tbuf[0] = '-';
    strcpy(tbuf + 1,
	   (p = strrchr(pwd->pw_shell, '/')) ? p + 1 : pwd->pw_shell);
    execlp(pwd->pw_shell, tbuf, 0);
    fprintf(stderr, "login: no shell: ");
    perror(pwd->pw_shell);
    exit(0);
#endif /* else AFS_OSF_ENV */
}

getloginname(prompt)
     int prompt;
{
    register int ch;
    register char *p;
    static char nbuf[UT_NAMESIZE + 1];

    for (;;) {
	if (prompt) {
	    printf("login: ");
	    prompt = 0;
	}
	for (p = nbuf; (ch = getchar()) != '\n';) {
	    if (ch == EOF) {
		badlogin(username);
		exit(0);
	    }
	    if (p < nbuf + UT_NAMESIZE)
		*p++ = (char)ch;
	}
	if (p > nbuf) {
	    if (nbuf[0] == '-')
		fprintf(stderr, "login names may not start with '-'.\n");
	    else {
		*p = '\0';
		username = nbuf;
		break;
	    }
	} else
	    prompt = 1;
    }
}


#ifdef	AFS_HPUX_ENV
rootterm(tty)
     char *tty;
{
    register FILE *fd;
    char buf[100];

    /*
     * check to see if /etc/securetty exists, if it does then scan
     * file to see if passwd tty is in the file.  Return 1 if it is, 0 if not.
     */
    if ((fd = fopen(SECURETTY_FILE, "r")) == NULL)
	return (1);
    while (fgets(buf, sizeof buf, fd) != NULL) {
	buf[strlen(buf) - 1] = '\0';
	if (strcmp(tty, buf) == 0) {
	    fclose(fd);
	    return (1);
	}
    }
    fclose(fd);
    return (0);
}
#endif
#if !defined(AIX) && !defined(AFS_HPUX_ENV) && !defined(AFS_SUN5_ENV)
rootterm(ttyn)
     char *ttyn;
{
    struct ttyent *t;

    return ((t = getttynam(ttyn)) && t->ty_status & TTY_SECURE);
}
#endif

jmp_buf motdinterrupt;

motd()
{
    register int fd, nchars;
    void (*oldint) ();
    void sigint();
    char tbuf[8192];

    if ((fd = open(MOTDFILE, O_RDONLY, 0)) < 0)
	return;
    oldint = signal(SIGINT, sigint);
    if (setjmp(motdinterrupt) == 0)
	while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0)
	    (void)write(fileno(stdout), tbuf, nchars);
    (void)signal(SIGINT, oldint);
    (void)close(fd);
}

void
sigint()
{
    longjmp(motdinterrupt, 1);
}

checknologin()
{
    register int fd, nchars;
    char tbuf[8192];

    if ((fd = open(NOLOGIN, O_RDONLY, 0)) >= 0) {
	while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0)
	    (void)write(fileno(stdout), tbuf, nchars);
	sleepexit(0);
    }
}

#if defined(AIX) || defined(AFS_HPUX_ENV) || defined(AFS_AIX32_ENV)
dolastlog(quiet)
     int quiet;
{
    return 0;
}
#else
#ifdef	AFS_OSF_ENV
dolastlog(quiet)
     int quiet;
{
#define _PATH_LASTLOG   "/var/adm/lastlog"
    struct lastlog ll;
    int fd;
    struct passwd *pwd;

    pwd = entity->pwd;
    if ((fd = open(_PATH_LASTLOG, (O_RDWR | O_CREAT), 0666)) < 0)
	fprintf(stderr, "Can't open %s", _PATH_LASTLOG);
    else {
	(void)lseek(fd, (off_t) pwd->pw_uid * sizeof(ll), L_SET);
	if (!quiet) {
	    if (read(fd, (char *)&ll, sizeof(ll)) == sizeof(ll)
		&& ll.ll_time != 0) {
		(void)printf("Last login: %.*s ", 24 - 5,
			     (char *)ctime(&ll.ll_time));
		if (*ll.ll_host != '\0')
		    (void)printf("from %.*s\n", sizeof(ll.ll_host),
				 ll.ll_host);
		else
		    (void)printf("on %.*s\n", sizeof(ll.ll_line), ll.ll_line);
	    }
	}
	(void)close(fd);
    }
}
#else
dolastlog(quiet)
     int quiet;
{
    struct lastlog ll;
    int fd;

    if ((fd = open(LASTLOG, O_RDWR, 0)) >= 0) {
	(void)lseek(fd, (off_t) pwd->pw_uid * sizeof(ll), L_SET);
	if (!quiet) {
	    if (read(fd, (char *)&ll, sizeof(ll)) == sizeof(ll)
		&& ll.ll_time != 0) {
		printf("Last login: %.*s ", 24 - 5,
		       (char *)ctime(&ll.ll_time));
		if (*ll.ll_host != '\0')
		    printf("from %.*s\n", sizeof(ll.ll_host), ll.ll_host);
		else
		    printf("on %.*s\n", sizeof(ll.ll_line), ll.ll_line);
	    }
	    (void)lseek(fd, (off_t) pwd->pw_uid * sizeof(ll), L_SET);
	}
	memset(&ll, '\0', sizeof(ll));
	(void)time(&ll.ll_time);
	strncpy(ll.ll_line, tty, sizeof(ll.ll_line));
	if (hostname)
	    strncpy(ll.ll_host, hostname, sizeof(ll.ll_host));
	(void)write(fd, (char *)&ll, sizeof(ll));
	(void)close(fd);
    }
}
#endif
#endif /* AIX */

badlogin(name)
     char *name;
{
    if (failures == 0)
	return;
    if (hostname)
	syslog(LOG_NOTICE, "%d LOGIN FAILURE%s FROM %s, %s", failures,
	       failures > 1 ? "S" : "", hostname, name);
    else
	syslog(LOG_NOTICE, "%d LOGIN FAILURE%s ON %s, %s", failures,
	       failures > 1 ? "S" : "", tty, name);
}

#undef	UNKNOWN
#ifdef	AFS_SUN5_ENV
#define	UNKNOWN	"sun"
#else
#define	UNKNOWN	"su"
#endif

#if defined(AIX) || defined(AFS_HPUX_ENV) || defined(AFS_SUN5_ENV)
char *
stypeof(ttyid)
     char *ttyid;
{
    return (UNKNOWN);
}
#else
char *
stypeof(ttyid)
     char *ttyid;
{
    struct ttyent *t;

    return (ttyid && (t = getttynam(ttyid)) ? t->ty_type : UNKNOWN);
}
#endif

getstr(buf, cnt, err)
     char *buf, *err;
     int cnt;
{
    char ch;

    do {
	if (read(0, &ch, sizeof(ch)) != sizeof(ch))
	    exit(1);
	if (--cnt < 0) {
	    fprintf(stderr, "%s too long\r\n", err);
	    sleepexit(1);
	}
	*buf++ = ch;
    } while (ch);
}

sleepexit(eval)
     int eval;
{
    sleep((u_int) 5);
    exit(eval);
}


int
get_pag_from_groups(g0, g1)
     afs_uint32 g0, g1;
{
    afs_uint32 h, l, result;

    g0 -= 0x3f00;
    g1 -= 0x3f00;
    if (g0 < 0xc000 && g1 < 0xc000) {
	l = ((g0 & 0x3fff) << 14) | (g1 & 0x3fff);
	h = (g0 >> 14);
	h = (g1 >> 14) + h + h + h;
	result = ((h << 28) | l);
	/* Additional testing */
	if (((result >> 24) & 0xff) == 'A')
	    return result;
	else
	    return NOPAG;
    }
    return NOPAG;
}


get_groups_from_pag(pag, g0p, g1p)
     afs_uint32 pag;
     afs_uint32 *g0p, *g1p;
{
    unsigned short g0, g1;

    pag &= 0x7fffffff;
    g0 = 0x3fff & (pag >> 14);
    g1 = 0x3fff & pag;
    g0 |= ((pag >> 28) / 3) << 14;
    g1 |= ((pag >> 28) % 3) << 14;
    *g0p = g0 + 0x3f00;
    *g1p = g1 + 0x3f00;
}


#if	defined(AFS_SUN_ENV)
/*
 * set_fb_attrs -- change owner/group/permissions of framebuffers
 *		   listed in /etc/fbtab.
 *
 * Note:
 * Exiting from set_fb_attrs upon error is not advisable
 * since it would disable logins on console devices.
 *
 * File format:
 * console mode device_name[:device_name ...]
 * # begins a comment and may appear anywhere on a line.
 *
 * Example:
 * /dev/console 0660 /dev/fb:/dev/cgtwo0:/dev/bwtwo0
 * /dev/console 0660 /dev/gpone0a:/dev/gpone0b
 *
 * Description:
 * The example above sets the owner/group/permissions of the listed
 * devices to uid/gid/0660 if ttyn is /dev/console
 */

#define	FIELD_DELIMS 	" \t\n"
#define	COLON 		":"
#define COLON_C         ':'
#define WILDCARD        "/*"
#define WILDCARD_LEN    2
#define MAX_LINELEN     256
#ifdef AFS_SUN5_ENV
#define FBTAB           "/etc/logindevperm"
#else
#define FBTAB           "/etc/fbtab"
#endif


set_fb_attrs(ttyn, uid, gid)
     char *ttyn;
     int uid;
     int gid;
{
    char line[MAX_LINELEN];
    char *console;
    char *mode_str;
    char *dev_list;
    char *device;
    char *ptr;
    int mode;
    long strtol();
    FILE *fp;

    if ((fp = fopen(FBTAB, "r")) == NULL)
	return;
    while (fgets(line, MAX_LINELEN, fp)) {
	if (ptr = strchr(line, '#'))
	    *ptr = '\0';	/* handle comments */
	if ((console = strtok(line, FIELD_DELIMS)) == NULL)
	    continue;		/* ignore blank lines */
	if (strcmp(console, ttyn) != 0)
	    continue;		/* ignore non-consoles */
	mode_str = strtok((char *)NULL, FIELD_DELIMS);
	if (mode_str == NULL) {
	    (void)fprintf(stderr, "%s: invalid entry -- %s\n", FBTAB, line);
	    continue;
	}
	/* convert string to octal value */
	mode = (int)strtol(mode_str, (char **)NULL, 8);
	if (mode < 0 || mode > 0777) {
	    (void)fprintf(stderr, "%s: invalid mode -- %s\n", FBTAB,
			  mode_str);
	    continue;
	}
	dev_list = strtok((char *)NULL, FIELD_DELIMS);
	if (dev_list == NULL) {
	    (void)fprintf(stderr, "%s: %s -- empty device list\n", FBTAB,
			  console);
	    continue;
	}
#ifdef	AFS_SUN5_ENV
	device = strtok(dev_list, COLON);
	while (device) {
	    ptr = strstr(device, WILDCARD);
	    if (ptr && (strlen(device) - (ptr - &device[0])) == WILDCARD_LEN) {
		/* The device was a (legally-specified) directory. */
		DIR *dev_dir;
		struct dirent *dir_e;
		char dev_file[MAXPATHLEN];

		*ptr = '\0';	/* Remove the wildcard from the dir name. */
		if ((dev_dir = opendir(device)) == (DIR *) NULL) {
		    syslog(LOG_ERR, "bad device %s%s in %s, ignored", device,
			   WILDCARD, FBTAB);
		    continue;
		}

		/* Directory is open; alter its files. */
		/* Must link with /usr/lib/libc.a before 
		 * /usr/ucblib/libucb.a or the d_name structs
		 * miss the first two characters of the filename */
		while (dir_e = readdir(dev_dir)) {
		    if (strcmp(dir_e->d_name, "..")
			&& strcmp(dir_e->d_name, ".")) {
			strcpy(dev_file, device);
			strcat(dev_file, "/");
			strcat(dev_file, dir_e->d_name);
			(void)chown(dev_file, uid, gid);
			(void)chmod(dev_file, mode);
		    }
		}
		(void)closedir(dev_dir);
	    } else {
		/* 'device' was not a directory, so we can just do it. */
		(void)chown(device, uid, gid);
		(void)chmod(device, mode);
	    }
	    device = strtok((char *)NULL, COLON);	/* Move thru list. */
#else
	device = strtok(dev_list, COLON);
	while (device) {
	    (void)chown(device, uid, gid);
	    (void)chmod(device, mode);
	    device = strtok((char *)NULL, COLON);
#endif
	}
    }
    (void)fclose(fp);
}
#endif


#ifdef	AFS_SUN5_ENV
static int
quotec()
{
    register int c, i, num;

    switch (c = getc(stdin)) {
    case 'n':
	c = '\n';
	break;
    case 'r':
	c = '\r';
	break;
    case 'v':
	c = '\013';
	break;
    case 'b':
	c = '\b';
	break;
    case 't':
	c = '\t';
	break;
    case 'f':
	c = '\f';
	break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
	for (num = 0, i = 0; i < 3; i++) {
	    num = num * 8 + (c - '0');
	    if ((c = getc(stdin)) < '0' || c > '7')
		break;
	}
	ungetc(c, stdin);
	c = num & 0377;
	break;
    default:
	break;
    }
    return (c);
}

static char **
getargs(inline)
     char *inline;
{
    static char envbuf[256];
    static char *args[63];
    register char *ptr, **answer;
    register int c;
    int state;
    extern int quotec();

    for (ptr = envbuf; ptr < &envbuf[sizeof(envbuf)];)
	*ptr++ = '\0';
    for (answer = args; answer < &args[63];)
	*answer++ = (char *)NULL;
    for (ptr = envbuf, answer = &args[0], state = 0;
	 (c = getc(stdin)) != EOF;) {
	*(inline++) = c;
	switch (c) {
	case '\n':
	    if (ptr == &envbuf[0])
		return ((char **)NULL);
	    else
		return (&args[0]);
	case ' ':
	case '\t':
	    if (state == 1) {
		*ptr++ = '\0';
		state = 0;
	    }
	    break;
	case '\\':
	    c = quotec();
	default:
	    if (state == 0) {
		*answer++ = ptr;
		state = 1;
	    }
	    *ptr++ = c;
	}
	if (ptr == &envbuf[sizeof(envbuf) - 1]) {
	    ungetc('\n', stdin);
	    putc('\n', stdout);
	}
    }
    exit(0);
}
#endif

#ifdef	AFS_SUN_ENV
#ifdef	AFS_SUN5_ENV
doremotelogin()
{
    static struct passwd nouser = { "", "no:password", ~0 };

    getstr(rusername, sizeof(rusername), "remuser");
    getstr(lusername, sizeof(lusername), "locuser");
    if (!username)
	username = lusername;
    getstr(term, sizeof(term), "Terminal type");
    if (getuid()) {
	pwd = &nouser;
	goto out;

    }
    pwd = getpwnam(lusername);
    if (pwd == NULL) {
	pwd = &nouser;
	goto out;
    }
  out:
    doremoteterm(term);
    return;
}
#else
doremotelogin(host)
     char *host;
{
    int code;
    static char rusername[100], lusername[100];
    static struct passwd nouser = { "", "no:password", ~0 };

    getstr(rusername, sizeof(rusername), "remuser");
    getstr(lusername, sizeof(lusername), "locuser");
    if (!username)
	username = lusername;
    getstr(term, sizeof(term), "Terminal type");
    if (getuid()) {
	pwd = &nouser;
	return -1;
    }
    pwd = getpwnam(lusername);
    if (pwd == NULL) {
	pwd = &nouser;
	return -1;
    }
    code = ruserok(host, (pwd->pw_uid == 0), rusername, lusername);
/*printf("DOREM=%d, host=%s, uid=%d, ruser=%s, luser=%s\n", code, host, pwd->pw_uid, rusername, lusername);*/
    return code;
}
#endif

char *speeds[] = { "0", "50", "75", "110", "134", "150", "200", "300",
    "600", "1200", "1800", "2400", "4800", "9600", "19200", "38400"
};

#define	NSPEEDS	(sizeof (speeds) / sizeof (speeds[0]))

#ifdef	AFS_SUN5_ENV
doremoteterm(terminal)
#else
doremoteterm(terminal, tp)
     struct sgttyb *tp;
#endif
     char *terminal;
{
#ifdef	AFS_SUN5_ENV
    struct termios tp;
#endif
    register char *cp = strchr(terminal, '/'), **cpp;
    char *speed;

#ifdef	AFS_SUN5_ENV
    (void)ioctl(0, TCGETS, &tp);
#endif
    if (cp) {
	*cp++ = '\0';
	speed = cp;
	cp = strchr(speed, '/');
	if (cp)
	    *cp++ = '\0';
	for (cpp = speeds; cpp < &speeds[NSPEEDS]; cpp++)
	    if (strcmp(*cpp, speed) == 0) {
#ifdef	AFS_SUN5_ENV
		tp.c_cflag =
		    ((tp.c_cflag & ~CBAUD) | ((cpp - speeds) & CBAUD));
#else
		tp->sg_ispeed = tp->sg_ospeed = cpp - speeds;
#endif
		break;
	    }
    }
#ifdef	AFS_SUN5_ENV
    tp.c_lflag = ECHO | ICANON | IGNPAR | ICRNL;
    (void)ioctl(0, TCSETS, &tp);
#else
    tp->sg_flags = ECHO | CRMOD | ANYP | XTABS;
#endif
}
#endif

#ifdef AFS_OSF_ENV
/*
 *  Check if the specified program is really a shell (e.g. "sh" or "csh").
 */
usershell(shell)
     char *shell;
{
    register char *p;
    char *getusershell();

    setusershell();
    while ((p = getusershell()) != NULL)
	if (strcmp(p, shell) == 0)
	    break;
    endusershell();
    return (!!p);
}

static char *
getlinep(const char *string, int size, FILE * stream)
{
    extern int errno;
    int c;
    char *cp;

    errno = 0;
    if (!fgets(string, size, stream) || ferror(stream) || errno == EINTR)
	return NULL;
    else if (cp = strchr(string, '\n'))
	*cp = '\0';
    else
	while ((c = getc(stdin)) != '\n' && c != EOF && errno != EINTR);
    return string;
}

clean_entity_pwd(SIAENTITY * entity)
{
    if (entity->acctname != NULL) {
	free(entity->acctname);
	entity->acctname = NULL;
    }
    if (entity->pwd != NULL) {
	if (entity->pwd->pw_name) {
	    memset(entity->pwd->pw_name, '\0', strlen(entity->pwd->pw_name));
	    free(entity->pwd->pw_name);
	    entity->pwd->pw_name = NULL;
	}
	if (entity->pwd->pw_passwd) {
	    memset(entity->pwd->pw_passwd, '\0',
		   strlen(entity->pwd->pw_passwd));
	    free(entity->pwd->pw_passwd);
	    entity->pwd->pw_passwd = NULL;
	}
	if (entity->pwd->pw_comment) {
	    memset(entity->pwd->pw_comment, '\0',
		   strlen(entity->pwd->pw_comment));
	    free(entity->pwd->pw_comment);
	    entity->pwd->pw_comment = NULL;
	}
	if (entity->pwd->pw_gecos) {
	    memset(entity->pwd->pw_gecos, '\0',
		   strlen(entity->pwd->pw_gecos));
	    free(entity->pwd->pw_gecos);
	    entity->pwd->pw_gecos = NULL;
	}
	if (entity->pwd->pw_dir) {
	    memset(entity->pwd->pw_dir, '\0', strlen(entity->pwd->pw_dir));
	    free(entity->pwd->pw_dir);
	    entity->pwd->pw_dir = NULL;
	}
	if (entity->pwd->pw_shell) {
	    memset(entity->pwd->pw_shell, '\0',
		   strlen(entity->pwd->pw_shell));
	    free(entity->pwd->pw_shell);
	    entity->pwd->pw_shell = NULL;
	}
	free(entity->pwd);
	entity->pwd = NULL;
    }
}

int
getloginp(entity)
     SIAENTITY *entity;
{
    char result[100];

    fputs("login: ", stdout);
    if (getlinep(result, sizeof result, stdin) == (char *)NULL) {
	return (-1);
    }
    entity->name = (char *)malloc(SIANAMEMIN + 1);
    if (entity->name == NULL)
	return (-1);
    strcpy((unsigned char *)entity->name, result);
    return (1);
}

#ifdef AFS_KERBEROS_ENV
extern char *ktc_tkt_string();
#endif

int
do_afs_auth(pwd, pass, doafs)
     struct passwd *pwd;
     char *pass;
     int doafs;
{
    char *namep;
    int code, afsLoginFail = 0;
    char *lcell, realm[MAXKTCREALMLEN], *pword, prompt[256];
    char *shell, *reason;

  loop:
    if (pass) {
	pword = pass;
    } else {
	sprintf(prompt, "Enter AFS password: ");
	pword = getpass(prompt);
	if (strlen(pword) == 0) {
	    printf
		("Unable to read password because zero length password is illegal\n");
	    printf("Login incorrect\n");
	    fflush(stdout);
	    goto loop;
	}
    }

    if (ka_UserAuthenticateGeneral(KA_USERAUTH_VERSION + KA_USERAUTH_DOSETPAG, pwd->pw_name,	/* kerberos name */
				   NULL,	/* instance */
				   NULL,	/* realm */
				   pword,	/* password */
				   0,	/* default lifetime */
				   0,	/* spare 1 */
				   0,	/* spare 2 */
				   &reason /* error string */ )) {
	afsLoginFail = 1;
	if (doafs)
	    return 0;
    } else {
	if (pwd->pw_passwd && strcmp(pwd->pw_passwd, "*"))
	    return 1;		/* Success */
    }

    if (!pwd->pw_passwd)
	return 0;
    namep = crypt(pword, pwd->pw_passwd);
    code = strcmp(namep, pwd->pw_passwd) == 0 ? 1 : 0;
    if ((code == 1) && afsLoginFail) {
	/* Only print this if local authentication is successful */
	printf("Unable to authenticate to AFS because %s\n", reason);
	printf("proceeding with local authentication...\n");
    }
    return code;
}


#endif

#ifdef	AFS_SUN5_ENV
static void
defaults()
{
    extern int defopen(), defcntl();
    register int flags;
    register char *ptr;

    if (defopen(Pndefault) == 0) {
	/*
	 * ignore case
	 */
	flags = defcntl(DC_GETFLAGS, 0);
	TURNOFF(flags, DC_CASE);
	defcntl(DC_SETFLAGS, flags);

	if ((Console = defread("CONSOLE=")) != NULL)
	    Console = strdup(Console);
	if ((Altshell = defread("ALTSHELL=")) != NULL)
	    Altshell = strdup(Altshell);
	if ((Passreq = defread("PASSREQ=")) != NULL)
	    Passreq = strdup(Passreq);
	if ((Def_tz = defread("TIMEZONE=")) != NULL)
	    Def_tz = strdup(Def_tz);
	if ((Def_hertz = defread("HZ=")) != NULL)
	    Def_hertz = strdup(Def_hertz);
	if ((Def_path = defread("PATH=")) != NULL)
	    Def_path = strdup(Def_path);
	if ((Def_supath = defread("SUPATH=")) != NULL)
	    Def_supath = strdup(Def_supath);
	if ((ptr = defread("ULIMIT=")) != NULL)
	    Def_ulimit = atol(ptr);
	if ((ptr = defread("TIMEOUT=")) != NULL)
	    Def_timeout = (unsigned)atoi(ptr);
	if ((ptr = defread("UMASK=")) != NULL)
	    if (sscanf(ptr, "%lo", &Umask) != 1)
		Umask = DEFUMASK;
	if ((ptr = defread("IDLEWEEKS=")) != NULL)
	    Idleweeks = atoi(ptr);
	if ((ptr = defread("SLEEPTIME=")) != NULL)
	    Sleeptime = atoi(ptr);
	(void)defopen(NULL);
    }
    return;
}
#endif

#ifdef	AFS_HPUX_ENV
/*
 * returns 1 if user should be counted as 1 user;
 * returns 3 if user should be counted as a pty
 * may return other stuff in the future
 */
int
add_count()
{
    if (isapty()) {
	if (is_nlio())
	    return (1);
	else
	    return (3);
    } else
	return (1);
}

/*
 * returns 1 if user is a pty, 0 otherwise
 */
#define SSTRPTYMAJOR 157	/* major number for Streams slave pty's */
#define SPTYMAJOR 17		/* major number for slave  pty's */
#define PTYSC     0x00		/* select code for pty's         */
#define select_code(x) (((x) & 0xff0000) >> 16)
isapty()
{
    struct stat sbuf;
    static int firsttime = 1;
    static int retval;

    if (firsttime) {
	firsttime = 0;
	if (fstat(0, &sbuf) != 0 ||	/* can't stat */
	    (sbuf.st_mode & S_IFMT) != S_IFCHR ||	/* not char special */
	    major(sbuf.st_rdev) != SPTYMAJOR ||	/* not pty major num */
	    select_code(sbuf.st_rdev) != PTYSC) {	/* not pty minor num */
	    /* Check to see if it is a Streams PTY */
	    if (major(sbuf.st_rdev) == SSTRPTYMAJOR) {
		retval = 1;
	    } else
		retval = 0;	/* neither a BSD nor a Streams PTY */
	} else {
	    retval = 1;
#ifdef TRUX
	    if (ISB1 && !remote_login)
		retval = 0;	/* local pty for td tty simulation */
#endif /* TRUX */
	}
    }
    return (retval);
}

#define NLIOUCOUNT	_IO('K',28)	/* _IO(K, 28) */
#define NLIOSERV	0x4b33	/* (('K' << 8) + 51 */

/*
 * returns 1 if user is an NL server, else returns 0
 */
is_nlio()
{
    if (ioctl(0, NLIOUCOUNT) == NLIOSERV)
	return (1);
    else
	return (0);
}
#endif
