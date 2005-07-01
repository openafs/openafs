/* 
 *  Copyright (C) 1989,2004 by the Massachusetts Institute of Technology
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <afs/stds.h>
#include <krb.h>
#include <krb5.h>
#include <afs/ptserver.h>
#include <afs/ptuser.h>

#ifdef WIN32
#include <windows.h>

#include <cm_config.h>
#include <auth.h>
#include <cellconfig.h>
#include <pioctl_nt.h>
#include <smb_iocons.h>

#define stat _stat
#define lstat stat
#define __S_ISTYPE(mode, mask) (((mode) & _S_IFMT) == (mask))
#define S_ISDIR(mode)          __S_ISTYPE((mode), _S_IFDIR)

#define DONT_HAVE_GET_AD_TKT
#define MAXSYMLINKS 255

/* Win32 uses get_krb_err_txt_entry(status) instead of krb_err_txt[status],
* so we use a bit of indirection like the GNU CVS sources.
*/
#define krb_err_text(status) get_krb_err_txt_entry(status)

#define DRIVECOLON ':'		/* Drive letter separator */
#define BDIR '\\'		/* Other character that divides directories */

static int 
readlink(char *path, char *buf, int buffers)
{
	return -1;
}

char * getcwd(char*, size_t);

static long 
get_cellconfig_callback(void *cellconfig, struct sockaddr_in *addrp, char *namep)
{
	struct afsconf_cell *cc = (struct afsconf_cell *) cellconfig;

	cc->hostAddr[cc->numServers] = *addrp;
	strcpy(cc->hostName[cc->numServers], namep);
	cc->numServers++;
	return 0;
}

#else /* WIN32 */
#include <sys/param.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <afs/param.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/vice.h>
#include <afs/venus.h>
#include <afs/ptserver.h>

#define krb_err_text(status) krb_err_txt[status]

/* Cheesy test for determining AFS 3.5. */
#ifndef AFSCONF_CLIENTNAME
#define AFS35
#endif

#ifdef AFS35
#include <afs/dirpath.h>
#else
#define AFSDIR_CLIENT_ETC_DIRPATH AFSCONF_CLIENTNAME
#endif

#endif /* WIN32 */

#include "linked_list.h"

#define AFSKEY "afs"
#define AFSINST ""

#define AKLOG_SUCCESS 0
#define AKLOG_USAGE 1
#define AKLOG_SOMETHINGSWRONG 2
#define AKLOG_AFS 3
#define AKLOG_KERBEROS 4
#define AKLOG_TOKEN 5
#define AKLOG_BADPATH 6
#define AKLOG_MISC 7

#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef MAXSYMLINKS
#define MAXSYMLINKS 15
#endif

#define DIR '/'			/* Character that divides directories */
#define DIRSTRING "/"		/* String form of above */
#define VOLMARKER ':'		/* Character separating cellname from mntpt */
#define VOLMARKERSTRING ":"	/* String form of above */

typedef struct {
    char cell[BUFSIZ];
    char realm[REALM_SZ];
} cellinfo_t;


struct afsconf_cell ak_cellconfig; /* General information about the cell */

static char *progname = NULL;	/* Name of this program */
static int dflag = FALSE;	/* Give debugging information */
static int noprdb = FALSE;	/* Skip resolving name to id? */
static int force = FALSE;	/* Bash identical tokens? */
static linked_list authedcells;	/* List of cells already logged to */

static int usev5 = TRUE;   /* use kerberos 5? */
static int use524 = FALSE;  /* use krb524? */
static krb5_ccache _krb425_ccache;

long GetLocalCell(struct afsconf_dir **pconfigdir, char *local_cell)
{
    if (!(*pconfigdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH)))
    {
        fprintf(stderr, "%s: can't get afs configuration (afsconf_Open(%s))\n",
                 progname, AFSDIR_CLIENT_ETC_DIRPATH);
        exit(AKLOG_AFS);
    }

    return afsconf_GetLocalCell(*pconfigdir, local_cell, MAXCELLCHARS);
}

long GetCellInfo(struct afsconf_dir **pconfigdir, char* cell, 
struct afsconf_cell **pcellconfig)
{
    return afsconf_GetCellInfo(*pconfigdir, cell, NULL, *pcellconfig);
}

void CloseConf(struct afsconf_dir **pconfigdir)
{       
    (void) afsconf_Close(*pconfigdir);
}

#define ALLOW_REGISTER 1
void ViceIDToUsername(char *username, char *realm_of_user, char *realm_of_cell,
                      char * cell_to_use, CREDENTIALS *c,
                      int *status, 
                      struct ktc_principal *aclient, struct ktc_principal *aserver, struct ktc_token *atoken)
{
    static char lastcell[MAXCELLCHARS+1] = { 0 };
    static char confname[512] = { 0 };
    long viceId;			/* AFS uid of user */
#ifdef ALLOW_REGISTER
    afs_int32 id;
#endif /* ALLOW_REGISTER */

    if (confname[0] == '\0') {
        strncpy(confname, AFSDIR_CLIENT_ETC_DIRPATH, sizeof(confname));
        confname[sizeof(confname) - 2] = '\0';
    }

    if (dflag)
        printf("About to resolve name %s to id\n", username);

    strcpy(lastcell, aserver->cell);

	if (!pr_Initialize (0, confname, aserver->cell)) {
		char sname[PR_MAXNAMELEN];
		strncpy(sname, username, PR_MAXNAMELEN);
		sname[PR_MAXNAMELEN-1] = '\0';
        *status = pr_SNameToId (sname, &viceId);
	}

    if (dflag)
    {
        if (*status)
            printf("Error %d\n", *status);
        else
            printf("Id %d\n", viceId);
    }       

    /*
     * This is a crock, but it is Transarc's crock, so
     * we have to play along in order to get the
     * functionality.  The way the afs id is stored is
     * as a string in the username field of the token.
     * Contrary to what you may think by looking at
     * the code for tokens, this hack (AFS ID %d) will
     * not work if you change %d to something else.
     */

    /*
     * This code is taken from cklog -- it lets people
     * automatically register with the ptserver in foreign cells
     */

#ifdef ALLOW_REGISTER
    if (*status == 0) {
        if (viceId != ANONYMOUSID) {
#else /* ALLOW_REGISTER */
            if ((*status == 0) && (viceId != ANONYMOUSID))
#endif /* ALLOW_REGISTER */
            {
#ifdef AFS_ID_TO_NAME
                strncpy(username_copy, username, BUFSIZ);
                snprintf (username, BUFSIZ, "%s (AFS ID %d)", username_copy, (int) viceId);
#endif /* AFS_ID_TO_NAME */
            }
#ifdef ALLOW_REGISTER
        } else if (strcmp(realm_of_user, realm_of_cell) != 0) {
            if (dflag) {
                printf("doing first-time registration of %s "
                        "at %s\n", username, cell_to_use);
            }
            id = 0;
            strncpy(aclient->name, username, MAXKTCNAMELEN - 1);
            strcpy(aclient->instance, "");
            strncpy(aclient->cell, c->realm, MAXKTCREALMLEN - 1);
            if ((*status = ktc_SetToken(aserver, atoken, aclient, 0))) {
                printf("%s: unable to obtain tokens for cell %s "
                        "(status: %d).\n", progname, cell_to_use, status);
                *status = AKLOG_TOKEN;
                return ;
            }

            if ((*status = pr_Initialize(1L, confname, aserver->cell))) {
                printf("Error %d\n", status);
                return;
            }

            if ((*status = pr_CreateUser(username, &id))) {
                printf("%s: unable to create remote PTS "
                        "user %s in cell %s (status: %d).\n", progname,
                        username, cell_to_use, *status);
            } else {
                printf("created cross-cell entry for %s at %s\n",
                        username, cell_to_use);
#ifdef AFS_ID_TO_NAME
                strncpy(username_copy, username, BUFSIZ);
                snprintf (username, BUFSIZ, "%s (AFS ID %d)", username_copy, (int) viceId);
#endif /* AFS_ID_TO_NAME */
            }
        }
    }
#endif /* ALLOW_REGISTER */
}

char *LastComponent(char *str)
{
    char *ret = strrchr(str, DIR);

#ifdef WIN32
    if (!ret)
        ret = strrchr(str, BDIR);
#endif
    return ret;
}

int FirstComponent(char *str)
{
    return (int)(
#ifdef WIN32
                strchr(str, BDIR) ||
#endif
                strchr(str, DIR));
}

void CopyPathColon(char *origpath, char *path, char *pathtocheck)
{
#ifdef WIN32
    if (origpath[1] == DRIVECOLON)
    {
        strncpy(pathtocheck, origpath, 2);
        strcpy(path, origpath+2);
    }
    else
#endif
        strcpy(path, origpath);
}

int BeginsWithDir(char *str, int colon)
{
    return (str[0] == DIR) ||
#ifdef WIN32
        ((str[0] == BDIR) || (colon && str[1] == DRIVECOLON));
#else
    FALSE;
#endif
}


/* This is a pretty gross hack.  Linking against the Transarc
* libraries pulls in some rxkad functions which use des.  (I don't
* think they ever get called.)  With Transarc-supplied libraries this
* creates a reliance on the symbol des_pcbc_init() which is only in
* Transarc's DES libraries (it's an exportability symbol-hiding
* thing), which we don't want to use because they don't work with
* MIT's krb4 routines.  So export a des_pcbc_init() symbol here so we
* don't have to link against Transarc's des library.
*/
int des_pcbc_init()
{
    abort();
}

static int get_cred(char *name, char *inst, char *realm, CREDENTIALS *c)
{
    int status;

    status = krb_get_cred(name, inst, realm, c);
    if (status != KSUCCESS)
    {
#ifdef DONT_HAVE_GET_AD_TKT
        KTEXT_ST ticket;
        status = krb_mk_req(&ticket, name, inst, realm, 0);
#else
        status = get_ad_tkt(name, inst, realm, 255);
#endif
        if (status == KSUCCESS)
            status = krb_get_cred(name, inst, realm, c);
    }       

    return (status);
}

static int get_v5cred(krb5_context context, 
                      char *name, char *inst, char *realm, CREDENTIALS *c,
                      krb5_creds **creds)
{
    krb5_creds increds;
    krb5_error_code r;
    static krb5_principal client_principal = 0;

    memset((char *)&increds, 0, sizeof(increds));

    if ((r = krb5_build_principal(context, &increds.server,
                                  strlen(realm), realm,
                                  name,
                                  (inst && strlen(inst)) ? inst : 0,
                                  0))) {
        return((int)r);
    }

    if (!_krb425_ccache)
        krb5_cc_default(context, &_krb425_ccache);
    if (!client_principal)
        krb5_cc_get_principal(context, _krb425_ccache, &client_principal);

    increds.client = client_principal;
    increds.times.endtime = 0;
	/* Ask for DES since that is what V4 understands */
    increds.keyblock.enctype = ENCTYPE_DES_CBC_CRC;

    r = krb5_get_credentials(context, 0, _krb425_ccache, &increds, creds);
    if (r)
        return((int)r);

    /* This requires krb524d to be running with the KDC */
    if (c != NULL)
        r = krb5_524_convert_creds(context, *creds, c);
    return((int)r);
}

/* There is no header for this function.  It is supposed to be private */
int krb_get_admhst(char *h,char *r, int n);

static char *afs_realm_of_cell(struct afsconf_cell *cellconfig)
{
	char krbhst[MAX_HSTNM];
	static char krbrlm[REALM_SZ+1];

	if (!cellconfig)
		return 0;

	strcpy(krbrlm, (char *) krb_realmofhost(cellconfig->hostName[0]));

	if (krb_get_admhst(krbhst, krbrlm, 1) != KSUCCESS)
	{
		char *s = krbrlm;
		char *t = cellconfig->name;
		int c;

		while (c = *t++)
		{
			if (islower(c))
				c = toupper(c);
			*s++ = c;
		}
		*s++ = 0;
	}
	return krbrlm;
}

static char *afs_realm_of_cell5(krb5_context context, struct afsconf_cell *cellconfig)
{
	char ** krbrlms = 0;
	static char krbrlm[REALM_SZ+1];
	krb5_error_code status;

	if (!cellconfig)
		return 0;

	status = krb5_get_host_realm( context, cellconfig->hostName[0], &krbrlms );

	if (status == 0 && krbrlms && krbrlms[0]) {
		strcpy(krbrlm, krbrlms[0]);
    } else {
		strcpy(krbrlm, cellconfig->name);
		strupr(krbrlm);
	}

	if (krbrlms)
		krb5_free_host_realm( context, krbrlms );

	return krbrlm;
}

static char *copy_cellinfo(cellinfo_t *cellinfo)
{
	cellinfo_t *new_cellinfo;

	if (new_cellinfo = (cellinfo_t *)malloc(sizeof(cellinfo_t)))
		memcpy(new_cellinfo, cellinfo, sizeof(cellinfo_t));

	return ((char *)new_cellinfo);
}


static char *copy_string(char *string)
{
	char *new_string;

	if (new_string = (char *)calloc(strlen(string) + 1, sizeof(char)))
		(void) strcpy(new_string, string);

	return (new_string);
}


static int get_cellconfig(char *cell, struct afsconf_cell *cellconfig,
						  char *local_cell)
{
	int status = AKLOG_SUCCESS;
	struct afsconf_dir *configdir = 0;

	memset(local_cell, 0, sizeof(local_cell));
	memset(cellconfig, 0, sizeof(*cellconfig));

	if (GetLocalCell(&configdir, local_cell))
	{
		fprintf(stderr, "%s: can't determine local cell.\n", progname);
		exit(AKLOG_AFS);
	}

	if ((cell == NULL) || (cell[0] == 0))
		cell = local_cell;

	if (GetCellInfo(&configdir, cell, &cellconfig))
	{
		fprintf(stderr, "%s: Can't get information about cell %s.\n",
			progname, cell);
		status = AKLOG_AFS;
	}


	CloseConf(&configdir);

	return(status);
}

static int get_v5_user_realm(krb5_context context,char *realm)
{
    static krb5_principal client_principal = 0;
    int i;

    if (!_krb425_ccache)
        krb5_cc_default(context, &_krb425_ccache);
    if (!client_principal)
        krb5_cc_get_principal(context, _krb425_ccache, &client_principal);

    i = krb5_princ_realm(context, client_principal)->length;
    if (i < REALM_SZ-1) i = REALM_SZ-1;
    strncpy(realm,krb5_princ_realm(context, client_principal)->data,i);
    realm[i] = 0;
    return(KSUCCESS);
}

/*
* Log to a cell.  If the cell has already been logged to, return without
* doing anything.  Otherwise, log to it and mark that it has been logged
* to.  */
static int auth_to_cell(krb5_context context, char *cell, char *realm)
{
    int status = AKLOG_SUCCESS;
    char username[BUFSIZ];	/* To hold client username structure */

    char name[ANAME_SZ];		/* Name of afs key */
    char instance[INST_SZ];	/* Instance of afs key */
    char realm_of_user[REALM_SZ]; /* Kerberos realm of user */
    char realm_of_cell[REALM_SZ]; /* Kerberos realm of cell */
    char local_cell[MAXCELLCHARS+1];
    char cell_to_use[MAXCELLCHARS+1]; /* Cell to authenticate to */

    krb5_creds *v5cred = NULL;
    CREDENTIALS c;
    struct ktc_principal aserver;
    struct ktc_principal aclient;
    struct ktc_token atoken, btoken;


    /* try to avoid an expensive call to get_cellconfig */
    if (cell && ll_string_check(&authedcells, cell))
    {
        if (dflag)
            printf("Already authenticated to %s (or tried to)\n", cell);
        return(AKLOG_SUCCESS);
    }

    memset(name, 0, sizeof(name));
    memset(instance, 0, sizeof(instance));
    memset(realm_of_user, 0, sizeof(realm_of_user));
    memset(realm_of_cell, 0, sizeof(realm_of_cell));

    /* NULL or empty cell returns information on local cell */
    if (status = get_cellconfig(cell, &ak_cellconfig, local_cell))
        return(status);

    strncpy(cell_to_use, ak_cellconfig.name, MAXCELLCHARS);
    cell_to_use[MAXCELLCHARS] = 0;

    if (ll_string_check(&authedcells, cell_to_use))
    {
        if (dflag)
            printf("Already authenticated to %s (or tried to)\n", cell_to_use);
        return(AKLOG_SUCCESS);
    }

    /*
     * Record that we have attempted to log to this cell.  We do this
     * before we try rather than after so that we will not try
     * and fail repeatedly for one cell.
     */
    (void)ll_add_string(&authedcells, cell_to_use);

    if (dflag)
        printf("Authenticating to cell %s.\n", cell_to_use);

    if (realm && realm[0])
        strcpy(realm_of_cell, realm);
    else
        strcpy(realm_of_cell,
                (usev5)? 
                afs_realm_of_cell5(context, &ak_cellconfig) : 
                afs_realm_of_cell(&ak_cellconfig));

    /* We use the afs.<cellname> convention here... */
    strcpy(name, AFSKEY);
    strncpy(instance, cell_to_use, sizeof(instance));
    instance[sizeof(instance)-1] = '\0';

    /*
     * Extract the session key from the ticket file and hand-frob an
     * afs style authenticator.
     */

    if (usev5) 
    { /* using krb5 */
        int retry = 1;

        if ( strchr(name,'.') != NULL ) {
            fprintf(stderr, "%s: Can't support principal names including a dot.\n",
                    progname);
            return(AKLOG_MISC);
        }

      try_v5:
        if (dflag)
            printf("Getting v5 tickets: %s/%s@%s\n", name, instance, realm_of_cell);
        status = get_v5cred(context, name, instance, realm_of_cell, 
                            use524 ? &c : NULL, &v5cred);
        if (status == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN) {
            if (dflag)
                printf("Getting v5 tickets: %s@%s\n", name, realm_of_cell);
            status = get_v5cred(context, name, "", realm_of_cell, 
                                use524 ? &c : NULL, &v5cred);
        }
        if ( status == KRB5KRB_AP_ERR_MSG_TYPE && retry ) {
            retry = 0;
            goto try_v5;
        }       
    }       
    else 
    {
        /*
         * Try to obtain AFS tickets.  Because there are two valid service
         * names, we will try both, but trying the more specific first.
         *
         * 	afs.<cell>@<realm>
         * 	afs@<realm>
         */
        if (dflag)
            printf("Getting tickets: %s.%s@%s\n", name, instance, realm_of_cell);
        status = get_cred(name, instance, realm_of_cell, &c);
        if (status == KDC_PR_UNKNOWN)
        {
            if (dflag)
                printf("Getting tickets: %s@%s\n", name, realm_of_cell);
            status = get_cred(name, "", realm_of_cell, &c);
        }
    } 

    /* TODO: get k5 error text */
    if (status != KSUCCESS)
    {
        if (dflag)
            printf("Kerberos error code returned by get_cred: %d\n", status);
        fprintf(stderr, "%s: Couldn't get %s AFS tickets: %s\n",
                 progname, cell_to_use, 
                 (usev5)?"":
                 krb_err_text(status));
        return(AKLOG_KERBEROS);
    }

    strncpy(aserver.name, AFSKEY, MAXKTCNAMELEN - 1);
    strncpy(aserver.instance, AFSINST, MAXKTCNAMELEN - 1);
    strncpy(aserver.cell, cell_to_use, MAXKTCREALMLEN - 1);

    if (usev5 && !use524) {
        /* This code inserts the entire K5 ticket into the token
         * No need to perform a krb524 translation which is
         * commented out in the code below
         */
        char * p;
        int len;
        
        len = min(v5cred->client->data[0].length,MAXKTCNAMELEN - 1);
        strncpy(username, v5cred->client->data[0].data, len);
        username[len] = '\0';

        if ( v5cred->client->length > 1 ) {
            strcat(username, ".");
            p = username + strlen(username);
            len = min(v5cred->client->data[1].length,MAXKTCNAMELEN - strlen(username) - 1);
            strncpy(p, v5cred->client->data[1].data, len);
            p[len] = '\0';
        }

        memset(&atoken, '\0', sizeof(atoken));
        atoken.kvno = RXKAD_TKT_TYPE_KERBEROS_V5;
        atoken.startTime = v5cred->times.starttime;
        atoken.endTime = v5cred->times.endtime;
        memcpy(&atoken.sessionKey, v5cred->keyblock.contents, v5cred->keyblock.length);
        atoken.ticketLen = v5cred->ticket.length;
        memcpy(atoken.ticket, v5cred->ticket.data, atoken.ticketLen);
    } else {
        strcpy (username, c.pname);
        if (c.pinst[0])
        {
            strcat(username, ".");
            strcat(username, c.pinst);
        }

        atoken.kvno = c.kvno;
        atoken.startTime = c.issue_date;
        /* ticket lifetime is in five-minutes blocks. */
        atoken.endTime = c.issue_date + ((unsigned char)c.lifetime * 5 * 60);

        memcpy(&atoken.sessionKey, c.session, 8);
        atoken.ticketLen = c.ticket_st.length;
        memcpy(atoken.ticket, c.ticket_st.dat, atoken.ticketLen);
    }

    if (!force &&
         !ktc_GetToken(&aserver, &btoken, sizeof(btoken), &aclient) &&
         atoken.kvno == btoken.kvno &&
         atoken.ticketLen == btoken.ticketLen &&
         !memcmp(&atoken.sessionKey, &btoken.sessionKey, sizeof(atoken.sessionKey)) &&
         !memcmp(atoken.ticket, btoken.ticket, atoken.ticketLen))
    {       
        if (dflag)
            printf("Identical tokens already exist; skipping.\n");
        return 0;
    }

    if (noprdb)
    {
        if (dflag)
            printf("Not resolving name %s to id (-noprdb set)\n", username);
    }       
    else    
    {
        if (usev5) {
            if((status = get_v5_user_realm(context, realm_of_user)) != KSUCCESS) {
                fprintf(stderr, "%s: Couldn't determine realm of user: %d\n",
                         progname, status);
                return(AKLOG_KERBEROS);
            }
        } else {
            if ((status = krb_get_tf_realm(TKT_FILE, realm_of_user)) != KSUCCESS)
            {
                fprintf(stderr, "%s: Couldn't determine realm of user: %s)",
                         progname, krb_err_text(status));
                return(AKLOG_KERBEROS);
            }
        }

        /* For Khimaira we want to always append the realm to the name */
        if (1 /* strcmp(realm_of_user, realm_of_cell) */)
        {
            strcat(username, "@");
            strcat(username, realm_of_user);
        }

        ViceIDToUsername(username, realm_of_user, realm_of_cell, cell_to_use, &c, &status, &aclient, &aserver, &atoken);
    }

    if (dflag)
        printf("Set username to %s\n", username);

    /* Reset the "aclient" structure before we call ktc_SetToken.
     * This structure was first set by the ktc_GetToken call when
     * we were comparing whether identical tokens already existed.
     */
    strncpy(aclient.name, username, MAXKTCNAMELEN - 1);
    strcpy(aclient.instance, "");
    
    if (usev5 && !use524) {
        int len = min(v5cred->client->realm.length,MAXKTCNAMELEN - 1);
        strncpy(aclient.cell, v5cred->client->realm.data, len);
        aclient.cell[len] = '\0';
    } else
	strncpy(aclient.cell, c.realm, MAXKTCREALMLEN - 1);

    if (dflag)
        printf("Getting tokens.\n");
    if (status = ktc_SetToken(&aserver, &atoken, &aclient, 0))
    {
        fprintf(stderr,
                 "%s: unable to obtain tokens for cell %s (status: %d).\n",
                 progname, cell_to_use, status);
        status = AKLOG_TOKEN;
    }

    return(status);
}

static int get_afs_mountpoint(char *file, char *mountpoint, int size)
{
    char our_file[MAXPATHLEN + 1];
    char *parent_dir;
    char *last_component;
    struct ViceIoctl vio;
    char cellname[BUFSIZ];

    memset(our_file, 0, sizeof(our_file));
    strcpy(our_file, file);

    if (last_component = LastComponent(our_file))
    {
        *last_component++ = 0;
        parent_dir = our_file;
    }
    else
    {
        last_component = our_file;
        parent_dir = ".";
    }

    memset(cellname, 0, sizeof(cellname));

    vio.in = last_component;
    vio.in_size = strlen(last_component)+1;
    vio.out_size = size;
    vio.out = mountpoint;

    if (!pioctl(parent_dir, VIOC_AFS_STAT_MT_PT, &vio, 0))
    {
        if (strchr(mountpoint, VOLMARKER) == NULL)
        {
            vio.in = file;
            vio.in_size = strlen(file) + 1;
            vio.out_size = sizeof(cellname);
            vio.out = cellname;

            if (!pioctl(file, VIOC_FILE_CELL_NAME, &vio, 1))
            {
                strcat(cellname, VOLMARKERSTRING);
                strcat(cellname, mountpoint + 1);
                memset(mountpoint + 1, 0, size - 1);
                strcpy(mountpoint + 1, cellname);
            }
        }
        return(TRUE);
    }
    else {
        return(FALSE);
    }
}       

/*
* This routine each time it is called returns the next directory
* down a pathname.  It resolves all symbolic links.  The first time
* it is called, it should be called with the name of the path
* to be descended.  After that, it should be called with the arguemnt
* NULL.
*/
static char *next_path(char *origpath)
{
    static char path[MAXPATHLEN + 1];
    static char pathtocheck[MAXPATHLEN + 1];

    int link = FALSE;		/* Is this a symbolic link? */
    char linkbuf[MAXPATHLEN + 1];
    char tmpbuf[MAXPATHLEN + 1];

    static char *last_comp;	/* last component of directory name */
    static char *elast_comp;	/* End of last component */
    char *t;
    int len;

    static int symlinkcount = 0;	/* We can't exceed MAXSYMLINKS */

    /* If we are given something for origpath, we are initializing only. */
    if (origpath)
    {
        memset(path, 0, sizeof(path));
        memset(pathtocheck, 0, sizeof(pathtocheck));
        CopyPathColon(origpath, path, pathtocheck);
        last_comp = path;
        symlinkcount = 0;
        return(NULL);
    }

    /* We were not given origpath; find then next path to check */

    /* If we've gotten all the way through already, return NULL */
    if (last_comp == NULL)
        return(NULL);

    do
    {
        while (BeginsWithDir(last_comp, FALSE))
            strncat(pathtocheck, last_comp++, 1);
        len = (elast_comp = LastComponent(last_comp))
            ? elast_comp - last_comp : strlen(last_comp);
        strncat(pathtocheck, last_comp, len);
        memset(linkbuf, 0, sizeof(linkbuf));
        if (link = (readlink(pathtocheck, linkbuf, sizeof(linkbuf)) > 0))
        {
            if (++symlinkcount > MAXSYMLINKS)
            {
                fprintf(stderr, "%s: %s\n", progname, strerror(ELOOP));
                exit(AKLOG_BADPATH);
            }
            memset(tmpbuf, 0, sizeof(tmpbuf));
            if (elast_comp)
                strcpy(tmpbuf, elast_comp);
            if (BeginsWithDir(linkbuf, FALSE))
            {
                /*
                * If this is a symbolic link to an absolute path,
                * replace what we have by the absolute path.
                */
                memset(path, 0, strlen(path));
                memcpy(path, linkbuf, sizeof(linkbuf));
                strcat(path, tmpbuf);
                last_comp = path;
                elast_comp = NULL;
                memset(pathtocheck, 0, sizeof(pathtocheck));
            }
            else
            {
                /*
                * If this is a symbolic link to a relative path,
                * replace only the last component with the link name.
                */
                strncpy(last_comp, linkbuf, strlen(linkbuf) + 1);
                strcat(path, tmpbuf);
                elast_comp = NULL;
                if (t = LastComponent(pathtocheck))
                {
                    t++;
                    memset(t, 0, strlen(t));
                }
                else
                    memset(pathtocheck, 0, sizeof(pathtocheck));
            }
        }
        else
            last_comp = elast_comp;
    }       
    while(link);

    return(pathtocheck);
}

/*
* This routine descends through a path to a directory, logging to
* every cell it encounters along the way.
*/
static int auth_to_path(krb5_context context, char *path)
{
    int status = AKLOG_SUCCESS;
    int auth_to_cell_status = AKLOG_SUCCESS;

    char *nextpath;
    char pathtocheck[MAXPATHLEN + 1];
    char mountpoint[MAXPATHLEN + 1];

    char *cell;
    char *endofcell;

    /* Initialize */
    if (BeginsWithDir(path, TRUE))
        strcpy(pathtocheck, path);
    else
    {
        if (getcwd(pathtocheck, sizeof(pathtocheck)) == NULL)
        {
            fprintf(stderr, "Unable to find current working directory:\n");
            fprintf(stderr, "%s\n", pathtocheck);
            fprintf(stderr, "Try an absolute pathname.\n");
            exit(AKLOG_BADPATH);
        }
        else
        {
            /* in WIN32, if getcwd returns a root dir (eg: c:\), the returned string
            * will already have a trailing slash ('\'). Otherwise, the string will
            * end in the last directory name */
#ifdef WIN32    
            if(pathtocheck[strlen(pathtocheck) - 1] != BDIR)
#endif  
                strcat(pathtocheck, DIRSTRING);
            strcat(pathtocheck, path);
        }
    }
    next_path(pathtocheck);

    /* Go on to the next level down the path */
    while (nextpath = next_path(NULL))
    {
        strcpy(pathtocheck, nextpath);
        if (dflag)
            printf("Checking directory [%s]\n", pathtocheck);
        /*
        * If this is an afs mountpoint, determine what cell from
        * the mountpoint name which is of the form
        * #cellname:volumename or %cellname:volumename.
        */
        if (get_afs_mountpoint(pathtocheck, mountpoint, sizeof(mountpoint)))
        {
            if(dflag)
                printf("Found mount point [%s]\n", mountpoint);
            /* skip over the '#' or '%' */
            cell = mountpoint + 1;
            if (endofcell = strchr(mountpoint, VOLMARKER))
            {
                *endofcell = '\0';
                if (auth_to_cell_status = auth_to_cell(context, cell, NULL))
                {
                    if (status == AKLOG_SUCCESS)
                        status = auth_to_cell_status;
                    else if (status != auth_to_cell_status)
                        status = AKLOG_SOMETHINGSWRONG;
                }
            }
        }
        else
        {
            struct stat st;

            if (lstat(pathtocheck, &st) < 0)
            {
                /*
                * If we've logged and still can't stat, there's
                * a problem...
                */
                fprintf(stderr, "%s: stat(%s): %s\n", progname,
                         pathtocheck, strerror(errno));
                return(AKLOG_BADPATH);
            }
            else if (!S_ISDIR(st.st_mode))
            {
                /* Allow only directories */
                fprintf(stderr, "%s: %s: %s\n", progname, pathtocheck,
                         strerror(ENOTDIR));
                return(AKLOG_BADPATH);
            }
        }
    }

    return(status);
}

/* Print usage message and exit */
static void usage(void)
{
    fprintf(stderr, "\nUsage: %s %s%s%s%s\n", progname,
             "[-d] [[-cell | -c] cell [-k krb_realm]] ",
             "[[-p | -path] pathname]\n",
             "    [-noprdb] [-force]\n",
             "    [-5 [-m]| -4]\n"
             );
    fprintf(stderr, "    -d gives debugging information.\n");
    fprintf(stderr, "    krb_realm is the kerberos realm of a cell.\n");
    fprintf(stderr, "    pathname is the name of a directory to which ");
    fprintf(stderr, "you wish to authenticate.\n");
    fprintf(stderr, "    -noprdb means don't try to determine AFS ID.\n");
    fprintf(stderr, "    -5 or -4 selects whether to use Kerberos V or Kerberos IV.\n"
                    "       (default is Kerberos V)\n");
    fprintf(stderr, "       -m means use krb524d to convert Kerberos V tickets.\n");
    fprintf(stderr, "    No commandline arguments means ");
    fprintf(stderr, "authenticate to the local cell.\n");
    fprintf(stderr, "\n");
    exit(AKLOG_USAGE);
}

int main(int argc, char *argv[])
{
    int status = AKLOG_SUCCESS;
    int i;
    int somethingswrong = FALSE;

    cellinfo_t cellinfo;

    extern char *progname;	/* Name of this program */

    extern int dflag;		/* Debug mode */

    int cmode = FALSE;		/* Cellname mode */
    int pmode = FALSE;		/* Path name mode */

    char realm[REALM_SZ];		/* Kerberos realm of afs server */
    char cell[BUFSIZ];		/* Cell to which we are authenticating */
    char path[MAXPATHLEN + 1];	/* Path length for path mode */

    linked_list cells;		/* List of cells to log to */
    linked_list paths;		/* List of paths to log to */
    ll_node *cur_node;

    krb5_context context = 0;

    memset(&cellinfo, 0, sizeof(cellinfo));

    memset(realm, 0, sizeof(realm));
    memset(cell, 0, sizeof(cell));
    memset(path, 0, sizeof(path));

    ll_init(&cells);
    ll_init(&paths);

    /* Store the program name here for error messages */
    if (progname = LastComponent(argv[0]))
        progname++;
    else
        progname = argv[0];

    /* Initialize list of cells to which we have authenticated */
    (void)ll_init(&authedcells);

    /* Parse commandline arguments and make list of what to do. */
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-d") == 0)
            dflag++;
        else if (strcmp(argv[i], "-5") == 0)
            usev5++;
        else if (strcmp(argv[i], "-m") == 0)
            use524++;
        else if (strcmp(argv[i], "-4") == 0)
            usev5 = 0;
        else if (strcmp(argv[i], "-noprdb") == 0)
            noprdb++;
        else if (strcmp(argv[i], "-force") == 0)
            force++;
        else if (((strcmp(argv[i], "-cell") == 0) ||
                   (strcmp(argv[i], "-c") == 0)) && !pmode)
        {       
            if (++i < argc)
            {
                cmode++;
                strcpy(cell, argv[i]);
            }
            else
                usage();
        }
        else if (((strcmp(argv[i], "-path") == 0) ||
                   (strcmp(argv[i], "-p") == 0)) && !cmode)
        {       
            if (++i < argc)
            {
                pmode++;
                strcpy(path, argv[i]);
            }
            else
                usage();
        }
        else if (argv[i][0] == '-')
            usage();
        else if (!pmode && !cmode)
        {
            if (FirstComponent(argv[i]) || (strcmp(argv[i], ".") == 0) ||
                 (strcmp(argv[i], "..") == 0))
            {
                pmode++;
                strcpy(path, argv[i]);
            }
            else
            {
                cmode++;
                strcpy(cell, argv[i]);
            }
        }
        else
            usage();

        if (cmode)
        {
            if (((i + 1) < argc) && (strcmp(argv[i + 1], "-k") == 0))
            {
                i += 2;
                if (i < argc)
                    strcpy(realm, argv[i]);
                else
                    usage();
            }
            /* Add this cell to list of cells */
            strcpy(cellinfo.cell, cell);
            strcpy(cellinfo.realm, realm);
            if (cur_node = ll_add_node(&cells, ll_tail))
            {
                char *new_cellinfo;
                if (new_cellinfo = copy_cellinfo(&cellinfo))
                    ll_add_data(cur_node, new_cellinfo);
                else
                {
                    fprintf(stderr, "%s: failure copying cellinfo.\n", progname);
                    exit(AKLOG_MISC);
                }
            }
            else
            {
                fprintf(stderr, "%s: failure adding cell to cells list.\n",
                         progname);
                exit(AKLOG_MISC);
            }
            memset(&cellinfo, 0, sizeof(cellinfo));
            cmode = FALSE;
            memset(cell, 0, sizeof(cell));
            memset(realm, 0, sizeof(realm));
        }
        else if (pmode)
        {
            /* Add this path to list of paths */
            if (cur_node = ll_add_node(&paths, ll_tail))
            {
                char *new_path;
                if (new_path = copy_string(path))
                    ll_add_data(cur_node, new_path);
                else
                {
                    fprintf(stderr, "%s: failure copying path name.\n",
                             progname);
                    exit(AKLOG_MISC);
                }
            }
            else
            {
                fprintf(stderr, "%s: failure adding path to paths list.\n",
                         progname);
                exit(AKLOG_MISC);
            }
            pmode = FALSE;
            memset(path, 0, sizeof(path));
        }
    }

    if(usev5)
        krb5_init_context(&context);

    /* If nothing was given, log to the local cell. */
    if ((cells.nelements + paths.nelements) == 0)
        status = auth_to_cell(context, NULL, NULL);
    else
    {
        /* Log to all cells in the cells list first */
        for (cur_node = cells.first; cur_node; cur_node = cur_node->next)
        {
            memcpy(&cellinfo, cur_node->data, sizeof(cellinfo));
            if (status = auth_to_cell(context, 
                                       cellinfo.cell, cellinfo.realm))
                somethingswrong++;
        }       

        /* Then, log to all paths in the paths list */
        for (cur_node = paths.first; cur_node; cur_node = cur_node->next)
        {
            if (status = auth_to_path(context, 
                                       cur_node->data))
                somethingswrong++;
        }

        /*
        * If only one thing was logged to, we'll return the status
        * of the single call.  Otherwise, we'll return a generic
        * something failed status.
        */
        if (somethingswrong && ((cells.nelements + paths.nelements) > 1))
            status = AKLOG_SOMETHINGSWRONG;
    }       

    if(usev5)
        krb5_free_context(context);

    exit(status);
}       
