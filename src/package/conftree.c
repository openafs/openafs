/*
 * (C) Copyright Transarc Corporation 1989
 * Licensed Materials - Property of Transarc
 * All Rights Reserved.
 */

/*------------------------------------------------------------------------
 * conftree.c
 *
 * Description:
 *	Configuration tree module for package, the AFS workstation
 *	configuration tool.
 *
 * Author:
 *	Transarc Corporation & Carnegie Mellon University
 *------------------------------------------------------------------------*/

#include <afs/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <pwd.h>
#include <grp.h>

#include "globals.h"
#include "package.h"

#include "validupdates.h"

char *emalloc();
FILE *efopen();
char *strcpy();
extern FILE *yyin;  /*Input file for the YACC parser*/

/*------------------------------------------------------------------------
 * [static] namehash
 *
 * Description:
 *	Returns a hash value for the given name.
 *
 * Arguments:
 *	char *name : Ptr to name to hash.
 *
 * Returns:
 *	Hash value associated with the given name.
 *
 * Environment:
 *	This routine is private to the module.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int namehash(name)
    register char *name;

{ /*namehash*/

    register int hash;

    hash = 0;
    while (*name != '\0')
	hash += (hash << 6) + *name++;
    return(hash);

} /*namehash*/

/*------------------------------------------------------------------------
 * [static] AllocConfigNode
 *
 * Description:
 *	 Allocate storage for a configuration tree node.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	Ptr to the freshly-allocated node if successful,
 *	Otherwise we'll exit the program.
 *
 * Environment:
 *	This routine is private to the module.
 *
 * Side Effects:
 *	May exit from package entirely.
 *------------------------------------------------------------------------*/

static CTREEPTR AllocConfigNode()

{ /*AllocConfigNode*/

    register CTREEPTR np;

    np = (CTREEPTR) emalloc(sizeof(CTREE));
    memset((char *)np, 0, sizeof(CTREE));
    return(np);

} /*AllocConfigNode*/

/*------------------------------------------------------------------------
 * [static] ValidUpdtSpec
 *
 * Description:
 *	Checks whether the given update specification is valid for a file
 *	of the given filetype.
 *
 * Arguments:
 *	u_short ftype : The type of file to be updated.
 *	u_short uspec : The update spec to check.
 *
 * Returns:
 *	TRUE if the check succeeds,
 *	FALSE otherwise.
 *
 * Environment:
 *	This routine is private to the module.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int ValidUpdtSpec(ftype, uspec)
    u_short ftype;
    u_short uspec;

{ /*ValidUpdtSpec*/

    register struct updatetype *u;

    /*
      * Scan the list of valid update specs, succeed if you find an
      * exact match.
      */
    for (u = validupdates; u->filetype != 0; u++)
	if ((u->filetype == ftype) && (u->updtflags == uspec))
	    return(TRUE);
    return(FALSE);

} /*ValidUpdtSpec*/

/*------------------------------------------------------------------------
 * [static] ValidateUserName
 *
 * Description:
 *	Given a pointer to a user name, see if that name is a valid one
 *	by possibly looking in the password file.  If it's valid, return
 *	the associated uid via the uidp parameter.
 *
 * Arguments:
 *	char *name  : Ptr to user name to validate.
 *	short *uidp : Matching uid is placed here upon success.
 *
 * Returns:
 *	TRUE on success,
 *	FALSE otherwise.
 *
 * Environment:
 *	This routine is private to the module.
 *
 * Side Effects:
 *	The passwd structure pointer, pw, is a static.  Thus, we
 *	recall the last search into the password file.  Before
 *	doing a lookup, we check to see if we've lucked out and
 *	the results are already cached.
 *------------------------------------------------------------------------*/

static int ValidateUserName(name, uidp)
    register char *name;
    register short *uidp;

{ /*ValidateUserName*/

    register afs_int32 uid;
    char *nptr;
    static struct passwd *pw = NULL;	/*Ptr to passwd file entry*/
    
    /*
      * We always know what root is; don't bother with a passwd lookup
      * in this case.
      */
    if (strcmp(name, "root") == 0) {
        *uidp = 0;
	return (TRUE);
    }

    /*
      * If we have the results from a previous search, don't do it
      * again.  Otherwise, take the plunge.
      */
    if (pw == NULL || strcmp(pw->pw_name, name))
      pw = getpwnam(name);

    if (pw == NULL) {
        uid = strtol(name, &nptr, 10);
	if ((int)(nptr-name) == strlen(name))
	    pw = getpwuid((uid_t)uid);
    }

    if (pw != NULL) {
      /*
	* Found the given name.  Return the matching pid.
	*/
	*uidp = pw->pw_uid;
	return(TRUE);
      }
    else
      /*
	* Abject failure.
	*/
      return(FALSE);

} /*ValidateUserName*/

/*------------------------------------------------------------------------
 * [static] ValidateGroupName
 *
 * Description:
 *	Given a pointer to a group name, see if that name is a valid one
 *	by possibly looking in the group file.  If it's valid, return
 *	the associated gid via the gidp parameter.
 *
 * Arguments:
 *	char *name  : Ptr to group name to validate.
 *	short *gidp : Matching gid is placed here upon success.
 *
 * Returns:
 *	TRUE on success,
 *	FALSE otherwise.
 *
 * Environment:
 *	This routine is private to the module.
 *
 * Side Effects:
 *	The group structure pointer, gr, is a static.  Thus, we
 *	recall the last search into the group file.  Before
 *	doing a lookup, we check to see if we've lucked out and
 *	the results are already cached.
 *------------------------------------------------------------------------*/

static int ValidateGroupName(name, gidp)
    register char *name;
    register short *gidp;

{ /*ValidateGroupName*/

    register afs_int32 gid;
    char *nptr;
    static struct group *gr = NULL;	/*Ptr to group structure*/

    /*
      * We always know the group number for wheel, so don't bother doing
      * any lookups.
      */
    if (strcmp(name, "wheel") == 0) {
        *gidp = 0;
	return (TRUE);
    }

    /*
      * If we have the results from a previous search, don't do it
      * again.  Otherwise, take the plunge.
      */
    if (gr == NULL || strcmp(gr->gr_name, name))
      gr = getgrnam(name);

    if (gr == NULL) {
        gid = strtol(name, &nptr, 10);
	if ((int)(nptr-name) == strlen(name))
	    gr = getgrgid((gid_t)gid);
    }

    if (gr != NULL) {
      /*
	* Found the given group.  Return the matching gid.
	*/
      *gidp = gr->gr_gid;
      return(TRUE);
    }
    else
      return(FALSE);

} /*ValidateGroupName*/

/*------------------------------------------------------------------------
 * InitializeConfigTree
 *
 * Description:
 *	Allocates storage for the root of the configuration tree.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	0 if successful.  On failure, package exits.
 *
 * Environment:
 *	The newly-allocated space is recorded in the global variable
 *	config_root.
 *
 * Side Effects:
 *	As described; may exit from package.
 *------------------------------------------------------------------------*/

int InitializeConfigTree()

{ /*InitializeConfigTree*/

    config_root = AllocConfigNode();
    return(0);

} /*InitializeConfigTree*/

/*------------------------------------------------------------------------
 * LocateChildNode
 *
 * Description:
 *	Locate the node corresponding to the given name in the entries
 *	corresponding to the specified directory node.
 *
 * Arguments:
 *	CTREEPTR dp : Config tree node whose entries are to be searched
 *			for the given name.
 *	char *name  : Ptr to the string name to search for.
 *	int lmode   : Lookup mode, either C_LOCATE or C_CREATE.
 *
 * Returns:
 *	Ptr to located node if it existed, else
 *	Ptr to newly-created node if no match found and C_CREATE mode used,
 *	Otherwise a null pointer.  Null will also be returned if the given
 *		directory node isn't really for a directory.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	May create entries and nodes; may exit package if any of these
 *	creations fail.
 *------------------------------------------------------------------------*/

CTREEPTR LocateChildNode(dp, name, lmode)
    register CTREEPTR dp;
    register char *name;
    register int lmode;

{ /*LocateChildNode*/

    register int hash;		/*Hash value for given name*/
    register ENTRYPTR ep;	/*Ptr to entry being examined*/
    register int found_entry;	/*Found entry we want?*/

    /*
      * First, make sure dp corresponds to a directory
      */
    if (dp->type != S_IFDIR)
	return(NULL);

    /*
      * Set up to search the entries hanging off the directory node.
      * Precompute the hash value for the name.
      */
    hash = namehash(name);
    ep = dp->entryp;
    found_entry = FALSE;

    /*
      * Sweep through the list of entries until we find our match or
      * fall off the end.
      */
    while ((ep != NULL) && !found_entry) {
      /*
	* We compare the hash value first, and only if that succeeds
	* do we do the string compare.
	*/
      if ((ep->hash == hash) && (strcmp(ep->name, name) == 0))
	found_entry = TRUE;
      else
	/*
	  * No match.  Move on to the next entry, if any.
	  */
	ep = ep->nextp;

    } /*Search list of entries*/

    /*
      * If we found it, return the node hanging off the entry.
      */
    if (found_entry)
      return(ep->nodep);

    /*
      * We didn't find the given name.  If we aren't supposed to create
      * a node for the name, we return failure.
      */
    if (!(lmode & C_CREATE))
      return(NULL);

    /*
      * Create a new entry and node to stand for the given name, link it
      * in, and return it to our caller.
      */
    ep = (ENTRYPTR)emalloc(sizeof(ENTRY));
    ep->nodep = AllocConfigNode();
    ep->name = (char *) emalloc((unsigned)(strlen(name)+1));
    ep->hash = hash;
    (void)strcpy(ep->name, name);
    ep->nextp = dp->entryp;
    dp->entryp = ep;
    return(ep->nodep);

} /*LocateChildNode*/

/*------------------------------------------------------------------------
 * LocatePathNode
 *
 * Description:
 *	Locates the configuration tree node corresponding to the given
 *	file or directory.
 *
 * Arguments:
 *	CTREEPTR dp : Config tree node from which search is to begin.
 *			(Actually, config_root!!)
 *	char *path  : Path to be searched for.
 *	int lmode   : Search mode to use (C_LOCATE, C_CREATE).
 *
 * Returns:
 *	Ptr to located node if it existed, else
 *	Ptr to newly-created node if no match found and C_CREATE mode used,
 *	Otherwise a null pointer.  Null will also be returned if the given
 *		directory node isn't really for a directory.
 *
 * Environment:
 *	The search is breadth-first, plucking out each piece of the path
 *	from left to right.
 *
 * Side Effects:
 *	Exits from package.
 *------------------------------------------------------------------------*/

CTREEPTR LocatePathNode(dp, path, lmode)
    register CTREEPTR dp;
    register char *path;
    register int lmode;

{ /*LocatePathNode*/

    register char *name;	/*Points to start of new subdir/file in path*/
    register char savech;	/*Saves chars being murdered during search*/

    /*
      * Skip over leading slashes.
      */
    while (*path == '/')
	path++;

    while (dp != NULL && *path != '\0') {
	/*
	  * Pull off the leftmost portion of the (remaining) pathname,
	  * then search for it through all the entries for the current
	  * directory node serving as the root of the search.
	  */
	name = path;
	while (*path != '\0' && *path != '/')
	    path++;
	savech = *path; *path = '\0';
	if ((lmode & C_CREATE) && (dp->type == 0))
	    /*
	      * This is an unfilled non-leaf node.  Mark it as being
	      * a directory node
	      */
	    dp->type = S_IFDIR;

	/*
	  * Look for the name fragment among all entries corresponding
	  * to the root node.
	  */
	dp = LocateChildNode(dp, name, lmode);

	/*
	  * Restore the char we overwrote with a null, then bump the
	  * path to the start of the next component.
	  */
	*path = savech;
	while (*path == '/')
	    path++;

      } /*while loop*/

    /*
      * dp now contains the path associated with the given path, so
      * just return it.
      */
    return(dp);

} /*LocatePathNode*/

/*------------------------------------------------------------------------
 * BuildConfigTree
 *
 * Description:
 *	Builds a configuration tree from its YACC specification.
 *
 * Arguments:
 *	FILE *f : File containing the configuration info.
 *
 * Returns:
 *	Value returned by the YACC parser (0 for success, else 1), or
 *	Exits if there is insufficient memory in which to build the tree.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	May exit from package.
 *------------------------------------------------------------------------*/

int BuildConfigTree(f)
    FILE *f;

{ /*BuildConfigTree*/

    int ret;

    yyin = f;
    ret = yyparse();
    return(ret);

}  /*BuildConfigTree*/

/*------------------------------------------------------------------------
 * [static] AddEntry
 *
 * Description:
 *	Adds an entry to the configuration tree.
 *
 * Arguments:
 *	u_short filetype    : Specifies type of file to be updated (regular
 *				file, directory, device, etc.)
 *	u_short updtspec    : Specifies actions during update, if necessary.
 *	char *filename	    : Name of file to be updated.
 *	PROTOTYPE prototype : Prototype for filename (e.g., dev numbers,
 *				directory prefix, etc.)
 *	OWNER ownershipinfo : Ownership info for filename.
 *	MODE mode	    : Protection (mode) bits for filename.
 *
 * Returns:
 *	0 on success,
 *	Error value otherwise.
 *	If there is insufficient memory to add the entry, or if invalid
 *		parameters are encountered, the entire program exits.
 *
 * Environment:
 *	Searches always start from the root of the configuration tree.
 *
 * Side Effects:
 *	As advertised; may exit from package.
 *------------------------------------------------------------------------*/

int AddEntry(filetype, updtspec, filename, prototype, ownershipinfo, mode)
    u_short filetype;
    u_short updtspec;
    char *filename;
    PROTOTYPE prototype;
    OWNER ownershipinfo;
    MODE mode;

{ /*AddEntry*/

    CTREEPTR np;	/*Ptr to config tree node holding info on filename*/
    short uid, gid;	/*Uid, gid returned from validation functions*/
    
    debug_message("[AddEntry] Called for filename %s", filename);
    
    /*
     * Check that the given update specification is a legal one.
     */
    if (ValidUpdtSpec(filetype, updtspec) == FALSE)
	fatal("** Invalid update specification for file %s", filename);
    
    /*
     * Find the node corresponding to the given filename, creating one if
     * necessary.
     */
    if ((np = LocatePathNode(config_root, filename, C_LOCATE|C_CREATE)) == NULL)
	fatal("** Invalid path encountered: %s", filename);
    
    /*
     * Start adding entries to np after checking for potential conflicts.
     *
     * Should we print out a warning if the same file appears twice even
     * in the absence of a type confict?
     */
    if ((np->flag & F_TYPE) && (np->type != filetype))
	fatal("** Type conflict for file %s", filename);
    
    np->flag |= F_TYPE;
    np->type = filetype;
    
#if 0
    if ((np->flag & F_UPDT) && (np->updtspec != updtspec))
	fatal("** Update specification conflict for file %s", filename);
    else
#endif /* 0 */
    {
	np->flag |= F_UPDT;
	np->updtspec |= updtspec;
    }
    
    if ((filetype == S_IFCHR)
	|| (filetype == S_IFBLK)
#ifdef S_IFIFO
	|| (filetype == S_IFIFO)
#endif /* S_IFIFO */
	) {
	/*
	 * Device prototype
	 */
	if ((np->flag & F_PROTO) && (prototype.flag != P_NONE)) {
	    if ((prototype.flag != P_DEV)
		|| ((np->proto.info.rdev != prototype.info.rdev)))
		fatal("** Device number conflict for device %s", filename);
	}
	else
	    if (prototype.flag == P_FILE)
		fatal("** Prototype conflict for device %s", filename);
	    else
		if (prototype.flag == P_DEV) {
		    np->flag |= F_PROTO;
		    np->proto.flag = P_DEV;
		    np->proto.info.rdev = prototype.info.rdev;
		}
    }
    else {
	/*
	 * File prototype, if any
	 */
	if ((np->flag & F_PROTO) && (prototype.flag != P_NONE)) {
	    if ((prototype.flag != P_FILE)
		|| (strcmp(np->proto.info.path, prototype.info.path)))
		fatal("** Prototype conflict for file %s", filename);
	}
	else
	    if (prototype.flag == P_DEV)
		fatal("** Prototype conflict for file %s", filename);
	    else
		if (prototype.flag == P_FILE) {
		    np->flag |= F_PROTO;
		    np->proto.flag = P_FILE;
		    np->proto.info.path =
			emalloc((unsigned)(strlen(prototype.info.path)+1));
		    (void) strcpy(np->proto.info.path, prototype.info.path);
		}
    }
    
    if (ownershipinfo.username != NULL) {
	/*
	 * Ownership info, if any
	 */
	if (ValidateUserName(ownershipinfo.username, &uid) == FALSE)
	    fatal("** Unknown user %s for file %s",
		  ownershipinfo.username, filename);
	else
	    if ((np->flag & F_UID) && (np->uid != uid))
		fatal("** Uid conflict for file %s (new val: %d, old val: %d)",
		      filename, np->uid, uid);
	    else {
		np->flag |= F_UID;
		np->uid = uid;
	    }
    } /*Process user ownership info*/
    
    if (ownershipinfo.groupname != NULL) {
	if (ValidateGroupName(ownershipinfo.groupname, &gid) == FALSE)
	    fatal("** Unknown group %s for file %s",
		  ownershipinfo.groupname, filename);
	else
	    if ((np->flag & F_GID) && (np->gid != gid))
		fatal("** Gid conflict for file %s (new val: %d, old val: %d)",
		      filename, np->gid, gid);
	    else {
		np->flag |= F_GID; np->gid = gid;
	    }
    } /*Process group ownership info*/
    
    if (mode.inherit_flag != TRUE) {
	if (mode.modeval > (u_short)~S_IFMT)
	    fatal("** Bad mode %d for file %s", mode.modeval, filename);
	if ((np->flag & F_MODE) && ((np->mode & ~S_IFMT) != mode.modeval))
	    fatal("** Mode conflict for file %s", filename);
	else {
	    np->flag |= F_MODE;
	    np->mode |= mode.modeval;
	}
    } /*Mode inherit flag turned off*/
    
    /*
     * If we reached this point, everything must have been OK
     */
    return(0);

}  /*AddEntry*/

/*------------------------------------------------------------------------
 * ApplyConfigTree
 *
 * Description:
 *	Apply the given function to each node of the configuration tree
 *	in pre-order fashion.
 *
 * Arguments:
 *	int (*func)() : Function to apply.
 *
 * Returns:
 *	Void.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	Whatever the given function does.
 *------------------------------------------------------------------------*/

void ApplyConfigTree(func)
    int (*func)();

{ /*ApplyConfigTree*/

    char *path;	/*Path to pass on down*/

    /*
      * Create room for the largest path possible, and set it to the
      * null string.  This forces the application to be started at
      * ``/''.
      */
    path = (char *)emalloc(MAXPATHLEN+1);
    path[0] = '\0';

    TraverseConfigTree(config_root, path, func);

} /*ApplyConfigTree*/

/*------------------------------------------------------------------------
 * TraverseConfigTree
 *
 * Description:
 *	Traverses the subtree of the configuration tree rooted at np
 *	in pre-order fashion, applying function func to each node.
 *
 * Arguments:
 *	CTREEPTR np   : Root of config tree to traverse.
 *	char *path    : Path on which to start traversal.
 *	int (*func)() : Function to apply to each node.
 *
 * Returns:
 *	Void.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	Whatever func might do.
 *------------------------------------------------------------------------*/

TraverseConfigTree(np, path, func)
    register CTREEPTR np;
    char *path;
    int (*func)();

{ /*TraverseConfigTree*/

    register char *endp;	/*Marks the end of a string*/
    register ENTRYPTR ep;	/*Current entry pointer*/
    register int len;		/*Length of the pathname*/

    /*
      * If the path is empty, start it off with "/".
      */
    len = strlen(path);
    if (len == 0)
	(void) strcpy(path, "/");

    /*
      * Apply the function to the current node.
      */
    (void)(*func)(np, path);

    if (len == 0)
	(void) strcpy(path, "");

    /*
      * If we've reached a leaf node (a non-directory), start heading
      * back up.
      */
    if ((np->type) != S_IFDIR)
      return;

    /*
      * We're currently at a directory node.  For each entry in the entry
      * list for this node, conjure up the name associated with the entry's
      * node and call ourselves recursively.  This calling sequence gives
      * us the preorder traversal.
      */
    endp = path + len;
    *endp++ = '/';
    *endp = 0;
    for (ep = np->entryp; ep; ep = ep->nextp) {
      /*
	* Tack on the node's name component to the end of the path and
	* descend.
	*/
      (void) strcpy(endp, ep->name);
      TraverseConfigTree(ep->nodep, path, func);
    }

    /*
      * We've finished the preorder walk under this node.  Terminate
      * the path properly before returning.
      */
    *--endp = 0;

}  /*TraverseConfigTree*/
