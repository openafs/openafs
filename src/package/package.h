/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*------------------------------------------------------------------------
 * package.h
 *
 * Description:
 *	Definitions for package, the AFS workstation configuration
 *	facility.
 *
 *------------------------------------------------------------------------*/

#include <utmp.h>
#include <sys/types.h>

/*
 * Flags specifying update actions
 */
#define	U_LOSTFOUND	0001
#define	U_RMEXTRA	0002
#define	U_NOOVERWRITE	0004
#define	U_RENAMEOLD	0010
#define	U_ABSPATH	0020
#define	U_REBOOT	0040

/*
 * Specification of prototype info
 */
#define	P_NONE	0
#define	P_FILE	1
#define	P_DEV	2

/*
 * Flag for ownership info
 */
#define	UID_INHERIT 01
#define	GID_INHERIT 02

/*
 * Lookup modes for the configuration tree
 */
#define	C_LOCATE    01
#define	C_CREATE    02

/*
 * Flags for fields of config tree nodes
 */
#define	F_TYPE	0001
#define	F_UPDT	0002
#define	F_PROTO	0004
#define	F_UID	0010
#define	F_GID	0020
#define	F_MODE	0040
#define	F_MTIME	0100

/*
 * Current operating status
 */
#define	status_noerror	0
#define	status_error	2
#define	status_reboot	4

typedef struct prototype_struct
{
    u_short flag;	/*Union tag, or specifies absence of prototype*/
    union {
	char *path;	/*Path, dir prefix, or absolute path of prototype*/
	afs_uint32	rdev;	/*Device number*/
    } info;
} PROTOTYPE;

typedef struct owner_struct
{
    char *username;	/*Associated owner*/
    char *groupname;	/*Associated group*/
} OWNER;

typedef struct mode_struct
{
    u_short inherit_flag;   /*Specifies whether the mode is inherited
			      from the prototype or is given by the
			      mode field */
    afs_uint32 modeval;
} MODE;

typedef struct entry
{
    struct entry *nextp;	/*Ptr to next entry in the same dir*/
    struct node *nodep;		/*Ptr to config tree node w/info on this file*/
    int hash;			/*Hashed value for quick filename comparison*/
    char *name;			/*Actual file/directory name*/
} ENTRY, *ENTRYPTR;

typedef struct node
{
    ENTRYPTR entryp;	/*Ptr to child list for this node, if a directory*/
    u_short flag;	/*Keeps track of updates to fields of this node*/
    u_short type;	/*Type of file/directory*/
    u_short updtspec;	/*Update spec*/
    PROTOTYPE proto;	/*Prototype info*/
    short uid;		/*Ownership info*/
    short gid;		/*Group info*/
    u_short mode;	/*Mode info*/
    time_t mtime;	/*Last modification time*/
} CTREE, *CTREEPTR;

extern int status;	/*Operating status*/
extern int opt_lazy;	/*Just tell what you would have done, don't do it*/
extern int opt_silent;	/*Don't print any error messages*/
extern int opt_verbose;	/*Be chatty?*/
extern int opt_reboot;	/*Do files that will cause reboot*/
#ifdef KFLAG
extern int opt_kflag;	/* $$question: why was this ifdefed? */
#endif /* KFLAG */
extern int opt_debug;	/*Turn debugging output on*/

extern CTREEPTR config_root;	/*Top of the config tree*/
