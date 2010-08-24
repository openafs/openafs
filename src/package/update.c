/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*------------------------------------------------------------------------
 * update.c
 *
 * Description:
 *	Routines that actually do the disk updates.
 *
 *------------------------------------------------------------------------*/

#include <afs/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>
#if defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV)
#include <sys/mkdev.h>
#endif
#ifdef AFS_LINUX20_ENV
#include <sys/sysmacros.h>
#endif
#include "globals.h"
#include "package.h"

char *emalloc();
char *strcpy();
CTREEPTR LocateChildNode();

static struct stat stb;

static UpdateSock();
static UpdateDev();
static UpdatePipe();
static UpdateLnk();
static UpdateDir();
static UpdateReg();
static dochtyp();
static dochmod();
static dochown();
static dochtim();
static int FixLostFoundDir();
static int FixDir();
static FixReg();

/* $$important: these will have to be fixed with an error recovery mechanism */

int
update(CTREEPTR np, char *path)
{				/*update */

    switch (np->type) {
#ifndef AFS_AIX_ENV
    case S_IFSOCK:
	UpdateSock(np, path);
	break;
#endif /* AFS_AIX_ENV */

#ifdef S_IFIFO
    case S_IFIFO:
	UpdatePipe(np, path);
	break;
#endif /* S_IFIFO */

    case S_IFCHR:
    case S_IFBLK:
	UpdateDev(np, path);
	break;

    case S_IFLNK:
	UpdateLnk(np, path);
	break;

    case S_IFDIR:
	UpdateDir(np, path);
	break;

    case S_IFREG:
	UpdateReg(np, path);
	break;
    }

}				/*update */

static void
UpdateSock(CTREEPTR np, char *path)
{				/*UpdateSock */

    (void)dochtyp(np, path);

}				/*UpdateSock */


static void
UpdateDev(CTREEPTR np, char *path)
{				/*UpdateDev */

    register int ret;

    ret = dochtyp(np, path);

#ifdef	KFLAG
    if (ret == 1)
	return;
#endif /* KFLAG */
    if ((np->flag & F_PROTO) != 0) {
	if (ret >= 0) {
	    if (np->proto.info.rdev != stb.st_rdev) {
		rm(path);
		ret = -1;
	    }
	}
	if (ret < 0) {
	    char *type;

	    switch (np->type) {
	    case S_IFBLK:
		type = "b";
		break;

	    case S_IFCHR:
		type = "c";
		break;

	    default:
		message("Unknown device type: %d\n", np->type);
		break;
	    }

	    loudonly_message("mknod %s %d %d %s", type,
			     major(np->proto.info.rdev),
			     minor(np->proto.info.rdev), path);
	    if (!opt_lazy) {
		if (mknod
		    (path, (int)np->mode | (int)np->type,
		     (int)np->proto.info.rdev) < 0)
		    message("mknod %s %d %d %s; %m", type,
			    major(np->proto.info.rdev),
			    minor(np->proto.info.rdev), path);
		if ((ret = lstat(path, &stb)) < 0)
		    message("lstat %s; %m", path);
	    }
	}
    }
    if (ret >= 0) {
	dochmod(np, path);
	dochown(np, path);
    }

}				/*UpdateDev */

static void
UpdatePipe(CTREEPTR np, char *path)
{				/*UpdatePipe */

    register int ret;

    /*
     * Don't have to call dochtyp() here; just set ret to the value
     * saying everything is fine.
     */
    ret = -1;

#ifdef	KFLAG
    if (ret == 1)
	return;
#endif /* KFLAG */
    if ((np->flag & F_PROTO) != 0) {
	if (ret >= 0) {
	    if (np->proto.info.rdev != stb.st_rdev) {
		rm(path);
		ret = -1;
	    }
	}
	if (ret < 0) {
	    loudonly_message("mknod p %s", path);

	    if (!opt_lazy) {
		if (mknod
		    (path, (int)(np->mode) | (int)(np->type),
		     (int)(np->proto.info.rdev)) < 0)
		    message("mknod p %s; %m", path);
		if ((ret = lstat(path, &stb)) < 0)
		    message("lstat %s; %m", path);
	    }
	}
    }

    if (ret >= 0) {
	dochmod(np, path);
	dochown(np, path);
    }

}				/*UpdatePipe */

static void
UpdateLnk(CTREEPTR np, char *path)
{				/*UpdateLnk */

    register int ret;
    char temp[MAXPATHLEN], temp2[MAXPATHLEN];
    int cc;

    ret = dochtyp(np, path);
#ifdef	KFLAG
    if (ret == 1)
	return;
#endif /* KFLAG */
    if ((np->flag & F_PROTO) == 0)
	return;
    if (np->updtspec & U_ABSPATH)
	sprintf(temp, "%s", np->proto.info.path);
    else
	sprintf(temp, "%s%s", np->proto.info.path, path);
    if (ret >= 0) {
	if ((cc = readlink(path, temp2, sizeof(temp2) - 1)) < 0) {
	    message("readlink %s; %m", path);
	    return;
	}
	temp2[cc] = 0;
	if (strcmp(temp2, temp)) {
	    if ((np->updtspec & U_NOOVERWRITE) == 0) {
		rm(path);
		ret = -1;
	    } else {
		loudonly_message("INHIBIT %s updating", path);
	    }
	}
    }
    if (ret < 0) {
	loudonly_message("ln %s %s", path, temp);
	if (!opt_lazy && symlink(temp, path) < 0)
	    message("symlink %s %s; %m", temp, path);
    }

}				/*UpdateLnk */


static void
UpdateDir(CTREEPTR np, char *path)
{				/*UpdateDir */

    register int ret;

    ret = dochtyp(np, path);
#ifdef	KFLAG
    if (ret == 1)
	return;
#endif /* KFLAG */
    if (ret < 0) {
	loudonly_message("mkdir %s", path);
	if (!opt_lazy) {
	    if (mkdir(path, (int)np->mode & ~S_IFMT) < 0)
		message("mkdir %s; %m", path);
	    if ((ret = lstat(path, &stb)) < 0)
		message("lstat %s; %m", path);
	}
    }
    if (np->updtspec & U_LOSTFOUND)
	(void)FixLostFoundDir(path);
    if (np->updtspec & U_RMEXTRA)
	(void)FixDir(np, path);
    if (ret >= 0) {
	dochmod(np, path);
	dochown(np, path);
    }

}				/*UpdateDir */


static void
UpdateReg(CTREEPTR np, char *path)
{				/*UpdateReg */

    register int ret;

    ret = dochtyp(np, path);
#ifdef	KFLAG
    if (ret == 1)
	return;
#endif /* KFLAG */
    if ((np->flag & F_PROTO) != 0) {
	if (ret < 0)
	    np->updtspec &= ~U_RENAMEOLD;
	if (ret >= 0) {
	    if ((np->updtspec & U_NOOVERWRITE) == 0)
		if (np->mtime != stb.st_mtime)
		    ret = -1;
	}
	if (ret < 0) {
	    if ((ret = FixReg(np, path)) >= 0)
		ret = lstat(path, &stb);
	    if (ret >= 0)
		dochtim(np, path);
	}
    }
    if (ret >= 0) {
	dochmod(np, path);
	dochown(np, path);
    }

}				/*UpdateReg */


/*
 * dochtyp
 *
 * This function makes sure the path on local disk has the same file type
 * as that in the given prototype.  If it doesn't (and the -rebootfile
 * flag hasn't been used with a file marked as requiring a reboot), then
 * we delete the local disk copy and return -1.  If inhibiting the overwrite
 * is in order, we return 1.  If the types already match (or the above
 * reboot scenario is true), we return 0.
 */

static int
dochtyp(CTREEPTR np, char *path)
{				/*dochtyp */
    if (lstat(path, &stb) < 0)
	return -1;
#ifdef	KFLAG
    if (opt_kflag && (stb.st_mode & 0222) == 0) {
	loudonly_message("INHIBIT %s updating", path);
	return 1;
    }
#endif /* KFLAG */
    if ((stb.st_mode & S_IFMT) == np->type)
	return 0;
    if (!opt_reboot && (np->flag & F_UPDT) && (np->updtspec & U_REBOOT)) {
	message("%s is out of date; please REBOOT!", path);
	return 0;
    } else {
	rm(path);
	return -1;
    }
}				/*dochtyp */

static void
dochmod(CTREEPTR np, char *path)
{				/*dochmod */
    if ((np->flag & F_MODE) == 0)
	return;
    if ((np->mode & ~S_IFMT) == (stb.st_mode & ~S_IFMT))
	return;
    loudonly_message("chmod %s %o", path, np->mode & ~S_IFMT);
    if (!opt_lazy && chmod(path, (int)np->mode & ~S_IFMT) < 0)
	message("chmod %s; %m", path);
}				/*dochmod */

static void
dochown(CTREEPTR np, char *path)
{				/*dochown */
    if ((np->flag & F_UID) == 0)
	np->uid = stb.st_uid;
    if ((np->flag & F_GID) == 0)
	np->gid = stb.st_gid;
    if (np->uid == stb.st_uid && np->gid == stb.st_gid)
	return;
    loudonly_message("chown %s %d %d", path, np->uid, np->gid);
    if (!opt_lazy && chown(path, np->uid, np->gid) < 0)
	message("chown %s; %m", path);
}				/*dochown */

static void
dochtim(CTREEPTR np, char *path)
{				/*dochtim */
    struct timeval tm[2];

    if (np->mtime == stb.st_mtime
	|| (!opt_reboot && (np->updtspec & U_REBOOT)))
	return;
    tm[0].tv_sec = tm[1].tv_sec = np->mtime;
    tm[0].tv_usec = tm[1].tv_usec = 0;
    if (!opt_silent) {
	char *date;
	time_t mtime = np->mtime;

	date = ctime(&mtime);
	date[24] = 0;
	loudonly_message("utimes %s [%s]", path, date);
    }
    if (!opt_lazy && utimes(path, tm) < 0)
	message("utimes %s; %m", path);
}				/*dochtim */

static int
FixLostFoundDir(char *path)
{				/*FixLostFoundDir */
    if (stb.st_size >= 3584)
	return 0;
    return mklostfound(path);
}				/*FixLostFoundDir */

static int
FixDir(CTREEPTR np, char *path)
{				/*FixDir */
    register DIR *dp;
    register struct dirent *de;
    register char *endp;

    verbose_message("cleandir %s", path);
    if ((dp = opendir(path)) == 0) {
	message("opendir %s; %m", path);
	return -1;
    }
    endp = path + strlen(path);
    *endp++ = '/';
    while ((de = readdir(dp)) != 0) {
	if (de->d_name[0] == '.') {
	    if (de->d_name[1] == 0)
		continue;
	    if (de->d_name[1] == '.' && de->d_name[2] == 0)
		continue;
	}
	if (LocateChildNode(np, de->d_name, C_LOCATE) != 0)
	    continue;
	(void)strcpy(endp, de->d_name);
	rm(path);
    }
    *--endp = 0;
    (void)closedir(dp);
    return 0;
}				/*FixDir */

static int
FixReg(CTREEPTR np, char *path)
{				/*FixReg */
    char new[MAXPATHLEN], old[MAXPATHLEN], temp[MAXPATHLEN];

    if (!opt_reboot && (np->updtspec & U_REBOOT)) {
	verbose_message
	    ("%s is a 'Q' file and -rebootfiles is set; not updated!", path);
	return 0;
    }
    (void)sprintf(new, "%s.new", path);
    if (np->updtspec & U_ABSPATH)
	(void)sprintf(temp, "%s", np->proto.info.path);
    else
	(void)sprintf(temp, "%s%s", np->proto.info.path, path);
    if (cp(temp, new))
	return -1;
    if (np->updtspec & U_RENAMEOLD) {
	(void)sprintf(old, "%s.old", path);
	(void)rm(old);
	(void)ln(path, old);
    }
    if (mv(new, path))
	return -1;
    if (np->updtspec & U_REBOOT)
	status = status_reboot;
    return 0;
}				/*FixReg */
