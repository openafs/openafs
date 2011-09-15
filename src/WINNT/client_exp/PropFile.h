#pragma once
#include "resource.h"
#include "PropBase.h"

class CPropFile : public PropertyPage
{
public:
    CPropFile(const CStringArray& filenames) : PropertyPage(filenames){}

    virtual BOOL PropPageProc(HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam);

private:
    void ShowUnixMode(const CString& strUserRights, const CString& strGroupRights, const CString& strOtherRights, const CString& strSuidRights);
    void EnablePermChanges(BOOL bEnable);
    void MakeUnixModeString(CString& userRights, CString& groupRights, CString& otherRights, CString& suidRights);

    CString     m_cellName;
    CString     m_volName;
};
