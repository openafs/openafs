/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}
#include <stdio.h>
#include "afs_config.h"
#include <lanahelper.h>

/*
 * DEFINITIONS ________________________________________________________________
 *
 */


// Our dialog data
static BOOL fFirstTime = TRUE;
int nLanAdapter;
LANAINFO* lanainfo = NULL;

int GetAdapterNumber(TCHAR*);

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Binding_OnInitDialog (HWND hDlg);
void Binding_OnOK(HWND hDlg);
void Binding_OnCancel(HWND hDlg);
BOOL Binding_OnApply();

BOOL isGateway = FALSE;
/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK Binding_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         Binding_OnInitDialog (hDlg);
         break;

      case WM_CTLCOLORSTATIC:
         if ((HWND)lp == GetDlgItem (hDlg, IDC_CHUNK_SIZE))
            {
            if (IsWindowEnabled ((HWND)lp))
               {
               static HBRUSH hbrStatic = CreateSolidBrush (GetSysColor (COLOR_WINDOW));
               SetTextColor ((HDC)wp, GetSysColor (COLOR_WINDOWTEXT));
               SetBkColor ((HDC)wp, GetSysColor (COLOR_WINDOW));
               return (BOOL)hbrStatic;
               }
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
         {
            case IDHELP:
               Binding_DlgProc (hDlg, WM_HELP, 0, 0);
               break;

            case IDOK:
                Binding_OnOK(hDlg);
                break;
                
            case IDCANCEL:
                Binding_OnCancel(hDlg);
                break;
			      case IDC_DEFAULTNIC:
                if (HIWORD(wp) == BN_CLICKED)
                {
                  TCHAR name[MAX_PATH];
                  memset(name, 0, sizeof(name));
				          if (IsDlgButtonChecked(hDlg,IDC_DEFAULTNIC))
					          nLanAdapter=-1;
				          else
                  {
                    HWND hwndCombo = GetDlgItem(hDlg, IDC_NICSELECTION);
                    if (SendMessage(hwndCombo, CB_GETCURSEL, 0, 0) == CB_ERR)
                      SendMessage(hwndCombo, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);

                    TCHAR selected[MAX_PATH];
                    memset(selected, 0, sizeof(selected));
                    SendDlgItemMessage(hDlg, IDC_NICSELECTION, 
                          WM_GETTEXT, sizeof(selected), 
                          (LPARAM) selected); 

                    if (_tcslen(selected) <= 0)
                  
                      nLanAdapter = -1;
                    else
                      nLanAdapter = GetAdapterNumber(selected);
                  }
			            
                  lana_GetAfsNameString(nLanAdapter, isGateway, name);
                  SetDlgItemText (hDlg, IDC_BINDING_MESSAGE, name);
                  EnableWindow(GetDlgItem(hDlg,IDC_NICSELECTION),(nLanAdapter!=-1));
                  break;
                }
            case IDC_NICSELECTION:
              if (HIWORD(wp) == CBN_SELCHANGE)
              {
                TCHAR name[MAX_PATH];
                TCHAR selected[MAX_PATH];
                memset(name, 0, sizeof(name));
                memset(selected, 0, sizeof(selected));
                HWND hwndCombo = GetDlgItem(hDlg, IDC_NICSELECTION);
                int i = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                if (i != CB_ERR)
                  SendMessage(hwndCombo, CB_GETLBTEXT, (WPARAM)i, 
                        (LPARAM) selected); 

                if (_tcslen(selected) <= 0)
                  nLanAdapter = -1;
                else
                  nLanAdapter = GetAdapterNumber(selected);
        
			          
                lana_GetAfsNameString(nLanAdapter, isGateway, name);
                SetDlgItemText (hDlg, IDC_BINDING_MESSAGE, name);
                break;
              }

         }
         break;

      case WM_HELP:
         WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_ADVANCED_MISC);
         break;
      }

   return FALSE;
}


void Binding_OnInitDialog (HWND hDlg)
{
    TCHAR name[MAX_PATH];
    memset(name, 0, sizeof(name));
      
    if (fFirstTime) {
        Config_GetLanAdapter(&g.Configuration.nLanAdapter);
        nLanAdapter = g.Configuration.nLanAdapter;
        isGateway = g.Configuration.fBeGateway;
        fFirstTime = FALSE;
    }

    lanainfo = lana_FindLanaByName(NULL);

    // TODO: Show more useful error message.
    if (!lanainfo) {
        MessageBox(hDlg, "Unable to obtain LANA list", "LANA ERROR", MB_ICONERROR);
    } 
    else
    { 
        HWND hwndCombo = GetDlgItem(hDlg, IDC_NICSELECTION);
        int index = 0;
        TCHAR tmp[MAX_PATH];
        while (_tcslen(lanainfo[index].lana_name) > 0)
        {
            _stprintf(tmp, "%s (lana number = %d)", lanainfo[index].lana_name, 
                       lanainfo[index].lana_number);
            SendMessage(hwndCombo, CB_ADDSTRING, 
                         0, (LPARAM) tmp);
            if (nLanAdapter == lanainfo[index].lana_number)
                SendMessage(hwndCombo, CB_SELECTSTRING, (WPARAM)-1, 
                             (LPARAM)tmp);
            index++;
        }
    }

    lana_GetAfsNameString(nLanAdapter, isGateway, name);
    SetDlgItemText (hDlg, IDC_BINDING_MESSAGE, name);

    CheckDlgButton (hDlg, IDC_DEFAULTNIC, (nLanAdapter==-1));

    EnableWindow(GetDlgItem(hDlg,IDC_NICSELECTION),(nLanAdapter!=-1));
}

void Binding_OnOK (HWND hDlg)
{
  if (IsDlgButtonChecked(hDlg,IDC_DEFAULTNIC))
    nLanAdapter = -1;
  else
  {
    TCHAR selected[MAX_PATH];
    memset(selected, 0, sizeof(selected));
    SendDlgItemMessage(hDlg, IDC_NICSELECTION, 
                        WM_GETTEXT, sizeof(selected), 
                        (LPARAM) selected); 

    if (_tcslen(selected) <= 0)
    {
      MessageBox(hDlg, "Please select the NIC to bind to", "Error", MB_ICONERROR);
      return;
    }
    
    nLanAdapter = GetAdapterNumber(selected);

  }
 
   EndDialog(hDlg, IDOK);
}


BOOL Binding_OnApply()
{
   if (fFirstTime)
      return TRUE;

  if (nLanAdapter != g.Configuration.nLanAdapter) {
      if (!Config_SetLanAdapter (nLanAdapter))
         return FALSE;
      g.Configuration.nLanAdapter = nLanAdapter;
   }

  return TRUE;

   
}

void Binding_OnCancel(HWND hDlg)
{
   fFirstTime = TRUE;

   if (lanainfo)
   {
     delete lanainfo;
     lanainfo = NULL;
   }

   EndDialog(hDlg, IDCANCEL);
}


int GetAdapterNumber(TCHAR* n)
{
  int index = 0;
  while (_tcslen(lanainfo[index].lana_name) > 0)
   {
       if (_tcsncmp(lanainfo[index].lana_name, n, _tcslen(lanainfo[index].lana_name)) == 0)
       {
         return lanainfo[index].lana_number;
       }
       index++;
    }

  return -1;

}
