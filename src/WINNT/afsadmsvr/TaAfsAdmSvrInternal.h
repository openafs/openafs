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

