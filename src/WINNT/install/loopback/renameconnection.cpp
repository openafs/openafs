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

#include <afx.h>
#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#include <shlobj.h>
#include <wtypes.h>

#include "loopbackutils.h"

#define NETSHELL_LIBRARY _T("netshell.dll")

// Use the IShellFolder API to rename the connection.
static HRESULT rename_shellfolder(PCWSTR wGuid, PCWSTR wNewName)
{
    // This is the GUID for the network connections folder. It is constant.
    // {7007ACC7-3202-11D1-AAD2-00805FC1270E}
    const GUID CLSID_NetworkConnections = {
        0x7007ACC7, 0x3202, 0x11D1, {
	    0xAA, 0xD2, 0x00, 0x80, 0x5F, 0xC1, 0x27, 0x0E
	}
    };
    LPITEMIDLIST pidl;
    IShellFolder *pShellFolder;
    IMalloc *pShellMalloc;

    // Build the display name in the form "::{GUID}".
    if (wcslen(wGuid) >= MAX_PATH)
        return E_INVALIDARG;
    WCHAR szAdapterGuid[MAX_PATH + 2];
    swprintf(szAdapterGuid, L"::%ls", wGuid);

    // Initialize COM.
    CoInitialize(NULL);

    // Get the shell allocator.
    HRESULT hr = SHGetMalloc(&pShellMalloc);
    if (SUCCEEDED(hr))
    {
        // Create an instance of the network connections folder.
        hr = CoCreateInstance(CLSID_NetworkConnections, NULL,
			      CLSCTX_INPROC_SERVER, IID_IShellFolder,
			      reinterpret_cast<LPVOID *>(&pShellFolder));
    }
    // Parse the display name.
    if (SUCCEEDED(hr))
    {
        hr = pShellFolder->ParseDisplayName(NULL, NULL, szAdapterGuid, NULL,
					    &pidl, NULL);
    }
    if (SUCCEEDED(hr))
    {
	hr = pShellFolder->SetNameOf(NULL, pidl, wNewName, SHGDN_NORMAL,
				     &pidl);
	pShellMalloc->Free(pidl);
    }
    CoUninitialize();
    return hr;
}

extern "C" int RenameConnection(PCWSTR GuidString, PCWSTR NewName)
{
    typedef HRESULT (WINAPI *lpHrRenameConnection)(const GUID *, PCWSTR);
    lpHrRenameConnection RenameConnectionFunc = NULL;
    HRESULT status;

    // First try the IShellFolder interface, which was unimplemented
    // for the network connections folder before XP.
    status = rename_shellfolder(GuidString, NewName);
    if (status == E_NOTIMPL)
    {
	// The IShellFolder interface is not implemented on this platform.
        // Try the (undocumented) HrRenameConnection API in the netshell
        // library.
	CLSID clsid;
        HINSTANCE hNetShell;
	status = CLSIDFromString((LPOLESTR) GuidString, &clsid);
	if (FAILED(status))
	    return -1;
	hNetShell = LoadLibrary(NETSHELL_LIBRARY);
	if (hNetShell == NULL)
	    return -1;
	RenameConnectionFunc =
	  (lpHrRenameConnection) GetProcAddress(hNetShell,
						"HrRenameConnection");
	if (RenameConnectionFunc == NULL)
	{
	    FreeLibrary(hNetShell);
	    return -1;
	}
	status = RenameConnectionFunc(&clsid, NewName);
	FreeLibrary(hNetShell);
    }
    if (FAILED(status))
	return -1;
    return 0;
}
