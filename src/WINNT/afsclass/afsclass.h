/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFSCLASS_H
#define AFSCLASS_H

#include <WINNT/afsapplib.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define cchNAME   256

#define GHOST_HAS_VLDB_ENTRY   0x01
#define GHOST_HAS_SERVER_ENTRY 0x02
#define GHOST_HAS_KAS_ENTRY    0x04
#define GHOST_HAS_PTS_ENTRY    0x08
#define GHOST_HAS_ALL_ENTRIES  (GHOST_HAS_VLDB_ENTRY | GHOST_HAS_SERVER_ENTRY)

#define AFSCLASS_WANT_VOLUMES  0x00000001
#define AFSCLASS_WANT_USERS    0x00000002


/*
 * TYPEDEFS ___________________________________________________________________
 *
 */

typedef enum ACCOUNTACCESS
   {
   aaOWNER_ONLY,
   aaGROUP_ONLY,
   aaANYONE,
   } ACCOUNTACCESS;

typedef LPENUMERATION HENUM;

typedef unsigned int VOLUMEID, *LPVOLUMEID;

typedef class NOTIFYCALLBACK NOTIFYCALLBACK, *LPNOTIFYCALLBACK;

typedef class CELL      CELL,      *LPCELL;
typedef class SERVER    SERVER,    *LPSERVER;
typedef class SERVICE   SERVICE,   *LPSERVICE;
typedef class AGGREGATE AGGREGATE, *LPAGGREGATE;
typedef class FILESET   FILESET,   *LPFILESET;
typedef class USER      USER,      *LPUSER;
typedef class PTSGROUP  PTSGROUP,  *LPPTSGROUP;
typedef class IDENT     IDENT,     *LPIDENT;
typedef class IDENTLIST IDENTLIST, *LPIDENTLIST;


/*
 * INCLUSIONS _________________________________________________________________
 *
 */

#include <WINNT/c_debug.h>	// debugging utility functions
#include <WINNT/c_notify.h>	// NOTIFYCALLBACK class
#include <WINNT/c_ident.h>	// IDENT class
#include <WINNT/c_identlist.h>	// IDENTLIST class
#include <WINNT/c_cell.h>	// CELL class
#include <WINNT/c_svr.h>	// SERVER class
#include <WINNT/c_svc.h>	// SERVICE class
#include <WINNT/c_agg.h>	// AGGREGATE class
#include <WINNT/c_set.h>	// FILESET class
#include <WINNT/c_usr.h>	// USER class
#include <WINNT/c_grp.h>	// PTSGROUP class

#include <WINNT/afsclassfn.h>	// AfsClass_* cell-manipulation routines


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL AfsClass_Initialize (ULONG *pStatus = NULL);

void AfsClass_SpecifyRefreshDomain (DWORD dwWant); // AFSCLASS_WANT_*

void AfsClass_Enter (void);
void AfsClass_Leave (void);
int AfsClass_GetEnterCount (void);

void AfsClass_RequestLongServerNames (BOOL fWantLongNames);

void AfsClass_UnixTimeToSystemTime (LPSYSTEMTIME pst, ULONG ut, BOOL fElapsed = FALSE);
ULONG AfsClass_SystemTimeToUnixTime (LPSYSTEMTIME pst);

#ifndef REALLOC
#define REALLOC(_a,_c,_r,_i) AfsClass_ReallocFunction ((LPVOID*)&_a,sizeof(*_a),&_c,_r,_i)
BOOL AfsClass_ReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc);
#endif

void AfsClass_SkipRefresh (int idSection); // ID from evtRefreshAllSectionStart

void AfsClass_IntToAddress (LPSOCKADDR_IN pAddr, int IntAddr);
void AfsClass_AddressToInt (int *pIntAddr, LPSOCKADDR_IN pAddr);


#endif

