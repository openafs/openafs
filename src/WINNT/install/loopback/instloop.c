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

#include <windows.h>
#include <stdio.h>
#define INITGUID
#include <guiddef.h>
#include <devguid.h>
#include <setupapi.h>
#include <tchar.h>
#include "loopbackutils.h"

#undef REDIRECT_STDOUT

static void
ShowUsage(void)
{
    printf("instloop [-i [name [ip mask]] | -u]\n\n");
    printf("    -i  install the %s\n", DRIVER_DESC);
    _tprintf(_T("        (if unspecified, uses name %s,\n"), DEFAULT_NAME);
    _tprintf(_T("         ip %s, and mask %s)\n"), DEFAULT_IP, DEFAULT_MASK);    
    printf("    -u  uninstall the %s\n", DRIVER_DESC);
}

static void
DisplayStartup(BOOL bInstall)
{
    printf("%snstalling the %s\n"
           " (Note: This may take up to a minute or two...)\n",
           bInstall ? "I" : "Un",
           DRIVER_DESC);
    
}

static void
DisplayResult(BOOL bInstall, DWORD rc)
{
    if (rc)
    {
        printf("Could not %sinstall the %s\n", bInstall ? "" : "un",
               DRIVER_DESC);
        SLEEP;
        PAUSE;
    }
}


int _tmain(int argc, TCHAR *argv[])
{
    DWORD rc = 0;
#ifdef REDIRECT_STDOUT
    FILE *fh = NULL;
#endif

    PAUSE;

#ifdef REDIRECT_STDOUT
    fh = freopen("instlog.txt","a+", stdout);
#endif

    if (argc > 1)
    {
        if (_tcsicmp(argv[1], _T("-i")) == 0)
        {
            TCHAR* name = DEFAULT_NAME;
            TCHAR* ip = DEFAULT_IP;
            TCHAR* mask = DEFAULT_MASK;

            if (argc > 2)
            {
                name = argv[2];
                if (argc > 3)
                {
                    if (argc < 5)
                    {
                        ShowUsage();
#ifdef REDIRECT_STDOUT
                        fflush(fh); fclose(fh);
#endif
                        return 1;
                    }
                    else
                    {
                        ip = argv[3];
                        mask = argv[4];
                    }
                }
            }
            DisplayStartup(TRUE);
			if(IsLoopbackInstalled()) {
				printf("Loopback already installed\n");
				rc = 0; /* don't signal an error. */
			} else {
	            rc = InstallLoopBack(name, ip, mask);
			}
            DisplayResult(TRUE, rc);
#ifdef REDIRECT_STDOUT
            fflush(fh); fclose(fh);
#endif
            return rc;
        }
        else if (_tcsicmp(argv[1], _T("-u")) == 0)
        {
            DisplayStartup(FALSE);
            rc = UnInstallLoopBack();
            DisplayResult(FALSE, rc);
#ifdef REDIRECT_STDOUT
            fflush(fh); fclose(fh);
#endif
            return rc;
        }
        ShowUsage();
#ifdef REDIRECT_STDOUT
        fflush(fh); fclose(fh);
#endif
        return 1;
    }
    ShowUsage();
#ifdef REDIRECT_STDOUT
    fflush(fh); fclose(fh);
#endif
    return 0;
}

