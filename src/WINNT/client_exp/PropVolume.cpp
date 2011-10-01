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

            BOOL bFollow = (filenames.GetCount() == 1 && m_bIsMountpoint);

            if(filenames.GetCount() >= 1) {
                CString sText;

                SetDlgItemText(hwnd, IDC_PROP_VOLUMENAME, filenames.GetAt(0));
                sText = GetCellName(filenames.GetAt(0), bFollow);
                SetDlgItemText(hwnd, IDC_PROP_CELL, sText);
                sText = GetServer(filenames.GetAt(0), bFollow);
                SetDlgItemText(hwnd, IDC_PROP_FILESERVER, sText);

                TCHAR buf[100];
                CVolInfo volInfo;

                if (GetVolumeInfo(filenames.GetAt(0), volInfo, bFollow)) {
                    SetDlgItemText(hwnd, IDC_PROP_VOLUMENAME, volInfo.m_strName);

                    SetDlgItemText(hwnd, IDC_PROP_VOLUME_STATUS, volInfo.m_strAvail);

                    sText.Format(_T("%u"), volInfo.m_nID);
                    SetDlgItemText(hwnd, IDC_PROP_VID, sText);

                    if (volInfo.m_nQuota == 0) {
                        SetDlgItemText(hwnd, IDC_QUOTA_MAX, _T("unlimited"));
                        SetDlgItemText(hwnd, IDC_QUOTA_PERCENT, _T("0.00%"));
                    } else {
                        StrFormatByteSize64(volInfo.m_nQuota*1024, buf, 100);
                        SetDlgItemText(hwnd, IDC_QUOTA_MAX, buf);

                        sText.Format(_T("%.2f%%"), ((double)volInfo.m_nUsed / (double)volInfo.m_nQuota) * 100);
                        SetDlgItemText(hwnd, IDC_QUOTA_PERCENT, sText);
                    }

                    StrFormatByteSize64(volInfo.m_nUsed*1024, buf, 100);
                    SetDlgItemText(hwnd, IDC_QUOTA_USED, buf);

                    StrFormatByteSize64(volInfo.m_nPartSize*1024, buf, 100);
                    SetDlgItemText(hwnd, IDC_PARTITION_SIZE, buf);
                    StrFormatByteSize64(volInfo.m_nPartFree*1024, buf, 100);
                    SetDlgItemText(hwnd, IDC_PARTITION_FREE, buf);

                    sText.Format(_T("%.2f%%"), ((double)volInfo.m_nPartFree / (double)volInfo.m_nPartSize) * 100);
                    SetDlgItemText(hwnd, IDC_PARTITION_PERCENT, sText);
                }
                else
                {
                    SetDlgItemText(hwnd, IDC_PROP_VOLUMENAME, volInfo.m_strErrorMsg);
                }

                // "where is" info
                CStringArray servers;
                GetServers(filenames.GetAt(0), servers, bFollow);
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
