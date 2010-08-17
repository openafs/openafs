/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *	Interface to the basic procedures for the AFS user account
 *	facility.
 *
 */

#ifndef _USS_PROCS_H_
#define _USS_PROCS_H_ 1

/*
 * --------------------- Required definitions ---------------------
 */
#include "uss_common.h"		/*Commons uss definitions */


/*
 * --------------------- Exported definitions ---------------------
 */
#define uss_procs_YOUNG		1
#define uss_procs_ANCIENT	2


/*
 * ------------------------ Exported functions  -----------------------
 */
extern afs_int32 uss_procs_BuildDir(char *a_path, char *a_mode,
				    char *a_owner, char *a_access);
    /*
     * Summary:
     *    Create and fully set up a directory for the user.
     *
     * Args:
     *    char *a_path   : Pathname of directory to create.
     *    char *a_mode   : Unix mode to set for directory.
     *    char *a_owner  : Directory's owner.
     *    char *a_access : ACL to set on the directory.
     *
     * Returns:
     *    0 if everything went well,
     *    1 if there was a problem in the routine itself, or
     *    Other error code if problem occurred in lower-level call.
     */

extern afs_int32 uss_procs_CpFile(char *a_path, char *a_mode,
				  char *a_owner, char *a_proto);
    /*
     * Summary:
     *    Copy the given file to the user's directory.
     *
     * Args:
     *    char *a_path  : Pathname of file to create.
     *    char *a_mode  : Unix mode to set for file.
     *    char *a_owner : File's owner.
     *    char *a_proto : Prototype to copy from.
     *
     * Returns:
     *    0 if everything went well,
     *    1 if there was a problem in the routine itself, or
     *    Other error code if problem occurred in lower-level call.
     */

extern afs_int32 uss_procs_EchoToFile(char *a_path, char *a_mode,
		                      char *a_owner, char *a_content);
    /*
     * Summary:
     *    Put the specified contents into the given file.
     *
     * Args:
     *    a_path    : Pathname of file to fill in.
     *    a_mode    : Unix mode to set for file.
     *    a_owner   : File's owner.
     *    a_content : Value to insert into file.
     *
     * Returns:
     *    0 if everything went well,
     *    1 if there was a problem in the routine itself, or
     *    Other error code if problem occurred in lower-level call.
     */

extern afs_int32 uss_procs_Exec(char *a_command);
    /*
     * Summary:
     *    Execute the given Unix command.
     *
     * Args:
     *    char *a_command : Unix command to execute.
     *
     * Returns:
     *    0 if everything went well,
     *    1 if there was a problem in the routine itself, or
     *    Other error code if problem occurred in lower-level call.
     */

extern afs_int32 uss_procs_SetLink(char *a_path1, char *a_path2,
				   char a_type);
    /*
     * Summary:
     *    Create either the given symbolic or hard link.
     *
     * Args:
     *    char *a_path1 : Name of the link itself.
     *    char *a_path2 : Name of what the link points to.
     *    char a_type   : Specifies either a symbolic or hard link.
     *
     * Returns:
     *    0 if everything went well,
     *    1 if there was a problem in the routine itself, or
     *    Other error code if problem occurred in lower-level call.
     */

extern int uss_procs_AddToDirPool(char *a_dirToAdd);
    /*
     * Summary:
     *    Add the given pathname to the $AUTH directory pool
     *
     * Args:
     *    char *a_dirToAdd : Name of directory to add.
     *
     * Returns:
     *    Result of addition.
     */

extern FILE *uss_procs_FindAndOpen(char *a_fileToOpen);
    /*
     * Summary:
     *    Given a template filename, get that file open and return the
     *    file descriptor.  If a full pathname is given, try to open it
     *    directly.  Otherwise, use the default pathname prefixes.
     *
     * Args:
     *    char *a_fileToOpen : Name of file to open.
     *
     * Returns:
     *    The file descriptor for the open file (if it could be done),
     *    NULL otherwise.
     */

extern void uss_procs_PrintErr(int a_lineNum, char *a_fmt, ... );
    /*
     * Summary:
     *    Print out an error connected with template file parsing.
     *
     * Args:
     *    int a_lineNum : Template file line number having the error.
     *    char *a_fmt   : Format string to use.
     *
     * Returns:
     *    Nothing.
     */

extern int uss_procs_GetOwner(char *a_ownerStr);
    /*
     * Summary:
     *    Translate the owner string to the owner uid.
     *
     * Args:
     *    char *a_ownerStr : Name of the owner.
     *
     * Returns:
     *    Owner's uid.
     */

extern afs_int32 uss_procs_PickADir(char *path, char *cp);
    /*
     * Summary:
     *    Tries to replace $AUTO by a subdir.  Subdirs are given by means
     *    of "G" in the configuration file.  Each of the directories is
     *    examined, and the one with the inimum number of entries is
     *    picked.
     *
     * Args:
     *    a_path : ???
     *    a_cp   : ???
     *
     * Returns:
     *    0 if everything went well,
     *   -1 if there was a problem.
     */

#endif /* _USS_PROCS_H_ */
