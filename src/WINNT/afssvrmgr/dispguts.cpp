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

#include "svrmgr.h"
#include "display.h"
#include "dispguts.h"
#include "svr_col.h"
#include "svr_window.h"
#include "svc_col.h"
#include "svc_startstop.h"
#include "agg_col.h"
#include "set_col.h"
#include "set_general.h"
#include "creds.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Display_Servers_Internal_Clean (LPDISPLAYREQUEST pdr);
void Display_Servers_Internal_AddServer (LPDISPLAYREQUEST pdr, LPSERVER lpServer);

void Display_Services_Internal_Clean (LPDISPLAYREQUEST pdr);
void Display_Services_Internal_AddServer (LPDISPLAYREQUEST pdr, LPSERVER lpServer);
void Display_Services_Internal_AddService (LPDISPLAYREQUEST pdr, LPSERVICE lpService, HLISTITEM hServer);

void Display_Aggregates_Internal_Clean (LPDISPLAYREQUEST pdr);
void Display_Aggregates_Internal_AddServer (LPDISPLAYREQUEST pdr, LPSERVER lpServer);
void Display_Aggregates_Internal_AddAggregate (LPDISPLAYREQUEST pdr, LPAGGREGATE lpAggregate, HLISTITEM hServer);

void Display_Filesets_Internal_Clean (LPDISPLAYREQUEST pdr);
HLISTITEM Display_Filesets_Internal_AddCell (LPDISPLAYREQUEST pdr, LPCELL lpCell);
void Display_Filesets_Internal_AddServer (LPDISPLAYREQUEST pdr, LPSERVER lpServer, HLISTITEM hCell);
void Display_Filesets_Internal_AddAggregate (LPDISPLAYREQUEST pdr, LPAGGREGATE lpAggregate, HLISTITEM hServer);
void Display_Filesets_Internal_AddFileset (LPDISPLAYREQUEST pdr, LPFILESET lpFileset, HLISTITEM hAggregate);

#define PIF_ALWAYSSHOWICON     0x00000001
#define PIF_ONLYONEICON        0x00000002
#define PIF_TREEVIEW_ONLY      0x00000004
#define PIF_DISALLOW_COLLAPSE  0x00000008
void Display_PickImages (int *piFirstImage, int *piSecondImage, int iTypeImage, int iStatusImage, ICONVIEW iv, DWORD dwPickImageFlags = 0);
HLISTITEM Display_InsertItem (HWND hList, HLISTITEM hParent, LPTSTR pszText, LPARAM lp, int iImage0 = IMAGE_NOIMAGE, int iImage1 = IMAGE_NOIMAGE, ICONVIEW iv = (ICONVIEW)-1, DWORD dwPickImageFlags = 0);


/*
 * CELL _______________________________________________________________________
 *
 */

void Display_Cell_Internal (LPDISPLAYREQUEST pdr)
{
   // Update the "Current Cell:" field on g.hMain
   //
   TCHAR szCell[ cchNAME ];
   if (g.lpiCell == NULL)
      GetString (szCell, IDS_NO_CELL_SELECTED);
   else
      g.lpiCell->GetCellName (szCell);

   SetDlgItemText (g.hMain, IDC_CELL, szCell);

   // Update the "AFS Identity:" field on g.hMain
   //
   TCHAR szUser[ cchNAME ];
   SYSTEMTIME stExpire;
   if (!AfsAppLib_CrackCredentials (g.hCreds, szCell, szUser, &stExpire))
      {
      GetString (szUser, IDS_NO_AFS_ID);
      SetDlgItemText (g.hMain, IDC_AFS_ID, szUser);
      }
   else
      {
      int ids = (AfsAppLib_IsTimeInFuture (&stExpire)) ? IDS_AFS_ID_WILLEXP : IDS_AFS_ID_DIDEXP;
      LPTSTR pszText = FormatString (ids, TEXT("%s%t"), szUser, &stExpire);
      SetDlgItemText (g.hMain, IDC_AFS_ID, pszText);
      FreeString (pszText);
      }
}


/*
 * SERVERS ____________________________________________________________________
 *
 */

void Display_Servers_Internal (LPDISPLAYREQUEST pdr)
{
   pdr->hList = GetDlgItem (g.hMain, IDC_SERVERS);
   pdr->lpvi = (gr.fPreview && !gr.fVert) ? &gr.diHorz.viewSvr : &gr.diVert.viewSvr;

   if (!IsWindow (pdr->hList))
      return;

   // Fix up the image lists for the Servers window
   //
   HIMAGELIST hiSmall;
   HIMAGELIST hiLarge;
   FastList_GetImageLists (pdr->hList, &hiSmall, &hiLarge);
   if (!hiSmall || !hiLarge)
      {
      hiSmall = AfsAppLib_CreateImageList (FALSE);
      hiLarge = AfsAppLib_CreateImageList (TRUE);
      FastList_SetImageLists (pdr->hList, hiSmall, hiLarge);
      }

   // Add an entry in the Servers list for each server in the cell.
   //
//   LPIDENT lpiSelStart = (LPIDENT)FL_GetSelectedData (pdr->hList);
//   BOOL fRefresh = FALSE;

   LPCELL lpCell;
   if (!g.lpiCell || !(lpCell = g.lpiCell->OpenCell()))
      {
      LPTSTR pszCover = FormatString (IDS_ERROR_REFRESH_CELLSERVERS_NOCELL);
      AfsAppLib_CoverWindow (pdr->hList, pszCover);
      FreeString (pszCover);
      }
   else
      {
      BOOL rc = TRUE;

      pdr->lpiToSelect = (LPIDENT)FL_StartChange (pdr->hList, (pdr->lpiNotify) ? FALSE : TRUE);
      pdr->actOnDone = ACT_ENDCHANGE | ACT_SELPREVIEW;

      // Remove any to-be-replaced old servers
      //
      Display_Servers_Internal_Clean (pdr);

      // Add a server entry for each server as appropriate
      //
      HENUM hEnum;
      for (LPSERVER lpServer = lpCell->ServerFindFirst (&hEnum, pdr->lpiNotify, FALSE); lpServer; lpServer = lpCell->ServerFindNext (&hEnum))
         {
         Display_Servers_Internal_AddServer (pdr, lpServer);
         lpServer->Close();
         }

      if (rc)
         pdr->actOnDone |= ACT_UNCOVER;

      lpCell->Close();
      }
}


void Display_Servers_Internal_Clean (LPDISPLAYREQUEST pdr)
{
   if (pdr->lpiNotify) // else, we already emptied the list
      {
      HLISTITEM hItem;
      if ((hItem = FastList_FindItem (pdr->hList, (LPARAM)(pdr->lpiNotify))) != NULL)
         FastList_RemoveItem (pdr->hList, hItem);
      }
}


void Display_Servers_Internal_AddServer (LPDISPLAYREQUEST pdr, LPSERVER lpServer)
{
   ULONG status = 0;
   SERVERSTATUS ss;

   if (lpServer->GetIdentifier() == pdr->lpiNotify)
      status = pdr->status;

   if (!status)
      {
      lpServer->GetStatus (&ss, FALSE, &status);

      LPSERVER_PREF lpsp;
      if ((lpsp = (LPSERVER_PREF)lpServer->GetUserParam()) != NULL)
         memcpy (&lpsp->ssLast, &ss, sizeof(SERVERSTATUS));
      }

   ICONVIEW ivSvr = Display_GetServerIconView();

   if (status != 0)
      {
      LPTSTR pszCol1 = FormatString (TEXT("%1 - %2"), TEXT("%s%e"), Server_GetColumnText (lpServer->GetIdentifier(), svrcolNAME), status);

      Display_InsertItem (pdr->hList,
                          NULL,
                          pszCol1,
                          (LPARAM)lpServer->GetIdentifier(),
                          imageSERVER, imageSERVER_ALERT, ivSvr);

      FreeString (pszCol1);
      }
   else
      {
      int iStatusImage = IMAGE_NOIMAGE;
      if (!lpServer->fIsMonitored())
         iStatusImage = imageSERVER_UNMON;
      else if (Server_GetAlertCount (lpServer))
         iStatusImage = imageSERVER_ALERT;

      Display_InsertItem (pdr->hList,
                          NULL,
                          NULL,
                          (LPARAM)lpServer->GetIdentifier(),
                          imageSERVER, iStatusImage, ivSvr);
      }
}


/*
 * SERVICES ___________________________________________________________________
 *
 */

void Display_Services_Internal (LPDISPLAYREQUEST pdr)
{
   pdr->hList = GetDlgItem (pdr->hChild, IDC_SVC_LIST);
   pdr->lpvi = &gr.viewSvc;

   if (!IsWindow (pdr->hList))
      return;

   // First off, can we totally ignore this request?
   //
   LPIDENT lpiWindow = Server_GetServerForChild (pdr->hChild);
   if (lpiWindow && pdr->lpiNotify && (lpiWindow != pdr->lpiNotify->GetServer()))
      return;

   // Fix up the image lists for the Services window
   //
   HIMAGELIST hiSmall;
   FastList_GetImageLists (pdr->hList, &hiSmall, NULL);
   if (!hiSmall)
      {
      hiSmall = AfsAppLib_CreateImageList (FALSE);
      FastList_SetImageLists (pdr->hList, hiSmall, NULL);
      }

   // Add either all services, or one server's services, or one service.
   //
   LPCELL lpCell;
   if (g.lpiCell && (lpCell = g.lpiCell->OpenCell()))
      {
      pdr->lpiToSelect = (LPIDENT)FL_StartChange (pdr->hList, (pdr->lpiNotify) ? FALSE : TRUE);
      pdr->actOnDone = ACT_ENDCHANGE;

      // Remove any to-be-replaced old services
      //
      Display_Services_Internal_Clean (pdr);

      // Update all appropriate service entries
      //
      LPIDENT lpiRefresh = (pdr->lpiNotify) ? pdr->lpiNotify : lpiWindow;

      HENUM hEnum;
      for (LPSERVER lpServer = lpCell->ServerFindFirst (&hEnum, lpiRefresh, FALSE); lpServer; lpServer = lpCell->ServerFindNext (&hEnum))
         {
         if (lpServer->fIsMonitored())
            Display_Services_Internal_AddServer (pdr, lpServer);
         lpServer->Close();
         }

      if (pdr->lpiToSelect)
         FL_SetSelectedByData (pdr->hList, (LPARAM)pdr->lpiToSelect);
      lpCell->Close();
      }
}


void Display_Services_Internal_Clean (LPDISPLAYREQUEST pdr)
{
   if (pdr->lpiNotify) // else, we already emptied the list
      {
      HLISTITEM hItem;
      if ((hItem = FastList_FindItem (pdr->hList, (LPARAM)(pdr->lpiNotify))) != NULL)
         FastList_RemoveItem (pdr->hList, hItem);
      }
}


void Display_Services_Internal_AddServer (LPDISPLAYREQUEST pdr, LPSERVER lpServer)
{
   ULONG status = 0;
   SERVERSTATUS ss;

   if (lpServer->GetIdentifier() == pdr->lpiNotify)
      status = pdr->status;
   if (!status)
      lpServer->GetStatus (&ss, FALSE, &status);

   LPTSTR pszServerText = NULL;
   int iStatusImage = imageSERVER;
   BOOL fContinue = TRUE;

   if (status != 0)
      {
      pszServerText = FormatString (TEXT("%1 - %2"), TEXT("%s%e"), Server_GetColumnText (lpServer->GetIdentifier(), svrcolNAME), status);
      iStatusImage = imageSERVER_ALERT;
      fContinue = FALSE;
      }
   else if (Server_GetAlertCount (lpServer))
      {
      pszServerText = FormatString (TEXT("%1 - %2"), TEXT("%s%s"), Server_GetColumnText (lpServer->GetIdentifier(), svrcolNAME), Server_GetColumnText (lpServer->GetIdentifier(), svrcolSTATUS));
      iStatusImage = imageSERVER_ALERT;
      }

   HLISTITEM hServer;
   if ((hServer = FastList_FindItem (pdr->hList, (LPARAM)(lpServer->GetIdentifier()))) == NULL)
      {
      hServer = Display_InsertItem (pdr->hList, NULL, pszServerText,
                                    (LPARAM)lpServer->GetIdentifier(),
                                    imageSERVER, iStatusImage, gr.ivSet, PIF_ALWAYSSHOWICON | PIF_TREEVIEW_ONLY);
      }
   else
      {
      int iImage1, iImage2;
      Display_PickImages (&iImage1, &iImage2, imageSERVER, iStatusImage, gr.ivSet, PIF_ALWAYSSHOWICON | PIF_TREEVIEW_ONLY);
      FastList_SetItemFirstImage (pdr->hList, hServer, iImage1);
      FastList_SetItemSecondImage (pdr->hList, hServer, iImage2);
      FastList_SetItemText (pdr->hList, hServer, 0, pszServerText);
      }

   if (pszServerText)
      FreeString (pszServerText);

   if (fContinue)
      {
      // Update icons for all appropriate services
      //
      LPIDENT lpiSearch = (pdr->lpiNotify && pdr->lpiNotify->fIsService()) ? (pdr->lpiNotify) : NULL;

      HENUM hEnum;
      for (LPSERVICE lpService = lpServer->ServiceFindFirst (&hEnum, lpiSearch, FALSE); lpService; lpService = lpServer->ServiceFindNext (&hEnum))
         {
         Display_Services_Internal_AddService (pdr, lpService, hServer);
         lpService->Close();
         }
      }
}


void Display_Services_Internal_AddService (LPDISPLAYREQUEST pdr, LPSERVICE lpService, HLISTITEM hServer)
{
   ULONG status = 0;
   SERVICESTATUS ss;

   if (lpService->GetIdentifier() == pdr->lpiNotify)
      status = pdr->status;
   if (!status)
      lpService->GetStatus (&ss, FALSE, &status);

   BOOL fShowServerName = !Server_GetServer (pdr->hChild);

   if (status != 0)
      {
      LPTSTR pszCol1 = FormatString (TEXT("%1 - %2"), TEXT("%s%e"), Services_GetColumnText (lpService->GetIdentifier(), svccolNAME, fShowServerName), status);

      Display_InsertItem (pdr->hList,
                          hServer,
                          pszCol1,
                          (LPARAM)lpService->GetIdentifier(),
                          imageSERVICE, imageSERVICE_ALERT, gr.ivSvc);

      FreeString (pszCol1);
      }
   else
      {
      TCHAR szName[ cchRESOURCE ];
      lpService->GetName (szName);

      BOOL fIsBOS = !lstrcmpi (szName, TEXT("BOS"));

      LPSERVICE_PREF lpsp;
      if ((lpsp = (LPSERVICE_PREF)lpService->GetUserParam()) != NULL)
         memcpy (&lpsp->ssLast, &ss, sizeof(SERVICESTATUS));

      int iTypeImage = imageSERVICE;
      if (fIsBOS)
         iTypeImage = imageBOSSERVICE;

      int iStatusImage = IMAGE_NOIMAGE;
      if (Services_GetAlertCount (lpService))
         iStatusImage = imageSERVICE_ALERT;
      else if (!Services_fRunning (lpService))
         iStatusImage = imageSERVICE_STOPPED;
      else if (fIsBOS)
         iStatusImage = imageBOSSERVICE;

      Display_InsertItem (pdr->hList,
                          hServer,
                          NULL,
                          (LPARAM)lpService->GetIdentifier(),
                          iTypeImage, iStatusImage, gr.ivSvc);
      }
}


/*
 * AGGREGATES _________________________________________________________________
 *
 */

void Display_Aggregates_Internal (LPDISPLAYREQUEST pdr)
{
   if (pdr->lpvi == NULL)
      pdr->lpvi = &gr.viewAgg;

   if (!IsWindow (pdr->hList))
      return;

   TCHAR szClassName[ cchRESOURCE ];
   if (GetClassName (pdr->hList, szClassName, cchRESOURCE))
      {
      if (!lstrcmp (szClassName, WC_FASTLIST))
         pdr->fList = TRUE;
      }

   // First off, can we totally ignore this request?
   //
   if (pdr->lpiServer && pdr->lpiNotify && (pdr->lpiServer != pdr->lpiNotify->GetServer()))
      return;

   // Fix up the image lists for the Aggregates window
   //
   if (pdr->fList)
      {
      HIMAGELIST hiSmall;
      FastList_GetImageLists (pdr->hList, &hiSmall, NULL);
      if (!hiSmall)
         {
         hiSmall = AfsAppLib_CreateImageList (FALSE);
         FastList_SetImageLists (pdr->hList, hiSmall, NULL);
         }
      }

   // Add either all aggregates, or one server's aggregates, or one aggregate.
   //
   LPCELL lpCell;
   if (g.lpiCell && (lpCell = g.lpiCell->OpenCell()))
      {
      LPIDENT lpiSelectedNow;
      if (pdr->fList)
         lpiSelectedNow = (LPIDENT)FL_StartChange (pdr->hList, (pdr->lpiNotify) ? FALSE : TRUE);
      else
         lpiSelectedNow = (LPIDENT)CB_StartChange (pdr->hList, (pdr->lpiNotify) ? FALSE : TRUE);

      if (!pdr->lpiToSelect)
         pdr->lpiToSelect = lpiSelectedNow;

      pdr->actOnDone = ACT_ENDCHANGE;

      // Remove any to-be-replaced old services
      //
      Display_Aggregates_Internal_Clean (pdr);

      // Update all appropriate aggregate entries
      //
      LPIDENT lpiRefresh = (pdr->lpiNotify) ? pdr->lpiNotify : pdr->lpiServer;

      HENUM hEnum;
      for (LPSERVER lpServer = lpCell->ServerFindFirst (&hEnum, lpiRefresh, FALSE); lpServer; lpServer = lpCell->ServerFindNext (&hEnum))
         {
         if (lpServer->fIsMonitored())
            Display_Aggregates_Internal_AddServer (pdr, lpServer);
         lpServer->Close();
         }

      if (pdr->lpiToSelect)
         {
         if (pdr->fList)
            FL_SetSelectedByData (pdr->hList, (LPARAM)pdr->lpiToSelect);
         else
            CB_SetSelectedByData (pdr->hList, (LPARAM)pdr->lpiToSelect);
         }
      if (!pdr->fList) // combobox?  then always pick *something*...
         {
         if (!CB_GetSelectedData (pdr->hList))
            {
            pdr->lpiToSelect = (LPIDENT)CB_GetData (pdr->hList, 0);
            }
         }

      lpCell->Close();
      }
}


void Display_Aggregates_Internal_Clean (LPDISPLAYREQUEST pdr)
{
   if (pdr->lpiNotify) // else, we already emptied the list
      {
      if (pdr->fList)
         {
         HLISTITEM hItem;
         if ((hItem = FastList_FindItem (pdr->hList, (LPARAM)(pdr->lpiNotify))) != NULL)
            FastList_RemoveItem (pdr->hList, hItem);
         }
      else
         {
         size_t iMax = SendMessage (pdr->hList, CB_GETCOUNT, 0, 0);

         for (UINT iItem = 0; iItem < iMax; )
            {
            BOOL fDelete = FALSE;

            LPIDENT lpiItem;
            if ((lpiItem = (LPIDENT)CB_GetData (pdr->hList, iItem)) != NULL)
               {
               // delete the entry if it's the TBD server or aggregate
               //
               if (lpiItem == pdr->lpiNotify)
                  fDelete = TRUE;

               // delete the entry if it's the server of a TBD aggregate
               //
               if (lpiItem == pdr->lpiNotify->GetServer())
                  fDelete = TRUE;

               // delete the entry if it's an aggregate on the TBD server
               //
               if (lpiItem->GetServer() == pdr->lpiNotify)
                  fDelete = TRUE;
               }

            if (!fDelete)
               ++iItem;
            else
               {
               SendMessage (pdr->hList, CB_DELETESTRING, iItem, 0);
               --iMax;
               }
            }
         }
      }
}


void Display_Aggregates_Internal_AddServer (LPDISPLAYREQUEST pdr, LPSERVER lpServer)
{
   ULONG status = 0;
   SERVERSTATUS ss;

   if (lpServer->GetIdentifier() == pdr->lpiNotify)
      status = pdr->status;
   if (!status)
      lpServer->GetStatus (&ss, FALSE, &status);

   HLISTITEM hServer = NULL;
   if (pdr->fList)
      {
      LPTSTR pszServerText = NULL;
      int iStatusImage = imageSERVER;

      if (status != 0)
         {
         pszServerText = FormatString (TEXT("%1 - %2"), TEXT("%s%e"), Server_GetColumnText (lpServer->GetIdentifier(), svrcolNAME), status);
         iStatusImage = imageSERVER_ALERT;
         }
      else if (Server_GetAlertCount (lpServer))
         {
         pszServerText = FormatString (TEXT("%1 - %2"), TEXT("%s%s"), Server_GetColumnText (lpServer->GetIdentifier(), svrcolNAME), Server_GetColumnText (lpServer->GetIdentifier(), svrcolSTATUS));
         iStatusImage = imageSERVER_ALERT;
         }

      if ((hServer = FastList_FindItem (pdr->hList, (LPARAM)(lpServer->GetIdentifier()))) == NULL)
         {
         hServer = Display_InsertItem (pdr->hList, NULL, pszServerText,
                                       (LPARAM)lpServer->GetIdentifier(),
                                       imageSERVER, iStatusImage, gr.ivSet, PIF_ALWAYSSHOWICON | PIF_TREEVIEW_ONLY);
         }
      else
         {
         int iImage1, iImage2;
         Display_PickImages (&iImage1, &iImage2, imageSERVER, iStatusImage, gr.ivSet, PIF_ALWAYSSHOWICON | PIF_TREEVIEW_ONLY);
         FastList_SetItemFirstImage (pdr->hList, hServer, iImage1);
         FastList_SetItemSecondImage (pdr->hList, hServer, iImage2);
         FastList_SetItemText (pdr->hList, hServer, 0, pszServerText);
         }

      if (pszServerText)
         FreeString (pszServerText);
      }

   if ((hServer || !pdr->fList) && !status)
      {
      // Update icons for all appropriate aggregates
      //
      LPIDENT lpiSearch = (pdr->lpiNotify && pdr->lpiNotify->fIsAggregate()) ? (pdr->lpiNotify) : NULL;

      HENUM hEnum;
      for (LPAGGREGATE lpAggregate = lpServer->AggregateFindFirst (&hEnum, lpiSearch, FALSE); lpAggregate; lpAggregate = lpServer->AggregateFindNext (&hEnum))
         {
         Display_Aggregates_Internal_AddAggregate (pdr, lpAggregate, hServer);
         lpAggregate->Close();
         }
      }
}


void Display_Aggregates_Internal_AddAggregate (LPDISPLAYREQUEST pdr, LPAGGREGATE lpAggregate, HLISTITEM hServer)
{
   ULONG status = 0;
   AGGREGATESTATUS as;

   if (lpAggregate->GetIdentifier() == pdr->lpiNotify)
      status = pdr->status;
   if (!status)
      lpAggregate->GetStatus (&as, FALSE, &status);

   BOOL fShowServerName = !Server_GetServer (pdr->hChild);

   if (status != 0)
      {
      LPTSTR pszCol1 = FormatString (TEXT("%1 - %2"), TEXT("%s%e"), Aggregates_GetColumnText (lpAggregate->GetIdentifier(), aggcolNAME, fShowServerName), status);

      if (pdr->fList)
         {
         Display_InsertItem (pdr->hList, hServer, pszCol1,
                             (LPARAM)lpAggregate->GetIdentifier(),
                             imageAGGREGATE, imageAGGREGATE_ALERT, gr.ivAgg);
         }
      else
         {
         CB_AddItem (pdr->hList, pszCol1, (LPARAM)lpAggregate->GetIdentifier());
         }

      FreeString (pszCol1);
      }
   else
      {
      LPAGGREGATE_PREF lpap;
      if ((lpap = (LPAGGREGATE_PREF)lpAggregate->GetUserParam()) != NULL)
         {
         memcpy (&lpap->asLast, &as, sizeof(AGGREGATESTATUS));
         lpAggregate->GetDevice (lpap->szDevice);
         }

      if (pdr->fList)
         {
         int iStatusImage = IMAGE_NOIMAGE;
         if (Aggregates_GetAlertCount (lpAggregate))
            iStatusImage = imageAGGREGATE_ALERT;

         Display_InsertItem (pdr->hList, hServer, NULL,
                             (LPARAM)lpAggregate->GetIdentifier(),
                             imageAGGREGATE, iStatusImage, gr.ivAgg);
         }
      else
         {
         TCHAR szName[ cchNAME ];
         lpAggregate->GetIdentifier()->GetAggregateName (szName);
         CB_AddItem (pdr->hList, szName, (LPARAM)lpAggregate->GetIdentifier());
         }
      }
}


/*
 * FILESETS ___________________________________________________________________
 *
 */

void Display_Filesets_Internal (LPDISPLAYREQUEST pdr)
{
   if (pdr->hList == NULL)
      pdr->hList = GetDlgItem (pdr->hChild, IDC_SET_LIST);

   if (!IsWindow (pdr->hList))
      return;

   TCHAR szClassName[ cchRESOURCE ];
   if (GetClassName (pdr->hList, szClassName, cchRESOURCE))
      {
      if (!lstrcmp (szClassName, WC_FASTLIST))
         pdr->fList = TRUE;
      }

   pdr->lpvi = (pdr->fList) ? &gr.viewSet : NULL;

   // First off, can we totally ignore this request?
   //
   if (pdr->lpiServer && pdr->lpiNotify && (pdr->lpiServer != pdr->lpiNotify->GetServer()))
      return;

   // Fix up the image lists for the Filesets window
   //
   if (pdr->fList)
      {
      HIMAGELIST hiSmall;
      FastList_GetImageLists (pdr->hList, &hiSmall, NULL);
      if (!hiSmall)
         {
         hiSmall = AfsAppLib_CreateImageList (FALSE);
         FastList_SetImageLists (pdr->hList, hiSmall, NULL);
         }
      }

   // Add either all filesets, or one server's filesets, or one fileset.
   //
   LPCELL lpCell;
   if (g.lpiCell && (lpCell = g.lpiCell->OpenCell()))
      {
      pdr->actOnDone = ACT_ENDCHANGE;

      // Remove any to-be-replaced old services
      //
      LPIDENT lpiSelectedNow;
      if (pdr->fList)
         lpiSelectedNow = (LPIDENT)FL_StartChange (pdr->hList, (pdr->lpiNotify) ? FALSE : TRUE);
      else
         lpiSelectedNow = (LPIDENT)CB_StartChange (pdr->hList, (pdr->lpiNotify) ? FALSE : TRUE);
      Display_Filesets_Internal_Clean (pdr);

      if (! pdr->lpiToSelect)
         pdr->lpiToSelect = lpiSelectedNow;

      HLISTITEM hCell;
      hCell = Display_Filesets_Internal_AddCell (pdr, lpCell);

      // Update all appropriate fileset entries
      //
      LPIDENT lpiRefresh = (pdr->lpiNotify) ? pdr->lpiNotify : pdr->lpiServer;

      HENUM hEnum;
      for (LPSERVER lpServer = lpCell->ServerFindFirst (&hEnum, lpiRefresh, FALSE); lpServer; lpServer = lpCell->ServerFindNext (&hEnum))
         {
         if (lpServer->fIsMonitored())
            Display_Filesets_Internal_AddServer (pdr, lpServer, hCell);
         lpServer->Close();
         }

      if (pdr->lpiToSelect)
         {
         if (pdr->fList)
            FL_SetSelectedByData (pdr->hList, (LPARAM)pdr->lpiToSelect);
         else
            CB_SetSelectedByData (pdr->hList, (LPARAM)pdr->lpiToSelect);
         }
      if (!pdr->fList) // combobox?  then always pick *something*...
         {
         if (!CB_GetSelectedData (pdr->hList))
            {
            pdr->lpiToSelect = (LPIDENT)CB_GetData (pdr->hList, 0);
            }
         }

      lpCell->Close();
      }
}


void Display_Filesets_Internal_Clean (LPDISPLAYREQUEST pdr)
{
   if (pdr->lpiNotify)
      {
      if (pdr->fList)
         {
         HLISTITEM hItem;
         if ((hItem = FastList_FindItem (pdr->hList, (LPARAM)pdr->lpiNotify)) != NULL)
            {
            FastList_RemoveItem (pdr->hList, hItem);
            }
         }
      else // Must be a combobox
         {
         size_t iMax = SendMessage (pdr->hList, CB_GETCOUNT, 0, 0);

         for (UINT iItem = 0; iItem < iMax; )
            {
            BOOL fDelete = FALSE;

            LPIDENT lpiItem;
            if ((lpiItem = (LPIDENT)CB_GetData (pdr->hList, iItem)) != NULL)
               {
               if (lpiItem->GetServer() == pdr->lpiNotify)
                  fDelete = TRUE;
               if (lpiItem->GetAggregate() == pdr->lpiNotify)
                  fDelete = TRUE;
               if (lpiItem == pdr->lpiNotify)
                  fDelete = TRUE;
               }

            if (!fDelete)
               ++iItem;
            else
               {
               SendMessage (pdr->hList, CB_DELETESTRING, iItem, 0);
               --iMax;
               }
            }
         }
      }
}


HLISTITEM Display_Filesets_Internal_AddCell (LPDISPLAYREQUEST pdr, LPCELL lpCell)
{
   if (!pdr->fList)
      return NULL;

   HLISTITEM hCell;
   if ((hCell = FastList_FindItem (pdr->hList, (LPARAM)lpCell->GetIdentifier())) == NULL)
      {
      TCHAR szName[ cchNAME ];
      lpCell->GetName (szName);

      hCell = Display_InsertItem (pdr->hList, NULL, szName,
                                  (LPARAM)lpCell->GetIdentifier(),
                                  imageCELL, IMAGE_NOIMAGE, gr.ivSet,
                                  PIF_ALWAYSSHOWICON | PIF_ONLYONEICON | PIF_TREEVIEW_ONLY | PIF_DISALLOW_COLLAPSE);
      }

   return hCell;
}


void Display_Filesets_Internal_AddServer (LPDISPLAYREQUEST pdr, LPSERVER lpServer, HLISTITEM hCell)
{
   ULONG status = 0;
   SERVERSTATUS ss;

   if (lpServer->GetIdentifier() == pdr->lpiNotify)
      status = pdr->status;
   if (!status)
      lpServer->GetStatus (&ss, FALSE, &status);

   HLISTITEM hServer = NULL;
   if (pdr->fList)
      {
      LPTSTR pszServerText = NULL;
      int iStatusImage = imageSERVER;

      if (status != 0)
         {
         pszServerText = FormatString (TEXT("%1 - %2"), TEXT("%s%e"), Server_GetColumnText (lpServer->GetIdentifier(), svrcolNAME), status);
         iStatusImage = imageSERVER_ALERT;
         }
      else if (Server_GetAlertCount (lpServer))
         {
         pszServerText = FormatString (TEXT("%1 - %2"), TEXT("%s%s"), Server_GetColumnText (lpServer->GetIdentifier(), svrcolNAME), Server_GetColumnText (lpServer->GetIdentifier(), svrcolSTATUS));
         iStatusImage = imageSERVER_ALERT;
         }

      if ((hServer = FastList_FindItem (pdr->hList, (LPARAM)(lpServer->GetIdentifier()))) == NULL)
         {
         hServer = Display_InsertItem (pdr->hList, hCell, pszServerText,
                                       (LPARAM)lpServer->GetIdentifier(),
                                       imageSERVER, iStatusImage, gr.ivSet, PIF_ALWAYSSHOWICON | PIF_TREEVIEW_ONLY);
         }
      else
         {
         int iImage1, iImage2;
         Display_PickImages (&iImage1, &iImage2, imageSERVER, iStatusImage, gr.ivSet, PIF_ALWAYSSHOWICON | PIF_TREEVIEW_ONLY);
         FastList_SetItemFirstImage (pdr->hList, hServer, iImage1);
         FastList_SetItemSecondImage (pdr->hList, hServer, iImage2);
         FastList_SetItemText (pdr->hList, hServer, 0, pszServerText);
         }

      if (pszServerText)
         FreeString (pszServerText);
      }

   if (status == 0)
      {
      // Update filesets on all appropriate aggregates
      //
      LPIDENT lpiSearch = (pdr->lpiNotify && !pdr->lpiNotify->fIsServer()) ? (pdr->lpiNotify->GetAggregate()) : pdr->lpiAggregate;

      HENUM hEnum;
      for (LPAGGREGATE lpAggregate = lpServer->AggregateFindFirst (&hEnum, lpiSearch, FALSE); lpAggregate; lpAggregate = lpServer->AggregateFindNext (&hEnum))
         {
         Display_Filesets_Internal_AddAggregate (pdr, lpAggregate, hServer);
         lpAggregate->Close();
         }

      if (pdr->fList && hServer)
         {
         BOOL fExpand = TRUE;

         LPSERVER_PREF lpsp;
         if ((lpsp = (LPSERVER_PREF)lpServer->GetUserParam()) != NULL)
            fExpand = lpsp->fExpandTree;

         FastList_SetExpanded (pdr->hList, hServer, fExpand);
         }
      }
}


void Display_Filesets_Internal_AddAggregate (LPDISPLAYREQUEST pdr, LPAGGREGATE lpAggregate, HLISTITEM hServer)
{
   ULONG status = 0;
   AGGREGATESTATUS as;

   if (lpAggregate->GetIdentifier() == pdr->lpiNotify)
      status = pdr->status;
   if (!status)
      lpAggregate->GetStatus (&as, FALSE, &status);

   HLISTITEM hAggregate = NULL;
   if (pdr->fList)
      {
      LPTSTR pszAggregateText = NULL;
      int iStatusImage = imageAGGREGATE;

      if (status != 0)
         {
         pszAggregateText = FormatString (TEXT("%1 - %2"), TEXT("%s%e"), Aggregates_GetColumnText (lpAggregate->GetIdentifier(), aggcolNAME, FALSE), status);
         iStatusImage = imageAGGREGATE_ALERT;
         }
      else if (Aggregates_GetAlertCount (lpAggregate))
         {
         pszAggregateText = FormatString (TEXT("%1 - %2"), TEXT("%s%s"), Aggregates_GetColumnText (lpAggregate->GetIdentifier(), aggcolNAME), Aggregates_GetColumnText (lpAggregate->GetIdentifier(), aggcolSTATUS));
         iStatusImage = imageAGGREGATE_ALERT;
         }

      if ((hAggregate = FastList_FindItem (pdr->hList, (LPARAM)lpAggregate->GetIdentifier())) == NULL)
         {
         hAggregate = Display_InsertItem (pdr->hList, hServer, pszAggregateText,
                                          (LPARAM)lpAggregate->GetIdentifier(),
                                          imageAGGREGATE, iStatusImage, gr.ivSet, PIF_ALWAYSSHOWICON | PIF_TREEVIEW_ONLY);
         }
      else
         {
         int iImage1, iImage2;
         Display_PickImages (&iImage1, &iImage2, imageAGGREGATE, iStatusImage, gr.ivSet, PIF_ALWAYSSHOWICON | PIF_TREEVIEW_ONLY);
         FastList_SetItemFirstImage (pdr->hList, hAggregate, iImage1);
         FastList_SetItemSecondImage (pdr->hList, hAggregate, iImage2);
         FastList_SetItemText (pdr->hList, hAggregate, 0, pszAggregateText);
         }

      if (pszAggregateText)
         FreeString (pszAggregateText);
      }

   if (status == 0)
      {
      // Update all appropriate filesets from this aggregate
      //
      LPIDENT lpiSearch = (pdr->lpiNotify && pdr->lpiNotify->fIsFileset()) ? (pdr->lpiNotify) : NULL;

      HENUM hEnum;
      for (LPFILESET lpFileset = lpAggregate->FilesetFindFirst (&hEnum, lpiSearch, FALSE); lpFileset; lpFileset = lpAggregate->FilesetFindNext (&hEnum))
         {
         Display_Filesets_Internal_AddFileset (pdr, lpFileset, hAggregate);
         lpFileset->Close();
         }

      if (pdr->fList && hAggregate)
         {
         BOOL fExpand = (hServer == NULL) ? TRUE : FALSE;

         LPAGGREGATE_PREF lpap;
         if ((lpap = (LPAGGREGATE_PREF)lpAggregate->GetUserParam()) != NULL)
            fExpand = lpap->fExpandTree;

         FastList_SetExpanded (pdr->hList, hAggregate, fExpand);
         }
      }
}


void Display_Filesets_Internal_AddFileset (LPDISPLAYREQUEST pdr, LPFILESET lpFileset, HLISTITEM hAggregate)
{
   ULONG status = 0;
   FILESETSTATUS fs;

   if (lpFileset->GetIdentifier() == pdr->lpiNotify)
      status = pdr->status;
   if (!status)
      lpFileset->GetStatus (&fs, FALSE, &status);

   if (status != 0)
      {
      if (pdr->fList)
         {
         LPTSTR pszFilesetText = FormatString (TEXT("%1 - %2"), TEXT("%s%e"), Filesets_GetColumnText (lpFileset->GetIdentifier(), setcolNAME), status);

         Display_InsertItem (pdr->hList, hAggregate, pszFilesetText,
                             (LPARAM)lpFileset->GetIdentifier(),
                             imageFILESET, imageFILESET_ALERT, gr.ivSet);

         FreeString (pszFilesetText);
         }
      }
   else
      {
      if (!pdr->fList)
         {
         TCHAR szName[ cchNAME ];
         lpFileset->GetName (szName);
         CB_AddItem (pdr->hList, szName, (LPARAM)lpFileset->GetIdentifier());
         }
      else
         {
         LPFILESET_PREF lpfp;
         if ((lpfp = (LPFILESET_PREF)lpFileset->GetUserParam()) != NULL)
            {
            memcpy (&lpfp->fsLast, &fs, sizeof(FILESETSTATUS));
            lpfp->lpiRW = lpFileset->GetReadWriteIdentifier();
            }

         int iStatusImage = IMAGE_NOIMAGE;
         if (Filesets_GetAlertCount (lpFileset))
            iStatusImage = imageFILESET_ALERT;
         else if (Filesets_fIsLocked (&fs))
            iStatusImage = imageFILESET_LOCKED;

         Display_InsertItem (pdr->hList, hAggregate, NULL,
                             (LPARAM)lpFileset->GetIdentifier(),
                             imageFILESET, iStatusImage, gr.ivSet);
         }
      }
}


/*
 * REPLICAS ___________________________________________________________________
 *
 * (lpiNotify is the RW for which to list all replicas)
 *
 */

void Display_Replicas_Internal (LPDISPLAYREQUEST pdr)
{
   if (!IsWindow (pdr->hList))
      return;

   pdr->lpvi = &gr.viewRep;
   pdr->actOnDone = ACT_ENDCHANGE;

   LPIDENT lpiRW = pdr->lpiNotify;
   VOLUMEID vidReadWrite;
   lpiRW->GetFilesetID (&vidReadWrite);

   FL_StartChange (pdr->hList, TRUE);

   LPCELL lpCell;
   if ((lpCell = lpiRW->OpenCell()) != NULL)
      {
      HENUM heServer;
      for (LPSERVER lpServer = lpCell->ServerFindFirst (&heServer); lpServer; lpServer = lpCell->ServerFindNext (&heServer))
         {
         HENUM heAggregate;
         for (LPAGGREGATE lpAggregate = lpServer->AggregateFindFirst (&heAggregate); lpAggregate; lpAggregate = lpServer->AggregateFindNext (&heAggregate))
            {
            HENUM heFileset;
            for (LPFILESET lpFileset = lpAggregate->FilesetFindFirst (&heFileset); lpFileset; lpFileset = lpAggregate->FilesetFindNext (&heFileset))
               {
               FILESETSTATUS fs;
               if (lpFileset->GetStatus (&fs))
                  {
                  if ( (fs.Type == ftREPLICA) &&
                       (!memcmp (&vidReadWrite, &fs.idReadWrite, sizeof(VOLUMEID))) )
                     {
                     LPFILESET_PREF lpfp;
                     if ((lpfp = (LPFILESET_PREF)lpFileset->GetUserParam()) != NULL)
                        {
                        memcpy (&lpfp->fsLast, &fs, sizeof(FILESETSTATUS));
                        lpfp->lpiRW = lpiRW;
                        }

                     Display_InsertItem (pdr->hList, NULL, NULL,
                                         (LPARAM)lpFileset->GetIdentifier());
                     }
                  }
               lpFileset->Close();
               }
            lpAggregate->Close();
            }
         lpServer->Close();
         }
      lpCell->Close();
      }
}


/*
 * SERVER WINDOWS _____________________________________________________________
 *
 */

void Display_ServerWindow_Internal (LPDISPLAYREQUEST pdr)
{
   Server_SelectServer (pdr->hChild, pdr->lpiServer, TRUE); // redraw svr window
}


/*
 * ICON SELECTION _____________________________________________________________
 *
 */

void Display_PickImages (int *piImage0, int *piImage1, int iTypeImage, int iStatusImage, ICONVIEW iv, DWORD dwFlags)
{
   switch (iv)
      {
      case ivTWOICONS:
         *piImage0 = iTypeImage;
         *piImage1 = (iStatusImage == IMAGE_NOIMAGE) ? IMAGE_BLANKIMAGE : iStatusImage;
         if (*piImage1 == *piImage0)
            *piImage1 = IMAGE_BLANKIMAGE;
         if (dwFlags & PIF_ONLYONEICON)
            *piImage1 = IMAGE_NOIMAGE;
         break;

      case ivONEICON:
         *piImage0 = (iStatusImage == IMAGE_NOIMAGE) ? IMAGE_BLANKIMAGE : iStatusImage;
         *piImage1 = IMAGE_NOIMAGE;
         dwFlags |= PIF_ALWAYSSHOWICON;
         break;

      case ivSTATUS:
         *piImage0 = (iStatusImage == IMAGE_NOIMAGE) ? IMAGE_BLANKIMAGE : iStatusImage;
         *piImage1 = IMAGE_NOIMAGE;
         break;

      default:
         *piImage0 = iTypeImage;
         *piImage1 = iStatusImage;
         return;
      }

   if ((dwFlags & PIF_ALWAYSSHOWICON) && ((*piImage0 == IMAGE_NOIMAGE) || (*piImage0 == IMAGE_BLANKIMAGE)))
      {
      *piImage0 = iTypeImage;
      }
}


HLISTITEM Display_InsertItem (HWND hList, HLISTITEM hParent, LPTSTR pszText, LPARAM lp, int iImage0, int iImage1, ICONVIEW iv, DWORD dwPickImageFlags)
{
   FASTLISTADDITEM flai;
   memset (&flai, 0x00, sizeof(flai));
   flai.hParent = hParent;
   flai.pszText = pszText;
   flai.lParam = lp;
   Display_PickImages (&flai.iFirstImage, &flai.iSecondImage, iImage0, iImage1, iv, dwPickImageFlags);

   if (dwPickImageFlags & PIF_TREEVIEW_ONLY)
      flai.dwFlags |= FLIF_TREEVIEW_ONLY;

   if (dwPickImageFlags & PIF_DISALLOW_COLLAPSE)
      flai.dwFlags |= FLIF_DISALLOW_COLLAPSE;

   return FastList_AddItem (hList, &flai);
}

