/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#if !defined(AFX_SHELLEXT_H__DC515C28_6CAC_11D1_BAE7_00C04FD140D2__INCLUDED_)
#define AFX_SHELLEXT_H__DC515C28_6CAC_11D1_BAE7_00C04FD140D2__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// shell_ext.h : header file
//

#include <shlobj.h>

extern ULONG nCMRefCount;	// IContextMenu ref count
extern ULONG nSERefCount;	// IShellExtInit ref count
extern ULONG nICRefCount;	// IContextMenu ref count
extern ULONG nTPRefCount;	// IQueryInfo ref count
extern ULONG nXPRefCount;	// IPersistFile ref count

#define STR_EXT_TITLE   TEXT("AfsClientContextMenu")
#define STR_REG_PATH    TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers")

/////////////////////////////////////////////////////////////////////////////
// CShellExt command target

class CShellExt : public CCmdTarget
{
	DECLARE_DYNCREATE(CShellExt)

	BOOL m_bDirSelected;
	BOOL m_bIsSymlink;	// is symbolic link!
	TCHAR m_szFile[MAX_PATH];
	BOOL m_bIsOverlayEnabled;
	BOOL IsOverlayEnabled(){return m_bIsOverlayEnabled;}

    CStringArray m_astrFileNames;

	CShellExt();           // protected constructor used by dynamic creation
	LPMALLOC m_pAlloc;

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CShellExt)
	public:
	virtual void OnFinalRelease();
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CShellExt();

	// Generated message map functions
	//{{AFX_MSG(CShellExt)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
	// Generated OLE dispatch map functions
	//{{AFX_DISPATCH(CShellExt)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_DISPATCH
	DECLARE_DISPATCH_MAP()
	
	DECLARE_OLECREATE(CShellExt)
    
	// IFileViewer interface
    BEGIN_INTERFACE_PART(MenuExt, IContextMenu)
        STDMETHOD(QueryContextMenu)( HMENU hmenu,UINT indexMenu,UINT idCmdFirst,
            UINT idCmdLast,UINT uFlags);
        STDMETHOD(InvokeCommand)(LPCMINVOKECOMMANDINFO lpici);
        STDMETHOD(GetCommandString)(UINT_PTR idCmd,UINT uType,UINT* pwReserved,LPSTR pszName,
            UINT cchMax);
    END_INTERFACE_PART(MenuExt)

    // IShellExtInit interface
    BEGIN_INTERFACE_PART(ShellInit, IShellExtInit)
        STDMETHOD(Initialize)(LPCITEMIDLIST pidlFolder,IDataObject *lpdobj, HKEY hkeyProgID);
    END_INTERFACE_PART(ShellInit)

    BEGIN_INTERFACE_PART(IconExt, IShellIconOverlayIdentifier)
        STDMETHOD(GetOverlayInfo)(LPWSTR pwszIconFile,int cchMax,int* pIndex,DWORD* pdwFlags);
        STDMETHOD(GetPriority)(int* pPriority);
        STDMETHOD(IsMemberOf)(LPCWSTR pwszPath,DWORD dwAttrib);
    END_INTERFACE_PART(IconExt)

	BEGIN_INTERFACE_PART(ToolTipExt, IQueryInfo)
		STDMETHOD(GetInfoTip)(DWORD dwFlags, LPWSTR *ppwszTip);
		STDMETHOD(GetInfoFlags)(LPDWORD pdwFlags);
    END_INTERFACE_PART(ToolTipExt)

	BEGIN_INTERFACE_PART(PersistFileExt, IPersistFile)
		STDMETHOD(Load)(LPCOLESTR wszFile, DWORD dwMode);
		STDMETHOD(GetClassID)(LPCLSID);
		STDMETHOD(IsDirty)(VOID);
		STDMETHOD(Save)(LPCOLESTR, BOOL);
		STDMETHOD(SaveCompleted)(LPCOLESTR);
		STDMETHOD(GetCurFile)(LPOLESTR FAR*);
    END_INTERFACE_PART(PersistFileExt)

	DECLARE_INTERFACE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SHELLEXT_H__DC515C28_6CAC_11D1_BAE7_00C04FD140D2__INCLUDED_)
