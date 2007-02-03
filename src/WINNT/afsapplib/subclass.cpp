/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include <windows.h>
#include <WINNT/subclass.h>


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

HRESULT CALLBACK Subclass_WndProc (HWND hTarget, UINT msg, WPARAM wp, LPARAM lp);


/*
 * MISCELLANEOUS ______________________________________________________________
 *
 */

#ifndef REALLOC
#define REALLOC(_a,_c,_r,_i) SubclassReallocFunction ((LPVOID*)&_a,sizeof(*_a),&_c,_r,_i)
BOOL SubclassReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc)
{
   LPVOID pNew;
   size_t cNew;

   if (cReq <= *pcTarget)
      return TRUE;

   if ((cNew = cInc * ((cReq + cInc-1) / cInc)) <= 0)
      return FALSE;

   if ((pNew = (LPVOID)GlobalAlloc (GMEM_FIXED, cbElement * cNew)) == NULL)
      return FALSE;
   memset (pNew, 0x00, cbElement * cNew);

   if (*pcTarget != 0)
      {
      memcpy (pNew, *ppTarget, cbElement * (*pcTarget));
      GlobalFree ((HGLOBAL)*ppTarget);
      }

   *ppTarget = pNew;
   *pcTarget = cNew;
   return TRUE;
}
#endif


/*
 * VARIABLES __________________________________________________________________
 *
 */

typedef struct
   {
   HWND hTarget;	// window being subclassed
   PVOID procOrig;	// original pre-SubclassProc proc

   struct
      {
      PVOID wndProc;
      size_t nReq;
      } *aHooks;

   size_t nHooks;	// number of entries in aHooks
   size_t nHooksActive;	// number of hooks in use
   } SubclassWindow;

SubclassWindow *aTargets = NULL;
size_t nTargets = 0;

#define cREALLOC_SUBCLASS_TARGETS  8
#define cREALLOC_SUBCLASS_HOOKS    4


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL Subclass_AddHook (HWND hTarget, PVOID wndProc)
{
   size_t iTarget;
   for (iTarget = 0; iTarget < nTargets; ++iTarget)
      {
      if (aTargets[ iTarget ].hTarget == hTarget)
         break;
      }
   if (iTarget >= nTargets)
      {
      for (iTarget = 0; iTarget < nTargets; ++iTarget)
         {
         if (aTargets[ iTarget ].hTarget == NULL)
            break;
         }
      }
   if (iTarget >= nTargets)
      {
      if (!REALLOC (aTargets, nTargets, 1+iTarget, cREALLOC_SUBCLASS_TARGETS))
         return FALSE;
      }

   aTargets[ iTarget ].hTarget = hTarget;

   size_t iHook;
   for (iHook = 0; iHook < aTargets[iTarget].nHooks; ++iHook)
      {
      if (aTargets[ iTarget ].aHooks[ iHook ].wndProc == wndProc)
         break;
      }
   if (iHook >= aTargets[ iTarget ].nHooks)
      {
      for (iHook = 0; iHook < aTargets[iTarget].nHooks; ++iHook)
         {
         if (aTargets[ iTarget ].aHooks[ iHook ].wndProc == NULL)
            break;
         }
      }
   if (iHook >= aTargets[ iTarget ].nHooks)
      {
      if (!REALLOC (aTargets[ iTarget ].aHooks, aTargets[ iTarget ].nHooks, 1+iHook, cREALLOC_SUBCLASS_HOOKS))
         return FALSE;
      }

   aTargets[ iTarget ].aHooks[ iHook ].wndProc = wndProc;
   aTargets[ iTarget ].aHooks[ iHook ].nReq ++;
   aTargets[ iTarget ].nHooksActive ++;

   if (aTargets[ iTarget ].nHooksActive == 1)
      {
      aTargets[ iTarget ].procOrig = (PVOID)GetWindowLongPtr (hTarget, GWLP_WNDPROC);
      SetWindowLongPtr (hTarget, GWLP_WNDPROC, PtrToLong(Subclass_WndProc));
      }

   return TRUE;
}


void Subclass_RemoveHook (HWND hTarget, PVOID wndProc)
{
   size_t iTarget;
   for (iTarget = 0; iTarget < nTargets; ++iTarget)
      {
      if (aTargets[ iTarget ].hTarget == hTarget)
         break;
      }
   if (iTarget < nTargets)
      {
      size_t iHook;
      for (iHook = 0; iHook < aTargets[iTarget].nHooks; ++iHook)
         {
         if (aTargets[ iTarget ].aHooks[ iHook ].wndProc == wndProc)
            break;
         }
      if (iHook < aTargets[ iTarget ].nHooks)
         {
         aTargets[ iTarget ].aHooks[ iHook ].nReq --;
         if (aTargets[ iTarget ].aHooks[ iHook ].nReq == 0)
            {
            memset (&aTargets[ iTarget ].aHooks[ iHook ], 0x00, sizeof(aTargets[ iTarget ].aHooks[ iHook ]));
            }
         }

      aTargets[ iTarget ].nHooksActive --;
      if (aTargets[ iTarget ].nHooksActive == 0)
         {
         SetWindowLongPtr (aTargets[ iTarget ].hTarget, GWLP_WNDPROC, (LONG)aTargets[ iTarget ].procOrig);
         memset (&aTargets[ iTarget ], 0x00, sizeof(aTargets[ iTarget ]));
         }
      }
}


PVOID Subclass_FindNextHook (HWND hTarget, PVOID wndProc)
{
   size_t iTarget;
   for (iTarget = 0; iTarget < nTargets; ++iTarget)
      {
      if (aTargets[ iTarget ].hTarget == hTarget)
         break;
      }
   if (iTarget >= nTargets)
      return NULL;

   size_t iHook;
   for (iHook = 0; iHook < aTargets[iTarget].nHooks; ++iHook)
      {
      if (aTargets[ iTarget ].aHooks[ iHook ].wndProc == wndProc)
         break;
      }
   if (iHook >= aTargets[ iTarget ].nHooks)
      return aTargets[ iTarget ].procOrig;

   for (++iHook; iHook < aTargets[iTarget].nHooks; ++iHook)
      {
      if (aTargets[ iTarget ].aHooks[ iHook ].wndProc != NULL)
         return aTargets[ iTarget ].aHooks[ iHook ].wndProc;
      }

   return aTargets[ iTarget ].procOrig;
}


HRESULT CALLBACK Subclass_WndProc (HWND hTarget, UINT msg, WPARAM wp, LPARAM lp)
{
   size_t iTarget;
   for (iTarget = 0; iTarget < nTargets; ++iTarget)
      {
      if (aTargets[ iTarget ].hTarget == hTarget)
         break;
      }
   if (iTarget >= nTargets)
      return DefWindowProc (hTarget, msg, wp, lp);

   size_t iHook;
   for (iHook = 0; iHook < aTargets[iTarget].nHooks; ++iHook)
      {
      if (aTargets[ iTarget ].aHooks[ iHook ].wndProc != NULL)
         break;
      }

   if (iHook >= aTargets[iTarget].nHooks)
      return CallWindowProc ((WNDPROC)aTargets[ iTarget ].procOrig, hTarget, msg, wp, lp);
   else
      return CallWindowProc ((WNDPROC)aTargets[ iTarget ].aHooks[ iHook ].wndProc, hTarget, msg, wp, lp);
}

