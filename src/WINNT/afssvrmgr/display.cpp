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
#include "svr_window.h"
#include "svr_col.h"
#include "svc_col.h"
#include "agg_col.h"
#include "set_col.h"
#include "propcache.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define cREALLOC_DISPLAYQUEUE  128

#define cUPDATE_THREADS_MAX      4


/*
 * VARIABLES __________________________________________________________________
 *
 */

static size_t cDisplayQueueActive = 0;
static size_t cDisplayQueue = 0;
static size_t cUpdateThreadsActive = 0;
static DISPLAYREQUEST *aDisplayQueue = NULL;
static CRITICAL_SECTION *pcsDisplayQueue = NULL;

static DISPLAYREQUEST drActiveSERVERS;
static DISPLAYREQUEST drActiveSERVICES;
static DISPLAYREQUEST drActiveAGGREGATES;
static DISPLAYREQUEST drActiveFILESETS;
static DISPLAYREQUEST drActiveSERVERWINDOW;

static struct {
   HWND hWnd;
   WORD actOnDone;
   LPIDENT lpiSelectOnDone;
} *aWindowActOnDone = NULL;
static size_t cWindowActOnDone = 0;
static CRITICAL_SECTION *pcsWindowActOnDone = NULL;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

DWORD WINAPI DisplayQueue_ThreadProc (PVOID lp);

BOOL DisplayQueueFilter (size_t idqVictim, size_t idqKiller);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK GetItemText (HWND hList, LPFLN_GETITEMTEXT_PARAMS pfln, UINT_PTR dwCookie)
{ 
   LPVIEWINFO lpvi = (LPVIEWINFO)dwCookie;
   LPIDENT lpi = (LPIDENT)(pfln->item.lParam);
   LPTSTR psz = NULL;

   if (pfln->item.icol < (int)lpvi->nColsShown)
      {
      size_t iCol = lpvi->aColumns[ pfln->item.icol ];

      BOOL fShowServerName = !Server_GetServerForChild (GetParent(hList));

      if (lpi != NULL)
         {
         DISPLAYTARGET dt = dtSERVERS;
         if (lpvi == &gr.viewSet)
            dt = dtFILESETS;
         else if (lpvi == &gr.viewSvc)
            dt = dtSERVICES;
         else if ((lpvi == &gr.viewAgg) || (lpvi == &gr.viewAggMove) || (lpvi == &gr.viewAggCreate) || (lpvi == &gr.viewAggRestore))
            dt = dtAGGREGATES;
         else if (lpvi == &gr.viewRep)
            dt = dtREPLICAS;

         switch (dt)
            {
            case dtSERVERS:
               if (lpi->fIsServer())
                  psz = Server_GetColumnText (lpi, (SERVERCOLUMN)iCol);
               break;

            case dtSERVICES:
               if (lpi->fIsService())
                  psz = Services_GetColumnText (lpi, (SERVICECOLUMN)iCol, fShowServerName);
               break;

            case dtAGGREGATES:
               if (lpi->fIsAggregate())
                  psz = Aggregates_GetColumnText (lpi, (AGGREGATECOLUMN)iCol, fShowServerName);
               break;

            case dtFILESETS:
               if (lpi->fIsFileset())
                  psz = Filesets_GetColumnText (lpi, (FILESETCOLUMN)iCol, fShowServerName);
               else if (lpi->fIsAggregate() && (pfln->item.icol == setcolNAME))
                  psz = Aggregates_GetColumnText (lpi, aggcolNAME, FALSE);
               else if (lpi->fIsServer() && (pfln->item.icol == setcolNAME))
                  psz = Server_GetColumnText (lpi, svrcolNAME);
               break;

            case dtREPLICAS:
               if (lpi->fIsFileset())
                  psz = Replicas_GetColumnText (lpi, (REPLICACOLUMN)iCol);
               break;
            }
         }
      }

   lstrcpy (pfln->item.pszText, (psz) ? psz : TEXT(""));
   return TRUE;
}


void Display_AddActOnDone (HWND hWnd, WORD wAct, LPIDENT lpiSelectOnDone)
{
   if (pcsWindowActOnDone == NULL)
      {
      pcsWindowActOnDone = New (CRITICAL_SECTION);
      InitializeCriticalSection (pcsWindowActOnDone);
      }
   EnterCriticalSection (pcsWindowActOnDone);

   size_t ii;
   for (ii = 0; ii < cWindowActOnDone; ++ii)
      {
      if (aWindowActOnDone[ ii ].hWnd == hWnd)
         break;
      }
   if (ii == cWindowActOnDone)
      {
      for (ii = 0; ii < cWindowActOnDone; ++ii)
         {
         if (aWindowActOnDone[ ii ].hWnd == 0)
            break;
         }
      }
   if (ii == cWindowActOnDone)
      {
      (void)REALLOC( aWindowActOnDone, cWindowActOnDone, 1+ii, 1 );
      }
   if (ii < cWindowActOnDone)
      {
      aWindowActOnDone[ ii ].hWnd = hWnd;
      aWindowActOnDone[ ii ].actOnDone |= wAct;
      if (!aWindowActOnDone[ ii ].lpiSelectOnDone)
         aWindowActOnDone[ ii ].lpiSelectOnDone = lpiSelectOnDone;
      }

   LeaveCriticalSection (pcsWindowActOnDone);
}


WORD Display_FreeActOnDone (HWND hWnd, LPIDENT *plpiSelectOnDone)
{
   WORD wAct = 0;

   if (pcsWindowActOnDone == NULL)
      {
      pcsWindowActOnDone = New (CRITICAL_SECTION);
      InitializeCriticalSection (pcsWindowActOnDone);
      }
   EnterCriticalSection (pcsWindowActOnDone);

   for (size_t ii = 0; ii < cWindowActOnDone; ++ii)
      {
      if (aWindowActOnDone[ ii ].hWnd == hWnd)
         {
         wAct = aWindowActOnDone[ ii ].actOnDone;
         if (!(*plpiSelectOnDone))
            *plpiSelectOnDone = aWindowActOnDone[ ii ].lpiSelectOnDone;

         aWindowActOnDone[ ii ].actOnDone = 0;
         aWindowActOnDone[ ii ].hWnd = 0;
         aWindowActOnDone[ ii ].lpiSelectOnDone = 0;
         break;
         }
      }

   LeaveCriticalSection (pcsWindowActOnDone);
   return wAct;
}


void UpdateDisplay (LPDISPLAYREQUEST pdr, BOOL fWait)
{
   BOOL fRunBeforeReturning = FALSE;

   if (!ASSERT( pdr && pdr->hChild ))
      return;
   if (pdr->lpiNotify && pdr->lpiNotify->fIsCell())
      pdr->lpiNotify = NULL;

   if (pcsDisplayQueue == NULL)
      {
      pcsDisplayQueue = New (CRITICAL_SECTION);
      InitializeCriticalSection (pcsDisplayQueue);
      memset (&drActiveSERVERS, 0x00, sizeof(DISPLAYREQUEST));
      memset (&drActiveSERVICES, 0x00, sizeof(DISPLAYREQUEST));
      memset (&drActiveAGGREGATES, 0x00, sizeof(DISPLAYREQUEST));
      memset (&drActiveFILESETS, 0x00, sizeof(DISPLAYREQUEST));
      memset (&drActiveSERVERWINDOW, 0x00, sizeof(DISPLAYREQUEST));
      }

   EnterCriticalSection (pcsDisplayQueue);

   size_t idq;
   for (idq = 0; idq < cDisplayQueue; ++idq)
      {
      if (!aDisplayQueue[idq].hChild)
         break;
      }
   if (idq == cDisplayQueue)
      {
      (void)REALLOC (aDisplayQueue, cDisplayQueue, 1+idq, cREALLOC_DISPLAYQUEUE);
      }
   if (idq < cDisplayQueue)
      {
      memcpy (&aDisplayQueue[idq], pdr, sizeof(DISPLAYREQUEST));

      // Filter the display queue--for instance, if there's a request
      // to update all filesets, we don't need to file a request to
      // update an individual fileset.  Likewise, if we're about to
      // file a request to update all filesets, nix all existing update-
      // this-fileset and update-filesets-on-this-server requests.
      //
      for (size_t idqKiller = 0; idqKiller < cDisplayQueue; ++idqKiller)
         {
         if (DisplayQueueFilter (idq, idqKiller))
            {
            aDisplayQueue[idq].hChild = 0;
            break;
            }
         }

      // Hmmmm...even if there is no request in the queue which lets us
      // kill this request, there may be a request actively being serviced
      // *right now* which does so.  Test for that case too.
      //
      if (aDisplayQueue[idq].hChild)
         {
         if (DisplayQueueFilter (idq, (size_t)-1))
            aDisplayQueue[idq].hChild = 0;
         }

      // Did the new request make it through all those tests?  If so,
      // see if we can remove any other entries because of this one.
      // Then initiate a thread to actually do the work if there isn't one.
      //
      if (aDisplayQueue[idq].hChild)
         {
         for (size_t idqVictim = 0; idqVictim < cDisplayQueue; ++idqVictim)
            {
            if (DisplayQueueFilter (idqVictim, idq))
               {
               InterlockedDecrementByWindow (aDisplayQueue[idqVictim].hChild);
               aDisplayQueue[idqVictim].hChild = 0;
               --cDisplayQueueActive;
               }
            }

         if (aDisplayQueue[idq].hChild)
            {
            InterlockedIncrementByWindow (aDisplayQueue[ idq ].hChild);

            if (fWait)
               {
               aDisplayQueue[idq].hChild = NULL; // we'll handle this one.
               fRunBeforeReturning = TRUE;       // (remember to do so)
               }
            else if ((++cDisplayQueueActive) >= 1)
               {
               if (cUpdateThreadsActive < cUPDATE_THREADS_MAX)
                  {
                  ++cUpdateThreadsActive;
                  StartThread (DisplayQueue_ThreadProc, 0);
                  }
               }
            }
         }
      }

   LeaveCriticalSection (pcsDisplayQueue);

   if (fRunBeforeReturning)
      {
      DisplayQueue_ThreadProc (pdr);
      }
}


DWORD WINAPI DisplayQueue_ThreadProc (PVOID lp)
{
   LPDISPLAYREQUEST pdr = (LPDISPLAYREQUEST)lp;
   LPDISPLAYREQUEST pdrActive = NULL;

   Main_StartWorking();

   do {
      DISPLAYREQUEST dr;

      EnterCriticalSection (pcsDisplayQueue);
      if (pdr)
         {
         memcpy (&dr, pdr, sizeof(DISPLAYREQUEST));
         }
      else
         {
         size_t idq;
         for (idq = 0; idq < cDisplayQueue; ++idq)
            {
            if (aDisplayQueue[idq].hChild)
               {
               memcpy (&dr, &aDisplayQueue[idq], sizeof(DISPLAYREQUEST));
               memset (&aDisplayQueue[idq], 0x00, sizeof(DISPLAYREQUEST));
               --cDisplayQueueActive;
               break;
               }
            }
         if (idq == cDisplayQueue)
            {
            if (!pdr) // Are we losing a background thread?
               --cUpdateThreadsActive;
            LeaveCriticalSection (pcsDisplayQueue);
            break;
            }
         }

      switch (dr.dt)
         {
         case dtSERVERS:       pdrActive = &drActiveSERVERS;       break;
         case dtSERVICES:      pdrActive = &drActiveSERVICES;      break;
         case dtAGGREGATES:    pdrActive = &drActiveAGGREGATES;    break;
         case dtFILESETS:      pdrActive = &drActiveFILESETS;      break;
         case dtSERVERWINDOW:  pdrActive = &drActiveSERVERWINDOW;  break;
         }
      if (pdrActive)
         memcpy (pdrActive, &dr, sizeof(DISPLAYREQUEST));

      LeaveCriticalSection (pcsDisplayQueue);

      AfsClass_Enter();

      switch (dr.dt)
         {
         case dtCELL:
            Display_Cell_Internal (&dr);
            break;

         case dtSERVERS:
            Display_Servers_Internal (&dr);
            break;

         case dtSERVICES:
            Display_Services_Internal (&dr);
            break;

         case dtAGGREGATES:
            Display_Aggregates_Internal (&dr);
            break;

         case dtFILESETS:
            Display_Filesets_Internal (&dr);
            break;

         case dtREPLICAS:
            Display_Replicas_Internal (&dr);
            break;

         case dtSERVERWINDOW:
            Display_ServerWindow_Internal (&dr);
            break;
         }

      EnterCriticalSection (pcsDisplayQueue);
      LONG dw = InterlockedDecrementByWindow (dr.hChild);
      if (pdrActive)
         memset (pdrActive, 0x00, sizeof(DISPLAYREQUEST));
      LeaveCriticalSection (pcsDisplayQueue);

      if (dw == 0)
         {
         WORD actOnDone = dr.actOnDone;
         LPIDENT lpiSelectOnDone = dr.lpiToSelect;
         if (dr.hList)
            {
            actOnDone |= Display_FreeActOnDone (dr.hList, &lpiSelectOnDone);
            }

         if ((actOnDone & ACT_ENDCHANGE) && dr.hList)
            {
            if (dr.fList)
               FL_EndChange (dr.hList, (LPARAM)lpiSelectOnDone);
            else // must be a combobox
               CB_EndChange (dr.hList, (LPARAM)lpiSelectOnDone);
            }

         if ((actOnDone & ACT_UNCOVER) && dr.hList)
            AfsAppLib_Uncover (dr.hList);

         if (actOnDone & ACT_SELPREVIEW)
            {
            LPIDENT lpiOld = Server_GetServer (SERVERWINDOW_PREVIEWPANE);
            LPIDENT lpiNew = (LPIDENT)FL_GetSelectedData (GetDlgItem (g.hMain, IDC_SERVERS));
            if (lpiOld != lpiNew)
               Server_SelectServer (SERVERWINDOW_PREVIEWPANE, lpiNew, TRUE);
            }
         }
      else if (dr.hList)
         {
         Display_AddActOnDone (dr.hList, dr.actOnDone, dr.lpiToSelect);
         }

      AfsClass_Leave();

      } while (!lp);  // if given one task to do, stop; otherwise, loop forever

   Main_StopWorking();

   return 0;
}


BOOL DisplayQueueFilter (size_t idqVictim, size_t idqKiller)
{
   if (idqVictim == idqKiller)
      return FALSE;

   LPDISPLAYREQUEST pdrKiller = (idqKiller == (size_t)-1) ? NULL : &aDisplayQueue[ idqKiller ];
   LPDISPLAYREQUEST pdrVictim = &aDisplayQueue[ idqVictim ];

   // if there's currently an operation in progress for this window,
   // we may have just been asked to filter out a new request based on
   // what's being done now.  {idqKiller==-1} signifies this case.
   //
   if (pdrKiller == NULL) // was idqKiller==-1 etc?
      {
      switch (pdrVictim->dt)
         {
         case dtSERVERS:
            pdrKiller = &drActiveSERVERS;
            break;

         case dtSERVICES:
            pdrKiller = &drActiveSERVICES;
            break;

         case dtAGGREGATES:
            pdrKiller = &drActiveAGGREGATES;
            break;

         case dtFILESETS:
            pdrKiller = &drActiveFILESETS;
            break;

         case dtSERVERWINDOW:
            pdrKiller = &drActiveSERVERWINDOW;
            break;
         }

      if (!pdrKiller)
         return FALSE;
      }

   if ( (pdrVictim->dt == pdrKiller->dt) &&
        (pdrVictim->hChild == pdrKiller->hChild) )
      {
      // only some windows are subject to this filtering.
      //
      switch (pdrVictim->dt)
         {
         case dtCELL:
         case dtREPLICAS:
            return FALSE; // don't bother filtering these.

         case dtSERVERWINDOW:
            return TRUE;  // update svr window twice?  why?
         }

      // if the new request talks about displaying information for a different
      // server, the user must have selected or deselected a server in the
      // list. we'll keep the new request.
      //
      if (pdrKiller->lpiServer != pdrVictim->lpiServer)
         return FALSE;

      // if pdrKiller is told to update everything, then all other requests
      // are unnecessary.
      //
      if (!pdrKiller->lpiNotify || pdrKiller->lpiNotify->fIsCell())
         return TRUE;

      // if pdrVictim is told to update everything, then we'll always bow to it.
      //
      if (!pdrVictim->lpiNotify || pdrVictim->lpiNotify->fIsCell())
         return FALSE;

      // kill any duplicate request to update a particular object.
      //
      if (pdrVictim->lpiNotify == pdrKiller->lpiNotify)
         return TRUE;

      // kill any request to update a service or aggregate or fileset,
      // if updating the entire associated server.
      //
      if ( (pdrKiller->lpiNotify->fIsServer()) &&
           (pdrVictim->lpiNotify->GetServer() == pdrKiller->lpiNotify) )
         return TRUE;

      // kill any request to update a fileset, if updating the entire
      // associated aggregate.
      //
      if ( (pdrKiller->lpiNotify->fIsAggregate()) &&
           (pdrVictim->lpiNotify->fIsFileset()) &&
           (pdrVictim->lpiNotify->GetAggregate() == pdrKiller->lpiNotify) )
         return TRUE;
      }

   // hmmm...guess we need this request after all.
   //
   return FALSE;
}


void UpdateDisplay_Cell (BOOL fWait)
{
   DISPLAYREQUEST dr;
   memset (&dr, 0x00, sizeof(dr));
   dr.dt = dtCELL;
   dr.hChild = GetDlgItem (g.hMain, IDC_CELL);
   UpdateDisplay (&dr, fWait);
}

void UpdateDisplay_Servers (BOOL fWait, LPIDENT lpiNotify, ULONG status)
{
   DISPLAYREQUEST dr;
   memset (&dr, 0x00, sizeof(dr));
   dr.dt = dtSERVERS;
   dr.hChild = GetDlgItem (g.hMain, IDC_SERVERS);
   dr.lpiNotify = lpiNotify;
   dr.status = status;
   dr.fList = TRUE;
   UpdateDisplay (&dr, fWait);
}

void UpdateDisplay_Services (BOOL fWait, HWND hChild, LPIDENT lpiNotify, ULONG status)
{
   DISPLAYREQUEST dr;
   memset (&dr, 0x00, sizeof(dr));
   dr.dt = dtSERVICES;
   dr.hChild = hChild;
   dr.lpiNotify = lpiNotify;
   dr.status = status;
   dr.fList = TRUE;
   UpdateDisplay (&dr, fWait);
}

void UpdateDisplay_Aggregates (BOOL fWait, HWND hListOrCombo, LPIDENT lpiNotify, ULONG status, LPIDENT lpiServer, LPIDENT lpiToSelect, LPVIEWINFO lpvi)
{
   DISPLAYREQUEST dr;
   memset (&dr, 0x00, sizeof(dr));
   dr.dt = dtAGGREGATES;
   dr.hList = hListOrCombo;
   dr.hChild = GetParent (hListOrCombo);
   dr.lpiNotify = lpiNotify;
   dr.status = status;
   dr.lpiServer = (lpiServer == NULL) ? Server_GetServerForChild (dr.hChild) : lpiServer;
   dr.lpiToSelect = lpiToSelect;
   dr.lpvi = lpvi;
   UpdateDisplay (&dr, fWait);
}

void UpdateDisplay_Filesets (BOOL fWait, HWND hListOrCombo, LPIDENT lpiNotify, ULONG status, LPIDENT lpiServer, LPIDENT lpiAggregate, LPIDENT lpiToSelect)
{
   DISPLAYREQUEST dr;
   memset (&dr, 0x00, sizeof(dr));
   dr.dt = dtFILESETS;
   dr.hList = hListOrCombo;
   dr.hChild = GetParent (hListOrCombo);
   dr.lpiNotify = lpiNotify;
   dr.status = status;
   dr.lpiServer = (lpiServer == NULL) ? Server_GetServerForChild (dr.hChild) : lpiServer;
   dr.lpiAggregate = lpiAggregate;
   dr.lpiToSelect = lpiToSelect;
   UpdateDisplay (&dr, fWait);
}

void UpdateDisplay_Replicas (BOOL fWait, HWND hList, LPIDENT lpiRW, LPIDENT lpiRO)
{
   DISPLAYREQUEST dr;
   memset (&dr, 0x00, sizeof(dr));
   dr.dt = dtREPLICAS;
   dr.hChild = GetParent (hList);
   dr.hList = hList;
   dr.lpiNotify = lpiRW;
   dr.lpiToSelect = lpiRO;
   dr.fList = TRUE;
   UpdateDisplay (&dr, fWait);
}


void UpdateDisplay_ServerWindow (BOOL fWait, LPIDENT lpiServer)
{
   // First, if there is a dedicated server window out there, update it.
   //
   HWND hServer;
   if ((hServer = PropCache_Search (pcSERVER, lpiServer)) != NULL)
      {
      DISPLAYREQUEST dr;
      memset (&dr, 0x00, sizeof(dr));
      dr.dt = dtSERVERWINDOW;
      dr.hChild = hServer;
      dr.lpiServer = lpiServer;

      UpdateDisplay (&dr, fWait);
      }

   // Second, if the preview pane is visible and showing this server,
   // update it too.
   //
   if (gr.fPreview)
      {
      LPIDENT lpiPreview = Server_GetServer (g.hMain);
      if ((lpiPreview == NULL) || (lpiPreview == lpiServer))
         {
         DISPLAYREQUEST dr;
         memset (&dr, 0x00, sizeof(dr));
         dr.dt = dtSERVERWINDOW;
         dr.hChild = g.hMain;
         dr.lpiServer = lpiPreview;

         UpdateDisplay (&dr, fWait);
         }
      }
}


void UpdateDisplay_SetIconView (BOOL fWait, HWND hDialog, LPICONVIEW piv, ICONVIEW ivNew)
{
   *piv = ivNew;

   if (piv == &gr.ivSvr)
      {
      UpdateDisplay_Servers (fWait, NULL, 0);
      }
   else
      {
      LPIDENT lpi = Server_GetServer (hDialog);
      UpdateDisplay_ServerWindow (fWait, lpi);
      }
}


ICONVIEW Display_GetServerIconView (void)
{
   LONG lvs;

   if (gr.fPreview && !gr.fVert)
      lvs = gr.diHorz.viewSvr.lvsView;
   else
      lvs = gr.diVert.viewSvr.lvsView;


   if (lvs != FLS_VIEW_LIST)
      return ivONEICON;

   return gr.ivSvr;
}


BOOL HandleColumnNotify (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp, LPVIEWINFO pvi)
{
   if (msg == WM_NOTIFY)
      {
      HWND hList = GetDlgItem (hDlg, (int)((LPNMHDR)lp)->idFrom);
      if (fIsFastList (hList))
         {
         switch (((LPNMHDR)lp)->code)
            { 
            case FLN_COLUMNRESIZE:
               FL_StoreView (hList, pvi);
               return TRUE;

            case FLN_COLUMNCLICK:
               LPFLN_COLUMNCLICK_PARAMS pp = (LPFLN_COLUMNCLICK_PARAMS)lp;

               int iCol;
               BOOL fRev;
               FastList_GetSortStyle (hList, &iCol, &fRev);

               if (iCol == pp->icol)
                  FastList_SetSortStyle (hList, iCol, !fRev);
               else
                  FastList_SetSortStyle (hList, pp->icol, FALSE);

               FL_StoreView (hList, pvi);
               return TRUE;
            }
         }
      }

   return FALSE;
}

