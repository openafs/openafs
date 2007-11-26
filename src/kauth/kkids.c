/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * ALL RIGHTS RESERVED
 */

/* These two needed for rxgen output to work */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <afs/fs_utils.h>
#else
#include <netinet/in.h>
#include <afs/venus.h>
#endif
#include <signal.h>
#include <afs/stds.h>
#include <rx/xdr.h>
#include <afs/prs_fs.h>
#include <stdlib.h>
#include <string.h>

#define MAXNAME 100
#define MAXSIZE 2048

static int using_child = 0;
static FILE *childin, *childout;	/* file pointers on pipe to kpwvalid */

/* this removes symlinks from the tail end of path names.
 * PRECONDITION: name must be either absolute ('/something') or
 * explictly relative to current directory ('./something')
 */
static int
simplify_name(char *orig_name, char *true_name)
{
    int thru_symlink;
    struct stat statbuff;

    thru_symlink = 0;

#ifdef AFS_NT40_ENV
    if (stat(orig_name, &statbuff) < 0) {
	*true_name = '\0';
	return 0;
    } else {
	strcpy(true_name, orig_name);
	return 1;
    }
#else /* !NT40 */
    {
	int link_chars_read;
	char *last_component;
	if (lstat(orig_name, &statbuff) < 0) {
	    /* if lstat fails, it's possible that it's transient, but 
	     * unlikely.  Let's hope it isn't, and continue... */
	    *true_name = '\0';
	    return 0;
	}

	/*
	 * The lstat succeeded.  If the given file is a symlink, substitute
	 * the contents of the link for the file name.
	 */
	if ((statbuff.st_mode & S_IFMT) == S_IFLNK) {
	    thru_symlink = 1;
	    link_chars_read = readlink(orig_name, true_name, 1024);
	    if (link_chars_read <= 0) {
		*true_name = '\0';
		return 0;
	    }

	    true_name[link_chars_read++] = '\0';

	    /*
	     * If the symlink is an absolute pathname, we're fine.  Otherwise, we
	     * have to create a full pathname using the original name and the
	     * relative symlink name.  Find the rightmost slash in the original
	     * name (we know there is one) and splice in the symlink contents.
	     */
	    if (true_name[0] != '/') {
		last_component = (char *)strrchr(orig_name, '/');
		strcpy(++last_component, true_name);
		strcpy(true_name, orig_name);
	    }
	} else
	    strcpy(true_name, orig_name);

	return 1;		/* found it */
    }
#endif /* !NT40 */
}


  /* We find our own location by:
   * 1. checking for an absolute or relative path name in argv[0]
   *         this is moderately system-dependant: argv[0] is just a convention.
   * 2. successively checking each component of PATH, and concatenating argv[0]
   *    onto it, then stating the result.
   * if it exists, it must be us, eh?
   * NB there may be possible security implications involving
   * symlinks; I think they are only relevant if the symlink points
   * directly at kpasswd, not when it points at kpasswd's parent directory. 
   */
static int
find_me(char *arg, char *parent_dir)
{
    char *bp;			/* basename pointer */
    char *dp;			/* dirname pointer */
    char *pathelt, orig_name[1024], truename[1022];

#define explicitname(a,b,c) \
        (  ((a) == '/') ||                                \
           (   ((a) == '.') &&                            \
               (   ((b) == '/') ||                        \
                   (   ((b) == '.') && ((c) == '/') )       \
               )                                        \
            )                                           \
         )

    if (strlen(arg) > 510)	/* just give up */
	return 0;

    *parent_dir = '\0';
    truename[0] = '\0';

    if (explicitname(arg[0], arg[1], arg[2])) {
	strcpy(orig_name, arg);
	simplify_name(orig_name, truename);
    } else {
	bp = (char *)strrchr(arg, '/');
	if (bp) {
	    orig_name[0] = '.';
	    orig_name[1] = '/';
	    strcpy(orig_name + 2, arg);
	    simplify_name(orig_name, truename);
	}
    }

    if (!truename[0]) {		/* didn't find it */
	char path[2046];

	dp = getenv("PATH");
	if (!dp)
	    return 0;
	strncpy(path, dp, 2045);

	for (pathelt = strtok(path, ":"); pathelt;
	     pathelt = strtok(NULL, ":")) {
	    strncpy(orig_name, pathelt, 510);

	    bp = orig_name + strlen(orig_name);
	    *bp = '/';		/* replace NUL with / */
	    strncpy(bp + 1, arg, 510);

	    if (simplify_name(orig_name, truename))
		break;
	}
    }
    if (!truename[0])		/* didn't find it */
	return 0;		/* give up */

    /* DID FIND IT */
    /*
     * Find rightmost slash, if any.
     */
    bp = (char *)strrchr(truename, '/');
    if (bp) {
	/*
	 * Found it.  Designate everything before it as the parent directory,
	 * everything after it as the final component.
	 */
	strncpy(parent_dir, truename, bp - truename);
	parent_dir[bp - truename] = 0;
	bp++;			/*Skip the slash */
    } else {
	/*
	 * No slash appears in the given file name.  Set parent_dir to the current
	 * directory, and the last component as the given name.
	 */
	strcpy(parent_dir, ".");
	bp = truename;
    }

    return 1;			/* found it */
}

#define SkipLine(str) { while (*str !='\n') str++; str++; }

/* this function returns TRUE (1) if the file is in AFS, otherwise false (0) */
static int
InAFS(register char *apath)
{
    struct ViceIoctl blob;
    register afs_int32 code;
    char space[MAXSIZE];

    blob.in_size = 0;
    blob.out_size = MAXSIZE;
    blob.out = space;

    code = pioctl(apath, VIOC_FILE_CELL_NAME, &blob, 1);
    if (code) {
	if ((errno == EINVAL) || (errno == ENOENT))
	    return 0;
    }
    return 1;
}

struct Acl {
    int nplus;
    int nminus;
    struct AclEntry *pluslist;
    struct AclEntry *minuslist;
};

struct AclEntry {
    struct AclEntry *next;
    char name[MAXNAME];
    afs_int32 rights;
};

static struct Acl *
ParseAcl(char *astr)
{
    int nplus, nminus, i, trights;
    char tname[MAXNAME];
    struct AclEntry *first, *last, *tl;
    struct Acl *ta;
    sscanf(astr, "%d", &nplus);
    SkipLine(astr);
    sscanf(astr, "%d", &nminus);
    SkipLine(astr);

    ta = (struct Acl *)malloc(sizeof(struct Acl));
    ta->nplus = nplus;

    last = 0;
    first = 0;
    for (i = 0; i < nplus; i++) {
	sscanf(astr, "%100s %d", tname, &trights);
	SkipLine(astr);
	tl = (struct AclEntry *)malloc(sizeof(struct AclEntry));
	if (!first)
	    first = tl;
	strcpy(tl->name, tname);
	tl->rights = trights;
	tl->next = 0;
	if (last)
	    last->next = tl;
	last = tl;
    }
    ta->pluslist = first;

    return ta;
}

static char *
safestrtok(char *str, char *tok)
{
    char *temp;

    if (str)
	return (strtok(str, tok));

    temp = strtok(NULL, tok);
    if (temp)
	*(temp - 1) = *tok;

    return temp;

}


/* If it exists, we do some fussing about whether or not this
 * is a reasonably secure path - not that it makes *much* difference, since
 * there's not much point in being more secure than the kpasswd executable.
 */
/* 1.  is this directory in AFS?
 * 2.  Is every component of the pathname secure 
 *     (ie, only system:administrators have w or a rights)? 
 */
static int
is_secure(char *dir)
{
    char *temp;
    struct ViceIoctl blob;
    struct AclEntry *te;
    char space[2046];
    int secure = 1;
    afs_int32 code;
    struct Acl *ta;

    if (!InAFS(dir))		/* final component *must* be in AFS */
	return 0;

#ifndef INSECURE
    for (temp = safestrtok(dir, "/"); temp; temp = safestrtok(NULL, "/")) {
	/* strtok keeps sticking NUL in place of /, so we can look at 
	 * ever-longer chunks of the path.
	 */
	if (!InAFS(dir))
	    continue;

	blob.out_size = MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	code = pioctl(dir, VIOCGETAL, &blob, 1);
	if (code) {
	    continue;
	}
	ta = ParseAcl(space);
	if (ta->nplus <= 0)
	    continue;

	for (te = ta->pluslist; te; te = te->next) {
	    if (((te->rights & PRSFS_INSERT) && (te->rights & PRSFS_DELETE))
		|| (te->rights & (PRSFS_WRITE | PRSFS_ADMINISTER)))
		if (strcmp(te->name, "system:administrators"))
		    return 0;	/* somebody who we can't trust has got power */
	}
    }
#endif /* INSECURE */

    return 1;
}

/* Then, once we've found our own location, we look for a program named
 * kpwvalid.  
 */

/* look for a password-checking program named kpwvalid.
 * It has to be in a secure place (same place as this executable)
 */
static int
kpwvalid_is(char *dir)
{
    struct stat statbuff;
    int len;

    len = strlen(dir);
    strcpy(dir + len, "/kpwvalid");

    if (stat(dir, &statbuff) < 0) {
	/* if lstat fails, it's possible that it's transient, but 
	 * unlikely.  Let's hope it isn't, and continue... */
	*(dir + len) = '\0';
	return 0;
    }

    *(dir + len) = '\0';
    return 1;
}

#ifdef AFS_NT40_ENV
/* We don't allow the use of kpwvalid executable scripts to set policy
 * for passwd changes. 
 */
int
init_child(char *myname)
{

    using_child = 0;
    return using_child;

}
#else /* !NT40 */
int
init_child(char *myname)
{
    int pipe1[2], pipe2[2];
    pid_t pid;
    char dirpath[1024];
    char *argv[2];

    if (!(find_me(myname, dirpath) && is_secure(dirpath)
	  && kpwvalid_is(dirpath))) {
	using_child = 0;
	return 0;
    }

    /* make a couple of pipes, one for the child's stdin, and the other
     * for the child's stdout.  The parent writes to the former, and
     * reads from the latter, the child reads from the former, and
     * writes to the latter.
     */
    pipe(pipe1);
    pipe(pipe2);

    /* fork a child */
    pid = fork();
    if (pid == -1) {
	using_child = 0;
	perror("kpasswd: can't fork because ");
	return (using_child);
    }
    if (pid == 0) {		/* in child process */
	/* tie stdin and stdout to these pipes */
	/* if dup2 doesn't exist everywhere, close and then dup, but make */
	/* sure that you really get stdin or stdout from the dup. */
	if ((-1 == dup2(pipe1[0], 0)) || (-1 == dup2(pipe2[1], 1))) {
	    perror("kpasswd: can't exec kpwvalid because ");
	    exit(-1);
	}

	strcat(dirpath, "/kpwvalid");
	argv[1] = NULL;
	argv[0] = dirpath;
	execv(dirpath, argv);
	return 0;
    } else {
	using_child = pid;	/* save it for later */
	childin = fdopen(pipe1[1], "w");
	childout = fdopen(pipe2[0], "r");
	return (using_child);
    }
}
#endif /* not NT40 */

int
password_bad(char *pw)
{
    int rc;
    rc = 0;

    if (using_child) {
	fprintf(childin, "%s\n", pw);
	fflush(childin);
	fscanf(childout, "%d", &rc);
    }

    return (rc);
}

/* this is originally only used to give the child the old password, so she
 * can compare putative new passwords against it.
 */
int
give_to_child(char *pw)
{
    int rc;
    rc = 0;

    if (using_child) {
	fprintf(childin, "%s\n", pw);
	fflush(childin);
    }

    return (rc);
}

/* quickly and painlessly
 */
int
terminate_child(char *pw)
{
    int rc;
    rc = 0;

#ifndef AFS_NT40_ENV
    if (using_child) {
	rc = kill(using_child, SIGKILL);
    }
#endif
    return (rc);
}
