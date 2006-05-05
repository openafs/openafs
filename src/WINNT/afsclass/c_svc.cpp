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

#include <WINNT/afsclass.h>
#include "internal.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */


/*
 * VARIABLES __________________________________________________________________
 *
 */


/*
 * PROTOTYPES _________________________________________________________________
 *
 */


/*
 * ROUTINES ___________________________________________________________________
 *
 */


SERVICE::SERVICE (LPSERVER lpServerParent, LPTSTR pszName)
{
   m_lpiServer = lpServerParent->GetIdentifier();
   m_lpiCell = m_lpiCell;
   m_lpiThis = NULL;

   lstrcpy (m_szName, pszName);

   m_fStatusOutOfDate = TRUE;
   memset (&m_ss, 0x00, sizeof(SERVICESTATUS));
}


SERVICE::~SERVICE (void)
{
   if (m_lpiThis)
      m_lpiThis->m_cRef --;
}


void SERVICE::SendDeleteNotifications (void)
{
   NOTIFYCALLBACK::SendNotificationToAll (evtDestroy, GetIdentifier());
}


void SERVICE::Close (void)
{
   AfsClass_Leave();
}


LPIDENT SERVICE::GetIdentifier (void)
{
   if (m_lpiThis == NULL)
      {
      if ((m_lpiThis = IDENT::FindIdent (this)) == NULL)
         m_lpiThis = New2 (IDENT,(this));
      m_lpiThis->m_cRef ++;
      }

   return m_lpiThis;
}


void SERVICE::Invalidate (void)
{
   if (!m_fStatusOutOfDate)
      {
      m_fStatusOutOfDate = TRUE;
      NOTIFYCALLBACK::SendNotificationToAll (evtInvalidate, GetIdentifier());
      }
}


BOOL SERVICE::RefreshStatus (BOOL fNotify, ULONG *pStatus)
{
   BOOL rc = TRUE;
   DWORD status = 0;

   if (m_fStatusOutOfDate)
      {
      m_fStatusOutOfDate = FALSE;

      if (fNotify)
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusBegin, GetIdentifier());

      LPSERVER lpServer;
      if ((lpServer = OpenServer (&status)) == NULL)
         rc = FALSE;
      else
         {
         PVOID hCell;
         PVOID hBOS;
         if ((hBOS = lpServer->OpenBosObject (&hCell, &status)) == NULL)
            rc = FALSE;
         else
            {
            SERVICESTATUS ss;
            if (!lstrcmp (m_szName, TEXT("BOS")))
               {
               memset (&ss, 0x00, sizeof(SERVICESTATUS));
               AfsClass_UnixTimeToSystemTime (&ss.timeLastStart, 0);
               AfsClass_UnixTimeToSystemTime (&ss.timeLastStop, 0);
               AfsClass_UnixTimeToSystemTime (&ss.timeLastFail, 0);
               ss.nStarts = 1;
               ss.dwErrLast = 0;
               ss.dwSigLast = 0;
               ss.type = SERVICETYPE_SIMPLE;
               ss.state = SERVICESTATE_RUNNING;
               }
            else
               {
               WORKERPACKET wp;
               wp.wpBosProcessInfoGet.hServer = hBOS;
               wp.wpBosProcessInfoGet.pszService = m_szName;

               if (!Worker_DoTask (wtaskBosProcessInfoGet, &wp, &status))
                  rc = FALSE;
               else
                  {
                  memcpy (&ss, &wp.wpBosProcessInfoGet.ss, sizeof(SERVICESTATUS));

                  // Get the service's current state
                  //
                  wp.wpBosProcessExecutionStateGet.hServer = hBOS;
                  wp.wpBosProcessExecutionStateGet.pszService = m_szName;
                  wp.wpBosProcessExecutionStateGet.pszAuxStatus = ss.szAuxStatus;
                  if (!Worker_DoTask (wtaskBosProcessExecutionStateGet, &wp, &status))
                     ss.state = SERVICESTATE_STOPPED;
                  else
                     ss.state = wp.wpBosProcessExecutionStateGet.state;

                  // Get the service's notifier
                  //
                  ss.szNotifier[0] = TEXT('\0');
                  wp.wpBosProcessNotifierGet.hServer = hBOS;
                  wp.wpBosProcessNotifierGet.pszService = m_szName;
                  wp.wpBosProcessNotifierGet.pszNotifier = ss.szNotifier;;
                  Worker_DoTask (wtaskBosProcessNotifierGet, &wp, &status);

                  // Get the service's parameters
                  //
                  ss.szParams[0] = TEXT('\0');

                  WORKERPACKET wpBegin;
                  wpBegin.wpBosProcessParameterGetBegin.hServer = hBOS;
                  wpBegin.wpBosProcessParameterGetBegin.pszService = m_szName;
                  if (Worker_DoTask (wtaskBosProcessParameterGetBegin, &wpBegin, &status))
                     {
                     for (;;)
                        {
                        TCHAR szParam[ 256 ];
                        WORKERPACKET wpNext;
                        wpNext.wpBosProcessParameterGetNext.hEnum = wpBegin.wpBosProcessParameterGetBegin.hEnum;
                        wpNext.wpBosProcessParameterGetNext.pszParam = szParam;

                        if (!Worker_DoTask (wtaskBosProcessParameterGetNext, &wpNext, &status))
                           {
                           if (status == ADMITERATORDONE)
                              status = 0;
                           else
                              rc = FALSE;
                           break;
                           }

                        if (ss.szParams[0] != TEXT('\0'))
                           lstrcat (ss.szParams, TEXT(" "));
                        lstrcat (ss.szParams, szParam);
                        }

                     WORKERPACKET wpDone;
                     wpDone.wpBosProcessParameterGetDone.hEnum = wpBegin.wpBosProcessParameterGetBegin.hEnum;
                     Worker_DoTask (wtaskBosProcessParameterGetDone, &wpDone);
                     }

                  // Strip trailing CR/LF characters
                  //
                  size_t cch = lstrlen (ss.szAuxStatus);
                  while (cch && (ss.szAuxStatus[ cch-1 ] == TEXT('\r') || ss.szAuxStatus[ cch-1 ] == TEXT('\n')))
                     ss.szAuxStatus[ cch-- ] = TEXT('\0');

                  cch = lstrlen (ss.szParams);
                  while (cch && (ss.szParams[ cch-1 ] == TEXT('\r') || ss.szParams[ cch-1 ] == TEXT('\n')))
                     ss.szParams[ cch-- ] = TEXT('\0');

                  cch = lstrlen (ss.szNotifier);
                  while (cch && (ss.szNotifier[ cch-1 ] == TEXT('\r') || ss.szNotifier[ cch-1 ] == TEXT('\n')))
                     ss.szNotifier[ cch-- ] = TEXT('\0');
                  }
               }

            if (rc)
               {
               memcpy (&m_ss, &ss, sizeof(SERVICESTATUS));
               }

            lpServer->CloseBosObject();
            }

         lpServer->Close();
         }

      if (fNotify)
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusEnd, GetIdentifier(), ((rc) ? 0 : status));
      }

   if (pStatus && !rc)
      *pStatus = status;
   return TRUE;
}


void SERVICE::GetName (LPTSTR pszName)
{
   lstrcpy (pszName, m_szName);
}


LPCELL SERVICE::OpenCell (ULONG *pStatus)
{
   return m_lpiCell->OpenCell (pStatus);
}


LPSERVER SERVICE::OpenServer (ULONG *pStatus)
{
   return m_lpiServer->OpenServer (pStatus);
}



BOOL SERVICE::GetStatus (LPSERVICESTATUS lpss, BOOL fNotify, ULONG *pStatus)
{
   if (!RefreshStatus (fNotify, pStatus))
      return FALSE;

   memcpy (lpss, &m_ss, sizeof(SERVICESTATUS));
   return TRUE;
}


PVOID SERVICE::GetUserParam (void)
{
   return GetIdentifier()->GetUserParam();
}


void SERVICE::SetUserParam (PVOID pUserNew)
{
   GetIdentifier()->SetUserParam (pUserNew);
}

