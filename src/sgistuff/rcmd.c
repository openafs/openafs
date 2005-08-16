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

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#ifdef aiws /*AIX*/
#include <sys/types.h>
#define MAXHOSTNAMELEN	64	/* use BSD's, not sys/param's */
#define MAXPATHLEN	1024	/* from BSD */
#include <sys/ioctl.h>		/* for SIOCSPGRP */
#endif
#include <stdio.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/param.h>
#include <limits.h>
#include <sys/file.h>
#ifdef	AFS_SUN5_ENV
#include <fcntl.h>
#endif
#include <unistd.h>		/* select() prototype */
#include <sys/types.h>		/* fd_set on older platforms */
#include <sys/time.h>		/* struct timeval, select() prototype */
#ifndef	FD_SET
# include <sys/select.h>	/* fd_set on newer platforms */
#endif
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#if defined(AFS_HPUX_ENV)
/* HPUX uses a different call to set[res][gu]ids: */
#define seteuid(newEuid)	setresuid(-1, (newEuid), -1)
#define setegid(newEgid)	setresgid(-1, (newEgid), -1)
#endif /* defined(AFS_HPUX_ENV) */
#ifdef	TCP_DEBUG
#include <sys/syslog.h>
#	define	DPRINTF(args)	dprintf args
dprintf(args)
     char *args;
{
    char **argv;
    char buf[BUFSIZ];
    static char prefix[] = "rcmd: ";

    argv = &args;
    vsprintf(buf, argv[0], &argv[1]);
    syslog(LOG_DEBUG, "%s %s\n", prefix, buf);
}
#else
#	define	DPRINTF(args)
#endif
#include <syslog.h>
static _checkhost();

#ifdef AFS_HPUX102_ENV
int
rmcd(ahost, rport, locuser, remuser, cmd, fd2p)
     char **ahost;
     int rport;
     const char *locuser, *remuser, *cmd;
     int *fd2p;
#else
#ifdef	AFS_AIX32_ENV
rcmd(ahost, rport, locuser, remuser, cmd, fd2p, retry)
     int retry;
#else
rcmd(ahost, rport, locuser, remuser, cmd, fd2p)
#endif
     char **ahost;
     u_short rport;
#if defined(AFS_LINUX20_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV)
     const char *locuser, *remuser, *cmd;
#else
     char *locuser, *remuser, *cmd;
#endif
     int *fd2p;
#endif
{
    int s, timo = 1, pid;
    sigset_t oldset;
    sigset_t sigBlock;
    int someSignals[100];
    struct servent *sp, *getservbyport();
    struct sockaddr_in sin, from;
    char c;
    int lport = IPPORT_RESERVED - 1;
    struct hostent *hp;
    fd_set reads;

    memset((char *)someSignals, 0, sizeof(someSignals));
    someSignals[0] = 1 << (SIGURG - 1);
    sigBlock = *((sigset_t *) someSignals);

    pid = getpid();
    hp = gethostbyname(*ahost);	/* CAUTION: this uses global static */
    /* storage.  ANY OTHER CALLS to gethostbyname (including from within
     * syslog, eg) will trash the contents of hp. BE CAREFUL! */
    if (hp == 0) {
	herror(*ahost);
	return (-1);
    }
    *ahost = hp->h_name;
    /* was: syslog(LOG_ERR, "rcmd: host=%s, rport=%d, luser=%s,ruser=%s,cmd=%s,fd2p=%s\n", *ahost,rport,locuser,remuser,cmd,fd2p) 
     * but that trashes hp... */
    sigprocmask(SIG_BLOCK, &sigBlock, &oldset);
    for (;;) {
	s = rresvport(&lport);
	if (s < 0) {
	    if (errno == EAGAIN)
		fprintf(stderr, "socket: All ports in use\n");
	    else
		perror("rcmd: socket");
	    sigprocmask(SIG_SETMASK, &oldset, (sigset_t *) 0);
	    return (-1);
	}
#ifdef	AFS_AIX32_ENV
#ifndef aiws
	fcntl(s, F_SETOWN, pid);
#else
	/* since AIX has no F_SETOWN, we just do the ioctl */
	(void)ioctl(s, SIOCSPGRP, &pid);
#endif
#else
#if !defined(AFS_AIX_ENV) && !defined(AFS_HPUX_ENV)
	fcntl(s, F_SETOWN, pid);
#endif /* !defined(AIX) */
#endif
	sin.sin_family = hp->h_addrtype;
#ifdef	AFS_OSF_ENV
	memcpy((caddr_t) & sin.sin_addr, hp->h_addr_list[0], hp->h_length);
#else
	memcpy((caddr_t) & sin.sin_addr, hp->h_addr, hp->h_length);
#endif
	sin.sin_port = rport;
	/* attempt to remote authenticate first... */
	sp = getservbyport((int)rport, "tcp");
	if (sp) {
	    int ok = 0;

	    switch (ta_rauth(s, sp->s_name, sin.sin_addr)) {
	    case 0:
#ifndef	AFS_SGI_ENV
		fprintf(stderr, "No remote authentication\n");
#endif
		close(s);
		s = rresvport(&lport);
		if (s < 0) {
		    if (errno == EAGAIN)
			fprintf(stderr, "socket: All ports in use\n");
		    else
			perror("rcmd: socket");
		    sigprocmask(SIG_SETMASK, &oldset, (sigset_t *) 0);
		    return (-1);
		}
#if !defined(AFS_AIX_ENV) && !defined(AFS_HPUX_ENV)
		fcntl(s, F_SETOWN, pid);
#endif /* !defined(AIX) */
		break;
	    case 1:
		ok = 1;
		break;
	    case -1:
		fprintf(stderr, "Login incorrect.");
		exit(1);
		break;
	    case -2:
		fprintf(stderr, "internal failure, ta_rauth\n");
		exit(errno);
		break;
	    case -3:
		fprintf(stderr, "Cannot connect to remote machine\n");
		exit(errno);
		break;
	    }

	    if (ok) {
		break;		/* from for loop */
	    }
	}
	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) >= 0)
	    break;
	(void)close(s);
	if (errno == EADDRINUSE) {
	    lport--;
	    continue;
	}
	if (errno == ECONNREFUSED && timo <= 16) {
#ifdef	AFS_AIX32_ENV
	    if (!retry) {
		return (-2);
	    }
#endif
	    sleep(timo);
	    timo *= 2;
	    continue;
	}
	if (hp->h_addr_list[1] != NULL) {
	    int oerrno = errno;

	    fprintf(stderr, "connect to address %s: ",
		    inet_ntoa(sin.sin_addr));
	    errno = oerrno;
	    perror(0);
	    hp->h_addr_list++;
	    memcpy((caddr_t) & sin.sin_addr, hp->h_addr_list[0],
		   hp->h_length);
	    fprintf(stderr, "Trying %s...\n", inet_ntoa(sin.sin_addr));
	    continue;
	}
	perror(hp->h_name);
	sigprocmask(SIG_SETMASK, &oldset, (sigset_t *) 0);
	return (-1);
    }
    lport--;
    if (fd2p == 0) {
	write(s, "", 1);
	lport = 0;
    } else {
	char num[8];
	int s2 = rresvport(&lport), s3;
	int len = sizeof(from);
	int maxfd = -1;

	if (s2 < 0)
	    goto bad;
	listen(s2, 1);
	(void)sprintf(num, "%d", lport);
	if (write(s, num, strlen(num) + 1) != strlen(num) + 1) {
	    perror("write: setting up stderr");
	    (void)close(s2);
	    goto bad;
	}
	FD_ZERO(&reads);
	FD_SET(s, &reads);
	if (maxfd < s)
	    maxfd = s;
	FD_SET(s2, &reads);
	if (maxfd < s2)
	    maxfd = s2;
	errno = 0;
	if (select(maxfd + 1, &reads, 0, 0, 0) < 1 || !FD_ISSET(s2, &reads)) {
	    if (errno != 0)
		perror("select: setting up stderr");
	    else
		fprintf(stderr,
			"select: protocol failure in circuit setup.\n");
	    (void)close(s2);
	    goto bad;
	}
	s3 = accept(s2, (struct sockaddr *)&from, &len);
	(void)close(s2);
	if (s3 < 0) {
	    perror("accept");
	    lport = 0;
	    goto bad;
	}
	*fd2p = s3;
	from.sin_port = ntohs((u_short) from.sin_port);
	if (from.sin_family != AF_INET || from.sin_port >= IPPORT_RESERVED
	    || from.sin_port < IPPORT_RESERVED / 2) {
	    fprintf(stderr, "socket: protocol failure in circuit setup.\n");
	    goto bad2;
	}
    }
    (void)write(s, locuser, strlen(locuser) + 1);
    (void)write(s, remuser, strlen(remuser) + 1);
    (void)write(s, cmd, strlen(cmd) + 1);
    errno = 0;
    if (read(s, &c, 1) < 0) {
	perror(*ahost);
	goto bad2;
    }
    if (c != 0) {
#ifdef	AFS_OSF_ENV
	/*
	 *   Two different protocols seem to be used;
	 *   one prepends a "message" byte with a "small"
	 *   number; the other one just sends the message
	 */
	if (isalnum(c))
	    (void)write(2, &c, 1);

#endif
	while (read(s, &c, 1) == 1) {
	    (void)write(2, &c, 1);
	    if (c == '\n')
		break;
	}
	goto bad2;
    }
    sigprocmask(SIG_SETMASK, &oldset, (sigset_t *) 0);
    return (s);
  bad2:
    if (lport)
	(void)close(*fd2p);
  bad:
    (void)close(s);
    sigprocmask(SIG_SETMASK, &oldset, (sigset_t *) 0);
    return (-1);
}

#ifndef AFS_AIX32_ENV
rresvport(alport)
     int *alport;
{
    struct sockaddr_in sin;
    int s;

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
	return (-1);
    for (;;) {
	sin.sin_port = htons((u_short) * alport);
	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) >= 0)
	    return (s);
	if (errno != EADDRINUSE) {
	    (void)close(s);
	    return (-1);
	}
	(*alport)--;
	if (*alport == IPPORT_RESERVED / 2) {
	    (void)close(s);
	    errno = EAGAIN;	/* close */
	    return (-1);
	}
    }
}
#endif

int _check_rhosts_file = 1;

#if defined(AFS_HPUX102_ENV) || defined(AFS_LINUX20_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV)
ruserok(rhost, superuser, ruser, luser)
     const char *rhost;
     int superuser;
     const char *ruser, *luser;
#else
ruserok(rhost, superuser, ruser, luser)
     int superuser;
     char *rhost;
     char *ruser, *luser;
#endif
{
    FILE *hostf;
    char fhost[MAXHOSTNAMELEN];
    int first = 1;
    register char *sp, *p;
    int baselen = -1;
    int suid, sgid;
    int group_list_size = -1;
    gid_t groups[NGROUPS_MAX];
    sp = rhost;
    p = fhost;
    while (*sp) {
	if (*sp == '.') {
	    if (baselen == -1)
		baselen = sp - rhost;
	    *p++ = *sp++;
	} else {
	    *p++ = isupper(*sp) ? tolower(*sp++) : *sp++;
	}
    }
    *p = '\0';
    hostf = superuser ? (FILE *) 0 : fopen("/etc/hosts.equiv", "r");
  again:
    if (hostf) {
	if (!_validuser(hostf, fhost, luser, ruser, baselen)) {
	    (void)fclose(hostf);
#ifdef	AFS_OSF_ENV
	    if (first == 0) {
		(void)seteuid(suid);
		(void)setegid(sgid);
		if (group_list_size >= 0)
		    (void)setgroups(group_list_size, groups);
	    }
#endif
	    return (0);
	}
	(void)fclose(hostf);
    }
    if (first == 1 && (_check_rhosts_file || superuser)) {
	struct stat sbuf;
	struct passwd *pwd, *getpwnam();
	char pbuf[MAXPATHLEN];

	first = 0;
	suid = geteuid();
	sgid = getegid();
	group_list_size = getgroups(NGROUPS_MAX, groups);
	if ((pwd = getpwnam(luser)) == NULL)
	    return (-1);
	if (setegid(pwd->pw_gid) >= 0)
	    (void)initgroups(luser, pwd->pw_gid);
	(void)seteuid(pwd->pw_uid);
	(void)strcpy(pbuf, pwd->pw_dir);
	(void)strcat(pbuf, "/.rhosts");
	if ((hostf = fopen(pbuf, "r")) == NULL)
	    goto bad;
	/*
	 * if owned by someone other than user or root or if
	 * writeable by anyone but the owner, quit
	 */
	if (fstat(fileno(hostf), &sbuf) || sbuf.st_uid
	    && sbuf.st_uid != pwd->pw_uid || sbuf.st_mode & 022) {
	    fclose(hostf);
	    goto bad;
	}
	goto again;
    }
  bad:
    if (first == 0) {
	(void)seteuid(suid);
	(void)setegid(sgid);
	if (group_list_size >= 0)
	    (void)setgroups(group_list_size, groups);
    }
    return (-1);
}

/* don't make static, used by lpd(8) */
_validuser(hostf, rhost, luser, ruser, baselen)
     char *rhost, *luser, *ruser;
     FILE *hostf;
     int baselen;
{
    char *user;
    char ahost[MAXHOSTNAMELEN * 4];
    register char *p;
#ifdef	AFS_AIX32_ENV
#include <arpa/nameser.h>
    int hostmatch, usermatch;
    char domain[MAXDNAME], *dp;

    dp = NULL;
    if (getdomainname(domain, sizeof(domain)) == 0)
	dp = domain;
#endif
    while (fgets(ahost, sizeof(ahost), hostf)) {
#ifdef	AFS_AIX32_ENV
	hostmatch = usermatch = 0;
	p = ahost;
	if (*p == '#' || *p == '\n')	/* ignore comments and blanks */
	    continue;
	while (*p != '\n' && *p != ' ' && *p != '\t' && *p != '\0')
	    p++;
	if (*p == ' ' || *p == '\t') {
	    *p++ = '\0';
	    while (*p == ' ' || *p == '\t')
		p++;
	    user = p;
	    while (*p != '\n' && *p != ' ' && *p != '\t' && *p != '\0')
		p++;
	} else
	    user = p;
	*p = '\0';
	/*
	 *  + - anything goes
	 *  +@<name> - group <name> allowed
	 *  -@<name> - group <name> disallowed
	 *  -<name> - host <name> disallowed
	 */
	if (ahost[0] == '+' && ahost[1] == 0)
	    hostmatch = 1;
	else if (ahost[0] == '+' && ahost[1] == '@')
	    hostmatch = innetgr(ahost + 2, rhost, NULL, dp);
	else if (ahost[0] == '-' && ahost[1] == '@') {
	    if (innetgr(ahost + 2, rhost, NULL, dp))
		return (-1);
	} else if (ahost[0] == '-') {
	    if (_checkhost(rhost, ahost + 1, baselen))
		return (-1);
	} else
	    hostmatch = _checkhost(rhost, ahost, baselen);
	if (user[0]) {
	    if (user[0] == '+' && user[1] == 0)
		usermatch = 1;
	    else if (user[0] == '+' && user[1] == '@')
		usermatch = innetgr(user + 2, NULL, ruser, dp);
	    else if (user[0] == '-' && user[1] == '@') {
		if (hostmatch && innetgr(user + 2, NULL, ruser, dp))
		    return (-1);
	    } else if (user[0] == '-') {
		if (hostmatch && !strcmp(user + 1, ruser))
		    return (-1);
	    } else
		usermatch = !strcmp(user, ruser);
	} else
	    usermatch = !strcmp(ruser, luser);
	if (hostmatch && usermatch)
	    return (0);
#else
	p = ahost;
	while (*p != '\n' && *p != ' ' && *p != '\t' && *p != '\0') {
	    *p = isupper(*p) ? tolower(*p) : *p;
	    p++;
	}
	if (*p == ' ' || *p == '\t') {
	    *p++ = '\0';
	    while (*p == ' ' || *p == '\t')
		p++;
	    user = p;
	    while (*p != '\n' && *p != ' ' && *p != '\t' && *p != '\0')
		p++;
	} else
	    user = p;
	*p = '\0';
	if (_checkhost(rhost, ahost, baselen)
	    && !strcmp(ruser, *user ? user : luser)) {
	    return (0);
	}
#endif
    }
    return (-1);
}

static
_checkhost(rhost, lhost, len)
     char *rhost, *lhost;
     int len;
{
    static char ldomain[MAXHOSTNAMELEN + 1];
    static char *domainp = NULL;
    static int nodomain = 0;
    register char *cp;

#ifdef	AFS_AIX32_ENV
    struct hostent *hp;
    long addr;

    /*
     * check for ip address and do a lookup to convert to hostname
     */
    if (isinet_addr(lhost) && (addr = inet_addr(lhost)) != -1
	&& (hp = gethostbyaddr(&addr, sizeof(addr), AF_INET)))
	lhost = hp->h_name;

#endif
    if (len == -1) {
#ifdef	AFS_AIX32_ENV
	/* see if hostname from file has a domain name */
	for (cp = lhost; *cp; ++cp) {
	    if (*cp == '.') {
		len = cp - lhost;
		break;
	    }
	}
#endif
	return (!strcmp(rhost, lhost));
    }
    if (strncmp(rhost, lhost, len))
	return (0);
    if (!strcmp(rhost, lhost))
	return (1);
#ifdef	AFS_AIX32_ENV
    if (*(lhost + len) != '\0' && *(rhost + len) != '\0')
#else
    if (*(lhost + len) != '\0')
#endif
	return (0);
    if (nodomain)
	return (0);
    if (!domainp) {
	if (gethostname(ldomain, sizeof(ldomain)) == -1) {
	    nodomain = 1;
	    return (0);
	}
	ldomain[MAXHOSTNAMELEN] = '\0';
	if ((domainp = strchr(ldomain, '.')) == (char *)NULL) {
	    nodomain = 1;
	    return (0);
	}
	for (cp = ++domainp; *cp; ++cp)
	    if (isupper(*cp))
		*cp = tolower(*cp);
    }
    return (!strcmp(domainp, rhost + len + 1));
}
