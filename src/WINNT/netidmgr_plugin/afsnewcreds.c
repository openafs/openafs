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

#include<afscred.h>
#include<commctrl.h>
#include<assert.h>
#include<netidmgr_version.h>
#include<htmlhelp.h>
#include<help/afsplhlp.h>

/* UI stuff */

#define WMNC_AFS_UPDATE_ROWS (WMNC_USER + 1)

typedef struct tag_afs_ident_token_set {
    khm_handle ident;
    afs_cred_list * l;
    khm_boolean add_new;
    khm_boolean update_info;
} afs_ident_token_set;


void 
afs_cred_flush_rows(afs_cred_list * l) {
    int i;

    for(i=0; i<l->n_rows; i++) {
        if(l->rows[i].cell)
            PFREE(l->rows[i].cell);
        if(l->rows[i].realm)
            PFREE(l->rows[i].realm);
    }

    if(l->nc_rows) {
        ZeroMemory(l->rows, sizeof(l->rows[0]) * l->nc_rows);
    }

    l->n_rows = 0;
}

void 
afs_cred_free_rows(afs_cred_list * l) {

    afs_cred_flush_rows(l);

    if(l->rows)
        PFREE(l->rows);
    l->rows = NULL;
    l->n_rows = 0;
    l->nc_rows = 0;
}

void 
afs_cred_assert_rows(afs_cred_list * l, int n) {
    afs_cred_row * rows;

    if(n > l->nc_rows) {
        l->nc_rows = UBOUNDSS(n, AFS_DLG_ROW_ALLOC, AFS_DLG_ROW_ALLOC);
        rows = PMALLOC(sizeof(afs_cred_row) * l->nc_rows);
        ZeroMemory(rows, sizeof(afs_cred_row) * l->nc_rows);

        if(l->rows) {
            if(l->n_rows)
                memcpy(rows, l->rows, sizeof(afs_cred_row) * l->n_rows);
            PFREE(l->rows);
        }
        l->rows = rows;
    }
}

void 
afs_cred_delete_row(afs_cred_list * l, int i) {
    if (i < 0 || i >= l->n_rows)
        return;

    if(i < (l->n_rows - 1)) {
        if(l->rows[i].cell)
            PFREE(l->rows[i].cell);
        if(l->rows[i].realm)
            PFREE(l->rows[i].realm);
        memmove(&(l->rows[i]),
                &(l->rows[i+1]),
                ((l->n_rows - (i+1)) * 
                 sizeof(l->rows[0])));
    }
    l->n_rows--;
}

afs_cred_row * 
afs_cred_get_new_row(afs_cred_list * l) {
    afs_cred_row * r;

    afs_cred_assert_rows(l, l->n_rows + 1);
    r = &(l->rows[l->n_rows]);
    l->n_rows++;

    ZeroMemory(r, sizeof(*r));

    return r;
}

afs_cred_row *
afs_cred_add_row_from_cred(afs_cred_list * l,
                           khm_handle cred) {
    khm_int32 rv;
    afs_cred_row * row;
    khm_size cb;
    wchar_t cell[MAXCELLCHARS];
    int i;

    cb = sizeof(cell);
    rv = kcdb_cred_get_attr(cred,
                            afs_attr_cell,
                            NULL,
                            cell,
                            &cb);
#ifdef DEBUG
    assert(rv == KHM_ERROR_SUCCESS && cb != 0);
#endif

    /* check if we already have the cell listed. */
    for (i=0; i<l->n_rows; i++) {
        if (!_wcsicmp(l->rows[i].cell, cell))
            return &l->rows[i];
    }

    row = afs_cred_get_new_row(l);

    row->cell = PMALLOC(cb);
    StringCbCopy(row->cell, cb, cell);

    cb = sizeof(row->method);
    rv = kcdb_cred_get_attr(cred,
                            afs_attr_method,
                            NULL,
                            &row->method,
                            &cb);

    if (KHM_FAILED(rv)) {
        row->method = AFS_TOKEN_AUTO;
        row->realm = NULL;
        return row;
    }

    rv = kcdb_cred_get_attr(cred,
                            afs_attr_realm,
                            NULL,
                            NULL,
                            &cb);

    if (rv == KHM_ERROR_TOO_LONG && cb > sizeof(wchar_t)) {
        row->realm = PMALLOC(cb);
#ifdef DEBUG
        assert(row->realm);
#endif
        rv = kcdb_cred_get_attr(cred,
                                afs_attr_realm,
                                NULL,
                                row->realm,
                                &cb);

        if (KHM_FAILED(rv)) {
            if (row->realm)
                PFREE(row->realm);
            row->realm = NULL;
        }
    } else {
        row->realm = NULL;
    }

    return row;
}

khm_int32 KHMAPI
afs_cred_add_cred_proc(khm_handle cred, void * rock) {
    afs_cred_list * l = (afs_cred_list *) rock;
    khm_int32 t;

    if (KHM_FAILED(kcdb_cred_get_type(cred, &t)) ||
        t != afs_credtype_id)
        return KHM_ERROR_SUCCESS;

    afs_cred_add_row_from_cred(l, cred);

    return KHM_ERROR_SUCCESS;
}

void
afs_cred_get_context_creds(afs_cred_list *l,
                           khui_action_context * ctx) {
    khm_handle credset = NULL;

    if (KHM_FAILED(kcdb_credset_create(&credset)))
        return;

    if (KHM_FAILED(kcdb_credset_extract_filtered(credset,
                                                 NULL,
                                                 khui_context_cursor_filter,
                                                 (void *) ctx)))
        goto _cleanup;

    kcdb_credset_apply(credset,
                       afs_cred_add_cred_proc,
                       (void *) l);

 _cleanup:
    if (credset)
        kcdb_credset_delete(credset);
}

khm_int32 KHMAPI
afs_get_id_creds_apply_proc(khm_handle cred, void * rock) {
    khm_int32 t;
    afs_ident_token_set * ts;
    afs_cred_list * l;
    khm_handle ident;
    wchar_t cell[MAXCELLCHARS];
    khm_size cb;
    int i;
    khm_int32 cflags = 0;

    ts = (afs_ident_token_set *) rock;
    l = ts->l;

    kcdb_cred_get_type(cred, &t);
    if (t != afs_credtype_id)
        return KHM_ERROR_SUCCESS;

    cb = sizeof(cell);
    if (KHM_FAILED(kcdb_cred_get_attr(cred, afs_attr_cell,
                                      NULL,
                                      cell, &cb)))
        return KHM_ERROR_SUCCESS;

    kcdb_cred_get_flags(cred, &cflags);

    kcdb_cred_get_identity(cred, &ident);

    if (kcdb_identity_is_equal(ident, ts->ident)) {

        for (i=0; i < l->n_rows; i++) {
            if (!_wcsicmp(l->rows[i].cell, cell)) {
                khm_int32 method;

                /* if the token exists, then these are implied */
                l->rows[i].flags =
                    DLGROW_FLAG_EXISTS |
                    DLGROW_FLAG_CHECKED |
                    DLGROW_FLAG_VALID;

                if (cflags & KCDB_CRED_FLAG_EXPIRED)
                    l->rows[i].flags |= DLGROW_FLAG_EXPIRED;

                if (ts->update_info) {
                    wchar_t realm[KHUI_MAXCCH_NAME];

                    cb = sizeof(method);
                    if (KHM_SUCCEEDED
                        (kcdb_cred_get_attr(cred, afs_attr_method,
                                            NULL,
                                            &method, &cb)) &&
                        afs_is_valid_method_id(method))
                        l->rows[i].method = method;

                    cb = sizeof(realm);
                    if (KHM_SUCCEEDED
                        (kcdb_cred_get_attr(cred, afs_attr_realm,
                                            NULL,
                                            realm, &cb)) &&
                        cb > sizeof(wchar_t)) {

                        if (l->rows[i].realm)
                            PFREE(l->rows[i].realm);
                        l->rows[i].realm = PMALLOC(cb);
                        StringCbCopy(l->rows[i].realm,
                                     cb,
                                     realm);
                    }
                }
                break;
            }
        }

        /* not found? add! */
        if (i >= l->n_rows && ts->add_new) {
            afs_cred_row * r;

            r = afs_cred_add_row_from_cred(l, cred);

            r->flags = DLGROW_FLAG_VALID | DLGROW_FLAG_CHECKED |
                DLGROW_FLAG_EXISTS;

            if (cflags & KCDB_CRED_FLAG_EXPIRED)
                r->flags |= DLGROW_FLAG_EXPIRED;
        }

    } else {                    /* different identities */

        for (i=0; i < l->n_rows; i++) {
            if (!_wcsicmp(l->rows[i].cell, cell)) {
                l->rows[i].flags =
                    DLGROW_FLAG_NOTOWNED | DLGROW_FLAG_EXISTS |
                    DLGROW_FLAG_VALID | DLGROW_FLAG_CHECKED;
                if (cflags & KCDB_CRED_FLAG_EXPIRED)
                    l->rows[i].flags |= DLGROW_FLAG_EXPIRED;
            }
        }

    }

    kcdb_identity_release(ident);

    return KHM_ERROR_SUCCESS;
}

void
afs_remove_token_from_identities(wchar_t * cell) {
    wchar_t * idents = NULL;
    wchar_t * t;
    khm_size cb_id;
    khm_size n_id = 0;

    do {
        if (kcdb_identity_enum(KCDB_IDENT_FLAG_CONFIG,
                               KCDB_IDENT_FLAG_CONFIG,
                               NULL,
                               &cb_id,
                               &n_id) != KHM_ERROR_TOO_LONG ||
            n_id == 0) {
            if (idents)
                PFREE(idents);
            return;
        }

        if (idents)
            PFREE(idents);
        idents = PMALLOC(cb_id);

        if (kcdb_identity_enum(KCDB_IDENT_FLAG_CONFIG,
                               KCDB_IDENT_FLAG_CONFIG,
                               idents,
                               &cb_id,
                               &n_id) == KHM_ERROR_SUCCESS)
            break;
    } while(TRUE);

    for (t=idents;
         t && *t;
         t = multi_string_next(t)) {

        khm_handle h_id = NULL;
        khm_handle csp_ident = NULL;
        khm_handle csp_afs = NULL;
        khm_size cb;
        wchar_t vbuf[1024];
        wchar_t * tbuf = NULL;
        khm_int32 enabled = 0;

        kcdb_identity_create(t, 0, &h_id);
        if (h_id == NULL) {
#ifdef DEBUG
            assert(FALSE);
#endif
            continue;
        }

        if (KHM_FAILED(kcdb_identity_get_config(h_id, 0, &csp_ident)))
            goto _cleanup_loop;

        if (KHM_FAILED(khc_open_space(csp_ident, CSNAME_AFSCRED,
                                      0, &csp_afs)))
            goto _cleanup_loop;

        if (KHM_SUCCEEDED(khc_read_int32(csp_afs, L"AFSEnabled", &enabled)) &&
            !enabled)
            goto _cleanup_loop;

        if (khc_read_multi_string(csp_afs, L"Cells", NULL, &cb)
            != KHM_ERROR_TOO_LONG)
            goto _cleanup_loop;

        if (cb < sizeof(vbuf))
            tbuf = vbuf;
        else
            tbuf = PMALLOC(cb);

        if (khc_read_multi_string(csp_afs, L"Cells", tbuf, &cb)
            != KHM_ERROR_SUCCESS)
            goto _cleanup_loop;

        if (multi_string_find(tbuf, cell, 0) == NULL)
            goto _cleanup_loop;

        multi_string_delete(tbuf, cell, 0);

        khc_write_multi_string(csp_afs, L"Cells", tbuf);

    _cleanup_loop:
        kcdb_identity_release(h_id);
        if (csp_ident)
            khc_close_space(csp_ident);
        if (csp_afs)
            khc_close_space(csp_afs);
        if (tbuf && tbuf != vbuf)
            PFREE(tbuf);
    }

    if (idents)
        PFREE(idents);
}

khm_boolean
afs_check_add_token_to_identity(wchar_t * cell, khm_handle ident,
                                khm_handle * ident_conflict) {
    wchar_t * idents = NULL;
    wchar_t * t;
    khm_size cb_id;
    khm_size n_id = 0;
    khm_boolean ok_to_add = TRUE;

    /* check if this cell is listed for any other identity. */

    do {
        if (kcdb_identity_enum(KCDB_IDENT_FLAG_CONFIG,
                               KCDB_IDENT_FLAG_CONFIG,
                               NULL,
                               &cb_id,
                               &n_id) != KHM_ERROR_TOO_LONG ||
            n_id == 0) {
            if (idents)
                PFREE(idents);
            return TRUE;
        }

        if (idents)
            PFREE(idents);
        idents = PMALLOC(cb_id);

        if (kcdb_identity_enum(KCDB_IDENT_FLAG_CONFIG,
                               KCDB_IDENT_FLAG_CONFIG,
                               idents,
                               &cb_id,
                               &n_id) == KHM_ERROR_SUCCESS)
            break;
    } while(TRUE);

    for (t=idents;
         ok_to_add && t && *t;
         t = multi_string_next(t)) {

        khm_handle h_id = NULL;
        khm_handle csp_ident = NULL;
        khm_handle csp_afs = NULL;
        khm_size cb;
        wchar_t vbuf[1024];
        wchar_t * tbuf = NULL;
        khm_int32 enabled = 0;

        kcdb_identity_create(t, 0, &h_id);
        if (h_id == NULL) {
#ifdef DEBUG
            assert(FALSE);
#endif
            continue;
        }

        if (kcdb_identity_is_equal(h_id, ident)) {
            kcdb_identity_release(h_id);
            continue;
        }

        if (KHM_FAILED(kcdb_identity_get_config(h_id, 0, &csp_ident)))
            goto _cleanup_loop;

        if (KHM_FAILED(khc_open_space(csp_ident, CSNAME_AFSCRED,
                                      0, &csp_afs)))
            goto _cleanup_loop;

        if (KHM_SUCCEEDED(khc_read_int32(csp_afs, L"AFSEnabled", &enabled)) &&
            !enabled)
            goto _cleanup_loop;

        if (khc_read_multi_string(csp_afs, L"Cells", NULL, &cb)
            != KHM_ERROR_TOO_LONG)
            goto _cleanup_loop;

        if (cb < sizeof(vbuf))
            tbuf = vbuf;
        else
            tbuf = PMALLOC(cb);

        if (khc_read_multi_string(csp_afs, L"Cells", tbuf, &cb)
            != KHM_ERROR_SUCCESS)
            goto _cleanup_loop;

        if (multi_string_find(tbuf, cell, 0) == NULL)
            goto _cleanup_loop;

        /* we found another identity which gets tokens for the
           same cell */

        ok_to_add = FALSE;

        if (ident_conflict) {
            *ident_conflict = h_id;
            kcdb_identity_hold(h_id);
        }

    _cleanup_loop:
        kcdb_identity_release(h_id);
        if (csp_ident)
            khc_close_space(csp_ident);
        if (csp_afs)
            khc_close_space(csp_afs);
        if (tbuf && tbuf != vbuf)
            PFREE(tbuf);
    }

    if (idents)
        PFREE(idents);

    return ok_to_add;
}


void 
afs_cred_get_identity_creds(afs_cred_list * l, 
                            khm_handle ident,
                            khm_boolean * penabled) {
    khm_handle h_id = NULL;
    khm_handle h_afs = NULL;
    khm_handle h_cells = NULL;  /* per identity cells space */
    khm_handle h_gcells = NULL; /* global cells space */
    khm_boolean load_defs = TRUE;
    khm_size cbi;
    wchar_t * ms = NULL;
    wchar_t * s = NULL;
    afs_ident_token_set ts;
    khm_int32 t;

    if (penabled)
        *penabled = TRUE;

    afs_cred_flush_rows(l);

    kcdb_identity_get_config(ident, 0, &h_id);
    if(!h_id) 
        goto _done_config;

    if(KHM_FAILED(khc_open_space(h_id, CSNAME_AFSCRED, 
                                 0, &h_afs)))
        goto _done_config;

    if (penabled) {
        t = 1;
        if (KHM_FAILED(khc_read_int32(h_afs, L"AFSEnabled", &t)))
            khc_read_int32(csp_params, L"AFSEnabled", &t);
        *penabled = !!t;
    }

    if(KHM_FAILED(khc_open_space(h_afs, L"Cells", 
                                 0, &h_cells)))
        goto _done_config;

    if(khc_read_multi_string(h_afs, L"Cells", NULL, &cbi) != 
       KHM_ERROR_TOO_LONG)
        goto _done_config;

    load_defs = FALSE;

    ms = PMALLOC(cbi);
    ZeroMemory(ms, cbi);

    khc_read_multi_string(h_afs, L"Cells", ms, &cbi);

    s = ms;
    for(s = ms; s && *s; s = multi_string_next(s)) {
        afs_cred_row * r;
        size_t cb;
        khm_handle h_cell = NULL;

        /* is this a valid cell name? */
        if(FAILED(StringCbLength(s, MAXCELLCHARS, &cb)))
            continue;
        cb += sizeof(wchar_t);

        r = afs_cred_get_new_row(l);

        r->cell = PMALLOC(cb);
        StringCbCopy(r->cell, cb, s);

        r->realm = NULL;
        r->method = AFS_TOKEN_AUTO;
        r->flags = DLGROW_FLAG_CONFIG;

        if(KHM_SUCCEEDED(khc_open_space(h_cells, s, 
                                        0, &h_cell))) {
            khm_int32 i;
            wchar_t wname[KHUI_MAXCCH_NAME];
            khm_size cb;

            if(khc_read_string(h_cell, L"Realm", 
                               NULL, &cbi) == 
               KHM_ERROR_TOO_LONG &&
               cbi > sizeof(wchar_t)) {

                r->realm = PMALLOC(cbi);
                khc_read_string(h_cell, L"Realm", r->realm, &cbi);
            }

            i = AFS_TOKEN_AUTO;

            cb = sizeof(wname);
            if (KHM_SUCCEEDED(khc_read_string(h_cell, L"MethodName",
                                              wname, &cb))) {

                r->method = afs_get_method_id(wname);

                /* remove the deprecated value if it is present. */
                khc_remove_value(h_cell, L"Method", 0);

            } else if (KHM_SUCCEEDED(khc_read_int32(h_cell, 
                                                    L"Method", &i))) {
                /* the Method property is deprecated.  We detect and
                   correct this whenever possible. */

                if (!afs_is_valid_method_id(i))
                    i = AFS_TOKEN_AUTO;

                r->method = i;

                afs_get_method_name(i, wname, sizeof(wname));

                khc_write_string(h_cell, L"MethodName",
                                 wname);

                khc_remove_value(h_cell, L"Method", 0);
            }

            khc_close_space(h_cell);
        }
    }

    if(ms) {
        PFREE(ms);
        ms = NULL;
    }

 _done_config:

    if (load_defs) {
        /* We want to load defaults */
        char buf[MAXCELLCHARS];
        wchar_t wbuf[MAXCELLCHARS];
        wchar_t wmethod[KHUI_MAXCCH_NAME];
        wchar_t * defcells;
        khm_size cb_defcells;
        afs_cred_row * r;
        khm_size sz;

        khc_open_space(csp_params, L"Cells", 0, &h_gcells);

        if (!cm_GetRootCellName(buf) &&
            afs_check_for_cell_realm_match(ident, buf)) {
            AnsiStrToUnicode(wbuf, sizeof(wbuf), buf);

            if (afs_check_add_token_to_identity(wbuf, ident, NULL)) {
                khm_handle h_cell = NULL;

                r = afs_cred_get_new_row(l);

                StringCbLength(wbuf, sizeof(wbuf), &sz);
                sz += sizeof(wchar_t);

                r->cell = PMALLOC(sz);
                StringCbCopy(r->cell, sz, wbuf);

                if (h_gcells &&
                    KHM_SUCCEEDED(khc_open_space(h_gcells, wbuf, 0, &h_cell))) {
                    khm_size cb;

                    cb = sizeof(wmethod);
                    if (KHM_SUCCEEDED(khc_read_string(h_cell, L"MethodName", wmethod, &cb))) {
                        r->method = afs_get_method_id(wmethod);
                    } else {
                        r->method = AFS_TOKEN_AUTO;
                    }

                    cb = sizeof(wbuf);
                    if (KHM_SUCCEEDED(khc_read_string(h_cell, L"Realm", wbuf, &cb))) {
                        r->realm = PMALLOC(cb);
                        StringCbCopy(r->realm, cb, wbuf);
                    } else {
                        r->realm = NULL;
                    }

                    khc_close_space(h_cell);

                } else {
                    r->realm = NULL;
                    r->method = AFS_TOKEN_AUTO;
                }

                r->flags = 0;
            }
        }

        if (khc_read_multi_string(csp_params, L"DefaultCells",
                                  NULL, &cb_defcells) == KHM_ERROR_TOO_LONG &&
            cb_defcells > sizeof(wchar_t) * 2) {
            wchar_t * c_cell;

            defcells = PMALLOC(cb_defcells);
            if (defcells == NULL)
                goto _done_defaults;

            if (KHM_FAILED(khc_read_multi_string(csp_params, L"DefaultCells",
                                                 defcells, &cb_defcells))) {
                PFREE(defcells);
                goto _done_defaults;
            }

            for (c_cell = defcells;
                 c_cell && *c_cell;
                 c_cell = multi_string_next(c_cell)) {

                khm_size cb;
                int i;
                khm_handle h_cell = NULL;
                afs_cred_row * r;

                if (FAILED(StringCbLength(c_cell, (MAXCELLCHARS + 1) * sizeof(wchar_t),
                                          &cb)))
                    continue;
                cb += sizeof(wchar_t);

                for (i=0; i < l->n_rows; i++) {
                    if (!_wcsicmp(l->rows[i].cell, c_cell))
                        break;
                }

                if (i < l->n_rows)
                    continue;

                {
                    char cell[MAXCELLCHARS];

                    UnicodeStrToAnsi(cell, sizeof(cell), c_cell);

                    if (!afs_check_for_cell_realm_match(ident, cell))
                        continue;
                }

                r = afs_cred_get_new_row(l);

                r->cell = PMALLOC(cb);
                StringCbCopy(r->cell, cb, c_cell);

                if (h_gcells &&
                    KHM_SUCCEEDED(khc_open_space(h_gcells, c_cell, 0, &h_cell))) {

                    cb = sizeof(wmethod);
                    if (KHM_SUCCEEDED(khc_read_string(h_cell, L"MethodName", wmethod, &cb))) {
                        r->method = afs_get_method_id(wmethod);
                    } else {
                        r->method = AFS_TOKEN_AUTO;
                    }

                    cb = sizeof(wbuf);
                    if (KHM_SUCCEEDED(khc_read_string(h_cell, L"Realm", wbuf, &cb))) {
                        r->realm = PMALLOC(cb);
                        StringCbCopy(r->realm, cb, wbuf);
                    } else {
                        r->realm = NULL;
                    }

                    khc_close_space(h_cell);
                } else {
                    r->realm = NULL;
                    r->method = AFS_TOKEN_AUTO;
                }

                r->flags = 0;
            }

            PFREE(defcells);
        }
    }

 _done_defaults:

    ts.ident = ident;
    ts.l = l;
    ts.add_new = TRUE;
    ts.update_info = FALSE;

    kcdb_credset_apply(NULL, afs_get_id_creds_apply_proc,
                       &ts);

    if(h_id)
        khc_close_space(h_id);
    if(h_afs)
        khc_close_space(h_afs);
    if(h_cells)
        khc_close_space(h_cells);
    if(h_gcells)
        khc_close_space(h_gcells);
}

void 
nc_dlg_enable(HWND hwnd, BOOL enable) {
    if(enable) {
        SendDlgItemMessage(hwnd, IDC_NCAFS_OBTAIN, BM_SETCHECK, 
                           BST_CHECKED, 0);
    } else {
        SendDlgItemMessage(hwnd, IDC_NCAFS_OBTAIN, BM_SETCHECK, 
                           BST_UNCHECKED, 0);
    }

    EnableWindow(GetDlgItem(hwnd,IDC_NCAFS_CELL), enable);
    EnableWindow(GetDlgItem(hwnd,IDC_NCAFS_REALM), enable);
    EnableWindow(GetDlgItem(hwnd,IDC_NCAFS_METHOD), enable);
    EnableWindow(GetDlgItem(hwnd,IDC_NCAFS_TOKENLIST), enable);
    EnableWindow(GetDlgItem(hwnd,IDC_NCAFS_ADD_TOKEN), enable);
    EnableWindow(GetDlgItem(hwnd,IDC_NCAFS_DELETE_TOKEN), enable);
}

void 
nc_dlg_show_tooltip(HWND hwnd, 
                    UINT_PTR id, 
                    LPWSTR msg, 
                    LPWSTR title, 
                    int type, 
                    int x, 
                    int y)
{
    afs_dlg_data * d;
    TOOLINFO ti;

    d = (afs_dlg_data *)(LONG_PTR) GetWindowLongPtr(hwnd, DWLP_USER);

    if (d == NULL)
        return;

    ZeroMemory(&ti, sizeof(ti));
    ti.cbSize = sizeof(ti);
    ti.hwnd = hwnd;
    ti.uId = id;
    SendMessage(d->tooltip, TTM_GETTOOLINFO, 0, (LPARAM) &ti);

    ti.hinst = hResModule;
    ti.lpszText = msg;

    SendMessage(d->tooltip, TTM_SETTOOLINFO, 0, (LPARAM) &ti);

    if(IS_INTRESOURCE(title)) {
        wchar_t wbuf[1024];
        UINT resid;

        resid = (UINT)(UINT_PTR) title;

        LoadString(hResModule, resid, wbuf, ARRAYLENGTH(wbuf));
        SendMessage(d->tooltip, TTM_SETTITLE, type, (LPARAM) wbuf);
    } else
        SendMessage(d->tooltip, TTM_SETTITLE, type, (LPARAM) title);

    SendMessage(d->tooltip, TTM_TRACKACTIVATE, TRUE, (LPARAM) &ti);
    SendMessage(d->tooltip, TTM_TRACKPOSITION, 0, (LPARAM) MAKELONG(x,y));

    d->tooltip_visible = TRUE;

    SetTimer(hwnd, DLG_TOOLTIP_TIMER_ID, DLG_TOOLTIP_TIMEOUT, NULL);
}

void 
nc_dlg_hide_tooltip(HWND hwnd, UINT_PTR id)
{
    TOOLINFO ti;
    afs_dlg_data * d;

    d = (afs_dlg_data *)(LONG_PTR) GetWindowLongPtr(hwnd, DWLP_USER);

    if (d == NULL)
        return;

    if(!d->tooltip_visible)
        return;

    ZeroMemory(&ti, sizeof(ti));
    ti.cbSize = sizeof(ti);
    ti.hwnd = hwnd;
    ti.uId = id;

    SendMessage(d->tooltip, TTM_TRACKACTIVATE, FALSE, (LPARAM) &ti);
    d->tooltip_visible = FALSE;
}

void 
afs_dlg_update_rows(HWND hwnd, afs_dlg_data * d) {
    HWND hwlist;
    LVITEM lvi;
    wchar_t wauto[256];
    int i;

    CheckDlgButton(hwnd, IDC_NCAFS_OBTAIN,
                   (d->afs_enabled)? BST_CHECKED: BST_UNCHECKED);

    hwlist = GetDlgItem(hwnd, IDC_NCAFS_TOKENLIST);

    ListView_DeleteAllItems(hwlist);

    if(d->creds.n_rows == 0)
        return;

    LoadString(hResModule, IDS_NC_AUTO, wauto, ARRAYLENGTH(wauto));

    for(i=0; i < d->creds.n_rows; i++) {
        wchar_t wbuf[256];
        int flags;

        ZeroMemory(&lvi, sizeof(lvi));

        lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE;
        lvi.iItem = d->creds.n_rows + 1;

        lvi.stateMask = LVIS_STATEIMAGEMASK;
        flags = d->creds.rows[i].flags;
        if ((flags & DLGROW_FLAG_EXISTS) &&
            (flags & DLGROW_FLAG_NOTOWNED)) {
            lvi.state = INDEXTOSTATEIMAGEMASK(d->idx_bad_token);
        } else if ((flags & DLGROW_FLAG_EXISTS)) {
            lvi.state = INDEXTOSTATEIMAGEMASK(d->idx_existing_token);
        } else {
            lvi.state = INDEXTOSTATEIMAGEMASK(d->idx_new_token);
        }

        lvi.lParam = (LPARAM) i;

        lvi.iSubItem = NCAFS_IDX_CELL;
        lvi.pszText = d->creds.rows[i].cell;

        lvi.iItem = ListView_InsertItem(hwlist, &lvi);

        lvi.mask = LVIF_TEXT; /* subitems dislike lParam */
        lvi.iSubItem = NCAFS_IDX_REALM;
        if(d->creds.rows[i].realm != NULL)
            lvi.pszText = d->creds.rows[i].realm;
        else
            lvi.pszText = wauto;
        ListView_SetItem(hwlist, &lvi);

        lvi.iSubItem = NCAFS_IDX_METHOD;
        afs_method_describe(d->creds.rows[i].method,
                            KCDB_TS_SHORT,
                            wbuf, sizeof(wbuf));
        lvi.pszText = wbuf;

        ListView_SetItem(hwlist, &lvi);
    }
}

void 
nc_dlg_del_token(HWND hwnd) {
    afs_dlg_data * d;
    khui_new_creds_by_type * nct;

    d = (afs_dlg_data *)(LONG_PTR) GetWindowLongPtr(hwnd, DWLP_USER);

    if (d == NULL)
        return;

    if (d->nc)
        khui_cw_find_type(d->nc, afs_credtype_id, &nct);

    if(ListView_GetSelectedCount(GetDlgItem(hwnd, IDC_NCAFS_TOKENLIST)) == 0) {
        wchar_t cell[KCDB_MAXCCH_NAME];
        int i;

        /* nothing is selected in the list view */
        /* we delete the row that matches the current contents of the
        cell edit control */
        cell[0] = 0;
        GetDlgItemText(hwnd, IDC_NCAFS_CELL, cell, ARRAYLENGTH(cell));
        for(i=0; i<d->creds.n_rows; i++) {
            if(!_wcsicmp(d->creds.rows[i].cell, cell)) {
                /* found it */
                afs_cred_delete_row(&d->creds, i);
                afs_dlg_update_rows(hwnd, d);
                d->dirty = TRUE;
                break;
            }
        }
    } else {
        /* something is selected in the token list view */
        /* we delete that */
        HWND hw;
        LVITEM lvi;
        int idx;
        int row;
        BOOL deleted = FALSE;

        hw = GetDlgItem(hwnd, IDC_NCAFS_TOKENLIST);
        idx = -1;
        do {
            idx = ListView_GetNextItem(hw, idx, LVNI_SELECTED);
            if(idx >= 0) {
                ZeroMemory(&lvi, sizeof(lvi));
                lvi.iItem = idx;
                lvi.iSubItem = 0;
                lvi.mask = LVIF_PARAM;
                if(!ListView_GetItem(hw, &lvi))
                    continue;
                row = (int) lvi.lParam;
                if(row >= 0 && row < d->creds.n_rows) {
                    d->creds.rows[row].flags |= DLGROW_FLAG_DELETED;
                    deleted = TRUE;
                }
            }
        } while(idx != -1);

        if(deleted) {
            for(idx = 0; idx < d->creds.n_rows; idx ++) {
                if(d->creds.rows[idx].flags & DLGROW_FLAG_DELETED) {
                    afs_cred_delete_row(&d->creds, idx);
                    idx--; /* we have to look at the current item again */
                }
            }

            d->dirty = TRUE;
            afs_dlg_update_rows(hwnd, d);
        }
    }

    if (d->nc)
        SendMessage(d->nc->hwnd, KHUI_WM_NC_NOTIFY, 
                    MAKEWPARAM(0, WMNC_UPDATE_CREDTEXT), 0);
    else if (d->config_dlg && d->dirty)
        khui_cfg_set_flags_inst(&d->cfg, KHUI_CNFLAG_MODIFIED,
                                KHUI_CNFLAG_MODIFIED);
}

void 
nc_dlg_add_token(HWND hwnd) {
    afs_dlg_data * d;
    afs_cred_row * prow;
    afs_cred_row trow;
    khui_new_creds_by_type * nct;
    wchar_t buf[256];
    int idx;
    size_t n;
    size_t cb;
    int i;
    BOOL new_row = FALSE;
    khm_handle ident = NULL;

    d = (afs_dlg_data *)(LONG_PTR) GetWindowLongPtr(hwnd, DWLP_USER);

    if (d == NULL)
        return;

    if (d->nc)
        khui_cw_find_type(d->nc, afs_credtype_id, &nct);
    else
        nct = NULL;

    if((n = SendDlgItemMessage(hwnd, IDC_NCAFS_CELL, WM_GETTEXT, 
                               (WPARAM) ARRAYLENGTH(buf), (LPARAM) buf)) 
       == 0)
    {
        /* probably should indicate that user should type something */
        RECT r;
        GetWindowRect(GetDlgItem(hwnd, IDC_NCAFS_CELL), &r);
        nc_dlg_show_tooltip(hwnd, 
                            0, 
                            MAKEINTRESOURCE(IDS_NC_TT_NO_CELL), 
                            MAKEINTRESOURCE(IDS_NC_TT_CANT_ADD), 
                            2, (r.left + r.right)/ 2, r.bottom);
        return;
    }

    if(n != wcsspn(buf, AFS_VALID_CELL_CHARS)) {
        RECT r;
        GetWindowRect(GetDlgItem(hwnd, IDC_NCAFS_CELL), &r);
        nc_dlg_show_tooltip(hwnd, 
                            0, 
                            MAKEINTRESOURCE(IDS_NC_TT_MALFORMED_CELL), 
                            MAKEINTRESOURCE(IDS_NC_TT_CANT_ADD), 
                            2, (r.left + r.right)/2, r.bottom);
        return;
    }

    /* check if this is already listed */
    for(i=0;i<d->creds.n_rows;i++) {
        if(!_wcsicmp(buf, d->creds.rows[i].cell))
            break;
    }

    if(i < d->creds.n_rows) {
        new_row = FALSE;

        prow = &(d->creds.rows[i]);
    } else {
        new_row = TRUE;
        prow = NULL;
    }

    ZeroMemory(&trow, sizeof(trow));

    cb = (n+1) * sizeof(wchar_t);
    trow.cell = PMALLOC(cb);
    StringCbCopy(trow.cell, cb, buf);

    /* now for the realm */
    do {
        idx = (int) SendDlgItemMessage(hwnd, IDC_NCAFS_REALM, 
                                       CB_GETCURSEL, 0, 0);
        if(idx != CB_ERR) {
            int lp;
            lp = (int) SendDlgItemMessage(hwnd, IDC_NCAFS_REALM, 
                                          CB_GETITEMDATA, idx, 0);
            if(lp != CB_ERR && lp) /* this is the 'determine realm
                                      automatically' item */
            {
                trow.realm = NULL;
                break;
            }
        }

        if((n = SendDlgItemMessage(hwnd, IDC_NCAFS_REALM, WM_GETTEXT, 
                                   ARRAYLENGTH(buf), (LPARAM) buf)) == 0) {
            RECT r;
            GetWindowRect(GetDlgItem(hwnd, IDC_NCAFS_REALM), &r);
            nc_dlg_show_tooltip(hwnd, 
                                0, 
                                MAKEINTRESOURCE(IDS_NC_TT_NO_REALM), 
                                MAKEINTRESOURCE((new_row)?
                                                IDS_NC_TT_CANT_ADD:
                                                IDS_NC_TT_CANT_UPDATE),
                                2, (r.left + r.right)/2, r.bottom);
            goto _error_exit;
        }

        if(n != wcsspn(buf, AFS_VALID_REALM_CHARS)) {
            RECT r;
            GetWindowRect(GetDlgItem(hwnd, IDC_NCAFS_REALM), &r);
            nc_dlg_show_tooltip(hwnd,
                                0, 
                                MAKEINTRESOURCE(IDS_NC_TT_MALFORMED_REALM),
                                MAKEINTRESOURCE((new_row)?
                                                IDS_NC_TT_CANT_ADD:
                                                IDS_NC_TT_CANT_UPDATE), 
                                2, (r.left + r.right)/2, r.bottom);
            goto _error_exit;
        }

        cb = (n+1) * sizeof(wchar_t);
        trow.realm = PMALLOC(cb);
        StringCbCopy(trow.realm, cb, buf);

    } while(FALSE);

    idx = (int)SendDlgItemMessage(hwnd, IDC_NCAFS_METHOD, 
                                  CB_GETCURSEL, 0, 0);
    if (idx != CB_ERR) {
        trow.method = (afs_tk_method)
            SendDlgItemMessage(hwnd, IDC_NCAFS_METHOD, CB_GETITEMDATA, 
                               idx, 0);
    } else {
        trow.method = AFS_TOKEN_AUTO;
    }

    if (d->nc &&
        d->nc->n_identities > 0 &&
        d->nc->identities[0]) {
        
        ident = d->nc->identities[0];

    } else if (d->ident) {

        ident = d->ident;
        
    }

    if(new_row) {
        khm_boolean ok_to_add = TRUE;

        if (ident) {
            khm_handle id_conf = NULL;

            ok_to_add =
                afs_check_add_token_to_identity(trow.cell,
                                                ident,
                                                &id_conf);

            if (!ok_to_add) {
#if KH_VERSION_API >= 5
                khui_alert * a;
                wchar_t wbuf[512];
                wchar_t wfmt[128];
                wchar_t widname[KCDB_IDENT_MAXCCH_NAME];
                khm_size cb;

#ifdef DEBUG
                assert(id_conf);
#endif
                khui_alert_create_empty(&a);

                cb = sizeof(widname);
                kcdb_identity_get_name(id_conf, widname, &cb);

                LoadString(hResModule, IDS_NC_TT_CONFLICT,
                           wfmt, ARRAYLENGTH(wfmt));
                StringCbPrintf(wbuf, sizeof(wbuf),
                               wfmt, trow.cell, widname);
                khui_alert_set_message(a, wbuf);

                LoadString(hResModule, IDS_NC_TT_PROBLEM,
                           wbuf, ARRAYLENGTH(wbuf));
                khui_alert_set_title(a, wbuf);

                khui_alert_add_command(a, KHUI_PACTION_KEEP);
                khui_alert_add_command(a, KHUI_PACTION_REMOVE);
                khui_alert_add_command(a, KHUI_PACTION_CANCEL);

                khui_alert_set_severity(a, KHERR_INFO);

                khui_alert_show_modal(a);

                ok_to_add = TRUE;

                if (a->response == KHUI_PACTION_REMOVE) {
                    afs_remove_token_from_identities(trow.cell);
                } else if (a->response == KHUI_PACTION_CANCEL) {
                    ok_to_add = FALSE;
                }

                khui_alert_release(a);
#else
                wchar_t widname[KCDB_IDENT_MAXCCH_NAME];
                wchar_t wtitle[64];
                wchar_t wmsg[512];
                wchar_t wfmt[128];
                khm_size cb;
                int r;

#ifdef DEBUG
                assert(id_conf);
#endif

                cb = sizeof(widname);
                kcdb_identity_get_name(id_conf, widname, &cb);
                LoadString(hResModule, IDS_NC_TT_PROBLEM,
                           wtitle, ARRAYLENGTH(wtitle));
                LoadString(hResModule, IDS_NC_TT_CONFLICTM,
                           wfmt, ARRAYLENGTH(wfmt));
                StringCbPrintf(wmsg, sizeof(wmsg), wfmt,
                               trow.cell, widname);
                r = MessageBox(NULL, wmsg, wtitle,
                               MB_YESNOCANCEL | MB_ICONWARNING |
                               MB_APPLMODAL);

                ok_to_add = TRUE;
                if (r == IDNO) {
                    afs_remove_token_from_identities(trow.cell);
                } else if (r == IDCANCEL) {
                    ok_to_add = FALSE;
                }
#endif

                kcdb_identity_release(id_conf);
            }

            if (!ok_to_add)
                goto _error_exit;
        }

        prow = afs_cred_get_new_row(&d->creds);
    } else {
        if (prow->cell)
            PFREE(prow->cell);

        if(prow->realm)
            PFREE(prow->realm);

        ZeroMemory(prow, sizeof(*prow));
    }

    *prow = trow;

    if (ident) {
        afs_ident_token_set ts;

        ts.ident = ident;
        ts.l = &d->creds;
        ts.add_new = FALSE;
        ts.update_info = FALSE;

        kcdb_credset_apply(NULL, afs_get_id_creds_apply_proc,
                           &ts);
    }

    afs_dlg_update_rows(hwnd, d);

    d->dirty = TRUE;

    if (d->nc)
        SendMessage(d->nc->hwnd, KHUI_WM_NC_NOTIFY, 
                    MAKEWPARAM(0, WMNC_UPDATE_CREDTEXT), 0);
    else if (d->config_dlg) {
        khui_cfg_set_flags_inst(&d->cfg,
                                KHUI_CNFLAG_MODIFIED,
                                KHUI_CNFLAG_MODIFIED);
    }

    return;

_error_exit:
    if(trow.realm)
        PFREE(trow.realm);
    if(trow.cell)
        PFREE(trow.cell);
}

/* this is shared between the new credentials window and the AFS per
   identity configuration dialog. */
INT_PTR CALLBACK 
afs_dlg_proc(HWND hwnd,
             UINT uMsg,
             WPARAM wParam,
             LPARAM lParam)
{
    switch(uMsg) {
    case WM_INITDIALOG:
        {
            HWND hw;
            HIMAGELIST hw_ilist;
            afs_dlg_data * d;
            khui_new_creds_by_type * nct = NULL;
            RECT r;

            d = PMALLOC(sizeof(*d));
            ZeroMemory(d, sizeof(*d));

            InitializeCriticalSection(&d->cs);

            /* lParam is a pointer to a khui_new_creds structure */
            d->nc = (khui_new_creds *) lParam;

            if (d->nc)
                khui_cw_find_type(d->nc, afs_credtype_id, &nct);

#pragma warning(push)
#pragma warning(disable: 4244)
            SetWindowLongPtr(hwnd, DWLP_USER, (LPARAM) d);
#pragma warning(pop)

            EnterCriticalSection(&d->cs);

            if (nct)
                nct->aux = (LPARAM) d;

            /* create the tooltip window */
            d->tooltip = 
                CreateWindowEx(WS_EX_TOPMOST,
                               TOOLTIPS_CLASS,
                               NULL,
                               WS_POPUP | TTS_BALLOON | TTS_ALWAYSTIP,
                               CW_USEDEFAULT, CW_USEDEFAULT,
                               CW_USEDEFAULT, CW_USEDEFAULT,
                               hwnd,  /* make this an owned window, so
                                         we don't have to worry about
                                         destroying it */
                               NULL,
                               hInstance,
                               NULL);

            SetWindowPos(d->tooltip,
                         HWND_TOPMOST,
                         0,
                         0,
                         0,
                         0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

            {
                TOOLINFO ti;

                ZeroMemory(&ti, sizeof(ti));
                ti.cbSize = sizeof(ti);
                ti.uFlags = TTF_TRACK;
                ti.hwnd = hwnd;
                ti.uId = 0;
                ti.hinst = hResModule;
                ti.lpszText = L"";
                GetClientRect(hwnd, &(ti.rect));

                SendMessage(d->tooltip, TTM_ADDTOOL, 0, (LPARAM) &ti);
            }

            /* we only initialize the constant bits here. */
            hw = GetDlgItem(hwnd, IDC_NCAFS_TOKENLIST);

            GetClientRect(hw, &r);

            /* set the list view status icons */
            hw_ilist = ImageList_Create(GetSystemMetrics(SM_CXSMICON),
                                        GetSystemMetrics(SM_CYSMICON),
                                        ILC_COLOR8 | ILC_MASK,
                                        4, 4);
#ifdef DEBUG
            assert(hw_ilist);
#endif
            {
                HICON hi;

                hi = LoadImage(hResModule, MAKEINTRESOURCE(IDI_NC_NEW),
                               IMAGE_ICON,
                               GetSystemMetrics(SM_CXSMICON),
                               GetSystemMetrics(SM_CYSMICON),
                               LR_DEFAULTCOLOR);

                d->idx_new_token = ImageList_AddIcon(hw_ilist, hi) + 1;

                DestroyIcon(hi);

                hi = LoadImage(hResModule, MAKEINTRESOURCE(IDI_NC_EXIST),
                               IMAGE_ICON,
                               GetSystemMetrics(SM_CXSMICON),
                               GetSystemMetrics(SM_CYSMICON),
                               LR_DEFAULTCOLOR);
                d->idx_existing_token = ImageList_AddIcon(hw_ilist, hi) + 1;

                DestroyIcon(hi);

                hi = LoadImage(hResModule,
                               MAKEINTRESOURCE(IDI_NC_NOTOWNED),
                               IMAGE_ICON,
                               GetSystemMetrics(SM_CXSMICON),
                               GetSystemMetrics(SM_CYSMICON),
                               LR_DEFAULTCOLOR);
                d->idx_bad_token = ImageList_AddIcon(hw_ilist, hi) + 1 ;

                DestroyIcon(hi);
            }

            ListView_SetImageList(hw, hw_ilist, LVSIL_STATE);

            ListView_DeleteAllItems(hw);

            /* set the columns */
            {
                LVCOLUMN lc;
                wchar_t wbuf[256];

                lc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
                lc.fmt = LVCFMT_LEFT;
                lc.cx = ((r.right - r.left) * 2) / 5;
                LoadString(hResModule, IDS_NCAFS_COL_CELL, 
                           wbuf, ARRAYLENGTH(wbuf));
                lc.pszText = wbuf;

                ListView_InsertColumn(hw, 0, &lc);

                lc.mask |= LVCF_SUBITEM;
                //lc.cx is the same as above
                lc.iSubItem = NCAFS_IDX_REALM;
                LoadString(hResModule, IDS_NCAFS_COL_REALM, 
                           wbuf, ARRAYLENGTH(wbuf));

                ListView_InsertColumn(hw, 1, &lc);

                lc.cx = ((r.right - r.left) * 1) / 5;
                lc.iSubItem = NCAFS_IDX_METHOD;
                LoadString(hResModule, IDS_NCAFS_COL_METHOD, 
                           wbuf, ARRAYLENGTH(wbuf));

                ListView_InsertColumn(hw, 2, &lc);
            }

            /* Set the items for the 'method' combo box */
            hw = GetDlgItem(hwnd, IDC_NCAFS_METHOD);

            {
                wchar_t wbuf[KHUI_MAXCB_SHORT_DESC];
                afs_tk_method method = -1;
                int idx;

                SendMessage(hw, CB_RESETCONTENT, 0, 0);

                while((method = afs_get_next_method_id(method)) >= 0) {
                    afs_method_describe(method, KCDB_TS_SHORT,
                                        wbuf, sizeof(wbuf));
                    idx = (int)SendMessage(hw, CB_INSERTSTRING,
                                           (WPARAM) -1, (LPARAM) wbuf);
#ifdef DEBUG
                    assert(idx != CB_ERR);
#endif
                    SendMessage(hw, CB_SETITEMDATA, (WPARAM) idx,
                                (LPARAM)  method);
                }

                /* finally, set the current selection to auto, which
                   is the first method returned by
                   afs_get_next_method_id() */
                SendMessage(hw, CB_SETCURSEL, 0, 0);
            }

            d->afs_enabled = TRUE;
            SendDlgItemMessage(hwnd, IDC_NCAFS_OBTAIN, 
                               BM_SETCHECK, BST_CHECKED, 0);

            LeaveCriticalSection(&d->cs);

            /* the cells and realms combo boxes need to be filled
               in the plugin thread since that requires making
               potentially blocking and non-thread safe calls */
        }
        return TRUE;

    case WM_DESTROY:
        {
            afs_dlg_data * d;
            khui_new_creds_by_type * nct;

            d = (afs_dlg_data *)(LONG_PTR) 
                GetWindowLongPtr(hwnd, DWLP_USER);

            if (d == NULL)
                return TRUE;

            EnterCriticalSection(&d->cs);

            if (d->nc) {
                khui_cw_find_type(d->nc, afs_credtype_id, &nct);

                nct->aux = (LPARAM) NULL;
            }

            afs_cred_free_rows(&d->creds);

            LeaveCriticalSection(&d->cs);
            DeleteCriticalSection(&d->cs);

            PFREE(d);

            SetWindowLongPtr(hwnd, DWLP_USER, 0);
        }
        return TRUE;

    case WM_COMMAND:
        {
            afs_dlg_data * d;
            khui_new_creds_by_type * nct;

            d = (afs_dlg_data *)(LONG_PTR) 
                GetWindowLongPtr(hwnd, DWLP_USER);

            if (d == NULL)
                return FALSE;

            EnterCriticalSection(&d->cs);

            if (d->nc)
                khui_cw_find_type(d->nc, afs_credtype_id, &nct);
            else
                nct = NULL;

            nc_dlg_hide_tooltip(hwnd, 0);

            /* Handle WM_COMMAND */
            switch(wParam) {
            case MAKEWPARAM(IDC_NCAFS_OBTAIN, BN_CLICKED):
                {
                    BOOL c;
                    c = (SendDlgItemMessage(hwnd, IDC_NCAFS_OBTAIN, 
                                            BM_GETCHECK, 0, 0) 
                         == BST_CHECKED);
                    d->afs_enabled = c;
                    d->dirty = TRUE;
                    if (d->nc)
                        khui_cw_enable_type(d->nc, afs_credtype_id, c);
                    else if (d->config_dlg)
                        khui_cfg_set_flags_inst(&d->cfg,
                                                KHUI_CNFLAG_MODIFIED,
                                                KHUI_CNFLAG_MODIFIED);
                    nc_dlg_enable(hwnd, c);
                }
                break;

            case MAKEWPARAM(IDC_NCAFS_ADD_TOKEN, BN_CLICKED):
                {
                    nc_dlg_add_token(hwnd);
                }
                break;

            case MAKEWPARAM(IDC_NCAFS_DELETE_TOKEN, BN_CLICKED):
                {
                    nc_dlg_del_token(hwnd);
                }
                break;
            }

            LeaveCriticalSection(&d->cs);
        }
        return TRUE;

    case KHUI_WM_NC_NOTIFY:
        {
            afs_dlg_data * d;
            khui_new_creds_by_type * nct;

            d = (afs_dlg_data *)(LONG_PTR) 
                GetWindowLongPtr(hwnd, DWLP_USER);

            if (d == NULL)
                return TRUE;

            EnterCriticalSection(&d->cs);

            if (d->nc)
                khui_cw_find_type(d->nc, afs_credtype_id, &nct);
            else
                nct = NULL;

            switch(HIWORD(wParam)) {
            case WMNC_DIALOG_SETUP:
                {
                    SendDlgItemMessage(hwnd, IDC_NCAFS_CELL, 
                                       CB_RESETCONTENT, 0, 0);
                    
                    /* load the LRU cells */
                    {
                        wchar_t * buf;
                        wchar_t *s;
                        khm_size cbbuf;

                        if(khc_read_multi_string(csp_params, L"LRUCells", 
                                                 NULL, &cbbuf) == 
                           KHM_ERROR_TOO_LONG) {
                            buf = PMALLOC(cbbuf);
                            khc_read_multi_string(csp_params, L"LRUCells", 
                                                  buf, &cbbuf);
                            s = buf;
                            while(*s) {
                                SendDlgItemMessage(hwnd, IDC_NCAFS_CELL, 
                                                   CB_ADDSTRING, 0, (LPARAM) s);
                                s += wcslen(s) + 1;
                            }
                            PFREE(buf);
                        }
                    }

                    /* now, if the root cell is not in the LRU, add it */
                    {
                        char buf[256];
                        wchar_t wbuf[256];

                        if(!cm_GetRootCellName(buf)) {
                            AnsiStrToUnicode(wbuf, sizeof(wbuf), buf);
                            if(SendDlgItemMessage(hwnd,
                                                  IDC_NCAFS_CELL,
                                                  CB_FINDSTRINGEXACT,
                                                  (WPARAM) -1,
                                                  (LPARAM) wbuf) == CB_ERR) {
                                SendDlgItemMessage(hwnd, IDC_NCAFS_CELL, 
                                                   CB_ADDSTRING, 
                                                   0, (LPARAM) wbuf);
                            }
                            SendDlgItemMessage(hwnd, IDC_NCAFS_CELL, 
                                               CB_SELECTSTRING, 
                                               -1, (LPARAM) wbuf);
                        }
                    }

                    SendDlgItemMessage(hwnd, IDC_NCAFS_REALM, 
                                       CB_RESETCONTENT, 0, 0);

                    /* as for the realms, we have a special one here */
                    {
                        wchar_t wbuf[256];
                        int idx;

                        LoadString(hResModule, IDS_NC_REALM_AUTO, wbuf, 
                                   (int) ARRAYLENGTH(wbuf));
                        idx = (int) SendDlgItemMessage(hwnd, IDC_NCAFS_REALM, 
                                                       CB_ADDSTRING, 0, 
                                                       (LPARAM) wbuf);
                        /* item data for the realm strings is the
                           answer to the question, "is this the
                           'determine realm automatically' item?" */
                        SendDlgItemMessage(hwnd, IDC_NCAFS_REALM, 
                                           CB_SETITEMDATA, idx, TRUE);
                        SendDlgItemMessage(hwnd, IDC_NCAFS_REALM, 
                                           CB_SELECTSTRING, 
                                           -1, (LPARAM) wbuf);
                    }

                    /* load the LRU realms */
                    {
                        wchar_t * buf;
                        wchar_t *s;
                        int idx;
                        khm_size cbbuf;

                        if(khc_read_multi_string(csp_params, L"LRURealms", 
                                                 NULL, &cbbuf) == 
                           KHM_ERROR_TOO_LONG) {
                            buf = PMALLOC(cbbuf);
                            khc_read_multi_string(csp_params, L"LRURealms", 
                                                  buf, &cbbuf);
                            s = buf;
                            while(*s) {
                                if(SendDlgItemMessage(hwnd, IDC_NCAFS_REALM, 
                                                      CB_FINDSTRINGEXACT, -1, 
                                                      (LPARAM) s) == CB_ERR) {
                                    idx = 
                                        (int)
                                        SendDlgItemMessage(hwnd, 
                                                           IDC_NCAFS_REALM, 
                                                           CB_ADDSTRING, 
                                                           0, (LPARAM) s);
                                    SendDlgItemMessage(hwnd, IDC_NCAFS_REALM, 
                                                       CB_SETITEMDATA, 
                                                       idx, FALSE);
                                }

                                s += wcslen(s) + 1;
                            }
                            PFREE(buf);
                        }
                    }

                    if (d->nc)
                        khui_cw_enable_type(d->nc, afs_credtype_id, 
                                            d->afs_enabled);

                    nc_dlg_enable(hwnd, d->afs_enabled);

                    afs_dlg_update_rows(hwnd, d);
                }
                break;

            case WMNC_UPDATE_CREDTEXT:
                {
                    wchar_t wformat[256];
                    wchar_t wstr[2048];
                    khm_int32 flags;

                    if(nct->credtext) {
                        PFREE(nct->credtext);
                        nct->credtext = NULL;
                    }

#ifdef DEBUG
                    assert(d->nc);
#endif

                    if (d->nc->n_identities == 0 ||
                        KHM_FAILED(kcdb_identity_get_flags(d->nc->identities[0],
                                                           &flags)) ||
                        !(flags & KCDB_IDENT_FLAG_VALID))
                        /* in this case, we don't show any credential text */
                        break;

                    wstr[0] = 0;

                    if(!d->afs_enabled) {
                        LoadString(hResModule, IDS_AFS_CREDTEXT_DIS, 
                                   wstr, ARRAYLENGTH(wstr));
                    } else {
                        if(d->creds.n_rows == 0) {
                            LoadString(hResModule, IDS_AFS_CREDTEXT_0, 
                                       wstr, ARRAYLENGTH(wstr));
                        } else if(d->creds.n_rows == 1) {
                            LoadString(hResModule, IDS_AFS_CREDTEXT_1, 
                                       wformat, ARRAYLENGTH(wformat));
                            StringCbPrintf(wstr, sizeof(wstr), wformat, 
                                           d->creds.rows[0].cell);
                        } else {
                            int i;
                            wchar_t wcells[1024];

                            LoadString(hResModule, IDS_AFS_CREDTEXT_N, 
                                       wformat, ARRAYLENGTH(wformat));
                            wcells[0] = 0;
                            for(i=0; i<d->creds.n_rows; i++) {
                                if(i > 0)
                                    StringCbCat(wcells, sizeof(wcells), 
                                                L", ");
                                if(FAILED(StringCbCat(wcells, 
                                                      sizeof(wcells), 
                                                      d->creds.rows[i].cell))) {
                                    size_t cch;
                                    /* looks like we overflowed */
                                    /* add an ellipsis at the end */
                                    StringCchLength(wcells, ARRAYLENGTH(wcells), &cch);
                                    cch = min(ARRAYLENGTH(wcells) - 4, cch);
                                    StringCchCopy(wcells + cch, 4, L"...");

                                    break;
                                }
                            }

                            StringCbPrintf(wstr, sizeof(wstr), wformat, wcells);
                        }
                    }

                    if(wstr[0] != 0) {
                        size_t cbs;
                        StringCbLength(wstr, sizeof(wstr), &cbs);
                        cbs += sizeof(wchar_t);
                        assert(nct->credtext == NULL);
                        nct->credtext = PMALLOC(cbs);
                        StringCbCopy(nct->credtext, cbs, wstr);
                    } else {
                        /* something went wrong */
                        nct->credtext = NULL;
                    }
                }
                break;

            case WMNC_CREDTEXT_LINK:
                {
                    khui_htwnd_link * l;
                    wchar_t wid[KHUI_MAXCCH_HTLINK_FIELD];
                    wchar_t * wids;

                    l = (khui_htwnd_link *) lParam;

                    StringCchCopyN(wid, ARRAYLENGTH(wid), l->id, l->id_len);
                    wids = wcschr(wid, L':');
                        
                    if(!wids)
                        break;
                    else
                        wids++;

#ifdef DEBUG
                    assert(d->nc);
#endif

                    if(!wcscmp(wids, L"Enable")) {
                        SendDlgItemMessage(hwnd, IDC_NCAFS_OBTAIN, 
                                           BM_SETCHECK, BST_CHECKED, 0);
                        d->afs_enabled = TRUE;
                        khui_cw_enable_type(d->nc, afs_credtype_id, TRUE);
                        nc_dlg_enable(hwnd, TRUE);
                    }
                }
                break;

            case WMNC_IDENTITY_CHANGE:
                kmq_post_sub_msg(afs_sub, KMSG_CRED, 
                                 KMSG_CRED_DIALOG_NEW_IDENTITY, 0, 
                                 (void *) d->nc);
                break;
                
            case WMNC_AFS_UPDATE_ROWS:
                afs_dlg_update_rows(hwnd, d);

#ifdef DEBUG
                assert(d->nc);
#endif

                PostMessage(d->nc->hwnd, KHUI_WM_NC_NOTIFY, 
                            MAKEWPARAM(0, WMNC_UPDATE_CREDTEXT), 0);
                break;
            }

            LeaveCriticalSection(&d->cs);
        }
        return TRUE;

    case WM_NOTIFY:
        if(wParam == IDC_NCAFS_TOKENLIST) {
            LPNMHDR lpnmh = (LPNMHDR) lParam;

            if(lpnmh->code == LVN_ITEMCHANGED) {
                /* when an item in the list view is clicked, we
                   load the corresponding values into the edit and
                   combo boxes */
                NMLISTVIEW *lpnmlv = (NMLISTVIEW *) lpnmh;
                LVITEM lvi;
                HWND hw;
                int idx;
                int row;
                afs_dlg_data * d;

                if (!(lpnmlv->uChanged & LVIF_STATE) ||
                    !(lpnmlv->uNewState & LVIS_SELECTED) ||
                    (lpnmlv->iItem == -1))

                    return TRUE;

                d = (afs_dlg_data *)(LONG_PTR) 
                    GetWindowLongPtr(hwnd, DWLP_USER);

                if (d == NULL)
                    return FALSE;

                EnterCriticalSection(&d->cs);

                hw = GetDlgItem(hwnd, IDC_NCAFS_TOKENLIST);

                idx = lpnmlv->iItem;

                ZeroMemory(&lvi, sizeof(lvi));
                lvi.iItem = idx;
                lvi.iSubItem = 0;
                lvi.mask = LVIF_PARAM;

                if(!ListView_GetItem(hw, &lvi))
                    goto _done_notify_select;

                /* ok, now lvi.lParam should be the row of the token */
                row = (int) lvi.lParam;
                if(row < 0 || row >= d->creds.n_rows)
                    goto _done_notify_select;

                SetDlgItemText(hwnd, IDC_NCAFS_CELL, 
                               d->creds.rows[row].cell);
                if(d->creds.rows[row].realm != NULL) {
                    SetDlgItemText(hwnd, IDC_NCAFS_REALM, 
                                   d->creds.rows[row].realm);
                } else {
                    wchar_t wbuf[256];
                    int idx;
                        
                    LoadString(hResModule, IDS_NC_REALM_AUTO, wbuf, 
                               ARRAYLENGTH(wbuf));
                    idx = (int) SendDlgItemMessage(hwnd, IDC_NCAFS_REALM, 
                                                   CB_FINDSTRINGEXACT, -1, 
                                                   (LPARAM) wbuf);
                    SendDlgItemMessage(hwnd, IDC_NCAFS_REALM, CB_SETCURSEL,
                                       idx, 0);
                }
                SendDlgItemMessage(hwnd, IDC_NCAFS_METHOD, CB_SETCURSEL, 
                                   d->creds.rows[row].method, 0);
            _done_notify_select:
                LeaveCriticalSection(&d->cs);

            } else if (lpnmh->code == NM_DBLCLK) {

                LPNMITEMACTIVATE pnmi;
                LVITEM lvi;
                HWND hw;
                afs_dlg_data * d;
                int row;
                int x,y;
                RECT r;

                d = (afs_dlg_data *)(LONG_PTR) 
                    GetWindowLongPtr(hwnd, DWLP_USER);

                if (d == NULL)
                    return FALSE;

                EnterCriticalSection(&d->cs);

                pnmi = (LPNMITEMACTIVATE) lpnmh;

                hw = GetDlgItem(hwnd, IDC_NCAFS_TOKENLIST);

                ZeroMemory(&lvi, sizeof(lvi));
                lvi.iItem = pnmi->iItem;
                lvi.iSubItem = 0;
                lvi.mask = LVIF_PARAM;

                if (!ListView_GetItem(hw, &lvi))
                    goto _done_notify_click;

                row = (int) lvi.lParam;
                if(row < 0 || row >= d->creds.n_rows)
                    goto _done_notify_click;

                ListView_GetItemRect(hw, pnmi->iItem, &r, LVIR_SELECTBOUNDS);
                x = (r.left + r.right) / 2;
                y = (r.bottom);

                GetWindowRect(hw, &r);
                y += r.top;
                x += r.left;

                if (d->creds.rows[row].flags & DLGROW_FLAG_NOTOWNED) {
                    nc_dlg_show_tooltip(hwnd, 0,
                                        MAKEINTRESOURCE(IDS_NC_TT_CONFLICTD),
                                        MAKEINTRESOURCE(IDS_NC_TT_PROBLEM),
                                        2,
                                        x,y);
                } else if (d->creds.rows[row].flags &
                           DLGROW_FLAG_EXPIRED) {
                    nc_dlg_show_tooltip(hwnd, 0,
                                        MAKEINTRESOURCE(IDS_NC_TT_EXPIRED),
                                        MAKEINTRESOURCE(IDS_NC_TT_DETAILS),
                                        1,
                                        x, y);
                } else if (d->creds.rows[row].flags &
                           DLGROW_FLAG_EXISTS) {
                    nc_dlg_show_tooltip(hwnd, 0,
                                        MAKEINTRESOURCE(IDS_NC_TT_EXISTS),
                                        MAKEINTRESOURCE(IDS_NC_TT_DETAILS),
                                        1, x, y);
                } else {
                    nc_dlg_show_tooltip(hwnd, 0,
                                        MAKEINTRESOURCE(IDS_NC_TT_NEW),
                                        MAKEINTRESOURCE(IDS_NC_TT_DETAILS),
                                        1, x, y);
                }

            _done_notify_click:
                LeaveCriticalSection(&d->cs);
            }
        }
        return TRUE;

    case WM_TIMER:
        {
            if(wParam == DLG_TOOLTIP_TIMER_ID) {
                KillTimer(hwnd, DLG_TOOLTIP_TIMER_ID);
                nc_dlg_hide_tooltip(hwnd, 0);
            }
        }
        return TRUE;

    case WM_HELP:
        {
            static const DWORD ctx_help[] = {
                IDC_NCAFS_OBTAIN, IDH_OBTAIN,
                IDC_NCAFS_CELL, IDH_CELL,
                IDC_NCAFS_REALM, IDH_REALM,
                IDC_NCAFS_METHOD, IDH_METHOD,
                IDC_NCAFS_ADD_TOKEN, IDH_ADD,
                IDC_NCAFS_DELETE_TOKEN, IDH_DELETE,
                IDC_NCAFS_TOKENLIST, IDH_TOKENLIST,
                0
            };

            LPHELPINFO hlp;

            hlp = (LPHELPINFO) lParam;

            if (hlp->iContextType != HELPINFO_WINDOW)
                break;

            afs_html_help(hlp->hItemHandle, L"::/popups_newcred.txt",
                          HH_TP_HELP_WM_HELP, (DWORD_PTR) ctx_help);
        }
        return TRUE;
    } /* switch(uMsg) */

    return FALSE;
}


/* passed in to kcdb_credset_apply along with the afs_credset to adjust
   newly acquired credentials to include informatino derived from the
   new creds operation */
khm_int32 KHMAPI 
afs_adjust_token_ident_proc(khm_handle cred, void * vd)
{
    wchar_t cell[MAXCELLCHARS];
    afs_ident_token_set * b = (afs_ident_token_set *) vd;
    afs_cred_list * l;
    khm_size cbbuf;
    int i;

    l = b->l;

    /* ASSUMPTION: for each user, there can be tokens for only one
       cell */

    cbbuf = sizeof(cell);

    if(KHM_FAILED(kcdb_cred_get_attr(cred, afs_attr_cell, NULL, cell, &cbbuf)))
        return KHM_ERROR_SUCCESS; /* remember, kcdb doesn't care if
                                     this run succeeded or not.  all
                                     it wants to know if whether or
                                     not we want to continue the
                                     search */
    
    for(i=0; i<l->n_rows; i++) {
        if((l->rows[i].flags & DLGROW_FLAG_DONE) && 
           !_wcsicmp(cell, l->rows[i].cell)) {
            khm_int32 method;

            kcdb_cred_set_identity(cred, b->ident);
            if(l->rows[i].realm)
                kcdb_cred_set_attr(cred, afs_attr_realm, l->rows[i].realm, 
                                   KCDB_CBSIZE_AUTO);
            else
                kcdb_cred_set_attr(cred, afs_attr_realm, NULL, 0);

            method = l->rows[i].method;
            kcdb_cred_set_attr(cred, afs_attr_method, &method, 
                               KCDB_CBSIZE_AUTO);

            break;
        }
    }

    return KHM_ERROR_SUCCESS;
}

void
afs_cred_write_ident_data(afs_dlg_data * d) {
    wchar_t * lru_cell = NULL;
    wchar_t * lru_realm = NULL;
    wchar_t * id_cell = NULL;
    khm_size cbidcell;
    khm_size cbcell;
    khm_size cbrealm;
    khm_size cbt;
    size_t cbz;
    khm_handle h_idc = NULL;
    khm_handle h_afs = NULL;
    khm_handle h_acells = NULL;
    khm_handle h_cellmap = NULL;
    wchar_t idname[KCDB_IDENT_MAXCCH_NAME];
    khm_handle ident = NULL;
    afs_cred_list * l;
    int i;

    l = &d->creds;

    if (d->nc &&
        d->nc->n_identities > 0 &&
        d->nc->identities[0])

        ident = d->nc->identities[0];

    else if (d->config_dlg)

        ident = d->ident;

    if (!ident)
        return;

    cbt = sizeof(idname);
    kcdb_identity_get_name(ident, idname, &cbt);

    khc_open_space(csp_afscred, L"Cells", 0, &h_cellmap);

    if(ident) {
        if(KHM_SUCCEEDED(kcdb_identity_get_config(ident,
                                                  KHM_FLAG_CREATE,
                                                  &h_idc))) {
            khc_open_space(h_idc, CSNAME_AFSCRED, 
                           KHM_FLAG_CREATE, &h_afs);
        }

        if(h_afs) {
            khc_open_space(h_afs, L"Cells", KHM_FLAG_CREATE, 
                           &h_acells);
        }
    }

    if (h_afs && d) {
        khc_write_int32(h_afs, L"AFSEnabled",
                        !!d->afs_enabled);
    }

    if(khc_read_multi_string(csp_params, 
                             L"LRUCells", 
                             NULL,
                             &cbcell) == KHM_ERROR_TOO_LONG) {
        cbcell += MAXCELLCHARS * sizeof(wchar_t) * 
            l->n_rows;
        lru_cell = PMALLOC(cbcell);
        ZeroMemory(lru_cell, cbcell);
        cbt = cbcell;
                    
        khc_read_multi_string(csp_params,
                              L"LRUCells",
                              lru_cell,
                              &cbt);
    } else {
        cbcell = MAXCELLCHARS * sizeof(wchar_t) * l->n_rows + sizeof(wchar_t);
        if (l->n_rows > 0) {
            lru_cell = PMALLOC(cbcell);
            ZeroMemory(lru_cell, cbcell);
        } else {
            lru_cell = NULL;
            cbcell = 0;
        }
    }

    if(khc_read_multi_string(csp_params,
                             L"LRURealms",
                             NULL,
                             &cbrealm) == KHM_ERROR_TOO_LONG) {
        cbrealm += MAXCELLCHARS * sizeof(wchar_t) * l->n_rows;
        lru_realm = PMALLOC(cbrealm);
        ZeroMemory(lru_realm, cbrealm);
        cbt = cbrealm;

        khc_read_multi_string(csp_params,
                              L"LRURealms",
                              lru_realm,
                              &cbt);
    } else {
        cbrealm = MAXCELLCHARS * sizeof(wchar_t) * l->n_rows + sizeof(wchar_t);
        if (l->n_rows > 0) {
            lru_realm = PMALLOC(cbrealm);
            ZeroMemory(lru_realm, cbrealm);
        } else {
            lru_cell = NULL;
            cbrealm = 0;
        }
    }

    cbidcell = MAXCELLCHARS * sizeof(wchar_t) * l->n_rows + sizeof(wchar_t);
    if (l->n_rows > 0) {
        id_cell = PMALLOC(cbidcell);
        ZeroMemory(id_cell, cbidcell);
    } else {
        id_cell = NULL;
        cbidcell = 0;
    }

    for(i=0; i < l->n_rows; i++)
        if(!(l->rows[i].flags & DLGROW_FLAG_DELETED)) {
            khm_handle h_acell = NULL;
            
            if(!multi_string_find(lru_cell, 
                                  l->rows[i].cell, 0)) {
                cbz = cbcell;
                multi_string_append(lru_cell, &cbz, 
                                    l->rows[i].cell);
            }

            if(l->rows[i].realm && 
               !multi_string_find(lru_realm, 
                                  l->rows[i].realm, 0)) {
                cbz = cbrealm;
                multi_string_append(lru_realm, &cbz, 
                                    l->rows[i].realm);
            }

            cbz = cbidcell;
            multi_string_append(id_cell, &cbz, 
                                l->rows[i].cell);

            if(h_acells && 
               KHM_SUCCEEDED(khc_open_space(h_acells, 
                                            l->rows[i].cell, 
                                            KHM_FLAG_CREATE, 
                                            &h_acell))) {
                wchar_t methodname[KHUI_MAXCCH_NAME];

                afs_get_method_name(l->rows[i].method,
                                    methodname,
                                    sizeof(methodname));

                khc_write_string(h_acell, L"MethodName", 
                                 methodname);

                if(l->rows[i].realm)
                    khc_write_string(h_acell, L"Realm", 
                                     l->rows[i].realm);
                else
                    khc_write_string(h_acell, L"Realm", L"");
                khc_close_space(h_acell);
            }

            if (l->rows[i].flags & DLGROW_FLAG_DONE) {
                if (h_cellmap) {
                    khc_write_string(h_cellmap,
                                     l->rows[i].cell,
                                     idname);
                }
            }
        }

    if (lru_cell)                
        khc_write_multi_string(csp_params,
                               L"LRUCells", lru_cell);
    if (lru_realm)
        khc_write_multi_string(csp_params,
                               L"LRURealms", lru_realm);
    if (id_cell)
        khc_write_multi_string(h_afs, L"Cells",
                               id_cell);
    else
        khc_write_multi_string(h_afs, L"Cells", L"\0");

    if (d->config_dlg) {
        if (d->dirty)
            khui_cfg_set_flags_inst(&d->cfg, KHUI_CNFLAG_APPLIED,
                                    KHUI_CNFLAG_APPLIED |
                                    KHUI_CNFLAG_MODIFIED);
        else
            khui_cfg_set_flags_inst(&d->cfg, 0,
                                    KHUI_CNFLAG_MODIFIED);
    }

    d->dirty = FALSE;

    if(h_cellmap)
        khc_close_space(h_cellmap);
    if(h_idc)
        khc_close_space(h_idc);
    if(h_afs)
        khc_close_space(h_afs);
    if(h_acells)
        khc_close_space(h_acells);
    if(id_cell)
        PFREE(id_cell);
    if(lru_cell)
        PFREE(lru_cell);
    if(lru_realm)
        PFREE(lru_realm);
}

khm_int32
afs_msg_newcred(khm_int32 msg_subtype, 
                khm_ui_4 uparam, 
                void * vparam) {

    switch(msg_subtype) {
    case KMSG_CRED_NEW_CREDS:
        {
            khui_new_creds * nc;
            khui_new_creds_by_type * nct;
            wchar_t wbuf[256];
            size_t cbsize;

            nc = (khui_new_creds *) vparam;

            nct = PMALLOC(sizeof(*nct));
            ZeroMemory(nct, sizeof(*nct));

            nct->type = afs_credtype_id;
            nct->ordinal = 3;

            LoadString(hResModule, IDS_AFS_NAME, wbuf, ARRAYLENGTH(wbuf));
            StringCbLength(wbuf, sizeof(wbuf), &cbsize);
            cbsize += sizeof(wchar_t);

            nct->name = PMALLOC(cbsize);
            StringCbCopy(nct->name, cbsize, wbuf);

            nct->h_module = hResModule;
            nct->dlg_proc = afs_dlg_proc;
            nct->dlg_template = MAKEINTRESOURCE(IDD_NC_AFS);
            nct->type_deps[nct->n_type_deps++] = krb5_credtype_id;

            if (krb4_credtype_id < 0) {
                kcdb_credtype_get_id(KRB4_CREDTYPE_NAME,
                                     &krb4_credtype_id);
            }
            if (krb4_credtype_id >= 0) {
                nct->type_deps[nct->n_type_deps++] =
                    krb4_credtype_id;
            }

            khui_cw_add_type(nc, nct);
        }
        break;

    case KMSG_CRED_RENEW_CREDS:
        {
            khui_new_creds * nc;
            khui_new_creds_by_type * nct;

            nc = (khui_new_creds *) vparam;

            nct = PMALLOC(sizeof(*nct));
            ZeroMemory(nct, sizeof(*nct));

            nct->type = afs_credtype_id;
            nct->type_deps[nct->n_type_deps++] = krb5_credtype_id;
            if (krb4_credtype_id < 0) {
                kcdb_credtype_get_id(KRB4_CREDTYPE_NAME,
                                     &krb4_credtype_id);
            }
            if (krb4_credtype_id >= 0) {
                nct->type_deps[nct->n_type_deps++] =
                    krb4_credtype_id;
            }

            khui_cw_add_type(nc, nct);
        }
        break;

    case KMSG_CRED_DIALOG_PRESTART:
        {
            khui_new_creds * nc;
            khui_new_creds_by_type * nct = NULL;
            HWND hwnd;

            nc = (khui_new_creds *) vparam;
            khui_cw_find_type(nc, afs_credtype_id, &nct);

            if(!nct)
                break;

            hwnd = nct->hwnd_panel;
            if (!hwnd)
                break;

            PostMessage(hwnd, KHUI_WM_NC_NOTIFY,
                        MAKEWPARAM(0,WMNC_DIALOG_SETUP), 0);
        }
        break;

    case KMSG_CRED_DIALOG_NEW_IDENTITY:
        {
            khui_new_creds * nc;
            khui_new_creds_by_type * nct = NULL;
            afs_dlg_data * d;

            nc = (khui_new_creds *) vparam;
            khui_cw_find_type(nc, afs_credtype_id, &nct);

            if(nct == NULL)
                break;

            d = (afs_dlg_data *) nct->aux;

            if(d == NULL)
                break;

            EnterCriticalSection(&d->cs);

            if (nct->aux == 0) {
                LeaveCriticalSection(&d->cs);
                break;
            }

            /* we should load up the selected tokens for this
               identity */
            if(nc->n_identities == 0) {
                LeaveCriticalSection(&d->cs);
                /* no identities selected. nothing to do */
                break;
            }

            afs_cred_get_identity_creds(&d->creds, nc->identities[0],
                                        &d->afs_enabled);

            LeaveCriticalSection(&d->cs);

            PostMessage(nct->hwnd_panel, KHUI_WM_NC_NOTIFY,
                        MAKEWPARAM(0, WMNC_AFS_UPDATE_ROWS), 0);
        }
        break;

    case KMSG_CRED_PROCESS:
        {
            khui_new_creds * nc;
            khui_new_creds_by_type * nct = NULL;
            afs_cred_list tlist;
            afs_cred_list * l;
            int i;
            BOOL failed = FALSE;    /* one or more cells failed */
            BOOL succeeded = FALSE; /* one or more cells succeeded */
            BOOL free_tlist = FALSE;
            khm_handle ident = NULL;
            afs_dlg_data * d = NULL;
            BOOL get_tokens = TRUE;
            BOOL ident_renew_triggered = TRUE;
            khm_handle csp_afscred = NULL;
            khm_handle csp_cells = NULL;

            nc = (khui_new_creds *) vparam;
            khui_cw_find_type(nc, afs_credtype_id, &nct);

            if(!nct)
                break;

            _begin_task(0);
            _report_cs0(KHERR_INFO,
                        L"Getting AFS tokens...");
            _describe();

            if(nc->result != KHUI_NC_RESULT_PROCESS &&
               nc->subtype != KMSG_CRED_RENEW_CREDS) {
                /* nothing to do */
                khui_cw_set_response(nc, afs_credtype_id,
                                     KHUI_NC_RESPONSE_SUCCESS);

                _report_cs0(KHERR_INFO, 
                            L"Cancelling");
                _end_task();
                break;
            }

            /* we can't proceed if Kerberos 5 has failed */
            if(!khui_cw_type_succeeded(nc, krb5_credtype_id)) {
                khui_cw_set_response(nc, afs_credtype_id,
                                     KHUI_NC_RESPONSE_FAILED);

                _report_cs0(KHERR_INFO,
                            L"Kerberos 5 plugin failed to process credentials request.  Aborting");
                _end_task();
                break;
            }

            if (nc->subtype == KMSG_CRED_RENEW_CREDS) {

                if (nc->ctx.scope == KHUI_SCOPE_IDENT ||

                    (nc->ctx.scope == KHUI_SCOPE_CREDTYPE &&
                     nc->ctx.cred_type == afs_credtype_id) ||

                    (nc->ctx.scope == KHUI_SCOPE_CRED &&
                     nc->ctx.cred_type == afs_credtype_id)) {

                    _report_cs1(KHERR_INFO,
                                L"AFS Renew Creds :: ident %1!p!", 
                                _cptr(nc->ctx.identity));

                } else {

                    _report_cs0(KHERR_INFO,
                                L"Renew request not applicable to AFS");
                    _end_task();
                    break;

                }

                if (nc->ctx.identity != NULL) {
                    ident = nc->ctx.identity;
                } else {
                    khui_cw_set_response(nc, afs_credtype_id,
                                         KHUI_NC_RESPONSE_FAILED);

                    _report_cs0(KHERR_INFO,
                                L"No identity specified.  Aborting");
                    _end_task();
                    break;
                }

                ZeroMemory(&tlist, sizeof(tlist));
                l = &tlist;
                free_tlist = TRUE;

                afs_cred_get_identity_creds(l, ident, NULL);

                /* if the identity has any tokens associated with it
                   that aren't persistent, we should renew those as
                   well. */
                afs_cred_get_context_creds(l, &nc->ctx);

                if (nc->ctx.scope == KHUI_SCOPE_CREDTYPE ||
                    nc->ctx.scope == KHUI_SCOPE_CRED) {

                    ident_renew_triggered = FALSE;

                }

            } else {
                _report_cs1(KHERR_INFO,
                            L"AFS New Creds :: ident %1!p!",
                            _cptr(nc->identities[0]));

                d = (afs_dlg_data *) nct->aux;
                if(!d) {
                    _report_cs0(KHERR_INFO,
                                L"No dialog data found.  Aborting");

                    khui_cw_set_response(nc, afs_credtype_id,
                                         KHUI_NC_RESPONSE_FAILED);
                    _end_task();
                    break;
                }

                EnterCriticalSection(&d->cs);

                l = &d->creds;

                ident = nc->identities[0];
                if (!ident) {
                    LeaveCriticalSection(&d->cs);

                    _report_cs0(KHERR_INFO,
                                L"No identity specified. Aborting");

                    khui_cw_set_response(nc, afs_credtype_id,
                                         KHUI_NC_RESPONSE_FAILED);

                    _end_task();
                    break;
                }

                get_tokens = d->afs_enabled;
            }

            if (!get_tokens)
                goto _skip_tokens;

            if (KHM_SUCCEEDED(kmm_get_plugin_config(AFS_PLUGIN_NAME, 0,
                                                    &csp_afscred)))
                khc_open_space(csp_afscred, L"Cells", 0, &csp_cells);

            /* looks like k5 worked.  Now see about getting those
               tokens */
            for(i=0; i<l->n_rows; i++) {
                int code;
                char cell[MAXCELLCHARS];
                char realm[MAXCELLCHARS];
                khm_handle ctoken;
                FILETIME ft_old;
                FILETIME ft_new;
                time_t new_exp = 0;
                khm_size cb;
                khm_int32 method = AFS_TOKEN_AUTO;
                khm_handle csp_cell = NULL;

                if (l->rows[i].flags &
                    (DLGROW_FLAG_DONE | DLGROW_FLAG_DELETED))

                    continue;

                ZeroMemory(cell, sizeof(cell));
                ZeroMemory(realm, sizeof(realm));

                UnicodeStrToAnsi(cell, sizeof(cell), l->rows[i].cell);
                if (l->rows[i].realm != NULL)
                    UnicodeStrToAnsi(realm, sizeof(realm), 
                                     l->rows[i].realm);

                ZeroMemory(&ft_old, sizeof(ft_old));

                if (!ident_renew_triggered &&
                    (ctoken = afs_find_token(NULL, l->rows[i].cell))) {

                    cb = sizeof(ft_old);
                    kcdb_cred_get_attr(ctoken, KCDB_ATTR_EXPIRE,
                                       NULL, &ft_old, &cb);

                    kcdb_cred_release(ctoken);
                }

                if (l->rows[i].method == AFS_TOKEN_AUTO && csp_cells &&
                    KHM_SUCCEEDED(khc_open_space(csp_cells,
                                                 l->rows[i].cell, 0,
                                                 &csp_cell))) {

                    if (KHM_FAILED(khc_read_int32(csp_cell, L"Method", &method))) {
                        method = l->rows[i].method;
                    } else {
                        _report_cs3(KHERR_INFO,
                                    L"Overriding method %1!d! with global default %2!d! for cell %3!s!",
                                    _int32(l->rows[i].method),
                                    _int32(method),
                                    _cstr(l->rows[i].cell));
                        _resolve();
                    }

                    khc_close_space(csp_cell);               
                } else {
                    method = l->rows[i].method;
                }

                _report_cs3(KHERR_INFO,
                            L"Getting tokens for cell %1!S! with realm %2!S! using method %3!d!",
                            _cstr(cell),
                            _cstr(realm),
                            _int32(method));
                _resolve();

                /* make the call */
                code = afs_klog(ident, "", cell, realm, 0, 
                                method, &new_exp);

                _report_cs1(KHERR_INFO,
                            L"klog returns code %1!d!",
                            _int32(code));

                if(code) {
                    failed = TRUE;
                    l->rows[i].flags &= ~DLGROW_FLAG_DONE;

                    if (!kherr_is_error()) {
                        /* failed to get tokens, but no error was reported */
                        _report_sr1(KHERR_ERROR, IDS_ERR_GENERAL,
                                    _cptr(cell));
                        _resolve();
                    }

                } else {
                    l->rows[i].flags |= DLGROW_FLAG_DONE;
                    succeeded = TRUE;

                    if (new_exp &&
                        !ident_renew_triggered) {
                        TimetToFileTime(new_exp, &ft_new);

                        if (CompareFileTime(&ft_old, &ft_new) >= 0) {
                            /* getting a new token didn't improve the
                               situation much.  We only get here if we
                               were trying to renew tokens. So we try
                               to trigger an identity renewal.  Doing
                               so should get us new initial tickets
                               which will allow us to get a better
                               token. */

                            khui_action_context ctx;

                            _reportf(L"Renewal of AFS tokens for cell %s failed to get a longer token.  Triggering identity renewal", l->rows[i].cell);

                            khui_context_create(&ctx,
                                                KHUI_SCOPE_IDENT,
                                                nc->ctx.identity,
                                                KCDB_CREDTYPE_INVALID,
                                                NULL);
                            khui_action_trigger(KHUI_ACTION_RENEW_CRED,
                                                &ctx);

                            khui_context_release(&ctx);

                            ident_renew_triggered = TRUE;
                        }
                    }
                }
            }

        _skip_tokens:

            if(failed) {
                /* we should indicate errors if anything went wrong */
                khui_cw_set_response(nc, afs_credtype_id, 
                                     KHUI_NC_RESPONSE_FAILED);
            } else {
                khui_cw_set_response(nc, afs_credtype_id,
                                     KHUI_NC_RESPONSE_SUCCESS);
            }

            if (succeeded && nc->subtype == KMSG_CRED_RENEW_CREDS) {
                afs_ident_token_set b;

                afs_list_tokens_internal();

                /* the tokens that we just acquired need adjusting to
                   include the realm, method and identity information
                   derived from the new creds operation.  this is done
                   in afs_adjust_token_ident_proc */
                b.ident = ident;
                b.l = l;
                b.add_new = FALSE;
                b.update_info = FALSE;

                kcdb_credset_apply(afs_credset, afs_adjust_token_ident_proc, 
                                   (void *) &b);

                kcdb_credset_collect(NULL, afs_credset, NULL, 
                                     afs_credtype_id, NULL);

            } else if (nc->subtype == KMSG_CRED_NEW_CREDS) {
                afs_ident_token_set b;

                afs_list_tokens_internal();

                /* the tokens that we just acquired need adjusting to
                   include the realm, method and identity information
                   derived from the new creds operation.  this is done
                   in afs_adjust_token_ident_proc */
                b.ident = ident;
                b.l = l;
                b.add_new = FALSE;
                b.update_info = FALSE;

                kcdb_credset_apply(afs_credset, afs_adjust_token_ident_proc, 
                                   (void *) &b);

                kcdb_credset_collect(NULL, afs_credset, NULL, 
                                     afs_credtype_id, NULL);

                afs_cred_write_ident_data(d);
            }

            if (d)
                LeaveCriticalSection(&d->cs);

            if (free_tlist) {
                afs_cred_free_rows(&tlist);
            }

            if (csp_afscred)
                khc_close_space(csp_afscred);

            if (csp_cells)
                khc_close_space(csp_cells);

            _end_task();
        }
        break;

    case KMSG_CRED_END:
        {
            khui_new_creds * nc;
            khui_new_creds_by_type * nct;

            nc = (khui_new_creds *) vparam;
            khui_cw_find_type(nc, afs_credtype_id, &nct);

            if(!nct)
                break;

            khui_cw_del_type(nc, afs_credtype_id);
    
            if (nct->name)
                PFREE(nct->name);
            if (nct->credtext)
                PFREE(nct->credtext);

            PFREE(nct);
        }
        break;
    }

    return KHM_ERROR_SUCCESS;
}
