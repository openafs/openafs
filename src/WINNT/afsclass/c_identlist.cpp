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
 * IDENTLIST CLASS ____________________________________________________________
 *
 */

IDENTLIST::IDENTLIST (void)
{
   m_lIdents = New (HASHLIST);
}


IDENTLIST::~IDENTLIST (void)
{
   if (m_lIdents)
      Delete (m_lIdents);
   m_lIdents = NULL;
}


void IDENTLIST::Add (LPIDENT lpi)
{
   m_lIdents->AddUnique (lpi);
}


void IDENTLIST::Remove (LPIDENT lpi)
{
   m_lIdents->Remove (lpi);
}


void IDENTLIST::RemoveAll (void)
{
   LPIDENT lpi;
   while ((lpi = (LPIDENT)m_lIdents->GetFirstObject()) != NULL)
      m_lIdents->Remove (lpi);
}


void IDENTLIST::CopyFrom (LPIDENTLIST pil)
{
   RemoveAll();

   HENUM hEnum;
   for (LPIDENT lpi = pil->FindFirst (&hEnum); lpi; lpi = pil->FindNext (&hEnum))
      Add (lpi);
}


size_t IDENTLIST::GetCount (void)
{
   return m_lIdents->GetCount();
}


BOOL IDENTLIST::fIsInList (LPIDENT lpi)
{
   return m_lIdents->fIsInList (lpi);
}


LPIDENT IDENTLIST::FindFirst (HENUM *phEnum)
{
   LPIDENT lpi = NULL;
   if ((*phEnum = m_lIdents->FindFirst()) != NULL)
      lpi = (LPIDENT)( (*phEnum)->GetObject() );
   return lpi;
}


LPIDENT IDENTLIST::FindNext (HENUM *phEnum)
{
   LPIDENT lpi = NULL;

   if ((*phEnum) && ((*phEnum = (*phEnum)->FindNext()) != NULL))
      lpi = (LPIDENT)( (*phEnum)->GetObject() );

   return lpi;
}


void IDENTLIST::FindClose (HENUM *phEnum)
{
   if (*phEnum)
      {
      Delete (*phEnum);
      *phEnum = NULL;
      }
}

