/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of IBM not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
IBM BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/
/* $ACIS: $ */

/* ALSO utimes and strip the file

Generic install command.  Options are:
	-s 		strip the file	(default for executable files with no extension)
	-ns		do not strip the file	(default for other files)
	-c		ignored for compatability
	-m <mode>	chmod to this value
	-o <user>	chown to this user
	-g <group>	chgrp to this group
	-f		target path is a file
	-q		be very, very quick and quiet
	-l <envcwd>	attempt symbolic link back from destination to source
			with the current directory in the specified environment
			variable
*/

#define MAXFILES 200
#define BUFSIZE 32768
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/pinstall/Attic/install.c,v 1.23 2003/08/08 21:54:44 shadow Exp $");

#include <stdio.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#ifdef	AFS_SUN5_ENV
#include <fcntl.h>
#include <string.h>
#include <elf.h>
#else
#ifdef AFS_DARWIN_ENV
#include <fcntl.h>
#include <string.h>
#else
#include <strings.h>
#include <a.out.h>
#endif
#endif
#ifdef	AFS_HPUX_ENV
#include <utime.h>
#endif
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <stdio.h>

static struct stat istat, ostat;
static int stripcalled = 0;

/* How many systems don't have strerror now? */
#ifndef HAVE_STRERROR
#if !defined(AFS_DARWIN60_ENV) && !defined(AFS_FBSD50_ENV)
extern int sys_nerr;
#endif
#if !defined(AFS_LINUX20_ENV) && !defined(AFS_DARWIN_ENV) && !defined(AFS_XBSD_ENV)
extern char *sys_errlist[];
#endif
char *ErrorString(int aerrno);
#else
#define ErrorString strerror
#endif

/* static prototypes */
int stripName(char *aname);
int atoo(register char *astr);


#if defined(AFS_HPUX_ENV) && !defined(AFS_HPUX102_ENV)
int
utimes(char *file, struct timeval tvp[2])
{
    struct utimbuf times;

    times.actime = tvp[0].tv_sec;
    times.modtime = tvp[1].tv_sec;
    return (utime(file, &times));
}
#endif

static char *
strrpbrk(char *s, char *set)
{
    char sets[256];
    int i;

    memset(sets, 0, sizeof(sets));
    while (*set)
	sets[(int)*set++] = 1;
    i = strlen(s);
    while (i > 0)
	if (sets[(int)s[--i]])
	    return &s[i];
    return 0;
}

#ifndef HAVE_STRERROR
char *
ErrorString(int aerrno)
{
    static char tbuffer[100];
    if (aerrno < 0 || aerrno >= sys_nerr) {
	sprintf(tbuffer, "undefined error code %d", aerrno);
    } else {
	strcpy(tbuffer, sys_errlist[aerrno]);
    }
    return tbuffer;
}
#endif

int
stripName(char *aname)
{
    if (strrchr(aname, '.') == 0)
	return 1;
    else
	return 0;
}

int
atoo(register char *astr)
{
    register afs_int32 value;
    register char tc;
    value = 0;
    while ((tc = *astr++)) {
	value <<= 3;
	value += tc - '0';
    }
    return value;
}

#if	defined(AFS_HPUX_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_DECOSF_ENV) || defined(AFS_SGI_ENV) || defined(AFS_LINUX20_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_OBSD_ENV) || defined(AFS_NBSD_ENV)
/*
 * Implementation lifted from that for AIX 3.1, since there didn't seem to be any
 * reason why it wouldn't work.
 */
static int
quickStrip(char *iname, char *oname, int ignored, int copy_only)
{
    int pid;
    pid_t status;
    static char *env[] = {
	0,
    };
    static char *strip[] = {
	"strip", 0, 0,
    };
    static char *copy[] = {
	"cp", 0, 0, 0,
    };

    /*
     * first, copy the `iname' to the `oname'
     */
    switch (pid = fork()) {
    case -1:			/* error        */
	perror("fork");
	return -1;

    case 0:			/* child        */
	copy[1] = iname;
	copy[2] = oname;
	execve("/bin/cp", copy, env);
	perror("/bin/cp");
	exit(1);

    default:			/* parent       */
	if (waitpid(pid, &status, 0) != pid && errno != ECHILD) {
	    perror("waitpid");
	    return -1;
	}
    }

    if (status != 0) {
	fprintf(stderr, "Bad exit code from /bin/cp: %d\n", status);
	return -1;
    }

    /*
     * need to do a chmod to guarantee that the perms will permit
     * the strip.  Perms are fixed up later.
     */
    if (chmod(oname, 0700)) {
	perror("chmod");
	return -1;
    }
#if !defined(AFS_OBSD_ENV) && !defined(AFS_NBSD_ENV)
    /*
     * done the copy, now strip if desired.
     */
    if (copy_only)
	return 0;

    switch (pid = fork()) {
    case -1:			/* error        */
	perror("fork");
	return -1;

    case 0:			/* child        */
	strip[1] = oname;
#ifdef	AFS_SUN5_ENV
#define	STRIP_BIN	"/usr/ccs/bin/strip"
#elif defined(AFS_LINUX20_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV)
#define STRIP_BIN	"/usr/bin/strip"
#else
#define	STRIP_BIN	"/bin/strip"
#endif
	execve(STRIP_BIN, strip, env);
	perror(STRIP_BIN);
	exit(1);

    default:			/* parent       */
	if (waitpid(pid, &status, 0) != pid && errno != ECHILD) {
	    perror("waitpid");
	    return -1;
	}
    }
#endif

    return status;
}

#else
#ifdef AFS_AIX_ENV
#ifdef AFS_AIX32_ENV
/*
 * whoa! back up and be a little more rational (every little bit helps in
 * aix_31).
 */
static int
quickStrip(char *iname, char *oname, int ignored, int copy_only)
{
    int pid, status;
    static char *env[] = {
	0,
    };
    static char *strip[] = {
	"strip", 0, 0,
    };
    static char *copy[] = {
	"cp", 0, 0, 0,
    };

    /*
     * first, copy the `iname' to the `oname'
     */
    switch (pid = fork()) {
    case -1:			/* error        */
	perror("fork");
	return -1;

    case 0:			/* child        */
	copy[1] = iname;
	copy[2] = oname;
	execve("/bin/cp", copy, env);
	perror("/bin/cp");
	exit(1);

    default:			/* parent       */
	if (waitpid(pid, &status, 0) != pid && errno != ECHILD) {
	    perror("waitpid");
	    return -1;
	}
    }

    if (status != 0) {
	fprintf(stderr, "Bad exit code from /bin/cp: %d\n", status);
	return -1;
    }

    /*
     * need to do a chmod to guarantee that the perms will permit
     * the strip.  Perms are fixed up later.
     */
    if (chmod(oname, 0700)) {
	perror("chmod");
	return -1;
    }

    /*
     * done the copy, now strip if desired.
     */
    if (copy_only)
	return 0;

    switch (pid = fork()) {
    case -1:			/* error        */
	perror("fork");
	return -1;

    case 0:			/* child        */
	strip[1] = oname;
	execve("/bin/strip", strip, env);
	perror("/bin/strip");
	exit(1);

    default:			/* parent       */
	if (waitpid(pid, &status, 0) != pid && errno != ECHILD) {
	    perror("waitpid");
	    return -1;
	}
    }

    return status;
}

#endif /* AFS_AIX32_ENV        */
#else /* !AFS_AIX_ENV         */

#ifdef	mips
#include "sex.h"
int
quickStrip(int fd, afs_int32 asize)
{
    FILHDR fheader;
    int dum, newlen;
    int mysex, swapheader;

    /* Read the file header, if it is one. */
    if (lseek(fd, 0, L_SET) == -1) {
	printf("Initial lseek failed while stripping file: %s\n",
	       ErrorString(errno));
	return -1;
    }
    dum = read(fd, (char *)&fheader, sizeof(fheader));
    /* Fail on I/O error */
    if (dum < 0) {
	printf("Initial read failed while stripping: %s\n",
	       ErrorString(errno));
	return -1;
    }
    /* If the file is smaller than a file header, forget it. */
    if (dum != sizeof(fheader))
	return 0;
#ifdef AFS_DECOSF_ENV
    mysex = LITTLEENDIAN;
#else
    mysex = gethostsex();
    if (mysex != BIGENDIAN && mysex != LITTLEENDIAN)
	return 0;
#endif /* DEC OSF */
    swapheader = 0;
    if (fheader.f_magic == MIPSELMAGIC) {
	if (mysex == BIGENDIAN)
	    swapheader = 1;
    } else if (fheader.f_magic == MIPSEBMAGIC) {
	if (mysex == LITTLEENDIAN)
	    swapheader = 1;
    } else
	return 0;		/* not executable */
#ifdef AFS_DECOSF_ENV
    if (swapheader)
	return 0;
#else
    if (swapheader)
	swap_filehdr(&fheader, gethostsex());
#endif /* DEC OSF */
    /* Already stripped? */
    if (fheader.f_symptr == 0 || fheader.f_nsyms == 0)
	return 0;
    /* Strip it.  Zero out the symbol pointers. */
    newlen = fheader.f_symptr;
    fheader.f_symptr = 0;
    fheader.f_nsyms = 0;
#ifndef AFS_DECOSF_ENV
    if (swapheader)
	swap_filehdr(&fheader, gethostsex());
#endif /* DEC OSF */
    if (lseek(fd, 0, L_SET) == -1)
	return -1;
    if (write(fd, (char *)&fheader, sizeof(fheader)) != sizeof(fheader))
	return -1;
/* Now truncate the file itself. */
    if (ftruncate(fd, newlen) != 0)
	return -1;
    return 0;
}
#else /* !mips */
static int
quickStrip(int afd, afs_int32 asize)
{
    int n, bytesLeft;
    struct exec buf;
    struct exec *head;
    n = lseek(afd, 0, 0);
    if (n < 0) {
	printf("Initial lseek failed while stripping file: %s\n",
	       ErrorString(errno));
	return -1;
    }
    n = read(afd, &buf, sizeof(buf));
    if (n < 0) {
	printf("Initial read failed while stripping: %s\n",
	       ErrorString(errno));
	return -1;
    }
    head = &buf;
    if (n >= sizeof(*head) && !N_BADMAG(*head)) {	/* This code lifted from strip.c. */
	bytesLeft = (afs_int32) head->a_text + head->a_data;
	head->a_syms = head->a_trsize = head->a_drsize = 0;
	if (head->a_magic == ZMAGIC)
	    bytesLeft += N_TXTOFF(*head) - sizeof(*head);
	/* also include size of header */
	bytesLeft += sizeof(*head);
	n = lseek(afd, 0, 0);
	if (n < 0) {
	    printf("lseek failed while stripping file: %s\n",
		   ErrorString(errno));
	    return -1;
	}
	n = write(afd, &buf, sizeof(buf));
	if (n < 0) {
	    printf("write failed while stripping file: %s\n",
		   ErrorString(errno));
	    return -1;
	}
    } else
	bytesLeft = 0;

    /* check if size of stripped file is same as existing file */
    if (bytesLeft != 0 && bytesLeft != asize) {
	if (ftruncate(afd, bytesLeft) < 0) {
	    printf("ftruncate failed after stripping file: %s\n",
		   ErrorString(errno));
	    return -1;
	}
    }
    return 0;
}
#endif /* mips */
#endif
#endif /* AFS_HPUX_ENV */

#include "AFS_component_version_number.c"

int
main(int argc, char *argv[])
{
    int setOwner, setMode, setGroup, ifd, ofd;
    afs_int32 mode = 0, owner, group;
    struct passwd *tpw;
    struct group *tgp;
    char *fnames[MAXFILES], *newNames[MAXFILES];
    afs_int32 rcode, code;
    char *dname;
    char pname[1024];
#if defined (AFS_HPUX_ENV)
    char pnameBusy[1024];
#endif /* AFS_HPUX_ENV */
    char pnametmp[1024];
    int pnamelen;
    afs_int32 newcode;
    static char diskBuffer[BUFSIZE];	/* must be static to avoid compiler bugs for large stuff */
    char myHostName[100];
    struct timeval tvp[2];
    int isDir;
    int strip;
    int fptr;
    register char *tp;
    register afs_int32 i;

#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a 
     * crash (i.e. core is generated) we can include the user's data section 
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    fptr = 0;
    rcode = 0;
    strip = -1;			/* don't know yet */
    owner = 0;
    setOwner = 0;
    setMode = 0;
    group = 0;
    setGroup = 0;
    isDir = -1;			/* don't know yet */

    for (i = 1; i < argc; i++) {
	tp = argv[i];
	if (tp[0] == '-') {	/* a switch */
	    if (!strcmp(tp, "-m"))
		mode = atoo(argv[++i]), setMode = 1;
	    else if (!strcmp(tp, "-s"))
		strip = 1;
	    else if (!strcmp(tp, "-ns"))
		strip = 0;
	    else if (!strcmp(tp, "-c"))	/* nothing */
		;
	    else if (!strcmp(tp, "-f"))
		isDir = 0;	/* => dest is file */
	    else if (!strcmp(tp, "-o")) {	/* look up the dude */
		tpw = getpwnam(argv[++i]);
		if (!tpw) {
		    printf("User %s not found in passwd database, ignored\n",
			   argv[i]);
		} else {
		    owner = tpw->pw_uid;
		    setOwner = 1;
		}
	    } else if (!strcmp(tp, "-g")) {	/* look up the dude */
		tgp = getgrnam(argv[++i]);
		if (!tgp) {
		    printf("Group %s not found in passwd database; ignored\n",
			   argv[i]);
		} else {
		    group = tgp->gr_gid;
		    setGroup = 1;
		}
	    } else {
		printf("Bad switch %s\n", argv[i]);
		exit(1);
	    }
	} else {		/* a file name */
	    if (fptr >= MAXFILES) {
		printf("Too many files on command line, max is %d\n",
		       MAXFILES);
		exit(1);
	    }
	    fnames[fptr++] = argv[i];
	}
    }

    /* we've parse the commands, now *do* them */

    /* otherwise we are doing a local install, so we do the work for each file
     * here the last name in the fname array is the dir in which to put all
     * this stuff */

    if (fptr < 2) {
	printf("Not enough file names\n");
	exit(1);
    }

    /* N file usage requires last argument to be a directory.  If -f was
     * specified it is an error.  In the 2 file usage when -f is not specified
     * use a heuristic.  If the ends of the two pathnames are equal then assume
     * the target is a file, otherwise assume it is a directory. */

    if ((fptr > 2) && (isDir == 0)) {
	printf
	    ("target must be a directory, don't use multiple source files with -f switch\n");
	exit(1);
    } else if (fptr > 2)
	isDir = 1;
    else if (isDir != 0) {
	char *targetSuffix;
	char *sourceSuffix;

	targetSuffix = strrpbrk(fnames[1], "./");
	sourceSuffix = strrpbrk(fnames[0], "./");
	if (sourceSuffix == 0) {
	    sourceSuffix = fnames[0];
	    if (targetSuffix == 0)
		targetSuffix = fnames[1];
	    else
		targetSuffix++;
	} else if (targetSuffix == 0)
	    targetSuffix = fnames[1];
	if (strcmp(targetSuffix, sourceSuffix) == 0)
	    isDir = 0;
    }

    dname = fnames[--fptr];
    if (stat(dname, &istat) < 0) {
	if ((errno == ENOENT) || (errno == ENOTDIR)) {
	    /* create path */
	    char protopath[BUFSIZ];
	    int i = 0;
	    char c;
	    while (dname[i]) {
		do {
		    protopath[i] = dname[i];
		    c = dname[++i];	/* next char */
		} while (!((c == 0) || (c == '/')));
		protopath[i] = 0;

		/* don't mkdir last component if target is a file */
		if ((c == 0) && (isDir == 0))
		    break;

		/* else create dir component if it doesn't exist */
		code = stat(protopath, &istat);
		if (code && (errno == ENOENT)) {
		    code = mkdir(protopath, 0755);
		    if (code) {
			printf("Can't create destination path at %s\n",
			       protopath);
			exit(1);
		    }
		}
	    }			/* while dname not exhausted */
	    if (isDir == -1)
		isDir = 1;
	} else {
	    printf("Can't stat destination ``%s'': %s\n", dname,
		   ErrorString(errno));
	    exit(1);
	}
    } else {
	if ((istat.st_mode & S_IFMT) == S_IFDIR)
	    isDir = 1;
	else
	    isDir = 0;
    }

    /* either can be n files and one dir, or one file and one target */
    if (!isDir && fptr != 1) {
	printf("target for multiple files must be a dir\n");
	exit(1);
    }

    for (i = 0; i < fptr; i++) {	/* figure out name to put as entry name for file */
	tp = strrchr(fnames[i], '/');
	if (tp)
	    newNames[i] = tp + 1;
	else
	    newNames[i] = fnames[i];
    }
    for (i = 0; i < fptr; i++) {	/* copy newName[i] into directory dname */

	/* pname is target file in either case */
	if (isDir) {
	    strcpy(pname, dname);
	    strcat(pname, "/");
	    strcat(pname, newNames[i]);
	} else
	    strcpy(pname, dname);
	strcpy(pnametmp, pname);
	/* Make up a temporary name for a destination */
	pnamelen = strlen(pnametmp);
	gethostname(myHostName, sizeof(myHostName) - 1);	/* lv room for null */
	if (pnamelen > 1020 - strlen(myHostName))
	    pnamelen = 1020 - strlen(myHostName);
	pnametmp[pnamelen] = '.';
	strcpy(&pnametmp[pnamelen + 1], myHostName);
	if (strcmp(fnames[i], pnametmp) == 0)
	    strcpy(&pnametmp[pnamelen], ".NeW");

	ifd = open(fnames[i], O_RDONLY, 0);
	if (ifd < 0) {
	    printf("Can't open source file ``%s'': %s\n", fnames[i],
		   ErrorString(errno));
	    rcode = 1;
	    continue;
	}
	if (fstat(ifd, &istat) < 0) {
	    printf("Cound not fstat input file ``%s'': %s; skipping it\n",
		   fnames[i], ErrorString(errno));
	    close(ifd);
	    rcode = 1;
	    continue;
	}
	if (lstat(pname, &ostat) == 0) {
	    if ((ostat.st_size == istat.st_size)
		&& (ostat.st_mtime == istat.st_mtime) && ((!setMode)
							  ||
							  ((ostat.
							    st_mode & S_IFMT)
							   == mode))
		&& ((!setOwner) || (ostat.st_uid == owner)) && ((!setGroup)
								|| (ostat.
								    st_gid ==
								    group))) {
		close(ifd);
		printf("No changes to %s since %s installed\n", fnames[i],
		       pname);
		continue;
	    }
	}
#if	defined(AFS_AIX_ENV) || defined(AFS_HPUX_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_DECOSF_ENV) || defined(AFS_SGI_ENV) || defined(AFS_LINUX20_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_OBSD_ENV) || defined(AFS_NBSD_ENV)
	stripcalled = 0;
	if (strip == 1 || ((strip == -1 && ((istat.st_mode & 0111) == 0111)
			    && stripName(newNames[i]))))
	    stripcalled = 1;
	if (!stripcalled) {
	    /* Simply copy target to dest */
	    quickStrip(fnames[i], pnametmp, istat.st_size, 1);
	} else {
	    if (quickStrip(fnames[i], pnametmp, istat.st_size, 0) < 0) {
		printf
		    ("...strip failed for output temp file ``%s''; skipping it\n",
		     pnametmp);
		close(ifd);
		unlink(pnametmp);
		rcode = 1;
		continue;
	    }
	}
	close(ifd);

	ofd = open(pnametmp, O_RDWR, 0);
	if (ofd < 0) {
	    printf("Could not open output temp file ``%s'': %s\n", pnametmp,
		   ErrorString(errno));
	    close(ifd);
	    rcode = 1;
	    continue;
	}
	if (!setMode)
	    mode = istat.st_mode;	/* Was 0755:> this is the default for our rcs to work */
#else /* AFS_AIX_ENV */
	/* check to see if this file is hard to duplicate */
	ofd = open(pnametmp, O_RDWR | O_TRUNC | O_CREAT, 0666);
	if (ofd < 0) {
	    printf("Could not create output temp file ``%s'': %s\n", pnametmp,
		   ErrorString(errno));
	    close(ifd);
	    rcode = 1;
	    continue;
	}
	if (!setMode)
	    mode = istat.st_mode;	/* Was 0755:> this is the default for our rcs to work */
	/* here both files are open and ready to go */
	while (1) {
	    code = read(ifd, diskBuffer, BUFSIZE);
	    if (code == 0)
		break;
	    if (code < 0) {
		printf("READ ERROR %d: %s\n", errno, ErrorString(errno));
		break;
	    }
	    errno = 0;
	    newcode = write(ofd, diskBuffer, code);
	    if (newcode != code) {
		printf("WRITE ERROR %d: %s\n", errno, ErrorString(errno));
		break;
	    }
	}
	if (code != 0) {
	    rcode = 1;		/* an error occurred copying the file */
	    printf
		("Warning: Error occurred writing output temp file %s; skipping it\n",
		 pnametmp);
	    close(ifd);
	    unlink(pnametmp);
	    close(ofd);
	    continue;		/* to the next file */
	}
	/* strip the file? */
	if (strip == 1 || (strip == -1 && ((istat.st_mode & 0111) == 0111)
			   && stripName(newNames[i])))
	    if (quickStrip(ofd, istat.st_size) < 0) {
		printf
		    ("...strip failed for output temp file ``%s''; skipping it\n",
		     pnametmp);
		close(ifd);
		unlink(pnametmp);
		rcode = 1;
		continue;
	    }

	/* do the chmod, etc calls before closing the file for max parallelism on store behind */
	close(ifd);

#endif /* AFS_AIX_ENV */
	if (fchmod(ofd, mode) < 0) {
	    printf("Couldn't chmod output temp file ``%s'': %s\n", pnametmp,
		   ErrorString(errno));
	    unlink(pnametmp);
	    close(ofd);
	    rcode = 1;
	    continue;
	}

	tvp[0].tv_sec = istat.st_atime;
	tvp[0].tv_usec = 0;
	tvp[1].tv_sec = istat.st_mtime;
	tvp[1].tv_usec = 0;
	if (utimes(pnametmp, tvp) < 0) {
	    printf("Couldn't utimes output temp file ``%s'': %s\n", pnametmp,
		   ErrorString(errno));
	    unlink(pnametmp);
	    close(ofd);
	    rcode = 1;
	    continue;
	}
	code = close(ofd);
	if (code != 0) {
	    printf("Warning: Could not close output temp file %s (%s)\n",
		   pnametmp, ErrorString(errno));
	    unlink(pnametmp);
	    rcode = 1;		/* an error occurred closing the output file */
	    continue;		/* to the next file */
	}

	/* do this later so vice doesn't see chown of unstored file */
	if (setOwner || setGroup)
	    if (chown
		(pnametmp, (setOwner ? owner : -1),
		 (setGroup ? group : -1)) < 0) {
		printf("Couldn't set %s for output temp file %s: %s\n",
		       (setOwner ? (setGroup ? "owner and group" : "owner") :
			"group"), pnametmp, ErrorString(errno));
		unlink(pnametmp);
		rcode = 1;
		continue;
	    }

	if (rename(pnametmp, pname) < 0) {
#if defined(AFS_HPUX_ENV)
	    if (errno == ETXTBSY) {
		(void)strcpy(pnameBusy, pname);
		(void)strcat(pnameBusy, ".BUSY");
		if (rename(pname, pnameBusy) == 0) {
		    fprintf(stdout, "Had to leave old file: %s.\n",
			    pnameBusy);
		    fprintf(stdout,
			    "Please delete this file when the program using it is finished.\n");
		    if (rename(pnametmp, pname) < 0) {
#endif /* AFS_HPUX_ENV */

			printf
			    ("Couldn't rename temp file %s to be output file %s: %s\n",
			     pnametmp, pname, ErrorString(errno));
			unlink(pnametmp);
			rcode = 1;
			continue;

#if defined(AFS_HPUX_ENV)
		    }
		} else {
		    fprintf(stderr,
			    "Couldn't move busy target file %s to make room for new version %s: %s\n",
			    pname, pnametmp, ErrorString(errno));
		    if (errno == ETXTBSY) {
			fprintf(stderr,
				"Try terminating any programs using the file %s and then re-run %s.\n",
				pnameBusy, argv[0]);
		    }
		    unlink(pnametmp);
		    rcode = 1;
		    continue;
		}
	    }
#endif /* AFS_HPUX_ENV */
	}
    }
    /* all done now */
    exit(rcode);
}
