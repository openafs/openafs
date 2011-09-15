// PropFile.cpp : implementation file
//

#include "stdafx.h"
#include "PropVolume.h"
#include <shlwapi.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "WINNT\talocale.h"
#include "afs_shl_ext.h"
#include "gui2fs.h"
#include "msgs.h"


BOOL CPropVolume::PropPageProc( HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam )
{
    switch(uMessage)
    {
    case WM_INITDIALOG:
        {
            CPropVolume * sheetpage = (CPropVolume*) ((LPPROPSHEETPAGE) lParam)->lParam;
            SetWindowLongPtr (hwnd, GWLP_USERDATA, (LONG_PTR) sheetpage);
            sheetpage->SetHwnd(hwnd);
            AfxSetResourceHandle(m_hInst);

            CString sText;

            CString user, group, other, suid;
            GetUnixModeBits(filenames.GetAt(0), user, group, other, suid);

            if(filenames.GetCount() > 1) {
                // multiple items selected
                LoadString(sText, IDS_PROP_MULTIPLEITEMS);
                SetDlgItemText(hwnd, IDC_PROP_FILENAME, sText);
            } else {
                SetDlgItemText(hwnd, IDC_PROP_VOLUMENAME, filenames.GetAt(0));
                if (!GetFID(filenames.GetAt(0), sText, TRUE))
                    sText = _T("(unknown)");
                SetDlgItemText(hwnd, IDC_PROP_FID, sText);
                sText = GetCellName(filenames.GetAt(0));
                SetDlgItemText(hwnd, IDC_PROP_CELL, sText);
                sText = GetServer(filenames.GetAt(0));
                SetDlgItemText(hwnd, IDC_PROP_FILESERVER, sText);
                sText = GetOwner(filenames.GetAt(0));
                SetDlgItemText(hwnd, IDC_PROP_OWNER, sText);
                sText = GetGroup(filenames.GetAt(0));
                SetDlgItemText(hwnd, IDC_PROP_GROUP, sText);

                TCHAR buf[100];
                CVolInfo volInfo;
                GetVolumeInfo(filenames.GetAt(0), volInfo);
                SetDlgItemText(hwnd, IDC_PROP_VOLUMENAME, volInfo.m_strName);
                sText.Format(_T("%ld bytes"), volInfo.m_nPartSize - volInfo.m_nPartFree);
                SetDlgItemText(hwnd, IDC_USEDBYTES, sText);
                StrFormatByteSize64(volInfo.m_nPartSize - volInfo.m_nPartFree, buf, 100);
                SetDlgItemText(hwnd, IDC_USEDBYTES2, buf);
                sText.Format(_T("%ld bytes"), volInfo.m_nPartFree);
                SetDlgItemText(hwnd, IDC_FREEBYTES, sText);
                StrFormatByteSize64(volInfo.m_nPartFree, buf, 100);
                SetDlgItemText(hwnd, IDC_FREEBYTES2, buf);
                sText.Format(_T("%ld bytes"), volInfo.m_nPartSize);
                SetDlgItemText(hwnd, IDC_TOTALBYTES, sText);
                StrFormatByteSize64(volInfo.m_nPartSize, buf, 100);
                SetDlgItemText(hwnd, IDC_TOTALBYTES2, buf);

                // "where is" info
                CStringArray servers;
                GetServers(filenames.GetAt(0), servers);
                int tabstops[1] = {118};
                SendDlgItemMessage(hwnd, IDC_SERVERS, LB_SETTABSTOPS, 1, (LPARAM)&tabstops);
                for (int i=0;i<servers.GetCount();++i){
                    SendDlgItemMessage(m_hwnd, IDC_SERVERS, LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)servers.GetAt(i));
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
                FlushVolume(filenames);
                return TRUE;
            }
            break;
        }
        break;
    }

    return FALSE;
}
