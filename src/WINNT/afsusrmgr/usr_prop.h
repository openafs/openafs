/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef USR_PROP_H
#define USR_PROP_H


/*
 * CONFIGURATION ______________________________________________________________
 *
 */

#define csecTICKETLIFETIME_DEFAULT  (csec1HOUR * 25L)
#define csecTICKETLIFETIME_MAX      (csec1HOUR * 720L)

#define cGROUPQUOTA_MIN          1
#define cGROUPQUOTA_DEFAULT     10
#define cGROUPQUOTA_MAX       9999
#define cGROUPQUOTA_INFINITE     0


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef enum
   {
   uptANY = -1,
   uptGENERAL,
   uptMEMBERSHIP,
   uptADVANCED
   } USERPROPTAB;

#define nUSERPROPTAB_MAX  4

typedef struct
   {
   LPASIDLIST pUserList;	// NULL if creating new user
   BOOL fDeleteMeOnClose;	// TRUE to delete structure when done
   BOOL fShowModal;	// TRUE to block while showing
   HWND hParent;
   BOOL fMachine;	// TRUE to show machine pages instead

   BOOL fApplyGeneral;	// TRUE to apply these fields:
   BOOL fCanChangePw;
   BOOL fCanChangePw_Mixed;
   BOOL fCanReusePw;
   BOOL fCanReusePw_Mixed;
   BOOL fExpires;
   BOOL fExpires_Mixed;
   SYSTEMTIME stExpires;
   LONG cdayPwExpires;
   BOOL fPwExpires_Mixed;
   LONG cFailLock;
   BOOL fFailLock_Mixed;
   LONG csecFailLock;

   BOOL fApplyAdvanced;	// TRUE to apply these fields:
   BOOL fCreateKAS;
   BOOL fCreatePTS;
   BOOL fSeal;
   BOOL fSeal_Mixed;
   BOOL fAdmin;
   BOOL fAdmin_Mixed;
   BOOL fGrantTickets;
   BOOL fGrantTickets_Mixed;
   ULONG csecLifetime;
   LONG cGroupQuota;
   BOOL fGroupQuota_Mixed;
   ACCOUNTACCESS aaStatus;
   BOOL fStatus_Mixed;
   ACCOUNTACCESS aaOwned;
   BOOL fOwned_Mixed;
   ACCOUNTACCESS aaMember;
   BOOL fMember_Mixed;

   LPASIDLIST pGroupsMember;
   LPASIDLIST pGroupsOwner;
   } USERPROPINFO, *LPUSERPROPINFO;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void User_ShowProperties (LPASIDLIST pAsidList, USERPROPTAB uptTarget = uptANY);
void User_ShowProperties (LPUSERPROPINFO lpp, USERPROPTAB uptTarget = uptANY);
void User_FreeProperties (LPUSERPROPINFO lpp);


#endif

