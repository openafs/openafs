// PropFile.cpp : implementation file
//

#include "stdafx.h"
#include "PropACL.h"

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
#include "gui2fs.h"
#include "msgs.h"


BOOL CPropACL::PropPageProc( HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam )
{
    switch(uMessage)
    {
    case WM_INITDIALOG:
        {
            CPropACL * sheetpage = (CPropACL*) ((LPPROPSHEETPAGE) lParam)->lParam;
            SetWindowLongPtr (hwnd, GWLP_USERDATA, (LONG_PTR) sheetpage);
            sheetpage->SetHwnd(hwnd);
            AfxSetResourceHandle(m_hInst);

            CString sText;

            if(filenames.GetCount() > 1) {
                // multiple items selected
                LoadString(sText, IDS_PROP_MULTIPLEITEMS);
                SetDlgItemText(hwnd, IDC_PROP_FILENAME, sText);

                EnablePermChanges(FALSE);
                EnableWindow(GetDlgItem(m_hwnd, IDC_ADD), FALSE);
                EnableWindow(GetDlgItem(m_hwnd, IDC_COPY), FALSE);
                EnableWindow(GetDlgItem(m_hwnd, IDC_CLEAN), FALSE);
                EnableWindow(GetDlgItem(m_hwnd, IDC_REMOVE), FALSE);
            } else {
                SetDlgItemText(hwnd, IDC_PROP_FILENAME, filenames.GetAt(0));

                if (GetRights(filenames.GetAt(0), m_Normal, m_Negative)) {
                    EnablePermChanges(FALSE);

                    int tabstops[2] = {10,58};
                    SendDlgItemMessage(hwnd, IDC_NORMAL_RIGHTS, LB_SETTABSTOPS, 2, (LPARAM)&tabstops);
                    FillACLList();
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
                    SaveACL(CString(), filenames.GetAt(0), m_Normal, m_Negative);
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
        case LBN_SELCHANGE:
            OnSelChange();
            return TRUE;
            break;
        case BN_CLICKED:
            switch (LOWORD(wParam))
            {
            case IDC_ADD:
                {
                    CAddAclEntryDlg dlg;

                    dlg.SetAclDlg(this);

                    if (dlg.DoModal() == IDCANCEL)
                        return FALSE;

                    OnSelChange();

                    CString name = dlg.GetName();
                    CString rights = dlg.GetRights();
                    BOOL bNormal = dlg.IsNormal();

                    if (bNormal) {
                        m_Normal.Add(name);
                        m_Normal.Add(rights);
                    } else {
                        m_Negative.Add(name);
                        m_Negative.Add(rights);
                    }

                    FillACLList();
                    ShowRights(rights);
                    EnablePermChanges(TRUE);
                    EnableWindow(GetDlgItem(m_hwnd, IDC_REMOVE), TRUE);
                    return TRUE;
                }
            case IDC_COPY:
                {
                    CCopyAclDlg dlg;

                    dlg.SetFromDir(filenames.GetAt(0));

                    if (dlg.DoModal() == IDCANCEL)
                        return FALSE;

                    CString strToDir = dlg.GetToDir();
                    BOOL bClear = dlg.GetClear();

                    CopyACL(strToDir, m_Normal, m_Negative, bClear);
                    OnSelChange();
                    return TRUE;
                }
            case IDC_REMOVE:
                OnRemove();
                return TRUE;
            case IDC_CLEAN:
                CleanACL(filenames);
                OnSelChange();
                return TRUE;
            case IDC_READ:
            case IDC_WRITE:
            case IDC_LOOKUP:
            case IDC_DELETE:
            case IDC_INSERT:
            case IDC_LOCK:
            case IDC_ADMINISTER:
                OnPermChange();
                return TRUE;
            }
            break;
        }
        break;
    }

    return FALSE;
}

void CPropACL::FillACLList()
{
    SendDlgItemMessage(m_hwnd, IDC_NORMAL_RIGHTS, LB_RESETCONTENT, 0, 0);
    int i;
    for (i = 0; i < m_Normal.GetSize(); i += 2) {
        CString sText = _T("+\t") + m_Normal[i + 1] + _T("\t") + m_Normal[i];
        SendDlgItemMessage(m_hwnd, IDC_NORMAL_RIGHTS, LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)sText);
    }

    for (i = 0; i < m_Negative.GetSize(); i += 2) {
        CString sText = _T("-\t") + m_Negative[i + 1] + _T("\t") + m_Negative[i];
        SendDlgItemMessage(m_hwnd, IDC_NORMAL_RIGHTS, LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)sText);
    }
}

void CPropACL::ShowRights(const CString& strRights)
{
    SendDlgItemMessage(m_hwnd, IDC_READ, BM_SETCHECK, (strRights.Find(_T("r")) == -1) ? UNCHECKED : CHECKED, 0);
    SendDlgItemMessage(m_hwnd, IDC_WRITE, BM_SETCHECK, (strRights.Find(_T("w")) == -1) ? UNCHECKED : CHECKED, 0);
    SendDlgItemMessage(m_hwnd, IDC_LOOKUP, BM_SETCHECK, (strRights.Find(_T("l")) == -1) ? UNCHECKED : CHECKED, 0);
    SendDlgItemMessage(m_hwnd, IDC_DELETE, BM_SETCHECK, (strRights.Find(_T("d")) == -1) ? UNCHECKED : CHECKED, 0);
    SendDlgItemMessage(m_hwnd, IDC_INSERT, BM_SETCHECK, (strRights.Find(_T("i")) == -1) ? UNCHECKED : CHECKED, 0);
    SendDlgItemMessage(m_hwnd, IDC_LOCK, BM_SETCHECK, (strRights.Find(_T("k")) == -1) ? UNCHECKED : CHECKED, 0);
    SendDlgItemMessage(m_hwnd, IDC_ADMINISTER, BM_SETCHECK, (strRights.Find(_T("a")) == -1) ? UNCHECKED : CHECKED, 0);
}

CString CPropACL::MakeRightsString()
{
    CString str;

    if (SendDlgItemMessage(m_hwnd, IDC_READ, BM_GETCHECK, 0,0) == CHECKED)
        str += _T("r");
    if (SendDlgItemMessage(m_hwnd, IDC_LOOKUP, BM_GETCHECK, 0,0) == CHECKED)
        str += _T("l");
    if (SendDlgItemMessage(m_hwnd, IDC_INSERT, BM_GETCHECK, 0,0) == CHECKED)
        str += _T("i");
    if (SendDlgItemMessage(m_hwnd, IDC_DELETE, BM_GETCHECK, 0,0) == CHECKED)
        str += _T("d");
    if (SendDlgItemMessage(m_hwnd, IDC_WRITE, BM_GETCHECK, 0,0) == CHECKED)
        str += _T("w");
    if (SendDlgItemMessage(m_hwnd, IDC_LOCK, BM_GETCHECK, 0,0) == CHECKED)
        str += _T("k");
    if (SendDlgItemMessage(m_hwnd, IDC_ADMINISTER, BM_GETCHECK, 0,0) == CHECKED)
        str += _T("a");

    return str;
}

void CPropACL::EnablePermChanges(BOOL bEnable)
{
    EnableWindow(GetDlgItem(m_hwnd, IDC_READ), bEnable);
    EnableWindow(GetDlgItem(m_hwnd, IDC_WRITE), bEnable);
    EnableWindow(GetDlgItem(m_hwnd, IDC_LOOKUP), bEnable);
    EnableWindow(GetDlgItem(m_hwnd, IDC_DELETE), bEnable);
    EnableWindow(GetDlgItem(m_hwnd, IDC_INSERT), bEnable);
    EnableWindow(GetDlgItem(m_hwnd, IDC_LOCK), bEnable);
    EnableWindow(GetDlgItem(m_hwnd, IDC_ADMINISTER), bEnable);
}

void CPropACL::OnNothingSelected()
{
    SendDlgItemMessage(m_hwnd, IDC_NORMAL_RIGHTS, LB_SETSEL, FALSE, (LPARAM)-1);

    ShowRights(_T(""));			// Show no rights
    EnablePermChanges(FALSE);		// Allow no rights changes
    EnableWindow(GetDlgItem(m_hwnd, IDC_REMOVE), FALSE);
}

void CPropACL::OnSelection()
{
    EnablePermChanges(TRUE);
    EnableWindow(GetDlgItem(m_hwnd, IDC_REMOVE), TRUE);
}

void CPropACL::OnSelChange()
{
    int nNum = (int)SendDlgItemMessage(m_hwnd, IDC_NORMAL_RIGHTS, LB_GETSELCOUNT, 0, 0);

    if (nNum <= 0)
        EnableWindow(GetDlgItem(m_hwnd, IDC_REMOVE), FALSE);

    if (nNum != 1) {
        ShowRights(_T(""));
        EnablePermChanges(FALSE);
        return;
    }

    int nSel = 0;
    SendDlgItemMessage(m_hwnd, IDC_NORMAL_RIGHTS, LB_GETSELITEMS, 1, (LPARAM)&nSel);

    CString strRights;
    if (nSel * 2 >= m_Normal.GetCount()) {
        nSel -= (int)m_Normal.GetCount() / 2;
        strRights = m_Negative[(nSel * 2) + 1];
    } else {
        strRights = m_Normal[(nSel * 2) + 1];
    }

    ShowRights(strRights);
    OnSelection();
}

void CPropACL::OnRemove()
{
    int nNum = (int)SendDlgItemMessage(m_hwnd, IDC_NORMAL_RIGHTS, LB_GETSELCOUNT, 0, 0);
    if (nNum < 0) {
        ASSERT(FALSE);
        return;
    }

    int *pIndexes = new int[nNum];
    SendDlgItemMessage(m_hwnd, IDC_NORMAL_RIGHTS, LB_GETSELITEMS, nNum, (LPARAM)pIndexes);

    for (int i = nNum - 1; i >= 0; i--) {
        SendDlgItemMessage(m_hwnd, IDC_NORMAL_RIGHTS, LB_DELETESTRING, pIndexes[i], 0);

        if (pIndexes[i] * 2 >= m_Normal.GetCount()) {
            pIndexes[i] -= (int)m_Normal.GetCount() / 2;
            m_Negative.RemoveAt((pIndexes[i] * 2), 2);
        } else {
            m_Normal.RemoveAt((pIndexes[i] * 2), 2);
        }
    }

    delete [] pIndexes;

    OnSelChange();
}

BOOL CPropACL::IsNameInUse( BOOL bNormal, const CString& strName )
{
    if (bNormal) {
        for (int i = 0; i < m_Normal.GetSize(); i += 2)
            if (m_Normal[i] == strName)
                return TRUE;
        return FALSE;
    }

    for (int i = 0; i < m_Negative.GetSize(); i += 2)
        if (m_Negative[i] == strName)
            return TRUE;
    return FALSE;
}

void CPropACL::OnPermChange()
{
    int nSel = 0;
    SendDlgItemMessage(m_hwnd, IDC_NORMAL_RIGHTS, LB_GETSELITEMS, 1, (LPARAM)&nSel);

    CString str = MakeRightsString();
    if (nSel * 2 >= m_Normal.GetCount()) {
        nSel -= (int)m_Normal.GetCount() / 2;
        m_Negative[(2 * nSel) + 1] = str;
        str += _T("\t") + m_Negative[(2 * nSel)];

        // restore value for LB_SETSEL message
        nSel += (int)m_Normal.GetCount() / 2;
    } else {
        m_Normal[(2 * nSel) + 1] = str;
        str += _T("\t") + m_Normal[(2 * nSel)];
    }

    FillACLList();
    SendDlgItemMessage(m_hwnd, IDC_NORMAL_RIGHTS, LB_SETSEL, TRUE, nSel);

    SendMessage(GetParent(m_hwnd), PSM_CHANGED, (WPARAM)m_hwnd, 0);

    OnSelChange();
}
