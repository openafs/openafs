/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef TAAFSADMSVRINTERNAL_H
#define TAAFSADMSVRINTERNAL_H


/*
 * INCLUSIONS _________________________________________________________________
 *
 */

#include <rpc.h>
#include <rpcndr.h>
#include <WINNT/AfsClass.h>
#include <WINNT/TaAfsAdmSvr.h>

#include "TaAfsAdmSvrDebug.h"
#include "TaAfsAdmSvrSearch.h"
#include "TaAfsAdmSvrProperties.h"
#include "TaAfsAdmSvrGeneral.h"
#include "TaAfsAdmSvrCallback.h"

extern "C" {
#include <afs/afs_Admin.h>
#include <afs/afs_kasAdmin.h>
#include <afs/afs_ptsAdmin.h>
#include <afs/afs_vosAdmin.h>
#include <afs/afs_bosAdmin.h>
#include <afs/afs_clientAdmin.h>
#include <afs/afs_utilAdmin.h>
} // extern "C"


#endif // TAAFSADMSVRINTERNAL_H

