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
#include "command.h"
#include "columns.h"
#include "subset.h"
#include "creds.h"
#include "task.h"
#include "helpfunc.h"
#include "options.h"
#include "display.h"
#include "svr_window.h"
#include "svr_security.h"
#include "svr_syncvldb.h"
#include "svr_salvage.h"
#include "svc_create.h"
#include "svc_delete.h"
#include "svc_startstop.h"
#include "svc_viewlog.h"
#include "svr_prop.h"
#include "svr_install.h"
#include "svr_uninstall.h"
#include "svr_prune.h"
#include "svr_getdates.h"
#include "svr_execute.h"
#include "svr_hosts.h"
#include "svc_prop.h"
#include "agg_prop.h"
#include "set_prop.h"
#include "set_repprop.h"
#include "set_create.h"
#include "set_delete.h"
#include "set_move.h"
#include "set_quota.h"
#include "set_rename.h"
#include "set_release.h"
#include "set_clone.h"
#include "set_dump.h"
#include "set_restore.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Command_OnProperties (LPIDENT lpi);
void Command_OnIconView (HWND hDialog, BOOL fServerView, CHILDTAB iTab, int cmd);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void StartContextCommand (HWND hDialog,
                          LPIDENT lpiRepresentedByWindow,
                          LPIDENT lpiChosenByClick,
                          int cmd)
{
   CHILDTAB iTab = Server_GetDisplayedTab (hDialog);
   LPIDENT lpi = (lpiChosenByClick) ? lpiChosenByClick : lpiRepresentedByWindow;

   if (lpi && lpi->fIsCell())
      lpi = NULL;

   switch (cmd)
      {
      case M_COLUMNS:
         if (iTab == tabSERVICES)
            ShowColumnsDialog (hDialog, &gr.viewSvc);
         else if (iTab == tabAGGREGATES)
            ShowColumnsDialog (hDialog, &gr.viewAgg);
         else if (iTab == tabFILESETS)
            ShowColumnsDialog (hDialog, &gr.viewSet);
         else
            ShowColumnsDialog (hDialog, NULL);
         break;

      case M_SVR_VIEW_ONEICON:
      case M_SVR_VIEW_TWOICONS:
      case M_SVR_VIEW_STATUS:
         Command_OnIconView (hDialog, TRUE, iTab, cmd);
         break;

      case M_VIEW_ONEICON:
      case M_VIEW_TWOICONS:
      case M_VIEW_STATUS:
         Command_OnIconView (hDialog, FALSE, iTab, cmd);
         break;

      case M_PROPERTIES:
         if (lpi)
            Command_OnProperties (lpi);
         break;

      case M_SUBSET:
         ShowSubsetsDialog();
         break;

      case M_REFRESHALL:
         if (g.lpiCell)
            StartTask (taskREFRESH, NULL, g.lpiCell);
         break;

      case M_REFRESH:
         if (lpi)
            StartTask (taskREFRESH, NULL, lpi);
         else if (g.lpiCell)
            StartTask (taskREFRESH, NULL, g.lpiCell);
         break;

      case M_SYNCVLDB:
         if (lpi)
            Server_SyncVLDB (lpi);
         break;

      case M_SALVAGE:
         if (lpi)
            Server_Salvage (lpi);
         break;

      case M_SET_CREATE:
         Filesets_Create (lpi);
         break;

      case M_SET_REPLICATION:
         if (lpi && lpi->fIsFileset())
            Filesets_ShowReplication (Server_GetWindowForChild (hDialog), lpi);
         break;

      case M_SET_DELETE:
         if (lpi && lpi->fIsFileset())
            Filesets_Delete (lpi);
         break;

      case M_SET_CLONE:
         Filesets_Clone (lpi);
         break;

      case M_SET_DUMP:
         if (lpi && lpi->fIsFileset())
            Filesets_Dump (lpi);
         break;

      case M_SET_RESTORE:
         Filesets_Restore (lpi);
         break;

      case M_SET_RELEASE:
         if (lpi && lpi->fIsFileset())
            Filesets_Release (lpi);
         break;

      case M_SET_MOVETO:
         if (lpi && lpi->fIsFileset())
            Filesets_ShowMoveTo (lpi, NULL);
         break;

      case M_SET_SETQUOTA:
         if (lpi && lpi->fIsFileset())
            Filesets_SetQuota (lpi);
         break;

      case M_SET_LOCK:
         if (lpi && lpi->fIsFileset())
            StartTask (taskSET_LOCK, NULL, lpi);
         break;

      case M_SET_UNLOCK:
         if (lpi && !lpi->fIsService())
            StartTask (taskSET_UNLOCK, NULL, lpi);
         else if (!lpi && g.lpiCell)
            StartTask (taskSET_UNLOCK, NULL, g.lpiCell);
         break;

      case M_SET_RENAME:
         if (lpi && lpi->fIsFileset())
            Filesets_ShowRename (lpi);
         break;

      case M_SVR_OPEN:
         if (lpi && lpi->fIsServer())
            StartTask (taskSVR_GETWINDOWPOS, g.hMain, lpi);
         break;

      case M_SVR_CLOSE:
         if (lpi && lpi->fIsServer())
            Server_Close (lpi);
         break;

      case M_SVR_CLOSEALL:
         Server_CloseAll (TRUE);
         break;

      case M_SVR_SECURITY:
         Server_Security (lpi);
         break;

      case M_SVR_HOSTS:
         Server_Hosts (lpi);
         break;

      case M_SVR_INSTALL:
         Server_Install (lpi);
         break;

      case M_SVR_UNINSTALL:
         Server_Uninstall (lpi);
         break;

      case M_SVR_PRUNE:
         Server_Prune (lpi);
         break;

      case M_SVR_GETDATES:
         Server_GetDates (lpi);
         break;

      case M_EXECUTE:
         Server_Execute (lpi);
         break;

      case M_VIEWLOG:
         if (lpi && lpi->fIsService())
            Services_ShowServiceLog (lpi);
         else
            Services_ShowServerLog (lpi);
         break;

      case M_SVR_MONITOR:
         if (lpi && lpi->fIsServer())
            StartTask (taskSVR_MONITOR_ONOFF, NULL, lpi);
         break;

      case M_SVC_CREATE:
         if (!lpi)
            Services_Create (NULL);
         else
            Services_Create (lpi->GetServer());
         break;

      case M_SVC_DELETE:
         if (lpi && lpi->fIsService())
            Services_Delete (lpi);
         break;

      case M_SVC_START:
         if (lpi && lpi->fIsService())
            Services_Start (lpi);
         break;

      case M_SVC_STOP:
         if (lpi && lpi->fIsService())
            Services_Stop (lpi);
         break;

      case M_SVC_RESTART:
         if (lpi && lpi->fIsService())
            Services_Restart (lpi);
         break;

      case M_CELL_OPEN:
         OpenCellDialog();
         break;

      case M_CREDENTIALS:
         NewCredsDialog();
         break;

      case M_OPTIONS:
         ShowOptionsDialog();
         break;

      case M_HELP:
         WinHelp (g.hMain, cszHELPFILENAME, HELP_FINDER, 0);
         break;

      case M_HELP_FIND:
         Help_FindCommand();
         break;

      case M_HELP_XLATE:
         Help_FindError();
         break;

      case M_ABOUT:
         Help_About();
         break;
      }
}


void Command_OnProperties (LPIDENT lpi)
{
   if (lpi)
      {
      size_t nAlerts = Alert_GetCount (lpi);

      if (lpi->fIsServer())
         {
         Server_ShowProperties (lpi, nAlerts);
         }
      else if (lpi->fIsService())
         {
         Services_ShowProperties (lpi, nAlerts);
         }
      else if (lpi->fIsAggregate())
         {
         Aggregates_ShowProperties (lpi, nAlerts);
         }
      else if (lpi->fIsFileset())
         {
         Filesets_ShowProperties (lpi, nAlerts);
         }
      }
}


void Command_OnIconView (HWND hDialog, BOOL fServerView, CHILDTAB iTab, int cmd)
{
   ICONVIEW *piv = NULL;

   if (fServerView)
      {
      piv = &gr.ivSvr;
      }
   else if (iTab == tabAGGREGATES)
      {
      piv = &gr.ivAgg;
      }
   else if (iTab == tabSERVICES)
      {
      piv = &gr.ivSvc;
      }
   else if (iTab == tabFILESETS)
      {
      piv = &gr.ivSet;
      }

   ICONVIEW iv;
   switch (cmd)
      {
      case M_SVR_VIEW_TWOICONS:
      case M_VIEW_TWOICONS:
         iv = ivTWOICONS;
         break;

      case M_SVR_VIEW_STATUS:
      case M_VIEW_STATUS:
         iv = ivSTATUS;
         break;

      case M_SVR_VIEW_ONEICON:
      case M_VIEW_ONEICON:
      default:
         iv = ivONEICON;
         break;
      }

   if (piv)
      {
      UpdateDisplay_SetIconView (FALSE, hDialog, piv, iv);
      }

   if (fServerView)
      Main_SetServerViewMenus();
}

