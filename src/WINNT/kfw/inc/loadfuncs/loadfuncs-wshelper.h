#ifndef __LOADFUNCS_WSHELPER_H__
#define __LOADFUNCS_WSHELPER_H__

#include <loadfuncs.h>
#include <wshelper.h>

#if defined(_WIN64)
#define WSHELPER_DLL "wshelp64.dll"
#else
#define WSHELPER_DLL "wshelp32.dll"
#endif
#define CALLCONV_C

TYPEDEF_FUNC(
    struct hostent *,
    WINAPI,
    rgethostbyname,
    (char *name)
    );

TYPEDEF_FUNC(
    struct hostent *,
    WINAPI,
    rgethostbyaddr,
    (char* addr, int len, int type)
    );

TYPEDEF_FUNC(
    struct servent,
    WINAPI,
    rgetservbyname,
    (LPSTR name, LPSTR proto)
    );

TYPEDEF_FUNC(
    LPSTR,
    WINAPI,
    gethinfobyname,
    (LPSTR name)
    );

TYPEDEF_FUNC(
    LPSTR,
    WINAPI,
    getmxbyname,
    (LPSTR name)
    );

TYPEDEF_FUNC(
    LPSTR,
    WINAPI,
    getrecordbyname,
    (LPSTR name, int rectype)
    );

TYPEDEF_FUNC(
    DWORD,
    WINAPI,
    rrhost,
    (LPSTR lpHost)
    );

TYPEDEF_FUNC(
    unsigned long,
    WINAPI,
    inet_aton,
    (const char* cp, struct in_addr *addr)
    );

TYPEDEF_FUNC(
    DWORD,
    CALLCONV_C,
    WhichOS,
    (DWORD * check)
    );

TYPEDEF_FUNC(
    int,
    WINAPI,
    wsh_gethostname,
    (char* name, int size)
    );

TYPEDEF_FUNC(
    int,
    WINAPI,
    wsh_getdomainname,
    (char* name, int size)
    );

TYPEDEF_FUNC(
    LONG,
    CALLCONV_C,
    WSHGetHostID,
    ()
    );

TYPEDEF_FUNC(
    int,
    WINAPI,
    res_init,
    ()
    );

TYPEDEF_FUNC(
    void,
    WINAPI,
    res_setopts,
    (long opts)
    );

TYPEDEF_FUNC(
    long,
    WINAPI,
    res_getopts,
    (void)
    );

TYPEDEF_FUNC(
    int,
    WINAPI,
    res_mkquery,
    (int op, const char FAR *dname, 
     int qclass, int type, 
     const char FAR *data, int datalen, 
     const struct rrec FAR *newrr,
     char FAR *buf, int buflen)
    );

TYPEDEF_FUNC(
    int,
    WINAPI,
    res_send,
    (const char FAR *msg, int msglen, 
     char FAR *answer, int anslen)
    );

TYPEDEF_FUNC(
    int,
    WINAPI,
    res_querydomain,
    (const char FAR *name, 
     const char FAR *domain, 
     int qclass, int type, 
     u_char FAR *answer, int anslen)
    );

TYPEDEF_FUNC(
    int,
    WINAPI,
    res_search,
    (const char FAR *name, 
     int qclass, int type, 
     u_char FAR *answer, int anslen)
    );

TYPEDEF_FUNC(
    int,
    WINAPI,
    dn_comp,
    (const u_char FAR *exp_dn, 
     u_char FAR *comp_dn, 
     int length, u_char FAR * FAR *dnptrs, 
     u_char FAR * FAR *lastdnptr)
    );

TYPEDEF_FUNC(
    int,
    WINAPI,
    rdn_expand,
    (const u_char FAR *msg, 
     const u_char FAR *eomorig, 
     const u_char FAR *comp_dn, 
     u_char FAR *exp_dn, 
     int length)
    );

TYPEDEF_FUNC(
    LPSTR,
    WINAPI,
    hes_to_bind,
    (
        LPSTR HesiodName, 
        LPSTR HesiodNameType
        )
    );

TYPEDEF_FUNC(
    LPSTR *,
    WINAPI,
    hes_resolve,
    (
        LPSTR HesiodName,
        LPSTR HesiodNameType
        )
    );

TYPEDEF_FUNC(
    int,
    WINAPI,
    hes_error,
    (
        void
        )
    );

TYPEDEF_FUNC(
    struct hes_postoffice *,
    WINAPI,
    hes_getmailhost,
    (LPSTR user)
    );

TYPEDEF_FUNC(
    struct servent *,
    WINAPI,
    hes_getservbyname,
    (LPSTR name, 
     LPSTR proto)
    );

TYPEDEF_FUNC(
    struct passwd *,
    WINAPI,
    hes_getpwnam,
    (LPSTR nam)
    );

TYPEDEF_FUNC(
    struct passwd *,
    WINAPI,
    hes_getpwuid,
    (int uid)
    );

#endif /* __LOADFUNCS_WSHELPER_H__ */
