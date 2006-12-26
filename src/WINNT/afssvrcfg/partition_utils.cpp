/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * INCLUDES ___________________________________________________________________
 *
 */
#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "afscfg.h"
#include "partition_utils.h"
#include <afs\afs_cfgAdmin.h>


/*
 * DEFINITIONS _________________________________________________________________
 *
 */
static cfg_partitionEntry_t *pTable = 0;	// Partition table
static int cPartitions = 0;					// Count of partitions


/*
 * EXPORTED FUNCTIONS _________________________________________________________
 *
 */
cfg_partitionEntry_t *GetPartitionTable(int &nNumPartitions)
{
    nNumPartitions = cPartitions;

    return pTable;
}

int GetNumPartitions()
{	
    return cPartitions;
}	

int ReadPartitionTable(afs_status_t *pStatus)
{
    ASSERT(g_hServer);

    FreePartitionTable();

    cPartitions = 0;
    pTable = 0;
	
    int nResult = cfg_HostPartitionTableEnumerate(g_hServer, &pTable, &cPartitions, pStatus);

    return nResult;
}

BOOL IsAnAfsPartition(LPCTSTR pszRootDir)
{
    for (int ii = 0; ii < cPartitions; ii++) {

	TCHAR ch1 = pTable[ii].deviceName[0];
	if (_istupper(ch1))
	    ch1 = _totlower(ch1);

	TCHAR ch2 = pszRootDir[0];
	if (_istupper(ch2))
	    ch2 = _totlower(ch2);

	if (ch1 == ch2)
	    return TRUE;
    }	

    return FALSE;
}

BOOL DoesPartitionExist(LPCTSTR pszName)
{
    for (int ii = 0; ii < cPartitions; ii++) {
	if (lstrcmp(A2S(pTable[ii].partitionName), pszName) == 0)
	    return TRUE;
    }	
	
    return FALSE;
}
	
void FreePartitionTable()
{
    if (pTable) {
	afs_status_t nStatus;
	cfg_PartitionListDeallocate(pTable, &nStatus);
    }

    pTable = 0;
    cPartitions = 0;
}	

