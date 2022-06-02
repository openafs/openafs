/*
 * Copyright (c) 2004, 2005, 2006, 2007, 2008 Secure Endpoints Inc.
 * Copyright (c) 2003 SkyRope, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of Skyrope, LLC nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission from Skyrope, LLC.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this code are derived from portions of the MIT
 * Leash Ticket Manager and LoadFuncs utilities.  For these portions the
 * following copyright applies.
 *
 * Copyright (c) 2003,2004 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 */

#undef  USE_KRB4
#undef  USE_KRB524
#define USE_MS2MIT 1

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>
#include <roken.h>

#include <osilog.h>
#include <afs/ptserver.h>
#include <afs/ptuser.h>
#include <afs/auth.h>
#include <afs/com_err.h>
#include <rx/rxkad.h>
#include <WINNT\afsreg.h>
#include "cm.h"

#include "afskfw.h"
#include "afskfw-int.h"
#include <userenv.h>
#include <strsafe.h>

#include <Sddl.h>
#include <Aclapi.h>

#include <krbcompat_delayload.h>

#ifndef KRB5_TC_OPENCLOSE
#define KRB5_TC_OPENCLOSE 0x00000001
#endif

/*
 * TIMING _____________________________________________________________________
 *
 */

#define cminREMIND_TEST      1    // test every minute for expired creds
#define cminREMIND_WARN      15   // warn if creds expire in 15 minutes
#define cminRENEW            20   // renew creds when there are 20 minutes remaining
#define cminMINLIFE          30   // minimum life of Kerberos creds

#define c100ns1SECOND        (LONGLONG)10000000
#define cmsec1SECOND         1000
#define cmsec1MINUTE         60000
#define csec1MINUTE          60

/* Static Prototypes */
char *afs_realm_of_cell(krb5_context, struct afsconf_cell *);
static long get_cellconfig_callback(void *, struct sockaddr_in *, char *, unsigned short);
int KFW_AFS_get_cellconfig(char *, struct afsconf_cell *, char *);
static krb5_error_code KRB5_CALLCONV KRB5_prompter( krb5_context context,
           void *data, const char *name, const char *banner, int num_prompts,
           krb5_prompt prompts[]);

/* Static Declarations */
static int                inited = 0;
static int                mid_cnt = 0;
static struct textField * mid_tb = NULL;
static struct principal_ccache_data * princ_cc_data = NULL;
static struct cell_principal_map    * cell_princ_map = NULL;

#ifdef USE_LEASH
#define DEFAULT_LIFETIME pLeash_get_default_lifetime()
#else
#define DEFAULT_LIFETIME (24 * 60)
#endif

void
DebugPrintf(const char * fmt, ...)
{
    if (IsDebuggerPresent()) {
        va_list vl;
        char buf[1024];

        va_start(vl, fmt);
        StringCbVPrintfA(buf, sizeof(buf), fmt, vl);
        OutputDebugStringA(buf);
        va_end(vl);
    }
}

void
KFW_initialize(void)
{
    static int inited = 0;

    if ( !inited ) {
        char mutexName[MAX_PATH];
        HANDLE hMutex = NULL;

        StringCbPrintf( mutexName, sizeof(mutexName), "AFS KFW Init pid=%d", getpid());

        hMutex = CreateMutex( NULL, TRUE, mutexName );
        if ( GetLastError() == ERROR_ALREADY_EXISTS ) {
            if ( WaitForSingleObject( hMutex, INFINITE ) != WAIT_OBJECT_0 ) {
                return;
            }
        }
        if ( !inited ) {
            inited = 1;

            DelayLoadHeimdal();

            if ( KFW_is_available() ) {
                char rootcell[CELL_MAXNAMELEN+1];

                KFW_enable_DES(NULL);
#ifdef USE_MS2MIT
                KFW_import_windows_lsa();
#endif /* USE_MS2MIT */
                KFW_import_ccache_data();
                KFW_AFS_renew_expiring_tokens();

                /* WIN32 NOTE: no way to get max chars */
                if (!cm_GetRootCellName(rootcell))
                    KFW_AFS_renew_token_for_cell(rootcell);
            }
        }
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);

        initialize_KTC_error_table();
        initialize_PT_error_table();
    }
}

void
KFW_cleanup(void)
{
}

typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
static int IsWow64()
{
    static int init = TRUE;
    static int bIsWow64 = FALSE;

    if (init) {
        HMODULE hModule;
        LPFN_ISWOW64PROCESS fnIsWow64Process = NULL;

        hModule = GetModuleHandle(TEXT("kernel32"));
        if (hModule) {
            fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(hModule, "IsWow64Process");

            if (NULL != fnIsWow64Process)
            {
                if (!fnIsWow64Process(GetCurrentProcess(),&bIsWow64))
                {
                    // on error, assume FALSE.
                    // in other words, do nothing.
                }
            }
            FreeLibrary(hModule);
        }
        init = FALSE;
    }
    return bIsWow64;
}

int
KFW_accept_dotted_usernames(void)
{
    HKEY parmKey;
    DWORD code, len;
    DWORD value = 1;

    code = RegOpenKeyEx(HKEY_CURRENT_USER, AFSREG_USER_OPENAFS_SUBKEY,
                        0, (IsWow64()?KEY_WOW64_64KEY:0)|KEY_QUERY_VALUE, &parmKey);
    if (code == ERROR_SUCCESS) {
        len = sizeof(value);
        code = RegQueryValueEx(parmKey, "AcceptDottedPrincipalNames", NULL, NULL,
                               (BYTE *) &value, &len);
        RegCloseKey(parmKey);
    }
    if (code != ERROR_SUCCESS) {
        code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_OPENAFS_SUBKEY,
                            0, (IsWow64()?KEY_WOW64_64KEY:0)|KEY_QUERY_VALUE, &parmKey);
        if (code == ERROR_SUCCESS) {
            len = sizeof(value);
            code = RegQueryValueEx(parmKey, "AcceptDottedPrincipalNames", NULL, NULL,
                                   (BYTE *) &value, &len);
            RegCloseKey (parmKey);
        }
    }
    return value;
}


int
KFW_use_krb524(void)
{
    return 0;
}

int
KFW_is_available(void)
{
    HKEY parmKey;
    DWORD code, len;
    DWORD enableKFW = 1;

    code = RegOpenKeyEx(HKEY_CURRENT_USER, AFSREG_USER_OPENAFS_SUBKEY,
                        0, (IsWow64()?KEY_WOW64_64KEY:0)|KEY_QUERY_VALUE, &parmKey);
    if (code == ERROR_SUCCESS) {
        len = sizeof(enableKFW);
        code = RegQueryValueEx(parmKey, "EnableKFW", NULL, NULL,
                               (BYTE *) &enableKFW, &len);
        RegCloseKey (parmKey);
    }

    if (code != ERROR_SUCCESS) {
        code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_OPENAFS_SUBKEY,
                            0, (IsWow64()?KEY_WOW64_64KEY:0)|KEY_QUERY_VALUE, &parmKey);
        if (code == ERROR_SUCCESS) {
            len = sizeof(enableKFW);
            code = RegQueryValueEx(parmKey, "EnableKFW", NULL, NULL,
                                   (BYTE *) &enableKFW, &len);
            RegCloseKey (parmKey);
        }
    }

    if ( !enableKFW )
        return FALSE;

    KFW_initialize();

    /* If this is non-zero, then some Kerberos library was loaded. */
    return (krb5_init_context != NULL);
}

int
KRB5_error(krb5_error_code rc, LPCSTR FailedFunctionName,
           int FreeContextFlag, krb5_context * context,
           krb5_ccache * cache)
{
    char message[256];
    const char *errText;
    int krb5Error = ((int)(rc & 255));

    errText = krb5_get_error_message(*context, rc);
    StringCbPrintf(message, sizeof(message),
                   "%s\n(Kerberos error %ld)\n\n%s failed",
                   errText,
                   krb5Error,
                   FailedFunctionName);
    krb5_free_error_message(*context, (char *)errText);

    DebugPrintf("%s", message);

    MessageBox(NULL, message, "Kerberos Five", MB_OK | MB_ICONERROR |
               MB_TASKMODAL |
               MB_SETFOREGROUND);

    if (FreeContextFlag == 1) {

        if (context && *context != NULL) {

            if (cache && *cache != NULL) {
                krb5_cc_close(*context, *cache);
                *cache = NULL;
            }

            krb5_free_context(*context);
            *context = NULL;
        }
    }

    return rc;
}

void
KFW_AFS_update_princ_ccache_data(krb5_context context, krb5_ccache cc, int lsa)
{
    struct principal_ccache_data * next = princ_cc_data;
    krb5_principal principal = 0;
    char * pname = NULL;
    const char * ccname = NULL;
    const char * cctype = NULL;
    char * ccfullname = NULL;
    krb5_error_code code = 0;
    krb5_error_code cc_code = 0;
    krb5_cc_cursor cur;
    krb5_creds creds;
    krb5_flags flags=0;
    krb5_timestamp now;
    size_t len;

    if (context == 0 || cc == 0)
        return;

    code = krb5_cc_get_principal(context, cc, &principal);
    if ( code ) return;

    code = krb5_unparse_name(context, principal, &pname);
    if ( code ) goto cleanup;

    ccname = krb5_cc_get_name(context, cc);
    if (!ccname) goto cleanup;

    cctype = krb5_cc_get_type(context, cc);
    if (!cctype) goto cleanup;

    len = strlen(ccname) + strlen(cctype) + 2;
    ccfullname = malloc(len);
    if (!ccfullname) goto cleanup;

    StringCbPrintf(ccfullname, len, "%s:%s", cctype, ccname);

    // Search the existing list to see if we have a match
    if ( next ) {
        for ( ; next ; next = next->next ) {
            if ( !strcmp(next->principal,pname) && !strcmp(next->ccache_name, ccfullname) )
                break;
        }
    }

    // If not, match add a new node to the beginning of the list and assign init it
    if ( !next ) {
        next = (struct principal_ccache_data *) malloc(sizeof(struct principal_ccache_data));
        next->next = princ_cc_data;
        princ_cc_data = next;
        next->principal = strdup(pname);
        next->ccache_name = ccfullname;
        ccfullname = NULL;
        next->from_lsa = lsa;
        next->expired = 1;
        next->expiration_time = 0;
        next->renew = 0;
    }

    flags = 0;  // turn off OPENCLOSE mode
    code = krb5_cc_set_flags(context, cc, flags);
    if ( code ) goto cleanup;

    code = krb5_timeofday(context, &now);

    cc_code = krb5_cc_start_seq_get(context, cc, &cur);

    while (!(cc_code = krb5_cc_next_cred(context, cc, &cur, &creds))) {
        if ( creds.flags.b.initial) {
            int valid;
            // we found the ticket we are looking for
            // check validity of timestamp
            // We add a 5 minutes fudge factor to compensate for potential
            // clock skew errors between the KDC and client OS

            valid = ((creds.times.starttime > 0) &&
                     now >= (creds.times.starttime - 300) &&
                     now < (creds.times.endtime + 300) &&
                     !creds.flags.b.invalid);

            if ( next->from_lsa) {
                next->expired = 0;
                next->expiration_time = creds.times.endtime;
                next->renew = 1;
            } else if ( valid ) {
                next->expired = 0;
                next->expiration_time = creds.times.endtime;
                next->renew = (creds.times.renew_till > creds.times.endtime) &&
                    creds.flags.b.renewable;
            } else {
                next->expired = 1;
                next->expiration_time = 0;
                next->renew = 0;
            }

            krb5_free_cred_contents(context, &creds);
            cc_code = KRB5_CC_END;
            break;
        }
        krb5_free_cred_contents(context, &creds);
    }

    if (cc_code == KRB5_CC_END) {
        code = krb5_cc_end_seq_get(context, cc, &cur);
        if (code) goto cleanup;
    }

  cleanup:
    flags = KRB5_TC_OPENCLOSE;  //turn on OPENCLOSE
    code = krb5_cc_set_flags(context, cc, flags);

    if ( ccfullname)
        free(ccfullname);
    if ( pname )
        krb5_free_unparsed_name(context,pname);
    if ( principal )
        krb5_free_principal(context,principal);
}

int
KFW_AFS_find_ccache_for_principal(krb5_context context, char * principal,
                                  char **ccache, int valid_only)
{
    struct principal_ccache_data * next = princ_cc_data;
    char * response = NULL;

    if ( !principal || !ccache )
        return 0;

    while ( next ) {
        if ( (!valid_only || !next->expired) && !strcmp(next->principal, principal) ) {
            if (response) {
                // we always want to prefer the MS Kerberos LSA cache or
                // the cache afscreds created specifically for the principal
                // if the current entry is either one, drop the previous find
                if ( next->from_lsa || !strcmp(next->ccache_name, principal) )
                    free(response);
            }
            response = strdup(next->ccache_name);
            // MS Kerberos LSA is our best option so use it and quit
            if ( next->from_lsa )
                break;
        }
        next = next->next;
    }

    if ( response ) {
        *ccache = response;
        return 1;
    }
    return 0;
}

void
KFW_AFS_delete_princ_ccache_data(krb5_context context, char * pname, char * ccname)
{
    struct principal_ccache_data ** next = &princ_cc_data;

    if ( !pname && !ccname )
        return;

    while ( (*next) ) {
        if ( !strcmp((*next)->principal,pname) ||
             !strcmp((*next)->ccache_name,ccname) ) {
            void * temp;
            free((*next)->principal);
            free((*next)->ccache_name);
            temp = (*next);
            (*next) = (*next)->next;
            free(temp);
        }
    }
}

void
KFW_AFS_update_cell_princ_map(krb5_context context, char * cell, char *pname, int active)
{
    struct cell_principal_map * next = cell_princ_map;

    // Search the existing list to see if we have a match
    if ( next ) {
        for ( ; next ; next = next->next ) {
            if ( !strcmp(next->cell, cell) ) {
                if ( !strcmp(next->principal,pname) ) {
                    next->active = active;
					break;
                } else {
                    // OpenAFS currently has a restriction of one active token per cell
                    // Therefore, whenever we update the table with a new active cell we
                    // must mark all of the other principal to cell entries as inactive.
                    if (active)
                        next->active = 0;
                }
            }
        }
    }

    // If not, match add a new node to the beginning of the list and assign init it
    if ( !next ) {
        next = (struct cell_principal_map *) malloc(sizeof(struct cell_principal_map));
        next->next = cell_princ_map;
        cell_princ_map = next;
        next->principal = strdup(pname);
        next->cell = strdup(cell);
        next->active = active;
    }
}

void
KFW_AFS_delete_cell_princ_maps(krb5_context context, char * pname, char * cell)
{
    struct cell_principal_map ** next = &cell_princ_map;

    if ( !pname && !cell )
        return;

    while ( (*next) ) {
        if ( !strcmp((*next)->principal,pname) ||
             !strcmp((*next)->cell,cell) ) {
            void * temp;
            free((*next)->principal);
            free((*next)->cell);
            temp = (*next);
            (*next) = (*next)->next;
            free(temp);
        }
    }
}

// Returns (if possible) a principal which has been known in
// the past to have been used to obtain tokens for the specified
// cell.
// TODO: Attempt to return one which has not yet expired by checking
// the principal/ccache data
int
KFW_AFS_find_principals_for_cell(krb5_context context, char * cell, char **principals[], int active_only)
{
    struct cell_principal_map * next_map = cell_princ_map;
    const char * princ = NULL;
    int count = 0, i;

    if ( !cell )
        return 0;

    while ( next_map ) {
        if ( (!active_only || next_map->active) && !strcmp(next_map->cell,cell) ) {
            count++;
        }
        next_map = next_map->next;
    }

    if ( !principals || !count )
        return count;

    *principals = (char **) malloc(sizeof(char *) * count);
    for ( next_map = cell_princ_map, i=0 ; next_map && i<count; next_map = next_map->next )
    {
        if ( (!active_only || next_map->active) && !strcmp(next_map->cell,cell) ) {
            (*principals)[i++] = strdup(next_map->principal);
        }
    }
    return count;
}

int
KFW_AFS_find_cells_for_princ(krb5_context context, char * pname, char **cells[], int active_only)
{
    int     count = 0, i;
    struct cell_principal_map * next_map = cell_princ_map;
    const char * princ = NULL;

    if ( !pname )
        return 0;

    while ( next_map ) {
        if ( (!active_only || next_map->active) && !strcmp(next_map->principal,pname) ) {
            count++;
        }
        next_map = next_map->next;
    }

    if ( !cells )
        return count;

    *cells = (char **) malloc(sizeof(char *) * count);
    for ( next_map = cell_princ_map, i=0 ; next_map && i<count; next_map = next_map->next )
    {
        if ( (!active_only || next_map->active) && !strcmp(next_map->principal,pname) ) {
            (*cells)[i++] = strdup(next_map->cell);
        }
    }
    return count;
}

static void
escape_unsafe_principal_characters(const char * pname,
                                   char ** new_name)
{
    const char * src;
    char * dest;
    size_t len = 0;

    /* Count first */
    for (src = pname; *src != '\0'; ++len, ++src) {
        if (*src == '\\' || *src == '#' || *src == '<' ||
            *src == '>' || *src == ':' || *src == '"' ||
            *src == '/' || *src == '|' || *src == '?' ||
            *src == '*')
            ++len;
    }

    ++len;

    *new_name = (char *) malloc(len);

    if (*new_name == NULL)
        return;

    for (src = pname, dest = *new_name; *src != '\0'; ++src) {
        switch (*src) {
        case '\\': *dest++ = '#'; *dest++ = 'b'; break;

        case '#' : *dest++ = '#'; *dest++ = '#'; break;

        case '<' : *dest++ = '#'; *dest++ = 'l'; break;

        case '>' : *dest++ = '#'; *dest++ = 'g'; break;

        case ':' : *dest++ = '#'; *dest++ = 'c'; break;

        case '"' : *dest++ = '#'; *dest++ = 't'; break;

        case '/' : *dest++ = '#'; *dest++ = 'f'; break;

        case '|' : *dest++ = '#'; *dest++ = 'p'; break;

        case '?' : *dest++ = '#'; *dest++ = 'q'; break;

        case '*' : *dest++ = '#'; *dest++ = 'a'; break;

        default: *dest++ = *src;
        }
    }

    *dest++ = '\0';
}

static void
get_default_ccache_name_for_principal(krb5_context context, krb5_principal principal,
                                      char ** cc_name)
{
    char * pname = NULL;
    krb5_error_code code;
    size_t len = 0;

    *cc_name = NULL;

    code = krb5_unparse_name(context, principal, &pname);
    if (code) goto cleanup;

    len = strlen(pname) + 5;
    *cc_name = (char *) malloc(len);

    StringCbPrintfA(*cc_name, len, "API:%s", pname, GetCurrentThreadId());

cleanup:
    if (pname)
        krb5_free_unparsed_name(context, pname);

    return;
}

static int
is_default_ccache_for_principal(krb5_context context, krb5_principal principal,
                                krb5_ccache cc)
{
    const char * cc_name;
    char * def_cc_name = NULL;
    const char *bs_cc;
    const char *bs_def_cc;
    int is_default;

    cc_name = krb5_cc_get_name(context, cc);

    get_default_ccache_name_for_principal(context, principal, &def_cc_name);

    is_default = (cc_name != NULL && def_cc_name != NULL &&

                  (bs_cc = strrchr(cc_name, '\\')) != NULL &&

                  (bs_def_cc = strrchr(def_cc_name, '\\')) != NULL &&

                  !strcmp(bs_cc, bs_def_cc));

    if (def_cc_name)
        free(def_cc_name);

    return is_default;
}

/** Given a principal return an existing ccache or create one and return */
int
KFW_get_ccache(krb5_context alt_context, krb5_principal principal, krb5_ccache * cc)
{
    krb5_context context = NULL;
    char * pname = NULL;
    char * ccname = NULL;
    krb5_error_code code;

    if ( alt_context ) {
        context = alt_context;
    } else {
        code = krb5_init_context(&context);
        if (code) goto cleanup;
    }

    if ( principal ) {
        code = krb5_unparse_name(context, principal, &pname);
        if (code) goto cleanup;

        if ( !KFW_AFS_find_ccache_for_principal(context,pname,&ccname,TRUE) &&
             !KFW_AFS_find_ccache_for_principal(context,pname,&ccname,FALSE)) {

            get_default_ccache_name_for_principal(context, principal, &ccname);
        }
        code = krb5_cc_resolve(context, ccname, cc);
    } else {
        code = krb5_cc_default(context, cc);
        if (code) goto cleanup;
    }

  cleanup:
    if (ccname)
        free(ccname);
    if (pname)
        krb5_free_unparsed_name(context,pname);
    if (context && (context != alt_context))
        krb5_free_context(context);
    return(code);
}

#ifdef USE_MS2MIT

// Import Microsoft Credentials into a new MIT ccache
void
KFW_import_windows_lsa(void)
{
    krb5_context context = NULL;
    krb5_ccache  cc = NULL;
    krb5_principal princ = NULL;
    char * pname = NULL;
    const char *  princ_realm;
    krb5_error_code code;
    char cell[128]="", realm[128]="", *def_realm = 0;
    DWORD dwMsLsaImport = 1;

    code = krb5_init_context(&context);
    if (code) goto cleanup;

    code = krb5_cc_resolve(context, LSA_CCNAME, &cc);
    if (code) goto cleanup;

    KFW_AFS_update_princ_ccache_data(context, cc, TRUE);

    code = krb5_cc_get_principal(context, cc, &princ);
    if ( code ) goto cleanup;

    dwMsLsaImport = KFW_get_default_mslsa_import(context);
    switch ( dwMsLsaImport ) {
    case 0: /* do not import */
        goto cleanup;
    case 1: /* always import */
        break;
    case 2: { /* matching realm */
        const char *ms_realm;

        ms_realm = krb5_principal_get_realm(context, princ);

        if (code = krb5_get_default_realm(context, &def_realm))
            goto cleanup;

        if (strcmp(def_realm, ms_realm))
            goto cleanup;
        break;
    }
    default:
        break;
    }

    code = krb5_unparse_name(context,princ,&pname);
    if ( code ) goto cleanup;

    princ_realm = krb5_principal_get_realm(context, princ);
    StringCchCopyA(realm, sizeof(realm), princ_realm);
    StringCchCopyA(cell, sizeof(cell), princ_realm);
    strlwr(cell);

    code = KFW_AFS_klog(context, cc, "afs", cell, realm,
                        KFW_get_default_lifetime(context, realm), NULL);

    DebugPrintf("KFW_AFS_klog() returns: %d\n", code);

    if ( code ) goto cleanup;

    KFW_AFS_update_cell_princ_map(context, cell, pname, TRUE);

  cleanup:
    if (pname)
        krb5_free_unparsed_name(context,pname);
    if (princ)
        krb5_free_principal(context,princ);
    if (def_realm)
        krb5_free_default_realm(context, def_realm);
    if (cc)
        krb5_cc_close(context,cc);
    if (context)
        krb5_free_context(context);
}
#endif /* USE_MS2MIT */

static krb5_boolean
get_canonical_ccache(krb5_context context, krb5_ccache * pcc)
{
    krb5_error_code code;
    krb5_ccache cc = *pcc;
    krb5_principal principal = 0;

    code = krb5_cc_get_principal(context, cc, &principal);
    if (code)
        return FALSE;

    if ( !is_default_ccache_for_principal(context, principal, cc)
         && strcmp(krb5_cc_get_type(context, cc), LSA_CCTYPE) != 0) {

        char * def_cc_name = NULL;
        krb5_ccache def_cc = 0;
        krb5_principal def_cc_princ = 0;

        do {
            get_default_ccache_name_for_principal(context, principal, &def_cc_name);

            code = krb5_cc_resolve(context, def_cc_name, &def_cc);
            if (code) break;

            code = krb5_cc_get_principal(context, def_cc, &def_cc_princ);
            if (code || !krb5_principal_compare(context, def_cc_princ, principal)) {
                /* def_cc either doesn't exist or is home to an
                 * imposter. */

                DebugPrintf("Copying ccache [%s:%s]->[%s:%s]",
                            krb5_cc_get_type(context, cc), krb5_cc_get_name(context, cc),
                            krb5_cc_get_type(context, def_cc),
                            krb5_cc_get_name(context, def_cc));

                code = krb5_cc_initialize(context, def_cc, principal);
                if (code) break;

                code = krb5_cc_copy_creds(context, cc, def_cc);
                if (code) {
                    KRB5_error(code, "krb5_cc_copy_creds", 0, NULL, NULL);
                    break;
                }

                code = krb5_cc_close(context, cc);

                cc = def_cc;
                def_cc = 0;
            }
        } while (FALSE);

        if (def_cc)
            krb5_cc_close(context, def_cc);

        if (def_cc_princ)
            krb5_free_principal(context, def_cc_princ);

        if (def_cc_name)
            free(def_cc_name);
    }

    if (principal)
        krb5_free_principal(context, principal);

    if (code == 0 && cc != 0) {
        *pcc = cc;
        return TRUE;
    }

    *pcc = cc;
    return FALSE;
}

static krb5_error_code
check_and_get_tokens_for_ccache(krb5_context context, krb5_ccache cc)
{
    krb5_error_code code = 0;
    krb5_error_code cc_code = 0;
    krb5_cc_cursor  cur;
    krb5_creds      creds;
    char * principal_name = NULL;

    {
        krb5_principal principal = 0;
        code = krb5_cc_get_principal(context, cc, &principal);

        if (code == 0)
            code = krb5_unparse_name(context, principal, &principal_name);

        if (principal)
            krb5_free_principal(context, principal);
    }

    if (code != 0) {
        if (principal_name)
            krb5_free_unparsed_name(context, principal_name);
        return code;
    }

    cc_code = krb5_cc_start_seq_get(context, cc, &cur);

    while (!(cc_code = krb5_cc_next_cred(context, cc, &cur, &creds))) {

        const char * sname = krb5_principal_get_comp_string(context, creds.server, 0);
        const char * cell  = krb5_principal_get_comp_string(context, creds.server, 1);
        const char * realm = krb5_principal_get_realm(context, creds.server);

        if ( sname && cell && !strcmp("afs",sname) ) {

            struct ktc_principal    aserver;
            struct ktc_principal    aclient;
            struct ktc_token	    atoken;
            int active = TRUE;

            DebugPrintf("Found AFS ticket: %s%s%s@%s\n",
                        sname, (cell ? "/":""), (cell? cell : ""), realm);

            memset(&aserver, '\0', sizeof(aserver));
            StringCbCopy(aserver.name, sizeof(aserver.name), sname);
            StringCbCopy(aserver.cell, sizeof(aserver.cell), cell);

            code = ktc_GetToken(&aserver, &atoken, sizeof(atoken), &aclient);
            if (!code) {
                // Found a token in AFS Client Server which matches

                char pname[128], *p, *q;

                for ( p=pname, q=aclient.name; *q; p++, q++)
                    *p = *q;

                for ( *p++ = '@', q=aclient.cell; *q; p++, q++)
                    *p = toupper(*q);

                *p = '\0';

                DebugPrintf("Found AFS token: %s\n", pname);

                if (strcmp(pname, principal_name) != 0)
                    active = FALSE;

                KFW_AFS_update_cell_princ_map(context, cell, principal_name, active);

            } else {
                // Attempt to import it

                KFW_AFS_update_cell_princ_map(context, cell, principal_name, active);

                DebugPrintf("Calling KFW_AFS_klog() to obtain token\n");

                code = KFW_AFS_klog(context, cc, "afs", cell, realm,
                                    KFW_get_default_lifetime(context, realm), NULL);

                DebugPrintf("KFW_AFS_klog() returns: %d\n", code);
            }

        } else {

            DebugPrintf("Found ticket: %s%s%s@%s\n", sname,
                        (cell? "/":""), (cell? cell:""), realm);
        }

        krb5_free_cred_contents(context, &creds);
    }

    if (cc_code == KRB5_CC_END) {
        cc_code = krb5_cc_end_seq_get(context, cc, &cur);
    }

    return code;
}

// If there are existing MIT credentials, copy them to a new
// ccache named after the principal

// Enumerate all existing MIT ccaches and construct entries
// in the principal_ccache table

// Enumerate all existing AFS Tokens and construct entries
// in the cell_principal table
void
KFW_import_ccache_data(void)
{
    krb5_context context = NULL;
    krb5_ccache  cc;
    krb5_error_code code;
    krb5_cccol_cursor cccol_cur;
    int flags;

    if ( IsDebuggerPresent() )
        OutputDebugString("KFW_import_ccache_data()\n");

    code = krb5_init_context(&context);
    if (code) goto cleanup;

    code = krb5_cccol_cursor_new(context, &cccol_cur);
    if (code) goto cleanup;

    while ((code = krb5_cccol_cursor_next(context, cccol_cur, &cc)) == 0 && cc != NULL) {

        if (!get_canonical_ccache(context, &cc)) {
            if (cc)
                krb5_cc_close(context, cc);
            continue;
        }

        /* Turn off OPENCLOSE mode */
        code = krb5_cc_set_flags(context, cc, 0);
        if ( code ) goto cleanup;

        KFW_AFS_update_princ_ccache_data(context, cc,
                                         !strcmp(krb5_cc_get_type(context, cc),
                                                 LSA_CCTYPE));

        check_and_get_tokens_for_ccache(context, cc);

        flags = KRB5_TC_OPENCLOSE;  //turn on OPENCLOSE
        code = krb5_cc_set_flags(context, cc, flags);

        if (cc) {
            krb5_cc_close(context,cc);
            cc = 0;
        }
    }

    krb5_cccol_cursor_free(context, &cccol_cur);

  cleanup:
    if (context)
        krb5_free_context(context);
}

void
KFW_enable_DES(krb5_context alt_context)
{
    krb5_context context;
    krb5_error_code code;

    if ( alt_context ) {
        context = alt_context;
    } else {
        code = krb5_init_context(&context);
        if (code) goto cleanup;
    }

    if (krb5_enctype_valid(context, ETYPE_DES_CBC_CRC))
        krb5_enctype_enable(context, ETYPE_DES_CBC_CRC);

  cleanup:
    if (context && (context != alt_context))
        krb5_free_context(context);
}


int
KFW_AFS_get_cred( char * username,
                  char * cell,
                  char * password,
                  int lifetime,
                  char * smbname,
                  char ** reasonP )
{
    static char reason[1024]="";
    krb5_context context = NULL;
    krb5_ccache cc = NULL;
    char * realm = NULL, * userrealm = NULL;
    krb5_principal principal = NULL;
    char * pname = NULL;
    krb5_error_code code;
    char local_cell[CELL_MAXNAMELEN+1];
    char **cells = NULL;
    int  cell_count=0;
    struct afsconf_cell cellconfig;
    char * dot;

    DebugPrintf("KFW_AFS_get_cred for token %s in cell %s\n", username, cell);

    memset(&cellconfig, 0, sizeof(cellconfig));

    code = krb5_init_context(&context);
    if ( code ) goto cleanup;

    code = KFW_AFS_get_cellconfig( cell, (void*)&cellconfig, local_cell);
    if ( code ) goto cleanup;

    realm = afs_realm_of_cell(context, &cellconfig);  // do not free

    userrealm = strchr(username,'@');
    if ( userrealm ) {
        pname = strdup(username);
        if (!KFW_accept_dotted_usernames()) {
            userrealm = strchr(pname, '@');
            *userrealm = '\0';

            /* handle kerberos iv notation */
            while ( dot = strchr(pname,'.') ) {
                *dot = '/';
            }
            *userrealm = '@';
        }
        userrealm++;
    } else {
        size_t len = strlen(username) + strlen(realm) + 2;
        pname = malloc(len);
        if (pname == NULL) {
            code = KRB5KRB_ERR_GENERIC;
            goto cleanup;
        }
        StringCbCopy(pname, len, username);

        if (!KFW_accept_dotted_usernames()) {
            /* handle kerberos iv notation */
            while ( dot = strchr(pname,'.') ) {
                *dot = '/';
            }
        }
        StringCbCat( pname, len, "@");
        StringCbCat( pname, len, realm);
    }
    if ( IsDebuggerPresent() ) {
        OutputDebugString("Realm of Cell: ");
        OutputDebugString(realm);
        OutputDebugString("\n");
        OutputDebugString("Realm of User: ");
        OutputDebugString(userrealm?userrealm:"<NULL>");
        OutputDebugString("\n");
    }

    code = krb5_parse_name(context, pname, &principal);
    if ( code ) goto cleanup;

    code = KFW_get_ccache(context, principal, &cc);
    if ( code ) goto cleanup;

    if ( lifetime == 0 )
        lifetime = KFW_get_default_lifetime(context, realm);

    if ( password && password[0] ) {
        code = KFW_kinit( context, cc, HWND_DESKTOP,
                          pname,
                          password,
                          lifetime,
#ifndef USE_LEASH
                          0, /* forwardable */
                          0, /* not proxiable */
                          1, /* renewable */
                          1, /* noaddresses */
                          0  /* no public ip */
#else
                          pLeash_get_default_forwardable(),
                          pLeash_get_default_proxiable(),
                          pLeash_get_default_renewable() ? pLeash_get_default_renew_till() : 0,
                          pLeash_get_default_noaddresses(),
                          pLeash_get_default_publicip()
#endif /* USE_LEASH */
                          );

        if ( IsDebuggerPresent() ) {
            char message[256];
            StringCbPrintf(message, sizeof(message), "KFW_kinit() returns: %d\n", code);
            OutputDebugString(message);
        }
        if ( code ) goto cleanup;

        KFW_AFS_update_princ_ccache_data(context, cc, FALSE);

        code = KFW_AFS_klog(context, cc, "afs", cell, realm, lifetime, smbname);
        if ( IsDebuggerPresent() ) {
            char message[256];
            StringCbPrintf(message, sizeof(message), "KFW_AFS_klog() returns: %d\n", code);
            OutputDebugString(message);
        }
        if ( code ) goto cleanup;
    }

    KFW_AFS_update_cell_princ_map(context, cell, pname, TRUE);

    // Attempt to obtain new tokens for other cells supported by the same
    // principal
    cell_count = KFW_AFS_find_cells_for_princ(context, pname, &cells, TRUE);
    if ( cell_count > 1 ) {
        while ( cell_count-- ) {
            if ( strcmp(cells[cell_count],cell) ) {
                if ( IsDebuggerPresent() ) {
                    char message[256];
                    StringCbPrintf(message, sizeof(message),
                                   "found another cell for the same principal: %s\n", cell);
                    OutputDebugString(message);
                }

                if (cellconfig.linkedCell) {
                    free(cellconfig.linkedCell);
                    cellconfig.linkedCell = NULL;
                }
                code = KFW_AFS_get_cellconfig( cells[cell_count], (void*)&cellconfig, local_cell);
                if ( code ) continue;

                realm = afs_realm_of_cell(context, &cellconfig);  // do not free
                if ( IsDebuggerPresent() ) {
                    OutputDebugString("Realm: ");
                    OutputDebugString(realm);
                    OutputDebugString("\n");
                }

                code = KFW_AFS_klog(context, cc, "afs", cells[cell_count], realm, lifetime, smbname);
                if ( IsDebuggerPresent() ) {
                    char message[256];
                    StringCbPrintf(message, sizeof(message), "KFW_AFS_klog() returns: %d\n", code);
                    OutputDebugString(message);
                }
            }
            free(cells[cell_count]);
        }
        free(cells);
    } else if ( cell_count == 1 ) {
        free(cells[0]);
        free(cells);
    }

  cleanup:
    if ( pname )
        free(pname);
    if ( cc )
        krb5_cc_close(context, cc);
    if ( cellconfig.linkedCell )
        free(cellconfig.linkedCell);

    if ( code && reasonP ) {
        const char *msg = krb5_get_error_message(context, code);
        StringCbCopyN( reason, sizeof(reason),
                       msg, sizeof(reason) - 1);
        *reasonP = reason;
        krb5_free_error_message(context, msg);
    }
    return(code);
}

int
KFW_AFS_destroy_tickets_for_cell(char * cell)
{
    krb5_context	context = NULL;
    krb5_error_code	code;
    int count;
    char ** principals = NULL;

    DebugPrintf("KFW_AFS_destroy_tickets_for_cell: %s\n", cell);

    code = krb5_init_context(&context);
    if (code) context = 0;

    count = KFW_AFS_find_principals_for_cell(context, cell, &principals, FALSE);
    if ( count > 0 ) {
        krb5_principal      princ = 0;
        krb5_ccache			cc  = 0;

        while ( count-- ) {
            int cell_count = KFW_AFS_find_cells_for_princ(context, principals[count], NULL, TRUE);
            if ( cell_count > 1 ) {
                // TODO - What we really should do here is verify whether or not any of the
                // other cells which use this principal to obtain its credentials actually
                // have valid tokens or not.  If they are currently using these credentials
                // we will skip them.  For the time being we assume that if there is an active
                // map in the table that they are actively being used.
                goto loop_cleanup;
            }

            code = krb5_parse_name(context, principals[count], &princ);
            if (code) goto loop_cleanup;

            code = KFW_get_ccache(context, princ, &cc);
            if (code) goto loop_cleanup;

            code = krb5_cc_destroy(context, cc);
            if (!code) cc = 0;

          loop_cleanup:
            if ( cc ) {
                krb5_cc_close(context, cc);
                cc = 0;
            }
            if ( princ ) {
                krb5_free_principal(context, princ);
                princ = 0;
            }

            KFW_AFS_update_cell_princ_map(context, cell, principals[count], FALSE);
            free(principals[count]);
        }
        free(principals);
    }
    if (context)
		krb5_free_context(context);
    return 0;
}

int
KFW_AFS_destroy_tickets_for_principal(char * user)
{
    krb5_context	context = NULL;
    krb5_error_code	code;
    int count;
    char ** cells = NULL;
    krb5_principal      princ = NULL;
    krb5_ccache		cc  = NULL;

    DebugPrintf("KFW_AFS_destroy_tickets_for_user: %s\n", user);

    code = krb5_init_context(&context);
    if (code) return 0;

    code = krb5_parse_name(context, user, &princ);
    if (code) goto loop_cleanup;

    code = KFW_get_ccache(context, princ, &cc);
    if (code) goto loop_cleanup;

    code = krb5_cc_destroy(context, cc);
    if (!code) cc = 0;

  loop_cleanup:
    if ( cc ) {
        krb5_cc_close(context, cc);
        cc = 0;
    }
    if ( princ ) {
        krb5_free_principal(context, princ);
        princ = 0;
    }

    count = KFW_AFS_find_cells_for_princ(context, user, &cells, TRUE);
    if ( count >= 1 ) {
        while ( count-- ) {
            KFW_AFS_update_cell_princ_map(context, cells[count], user, FALSE);
            free(cells[count]);
        }
        free(cells);
    }

    if (context)
        krb5_free_context(context);

    return 0;
}

int
KFW_AFS_renew_expiring_tokens(void)
{
    krb5_error_code     code = 0;
    krb5_context	context = NULL;
    krb5_ccache		cc = NULL;
    krb5_timestamp now;
    struct principal_ccache_data * pcc_next = princ_cc_data;
    int cell_count;
    char ** cells=NULL;
    const char * realm = NULL;
    char local_cell[CELL_MAXNAMELEN+1]="";
    struct afsconf_cell cellconfig;

    if ( pcc_next == NULL ) // nothing to do
        return 0;

    if ( IsDebuggerPresent() ) {
        OutputDebugString("KFW_AFS_renew_expiring_tokens\n");
    }

    memset(&cellconfig, 0, sizeof(cellconfig));

    code = krb5_init_context(&context);
    if (code) goto cleanup;

    code = krb5_timeofday(context, &now);
    if (code) goto cleanup;

    for ( ; pcc_next ; pcc_next = pcc_next->next ) {
        if ( pcc_next->expired )
            continue;

        if ( now >= (pcc_next->expiration_time) ) {
            if ( !pcc_next->from_lsa ) {
                pcc_next->expired = 1;
                continue;
            }
        }

        if ( pcc_next->renew && now >= (pcc_next->expiration_time - cminRENEW * csec1MINUTE) ) {
            code = krb5_cc_resolve(context, pcc_next->ccache_name, &cc);
            if ( code )
                goto loop_cleanup;
            code = KFW_renew(context,cc);
#ifdef USE_MS2MIT
            if ( code && pcc_next->from_lsa)
                goto loop_cleanup;
#endif /* USE_MS2MIT */


            KFW_AFS_update_princ_ccache_data(context, cc, pcc_next->from_lsa);
            if (code) goto loop_cleanup;

            // Attempt to obtain new tokens for other cells supported by the same
            // principal
            cell_count = KFW_AFS_find_cells_for_princ(context, pcc_next->principal, &cells, TRUE);
            if ( cell_count > 0 ) {
                while ( cell_count-- ) {
                    if ( IsDebuggerPresent() ) {
                        OutputDebugString("Cell: ");
                        OutputDebugString(cells[cell_count]);
                        OutputDebugString("\n");
                    }
                    if (cellconfig.linkedCell) {
                        free(cellconfig.linkedCell);
                        cellconfig.linkedCell = NULL;
                    }
                    code = KFW_AFS_get_cellconfig( cells[cell_count], (void*)&cellconfig, local_cell);
                    if ( code ) continue;
                    realm = afs_realm_of_cell(context, &cellconfig);  // do not free
                    if ( IsDebuggerPresent() ) {
                        OutputDebugString("Realm: ");
                        OutputDebugString(realm);
                        OutputDebugString("\n");
                    }
                    code = KFW_AFS_klog(context, cc, "afs", cells[cell_count], (char *)realm, 0, NULL);
                    if ( IsDebuggerPresent() ) {
                        char message[256];
                        StringCbPrintf(message, sizeof(message), "KFW_AFS_klog() returns: %d\n", code);
                        OutputDebugString(message);
                    }
                    free(cells[cell_count]);
                }
                free(cells);
            }
        }

      loop_cleanup:
        if ( cc ) {
            krb5_cc_close(context,cc);
            cc = 0;
        }
    }

  cleanup:
    if ( cc )
        krb5_cc_close(context,cc);
    if ( context )
        krb5_free_context(context);
    if (cellconfig.linkedCell)
        free(cellconfig.linkedCell);

    return 0;
}


BOOL
KFW_AFS_renew_token_for_cell(char * cell)
{
    krb5_error_code     code = 0;
    krb5_context	context = NULL;
    int count;
    char ** principals = NULL;

    if ( IsDebuggerPresent() ) {
        OutputDebugString("KFW_AFS_renew_token_for_cell:");
        OutputDebugString(cell);
        OutputDebugString("\n");
    }

    code = krb5_init_context(&context);
    if (code) goto cleanup;

    count = KFW_AFS_find_principals_for_cell(context, cell, &principals, TRUE);
    if ( count == 0 ) {
        // We know we must have a credential somewhere since we are
        // trying to renew a token

        KFW_import_ccache_data();
        count = KFW_AFS_find_principals_for_cell(context, cell, &principals, TRUE);
    }
    if ( count > 0 ) {
        krb5_principal      princ = 0;
        krb5_principal      service = 0;
#ifdef COMMENT
        krb5_creds          mcreds, creds;
#endif /* COMMENT */
        krb5_ccache			cc  = 0;
        const char * realm = NULL;
        struct afsconf_cell cellconfig;
        char local_cell[CELL_MAXNAMELEN+1];

        memset(&cellconfig, 0, sizeof(cellconfig));

        while ( count-- ) {
            code = krb5_parse_name(context, principals[count], &princ);
            if (code) goto loop_cleanup;

            code = KFW_get_ccache(context, princ, &cc);
            if (code) goto loop_cleanup;

            if (cellconfig.linkedCell) {
                free(cellconfig.linkedCell);
                cellconfig.linkedCell = NULL;
            }
            code = KFW_AFS_get_cellconfig( cell, (void*)&cellconfig, local_cell);
            if ( code ) goto loop_cleanup;

            realm = afs_realm_of_cell(context, &cellconfig);  // do not free
            if ( IsDebuggerPresent() ) {
                OutputDebugString("Realm: ");
                OutputDebugString(realm);
                OutputDebugString("\n");
            }

#ifdef COMMENT
            /* krb5_cc_remove_cred() is not implemented
             * for a single cred
             */
            code = krb5_build_principal(context, &service, strlen(realm),
                                          realm, "afs", cell, NULL);
            if (!code) {
                memset(&mcreds, 0, sizeof(krb5_creds));
                mcreds.client = princ;
                mcreds.server = service;

                code = krb5_cc_retrieve_cred(context, cc, 0, &mcreds, &creds);
                if (!code) {
                    if ( IsDebuggerPresent() ) {
                        char * cname, *sname;
                        krb5_unparse_name(context, creds.client, &cname);
                        krb5_unparse_name(context, creds.server, &sname);
                        OutputDebugString("Removing credential for client \"");
                        OutputDebugString(cname);
                        OutputDebugString("\" and service \"");
                        OutputDebugString(sname);
                        OutputDebugString("\"\n");
                        krb5_free_unparsed_name(context,cname);
                        krb5_free_unparsed_name(context,sname);
                    }

                    code = krb5_cc_remove_cred(context, cc, 0, &creds);
                    krb5_free_principal(context, creds.client);
                    krb5_free_principal(context, creds.server);
                }
            }
#endif /* COMMENT */

            code = KFW_AFS_klog(context, cc, "afs", cell, (char *)realm, 0,NULL);
            if ( IsDebuggerPresent() ) {
                char message[256];
                StringCbPrintf(message, sizeof(message), "KFW_AFS_klog() returns: %d\n", code);
                OutputDebugString(message);
            }

          loop_cleanup:
            if (cc) {
                krb5_cc_close(context, cc);
                cc = 0;
            }
            if (princ) {
                krb5_free_principal(context, princ);
                princ = 0;
            }
            if (service) {
                krb5_free_principal(context, service);
                princ = 0;
            }
            if (cellconfig.linkedCell) {
                free(cellconfig.linkedCell);
                cellconfig.linkedCell = NULL;
            }

            KFW_AFS_update_cell_princ_map(context, cell, principals[count], code ? FALSE : TRUE);
            free(principals[count]);
        }
        free(principals);
    } else
        code = -1;      // we did not renew the tokens

  cleanup:
    if (context)
        krb5_free_context(context);
    return (code ? FALSE : TRUE);

}

int
KFW_AFS_renew_tokens_for_all_cells(void)
{
    struct cell_principal_map * next = cell_princ_map;

    DebugPrintf("KFW_AFS_renew_tokens_for_all()\n");

    if ( !next )
        return 0;

    for ( ; next ; next = next->next ) {
        if ( next->active )
            KFW_AFS_renew_token_for_cell(next->cell);
    }
    return 0;
}

int
KFW_renew(krb5_context alt_context, krb5_ccache alt_cc)
{
    krb5_error_code     code = 0;
    krb5_context	context = NULL;
    krb5_ccache		cc = NULL;
    krb5_principal	me = NULL;
    krb5_principal      server = NULL;
    krb5_creds		my_creds;
    const char          *realm = NULL;

    memset(&my_creds, 0, sizeof(krb5_creds));

    if ( alt_context ) {
        context = alt_context;
    } else {
        code = krb5_init_context(&context);
        if (code) goto cleanup;
    }

    if ( alt_cc ) {
        cc = alt_cc;
    } else {
        code = krb5_cc_default(context, &cc);
        if (code) goto cleanup;
    }

    code = krb5_cc_get_principal(context, cc, &me);
    if (code) goto cleanup;

    realm = krb5_principal_get_realm(context, me);

    code = krb5_make_principal(context, &server, realm,
                               KRB5_TGS_NAME, realm, NULL);
    if ( code )
        goto cleanup;

    if ( IsDebuggerPresent() ) {
        char * cname, *sname;
        krb5_unparse_name(context, me, &cname);
        krb5_unparse_name(context, server, &sname);
        DebugPrintf("Renewing credential for client \"%s\" and service\"%s\"\n",
                    cname, sname);
        krb5_free_unparsed_name(context,cname);
        krb5_free_unparsed_name(context,sname);
    }

    my_creds.client = me;
    my_creds.server = server;

    code = krb5_get_renewed_creds(context, &my_creds, me, cc, NULL);
    if (code) {
        DebugPrintf("krb5_get_renewed_creds() failed: %d\n", code);
        goto cleanup;
    }

    code = krb5_cc_initialize(context, cc, me);
    if (code) {
        DebugPrintf("krb5_cc_initialize() failed: %d\n", code);
        goto cleanup;
    }

    code = krb5_cc_store_cred(context, cc, &my_creds);
    if (code) {
        DebugPrintf("krb5_cc_store_cred() failed: %d\n", code);
        goto cleanup;
    }

  cleanup:
    if (my_creds.client == me)
        my_creds.client = 0;
    if (my_creds.server == server)
        my_creds.server = 0;
    krb5_free_cred_contents(context, &my_creds);
    if (me)
        krb5_free_principal(context, me);
    if (server)
        krb5_free_principal(context, server);
    if (cc && (cc != alt_cc))
        krb5_cc_close(context, cc);
    if (context && (context != alt_context))
        krb5_free_context(context);
    return(code);
}

int
KFW_kinit( krb5_context alt_context,
           krb5_ccache  alt_cc,
           HWND hParent,
           char *principal_name,
           char *password,
           krb5_deltat lifetime,
           DWORD                       forwardable,
           DWORD                       proxiable,
           krb5_deltat                 renew_life,
           DWORD                       addressless,
           DWORD                       publicIP)
{
    krb5_error_code		code = 0;
    krb5_context		context = NULL;
    krb5_ccache			cc = NULL;
    krb5_principal		me = NULL;
    char*                       name = NULL;
    krb5_creds			my_creds;
    krb5_get_init_creds_opt     *options = NULL;
    krb5_addresses              addrs = {0, NULL};
    int                         i = 0, addr_count = 0;

    memset(&my_creds, 0, sizeof(my_creds));

    if (alt_context) {
        context = alt_context;
    } else {
        code = krb5_init_context(&context);
        if (code) goto cleanup;
    }

    if ( alt_cc ) {
        cc = alt_cc;
    } else {
        code = krb5_cc_default(context, &cc);
        if (code) goto cleanup;
    }

    code = krb5_get_init_creds_opt_alloc(context, &options);
    if (code) goto cleanup;

    code = krb5_parse_name(context, principal_name, &me);
    if (code) goto cleanup;

    code = krb5_unparse_name(context, me, &name);
    if (code) goto cleanup;

    if (lifetime == 0)
        lifetime = KFW_get_default_lifetime(context,
                                            krb5_principal_get_realm(context, me));

    lifetime *= 60;

    if (renew_life > 0)
	renew_life *= 60;

    if (lifetime)
        krb5_get_init_creds_opt_set_tkt_life(options, lifetime);
    krb5_get_init_creds_opt_set_forwardable(options, forwardable ? 1 : 0);
    krb5_get_init_creds_opt_set_proxiable(options, proxiable ? 1 : 0);
    krb5_get_init_creds_opt_set_renew_life(options, renew_life);
    if (addressless) {
        krb5_get_init_creds_opt_set_addressless(context, options, TRUE);
    } else {
	if (publicIP) {
            // we are going to add the public IP address specified by the user
            // to the list provided by the operating system
            struct sockaddr_in     in_addr;
            krb5_address    addr;
            krb5_addresses  addr_l;

            krb5_get_all_client_addrs(context, &addrs);

            in_addr.sin_family = AF_INET;
            in_addr.sin_port = 0;
            in_addr.sin_addr.S_un.S_addr = htonl(publicIP);

            code = krb5_sockaddr2address(context, (struct sockaddr *)&in_addr,
                                         &addr);

            if (code == 0) {
                addr_l.len = 1;
                addr_l.val = &addr;

                code = krb5_append_addresses(context, &addrs, &addr_l);

                krb5_free_address(context, &addr);
            }

            krb5_get_init_creds_opt_set_address_list(options, &addrs);
        }
    }

    code = krb5_get_init_creds_password(context,
                                       &my_creds,
                                       me,
                                       password, // password
                                       KRB5_prompter, // prompter
                                       hParent, // prompter data
                                       0, // start time
                                       0, // service name
                                       options);
    if (code)
	goto cleanup;

    code = krb5_cc_initialize(context, cc, me);
    if (code)
	goto cleanup;

    code = krb5_cc_store_cred(context, cc, &my_creds);
    if (code)
	goto cleanup;

 cleanup:
    if ( addrs.len > 0 )
        krb5_free_addresses(context, &addrs);

    if (my_creds.client == me)
	my_creds.client = 0;

    krb5_free_cred_contents(context, &my_creds);
    if (name)
        krb5_free_unparsed_name(context, name);
    if (me)
        krb5_free_principal(context, me);
    if (options)
        krb5_get_init_creds_opt_free(context, options);
    if (cc && (cc != alt_cc))
        krb5_cc_close(context, cc);
    if (context && (context != alt_context))
        krb5_free_context(context);
    return(code);
}


int
KFW_kdestroy(krb5_context alt_context, krb5_ccache alt_cc)
{
    krb5_context		context = NULL;
    krb5_ccache			cc = NULL;
    krb5_error_code		code;

    if (alt_context) {
        context = alt_context;
    } else {
        code = krb5_init_context(&context);
        if (code) goto cleanup;
    }

    if ( alt_cc ) {
        cc = alt_cc;
    } else {
        code = krb5_cc_default(context, &cc);
        if (code) goto cleanup;
    }

    code = krb5_cc_destroy(context, cc);
    if ( !code ) cc = 0;

  cleanup:
    if (cc && (cc != alt_cc))
        krb5_cc_close(context, cc);
    if (context && (context != alt_context))
        krb5_free_context(context);

    return(code);
}


#ifdef USE_MS2MIT
static BOOL
GetSecurityLogonSessionData(PSECURITY_LOGON_SESSION_DATA * ppSessionData)
{
    NTSTATUS Status = 0;
    HANDLE  TokenHandle;
    TOKEN_STATISTICS Stats;
    DWORD   ReqLen;
    BOOL    Success;

    if (!ppSessionData)
        return FALSE;
    *ppSessionData = NULL;

    Success = OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY, &TokenHandle );
    if ( !Success )
        return FALSE;

    Success = GetTokenInformation( TokenHandle, TokenStatistics, &Stats, sizeof(TOKEN_STATISTICS), &ReqLen );
    CloseHandle( TokenHandle );
    if ( !Success )
        return FALSE;

    Status = LsaGetLogonSessionData( &Stats.AuthenticationId, ppSessionData );
    if ( FAILED(Status) || !ppSessionData )
        return FALSE;

    return TRUE;
}

//
// MSLSA_IsKerberosLogon() does not validate whether or not there are valid tickets in the
// cache.  It validates whether or not it is reasonable to assume that if we
// attempted to retrieve valid tickets we could do so.  Microsoft does not
// automatically renew expired tickets.  Therefore, the cache could contain
// expired or invalid tickets.  Microsoft also caches the user's password
// and will use it to retrieve new TGTs if the cache is empty and tickets
// are requested.

static BOOL
MSLSA_IsKerberosLogon(VOID)
{
    PSECURITY_LOGON_SESSION_DATA pSessionData = NULL;
    BOOL    Success = FALSE;

    if ( GetSecurityLogonSessionData(&pSessionData) ) {
        if ( pSessionData->AuthenticationPackage.Buffer ) {
            WCHAR buffer[256];
            WCHAR *usBuffer;
            int usLength;

            Success = FALSE;
            usBuffer = (pSessionData->AuthenticationPackage).Buffer;
            usLength = (pSessionData->AuthenticationPackage).Length;
            if (usLength < 256)
            {
                StringCbCopyNW( buffer, sizeof(buffer)/sizeof(WCHAR),
                                usBuffer, usLength);
                if ( !lstrcmpW(L"Kerberos",buffer) )
                    Success = TRUE;
            }
        }
        LsaFreeReturnBuffer(pSessionData);
    }
    return Success;
}
#endif /* USE_MS2MIT */

static BOOL CALLBACK
MultiInputDialogProc( HWND hDialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    int i;

    switch ( message ) {
    case WM_INITDIALOG:
        if ( GetDlgCtrlID((HWND) wParam) != ID_MID_TEXT )
        {
            SetFocus(GetDlgItem( hDialog, ID_MID_TEXT));
            return FALSE;
        }
		for ( i=0; i < mid_cnt ; i++ ) {
			if (mid_tb[i].echo == 0)
				SendDlgItemMessage(hDialog, ID_MID_TEXT+i, EM_SETPASSWORDCHAR, 32, 0);
		    else if (mid_tb[i].echo == 2)
				SendDlgItemMessage(hDialog, ID_MID_TEXT+i, EM_SETPASSWORDCHAR, '*', 0);
		}
        return TRUE;

    case WM_COMMAND:
        switch ( LOWORD(wParam) ) {
        case IDOK:
            for ( i=0; i < mid_cnt ; i++ ) {
                if ( !GetDlgItemText(hDialog, ID_MID_TEXT+i, mid_tb[i].buf, mid_tb[i].len) )
                    *mid_tb[i].buf = '\0';
            }
            AFS_FALLTHROUGH;
        case IDCANCEL:
            EndDialog(hDialog, LOWORD(wParam));
            return TRUE;
        }
    }
    return FALSE;
}

static LPWORD
lpwAlign( LPWORD lpIn )
{
    ULONG_PTR ul;

    ul = (ULONG_PTR) lpIn;
    ul += 3;
    ul >>=2;
    ul <<=2;
    return (LPWORD) ul;;
}

/*
 * dialog widths are measured in 1/4 character widths
 * dialog height are measured in 1/8 character heights
 */

static LRESULT
MultiInputDialog( HINSTANCE hinst, HWND hwndOwner,
                  char * ptext[], int numlines, int width,
                  int tb_cnt, struct textField * tb)
{
    HGLOBAL hgbl;
    LPDLGTEMPLATE lpdt;
    LPDLGITEMTEMPLATE lpdit;
    LPWORD lpw;
    LPWSTR lpwsz;
    LRESULT ret;
    int nchar, i, pwid;

    hgbl = GlobalAlloc(GMEM_ZEROINIT, 4096);
    if (!hgbl)
        return -1;

    mid_cnt = tb_cnt;
    mid_tb = tb;

    lpdt = (LPDLGTEMPLATE)GlobalLock(hgbl);

    // Define a dialog box.

    lpdt->style = WS_POPUP | WS_BORDER | WS_SYSMENU
                   | DS_MODALFRAME | WS_CAPTION | DS_CENTER
                   | DS_SETFOREGROUND | DS_3DLOOK
                   | DS_SETFONT | DS_FIXEDSYS | DS_NOFAILCREATE;
    lpdt->cdit = numlines + (2 * tb_cnt) + 2;  // number of controls
    lpdt->x  = 10;
    lpdt->y  = 10;
    lpdt->cx = 20 + width * 4;
    lpdt->cy = 20 + (numlines + tb_cnt + 4) * 14;

    lpw = (LPWORD) (lpdt + 1);
    *lpw++ = 0;   // no menu
    *lpw++ = 0;   // predefined dialog box class (by default)

    lpwsz = (LPWSTR) lpw;
    nchar = MultiByteToWideChar (CP_ACP, 0, "", -1, lpwsz, 128);
    lpw   += nchar;
    *lpw++ = 8;                        // font size (points)
    lpwsz = (LPWSTR) lpw;
    nchar = MultiByteToWideChar (CP_ACP, 0, "MS Shell Dlg",
                                    -1, lpwsz, 128);
    lpw   += nchar;

    //-----------------------
    // Define an OK button.
    //-----------------------
    lpw = lpwAlign (lpw); // align DLGITEMTEMPLATE on DWORD boundary
    lpdit = (LPDLGITEMTEMPLATE) lpw;
    lpdit->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP | WS_BORDER;
    lpdit->dwExtendedStyle = 0;
    lpdit->x  = (lpdt->cx - 14)/4 - 20;
    lpdit->y  = 10 + (numlines + tb_cnt + 2) * 14;
    lpdit->cx = 40;
    lpdit->cy = 14;
    lpdit->id = IDOK;  // OK button identifier

    lpw = (LPWORD) (lpdit + 1);
    *lpw++ = 0xFFFF;
    *lpw++ = 0x0080;    // button class

    lpwsz = (LPWSTR) lpw;
    nchar = MultiByteToWideChar (CP_ACP, 0, "OK", -1, lpwsz, 50);
    lpw   += nchar;
    *lpw++ = 0;           // no creation data

    //-----------------------
    // Define an Cancel button.
    //-----------------------
    lpw = lpwAlign (lpw); // align DLGITEMTEMPLATE on DWORD boundary
    lpdit = (LPDLGITEMTEMPLATE) lpw;
    lpdit->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP | WS_BORDER;
    lpdit->dwExtendedStyle = 0;
    lpdit->x  = (lpdt->cx - 14)*3/4 - 20;
    lpdit->y  = 10 + (numlines + tb_cnt + 2) * 14;
    lpdit->cx = 40;
    lpdit->cy = 14;
    lpdit->id = IDCANCEL;  // CANCEL button identifier

    lpw = (LPWORD) (lpdit + 1);
    *lpw++ = 0xFFFF;
    *lpw++ = 0x0080;    // button class

    lpwsz = (LPWSTR) lpw;
    nchar = MultiByteToWideChar (CP_ACP, 0, "Cancel", -1, lpwsz, 50);
    lpw   += nchar;
    *lpw++ = 0;           // no creation data

    /* Add controls for preface data */
    for ( i=0; i<numlines; i++) {
        /*-----------------------
         * Define a static text control.
         *-----------------------*/
        lpw = lpwAlign (lpw); /* align DLGITEMTEMPLATE on DWORD boundary */
        lpdit = (LPDLGITEMTEMPLATE) lpw;
        lpdit->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
        lpdit->dwExtendedStyle = 0;
        lpdit->x  = 10;
        lpdit->y  = 10 + i * 14;
        lpdit->cx = (short)strlen(ptext[i]) * 4 + 10;
        lpdit->cy = 14;
        lpdit->id = ID_TEXT + i;  // text identifier

        lpw = (LPWORD) (lpdit + 1);
        *lpw++ = 0xFFFF;
        *lpw++ = 0x0082;                         // static class

        lpwsz = (LPWSTR) lpw;
        nchar = MultiByteToWideChar (CP_ACP, 0, ptext[i],
                                         -1, lpwsz, 2*width);
        lpw   += nchar;
        *lpw++ = 0;           // no creation data
    }

    for ( i=0, pwid = 0; i<tb_cnt; i++) {
	int len = (int)strlen(tb[i].label);
        if ( pwid < len )
            pwid = len;
    }

    for ( i=0; i<tb_cnt; i++) {
        /* Prompt */
        /*-----------------------
         * Define a static text control.
         *-----------------------*/
        lpw = lpwAlign (lpw); /* align DLGITEMTEMPLATE on DWORD boundary */
        lpdit = (LPDLGITEMTEMPLATE) lpw;
        lpdit->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
        lpdit->dwExtendedStyle = 0;
        lpdit->x  = 10;
        lpdit->y  = 10 + (numlines + i + 1) * 14;
        lpdit->cx = pwid * 4;
        lpdit->cy = 14;
        lpdit->id = ID_TEXT + numlines + i;  // text identifier

        lpw = (LPWORD) (lpdit + 1);
        *lpw++ = 0xFFFF;
        *lpw++ = 0x0082;                         // static class

        lpwsz = (LPWSTR) lpw;
        nchar = MultiByteToWideChar (CP_ACP, 0, tb[i].label ? tb[i].label : "",
                                     -1, lpwsz, 128);
        lpw   += nchar;
        *lpw++ = 0;           // no creation data

        /*-----------------------
         * Define an edit control.
         *-----------------------*/
        lpw = lpwAlign (lpw); /* align DLGITEMTEMPLATE on DWORD boundary */
        lpdit = (LPDLGITEMTEMPLATE) lpw;
        lpdit->style = WS_CHILD | WS_VISIBLE | ES_LEFT | WS_TABSTOP | WS_BORDER | (tb[i].echo == 1 ? 0L : ES_PASSWORD);
        lpdit->dwExtendedStyle = 0;
        lpdit->x  = 10 + (pwid + 1) * 4;
        lpdit->y  = 10 + (numlines + i + 1) * 14;
        lpdit->cx = (width - (pwid + 1)) * 4;
        lpdit->cy = 14;
        lpdit->id = ID_MID_TEXT + i;             // identifier

        lpw = (LPWORD) (lpdit + 1);
        *lpw++ = 0xFFFF;
        *lpw++ = 0x0081;                         // edit class

        lpwsz = (LPWSTR) lpw;
        nchar = MultiByteToWideChar (CP_ACP, 0, tb[i].def ? tb[i].def : "",
                                     -1, lpwsz, 128);
        lpw   += nchar;
        *lpw++ = 0;           // no creation data
    }

    GlobalUnlock(hgbl);
    ret = DialogBoxIndirect(hinst, (LPDLGTEMPLATE) hgbl,
                            hwndOwner, (DLGPROC) MultiInputDialogProc);
    GlobalFree(hgbl);

    switch ( ret ) {
    case 0:     /* Timeout */
        return -1;
    case IDOK:
        return 1;
    case IDCANCEL:
        return 0;
    default: {
        char buf[256];
        StringCbPrintf(buf, sizeof(buf), "DialogBoxIndirect() failed: %d", GetLastError());
        MessageBox(hwndOwner,
                    buf,
                    "GetLastError()",
                    MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
        return -1;
    }
    }
}

static int
multi_field_dialog(HWND hParent, char * preface, int n, struct textField tb[])
{
    HINSTANCE hInst = 0;
    size_t maxwidth = 0;
    int numlines = 0;
    size_t len;
    char * plines[16], *p = preface ? preface : "";
    int i;

    for ( i=0; i<16; i++ )
        plines[i] = NULL;

    while (*p && numlines < 16) {
        plines[numlines++] = p;
        for ( ;*p && *p != '\r' && *p != '\n'; p++ );
        if ( *p == '\r' && *(p+1) == '\n' ) {
            *p++ = '\0';
            p++;
        } else if ( *p == '\n' ) {
            *p++ = '\0';
        }
        if ( strlen(plines[numlines-1]) > maxwidth )
            maxwidth = strlen(plines[numlines-1]);
    }

    for ( i=0;i<n;i++ ) {
        len = (int)strlen(tb[i].label) + 1 + (tb[i].len > 40 ? 40 : tb[i].len);
        if ( maxwidth < len )
            maxwidth = len;
    }

    return(MultiInputDialog(hInst, hParent, plines, numlines, maxwidth, n, tb)?1:0);
}

static krb5_error_code KRB5_CALLCONV
KRB5_prompter( krb5_context context,
               void *data,
               const char *name,
               const char *banner,
               int num_prompts,
               krb5_prompt prompts[])
{
    krb5_error_code     errcode = 0;
    int                 i;
    struct textField * tb = NULL;
    int    len = 0, blen=0, nlen=0;
	HWND hParent = (HWND)data;

    if (name)
        nlen = (int)strlen(name)+2;

    if (banner)
        blen = (int)strlen(banner)+2;

    tb = (struct textField *) malloc(sizeof(struct textField) * num_prompts);
    if ( tb != NULL ) {
        int ok;
        memset(tb,0,sizeof(struct textField) * num_prompts);
        for ( i=0; i < num_prompts; i++ ) {
            tb[i].buf = prompts[i].reply->data;
            tb[i].len = prompts[i].reply->length;
            tb[i].label = prompts[i].prompt;
            tb[i].def = NULL;
            tb[i].echo = (prompts[i].hidden ? 2 : 1);
        }

        ok = multi_field_dialog(hParent,(char *)banner,num_prompts,tb);
        if ( ok ) {
            for ( i=0; i < num_prompts; i++ )
                prompts[i].reply->length = (unsigned int)strlen(prompts[i].reply->data);
        } else
            errcode = -2;
    }

    if ( tb )
        free(tb);
    if (errcode) {
        for (i = 0; i < num_prompts; i++) {
            memset(prompts[i].reply->data, 0, prompts[i].reply->length);
        }
    }
    return errcode;
}

BOOL
KFW_AFS_wait_for_service_start(void)
{
    char    HostName[64];
    DWORD   CurrentState;

    CurrentState = SERVICE_START_PENDING;
    memset(HostName, '\0', sizeof(HostName));
    gethostname(HostName, sizeof(HostName));

    while (CurrentState != SERVICE_RUNNING || CurrentState != SERVICE_STOPPED)
    {
        if (GetServiceStatus(HostName, TRANSARCAFSDAEMON, &CurrentState) != NOERROR)
            return(0);
        if ( IsDebuggerPresent() ) {
            switch ( CurrentState ) {
            case SERVICE_STOPPED:
                OutputDebugString("SERVICE_STOPPED\n");
                break;
            case SERVICE_START_PENDING:
                OutputDebugString("SERVICE_START_PENDING\n");
                break;
            case SERVICE_STOP_PENDING:
                OutputDebugString("SERVICE_STOP_PENDING\n");
                break;
            case SERVICE_RUNNING:
                OutputDebugString("SERVICE_RUNNING\n");
                break;
            case SERVICE_CONTINUE_PENDING:
                OutputDebugString("SERVICE_CONTINUE_PENDING\n");
                break;
            case SERVICE_PAUSE_PENDING:
                OutputDebugString("SERVICE_PAUSE_PENDING\n");
                break;
            case SERVICE_PAUSED:
                OutputDebugString("SERVICE_PAUSED\n");
                break;
            default:
                OutputDebugString("UNKNOWN Service State\n");
            }
        }
        if (CurrentState == SERVICE_STOPPED)
            return(0);
        if (CurrentState == SERVICE_RUNNING)
            return(1);
        Sleep(500);
    }
    return(0);
}


int
KFW_AFS_unlog(void)
{
    long	rc;
    char    HostName[64];
    DWORD   CurrentState;

    CurrentState = 0;
    memset(HostName, '\0', sizeof(HostName));
    gethostname(HostName, sizeof(HostName));
    if (GetServiceStatus(HostName, TRANSARCAFSDAEMON, &CurrentState) != NOERROR)
        return(0);
    if (CurrentState != SERVICE_RUNNING)
        return(0);

    rc = ktc_ForgetAllTokens();

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
    static char lastcell[CELL_MAXNAMELEN+1] = { 0 };
    static char confdir[512] = { 0 };
#ifdef AFS_ID_TO_NAME
    char username_copy[BUFSIZ];
#endif /* AFS_ID_TO_NAME */
    long viceId = ANONYMOUSID;		/* AFS uid of user */
    int  status = 0;
#ifdef ALLOW_REGISTER
    afs_int32 id;
#endif /* ALLOW_REGISTER */

    if (confdir[0] == '\0')
        cm_GetConfigDir(confdir, sizeof(confdir));

    StringCbCopyN( lastcell, sizeof(lastcell),
                   aserver->cell, sizeof(lastcell) - 1);

    if (!pr_Initialize (0, confdir, aserver->cell)) {
        char sname[PR_MAXNAMELEN];
        StringCbCopyN( sname, sizeof(sname),
                       username, sizeof(sname) - 1);
        status = pr_SNameToId (sname, &viceId);
	pr_End();
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
    if (status == 0) {
        if (viceId != ANONYMOUSID) {
#else /* ALLOW_REGISTER */
            if ((status == 0) && (viceId != ANONYMOUSID))
#endif /* ALLOW_REGISTER */
            {
#ifdef AFS_ID_TO_NAME
                StringCbCopyN( username_copy, sizeof(username_copy),
                               username, sizeof(username_copy) - 1);
                snprintf (username, BUFSIZ, "%s (AFS ID %d)", username_copy, (int) viceId);
#endif /* AFS_ID_TO_NAME */
            }
#ifdef ALLOW_REGISTER
        } else if (strcmp(realm_of_user, realm_of_cell) != 0) {
            id = 0;
            StringCbCopyN( aclient->name, sizeof(aclient->name),
                           username, sizeof(aclient->name) - 1);
            aclient->instance[0] = '\0';
            StringCbCopyN( aclient->cell, sizeof(aclient->cell),
                           realm_of_user, sizeof(aclient->cell) - 1);
            if (status = ktc_SetToken(aserver, atoken, aclient, 0))
                return status;
            if (status = pr_Initialize(1L, confdir, aserver->cell))
                return status;
            status = pr_CreateUser(username, &id);
	    pr_End();
	    if (status)
		return status;
#ifdef AFS_ID_TO_NAME
            StringCbCopyN( username_copy, sizeof(username_copy),
                           username, sizeof(username_copy) - 1);
            snprintf (username, BUFSIZ, "%s (AFS ID %d)", username_copy, (int) viceId);
#endif /* AFS_ID_TO_NAME */
        }
    }
#endif /* ALLOW_REGISTER */
    return status;
}


static void
copy_realm_of_ticket(krb5_context context, char * dest, size_t destlen, krb5_creds *v5cred) {
    Ticket ticket;
    size_t len;
    int ret;

    ret = decode_Ticket(v5cred->ticket.data, v5cred->ticket.length,
                        &ticket, &len);
    if (ret == 0) {
        StringCbCopy(dest, destlen, ticket.realm);

        free_Ticket(&ticket);
    }
}

static int
KFW_AFS_continue_aklog_processing_after_krb5_error(krb5_error_code code)
{
    if (code == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN ||
        code == KRB5_ERR_HOST_REALM_UNKNOWN ||
        code == KRB5KRB_ERR_GENERIC /* heimdal */ ||
        code == KRB5KRB_AP_ERR_MSG_TYPE) {
        return 1;
    }

    return 0;
}

int
KFW_AFS_klog(
    krb5_context alt_context,
    krb5_ccache  alt_cc,
    char *service,
    char *cell,
    char *realm,
    int  lifetime,  	/* unused parameter */
    char *smbname
    )
{
    long	rc = 0;
    struct ktc_principal	aserver;
    struct ktc_principal	aclient;
    char	realm_of_user[REALM_SZ]; /* Kerberos realm of user */
    char	realm_of_cell[REALM_SZ]; /* Kerberos realm of cell */
    char	local_cell[CELL_MAXNAMELEN+1];
    char	Dmycell[CELL_MAXNAMELEN+1];
    struct ktc_token	atoken;
    struct ktc_token	btoken;
    struct afsconf_cell	ak_cellconfig; /* General information about the cell */
    char	RealmName[128];
    char	CellName[128];
    char	ServiceName[128];
    DWORD       CurrentState;
    char        HostName[64];
    krb5_context  context = NULL;
    krb5_ccache   cc = NULL;
    krb5_creds increds;
    krb5_creds * k5creds = NULL;
    krb5_error_code code;
    krb5_principal client_principal = NULL;
    krb5_data * k5data = NULL;
    unsigned int retry = 0;

    CurrentState = 0;
    memset(HostName, '\0', sizeof(HostName));
    gethostname(HostName, sizeof(HostName));
    if (GetServiceStatus(HostName, TRANSARCAFSDAEMON, &CurrentState) != NOERROR) {
        if ( IsDebuggerPresent() )
            OutputDebugString("Unable to retrieve AFSD Service Status\n");
        return(-1);
    }
    if (CurrentState != SERVICE_RUNNING) {
        if ( IsDebuggerPresent() )
            OutputDebugString("AFSD Service NOT RUNNING\n");
        return(-2);
    }

    memset(&ak_cellconfig, 0, sizeof(ak_cellconfig));
    memset(RealmName, '\0', sizeof(RealmName));
    memset(CellName, '\0', sizeof(CellName));
    memset(ServiceName, '\0', sizeof(ServiceName));
    memset(realm_of_user, '\0', sizeof(realm_of_user));
    memset(realm_of_cell, '\0', sizeof(realm_of_cell));
    if (cell && cell[0])
        StringCbCopyN( Dmycell, sizeof(Dmycell),
                       cell, sizeof(Dmycell) - 1);
    else
        memset(Dmycell, '\0', sizeof(Dmycell));

    // NULL or empty cell returns information on local cell
    if (rc = KFW_AFS_get_cellconfig(Dmycell, &ak_cellconfig, local_cell))
    {
        // KFW_AFS_error(rc, "get_cellconfig()");
        return(rc);
    }

    if ( alt_context ) {
        context = alt_context;
    } else {
        code = krb5_init_context(&context);
        if (code) goto cleanup;
    }

    if ( alt_cc ) {
        cc = alt_cc;
    } else {
        code = krb5_cc_default(context, &cc);
        if (code)
            goto cleanup;
    }

    memset(&increds, 0, sizeof(increds));

    code = krb5_cc_get_principal(context, cc, &client_principal);
    if (code) {
        if ( code == KRB5_CC_NOTFOUND && IsDebuggerPresent() )
        {
            OutputDebugString("Principal Not Found for ccache\n");
        }
        goto cleanup;
    }

    if (!KFW_accept_dotted_usernames()) {
        const char * comp;
        /* look for client principals which cannot be distinguished
         * from Kerberos 4 multi-component principal names
         */
        comp = krb5_principal_get_comp_string(context,client_principal,0);
        if (strchr(comp, '.') != NULL) {
            OutputDebugString("Illegal Principal name contains dot in first component\n");
            rc = KRB5KRB_ERR_GENERIC;
            goto cleanup;
        }
    }

    StringCbCopy(realm_of_user, sizeof(realm_of_user),
                 krb5_principal_get_realm(context, client_principal));

    StringCbCopyN( realm_of_cell, sizeof(realm_of_cell),
                   afs_realm_of_cell(context, &ak_cellconfig),
                   sizeof(realm_of_cell) - 1);

    if (strlen(service) == 0)
        StringCbCopy( ServiceName, sizeof(ServiceName), "afs");
    else
        StringCbCopyN( ServiceName, sizeof(ServiceName),
                       service, sizeof(ServiceName) - 1);

    if (strlen(cell) == 0)
        StringCbCopyN( CellName, sizeof(CellName),
                       local_cell, sizeof(CellName) - 1);
    else
        StringCbCopyN( CellName, sizeof(CellName),
                       cell, sizeof(CellName) - 1);

    if (strlen(realm) == 0)
        StringCbCopyN( RealmName, sizeof(RealmName),
                       realm_of_cell, sizeof(RealmName) - 1);
    else
        StringCbCopyN( RealmName, sizeof(RealmName),
                       realm, sizeof(RealmName) - 1);

    code = KRB5KRB_ERR_GENERIC;

    increds.client = client_principal;
    increds.times.endtime = 0;

    /* ALWAYS first try service/cell@CLIENT_REALM */
    if (code = krb5_build_principal(context, &increds.server,
                                    (int)strlen(realm_of_user),
                                    realm_of_user,
                                    ServiceName,
                                    CellName,
                                    0))
    {
        goto cleanup;
    }

    if ( IsDebuggerPresent() ) {
        char * cname, *sname;
        krb5_unparse_name(context, increds.client, &cname);
        krb5_unparse_name(context, increds.server, &sname);
        OutputDebugString("Getting tickets for \"");
        OutputDebugString(cname);
        OutputDebugString("\" and service \"");
        OutputDebugString(sname);
        OutputDebugString("\"\n");
        krb5_free_unparsed_name(context,cname);
        krb5_free_unparsed_name(context,sname);
    }

    code = krb5_get_credentials(context, 0, cc, &increds, &k5creds);
    if (code == 0) {
        /*
         * The client's realm is a local realm for the cell.
         * Save it so that later the pts registration will not
         * be performed.
         */
        StringCbCopyN(realm_of_cell, sizeof(realm_of_cell),
                      realm_of_user, sizeof(realm_of_cell) - 1);
    }

    if (KFW_AFS_continue_aklog_processing_after_krb5_error(code)) {
        if (strcmp(realm_of_user, RealmName)) {
            /* service/cell@REALM */
            increds.server = 0;
            code = krb5_build_principal(context, &increds.server,
                                        (int)strlen(RealmName),
                                        RealmName,
                                        ServiceName,
                                        CellName,
                                        0);

            if ( IsDebuggerPresent() ) {
                char * cname, *sname;
                krb5_unparse_name(context, increds.client, &cname);
                krb5_unparse_name(context, increds.server, &sname);
                OutputDebugString("Getting tickets for \"");
                OutputDebugString(cname);
                OutputDebugString("\" and service \"");
                OutputDebugString(sname);
                OutputDebugString("\"\n");
                krb5_free_unparsed_name(context,cname);
                krb5_free_unparsed_name(context,sname);
            }

            if (!code)
                code = krb5_get_credentials(context, 0, cc, &increds, &k5creds);
        }

        if (KFW_AFS_continue_aklog_processing_after_krb5_error(code)) {
            /* Or service@REALM */
            krb5_free_principal(context,increds.server);
            increds.server = 0;
            code = krb5_build_principal(context, &increds.server,
                                        (int)strlen(RealmName),
                                        RealmName,
                                        ServiceName,
                                        0);

            if ( IsDebuggerPresent() ) {
                char * cname, *sname;
                krb5_unparse_name(context, increds.client, &cname);
                krb5_unparse_name(context, increds.server, &sname);
                DebugPrintf("Getting tickets for \"%s\" and service \"%s\"\n", cname, sname);
                krb5_free_unparsed_name(context,cname);
                krb5_free_unparsed_name(context,sname);
            }

            if (!code)
                code = krb5_get_credentials(context, 0, cc, &increds, &k5creds);
        }
    }
    
    if (!code)
        copy_realm_of_ticket(context, realm_of_cell, sizeof(realm_of_cell), k5creds);

    if (code) {
        DebugPrintf("krb5_get_credentials returns: %d\n", code);
        goto cleanup;
    }

    /* This code inserts the entire K5 ticket into the token */
    memset(&aserver, '\0', sizeof(aserver));
    StringCbCopyN(aserver.name, sizeof(aserver.name),
                  ServiceName, sizeof(aserver.name) - 1);
    StringCbCopyN(aserver.cell, sizeof(aserver.cell),
                  CellName, sizeof(aserver.cell) - 1);

    memset(&atoken, '\0', sizeof(atoken));
    atoken.kvno = RXKAD_TKT_TYPE_KERBEROS_V5;
    atoken.startTime = k5creds->times.starttime;
    atoken.endTime = k5creds->times.endtime;
    if (tkt_DeriveDesKey(k5creds->session.keytype, k5creds->session.keyvalue.data,
			 k5creds->session.keyvalue.length, &atoken.sessionKey))
	goto cleanup;
    atoken.ticketLen = k5creds->ticket.length;
    memcpy(atoken.ticket, k5creds->ticket.data, atoken.ticketLen);

  retry_gettoken5:
    rc = ktc_GetToken(&aserver, &btoken, sizeof(btoken), &aclient);
    if ( IsDebuggerPresent() ) {
        char message[256];
        StringCbPrintf(message, sizeof(message), "ktc_GetToken returns: %d\n", rc);
        OutputDebugString(message);
    }
    if (rc != 0 && rc != KTC_NOENT && rc != KTC_NOCELL) {
        if ( rc == KTC_NOCM && retry < 20 ) {
            Sleep(500);
            retry++;
            goto retry_gettoken5;
        }
        goto cleanup;
    }

    if (atoken.kvno == btoken.kvno &&
         atoken.ticketLen == btoken.ticketLen &&
         !memcmp(&atoken.sessionKey, &btoken.sessionKey, sizeof(atoken.sessionKey)) &&
         !memcmp(atoken.ticket, btoken.ticket, atoken.ticketLen))
    {
        /* Success - Nothing to do */
        goto cleanup;
    }

    // * Reset the "aclient" structure before we call ktc_SetToken.
    // * This structure was first set by the ktc_GetToken call when
    // * we were comparing whether identical tokens already existed.

    StringCbCopy(aclient.name, sizeof(aclient.name),
                 krb5_principal_get_comp_string(context, k5creds->client, 0));

    if ( krb5_principal_get_num_comp(context, k5creds->client) > 1 ) {
        StringCbCat(aclient.name, sizeof(aclient.name), ".");
        StringCbCat(aclient.name, sizeof(aclient.name),
                    krb5_principal_get_comp_string(context, k5creds->client, 1));
    }
    aclient.instance[0] = '\0';

    StringCbCopyN(aclient.cell, sizeof(aclient.cell),
                  realm_of_cell, sizeof(aclient.cell) - 1);

    /* For Khimaira, always append the realm name */
    StringCbCat(aclient.name, sizeof(aclient.name), "@");
    StringCbCat(aclient.name, sizeof(aclient.name),
                krb5_principal_get_realm(context, k5creds->client));

    GetEnvironmentVariable(DO_NOT_REGISTER_VARNAME, NULL, 0);
    if (GetLastError() == ERROR_ENVVAR_NOT_FOUND)
        ViceIDToUsername(aclient.name, realm_of_user, realm_of_cell, CellName,
                         &aclient, &aserver, &atoken);

    if ( smbname ) {
        StringCbCopyN(aclient.smbname, sizeof(aclient.smbname),
                      smbname, sizeof(aclient.smbname) - 1);
    } else {
        aclient.smbname[0] = '\0';
    }
    if ( IsDebuggerPresent() ) {
        char message[256];
        StringCbPrintf(message, sizeof(message), "aclient.name: %s\n", aclient.name);
        OutputDebugString(message);
        StringCbPrintf(message, sizeof(message), "aclient.smbname: %s\n", aclient.smbname);
        OutputDebugString(message);
    }

    rc = ktc_SetToken(&aserver, &atoken, &aclient, (aclient.smbname[0]?AFS_SETTOK_LOGON:0));

cleanup:
    if (client_principal)
        krb5_free_principal(context,client_principal);
    /* increds.client == client_principal */
    if (increds.server)
        krb5_free_principal(context,increds.server);
    if (cc && (cc != alt_cc))
        krb5_cc_close(context, cc);
    if (context && (context != alt_context))
        krb5_free_context(context);
    if (ak_cellconfig.linkedCell)
        free(ak_cellconfig.linkedCell);

    return(rc? rc : code);
}

/**************************************/
/* afs_realm_of_cell():               */
/**************************************/
static char *
afs_realm_of_cell(krb5_context context, struct afsconf_cell *cellconfig)
{
    static char krbrlm[REALM_SZ+1]="";
    char ** realmlist=NULL;
    krb5_error_code r;

    if (!cellconfig)
        return 0;

    r = krb5_get_host_realm(context, cellconfig->hostName[0], &realmlist);
    if ( !r && realmlist && realmlist[0] ) {
        StringCbCopyN( krbrlm, sizeof(krbrlm),
                       realmlist[0], sizeof(krbrlm) - 1);
        krb5_free_host_realm(context, realmlist);
    }

    if ( !krbrlm[0] )
    {
        char *s = krbrlm;
        char *t = cellconfig->name;
        int c;

        while (c = *t++)
        {
            if (islower(c)) c=toupper(c);
            *s++ = c;
        }
        *s++ = 0;
    }
    return(krbrlm);
}

/**************************************/
/* KFW_AFS_get_cellconfig():          */
/**************************************/
int
KFW_AFS_get_cellconfig(char *cell, struct afsconf_cell *cellconfig, char *local_cell)
{
    int	rc;
    char newcell[CELL_MAXNAMELEN+1];
    char linkedcell[CELL_MAXNAMELEN+1]="";

    local_cell[0] = (char)0;
    memset(cellconfig, 0, sizeof(*cellconfig));

    /* WIN32: cm_GetRootCellName(local_cell) - NOTE: no way to get max chars */
    if (rc = cm_GetRootCellName(local_cell))
    {
        return(rc);
    }

    if (strlen(cell) == 0)
        StringCbCopy(cell, CELL_MAXNAMELEN, local_cell);

    rc = cm_SearchCellRegistry(1, cell, newcell, linkedcell, get_cellconfig_callback, (void*)cellconfig);
    if (rc && rc != CM_ERROR_FORCE_DNS_LOOKUP)
        rc = cm_SearchCellFileEx(cell, newcell, linkedcell, get_cellconfig_callback, (void*)cellconfig);
    if (rc != 0) {
        int ttl;
        rc = cm_SearchCellByDNS(cell, newcell, &ttl, get_cellconfig_callback, (void*)cellconfig);
    }

    if (rc == 0) {
        StringCbCopyN( cellconfig->name, sizeof(cellconfig->name),
                       newcell, sizeof(cellconfig->name) - 1);
        if (linkedcell[0])
            cellconfig->linkedCell = strdup(linkedcell);
    }
    return rc;
}

/**************************************/
/* get_cellconfig_callback():         */
/**************************************/
static long
get_cellconfig_callback(void *cellconfig, struct sockaddr_in *addrp, char *namep, unsigned short ipRank)
{
    struct afsconf_cell *cc = (struct afsconf_cell *)cellconfig;

    cc->hostAddr[cc->numServers] = *addrp;
    StringCbCopyN( cc->hostName[cc->numServers], sizeof(cc->hostName[cc->numServers]),
                   namep, sizeof(cc->hostName[cc->numServers]) - 1);
    cc->numServers++;
    return(0);
}


/**************************************/
/* KFW_AFS_error():                  */
/**************************************/
void
KFW_AFS_error(LONG rc, LPCSTR FailedFunctionName)
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

    StringCbPrintf(message, sizeof(message), "%s (0x%x)\n(%s failed)", errText, rc, FailedFunctionName);

    if ( IsDebuggerPresent() ) {
        OutputDebugString(message);
        OutputDebugString("\n");
    }
    MessageBox(NULL, message, "AFS", MB_OK | MB_ICONERROR | MB_TASKMODAL | MB_SETFOREGROUND);
    return;
}

static DWORD
GetServiceStatus(
    LPSTR lpszMachineName,
    LPSTR lpszServiceName,
    DWORD *lpdwCurrentState)
{
    DWORD           hr               = NOERROR;
    SC_HANDLE       schSCManager     = NULL;
    SC_HANDLE       schService       = NULL;
    DWORD           fdwDesiredAccess = 0;
    SERVICE_STATUS  ssServiceStatus  = {0};
    BOOL            fRet             = FALSE;

    *lpdwCurrentState = 0;

    fdwDesiredAccess = GENERIC_READ;

    schSCManager = OpenSCManager(lpszMachineName,
                                 NULL,
                                 fdwDesiredAccess);

    if(schSCManager == NULL)
    {
        hr = GetLastError();
        goto cleanup;
    }

    schService = OpenService(schSCManager,
                             lpszServiceName,
                             fdwDesiredAccess);

    if(schService == NULL)
    {
        hr = GetLastError();
        goto cleanup;
    }

    fRet = QueryServiceStatus(schService,
                              &ssServiceStatus);

    if(fRet == FALSE)
    {
        hr = GetLastError();
        goto cleanup;
    }

    *lpdwCurrentState = ssServiceStatus.dwCurrentState;

cleanup:

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);

    return(hr);
}

BOOL KFW_probe_kdc(struct afsconf_cell * cellconfig)
{
    krb5_context context = NULL;
    krb5_ccache cc = NULL;
    krb5_error_code code;
    krb5_data pwdata;
    const char * realm = NULL;
    krb5_principal principal = NULL;
    char * pname = NULL;
    char   password[PROBE_PASSWORD_LEN+1];
    BOOL serverReachable = 0;

    code = krb5_init_context(&context);
    if (code) goto cleanup;


    realm = afs_realm_of_cell(context, cellconfig);  // do not free

    code = krb5_build_principal(context, &principal, (int)strlen(realm),
                                  realm, PROBE_USERNAME, NULL, NULL);
    if ( code ) goto cleanup;

    code = KFW_get_ccache(context, principal, &cc);
    if ( code ) goto cleanup;

    code = krb5_unparse_name(context, principal, &pname);
    if ( code ) goto cleanup;

    pwdata.data = password;
    pwdata.length = PROBE_PASSWORD_LEN;
    krb5_c_random_make_octets(context, &pwdata);
    {
        int i;
        for ( i=0 ; i<PROBE_PASSWORD_LEN ; i++ )
            if (password[i] == '\0')
                password[i] = 'x';
    }
    password[PROBE_PASSWORD_LEN] = '\0';

    code = KFW_kinit(NULL, NULL, HWND_DESKTOP,
                      pname,
                      password,
                      5,
                      0,
                      0,
                      0,
                      1,
                      0);
    switch ( code ) {
    case KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN:
    case KRB5KDC_ERR_CLIENT_REVOKED:
    case KRB5KDC_ERR_CLIENT_NOTYET:
    case KRB5KDC_ERR_PREAUTH_FAILED:
    case KRB5KDC_ERR_PREAUTH_REQUIRED:
    case KRB5KDC_ERR_PADATA_TYPE_NOSUPP:
        serverReachable = TRUE;
        break;
    default:
        serverReachable = FALSE;
    }

  cleanup:
    if ( pname )
        krb5_free_unparsed_name(context,pname);
    if ( principal )
        krb5_free_principal(context,principal);
    if (cc)
        krb5_cc_close(context,cc);
    if (context)
        krb5_free_context(context);

    return serverReachable;
}

BOOL
KFW_AFS_get_lsa_principal(char * szUser, DWORD *dwSize)
{
    krb5_context   context = NULL;
    krb5_error_code code;
    krb5_ccache mslsa_ccache=NULL;
    krb5_principal princ = NULL;
    char * pname = NULL;
    BOOL success = 0;

    if (!KFW_is_available())
        return FALSE;

    if (code = krb5_init_context(&context))
        goto cleanup;

    if (code = krb5_cc_resolve(context, "MSLSA:", &mslsa_ccache))
        goto cleanup;

    if (code = krb5_cc_get_principal(context, mslsa_ccache, &princ))
        goto cleanup;

    if (code = krb5_unparse_name(context, princ, &pname))
        goto cleanup;

    if ( strlen(pname) < *dwSize ) {
        StringCbCopyN(szUser, *dwSize, pname, (*dwSize) - 1);
        success = 1;
    }
    *dwSize = (DWORD)strlen(pname);

  cleanup:
    if (pname)
        krb5_free_unparsed_name(context, pname);

    if (princ)
        krb5_free_principal(context, princ);

    if (mslsa_ccache)
        krb5_cc_close(context, mslsa_ccache);

    if (context)
        krb5_free_context(context);
    return success;
}

int
KFW_AFS_set_file_cache_dacl(char *filename, HANDLE hUserToken)
{
    // SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_SID_AUTHORITY;
    PSID pSystemSID = NULL;
    DWORD SystemSIDlength = 0, UserSIDlength = 0;
    PACL ccacheACL = NULL;
    DWORD ccacheACLlength = 0;
    PTOKEN_USER pTokenUser = NULL;
    DWORD retLen;
    DWORD gle;
    int ret = 0;

    if (!filename) {
	return 1;
    }

    /* Get System SID */
    if (!ConvertStringSidToSid("S-1-5-18", &pSystemSID)) {
	ret = 1;
	goto cleanup;
    }

    /* Create ACL */
    SystemSIDlength = GetLengthSid(pSystemSID);
    ccacheACLlength = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE)
        + SystemSIDlength - sizeof(DWORD);

    if (hUserToken) {
	if (!GetTokenInformation(hUserToken, TokenUser, NULL, 0, &retLen))
	{
	    if ( GetLastError() == ERROR_INSUFFICIENT_BUFFER ) {
		pTokenUser = (PTOKEN_USER) LocalAlloc(LPTR, retLen);

		GetTokenInformation(hUserToken, TokenUser, pTokenUser, retLen, &retLen);
	    }
	}

	if (pTokenUser) {
	    UserSIDlength = GetLengthSid(pTokenUser->User.Sid);

	    ccacheACLlength += sizeof(ACCESS_ALLOWED_ACE) + UserSIDlength
		- sizeof(DWORD);
	}
    }

    ccacheACL = (PACL) LocalAlloc(LPTR, ccacheACLlength);
    if (!ccacheACL) {
 	ret = 1;
 	goto cleanup;
     }
    InitializeAcl(ccacheACL, ccacheACLlength, ACL_REVISION);
    AddAccessAllowedAceEx(ccacheACL, ACL_REVISION, 0,
                         STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL,
                         pSystemSID);
    if (pTokenUser) {
	AddAccessAllowedAceEx(ccacheACL, ACL_REVISION, 0,
			     STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL,
			     pTokenUser->User.Sid);
	if (!SetNamedSecurityInfo( filename, SE_FILE_OBJECT,
				   DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
				   NULL,
				   NULL,
				   ccacheACL,
				   NULL)) {
 	    gle = GetLastError();
 	    if (gle != ERROR_NO_TOKEN)
		ret = 1;
	}
	if (!SetNamedSecurityInfo( filename, SE_FILE_OBJECT,
				   OWNER_SECURITY_INFORMATION,
				   pTokenUser->User.Sid,
				   NULL,
				   NULL,
				   NULL)) {
 	    gle = GetLastError();
 	    if (gle != ERROR_NO_TOKEN)
		ret = 1;
	}
    } else {
	if (!SetNamedSecurityInfo( filename, SE_FILE_OBJECT,
				   DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
				   NULL,
				   NULL,
				   ccacheACL,
				   NULL)) {
 	    gle = GetLastError();
 	    if (gle != ERROR_NO_TOKEN)
		ret = 1;
	}
    }

  cleanup:
    if (pSystemSID)
	LocalFree(pSystemSID);
    if (pTokenUser)
	LocalFree(pTokenUser);
    if (ccacheACL)
	LocalFree(ccacheACL);
    return ret;
}

int
KFW_AFS_obtain_user_temp_directory(HANDLE hUserToken, char *newfilename, int size)
{
    int  retval = 0;
    DWORD dwSize = size-1;	/* leave room for nul */
    DWORD dwLen  = 0;

    if (!hUserToken || !newfilename || size <= 0)
 	return 1;

     *newfilename = '\0';

     dwLen = ExpandEnvironmentStringsForUser(hUserToken, "%TEMP%", newfilename, dwSize);
     if ( !dwLen || dwLen > dwSize )
 	dwLen = ExpandEnvironmentStringsForUser(hUserToken, "%TMP%", newfilename, dwSize);
     if ( !dwLen || dwLen > dwSize )
 	return 1;

     newfilename[dwSize] = '\0';
    return 0;
}

void
KFW_AFS_copy_cache_to_system_file(char * user, char * szLogonId)
{
    char filename[MAX_PATH] = "";
    DWORD count;
    char cachename[MAX_PATH + 8] = "FILE:";
    krb5_context		context = NULL;
    krb5_error_code		code;
    krb5_principal              princ = NULL;
    krb5_ccache			cc  = NULL;
    krb5_ccache                 ncc = NULL;

    if (!user || !szLogonId)
        return;

    count = GetEnvironmentVariable("TEMP", filename, sizeof(filename));
    if ( count > sizeof(filename) || count == 0 ) {
        GetWindowsDirectory(filename, sizeof(filename));
    }

    if ( strlen(filename) + strlen(szLogonId) + 2 > sizeof(filename) )
        return;

    StringCbCat( filename, sizeof(filename), "\\");
    StringCbCat( filename, sizeof(filename), szLogonId);

    StringCbCat( cachename, sizeof(cachename), filename);

    DeleteFile(filename);

    code = krb5_init_context(&context);
    if (code) goto cleanup;

    code = krb5_parse_name(context, user, &princ);
    if (code) goto cleanup;

    code = KFW_get_ccache(context, princ, &cc);
    if (code) goto cleanup;

    code = krb5_cc_resolve(context, cachename, &ncc);
    if (code) goto cleanup;

    code = krb5_cc_initialize(context, ncc, princ);
    if (code) goto cleanup;

    code = KFW_AFS_set_file_cache_dacl(filename, NULL);
    if (code) goto cleanup;

    code = krb5_cc_copy_creds(context,cc,ncc);

  cleanup:
    if ( cc ) {
        krb5_cc_close(context, cc);
        cc = 0;
    }
    if ( ncc ) {
        krb5_cc_close(context, ncc);
        ncc = 0;
    }
    if ( princ ) {
        krb5_free_principal(context, princ);
        princ = 0;
    }

    if (context)
        krb5_free_context(context);
}

int
KFW_AFS_copy_file_cache_to_default_cache(char * filename)
{
    char cachename[MAX_PATH + 8] = "FILE:";
    krb5_context		context = NULL;
    krb5_error_code		code;
    krb5_principal              princ = NULL;
    krb5_ccache			cc  = NULL;
    krb5_ccache                 ncc = NULL;
    int retval = 1;

    if (!filename)
        return 1;

    if ( strlen(filename) + sizeof("FILE:") > sizeof(cachename) )
        return 1;

    code = krb5_init_context(&context);
    if (code) return 1;

    StringCbCat( cachename, sizeof(cachename), filename);

    code = krb5_cc_resolve(context, cachename, &cc);
    if (code) goto cleanup;

    code = krb5_cc_get_principal(context, cc, &princ);

    code = krb5_cc_default(context, &ncc);
    if (!code) {
        code = krb5_cc_initialize(context, ncc, princ);

        if (!code)
            code = krb5_cc_copy_creds(context,cc,ncc);
    }
    if ( ncc ) {
        krb5_cc_close(context, ncc);
        ncc = 0;
    }

    retval=0;   /* success */

  cleanup:
    if ( cc ) {
        krb5_cc_close(context, cc);
        cc = 0;
    }

    DeleteFile(filename);

    if ( princ ) {
        krb5_free_principal(context, princ);
        princ = 0;
    }

    if (context)
        krb5_free_context(context);

    return 0;
}

char *
KFW_get_default_realm(void)
{
    krb5_context ctx = NULL;
    char * def_realm = NULL, *realm = NULL;
    krb5_error_code code;

    code = krb5_init_context(&ctx);
    if (code)
        return NULL;

    if (code = krb5_get_default_realm(ctx, &def_realm))
        goto cleanup;

    realm = strdup(def_realm);

  cleanup:
    if (def_realm)
        krb5_free_default_realm(ctx, def_realm);

    if (ctx)
        krb5_free_context(ctx);

    return realm;
}

/* We are including this

/* Ticket lifetime.  This defines the table used to lookup lifetime for the
   fixed part of rande of the one byte lifetime field.  Values less than 0x80
   are intrpreted as the number of 5 minute intervals.  Values from 0x80 to
   0xBF should be looked up in this table.  The value of 0x80 is the same using
   both methods: 10 and two-thirds hours .  The lifetime of 0xBF is 30 days.
   The intervening values of have a fixed ratio of roughly 1.06914.  The value
   oxFF is defined to mean a ticket has no expiration time.  This should be
   used advisedly since individual servers may impose defacto upperbounds on
   ticket lifetimes. */

#define TKTLIFENUMFIXED 64
#define TKTLIFEMINFIXED 0x80
#define TKTLIFEMAXFIXED 0xBF
#define TKTLIFENOEXPIRE 0xFF
#define MAXTKTLIFETIME	(30*24*3600)	/* 30 days */

static const int tkt_lifetimes[TKTLIFENUMFIXED] = {
    38400,			/* 10.67 hours, 0.44 days */
    41055,			/* 11.40 hours, 0.48 days */
    43894,			/* 12.19 hours, 0.51 days */
    46929,			/* 13.04 hours, 0.54 days */
    50174,			/* 13.94 hours, 0.58 days */
    53643,			/* 14.90 hours, 0.62 days */
    57352,			/* 15.93 hours, 0.66 days */
    61318,			/* 17.03 hours, 0.71 days */
    65558,			/* 18.21 hours, 0.76 days */
    70091,			/* 19.47 hours, 0.81 days */
    74937,			/* 20.82 hours, 0.87 days */
    80119,			/* 22.26 hours, 0.93 days */
    85658,			/* 23.79 hours, 0.99 days */
    91581,			/* 25.44 hours, 1.06 days */
    97914,			/* 27.20 hours, 1.13 days */
    104684,			/* 29.08 hours, 1.21 days */
    111922,			/* 31.09 hours, 1.30 days */
    119661,			/* 33.24 hours, 1.38 days */
    127935,			/* 35.54 hours, 1.48 days */
    136781,			/* 37.99 hours, 1.58 days */
    146239,			/* 40.62 hours, 1.69 days */
    156350,			/* 43.43 hours, 1.81 days */
    167161,			/* 46.43 hours, 1.93 days */
    178720,			/* 49.64 hours, 2.07 days */
    191077,			/* 53.08 hours, 2.21 days */
    204289,			/* 56.75 hours, 2.36 days */
    218415,			/* 60.67 hours, 2.53 days */
    233517,			/* 64.87 hours, 2.70 days */
    249664,			/* 69.35 hours, 2.89 days */
    266926,			/* 74.15 hours, 3.09 days */
    285383,			/* 79.27 hours, 3.30 days */
    305116,			/* 84.75 hours, 3.53 days */
    326213,			/* 90.61 hours, 3.78 days */
    348769,			/* 96.88 hours, 4.04 days */
    372885,			/* 103.58 hours, 4.32 days */
    398668,			/* 110.74 hours, 4.61 days */
    426234,			/* 118.40 hours, 4.93 days */
    455705,			/* 126.58 hours, 5.27 days */
    487215,			/* 135.34 hours, 5.64 days */
    520904,			/* 144.70 hours, 6.03 days */
    556921,			/* 154.70 hours, 6.45 days */
    595430,			/* 165.40 hours, 6.89 days */
    636601,			/* 176.83 hours, 7.37 days */
    680618,			/* 189.06 hours, 7.88 days */
    727680,			/* 202.13 hours, 8.42 days */
    777995,			/* 216.11 hours, 9.00 days */
    831789,			/* 231.05 hours, 9.63 days */
    889303,			/* 247.03 hours, 10.29 days */
    950794,			/* 264.11 hours, 11.00 days */
    1016537,			/* 282.37 hours, 11.77 days */
    1086825,			/* 301.90 hours, 12.58 days */
    1161973,			/* 322.77 hours, 13.45 days */
    1242318,			/* 345.09 hours, 14.38 days */
    1328218,			/* 368.95 hours, 15.37 days */
    1420057,			/* 394.46 hours, 16.44 days */
    1518247,			/* 421.74 hours, 17.57 days */
    1623226,			/* 450.90 hours, 18.79 days */
    1735464,			/* 482.07 hours, 20.09 days */
    1855462,			/* 515.41 hours, 21.48 days */
    1983758,			/* 551.04 hours, 22.96 days */
    2120925,			/* 589.15 hours, 24.55 days */
    2267576,			/* 629.88 hours, 26.25 days */
    2424367,			/* 673.44 hours, 28.06 days */
    2592000
};				/* 720.00 hours, 30.00 days */

/* life_to_time - takes a start time and a Kerberos standard lifetime char and
 * returns the corresponding end time.  There are four simple cases to be
 * handled.  The first is a life of 0xff, meaning no expiration, and results in
 * an end time of 0xffffffff.  The second is when life is less than the values
 * covered by the table.  In this case, the end time is the start time plus the
 * number of 5 minute intervals specified by life.  The third case returns
 * start plus the MAXTKTLIFETIME if life is greater than TKTLIFEMAXFIXED.  The
 * last case, uses the life value (minus TKTLIFEMINFIXED) as an index into the
 * table to extract the lifetime in seconds, which is added to start to produce
 * the end time. */

afs_uint32
life_to_time(afs_uint32 start, unsigned char life)
{
    int realLife;

    if (life == TKTLIFENOEXPIRE)
	return NEVERDATE;
    if (life < TKTLIFEMINFIXED)
	return start + life * 5 * 60;
    if (life > TKTLIFEMAXFIXED)
	return start + MAXTKTLIFETIME;
    realLife = tkt_lifetimes[life - TKTLIFEMINFIXED];
    return start + realLife;
}

/* time_to_life - takes start and end times for the ticket and returns a
 * Kerberos standard lifetime char possibily using the tkt_lifetimes table for
 * lifetimes above 127*5minutes.  First, the special case of (end ==
 * 0xffffffff) is handled to mean no expiration.  Then negative lifetimes and
 * those greater than the maximum ticket lifetime are rejected.  Then lifetimes
 * less than the first table entry are handled by rounding the requested
 * lifetime *up* to the next 5 minute interval.  The final step is to search
 * the table for the smallest entry *greater than or equal* to the requested
 * entry.  The actual code is prepared to handle the case where the table is
 * unordered but that it an unnecessary frill. */

static unsigned char
time_to_life(afs_uint32 start, afs_uint32 end)
{
    int lifetime = end - start;
    int best, best_i;
    int i;

    if (end == NEVERDATE)
	return TKTLIFENOEXPIRE;
    if ((lifetime > MAXKTCTICKETLIFETIME) || (lifetime <= 0))
	return 0;
    if (lifetime < tkt_lifetimes[0])
	return (lifetime + 5 * 60 - 1) / (5 * 60);
    best_i = -1;
    best = MAXKTCTICKETLIFETIME;
    for (i = 0; i < TKTLIFENUMFIXED; i++)
	if (tkt_lifetimes[i] >= lifetime) {
	    int diff = tkt_lifetimes[i] - lifetime;
	    if (diff < best) {
		best = diff;
		best_i = i;
	    }
	}
    if (best_i < 0)
	return 0;
    return best_i + TKTLIFEMINFIXED;
}

DWORD KFW_get_default_mslsa_import(krb5_context context)
{
    static const char * lsh_settings_key = "";
    static const char * lsh_mslsa_value = "";
    DWORD import = 0;
    HKEY hKey;
    DWORD dwCount;
    LONG rc;

    rc = RegOpenKeyEx(HKEY_CURRENT_USER, lsh_settings_key, 0, KEY_QUERY_VALUE, &hKey);
    if (rc)
        return import;

    dwCount = sizeof(DWORD);
    rc = RegQueryValueEx(hKey, lsh_mslsa_value, 0, 0, (LPBYTE) &import, &dwCount);
    RegCloseKey(hKey);

    if (rc == 0)
        return import;

    rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, lsh_settings_key, 0, KEY_QUERY_VALUE, &hKey);
    if (rc)
        return import;

    dwCount = sizeof(DWORD);
    rc = RegQueryValueEx(hKey, lsh_mslsa_value, 0, 0, (LPBYTE) &import, &dwCount);
    RegCloseKey(hKey);

    return import;
}

DWORD KFW_get_default_lifetime(krb5_context context, const char * realm)
{
    static const char * lifetime_val_name = "ticket_lifetime";
    time_t t = 0;

    krb5_appdefault_time(context, "aklog", realm, lifetime_val_name, 0, &t);

    if (t == 0)
        t = krb5_config_get_time_default(context, NULL, 0,
                                         "realms", realm, lifetime_val_name, NULL);

    if (t == 0)
        t = krb5_config_get_time_default(context, NULL, 0,
                                         "libdefaults", lifetime_val_name, NULL);

    if (t == 0)
        t = 10 * 60 * 60;

    return (DWORD) t;
}

