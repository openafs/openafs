/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef PROPCACHE_H
#define PROPCACHE_H


/*
 * PROPERTIES-DIALOG CACHE ____________________________________________________
 *
 */

typedef enum
   {
   pcSERVER,	// Server window (Open mode only)
   pcSVR_PROP,	// Server's Properties dialog
   pcSVR_LISTS,	// Server's Admin Lists dialog
   pcSVR_CREATE,	// Server's Create dialog
   pcSVR_SYNCVLDB,	// Syncronize VLDB dialog
   pcSVR_INSTALL,	// Install File dialog
   pcSVR_UNINSTALL,	// Uninstall File dialog
   pcSVR_PRUNE,	// Prune Old Files dialog
   pcSVR_GETDATES,	// Get File Dates (non-results) dialog
   pcSVR_EXECUTE,	// Execute Command dialog
   pcSVR_SECURITY,	// Server's Security dialog
   pcSVR_SALVAGE,	// Salvage dialog
   pcSVR_HOSTS,	// Server's Hosts dialog
   pcSVC_PROP,	// Service's Properties dialog
   pcSVC_CREATE,	// Service's CreateService dialog
   pcAGG_PROP,	// Aggregate's Properties dialog
   pcSET_REP,	// Fileset's Replication information
   pcSET_PROP,	// Fileset's Properties dialog
   pcSET_RELEASE,	// Release Fileset dialog
   pcSET_CLONE,	// Clone Fileset dialog
   pcSET_CLONESYS_R,	// Clone Filesets Results dialog
   pcSET_DELETE,	// Delete Fileset dialog
   pcERROR,	// Error dialog
   pcGENERAL,	// Any other dialog
   } PropCache;

#define ANYVALUE (PVOID)-1

void PropCache_Add    (PropCache pcType, PVOID pv, HWND hDialog);
HWND PropCache_Search (PropCache pcType, PVOID pv, HWND hwndSearch = (HWND)NULL);
void PropCache_Delete (PropCache pcType, PVOID pv);
void PropCache_Delete (HWND hDialog);


#endif

