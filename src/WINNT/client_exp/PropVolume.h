#pragma once
#include "resource.h"
#include "PropBase.h"

class CPropVolume : public PropertyPage
{
public:
    CPropVolume(const CStringArray& filenames) : PropertyPage(filenames){}

    virtual BOOL PropPageProc(HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam);

private:
};
