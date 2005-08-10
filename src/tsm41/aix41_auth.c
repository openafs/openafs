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
    ("$Header: /cvs/openafs/src/tsm41/aix41_auth.c,v 1.10.2.1 2005/05/08 05:51:24 shadow Exp $");

#if defined(AFS_AIX41_ENV)
#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <locale.h>
#include <nl_types.h>
#include <pwd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <errno.h>
#include <usersec.h>

#include <afs/kauth.h>
#include <afs/kautils.h>

struct passwd *afs_getpwnam_int(char *, int);

int
afs_authenticate(char *userName, char *response, int *reenter, char **message)
{
    char *reason, *pword, prompt[256];
    struct passwd *pwd;
    int code, unixauthneeded, password_expires = -1;

    *reenter = 0;
    *message = (char *)0;
    if (response) {
	pword = response;
    } else {
	sprintf(prompt, "Enter AFS password for %s: ", userName);
	pword = getpass(prompt);
	if (strlen(pword) == 0) {
	    printf
		("Unable to read password because zero length passord is illegal\n");
	    *message = (char *)malloc(256);
	    sprintf(*message,
		    "Unable to read password because zero length passord is illegal\n");
	    return AUTH_FAILURE;
	}
    }
#ifdef AFS_AIX51_ENV
    if ((pwd = afs_getpwnam_int(userName, 1)) == NULL) 
#else
    if ((pwd = getpwnam(userName)) == NULL) 
#endif
      {
	*message = (char *)malloc(256);
	sprintf(*message, "getpwnam for user failed\n");
	return AUTH_FAILURE;
    }
    if (code =
	ka_UserAuthenticateGeneral(KA_USERAUTH_VERSION + KA_USERAUTH_DOSETPAG,
				   userName, (char *)0, (char *)0, pword, 0,
				   &password_expires, 0, &reason)) {
	if (code == KANOENT)
	    return AUTH_NOTFOUND;
	*message = (char *)malloc(1024);
	sprintf(*message, "Unable to authenticate to AFS because %s.\n",
		reason);
	return AUTH_FAILURE;
    }
#if defined(AFS_KERBEROS_ENV)
    setup_ticket_file(userName);
#endif
    return AUTH_SUCCESS;
}

int
afs_chpass(char *userName, char *oldPasswd, char *newPasswd, char **message)
{
    return AUTH_SUCCESS;
}

int
afs_passwdexpired(char *userName, char **message)
{
    return AUTH_SUCCESS;
}

int
afs_passwdrestrictions(char *userName, char *newPasswd, char *oldPasswd,
		       char **message)
{
    return AUTH_SUCCESS;
}

int
afs_getgrset(char *userName)
{
    return NULL;
}

struct group *
afs_getgrgid(int id)
{
#ifdef AFS_AIX51_ENV
    static char name[64];
    static char passwd[64];
    static struct group grp;
    struct group *g;
    char *mem = NULL;

    while ((g = getgrent()) != NULL) {
	if (g->gr_gid == id) {
	    strncpy(&name, g->gr_name, sizeof(name));
	    strncpy(&passwd, g->gr_passwd, sizeof(passwd));
	    grp.gr_name = &name;
	    grp.gr_passwd = &passwd;
	    grp.gr_gid = g->gr_gid;
	    grp.gr_mem = &mem;
	    break;
	}
    }
    endgrent();
    if (g)
	return &grp;
#endif
    return NULL;
}

struct group *
afs_getgrnam(char *name)
{
    return NULL;
}

#ifdef AFS_AIX51_ENV
struct passwd *
afs_getpwnam(char *user)
{
  return (NULL);
}

struct passwd *
afs_getpwnam_int(char *user, int ignore)
{
    static char name[64];
    static char passwd[64];
    static char gecos[256];
    static char dir[256];
    static char shell[256];
    static struct passwd pwd;
    struct passwd *p;

    pwd.pw_uid = 4294967294;
    pwd.pw_gid = 4294967294;
    strcpy((char *)&shell, "/bin/false");
    if (!user)
       return &pwd;

    p = getpwnam (user);
    
    if (p) {
      strncpy(&name, p->pw_name, sizeof(name));
      strncpy(&passwd, p->pw_passwd, sizeof(passwd));
      strncpy(&gecos, p->pw_gecos, sizeof(gecos));
      strncpy(&dir, p->pw_dir, sizeof(dir));
      strncpy(&shell, p->pw_shell, sizeof(shell));
    }
    pwd.pw_name = &name;
    pwd.pw_passwd = &passwd;
    pwd.pw_uid = p->pw_uid;
    pwd.pw_gid = p->pw_gid;
    pwd.pw_gecos = &gecos;
    pwd.pw_dir = &dir;
    pwd.pw_shell = &shell;

    if (ignore && (p == NULL))
       return NULL;
    return &pwd;
}
#else
int
afs_getpwnam(int id)
{
    return NULL;
}
#endif

int
afs_getpwuid(char *name)
{
    return NULL;
}

int
afs_initialize(struct secmethod_table *meths)
{
    /*
     * Initialize kauth package here so we don't have to call it
     * each time we call the authenticate routine.      
     */
    ka_Init(0);
    memset(meths, 0, sizeof(struct secmethod_table));
    /*
     * Initialize the exported interface routines. Except the authenticate one
     * the others are currently mainly noops.
     */
    meths->method_chpass = afs_chpass;
    meths->method_authenticate = afs_authenticate;
    meths->method_passwdexpired = afs_passwdexpired;
    meths->method_passwdrestrictions = afs_passwdrestrictions;
    /*
     * These we need to bring in because, for afs users, /etc/security/user's
     * "registry" must non-local (i.e. DCE) since otherwise it assumes it's a
     * local domain and uses valid_crypt(passwd) to validate the afs passwd
     * which, of course, will fail. NULL return from these routine simply
     * means use the local version ones after all.
     */
    meths->method_getgrgid = afs_getgrgid;
    meths->method_getgrset = afs_getgrset;
    meths->method_getgrnam = afs_getgrnam;
    meths->method_getpwnam = afs_getpwnam;
    meths->method_getpwuid = afs_getpwuid;
    return (0);
}

#if defined(AFS_KERBEROS_ENV)

setup_ticket_file(userName)
     char *userName;
{
    extern char *ktc_tkt_string();
    struct passwd *pwd;

    setpwent();			/* open the pwd database */
    pwd = getpwnam(userName);
    if (pwd) {
	if (chown(ktc_tkt_string(), pwd->pw_uid, pwd->pw_gid) < 0)
	    perror("chown: ");
    } else
	perror("getpwnam : ");
    endpwent();			/* close the pwd database */
}
#endif /* AFS_KERBEROS_ENV */

#endif
