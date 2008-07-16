/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _AFS_VICED_VICED_PROTOTYPES_H
#define _AFS_VICED_VICED_PROTOTYPES_H

extern int sendBufSize;
afs_int32 sys_error_to_et(afs_int32 in);
void init_sys_error_to_et(void);
  
#ifdef AFS_DEMAND_ATTACH_FS
/*
 * demand attach fs
 * fileserver state serialization
 */
extern int fs_stateSave(void);
extern int fs_stateRestore(void);
#endif /* AFS_DEMAND_ATTACH_FS */


#endif /* _AFS_VICED_VICED_PROTOTYPES_H */
