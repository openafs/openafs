/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFSCLASS_SERVICE_H
#define AFSCLASS_SERVICE_H

#include <WINNT/afsclass.h>


/*
 * SERVICE CLASS ______________________________________________________________
 *
 */

typedef enum
   {
   SERVICETYPE_SIMPLE,
   SERVICETYPE_FS,
   SERVICETYPE_CRON
   } AFSSERVICETYPE;

typedef enum
   {
   SERVICESTATE_RUNNING,
   SERVICESTATE_STOPPED,
   SERVICESTATE_STARTING,
   SERVICESTATE_STOPPING
   } SERVICESTATE;

typedef struct
   {
   TCHAR szAuxStatus[ cchRESOURCE ];
   TCHAR szParams[ cchRESOURCE ];
   TCHAR szNotifier[ cchRESOURCE ];
   SYSTEMTIME timeLastStart;
   SYSTEMTIME timeLastStop;
   SYSTEMTIME timeLastFail;
   ULONG nStarts;
   ULONG dwErrLast;
   ULONG dwSigLast;
   AFSSERVICETYPE type;
   SERVICESTATE state;
   } SERVICESTATUS, *LPSERVICESTATUS;

typedef struct
   {
#define ENCRYPTIONKEY_LEN 8
   BYTE key[ ENCRYPTIONKEY_LEN ];
   } ENCRYPTIONKEY, *LPENCRYPTIONKEY;

typedef struct
   {
   SYSTEMTIME timeLastModification;
   DWORD dwChecksum;
   } ENCRYPTIONKEYINFO, *LPENCRYPTIONKEYINFO;

class SERVICE
   {
   friend class CELL;
   friend class SERVER;
   friend class IDENT;

   public:
      void Close (void);
      void Invalidate (void);
      BOOL RefreshStatus (BOOL fNotify = TRUE, ULONG *pStatus = NULL);	// does nothing if not invalidated

      // Service properties
      //
      LPIDENT GetIdentifier (void);
      LPCELL OpenCell (ULONG *pStatus = NULL);
      LPSERVER OpenServer (ULONG *pStatus = NULL);
      void GetName (LPTSTR pszName);

      BOOL GetStatus (LPSERVICESTATUS lpss, BOOL fNotify = TRUE, ULONG *pStatus = NULL);

      PVOID GetUserParam (void);
      void SetUserParam (PVOID pUserParam);

   // Private functions
   //
   private:
      SERVICE (LPSERVER lpServerParent, LPTSTR pszName);
      ~SERVICE (void);
      void SendDeleteNotifications (void);

   // Private data
   //
   private:
      LPIDENT m_lpiCell;
      LPIDENT m_lpiServer;
      LPIDENT m_lpiThis;
      TCHAR   m_szName[ cchNAME ];

      BOOL m_fStatusOutOfDate;
      SERVICESTATUS m_ss;
   };


#endif // AFSCLASS_SERVICE_H

