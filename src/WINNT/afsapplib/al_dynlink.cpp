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

#include <WINNT/afsapplib.h>
#include "al_dynlink.h"


/*
 * LIBRARIES __________________________________________________________________
 *
 */

#define cszLIBRARY_UTIL        TEXT("AfsAdminUtil.dll")
#define cszLIBRARY_KAS         TEXT("AfsKasAdmin.dll")
#define cszLIBRARY_CLIENT      TEXT("AfsClientAdmin.dll")


/*
 * ADMIN ______________________________________________________________________
 *
 */

static size_t g_cReqUtilLibrary = 0;
static HINSTANCE g_hiUtilLibrary = NULL;

util_AdminErrorCodeTranslate_t util_AdminErrorCodeTranslateP = NULL;


BOOL OpenUtilLibrary (ULONG *pStatus)
{
   BOOL rc = FALSE;
   ULONG status = ERROR_DLL_NOT_FOUND;

   if ((++g_cReqUtilLibrary) == 1)
      {
      if ( ((g_hiUtilLibrary = LoadLibrary (cszLIBRARY_UTIL)) == NULL) ||
           ((util_AdminErrorCodeTranslateP = (util_AdminErrorCodeTranslate_t)GetProcAddress (g_hiUtilLibrary, "util_AdminErrorCodeTranslate")) == NULL) )
         {
         status = GetLastError();
         CloseUtilLibrary();
         }
      }
   if (g_hiUtilLibrary)
      {
      rc = TRUE;
      }

   if (pStatus)
      *pStatus = status;
   return rc;
}


void CloseUtilLibrary (void)
{
   if ((--g_cReqUtilLibrary) == 0)
      {
      if (g_hiUtilLibrary)
         FreeLibrary (g_hiUtilLibrary);
      g_hiUtilLibrary = NULL;
      }
}


/*
 * KAS ________________________________________________________________________
 *
 */

static size_t g_cReqKasLibrary = 0;
static HINSTANCE g_hiKasLibrary = NULL;

kas_PrincipalGetBegin_t kas_PrincipalGetBeginP = NULL;
kas_PrincipalGetNext_t kas_PrincipalGetNextP = NULL;
kas_PrincipalGetDone_t kas_PrincipalGetDoneP = NULL;
kas_PrincipalGet_t kas_PrincipalGetP = NULL;


BOOL OpenKasLibrary (ULONG *pStatus)
{
   BOOL rc = FALSE;
   ULONG status = ERROR_DLL_NOT_FOUND;

   if ((++g_cReqKasLibrary) == 1)
      {
      if ( ((g_hiKasLibrary = LoadLibrary (cszLIBRARY_KAS)) == NULL) ||
           ((kas_PrincipalGetBeginP = (kas_PrincipalGetBegin_t)GetProcAddress (g_hiKasLibrary, "kas_PrincipalGetBegin")) == NULL) ||
           ((kas_PrincipalGetNextP = (kas_PrincipalGetNext_t)GetProcAddress (g_hiKasLibrary, "kas_PrincipalGetNext")) == NULL) ||
           ((kas_PrincipalGetDoneP = (kas_PrincipalGetDone_t)GetProcAddress (g_hiKasLibrary, "kas_PrincipalGetDone")) == NULL) ||
           ((kas_PrincipalGetP = (kas_PrincipalGet_t)GetProcAddress (g_hiKasLibrary, "kas_PrincipalGet")) == NULL) )
         {
         status = GetLastError();
         CloseKasLibrary();
         }
      }
   if (g_hiKasLibrary)
      {
      rc = TRUE;
      }

   if (pStatus)
      *pStatus = status;
   return rc;
}


void CloseKasLibrary (void)
{
   if ((--g_cReqKasLibrary) == 0)
      {
      if (g_hiKasLibrary)
         FreeLibrary (g_hiKasLibrary);
      g_hiKasLibrary = NULL;
      }
}


/*
 * CLIENT _____________________________________________________________________
 *
 */

static size_t g_cReqClientLibrary = 0;
static HINSTANCE g_hiClientLibrary = NULL;

typedef int (ADMINAPI *afsclient_Init_t)(afs_status_p st);

afsclient_TokenGetExisting_t afsclient_TokenGetExistingP = NULL;
afsclient_TokenGetNew_t afsclient_TokenGetNewP = NULL;
afsclient_TokenClose_t afsclient_TokenCloseP = NULL;
afsclient_TokenQuery_t afsclient_TokenQueryP = NULL;
afsclient_CellOpen_t afsclient_CellOpenP = NULL;
afsclient_CellClose_t afsclient_CellCloseP = NULL;
afsclient_LocalCellGet_t afsclient_LocalCellGetP = NULL;
afsclient_Init_t afsclient_InitP = NULL;


BOOL OpenClientLibrary (ULONG *pStatus)
{
   BOOL rc = FALSE;
   ULONG status = ERROR_DLL_NOT_FOUND;

   if ((++g_cReqClientLibrary) == 1)
      {
      if ( ((g_hiClientLibrary = LoadLibrary (cszLIBRARY_CLIENT)) == NULL) ||
           ((afsclient_TokenGetExistingP = (afsclient_TokenGetExisting_t)GetProcAddress (g_hiClientLibrary, "afsclient_TokenGetExisting")) == NULL) ||
           ((afsclient_TokenGetNewP = (afsclient_TokenGetNew_t)GetProcAddress (g_hiClientLibrary, "afsclient_TokenGetNew")) == NULL) ||
           ((afsclient_TokenCloseP = (afsclient_TokenClose_t)GetProcAddress (g_hiClientLibrary, "afsclient_TokenClose")) == NULL) ||
           ((afsclient_TokenQueryP = (afsclient_TokenQuery_t)GetProcAddress (g_hiClientLibrary, "afsclient_TokenQuery")) == NULL) ||
           ((afsclient_CellOpenP = (afsclient_CellOpen_t)GetProcAddress (g_hiClientLibrary, "afsclient_CellOpen")) == NULL) ||
           ((afsclient_CellCloseP = (afsclient_CellClose_t)GetProcAddress (g_hiClientLibrary, "afsclient_CellClose")) == NULL) ||
           ((afsclient_LocalCellGetP = (afsclient_LocalCellGet_t)GetProcAddress (g_hiClientLibrary, "afsclient_LocalCellGet")) == NULL) ||
           ((afsclient_InitP = (afsclient_Init_t)GetProcAddress (g_hiClientLibrary, "afsclient_Init")) == NULL) )
         {
         status = GetLastError();
         CloseClientLibrary();
         }
      else if (!(*afsclient_InitP)((afs_status_p)&status))
         {
         CloseClientLibrary();
         }
      }
   if (g_hiClientLibrary)
      {
      rc = TRUE;
      }

   if (pStatus)
      *pStatus = status;
   return rc;
}


void CloseClientLibrary (void)
{
   if ((--g_cReqClientLibrary) == 0)
      {
      if (g_hiClientLibrary)
         FreeLibrary (g_hiClientLibrary);
      g_hiClientLibrary = NULL;
      }
}

