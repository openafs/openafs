/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * This file implements the bos related funtions for afscp
 */

#include <afs/stds.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <pthread.h>

#include <rx/rx.h>
#include <rx/rxstat.h>

#include <afs/afs_Admin.h>
#include <afs/afs_bosAdmin.h>
#include <afs/afs_utilAdmin.h>
#include <afs/afs_clientAdmin.h>

#include <afs/cellconfig.h>
#include <afs/cmd.h>
#include "common.h"

void
  SetupBosAdminCmd(void);
