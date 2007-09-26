/*
* Copyright (c) 2004, 2005, 2006 Secure Endpoints Inc.
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
 */

#ifndef AFSKFW_H
#define AFSKFW_H
#ifdef  __cplusplus
extern "C" {
#endif
#include <afs/stds.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <rxkad.h>

#define MAXCELLCHARS   64
#define MAXHOSTCHARS   64
#define MAXHOSTSPERCELL 8
#define TRANSARCAFSDAEMON "TransarcAFSDaemon"

void KFW_initialize(void);
void KFW_cleanup(void);
void KFW_initialize_funcs(void);
void KFW_cleanup_funcs(void);
int  KFW_is_available(void);
int  KFW_AFS_destroy_tickets_for_cell(char *);
int  KFW_AFS_destroy_tickets_for_principal(char *);
int  KFW_AFS_renew_expiring_tokens(void);
int  KFW_AFS_get_cred( char * username, 
                        char * cell,
                        char * password,
                        int lifetime,
                        char * smbname,
                        char ** reasonP );
int  KFW_AFS_renew_token_for_cell(char * cell);
int  KFW_AFS_renew_tokens_for_all_cells(void);
BOOL KFW_AFS_wait_for_service_start(void);
BOOL KFW_probe_kdc(struct afsconf_cell *);
int  KFW_AFS_get_cellconfig(char *, struct afsconf_cell *, char *);
void KFW_import_windows_lsa(void);
BOOL KFW_AFS_get_lsa_principal(char *, DWORD *);
int  KFW_AFS_set_file_cache_dacl(char *filename, HANDLE hUserToken);
int  KFW_AFS_obtain_user_temp_directory(HANDLE hUserToken, char *newfilename, int size);
int  KFW_AFS_copy_file_cache_to_default_cache(char * filename);


/* These functions are only to be used in the afslogon.dll */
void KFW_AFS_copy_cache_to_system_file(char *, char *);
int  KFW_AFS_copy_system_file_to_default_cache(char *);

/* From afs/krb_prot.h */
/* values for kerb error codes */
#define         KERB_ERR_OK                              0
#define         KERB_ERR_NAME_EXP                        1
#define         KERB_ERR_SERVICE_EXP                     2
#define         KERB_ERR_AUTH_EXP                        3
#define         KERB_ERR_PKT_VER                         4
#define         KERB_ERR_NAME_MAST_KEY_VER               5
#define         KERB_ERR_SERV_MAST_KEY_VER               6
#define         KERB_ERR_BYTE_ORDER                      7
#define         KERB_ERR_PRINCIPAL_UNKNOWN               8
#define         KERB_ERR_PRINCIPAL_NOT_UNIQUE            9
#define         KERB_ERR_NULL_KEY                       10

/* From afs/krb.h */
#define           RD_AP_TIME     37       /* delta_t too big */
#define           INTK_BADPW     62       /* Incorrect password */

#define PROBE_USERNAME               "OPENAFS-KDC-PROBE"
#define PROBE_PASSWORD_LEN           16

#define DO_NOT_REGISTER_VARNAME  "OPENAFS_DO_NOT_REGISTER_AFS_ID"
#ifdef  __cplusplus
}
#endif
#endif /* AFSKFW_H */
