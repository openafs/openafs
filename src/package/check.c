/*
 * (C) Copyright Transarc Corporation 1989
 * Licensed Materials - Property of Transarc
 * All Rights Reserved.
 */

/*------------------------------------------------------------------------
 * check.c
 *
 * Description:
 *	Check the integrity of the configuration tree for package, the
 *	AFS workstation configuration tool.
 *
 * Author:
 *	Transarc Corporation & Carnegie Mellon University
 *------------------------------------------------------------------------*/

#include <afs/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dirent.h>
#include "globals.h"
#include "package.h"

char *emalloc();
char *strcpy();
CTREEPTR LocateChildNode();

static char path2[MAXPATHLEN+1];    /* $$get rid of this */
static char path3[MAXPATHLEN+1];    /* $$get rid of this */

/*------------------------------------------------------------------------
 * [static] CheckMount
 *
 * Description:
 *	Check the assertion that the given path is a Unix mountpoint.
 *
 * Arguments:
 *	char *path : Path to check.
 *
 * Returns:
 *	0 on success,
 *	Exit from package on failure.
 *
 * Environment:
 *	This routine is private to the module.
 *
 * Side Effects:
 *	May exit from package.
 *------------------------------------------------------------------------*/

static CheckMount(path)
     char *path;

{ /*CheckMount*/

    struct stat stb;			/*Parent's stat block*/
    struct stat stb2;			/*Child's stat block*/
    char dir[MAXPATHLEN];		/*Pathname of candidate mount point*/
    char parent[MAXPATHLEN];		/*Parent's pathname*/
    register char *ep, *dp, *sp;	/*Sliding ptr to above strings*/
    int ret;				/*Return value*/

    debug_message("%% CheckMount called on path %s", path);
    ret = 0;

    /*
      * Copy out the candidate mountpoint's pathname into dir, throwing
      * off any leaf component from the original path.
      */
    ep = strrchr(path, '/');
    for (sp = path, dp = dir; sp < ep; *dp++ = *sp++);
    if (dp == dir)
	*dp++ = '/';
    *dp = '\0';

    /*
      * Copy out the parent's pathname into parent.
      */
    ep = strrchr(dir, '/');
    for (sp = dir, dp = parent; sp < ep; *dp++ = *sp++);
    if (dp == parent)
	*dp++ = '/';
    *dp = '\0';

    /*
      * Only perform the following test if the candidate mountpoint is
      * something other than `/', which we know is a mountpoint.
      */
    if (strcmp(dir, "/")) {
      /*
        * Stat the given directory and its parent.  If either is not a
	* directory or if the device numbers are the same, then the
	* candidate has failed and is not a Unix mountpoint.
	*/
      if (stat(dir, &stb) < 0)
	ret = -1;
      if (stat(parent, &stb2) < 0)
	ret = -1;
      if ((stb.st_mode & S_IFMT) != S_IFDIR)
	ret = -1;
      if ((stb2.st_mode & S_IFMT) != S_IFDIR)
	ret = -1;
      if (stb2.st_dev == stb.st_dev)
	ret = -1;
    }

    if (ret < 0) {
      /*
	* Our assertion that the given path is a mountpoint is false.
	* Tell our caller, then croak off.
	*/
      fatal("** %s is not a Unix mountpoint, as was expected!", dir);
    }

    /*
      * The candidate mountpoint has passed the test.  If we're being
      * verbose, tell everyone.
      */
    verbose_message("Found Unix mountpoint %s", dir);

} /*CheckMount*/

/*------------------------------------------------------------------------
 * check
 *
 * Description:
 *	Check the validity of the given node compared to the associated
 *	pathname.
 *
 * Arguments:
 *	CTREEPTR np : Node pointer to check.
 *	char *path  : Associated pathname.
 *
 * Returns:
 *	0 upon success,
 *	Exits the program otherwise.
 *
 * Environment:
 *	This is one of the routines applied to the entire configuration
 *	tree.
 *
 * Side Effects:
 *	May exit from package.
 *------------------------------------------------------------------------*/

int check(np, path)
    register CTREEPTR np;
    char *path;

{ /*check*/

    register CTREEPTR np2;		/*Node ptr for np's child*/
    register struct dirent *de;		/*Ptr to directory entry*/
    DIR	*dp;				/*Ptr to directory being read*/
    struct stat stb;			/*Stat block for path2*/
    int retval;				/*Return value*/

    retval = 0;
    debug_message("[check] Checking pathname %s", path, np);

    if ((np->flag & F_TYPE) && (np->updtspec & U_LOSTFOUND))
	CheckMount(path);
    if (!(np->flag & F_PROTO)) {
	if (!(np->flag & F_TYPE))
	    fatal("** Incomplete: %s", path);
	else
	  if ((np->type == S_IFCHR) || (np->type == S_IFBLK))
	    /* $$Note: Actually, the parser takes care of this */
	    fatal("** No device numbers specified for device %s\n", path);
	return(retval);
    }

    /*
     * No checks needed for character & block special devices (& sockets
     * & named pipes).
     */
    if ((np->type == S_IFCHR)
	|| (np->type == S_IFBLK)
#ifndef AFS_AIX_ENV
	|| (np->type == S_IFSOCK)
#endif /* AFS_AIX_ENV */
#ifdef S_IFIFO
	|| (np->type == S_IFIFO)
#endif /* S_IFIFO */
	)
      return(retval);

    /*
      * Construct the target path, either absolute or prefixed.
      */
    if (np->updtspec & U_ABSPATH)
      /*
	* Absolute path.
	*/
      sprintf(path2, "%s", np->proto.info.path);
    else
      /*
	* Prefixed path.
	*/
      sprintf(path2, "%s%s", np->proto.info.path, path);

    debug_message("[check] Statting %s", path2);

    /*
      * If this is a symlink, lstat the guy and warn people if it
      * isn't found.
      */
    if ((np->flag & F_TYPE) && (np->type == S_IFLNK)) {
      if (lstat(path2, &stb) < 0)
	verbose_message("* Warning: symlink %s not found", path2);
      return(retval);
    }

    /*
      * Do a normal stat, failing if it does.
      */
    if (stat(path2, &stb) < 0)
	fatal("** Stat failed for %s; %m", path2);

    if (np->flag & F_TYPE) {
      if (np->type == S_IFLNK)
	return(retval);
      if ((stb.st_mode & S_IFMT) != np->type)
	fatal("** Type conflict for %s", path2);
    }
    else {
      np->type = stb.st_mode & S_IFMT;
      np->flag |= F_TYPE;
    }

    if (!(np->flag & F_MODE)) {
      /*Fill in the mode (protection) info from the stat block*/
      np->mode |= stb.st_mode & ~S_IFMT;
      np->flag |= F_MODE;
    }

    if (!(np->flag & F_UID)) {
      /*Fill in the user info from the stat block*/
      np->uid = stb.st_uid;
      np->flag |= F_UID;
    }

    if (!(np->flag & F_GID)) {
      /*Fill in the group info from the stat block*/
#ifdef	VICE
      np->gid = (stb.st_gid == 32767) ? 0 : stb.st_gid;
#else /* VICE */
      np->gid = stb.st_gid;
#endif /* VICE */
      np->flag |= F_GID;
    }

    if (!(np->flag & F_MTIME)) {
      /*Fill in the last modified time from the stat block*/
      np->mtime = stb.st_mtime;
      np->flag |= F_MTIME;
    }

    /*
      * If we've reached a non-directory, we're all done.
      */
    if (np->type != S_IFDIR) {
      debug_message("[check] Reached leaf node, returning");
      return(retval);
    }

    /*
      * Open up the directory and sweep through it, checking all its
      * entries.
      */
    verbose_message("Scanning directory %s", path2);
    if ((dp = opendir(path2)) == 0)
	fatal("** Opendir failed on %s; %m", path2);

    while ((de = readdir(dp)) != 0) {
      if (de->d_name[0] == '.') {
	/*
	  * Don't process dot, the current directory, or dotdot, the
	  * parent.
	  */
	if (de->d_name[1] == 0)
	  continue;
	if (de->d_name[1] == '.' && de->d_name[2] == 0)
	  continue;
      }

      if ((np2 = LocateChildNode(np, de->d_name, C_LOCATE|C_CREATE)) == NULL)
	fatal("** Bad path: %s/%s", path, de->d_name);

      if (!(np2->flag & F_PROTO)) {
	if (np->updtspec & U_ABSPATH) {
	  np2->updtspec |= U_ABSPATH;
	  sprintf(path3,"%s/%s",np->proto.info.path,de->d_name);
	  np2->proto.info.path = emalloc((unsigned)(strlen(path3)+1));
	  (void)strcpy(np2->proto.info.path, path3);
	}
	else {
	  np2->proto.info.path = np->proto.info.path;
	}
	np2->flag |= F_PROTO;
	continue;
      }
      if ((np2->updtspec & U_ABSPATH) != (np->updtspec & U_ABSPATH))
	fatal("** Prototype conflict: %s/%s: Absolute & relative paths given",
	      path, de->d_name);
      if (np->updtspec & U_ABSPATH) {
	sprintf(path3,"%s/%s", np->proto, de->d_name);
	if (strcmp(path3, np2->proto.info.path))
	  fatal("** Prototype conflict: %s/%s: Previously %s",
		path, de->d_name, np2->proto.info.path);
      }
      else {
	if (strcmp(np->proto.info.path, np2->proto.info.path))
	  fatal("** Prototype conflict: %s/%s: Mismatch for %s and %s",
		path, de->d_name,
		np->proto.info.path,
		np2->proto.info.path);
      }
    } /*For each directory entry*/

    /*
      * Make sure to close the directory before we leave.
      */
    (void) closedir(dp);
    return(retval);

} /*check*/

