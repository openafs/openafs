#ifndef __LOADFUNCS_AFS36_H__
#define __LOADFUNCS_AFS36_H__

#include "loadfuncs.h"

#define AFSTOKENS_DLL "afsauthent.dll"
#define AFSCONF_DLL   "libafsconf.dll"

#define CALLCONV_C
typedef long cm_configProc_t(void *, struct sockaddr_in *, char *);

TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    ktc_ListTokens,
    (int, int *, struct ktc_principal *)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    ktc_GetToken,
    (struct ktc_principal *, struct ktc_token *, int, struct ktc_principal *)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    ktc_SetToken,
    (struct ktc_principal *, struct ktc_token *, struct ktc_principal *, int)
    );
TYPEDEF_FUNC(
    int,
    CALLCONV_C,
    ktc_ForgetAllTokens,
    ()
    );


TYPEDEF_FUNC(
    long,
    CALLCONV_C,
    cm_SearchCellFile,
    (char *, char *, cm_configProc_t *, void *)
    );
TYPEDEF_FUNC(
    long,
    CALLCONV_C,
    cm_GetRootCellName,
    (char *)
    );

#endif /* __LOADFUNCS_AFS_H__ */
