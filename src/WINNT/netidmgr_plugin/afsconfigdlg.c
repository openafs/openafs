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
#include<kherror.h>
#include<khuidefs.h>
#include<commctrl.h>
#include<help/afsplhlp.h>
#include<htmlhelp.h>

/* disable the 'name was marked as deprecated' warnings.  These get
   issued for Str?Cat? and Str?Cpy? functions.  We don't use those
   anyway. */
#pragma warning(push)
#pragma warning(disable: 4995)
#include<shlwapi.h>
#include<shlobj.h>
#pragma warning(pop)

#include<assert.h>

typedef struct tag_afs_ids_dlg_data {
    khui_config_init_data cfg;

    khm_boolean afs_enabled;
} afs_ids_dlg_data;

INT_PTR CALLBACK
afs_cfg_ids_proc(HWND hwnd,
                 UINT uMsg,
                 WPARAM wParam,
                 LPARAM lParam) {

    afs_ids_dlg_data * d = NULL;

    switch(uMsg) {
    case WM_INITDIALOG:
        {
            khm_int32 t = 1;

            d = PMALLOC(sizeof(*d));
            ZeroMemory(d, sizeof(*d));

#pragma warning(push)
#pragma warning(disable: 4244)
            SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR) d);
#pragma warning(pop)

            d->cfg = *((khui_config_init_data *) lParam);

            khc_read_int32(csp_params, L"AFSEnabled", &t);

            d->afs_enabled = !!t;

            CheckDlgButton(hwnd, IDC_CFG_OBTAIN,
                           (d->afs_enabled)? BST_CHECKED: BST_UNCHECKED);
        }
        return FALSE;

    case WM_DESTROY:
        {
            d = (afs_ids_dlg_data *) (LONG_PTR)
                GetWindowLongPtr(hwnd, DWLP_USER);

            if (d != NULL) {
                PFREE(d);
                SetWindowLongPtr(hwnd, DWLP_USER, 0);
            }
        }
        return TRUE;

    case WM_COMMAND:
        {
            d = (afs_ids_dlg_data *) (LONG_PTR)
                GetWindowLongPtr(hwnd, DWLP_USER);

            if (d == NULL)
                return FALSE;

            if (wParam == MAKEWPARAM(IDC_CFG_OBTAIN, BN_CLICKED)) {
                d->afs_enabled =
                    (IsDlgButtonChecked(hwnd, IDC_CFG_OBTAIN) ==
                     BST_CHECKED);
                khui_cfg_set_flags_inst(&d->cfg, KHUI_CNFLAG_MODIFIED,
                                        KHUI_CNFLAG_MODIFIED);
                return TRUE;
            }
        }
        return FALSE;

    case KHUI_WM_CFG_NOTIFY:
        {
            d = (afs_ids_dlg_data *) (LONG_PTR)
                GetWindowLongPtr(hwnd, DWLP_USER);

            if (d == NULL)
                return FALSE;

            if (HIWORD(wParam) == WMCFG_APPLY) {
                khm_int32 t;

                if (KHM_FAILED(khc_read_int32(csp_params,
                                              L"AFSEnabled", &t)) ||
                    !!t != !!d->afs_enabled) {
                    khc_write_int32(csp_params, L"AFSEnabled",
                                    !!d->afs_enabled);

                    khui_cfg_set_flags_inst(&d->cfg,
                                            KHUI_CNFLAG_APPLIED,
                                            KHUI_CNFLAG_APPLIED |
                                            KHUI_CNFLAG_MODIFIED);
                } else {
                    khui_cfg_set_flags_inst(&d->cfg,
                                            0,
                                            KHUI_CNFLAG_MODIFIED);
                }
            }
        }
        return TRUE;
    }

    return FALSE;
}

INT_PTR CALLBACK
afs_cfg_id_proc(HWND hwnd,
                UINT uMsg,
                WPARAM wParam,
                LPARAM lParam) {

    switch(uMsg) {

    case WM_INITDIALOG:
        {
            INT_PTR rv;
            afs_dlg_data * d;
            wchar_t idname[KCDB_IDENT_MAXCCH_NAME];
            khm_size cb;

            rv = afs_dlg_proc(hwnd, uMsg, wParam, 0);

            d = (afs_dlg_data *) (LONG_PTR) GetWindowLongPtr(hwnd, DWLP_USER);
#ifdef DEBUG
            assert(d != NULL);
#endif
            if (d == NULL)
                return rv;

            d->cfg = *((khui_config_init_data *) lParam);

            idname[0] = L'\0';
            cb = sizeof(idname);
            khui_cfg_get_name(d->cfg.ctx_node, idname, &cb);

            d->ident = NULL;
            kcdb_identity_create(idname, 0, &d->ident);

#ifdef DEBUG
            assert(d->ident);
#endif

            d->config_dlg = TRUE;

            afs_cred_get_identity_creds(&d->creds, d->ident, &d->afs_enabled);

            afs_dlg_proc(hwnd, KHUI_WM_NC_NOTIFY,
                         MAKEWPARAM(0, WMNC_DIALOG_SETUP), 0);

            return rv;
        }
        break;                  /* not reached */

    case WM_DESTROY:
        {
            afs_dlg_data * d;

            d = (afs_dlg_data *) (LONG_PTR) GetWindowLongPtr(hwnd, DWLP_USER);
#ifdef DEBUG
            assert(d && d->ident);
#endif
            if (d && d->ident) {
                kcdb_identity_release(d->ident);
            }

            return afs_dlg_proc(hwnd, uMsg, wParam, lParam);
        }
        break;                  /* not reached */

    case KHUI_WM_CFG_NOTIFY:
        {
            afs_dlg_data * d;

            d = (afs_dlg_data *) (LONG_PTR) GetWindowLongPtr(hwnd, DWLP_USER);

            if (d == NULL)
                return TRUE;

            if (HIWORD(wParam) == WMCFG_APPLY) {
                afs_cred_write_ident_data(d);
            }
        }
        return TRUE;

    default:
        return afs_dlg_proc(hwnd, uMsg, wParam, lParam);
    }
}

static void
set_service_status(HWND hwnd) {
    static DWORD wait_start = 0;
    DWORD status = 0;
    DWORD wait_hint = 0;
    unsigned int i;
    wchar_t status_strings_csv[1024];
    wchar_t status_strings_ms[1024];
    khm_size cb;
    wchar_t *t;

    GetServiceStatus(NULL,
                     TRANSARCAFSDAEMON,
                     &status, &wait_hint);

    LoadString(hResModule, IDS_CFG_SVCSTATUS,
               status_strings_csv, ARRAYLENGTH(status_strings_csv));

    cb = sizeof(status_strings_ms);
    csv_to_multi_string(status_strings_ms, &cb, status_strings_csv);

    for(i=0, t = status_strings_ms; t && *t && *t != L' ';
        t = multi_string_next(t), i++) {
        if (i == status)
            break;
    }

    if (!t || !*t)
        t = status_strings_ms;  /* the first one is "unknown". */

    SetDlgItemText(hwnd, IDC_CFG_STATUS, t);

    if (status != SERVICE_RUNNING) {
        HWND hw;

        hw = GetDlgItem(hwnd, IDC_CFG_STOP);
        if (hw == GetFocus())
            SetFocus(GetNextDlgTabItem(hwnd, hw, FALSE));

        EnableWindow(hw, FALSE);
    } else {
        EnableWindow(GetDlgItem(hwnd, IDC_CFG_STOP), TRUE);
    }

    if (status != SERVICE_STOPPED &&
        status != SERVICE_PAUSED) {
        HWND hw;

        hw = GetDlgItem(hwnd, IDC_CFG_START);
        if (hw == GetFocus())
            SetFocus(GetNextDlgTabItem(hwnd, hw, FALSE));

        EnableWindow(hw, FALSE);
    } else {
        EnableWindow(GetDlgItem(hwnd, IDC_CFG_START), TRUE);
    }

    if (status == SERVICE_START_PENDING ||
        status == SERVICE_STOP_PENDING) {
        HWND hw;
        DWORD now;
        int progress;

        hw = GetDlgItem(hwnd, IDC_CFG_PROGRESS);
#ifdef DEBUG
        assert(hw);
#endif
        if (!IsWindowVisible(hw))
            ShowWindow(hw, SW_SHOW);

        if (wait_start == 0)
            wait_start = GetTickCount();

        now = GetTickCount();

        if (now + wait_hint != wait_start)
            progress = (now - wait_start) * 100 /
                (now + wait_hint - wait_start);
        else
            progress = 0;

        SendMessage(hw, PBM_SETPOS, progress, 0);

        SetTimer(hwnd, 1, 500, NULL);
    } else {
        HWND hw;

        hw = GetDlgItem(hwnd, IDC_CFG_PROGRESS);
#ifdef DEBUG
        assert(hw);
#endif
        wait_start = 0;
        if (IsWindowVisible(hw))
            ShowWindow(hw, SW_HIDE);
    }
}

void
afs_cfg_show_last_error(HWND hwnd, wchar_t * prefix, DWORD code) {
    DWORD r;
    wchar_t * err_desc = NULL;
    wchar_t title[64];
    wchar_t msg[1024];
    wchar_t tmp[128];

    r = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_IGNORE_INSERTS |
                      FORMAT_MESSAGE_FROM_SYSTEM,
                      NULL,
                      code,
                      0,
                      (LPWSTR) &err_desc,
                      0,
                      NULL);

    if (r == 0 || err_desc == NULL)
        return;

    LoadString(hResModule, IDS_PLUGIN_DESC,
               title, ARRAYLENGTH(title));
    if (prefix == NULL)
        tmp[0] = L'\0';
    else if (IS_INTRESOURCE(prefix))
        LoadString(hResModule, (UINT)(UINT_PTR) prefix,
                   tmp, ARRAYLENGTH(tmp));
    else
        StringCbCopy(tmp, sizeof(tmp), prefix);

    StringCbPrintf(msg, sizeof(msg), L"%s%s",
                   tmp, err_desc);

    MessageBox(hwnd, msg, title, MB_OK | MB_APPLMODAL);

    LocalFree(err_desc);
}

#define SCNAME_AFSCREDS L"AFS Credentials.lnk"

BOOL
afs_cfg_get_afscreds_shortcut(wchar_t * wpath) {
    HRESULT hr;
    BOOL shortcut_found = FALSE;

    hr = SHGetFolderPath(NULL, CSIDL_COMMON_STARTUP,
                         NULL, SHGFP_TYPE_CURRENT,
                         wpath);
    if (FAILED(hr))
        goto _noshortcut;

    if (!PathAppend(wpath, SCNAME_AFSCREDS)) {
        goto _noshortcut;
    }

    if (PathFileExists(wpath)) {
        shortcut_found = TRUE;
    }

 _noshortcut:

    return shortcut_found;
}

INT_PTR CALLBACK
afs_cfg_main_proc(HWND hwnd,
                  UINT uMsg,
                  WPARAM wParam,
                  LPARAM lParam) {
    switch(uMsg) {
    case WM_INITDIALOG:
        {
            wchar_t imagepath[MAX_PATH];
            wchar_t blockname[MAX_PATH];
            HKEY service_key;
            LONG l;
            DWORD cb;
            DWORD dummy;
            LPVOID ver_info;
            wchar_t * value;

            struct LANGANDCODEPATH {
                WORD wLanguage;
                WORD wCodePage;
            } *translations;

#pragma warning(push)
#pragma warning(disable: 4244)
            SetWindowLongPtr(hwnd, DWLP_USER, (DWORD_PTR) lParam);
#pragma warning(pop)

            /* Try to figure out if afscreds.exe is on the startup
               group for all users. */
            {
                khm_handle csp_afscred = NULL;
                khm_int32 disable = FALSE;

                if (KHM_SUCCEEDED(kmm_get_plugin_config(AFS_PLUGIN_NAME,
                                                        0,
                                                        &csp_afscred))) {

                    khc_read_int32(csp_afscred, L"Disableafscreds",
                                   &disable);

                    khc_close_space(csp_afscred);
                }

                if (!disable) {
                    CheckDlgButton(hwnd, IDC_CFG_STARTAFSCREDS,
                                   BST_UNCHECKED);
                } else {
                    CheckDlgButton(hwnd, IDC_CFG_STARTAFSCREDS,
                                   BST_CHECKED);
                }
            }

            l = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                             L"SYSTEM\\CurrentControlSet\\Services\\TransarcAFSDaemon",
                             0,
                             KEY_READ,
                             &service_key);

            if (l != ERROR_SUCCESS)
                goto _set_status;

            cb = sizeof(imagepath);
            l = RegQueryValueEx(service_key,
                                L"ImagePath",
                                NULL, NULL,
                                (LPBYTE) imagepath,
                                (DWORD *)&cb);
            if (l != ERROR_SUCCESS)
                goto _close_key;

            PathUnquoteSpaces(imagepath);

            dummy = 1;
            cb = GetFileVersionInfoSize(imagepath, &dummy);
            if (cb == 0 || dummy)
                goto _close_key;

            ver_info = malloc(cb);
#ifdef DEBUG
            assert(ver_info);
#endif
            if (!ver_info)
                goto _close_key;

            if (!GetFileVersionInfo(imagepath,
                                    0, cb, ver_info))
                goto _free_buffer;

            cb = 0;
            if (!VerQueryValue(ver_info, 
                               L"\\VarFileInfo\\Translation",
                               (LPVOID*) &translations,
                               (PUINT)&cb) ||
                cb == 0)
                goto _free_buffer;

            StringCbPrintf(blockname, sizeof(blockname),
                           L"\\StringFileInfo\\%04x%04x\\FileVersion",
                           translations[0].wLanguage,
                           translations[0].wCodePage);

            if (!VerQueryValue(ver_info,
                               blockname,
                               (LPVOID*) &value,
                               (PUINT)&cb) ||
                cb == 0)
                goto _free_buffer;

            SetDlgItemText(hwnd, IDC_CFG_VERSION, value);

            StringCbPrintf(blockname, sizeof(blockname),
                           L"\\StringFileInfo\\%04x%04x\\CompanyName",
                           translations[0].wLanguage,
                           translations[0].wCodePage);

            if (!VerQueryValue(ver_info,
                               blockname,
                               (LPVOID*) &value,
                               (PUINT)&cb) ||
                cb == 0)
                goto _free_buffer;

            SetDlgItemText(hwnd, IDC_CFG_COMPANY, value);

        _free_buffer:
            free(ver_info);
        _close_key:
            RegCloseKey(service_key);
        _set_status:
            set_service_status(hwnd);
        }
        return FALSE;

    case WM_COMMAND:
        switch(wParam) {
        case MAKEWPARAM(IDC_CFG_STOP, BN_CLICKED):
            {
                DWORD r;

                r = ServiceControl(NULL, TRANSARCAFSDAEMON, SERVICE_STOPPED);

                if (r)
                    afs_cfg_show_last_error(hwnd,
                                            MAKEINTRESOURCE(IDS_CFG_CANTSTOP),
                                            r);
                else
                    set_service_status(hwnd);
            }
            break;

        case MAKEWPARAM(IDC_CFG_START,BN_CLICKED):
            {
                DWORD r;
                r = ServiceControl(NULL, TRANSARCAFSDAEMON, SERVICE_RUNNING);

                if (r)
                    afs_cfg_show_last_error(hwnd,
                                            MAKEINTRESOURCE(IDS_CFG_CANTSTART),
                                            r);
                else
                    set_service_status(hwnd);
            }
            break;

        case MAKEWPARAM(IDC_CFG_CPL, BN_CLICKED):
            if (32 >= (LRESULT) ShellExecute (NULL, NULL, 
                                              L"AFS_CONFIG.EXE", NULL,
                                              NULL, SW_SHOW)) {
                MessageBox(NULL, 
                           L"Can't find file AFS_CONFIG.EXE", 
                           L"Error", MB_OK);
            }
            break;

        case MAKEWPARAM(IDC_CFG_STARTAFSCREDS, BN_CLICKED):
            {
                khui_config_node node;

                node = (khui_config_node) (DWORD_PTR)
                    GetWindowLongPtr(hwnd, DWLP_USER);

                if (node != NULL) {
                    khui_cfg_set_flags(node,
                                       KHUI_CNFLAG_MODIFIED,
                                       KHUI_CNFLAG_MODIFIED);
                }
            }
            break;
        }
        return TRUE;

    case KHUI_WM_CFG_NOTIFY:
        {
            if (HIWORD(wParam) == WMCFG_APPLY) {
                wchar_t wpath[MAX_PATH];
                int dlg_state;
                khui_config_node node;
                khm_handle csp_afscred = NULL;
                khm_int32 disable = FALSE;

                node = (khui_config_node) (DWORD_PTR)
                    GetWindowLongPtr(hwnd, DWLP_USER);

#ifdef DEBUG
                assert(node != NULL);
#endif
                if (node == NULL)
                    break;

                kmm_get_plugin_config(AFS_PLUGIN_NAME, KHM_PERM_WRITE,
                                      &csp_afscred);

                if (csp_afscred)
                    khc_read_int32(csp_afscred, L"Disableafscreds",
                                   &disable);

                dlg_state = IsDlgButtonChecked(hwnd, IDC_CFG_STARTAFSCREDS);

                if (!!disable !=
                    (dlg_state == BST_CHECKED)) {
                    if (csp_afscred)
                        khc_write_int32(csp_afscred,
                                        L"Disableafscreds",
                                        (dlg_state == BST_CHECKED));

                    khui_cfg_set_flags(node,
                                       KHUI_CNFLAG_APPLIED,
                                       KHUI_CNFLAG_MODIFIED |
                                       KHUI_CNFLAG_APPLIED);
                } else {
                    khui_cfg_set_flags(node, 0,
                                       KHUI_CNFLAG_MODIFIED);
                }

                if (dlg_state == BST_CHECKED &&
                    afs_cfg_get_afscreds_shortcut(wpath)) {

                    DeleteFile(wpath);
                }
            }
        }
        return TRUE;

    case WM_TIMER:
        if (wParam == 1) {
            KillTimer(hwnd, 1);
            set_service_status(hwnd);
        }
        break;

    case WM_DESTROY:
        return FALSE;

    case WM_HELP:
        {
            static const DWORD ctx_help[] = {
                IDC_CFG_STATUS, IDH_SVCSTATUS,
                IDC_CFG_STOP, IDH_SVCSTOP,
                IDC_CFG_START, IDH_SVCSTART,
                IDC_CFG_VERSION, IDH_SVCVERSION,
                IDC_CFG_COMPANY, IDH_SVCCOMPANY,
                IDC_CFG_CPL, IDH_SVCCPL,
                IDC_CFG_STARTAFSCREDS, IDH_STARTAFSCREDS,
                0
            };

            LPHELPINFO hlp;

            hlp = (LPHELPINFO) lParam;

            if (hlp->iContextType != HELPINFO_WINDOW)
                break;

            afs_html_help(hlp->hItemHandle, L"::/popups_cfg.txt",
                          HH_TP_HELP_WM_HELP, (DWORD_PTR) ctx_help);
        }
        return TRUE;
    }
    return FALSE;
}
