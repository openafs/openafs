/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <windows.h>

typedef class HOURGLASS
{
   protected:
      HCURSOR m_OldCursor;

   public:
      HOURGLASS (LPCSTR idCursor = IDC_WAIT)
      {
         m_OldCursor = GetCursor();
         SetCursor (LoadCursor (NULL, idCursor));
      }

      virtual ~HOURGLASS (void)
      {
         SetCursor (m_OldCursor);
      }

} HOURGLASS, *PHOURGLASS;
