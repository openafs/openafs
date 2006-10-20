/* 

Copyright © 1996 by Project Mandarin, Inc.

Error codes copyright 1996 by Massachusetts Institute of Technology

*/

#include "kcmacerr.h"

#if defined(_WIN32)
/* unfortunately the 32-bit compiler doesn't allow a function */
/* to be declared as __stdcall AND __declspec(dllexport), so  */
/* I have to use a .def file for the export part */
#define KC_CALLTYPE __stdcall
#define KC_EXPORT 
#else
#define KC_CALLTYPE WINAPI
#define KC_EXPORT _export
#endif

BOOL KC_CALLTYPE GetTicketForService (LPSTR, LPSTR, LPDWORD) ;
BOOL KC_CALLTYPE GetTicketGrantingTicket (void) ;
BOOL KC_CALLTYPE DeleteAllSessions (void) ;
BOOL KC_CALLTYPE SetUserName (LPSTR) ;
#if defined(_WIN32)
BOOL KC_CALLTYPE KCGetUserName (LPSTR) ;
#else
BOOL KC_CALLTYPE GetUserName (LPSTR) ;
#endif
BOOL KC_CALLTYPE ListTickets (HWND) ;
void KC_CALLTYPE SetTicketLifeTime (int) ;
void KC_CALLTYPE SetKrbdllMode (BOOL) ;
BOOL KC_CALLTYPE TgtExist (void) ;
#if !defined(KLITE)
BOOL KC_CALLTYPE ChangePassword (void) ;
#endif
// the following two functions will exist ONLY until the other functions
// are revised to return OSErr themselves.  this minimizes the API
// change for this release.
OSErr KC_CALLTYPE KClientErrno(void) ;
signed long KC_CALLTYPE KClientKerberosErrno(void) ;

BOOL KC_CALLTYPE SendTicketForService(LPSTR service, LPSTR version, int fd);

#ifdef _WIN32
DWORD KC_CALLTYPE _KCGetNumInUse();
#endif
