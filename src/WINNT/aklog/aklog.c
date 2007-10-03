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

#ifndef _WIN64
#define HAVE_KRB4
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <afs/stds.h>
#ifdef HAVE_KRB4
#include <krb.h>
#else
#define REALM_SZ 64
#define ANAME_SZ 64
#define INST_SZ  64
#define KSUCCESS 0

#define CREDENTIALS void
#endif
#include <krb5.h>
#include <afs/ptserver.h>
#include <afs/ptuser.h>

#ifdef WIN32
#include <windows.h>

#include <cm_config.h>
#include <cellconfig.h>
#ifdef AFS_RXK5
#include <afs/rxk5_utilafs.h>
#endif
#include <auth.h>
#include <pioctl_nt.h>
#include <smb_iocons.h>
#include <afs/afskfw.h>

#define stat _stat
#define lstat stat
#define __S_ISTYPE(mode, mask) (((mode) & _S_IFMT) == (mask))
#define S_ISDIR(mode)          __S_ISTYPE((mode), _S_IFDIR)

#define DONT_HAVE_GET_AD_TKT
#define MAXSYMLINKS 255

#if !defined(USING_HEIMDAL)
#define get_cred_keydata(c) c->keyblock.contents
#define get_cred_keylen(c) c->keyblock.length
#define get_creds_enctype(c) c->keyblock.enctype

#define get_princ_str(c, p, n) krb5_princ_component(c, p, n)->data
#define get_princ_len(c, p, n) krb5_princ_component(c, p, n)->length
#define second_comp(c, p) (krb5_princ_size(c, p) > 1)
#define realm_data(c, p) krb5_princ_realm(c, p)->data
#define realm_len(c, p) krb5_princ_realm(c, p)->length

#endif

#ifdef HAVE_KRB4
/* Win32 uses get_krb_err_txt_entry(status) instead of krb_err_txt[status],
* so we use a bit of indirection like the GNU CVS sources.
*/
#define krb_err_text(status) get_krb_err_txt_entry(status)
#endif

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
#define AFS_K5_KEY "afs-k5"
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

#ifdef AFS_RXK5
int max_enc;			/* # of kernel enc types */
krb5_enctype enctypes_pref_order[20];	/* list of kernel enctypes */
#endif	/* AFS_RXK5 */

static int usev5 = TRUE;   /* use kerberos 5? */
#ifdef HAVE_KRB4
static int use524 = FALSE;  /* use krb524? */
#endif
#ifdef AFS_RXK5
static int rxk5;		/* Use rxk5 enctype selection and settoken behavior */
#endif
static krb5_ccache aklog_ccache;

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
                      struct ktc_principal *aclient, 
					  struct ktc_principal *aserver,
					  afs_int32 *viceId,			/* AFS uid of user */  
					  struct ktc_token *atoken)
{
    static char lastcell[MAXCELLCHARS+1] = { 0 };
    static char confname[512] = { 0 };
    char username_copy[BUFSIZ];
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
        char sname[PR_MAXNAMELEN], *at;

        strncpy(sname, username, PR_MAXNAMELEN);
        sname[PR_MAXNAMELEN-1] = '\0';

	at = strchr(sname, '@');
	if (at && !stricmp(at+1, realm_of_cell))
	    *at = '\0';
	*status = pr_SNameToId (sname, viceId);
    }

    if (dflag)
    {
        if (*status)
            printf("Error %d\n", *status);
        else
            printf("Id %d\n", *viceId);
    }       

    /*
     * This code is taken from cklog -- it lets people
     * automatically register with the ptserver in foreign cells
     */

#ifdef ALLOW_REGISTER
    if (*status == 0) {
        if (*viceId != ANONYMOUSID) {
#else /* ALLOW_REGISTER */
            if ((*status == 0) && (*viceId != ANONYMOUSID))
#endif /* ALLOW_REGISTER */
            {
#ifdef AFS_ID_TO_NAME
                strncpy(username_copy, username, BUFSIZ);
                snprintf (username, BUFSIZ, "%s (AFS ID %d)", username_copy, (int) *viceId);
#endif /* AFS_ID_TO_NAME */
            }
#ifdef ALLOW_REGISTER
        } else if (strcmp(realm_of_user, realm_of_cell) != 0) {
            int i;
            if (dflag) {
                printf("doing first-time registration of %s "
                        "at %s\n", username, cell_to_use);
            }
            id = 0;
            strncpy(aclient->name, username, MAXKTCNAMELEN - 1);
            aclient->name[MAXKTCNAMELEN - 1] = '\0';
            strcpy(aclient->instance, "");
            strncpy(aclient->cell, cell_to_use, MAXKTCREALMLEN - 1);
            aclient->cell[MAXKTCREALMLEN - 1] = '\0';

            for ( i=0; aclient->cell[i]; i++ ) {
                if ( islower(aclient->cell[i]) )
                    aclient->cell[i] = toupper(aclient->cell[i]);
            }

            if ((*status = ktc_SetToken(aserver, atoken, aclient, 0))) {
                printf("%s: unable to set tokens for cell %s "
                        "(status: %d).\n", progname, cell_to_use, *status);
                *status = AKLOG_TOKEN;
                return ;
            }

            /*
             * In case you're wondering, we don't need to change the
             * filename here because we're still connecting to the
             * same cell -- we're just using a different authentication
             * level
             */

            if ((*status = pr_Initialize(1L, confname, aserver->cell))) {
                printf("Error %d\n", *status);
                return;
            }

            /* copy the name because pr_CreateUser lowercases the realm */
            strncpy(username_copy, username, BUFSIZ);

            *status = pr_CreateUser(username, &id);

            /* and restore the name to the original state */
            strncpy(username, username_copy, BUFSIZ);

            if (*status) {
                printf("%s: unable to create remote PTS "
                        "user %s in cell %s (status: %d).\n", progname,
                        username, cell_to_use, *status);
            } else {
                printf("created cross-cell entry for %s (Id %d) at %s\n",
                        username, viceId, cell_to_use);
#ifdef AFS_ID_TO_NAME
                snprintf (username, BUFSIZ, "%s (AFS ID %d)", username_copy, (int) *viceId);
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

#ifdef HAVE_KRB4
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
#endif

static int get_v5cred(krb5_context context, 
                      char *name, char *inst, char *realm, CREDENTIALS *c,
                      krb5_creds **creds)
{
    krb5_creds increds;
    krb5_error_code r;
    static krb5_principal client_principal = 0;

    memset((char *)&increds, 0, sizeof(increds));

    if ((r = krb5_build_principal(context, &increds.server,
                                  (int)strlen(realm), realm,
                                  name,
                                  (inst && strlen(inst)) ? inst : 0,
                                  0))) {
        return((int)r);
    }

    if (!aklog_ccache)
        krb5_cc_default(context, &aklog_ccache);
    if (!client_principal)
        krb5_cc_get_principal(context, aklog_ccache, &client_principal);

    increds.client = client_principal;
    increds.times.endtime = 0;
	
#ifdef AFS_RXK5
    if(rxk5) {
    	/* Get the strongest credentials this KDC can issue for the princ, and the
	   cache manager supports */
	   int enc_ix;
	   r = KTC_ERROR;
	   for(enc_ix = 0; enc_ix < max_enc; ++enc_ix) {
           get_creds_enctype((&increds)) = enctypes_pref_order[enc_ix];
	       r = krb5_get_credentials(context, 0, aklog_ccache, &increds, creds);
	       if(!r) {
		      if(dflag) {
		      printf("Successful get_creds_enctype with enctype == %d\n",
			     enctypes_pref_order[enc_ix]);
		  }
		  break;
	    }
	}

    } else {
#endif	/* AFS_RXK5 */
		/* Ask for DES since that is what V4 understands */
    	increds.keyblock.enctype = ENCTYPE_DES_CBC_CRC;

    	r = krb5_get_credentials(context, 0, aklog_ccache, &increds, creds);
    	if (r)
        	return((int)r);

    	/* This requires krb524d to be running with the KDC */
    	if (c != NULL)
        	r = krb5_524_convert_creds(context, *creds, c);
#ifdef AFS_RXK5
    }
#endif	/* AFS_RXK5 */
		
    return((int)r);
}

static krb5_error_code get_credv5(krb5_context context, 
			char *name, krb5_creds **creds)
{
    krb5_creds increds;
    krb5_error_code r;
    static krb5_principal client_principal = 0;

    memset((char *)&increds, 0, sizeof(increds));
    if ((r = krb5_parse_name(context, name, &increds.server))) {
	goto Done;
    }

    if (!aklog_ccache) {
        r = krb5_cc_default(context, &aklog_ccache);
	if (r)
	    goto Done;
    }
    if (!client_principal) {
        r = krb5_cc_get_principal(context, aklog_ccache, &client_principal);
	if (r)
	    goto Done;
    }

    if (dflag) {
	char *temp;
	if ((r = krb5_unparse_name(context, increds.server, &temp)))
	    temp = 0;
	printf("Try to get ticket for: %s\n", temp ? temp : name);
	if (temp) free(temp);
    }

    increds.client = client_principal;
    increds.times.endtime = 0;

#ifdef AFS_RXK5
    /* 1st component service name will be either afs (3) or afs-k5 (6) */
    if (get_princ_len(context, increds.server, 0) != 3) {
    	/* Get the strongest credentials this KDC can issue for the princ, and the
	   cache manager supports */
	int enc_ix;
	r = KTC_ERROR;
	for(enc_ix = 0; enc_ix < max_enc; ++enc_ix) {
	    get_creds_enctype((&increds)) = enctypes_pref_order[enc_ix];
	    r = krb5_get_credentials(context, 0, aklog_ccache, &increds, creds);
	    if(!r) {
		if(dflag) {
		    printf("Successful get_creds_enctype with enctype == %d\n",
			    enctypes_pref_order[enc_ix]);
		}
		break;
	    }
	}
    } else {
#endif	/* AFS_RXK5 */
    	/* Ask for DES since that is what V4 understands */
    	get_creds_enctype((&increds)) = ENCTYPE_DES_CBC_CRC;
    	r = krb5_get_credentials(context, 0, aklog_ccache, &increds, creds);
#ifdef AFS_RXK5
    }
#endif	/* AFS_RXK5 */

Done:
    krb5_free_principal(context, increds.server);

    return r;
}

#ifdef HAVE_KRB4
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
#endif

/* As of MIT Kerberos 1.6, krb5_get_host_realm() will return the NUL-string 
 * if there is no domain_realm mapping for the hostname's domain.  This is 
 * used as a trigger indicating that referrals should be used within the
 * krb5_get_credentials() call.  However, if the KDC does not support referrals
 * that will result in a KRB5_ERR_HOST_REALM_UNKNOWN error and we will have
 * to manually fallback to mapping the domain of the host as a realm name.
 * Hence, the new fallback parameter.
 */
static char *afs_realm_of_cell5(krb5_context context, struct afsconf_cell *cellconfig, int fallback)
{
    char ** krbrlms = 0;
    static char krbrlm[REALM_SZ+1];
    krb5_error_code status;

    if (!cellconfig)
	return 0;

    if (fallback) {
	char * p;
	p = strchr(cellconfig->hostName[0], '.');
	if (p++)
	    strcpy(krbrlm, p);
	else
	    strcpy(krbrlm, cellconfig->name);
	strupr(krbrlm);
    } else {
	status = krb5_get_host_realm( context, cellconfig->hostName[0], &krbrlms );
	if (status == 0 && krbrlms && krbrlms[0]) {
	    strcpy(krbrlm, krbrlms[0]);
	} else {
	    strcpy(krbrlm, cellconfig->name);
	    strupr(krbrlm);
	}

	if (krbrlms)
	    krb5_free_host_realm( context, krbrlms );
    }
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

    if (!aklog_ccache)
        krb5_cc_default(context, &aklog_ccache);
    if (!client_principal)
        krb5_cc_get_principal(context, aklog_ccache, &client_principal);

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
    char username[BUFSIZ];	  /* To hold client username structure */
    
    char *service_list[4], service_temp[MAXKTCREALMLEN + 20];
    char service_temp_ref[MAXKTCREALMLEN + 20];
    char *k5service = 0, *service;
    int i;

    char name[ANAME_SZ];	  /* Name of afs key */
    char instance[INST_SZ];	  /* Instance of afs key */
    char realm_of_user[REALM_SZ]; /* Kerberos realm of user */
    char realm_of_cell[REALM_SZ]; /* Kerberos realm of cell */
    char local_cell[MAXCELLCHARS+1];
    char cell_to_use[MAXCELLCHARS+1]; /* Cell to authenticate to */

    krb5_creds *v5cred = NULL;
#ifdef HAVE_KRB4
    CREDENTIALS c;
#endif
    struct ktc_principal aserver;
    struct ktc_principal aclient;
    struct ktc_token atoken, btoken;
	afs_int32 viceId = ANONYMOUSID;

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
    memset(service_temp, 0, sizeof(service_temp));
    memset(service_temp_ref, 0, sizeof(service_temp_ref));

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
	
    strncpy(instance, cell_to_use, sizeof(instance));
    instance[sizeof(instance)-1] = '\0';

    /* XXX */
    /*
     * Extract the session key from the ticket file and hand-frob an
     * afs style authenticator.
     */

    if (usev5) 
    { /* using krb5 */
        int retry = 1;
        int realm_fallback = 0;

        if ( strchr(name,'.') != NULL ) {
            fprintf(stderr, "%s: Can't support principal names including a dot.\n",
                    progname);
            return(AKLOG_MISC);
        }

      try_v5:
	if (realm && realm[0])
	    strcpy(realm_of_cell, realm);
	else
	    strcpy(realm_of_cell,
		    afs_realm_of_cell5(context, &ak_cellconfig, realm_fallback));

	if (dflag)
            printf("Getting v5 tickets: %s/%s@%s\n", name, instance, realm_of_cell);
            
	if (*realm_of_cell)
	    status = krb5_set_default_realm(context, realm_of_cell);
	if (status) {
	    if (dflag) {
		printf("Kerberos error code returned by krb5_set_default_realm: %d\n",
			status);
	    }
            /* XXX should be afs_com_err, eventually */
	    com_err(progname, status, "can't make <%s> the default realm",
		realm_of_cell);
	    return(AKLOG_KERBEROS);
	}

	i = 0;
#ifdef AFS_RXK5
	if (rxk5 & FORCE_RXK5) {
	    max_enc = ktc_GetK5Enctypes(enctypes_pref_order,
		sizeof enctypes_pref_order/sizeof*enctypes_pref_order);
	    if (max_enc > 0) {
		k5service = get_afs_krb5_svc_princ(&ak_cellconfig);
		service_list[i++] = k5service;
	    }
	}
#endif	/* AFS_RXK5 */    
	if (rxk5 & FORCE_RXKAD) {
	    snprintf(service_temp, sizeof service_temp,
		"%s/%s", AFSKEY, cell_to_use);
	    if (strcasecmp(cell_to_use, realm_of_cell) != 0) {
		service_list[i++] = service_temp;
		if (strcasecmp(cell_to_use, realm_of_cell) == 0) {
		    service_list[i++] = AFSKEY;
		}
	    } else {
		service_list[i++] = AFSKEY;
		service_list[i++] = service_temp;
	    }
	}
	service_list[i] = 0;

	if (!i) {
        /* XXX afs_com_err */
	    com_err(progname, 0, "requested security mechanism is not available.");
	    return(AKLOG_KERBEROS);
	}

        for (i = 0; (service = service_list[i]); ++i) {
	    status = get_credv5(context, service, &v5cred);

	    if (status != KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN
		&& status != KRB5KRB_ERR_GENERIC)
	    break;
        }
	if (k5service) free(k5service);
            
	if (status == KRB5_ERR_HOST_REALM_UNKNOWN) {
	    realm_fallback = 1;
	    goto try_v5;
	} else if (status == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN) {
	    if (!realm_of_cell[0]) {
		realm_fallback = 1;
		goto try_v5;
	    }
            if (dflag)
                printf("Getting v5 tickets: %s@%s\n", name, realm_of_cell);
            status = get_v5cred(context, name, "", realm_of_cell, 
#ifdef HAVE_KRB4
                                use524 ? &c : NULL, 
#else
                                NULL,
#endif
                                &v5cred);
	}
        if ( status == KRB5KRB_AP_ERR_MSG_TYPE && retry ) {
            retry = 0;
	    realm_fallback = 0;
            goto try_v5;
        }
    } /* usev5 */
    else 
    {
#ifdef HAVE_KRB4
	if (realm && realm[0])
	    strcpy(realm_of_cell, realm);
	else
	    strcpy(realm_of_cell, afs_realm_of_cell(&ak_cellconfig));

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
#else
        return(AKLOG_MISC);
#endif
    } /* else !usev5 */

    /* TODO: get k5 error text */
    if (status != KSUCCESS)
    {
        if (dflag)
            printf("Kerberos error code returned by get_cred: %d\n", status);
        fprintf(stderr, "%s: Couldn't get %s AFS tickets: %s\n",
                 progname, cell_to_use, 
#ifdef HAVE_KRB4
                 (usev5)?"":krb_err_text(status)
#else
                 ""
#endif
                 );
        return(AKLOG_KERBEROS);
    }

    strncpy(aserver.name, AFSKEY, MAXKTCNAMELEN - 1);
    strncpy(aserver.instance, AFSINST, MAXKTCNAMELEN - 1);
    strncpy(aserver.cell, cell_to_use, MAXKTCREALMLEN - 1);

    if (usev5
#ifdef HAVE_KRB4
	&& !use524
#endif
	) {
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
#ifdef HAVE_KRB4
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
#else
        return(AKLOG_MISC);
#endif
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
#ifdef HAVE_KRB4
            if ((status = krb_get_tf_realm(TKT_FILE, realm_of_user)) != KSUCCESS)
            {
                fprintf(stderr, "%s: Couldn't determine realm of user: %s)",
                         progname, krb_err_text(status));
                return(AKLOG_KERBEROS);
            }
#else
            return(AKLOG_MISC);
#endif
        }

        /* For Khimaira we want to always append the realm to the name */
        if (1 /* strcmp(realm_of_user, realm_of_cell) */)
        {
            strcat(username, "@");
            strcat(username, realm_of_user);
        }

        ViceIDToUsername(username, realm_of_user, realm_of_cell, cell_to_use,
#ifdef HAVE_KRB4
			&c, 
#else
			NULL,
#endif
			&status, &aclient, &aserver, &viceId, &atoken);
    }

    if (dflag)
        printf("Set username to %s\n", username);

    /* Reset the "aclient" structure before we call ktc_SetToken.
     * This structure was first set by the ktc_GetToken call when
     * we were comparing whether identical tokens already existed.
     */
    strncpy(aclient.name, username, MAXKTCNAMELEN - 1);
    strcpy(aclient.instance, "");
        
    if (usev5
#ifdef HAVE_KRB4
	&& !use524
#endif
	) {
        int len = min(v5cred->client->realm.length,MAXKTCNAMELEN - 1);
        strncpy(aclient.cell, v5cred->client->realm.data, len);
        aclient.cell[len] = '\0';
    } 
#ifdef HAVE_KRB4
    else
	strncpy(aclient.cell, c.realm, MAXKTCREALMLEN - 1);
#endif

    for ( i=0; aclient.cell[i]; i++ ) {
        if ( islower(aclient.cell[i]) )
            aclient.cell[i] = toupper(aclient.cell[i]);
    }

    if (dflag)
        printf("Getting tokens.\n");
#ifdef AFS_RXK5
	if(rxk5) {	
	  if ((status = ktc_SetK5Token(context, aserver.cell, v5cred,  username, username, FALSE /* afssetpag */))) {
	    	fprintf(stderr, 
		    "%s: unable to obtain tokens for cell %s (status: %d).\n",
		    progname, cell_to_use, status);
	    	status = AKLOG_TOKEN;
	    }
	} else {
#endif	/* AFS_RXK5 */		
    if (status = ktc_SetToken(&aserver, &atoken, &aclient, 0))
    {
        fprintf(stderr,
                 "%s: unable to obtain tokens for cell %s (status: %d).\n",
                 progname, cell_to_use, status);
        status = AKLOG_TOKEN;
    }
#ifdef AFS_RXK5
	}
#endif	/* AFS_RXK5 */
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
#ifdef HAVE_KRB4
             "    [-5 [-m]| -4]\n",
#else
             "    [-5]\n"
#endif
	#ifdef AFS_RXK5
			 "    [-k5] [-k4]\n"
	#endif
             );
    fprintf(stderr, "    -d gives debugging information.\n");
    fprintf(stderr, "    krb_realm is the kerberos realm of a cell.\n");
    fprintf(stderr, "    pathname is the name of a directory to which ");
    fprintf(stderr, "you wish to authenticate.\n");
    fprintf(stderr, "    -noprdb means don't try to determine AFS ID.\n");
#ifdef HAVE_KRB4
    fprintf(stderr, "    -5 or -4 selects whether to use Kerberos v5 or Kerberos v4.\n"
                    "       (default is Kerberos v5)\n");
    fprintf(stderr, "       -m means use krb524d to convert Kerberos v5 tickets.\n");
#else
    fprintf(stderr, "    -5 use Kerberos v5.\n"
                    "       (only Kerberos v5 is available)\n");
#endif
#ifdef AFS_RXK5
    fprintf(stderr, "    -k5 means do rxk5 (kernel uses V5 tickets)\n");
    fprintf(stderr, "    -k4 means do rxkad (kernel uses V4 or 2b tickets)\n");
#endif	/* AFS_RXK5 */
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
	
#ifdef AFS_RXK5
     /* Select for rxk5 unless AFS_RXK5_DEFAULT envvar is not 1|yes */
    rxk5 = env_afs_rxk5_default();
#endif	

    /* Parse commandline arguments and make list of what to do. */
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-d") == 0)
            dflag++;
        else if (strcmp(argv[i], "-5") == 0)
            usev5++;
#ifdef HAVE_KRB4
        else if (strcmp(argv[i], "-m") == 0)
            use524++;
        else if (strcmp(argv[i], "-4") == 0)
            usev5 = 0;
#endif
#ifdef AFS_RXK5
		else if (strcmp(argv[i], "-k4") == 0)
	    	rxk5 = 0;
		else if (strcmp(argv[i], "-k5") == 0)
	    	rxk5 = 1;
#endif	/* AFS_RXK5 */
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
