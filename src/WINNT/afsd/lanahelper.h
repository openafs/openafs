#ifndef __LANAHELPER_H__
#define __LANAHELPER_H__

#include <windows.h>
#include <tchar.h>

#ifdef __cplusplus
extern "C" {
#endif

  typedef BYTE lana_number_t;

#define LANA_INVALID 0xff

  int lana_GetNameFromGuid(char *Guid, char **Name);

    struct LANAINFO
    {
        lana_number_t lana_number;
        TCHAR lana_name[MAX_PATH];
    };

#define LANA_INVALID 0xff
#define MAX_NB_NAME_LENGTH 17

#define LANA_NETBIOS_NAME_SUFFIX 1
#define LANA_NETBIOS_NAME_FULL 0

#define LANA_NETBIOS_NAME_IN 2

  int lana_GetNameFromGuid(char *Guid, char **Name);

  struct LANAINFO * lana_FindLanaByName(const char *LanaName);

  lana_number_t lana_FindLoopback(void);

  BOOL lana_IsLoopback(lana_number_t lana);

  long lana_GetUncServerNameEx(char *buffer, lana_number_t * pLana, int * pIsGateway, int flags);

  void lana_GetUncServerNameDynamic(int lanaNumber, BOOL isGateway, TCHAR *name, int type);

  void lana_GetUncServerName(TCHAR *name, int type);

  void lana_GetAfsNameString(int lanaNumber, BOOL isGateway, TCHAR* name);

  void lana_GetNetbiosName(LPTSTR pszName, int type);

#ifdef __cplusplus
}
#endif

#endif

