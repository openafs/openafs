/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

/* missing type from C language */

#include <errno.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#undef	_NONSTD_TYPES
#endif
#include <stdio.h>
#include <unistd.h>
#include <afs/afs_args.h>
#include <sys/param.h>
#ifdef	AFS_SUN5_ENV
#include <fcntl.h>
#endif
#include <sys/file.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/time.h>
#define VIRTUE
#define VICE
#include <sys/ioctl.h>
#include <afs/vice.h>
#undef VIRTUE
#undef VICE
#include <sys/ioctl.h>
#include <netdb.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <string.h>

#include <afs/venus.h>

/* ************************************************************* */

#define MAXACL 400

short verbose = 0;
short renameTargets = 0;
short oneLevel = 0;
short preserveDate = 1;
short preserveMountPoints = 0;
short forceOverwrite = 0;

int pageSize;
short setacl = 1;
short oldAcl = 0;
char file1[MAXPATHLEN], file2[MAXPATHLEN];

#define	MAXSIZE	2048
static char space[MAXSIZE];

struct OldAcl {
    int nplus;
    int nminus;
    int offset;
    char data[1];
};

static void ScanArgs(int argc, char *argv[]);
static short MakeParent(char *file, afs_int32 owner);
static int Copy(char *file1, char *file2, short recursive, int level);
static int isMountPoint(char *name, struct ViceIoctl *blob);


/* ************************************************************ */
/* 								 */
/* main program							 */
/* 								 */
/* ************************************************************ */

#include "AFS_component_version_number.c"

int
main(int argc, char *argv[])
{
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
#if !defined (AFS_AIX_ENV) && !defined (AFS_HPUX_ENV)
    pageSize = getpagesize();
#endif
    ScanArgs(argc, argv);

    /* now read each line of the CopyList */
    if (Copy(file1, file2, !oneLevel, 0))
	return(1);		/* some type of failure */

    return(0);
}


#define USAGE "usage: up [-v1frxm] from to\n"
static void
ScanArgs(int argc, char *argv[])
{
    /* skip program name */
    argc--, argv++;

    /* check for -flag options */
    while (argc > 0 && *argv[0] == '-') {
	char *cp = *argv;

	switch (*++cp) {
	case 'v':
	    verbose = 1;
	    break;

	case '1':
	    oneLevel = 1;
	    break;

	case 'r':
	    renameTargets = 1;
	    break;

	case 'f':
	    forceOverwrite = 1;
	    break;

	case 'x':
	    preserveDate = 0;
	    break;

	case 'm':
	    preserveMountPoints = 1;
	    break;

	default:
	    fprintf(stderr, "Unknown option: '%c'\n", *cp);
	    fprintf(stderr, USAGE);
	    exit(1);
	}
	argc--, argv++;
    }

    if (argc != 2) {
	fprintf(stderr, USAGE);
	exit(1);
    }

    strncpy(file1, argv[0], MAXPATHLEN);
    strncpy(file2, argv[1], MAXPATHLEN);

}				/*ScanArgs */



/*
 * MakeParent
 *	Make sure the parent directory of this file exists.  Returns
 * 	1 if it exists, 0 otherwise.  Note: the owner argument
 * 	is a hack.  All directories made will have this owner.
 */
static short
MakeParent(char *file, afs_int32 owner)
{
    char parent[MAXPATHLEN];
    char *p;
    struct stat s;

    strlcpy(parent, file, sizeof parent);

    p = strrchr(parent, '/');
    if (!p) {
	strlcpy(parent, ".", sizeof parent);
    } else if (p > parent) {
	*p = '\0';
    } else {
	p[1] = '\0';
    }

    if (stat(parent, &s) < 0) {
	if (!MakeParent(parent, owner))
	    return (0);

	if (verbose) {
	    printf("Creating directory %s\n", parent);
	    fflush(stdout);
	}

	mkdir(parent, 0777);
	chown(parent, owner, -1);
    }
    return (1);
}				/*MakeParent */


/*
 * Copy
 * 	This does the bulk of the work of the program.  Handle one file,
 *	possibly copying subfiles if this is a directory
 */
static int
Copy(char *file1, char *file2, short recursive, int level)
{
    struct stat s1, s2;		/*Stat blocks */
    struct ViceIoctl blob;
    char aclspace[MAXACL];
    afs_int32 rcode = 0, code;
    int goods2 = 1;

    code = lstat(file1, &s1);
    if (code < 0) {
	fprintf(stderr, "Can't find %s\n", file1);
	return 1;
    }

    code = lstat(file2, &s2);
    if (code < 0) {
	if (!MakeParent(file2, s1.st_uid))
	    return 0;
	goods2 = 0;
    }

    if ((s1.st_mode & S_IFMT) == S_IFREG) {
	/*
	 * -------------------- Copy regular file --------------------
	 */
	int f1, f2, n;
	char buf[4096];		/* Must be bigger than sizeof (*head) */
	struct timeval tv[2];
	char tmpfile[MAXPATHLEN], newName[MAXPATHLEN];

	if (verbose) {
	    printf("Level %d: File %s to %s\n", level, file1, file2);
	    fflush(stdout);
	}

	/* Wonder if there is a security hole */
	if (((s1.st_mode & 04002) == 04002) || ((s1.st_mode & 04020) == 04020)
	    || ((s1.st_mode & 02002) == 02002)) {
	    fprintf(stderr,
		    "WARNING: Mode-bits security hole in files %s and %s\n",
		    file1, file2);
	}

	if (!goods2 || (s1.st_mtime != s2.st_mtime) || (s1.st_size != s2.st_size)) {	/*c */
	    /* Don't ovewrite a write protected file (unless force: -f) */
	    if (!forceOverwrite && goods2 && (s2.st_mode & 0200) == 0) {
		fprintf(stderr,
			"File %s is write protected against its owner; not changed\n",
			file2);
		return 1;
	    }

	    if (verbose) {
		printf("  Copy file %s to %s (%u Bytes)\n", file1, file2,
		       s1.st_size);
		fflush(stdout);
	    }

	    strlcpy(tmpfile, file2, sizeof tmpfile);	/* Name of temporary file */
	    strlcat(tmpfile, ".UPD", sizeof tmpfile);

	    /* open file1 for input */
	    f1 = open(file1, O_RDONLY);
	    if (f1 < 0) {
		fprintf(stderr, "Unable to open input file %s: %s\n",
			file1, strerror(errno));
		return 1;
	    }

	    /* open temporary output file */
	    f2 = open(tmpfile, (O_WRONLY | O_CREAT | O_TRUNC), s1.st_mode);
	    if (f2 < 0) {
		fprintf(stderr, "Unable to open output file %s: %s\n",
			tmpfile, strerror(errno));
		fflush(stdout);
		close(f1);
		return 1;
	    }

	    /* Copy file1 to temporary file */
	    while ((n = read(f1, buf, sizeof(buf))) > 0) {
		if (write(f2, buf, n) != n) {
		    fprintf(stderr,
			    "Write failed, file %s must be copied again.\n",
			    file2);
		}
	    }

	    /* preserve access and modification times: ("-x" disables) */
	    if (preserveDate) {
		tv[0].tv_sec = s1.st_atime;
		tv[0].tv_usec = 0;
		tv[1].tv_sec = s1.st_mtime;
		tv[1].tv_usec = 0;
		utimes(tmpfile, tv);
	    }

	    /* Close the files */
	    code = close(f1);
	    code = close(f2);
	    if (code < 0) {
		perror("close ");
		rcode = 1;
	    }

	    /* Rename file2 to file2.old. [-r] */
	    if (renameTargets && goods2) {
		strlcpy(newName, file2, sizeof newName);
		strlcat(newName, ".old", sizeof newName);
		if (verbose) {
		    printf("  Renaming %s to %s\n", file2, newName);
		    fflush(stdout);
		}
		if (rename(file2, newName) < 0) {
		    fprintf(stderr, "Rename of %s to %s failed.\n", file2,
			    newName);
		}
	    }

	    /* Rename temporary file to file2 */
	    code = rename(tmpfile, file2);
	    if (code < 0) {
		fprintf(stderr, "Rename of %s to %s failed.\n", tmpfile,
			file2);
		return 1;
	    }

	    /* Re-stat file2 and compare file sizes */
	    code = lstat(file2, &s2);
	    if (code < 0) {
		fprintf(stderr, "WARNING: Unable to stat new file %s\n",
			file2);
		return 1;
	    }
	    if (s1.st_size != s2.st_size) {
		fprintf(stderr,
			"WARNING: New file %s is %u bytes long; should be %u\n",
			file2, s2.st_size, s1.st_size);
	    }
	}

	/*c */
	/* Set the user-id */
	if (s2.st_uid != s1.st_uid) {
	    if (verbose) {
		printf("  Set owner-id for %s to %d\n", file2, s1.st_uid);
		fflush(stdout);
	    }
	    code = chown(file2, s1.st_uid, -1);
	    if (code) {
		fprintf(stderr, "Unable to set owner-id for %s to %d\n",
			file2, s1.st_uid);
		fflush(stdout);
		rcode = 1;
		s1.st_mode &= ~04000;	/* Don't set suid bit */
	    }
	}

	/* Set the group-id */
	if (s2.st_gid != s1.st_gid) {
	    if (verbose) {
		printf("  Set group-id for %s to %d\n", file2, s1.st_gid);
		fflush(stdout);
	    }
	    code = chown(file2, -1, s1.st_gid);
	    if (code) {
		fprintf(stderr, "Unable to set group-id for %s to %d\n",
			file2, s1.st_gid);
		fflush(stdout);
		rcode = 1;
		s1.st_mode &= ~02000;	/* Don't set sgid bit */
	    }
	}

	/* Set the mode bits */
	if (s1.st_mode != s2.st_mode) {
	    if (verbose) {
		printf("  Set mode-bit for %s to %o\n", file2,
		       (s1.st_mode & 07777));
		fflush(stdout);
	    }
	    code = chmod(file2, s1.st_mode);
	    if (code) {
		fprintf(stderr, "Unable to set mode-bits for %s to %d\n",
			file2, s1.st_mode);
		rcode = 1;
	    }
	}
    }
    /* regular file */
    else if ((s1.st_mode & S_IFMT) == S_IFLNK) {
	/*
	 * --------------------- Copy symlink  --------------------
	 */
	char linkvalue[MAXPATHLEN + 1];
	int n;

	if (verbose) {
	    printf("Level %d: Symbolic link %s to %s\n", level, file1, file2);
	    fflush(stdout);
	}

	/* Don't ovewrite a write protected directory (unless force: -f) */
	if (!forceOverwrite && goods2 && (s2.st_mode & 0200) == 0) {
	    fprintf(stderr,
		    "Link %s is write protected against its owner; not changed\n",
		    file2);
	    return 1;
	}

	if (verbose) {
	    printf("  Copy symbolic link %s->%s to %s\n", file1, linkvalue,
		   file2);
	    fflush(stdout);
	}

	n = readlink(file1, linkvalue, sizeof(linkvalue));
	if (n == -1) {
	    fprintf(stderr, "Could not read symbolic link %s\n", file1);
	    perror("read link ");
	    return 1;
	}
	linkvalue[n] = 0;

	unlink(file2);		/* Always make the new link (it was easier) */

	code = symlink(linkvalue, file2);
	if (code == -1) {
	    fprintf(stderr, "Could not create symbolic link %s\n", file2);
	    perror("create link ");
	    return 1;
	}
    }
    /*Dealing with symlink */
    else if (preserveMountPoints && (code = isMountPoint(file1, &blob))) {
	/*
	 * --------------------- Copy mount point  --------------------
	 */

	if (code > 1) {
	    perror("checking for mount point ");
	    return 1;
	}
	if (verbose) {
	    printf("Level %d: Mount point %s to %s\n", level, file1, file2);
	    fflush(stdout);
	}

	/* Don't ovewrite a write protected directory (unless force: -f) */
	if (!forceOverwrite && goods2 && (s2.st_mode & 0200) == 0) {
	    fprintf(stderr,
		    "Target %s is write protected against its owner; not changed\n",
		    file2);
	    return 1;
	}

	if (verbose) {
	    printf("  Copy mount point %s for vol %s to %s\n", file1,
		   blob.out, file2);
	    fflush(stdout);
	}

	unlink(file2);		/* Always make the new link (it was easier) */

	strcat(blob.out, ".");	/* stupid convention; these end with a period */
	code = symlink(blob.out, file2);
	if (code == -1) {
	    fprintf(stderr, "Could not create mount point %s for vol %s\n",
		    file2, blob.out);
	    perror("create mount point ");
	    return 1;
	}

    }
    /*Dealing with mount point */
    else if (((s1.st_mode & S_IFMT) == S_IFDIR)
	     && (recursive || (level == 0))) {
	/*
	 * ----------------------- Copy directory -----------------------
	 */
	DIR *dir;
	int tfd, code, i;
	struct OldAcl *oacl;
	char tacl[MAXACL];
	char f1[MAXPATHLEN], f2[MAXPATHLEN];
	char *p1, *p2;
	struct dirent *d;
	struct timeval tv[2];

	if (verbose) {
	    printf("Level %d: Directory %s to %s\n", level, file1, file2);
	    fflush(stdout);
	}

	/* Don't ovewrite a write protected directory (unless force: -f) */
	if (!forceOverwrite && goods2 && (s2.st_mode & 0200) == 0) {
	    fprintf(stderr,
		    "Directory %s is write protected against its owner; not changed\n",
		    file2);
	    return 1;
	}

	strlcpy(f1, file1, sizeof f1);
	strlcpy(f2, file2, sizeof f2);
	p1 = f1 + strlen(f1);
	p2 = f2 + strlen(f2);
	if (p1 == f1 || p1[-1] != '/')
	    *p1++ = '/';
	if (p2 == f2 || p2[-1] != '/')
	    *p2++ = '/';

	dir = opendir(file1);
	if (dir == NULL) {
	    fprintf(stderr, "Couldn't open %s\n", file1);
	    return 1;
	}

	while ((d = readdir(dir)) != NULL) {
	    if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
		continue;
	    strlcpy(p1, d->d_name, sizeof f1 - (p1 - f1));
	    strlcpy(p2, d->d_name, sizeof f2 - (p2 - f2));
	    code = Copy(f1, f2, recursive, level + 1);
	    if (code && !rcode)
		rcode = 1;	/* remember errors */
	}

	closedir(dir);

	if (verbose) {
	    printf("Level %d: Copied directory %s to %s\n", level, file1,
		   file2);
	    fflush(stdout);
	}

	mkdir(file2, 0777);	/* Handle case where MakeParent not invoked. */

	if (verbose) {
	    printf("  Set owner-id for %s to %d\n", file2, s1.st_uid);
	    fflush(stdout);
	}
	code = chown(file2, s1.st_uid, -1);
	if (code) {
	    fprintf(stderr, "Unable to set owner-id for %s to %d\n", file2,
		    s1.st_uid);
	    fflush(stdout);
	    s1.st_mode &= ~04000;	/* Don't set suid bit */
	}

	if (verbose) {
	    printf("  Set group-id for %s to %d\n", file2, s1.st_gid);
	    fflush(stdout);
	}
	code = chown(file2, -1, s1.st_gid);
	if (code) {
	    fprintf(stderr, "Unable to set group-id for %s to %d\n", file2,
		    s1.st_gid);
	    fflush(stdout);
	    s1.st_mode &= ~02000;	/* Don't set sgid bit */
	}

	if (verbose) {
	    printf("  Set mode-bit for %s to %o\n", file2,
		   (s1.st_mode & 07777));
	    fflush(stdout);
	}
	code = chmod(file2, s1.st_mode);
	if (code) {
	    fprintf(stderr, "Unable to set mode-bits for %s to %d\n", file2,
		    s1.st_mode);
	    fflush(stdout);
	    rcode = 1;
	}

	if (setacl == 1) {
	    if (verbose) {
		printf("  Set acls for %s\n", file2);
		fflush(stdout);
	    }

	    blob.in = aclspace;
	    blob.out = aclspace;
	    blob.in_size = 0;
	    blob.out_size = MAXACL;

	    if (oldAcl) {
		/* Get an old-style ACL and convert it */
                if (verbose) {
                    printf("  Getting old style acl\n");
                    fflush(stdout);
                }

		for (i = 1; i < strlen(file1); i++)
		    if (file1[i] == '/')
			break;
		strlcpy(aclspace, &file1[i], sizeof aclspace);

		blob.in_size = 1 + strlen(aclspace);
		tfd = open(file1, O_RDONLY, 0);
		if (tfd < 0) {
		    perror("old-acl open ");
		    return 1;
		}
		code = ioctl(tfd, _VICEIOCTL(4), &blob);
		close(tfd);
		if (code < 0) {
		    if (errno == EINVAL) {
			setacl = 0;
                        if (verbose) {
                            printf("  _VICEIOCTL(4) returns EINVAL\n");
                            fflush(stdout);
                        }
		    } else {
			return 1;
		    }
		}
		/* Now convert the thing. */
		oacl = (struct OldAcl *)(aclspace + 4);
		sprintf(tacl, "%d\n%d\n", oacl->nplus, oacl->nminus);
		strlcat(tacl, oacl->data, sizeof tacl);
		strlcpy(aclspace, tacl, sizeof aclspace);
	    } /*Grab and convert old-style ACL */
	    else {
		/* Get a new-style ACL */
                if (verbose) {
                    printf("  Getting new style acl\n");
                    fflush(stdout);
                }

		code = pioctl(file1, _VICEIOCTL(2), &blob, 1);
		if (code < 0) {
		    if (errno == EINVAL) {
			setacl = 0;
                        if (verbose) {
                            printf("  _VICEIOCTL(2) returns EINVAL\n");
                            fflush(stdout);
                        }
		    } else {
			perror("getacl ");
			return 1;
		    }
		}
	    }			/*Grab new-style ACL */

	    /*
	     * Now, set the new-style ACL.
	     */
	    if (setacl == 1) {
                if (verbose) {
                    printf("  Setting new style acl\n");
                    fflush(stdout);
                }
		blob.out = aclspace;
		blob.in = aclspace;
		blob.out_size = 0;
		blob.in_size = 1 + strlen(aclspace);
		code = pioctl(file2, _VICEIOCTL(1), &blob, 1);
		if (code) {
		    if (errno == EINVAL) {
			setacl = 0;
                        if (verbose) {
                            printf("  _VICEIOCTL(1) returns EINVAL\n");
                            fflush(stdout);
                        }
		    } else {
			fprintf(stderr, "Couldn't set acls for %s\n", file2);
			return 1;
		    }
		}
	    }

	    if (setacl == 0) {
		printf("Not setting acls\n");
	    }
	}

        /* preserve access and modification times: ("-x" disables) */
        if (preserveDate) {
            tv[0].tv_sec = s1.st_atime;
            tv[0].tv_usec = 0;
            tv[1].tv_sec = s1.st_mtime;
            tv[1].tv_usec = 0;
            utimes(file2, tv);
        }
    }

    return rcode;
}				/*Copy */


static int
isMountPoint(char *name, struct ViceIoctl *blob)
{
    afs_int32 code;
    char true_name[1024];	/*dirname */
    char parent_dir[1024];	/*Parent directory of true name */
    char *last_component;	/*Last component of true name */

    sprintf(true_name, "%s%s", (name[0] == '/') ? "" : "./", name);

    /*
     * Find rightmost slash, if any.
     */
    last_component = (char *)strrchr(true_name, '/');
    if (last_component) {
	/*
	 * Found it.  Designate everything before it as the parent directory,
	 * everything after it as the final component.
	 */
	strncpy(parent_dir, true_name, last_component - true_name);
	parent_dir[last_component - true_name] = 0;
	last_component++;	/*Skip the slash */
    } else {
	/*
	 * No slash appears in the given file name.  Set parent_dir to the current
	 * directory, and the last component as the given name.
	 */
	strlcpy(parent_dir, ".", sizeof parent_dir);
	last_component = true_name;
    }

    if (strcmp(last_component, ".") == 0 || strcmp(last_component, "..") == 0) {
	fprintf(stderr,
		"up: you may not use '.' or '..' as the last component\n");
	fprintf(stderr, "up: of a name in the 'up' command.\n");
	return 3;
    }

    blob->in = last_component;
    blob->in_size = strlen(last_component) + 1;
    blob->out_size = MAXSIZE;
    blob->out = space;
    memset(space, 0, MAXSIZE);

    code = pioctl(parent_dir, VIOC_AFS_STAT_MT_PT, blob, 0);

    if (code == 0) {
	printf("'%s' is a mount point for volume '%s'\n", name, space);
	fflush(stdout);
	return 1;
    } else {
	if (errno == EINVAL) {
	    /* printf( "'%s' is not a mount point.\n", name);
	     * fflush(stdout);
	     */
	    return 0;
	} else {
	    fprintf(stderr, "problem examining '%s' in '%s'.\n",
		    last_component, parent_dir);
	    return 2;
	    /* Die(errno, (ti->data ? ti->data : parent_dir));
	     */
	}
    }
    return 4;
}
