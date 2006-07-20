/* copyright (c) 2005
 * the regents of the university of michigan
 * all rights reserved
 * 
 * permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any purpose,
 * so long as the name of the university of michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.  if
 * the above copyright notice or any other identification of the
 * university of michigan is included in any copy of any portion of
 * this software, then the disclaimer below must also be included.
 * 
 * this software is provided as is, without representation from the
 * university of michigan as to its fitness for any purpose, and without
 * warranty by the university of michigan of any kind, either express 
 * or implied, including without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  the regents
 * of the university of michigan shall not be liable for any damages,   
 * including special, indirect, incidental, or consequential damages, 
 * with respect to any claim arising out or in connection with the use
 * of the software, even if it has been or is hereafter advised of the
 * possibility of such damages.
 */

/* versioning history
 * 
 * 03-jun 2005 (eric williams) entered into versioning
 */

/* developer: eric williams,
 *  center for information technology integration,
 *  university of michigan, ann arbor
 *
 * comments: nativeafs@citi.umich.edu
 */

#define IRPMJFUNCDESC		/* pull in text strings of the names of irp codes */
#define RPC_SRV			/* which half of the rpc library to use */

#include <ntifs.h>
#include <ntdddisk.h>
#include <strsafe.h>
#include <ksdebug.h>
#include "ifs_rpc.h"
#include "afsrdr.h"
#include "kif.h"


/*** notes ***/
/* flag pooltag Io(sp)(sp) to check queryinfo requests for buffer overrun */

NTKERNELAPI
NTSTATUS
IoAllocateDriverObjectExtension(
    IN PDRIVER_OBJECT DriverObject,
    IN PVOID ClientIdentificationAddress,
    IN ULONG DriverObjectExtensionSize,
    OUT PVOID *DriverObjectExtension
    );

/*** local structs ***/
/* represents a specific file */
struct afs_fcb
{
    FSRTL_COMMON_FCB_HEADER;		/* needed to interface with cache manager, etc. */
    SECTION_OBJECT_POINTERS sectionPtrs;
    ERESOURCE _resource;		/* pointed to by fcb_header->Resource */
    ERESOURCE _pagingIoResource;
    unsigned long fid;			/* a courtesy from the userland daemon (a good hash function) */
    UCHAR delPending;
    struct afs_ccb *ccb_list;		/* list of open instances */
    SHARE_ACCESS share_access;		/* common access control */
    PFILE_LOCK lock;
};
typedef struct afs_fcb afs_fcb_t;

/* represents an open instance of a file */
struct afs_ccb
{
    struct afs_ccb *next;		/* these are chained as siblings, non-circularly */
    ULONG access;			/* how this instance is opened */
    wchar_t *name;			/* ptr to name buffer */
    UNICODE_STRING str;			/* full name struct, for notification fns */
    FILE_OBJECT *fo;			/* can get parent fcb from this */
    LARGE_INTEGER cookie;		/* on enum ops, where we are */
    wchar_t *filter;			/* on enum ops, what we are filtering on */
    PACCESS_TOKEN token;		/* security data of opening app */
    ULONG attribs;			/* is dir, etc. */
};
typedef struct afs_ccb afs_ccb_t;


/*** globals ***/
/* use wisely -- should be set by each thread in an arbitrary context */
/* here mostly as a development convenience */
DEVICE_OBJECT *RdrDevice, *ComDevice;
struct AfsRdrExtension *rdrExt;
struct ComExtension *comExt;


/*** error and return handling ***/
/* current code is stable without, but these should wrap all operations */
/*#define TRY					try{
#define EXCEPT(x, y)		} except(EXCEPTION_EXECUTE_HANDLER) \
							{ \
							KdBreakPoint(); \
							rpt4(("exception", "occurred")); \
							Irp->IoStatus.Status = x; \
							Irp->IoStatus.Information = y; \
							}*/
#define TRY
#define EXCEPT(x, y)

#define STATUS(st, in)			Irp->IoStatus.Information = (in), Irp->IoStatus.Status = (st)
#define SYNC_FAIL(st)  \
		{	\
		STATUS(st, 0); \
		COMPLETE_NO_BOOST; \
		return st; \
		}
#define SYNC_FAIL2(st, in)  \
		{	\
		STATUS(st, in); \
		COMPLETE_NO_BOOST; \
		return st; \
		}
#define SYNC_RET(st)  \
		{	\
		STATUS(st, 0); \
		COMPLETE; \
		return st; \
		}
#define SYNC_RET2(st, in)  \
		{	\
		STATUS(st, in); \
		COMPLETE; \
		return st; \
		}
#define SYNC_FAIL_RPC_TIMEOUT		SYNC_FAIL(STATUS_NETWORK_BUSY)

#define LOCK_FCB_LIST			FsRtlEnterFileSystem(); ExAcquireFastMutex(&rdrExt->fcbLock);
#define UNLOCK_FCB_LIST			ExReleaseFastMutex(&rdrExt->fcbLock); FsRtlExitFileSystem();

#define LOCK_FCB			ExAcquireResourceExclusiveLite(fcb->Resource, TRUE)
#define SLOCK_FCB			ExAcquireResourceSharedLite(fcb->Resource, TRUE)
#define UNLOCK_FCB			if (fcb) ExReleaseResourceLite(fcb->Resource)

#define LOCK_PAGING_FCB			ExAcquireResourceExclusiveLite(fcb->PagingIoResource, TRUE)
#define SLOCK_PAGING_FCB		ExAcquireResourceSharedLite(fcb->PagingIoResource, TRUE)
#define UNLOCK_PAGING_FCB		if (fcb) ExReleaseResourceLite(fcb->PagingIoResource)

/*** constants ***/
#define AFS_RDR_TAG			(0x73666440)//@dfs//0x482ac230//0x3029a4f2
#define MAX_PATH			700
#define AFS_FS_NAME			L"Andrew File System"

#define COMM_IOCTL			(void*)0x01
#define COMM_DOWNCALL			(void*)0x02
#define COMM_UPCALLHOOK			(void*)0x03

/* flag to use caching kernel service.  needs to handle cache invalidation in more places.  */
#define EXPLICIT_CACHING

/* dates from afs are time_t style -- seconds since 1970 */		/* 11644505691.6384 */
#define AfsTimeToWindowsTime(x)		(((x)+11644505692L)*10000000L)	/*(1970L-1601L)*365.242199*24L*3600L)*/
#define WindowsTimeToAfsTime(x)		((x)/10000000L-11644505692L)	/*(1970L-1601L)*365.242199*24L*3600L)*/
#define IsDeviceFile(fo)		(!fo->FsContext)


/*** auxiliary functions ***/
#define COMPLETE_NO_BOOST		IoCompleteRequest(Irp, IO_NO_INCREMENT)
#define COMPLETE			IoCompleteRequest(Irp, IO_NETWORK_INCREMENT)

afs_fcb_t *FindFcb(FILE_OBJECT *fo)
{
    if (fo)
        return fo->FsContext;
    return NULL;
}

void *AfsFindBuffer(IRP *Irp)
{
    void *outPtr;
    if (Irp->MdlAddress)
    {
        outPtr = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
        if (outPtr)
            return outPtr;
    }
    outPtr = Irp->AssociatedIrp.SystemBuffer;
    return outPtr;
}

/* we do Resource locking because we don't want to figure out when to pull MainResource */
BOOLEAN lazyWriteLock(PVOID context, BOOLEAN wait)
{
    afs_fcb_t *fcb = context;
    ASSERT(fcb);
    return ExAcquireResourceExclusiveLite(fcb->Resource, wait);
}
	
void lazyWriteUnlock(PVOID context)
{
    afs_fcb_t *fcb = context;
    ASSERT(fcb);
    ExReleaseResourceLite(fcb->Resource);
}

BOOLEAN readAheadLock(PVOID context, BOOLEAN wait)
{
    afs_fcb_t *fcb = context;
    ASSERT(fcb);
    return ExAcquireResourceSharedLite(fcb->Resource, wait);
}

void readAheadUnlock(PVOID context)
{
    afs_fcb_t *fcb = context;
    ASSERT(fcb);
    ExReleaseResourceLite(fcb->Resource);
}

afs_fcb_t *find_fcb(ULONG fid)
{
    afs_fcb_t compare, *compare_ptr, **fcbp;

    compare.fid = fid;
    compare_ptr = &compare;
    fcbp = RtlLookupElementGenericTable(&rdrExt->fcbTable, (void*)&compare_ptr);

    if (fcbp)
	return (*fcbp);
    return NULL;
}


/**********************************************************
 * AfsRdrCreate
 * - handle open and create requests
 * - on success
 *  - FsContext of file object points to FCB
 *  - FsContext2 of file object is usermode handle of file instance
 * - beginning failure codes and conditions came from ifstest
 **********************************************************/
NTSTATUS AfsRdrCreate(DEVICE_OBJECT *DeviceObject, IRP *Irp, IO_STACK_LOCATION *IrpSp, afs_fcb_t *_fcb)
{
    afs_fcb_t *pfcb, *fcb, **fcbp;
    afs_ccb_t *ccb, *pccb, *curr_ccb;
    ULONG len;
    LONG x, y;
    wchar_t *str, *ptr;
    ULONG fid, access, granted, disp;
    LARGE_INTEGER size, creation, accesst, change, written, zero, wait;
    ULONG attribs;
    ULONG share;
    CC_FILE_SIZES sizes;
    NTSTATUS status;
    char created;
    PACCESS_TOKEN acc_token;

    /* set rpc security context for current thread */
    acc_token = SeQuerySubjectContextToken(&IrpSp->Parameters.Create.SecurityContext->AccessState->SubjectSecurityContext);
    ASSERT(acc_token);

    wait.QuadPart = -1000;
    while (rpc_set_context(acc_token) != 0)				/* if there are no open thread spots... */
        KeDelayExecutionThread(KernelMode, FALSE, &wait);		/* ...wait */

    /* attempt to open afs volume directly */
    if (IrpSp->FileObject->FileName.Length == 0)
	SYNC_FAIL(STATUS_SHARING_VIOLATION);

    if (IrpSp->FileObject->FileName.Length > MAX_PATH*sizeof(wchar_t))
	SYNC_FAIL(STATUS_OBJECT_NAME_INVALID);

    if (IrpSp->Parameters.Create.FileAttributes & FILE_ATTRIBUTE_TEMPORARY &&
	IrpSp->Parameters.Create.Options & FILE_DIRECTORY_FILE)
	SYNC_FAIL(STATUS_INVALID_PARAMETER);

    /* relative opens cannot start with \ */
    if (IrpSp->FileObject->RelatedFileObject &&
	IrpSp->FileObject->FileName.Length &&
	IrpSp->FileObject->FileName.Buffer[0] == L'\\')
	SYNC_FAIL(STATUS_INVALID_PARAMETER);/*STATUS_OBJECT_PATH_SYNTAX_BAD);*/

    /* a create request can be relative to a prior file.  build filename here */
    pccb = NULL;
    if (IrpSp->FileObject->RelatedFileObject)
	pccb = IrpSp->FileObject->RelatedFileObject->FsContext2;
    len = IrpSp->FileObject->FileName.Length + (pccb?wcslen(pccb->name):0)*sizeof(wchar_t) + 6;
    str = ExAllocatePoolWithTag(NonPagedPool, len, AFS_RDR_TAG);
    RtlZeroMemory(str, len);
    if (pccb)
    {
	StringCbCatN(str, len, IrpSp->FileObject->RelatedFileObject->FileName.Buffer, IrpSp->FileObject->RelatedFileObject->FileName.Length);
	StringCbCat(str, len, L"\\");
    }
    StringCbCatN(str, len, IrpSp->FileObject->FileName.Buffer, IrpSp->FileObject->FileName.Length);

    /* request to open hierarchical parent of specified path */
    /* remove last path component */
    if (IrpSp->Flags & SL_OPEN_TARGET_DIRECTORY)
    {
	y = x = wcslen(str);
	if (x)
            x--, y--;
	for (x; x >= 0; x--)
        {
            if (str[x] == L'\\')
            {
                if (y == x && x != 0)
                    str[x] = L'\0';
                else if (x != 0)
                {
                    str[x] = L'\0';
                    break;
                }
                else
                {
                    str[x+1] = L'\0';
                    break;
                }
            }
        }
    }

    /* first two characters are \%01d . %d is number of path components immediately
     * following that should not be returned in file name queries.
     * EX: \3\mount\path\start\fname.ext is mapped to <driveletter>:\fname.ext 
     */
    if (wcslen(str) <= 2)
    {
	ExFreePoolWithTag(str, AFS_RDR_TAG);
	SYNC_FAIL(STATUS_OBJECT_NAME_INVALID);
    }

    fid = 0;
    disp = (unsigned char)((IrpSp->Parameters.Create.Options >> 24) & 0xff);
    access = IrpSp->Parameters.Create.SecurityContext->DesiredAccess;
    share = IrpSp->Parameters.Create.ShareAccess;
    size.QuadPart = 0;
    created = 0;

  open:
	/* check for file existance.  we jump to creating it if needed */ 
    if (disp == FILE_OPEN || disp == FILE_OPEN_IF || disp == FILE_OVERWRITE ||
         disp == FILE_OVERWRITE_IF || disp == FILE_SUPERSEDE)
    {
	/* chop our internal mount flagging from the beginning */
	status = uc_namei(str+2, &fid);
	if (status == IFSL_DOES_NOT_EXIST &&
             (disp == FILE_OPEN_IF ||
               disp == FILE_OVERWRITE_IF ||
               disp == FILE_SUPERSEDE))
            goto create;

	if (status != IFSL_SUCCESS)
        {
            ExFreePoolWithTag(str, AFS_RDR_TAG);
            switch (status)
            {
            case IFSL_RPC_TIMEOUT:		SYNC_FAIL_RPC_TIMEOUT;
            case IFSL_NO_ACCESS:		SYNC_FAIL(STATUS_ACCESS_DENIED);
            case IFSL_BAD_INPUT:		SYNC_FAIL(STATUS_INVALID_PARAMETER);
            case IFSL_DOES_NOT_EXIST:		SYNC_FAIL2(STATUS_OBJECT_NAME_NOT_FOUND, FILE_DOES_NOT_EXIST);
            case IFSL_PATH_DOES_NOT_EXIST:	SYNC_FAIL2(STATUS_OBJECT_PATH_NOT_FOUND, FILE_DOES_NOT_EXIST);
            default:				SYNC_FAIL(STATUS_UNSUCCESSFUL);
            }
        }

	/* make upcall to get ACL in afs.  we specify our requested level to 
         * keep it from hitting the network on a public volume. 
         */
	status = uc_check_access(fid, access, &granted);
	if (status != IFSL_SUCCESS)
        {
            ExFreePoolWithTag(str, AFS_RDR_TAG);
            switch (status)
            {
            case IFSL_RPC_TIMEOUT:		SYNC_FAIL_RPC_TIMEOUT;
            case IFSL_BAD_INPUT:		SYNC_FAIL(STATUS_INVALID_PARAMETER);
            default:				SYNC_FAIL(STATUS_UNSUCCESSFUL);
            }
        }

	/* make sure daemon approved access */
	if ((access & granted) != access)
        {
            ExFreePoolWithTag(str, AFS_RDR_TAG);
            SYNC_FAIL(STATUS_ACCESS_DENIED);
        }

	/* we depend on this information for caching, etc. */
	status = uc_stat(fid, &attribs, &size, &creation, &accesst, &change, &written);
	if (status != IFSL_SUCCESS)
        {
            ExFreePoolWithTag(str, AFS_RDR_TAG);
            switch (status)
            {
            case IFSL_RPC_TIMEOUT:		SYNC_FAIL_RPC_TIMEOUT;
            case IFSL_BAD_INPUT:		SYNC_FAIL(STATUS_INVALID_PARAMETER);
            default:				SYNC_FAIL(STATUS_UNSUCCESSFUL);
            }
        }

	/* afsd does not maintain instance-specific data, so we adjust here */
	if (granted & FILE_READ_DATA && !(granted & FILE_WRITE_DATA))
            attribs |= FILE_ATTRIBUTE_READONLY;

	if (IrpSp->Parameters.Create.Options & FILE_DIRECTORY_FILE &&
             !(attribs & FILE_ATTRIBUTE_DIRECTORY))
        {
            ExFreePoolWithTag(str, AFS_RDR_TAG);
            SYNC_FAIL(STATUS_NOT_A_DIRECTORY);
        }
	if (IrpSp->Parameters.Create.Options & FILE_NON_DIRECTORY_FILE &&
             attribs & FILE_ATTRIBUTE_DIRECTORY)
        {
            ExFreePoolWithTag(str, AFS_RDR_TAG);
            SYNC_FAIL(STATUS_FILE_IS_A_DIRECTORY);
        }

	/* check for previous open instance(s) of file.  it will be in the fcb chain.
	 * when we have this locked, we cannot make upcalls -- running at APC_LEVEL.
	 * lock here (not later) to prevent possible truncation races. 
	 */
	LOCK_FCB_LIST;
	fcb = find_fcb(fid);
	if (fcb)
        {
            LOCK_PAGING_FCB;
            LOCK_FCB;
        }

	/* if we found it, check to make sure open disposition and sharing
         * are not in conflict with previous opens.  then, make new file
	 * instance entry and, if necessary, file entry. 
         */
	if (fcb)
        {
            /* file contents cannot be cached for a successful delete */
            if (access & FILE_WRITE_DATA)
                if (!MmFlushImageSection(&(fcb->sectionPtrs), MmFlushForWrite))
                {
                    UNLOCK_FCB;
                    UNLOCK_PAGING_FCB;
                    UNLOCK_FCB_LIST;
                    ExFreePoolWithTag(str, AFS_RDR_TAG);
                    SYNC_FAIL(STATUS_SHARING_VIOLATION);
                }
            if (access & DELETE)
                if (!MmFlushImageSection(&(fcb->sectionPtrs), MmFlushForWrite))//Delete))
                {
                    UNLOCK_FCB;
                    UNLOCK_PAGING_FCB;
                    UNLOCK_FCB_LIST;
                    ExFreePoolWithTag(str, AFS_RDR_TAG);
                    SYNC_FAIL(STATUS_SHARING_VIOLATION);
                }

            /* check common sharing flags, but do not change */
            if (IoCheckShareAccess(access, share, IrpSp->FileObject, &fcb->share_access, FALSE) != STATUS_SUCCESS)
            {
                rpt0(("create", "sharing violation for %ws", str));
                UNLOCK_FCB;
                UNLOCK_PAGING_FCB;
                UNLOCK_FCB_LIST;
                ExFreePoolWithTag(str, AFS_RDR_TAG);
                SYNC_FAIL(STATUS_SHARING_VIOLATION);
            }

            zero.QuadPart = 0;
            if (access & DELETE)
                if (!MmCanFileBeTruncated(&(fcb->sectionPtrs), &zero))
                {
                    UNLOCK_FCB;
                    UNLOCK_PAGING_FCB;
                    UNLOCK_FCB_LIST;
                    ExFreePoolWithTag(str, AFS_RDR_TAG);
                    SYNC_FAIL(STATUS_SHARING_VIOLATION);
                }
        }

	/* do overwrite/supersede tasks */
	if (disp == FILE_OVERWRITE ||
             disp == FILE_OVERWRITE_IF ||
             disp == FILE_SUPERSEDE)
        {
            zero.QuadPart = 0;
            if (fcb && !MmCanFileBeTruncated(&(fcb->sectionPtrs), &zero))
            {
                UNLOCK_FCB;
                UNLOCK_PAGING_FCB;
                UNLOCK_FCB_LIST;
                ExFreePoolWithTag(str, AFS_RDR_TAG);
                SYNC_FAIL(STATUS_SHARING_VIOLATION);
            }
            if (size.QuadPart != 0)
            {
                size.QuadPart = 0;
                status = uc_trunc(fid, size);
                if (status != IFSL_SUCCESS)
                {
                    UNLOCK_FCB;
                    UNLOCK_PAGING_FCB;
                    UNLOCK_FCB_LIST;
                    ExFreePoolWithTag(str, AFS_RDR_TAG);
                    SYNC_FAIL(STATUS_ACCESS_DENIED);
                }
                if (fcb)
                {
                    fcb->AllocationSize = fcb->FileSize = fcb->ValidDataLength = size;
		    curr_ccb = fcb->ccb_list;
		    while (curr_ccb && curr_ccb->fo)
			{
			if (CcIsFileCached(curr_ccb->fo))
			    {
			    CcSetFileSizes(curr_ccb->fo, (CC_FILE_SIZES*)&fcb->AllocationSize);
			    break;
			    }
			curr_ccb = curr_ccb->next;
			}
                }
            }
            if (Irp->Overlay.AllocationSize.QuadPart)
            {
                status = uc_trunc(fid, Irp->Overlay.AllocationSize);
                size = Irp->Overlay.AllocationSize;
                if (status != IFSL_SUCCESS)
                {
                    UNLOCK_FCB;
                    UNLOCK_PAGING_FCB;
                    UNLOCK_FCB_LIST;
                    ExFreePoolWithTag(str, AFS_RDR_TAG);
                    SYNC_FAIL(STATUS_DISK_FULL);
                }
                if (fcb)
                {
                    fcb->AllocationSize = fcb->FileSize = fcb->ValidDataLength = size;
		    curr_ccb = fcb->ccb_list;
		    while (curr_ccb && curr_ccb->fo)
			{
			if (CcIsFileCached(curr_ccb->fo))
			    {
			    CcSetFileSizes(curr_ccb->fo, (CC_FILE_SIZES*)&fcb->AllocationSize);
			    break;
			    }
			curr_ccb = curr_ccb->next;
			}
                }
            }
        }

	if (fcb)
        {
            /* make actual change in common sharing flags */
            IoUpdateShareAccess(IrpSp->FileObject, &fcb->share_access);
            goto makeccb;
        }

	goto makefcb;
    }


    /* if disposition was to create the file, or its non-existance necessitates this */
  create:
    if (disp == FILE_CREATE ||
         status == IFSL_DOES_NOT_EXIST && (disp == FILE_OPEN_IF || disp == FILE_OVERWRITE_IF || disp == FILE_SUPERSEDE))
    {       
	attribs = IrpSp->Parameters.Create.FileAttributes;
	if (IrpSp->Parameters.Create.Options & FILE_DIRECTORY_FILE)
            attribs |= FILE_ATTRIBUTE_DIRECTORY;
	if (IrpSp->Parameters.Create.Options & FILE_NON_DIRECTORY_FILE)
            attribs &= ~FILE_ATTRIBUTE_DIRECTORY;
	status = uc_create(str+2, attribs, Irp->Overlay.AllocationSize, access, &granted, &fid);
	if (status != IFSL_SUCCESS)
        {
            ExFreePoolWithTag(str, AFS_RDR_TAG);
            switch (status)
            {
            case IFSL_RPC_TIMEOUT:		SYNC_FAIL_RPC_TIMEOUT;
            case IFSL_NO_ACCESS:		SYNC_FAIL(STATUS_ACCESS_DENIED);
            case IFSL_BAD_INPUT:		SYNC_FAIL(STATUS_INVALID_PARAMETER);
            case IFSL_PATH_DOES_NOT_EXIST:	SYNC_FAIL2(STATUS_OBJECT_PATH_NOT_FOUND, FILE_DOES_NOT_EXIST);
            case IFSL_OPEN_EXISTS:		SYNC_FAIL2(STATUS_OBJECT_NAME_COLLISION, FILE_EXISTS);
            case IFSL_OVERQUOTA:		SYNC_FAIL(STATUS_DISK_FULL);//STATUS_QUOTA_EXCEEDED);
            default:				SYNC_FAIL(STATUS_UNSUCCESSFUL);
            }
        }

	/* lock list like above.  check again just to make sure it isn't in the list.
         * if it is, we should process like above.  depending on how we follow symlinks
         * (same fid for multiple files), we cannot expect serialized create requests. 
         */
	LOCK_FCB_LIST;
	fcb = find_fcb(fid);
	if (fcb)				/* none of these should happen */
		if (disp == FILE_CREATE)
			{
			UNLOCK_FCB_LIST;
			SYNC_FAIL2(STATUS_OBJECT_NAME_COLLISION, FILE_EXISTS);
			}
		else
			{
			fcb = NULL;
			UNLOCK_FCB_LIST;
			goto open;
			}

	if (size.QuadPart != 0)
        {
            size.QuadPart = 0;
            uc_trunc(fid, size);
            if (status != IFSL_SUCCESS)
            {
                UNLOCK_FCB_LIST;
                ExFreePoolWithTag(str, AFS_RDR_TAG);
                SYNC_FAIL(STATUS_DISK_FULL);
            }
        }
	if (Irp->Overlay.AllocationSize.QuadPart)
        {
            uc_trunc(fid, Irp->Overlay.AllocationSize);
            size = Irp->Overlay.AllocationSize;
            if (status != IFSL_SUCCESS)
            {
                UNLOCK_FCB_LIST;
                ExFreePoolWithTag(str, AFS_RDR_TAG);
                SYNC_FAIL(STATUS_ACCESS_DENIED);
            }
        }

	created = 1;
	goto makefcb;
    }

    /* OS passed unexpected disposition */
    ExFreePoolWithTag(str, AFS_RDR_TAG);
    SYNC_FAIL(STATUS_NOT_IMPLEMENTED);


    /* allocate nonpaged struct to track this file and all open instances */
  makefcb:
    fcb = ExAllocateFromNPagedLookasideList(&rdrExt->fcbMemList);
    ASSERT(fcb);
    RtlZeroMemory(fcb, sizeof(afs_fcb_t));

    fcb->fid = fid;
    /*fcb->refs = 0;*/
    fcb->ccb_list = NULL;
    fcb->IsFastIoPossible = FastIoIsPossible;

    ExInitializeResourceLite(&fcb->_resource);
    ExInitializeResourceLite(&fcb->_pagingIoResource);
    fcb->Resource = &fcb->_resource;
    fcb->PagingIoResource = &fcb->_pagingIoResource;
    LOCK_PAGING_FCB;
    LOCK_FCB;

    fcb->sectionPtrs.DataSectionObject = NULL;
    fcb->sectionPtrs.SharedCacheMap = NULL;
    fcb->sectionPtrs.ImageSectionObject = NULL;
    fcb->AllocationSize = fcb->FileSize = fcb->ValidDataLength = size;

    IoSetShareAccess(access, share, IrpSp->FileObject, &fcb->share_access);
    fcb->lock = NULL;

    /* we store a pointer to the fcb.  the table would want to store a copy otherwise. */
    fcbp = &fcb;
    RtlInsertElementGenericTable(&rdrExt->fcbTable, fcbp, sizeof(fcbp), NULL);

    sizes.AllocationSize = sizes.FileSize = sizes.ValidDataLength = size;
    rpt1(("create", "created size %d name %ws", size.LowPart, str));

    /* allocate nonpaged struct tracking information specific to (file, open instance) pair,
     * and link from parent.  is put at head of parent's list. 
     */
  makeccb:
    /* there are two different types of pending deletes.  we return different
     * errors in places depending on a) delete specified on call to open,
     * or b) file specifically deleted 
     */
    /* need to check for DELETE perms too? */
    if (IrpSp->Parameters.Create.Options & FILE_DELETE_ON_CLOSE)
        fcb->delPending = 2;

    ccb = ExAllocateFromNPagedLookasideList(&rdrExt->ccbMemList);
    ccb->name = str;
    ccb->access = granted;
    ccb->fo = IrpSp->FileObject;
    ccb->cookie.QuadPart = 0;
    ccb->filter = NULL;
    ccb->attribs = attribs;

    /* save security context information.  we never change this ccb attribute. */
    ObReferenceObjectByPointer(acc_token, TOKEN_QUERY, NULL, KernelMode);
    ccb->token = acc_token;	
    /*ObOpenObjectByPointer(acc_token, OBJ_KERNEL_HANDLE, NULL, TOKEN_QUERY, NULL, KernelMode, &ccb->token);*/

    if (!fcb->ccb_list)
    {
        ccb->next = NULL;
        fcb->ccb_list = ccb;
    }
    else
    {
        ccb->next = fcb->ccb_list;
        fcb->ccb_list = ccb;
    }
    UNLOCK_FCB;
    UNLOCK_PAGING_FCB;
    UNLOCK_FCB_LIST;

    /* how we pack (what the cache manager needs): a) fscontext same for all open instances
     * of a file, b) fscontext2 unique among each instance, c) private cache map pointer set
     * by cache manager upon caching, d) so pointers are per-file, as in (a). 
     */
    IrpSp->FileObject->FsContext = fcb;
    IrpSp->FileObject->FsContext2 = ccb;
    IrpSp->FileObject->PrivateCacheMap = NULL;
    IrpSp->FileObject->SectionObjectPointer = &(fcb->sectionPtrs);

    /* customize returns; semantics largely derived from output of ifstest.exe */
    switch (disp)
    {
    case FILE_OPEN:
        SYNC_FAIL2(STATUS_SUCCESS, FILE_OPENED);

    case FILE_OPEN_IF:
        if (created) {
            SYNC_FAIL2(STATUS_SUCCESS, FILE_CREATED);
        } else {
            SYNC_FAIL2(STATUS_SUCCESS, FILE_OPENED);
        }
    case FILE_OVERWRITE:
        SYNC_FAIL2(STATUS_SUCCESS, FILE_OVERWRITTEN);

    case FILE_OVERWRITE_IF:
        if (created) {
            SYNC_FAIL2(STATUS_SUCCESS, FILE_CREATED);
        } else {
            SYNC_FAIL2(STATUS_SUCCESS, FILE_OVERWRITTEN);
        }
    case FILE_SUPERSEDE:
        if (created) {
            SYNC_FAIL2(STATUS_SUCCESS, FILE_CREATED);
        } else {
            SYNC_FAIL2(STATUS_SUCCESS, FILE_SUPERSEDED);
        }
    case FILE_CREATE:
        SYNC_FAIL2(STATUS_SUCCESS, FILE_CREATED);
    }
    return 0;
}


/**********************************************************
 * AfsRdrRead
 * - handle reads
 **********************************************************/
NTSTATUS AfsRdrRead(DEVICE_OBJECT *DeviceObject, IRP *Irp, IO_STACK_LOCATION *IrpSp, afs_fcb_t *fcb)
{
    struct ReadKOut *p;
    void *ptr;
    LARGE_INTEGER offset, curroff;
    ULONG length, read;
    void *outPtr;
    NTSTATUS status;
    afs_ccb_t *ccb;
    MDL m;
    ULONG currpos, ttlread, toread;

    ccb = IrpSp->FileObject->FsContext2;
    if (IsDeviceFile(IrpSp->FileObject) || (ccb->attribs & FILE_ATTRIBUTE_DIRECTORY))
	SYNC_FAIL(STATUS_INVALID_DEVICE_REQUEST);

/* the second line enables read ahead and disables write behind.  since our data is
 * already cached in userland, and there are read-ahead parameters there, these are
 * not strictly necessary for decent performance.  also, writes-behind may happen on
 * handles that do not have write access, because the i/o manager picks some handle,
 * not necessarily the same one that writing was originally done with. */
#ifdef EXPLICIT_CACHING
    if (!IrpSp->FileObject->PrivateCacheMap)
    {
        CcInitializeCacheMap(IrpSp->FileObject, (CC_FILE_SIZES*)&fcb->AllocationSize, FALSE, &rdrExt->callbacks, IrpSp->FileObject->FsContext);//(PVOID)din->Context1);
        CcSetAdditionalCacheAttributes(IrpSp->FileObject, FALSE, TRUE);
        /* could do a call to CcSetReadAheadGranularity here */
    }
#endif

    if (!(ccb->access & FILE_READ_DATA) && !(Irp->Flags & IRP_PAGING_IO))
	SYNC_FAIL(STATUS_ACCESS_DENIED);

    if (!IrpSp->Parameters.Read.Length)
	SYNC_FAIL(STATUS_SUCCESS);

    offset = IrpSp->Parameters.Read.ByteOffset;
    length = IrpSp->Parameters.Read.Length;

    FsRtlEnterFileSystem();
    SLOCK_FCB;
    if (!(Irp->Flags & IRP_PAGING_IO) && fcb->lock)
	{
	if (!FsRtlCheckLockForReadAccess(fcb->lock, Irp))
	    {
	    UNLOCK_FCB;
	    FsRtlExitFileSystem();
	    SYNC_FAIL(STATUS_FILE_LOCK_CONFLICT);
	    }
	}

    /* fast out for reads starting beyond eof */
    if (offset.QuadPart > fcb->ValidDataLength.QuadPart)
	{
	UNLOCK_FCB;
	FsRtlExitFileSystem();
	SYNC_FAIL(STATUS_END_OF_FILE);
	}

    /* pre-truncate reads */
    if (offset.QuadPart + length > fcb->ValidDataLength.QuadPart)
	length = (ULONG)(fcb->ValidDataLength.QuadPart - offset.QuadPart);
    UNLOCK_FCB;
    FsRtlExitFileSystem();

    outPtr = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
    if (!outPtr)
	SYNC_FAIL(STATUS_INSUFFICIENT_RESOURCES);

#ifdef EXPLICIT_CACHING
	/* this block is used only when a) caching functionality is compiled in, 
	 * b) this is not a paging i/o request, and c) this is not a no_cache req. */
    if (!(Irp->Flags & (IRP_NOCACHE | IRP_PAGING_IO)))
    {
	FsRtlEnterFileSystem();
	SLOCK_PAGING_FCB;
	ttlread = 0;
	try
        {
            if (!CcCopyRead(IrpSp->FileObject, &offset, length, TRUE, outPtr, &Irp->IoStatus))
            {
                UNLOCK_PAGING_FCB;
                FsRtlExitFileSystem();
                SYNC_FAIL(STATUS_UNSUCCESSFUL);
            }
            ttlread = Irp->IoStatus.Information;
        }
	except (EXCEPTION_EXECUTE_HANDLER)
        {
            STATUS(STATUS_UNSUCCESSFUL, 0);
        }
	/* update byteoffset when this is not a paging or async request */
	if (Irp->IoStatus.Status != STATUS_UNSUCCESSFUL && IoIsOperationSynchronous(Irp) && !(Irp->Flags & IRP_PAGING_IO))
	    IrpSp->FileObject->CurrentByteOffset.QuadPart = offset.QuadPart + ttlread;
	UNLOCK_PAGING_FCB;
	FsRtlExitFileSystem();
	if (Irp->IoStatus.Status == STATUS_UNSUCCESSFUL)
            SYNC_FAIL(STATUS_UNSUCCESSFUL);
    }       
    else    
#endif
    {
	FsRtlEnterFileSystem();
	SLOCK_FCB;
	for (currpos = ttlread = 0; ttlread < length; )
        {
            toread = length - currpos;
            toread = (toread > TRANSFER_CHUNK_SIZE) ? TRANSFER_CHUNK_SIZE : toread;
            curroff.QuadPart = offset.QuadPart + currpos;
            status = uc_read(fcb->fid, curroff, toread, &read, outPtr);

	    if (status == 0)
            {
                ttlread += read;
                currpos += read;
                if (read < toread)
                    goto end;
                continue;
            }
            UNLOCK_FCB;
            FsRtlExitFileSystem();
            switch (status)
            {
            case IFSL_RPC_TIMEOUT:		SYNC_FAIL_RPC_TIMEOUT;
            case IFSL_IS_A_DIR:			SYNC_FAIL(STATUS_INVALID_DEVICE_REQUEST);
            case IFSL_BAD_INPUT:		SYNC_FAIL(STATUS_INVALID_PARAMETER);
            default:				SYNC_FAIL(STATUS_UNSUCCESSFUL);
            }
        }
      end:
	/* update byteoffset when this is not a paging or async request */
	if (IoIsOperationSynchronous(Irp) && !(Irp->Flags & IRP_PAGING_IO))
	    IrpSp->FileObject->CurrentByteOffset.QuadPart = offset.QuadPart + ttlread;
	UNLOCK_FCB;
	FsRtlExitFileSystem();
    }

    if (ttlread == 0) {
	SYNC_FAIL2(STATUS_END_OF_FILE, ttlread);
    } else {
        SYNC_FAIL2(STATUS_SUCCESS, ttlread);
    }
}       



/**********************************************************
 * AfsRdrWrite
 * - handle writes
 **********************************************************/
NTSTATUS AfsRdrWrite(DEVICE_OBJECT *DeviceObject, IRP *Irp, IO_STACK_LOCATION *IrpSp, afs_fcb_t *fcb)
{
    struct WriteKOut *p;
    void *ptr, *outPtr;
    ULONG length, written;
    LARGE_INTEGER offset, end;
    NTSTATUS status;
    afs_ccb_t *ccb;
    CC_FILE_SIZES sizes, oldSizes;
    ULONG towrite, ttlwritten, currpos;
    LARGE_INTEGER curroff;
    BOOLEAN change, paging_lock;

    ccb = IrpSp->FileObject->FsContext2;
    if (IsDeviceFile(IrpSp->FileObject) || (ccb->attribs & FILE_ATTRIBUTE_DIRECTORY))
	SYNC_FAIL(STATUS_INVALID_DEVICE_REQUEST);

    /* since we will be performing io on this instance, start caching (see above). */
#ifdef EXPLICIT_CACHING
    if (!IrpSp->FileObject->PrivateCacheMap)
    {
	CcInitializeCacheMap(IrpSp->FileObject, (CC_FILE_SIZES*)&fcb->AllocationSize, FALSE, &rdrExt->callbacks, IrpSp->FileObject->FsContext);//(PVOID)din->Context1);
	CcSetAdditionalCacheAttributes(IrpSp->FileObject, FALSE, TRUE);
    }
#endif

    if (!(ccb->access & FILE_WRITE_DATA) && !(Irp->Flags & IRP_PAGING_IO))
	SYNC_FAIL(STATUS_ACCESS_DENIED);

    /* fast-out for zero-length i/o */
    if (!IrpSp->Parameters.Write.Length)
	SYNC_FAIL(STATUS_SUCCESS);

    if (!(outPtr = AfsFindBuffer(Irp)))
	SYNC_FAIL(STATUS_INSUFFICIENT_RESOURCES);

    FsRtlEnterFileSystem();
    SLOCK_FCB;

    if (!(Irp->Flags & IRP_PAGING_IO) && fcb->lock)
	{
	if (!FsRtlCheckLockForWriteAccess(fcb->lock, Irp))
	    {
	    UNLOCK_FCB;
	    FsRtlExitFileSystem();
	    SYNC_FAIL(STATUS_FILE_LOCK_CONFLICT);
	    }
	}

    if (IrpSp->Parameters.Write.ByteOffset.HighPart == 0xffffffff &&
         IrpSp->Parameters.Write.ByteOffset.LowPart == FILE_WRITE_TO_END_OF_FILE)
	offset.QuadPart = fcb->FileSize.QuadPart;
    else
	offset = IrpSp->Parameters.Write.ByteOffset;
    length = IrpSp->Parameters.Write.Length;
    UNLOCK_FCB;

#ifdef EXPLICIT_CACHING
    if (!(Irp->Flags & (IRP_NOCACHE | IRP_PAGING_IO)))
    {
	/* extend file for cached writes */
	LOCK_PAGING_FCB;
	if (offset.QuadPart + length > fcb->FileSize.QuadPart)
        {
            end.QuadPart = offset.QuadPart + length;
            fcb->AllocationSize = fcb->FileSize = fcb->ValidDataLength = end;
            LOCK_FCB;
            CcSetFileSizes(IrpSp->FileObject, (CC_FILE_SIZES*)&fcb->AllocationSize);
            UNLOCK_FCB;
            /*ret = *//*CcZeroData(IrpSp->FileObject, &oldSizes.FileSize, &end, FALSE);*/ /* should wait? */
        }
	try
        {
            if (!CcCopyWrite(IrpSp->FileObject, &offset, length, TRUE, outPtr))
		{
		UNLOCK_PAGING_FCB;
		FsRtlExitFileSystem();
                SYNC_FAIL(STATUS_UNSUCCESSFUL);
		}
        }
	except (EXCEPTION_EXECUTE_HANDLER)
        {
            STATUS(STATUS_UNSUCCESSFUL, 0);
        }

	ttlwritten = written = length;
	if (Irp->IoStatus.Status != STATUS_UNSUCCESSFUL && IoIsOperationSynchronous(Irp) && !(Irp->Flags & IRP_PAGING_IO))
	    IrpSp->FileObject->CurrentByteOffset.QuadPart = offset.QuadPart + ttlwritten;
	UNLOCK_PAGING_FCB;
	FsRtlExitFileSystem();
	if (Irp->IoStatus.Status == STATUS_UNSUCCESSFUL)
            SYNC_FAIL(STATUS_UNSUCCESSFUL);
    }
    else
#endif
    {
	while (1)
        {
            if (offset.QuadPart + length > fcb->FileSize.QuadPart)
                change = 1;
            else
                change = 0;
            if (change && !(Irp->Flags & IRP_PAGING_IO))
                paging_lock = 1;
            else
                paging_lock = 0;
            if (paging_lock)
                LOCK_PAGING_FCB;
            LOCK_FCB;
            if (change)
            {
                if (Irp->Flags & IRP_PAGING_IO)
                {
                    /* the input buffer and length is for a full page.  ignore this. */
                    if (offset.QuadPart > fcb->FileSize.QuadPart)
                    {
			/* paging lock cannot be held here, so no need to release */
                        UNLOCK_FCB;
                        FsRtlExitFileSystem();
                        SYNC_FAIL2(STATUS_SUCCESS, length);
                    }
                    length = (ULONG)(fcb->FileSize.QuadPart - offset.QuadPart);
                    break;
                }
                else
                {
                    if (!change)
                    {
			/* paging lock cannot be held here, so no need to release */
                        UNLOCK_FCB;
                        continue;
                    }
                    end.QuadPart = offset.QuadPart + length;
                    fcb->AllocationSize = fcb->FileSize = fcb->ValidDataLength = end;
                    CcSetFileSizes(IrpSp->FileObject, (CC_FILE_SIZES*)&fcb->AllocationSize);
                    if (paging_lock)
                        UNLOCK_PAGING_FCB;
                    break;
                }
            }
            else
            {
                if (paging_lock)
                    UNLOCK_PAGING_FCB;
                break;
            }
        }

	for (currpos = ttlwritten = 0; ttlwritten < length; )
        {
            towrite = length - currpos;
            towrite = (towrite > TRANSFER_CHUNK_SIZE) ? TRANSFER_CHUNK_SIZE : towrite;
            curroff.QuadPart = offset.QuadPart + currpos;
            status = uc_write(fcb->fid, curroff, towrite, &written, outPtr);
            if (status == 0)
            {
                ttlwritten += written;
                currpos += written;
                if (written < towrite)
                    goto end;
                continue;
            }
            UNLOCK_FCB;
            FsRtlExitFileSystem();
            switch (status)
            {
            case IFSL_RPC_TIMEOUT:		SYNC_FAIL_RPC_TIMEOUT;
            case IFSL_BAD_INPUT:		SYNC_FAIL(STATUS_INVALID_PARAMETER);
            case IFSL_OVERQUOTA:		SYNC_FAIL(STATUS_DISK_FULL);
            default:				SYNC_FAIL(STATUS_UNSUCCESSFUL);
            }
        }
      end:
	if (IoIsOperationSynchronous(Irp) && !(Irp->Flags & IRP_PAGING_IO))
	    IrpSp->FileObject->CurrentByteOffset.QuadPart = offset.QuadPart + ttlwritten;
	UNLOCK_FCB;
	FsRtlExitFileSystem();
    }

    /* if we failed a write, we would not be here.  so, we must
     * tell the vmm that all data was written. */
    if (Irp->Flags & IRP_PAGING_IO)
	SYNC_FAIL2(STATUS_SUCCESS, IrpSp->Parameters.Write.Length);
    SYNC_FAIL2(STATUS_SUCCESS, ttlwritten);
}


static wchar_t *SEARCH_MATCH_ALL = L"**";

/**********************************************************
 * AfsRdrDirCtrl
 * - handle directory notification callbacks
 * - handle directory enumeration
 *
 * TOD: This code must performing globbing - it does not currently do so.
 **********************************************************/
NTSTATUS AfsRdrDirCtrl(DEVICE_OBJECT *DeviceObject, IRP *Irp, IO_STACK_LOCATION *IrpSp, afs_fcb_t *fcb)
{
    struct EnumDirKOut *p;
    LARGE_INTEGER size, creation, access, change, written;
    ULONG attribs, count;
    char buf[2048];
    afs_ccb_t *ccb;
    void *outPtr, *info, *pre_adj_info;
    FILE_BOTH_DIR_INFORMATION *info_both, *info_both_prev;
    FILE_DIRECTORY_INFORMATION *info_dir, *info_dir_prev;
    FILE_NAMES_INFORMATION *info_names, *info_names_prev;
    int info_size;
    readdir_data_t *ii;
    ULONG x, buf_size;
    LARGE_INTEGER last_cookie;
    BOOLEAN overflow;
    NTSTATUS status;
    FILE_NOTIFY_INFORMATION *notify;

    if (IsDeviceFile(IrpSp->FileObject))
	SYNC_FAIL(STATUS_INVALID_DEVICE_REQUEST);

    if (IrpSp->MinorFunction == IRP_MN_NOTIFY_CHANGE_DIRECTORY)
    {
	ccb = IrpSp->FileObject->FsContext2;
	ccb->str.Length =			wcslen(ccb->name)*sizeof(wchar_t);
	ccb->str.MaximumLength =	ccb->str.Length + sizeof(wchar_t);
	ccb->str.Buffer =			ccb->name;

	FsRtlEnterFileSystem();
	LOCK_FCB;
	FsRtlNotifyFullChangeDirectory(rdrExt->notifyList, &rdrExt->listHead,
                                        ccb,
                                        (STRING*)&ccb->str,
                                        IrpSp->Flags & SL_WATCH_TREE,
                                        FALSE,
                                        /*FILE_NOTIFY_CHANGE_FILE_NAME,*/IrpSp->Parameters.NotifyDirectory.CompletionFilter,
                                        Irp, NULL, NULL);
	UNLOCK_FCB;     
	FsRtlExitFileSystem();
	/* do NOT complete request; that will be done by fsrtlnotify functions */
	return STATUS_PENDING;
    }

    if (IrpSp->MinorFunction != IRP_MN_QUERY_DIRECTORY)
	SYNC_FAIL(STATUS_NOT_IMPLEMENTED);

    if ( IrpSp->Parameters.QueryDirectory.FileInformationClass != FileBothDirectoryInformation &&
         IrpSp->Parameters.QueryDirectory.FileInformationClass != FileDirectoryInformation &&
         IrpSp->Parameters.QueryDirectory.FileInformationClass != FileNamesInformation)
    {
	rpt0(("enum", "enum class %d not supported", IrpSp->Parameters.QueryDirectory.FileInformationClass));
	SYNC_FAIL(STATUS_NOT_IMPLEMENTED);
    }

    ccb = IrpSp->FileObject->FsContext2;
 
    if (!(outPtr = AfsFindBuffer(Irp)))
	SYNC_FAIL(STATUS_INSUFFICIENT_RESOURCES);

    if (IrpSp->Flags & SL_INDEX_SPECIFIED)
    {
	/* we were told where to start our search; afsd should scrub this input */
	ccb->cookie.QuadPart = IrpSp->Parameters.QueryDirectory.FileIndex;
	if (ccb->filter && ccb->filter != SEARCH_MATCH_ALL)
            ExFreePoolWithTag(ccb->filter, AFS_RDR_TAG);
	ccb->filter = SEARCH_MATCH_ALL;
    }
    else if (IrpSp->Flags & SL_RESTART_SCAN ||
              ((IrpSp->Flags & SL_RETURN_SINGLE_ENTRY) && ccb->cookie.QuadPart == 0))
    {
	/* copy new filter string into nonpaged memory */
	ccb->cookie.QuadPart = 0;
	if (ccb->filter && ccb->filter != SEARCH_MATCH_ALL)
            ExFreePoolWithTag(ccb->filter, AFS_RDR_TAG);
	if (IrpSp->Parameters.QueryDirectory.FileName)
	    {
	    buf_size = IrpSp->Parameters.QueryDirectory.FileName->Length+6;
	    ccb->filter = ExAllocatePoolWithTag(NonPagedPool, buf_size, AFS_RDR_TAG);
	    RtlCopyMemory(ccb->filter, IrpSp->Parameters.QueryDirectory.FileName->Buffer, buf_size);
	    ccb->filter[IrpSp->Parameters.QueryDirectory.FileName->Length / sizeof(WCHAR)] = L'\0';
	    }
	else
	    ccb->filter = SEARCH_MATCH_ALL;
    }

    if (IrpSp->Flags & SL_RETURN_SINGLE_ENTRY)
	count = 1;
    buf_size = 2040;

    status = uc_readdir(fcb->fid, ccb->cookie, ccb->filter, &count, buf, &buf_size);

    switch (status)
    {
    case IFSL_RPC_TIMEOUT:		SYNC_FAIL_RPC_TIMEOUT;
    case IFSL_BAD_INPUT:		SYNC_FAIL(STATUS_INVALID_PARAMETER);
    default:				SYNC_FAIL(STATUS_UNSUCCESSFUL);
    case 0: break;
    }

    switch (IrpSp->Parameters.QueryDirectory.FileInformationClass)
    {
    case FileBothDirectoryInformation:
        info_size = sizeof(FILE_BOTH_DIR_INFORMATION);
        break;
    case FileDirectoryInformation:
        info_size = sizeof(FILE_DIRECTORY_INFORMATION);
        break;
    case FileNamesInformation:
        info_size = sizeof(FILE_NAMES_INFORMATION);
        break;
    default:
        SYNC_FAIL(STATUS_NOT_IMPLEMENTED);
    }

    info = (FILE_BOTH_DIR_INFORMATION *)outPtr;
    ii = (readdir_data_t *)buf;
    Irp->IoStatus.Information = 0;
    if (!count)
    {
	if (IrpSp->Flags & SL_RETURN_SINGLE_ENTRY && ccb->cookie.QuadPart == 0)
            SYNC_FAIL(STATUS_NO_SUCH_FILE);
	SYNC_FAIL(STATUS_NO_MORE_FILES);
    }

    info_both_prev = NULL;
    info_dir_prev = NULL;
    info_names_prev = NULL;
    if (IrpSp->Flags & SL_RETURN_SINGLE_ENTRY)
 	count = 1;

    for (x = 0; x < count; x++)
    {
	pre_adj_info = info;

	/* must explicitly align second and subsequent entries on 8-byte boundaries */
	if (((ULONG)info & 0x7) && x)
            info = (void*)(((ULONG)info) + (8-((ULONG)info & 0x7)));

	overflow = ((long)info + info_size + (long)ii->name_length >
                     (long)outPtr + (long)IrpSp->Parameters.QueryDirectory.Length);
	if (overflow && x != 0)
        {
            ccb->cookie = ii->cookie;
            SYNC_FAIL2(STATUS_SUCCESS, (long)pre_adj_info - (long)outPtr);
        }

	memset(info, 0, info_size);
	switch (IrpSp->Parameters.QueryDirectory.FileInformationClass)
        {
        case FileBothDirectoryInformation:
            info_both = info;
            if (info_both_prev)
                info_both_prev->NextEntryOffset = (char*)info_both - (char*)info_both_prev;

            info_both->NextEntryOffset = 0;
            info_both->FileIndex = (ULONG)ii->cookie.QuadPart;
            info_both->CreationTime.QuadPart = AfsTimeToWindowsTime(ii->creation.QuadPart);
            info_both->LastAccessTime.QuadPart = AfsTimeToWindowsTime(ii->access.QuadPart);
            info_both->LastWriteTime.QuadPart = AfsTimeToWindowsTime(ii->write.QuadPart);
            info_both->ChangeTime.QuadPart = AfsTimeToWindowsTime(ii->change.QuadPart);
            info_both->EndOfFile = info_both->AllocationSize = ii->size;
            info_both->FileAttributes = ii->attribs;/*| FILE_ATTRIBUTE_READONLY*/;
            info_both->EaSize = 0;
            info_both->ShortNameLength = ii->short_name_length;
            info_both->FileNameLength = ii->name_length;
            RtlCopyMemory(info_both->ShortName, ii->short_name, ii->short_name_length*sizeof(WCHAR));

            if (overflow)
            {
                Irp->IoStatus.Status = STATUS_BUFFER_OVERFLOW;
                info_both->FileNameLength = 0;
                info_both->FileName[0] = L'\0';
                goto done;
            }

            RtlCopyMemory(info_both->FileName, ii->name, ii->name_length*sizeof(WCHAR));
            info_both_prev = info_both;
            break;

        case FileDirectoryInformation:
            info_dir = info;
            if (info_dir_prev)
                info_dir_prev->NextEntryOffset = (char*)info_dir - (char*)info_dir_prev;

            info_dir->NextEntryOffset = 0;
            info_dir->FileIndex = (ULONG)ii->cookie.QuadPart;
            info_dir->CreationTime.QuadPart = AfsTimeToWindowsTime(ii->creation.QuadPart);
            info_dir->LastAccessTime.QuadPart = AfsTimeToWindowsTime(ii->access.QuadPart);
            info_dir->LastWriteTime.QuadPart = AfsTimeToWindowsTime(ii->write.QuadPart);
            info_dir->ChangeTime.QuadPart = AfsTimeToWindowsTime(ii->change.QuadPart);
            info_dir->EndOfFile = info_dir->AllocationSize = ii->size;
            info_dir->FileAttributes = ii->attribs /*| FILE_ATTRIBUTE_READONLY*/;
            info_dir->FileNameLength = ii->name_length;

            if (overflow)
            {
                Irp->IoStatus.Status = STATUS_BUFFER_OVERFLOW;
                info_dir->FileNameLength = 0;
                info_dir->FileName[0] = L'\0';
                goto done;
            }

            RtlCopyMemory(info_dir->FileName, ii->name, ii->name_length*sizeof(WCHAR));
            info_dir_prev = info_dir;
            break;

        case FileNamesInformation:
            info_names = info;
            if (info_names_prev)
                info_names_prev->NextEntryOffset = (char*)info_names - (char*)info_names_prev;

            info_names->NextEntryOffset = 0;
            info_names->FileIndex = (ULONG)ii->cookie.QuadPart;
            info_names->FileNameLength = ii->name_length;

            if (overflow)
            {
                Irp->IoStatus.Status = STATUS_BUFFER_OVERFLOW;
                info_names->FileNameLength = 0;
                info_names->FileName[0] = L'\0';
                goto done;
            }

            RtlCopyMemory(info_names->FileName, ii->name, ii->name_length*sizeof(WCHAR));
            info_names_prev = info_names;
            break;
        }

	info = (void*)(((ULONG)info) + info_size + (ULONG)ii->name_length*sizeof(WCHAR));
	ii = (readdir_data_t *)(((unsigned char *)ii) + sizeof(readdir_data_t) + ii->name_length);
	last_cookie = ii->cookie;
    }

    Irp->IoStatus.Status = STATUS_SUCCESS;
  done:
    ccb->cookie = last_cookie;

    SYNC_FAIL2(Irp->IoStatus.Status, (long)info - (long)outPtr);
}




/**********************************************************
 * AfsRdrQueryInfo
 * - handle stat calls
 **********************************************************/
NTSTATUS AfsRdrQueryInfo(DEVICE_OBJECT *DeviceObject, IRP *Irp, IO_STACK_LOCATION *IrpSp, afs_fcb_t *fcb)
{
    FILE_BASIC_INFORMATION *infoBasic;
    FILE_NAME_INFORMATION *infoName;
    FILE_STANDARD_INFORMATION *infoStandard;
    FILE_POSITION_INFORMATION *infoPosition;
    FILE_ALL_INFORMATION *infoAll;
    FILE_INTERNAL_INFORMATION *infoInternal;
    LARGE_INTEGER size, creation, access, change, written;
    ULONG attribs;
    long count;
    afs_ccb_t *ccb;
    char *outPtr;
    NTSTATUS status;
    wchar_t *start;

    if (IsDeviceFile(IrpSp->FileObject))
	SYNC_FAIL(STATUS_UNSUCCESSFUL);				// what should happen?

    if (!(outPtr = AfsFindBuffer(Irp)))
	SYNC_FAIL(STATUS_INSUFFICIENT_RESOURCES);

    ccb = IrpSp->FileObject->FsContext2;

    status = uc_stat(fcb->fid, &attribs, &size, &creation, &access, &change, &written);
    switch (status)
    {
    case IFSL_RPC_TIMEOUT:		SYNC_FAIL_RPC_TIMEOUT;
    case IFSL_BAD_INPUT:		SYNC_FAIL(STATUS_INVALID_PARAMETER);
    default:				SYNC_FAIL(STATUS_UNSUCCESSFUL);
    case 0: break;
    }

    /* afsd does not maintain instance-specific data, so we adjust here */
    if (ccb->access & FILE_READ_DATA && !(ccb->access & FILE_WRITE_DATA))
	attribs |= FILE_ATTRIBUTE_READONLY;

    (void*)infoBasic = (void*)infoName = (void*)infoPosition = (void*)infoStandard = (void*)infoInternal = NULL;
    switch (IrpSp->Parameters.QueryFile.FileInformationClass)
    {
    case FileBasicInformation:
        infoBasic = (FILE_BASIC_INFORMATION*)outPtr;
        break;
    case FileNameInformation:
        infoName = (FILE_NAME_INFORMATION*)outPtr;
        break;
    case FileStandardInformation:
        infoStandard = (FILE_STANDARD_INFORMATION*)outPtr;
        break;
    case FilePositionInformation:
        infoPosition = (FILE_POSITION_INFORMATION*)outPtr;
        break;
    case FileAllInformation:
        infoAll = (FILE_ALL_INFORMATION*)outPtr;
        infoBasic = &infoAll->BasicInformation;
        infoName = &infoAll->NameInformation;
        infoStandard = &infoAll->StandardInformation;
        infoPosition = &infoAll->PositionInformation;
        break;
    case FileInternalInformation:
        infoInternal = (FILE_INTERNAL_INFORMATION*)outPtr;
        break;
    default:
        STATUS(STATUS_NOT_IMPLEMENTED, 0);
        break;
    }

    if (infoBasic)
    {
	memset(infoBasic, 0, sizeof(FILE_BASIC_INFORMATION));
	infoBasic->FileAttributes = attribs;
	infoBasic->CreationTime.QuadPart = AfsTimeToWindowsTime(creation.QuadPart);
	infoBasic->LastAccessTime.QuadPart = AfsTimeToWindowsTime(access.QuadPart);
	infoBasic->LastWriteTime.QuadPart = AfsTimeToWindowsTime(written.QuadPart);
	infoBasic->ChangeTime.QuadPart = AfsTimeToWindowsTime(change.QuadPart);
	STATUS(STATUS_SUCCESS, sizeof(FILE_BASIC_INFORMATION));
	//KdPrint(("query basicinfo %d,%d %x,%I64d,%I64d\n", fcb->fid, (ULONG)IrpSp->FileObject->FsContext2, infoBasic->FileAttributes, infoBasic->CreationTime.QuadPart, infoBasic->ChangeTime.QuadPart));
    }

    if (infoName)
    {
	memset(infoName, 0, sizeof(FILE_NAME_INFORMATION));
	start = ccb->name;
	count = (long)(*(start+1) - L'0');
	if (count > 9 || count < 0)
            SYNC_FAIL(STATUS_OBJECT_NAME_INVALID);
	if (count || *start == L'0')
        {
            for ( ; count >= 0 && start; count--)
                start = wcschr(start+1, L'\\');
            if (!start)
                SYNC_FAIL(STATUS_OBJECT_NAME_INVALID);
        }
	infoName->FileNameLength = wcslen(start)*sizeof(WCHAR);
	if (((IrpSp->Parameters.QueryFile.FileInformationClass == FileAllInformation) ? sizeof(*infoAll) : sizeof(*infoName)) +
             infoName->FileNameLength > IrpSp->Parameters.QueryFile.Length)
        {
            infoName->FileNameLength = 0;
            infoName->FileName[0] = L'\0';
            STATUS(STATUS_BUFFER_OVERFLOW, sizeof(FILE_NAME_INFORMATION));
            //KdPrint(("query overflowing buffer %d\n", IrpSp->Parameters.QueryFile.Length));
        }
	else
        {   //TODO:check filename is correct/correct format
            StringCbCopy(infoName->FileName, IrpSp->Parameters.QueryFile.Length - sizeof(*infoName), start);
            STATUS(STATUS_SUCCESS, sizeof(FILE_NAME_INFORMATION) + (infoName->FileNameLength - sizeof(WCHAR)));
        }
	//KdPrint(("query nameinfo %ws from %ws\n", infoName->FileName, ccb->name));
    }

    if (infoStandard)
    {
	memset(infoStandard, 0, sizeof(FILE_STANDARD_INFORMATION));
	infoStandard->AllocationSize.QuadPart = size.QuadPart;
	infoStandard->EndOfFile.QuadPart = size.QuadPart;
	infoStandard->NumberOfLinks = 1;
	infoStandard->DeletePending = (fcb->delPending == 1);
	infoStandard->Directory = (attribs & FILE_ATTRIBUTE_DIRECTORY)?TRUE:FALSE;	//TODO:check if flag is valid at this point
	STATUS(STATUS_SUCCESS, sizeof(FILE_STANDARD_INFORMATION));
	//KdPrint(("query stdinfo %d,%d %I64d,%s\n", fcb->fid, (ULONG)IrpSp->FileObject->FsContext2, infoStandard->EndOfFile.QuadPart, infoStandard->Directory?"dir":"file"));
	}

    if (infoPosition)
    {
	infoPosition->CurrentByteOffset = IrpSp->FileObject->CurrentByteOffset;
	STATUS(STATUS_SUCCESS, sizeof(FILE_POSITION_INFORMATION));
	//KdPrint(("query position %d,%d %I64d\n", fcb, (ULONG)IrpSp->FileObject->FsContext2, infoPosition->CurrentByteOffset.QuadPart));
    }

    if (IrpSp->Parameters.QueryFile.FileInformationClass == FileAllInformation)
    {
	if (!infoName->FileNameLength)
            STATUS(STATUS_BUFFER_OVERFLOW, sizeof(FILE_ALL_INFORMATION));
	else    
            STATUS(STATUS_SUCCESS, sizeof(FILE_ALL_INFORMATION) + (infoName->FileNameLength - sizeof(WCHAR)));
	}

    if (infoInternal)
    {
	infoInternal->IndexNumber.QuadPart = fcb->fid;
	STATUS(STATUS_SUCCESS, sizeof(FILE_INTERNAL_INFORMATION));
    }

    status = Irp->IoStatus.Status;
    COMPLETE;
    return status;
}




/**********************************************************
 * AfsRdrSetInfo
 * - handle setting mod time
 * - handle deleting
 * - handle truncation/extension
 * - handle renaming
 **********************************************************/
NTSTATUS AfsRdrSetInfo(DEVICE_OBJECT *DeviceObject, IRP *Irp, IO_STACK_LOCATION *IrpSp, afs_fcb_t *fcb)
{
    struct SetInfoKOut *p;
    afs_fcb_t *fcbt;
    afs_ccb_t *ccb;
    FILE_DISPOSITION_INFORMATION *infoDisp;
    FILE_BASIC_INFORMATION *infoBasic;
    FILE_END_OF_FILE_INFORMATION *infoLength;
    FILE_RENAME_INFORMATION *infoRename;
    FILE_POSITION_INFORMATION *infoPosition;
    NTSTATUS ret;
    wchar_t *buf, *part, *ptr;
    ULONG size, new_fid;
    NTSTATUS status;

    if ( IrpSp->FileObject->FileName.Length == 0)
        SYNC_FAIL(STATUS_INVALID_DEVICE_REQUEST);

    ccb = IrpSp->FileObject->FsContext2;
 
    switch (IrpSp->Parameters.SetFile.FileInformationClass)
    {
	/* delete disposition */
    case FileDispositionInformation:
        infoDisp = Irp->AssociatedIrp.SystemBuffer;

        FsRtlEnterFileSystem();
        LOCK_FCB;

        if (infoDisp->DeleteFile)
        {
            if (!MmFlushImageSection(&(fcb->sectionPtrs), MmFlushForDelete))
		{
		UNLOCK_FCB;
		FsRtlExitFileSystem();
                SYNC_FAIL(STATUS_ACCESS_DENIED);
		}
            fcb->delPending |= 0x1;
        }
        else
            fcb->delPending &= ~0x1;
        UNLOCK_FCB;
        FsRtlExitFileSystem();
        SYNC_RET(STATUS_SUCCESS);

	/*case FileAllocationInformation:*/
    case FileEndOfFileInformation:
        /* ignore extensions caused by paging requests*/
        if (Irp->Flags & IRP_PAGING_IO)
            SYNC_FAIL(STATUS_SUCCESS);

        infoLength = Irp->AssociatedIrp.SystemBuffer;

        FsRtlEnterFileSystem();
        LOCK_PAGING_FCB;
        LOCK_FCB;
        if (IrpSp->Parameters.SetFile.AdvanceOnly && (infoLength->EndOfFile.QuadPart > fcb->FileSize.QuadPart) ||
             !IrpSp->Parameters.SetFile.AdvanceOnly && (infoLength->EndOfFile.QuadPart < fcb->FileSize.QuadPart))
        {
            status = uc_trunc(fcb->fid, infoLength->EndOfFile);
            /* because it is not written to the server immediately, this error will not always happen */
            if (status == IFSL_OVERQUOTA)
            {
                UNLOCK_FCB;
                UNLOCK_PAGING_FCB;
                FsRtlExitFileSystem();
                SYNC_FAIL(STATUS_DISK_FULL);
            }
        }
        fcb->FileSize = fcb->AllocationSize = fcb->ValidDataLength = infoLength->EndOfFile;
        CcSetFileSizes(IrpSp->FileObject, (CC_FILE_SIZES*)&fcb->AllocationSize);
        //CcPurgeCacheSection(IrpSp->FileObject->SectionObjectPointer, &fcb->AllocationSize, 0, FALSE);
        UNLOCK_FCB;
        UNLOCK_PAGING_FCB;
        FsRtlExitFileSystem();
        SYNC_FAIL(STATUS_SUCCESS);

    case FileBasicInformation:
        infoBasic = Irp->AssociatedIrp.SystemBuffer;
        status = uc_setinfo(fcb->fid, infoBasic->FileAttributes, infoBasic->CreationTime, infoBasic->LastAccessTime, infoBasic->ChangeTime, infoBasic->LastWriteTime);
        SYNC_FAIL(STATUS_SUCCESS);

    case FileRenameInformation:
        //KdPrint(("set rename %d\n", fcb->fid));
        infoRename = Irp->AssociatedIrp.SystemBuffer;
        new_fid = fcb->fid;
        //rpt1(("setinfo", "rename %d,%d %d,%ws", ExtractFid(fcb), (ULONG)IrpSp->FileObject->FsContext2, infoRename->ReplaceIfExists, infoRename->FileName));

        if (IrpSp->Parameters.SetFile.FileObject)
            fcbt = FindFcb(IrpSp->Parameters.SetFile.FileObject);			

        //null-terminate all strings into uc_rename?

        if (IrpSp->Parameters.SetFile.FileObject == NULL &&
             infoRename->RootDirectory == NULL)
        {
            WCHAR fname[MAX_PATH];
            StringCchCopyNW(fname, MAX_PATH-1, infoRename->FileName, infoRename->FileNameLength/sizeof(WCHAR));
            uc_rename(fcb->fid, ccb->name+2, NULL, fname, &new_fid);
            fcb->fid = new_fid;
        }
        else if (IrpSp->Parameters.SetFile.FileObject != NULL &&
                  infoRename->RootDirectory == NULL)
        {
            WCHAR fname[300];
            StringCchCopyNW(fname, 300-1, infoRename->FileName, infoRename->FileNameLength/sizeof(WCHAR));
            uc_rename(fcb->fid, ccb->name+2, fcbt->ccb_list->name+2, fname, &new_fid);
            fcb->fid = new_fid;
        }
        else
        {
			/* FIXFIX: implement this case, although we have never seen it called */
			SYNC_RET(STATUS_UNSUCCESSFUL);

			/*fcbt = FindFcb(IrpSp->Parameters.SetFile.FileObject);
            p->CurrNameOff = 0;
            StringCbCopyW(buf, size, fcb->name);
            p->NewNameOff = wcslen(buf)+1;
            StringCbCopyNW(buf + p->NewNameOff,
            size - p->NewNameOff,
            infoRename->FileName,
            infoRename->FileNameLength*sizeof(wchar_t));
            buf[p->NewNameOff+infoRename->FileNameLength] = L'\0';
            p->NewDirOff = p->NewNameOff + wcslen(buf + p->NewNameOff)+1;
            StringCbCopyW(buf + p->NewDirOff,
            size - p->NewDirOff,
            fcbt->name);*/
        }
        SYNC_RET(STATUS_SUCCESS);
        break;

    case FilePositionInformation:
        infoPosition = Irp->AssociatedIrp.SystemBuffer;
        IrpSp->FileObject->CurrentByteOffset = infoPosition->CurrentByteOffset;
        SYNC_FAIL(STATUS_SUCCESS);

    default:
        KdPrint(("set unsupp %d type %d\n", fcb->fid, IrpSp->Parameters.SetFile.FileInformationClass));
        SYNC_FAIL(STATUS_NOT_IMPLEMENTED);
    }

    SYNC_FAIL(STATUS_UNSUCCESSFUL);
}       

long
dc_break_callback(ULONG fid)
{
    afs_fcb_t *fcb;
    int pos;
    USHORT len;
    UNICODE_STRING *s;

    LOCK_FCB_LIST;
 
    fcb = find_fcb(fid);
    if (!fcb)
    {
	UNLOCK_FCB_LIST;
	return 1;					/* we are done with this file */
    }

    ASSERT(fcb->ccb_list);

    len = (wcslen(fcb->ccb_list->name) + 10) * sizeof(wchar_t);
    s = ExAllocatePool(NonPagedPool, sizeof(UNICODE_STRING)+len+sizeof(wchar_t));
    s->Length = len;
    s->MaximumLength = len + sizeof(wchar_t);
    s->Buffer = (PWSTR)(s+1);

    /* we are making a bogus change notification for now, because
     * it does what we need.
     */
    StringCbCopyW((PWSTR)(s+1), len, fcb->ccb_list->name);
    if (s->Buffer[wcslen(s->Buffer) - 1] != L'\\')
	StringCbCatW(s->Buffer, len, L"\\");
    pos = wcslen(s->Buffer);
    StringCbCatW(s->Buffer, len, L"jj");

    KdPrint(("break callback on %d %ws %ws\n", fid, fcb->ccb_list->name, fcb->ccb_list->name+pos));

    FsRtlNotifyFullReportChange(rdrExt->notifyList, &rdrExt->listHead,
                                 (PSTRING)s, (USHORT)pos*sizeof(wchar_t), NULL, NULL,
                                 FILE_NOTIFY_CHANGE_FILE_NAME/*FILE_NOTIFY_VALID_MASK/*FILE_NOTIFY_CHANGE_FILE_NAME*/, FILE_ACTION_ADDED, NULL);

    ExFreePool(s);
    UNLOCK_FCB_LIST;
    return 0;
}

long
dc_release_hooks(void)
{
    KeSetEvent(&comExt->cancelEvent, 0, FALSE);
    return 0;
}

/**********************************************************
 * AfsRdrDeviceControl
 * - handle communication requests from fs, etc.
 * - handle ioctls
 **********************************************************/
NTSTATUS AfsRdrDeviceControl(DEVICE_OBJECT *DeviceObject, IRP *Irp, IO_STACK_LOCATION *IrpSp, afs_fcb_t *fcb)
{
    struct KOutEntry *entry;
    NTSTATUS ret;
    struct CbKIn *kin;
    USHORT offset;
    UNICODE_STRING nm;
    void *outPtr;
    ULONG key, code, length;
    LARGE_INTEGER wait;

    /* utility ioctls */
    if (DeviceObject == ComDevice &&
         IrpSp->FileObject->FsContext2 == COMM_IOCTL &&
         IrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_AFSRDR_IOCTL)
    {
	outPtr = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
	if (!outPtr)
            SYNC_FAIL(STATUS_INSUFFICIENT_RESOURCES);

	wait.QuadPart = -1000;
	while (rpc_set_context(IrpSp->FileObject->FsContext) != 0)	/* if there are no open thread spots... */
	    KeDelayExecutionThread(KernelMode, FALSE, &wait);		/* ...wait */
	code = uc_ioctl_write(IrpSp->Parameters.DeviceIoControl.InputBufferLength,
                              Irp->AssociatedIrp.SystemBuffer,
                              (ULONG*)&key);
	length = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
	if (!code)		
            code = uc_ioctl_read(key, &length, outPtr);
	rpc_remove_context();

	switch (code)
        {
        case IFSL_SUCCESS:
            STATUS(STATUS_SUCCESS, length);
            break;
        default:
            STATUS(STATUS_UNSUCCESSFUL, 0);
            break;
        }
    }
    /* downcalls by afsd */
    else if (DeviceObject == ComDevice &&
              IrpSp->FileObject->FsContext2 == COMM_DOWNCALL &&
              IrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_AFSRDR_DOWNCALL)
    {       
	outPtr = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
	if (!outPtr)
            SYNC_FAIL(STATUS_INSUFFICIENT_RESOURCES);

	wait.QuadPart = -1000;
	while (rpc_set_context(IrpSp->FileObject->FsContext) != 0)	/* if there are no open thread spots... */
	    KeDelayExecutionThread(KernelMode, FALSE, &wait);		/* ...wait */
	code = rpc_call(IrpSp->Parameters.DeviceIoControl.InputBufferLength,
                         Irp->AssociatedIrp.SystemBuffer,
                         IrpSp->Parameters.DeviceIoControl.OutputBufferLength,
                         outPtr,
                         &length);
	rpc_remove_context();
	switch (code)
        {
        case IFSL_SUCCESS:
            STATUS(STATUS_SUCCESS, length);
            break;
        default:
            STATUS(STATUS_UNSUCCESSFUL, 0);
            break;
        }
    }
    /* ioctl to get full file afs path (used by pioctl) */
    else if (DeviceObject == RdrDevice &&
              IrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_AFSRDR_GET_PATH)
    {       
	outPtr = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
	if (!outPtr)
            SYNC_FAIL(STATUS_INSUFFICIENT_RESOURCES);

	StringCbCopyW(outPtr, IrpSp->Parameters.DeviceIoControl.OutputBufferLength, fcb->ccb_list->name+2);

	STATUS(STATUS_SUCCESS, (wcslen(outPtr)+1)*sizeof(wchar_t));
    }
    else
    {
	rpt0(("devctl", "devctl %d rejected", IrpSp->Parameters.DeviceIoControl.IoControlCode));
	SYNC_FAIL(STATUS_INVALID_DEVICE_REQUEST);
    }

    ret = Irp->IoStatus.Status;
    COMPLETE;	/* complete with priority boost */
    return ret;
}



/**********************************************************
 * AfsRdrCleanup
 * - called when usermode handle count reaches zero
 **********************************************************/
NTSTATUS AfsRdrCleanup(DEVICE_OBJECT *DeviceObject, IRP *Irp, IO_STACK_LOCATION *IrpSp, afs_fcb_t *fcb)
{
    NTSTATUS ret;
    struct AfsRdrExtension *ext;

#if 0
    try {
#endif
        if (IrpSp->FileObject->FileName.Length == 0)
            SYNC_FAIL(STATUS_SUCCESS);

        ext = ((struct AfsRdrExtension*)RdrDevice->DeviceExtension);

        LOCK_FCB_LIST;
        LOCK_PAGING_FCB;
        LOCK_FCB;

        FsRtlNotifyCleanup(ext->notifyList, &ext->listHead, IrpSp->FileObject->FsContext2);

	IoRemoveShareAccess(IrpSp->FileObject, &fcb->share_access);

        CcFlushCache(IrpSp->FileObject->SectionObjectPointer, NULL, 0, NULL);

        if (fcb->delPending)
	    {
	    if (!MmFlushImageSection(&(fcb->sectionPtrs), MmFlushForDelete))
		/* yes, moot at this point */
		STATUS(STATUS_ACCESS_DENIED, 0);
	    }
        else
	    {
	    MmFlushImageSection(IrpSp->FileObject->SectionObjectPointer, MmFlushForWrite);
            STATUS(STATUS_SUCCESS, 0);
	    }

        CcPurgeCacheSection(IrpSp->FileObject->SectionObjectPointer, NULL, 0, TRUE);
        CcUninitializeCacheMap(IrpSp->FileObject, NULL, NULL);

        UNLOCK_FCB;
        UNLOCK_PAGING_FCB;
        UNLOCK_FCB_LIST;

#if 0
    } except(EXCEPTION_EXECUTE_HANDLER)
    {
	STATUS(STATUS_UNSUCCESSFUL, 0);
	ExReleaseResourceLite(&ext->fcbLock);
	FsRtlExitFileSystem();
    }
#endif

    ret = Irp->IoStatus.Status;
    COMPLETE;
    return ret;
}


/**********************************************************
 * AfsRdrClose
 * - handle actual unlinking
 * - handle closing
 **********************************************************/
NTSTATUS AfsRdrClose(DEVICE_OBJECT *DeviceObject, IRP *Irp, IO_STACK_LOCATION *IrpSp, afs_fcb_t *fcb)
{
    ULONG length;
    wchar_t *name;
    char kill;
    KEVENT ev;
    LARGE_INTEGER timeout;
    afs_ccb_t *ccb, *curr;

    if (IrpSp->FileObject->FileName.Length == 0)
	SYNC_FAIL(STATUS_SUCCESS);

    ccb = IrpSp->FileObject->FsContext2;
    LOCK_FCB_LIST;

    ObDereferenceObject(ccb->token);

    curr = fcb->ccb_list;
    if (fcb->ccb_list == ccb)
	fcb->ccb_list = fcb->ccb_list->next;
    else
	while (curr->next)
        {
            if (curr->next == ccb)
            {
                curr->next = curr->next->next;
                break;
            }
            curr = curr->next;
        }

    if (!fcb->ccb_list)
    {
	uc_close(fcb->fid);
	if (fcb->delPending)
        {
            uc_unlink(ccb->name+2);
        }
	if (fcb->lock)
	    FsRtlFreeFileLock(fcb->lock);
	ExDeleteResourceLite(&fcb->_resource);
	ExDeleteResourceLite(&fcb->_pagingIoResource);
	RtlDeleteElementGenericTable(&rdrExt->fcbTable, &fcb);
	ExFreeToNPagedLookasideList(&rdrExt->fcbMemList, fcb);
    }

    ExFreePoolWithTag(ccb->name, AFS_RDR_TAG);
    if (ccb->filter && ccb->filter != SEARCH_MATCH_ALL)
	ExFreePoolWithTag(ccb->filter, AFS_RDR_TAG);
    ExFreeToNPagedLookasideList(&rdrExt->ccbMemList, ccb);

    UNLOCK_FCB_LIST;

    SYNC_FAIL(STATUS_SUCCESS);
}


/**********************************************************
 * AfsRdrShutdown
 * - should flush all data
 **********************************************************/
NTSTATUS AfsRdrShutdown(DEVICE_OBJECT *DeviceObject, IRP *Irp, IO_STACK_LOCATION *IrpSp)
{
    STATUS(STATUS_SUCCESS, 0);
    COMPLETE;
    return 0;
}


/**********************************************************
 * AfsRdrFlushFile
 * - flushes specified file to userspace and then disk
 **********************************************************/
NTSTATUS AfsRdrFlushFile(DEVICE_OBJECT *DeviceObject, IRP *Irp, IO_STACK_LOCATION *IrpSp, afs_fcb_t *fcb)
{
    NTSTATUS ret;
    afs_ccb_t *ccb;

    if (IsDeviceFile(IrpSp->FileObject))
	SYNC_RET(STATUS_INVALID_DEVICE_REQUEST);

    ccb = IrpSp->FileObject->FsContext2;

    CcFlushCache(&fcb->sectionPtrs, NULL, 0, &Irp->IoStatus);
    uc_flush(fcb->fid);

    ret = Irp->IoStatus.Status;
    COMPLETE;
    return ret;
}


/**********************************************************
 * AfsRdrLockCtrl
 * - should handle lock requests
 **********************************************************/
NTSTATUS AfsRdrLockCtrl(DEVICE_OBJECT *DeviceObject, IRP *Irp, IO_STACK_LOCATION *IrpSp, afs_fcb_t *fcb)
{
    NTSTATUS ret;

    /* complete lock on control device object without processing, as directed */
    if (IrpSp->FileObject->FileName.Length == 0)
    {
	rpt0(("lock", "lock granted on root device obj"));
	SYNC_FAIL(STATUS_SUCCESS);
    }

    switch (IrpSp->MinorFunction)
    {
	case IRP_MN_LOCK:
	case IRP_MN_UNLOCK_ALL:
	case IRP_MN_UNLOCK_ALL_BY_KEY:
	case IRP_MN_UNLOCK_SINGLE:
	    FsRtlEnterFileSystem();
	    LOCK_PAGING_FCB;
	    LOCK_FCB;
	    if (!fcb->lock)
		{
		fcb->lock = FsRtlAllocateFileLock(NULL, NULL);
		}
	    UNLOCK_FCB;
	    UNLOCK_PAGING_FCB;
	    FsRtlExitFileSystem();
	    FsRtlProcessFileLock(fcb->lock, Irp, NULL);
	    fcb->IsFastIoPossible = fcb->lock->FastIoIsQuestionable ? FastIoIsQuestionable : FastIoIsPossible;
	    return STATUS_PENDING;
	default:
	    SYNC_FAIL2(STATUS_NOT_IMPLEMENTED, 0);
    }
    SYNC_FAIL2(/*STATUS_LOCK_NOT_GRANTED*//*STATUS_NOT_IMPLEMENTED*/STATUS_SUCCESS, 0);

    /*EXCEPT(STATUS_UNSUCCESSFUL, 0);*/
}


/**********************************************************
 * AfsRdrQueryVol
 * - handle volume information requests
 **********************************************************/
NTSTATUS AfsRdrQueryVol(DEVICE_OBJECT *DeviceObject, IRP *Irp, IO_STACK_LOCATION *IrpSp)
{
    FILE_FS_ATTRIBUTE_INFORMATION *infoAttr;
    FILE_FS_DEVICE_INFORMATION *infoDevice;
    FILE_FS_SIZE_INFORMATION * infoSize;
    FILE_FS_VOLUME_INFORMATION *infoVolume;
    NTSTATUS ret;

    TRY

        switch (IrpSp->Parameters.QueryVolume.FsInformationClass)
	{
	case FileFsAttributeInformation:
            infoAttr = (FILE_FS_ATTRIBUTE_INFORMATION*)Irp->AssociatedIrp.SystemBuffer;
            memset(infoAttr, 0, sizeof(FILE_FS_ATTRIBUTE_INFORMATION));
            infoAttr->FileSystemAttributes = FILE_CASE_PRESERVED_NAMES | FILE_CASE_SENSITIVE_SEARCH;	//TODOTODO:?
            infoAttr->MaximumComponentNameLength = 255;		// should this be 255?
            ///RtlCopyMemory(infoAttr->FileSystemName, L"AFS", 2);
            ///infoAttr->FileSystemNameLength = 2;
            //StringCbCopyLen(infoAttr->FileSystemName, IrpSp->Parameters.QueryVolume.Length-sizeof(*infoAttr)-2, L"AFS", &infoAttr->FileSystemNameLength);
            //IrpSp->Parameters.QueryVolume.Length = 0;
            //Irp->IoStatus.Information = sizeof(*infoAttr) + (infoAttr->FileSystemNameLength - sizeof(WCHAR));
            if (sizeof(*infoAttr) + wcslen(AFS_FS_NAME)*sizeof(wchar_t) > IrpSp->Parameters.QueryVolume.Length)
            {
                infoAttr->FileSystemNameLength = 0;
                rpt0(("vol", "overflowing attr buffer %d", IrpSp->Parameters.QueryVolume.Length));
                SYNC_FAIL2(STATUS_BUFFER_OVERFLOW, sizeof(*infoAttr));
            }
            else
            {
                infoAttr->FileSystemNameLength = wcslen(AFS_FS_NAME)*sizeof(wchar_t);
                StringCbCopyW(infoAttr->FileSystemName, IrpSp->Parameters.QueryVolume.Length - sizeof(*infoAttr) + sizeof(WCHAR), AFS_FS_NAME);
                SYNC_FAIL2(STATUS_SUCCESS, sizeof(*infoAttr) + (infoAttr->FileSystemNameLength - sizeof(WCHAR)));
            }		
            break;

	case FileFsDeviceInformation:
            infoDevice = (FILE_FS_DEVICE_INFORMATION*)Irp->AssociatedIrp.SystemBuffer;
            memset(infoDevice, 0, sizeof(FILE_FS_DEVICE_INFORMATION));
            infoDevice->DeviceType = FILE_DEVICE_DISK;//DeviceObject->DeviceType;// FILE_DEVICE_NETWORK_FILE_SYSTEM;
            infoDevice->Characteristics = DeviceObject->Characteristics;//FILE_DEVICE_IS_MOUNTED /*| FILE_REMOTE_DEVICE*/;		// remote device?
            IrpSp->Parameters.QueryVolume.Length = sizeof(*infoDevice);
            SYNC_FAIL2(STATUS_SUCCESS, IrpSp->Parameters.QueryVolume.Length);
            break;

	case FileFsSizeInformation:
            infoSize = (FILE_FS_SIZE_INFORMATION*)Irp->AssociatedIrp.SystemBuffer;
            memset(infoSize, 0, sizeof(FILE_FS_SIZE_INFORMATION));
			/* this data needs to come from an upcall */
            infoSize->TotalAllocationUnits.QuadPart =		0x00000000F0000000;
            infoSize->AvailableAllocationUnits.QuadPart =	0x00000000E0000000;
            infoSize->SectorsPerAllocationUnit = 1;
            infoSize->BytesPerSector = 1;
            IrpSp->Parameters.QueryVolume.Length = sizeof(*infoSize);
            SYNC_FAIL2(STATUS_SUCCESS, IrpSp->Parameters.QueryVolume.Length);
            break;

	case FileFsVolumeInformation:
            infoVolume = (FILE_FS_VOLUME_INFORMATION*)Irp->AssociatedIrp.SystemBuffer;
            memset(infoVolume, 0, sizeof(FILE_FS_VOLUME_INFORMATION));
			/* this data needs to come from an upcall */
            infoVolume->VolumeCreationTime.QuadPart = AfsTimeToWindowsTime(1080000000);
            infoVolume->VolumeSerialNumber = 0x12345678;
            infoVolume->SupportsObjects = FALSE;
            //StringCbCopyLen(infoVolume->VolumeLabel, IrpSp->Parameters.QueryVolume.Length-sizeof(*infoVolume)-2, L"AfsRed", &infoVolume->VolumeLabelLength);
            //IrpSp->Parameters.QueryVolume.Length = 0;
            if (sizeof(*infoVolume) + 12 > IrpSp->Parameters.QueryVolume.Length)
            {
                infoVolume->VolumeLabelLength = 0;
                SYNC_FAIL2(STATUS_BUFFER_OVERFLOW, sizeof(*infoVolume));
            }
            else
            {
                infoVolume->VolumeLabelLength = 12;
                RtlCopyMemory(infoVolume->VolumeLabel, L"AfsRed", 12);
                SYNC_FAIL2(STATUS_SUCCESS, sizeof(*infoVolume) + (infoVolume->VolumeLabelLength - sizeof(WCHAR)));
            }		
            break;
	}

    EXCEPT(STATUS_UNSUCCESSFUL, 0);

    rpt0(("vol", "vol class %d unknown", IrpSp->Parameters.QueryVolume.FsInformationClass));
    SYNC_FAIL(STATUS_NOT_IMPLEMENTED);
}

VOID AfsRdrUnload(DRIVER_OBJECT *DriverObject)
{
    UNICODE_STRING userModeName;

    FsRtlNotifyUninitializeSync(&rdrExt->notifyList);

    RtlInitUnicodeString(&userModeName, L"\\DosDevices\\afscom");
    IoDeleteSymbolicLink(&userModeName);

    /*RtlInitUnicodeString(&userModeName, L"\\DosDevices\\T:");
    IoDeleteSymbolicLink(&userModeName);*/

    ExDeleteNPagedLookasideList(&rdrExt->fcbMemList);
    ExDeleteNPagedLookasideList(&rdrExt->ccbMemList);

    rpc_shutdown();

    IoDeleteDevice(ComDevice);
    IoDeleteDevice(RdrDevice);

    KdPrint(("RdrUnload exiting.\n"));
}


// handles all non-handled irp's synchronously
NTSTATUS AfsRdrNull(DEVICE_OBJECT *DeviceObject, IRP *Irp, IO_STACK_LOCATION *IrpSp)
{
    NTSTATUS ret;

    /*TRY
    rpt0(("kunhand", IrpMjFuncDesc[IrpSp->MajorFunction]));*/

    SYNC_FAIL(STATUS_NOT_IMPLEMENTED);

    /*EXCEPT(STATUS_UNSUCCESSFUL, 0);

    ret = Irp->IoStatus.Status;
    COMPLETE_NO_BOOST;
    return ret;*/
}

NTSTATUS ComDispatch(DEVICE_OBJECT *DeviceObject, IRP *Irp)
{
    IO_STACK_LOCATION *IrpSp, *IrpSpClient;
    NTSTATUS ret;
    struct ComExtension *ext;
    void *ptr, *ptr2;
    rpc_t find, *find_ptr;
    LARGE_INTEGER timeout;
    struct afsFcb *fcb;
    rpc_t *rpc, **rpcp;
    ULONG len;
    ULONG code, read;
    PACCESS_TOKEN acc_token;
    PVOID waitPair[2];

    ext = (struct ComExtension *)DeviceObject->DeviceExtension;
    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    switch (IrpSp->MajorFunction)
    {
    case IRP_MJ_CREATE:
        IrpSp->FileObject->FsContext = 0;
        IrpSp->FileObject->FsContext2 = 0;
        if (IrpSp->FileObject->FileName.Length)
        {
            /* ioctls come from fs, vos, bos, etc. using a pre-existing interface */
            /* downcalls come from afsd, using a new interface */
            /* upcall hooks come from afsd */
            if (!wcscmp(IrpSp->FileObject->FileName.Buffer, L"\\ioctl"))
                IrpSp->FileObject->FsContext2 = COMM_IOCTL;
            else if (!wcscmp(IrpSp->FileObject->FileName.Buffer, L"\\downcall"))
                IrpSp->FileObject->FsContext2 = COMM_DOWNCALL;
            else if (!wcscmp(IrpSp->FileObject->FileName.Buffer, L"\\upcallhook"))
                IrpSp->FileObject->FsContext2 = COMM_UPCALLHOOK;
        }
        if (!IrpSp->FileObject->FsContext2)
            SYNC_FAIL2(STATUS_INVALID_DEVICE_REQUEST, 0);

        acc_token = SeQuerySubjectContextToken(&IrpSp->Parameters.Create.SecurityContext->AccessState->SubjectSecurityContext);
        ASSERT(acc_token);
        /* SeQueryAuthenticationIdToken */
        IrpSp->FileObject->FsContext = acc_token;
        STATUS(STATUS_SUCCESS, FILE_OPENED);
        break;

    case IRP_MJ_CLEANUP:
        /* acc_token does not have to be released */
    case IRP_MJ_CLOSE:
        STATUS(STATUS_SUCCESS, 0);
        break;

    case IRP_MJ_WRITE:
        /* we only process MDL writes */
        if (!Irp->MdlAddress ||
             !(ptr = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority)))	/* should be LowPagePriority */
            SYNC_FAIL(STATUS_INSUFFICIENT_RESOURCES);

        if (!IrpSp->FileObject->FsContext ||
             !IrpSp->FileObject->FsContext2)
            SYNC_FAIL(STATUS_INVALID_HANDLE);

        if (IrpSp->FileObject->FsContext2 == COMM_UPCALLHOOK)
        {
            rpc_recv(ptr, IrpSp->Parameters.Write.Length);
            STATUS(STATUS_SUCCESS, IrpSp->Parameters.Write.Length);
        }
        else
            STATUS(STATUS_INVALID_DEVICE_REQUEST, 0);
        break;
    case IRP_MJ_READ:
        if (!Irp->MdlAddress || !(ptr = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority)))	// should be LowPagePriority
            SYNC_FAIL(STATUS_INSUFFICIENT_RESOURCES);

        if (IrpSp->FileObject->FsContext2 == COMM_UPCALLHOOK)
        {
	    KeClearEvent(&comExt->cancelEvent);

            timeout.QuadPart = -100000000L;
	    waitPair[0] = &comExt->outEvent;
	    waitPair[1] = &comExt->cancelEvent;
            ret = KeWaitForMultipleObjects(2, waitPair, WaitAny, Executive, KernelMode, FALSE, &timeout, NULL);

	    if (ret == STATUS_WAIT_1)
		SYNC_FAIL(STATUS_UNSUCCESSFUL);

            if (!rpc_send(ptr, IrpSp->Parameters.Read.Length, &read))
            {
                KeClearEvent(&comExt->outEvent);
	        ret = KeWaitForMultipleObjects(2, waitPair, WaitAny, Executive, KernelMode, FALSE, &timeout, NULL);
                if (!rpc_send(ptr, IrpSp->Parameters.Read.Length, &read))
                {
                    KeClearEvent(&comExt->outEvent);
                    SYNC_FAIL(STATUS_UNSUCCESSFUL);
                }
            }
            SYNC_RET2(STATUS_SUCCESS, read);			/* complete with priority boost */
        }
        else
            STATUS(STATUS_INVALID_DEVICE_REQUEST, 0);
        break;
    case IRP_MJ_DEVICE_CONTROL:
        return AfsRdrDeviceControl(DeviceObject, Irp, IrpSp, NULL);
    default:
        STATUS(STATUS_INVALID_DEVICE_REQUEST, 0);
        break;
    }

    ret = Irp->IoStatus.Status;
    COMPLETE;							/* complete with priority boost */
    return ret;
}


/* handles all irp's for primary (fs) device */
NTSTATUS Dispatch(DEVICE_OBJECT *DeviceObject, IRP *Irp)
{
    IO_STACK_LOCATION *IrpSp;
    NTSTATUS ret;
    afs_fcb_t *fcb;
    afs_ccb_t *ccb;
    LARGE_INTEGER wait;

    if (DeviceObject->DeviceType == FILE_DEVICE_DATALINK)
	return ComDispatch(DeviceObject, Irp);

    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    rpt4(("irp", "%s min %d on %d (%ws) %x %x", IrpMjFuncDesc[IrpSp->MajorFunction], IrpSp->MinorFunction, 0/*ExtractFid(IrpSp->FileObject->FsContext)*/, IrpSp->FileObject->FileName.Buffer, IrpSp->Flags, IrpSp->Parameters.Create.Options));

    fcb = IrpSp->FileObject->FsContext;
    if (IrpSp->MajorFunction != IRP_MJ_CREATE)
	 ASSERT(fcb);

    if (IrpSp->FileObject && IrpSp->FileObject->FsContext2)
    {
	ccb = IrpSp->FileObject->FsContext2;
	wait.QuadPart = -1000;
	while (rpc_set_context(ccb->token) != 0)			/* if there are no open thread spots... */
	    KeDelayExecutionThread(KernelMode, FALSE, &wait);		/* ...wait */
    }

    switch (IrpSp->MajorFunction)
    {
    case IRP_MJ_CREATE:
        ret = AfsRdrCreate(DeviceObject, Irp, IrpSp, NULL);			
        break;
    case IRP_MJ_DIRECTORY_CONTROL:
        ret = AfsRdrDirCtrl(DeviceObject, Irp, IrpSp, fcb);			
        break;
    case IRP_MJ_READ:
        ret = AfsRdrRead(DeviceObject, Irp, IrpSp, fcb);			
        break;
    case IRP_MJ_WRITE:
        ret = AfsRdrWrite(DeviceObject, Irp, IrpSp, fcb);			        
        break;
    case IRP_MJ_CLOSE:
        ret = AfsRdrClose(DeviceObject, Irp, IrpSp, fcb);		       
        break;
    case IRP_MJ_QUERY_INFORMATION:
        ret = AfsRdrQueryInfo(DeviceObject, Irp, IrpSp, fcb);	       
        break;
    case IRP_MJ_DEVICE_CONTROL:
        ret = AfsRdrDeviceControl(DeviceObject, Irp, IrpSp, fcb);	
        break;
    case IRP_MJ_CLEANUP:
        ret = AfsRdrCleanup(DeviceObject, Irp, IrpSp, fcb);		       
        break;
    case IRP_MJ_LOCK_CONTROL:
        ret = AfsRdrLockCtrl(DeviceObject, Irp, IrpSp, fcb);		
        break;
    case IRP_MJ_QUERY_VOLUME_INFORMATION:
        ret = AfsRdrQueryVol(DeviceObject, Irp, IrpSp);				
        break;
    case IRP_MJ_SHUTDOWN:
        ret = AfsRdrShutdown(DeviceObject, Irp, IrpSp);				
        break;
    case IRP_MJ_FILE_SYSTEM_CONTROL:
        if (IrpSp->MinorFunction == IRP_MN_USER_FS_REQUEST &&
             (IrpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_1 ||
               IrpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_2 ||
               IrpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_REQUEST_BATCH_OPLOCK ||
               IrpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_REQUEST_FILTER_OPLOCK))
            STATUS(STATUS_OPLOCK_NOT_GRANTED, 0);
        else
            STATUS(STATUS_INVALID_DEVICE_REQUEST/*STATUS_NOT_IMPLEMENTED*//*STATUS_INVALID_PARAMETER*/, 0);
		ret = Irp->IoStatus.Status;
        goto complete;
    case IRP_MJ_SET_INFORMATION:
        ret = AfsRdrSetInfo(DeviceObject, Irp, IrpSp, fcb);			
        break;
    case IRP_MJ_FLUSH_BUFFERS:
        ret = AfsRdrFlushFile(DeviceObject, Irp, IrpSp, fcb);		
        break;
    default:
        ret = AfsRdrNull(DeviceObject, Irp, IrpSp);				       
        break;
    }
    rpc_remove_context();
    return ret;

  complete:
    rpc_remove_context();
    COMPLETE;
    return ret;
}


BOOLEAN
fastIoCheck (
    IN struct _FILE_OBJECT *FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    IN BOOLEAN CheckForReadOperation,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN struct _DEVICE_OBJECT *DeviceObject
    )
{
afs_fcb_t *fcb;
LARGE_INTEGER temp;

fcb = FileObject->FsContext;
ASSERT(fcb);

if (!fcb->lock)
    return TRUE;

temp.QuadPart = Length;
if (CheckForReadOperation)
    return FsRtlFastCheckLockForRead(fcb->lock, FileOffset, &temp, LockKey, FileObject, IoGetCurrentProcess());
return FsRtlFastCheckLockForWrite(fcb->lock, FileOffset, &temp, LockKey, FileObject, IoGetCurrentProcess());
}


BOOLEAN
fastIoRead (
    IN struct _FILE_OBJECT *FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    OUT PVOID Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN struct _DEVICE_OBJECT *DeviceObject
    )
{
    BOOLEAN ret;
    ULONG adj_len;
    afs_fcb_t *fcb;
    LARGE_INTEGER temp;

    fcb = FileObject->FsContext;
    ASSERT(fcb);

    FsRtlEnterFileSystem();
    LOCK_PAGING_FCB;

    if (fcb->lock)
    {
	temp.QuadPart = Length;
	if (!FsRtlFastCheckLockForRead(fcb->lock, FileOffset, &temp, LockKey, FileObject, IoGetCurrentProcess()))
	{
	    UNLOCK_PAGING_FCB;
	    FsRtlExitFileSystem();
	    return FALSE;
	}
    }

    adj_len = Length;
    if (FileOffset->QuadPart > fcb->FileSize.QuadPart)
    {
	UNLOCK_PAGING_FCB;
	FsRtlExitFileSystem();
	IoStatus->Status = STATUS_END_OF_FILE;
	IoStatus->Information = 0;
	return TRUE;
    }
    if (FileOffset->QuadPart + Length > fcb->FileSize.QuadPart)
	adj_len = (ULONG)(fcb->FileSize.QuadPart - FileOffset->QuadPart);

    try
    {
	ret = CcCopyRead(FileObject, FileOffset, adj_len, Wait, Buffer, IoStatus);

	FileObject->CurrentByteOffset.QuadPart = FileOffset->QuadPart + IoStatus->Information;
	UNLOCK_PAGING_FCB;
	FsRtlExitFileSystem();
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
	UNLOCK_PAGING_FCB;
	FsRtlExitFileSystem();
	return FALSE;
    }
    return ret;
}

BOOLEAN
fastIoWrite (
    IN struct _FILE_OBJECT *FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    OUT PVOID Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN struct _DEVICE_OBJECT *DeviceObject
    )
{
    BOOLEAN ret;
    LARGE_INTEGER adj_end;
    afs_fcb_t *fcb;
    LARGE_INTEGER temp;

    fcb = FileObject->FsContext;
    ASSERT(fcb);

    FsRtlEnterFileSystem();
    LOCK_PAGING_FCB;

    if (fcb->lock)
    {
	temp.QuadPart = Length;
	if (!FsRtlFastCheckLockForWrite(fcb->lock, FileOffset, &temp, LockKey, FileObject, IoGetCurrentProcess()))
	{
	    UNLOCK_PAGING_FCB;
	    FsRtlExitFileSystem();
	    return FALSE;
	}
    }


    if (FileOffset->QuadPart + Length > fcb->FileSize.QuadPart)
    {
	adj_end.QuadPart = FileOffset->QuadPart + Length;
	fcb->AllocationSize = fcb->FileSize = fcb->ValidDataLength = adj_end;
	LOCK_FCB;
	try
        {
            CcSetFileSizes(FileObject, (CC_FILE_SIZES*)&fcb->AllocationSize);
        }
	except (EXCEPTION_EXECUTE_HANDLER)
        {
            UNLOCK_FCB;
            UNLOCK_PAGING_FCB;
            FsRtlExitFileSystem();
            return FALSE;
        }
	UNLOCK_FCB;
    }

    try
    {
	ret = CcCopyWrite(FileObject, FileOffset, Length, Wait, Buffer);
	IoStatus->Status = ret?STATUS_SUCCESS:STATUS_UNSUCCESSFUL;
	IoStatus->Information = ret?Length:0;

	FileObject->CurrentByteOffset.QuadPart = FileOffset->QuadPart + IoStatus->Information;
	UNLOCK_PAGING_FCB;
	FsRtlExitFileSystem();
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
	UNLOCK_PAGING_FCB;
	FsRtlExitFileSystem();
	return FALSE;
    }
    return ret;
}

RTL_GENERIC_COMPARE_RESULTS FcbCompareRoutine(struct _RTL_GENERIC_TABLE *Table, PVOID FirstStruct, PVOID SecondStruct)
{
    afs_fcb_t *p1, *p2;

    p1 = (void*)*(afs_fcb_t**)FirstStruct;  p2 = (void*)*(afs_fcb_t**)SecondStruct;
    if (p1->fid < p2->fid)
        return GenericLessThan;
    if (p1->fid > p2->fid)
        return GenericGreaterThan;
    return GenericEqual;
}

RTL_GENERIC_COMPARE_RESULTS ReqCompareRoutine(struct _RTL_GENERIC_TABLE *Table, PVOID FirstStruct, PVOID SecondStruct)
{
    rpc_t *p1, *p2;

    p1 = *(rpc_t**)FirstStruct;  p2 = *(rpc_t**)SecondStruct;
    if (p1->key < p2->key)
        return GenericLessThan;
    if (p1->key > p2->key)
        return GenericGreaterThan;
    return GenericEqual;
}

PVOID AllocateRoutine(struct _RTL_GENERIC_TABLE *Table, CLONG  ByteSize)
{
    PVOID ret;
    ret = ExAllocatePoolWithTag(NonPagedPool, ByteSize, AFS_RDR_TAG);
    ASSERT(ret);
    RtlZeroMemory(ret, ByteSize);
    return ret;
}

VOID FreeRoutine(struct _RTL_GENERIC_TABLE *Table, PVOID Buffer)
{
    ExFreePoolWithTag(Buffer, AFS_RDR_TAG);
}

//KSPIN_LOCK rpc_lock;
//KIRQL irql;
FAST_MUTEX rpc_lock;

void ifs_lock_rpcs()
{
    ExAcquireFastMutex(&rpc_lock);
    //KeAcquireSpinLock(&rpc_lock, &irql);
}

void ifs_unlock_rpcs()
{
    ExReleaseFastMutex(&rpc_lock);
    //KeReleaseSpinLock(&rpc_lock, irql);
}

NTSTATUS DriverEntry(DRIVER_OBJECT *DriverObject, UNICODE_STRING *RegistryPath)
{
    NTSTATUS err;
    UNICODE_STRING rdrName, comName, userModeName, userModeCom;
    int x;
    IO_STATUS_BLOCK status;
    FAST_IO_DISPATCH *fastIoDispatch;

    //try {
    RtlInitUnicodeString(&rdrName, L"\\Device\\afsrdr");
    RtlInitUnicodeString(&comName, L"\\Device\\afscom");

    rpt0(("init", "kern initializing"));
    //KeInitializeSpinLock(&rpc_lock);
    ExInitializeFastMutex(&rpc_lock);

    IoAllocateDriverObjectExtension(DriverObject, (void *)0x394389f7, sizeof(FAST_IO_DISPATCH), &fastIoDispatch);
    RtlZeroMemory(fastIoDispatch, sizeof(FAST_IO_DISPATCH));
    fastIoDispatch->SizeOfFastIoDispatch = sizeof(FAST_IO_DISPATCH);
    fastIoDispatch->FastIoCheckIfPossible = fastIoCheck;
    fastIoDispatch->FastIoRead = /*FsRtlCopyRead;*/fastIoRead;
    fastIoDispatch->FastIoWrite = /*FsRtlCopyWrite;*/fastIoWrite;
    DriverObject->FastIoDispatch = fastIoDispatch;

    for (x = 0; x < IRP_MJ_MAXIMUM_FUNCTION; x++)
	DriverObject->MajorFunction[x] = Dispatch;
    DriverObject->DriverUnload = AfsRdrUnload;

    err = IoCreateDevice(DriverObject, sizeof(struct AfsRdrExtension)*2, &rdrName, FILE_DEVICE_NETWORK_FILE_SYSTEM, /*FILE_REMOTE_DEVICE*/0, FALSE, &RdrDevice);
    if (!NT_SUCCESS(STATUS_SUCCESS))
	return STATUS_UNSUCCESSFUL;
    err = IoCreateDevice(DriverObject, sizeof(struct ComExtension)*2, &comName, FILE_DEVICE_DATALINK, 0, FALSE, &ComDevice);
    if (!NT_SUCCESS(STATUS_SUCCESS))
	return STATUS_UNSUCCESSFUL;

    RdrDevice->Flags |= DO_DIRECT_IO;
    RdrDevice->StackSize = 5;					/* could this be zero? */
    rdrExt = ((struct AfsRdrExtension*)RdrDevice->DeviceExtension);
    RtlZeroMemory(rdrExt, sizeof(struct AfsRdrExtension));

    /* could raise exception */
    FsRtlNotifyInitializeSync(&rdrExt->notifyList);
    InitializeListHead(&rdrExt->listHead);

    rdrExt->callbacks.AcquireForLazyWrite = lazyWriteLock;
    rdrExt->callbacks.ReleaseFromLazyWrite = lazyWriteUnlock;
    rdrExt->callbacks.AcquireForReadAhead = readAheadLock;
    rdrExt->callbacks.ReleaseFromReadAhead = readAheadUnlock;
    ExInitializeNPagedLookasideList(&rdrExt->fcbMemList, NULL, NULL, 0, sizeof(afs_fcb_t), AFS_RDR_TAG, 0);
    ExInitializeNPagedLookasideList(&rdrExt->ccbMemList, NULL, NULL, 0, sizeof(afs_ccb_t), AFS_RDR_TAG, 0);
    ExInitializeFastMutex(&rdrExt->fcbLock);
    RtlInitializeGenericTable(&rdrExt->fcbTable, FcbCompareRoutine, AllocateRoutine, FreeRoutine, NULL);


    ComDevice->Flags |= DO_DIRECT_IO;
    ComDevice->StackSize = 5;					// ??
    comExt = ((struct ComExtension*)ComDevice->DeviceExtension);
    RtlZeroMemory(comExt, sizeof(struct ComExtension));

    InitializeListHead(&comExt->outReqList);
    KeInitializeSpinLock(&comExt->outLock);
    ExInitializeFastMutex(&comExt->inLock);
    KeInitializeEvent(&comExt->outEvent, SynchronizationEvent/*NotificationEvent*/, TRUE);
    KeInitializeEvent(&comExt->cancelEvent, NotificationEvent, TRUE);

    comExt->rdr = rdrExt;
    rdrExt->com = comExt;

    RtlInitUnicodeString(&userModeCom, L"\\DosDevices\\afscom");
    err = IoCreateSymbolicLink(&userModeCom, &comName);

    /*RtlInitUnicodeString(&rdrName, L"\\Device\\afsrdr\\1\\CITI.UMICH.EDU");
    RtlInitUnicodeString(&userModeName, L"\\DosDevices\\W:");
    err = IoCreateSymbolicLink(&userModeName, &rdrName);*/

    /*} except(EXCEPTION_EXECUTE_HANDLER)
    {
    return -1;
    }*/

    KdPrint(("DriverEntry exiting.\n"));
    return STATUS_SUCCESS;
}
