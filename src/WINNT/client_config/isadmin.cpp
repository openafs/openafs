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
#include <WINNT/TaLocale.h>


/*
 * ISWINNT ____________________________________________________________________
 *
 */

BOOL IsWindowsNT (void)
{
   static BOOL fChecked = FALSE;
   static BOOL fIsWinNT = FALSE;

   if (!fChecked)
      {
      fChecked = TRUE;

      OSVERSIONINFO Version;
      memset (&Version, 0x00, sizeof(Version));
      Version.dwOSVersionInfoSize = sizeof(Version);

      if (GetVersionEx (&Version))
         {
         if (Version.dwPlatformId == VER_PLATFORM_WIN32_NT)
            fIsWinNT = TRUE;
         }
      }

   return fIsWinNT;
}


/*
 * ISADMIN ____________________________________________________________________
 *
 */

BOOL IsAdmin (void)
{
   static BOOL fAdmin = FALSE;
   static BOOL fTested = FALSE;
   if (!fTested)
      {
      fTested = TRUE;

      // Obtain the SID for BUILTIN\Administrators. If this is Windows NT,
      // expect this call to succeed; if it does not, we can presume that
      // it's not NT and therefore the user always has administrative
      // privileges.
      //
      PSID psidAdmin = NULL;
      SID_IDENTIFIER_AUTHORITY auth = SECURITY_NT_AUTHORITY;
      if (!AllocateAndInitializeSid (&auth, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &psidAdmin))
         fAdmin = TRUE;
      else
         {

         // Then open our current ProcessToken
         //
         HANDLE hToken;
         if (OpenProcessToken (GetCurrentProcess(), TOKEN_QUERY, &hToken))
            {

            // We'll have to allocate a chunk of memory to store the list of
            // groups to which this user belongs; find out how much memory
            // we'll need.
            //
            DWORD dwSize = 0;
            GetTokenInformation (hToken, TokenGroups, NULL, dwSize, &dwSize);
            
            // Allocate that buffer, and read in the list of groups.
            //
            PTOKEN_GROUPS pGroups = (PTOKEN_GROUPS)Allocate (dwSize);
            if (GetTokenInformation (hToken, TokenGroups, pGroups, dwSize, &dwSize))
               {
               // Look through the list of group SIDs and see if any of them
               // matches the Administrator group SID.
               //
               for (size_t iGroup = 0; (!fAdmin) && (iGroup < pGroups->GroupCount); ++iGroup)
                  {
                  if (EqualSid (psidAdmin, pGroups->Groups[ iGroup ].Sid))
                     fAdmin = TRUE;
                  }
               }

            if (pGroups)
               Free (pGroups);
            }
         }

      if (psidAdmin)
         FreeSid (psidAdmin);
      }

   return fAdmin;
}

