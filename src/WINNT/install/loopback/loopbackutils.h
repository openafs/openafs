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

#ifdef  __cplusplus
extern "C" {
#endif
DWORD InstallLoopBack(LPCTSTR pConnectionName, LPCTSTR ip, LPCTSTR mask);
BOOL IsLoopbackInstalled(void);
DWORD UnInstallLoopBack(void);
int RenameConnection(PCWSTR GuidString, PCWSTR pszNewName);
DWORD SetIpAddress(LPCWSTR guid, LPCWSTR ip, LPCWSTR mask);
HRESULT LoopbackBindings (LPCWSTR loopback_guid);
BOOL UpdateHostsFile( LPCWSTR swName, LPCWSTR swIp, LPCSTR szFilename, BOOL bPre );
void ReportMessage(int level, LPCSTR msg, LPCSTR str, LPCWSTR wstr, DWORD dw);
void SetMsiReporter(LPCSTR strAction, LPCSTR strDesc, DWORD h);
#ifdef  __cplusplus
}
#endif

#define DRIVER_DESC "Microsoft Loopback Adapter"
#define DRIVER _T("loopback")
#define DRIVERHWID _T("*msloop")
#define MANUFACTURE _T("microsoft")
#define DEFAULT_NAME _T("AFS")
#define DEFAULT_IP _T("10.254.254.253")
#define DEFAULT_MASK _T("255.255.255.252")

#ifdef USE_PAUSE
#define PAUSE                                          \
    do {                                               \
        char c;                                        \
        printf("PAUSED - PRESS ENTER TO CONTINUE\n");  \
        scanf("%c", &c);                               \
    } while(0)
#else
#define PAUSE
#endif

/*#define USE_SLEEP*/

#ifdef USE_SLEEP
#define SLEEP Sleep(10*1000)
#else
#define SLEEP
#endif

/* Reporting mechanisms */
#define REPORT_PRINTF 1
#define REPORT_MSI 2
#define REPORT_IGNORE 3

extern DWORD dwReporterType;
extern DWORD hMsiHandle;
