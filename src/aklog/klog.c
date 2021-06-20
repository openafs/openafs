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
#include <afs/stds.h>

#include <roken.h>

#include <rx/xdr.h>
#include <lock.h>
#include <ubik.h>
#include <afs/com_err.h>
#include <afs/auth.h>
#include <afs/afsutil.h>
#include <afs/cellconfig.h>
#include <afs/ptclient.h>
#include <afs/cmd.h>
#include <afs/ptuser.h>

#define KERBEROS_APPLE_DEPRECATED(x)
#include <krb5.h>

#ifdef HAVE_KRB5_CREDS_KEYBLOCK
#define USING_MIT 1
#endif
#ifdef HAVE_KRB5_CREDS_SESSION
#define USING_HEIMDAL 1
#endif

#include "skipwrap.h"

/* This code borrowed heavily from the previous version of log.  Here is the
   intro comment for that program: */

/*
	log -- tell the Andrew Cache Manager your password
	5 June 1985
	modified
	February 1986

	Further modified in August 1987 to understand cell IDs.

	Further modified in October 2006 to understand kerberos 5.
 */

/* Current Usage:
     klog [principal [password]] [-t] [-c cellname] [-k <k5realm>]

     where:
       principal is of the form 'name' or 'name@cell' which provides the
	  cellname.  See the -c option below.
       password is the user's password.  This form is NOT recommended for
	  interactive users.
       -t advises klog to write a Kerberos style ticket file in /tmp.
       -c identifies cellname as the cell in which authentication is to take
	  place.
       -k identifies an alternate kerberos realm to use provide
	  authentication services for the cell.
 */

#define KLOGEXIT(code) rx_Finalize(); \
                       (exit(!!code))
static int CommandProc(struct cmd_syndesc *as, void *arock);

static int zero_argc;
static char **zero_argv;

static krb5_context k5context;
static struct afsconf_dir *tdir;
static int always_evil = 2;	/* gcc optimizes 0 into bss.  fools. */

int
main(int argc, char *argv[])
{
    struct cmd_syndesc *ts;
    afs_int32 code;
#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a
     * crash (i.e. core is generated) we can include the user's data section
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGABRT, &nsa, NULL);
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    zero_argc = argc;
    zero_argv = argv;

    ts = cmd_CreateSyntax(NULL, CommandProc, NULL, 0,
			  "obtain Kerberos authentication");

#define aXFLAG 0
#define aPRINCIPAL 1
#define aPASSWORD 2
#define aCELL 3
#define aKRBREALM 4
#define aPIPE 5
#define aSILENT 6
#define aLIFETIME 7
#define aSETPAG 8
#define aTMP 9
#define aNOPRDB 10
#define aUNWRAP 11
#define aK5 12
#define aK4 13
#define aDES 14

    cmd_AddParm(ts, "-x", CMD_FLAG, CMD_OPTIONAL, "obsolete, noop");
    cmd_Seek(ts, aPRINCIPAL);
    cmd_AddParm(ts, "-principal", CMD_SINGLE, CMD_OPTIONAL, "user name");
    cmd_AddParm(ts, "-password", CMD_SINGLE, CMD_OPTIONAL, "user's password");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    cmd_AddParm(ts, "-k", CMD_SINGLE, CMD_OPTIONAL, "krb5 realm");
    cmd_AddParm(ts, "-pipe", CMD_FLAG, CMD_OPTIONAL,
		"read password from stdin");
    cmd_AddParm(ts, "-silent", CMD_FLAG, CMD_OPTIONAL, "silent operation");
    /* Note: -lifetime is not implemented in this version of klog. */
    cmd_AddParm(ts, "-lifetime", CMD_SINGLE, CMD_OPTIONAL,
		"ticket lifetime in hh[:mm[:ss]]");
    cmd_AddParm(ts, "-setpag", CMD_FLAG, CMD_OPTIONAL,
		"Create a new setpag before authenticating");
    cmd_AddParm(ts, "-tmp", CMD_FLAG, CMD_OPTIONAL,
		"write Kerberos-style ticket file in /tmp");
    cmd_AddParm(ts, "-noprdb", CMD_FLAG, CMD_OPTIONAL, "don't consult pt");
    cmd_AddParm(ts, "-unwrap", CMD_FLAG, CMD_OPTIONAL, "perform 524d conversion");
#ifdef AFS_RXK5
    cmd_AddParm(ts, "-k5", CMD_FLAG, CMD_OPTIONAL, "get rxk5 credentials");
    cmd_AddParm(ts, "-k4", CMD_FLAG, CMD_OPTIONAL, "get rxkad credentials");
#else
    ++ts->nParms;	/* skip -k5 */
    cmd_AddParm(ts, "-k4", CMD_FLAG, CMD_OPTIONAL|CMD_HIDDEN, 0);
#endif
    cmd_AddParm(ts, "-insecure_des", CMD_FLAG, CMD_OPTIONAL,
		"enable insecure single-DES for krb5");

    code = cmd_Dispatch(argc, argv);
    KLOGEXIT(code);
}

static char *
getpipepass(void)
{
    static char gpbuf[BUFSIZ];
    /* read a password from stdin, stop on \n or eof */
    int i, tc;
    memset(gpbuf, 0, sizeof(gpbuf));
    for (i = 0; i < (sizeof(gpbuf) - 1); i++) {
	tc = fgetc(stdin);
	if (tc == '\n' || tc == EOF)
	    break;
	gpbuf[i] = tc;
    }
    return gpbuf;
}

void
silent_errors(const char *who,
    afs_int32 code,
    const char *fmt,
    va_list ap)
{
    /* ignore and don't print error */
}

#if defined(HAVE_KRB5_PRINC_SIZE) || defined(krb5_princ_size)

#define get_princ_str(c, p, n) krb5_princ_component(c, p, n)->data
#define get_princ_len(c, p, n) krb5_princ_component(c, p, n)->length
#define num_comp(c, p) (krb5_princ_size(c, p))
#define realm_data(c, p) krb5_princ_realm(c, p)->data
#define realm_len(c, p) krb5_princ_realm(c, p)->length

#elif defined(HAVE_KRB5_PRINCIPAL_GET_COMP_STRING)

#define get_princ_str(c, p, n) krb5_principal_get_comp_string(c, p, n)
#define get_princ_len(c, p, n) strlen(krb5_principal_get_comp_string(c, p, n))
#define num_comp(c, p) ((p)->name.name_string.len)
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

static int
whoami(struct ktc_token *atoken,
    struct afsconf_cell *cellconfig,
    struct ktc_principal *aclient,
    int *vicep)
{
    int code;
    char tempname[2*PR_MAXNAMELEN];

    code = pr_Initialize(0, AFSDIR_CLIENT_ETC_DIRPATH, cellconfig->name);
    if (code)
	goto Failed;

    if (*aclient->instance)
	snprintf (tempname, sizeof tempname, "%s.%s",
	    aclient->name, aclient->instance);
    else
	snprintf (tempname, sizeof tempname, "%s", aclient->name);
    code = pr_SNameToId(tempname, vicep);
Failed:
    return code;
}

static void
k5_to_k4_name(krb5_context k5context,
    krb5_principal k5princ,
    struct ktc_principal *ktcprinc)
{
    int i;

    switch(num_comp(k5context, k5princ)) {
	default:
	/* case 2: */
	    i = get_princ_len(k5context, k5princ, 1);
	    if (i > MAXKTCNAMELEN-1) i = MAXKTCNAMELEN-1;
	    memcpy(ktcprinc->instance, get_princ_str(k5context, k5princ, 1), i);
	    AFS_FALLTHROUGH;
	case 1:
	    i = get_princ_len(k5context, k5princ, 0);
	    if (i > MAXKTCNAMELEN-1) i = MAXKTCNAMELEN-1;
	    memcpy(ktcprinc->name, get_princ_str(k5context, k5princ, 0), i);
	    AFS_FALLTHROUGH;
	case 0:
	    break;
	}
}

static int
CommandProc(struct cmd_syndesc *as, void *arock)
{
    krb5_principal princ = 0;
    char *cell, *pname, **hrealms, *service;
    char service_temp[MAXKTCREALMLEN + 20];
    krb5_creds incred[1], mcred[1], *outcred = 0, *afscred;
    krb5_ccache cc = 0;
#ifdef HAVE_KRB5_GET_INIT_CREDS_OPT_ALLOC
    krb5_get_init_creds_opt *gic_opts;
#else
    krb5_get_init_creds_opt gic_opts[1];
#endif
    char *tofree = NULL, *outname;
    int code;
    char *what;
    int i, dosetpag, evil, noprdb, id;
#ifdef AFS_RXK5
    int authtype;
#endif
    krb5_data enc_part[1];
    char *pass = 0;

    char passwd[BUFSIZ];
    struct afsconf_cell cellconfig[1];

    static char rn[] = "klog";	/*Routine name */
    static int Pipe = 0;	/* reading from a pipe */
    static int Silent = 0;	/* Don't want error messages */

    int writeTicketFile = 0;	/* write ticket file to /tmp */

    service = 0;
    memset(incred, 0, sizeof *incred);
    /* blow away command line arguments */
    for (i = 1; i < zero_argc; i++)
	memset(zero_argv[i], 0, strlen(zero_argv[i]));
    zero_argc = 0;

    /* first determine quiet flag based on -silent switch */
    Silent = (as->parms[aSILENT].items ? 1 : 0);

    if (Silent) {
	afs_set_com_err_hook(silent_errors);
    }

    if ((code = krb5_init_context(&k5context))) {
	afs_com_err(rn, code, "while initializing Kerberos 5 library");
	KLOGEXIT(code);
    }
    if ((code = rx_Init(0))) {
	afs_com_err(rn, code, "while initializing rx");
	KLOGEXIT(code);
    }
    initialize_U_error_table();
    /*initialize_krb5_error_table();*/
    initialize_RXK_error_table();
    initialize_KTC_error_table();
    initialize_ACFG_error_table();
    /* initialize_rx_error_table(); */
    if (!(tdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH))) {
	afs_com_err(rn, 0, "can't get afs configuration (afsconf_Open(%s))",
	    AFSDIR_CLIENT_ETC_DIRPATH);
	KLOGEXIT(1);
    }

    /*
     * Enable DES enctypes, which are currently still required for AFS.
     * krb5_allow_weak_crypto is MIT Kerberos 1.8.  krb5_enctype_enable is
     * Heimdal.
     */
    if (as->parms[aDES].items) {
#if defined(HAVE_KRB5_ENCTYPE_ENABLE)
	i = krb5_enctype_valid(k5context, ETYPE_DES_CBC_CRC);
	if (i)
	    krb5_enctype_enable(k5context, ETYPE_DES_CBC_CRC);
#elif defined(HAVE_KRB5_ALLOW_WEAK_CRYPTO)
	krb5_allow_weak_crypto(k5context, 1);
#endif
    }

    /* Parse remaining arguments. */

    dosetpag = !! as->parms[aSETPAG].items;
    Pipe = !! as->parms[aPIPE].items;
    writeTicketFile = !! as->parms[aTMP].items;
    noprdb = !! as->parms[aNOPRDB].items;
    evil = (always_evil&1) || !! as->parms[aUNWRAP].items;

#ifdef AFS_RXK5
    authtype = 0;
    if (as->parms[aK5].items)
	authtype |= FORCE_RXK5;
    if (as->parms[aK4].items)
	authtype |= FORCE_RXKAD;
    if (!authtype)
	authtype |= env_afs_rxk5_default();
#endif

    cell = as->parms[aCELL].items ? as->parms[aCELL].items->data : 0;
    if ((code = afsconf_GetCellInfo(tdir, cell, "afsprot", cellconfig))) {
	if (cell)
	    afs_com_err(rn, code, "Can't get cell information for '%s'", cell);
	else
	    afs_com_err(rn, code, "Can't get determine local cell!");
	KLOGEXIT(code);
    }

    if (as->parms[aKRBREALM].items) {
	code = krb5_set_default_realm(k5context,
		as->parms[aKRBREALM].items->data);
	if (code) {
	    afs_com_err(rn, code, "Can't make <%s> the default realm",
		as->parms[aKRBREALM].items->data);
	    KLOGEXIT(code);
	}
    }
    else if ((code = krb5_get_host_realm(k5context, cellconfig->hostName[0], &hrealms))) {
	afs_com_err(rn, code, "Can't get realm for host <%s> in cell <%s>\n",
		cellconfig->hostName[0], cellconfig->name);
	KLOGEXIT(code);
    } else {
	if (hrealms && *hrealms) {
	    code = krb5_set_default_realm(k5context,
		    *hrealms);
	    if (code) {
		afs_com_err(rn, code, "Can't make <%s> the default realm",
		    *hrealms);
		KLOGEXIT(code);
	    }
	}
	if (hrealms) krb5_free_host_realm(k5context, hrealms);
    }

    id = getuid();
    if (as->parms[aPRINCIPAL].items) {
	pname = as->parms[aPRINCIPAL].items->data;
    } else {
	/* No explicit name provided: use Unix uid. */
	struct passwd *pw;
	pw = getpwuid(id);
	if (pw == 0) {
	    afs_com_err(rn, 0,
		"Can't figure out your name from your user id (%d).", id);
	    if (!Silent)
		fprintf(stderr, "%s: Try providing the user name.\n", rn);
	    KLOGEXIT(1);
	}
	pname = pw->pw_name;
    }
    code = krb5_parse_name(k5context, pname, &princ);
    if (code) {
	afs_com_err(rn, code, "Can't parse principal <%s>", pname);
	KLOGEXIT(code);
    }

    if (as->parms[aPASSWORD].items) {
	/*
	 * Current argument is the desired password string.  Remember it in
	 * our local buffer, and zero out the argument string - anyone can
	 * see it there with ps!
	 */
	strncpy(passwd, as->parms[aPASSWORD].items->data, sizeof(passwd));
	memset(as->parms[aPASSWORD].items->data, 0,
	       strlen(as->parms[aPASSWORD].items->data));
	pass = passwd;
    }

    /* Get the password from stdin if it wasn't provided. */
    if (!pass && Pipe) {
	strncpy(passwd, getpipepass(), sizeof(passwd));
	pass = passwd;
    }

    service = 0;
#ifdef AFS_RXK5
    if (authtype & FORCE_RXK5) {
	tofree = get_afs_krb5_svc_princ(cellconfig);
	snprintf(service_temp, sizeof service_temp, "%s", tofree);
    } else
#endif
    snprintf (service_temp, sizeof service_temp, "afs/%s", cellconfig->name);

    /* XXX should allow k5 to prompt in most cases -- what about expired pw?*/
#ifdef HAVE_KRB5_GET_INIT_CREDS_OPT_ALLOC
    code = krb5_get_init_creds_opt_alloc(k5context, &gic_opts);
    if (code) {
	afs_com_err(rn, code, "Can't allocate get_init_creds options");
	KLOGEXIT(code);
    }
#else
    krb5_get_init_creds_opt_init(gic_opts);
#endif

    for (;;) {
        code = krb5_get_init_creds_password(k5context,
	    incred,
	    princ,
	    pass,
	    krb5_prompter_posix,	/* prompter */
	    NULL,			/* data */
	    0,				/* start_time */
	    0,				/* in_tkt_service */
	    gic_opts);
	if (code != KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN)
            break;
    }
    memset(passwd, 0, sizeof(passwd));
    if (code) {
	char *r = 0;
	if (krb5_get_default_realm(k5context, &r))
	    r = 0;
	if (r)
	    afs_com_err(rn, code, "Unable to authenticate in realm %s", r);
	else
	    afs_com_err(rn, code, "Unable to authenticate to use cell %s",
		cellconfig->name);
	if (r) free(r);
	KLOGEXIT(code);
    }

    for (;;writeTicketFile = 0) {
        if (writeTicketFile) {
            what = "getting default ccache";
            code = krb5_cc_default(k5context, &cc);
        } else {
            what = "krb5_cc_resolve";
            code = krb5_cc_resolve(k5context, "MEMORY:core", &cc);
            if (code) goto Failed;
        }
        what = "initializing ccache";
        code = krb5_cc_initialize(k5context, cc, princ);
        if (code) goto Failed;
        what = "writing Kerberos ticket file";
        code = krb5_cc_store_cred(k5context, cc, incred);
        if (code) goto Failed;
        if (writeTicketFile)
            fprintf(stderr,
                    "Wrote ticket file to %s\n",
                    krb5_cc_get_name(k5context, cc));
        break;
      Failed:
        if (code)
            afs_com_err(rn, code, "%s", what);
        if (writeTicketFile) {
            if (cc) {
                krb5_cc_close(k5context, cc);
                cc = 0;
            }
            continue;
        }
        KLOGEXIT(code);
    }

    for (service = service_temp;;service = "afs") {
        memset(mcred, 0, sizeof *mcred);
        mcred->client = princ;
        code = krb5_parse_name(k5context, service, &mcred->server);
        if (code) {
            afs_com_err(rn, code, "Unable to parse service <%s>\n", service);
            KLOGEXIT(code);
        }
        if (tofree) { free(tofree); tofree = 0; }
        if (!(code = krb5_unparse_name(k5context, mcred->server, &outname)))
            tofree = outname;
        else outname = service;
        code = krb5_get_credentials(k5context, 0, cc, mcred, &outcred);
        krb5_free_principal(k5context, mcred->server);
        if (code != KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN || service != service_temp) break;
#ifdef AFS_RXK5
        if (authtype & FORCE_RXK5)
            break;
#endif
    }
    afscred = outcred;

    if (code) {
	afs_com_err(rn, code, "Unable to get credentials to use %s", outname);
	KLOGEXIT(code);
    }

#ifdef AFS_RXK5
    if (authtype & FORCE_RXK5) {
	struct ktc_principal aserver[1];
	int viceid = 555;

	memset(aserver, 0, sizeof *aserver);
	strncpy(aserver->cell, cellconfig->name, MAXKTCREALMLEN-1);
	code = ktc_SetK5Token(k5context, aserver, afscred, viceid, dosetpag);
	if (code) {
	    afs_com_err(rn, code, "Unable to store tokens for cell %s\n",
		cellconfig->name);
	    KLOGEXIT(1);
	}
    } else
#endif
    {
	struct ktc_principal aserver[1], aclient[1];
	struct ktc_token atoken[1];

	memset(atoken, 0, sizeof *atoken);
	if (evil) {
	    size_t elen = enc_part->length;
	    atoken->kvno = RXKAD_TKT_TYPE_KERBEROS_V5_ENCPART_ONLY;
	    if (afs_krb5_skip_ticket_wrapper(afscred->ticket.data,
			afscred->ticket.length, (char **) &enc_part->data,
			&elen)) {
		afs_com_err(rn, 0, "Can't unwrap %s AFS credential",
		    cellconfig->name);
		KLOGEXIT(1);
	    }
	} else {
	    atoken->kvno = RXKAD_TKT_TYPE_KERBEROS_V5;
	    *enc_part = afscred->ticket;
	}
	atoken->startTime = afscred->times.starttime;
	atoken->endTime = afscred->times.endtime;
	if (tkt_DeriveDesKey(get_creds_enctype(afscred),
			     get_cred_keydata(afscred),
			     get_cred_keylen(afscred), &atoken->sessionKey)) {
	    afs_com_err(rn, 0,
			"Cannot derive DES key from enctype %i of length %u",
			get_creds_enctype(afscred),
			(unsigned)get_cred_keylen(afscred));
	    KLOGEXIT(1);
	}
	memcpy(atoken->ticket, enc_part->data,
	    atoken->ticketLen = enc_part->length);
	memset(aserver, 0, sizeof *aserver);
	strncpy(aserver->name, "afs", 4);
	strncpy(aserver->cell, cellconfig->name, MAXKTCREALMLEN-1);
	memset(aclient, 0, sizeof *aclient);
	i = realm_len(k5context, afscred->client);
	if (i > MAXKTCREALMLEN-1) i = MAXKTCREALMLEN-1;
	memcpy(aclient->cell, realm_data(k5context, afscred->client), i);
	if (!noprdb) {
	    int viceid = 0;
	    k5_to_k4_name(k5context, afscred->client, aclient);
	    code = whoami(atoken, cellconfig, aclient, &viceid);
	    if (code) {
		afs_com_err(rn, code, "Can't get your viceid for cell %s", cellconfig->name);
		*aclient->name = 0;
	    } else
		snprintf(aclient->name, MAXKTCNAMELEN-1, "AFS ID %d", viceid);
	}
	if (!*aclient->name)
	    k5_to_k4_name(k5context, afscred->client, aclient);
	code = ktc_SetToken(aserver, atoken, aclient, dosetpag);
	if (code) {
	    afs_com_err(rn, code, "Unable to store tokens for cell %s\n",
		cellconfig->name);
	    KLOGEXIT(1);
	}
    }

    krb5_free_principal(k5context, princ);
    krb5_free_cred_contents(k5context, incred);
    if (outcred) krb5_free_creds(k5context, outcred);
    if (cc)
	krb5_cc_close(k5context, cc);
    if (tofree) free(tofree);

    return 0;
}
