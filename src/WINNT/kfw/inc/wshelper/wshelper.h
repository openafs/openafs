/*
   WSHelper DNS/Hesiod Library for WINSOCK
   wshelper.h
*/

#ifndef _WSHELPER_
#define _WSHELPER_

#include <winsock.h>
#include <mitwhich.h>

#include <resolv.h>
#include <hesiod.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hostent FAR* WINAPI rgethostbyname(char FAR *name);
struct hostent FAR* WINAPI rgethostbyaddr(char FAR *addr, 
                                                          int len, int type);
struct servent FAR* WINAPI rgetservbyname(LPSTR name, 
                                                          LPSTR proto); 

LPSTR WINAPI gethinfobyname(LPSTR name);
LPSTR WINAPI getmxbyname(LPSTR name);
LPSTR WINAPI getrecordbyname(LPSTR name, int rectype);
DWORD WINAPI rrhost( LPSTR lpHost );

unsigned long WINAPI inet_aton(register const char *cp, 
                                               struct in_addr *addr);

DWORD WhichOS( DWORD *check);

#ifdef _WIN32
int WINAPI wsh_gethostname(char* name, int size);
int WINAPI wsh_getdomainname(char* name, int size);
LONG FAR WSHGetHostID();
#endif

/* some definitions to determine which OS were using and which subsystem */
                              
#if !defined( STACK_UNKNOWN )                              
#define STACK_UNKNOWN    -1
#define MS_NT_32          1
#define MS_NT_16          2
#define MS_95_32          3
#define MS_95_16          4
#define NOVELL_LWP_16     5
#endif /* STACK_UNKNOWN */


#ifdef __cplusplus
}
#endif

#endif  /* _WSHELPER_ */

