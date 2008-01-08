/*

Copyright 2004 by the Massachusetts Institute of Technology

All rights reserved.

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of the Massachusetts
Institute of Technology (M.I.T.) not be used in advertising or publicity
pertaining to distribution of the software without specific, written
prior permission.

M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
M.I.T. BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/

#ifndef __LANAHELPER_H__
#define __LANAHELPER_H__

#include <windows.h>
#include <tchar.h>

#ifdef __cplusplus
extern "C" {
#endif

  typedef BYTE lana_number_t;

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

    BOOL lana_OnlyLoopback(void);

    BOOL lana_IsLoopback(lana_number_t lana, BOOL reset);

    long lana_GetUncServerNameEx(char *buffer, lana_number_t * pLana, int * pIsGateway, int flags);

    void lana_GetUncServerNameDynamic(int lanaNumber, BOOL isGateway, TCHAR *name, int type);

    void lana_GetUncServerName(TCHAR *name, int type);

    void lana_GetAfsNameString(int lanaNumber, BOOL isGateway, TCHAR* name);

    void lana_GetNetbiosName(LPTSTR pszName, int type);

#ifdef __cplusplus
}
#endif

#endif

