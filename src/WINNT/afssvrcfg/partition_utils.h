/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	_PARTITION_UTILS_H_
#define _PARTITION_UTILS_H_

int ReadPartitionTable(afs_status_t *pStatus);
cfg_partitionEntry_t *GetPartitionTable(int &nNumPartitions);
int GetNumPartitions();
BOOL IsAnAfsPartition(LPCTSTR pszRootDir);
BOOL DoesPartitionExist(LPCTSTR pszName);
void FreePartitionTable();

#endif	// _PARTITION_UTILS_H_

