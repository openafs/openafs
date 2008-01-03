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
    ("$Header$");

#include "uss_kauth.h"		/*Module interface */
#include "uss_common.h"		/*Common defs & operations */
#include <errno.h>
#include <pwd.h>

#include <string.h>

#include <afs/com_err.h>
#include <afs/kautils.h> /*MAXKTCREALMLEN*/
#include <afs/kaport.h>		/* pack_long */
#define uss_kauth_MAX_SIZE	2048
#undef USS_KAUTH_DB
/*
 * ---------------------- Exported variables ----------------------
 */
struct ubik_client *uconn_kauthP;	/*Ubik connections
					 * to AuthServers */

/*
 * ------------------------ Private globals -----------------------
 */
static int initDone = 0;	/*Module initialized? */
static char CreatorInstance[MAXKTCNAMELEN];	/*Instance string */
static char UserPrincipal[MAXKTCNAMELEN];	/*Parsed user principal */
static char UserInstance[MAXKTCNAMELEN];	/*Parsed user instance */
static char UserCell[MAXKTCREALMLEN];	/*Parsed user cell */
int doUnlog = 0;

/*-----------------------------------------------------------------------
 * EXPORTED uss_kauth_InitAccountCreator
 *
 * Environment:
 *      The command line must have been parsed.
 *
 * Side Effects:
 *      As advertised.
 *-----------------------------------------------------------------------*/

afs_int32
uss_kauth_InitAccountCreator()
{				/*uss_kauth_InitAccountCreator */

    char *name;
    struct passwd *pw;
    int dotPosition;

    /*
     * Set up the identity of the principal performing the account
     * creation (uss_AccountCreator).  It's either the administrator
     * name provided at the call or the identity of the caller as
     * gleaned from the password info.
     */
    if (uss_Administrator[0] != '\0') {
	name = uss_Administrator;
    } else {			/* Administrator name not passed in */
	pw = getpwuid(getuid());
	if (pw == 0) {
	    fprintf(stderr,
		    "%s: Can't figure out your name from your user id.\n",
		    uss_whoami);
	    return (1);
	}
	name = pw->pw_name;
    }

    /* Break the *name into principal and instance */
    dotPosition = strcspn(name, ".");
    if (dotPosition >= MAXKTCNAMELEN) {
	fprintf(stderr, "Admin principal name too long.\n");
	return (1);
    }
    strncpy(uss_AccountCreator, name, dotPosition);
    uss_AccountCreator[dotPosition] = '\0';

    name += dotPosition;
    if (name[0] == '.') {
	name++;
	if (strlen(name) >= MAXKTCNAMELEN) {
	    fprintf(stderr, "Admin instance name too long.\n");
	    return (1);
	}
	strcpy(CreatorInstance, name);
    } else {
	CreatorInstance[0] = '\0';
    }

#ifdef USS_KAUTH_DB_INSTANCE
    fprintf(stderr, "%s: Starting CreatorInstance is '%s', %d bytes\n",
	    uss_whoami, CreatorInstance, strlen(CreatorInstance));
#endif /* USS_KAUTH_DB_INSTANCE */

    return (0);
}

/*-----------------------------------------------------------------------
 * static InitThisModule
 *
 * Description:
 *	Set up this module, namely set up all the client state for
 *	dealing with the Volume Location Server(s), including
 *	network connections.
 *
 * Arguments:
 *	a_noAuthFlag : Do we need authentication?
 *	a_confDir    : Configuration directory to use.
 *	a_cellName   : Cell we want to talk to.
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

int Pipe = 0;
static char *
getpipepass()
{
    static char gpbuf[BUFSIZ];
    /* read a password from stdin, stop on \n or eof */
    register int i, tc;
    memset(gpbuf, 0, sizeof(gpbuf));
    for (i = 0; i < (sizeof(gpbuf) - 1); i++) {
	tc = fgetc(stdin);
	if (tc == '\n' || tc == EOF)
	    break;
	gpbuf[i] = tc;
    }
    return gpbuf;
}


afs_int32
InitThisModule()
{				/*InitThisModule */

    static char rn[] = "uss_kauth:InitThisModule";
    register afs_int32 code;
    char *name, prompt[2 * MAXKTCNAMELEN + 20];
    char *reasonString, longPassBuff[1024], shortPassBuff[9];
    struct ktc_encryptionKey key;
    struct ktc_token token, tok;
    struct ktc_principal Name;

    /*
     * Only call this routine once.
     */
    if (initDone)
	return (0);


    /*
     * Pull out the caller's administrator token if they have one.
     */
    code =
	ka_GetAdminToken(0, 0, uss_Cell, 0, 10 * 60 * 60, &token,
			 0 /*new */ );
    if (code) {
	if (Pipe) {
	    strncpy(longPassBuff, getpipepass(), sizeof(longPassBuff));
	} else {
	    /*
	     * Nope, no admin tokens available.  Get the key based on the
	     * full password and try again.
	     */
	    sprintf(prompt, "Password for '%s", uss_AccountCreator);
	    if (CreatorInstance[0])
		sprintf(prompt + strlen(prompt), ".%s", CreatorInstance);
	    strcat(prompt, "': ");
	    code = ka_UserReadPassword(prompt,	/*Prompt to use */
				       longPassBuff,	/*Long pwd buffer */
				       sizeof(longPassBuff),	/*Size of above */
				       &reasonString);
	    if (code) {
		afs_com_err(uss_whoami, code, "while getting password ");
#ifdef USS_KAUTH_DB
		printf("%s: Error code from ka_UserReadPassword(): %d\n", rn,
		       code);
#endif /* USS_KAUTH_DB */
		return (code);
	    }
	}
	ka_StringToKey(longPassBuff, uss_Cell, &key);
	code =
	    ka_GetAdminToken(uss_AccountCreator, CreatorInstance, uss_Cell,
			     &key, 24 * 60 * 60, &token, 0 /*new */ );
	if (code) {
	    if ((code == KABADREQUEST) && (strlen(longPassBuff) > 8)) {
		/*
		 * The key we provided just doesn't work, yet we
		 * suspect that since the password is greater than 8
		 * chars, it might be the case that we really need
		 * to truncate the password to generate the appropriate
		 * key.
		 */
		afs_com_err(uss_whoami, code,
			"while getting administrator token (trying shortened password next...)");
#ifdef USS_KAUTH_DB
		printf("%s: Error code from ka_GetAdminToken: %d\n", rn,
		       code);
#endif /* USS_KAUTH_DB */
		strncpy(shortPassBuff, longPassBuff, 8);
		shortPassBuff[8] = 0;
		ka_StringToKey(shortPassBuff, uss_Cell, &key);
		code =
		    ka_GetAdminToken(uss_AccountCreator, CreatorInstance,
				     uss_Cell, &key, 24 * 60 * 60, &token,
				     0 /*new */ );
		if (code) {
		    afs_com_err(uss_whoami, code,
			    "while getting administrator token (possibly wrong password, or not an administrative account)");
#ifdef USS_KAUTH_DB
		    printf("%s: Error code from ka_GetAdminToken: %d\n", rn,
			   code);
#endif /* USS_KAUTH_DB */
		    return (code);
		} else {
		    /*
		     * The silly administrator has a long password!  Tell
		     * him or her off in a polite way.
		     */
		    printf
			("%s: Shortened password accepted by the Authentication Server\n",
			 uss_whoami);
		}
	    } /*Try a shorter password */
	    else {
		/*
		 * We failed to get an admin token, but the password is
		 * of a reasonable length, so we're just hosed.
		 */
		afs_com_err(uss_whoami, code,
			"while getting administrator token (possibly wrong password, or not an administrative account)");
#ifdef USS_KAUTH_DB
		printf("%s: Error code from ka_GetAdminToken: %d\n", rn,
		       code);
#endif /* USS_KAUTH_DB */
		return (code);
	    }			/*Even the shorter password didn't work */
	}			/*Key from given password didn't work */
    }

    /*First attempt to get admin token failed */
    /*
     * At this point, we have acquired an administrator token.  Let's
     * proceed to set up a connection to the AuthServer.
     */
#ifdef USS_KAUTH_DB_INSTANCE
    fprintf(stderr,
	    "%s: CreatorInstance after ka_GetAdminToken(): '%s', %d bytes\n",
	    rn, CreatorInstance, strlen(CreatorInstance));
#endif /* USS_KAUTH_DB_INSTANCE */

    /*
     * Set up the connection to the AuthServer read/write site.
     */
    code =
	ka_AuthServerConn(uss_Cell, KA_MAINTENANCE_SERVICE, &token,
			  &uconn_kauthP);
    if (code) {
	afs_com_err(uss_whoami, code,
		"while establishing Authentication Server connection");
#ifdef USS_KAUTH_DB
	printf("%s: Error code from ka_AuthServerConn: %d\n", rn, code);
#endif /* USS_KAUTH_DB */
	return (code);
    }

    if (uss_Administrator[0]) {
	/*
	 * We must check to see if we have local tokens for admin since he'll may do
	 * various pioctl or calls to protection server that require tokens. Remember
	 * to remove this tokens at the end of the program...
	 */
	strcpy(Name.name, "afs");
	Name.instance[0] = '\0';
	strncpy(Name.cell, uss_Cell, sizeof(Name.cell));
	if (code =
	    ktc_GetToken(&Name, &token, sizeof(struct ktc_token), &tok)) {
	    code =
		ka_UserAuthenticateLife(0, uss_AccountCreator,
					CreatorInstance, uss_Cell,
					longPassBuff, 10 * 60 * 60,
					&reasonString);
	    if (!code)
		doUnlog = 1;
	}
    }

    /*
     * Declare our success.
     */
    initDone = 1;
    return (0);

}				/*InitThisModule */


/*-----------------------------------------------------------------------
 * EXPORTED uss_kauth_AddUser
 *
 * Environment:
 *	The uconn_kauthP variable may already be set to an AuthServer
 *	connection.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_kauth_AddUser(a_user, a_passwd)
     char *a_user;
     char *a_passwd;

{				/*uss_kauth_AddUser */

    static char rn[] = "uss_kauth_AddUser";	/*Routine name */
    struct ktc_encryptionKey key;
    afs_int32 code;

    if (uss_SkipKaserver) {
	/*
	 * Don't talk to the kaserver; assume calls succeded and simply return.
	 * Amasingly people want to update it (most likely kerberos) themselves...
	 */
	if (uss_verbose)
	    printf
		("[Skip Kaserver option - Adding of user %s in Authentication DB not done]\n",
		 a_user);
	return 0;
    }


    /*
     * Make sure the module has been initialized before we start trying
     * to talk to AuthServers.
     */
    if (!initDone) {
	code = InitThisModule();
	if (code)
	    exit(code);
    }

    /*
     * Given the (unencrypted) password and cell, generate a key to
     * pass to the AuthServer.
     */
    ka_StringToKey(a_passwd, uss_Cell, &key);

    if (!uss_DryRun) {
	if (uss_verbose)
	    fprintf(stderr, "Adding user '%s' to the Authentication DB\n",
		    a_user);

#ifdef USS_KAUTH_DB_INSTANCE
	fprintf(stderr,
		"%s: KAM_CreateUser: user='%s', CreatorInstance='%s', %d bytes\n",
		rn, a_user, CreatorInstance, strlen(CreatorInstance));
#endif /* USS_KAUTH_DB_INSTANCE */
	code = ubik_Call(KAM_CreateUser, uconn_kauthP, 0, a_user, UserInstance,	/*set by CheckUsername() */
			 key);
	if (code) {
	    if (code == KAEXIST) {
		if (uss_verbose)
		    fprintf(stderr,
			    "%s: Warning: User '%s' already in Authentication DB\n",
			    uss_whoami, a_user);
	    } else {
		afs_com_err(uss_whoami, code,
			"while adding user '%s' to Authentication DB",
			a_user);
#ifdef USS_KAUTH_DB
		printf("%s: Error code from KAM_CreateUser: %d\n", rn, code);
#endif /* USS_KAUTH_DB */
		return (code);
	    }
	}			/*KAM_CreateUser failed */
    } /*Not a dry run */
    else
	fprintf(stderr,
		"\t[Dry run - user '%s' NOT added to Authentication DB]\n",
		a_user);

    return (0);

}				/*uss_kauth_AddUser */


/*-----------------------------------------------------------------------
 * EXPORTED uss_kauth_DelUser
 *
 * Environment:
 *	The uconn_kauthP variable may already be set to an AuthServer
 *	connection.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_kauth_DelUser(a_user)
     char *a_user;

{				/*uss_kauth_DelUser */

    static char rn[] = "uss_kauth_DelUser";	/*Routine name */
    register afs_int32 code;	/*Return code */

    if (uss_SkipKaserver) {
	/*
	 * Don't talk to the kaserver; assume calls succeded and simply return.
	 * Amasingly people want to update it (most likely kerberos) themselves...
	 */
	if (uss_verbose)
	    printf
		("[Skip Kaserver option - Deleting of user %s in Authentication DB not done]\n",
		 a_user);
	return 0;
    }

    /*
     * Make sure the module has been initialized before we start trying
     * to talk to AuthServers.
     */
    if (!initDone) {
	code = InitThisModule();
	if (code)
	    exit(code);
    }

    if (!uss_DryRun) {
#ifdef USS_KAUTH_DB_INSTANCE
	printf("%s: KAM_DeleteUser: user='%s', CreatorInstance='%s'\n",
	       uss_whoami, a_user, CreatorInstance);
#endif /* USS_KAUTH_DB_INSTANCE */
	if (uss_verbose)
	    printf("Deleting user '%s' from Authentication DB\n", a_user);
	code = ubik_Call(KAM_DeleteUser,	/*Procedure to call */
			 uconn_kauthP,	/*Ubik client connection struct */
			 0,	/*Flags */
			 a_user,	/*User name to delete */
			 UserInstance);	/*set in CheckUserName() */
	if (code) {
	    if (code == KANOENT) {
		if (uss_verbose)
		    printf
			("%s: No entry for user '%s' in Authentication DB\n",
			 uss_whoami, a_user);
		return (0);
	    } else {
		afs_com_err(uss_whoami, code,
			"while deleting entry in Authentication DB\n");
#ifdef USS_KAUTH_DB
		printf("%s: Error code from KAM_DeleteUser: %d\n", rn, code);
#endif /* USS_KAUTH_DB */
		return (code);
	    }
	}			/*KAM_DeleteUser failed */
    } /*Not a dry run */
    else
	printf("\t[Dry run - user '%s' NOT deleted from Authentication DB]\n",
	       a_user);

    return (0);

}				/*uss_kauth_DelUser */


/*-----------------------------------------------------------------------
 * EXPORTED uss_kauth_CheckUserName
 *
 * Environment:
 *	The user name has already been parsed and placed into
 *	uss_User.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_kauth_CheckUserName()
{				/*uss_kauth_CheckUserName */

    static char rn[] = "uss_kauth_CheckUserName";	/*Routine name */
    register afs_int32 code;	/*Return code */

    if (uss_SkipKaserver) {
	/*
	 * Don't talk to the kaserver; assume calls succeded and simply return.
	 * Amasingly people want to update it (most likely kerberos) themselves...
	 */
	if (uss_verbose)
	    printf
		("[Skip Kaserver option - Checking of user name in Authentication DB not done]\n");
	return 0;
    }

    /*
     * Make sure the module has been initialized before we start trying
     * to talk to AuthServers.
     */
    if (!initDone) {
	code = InitThisModule();
	if (code)
	    exit(code);
    }

    /*
     * Use the AuthServer's own routine to decide if the parsed user name
     * is legal.  Specifically, it can't have any weird characters or
     * embedded instance or cell names. 
     */
    code = ka_ParseLoginName(uss_User, UserPrincipal, UserInstance, UserCell);
    if (strlen(UserInstance) > 0) {
	fprintf(stderr,
		"%s: User name can't have an instance string ('%s')\n",
		uss_whoami, UserInstance);
	return (-1);
    }
    if (strlen(UserCell) > 0) {
	fprintf(stderr, "%s: User name can't have a cell string ('%s')\n",
		uss_whoami, UserCell);
	return (-1);
    }
    if (strchr(UserPrincipal, ':') != NULL) {
	fprintf(stderr, "%s: User name '%s' can't have a colon\n", uss_whoami,
		UserPrincipal);
	return (-1);
    }
    if (strlen(UserPrincipal) > 8) {
	fprintf(stderr,
		"%s: User name '%s' must have 8 or fewer characters\n",
		uss_whoami, UserPrincipal);
	return (-1);
    }

    /*
     * The name's OK in my book.  Replace the user name with the parsed
     * value.
     */
    strcpy(uss_User, UserPrincipal);
    return (0);

}				/*uss_kauth_CheckUserName */


/*
 * EXPORTED uss_kauth_SetFields
 *
 * Environment:
 *	The uconn_kauthP variable may already be set to an AuthServer
 *	connection.
 *
 * Side Effects:
 *	As advertised.
 */

afs_int32
uss_kauth_SetFields(username, expirestring, reuse, failures, lockout)
     char *reuse;
     char *username;
     char *expirestring;
     char *failures;
     char *lockout;
{
    static char rn[] = "uss_kauth_SetFields";
    afs_int32 code;
    char misc_auth_bytes[4];
    int i;
    afs_int32 flags = 0;
    Date expiration = 0;
    afs_int32 lifetime = 0;
    afs_int32 maxAssociates = -1;
    afs_int32 was_spare = 0;
    char instance = '\0';
    int pwexpiry;
    int nfailures, locktime, hrs, mins;

    if (strlen(username) > uss_UserLen) {
	fprintf(stderr,
		"%s: * User field in add cmd too long (max is %d chars; truncated value is '%s')\n",
		uss_whoami, uss_UserLen, uss_User);
	return (-1);
    }

    strcpy(uss_User, username);
    code = uss_kauth_CheckUserName();
    if (code)
	return (code);

    /*  no point in doing this any sooner than necessary */
    for (i = 0; i < 4; misc_auth_bytes[i++] = 0);

    pwexpiry = atoi(expirestring);
    if (pwexpiry < 0 || pwexpiry > 254) {
	fprintf(stderr, "Password lifetime range must be [0..254] days.\n");
	fprintf(stderr, "Zero represents an unlimited lifetime.\n");
	fprintf(stderr,
		"Continuing with default lifetime == 0 for user %s.\n",
		username);
	pwexpiry = 0;
    }
    misc_auth_bytes[0] = pwexpiry + 1;

    if (!strcmp(reuse, "noreuse")) {
	misc_auth_bytes[1] = KA_NOREUSEPW;
    } else {
	misc_auth_bytes[1] = KA_REUSEPW;
	if (strcmp(reuse, "reuse"))
	  fprintf(stderr, "must specify \"reuse\" or \"noreuse\": \"reuse\" assumed\n");
    }

    nfailures = atoi(failures);
    if (nfailures < 0 || nfailures > 254) {
	fprintf(stderr, "Failure limit must be in [0..254].\n");
	fprintf(stderr, "Zero represents unlimited login attempts.\n");
	fprintf(stderr, "Continuing with limit == 254 for user %s.\n",
		username);
	misc_auth_bytes[2] = 255;
    } else
	misc_auth_bytes[2] = nfailures + 1;

    hrs = 0;
    if (strchr(lockout, ':'))
	sscanf(lockout, "%d:%d", &hrs, &mins);
    else
	sscanf(lockout, "%d", &mins);

    locktime = hrs*60 + mins;
    if (hrs < 0 || hrs > 36 || mins < 0) {
	fprintf(stderr,"Lockout times must be either minutes or hh:mm.\n");
	fprintf(stderr,"Lockout times must be less than 36 hours.\n");
	return KABADCMD;
    } else if (locktime > 36*60) {
	fprintf(stderr, "Lockout times must be either minutes or hh:mm.\n");
	fprintf(stderr, "Lockout times must be less than 36 hours.\n");
	fprintf(stderr, "Continuing with lock time == forever for user %s.\n",
		username);
	misc_auth_bytes[3] = 1;
    } else {
	locktime = (locktime * 60 + 511) >> 9;  /* ceil(l*60/512) */
	misc_auth_bytes[3] = locktime + 1;
    }

    if (uss_SkipKaserver) {
	if (uss_verbose)
	    printf("[Skipping Kaserver as requested]\n");
	return 0;
    }

    /*
     * Make sure the module has been initialized before we start trying
     * to talk to AuthServers.
     */
    if (!initDone) {
	code = InitThisModule();
	if (code)
	    exit(code);
    }

    if (!uss_DryRun) {
	if (uss_verbose)
	    fprintf(stderr, "Setting options for '%s' in database.\n",
		    username);

	was_spare = pack_long(misc_auth_bytes);

	if (was_spare || flags || expiration || lifetime
	    || (maxAssociates >= 0)) {

	    if (!expiration)
		expiration = uss_Expires;
	    code =
		ubik_Call(KAM_SetFields, uconn_kauthP, 0, username, &instance,
			  flags, expiration, lifetime, maxAssociates,
			  was_spare, /* spare */ 0);
	} else
	    fprintf(stderr,
		    "Must specify one of the optional parameters. Continuing...\n");

	if (code) {
	    afs_com_err(uss_whoami, code, "calling KAM_SetFields for %s.%s",
		    username, instance);

	    return (code);
	}
    } /*Not a dry run */
    else
	fprintf(stderr, "\t[Dry run - user '%s' NOT changed.]\n", username);

    return (0);

}				/*uss_kauth_SetFields */
