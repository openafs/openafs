/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
 */

// shell_ext.cpp : implementation file
//

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "stdafx.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "afs_shl_ext.h"
#include "shell_ext.h"
#include "volume_info.h"
#include "set_afs_acl.h"
#include "gui2fs.h"
#include "make_mount_point_dlg.h"
#include "msgs.h"
#include "server_status_dlg.h"
#include "auth_dlg.h"
#include "submounts_dlg.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


ULONG nCMRefCount = 0;	// IContextMenu ref count
ULONG nSERefCount = 0;	// IShellExtInit ref count


static BOOL IsADir(const CString& strName)
{
	struct _stat statbuf;

	if (_stat(strName, &statbuf) < 0)
		return FALSE;

	return statbuf.st_mode & _S_IFDIR;
}

/////////////////////////////////////////////////////////////////////////////
// CShellExt

IMPLEMENT_DYNCREATE(CShellExt, CCmdTarget)

CShellExt::CShellExt()
{
	EnableAutomation();
	nCMRefCount++;
}

CShellExt::~CShellExt()
{
	nCMRefCount--;
}


void CShellExt::OnFinalRelease()
{
	// When the last reference for an automation object is released
	// OnFinalRelease is called.  The base class will automatically
	// deletes the object.  Add additional cleanup required for your
	// object before calling the base class.

	CCmdTarget::OnFinalRelease();
}


BEGIN_MESSAGE_MAP(CShellExt, CCmdTarget)
	//{{AFX_MSG_MAP(CShellExt)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BEGIN_DISPATCH_MAP(CShellExt, CCmdTarget)
	//{{AFX_DISPATCH_MAP(CShellExt)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_DISPATCH_MAP
END_DISPATCH_MAP()

// Note: we add support for IID_IShellExt to support typesafe binding
//  from VBA.  This IID must match the GUID that is attached to the 
//  dispinterface in the .ODL file.

// {DC515C27-6CAC-11D1-BAE7-00C04FD140D2}
static const IID IID_IShellExt =
{ 0xdc515c27, 0x6cac, 0x11d1, { 0xba, 0xe7, 0x0, 0xc0, 0x4f, 0xd1, 0x40, 0xd2 } };

BEGIN_INTERFACE_MAP(CShellExt, CCmdTarget)
	INTERFACE_PART(CShellExt, IID_IShellExt, Dispatch)
    INTERFACE_PART(CShellExt, IID_IContextMenu, MenuExt)
    INTERFACE_PART(CShellExt, IID_IShellExtInit, ShellInit)
END_INTERFACE_MAP()

IMPLEMENT_OLECREATE(CShellExt, "AfsClientContextMenu", 0xdc515c27, 0x6cac, 0x11d1, 0xba, 0xe7, 0x0, 0xc0, 0x4f, 0xd1, 0x40, 0xd2)


/////////////////////////////////////////////////////////////////////////////
// CShellExt message handlers
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// IUnknown for IContextMenu
/////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CShellExt::XMenuExt::QueryInterface(REFIID riid, void** ppv)
{
    METHOD_PROLOGUE(CShellExt, MenuExt);

    return pThis->ExternalQueryInterface(&riid, ppv);
}

STDMETHODIMP_(ULONG) CShellExt::XMenuExt::AddRef(void)
{
	return ++nCMRefCount;
}

STDMETHODIMP_(ULONG) CShellExt::XMenuExt::Release(void)
{
	if (nCMRefCount > 0)
		nCMRefCount--;

	return nCMRefCount;
}

/////////////////////////////////////////////////////////////////////////////
// IConextMenu Functions
/////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CShellExt::XMenuExt::QueryContextMenu(HMENU hMenu,UINT indexMenu,
    UINT idCmdFirst, UINT idCmdLast,UINT uFlags)
{
    METHOD_PROLOGUE(CShellExt, MenuExt);

	// Don't add any menu items if we're being asked to deal with this file as a shortcut.
	if (uFlags & CMF_VERBSONLY)
		return ResultFromScode(MAKE_SCODE(SEVERITY_SUCCESS, FACILITY_NULL, (USHORT)0));

	// Check to see if there's already an AFS menu here; if so, remove it
	int nItemsNow = GetMenuItemCount (hMenu);
	CString strAfsItemText = GetMessageString(IDS_AFS_ITEM);
	LPCTSTR pszAfsItemText = (LPCTSTR)strAfsItemText;
	for (int iItem = 0; iItem < nItemsNow; iItem++) {
		TCHAR szItemText[256];
		if (!GetMenuString (hMenu, iItem, szItemText, 256, MF_BYPOSITION))
			continue;
		if (!lstrcmp (szItemText, pszAfsItemText)) {
			DeleteMenu (hMenu, iItem, MF_BYPOSITION);
			break;
		}
	}

	int indexShellMenu = 0;

	// Create the AFS submenu using the allowed ID's.
	HMENU hAfsMenu = CreatePopupMenu();
	int indexAfsMenu = 0;

	// The Authentication item has been removed from the AFS menu because
	// there is now a tray icon to handle authentication.
	//
	//::InsertMenu(hAfsMenu, indexAfsMenu++, MF_STRING | MF_BYPOSITION, idCmdFirst + IDM_AUTHENTICATION, GetMessageString(IDS_AUTHENTICATION_ITEM));
	
	// Only enable the ACL menu item if a single directory is selected
	int nSingleDirOnly = MF_GRAYED;
	if (pThis->m_bDirSelected && (pThis->m_astrFileNames.GetSize() == 1))
		nSingleDirOnly = MF_ENABLED;
	::InsertMenu(hAfsMenu, indexAfsMenu++, MF_STRING | MF_BYPOSITION | nSingleDirOnly, idCmdFirst + IDM_ACL_SET, GetMessageString(IDS_ACLS_ITEM));
	
	// Volume/Partition submenu of the AFS submenu
	HMENU hVolPartMenu = CreatePopupMenu();
	int indexVolPartMenu = 0;
	::InsertMenu(hVolPartMenu, indexVolPartMenu++, MF_STRING | MF_BYPOSITION, idCmdFirst + IDM_VOLUME_PROPERTIES, GetMessageString(IDS_VOL_PART_PROPS_ITEM));
	::InsertMenu(hVolPartMenu, indexVolPartMenu++, MF_STRING | MF_BYPOSITION, idCmdFirst + IDM_VOLUMEPARTITION_UPDATENAMEIDTABLE, GetMessageString(IDS_VOL_PART_REFRESH_ITEM));
	::InsertMenu(hAfsMenu, indexAfsMenu++, MF_STRING | MF_BYPOSITION | MF_POPUP, (UINT)hVolPartMenu, GetMessageString(IDS_VOL_PART_ITEM));

	// Mount Point submenu of the AFS submenu
	HMENU hMountPointMenu = CreatePopupMenu();
	int indexMountPointMenu = 0;
	::InsertMenu(hMountPointMenu, indexMountPointMenu++, MF_STRING | MF_BYPOSITION, idCmdFirst + IDM_MOUNTPOINT_SHOW, GetMessageString(IDS_MP_SHOW_ITEM));
	::InsertMenu(hMountPointMenu, indexMountPointMenu++, MF_STRING | MF_BYPOSITION, idCmdFirst + IDM_MOUNTPOINT_REMOVE, GetMessageString(IDS_MP_REMOVE_ITEM));
	::InsertMenu(hMountPointMenu, indexMountPointMenu++, MF_STRING | MF_BYPOSITION, idCmdFirst + IDM_MOUNTPOINT_MAKE, GetMessageString(IDS_MP_MAKE_ITEM));
	::InsertMenu(hAfsMenu, indexAfsMenu++, MF_STRING | MF_BYPOSITION | MF_POPUP, (UINT)hMountPointMenu, GetMessageString(IDS_MOUNT_POINT_ITEM));

	::InsertMenu(hAfsMenu, indexAfsMenu++, MF_STRING | MF_BYPOSITION, idCmdFirst + IDM_FLUSH, GetMessageString(IDS_FLUSH_FILE_DIR_ITEM));	
	::InsertMenu(hAfsMenu, indexAfsMenu++, MF_STRING | MF_BYPOSITION, idCmdFirst + IDM_FLUSH_VOLUME, GetMessageString(IDS_FLUSH_VOLUME_ITEM));
	::InsertMenu(hAfsMenu, indexAfsMenu++, MF_STRING | MF_BYPOSITION, idCmdFirst + IDM_SHOW_SERVER, GetMessageString(IDS_SHOW_FILE_SERVERS_ITEM));
	::InsertMenu(hAfsMenu, indexAfsMenu++, MF_STRING | MF_BYPOSITION, idCmdFirst + IDM_SHOWCELL, GetMessageString(IDS_SHOW_CELL_ITEM));
	::InsertMenu(hAfsMenu, indexAfsMenu++, MF_STRING | MF_BYPOSITION, idCmdFirst + IDM_SERVER_STATUS, GetMessageString(IDS_SHOW_SERVER_STATUS_ITEM));
	
	// The Submounts menu has been removed because the AFS tray icon
	// and control panel now support mapping drives directly to an AFS
	// path.
	//
	// HMENU hSubmountMenu = CreatePopupMenu();
	// int indexSubmountMenu = 0;
	// ::InsertMenu(hSubmountMenu, indexSubmountMenu++, MF_STRING | MF_BYPOSITION | nSingleDirOnly, idCmdFirst + IDM_SUBMOUNTS_CREATE, GetMessageString(IDS_SUBMOUNTS_CREATE_ITEM));
	// ::InsertMenu(hSubmountMenu, indexSubmountMenu++, MF_STRING | MF_BYPOSITION, idCmdFirst + IDM_SUBMOUNTS_EDIT, GetMessageString(IDS_SUBMOUNTS_EDIT_ITEM));
	// ::InsertMenu(hAfsMenu, indexAfsMenu++, MF_STRING | MF_BYPOSITION | MF_POPUP, (UINT)hSubmountMenu, GetMessageString(IDS_SUBMOUNTS_ITEM));

	// Add a separator
	::InsertMenu (hMenu, indexMenu + indexShellMenu++, MF_STRING | MF_BYPOSITION | MF_SEPARATOR, 0, TEXT(""));

	// Add the AFS submenu to the shell's menu
	::InsertMenu(hMenu, indexMenu + indexShellMenu++, MF_STRING | MF_BYPOSITION | MF_POPUP, (UINT)hAfsMenu, GetMessageString(IDS_AFS_ITEM));

	// Add a separator after us
	::InsertMenu (hMenu, indexMenu + indexShellMenu++, MF_STRING | MF_BYPOSITION | MF_SEPARATOR, 0, TEXT(""));
	
    return ResultFromScode(MAKE_SCODE(SEVERITY_SUCCESS, FACILITY_NULL, 
			(USHORT)indexAfsMenu + indexVolPartMenu + indexMountPointMenu + indexShellMenu));
}

STDMETHODIMP CShellExt::XMenuExt::InvokeCommand(LPCMINVOKECOMMANDINFO lpici)
{
    METHOD_PROLOGUE(CShellExt, MenuExt);

    if (HIWORD(lpici->lpVerb ))
        return E_FAIL;

	AddRef();

	CStringArray &files = pThis->m_astrFileNames;

	switch (LOWORD(lpici->lpVerb))
	{
		case IDM_AUTHENTICATION:	{
										CAuthDlg dlg;
										dlg.DoModal();
										break;
									}
		
		case IDM_ACL_SET:			{ 
										CSetAfsAcl dlg;
										ASSERT(files.GetSize() == 1);
										dlg.SetDir(files[0]);
										dlg.DoModal();
									}
									break;
		
		case IDM_VOLUME_PROPERTIES:	{
										CVolumeInfo dlg;
										dlg.SetFiles(files);
										dlg.DoModal();
									}
									break;

		case IDM_VOLUMEPARTITION_UPDATENAMEIDTABLE:	CheckVolumes();
													break;

		case IDM_MOUNTPOINT_SHOW:	ListMount(files);
									break;

		case IDM_MOUNTPOINT_REMOVE:	{
										int nChoice = ShowMessageBox(IDS_REALLY_DEL_MOUNT_POINTS, MB_ICONQUESTION | MB_YESNO, IDS_REALLY_DEL_MOUNT_POINTS);
										if (nChoice == IDYES)
											RemoveMount(files);
									}
									break;
		
		case IDM_MOUNTPOINT_MAKE:	{
										CMakeMountPointDlg dlg;
										ASSERT(files.GetSize() == 1);
										dlg.SetDir(files[0]);
										dlg.DoModal();
									}
									break;


		case IDM_FLUSH:				Flush(files);
									break;
		
		case IDM_FLUSH_VOLUME:		FlushVolume(files);
									break;

		case IDM_SHOW_SERVER:		WhereIs(files);
									break;
		
		case IDM_SHOWCELL:			WhichCell(files);
									break;

		case IDM_SERVER_STATUS:		{
										CServerStatusDlg dlg;
										dlg.DoModal();
									}
									break;
		
		case IDM_SUBMOUNTS_EDIT:	{
										CSubmountsDlg dlg;
										dlg.DoModal();
									}
									break;

		case IDM_SUBMOUNTS_CREATE:	{
										ASSERT(files.GetSize() == 1);
										CSubmountsDlg dlg;
										dlg.SetAddOnlyMode(files[0]);
										dlg.DoModal();
									}
									break;

		default:
			ASSERT(FALSE);
			Release();
		    return E_INVALIDARG;
	}

	Release();

    return NOERROR;
}

STDMETHODIMP CShellExt::XMenuExt::GetCommandString(UINT idCmd, UINT uType,
    UINT* pwReserved, LPSTR pszName, UINT cchMax)
{
	if (uType != GCS_HELPTEXT)
		return NOERROR;			// ?????????????????????????????????????????????????

	UINT nCmdStrID;

    AfxSetResourceHandle(theApp.m_hInstance);

    switch (idCmd)
	{
		case IDM_AUTHENTICATION:	nCmdStrID = ID_AUTHENTICATE;
									break;
		
		case IDM_ACL_SET:			nCmdStrID = ID_ACL_SET;
									break;
		
		case IDM_VOLUME_PROPERTIES:	nCmdStrID = ID_VOLUME_PROPERTIES;
									break;

		case IDM_VOLUMEPARTITION_UPDATENAMEIDTABLE:	nCmdStrID = ID_VOLUMEPARTITION_UPDATENAMEIDTABLE;
													break;

		case IDM_MOUNTPOINT_SHOW:	nCmdStrID = ID_MOUNTPOINT_SHOW;
									break;

		case IDM_MOUNTPOINT_REMOVE:	nCmdStrID = ID_MOUNTPOINT_REMOVE;
									break;
		
		case IDM_MOUNTPOINT_MAKE:	nCmdStrID = ID_MOUNTPOINT_MAKE;
									break;

		case IDM_FLUSH:				nCmdStrID = ID_FLUSH;
									break;
		
		case IDM_FLUSH_VOLUME:		nCmdStrID = ID_VOLUME_FLUSH;
									break;

		case IDM_SHOW_SERVER:		nCmdStrID = ID_WHEREIS;
									break;
		
		case IDM_SHOWCELL:			nCmdStrID = ID_SHOWCELL;
									break;

		case IDM_SERVER_STATUS:		nCmdStrID = ID_SERVER_STATUS;
									break;

		case IDM_SUBMOUNTS_CREATE:	nCmdStrID = ID_SUBMOUNTS_CREATE;
									break;
		
		case IDM_SUBMOUNTS_EDIT:	nCmdStrID = ID_SUBMOUNTS_EDIT;
									break;
		
		default:
			ASSERT(FALSE);
			Release();
		    return E_INVALIDARG;
	}

    CString strMsg;
    LoadString (strMsg, nCmdStrID);

    strncpy(pszName, strMsg, cchMax);

    return NOERROR;
}

/////////////////////////////////////////////////////////////////////////////
// IUnknown for IShellExtInit
/////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CShellExt::XShellInit::QueryInterface(REFIID riid, void** ppv)
{
    METHOD_PROLOGUE(CShellExt, ShellInit);

    return pThis->ExternalQueryInterface(&riid, ppv);
}

STDMETHODIMP_(ULONG) CShellExt::XShellInit::AddRef(void)
{
	return ++nSERefCount;
}

STDMETHODIMP_(ULONG) CShellExt::XShellInit::Release(void)
{
	if (nSERefCount > 0)
		nSERefCount--;
	
	return nSERefCount;
}

/////////////////////////////////////////////////////////////////////////////
// IShellInit Functions
/////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CShellExt::XShellInit::Initialize(LPCITEMIDLIST pidlFolder, IDataObject *pdobj, HKEY hkeyProgID)
{
    METHOD_PROLOGUE(CShellExt, ShellInit);

    HRESULT hres = E_FAIL;
    FORMATETC fmte = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM medium;

	// We must have a data object
    if (pdobj == NULL)
	    return E_FAIL;

    //  Use the given IDataObject to get a list of filenames (CF_HDROP)
    hres = pdobj->GetData(&fmte, &medium);
    if (FAILED(hres))
	    return E_FAIL;

    int nNumFiles = DragQueryFile((HDROP)medium.hGlobal, 0xFFFFFFFF, NULL, 0);
	if (nNumFiles == 0)
		hres = E_FAIL;
	else {
		pThis->m_bDirSelected = FALSE;

		for (int ii = 0; ii < nNumFiles; ii++) {
			CString strFileName;

			// Get the size of the file name string
			int nNameLen = DragQueryFile((HDROP)medium.hGlobal, ii, 0, 0);

			// Make room for it in our string object
			LPTSTR pszFileNameBuf = strFileName.GetBuffer(nNameLen + 1);	// +1 for the terminating NULL
			ASSERT(pszFileNameBuf);
			
			// Get the file name
			DragQueryFile((HDROP)medium.hGlobal, ii, pszFileNameBuf, nNameLen + 1);
			
			strFileName.ReleaseBuffer();

			if (!IsPathInAfs(strFileName)) {
				pThis->m_astrFileNames.RemoveAll();
				break;
			}

			if (IsADir(strFileName))
				pThis->m_bDirSelected = TRUE;

			pThis->m_astrFileNames.Add(strFileName);
		}

		if (pThis->m_astrFileNames.GetSize() > 0)
			hres = S_OK;
		else
			hres = E_FAIL;
    }
 
    //	Release the data
    ReleaseStgMedium(&medium);

    return hres;
}

