/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * INCLUDES ___________________________________________________________________
 *
 */
extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include <windows.h>
#include <WINNT/talocale.h>
#include "../afsapplib/subclass.h"
#include "animate_icon.h"
#include "resource.h"


/*
 * Definitions ___________________________________________________________________
 *
 */
#define GWD_ANIMATIONFRAME  TEXT("afsapplib/al_misc.cpp - next animation frame")


/*
 * ANIMATION __________________________________________________________________
 *
 */

void AnimateIcon (HWND hIcon, int *piFrameLast)
{
   static HICON hiStop;
   static HICON hiFrame[8];
   static BOOL fLoaded = FALSE;

   if (!fLoaded)
      {
      hiStop     = TaLocale_LoadIcon (IDI_SPINSTOP);
      hiFrame[0] = TaLocale_LoadIcon (IDI_SPIN1);
      hiFrame[1] = TaLocale_LoadIcon (IDI_SPIN2);
      hiFrame[2] = TaLocale_LoadIcon (IDI_SPIN3);
      hiFrame[3] = TaLocale_LoadIcon (IDI_SPIN4);
      hiFrame[4] = TaLocale_LoadIcon (IDI_SPIN5);
      hiFrame[5] = TaLocale_LoadIcon (IDI_SPIN6);
      hiFrame[6] = TaLocale_LoadIcon (IDI_SPIN7);
      hiFrame[7] = TaLocale_LoadIcon (IDI_SPIN8);
      fLoaded = TRUE;
      }

   if (piFrameLast)
      {
      *piFrameLast = (*piFrameLast == 7) ? 0 : (1+*piFrameLast);
      }

   SendMessage (hIcon, STM_SETICON, (WPARAM)((piFrameLast) ? hiFrame[ *piFrameLast ] : hiStop), 0);
}


BOOL CALLBACK AnimationHook (HWND hIcon, UINT msg, WPARAM wp, LPARAM lp)
{
   PVOID oldProc = Subclass_FindNextHook (hIcon, AnimationHook);

   static int iFrame = 0;

   switch (msg)
      {
      case WM_TIMER:
         AnimateIcon (hIcon, &iFrame);
         break;

      case WM_DESTROY:
         Subclass_RemoveHook (hIcon, AnimationHook);
         break;
      }

   if (oldProc)
      return CallWindowProc ((WNDPROC)oldProc, hIcon, msg, wp, lp);
   else
      return DefWindowProc (hIcon, msg, wp, lp);
}

void StartAnimation (HWND hIcon, int fps)
{
   Subclass_AddHook (hIcon, AnimationHook);
   SetTimer (hIcon, 0, 1000/((fps) ? fps : 8), NULL);
   AnimateIcon (hIcon);
}

void StopAnimation (HWND hIcon)
{
   KillTimer (hIcon, 0);
   AnimateIcon (hIcon);
   Subclass_RemoveHook (hIcon, AnimationHook);
}

