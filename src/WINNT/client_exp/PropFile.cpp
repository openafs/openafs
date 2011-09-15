// PropFile.cpp : implementation file
//

#include "stdafx.h"
#include "PropFile.h"

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "WINNT\talocale.h"
#include "afs_shl_ext.h"
#include "set_afs_acl.h"
#include "clear_acl_dlg.h"
#include "add_acl_entry_dlg.h"
#include "copy_acl_dlg.h"
#include "make_mount_point_dlg.h"
#include "make_symbolic_link_dlg.h"
#include "gui2fs.h"
#include "msgs.h"


BOOL CPropFile::PropPageProc( HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam )
{
    switch(uMessage)
    {
    case WM_INITDIALOG:
        {
            CPropFile * sheetpage = (CPropFile*) ((LPPROPSHEETPAGE) lParam)->lParam;
            SetWindowLongPtr (hwnd, GWLP_USERDATA, (LONG_PTR) sheetpage);
            sheetpage->SetHwnd(hwnd);
            AfxSetResourceHandle(m_hInst);

            CString sText;

            if(filenames.GetCount() > 1) {
                // multiple items selected
                LoadString(sText, IDS_PROP_MULTIPLEITEMS);
                SetDlgItemText(hwnd, IDC_PROP_TYPE, sText);
            } else {
                if (m_bIsDir)
                    LoadString(sText, IDS_PROP_TYPEDIRECTORY);
                else
                    LoadString(sText, IDS_PROP_TYPEFILE);
                if (m_bIsMountpoint)
                    LoadString(sText, IDS_PROP_TYPEMOUNTPOINT);
                if (m_bIsSymlink)
                    LoadString(sText, IDS_PROP_TYPESYMLINK);
                SetDlgItemText(hwnd, IDC_PROP_TYPE, sText);
            }
            if (m_bIsMountpoint) {
                ShowWindow(GetDlgItem(m_hwnd, IDC_REMOVEMOUNTPOINT), SW_SHOW);
                ShowWindow(GetDlgItem(m_hwnd, IDC_SYMLINK_LABEL), SW_HIDE);
            }
            if (m_bIsSymlink) {
                ShowWindow(GetDlgItem(m_hwnd, IDC_MOUNTPOINT_LABEL), SW_HIDE);
            }
            CString user, group, other, suid;
            GetUnixModeBits(filenames.GetAt(0), user, group, other, suid);
            ShowUnixMode(user, group, other, suid);

            if (filenames.GetCount() == 1) {
                SetDlgItemText(hwnd, IDC_PROP_FILENAME, filenames.GetAt(0));
                if (!GetFID(filenames.GetAt(0), sText, TRUE))
                    sText = _T("(unknown)");
                SetDlgItemText(hwnd, IDC_PROP_FID, sText);
                sText = GetCellName(filenames.GetAt(0));
                m_cellName = sText;
                SetDlgItemText(hwnd, IDC_PROP_CELL, sText);
                sText = GetServer(filenames.GetAt(0));
                m_volName = sText;
                SetDlgItemText(hwnd, IDC_PROP_FILESERVER, sText);
                sText = GetOwner(filenames.GetAt(0));
                SetDlgItemText(hwnd, IDC_PROP_OWNER, sText);
                sText = GetGroup(filenames.GetAt(0));
                SetDlgItemText(hwnd, IDC_PROP_GROUP, sText);

                if (!m_bIsMountpoint && !m_bIsSymlink)
                    ShowWindow(GetDlgItem(m_hwnd, IDC_EDIT), SW_HIDE);

                if (!m_bIsMountpoint && m_bIsSymlink)
                    ShowWindow(GetDlgItem(m_hwnd, IDC_REMOVESYMLINK), SW_SHOW);

                if (m_bIsMountpoint){
                    sText = GetMountpoint(filenames.GetAt(0));
                    sText = sText.Mid(sText.Find('\t')+1);
                    SetDlgItemText(hwnd, IDC_PROP_SMINFO, sText);
                }
                if (m_bIsSymlink){
                    sText = GetSymlink(filenames.GetAt(0));
                    sText = sText.Mid(sText.Find('\t')+1);
                    SetDlgItemText(hwnd, IDC_PROP_SMINFO, sText);
                }
            }
            return TRUE;
        }
        break;
    case WM_NOTIFY:
        {
            LPNMHDR point = (LPNMHDR)lParam;
            int code = point->code;
            BOOL bRet = FALSE;
            switch (code)
            {
            case PSN_APPLY:
                {
                    CString user, group, other, suid;
                    MakeUnixModeString(user, group, other, suid);
                    SetUnixModeBits(filenames, user, group, other, suid);
                    // Return PSNRET_NOERROR to allow the sheet to close if the user clicked OK.
                    SetWindowLongPtr(m_hwnd, DWLP_MSGRESULT, PSNRET_NOERROR);
                }
                break;
            }
            SetWindowLongPtr(m_hwnd, DWLP_MSGRESULT, FALSE);
            return bRet;
        }
        break;
    case WM_COMMAND:
        switch (HIWORD(wParam))
        {
        case BN_CLICKED:
            switch (LOWORD(wParam))
            {
            case IDC_FLUSH:
                Flush(filenames);
                return TRUE;
            case IDC_REMOVESYMLINK:
                {
                    int nChoice = ShowMessageBox(IDS_REALLY_REMOVE_SYMLINK, MB_ICONQUESTION | MB_YESNO, IDS_REALLY_REMOVE_SYMLINK);
                    if (nChoice == IDYES)
                        RemoveSymlink(filenames.GetAt(0));
                    return TRUE;
                }
            case IDC_REMOVEMOUNTPOINT:
                {
                    int nChoice = ShowMessageBox(IDS_REALLY_DEL_MOUNT_POINTS, MB_ICONQUESTION | MB_YESNO, IDS_REALLY_DEL_MOUNT_POINTS);
                    if (nChoice == IDYES)
                        RemoveMount(filenames);
                    return TRUE;
                }
            case IDC_EDIT:
                {
                    if (m_bIsMountpoint){
                        CMakeMountPointDlg dlg;
                        dlg.SetDir(filenames.GetAt(0));
                        dlg.SetCell(m_cellName);
                        dlg.SetVol(m_volName);
                        dlg.DoModal();
                    }
                    if (m_bIsSymlink){
                        CMakeSymbolicLinkDlg dlg;
                        CStringA msg(filenames.GetAt(0));
                        int i;
                        if ((i=msg.ReverseFind('\\'))>0)
                            msg=msg.Left(i+1);
                        else if ((i=msg.ReverseFind(':'))>0)
                            msg=msg.Left(i+1)+"\\";
                        dlg.Setbase(msg);
                        dlg.m_strDir = filenames.GetAt(0);
                        dlg.m_strName = GetSymlink(filenames.GetAt(0));
                        dlg.DoModal();
                    }
                }
            case IDC_ATTR_USER_READ:
            case IDC_ATTR_USER_WRITE:
            case IDC_ATTR_USER_EXECUTE:
            case IDC_ATTR_GROUP_READ:
            case IDC_ATTR_GROUP_WRITE:
            case IDC_ATTR_GROUP_EXECUTE:
            case IDC_ATTR_OTHER_READ:
            case IDC_ATTR_OTHER_WRITE:
            case IDC_ATTR_OTHER_EXECUTE:
                SendMessage(GetParent(m_hwnd), PSM_CHANGED, (WPARAM)m_hwnd, 0);     // enable the "apply" button
                return TRUE;
            }
            break;
        }
        break;
    }

    return FALSE;
}

void CPropFile::ShowUnixMode(const CString& strUserRights, const CString& strGroupRights, const CString& strOtherRights, const CString& strSuidRights)
{
    SendDlgItemMessage(m_hwnd, IDC_ATTR_USER_READ, BM_SETCHECK, (strUserRights.Find(_T("r")) == -1) ? UNCHECKED : CHECKED, 0);
    SendDlgItemMessage(m_hwnd, IDC_ATTR_USER_WRITE, BM_SETCHECK, (strUserRights.Find(_T("w")) == -1) ? UNCHECKED : CHECKED, 0);
    SendDlgItemMessage(m_hwnd, IDC_ATTR_USER_EXECUTE, BM_SETCHECK, (strUserRights.Find(_T("x")) == -1) ? UNCHECKED : CHECKED, 0);

    SendDlgItemMessage(m_hwnd, IDC_ATTR_GROUP_READ, BM_SETCHECK, (strGroupRights.Find(_T("r")) == -1) ? UNCHECKED : CHECKED, 0);
    SendDlgItemMessage(m_hwnd, IDC_ATTR_GROUP_WRITE, BM_SETCHECK, (strGroupRights.Find(_T("w")) == -1) ? UNCHECKED : CHECKED, 0);
    SendDlgItemMessage(m_hwnd, IDC_ATTR_GROUP_EXECUTE, BM_SETCHECK, (strGroupRights.Find(_T("x")) == -1) ? UNCHECKED : CHECKED, 0);

    SendDlgItemMessage(m_hwnd, IDC_ATTR_OTHER_READ, BM_SETCHECK, (strOtherRights.Find(_T("r")) == -1) ? UNCHECKED : CHECKED, 0);
    SendDlgItemMessage(m_hwnd, IDC_ATTR_OTHER_WRITE, BM_SETCHECK, (strOtherRights.Find(_T("w")) == -1) ? UNCHECKED : CHECKED, 0);
    SendDlgItemMessage(m_hwnd, IDC_ATTR_OTHER_EXECUTE, BM_SETCHECK, (strOtherRights.Find(_T("x")) == -1) ? UNCHECKED : CHECKED, 0);

    SendDlgItemMessage(m_hwnd, IDC_ATTR_SUID_UID, BM_SETCHECK, (strSuidRights.Find(_T("s")) == -1) ? UNCHECKED : CHECKED, 0);
    SendDlgItemMessage(m_hwnd, IDC_ATTR_SUID_GID, BM_SETCHECK, (strSuidRights.Find(_T("g")) == -1) ? UNCHECKED : CHECKED, 0);
    SendDlgItemMessage(m_hwnd, IDC_ATTR_SUID_VTX, BM_SETCHECK, (strSuidRights.Find(_T("v")) == -1) ? UNCHECKED : CHECKED, 0);
}

void CPropFile::MakeUnixModeString(CString& userRights, CString& groupRights, CString& otherRights, CString& suidRights)
{
    userRights.Empty();
    groupRights.Empty();
    otherRights.Empty();
    suidRights.Empty();

    if (SendDlgItemMessage(m_hwnd, IDC_ATTR_USER_READ, BM_GETCHECK, 0,0) == CHECKED)
        userRights += _T("r");
    if (SendDlgItemMessage(m_hwnd, IDC_ATTR_USER_WRITE, BM_GETCHECK, 0,0) == CHECKED)
        userRights += _T("w");
    if (SendDlgItemMessage(m_hwnd, IDC_ATTR_USER_EXECUTE, BM_GETCHECK, 0,0) == CHECKED)
        userRights += _T("x");

    if (SendDlgItemMessage(m_hwnd, IDC_ATTR_GROUP_READ, BM_GETCHECK, 0,0) == CHECKED)
        groupRights += _T("r");
    if (SendDlgItemMessage(m_hwnd, IDC_ATTR_GROUP_WRITE, BM_GETCHECK, 0,0) == CHECKED)
        groupRights += _T("w");
    if (SendDlgItemMessage(m_hwnd, IDC_ATTR_GROUP_EXECUTE, BM_GETCHECK, 0,0) == CHECKED)
        groupRights += _T("x");

    if (SendDlgItemMessage(m_hwnd, IDC_ATTR_OTHER_READ, BM_GETCHECK, 0,0) == CHECKED)
        otherRights += _T("r");
    if (SendDlgItemMessage(m_hwnd, IDC_ATTR_OTHER_WRITE, BM_GETCHECK, 0,0) == CHECKED)
        otherRights += _T("w");
    if (SendDlgItemMessage(m_hwnd, IDC_ATTR_OTHER_EXECUTE, BM_GETCHECK, 0,0) == CHECKED)
        otherRights += _T("x");

    if (SendDlgItemMessage(m_hwnd, IDC_ATTR_SUID_UID, BM_GETCHECK, 0,0) == CHECKED)
        suidRights += _T("s");
    if (SendDlgItemMessage(m_hwnd, IDC_ATTR_SUID_GID, BM_GETCHECK, 0,0) == CHECKED)
        suidRights += _T("g");
    if (SendDlgItemMessage(m_hwnd, IDC_ATTR_SUID_VTX, BM_GETCHECK, 0,0) == CHECKED)
        suidRights += _T("v");
}
