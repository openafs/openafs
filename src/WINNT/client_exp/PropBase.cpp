#include "stdafx.h"
#include "PropBase.h"

PropertyPage::PropertyPage(const CStringArray& newFilenames)
{
    filenames.Copy(newFilenames);
}

PropertyPage::~PropertyPage(void)
{
}

void PropertyPage::SetHwnd(HWND newHwnd)
{
    m_hwnd = newHwnd;
}

BOOL PropertyPage::PropPageProc( HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam )
{
    return FALSE;
}
