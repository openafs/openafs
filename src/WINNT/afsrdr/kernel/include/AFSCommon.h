/*
 * Copyright (c) 2008 Kernel Drivers, LLC.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice,
 *   this list of conditions and the following disclaimer in the
 *   documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of Kernel Drivers, LLC nor the names of its
 *   contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission from Kernel Drivers, LLC.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _AFS_COMMON_H
#define _AFS_COMMON_H

//
// File: AFSCommon.h
//

extern "C"
{

#define AFS_KERNEL_MODE

#include <ntifs.h>
#include <wdmsec.h> // for IoCreateDeviceSecure
#include <initguid.h>

#include "AFSDefines.h"

#include "AFSUserCommon.h"

#include "AFSStructs.h"

#include "AFSProvider.h"

#ifndef NO_EXTERN
#include "AFSExtern.h"
#endif

#define NTSTRSAFE_LIB
#include "ntstrsafe.h"

//
// AFSBTreeSupport.cpp Prototypes
//

NTSTATUS
AFSLocateCaseSensitiveDirEntry( IN AFSDirEntryCB *RootNode,
                                IN ULONG Index,
                                IN OUT AFSDirEntryCB **DirEntry);

NTSTATUS
AFSLocateCaseInsensitiveDirEntry( IN AFSDirEntryCB *RootNode,
                                  IN ULONG Index,
                                  IN OUT AFSDirEntryCB **DirEntry);

NTSTATUS
AFSInsertCaseSensitiveDirEntry( IN AFSDirEntryCB *RootNode,
                                IN AFSDirEntryCB *DirEntry);

NTSTATUS
AFSInsertCaseInsensitiveDirEntry( IN AFSDirEntryCB *RootNode,
                                  IN AFSDirEntryCB *DirEntry);

NTSTATUS
AFSRemoveCaseSensitiveDirEntry( IN AFSDirEntryCB **RootNode,
                                IN AFSDirEntryCB *DirEntry);

NTSTATUS
AFSRemoveCaseInsensitiveDirEntry( IN AFSDirEntryCB **RootNode,
                                  IN AFSDirEntryCB *DirEntry);

NTSTATUS
AFSLocateShortNameDirEntry( IN AFSDirEntryCB *RootNode,
                            IN ULONG Index,
                            IN OUT AFSDirEntryCB **DirEntry);

NTSTATUS
AFSInsertShortNameDirEntry( IN AFSDirEntryCB *RootNode,
                            IN AFSDirEntryCB *DirEntry);

NTSTATUS
AFSRemoveShortNameDirEntry( IN AFSDirEntryCB **RootNode,
                            IN AFSDirEntryCB *DirEntry);

NTSTATUS
AFSLocateHashEntry( IN AFSBTreeEntry *TopNode,
                    IN ULONGLONG Index,
                    IN OUT AFSFcb **Fcb);

NTSTATUS
AFSLocateTreeEntry( IN AFSBTreeEntry *TopNode,
                    IN ULONGLONG HashIndex,
                    IN OUT AFSBTreeEntry **TreeEntry);

NTSTATUS
AFSInsertHashEntry( IN AFSBTreeEntry *TopNode,
                    IN AFSBTreeEntry *FileIDEntry);

NTSTATUS
AFSRemoveHashEntry( IN AFSBTreeEntry **TopNode,
                    IN AFSBTreeEntry *FileIDEntry);

//
// AFSInit.cpp Prototypes
//

NTSTATUS
DriverEntry( IN PDRIVER_OBJECT DriverObj,
             IN PUNICODE_STRING RegPath);

void
AFSUnload( IN PDRIVER_OBJECT DriverObject);

//
// AFSCommSupport.cpp Prototypes
//

NTSTATUS
AFSEnumerateDirectory( IN AFSFcb *Dcb,
                       IN OUT AFSDirHdr      *DirectoryHdr,
                       IN OUT AFSDirEntryCB **DirListHead,
                       IN OUT AFSDirEntryCB **DirListTail,
                       IN OUT AFSDirEntryCB **ShortNameTree,
                       IN BOOLEAN FastCall);

NTSTATUS
AFSEnumerateDirectoryNoResponse( IN AFSFcb *Dcb);

NTSTATUS
AFSVerifyDirectoryContent( IN AFSFcb *Dcb);

NTSTATUS
AFSNotifyFileCreate( IN AFSFcb *ParentDcb,
                     IN PLARGE_INTEGER FileSize,
                     IN ULONG FileAttributes,
                     IN UNICODE_STRING *FileName,
                     OUT AFSDirEntryCB **DirNode);

NTSTATUS
AFSUpdateFileInformation( IN PDEVICE_OBJECT DeviceObject,
                          IN AFSFcb *Fcb);

NTSTATUS
AFSNotifyDelete( IN AFSFcb *Fcb);

NTSTATUS
AFSNotifyRename( IN AFSFcb *Fcb,
                 IN AFSFcb *ParentDcb,
                 IN AFSFcb *TargetDcb,
                 IN UNICODE_STRING *TargetName);

NTSTATUS
AFSEvaluateTargetByID( IN AFSFileID *SourceFileId,
                       IN ULONGLONG ProcessID,
                       IN BOOLEAN FastCall,
                       OUT AFSDirEnumEntry **DirEnumEntry);

NTSTATUS
AFSEvaluateTargetByName( IN AFSFileID *ParentFileId,
                         IN PUNICODE_STRING SourceName,
                         OUT AFSDirEnumEntry **DirEnumEntry);

NTSTATUS
AFSRetrieveVolumeInformation( IN AFSFileID *FileID,
                              OUT AFSVolumeInfoCB *VolumeInformation);

NTSTATUS
AFSProcessRequest( IN ULONG RequestType,
                   IN ULONG RequestFlags,
                   IN ULONGLONG CallerProcess,
                   IN PUNICODE_STRING FileName,
                   IN AFSFileID *FileId,
                   IN void  *Data,
                   IN ULONG DataLength,
                   IN OUT void *ResultBuffer,
                   IN OUT PULONG ResultBufferLength);

NTSTATUS
AFSProcessControlRequest( IN PIRP Irp);

NTSTATUS
AFSInitIrpPool( void);

void
AFSCleanupIrpPool( void);

NTSTATUS 
AFSProcessIrpRequest( IN PIRP Irp);

NTSTATUS 
AFSProcessIrpResult( IN PIRP Irp);

NTSTATUS
AFSInsertRequest( IN AFSCommSrvcCB *CommSrvc,
                  IN AFSPoolEntry *Entry);

//
// AFSCreate.cpp Prototypes
//

NTSTATUS
AFSCreate( IN PDEVICE_OBJECT DeviceObject,
           IN PIRP Irp);

NTSTATUS
AFSCommonCreate( IN PDEVICE_OBJECT DeviceObject,
                 IN PIRP Irp);

NTSTATUS
AFSOpenAFSRoot( IN PIRP Irp,
                IN AFSFcb **Fcb,
                IN AFSCcb **Ccb);

NTSTATUS
AFSOpenRoot( IN PIRP Irp,
             IN AFSFcb *Fcb,
             IN AFSCcb **Ccb);

NTSTATUS
AFSProcessCreate( IN PIRP               Irp,
                  IN AFSFcb            *ParentDcb,
                  IN PUNICODE_STRING    FileName,
                  IN PUNICODE_STRING    ComponentName,
                  IN PUNICODE_STRING    FullFileName,
                  IN OUT AFSFcb       **Fcb,
                  IN OUT AFSCcb       **Ccb);

NTSTATUS
AFSOpenTargetDirectory( IN PIRP Irp,
                        IN AFSFcb *Fcb,
                        IN PUNICODE_STRING TargetName,
                        IN OUT AFSCcb **Ccb);

NTSTATUS
AFSProcessOpen( IN PIRP Irp,
                IN AFSFcb *ParentDcb,
                IN OUT AFSFcb *Fcb,
                IN OUT AFSCcb **Ccb);

NTSTATUS
AFSProcessOverwriteSupersede( IN PDEVICE_OBJECT DeviceObject,
                              IN PIRP           Irp,
                              IN AFSFcb        *ParentDcb,
                              IN AFSFcb        *Fcb,
                              IN AFSCcb       **Ccb);

NTSTATUS
AFSControlDeviceCreate( IN PIRP Irp);

NTSTATUS
AFSOpenIOCtlFcb( IN PIRP Irp,
                 IN AFSFcb *ParentDcb,
                 OUT AFSFcb **Fcb,
                 OUT AFSCcb **Ccb);

NTSTATUS
AFSOpenSpecialShareFcb( IN PIRP Irp,
                        IN AFSFcb *ParentDcb,
                        IN UNICODE_STRING *ShareName,
                        OUT AFSFcb **Fcb,
                        OUT AFSCcb **Ccb);

//
// AFSExtentsSupport.cpp Prototypes
//
VOID
AFSReferenceExtents( IN AFSFcb *Fcb);

VOID
AFSDereferenceExtents( IN AFSFcb *Fcb);

VOID 
AFSLockForExtentsTrim( IN AFSFcb *Fcb);

PAFSExtent
AFSExtentForOffset( IN AFSFcb *Fcb, 
                    IN PLARGE_INTEGER Offset,
                    IN BOOLEAN ReturnPrevious);
BOOLEAN
AFSExtentContains( IN AFSExtent *Extent, IN PLARGE_INTEGER Offset);


NTSTATUS
AFSRequestExtents( IN  AFSFcb *Fcb, 
                   IN  PLARGE_INTEGER Offset, 
                   IN  ULONG Size,
                   OUT BOOLEAN *FullyMApped);

BOOLEAN AFSDoExtentsMapRegion(IN AFSFcb *Fcb, 
                              IN PLARGE_INTEGER Offset, 
                              IN ULONG Size,
                              IN OUT AFSExtent **FirstExtent,
                              OUT AFSExtent **LastExtent);
    
NTSTATUS
AFSRequestExtentsAsync( IN AFSFcb *Fcb, 
                        IN PLARGE_INTEGER Offset, 
                        IN ULONG Size);

NTSTATUS
AFSWaitForExtentMapping ( IN AFSFcb *Fcb );

NTSTATUS
AFSProcessSetFileExtents( IN AFSSetFileExtentsCB *SetExtents );

NTSTATUS
AFSProcessReleaseFileExtents( IN PIRP Irp, IN BOOLEAN CallBack, OUT BOOLEAN *Complete);

NTSTATUS
AFSProcessReleaseFileExtentsDone( IN PIRP Irp );

VOID
AFSReleaseExtentsWork(AFSWorkItem *WorkItem);

NTSTATUS
AFSProcessSetExtents( IN AFSFcb *pFcb,
                      IN ULONG   Count,
                      IN AFSFileExtentCB *Result);

NTSTATUS
AFSFlushExtents( IN AFSFcb *pFcb);

VOID
AFSMarkDirty( IN AFSFcb *pFcb,
              IN PLARGE_INTEGER Offset,
              IN ULONG   Count);

NTSTATUS
AFSTearDownFcbExtents( IN AFSFcb *Fcb ) ;

void
AFSTrimExtents( IN AFSFcb *Fcb,
                IN PLARGE_INTEGER FileSize);

void
AFSTrimSpecifiedExtents( IN AFSFcb *Fcb,
                         IN ULONG   Count,
                         IN AFSFileExtentCB *Result);

//
//
// AFSIoSupp.cpp Prototypes
//
NTSTATUS
AFSGetExtents( IN  AFSFcb *pFcb,
               IN  PLARGE_INTEGER Offset,
               IN  ULONG Length,
               IN  AFSExtent *From,
               OUT ULONG *Count);

NTSTATUS
AFSSetupIoRun( IN PDEVICE_OBJECT CacheDevice,
               IN PIRP           MasterIrp,
               IN PVOID          SystemBuffer,
               IN OUT AFSIoRun  *IoRun,
               IN PLARGE_INTEGER Start,
               IN ULONG          Length,
               IN AFSExtent     *From,
               IN OUT ULONG     *Count);

NTSTATUS 
AFSStartIos( IN PFILE_OBJECT     CacheFileObject,
             IN UCHAR            FunctionCode,
             IN ULONG            IrpFlags,
             IN AFSIoRun        *IoRun,
             IN ULONG            Count,
             IN OUT AFSGatherIo *Gather);

VOID
AFSCompleteIo( IN AFSGatherIo *Gather, NTSTATUS Status);
//
// AFSClose.cpp Prototypes
//

NTSTATUS
AFSClose( IN PDEVICE_OBJECT DeviceObject,
          IN PIRP Irp);

//
// AFSFcbSupport.cpp Prototypes
//

NTSTATUS
AFSInitFcb( IN AFSFcb          *ParentFcb,
            IN AFSDirEntryCB   *DirEntry,
            IN OUT AFSFcb     **Fcb);

NTSTATUS
AFSInitAFSRoot( void);

NTSTATUS
AFSRemoveAFSRoot( void);

NTSTATUS
AFSInitRootFcb( IN AFSDirEntryCB *RootDirEntry,
                OUT AFSFcb **RootVcb);

NTSTATUS
AFSInitRootForMountPoint( IN AFSDirEnumEntryCB *MountPointDirEntry,
                          IN AFSVolumeInfoCB *VolumeInfoCB,
                          OUT AFSFcb **RootVcb);

void
AFSRemoveRootFcb( IN AFSFcb *RootFcb,
                  IN BOOLEAN CloseWorkerThread);

NTSTATUS
AFSInitCcb( IN AFSFcb     *Fcb,
            IN OUT AFSCcb **Ccb);

void
AFSRemoveFcb( IN AFSFcb *Fcb);

NTSTATUS
AFSRemoveCcb( IN AFSFcb *Fcb,
              IN AFSCcb *Ccb);

//
// AFSNameSupport.cpp Prototypes
//

NTSTATUS
AFSLocateNameEntry( IN AFSFcb *RootFcb,
                    IN PFILE_OBJECT FileObject,
                    IN UNICODE_STRING *FullPathName,
                    IN OUT AFSFcb **ParentFcb,
                    OUT AFSFcb **Fcb,
                    OUT PUNICODE_STRING ComponentName);

NTSTATUS
AFSCreateDirEntry( IN AFSFcb *ParentDcb,
                   IN PUNICODE_STRING FileName,
                   IN PUNICODE_STRING ComponentName,
                   IN ULONG Attributes,
                   IN OUT AFSDirEntryCB **DirEntry);

NTSTATUS
AFSGenerateShortName( IN AFSFcb *ParentDcb,
                      IN AFSDirEntryCB *DirNode);

void
AFSInsertDirectoryNode( IN AFSFcb *ParentDcb,
                        IN AFSDirEntryCB *DirEntry);

NTSTATUS
AFSDeleteDirEntry( IN AFSFcb *ParentDcb,
                   IN AFSDirEntryCB *DirEntry);

NTSTATUS
AFSRemoveDirNodeFromParent( IN AFSFcb *ParentDcb,
                            IN AFSDirEntryCB *DirEntry);

NTSTATUS
AFSFixupTargetName( IN OUT PUNICODE_STRING FileName,
                    IN OUT PUNICODE_STRING TargetFileName);

NTSTATUS 
AFSParseName( IN PIRP Irp,
              OUT PUNICODE_STRING FileName,
              OUT PUNICODE_STRING FullFileName,
              OUT BOOLEAN *FreeNameString,
              OUT AFSFcb **RootFcb,
              OUT AFSFcb **ParentFcb);

NTSTATUS
AFSCheckCellName( IN UNICODE_STRING *CellName,
                  OUT AFSDirEntryCB **ShareDirEntry);

NTSTATUS
AFSBuildTargetDirectory( IN ULONGLONG ProcessID,
                         IN AFSFcb *Fcb);

NTSTATUS
AFSProcessDFSLink( IN AFSFcb *Fcb,
                   IN PFILE_OBJECT FileObject,
                   IN UNICODE_STRING *RemainingPath);

//
// AFSNetworkProviderSupport.cpp
//

NTSTATUS
AFSAddConnection( IN AFSNetworkProviderConnectionCB *ConnectCB,
                  IN OUT PULONG ResultStatus,
                  IN OUT ULONG_PTR *ReturnOutputBufferLength);

NTSTATUS
AFSAddConnectionEx( IN UNICODE_STRING *RemoteName,
                    IN ULONG DisplayType,
                    IN ULONG Flags);

NTSTATUS
AFSCancelConnection( IN AFSNetworkProviderConnectionCB *ConnectCB,
                     IN OUT PULONG ResultStatus,
                     IN OUT ULONG_PTR *ReturnOutputBufferLength);

NTSTATUS
AFSGetConnection( IN AFSNetworkProviderConnectionCB *ConnectCB,
                  IN OUT WCHAR *RemoteName,
                  IN ULONG RemoteNameBufferLength,
                  IN OUT ULONG_PTR *ReturnOutputBufferLength);

NTSTATUS
AFSListConnections( IN OUT AFSNetworkProviderConnectionCB *ConnectCB,
                    IN ULONG ConnectionBufferLength,
                    IN OUT ULONG_PTR *ReturnOutputBufferLength);

void
AFSInitializeConnectionInfo( IN AFSProviderConnectionCB *Connection,
                             IN ULONG DisplayType);

AFSProviderConnectionCB *
AFSLocateEnumRootEntry( IN UNICODE_STRING *RemoteName);

NTSTATUS
AFSEnumerateConnection( IN OUT AFSNetworkProviderConnectionCB *ConnectCB,
                        IN AFSProviderConnectionCB *RootConnection,
                        IN ULONG BufferLength,
                        OUT PULONG CopiedLength);

NTSTATUS
AFSGetConnectionInfo( IN AFSNetworkProviderConnectionCB *ConnectCB,
                      IN ULONG BufferLength,
                      IN OUT ULONG_PTR *ReturnOutputBufferLength);

//
// AFSRead.cpp Prototypes
//

NTSTATUS
AFSCommonRead( IN PDEVICE_OBJECT DeviceObject,
               IN PIRP Irp,
               IN HANDLE OnBehalfOf);

NTSTATUS
AFSRead( IN PDEVICE_OBJECT DeviceObject,
         IN PIRP Irp);


NTSTATUS
AFSIOCtlRead( IN PDEVICE_OBJECT DeviceObject,
              IN PIRP Irp);

//
// AFSWrite.cpp Prototypes
//

NTSTATUS
AFSCommonWrite( IN PDEVICE_OBJECT DeviceObject,
          IN PIRP Irp,
          IN HANDLE CallingUser);

NTSTATUS
AFSWrite( IN PDEVICE_OBJECT DeviceObject,
                IN PIRP Irp);

NTSTATUS
AFSIOCtlWrite( IN PDEVICE_OBJECT DeviceObject,
               IN PIRP Irp);

//
// AFSFileInfo.cpp Prototypes
//

NTSTATUS
AFSQueryFileInfo( IN PDEVICE_OBJECT DeviceObject,
                  IN PIRP Irp);

NTSTATUS
AFSQueryBasicInfo( IN PIRP Irp,
                   IN AFSFcb *Fcb,
                   IN OUT PFILE_BASIC_INFORMATION Buffer,
                   IN OUT PLONG Length);

NTSTATUS
AFSQueryStandardInfo( IN PIRP Irp,
                      IN AFSFcb *Fcb,
                      IN OUT PFILE_STANDARD_INFORMATION Buffer,
                      IN OUT PLONG Length);

NTSTATUS
AFSQueryInternalInfo( IN PIRP Irp,
                      IN AFSFcb *Fcb,
                      IN OUT PFILE_INTERNAL_INFORMATION Buffer,
                      IN OUT PLONG Length);

NTSTATUS
AFSQueryEaInfo( IN PIRP Irp,
                IN AFSFcb *Fcb,
                IN OUT PFILE_EA_INFORMATION Buffer,
                IN OUT PLONG Length);

NTSTATUS
AFSQueryPositionInfo( IN PIRP Irp,
                      IN AFSFcb *Fcb,
                      IN OUT PFILE_POSITION_INFORMATION Buffer,
                      IN OUT PLONG Length);

NTSTATUS
AFSQueryNameInfo( IN PIRP Irp,
                  IN AFSFcb *Fcb,
                  IN OUT PFILE_NAME_INFORMATION Buffer,
                  IN OUT PLONG Length);

NTSTATUS
AFSQueryShortNameInfo( IN PIRP Irp,
                       IN AFSFcb *Fcb,
                       IN OUT PFILE_NAME_INFORMATION Buffer,
                       IN OUT PLONG Length);

NTSTATUS
AFSQueryNetworkInfo( IN PIRP Irp,
                     IN AFSFcb *Fcb,
                     IN OUT PFILE_NETWORK_OPEN_INFORMATION Buffer,
                     IN OUT PLONG Length);


NTSTATUS
AFSQueryAccess( IN PIRP Irp,
                IN AFSFcb *Fcb,
                IN OUT PFILE_ACCESS_INFORMATION Buffer,
                IN OUT PLONG Length);

NTSTATUS
AFSQueryMode( IN PIRP Irp,
              IN AFSFcb *Fcb,
              IN OUT PFILE_MODE_INFORMATION Buffer,
              IN OUT PLONG Length);

NTSTATUS
AFSQueryAlignment( IN PIRP Irp,
                   IN AFSFcb *Fcb,
                   IN OUT PFILE_ALIGNMENT_INFORMATION Buffer,
                   IN OUT PLONG Length);

NTSTATUS
AFSSetFileInfo( IN PDEVICE_OBJECT DeviceObject,
                IN PIRP Irp);

NTSTATUS
AFSSetBasicInfo( IN PIRP Irp,
                 IN AFSFcb *Fcb);

NTSTATUS
AFSSetDispositionInfo( IN PIRP Irp,
                       IN AFSFcb *Fcb);

NTSTATUS
AFSSetRenameInfo( IN PDEVICE_OBJECT DeviceObject,
                  IN PIRP Irp,
                  IN AFSFcb *Fcb);

NTSTATUS
AFSSetPositionInfo( IN PIRP Irp,
                    IN AFSFcb *Fcb);

NTSTATUS
AFSSetAllocationInfo( IN PIRP Irp,
                      IN AFSFcb *Fcb);

NTSTATUS
AFSSetEndOfFileInfo( IN PIRP Irp,
                     IN AFSFcb *Fcb);

//
// AFSEa.cpp Prototypes
//

NTSTATUS
AFSQueryEA( IN PDEVICE_OBJECT DeviceObject,
            IN PIRP Irp);

NTSTATUS
AFSSetEA( IN PDEVICE_OBJECT DeviceObject,
          IN PIRP Irp);

//
// AFSFlushBuffers.cpp Prototypes
//

NTSTATUS
AFSFlushBuffers( IN PDEVICE_OBJECT DeviceObject,
                 IN PIRP Irp);

//
// AFSVolumeInfo.cpp Prototypes
//

NTSTATUS
AFSQueryVolumeInfo( IN PDEVICE_OBJECT DeviceObject,
                    IN PIRP Irp);

NTSTATUS
AFSSetVolumeInfo( IN PDEVICE_OBJECT DeviceObject,
                  IN PIRP Irp);

NTSTATUS
AFSQueryFsVolumeInfo( IN AFSVolumeInfoCB *VolumeInfo,
                      IN PFILE_FS_VOLUME_INFORMATION Buffer,
                      IN OUT PULONG Length);

NTSTATUS
AFSQueryFsSizeInfo( IN AFSVolumeInfoCB *VolumeInfo,
                    IN PFILE_FS_SIZE_INFORMATION Buffer,
                    IN OUT PULONG Length);

NTSTATUS
AFSQueryFsDeviceInfo( IN AFSVolumeInfoCB *VolumeInfo,
                      IN PFILE_FS_DEVICE_INFORMATION Buffer,
                      IN OUT PULONG Length);

NTSTATUS
AFSQueryFsAttributeInfo( IN AFSVolumeInfoCB *VolumeInfo,
                         IN PFILE_FS_ATTRIBUTE_INFORMATION Buffer,
                         IN OUT PULONG Length);

NTSTATUS
AFSQueryFsFullSizeInfo( IN AFSVolumeInfoCB *VolumeInfo,
                        IN PFILE_FS_FULL_SIZE_INFORMATION Buffer,
                        IN OUT PULONG Length);

//
// AFSDirControl.cpp Prototypes
//

NTSTATUS
AFSDirControl( IN PDEVICE_OBJECT DeviceObject,
               IN PIRP Irp);

NTSTATUS 
AFSQueryDirectory( IN PIRP Irp);

NTSTATUS
AFSNotifyChangeDirectory( IN PIRP Irp);

//
// AFSFSControl.cpp Prototypes
//

NTSTATUS
AFSFSControl( IN PDEVICE_OBJECT DeviceObject,
              IN PIRP Irp);

NTSTATUS
AFSProcessUserFsRequest( IN PIRP Irp);

//
// AFSDevControl.cpp Prototypes
//

NTSTATUS
AFSDevControl( IN PDEVICE_OBJECT DeviceObject,
               IN PIRP Irp);

//
// AFSInternalDevControl.cpp Prototypes
//

NTSTATUS
AFSInternalDevControl( IN PDEVICE_OBJECT DeviceObject,
                       IN PIRP Irp);

//
// AFSShutdown.cpp Prototypes
//

NTSTATUS
AFSShutdown( IN PDEVICE_OBJECT DeviceObject,
             IN PIRP Irp);


NTSTATUS
AFSShutdownFilesystem( void);

//
// AFSLockControl.cpp Prototypes
//

NTSTATUS
AFSLockControl( IN PDEVICE_OBJECT DeviceObject,
                IN PIRP Irp);

//
// AFSCleanup.cpp Prototypes
//

NTSTATUS
AFSCleanup( IN PDEVICE_OBJECT DeviceObject,
            IN PIRP Irp);

//
// AFSSecurity.cpp Prototypes
//

NTSTATUS
AFSQuerySecurity( IN PDEVICE_OBJECT DeviceObject,
                  IN PIRP Irp);

NTSTATUS
AFSSetSecurity( IN PDEVICE_OBJECT DeviceObject,
                IN PIRP Irp);

//
// AFSSystemControl.cpp Prototypes
//

NTSTATUS
AFSSystemControl( IN PDEVICE_OBJECT DeviceObject,
                  IN PIRP Irp);

//
// AFSQuota.cpp Prototypes
//

NTSTATUS
AFSQueryQuota( IN PDEVICE_OBJECT DeviceObject,
               IN PIRP Irp);

NTSTATUS
AFSSetQuota( IN PDEVICE_OBJECT DeviceObject,
             IN PIRP Irp);

//
// AFSGeneric.cpp Prototypes
//

ULONG
AFSExceptionFilter( IN ULONG Code,
                    IN PEXCEPTION_POINTERS ExceptPtrs);

BOOLEAN
AFSAcquireExcl( IN PERESOURCE Resource,
                IN BOOLEAN wait);

BOOLEAN
AFSAcquireShared( IN PERESOURCE Resource,
                  IN BOOLEAN wait);

void
AFSReleaseResource( IN PERESOURCE Resource);

void
AFSConvertToShared( IN PERESOURCE Resource);

void
AFSCompleteRequest( IN PIRP Irp,
                    IN ULONG Status);

void 
AFSBuildCRCTable( void);

ULONG
AFSGenerateCRC( IN PUNICODE_STRING FileName,
                IN BOOLEAN UpperCaseName);

void *
AFSLockSystemBuffer( IN PIRP Irp,
                     IN ULONG Length);

void *
AFSMapToService( IN PIRP Irp,
                 IN ULONG ByteCount);

NTSTATUS
AFSUnmapServiceMappedBuffer( IN void *MappedBuffer,
                             IN PMDL Mdl);

NTSTATUS
AFSReadRegistry( IN PUNICODE_STRING RegistryPath);

NTSTATUS
AFSUpdateRegistryParameter( IN PUNICODE_STRING ValueName,
                            IN ULONG ValueType,
                            IN void *ValueData,
                            IN ULONG ValueDataLength);

NTSTATUS
AFSInitializeControlDevice( void);

NTSTATUS
AFSDefaultDispatch( IN PDEVICE_OBJECT DeviceObject,
                    IN PIRP Irp);

NTSTATUS
AFSInitializeDirectory( IN AFSFcb *Dcb);

NTSTATUS
AFSInitNonPagedDirEntry( IN AFSDirEntryCB *DirNode);

AFSDirEntryCB *
AFSInitDirEntry( IN AFSFileID *ParentFileID,
                 IN PUNICODE_STRING FileName,
                 IN PUNICODE_STRING TargetName,
                 IN AFSDirEnumEntry *DirEnumEntry,
                 IN ULONG FileIndex);

BOOLEAN
AFSCheckForReadOnlyAccess( IN ACCESS_MASK DesiredAccess);

NTSTATUS
AFSEvaluateNode( IN AFSFcb *Fcb);

NTSTATUS
AFSInvalidateCache( IN AFSInvalidateCacheCB *InvalidateCB);

BOOLEAN
AFSIsChildOfParent( IN AFSFcb *Dcb,
                    IN AFSFcb *Fcb);

inline
ULONGLONG
AFSCreateHighIndex( IN AFSFileID *FileID);

inline
ULONGLONG
AFSCreateLowIndex( IN AFSFileID *FileID);

BOOLEAN
AFSCheckAccess( IN ACCESS_MASK DesiredAccess,
                IN ACCESS_MASK GrantedAccess);

NTSTATUS
AFSGetDriverStatus( IN AFSDriverStatusRespCB *DriverStatus);

NTSTATUS
AFSSetSysNameInformation( IN AFSSysNameNotificationCB *SysNameInfo,
                          IN ULONG SysNameInfoBufferLength);

NTSTATUS
AFSSubstituteSysName( IN UNICODE_STRING *ComponentName,
                      IN UNICODE_STRING *SubstituteName,
                      IN ULONG StringIndex);

void
AFSResetSysNameList( IN AFSSysNameCB *SysNameList);

NTSTATUS
AFSSubstituteNameInPath( IN UNICODE_STRING *FullPathName,
                         IN UNICODE_STRING *ComponentName,
                         IN UNICODE_STRING *SubstituteName,
                         IN BOOLEAN FreePathName);

void
AFSInitServerStrings( void);

NTSTATUS
AFSReadServerName( void);

NTSTATUS
AFSInvalidateVolume( IN AFSFcb *Vcb,
                     IN ULONG Reason);

NTSTATUS
AFSVerifyEntry( IN AFSFcb *Fcb);

NTSTATUS
AFSSetVolumeState( IN AFSVolumeStatusCB *VolumeStatus);

NTSTATUS
AFSSetNetworkState( IN AFSNetworkStatusCB *NetworkStatus);

NTSTATUS
AFSValidateDirectoryCache( IN AFSFcb *Dcb);

NTSTATUS
AFSValidateRootDirectoryCache( IN AFSFcb *Dcb);

NTSTATUS
AFSWalkTargetChain( IN AFSFcb *ParentFcb,
                    OUT AFSFcb **TargetFcb);

BOOLEAN
AFSIsVolumeFID( IN AFSFileID *FileID);

BOOLEAN
AFSIsFinalNode( IN AFSFcb *Fcb);

void
AFSUpdateMetaData( IN AFSDirEntryCB *DirEntry,
                   IN AFSDirEnumEntry *DirEnumEntry);

NTSTATUS
AFSValidateEntry( IN AFSDirEntryCB *DirEntry,
                  IN BOOLEAN PurgeContent,
                  IN BOOLEAN FastCall);

BOOLEAN
AFSIsSpecialShareName( IN UNICODE_STRING *ShareName);

void
AFSInitializeSpecialShareNameList( void);

void
AFSWaitOnQueuedFlushes( IN AFSFcb *Fcb);

void
AFSWaitOnQueuedReleases();

//
// Prototypes in AFSFastIoSupprt.cpp
//

BOOLEAN
AFSFastIoCheckIfPossible( IN struct _FILE_OBJECT *FileObject,
                          IN PLARGE_INTEGER FileOffset,
                          IN ULONG Length,
                          IN BOOLEAN Wait,
                          IN ULONG LockKey,
                          IN BOOLEAN CheckForReadOperation,
                          OUT PIO_STATUS_BLOCK IoStatus,
                          IN struct _DEVICE_OBJECT *DeviceObject);

BOOLEAN
AFSFastIoRead( IN struct _FILE_OBJECT *FileObject,
               IN PLARGE_INTEGER FileOffset,
               IN ULONG Length,
               IN BOOLEAN Wait,
               IN ULONG LockKey,
               OUT PVOID Buffer,
               OUT PIO_STATUS_BLOCK IoStatus,
               IN struct _DEVICE_OBJECT *DeviceObject);

BOOLEAN
AFSFastIoWrite( IN struct _FILE_OBJECT *FileObject,
                IN PLARGE_INTEGER FileOffset,
                IN ULONG Length,
                IN BOOLEAN Wait,
                IN ULONG LockKey,
                IN PVOID Buffer,
                OUT PIO_STATUS_BLOCK IoStatus,
                IN struct _DEVICE_OBJECT *DeviceObject);

BOOLEAN
AFSFastIoQueryBasicInfo( IN struct _FILE_OBJECT *FileObject,
                         IN BOOLEAN Wait,
                         OUT PFILE_BASIC_INFORMATION Buffer,
                         OUT PIO_STATUS_BLOCK IoStatus,
                         IN struct _DEVICE_OBJECT *DeviceObject);

BOOLEAN
AFSFastIoQueryStandardInfo( IN struct _FILE_OBJECT *FileObject,
                            IN BOOLEAN Wait,
                            OUT PFILE_STANDARD_INFORMATION Buffer,
                            OUT PIO_STATUS_BLOCK IoStatus,
                            IN struct _DEVICE_OBJECT *DeviceObject);

BOOLEAN
AFSFastIoLock( IN struct _FILE_OBJECT *FileObject,
               IN PLARGE_INTEGER FileOffset,
               IN PLARGE_INTEGER Length,
               IN PEPROCESS ProcessId,
               IN ULONG Key,
               IN BOOLEAN FailImmediately,
               IN BOOLEAN ExclusiveLock,
               OUT PIO_STATUS_BLOCK IoStatus,
               IN struct _DEVICE_OBJECT *DeviceObject);

BOOLEAN
AFSFastIoUnlockSingle( IN struct _FILE_OBJECT *FileObject,
                       IN PLARGE_INTEGER FileOffset,
                       IN PLARGE_INTEGER Length,
                       IN PEPROCESS ProcessId,
                       IN ULONG Key,
                       OUT PIO_STATUS_BLOCK IoStatus,
                       IN struct _DEVICE_OBJECT *DeviceObject);

BOOLEAN
AFSFastIoUnlockAll( IN struct _FILE_OBJECT *FileObject,
                    IN PEPROCESS ProcessId,
                    OUT PIO_STATUS_BLOCK IoStatus,
                    IN struct _DEVICE_OBJECT *DeviceObject);

BOOLEAN
AFSFastIoUnlockAllByKey( IN struct _FILE_OBJECT *FileObject,
                         IN PVOID ProcessId,
                         IN ULONG Key,
                         OUT PIO_STATUS_BLOCK IoStatus,
                         IN struct _DEVICE_OBJECT *DeviceObject);

BOOLEAN
AFSFastIoDevCtrl( IN struct _FILE_OBJECT *FileObject,
                  IN BOOLEAN Wait,
                  IN PVOID InputBuffer OPTIONAL,
                  IN ULONG InputBufferLength,
                  OUT PVOID OutputBuffer OPTIONAL,
                  IN ULONG OutputBufferLength,
                  IN ULONG IoControlCode,
                  OUT PIO_STATUS_BLOCK IoStatus,
                  IN struct _DEVICE_OBJECT *DeviceObject);

VOID
AFSFastIoAcquireFile( IN struct _FILE_OBJECT *FileObject);

VOID
AFSFastIoReleaseFile( IN struct _FILE_OBJECT *FileObject);

VOID
AFSFastIoDetachDevice( IN struct _DEVICE_OBJECT *SourceDevice,
                       IN struct _DEVICE_OBJECT *TargetDevice);

BOOLEAN
AFSFastIoQueryNetworkOpenInfo( IN struct _FILE_OBJECT *FileObject,
                               IN BOOLEAN Wait,
                               OUT struct _FILE_NETWORK_OPEN_INFORMATION *Buffer,
                               OUT struct _IO_STATUS_BLOCK *IoStatus,
                               IN struct _DEVICE_OBJECT *DeviceObject);

BOOLEAN
AFSFastIoMdlRead( IN struct _FILE_OBJECT *FileObject,
                  IN PLARGE_INTEGER FileOffset,
                  IN ULONG Length,
                  IN ULONG LockKey,
                  OUT PMDL *MdlChain,
                  OUT PIO_STATUS_BLOCK IoStatus,
                  IN struct _DEVICE_OBJECT *DeviceObject);

BOOLEAN
AFSFastIoMdlReadComplete( IN struct _FILE_OBJECT *FileObject,
                          IN PMDL MdlChain,
                          IN struct _DEVICE_OBJECT *DeviceObject);

BOOLEAN
AFSFastIoPrepareMdlWrite( IN struct _FILE_OBJECT *FileObject,
                          IN PLARGE_INTEGER FileOffset,
                          IN ULONG Length,
                          IN ULONG LockKey,
                          OUT PMDL *MdlChain,
                          OUT PIO_STATUS_BLOCK IoStatus,
                          IN struct _DEVICE_OBJECT *DeviceObject);

BOOLEAN
AFSFastIoMdlWriteComplete( IN struct _FILE_OBJECT *FileObject,
                           IN PLARGE_INTEGER FileOffset,
                           IN PMDL MdlChain,
                           IN struct _DEVICE_OBJECT *DeviceObject);

NTSTATUS
AFSFastIoAcquireForModWrite( IN struct _FILE_OBJECT *FileObject,
                             IN PLARGE_INTEGER EndingOffset,
                             OUT struct _ERESOURCE **ResourceToRelease,
                             IN struct _DEVICE_OBJECT *DeviceObject);

NTSTATUS
AFSFastIoReleaseForModWrite( IN struct _FILE_OBJECT *FileObject,
                             IN struct _ERESOURCE *ResourceToRelease,
                             IN struct _DEVICE_OBJECT *DeviceObject);

NTSTATUS
AFSFastIoAcquireForCCFlush( IN struct _FILE_OBJECT *FileObject,
                            IN struct _DEVICE_OBJECT *DeviceObject);

NTSTATUS
AFSFastIoReleaseForCCFlush( IN struct _FILE_OBJECT *FileObject,
                            IN struct _DEVICE_OBJECT *DeviceObject);

BOOLEAN
AFSFastIoReadCompressed( IN struct _FILE_OBJECT *FileObject,
                         IN PLARGE_INTEGER FileOffset,
                         IN ULONG Length,
                         IN ULONG LockKey,
                         OUT PVOID Buffer,
                         OUT PMDL *MdlChain,
                         OUT PIO_STATUS_BLOCK IoStatus,
                         OUT struct _COMPRESSED_DATA_INFO *CompressedDataInfo,
                         IN ULONG CompressedDataInfoLength,
                         IN struct _DEVICE_OBJECT *DeviceObject);

BOOLEAN
AFSFastIoWriteCompressed( IN struct _FILE_OBJECT *FileObject,
                          IN PLARGE_INTEGER FileOffset,
                          IN ULONG Length,
                          IN ULONG LockKey,
                          IN PVOID Buffer,
                          OUT PMDL *MdlChain,
                          OUT PIO_STATUS_BLOCK IoStatus,
                          IN struct _COMPRESSED_DATA_INFO *CompressedDataInfo,
                          IN ULONG CompressedDataInfoLength,
                          IN struct _DEVICE_OBJECT *DeviceObject);

BOOLEAN
AFSFastIoMdlReadCompleteCompressed( IN struct _FILE_OBJECT *FileObject,
                                    IN PMDL MdlChain,
                                    IN struct _DEVICE_OBJECT *DeviceObject);

BOOLEAN
AFSFastIoMdlWriteCompleteCompressed( IN struct _FILE_OBJECT *FileObject,
                                     IN PLARGE_INTEGER FileOffset,
                                     IN PMDL MdlChain,
                                     IN struct _DEVICE_OBJECT *DeviceObject);

BOOLEAN
AFSFastIoQueryOpen( IN struct _IRP *Irp,
                    OUT PFILE_NETWORK_OPEN_INFORMATION NetworkInformation,
                    IN struct _DEVICE_OBJECT *DeviceObject);

BOOLEAN
AFSAcquireFcbForLazyWrite( IN PVOID Fcb,
                           IN BOOLEAN Wait);

VOID
AFSReleaseFcbFromLazyWrite( IN PVOID Fcb);

BOOLEAN
AFSAcquireFcbForReadAhead( IN PVOID Fcb,
                           IN BOOLEAN Wait);

VOID
AFSReleaseFcbFromReadAhead( IN PVOID Fcb);

//
// AFSWorker.cpp Prototypes
//

NTSTATUS
AFSInitializeWorkerPool( void);

NTSTATUS
AFSRemoveWorkerPool( void);

NTSTATUS
AFSInitWorkerThread( IN AFSWorkQueueContext *PoolContext);

NTSTATUS
AFSShutdownWorkerThread( IN AFSWorkQueueContext *PoolContext);

void
AFSWorkerThread( IN PVOID Context);

void
AFSVolumeWorkerThread( IN PVOID Context);

NTSTATUS
AFSInsertWorkitem( IN AFSWorkItem *WorkItem);

NTSTATUS
AFSInsertWorkitemAtHead( IN AFSWorkItem *WorkItem);

AFSWorkItem *
AFSRemoveWorkItem( void);

NTSTATUS
AFSQueueWorkerRequest( IN AFSWorkItem *WorkItem);

NTSTATUS
AFSQueueWorkerRequestAtHead( IN AFSWorkItem *WorkItem);

NTSTATUS
AFSInitVolumeWorker( IN AFSFcb *VolumeVcb);

NTSTATUS
AFSShutdownVolumeWorker( IN AFSFcb *VolumeVcb);

NTSTATUS
AFSQueueBuildTargetDirectory( IN AFSFcb *Fcb);

NTSTATUS
AFSQueueFlushExtents( IN AFSFcb *Fcb);

NTSTATUS
AFSQueueExtentRelease( IN PIRP Irp);

NTSTATUS
AFSQueueAsyncRead( IN PDEVICE_OBJECT DeviceObject,
                   IN PIRP Irp,
                   IN HANDLE CallerProcess);

NTSTATUS
AFSQueueAsyncWrite( IN PDEVICE_OBJECT DeviceObject,
                    IN PIRP Irp,
                    IN HANDLE CallerProcess);

//
// AFSRDRSupport.cpp Prototypes
//

NTSTATUS
AFSInitRDRDevice( void);

void
AFSDeleteRDRDevice( void);

NTSTATUS
AFSRDRDeviceControl( IN PDEVICE_OBJECT DeviceObject,
                     IN PIRP Irp);

NTSTATUS
AFSInitializeRedirector( IN AFSRedirectorInitInfo *CacheFileInfo);

NTSTATUS
AFSCloseRedirector( void);

NTSTATUS
AFSShutdownRedirector( void);

//
// AFSLogSupport.cpp
//

NTSTATUS
AFSDbgLogMsg( IN ULONG Subsystem,
              IN ULONG Level,
              IN PCCH Format,
              ...);

NTSTATUS
AFSInitializeDbgLog( void);

NTSTATUS
AFSTearDownDbgLog( void);

NTSTATUS
AFSConfigureTrace( IN AFSTraceConfigCB *TraceInfo);

NTSTATUS
AFSGetTraceBuffer( IN ULONG TraceBufferLength,
                   OUT void *TraceBuffer,
                   OUT ULONG_PTR *CopiedLength);

void
AFSTagInitialLogEntry( void);

//
// AFSProcessSupport.cpp Prototypes
//

void
AFSProcessNotify( IN HANDLE  ParentId,
                  IN HANDLE  ProcessId,
                  IN BOOLEAN  Create);

void
AFSValidateProcessEntry( void);

BOOLEAN
AFSIs64BitProcess( IN ULONGLONG ProcessId);


};

#endif /* _AFS_COMMON_H */
