/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFSCLASS_IDENTLIST_H
#define AFSCLASS_IDENTLIST_H

#include <WINNT/afsclass.h>


/*
 * IDENTIFIER-LIST CLASS ______________________________________________________
 *
 */

class IDENTLIST
   {
   public:
      IDENTLIST (void);
      ~IDENTLIST (void);

      void Add (LPIDENT lpi);
      void Remove (LPIDENT lpi);
      void RemoveAll (void);
      void CopyFrom (LPIDENTLIST pil);

      size_t GetCount (void);
      BOOL fIsInList (LPIDENT lpi);

      LPIDENT FindFirst (HENUM *phEnum);
      LPIDENT FindNext (HENUM *phEnum);
      void FindClose (HENUM *phEnum);

   private:
      LPHASHLIST m_lIdents;
   };


#endif  // AFSCLASS_IDENTLIST_H

