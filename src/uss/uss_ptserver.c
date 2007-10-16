/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *	Implementation of basic procedures for the AFS user account
 *	facility.
 */

/*
 * --------------------- Required definitions ---------------------
 */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/uss/uss_ptserver.c,v 1.7.2.1 2007/04/10 18:43:46 shadow Exp $");

#include "uss_ptserver.h"	/*Module interface */
#include <afs/ptclient.h>	/*Protection Server client interface */
#include <afs/pterror.h>	/*Protection Server error codes */
#include <afs/com_err.h>	/*Error code xlation */


#undef USS_PTSERVER_DB

extern int line;


/*
 * ---------------------- Private definitions ---------------------
 */
#define uss_ptserver_MAX_SIZE	2048


/*
 * ------------------------ Private globals -----------------------
 */
static int initDone = 0;	/*Module initialized? */


/*-----------------------------------------------------------------------
 * static InitThisModule
 *
 * Description:
 *	Set up this module, namely make the connection to the Protection
 *	Server.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	0 if everything went fine, or
 *	lower-level error code otherwise.
 *
 * Environment:
 *	This routine will only be called once.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static afs_int32
InitThisModule()
{				/*InitThisModule */

    static char rn[] = "uss_ptserver:InitThisModule";	/*Routine name */
    register afs_int32 code;	/*Return code */

    /*
     * Only once, guys.
     */
    if (initDone)
	return (0);

    /*
     * Connect up with the Protection Server.
     */
#ifdef USS_PTSERVER_DB
    printf
	("%s: Initializing Protection Server: security=1, confdir = '%s', cell = '%s'\n",
	 rn, uss_ConfDir, uss_Cell);
#endif /* USS_PTSERVER_DB */
    code = pr_Initialize(1,	/*Security level */
			 uss_ConfDir,	/*Config directory */
			 uss_Cell);	/*Cell to touch */
    if (code) {
	afs_com_err(uss_whoami, code,
		"while initializing Protection Server library");
	return (code);
    }

    initDone = 1;
    return (0);

}				/*InitThisModule */


/*-----------------------------------------------------------------------
 * EXPORTED uss_ptserver_AddUser
 *
 * Environment:
 *	The common DesiredUID variable, if non-zero, is the value
 *	desired for the user's uid.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_ptserver_AddUser(a_user, a_uid)
     char *a_user;
     char *a_uid;

{				/*uss_ptserver_AddUser */

    afs_int32 code;		/*Various return codes */
    afs_int32 id = uss_DesiredUID;	/*ID desired for user, if any */
    afs_int32 mappedUserID;	/*ID user already has */

    if (uss_verbose) {
	fprintf(stderr, "Adding user '%s' to the Protection DB\n", a_user);
	if (id)
	    fprintf(stderr, "\t[Presetting uid to %d]\n", id);
    }

    /*
     * Make sure we're initialized before doing anything.
     */
    if (!initDone) {
	code = InitThisModule();
	if (code)
	    return (code);
    }

    /*
     * If this is a dry run, we still need to setup the uid before
     * returning.
     */
    if (uss_DryRun) {
	fprintf(stderr, "\t[Dry run - user %d not created]\n",
		uss_DesiredUID);
	sprintf(a_uid, "%d", uss_DesiredUID);
	return (0);
    }

    /*
     * Go ahead and create the user.
     */
    code = pr_CreateUser(a_user, &id);
    if (code) {
	if (code == PREXIST || code == PRIDEXIST) {
	    if (code == PREXIST)
		fprintf(stderr,
			"%s: Warning: '%s' already in the Protection DB\n",
			uss_whoami, a_user);
	    else
		fprintf(stderr,
			"%s: Warning: Id '%d' already in Protection DB\n",
			uss_whoami, id);

	    /*
	     * Make sure the user name given matches the id that has
	     * already been registered with the Protection Server.
	     *
	     * Note: pr_SNameToId ONLY returns a non-zero error code
	     * for a major problem, like a network partition, so we
	     * have to explicitly check the ID returned against
	     * ANONYMOUSID, which is what we get when there is no
	     * ID known for the user name.
	     */
	    mappedUserID = id;
	    if (code = pr_SNameToId(a_user, &mappedUserID)) {
		afs_com_err(uss_whoami, code,
			"while getting uid from Protection Server");
		return (code);
	    }
	    if (mappedUserID == ANONYMOUSID) {
		fprintf(stderr,
			"%s: User '%s' unknown, yet given id (%d) already has a mapping!\n",
			uss_whoami, a_user, id);
		return (PRIDEXIST);
	    }
	    if (id == 0)
		id = mappedUserID;
	    else if (mappedUserID != id) {
		fprintf(stderr,
			"%s: User '%s' already has id %d; won't assign id %d\n",
			uss_whoami, a_user, mappedUserID, id);
		return (PRIDEXIST);
	    }
	} else {
	    /*
	     * Got a fatal error.
	     */
	    afs_com_err(uss_whoami, code, "while accessing Protection Server");
	    return (code);
	}
    }
    /*Create the user's protection entry */
    sprintf(a_uid, "%d", id);
    if (uss_verbose)
	fprintf(stderr, "The uid for user '%s' is %s\n", a_user, a_uid);

    /*
     * Return sweetness & light.
     */
    return (0);

}				/*uss_ptserver_AddUser */


/*-----------------------------------------------------------------------
 * EXPORTED uss_ptserver_DelUser
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_ptserver_DelUser(a_name)
     char *a_name;

{				/*uss_ptserver_DelUser */

    afs_int32 code;		/*Various return codes */

    /*
     * Make sure we're initialized before doing anything.
     */
    if (!initDone) {
	code = InitThisModule();
	if (code)
	    return (code);
    }

    if (uss_DryRun) {
	fprintf(stderr,
		"\t[Dry run - user '%s' not deleted from Protection DB]\n",
		a_name);
	return (0);
    }

    if (uss_verbose)
	fprintf(stderr, "Deleting user '%s' from the Protection DB\n",
		a_name);

    /*
     * Go ahead and delete the user.
     */
    code = pr_Delete(a_name);
    if (code) {
	if (code == PRNOENT) {
	    /*
	     * There's no entry for that user in the Protection DB,
	     * so our job is done.
	     */
	    fprintf(stderr,
		    "%s: Warning: User '%s' not found in Protection DB\n",
		    uss_whoami, a_name);
	} /*User not registered */
	else {
	    afs_com_err(uss_whoami, code,
		    "while deleting user from Protection DB");
	    return (code);
	}			/*Fatal PTS error */
    }

    /*Error in deletion */
    /*
     * Return sweetness & light.
     */
    return (0);

}				/*uss_ptserver_DelUser */


/*-----------------------------------------------------------------------
 * EXPORTED uss_ptserver_XlateUser
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_ptserver_XlateUser(a_user, a_uidP)
     char *a_user;
     afs_int32 *a_uidP;

{				/*uss_ptserver_XlateUser */

    static char rn[] = "uss_ptserver_XlateUser";	/*Routine name */
    register afs_int32 code;	/*Various return codes */

    if (uss_verbose)
	fprintf(stderr, "Translating user '%s' via the Protection DB\n",
		a_user);

    /*
     * Make sure we're initialized before doing anything.
     */
    if (!initDone) {
	code = InitThisModule();
	if (code)
	    return (code);
    }

    /*
     * Note: pr_SNameToId ONLY returns a non-zero error code
     * for a major problem, like a network partition, so we
     * have to explicitly check the ID returned against
     * ANONYMOUSID, which is what we get when there is no
     * ID known for the user name.
     */
    *a_uidP = 0;
    code = pr_SNameToId(a_user, a_uidP);
    if (code) {
	afs_com_err(uss_whoami, code, "while getting uid from Protection DB");
	return (code);
    }
    if (*a_uidP == ANONYMOUSID) {
	fprintf(stderr, "%s: No entry for user '%s' in the Protection DB\n",
		uss_whoami, a_user);
	return (code);
    }

    /*
     * Return sweetness & light.
     */
#ifdef USS_PTSERVER_DB
    printf("%s: User '%s' maps to uid %d\n", rn, a_user, *a_uidP);
#endif /* USS_PTSERVER_DB */
    return (0);

}				/*uss_ptserver_XlateUser */
