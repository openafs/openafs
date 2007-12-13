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

#if defined(AFS_AIX51_ENV)
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

#include <krb5.h>

#include <afs/cellconfig.h>
#include <afs/dirpath.h>
#include <rx/rxkad.h>
#include <afs/auth.h>
#include <afs/ptserver.h>
#include "aix_auth_prototypes.h"

struct afsconf_cell ak_cellconfig; /* General information about the cell */
static char linkedcell[MAXCELLCHARS+1];
static krb5_ccache  _krb425_ccache = NULL;

#define AFSKEY "afs"
#define AFSINST ""

#ifndef ANAME_SZ
#define ANAME_SZ 40
#endif /* ANAME_SZ */
#ifndef REALM_SZ
#define REALM_SZ 40
#endif /* REALM_SZ */
#ifndef SNAME_SZ
#define SNAME_SZ 40
#endif /* SNAME_SZ */
#ifndef INST_SZ
#define INST_SZ 40
#endif /* INST_SZ */

/*
 * Why doesn't AFS provide these prototypes?
 */

extern int pioctl(char *, afs_int32, struct ViceIoctl *, afs_int32);

/*
 * Other prototypes
 */

static krb5_error_code get_credv5(krb5_context context, char *, char *,
				  char *, krb5_creds **);
static int get_user_realm(krb5_context, char *);

#if defined(HAVE_KRB5_PRINC_SIZE) || defined(krb5_princ_size)

#define get_princ_str(c, p, n) krb5_princ_component(c, p, n)->data
#define get_princ_len(c, p, n) krb5_princ_component(c, p, n)->length
#define second_comp(c, p) (krb5_princ_size(c, p) > 1)
#define realm_data(c, p) krb5_princ_realm(c, p)->data
#define realm_len(c, p) krb5_princ_realm(c, p)->length

#elif defined(HAVE_KRB5_PRINCIPAL_GET_COMP_STRING)

#define get_princ_str(c, p, n) krb5_principal_get_comp_string(c, p, n)
#define get_princ_len(c, p, n) strlen(krb5_principal_get_comp_string(c, p, n))
#define second_comp(c, p) (krb5_principal_get_comp_string(c, p, 1) != NULL)
#define realm_data(c, p) krb5_realm_data(krb5_principal_get_realm(c, p))
#define realm_len(c, p) krb5_realm_length(krb5_principal_get_realm(c, p))

#else
#error "Must have either krb5_princ_size or krb5_principal_get_comp_string"
#endif

#if defined(HAVE_KRB5_CREDS_KEYBLOCK)

#define get_cred_keydata(c) c->keyblock.contents
#define get_cred_keylen(c) c->keyblock.length
#define get_creds_enctype(c) c->keyblock.enctype

#elif defined(HAVE_KRB5_CREDS_SESSION)

#define get_cred_keydata(c) c->session.keyvalue.data
#define get_cred_keylen(c) c->session.keyvalue.length
#define get_creds_enctype(c) c->session.keytype

#else
#error "Must have either keyblock or session member of krb5_creds"
#endif

char *afs_realm_of_cell(krb5_context context, struct afsconf_cell *cellconfig, int fallback)
{
    static char krbrlm[REALM_SZ+1];
        char **hrealms = 0;
        krb5_error_code retval;

    if (!cellconfig)
        return 0;

    if (fallback) {
        char * p;
        p = strchr(cellconfig->hostName[0], '.');
        if (p++)
            strcpy(krbrlm, p);
        else
            strcpy(krbrlm, cellconfig->name);
        for (p=krbrlm; *p; p++) {
            if (islower(*p)) 
                *p = toupper(*p);
        }
    } else {
        if (retval = krb5_get_host_realm(context,
                                         cellconfig->hostName[0], &hrealms))
            return 0; 
        if(!hrealms[0]) return 0;
        strcpy(krbrlm, hrealms[0]);

        if (hrealms) krb5_free_host_realm(context, hrealms);
    }
    return krbrlm;
}

int
aklog_authenticate(char *userName, char *response, int *reenter, char **message)
{
    char *reason, *pword, prompt[256];
    struct passwd *pwd;
    int code, unixauthneeded, password_expires = -1;
    int status;
    krb5_context context;

    krb5_init_context(&context);
    *reenter = 0;
    *message = (char *)0;

#if 0
    if ((pwd = getpwnam(userName)) == NULL) {
	*message = (char *)malloc(256);
	sprintf(*message, "getpwnam for user failed\n");
	return AUTH_FAILURE;
    }
#endif

    status = auth_to_cell(context, NULL, NULL);
    if (status) {
	*message = (char *)malloc(1024);
	sprintf(*message, "Unable to obtain AFS tokens: %s.\n",
		afs_error_message(status));
	return AUTH_FAILURE; /* NOTFOUND? */
    }
    
#if 0
    /*
     * Local hack - if the person has a file in their home
     * directory called ".xlog", read that for a list of
     * extra cells to authenticate to
     */
    
    if ((pwd = getpwuid(getuid())) != NULL) {
	struct stat sbuf;
	FILE *f;
	char fcell[100], xlog_path[512];
	
	strcpy(xlog_path, pwd->pw_dir);
	strcat(xlog_path, "/.xlog");
	
	if ((stat(xlog_path, &sbuf) == 0) &&
	    ((f = fopen(xlog_path, "r")) != NULL)) {
	    
	    while (fgets(fcell, 100, f) != NULL) {
		int auth_status;
		
		fcell[strlen(fcell) - 1] = '\0';
		
		auth_status = auth_to_cell(context, fcell, NULL);
		if (status == AKLOG_SUCCESS)
		    status = auth_status;
		else
		    status = AKLOG_SOMETHINGSWRONG;
	    }
	}
    }
#endif
    return AUTH_SUCCESS;
}
	
static krb5_error_code get_credv5(krb5_context context, 
			char *name, char *inst, char *realm,
			krb5_creds **creds)
{
    krb5_creds increds;
    krb5_error_code r;
    static krb5_principal client_principal = 0;

    memset((char *)&increds, 0, sizeof(increds));
    /* instance may be ptr to a null string. Pass null then */
    if ((r = krb5_build_principal(context, &increds.server,
                     strlen(realm), realm,
                     name,
           (inst && strlen(inst)) ? inst : (void *) NULL,
                     (void *) NULL))) {
        return r;
    }

    if (!_krb425_ccache) {
        r = krb5_cc_default(context, &_krb425_ccache);
	if (r)
	    return r;
    }
    if (!client_principal) {
        r = krb5_cc_get_principal(context, _krb425_ccache, &client_principal);
	if (r)
	    return r;
    }

    increds.client = client_principal;
    increds.times.endtime = 0;
	/* Ask for DES since that is what V4 understands */
    get_creds_enctype((&increds)) = ENCTYPE_DES_CBC_CRC;

    r = krb5_get_credentials(context, 0, _krb425_ccache, &increds, creds);

    return r;
}


static int get_user_realm(krb5_context context, char *realm)
{
    static krb5_principal client_principal = 0;
    int i;

    if (!_krb425_ccache)
        krb5_cc_default(context, &_krb425_ccache);
    if (!client_principal)
        krb5_cc_get_principal(context, _krb425_ccache, &client_principal);

    i = realm_len(context, client_principal);
    if (i > REALM_SZ-1) i = REALM_SZ-1;
    strncpy(realm,realm_data(context, client_principal), i);
    realm[i] = 0;

    return 0;
}

int
aklog_chpass(char *userName, char *oldPasswd, char *newPasswd, char **message)
{
    return AUTH_SUCCESS;
}

int
aklog_passwdexpired(char *userName, char **message)
{
    return AUTH_SUCCESS;
}

int
aklog_passwdrestrictions(char *userName, char *newPasswd, char *oldPasswd,
		       char **message)
{
    return AUTH_SUCCESS;
}

char *
aklog_getpasswd(char * userName)
{
    errno = ENOSYS;
    return NULL;
}


static int get_cellconfig(char *cell, struct afsconf_cell *cellconfig, char *local_cell, char *linkedcell)
{
    int status = 0;
    struct afsconf_dir *configdir;

    memset(local_cell, 0, sizeof(local_cell));
    memset((char *)cellconfig, 0, sizeof(*cellconfig));

    if (!(configdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH))) {
	return AFSCONF_NODB;
    }

    if (afsconf_GetLocalCell(configdir, local_cell, MAXCELLCHARS)) {
	return AFSCONF_FAILURE;
    }

    if ((cell == NULL) || (cell[0] == 0))
	cell = local_cell;

	linkedcell[0] = '\0';
    if (afsconf_GetCellInfo(configdir, cell, NULL, cellconfig)) {
	status = AFSCONF_NOTFOUND;
    }
    if (cellconfig->linkedCell) 
	strncpy(linkedcell,cellconfig->linkedCell,MAXCELLCHARS);

    (void) afsconf_Close(configdir);

    return(status);
}

/* 
 * Log to a cell.  If the cell has already been logged to, return without
 * doing anything.  Otherwise, log to it and mark that it has been logged
 * to.
 */
static int auth_to_cell(krb5_context context, char *cell, char *realm)
{
    int status = 0;
    char username[BUFSIZ];	/* To hold client username structure */
    afs_int32 viceId;		/* AFS uid of user */

    char name[ANAME_SZ];	/* Name of afs key */
    char primary_instance[INST_SZ];	/* Instance of afs key */
    char secondary_instance[INST_SZ];	/* Backup instance to try */
    int try_secondary = 0;		/* Flag to indicate if we try second */
    char realm_of_user[REALM_SZ]; /* Kerberos realm of user */
    char realm_of_cell[REALM_SZ]; /* Kerberos realm of cell */
    char local_cell[MAXCELLCHARS+1];
    char cell_to_use[MAXCELLCHARS+1]; /* Cell to authenticate to */
    static char lastcell[MAXCELLCHARS+1] = { 0 };
    static char confname[512] = { 0 };
    krb5_creds *v5cred = NULL;
    struct ktc_principal aserver;
    struct ktc_principal aclient;
    struct ktc_token atoken, btoken;
    int afssetpag = 1;

    memset(name, 0, sizeof(name));
    memset(primary_instance, 0, sizeof(primary_instance));
    memset(secondary_instance, 0, sizeof(secondary_instance));
    memset(realm_of_user, 0, sizeof(realm_of_user));
    memset(realm_of_cell, 0, sizeof(realm_of_cell));

    if (confname[0] == '\0') {
	strncpy(confname, AFSDIR_CLIENT_ETC_DIRPATH, sizeof(confname));
	confname[sizeof(confname) - 2] = '\0';
    }

    /* NULL or empty cell returns information on local cell */
    if ((status = get_cellconfig(cell, &ak_cellconfig,
			 local_cell, linkedcell)))
	return(status);

    strncpy(cell_to_use, ak_cellconfig.name, MAXCELLCHARS);
    cell_to_use[MAXCELLCHARS] = 0;

    /*
     * Find out which realm we're supposed to authenticate to.  If one
     * is not included, use the kerberos realm found in the credentials
     * cache.
     */
    
    if (realm && realm[0]) {
	strcpy(realm_of_cell, realm);
    }
    else {
	char *afs_realm = afs_realm_of_cell(context, &ak_cellconfig, FALSE);
	
	if (!afs_realm) {
	    return AFSCONF_FAILURE;
	}
	
	strcpy(realm_of_cell, afs_realm);
    }
    
    /* We use the afs.<cellname> convention here... 
     *
     * Doug Engert's original code had principals of the form:
     *
     * "afsx/cell@realm"
     *
     * in the KDC, so the name wouldn't conflict with DFS.  Since we're
     * not using DFS, I changed it just to look for the following
     * principals:
     *
     * afs/<cell>@<realm>
     * afs@<realm>
     *
     * Because people are transitioning from afs@realm to afs/cell,
     * we configure things so that if the first one isn't found, we
     * try the second one.  You can select which one you prefer with
     * a configure option.
     */
    
    strcpy(name, AFSKEY);
    
    if (1 || strcasecmp(cell_to_use, realm_of_cell) != 0) {
	strncpy(primary_instance, cell_to_use, sizeof(primary_instance));
	primary_instance[sizeof(primary_instance)-1] = '\0';
	if (strcasecmp(cell_to_use, realm_of_cell) == 0) {
	    try_secondary = 1;
	    secondary_instance[0] = '\0';
	}
    } else {
	primary_instance[0] = '\0';
	try_secondary = 1;
	strncpy(secondary_instance, cell_to_use,
		sizeof(secondary_instance));
	secondary_instance[sizeof(secondary_instance)-1] = '\0';
    }
    
    /* 
     * Extract the session key from the ticket file and hand-frob an
     * afs style authenticator.
     */
    
    /*
     * Try to obtain AFS tickets.  Because there are two valid service
     * names, we will try both, but trying the more specific first.
     *
     *	afs/<cell>@<realm> i.e. allow for single name with "."
     * 	afs@<realm>
     */
    
    status = get_credv5(context, name, primary_instance, realm_of_cell,
			&v5cred);
    
    if ((status == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN || 
	 status == KRB5KRB_ERR_GENERIC) && !realm_of_cell[0]) {
	char *afs_realm = afs_realm_of_cell(context, &ak_cellconfig, TRUE);
	
	if (!afs_realm) {
	    return AFSCONF_FAILURE;
	}
	
	strcpy(realm_of_cell, afs_realm);
	
	if (strcasecmp(cell_to_use, realm_of_cell) == 0) {
	    try_secondary = 1;
	    secondary_instance[0] = '\0';
	}
	
	status = get_credv5(context, name, primary_instance, realm_of_cell,
			    &v5cred);
	
    }
    if (status == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN || 
	status == KRB5KRB_ERR_GENERIC) {
	if (try_secondary)
	    status = get_credv5(context, name, secondary_instance,
				realm_of_cell, &v5cred);
    }
    
    if (status) {
	return status;
    }
    
    strncpy(aserver.name, AFSKEY, MAXKTCNAMELEN - 1);
    strncpy(aserver.instance, AFSINST, MAXKTCNAMELEN - 1);
    strncpy(aserver.cell, cell_to_use, MAXKTCREALMLEN - 1);
    
    /*
     * The default is to use rxkad2b, which means we put in a full
     * V5 ticket.  If the user specifies -524, we talk to the
     * 524 ticket converter.
     */
    
    {
	char *p;
	int len;
	
	len = min(get_princ_len(context, v5cred->client, 0),
		  second_comp(context, v5cred->client) ?
		  MAXKTCNAMELEN - 2 : MAXKTCNAMELEN - 1);
	strncpy(username, get_princ_str(context, v5cred->client, 0), len);
	username[len] = '\0';
	
	if (second_comp(context, v5cred->client)) {
	    strcat(username, ".");
	    p = username + strlen(username);
	    len = min(get_princ_len(context, v5cred->client, 1),
		      MAXKTCNAMELEN - strlen(username) - 1);
	    strncpy(p, get_princ_str(context, v5cred->client, 1), len);
	    p[len] = '\0';
	}
	
	memset(&atoken, 0, sizeof(atoken));
	atoken.kvno = RXKAD_TKT_TYPE_KERBEROS_V5;
	atoken.startTime = v5cred->times.starttime;;
	atoken.endTime = v5cred->times.endtime;
	memcpy(&atoken.sessionKey, get_cred_keydata(v5cred),
	       get_cred_keylen(v5cred));
	atoken.ticketLen = v5cred->ticket.length;
	memcpy(atoken.ticket, v5cred->ticket.data, atoken.ticketLen);
    }
	
    if ((status = get_user_realm(context, realm_of_user))) {
	return KRB5_REALM_UNKNOWN;
    }
    if (strcmp(realm_of_user, realm_of_cell)) {
	strcat(username, "@");
	strcat(username, realm_of_user);
    }
    
    strcpy(lastcell, aserver.cell);
    
    if (!pr_Initialize (0, confname, aserver.cell))
	status = pr_SNameToId (username, &viceId);
    
    /*
     * This is a crock, but it is Transarc's crock, so
     * we have to play along in order to get the
     * functionality.  The way the afs id is stored is
     * as a string in the username field of the token.
     * Contrary to what you may think by looking at
     * the code for tokens, this hack (AFS ID %d) will
     * not work if you change %d to something else.
     */
    
    /* Don't do first-time registration. Handle only the simple case */
    if ((status == 0) && (viceId != ANONYMOUSID))
	sprintf (username, "AFS ID %d", (int) viceId);

    /* Reset the "aclient" structure before we call ktc_SetToken.
     * This structure was first set by the ktc_GetToken call when
     * we were comparing whether identical tokens already existed.
     */
    strncpy(aclient.name, username, MAXKTCNAMELEN - 1);
    strcpy(aclient.instance, "");
    strncpy(aclient.cell, realm_of_user, MAXKTCREALMLEN - 1);
    
#ifndef AFS_AIX51_ENV
    /* on AIX 4.1.4 with AFS 3.4a+ if a write is not done before 
     * this routine, it will not add the token. It is not clear what 
     * is going on here! So we will do the following operation.
     * On AIX 5 this kills our parent. So we won't.
     */
    write(2,"",0); /* dummy write */
#endif
    status = ktc_SetToken(&aserver, &atoken, &aclient, afssetpag);

    return(status);
}

int
aklog_initialize(struct secmethod_table *meths)
{
    memset(meths, 0, sizeof(struct secmethod_table));

    /*
     * Initialize the exported interface routines.
     * Aside from method_authenticate, these are just no-ops.
     */
    meths->method_chpass = aklog_chpass;
    meths->method_authenticate = aklog_authenticate;
    meths->method_passwdexpired = aklog_passwdexpired;
    meths->method_passwdrestrictions = aklog_passwdrestrictions;
    meths->method_getpasswd = aklog_getpasswd;

    return (0);
}
#endif /* AFS_AIX51_ENV */
