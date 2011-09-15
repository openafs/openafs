#pragma once
#include "resource.h"

class PropertyPage
{
public:
    PropertyPage(const CStringArray& filenames);
    virtual ~PropertyPage();

    virtual void SetHwnd(HWND hwnd);
    virtual BOOL PropPageProc(HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd;
    HINSTANCE m_hInst;
    CStringArray filenames;
    BOOL m_bIsSymlink;	        // is symbolic link!
    BOOL m_bIsMountpoint;	// is mount point!
    BOOL m_bIsDir;              // is a directory
};
