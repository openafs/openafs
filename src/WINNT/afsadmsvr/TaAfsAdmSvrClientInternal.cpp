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

#include "TaAfsAdmSvrClientInternal.h"


/*
 * VARIABLES __________________________________________________________________
 *
 */

static struct
   {
   LPCRITICAL_SECTION pcs;
   } l;


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void ADMINAPI asc_Enter (void)
{
   if (!l.pcs)
      {
      l.pcs = New (CRITICAL_SECTION);
      InitializeCriticalSection (l.pcs);
      }
   EnterCriticalSection (l.pcs);
}


void ADMINAPI asc_Leave (void)
{
   LeaveCriticalSection (l.pcs);
}


LPCRITICAL_SECTION ADMINAPI asc_GetCriticalSection (void)
{
   if (!l.pcs)
      {
      l.pcs = New (CRITICAL_SECTION);
      InitializeCriticalSection (l.pcs);
      }
   return l.pcs;
}

