/*
 * Copyright (C) 1998  Transarc Corporation.
 * All rights reserved.
 *
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

