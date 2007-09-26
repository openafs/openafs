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

#ifndef __AFS_NEWCREDS_H
#define __AFS_NEWCREDS_H

typedef struct tag_afs_cred_row {
    wchar_t * cell;
    wchar_t * realm;
    afs_tk_method method;
    khm_int32 flags;
} afs_cred_row;

/* we checked whether this cell exists */
#define DLGROW_FLAG_CHECKED  0x00000001

/* cell was checked and was found to be valid */
#define DLGROW_FLAG_VALID    0x00000002

/* cell was deleted  */
#define DLGROW_FLAG_DELETED  0x00000004

/* tokens obtained for cell */
#define DLGROW_FLAG_DONE     0x00000008

/* tokens for this cell already exist */
#define DLGROW_FLAG_EXISTS   0x00000010

/* tokens for this cell exist and is listed under a different
   identity */
#define DLGROW_FLAG_NOTOWNED 0x00000020

/* tokens for this cell exist and are expired */
#define DLGROW_FLAG_EXPIRED  0x00000040

/* this cell was added because it was listed in the identity
   configuration */
#define DLGROW_FLAG_CONFIG   0x00000080

/* the subitem indexes for each data field */
enum afs_ncwnd_subitems {
    NCAFS_IDX_CELL=0,
    NCAFS_IDX_REALM,
    NCAFS_IDX_METHOD
};

#define DLG_TOOLTIP_TIMER_ID 1
#define DLG_TOOLTIP_TIMEOUT  5000

typedef struct tag_afs_cred_list {
    afs_cred_row * rows;
    int n_rows;
    int nc_rows;
} afs_cred_list;

typedef struct tag_afs_dlg_data {
    khui_new_creds * nc;

    afs_cred_list creds;

    khm_int32 afs_enabled;

    BOOL tooltip_visible;
    BOOL dirty;
    HWND tooltip;

    /* list view state image indices */
    int idx_new_token;
    int idx_existing_token;
    int idx_bad_token;

    CRITICAL_SECTION cs;

    /* used with configuration dialogs */
    khm_boolean config_dlg;
    khui_config_init_data cfg;
    khm_handle ident;
} afs_dlg_data;

#define AFS_DLG_ROW_ALLOC 4

INT_PTR CALLBACK 
afs_dlg_proc(HWND hwnd,
             UINT uMsg,
             WPARAM wParam,
             LPARAM lParam);

void 
afs_dlg_update_rows(HWND hwnd, afs_dlg_data * d);

void 
afs_cred_flush_rows(afs_cred_list * l);

void 
afs_cred_free_rows(afs_cred_list * l);

void 
afs_cred_assert_rows(afs_cred_list * l, int n);

void 
afs_cred_delete_row(afs_cred_list * l, int i);

afs_cred_row * 
afs_cred_get_new_row(afs_cred_list * l);

khm_int32 KHMAPI
afs_cred_add_cred_proc(khm_handle cred, void * rock);

void
afs_cred_get_context_creds(afs_cred_list *l,
                           khui_action_context * ctx);

void 
afs_cred_get_identity_creds(afs_cred_list * l, 
                            khm_handle ident,
                            khm_boolean * enabled);

void
afs_cred_write_ident_data(afs_dlg_data * d);

khm_int32
afs_msg_newcred(khm_int32 msg_subtype, 
                khm_ui_4 uparam, 
                void * vparam);

#endif
