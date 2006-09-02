/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* AFS SIA mechanism library. 
 *
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/sia/Attic/siad.c,v 1.5 2003/07/15 23:16:51 shadow Exp $");

#include <afs/stds.h>
#include <sys/types.h>
#include <rx/xdr.h>
#include <lock.h>

#include <stdio.h>
#include <pwd.h>
#include <afs/com_err.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/cmd.h>
#include <afs/kautils.h>

#include <sia.h>
#include <siad.h>
#include <stdarg.h>


/* afs_sia_log logs to the standard sialog. */
static void
afs_sia_log(char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    sia_log("AFS", fmt, args);
    va_end(args);
}

#if defined(AFS_KERBEROS_ENV)
extern char *ktc_tkt_string();
#endif

/* afs_siad_debug gives more detailed debugging information for AFS 
 * than I want to put into the regular sialog.
 */
#include <sys/stat.h>
#include <time.h>
#define DEBUG_FILE "/var/adm/afssialog"
/* Modify VERS to ensure you're testing with the current libafssiad.so.
 * To make SIA recognize a new library, touch /etc/sia/matrix.conf.
 */
#define VERS "AFS3"
static void
afs_siad_debug(char *fmt, ...)
{
    struct stat sbuf;
    FILE *fp;
    time_t now;
    char *when;

    va_list args;

    /* Only print if file exists. */
    if (stat(DEBUG_FILE, &sbuf) < 0)
	return;

    if ((fp = fopen(DEBUG_FILE, "a")) == NULL)
	return;

    (void)time(&now);
    when = ctime(&now);
    when[24] = '\0';

    fprintf(fp, "%s %s: ", VERS, when);
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);

    fflush(fp);
    fclose(fp);

}

/* siad_init - Once per reboot processing goes here. */
int
siad_init(void)
{
    return SIADSUCCESS;
}

/* malloc any needed space required over the authentication session here. */
int
siad_ses_init(SIAENTITY * entity, int pkgind)
{
    return SIADSUCCESS;
}

/* We set the pwd entry in siad_ses_authent if we succeed in authenticating. 
 * Otherwise the BSD mechanism will incur a core dump.
 */
int
siad_ses_estab(sia_collect_func_t * collect, SIAENTITY * entity, int pkgind)
{
    return SIASUCCESS;
}

int
siad_ses_launch(sia_collect_func_t * collect, SIAENTITY * entity, int pkgind)
{
    return SIADSUCCESS;
}

/* Free up space malloc'd in siad_ses_init() */
int
siad_ses_release(SIAENTITY * entity, int pkgind)
{
    return SIADSUCCESS;
}

int
siad_get_groups(struct sia_context *context, const char *username,
		gid_t * buf, int *numgroups, int maxgroups)
{
    afs_siad_debug("siad_get_groups returning failure.\n");
    return SIADFAIL;
}

/* Print the reason we failed to authenticate. */
static void
afs_siad_authent_print_reason(sia_collect_func_t * collect, char *reason)
{
    unsigned char err_msg[128];

    if (collect) {
	(void)sprintf(err_msg, "Unable to authenticate to AFS because %s",
		      reason);
	sia_warning(collect, err_msg);
    }
}

/* afs_siad_get_name_password
 *
 * Common code for siad_ses_authent and siad_ses_reauthent. Gather name and 
 * password if required.
 *
 * Arguments:
 * collect - prompt collection function.
 * entity - SIA entity
 * got_pass - set to 1 if we gather'd the password ourselves.
 *
 *
 * Return value:
 * SIADFAIL - failed to malloc, calling routine should return SIADFAIL.
 * SIADSUCESS - name and password have been collected (maybe not by us).
 * SIADFAIL | SIADSTOP - calling routine should return.
 */
int
afs_siad_get_name_password(sia_collect_func_t * collect, SIAENTITY * entity,
			   int *got_pass)
{
    int need_name = 0;
    int need_pass = 0;
    int code = 0;
    struct prompt_t prompts[2];
    int n_prompts = 0;

    *got_pass = 0;

    if ((!entity->name) || (!(*entity->name))) {
	entity->name = malloc(SIANAMEMIN + 1);
	if (entity->name == NULL) {
	    afs_siad_debug
		("afs_siad_get_name_password: failed to malloc name.\n");
	    code = SIADFAIL;
	    goto fail_free;
	}
	*(entity->name) = '\0';
	need_name = 1;
    }
    if ((!entity->password) || (!(*entity->password))) {
	entity->password = malloc(SIAMXPASSWORD + 1);
	if (entity->password == NULL) {
	    afs_siad_debug
		("afs_siad_get_name_password: failed to malloc password.\n");
	    code = SIADFAIL;
	    goto fail_free;
	}
	*(entity->password) = '\0';
	need_pass = 1;
    }

    if (need_name || need_pass) {
	if (!collect || !entity->colinput) {
	    code = SIADFAIL;
	    goto fail_free;
	}
	if (need_name) {
	    prompts[0].prompt = (unsigned char *)"login: ";
	    prompts[0].result = (unsigned char *)entity->name;
	    prompts[0].min_result_length = 1;
	    prompts[0].max_result_length = SIANAMEMIN;
	    prompts[0].control_flags = SIAPRINTABLE;
	    n_prompts = 1;
	}
	if (need_pass) {
	    prompts[n_prompts].prompt = (unsigned char *)"Password:";
	    prompts[n_prompts].result = (unsigned char *)entity->password;
	    prompts[n_prompts].min_result_length = 0;
	    prompts[n_prompts].max_result_length = SIAMXPASSWORD;
	    prompts[n_prompts].control_flags = SIARESINVIS;
	    n_prompts++;
	}
	if (n_prompts > 1)
	    code =
		(*collect) (0, SIAFORM, (uchar_t *) "", n_prompts, prompts);
	else
	    code = (*collect) (240, SIAONELINER, (uchar_t *) "", 1, prompts);
	if (code != SIACOLSUCCESS) {
	    code = SIADFAIL | SIADSTOP;
	    goto fail_free;
	}
    }
    *got_pass = need_pass;
    return SIADSUCCESS;

  fail_free:
    if (need_name) {
	free(entity->name);
	entity->name = (char *)0;
    }
    if (need_pass) {
	free(entity->password);
	entity->password = (char *)0;
    }
    return code;
}

/* siad_ses_authent
 *
 * Authenticate user for AFS.
 *
 * Rules on when to authenticate, from the AFS SysAdmin Guide:
 * 1) If no entry in password file, try the authentication.
 * 2) If '*' in password file, don't attempt to authenticate.
 * NOTE: If enhanced security is turned on, '*' means to check the data base
 * for the encrypted password.
 * 3) If passwd field is not 13 characters long, try AFS authentication.
 * 4) If passwd field is 13 characters, try to authenticate.
 * Comes down to:
 * 1) Don't try to authenticate if '*' in password field.
 * 2) Use "Entry AFS Password" if password field is not 13 charaters long.
 *    This really isn't possible if the CDE login is being used since it
 *    prints it's own prompts. 
 *
 * This is an integrated login environement. So I do not print any AFS
 * specific login messages.
 *
 * entityhdl->colinput == 1 means the collect function can be used to prompt
 * for input. If it's 0, then it can only be used to print messages.
 * For this case, one also has to test for a non-null collect function.
 *
 * DCE, AFS, BSD is the proper order to do the authentication. Generally
 * speaking AFS should come just before BSD which is last. The reason is that
 * if some other mechanism succeeds in authenticating it will probably want to
 * set the entity->pwd field to something other than /etc/passwd. 
 */
int
siad_ses_authent(sia_collect_func_t * collect, SIAENTITY * entity,
		 int siastat, int pkgind)
{
    int got_pass = 0;
    int code = 0;
    char *reason;		/* returned by authenticate. */
    int password_expires = -1;
    struct passwd *pwd = (struct passwd *)0;
    extern struct passwd *getpwnam();

    code = afs_siad_get_name_password(collect, entity, &got_pass);
    if (code != SIADSUCCESS)
	return code;

    pwd = getpwnam(entity->name);
    if (!pwd) {
	/* Only authenticate if user is in /etc/passwd. */
	code = SIADFAIL;
	goto authent_fail;
    }
    if ((pwd->pw_passwd[0] == '*') && (pwd->pw_passwd[1] == '\0')) {
	afs_siad_debug("siad_ses_authent: refusing to authenticate\n");
	code = SIADFAIL;
	goto authent_fail;
    }

    code = ka_UserAuthenticateGeneral(KA_USERAUTH_VERSION | KA_USERAUTH_DOSETPAG, entity->name, (char *)0,	/* instance */
				      (char *)0,	/* realm */
				      entity->password, 0,	/* lifetime */
				      &password_expires, 0 /* spare2 */ ,
				      &reason);

    if (code) {
	afs_siad_debug("siad_sis_authent: auth1 failure: %s\n", reason);
    }

    if (code) {
	code = SIADFAIL;
	goto authent_fail;
    }

    if (!entity->pwd) {
	entity->pwd = (struct passwd *)malloc(sizeof(struct passwd));
	if (!entity->pwd) {
	    code = SIADFAIL;
	    goto authent_fail;
	}
	memset((void *)entity->pwd, '\0', sizeof(struct passwd));
	if (sia_make_entity_pwd(pwd, entity) != SIASUCCESS) {
	    afs_siad_debug("siad_ses_authent: Can't set pwd into entity.\n");
	    code = SIADFAIL;
	    goto authent_fail;
	}
    }

    /* Set PASSWORD_EXPIRES env variable if necessary */
    if (password_expires >= 0 && password_expires < 255) {
	char sbuffer[10];
	sprintf(sbuffer, "%d", password_expires);
	(void)setenv("PASSWORD_EXPIRES", sbuffer, 1);
    }
#if defined(AFS_KERBEROS_ENV)
    if (pwd) {
	if (chown(ktc_tkt_string(), pwd->pw_uid, pwd->pw_gid) < 0)
	    afs_siad_debug("siad_ses_authent fails - krb chown.\n");
    }
#endif

    afs_siad_debug("siad_ses_authent returning success.\n");
    afs_sia_log("siad_ses_authent returning success.\n");
    return SIADSUCCESS;

  authent_fail:
    afs_sia_log("siad_ses_authent fails, code=%d.\n", code);
    afs_siad_debug("siad_ses_authent fails, code=%d.\n", code);
    return code;

}


/* siad_ses_reauthent.
 * Used for such things as as locking/unlocking terminal. This implies
 * authenticate, but do not set a pag. The oher differences is that we
 * accept vouching from other mechanism.
 *
 * Note the dtsession collects the password itself and will always pass it
 * in. Also, colinput is typically false in this case as well as collect
 * being null.
 */
int
siad_ses_reauthent(sia_collect_func_t * collect, SIAENTITY * entity,
		   int siastat, int pkgind)
{
    int got_pass = 0;
    int code = 0;
    char *reason;		/* returned by authenticate. */
    struct passwd *pwd = (struct passwd *)0;
    extern struct passwd *getpwnam();

    if (siastat == SIADSUCCESS)
	return;

    code = afs_siad_get_name_password(collect, entity, &got_pass);
    if (code != SIADSUCCESS)
	return code;

    pwd = getpwnam(entity->name);
    if (!pwd) {
	code = SIADFAIL;
	goto reauthent_fail;
    }

    code = ka_VerifyUserPassword(KA_USERAUTH_VERSION, entity->name, (char *)0,	/* instance */
				 (char *)0,	/* realm */
				 entity->password, 0 /* spare2 */ ,
				 &reason);

    if (code) {
	afs_siad_debug("siad_sis_reauthent: auth failure: %s\n", reason);
    }

    if (code) {
	code = SIADFAIL;
	goto reauthent_fail;
    }

    if (!entity->pwd) {
	entity->pwd = (struct passwd *)malloc(sizeof(struct passwd));
	if (!entity->pwd) {
	    code = SIADFAIL;
	    goto reauthent_fail;
	}
	memset((void *)entity->pwd, '\0', sizeof(struct passwd));
	if (sia_make_entity_pwd(pwd, entity) != SIASUCCESS) {
	    afs_siad_debug
		("siad_ses_reauthent: Can't set pwd into entity.\n");
	    code = SIADFAIL;
	    goto reauthent_fail;
	}
    }

    afs_siad_debug("siad_ses_reauthent returning success.\n");
    afs_sia_log("siad_ses_reauthent returning success.\n");
    return SIADSUCCESS;

  reauthent_fail:
    afs_sia_log("siad_ses_reauthent fails, code=%d.\n", code);
    afs_siad_debug("siad_ses_reauthent fails, code=%d.\n", code);
    return code;
}

int
siad_chk_invoker(void)
{
    afs_siad_debug("siad_chk_invoker returning failure.\n");
    return SIADFAIL;
}

int
siad_ses_suauthent(sia_collect_func_t * collect, SIAENTITY * entity,
		   int siastat, int pkgind)
{
    afs_siad_debug("siad_ses_suauthent returning failure.\n");
    return SIADFAIL;
}


int
siad_chg_finger(sia_collect_func_t * collect, const char *username, int argc,
		char *argv[])
{
    afs_siad_debug("siad_chg_finger returning failure.\n");
    return SIADFAIL;
}

int
siad_chg_password(sia_collect_func_t * collect, const char *username,
		  int argc, char *argv[])
{
    afs_siad_debug("siad_chg_passwd returning failure.\n");
    return SIADFAIL;
}

int
siad_chg_shell(sia_collect_func_t * collect, const char *username, int argc,
	       char *argv[])
{
    afs_siad_debug("siad_chg_shell returning failure.\n");
    return SIADFAIL;
}

int
siad_getpwent(struct passwd *result, char *buf, int bufsize,
	      struct sia_context *context)
{
    afs_siad_debug("siad_getpwent returning failure.\n");
    return SIADFAIL;
}

int
siad_getpwuid(uid_t uid, struct passwd *result, char *buf, int bufsize,
	      struct sia_context *context)
{

    afs_siad_debug("siad_getpwuid returning failure.\n");
    return SIADFAIL;
}

int
siad_getpwnam(const char *name, struct passwd *result, char *buf, int bufsize,
	      struct sia_context *context)
{
    afs_siad_debug("siad_ses_getpwnam returning failure.\n");
    return SIADFAIL;
}

int
siad_setpwent(struct sia_context *context)
{
    afs_siad_debug("siad_ses_setpwent returning failure.\n");
    return SIADFAIL;
}

int
siad_endpwent(struct sia_context *context)
{
    afs_siad_debug("siad_ses_endpwent returning failure.\n");
    return SIADFAIL;
}

int
siad_getgrent(struct group *result, char *buf, int bufsize,
	      struct sia_context *context)
{
    afs_siad_debug("siad_ses_getgrent returning failure.\n");
    return SIADFAIL;
}

int
siad_getgrgid(gid_t gid, struct group *result, char *buf, int bufsize,
	      struct sia_context *context)
{
    afs_siad_debug("siad_ses_getgrgid returning failure.\n");
    return SIADFAIL;
}

int
siad_getgrnam(const char *name, struct group *result, char *buf, int bufsize,
	      struct sia_context *context)
{
    afs_siad_debug("siad_ses_getgrnam returning failure.\n");
    return SIADFAIL;
}

int
siad_setgrent(struct sia_context *context)
{
    afs_siad_debug("siad_ses_setgrent returning failure.\n");
    return SIADFAIL;
}

int
siad_endgrent(struct sia_context *context)
{
    afs_siad_debug("siad_ses_endgrent returning failure.\n");
    return SIADFAIL;
}

int
siad_chk_user(const char *logname, int checkflag)
{
    afs_siad_debug("siad_ses_chk_user returning success.\n");
    return SIADFAIL;
}


#ifdef notdef
/* These are not in the current implementation. */
void
siad_ses_toggle_privs(SIAENTITY * entity, int pkgind, int elevate)
{
    afs_siad_debug("siad_ses_toggle_privs.\n");
    return;
}


void
siad_ses_update_audit_record(SIAENTITY * entity, int pkgind, int event,
			     char *tokenp, char **datap, int *used,
			     int maxused)
{
    afs_siad_debug("siad_ses_update_audit_record.\n");
    return;
}
#endif
