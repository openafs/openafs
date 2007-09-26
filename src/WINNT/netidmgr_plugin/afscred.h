/*
 * Copyright (c) 2005,2006 Secure Endpoints Inc.
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

#ifndef __KHIMAIRA_AFSCRED_H
#define __KHIMAIRA_AFSCRED_H

#ifndef _WIN64
#define _USE_32BIT_TIME_T 1
#endif

#define _WINSOCKAPI_
#include<windows.h>
#include<time.h>

#define KHERR_FACILITY L"AfsCred"
#define KHERR_HMODULE hResModule
#include<netidmgr.h>

#include<langres.h>

#include <afs/cm_config.h>
#include <afs/stds.h>
#include <afs/auth.h>
#include <afs/ptserver.h>
#include <afs/ptuser.h>

#include<afspext.h>

#include<afsfuncs.h>
#include<afsnewcreds.h>

#ifndef NOSTRSAFE
#include<strsafe.h>
#endif

#define AFS_PLUGIN_NAME         L"AfsCred"
#define AFS_CREDTYPE_NAME       L"AfsCred"

#define AFS_PLUGIN_DEPS         L"Krb5Cred\0"

#define KRB5_CREDTYPE_NAME      L"Krb5Cred"
#define KRB4_CREDTYPE_NAME      L"Krb4Cred"

#define AFS_TYPENAME_PRINCIPAL      L"AFSPrincipal"
#define AFS_TYPENAME_METHOD         L"AFSTokenMethod"
#define AFS_ATTRNAME_CLIENT_PRINC   L"AFSClientPrinc"
#define AFS_ATTRNAME_SERVER_PRINC   L"AFSServerPrinc"
#define AFS_ATTRNAME_CELL           L"AFSCell"
#define AFS_ATTRNAME_METHOD         L"AFSMethod"
#define AFS_ATTRNAME_REALM          L"AFSRealm"

#define AFS_VALID_CELL_CHARS    L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-"
#define AFS_VALID_REALM_CHARS   AFS_VALID_CELL_CHARS

#define AFS_CONFIG_NODE_IDS     L"AfsIdentities"
#define AFS_CONFIG_NODE_ID      L"AfsIdentity"
#define AFS_CONFIG_NODE_MAIN    L"AfsOptions"

#define AFS_HELPFILE            L"afsplhlp.chm"

/* token acquisition methods provided by extensions begin with this
   ID */
#define AFS_TOKEN_USER          8

void init_afs();
void exit_afs();
KHMEXP khm_int32 KHMAPI init_module(kmm_module h_module);
KHMEXP khm_int32 KHMAPI exit_module(kmm_module h_module);

/* globals */
extern kmm_module h_khModule;
extern HMODULE hResModule;
extern HINSTANCE hInstance;

extern khm_int32 afs_credtype_id;
extern khm_int32 krb5_credtype_id;
extern khm_int32 krb4_credtype_id;
extern khm_int32 afs_msg_type_id;
extern khm_handle afs_credset;

extern khm_int32 afs_type_principal;
extern khm_int32 afs_attr_client_princ;
extern khm_int32 afs_attr_server_princ;
extern khm_int32 afs_attr_cell;
extern khm_int32 afs_attr_method;
extern khm_int32 afs_attr_realm;

/* Configuration spaces */
#define CSNAME_PLUGINS      L"Plugins"
#define CSNAME_AFSCRED      L"AfsCred"
#define CSNAME_PARAMS       L"Parameters"

extern khm_handle csp_plugins;
extern khm_handle csp_afscred;
extern khm_handle csp_params;

extern khm_handle afs_sub;

/* defined in afsconfig.c which is generated from afsconfig.csv */
extern kconf_schema schema_afsconfig[];


/* plugin callback procedure */
khm_int32 KHMAPI 
afs_plugin_cb(khm_int32 msg_type,
              khm_int32 msg_subtype,
              khm_ui_4 uparam,
              void * vparam);

INT_PTR CALLBACK
afs_cfg_ids_proc(HWND hwnd,
                 UINT uMsg,
                 WPARAM wParam,
                 LPARAM lParam);

INT_PTR CALLBACK
afs_cfg_id_proc(HWND hwnd,
                UINT uMsg,
                WPARAM wParam,
                LPARAM lParam);

INT_PTR CALLBACK
afs_cfg_main_proc(HWND hwnd,
                  UINT uMsg,
                  WPARAM wParam,
                  LPARAM lParam);

HWND
afs_html_help(HWND caller,
              wchar_t * postfix,
              UINT cmd,
              DWORD_PTR data);

/* extensions */
typedef afs_msg_announce afs_extension;

/* not thread safe. only call from the plugin thread */
afs_extension *
afs_find_extension(const wchar_t * name);

/* not thread safe. only call from the plugin thread */
afs_extension *
afs_get_extension(khm_size i);

/* not thread safe.  only call from the plugin thread */
afs_extension *
afs_get_next_token_acq(afs_extension * f);

/* not thread safe.  only call from the plugin thread */
khm_boolean
afs_is_valid_method_id(afs_tk_method method);

afs_tk_method
afs_get_next_method_id(afs_tk_method method);

afs_tk_method
afs_get_method_id(wchar_t * name);

khm_boolean
afs_get_method_name(afs_tk_method method, wchar_t * buf, khm_size cbbuf);

afs_extension *
afs_get_method_ext(afs_tk_method method);

khm_boolean
afs_method_describe(afs_tk_method method, khm_int32 flags,
                    wchar_t * wbuf, khm_size cbbuf);

khm_boolean
afs_ext_resolve_token(const wchar_t * cell,
                      const struct ktc_token * token,
                      const struct ktc_principal * serverp,
                      const struct ktc_principal * clientp,
                      khm_handle * pident,
                      afs_tk_method * pmethod);

khm_boolean
afs_ext_klog(afs_tk_method method,
             khm_handle   identity,
             const char * service,
             const char * cell,
             const char * realm,
             const afs_conf_cell * cell_config,
             khm_int32    lifetime);

BOOL
afs_cfg_get_afscreds_shortcut(wchar_t * wpath);

#endif
