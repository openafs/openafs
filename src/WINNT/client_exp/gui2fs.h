/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
 */

#ifndef __GUI2FS_H__
#define __GUI2FS_H__

#include "volume_inf.h"

void Flush(const CStringArray& files);
void FlushVolume(const CStringArray& files);
void WSCellCmd();
void WhichCell(CStringArray& files);
BOOL CheckVolumes();
void SetCacheSize(LONG nNewCacheSize);
void RemoveMountCmd(const CStringArray& files);
void WhereIs(CStringArray& files);
CString GetAfsError(int code, const char *filename = 0);
void CleanACL(CStringArray& names);
BOOL GetRights(const CString& strDir, CStringArray& strNormal, CStringArray& strNegative);
BOOL SaveACL(const CString& strCellName, const CString& strDir, const CStringArray& normal, const CStringArray& negative);
BOOL CopyACL(const CString& strToDir, const CStringArray& normal, const CStringArray& negative, BOOL bClear);
BOOL ListMount(CStringArray& files);
BOOL MakeMount(const CString& strDir, const CString& strVolName, const CString& strCellName, BOOL bRW);
BOOL RemoveMount(CStringArray& files);
BOOL GetVolumeInfo(CString strFile, CVolInfo& volInfo);
BOOL SetVolInfo(CVolInfo& volInfo);
enum WHICH_CELLS { LOCAL_CELL = 0, SPECIFIC_CELL = 1, ALL_CELLS = 2 };
BOOL CheckServers(const CString& strCellName, WHICH_CELLS nCellsToCheck, BOOL bFast);
BOOL GetTokenInfo(CStringArray& tokenInfo);
BOOL IsPathInAfs(const CString& strPath);
int GetCellName(char *baseNamep, struct afsconf_cell *infop);

#endif //__GUI2FS_H__
