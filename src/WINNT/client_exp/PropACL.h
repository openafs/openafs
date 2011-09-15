#pragma once
#include "resource.h"
#include "PropBase.h"
#include "add_acl_entry_dlg.h"


class CPropACL : public PropertyPage, public CSetACLInterface
{
public:
    CPropACL(const CStringArray& filenames) : PropertyPage(filenames){}

    virtual BOOL PropPageProc(HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam);

    virtual BOOL IsNameInUse(BOOL bNormal, const CString& strName);

private:
    void ShowRights(const CString& strRights);
    CString MakeRightsString();
    void EnablePermChanges(BOOL bEnable);
    void OnNothingSelected();
    void OnSelection();
    void OnSelChange();
    void FillACLList();
    void OnRemove();
    void OnPermChange();

    CStringArray m_Normal, m_Negative;
};
