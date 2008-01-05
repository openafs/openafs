/*
 * Copyright (c) 2005,2006,2007, 2008 Secure Endpoints Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* $Id$ */

/* Disable the 'macro redefinition' warning which is getting
   triggerred by a redefinition of the ENCRYPT and DECRYPT macros. */
#pragma warning (push)
#pragma warning (disable: 4005)

#include<afscred.h>
#include<dynimport.h>
#include<krb5common.h>

#pragma warning (pop)

BOOL
afs_is_running(void) {
    DWORD CurrentState;

    if (!AfsAvailable)
        return FALSE;

    if (GetServiceStatus(NULL, TRANSARCAFSDAEMON, 
                         &CurrentState, NULL) != NOERROR)
        return FALSE;
    if (CurrentState != SERVICE_RUNNING)
        return FALSE;

    return TRUE;
}

int
afs_unlog(void)
{
    long	rc;

    if (!afs_is_running())
        return 0;

    rc = ktc_ForgetAllTokens();

    return rc;
}

int
afs_unlog_cred(khm_handle cred)
{
    long rc;
    struct ktc_principal princ;
    khm_size cbbuf;
    wchar_t name[KCDB_MAXCCH_NAME];

    if (!afs_is_running())
        return 0;

    cbbuf = sizeof(princ);
    if(KHM_FAILED(kcdb_cred_get_attr(cred, afs_attr_server_princ, 
                                     NULL, &princ, &cbbuf)))
        return 1;

    afs_princ_to_string(&princ, name, sizeof(name));

    _report_cs1(KHERR_INFO, L"Destroying token %1!s!",
                _cstr(name));
    _resolve();

    rc = ktc_ForgetToken(&princ);

    return rc;
}

/* convert a ktc_principal to a wchar_t string form that looks like
    name.instance@cell return 0 if it worked. non-zero otherwise
*/
int 
afs_princ_to_string(struct ktc_principal * p, 
                    wchar_t * buf, 
                    size_t cbbuf)
{
    wchar_t wbuf[256];
    int rv = 0;
    int l;

    l = AnsiStrToUnicode(wbuf, sizeof(wbuf), p->name);
    wbuf[l] = L'\0';

    rv = FAILED(StringCbCopy(buf, cbbuf, wbuf));
    if(p->instance[0]) {
        StringCbCat(buf, cbbuf, L".");
        if((l = AnsiStrToUnicode(wbuf, sizeof(wbuf), p->instance)) > 0) {
            wbuf[l] = L'\0';
            rv = rv || FAILED(StringCbCat(buf, cbbuf, wbuf));
        }
        else
            rv = 1;
    }
    if(p->cell[0]) {
        rv = rv || FAILED(StringCbCat(buf, cbbuf, L"@"));
        if((l = AnsiStrToUnicode(wbuf, sizeof(wbuf), p->cell)) > 0) {
            wbuf[l] = L'\0';
            rv = rv || FAILED(StringCbCat(buf, cbbuf, wbuf));
        }
        else
            rv = 1;
    }

    return rv;
}

int 
afs_list_tokens(void)
{
    int r;

    kcdb_credset_flush(afs_credset);
    r = afs_list_tokens_internal();
    kcdb_credset_collect(NULL, afs_credset, NULL, afs_credtype_id, NULL);

    return r;
}

/* is the credential provided an AFS token and is it from the
   specified cell? */
static khm_int32 KHMAPI 
afs_filter_by_cell(khm_handle cred, khm_int32 flags, void * rock)
{
    wchar_t wcell[MAXCELLCHARS];
    wchar_t * tcell;
    khm_size cbsize;
    khm_int32 type;

    tcell = (wchar_t *) rock;

    if(KHM_FAILED(kcdb_cred_get_type(cred, &type)) ||
        type != afs_credtype_id)
        return FALSE;

    cbsize = sizeof(wcell);
    if(KHM_FAILED(kcdb_cred_get_attr(cred, afs_attr_cell, 
                                     NULL, wcell, &cbsize)))
        return FALSE;

    if(wcscmp(wcell, tcell))
        return FALSE;

    return TRUE;
}

struct token_filter_data {
    wchar_t * cell;
};

khm_int32 KHMAPI
afs_filter_for_token(khm_handle cred, khm_int32 flags, void * rock) {
    struct token_filter_data * pdata;
    wchar_t ccell[MAXCELLCHARS];
    khm_size cb;
    khm_int32 ctype;

    pdata = (struct token_filter_data *) rock;

    if (KHM_FAILED(kcdb_cred_get_type(cred, &ctype)) ||
        ctype != afs_credtype_id)

        return 0;

    cb = sizeof(ccell);

    if (KHM_FAILED(kcdb_cred_get_attr(cred, afs_attr_cell,
                                      NULL,
                                      ccell,
                                      &cb)) ||
        _wcsicmp(ccell, pdata->cell))

        return 0;

    return 1;
}

khm_handle
afs_find_token(khm_handle credset, wchar_t * cell) {
    struct token_filter_data fdata;
    khm_handle cred = NULL;

    fdata.cell = cell;

    if (KHM_FAILED(kcdb_credset_find_filtered(credset,
                                              -1,
                                              afs_filter_for_token,
                                              &fdata,
                                              &cred,
                                              NULL)))
        return NULL;
    else
        return cred;
}

static khm_int32 KHMAPI 
afs_filter_krb5_tkt(khm_handle cred, khm_int32 flags, void * rock) 
{
    wchar_t cname[KCDB_CRED_MAXCCH_NAME];
    khm_int32 type;
    wchar_t * tcell;
    wchar_t * t, *tkt_cell;
    khm_size cbsize;

    tcell = (wchar_t *) rock;

    if(KHM_FAILED(kcdb_cred_get_type(cred, &type)) ||
       type != krb5_credtype_id)
        return FALSE;

    cbsize = sizeof(cname);
    if (KHM_FAILED(kcdb_cred_get_name(cred, cname, &cbsize)))
        return FALSE;

    if (!wcsncmp(cname, L"afs/", 4)) {

        tkt_cell = cname + 4;

        t = wcschr(tkt_cell, L'@');
        if (t == NULL)
            return FALSE;
        *t = L'\0';

    } else if (!wcsncmp(cname, L"afs@", 4)) {

        tkt_cell = cname + 4;

    } else {
        return FALSE;
    }

    if (_wcsicmp(tcell, tkt_cell))
        return FALSE;

    return TRUE;
}

static khm_int32 KHMAPI 
afs_filter_krb4_tkt(khm_handle cred, khm_int32 flags, void * rock) 
{
    wchar_t cname[KCDB_CRED_MAXCCH_NAME];
    khm_int32 type;
    wchar_t * tcell;
    wchar_t * t, *tkt_cell;
    khm_size cbsize;

    tcell = (wchar_t *) rock;

    if(KHM_FAILED(kcdb_cred_get_type(cred, &type)) ||
       type != krb4_credtype_id)
        return FALSE;

    cbsize = sizeof(cname);
    if (KHM_FAILED(kcdb_cred_get_name(cred, cname, &cbsize)))
        return FALSE;

    if (!wcsncmp(cname, L"afs.", 4)) {

        tkt_cell = cname + 4;

        t = wcschr(tkt_cell, L'@');
        if (t == NULL)
            return FALSE;
        *t = L'\0';

    } else if (!wcsncmp(cname, L"afs@", 4)) {

        tkt_cell = cname + 4;

    } else {
        return FALSE;
    }

    if (_wcsicmp(tcell, tkt_cell))
        return FALSE;

    return TRUE;
}

/* collects all AFS tokens to the root credential set using the
   generic afs_credset credential set
   */
int
afs_list_tokens_internal(void)
{
    struct ktc_principal    aserver;
    struct ktc_principal    aclient;
    struct ktc_token        atoken;
    int                     cellNum;
    int                     BreakAtEnd;
    wchar_t                 idname[256];
    wchar_t                 crname[256];
    wchar_t                 location[256];
    wchar_t                 *cell;

    DWORD                   rc;

    khm_handle              ident = NULL;
    khm_handle              cred = NULL;
    afs_tk_method           method;

    FILETIME                ft;

    if (!afs_is_running())
        return 0;

    kcdb_credset_flush(afs_credset);

    LoadString(hResModule, IDS_DEF_LOCATION, location, ARRAYLENGTH(location));

    BreakAtEnd = 0;
    cellNum = 0;
    while (1) 
    {
        memset(&aserver, 0, sizeof(aserver));
        if (rc = ktc_ListTokens(cellNum, &cellNum, &aserver))
        {
            if (rc != KTC_NOENT)
                return(0);

            if (BreakAtEnd == 1)
                break;
        }
        BreakAtEnd = 1;
        memset(&atoken, '\0', sizeof(atoken));
        if (rc = ktc_GetToken(&aserver, &atoken, sizeof(atoken), &aclient))
        {
            if (rc == KTC_ERROR)
                return(0);

            continue;
        }

#if 0
        /* failed attempt at trying to figure out the principal name from
           the token.  The ticket that is attached to the token is not
           in a form that is useful at this point */
        idname[0] = L'\0';
        if(atoken.kvno == RXKAD_TKT_TYPE_KERBEROS_V5) {
            krb5_context ctx = 0;
            krb5_ccache cc = 0;
            krb5_creds * k5c;
            krb5_error_code code;
            char * princ;

            code = khm_krb5_initialize(&ctx, &cc);
            if(code)
                goto _no_krb5;

            k5c = (krb5_creds *) atoken.ticket;

            code = pkrb5_unparse_name(ctx, k5c->client, &princ);
            if(code)
                goto _no_krb5;

            MultiByteToWideChar(CP_ACP, 0, princ, strlen(princ), idname, sizeof(idname)/sizeof(idname[0]));

            pkrb5_free_unparsed_name(ctx, princ);
_no_krb5:
            ;
        }
#endif

        method = AFS_TOKEN_AUTO;

        afs_princ_to_string(&aclient, idname, sizeof(idname));

        /* We need to figure out a good client name which we can use
           to create an identity which looks familiar to the user.  No
           good way of doing this, so we use a heuristic.

           Note that, we use another heuristic to find out which
           identity to associate the token with.
   
           ASSUMPTION:

           The assumption here is that the principal for the token is
           computed as follows:
           
           if realm != cell : principal looks like user@realm@cell
           if realm == cell : principal looks like user@realm
        
           HEURISTIC:
        
           We strip the part of the string that follows the second '@'
           sign to obtain the 'user@realm' part, which we use as the
           credential name.  If there is no second '@', we use the
           whole principal name. */
        {
            wchar_t * ats;

            ats = wcschr(idname, L'@');
            if(ats && (ats = wcschr(ats + 1, L'@')))
                *ats = L'\0';
        }

        afs_princ_to_string(&aserver, crname, sizeof(crname));

        /* Ok, now we need to figure out which identity to associate
           this token with.  This is a little bit tricky, and there is
           currently no good way of determining the original identity
           used to obtain the token if it was done outside of
           NetIDMgr.  So we use a heuristic here.

           REQUIREMENT:

           Elsewhere, (actually in afsnewcreds.c) just after obtaining
           AFS tokens through NetIDMgr, we enumerate the AFS tokens
           and assign the root identity (used to obtain new creds)
           with the AFS tokens.  This would still be there in the root
           credential set when we list tokens later on.

           HEURISTIC:

           If there exists an AFS token in the root credential set for
           the same cell, we associate this token with the same
           identity as that credential.
        */
        cell = wcschr(crname, L'@');
        if(cell) {
            cell++;
            if(!*cell)
                cell = NULL;
        }

        ident = NULL;
        if(cell) {
            khm_handle c;

            if(KHM_SUCCEEDED(kcdb_credset_find_filtered(NULL, -1, 
                                                        afs_filter_by_cell, 
                                                        (void *) cell, 
                                                        &c, NULL))) {
                khm_size cb;

                kcdb_cred_get_identity(c, &ident);
                cb = sizeof(method);
                kcdb_cred_get_attr(c, afs_attr_method, NULL,
                                   &method, &cb);
                kcdb_cred_release(c);
            }
        }

        /* If that failed, we have try another trick.  If there is a
           Krb5 ticket of the form afs/<cell>@<realm> or afs@<CELL>
           where <cell> matches our cell, then we pick the identity
           off of that.

           ASSUMPTION:

           If Krb5 was used to obtain the token, then there is a Krb5
           ticket of the form afs/<cell>@<REALM> or afs@<CELL> still
           in the cache.  This is also true for Krb524 token
           acquisition.

           HEURISTIC:

           If such a Krb5 ticket is found, use the identity of that
           credential as the identity of the AFS token.

        */
        if (ident == NULL && cell != NULL) {
            khm_handle c;

            if(KHM_SUCCEEDED(kcdb_credset_find_filtered(NULL, -1, 
                                                        afs_filter_krb5_tkt,
                                                        (void *) cell, 
                                                        &c, NULL))) {
                kcdb_cred_get_identity(c, &ident);
                /* this could be Krb5 or Krb524, so we leave method at
                   AFS_TOKEN_AUTO. */
                method = AFS_TOKEN_AUTO;
                kcdb_cred_release(c);
            }
        }

        /* If that didn't work either, we look for a Krb4 ticket of
           the form afs.<cell>@<REALM> or afs@<CELL> which matches the
           cell. 

           ASSUMPTION:

           If Krb4 was used to obtain an AFS token, then there should
           be a Krb4 ticket of the form afs.<cell>@<REALM> or
           afs@<CELL> in the cache.

           HEURISTIC:

           If such a ticket is found, then use the identity of that
           credential as the identity of the AFS token.
        */
        if (ident == NULL && cell != NULL) {
            khm_handle c;

            if (krb4_credtype_id < 0) {
                kcdb_credtype_get_id(KRB4_CREDTYPE_NAME,
                                     &krb4_credtype_id);
            }

            if (krb4_credtype_id >= 0 &&
                KHM_SUCCEEDED(kcdb_credset_find_filtered(NULL, -1,
                                                         afs_filter_krb4_tkt,
                                                         (void *) cell,
                                                         &c, NULL))) {

                kcdb_cred_get_identity(c, &ident);
                kcdb_cred_release(c);
                method = AFS_TOKEN_KRB4;

            }
        }

        /* Finally, we allow any extension plugins to give this a shot */
        if (ident == NULL && cell != NULL) {
            afs_ext_resolve_token(cell,
                                  &atoken,
                                  &aserver,
                                  &aclient,
                                  &ident,
                                  &method);
        }

        /* One more thing to try.  If we have a cell->identity
           mapping, then we try that. */
        if (ident == NULL && cell != NULL) {
            khm_handle h_cellmap;
            wchar_t tidname[KCDB_IDENT_MAXCCH_NAME];
            khm_size cb;

            cb = sizeof(tidname);

            if (KHM_SUCCEEDED(khc_open_space(csp_afscred,
                                             L"Cells", 0, 
                                             &h_cellmap))) {
                if (KHM_SUCCEEDED(khc_read_string(h_cellmap,
                                                  cell,
                                                  tidname,
                                                  &cb))) {
                    kcdb_identity_create(tidname,
                                         KCDB_IDENT_FLAG_CREATE,
                                         &ident);
                }
                khc_close_space(h_cellmap);
            }
        }

        /* all else failed */
        if(ident == NULL) {
            if(KHM_FAILED(kcdb_identity_create(idname, 
                                               KCDB_IDENT_FLAG_CREATE, 
                                               &ident)))
                goto _exit;
        }

        if(KHM_FAILED(kcdb_cred_create(crname, ident, afs_credtype_id, &cred)))
            goto _exit;

        kcdb_cred_set_attr(cred, afs_attr_method, &method, sizeof(method));

        TimetToFileTime(atoken.endTime, &ft);
        kcdb_cred_set_attr(cred, KCDB_ATTR_EXPIRE, &ft, sizeof(ft));
        if (atoken.startTime != 0) {
            TimetToFileTime(atoken.startTime, &ft);
            kcdb_cred_set_attr(cred, KCDB_ATTR_ISSUE, &ft, sizeof(ft));
        }
        kcdb_cred_set_attr(cred, afs_attr_client_princ, 
                           &aclient, sizeof(aclient));
        kcdb_cred_set_attr(cred, afs_attr_server_princ, 
                           &aserver, sizeof(aserver));

        if(cell) {
            kcdb_cred_set_attr(cred, afs_attr_cell, cell, (khm_size)KCDB_CBSIZE_AUTO);
        }

        kcdb_cred_set_attr(cred, KCDB_ATTR_LOCATION, 
                           location, (khm_size)KCDB_CBSIZE_AUTO);

        kcdb_credset_add_cred(afs_credset, cred, -1);

        /* both these calls are NULL pointer safe */
        kcdb_cred_release(cred);
        cred = NULL;
        kcdb_identity_release(ident);
        ident = NULL;
    }

_exit:
    if(ident)
        kcdb_identity_release(ident);
    if(cred)
        kcdb_cred_release(cred);

    return(0);
}


#define ALLOW_REGISTER 1
static int
ViceIDToUsername(char *username, 
                 char *realm_of_user, 
                 char *realm_of_cell,
                 char * cell_to_use,
                 struct ktc_principal *aclient, 
                 struct ktc_principal *aserver, 
                 struct ktc_token *atoken)
{
    static char lastcell[MAXCELLCHARS+1] = { 0 };
    static char confname[512] = { 0 };
    char username_copy[BUFSIZ];
    long viceId = ANONYMOUSID;		/* AFS uid of user */
    int  status = 0;
#ifdef ALLOW_REGISTER
    afs_int32 id;
#endif /* ALLOW_REGISTER */

    if (confname[0] == '\0') {
        StringCbCopyA(confname, sizeof(confname), AFSDIR_CLIENT_ETC_DIRPATH);
    }

    StringCbCopyA(lastcell, sizeof(lastcell), aserver->cell);

    if (!pr_Initialize (0, confname, aserver->cell)) {
        char sname[PR_MAXNAMELEN];
        StringCbCopyA(sname, sizeof(sname), username);
        status = pr_SNameToId (sname, &viceId);
	pr_End();
    }

#ifdef AFS_ID_TO_NAME
    /*
     * This is a crock, but it is Transarc's crock, so
     * we have to play along in order to get the
     * functionality.  The way the afs id is stored is
     * as a string in the username field of the token.
     * Contrary to what you may think by looking at
     * the code for tokens, this hack (AFS ID %d) will
     * not work if you change %d to something else.
     */
#endif /* AFS_ID_TO_NAME */
    /*
     * This code is taken from cklog -- it lets people
     * automatically register with the ptserver in foreign cells
     */

    /* copy the username because pr_CreateUser will lowercase it */
    StringCbCopyA(username_copy, BUFSIZ, username);

#ifdef ALLOW_REGISTER
    if (status == 0) {
        if (viceId != ANONYMOUSID) {
#else /* ALLOW_REGISTER */
	    if ((status == 0) && (viceId != ANONYMOUSID))
#endif /* ALLOW_REGISTER */
            {
#ifdef AFS_ID_TO_NAME
                StringCchPrintfA(username, BUFSIZ, "%s (AFS ID %d)", username_copy, (int) viceId);
#endif /* AFS_ID_TO_NAME */
            }
#ifdef ALLOW_REGISTER
        } else if (strcmp(realm_of_user, realm_of_cell) != 0) {
            id = 0;
            StringCbCopyA(aclient->name, sizeof(aclient->name), username);
            StringCbCopyA(aclient->instance, sizeof(aclient->instance), "");
            StringCbCopyA(aclient->cell, sizeof(aclient->cell), realm_of_user);
            if (status = ktc_SetToken(aserver, atoken, aclient, 0))
                return status;
            if (status = pr_Initialize(1L, confname, aserver->cell))
                return status;
            status = pr_CreateUser(username, &id);
	    pr_End();
            StringCbCopyA(username, BUFSIZ, username_copy);
#ifdef AFS_ID_TO_NAME
            StringCchPrintfA(username, BUFSIZ, "%s (AFS ID %d)", username_copy, (int) viceId);
#endif /* AFS_ID_TO_NAME */
        }
    }
#endif /* ALLOW_REGISTER */
    return status;
}


static void
copy_realm_of_ticket(krb5_context context, char * dest, size_t destlen, krb5_creds *v5cred) {
    krb5_error_code code;
    krb5_ticket *ticket;
    size_t len;

    code = pkrb5_decode_ticket(&v5cred->ticket, &ticket);
    if (code == 0) {
        len = krb5_princ_realm(context, ticket->server)->length;
        if (len > destlen - 1)
            len = destlen - 1;

        StringCbCopyA(dest, len, krb5_princ_realm(context, ticket->server)->data);

        pkrb5_free_ticket(context, ticket);
    }
}

int
afs_klog(khm_handle identity,
         char *service,
         char *cell,
         char *realm,
         int LifeTime,
         afs_tk_method method,
         time_t * tok_expiration) {

    long	rc;
    CREDENTIALS	creds;
    struct ktc_principal	aserver;
    struct ktc_principal	aclient;
    char	realm_of_user[REALM_SZ]; /* Kerberos realm of user */
    char	realm_of_cell[REALM_SZ]; /* Kerberos realm of cell */
    char	local_cell[MAXCELLCHARS+1];
    char	Dmycell[MAXCELLCHARS+1];
    struct ktc_token	atoken;
    struct ktc_token	btoken;
    afs_conf_cell	ak_cellconfig; /* General information about the cell */
    char	RealmName[128];
    char	CellName[128];
    char	ServiceName[128];
    khm_handle	confighandle = NULL;
    khm_int32	supports_krb4 = (pkrb_get_tf_realm == NULL ? 0 : 1);
    khm_int32   got524cred = 0;

    /* signalling */
    BOOL        bGotCreds = FALSE; /* got creds? */

    if (tok_expiration)
        *tok_expiration = (time_t) 0;

    if (!afs_is_running()) {
        _report_sr0(KHERR_WARNING, IDS_ERR_NOSERVICE);
        return(0);
    }

    if ( !realm )   realm = "";
    if ( !cell )    cell = "";
    if ( !service ) service = "";

    memset(RealmName, '\0', sizeof(RealmName));
    memset(CellName, '\0', sizeof(CellName));
    memset(ServiceName, '\0', sizeof(ServiceName));
    memset(realm_of_user, '\0', sizeof(realm_of_user));
    memset(realm_of_cell, '\0', sizeof(realm_of_cell));
    memset(Dmycell, '\0', sizeof(Dmycell));

    // NULL or empty cell returns information on local cell
    if (cell && cell[0])
        StringCbCopyA(Dmycell, sizeof(Dmycell), cell);

    rc = afs_get_cellconfig(Dmycell, &ak_cellconfig, local_cell);
    if (rc) {
        _reportf(L"afs_get_cellconfig returns %ld", rc);

        _report_sr2(KHERR_ERROR, IDS_ERR_CELLCONFIG, _cstr(Dmycell), _int32(rc));
        _suggest_sr(IDS_ERR_CELLCONFIG_S, KHERR_SUGGEST_NONE);
        _resolve();
        return(rc);
    }

    StringCbCopyA(realm_of_cell, sizeof(realm_of_cell), 
                  afs_realm_of_cell(&ak_cellconfig, FALSE));

    if (strlen(service) == 0)
        StringCbCopyA(ServiceName, sizeof(ServiceName), "afs");
    else
        StringCbCopyA(ServiceName, sizeof(ServiceName), service);

    if (strlen(cell) == 0)
        StringCbCopyA(CellName, sizeof(CellName), local_cell);
    else
        StringCbCopyA(CellName, sizeof(CellName), cell);

    if (strlen(realm) == 0)
        StringCbCopyA(RealmName, sizeof(RealmName), realm_of_cell);
    else
        StringCbCopyA(RealmName, sizeof(RealmName), realm);

    memset(&creds, '\0', sizeof(creds));

    /*** Kerberos 5 and 524 ***/

    if (method == AFS_TOKEN_AUTO ||
        method == AFS_TOKEN_KRB5 ||
        method == AFS_TOKEN_KRB524) {

        krb5_context   context = 0;
        krb5_ccache    k5cc = 0;
        krb5_creds     increds;
        krb5_creds *   k5creds = 0;
        krb5_error_code r;
        krb5_principal client_principal = 0;
        krb5_flags     flags = 0;

        int         retry = 0;
        int         len;
	char        *p;

        _reportf(L"Trying Kerberos 5");

        if (!(r = khm_krb5_initialize(identity, &context, &k5cc))) {
            int i;

            memset((char *)&increds, 0, sizeof(increds));

            pkrb5_cc_get_principal(context, k5cc, &client_principal);
            i = krb5_princ_realm(context, client_principal)->length;
            if (i > REALM_SZ-1) 
                i = REALM_SZ-1;
            StringCchCopyNA(realm_of_user, ARRAYLENGTH(realm_of_user),
                            krb5_princ_realm(context, client_principal)->data,
                            i);
        } else {
            _reportf(L"khm_krb5_initialize returns code %d", r);
            goto try_krb4;
        }

        /* First try Service/Cell@REALM */
        if (r = pkrb5_build_principal(context, &increds.server,
                                         (int) strlen(RealmName),
                                         RealmName,
                                         ServiceName,
                                         CellName,
                                         0)) {
            _reportf(L"krb5_build_principal returns %d", r);
            goto end_krb5;
        }

        increds.client = client_principal;
        increds.times.endtime = 0;
        /* Ask for DES since that is what V4 understands */
        increds.keyblock.enctype = ENCTYPE_DES_CBC_CRC;

#ifdef KRB5_TC_NOTICKET
        flags = KRB5_TC_OPENCLOSE;
        r = pkrb5_cc_set_flags(context, k5cc, flags);
#endif
      retry_retcred:
        r = pkrb5_get_credentials(context, 0, k5cc, &increds, &k5creds);
        if ((r == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN ||
	      r == KRB5KRB_ERR_GENERIC /* Heimdal */) &&
	     !RealmName[0]) {
	    StringCbCopyA(RealmName, sizeof(RealmName), 
			  afs_realm_of_cell(&ak_cellconfig, TRUE));

            pkrb5_free_principal(context, increds.server);
	    r = pkrb5_build_principal(context, &increds.server,
					 (int) strlen(RealmName),
					 RealmName,
					 ServiceName,
					 CellName,
					 0);
            if (r == 0)
                r = pkrb5_get_credentials(context, 0, k5cc, 
                                          &increds, &k5creds);
	}
	if (r == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN ||
            r == KRB5KRB_ERR_GENERIC /* Heimdal */) {
            /* Next try Service@REALM */
            pkrb5_free_principal(context, increds.server);
            r = pkrb5_build_principal(context, &increds.server,
                                         (int) strlen(RealmName),
                                         RealmName,
                                         ServiceName,
                                         0);
            if (r == 0)
                r = pkrb5_get_credentials(context, 0, k5cc, 
                                          &increds, &k5creds);
        }

	/* Check to make sure we received a valid ticket; if not remove it
	* and try again.  Perhaps there are two service tickets for the
	* same service in the ccache.
	*/
	if (r == 0 && k5creds && k5creds->times.endtime < time(NULL)) {
	    pkrb5_cc_remove_cred(context, k5cc, 0, k5creds);
	    pkrb5_free_creds(context, k5creds);
	    k5creds = NULL;
	    goto retry_retcred;
	}

        if (r == 0 && strlen(RealmName) == 0) 
            copy_realm_of_ticket(context, realm_of_cell, sizeof(realm_of_cell), k5creds);

        pkrb5_free_principal(context, increds.server);
        pkrb5_free_principal(context, client_principal);
        client_principal = 0;
#ifdef KRB5_TC_NOTICKET
        flags = KRB5_TC_OPENCLOSE | KRB5_TC_NOTICKET;
        pkrb5_cc_set_flags(context, k5cc, flags);
#endif

        (void) pkrb5_cc_close(context, k5cc);
        k5cc = 0;

        if (r) {
            _reportf(L"Code %d while getting credentials", r);
            k5creds = NULL;
            goto end_krb5;
        }

        if ( k5creds->ticket.length > MAXKTCTICKETLEN ||
             method == AFS_TOKEN_KRB524) {
            goto try_krb524d;
        }

        /* This code inserts the entire K5 ticket into the token */

        _reportf(L"Trying K5 SetToken");

        memset(&aserver, '\0', sizeof(aserver));
        StringCchCopyA(aserver.name, MAXKTCNAMELEN, ServiceName);
        StringCchCopyA(aserver.cell, MAXKTCREALMLEN, CellName);

        memset(&atoken, '\0', sizeof(atoken));
        atoken.kvno = RXKAD_TKT_TYPE_KERBEROS_V5;
        atoken.startTime = k5creds->times.starttime;
        atoken.endTime = k5creds->times.endtime;
        memcpy(&atoken.sessionKey, 
               k5creds->keyblock.contents, 
               k5creds->keyblock.length);
        atoken.ticketLen = k5creds->ticket.length;
        memcpy(atoken.ticket, k5creds->ticket.data, atoken.ticketLen);

        if (tok_expiration)
            *tok_expiration = k5creds->times.endtime;

    retry_gettoken5:
        rc = ktc_GetToken(&aserver, &btoken, sizeof(btoken), &aclient);
        if (rc != 0 && rc != KTC_NOENT && rc != KTC_NOCELL) {
            if ( rc == KTC_NOCM && retry < 20 ) {
                Sleep(500);
                retry++;
                goto retry_gettoken5;
            }
            goto try_krb524d;
        }

        if (atoken.kvno == btoken.kvno &&
            atoken.ticketLen == btoken.ticketLen &&
            !memcmp(&atoken.sessionKey, &btoken.sessionKey, 
                    sizeof(atoken.sessionKey)) &&
            !memcmp(atoken.ticket, btoken.ticket, atoken.ticketLen)) {

            /* success */
            if (k5creds && context)
                pkrb5_free_creds(context, k5creds);

            if (context)
                pkrb5_free_context(context);

            _reportf(L"Same token already exists");
            
            return 0;
        }

        // * Reset the "aclient" structure before we call ktc_SetToken.
        // * This structure was first set by the ktc_GetToken call when
        // * we were comparing whether identical tokens already existed.

        len = min(k5creds->client->data[0].length,MAXKTCNAMELEN - 1);
        StringCchCopyNA(aclient.name, MAXKTCNAMELEN,
                        k5creds->client->data[0].data, len);

        if ( k5creds->client->length > 1 ) {
            StringCbCatA(aclient.name, sizeof(aclient.name), ".");
            p = aclient.name + strlen(aclient.name);
            len = (int) min(k5creds->client->data[1].length,
                            MAXKTCNAMELEN - strlen(aclient.name) - 1);
            StringCchCopyNA(p, MAXKTCNAMELEN - strlen(aclient.name),
                            k5creds->client->data[1].data, len);
        }

        aclient.instance[0] = '\0';

        StringCbCopyA(aclient.cell, sizeof(aclient.cell), realm_of_cell);

	StringCbCatA(aclient.name, sizeof(aclient.name), "@");
	p = aclient.name + strlen(aclient.name);
	len = (int) min(k5creds->client->realm.length,
			 MAXKTCNAMELEN - strlen(aclient.name) - 1);
        StringCchCopyNA(p, MAXKTCNAMELEN - strlen(aclient.name),
                        k5creds->client->realm.data, len);

        ViceIDToUsername(aclient.name, realm_of_user, realm_of_cell, CellName, 
                         &aclient, &aserver, &atoken);

        rc = ktc_SetToken(&aserver, &atoken, &aclient, 0);
        if (!rc) {
            /* success */

            if (k5creds && context)
                pkrb5_free_creds(context, k5creds);

            if (context)
                pkrb5_free_context(context);
            
            return 0;
        }

        _reportf(L"SetToken returns code %d", rc);

    try_krb524d:

        _reportf(L"Trying Krb524");

        if (pkrb524_convert_creds_kdc && 
            (method == AFS_TOKEN_AUTO || method == AFS_TOKEN_KRB524)) {
            /* This requires krb524d to be running with the KDC */
            r = pkrb524_convert_creds_kdc(context, k5creds, &creds);
            if (r) {
                _reportf(L"Code %d while converting credentials", r);
                goto end_krb5;
            }
            rc = KSUCCESS;
	    got524cred = 1;
            bGotCreds = TRUE;
        }

    end_krb5:
        if (client_principal)
            pkrb5_free_principal(context, client_principal);

        if (k5creds && context)
            pkrb5_free_creds(context, k5creds);

        if (context)
            pkrb5_free_context(context);
    }

    /* Kerberos 4 */
 try_krb4:

    if (supports_krb4) {
	kcdb_identity_get_config(identity, 0, &confighandle);
	khc_read_int32(confighandle, L"Krb4Cred\\Krb4NewCreds", &supports_krb4);
	khc_close_space(confighandle); 
    }

    if (!supports_krb4)
        _reportf(L"Kerberos 4 not configured");

    if (!bGotCreds && supports_krb4 && 
        (method == AFS_TOKEN_AUTO ||
         method == AFS_TOKEN_KRB4)) {

        KTEXT_ST	ticket;

        _reportf(L"Trying Kerberos 4");

        if (!realm_of_user[0] ) {
            if ((rc = (*pkrb_get_tf_realm)((*ptkt_string)(), realm_of_user)) 
                != KSUCCESS) {
                /* can't determine realm of user */
                _reportf(L"krb_get_tf_realm returns %d", rc);
                goto end_krb4;
            }
        }

        _reportf(L"Trying to find %S.%S@%S", ServiceName, CellName, RealmName);
        rc = (*pkrb_get_cred)(ServiceName, CellName, RealmName, &creds);
        if (rc == NO_TKT_FIL) {
            // if the problem is that we have no krb4 tickets
            // do not attempt to continue
            _reportf(L"krb_get_cred returns %d (no ticket file)", rc);
            goto end_krb4;
        }

        if (rc != KSUCCESS) {
            _reportf(L"Trying to find %S@%S", ServiceName, RealmName);
            rc = (*pkrb_get_cred)(ServiceName, "", RealmName, &creds);
        }

        if (rc != KSUCCESS) {
            _reportf(L"Trying to obtain new ticket");
            if ((rc = (*pkrb_mk_req)(&ticket, ServiceName, 
                                     CellName, RealmName, 0))
                == KSUCCESS) {
                if ((rc = (*pkrb_get_cred)(ServiceName, CellName, 
                                           RealmName, &creds)) != KSUCCESS) {
                    goto end_krb4;
                } else {
                    _reportf(L"Got %S.%S@%S", ServiceName, CellName, RealmName);
                }
            } else if ((rc = (*pkrb_mk_req)(&ticket, ServiceName, 
                                            "", RealmName, 0))
                       == KSUCCESS) {
                if ((rc = (*pkrb_get_cred)(ServiceName, "", 
                                           RealmName, &creds)) != KSUCCESS) {
                    goto end_krb4;
                } else {
                    _reportf(L"Got %S@%S", ServiceName, RealmName);
                }
            } else {
                goto end_krb4;
            }
        }

        bGotCreds = TRUE;

    end_krb4:
        ;
    }

    if (bGotCreds) {

        memset(&aserver, '\0', sizeof(aserver));
        StringCchCopyA(aserver.name, MAXKTCNAMELEN, ServiceName);
        StringCchCopyA(aserver.cell, MAXKTCREALMLEN, CellName);

        memset(&atoken, '\0', sizeof(atoken));
        atoken.kvno = (short)creds.kvno;
        atoken.startTime = creds.issue_date;
        atoken.endTime = (*pkrb_life_to_time)(creds.issue_date,creds.lifetime);
        memcpy(&atoken.sessionKey, creds.session, 8);
        atoken.ticketLen = creds.ticket_st.length;
        memcpy(atoken.ticket, creds.ticket_st.dat, atoken.ticketLen);

        if (tok_expiration)
            *tok_expiration = atoken.endTime;

        if (!(rc = ktc_GetToken(&aserver, &btoken, 
                                sizeof(btoken), &aclient)) &&
            atoken.kvno == btoken.kvno &&
            atoken.ticketLen == btoken.ticketLen &&
            !memcmp(&atoken.sessionKey, &btoken.sessionKey, 
                    sizeof(atoken.sessionKey)) &&
            !memcmp(atoken.ticket, btoken.ticket, atoken.ticketLen)) {

            /* success! */
            return(0);
        }

        // Reset the "aclient" structure before we call ktc_SetToken.
        // This structure was first set by the ktc_GetToken call when
        // we were comparing whether identical tokens already existed.

        StringCchCopyA(aclient.name, MAXKTCNAMELEN, creds.pname);
        if (creds.pinst[0]) {
            StringCchCatA(aclient.name, MAXKTCNAMELEN, ".");
            StringCchCatA(aclient.name, MAXKTCNAMELEN, creds.pinst);
        }

        StringCbCopyA(aclient.instance, sizeof(aclient.instance), "");

        StringCchCatA(aclient.name, MAXKTCNAMELEN, "@");
		StringCchCatA(aclient.name, MAXKTCNAMELEN, got524cred ? realm_of_user : creds.realm);

        StringCbCopyA(aclient.cell, sizeof(aclient.cell), CellName);

        ViceIDToUsername(aclient.name, realm_of_user, realm_of_cell, CellName, 
                         &aclient, &aserver, &atoken);

        if (rc = ktc_SetToken(&aserver, &atoken, &aclient, 0)) {
            afs_report_error(rc, "ktc_SetToken()");
            return(rc);
        }
    } else if (method == AFS_TOKEN_AUTO ||
               method >= AFS_TOKEN_USER) {
        /* we couldn't get a token using Krb5, Krb524 or Krb4, either
           because we couldn't get the necessary credentials or
           because the method was set to not use those.  Now we
           dispatch to any extensions to see if they have better
           luck. */

        rc = !afs_ext_klog(method,
                           identity,
                           ServiceName,
                           CellName,
                           RealmName,
                           &ak_cellconfig,
                           LifeTime);
    } else {
        /* if the return code was not set, we should set it now.
           Otherwise we let the code go through. */
        if (!rc) {
            /* No tokens were obtained.  We should report something */
            _report_sr1(KHERR_ERROR, IDS_ERR_GENERAL,
                        _cptr(CellName));
            _resolve();

            rc = KHM_ERROR_GENERAL;
        }
    }

    return rc;
}

/**************************************/
/* afs_realm_of_cell():               */
/**************************************/
static char *
afs_realm_of_cell(afs_conf_cell *cellconfig, BOOL referral_fallback)
{
    char krbhst[MAX_HSTNM]="";
    static char krbrlm[REALM_SZ+1]="";
    krb5_context  ctx = 0;
    char ** realmlist=NULL;
    krb5_error_code r = 0;

    if (!cellconfig)
        return 0;

    if (referral_fallback) {
	char * p;
	p = strchr(cellconfig->hostName[0], '.');
	if (p++)
	    StringCbCopyA(krbrlm, sizeof(krbrlm), p);
	else
	    StringCbCopyA(krbrlm, sizeof(krbrlm), cellconfig->name);
#if _MSC_VER >= 1400
        _strupr_s(krbrlm, sizeof(krbrlm));
#else
        _strupr(krbrlm);
#endif
    } else {
	if ( pkrb5_init_context ) {
	    r = pkrb5_init_context(&ctx); 
	    if ( !r )
		r = pkrb5_get_host_realm(ctx, cellconfig->hostName[0], &realmlist);
	    if ( !r && realmlist && realmlist[0] ) {
		StringCbCopyA(krbrlm, sizeof(krbrlm), realmlist[0]);
		pkrb5_free_host_realm(ctx, realmlist);
	    }
	    if (ctx)
		pkrb5_free_context(ctx);
	}

	if (r) {
	    if (pkrb_get_krbhst && pkrb_realmofhost) {
		StringCbCopyA(krbrlm, sizeof(krbrlm), 
			       (char *)(*pkrb_realmofhost)(cellconfig->hostName[0]));
		if ((*pkrb_get_krbhst)(krbhst, krbrlm, 1) != KSUCCESS)
		    krbrlm[0] = '\0';
	    }

	    if ( !krbrlm[0] ) {
		char * p;
		p = strchr(cellconfig->hostName[0], '.');
		if (p++)
		    StringCbCopyA(krbrlm, sizeof(krbrlm), p);
		else
		    StringCbCopyA(krbrlm, sizeof(krbrlm), cellconfig->name);
#if _MSC_VER >= 1400
		_strupr_s(krbrlm, sizeof(krbrlm));
#else
                _strupr(krbrlm);
#endif
	    }
	}
    }
    return(krbrlm);
}

/**************************************/
/* afs_get_cellconfig():                  */
/**************************************/
static int 
afs_get_cellconfig(char *cell, afs_conf_cell *cellconfig, char *local_cell)
{
    int	rc;
    int ttl;

    local_cell[0] = (char)0;
    memset(cellconfig, 0, sizeof(*cellconfig));

    cellconfig->cbsize = sizeof(*cellconfig);

    /* WIN32: cm_GetRootCellName(local_cell) - NOTE: no way to get max chars */
    if (rc = cm_GetRootCellName(local_cell)) {
        return(rc);
    }

    if (strlen(cell) == 0)
        StringCbCopyA(cell, (MAXCELLCHARS+1) * sizeof(char), local_cell);

    /* WIN32: cm_SearchCellFile(cell, pcallback, pdata) */
    StringCbCopyA(cellconfig->name, (MAXCELLCHARS+1) * sizeof(char), cell);

    rc = cm_SearchCellFile(cell, NULL, afs_get_cellconfig_callback, 
                           (void*)cellconfig);
    if(rc)
        rc = cm_SearchCellByDNS(cell, NULL, &ttl, 
                                afs_get_cellconfig_callback, 
                                (void*) cellconfig);

    return rc;
}

/**************************************/
/* afs_get_cellconfig_callback():          */
/**************************************/
static long 
afs_get_cellconfig_callback(void *cellconfig, 
                            struct sockaddr_in *addrp, 
                            char *namep)
{
    afs_conf_cell *cc = (afs_conf_cell *)cellconfig;

    cc->hostAddr[cc->numServers] = *addrp;
    StringCbCopyA(cc->hostName[cc->numServers], 
                  sizeof(cc->hostName[0]), namep);
    cc->numServers++;
    return(0);
}


/**************************************/
/* afs_report_error():           */
/**************************************/
void
afs_report_error(LONG rc, LPCSTR FailedFunctionName)
{
    char message[256];
    const char *errText; 

    // Using AFS defines as error messages for now, until Transarc 
    // gets back to me with "string" translations of each of these 
    // const. defines. 
    if (rc == KTC_ERROR)
        errText = "KTC_ERROR";
    else if (rc == KTC_TOOBIG)
        errText = "KTC_TOOBIG";
    else if (rc == KTC_INVAL)
        errText = "KTC_INVAL";
    else if (rc == KTC_NOENT)
        errText = "KTC_NOENT";
    else if (rc == KTC_PIOCTLFAIL)
        errText = "KTC_PIOCTLFAIL";
    else if (rc == KTC_NOPIOCTL)
        errText = "KTC_NOPIOCTL";
    else if (rc == KTC_NOCELL)
        errText = "KTC_NOCELL";
    else if (rc == KTC_NOCM)
        errText = "KTC_NOCM: The service, Transarc AFS Daemon, most likely is not started!";
    else
        errText = "Unknown error!";

    StringCbPrintfA(message, sizeof(message), 
                    "%s\n(%s failed)", errText, FailedFunctionName);
    _report_cs1(KHERR_ERROR, L"%1!S!", _cptr(message));
    _resolve();
    return;
}

DWORD 
GetServiceStatus(LPSTR lpszMachineName, 
                 LPSTR lpszServiceName, 
                 DWORD *lpdwCurrentState,
                 DWORD *lpdwWaitHint) 
{ 
    DWORD           hr               = NOERROR; 
    SC_HANDLE       schSCManager     = NULL; 
    SC_HANDLE       schService       = NULL; 
    DWORD           fdwDesiredAccess = 0; 
    SERVICE_STATUS  ssServiceStatus  = {0}; 
    BOOL            fRet             = FALSE; 

    *lpdwCurrentState = 0; 
 
    fdwDesiredAccess = GENERIC_READ; 
 
    schSCManager = OpenSCManagerA(lpszMachineName,  
                                  NULL,
                                  fdwDesiredAccess); 
 
    if(schSCManager == NULL) { 
        hr = GetLastError();
        goto cleanup; 
    } 
 
    schService = OpenServiceA(schSCManager,
                              lpszServiceName,
                              fdwDesiredAccess);
 
    if(schService == NULL) { 
        hr = GetLastError();
        goto cleanup; 
    } 
 
    fRet = QueryServiceStatus(schService,
                              &ssServiceStatus); 
 
    if(fRet == FALSE) { 
        hr = GetLastError(); 
        goto cleanup; 
    } 
 
    *lpdwCurrentState = ssServiceStatus.dwCurrentState; 
    if (lpdwWaitHint)
        *lpdwWaitHint = ssServiceStatus.dwWaitHint;
cleanup: 
 
    CloseServiceHandle(schService); 
    CloseServiceHandle(schSCManager); 
 
    return(hr); 
} 

DWORD ServiceControl(LPSTR lpszMachineName, 
                     LPSTR lpszServiceName,
                     DWORD dwNewState) {

    DWORD           hr               = NOERROR; 
    SC_HANDLE       schSCManager     = NULL; 
    SC_HANDLE       schService       = NULL; 
    DWORD           fdwDesiredAccess = 0; 
    SERVICE_STATUS  ssServiceStatus  = {0}; 
    BOOL            fRet             = FALSE; 
    DWORD           dwCurrentState   = 0;

    dwCurrentState = 0; 
 
    fdwDesiredAccess = GENERIC_READ;
 
    schSCManager = OpenSCManagerA(lpszMachineName, NULL, 
                                  fdwDesiredAccess); 
 
    if(schSCManager == NULL) {
        hr = GetLastError();
        goto cleanup; 
    }

    fdwDesiredAccess = GENERIC_READ | GENERIC_EXECUTE;

    schService = OpenServiceA(schSCManager, lpszServiceName,
                              fdwDesiredAccess);
 
    if(schService == NULL) {
        hr = GetLastError();
        goto cleanup; 
    } 
 
    fRet = QueryServiceStatus(schService, &ssServiceStatus);
 
    if(fRet == FALSE) {
        hr = GetLastError(); 
        goto cleanup; 
    } 
 
    dwCurrentState = ssServiceStatus.dwCurrentState; 

    if (dwCurrentState == SERVICE_STOPPED &&
        dwNewState == SERVICE_RUNNING) {

        fRet = StartService(schService, 0, NULL);

        if (fRet == FALSE) {
            hr = GetLastError();
            goto cleanup;
        }
    }

    if (dwCurrentState == SERVICE_RUNNING &&
        dwNewState == SERVICE_STOPPED) {
        fRet = ControlService(schService, SERVICE_CONTROL_STOP, 
                              &ssServiceStatus);

        if (fRet == FALSE) {
            hr = GetLastError();
            goto cleanup;
        }
    }
 
cleanup: 
 
    CloseServiceHandle(schService); 
    CloseServiceHandle(schSCManager); 
 
    return(hr); 
}

khm_boolean
afs_check_for_cell_realm_match(khm_handle identity, char * cell) {
    char local_cell[MAXCELLCHARS];
    wchar_t wrealm[MAXCELLCHARS];
    wchar_t idname[KCDB_IDENT_MAXCCH_NAME];
    wchar_t * atsign;
    khm_size cb;
    char * realm;
    afs_conf_cell cellconfig;
    int rc;

    ZeroMemory(local_cell, sizeof(local_cell));

    rc = afs_get_cellconfig(cell, &cellconfig, local_cell);
    if (rc)
        return FALSE;

    realm = afs_realm_of_cell(&cellconfig, FALSE);
    if (realm == NULL)
        return FALSE;

    AnsiStrToUnicode(wrealm, sizeof(wrealm), realm);

    cb = sizeof(idname);
    idname[0] = L'\0';
    kcdb_identity_get_name(identity, idname, &cb);

    atsign = wcschr(idname, L'@');
    if (atsign && atsign[1] && !_wcsicmp(atsign + 1, wrealm)) {
        return TRUE;
    } else {
        return FALSE;
    }
}
