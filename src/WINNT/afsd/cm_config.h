/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 *
 */
#ifndef __CONFIG_H_ENV_
#define __CONFIG_H_ENV_ 1

#define CM_CONFIGDEFAULT_CACHESIZE	20480
#define CM_CONFIGDEFAULT_BLOCKSIZE	4096
#define CM_CONFIGDEFAULT_STATS		1000
#define CM_CONFIGDEFAULT_CHUNKSIZE	15
#define CM_CONFIGDEFAULT_DAEMONS	2
#define CM_CONFIGDEFAULT_SVTHREADS	4
#define CM_CONFIGDEFAULT_TRACEBUFSIZE	5000

#ifndef __CM_CONFIG_INTERFACES_ONLY__

#include <stdio.h>

extern char AFSConfigKeyName[];

typedef FILE cm_configFile_t;

typedef long (cm_configProc_t)(void *rockp, struct sockaddr_in *addrp, char *namep);

extern long cm_GetRootCellName(char *namep);

extern long cm_SearchCellFile(char *cellNamep, char *newCellNamep,
	cm_configProc_t *procp, void *rockp);

extern long cm_WriteConfigString(char *labelp, char *valuep);

extern long cm_WriteConfigInt(char *labelp, long value);

extern cm_configFile_t *cm_OpenCellFile(void);

extern long cm_AppendPrunedCellList(cm_configFile_t *filep, char *cellNamep);

extern long cm_AppendNewCell(cm_configFile_t *filep, char *cellNamep);

extern long cm_AppendNewCellLine(cm_configFile_t *filep, char *linep);

extern long cm_CloseCellFile(cm_configFile_t *filep);

#endif /* __CM_CONFIG_INTERFACES_ONLY__ */

#endif /* __CONFIG_H_ENV_ */
