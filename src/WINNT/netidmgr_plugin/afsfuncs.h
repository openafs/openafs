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

#ifndef __KHIMAIRA_AFSFUNCS_H
#define __KHIMAIRA_AFSFUNCS_H


BOOL
afs_is_running(void);

int 
afs_princ_to_string(struct ktc_principal * p, wchar_t * buf, size_t cbbuf);

int 
afs_list_tokens(void);

khm_handle
afs_find_token(khm_handle credset, wchar_t * cell);

int 
afs_list_tokens_internal(void);

int 
afs_klog(khm_handle identity,
         char *service,
         char *cell,
         char *realm,
         int LifeTime,
         afs_tk_method method,
         time_t * tok_expiration /* OUT: expiration time of new
                                    token */
         );

int
afs_unlog(void);

int
afs_unlog_cred(khm_handle cred);

DWORD 
GetServiceStatus(LPSTR lpszMachineName, 
                 LPSTR lpszServiceName, 
                 DWORD *lpdwCurrentState,
                 DWORD *lpdwWaitHint);

DWORD 
ServiceControl(LPSTR lpszMachineName, 
               LPSTR lpszServiceName,
               DWORD dwNewState);

void afs_report_error(LONG rc, LPCSTR FailedFunctionName);

static char *afs_realm_of_cell(afs_conf_cell *);
static long afs_get_cellconfig_callback(void *, struct sockaddr_in *, char *);
static int afs_get_cellconfig(char *, afs_conf_cell *, char *);

#endif
