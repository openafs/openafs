/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kernel Drivers, LLC.
 * Copyright (c) 2009, 2010, 2011 Your File System, Inc.
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
 * - Neither the names of Kernel Drivers, LLC and Your File System, Inc.
 *   nor the names of their contributors may be used to endorse or promote
 *   products derived from this software without specific prior written
 *   permission from Kernel Drivers, LLC and Your File System, Inc.
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
#include <ntintsafe.h>

#include "AFSDefines.h"

#include "AFSUserDefines.h"

#include "AFSUserIoctl.h"

#include "AFSUserStructs.h"

#include "AFSRedirCommonDefines.h"

#include "AFSRedirCommonStructs.h"

#include "AFSStructs.h"

#include "AFSProvider.h"

#ifndef NO_EXTERN
#include "AFSExtern.h"
#endif

#define NTSTRSAFE_LIB
#include "ntstrsafe.h"

NTSTATUS
ZwQueryInformationProcess(
  __in       HANDLE ProcessHandle,
  __in       PROCESSINFOCLASS ProcessInformationClass,
  __out      PVOID ProcessInformation,
  __in       ULONG ProcessInformationLength,
  __out_opt  PULONG ReturnLength
);

NTSYSAPI
NTSTATUS
NTAPI
RtlAbsoluteToSelfRelativeSD( IN PSECURITY_DESCRIPTOR  AbsoluteSecurityDescriptor,
                             OUT PSECURITY_DESCRIPTOR  SelfRelativeSecurityDescriptor,
                             IN OUT PULONG  BufferLength );

#ifndef FILE_OPEN_REPARSE_POINT
#define FILE_OPEN_REPARSE_POINT                 0x00200000
#endif
//
// AFSBTreeSupport.cpp Prototypes
//

NTSTATUS
AFSLocateCaseSensitiveDirEntry( IN AFSDirectoryCB *RootNode,
                                IN ULONG Index,
                                IN OUT AFSDirectoryCB **DirEntry);

NTSTATUS
AFSLocateCaseInsensitiveDirEntry( IN AFSDirectoryCB *RootNode,
                                  IN ULONG Index,
                                  IN OUT AFSDirectoryCB **DirEntry);

NTSTATUS
AFSInsertCaseSensitiveDirEntry( IN AFSDirectoryCB *RootNode,
                                IN AFSDirectoryCB *DirEntry);

NTSTATUS
AFSInsertCaseInsensitiveDirEntry( IN AFSDirectoryCB *RootNode,
                                  IN AFSDirectoryCB *DirEntry);

NTSTATUS
AFSRemoveCaseSensitiveDirEntry( IN AFSDirectoryCB **RootNode,
                                IN AFSDirectoryCB *DirEntry);

NTSTATUS
AFSRemoveCaseInsensitiveDirEntry( IN AFSDirectoryCB **RootNode,
                                  IN AFSDirectoryCB *DirEntry);

NTSTATUS
AFSLocateShortNameDirEntry( IN AFSDirectoryCB *RootNode,
                            IN ULONG Index,
                            IN OUT AFSDirectoryCB **DirEntry);

NTSTATUS
AFSInsertShortNameDirEntry( IN AFSDirectoryCB *RootNode,
                            IN AFSDirectoryCB *DirEntry);

NTSTATUS
AFSRemoveShortNameDirEntry( IN AFSDirectoryCB **RootNode,
                            IN AFSDirectoryCB *DirEntry);

NTSTATUS
AFSLocateHashEntry( IN AFSBTreeEntry *TopNode,
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
AFSEnumerateDirectory( IN GUID *AuthGroup,
                       IN AFSObjectInfoCB *ObjectInfoCB,
                       IN BOOLEAN   FastQuery);

NTSTATUS
AFSEnumerateDirectoryNoResponse( IN GUID *AuthGroup,
                                 IN AFSFileID *FileId);

NTSTATUS
AFSVerifyDirectoryContent( IN AFSObjectInfoCB *ObjectInfo,
                           IN GUID *AuthGroup);

NTSTATUS
AFSNotifyFileCreate( IN GUID            *AuthGroup,
                     IN AFSObjectInfoCB *ParentObjectInfo,
                     IN PLARGE_INTEGER FileSize,
                     IN ULONG FileAttributes,
                     IN UNICODE_STRING *FileName,
                     OUT AFSDirectoryCB **DirNode);

NTSTATUS
AFSUpdateFileInformation( IN AFSFileID *ParentFid,
                          IN AFSObjectInfoCB *ObjectInfo,
                          IN GUID *AuthGroup);

NTSTATUS
AFSNotifyDelete( IN AFSDirectoryCB *DirectoryCB,
                 IN GUID           *AuthGroup,
                 IN BOOLEAN         CheckOnly);

NTSTATUS
AFSNotifyRename( IN AFSObjectInfoCB *ObjectInfo,
                 IN GUID            *AuthGroup,
                 IN AFSObjectInfoCB *ParentObjectInfo,
                 IN AFSObjectInfoCB *TargetParentObjectInfo,
                 IN AFSDirectoryCB *DirectoryCB,
                 IN UNICODE_STRING *TargetName,
                 OUT AFSFileID  *UpdatedFID);

NTSTATUS
AFSEvaluateTargetByID( IN AFSObjectInfoCB *ObjectInfo,
                       IN GUID *AuthGroup,
                       IN BOOLEAN FastCall,
                       OUT AFSDirEnumEntry **DirEnumEntry);

NTSTATUS
AFSEvaluateTargetByName( IN GUID *AuthGroup,
                         IN AFSObjectInfoCB *ParentObjectInfo,
                         IN PUNICODE_STRING SourceName,
                         OUT AFSDirEnumEntry **DirEnumEntry);

NTSTATUS
AFSRetrieveVolumeInformation( IN GUID *AuthGroup,
                              IN AFSFileID *FileID,
                              OUT AFSVolumeInfoCB *VolumeInformation);

NTSTATUS
AFSRetrieveVolumeSizeInformation( IN GUID *AuthGroup,
                                  IN AFSFileID *FileID,
                                  OUT AFSVolumeSizeInfoCB *VolumeSizeInformation);

NTSTATUS
AFSNotifyPipeTransceive( IN AFSCcb *Ccb,
                         IN ULONG InputLength,
                         IN ULONG OutputLength,
                         IN void *InputDataBuffer,
                         OUT void *OutputDataBuffer,
                         OUT ULONG *BytesReturned);

NTSTATUS
AFSNotifySetPipeInfo( IN AFSCcb *Ccb,
                      IN ULONG InformationClass,
                      IN ULONG InputLength,
                      IN void *DataBuffer);

NTSTATUS
AFSNotifyQueryPipeInfo( IN AFSCcb *Ccb,
                        IN ULONG InformationClass,
                        IN ULONG OutputLength,
                        IN void *DataBuffer,
                        OUT ULONG *BytesReturned);

NTSTATUS
AFSReleaseFid( IN AFSFileID *FileId);

BOOLEAN
AFSIsExtentRequestQueued( IN AFSFileID *FileID,
                          IN LARGE_INTEGER *ExtentOffset,
                          IN ULONG Length);

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
AFSOpenRedirector( IN PIRP Irp,
                   IN AFSFcb **Fcb,
                   IN AFSCcb **Ccb);

NTSTATUS
AFSOpenAFSRoot( IN PIRP Irp,
                IN AFSFcb **Fcb,
                IN AFSCcb **Ccb);

NTSTATUS
AFSOpenRoot( IN PIRP Irp,
             IN AFSVolumeCB *VolumeCB,
             IN GUID *AuthGroup,
             OUT AFSFcb **Fcb,
             OUT AFSCcb **Ccb);

NTSTATUS
AFSProcessCreate( IN PIRP               Irp,
                  IN GUID              *AuthGroup,
                  IN AFSVolumeCB       *VolumeCB,
                  IN AFSDirectoryCB    *ParentDirCB,
                  IN PUNICODE_STRING    FileName,
                  IN PUNICODE_STRING    ComponentName,
                  IN PUNICODE_STRING    FullFileName,
                  OUT AFSFcb          **Fcb,
                  OUT AFSCcb          **Ccb);

NTSTATUS
AFSOpenTargetDirectory( IN PIRP Irp,
                        IN AFSVolumeCB *VolumeCB,
                        IN AFSDirectoryCB *ParentDirectoryCB,
                        IN AFSDirectoryCB *TargetDirectoryCB,
                        IN UNICODE_STRING *TargetName,
                        OUT AFSFcb **Fcb,
                        OUT AFSCcb **Ccb);

NTSTATUS
AFSProcessOpen( IN PIRP Irp,
                IN GUID *AuthGroup,
                IN AFSVolumeCB *VolumeCB,
                IN AFSDirectoryCB *ParentDirCB,
                IN AFSDirectoryCB *DirectoryCB,
                OUT AFSFcb **Fcb,
                OUT AFSCcb **Ccb);

NTSTATUS
AFSProcessOverwriteSupersede( IN PDEVICE_OBJECT DeviceObject,
                              IN PIRP           Irp,
                              IN AFSVolumeCB   *VolumeCB,
                              IN GUID          *AuthGroup,
                              IN AFSDirectoryCB *ParentDirCB,
                              IN AFSDirectoryCB *DirectoryCB,
                              OUT AFSFcb       **Fcb,
                              OUT AFSCcb       **Ccb);

NTSTATUS
AFSControlDeviceCreate( IN PIRP Irp);

NTSTATUS
AFSOpenIOCtlFcb( IN PIRP Irp,
                 IN GUID *AuthGroup,
                 IN AFSDirectoryCB *ParentDirCB,
                 OUT AFSFcb **Fcb,
                 OUT AFSCcb **Ccb);

NTSTATUS
AFSOpenSpecialShareFcb( IN PIRP Irp,
                        IN GUID *AuthGroup,
                        IN AFSDirectoryCB *DirectoryCB,
                        OUT AFSFcb **Fcb,
                        OUT AFSCcb **Ccb);

//
// AFSExtentsSupport.cpp Prototypes
//
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
                   IN  AFSCcb *Ccb,
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
                        IN AFSCcb *Ccb,
                        IN PLARGE_INTEGER Offset,
                        IN ULONG Size);

NTSTATUS
AFSWaitForExtentMapping ( IN AFSFcb *Fcb,
                          IN AFSCcb *Ccb);

NTSTATUS
AFSProcessSetFileExtents( IN AFSSetFileExtentsCB *SetExtents );

NTSTATUS
AFSProcessReleaseFileExtents( IN PIRP Irp);

NTSTATUS
AFSProcessExtentFailure( PIRP Irp);

NTSTATUS
AFSProcessSetExtents( IN AFSFcb *pFcb,
                      IN ULONG   Count,
                      IN AFSFileExtentCB *Result);

NTSTATUS
AFSFlushExtents( IN AFSFcb *pFcb,
                 IN GUID *AuthGroup);

NTSTATUS
AFSReleaseExtentsWithFlush( IN AFSFcb *Fcb,
                            IN GUID *AuthGroup,
                            IN BOOLEAN bReleaseAll);

NTSTATUS
AFSReleaseCleanExtents( IN AFSFcb *Fcb,
                        IN GUID *AuthGroup);

VOID
AFSMarkDirty( IN AFSFcb *pFcb,
              IN AFSExtent *StartExtent,
              IN ULONG ExtentsCount,
              IN LARGE_INTEGER *StartingByte,
              IN BOOLEAN DerefExtents);

VOID
AFSTearDownFcbExtents( IN AFSFcb *Fcb,
                       IN GUID *AuthGroup);

void
AFSTrimExtents( IN AFSFcb *Fcb,
                IN PLARGE_INTEGER FileSize);

void
AFSTrimSpecifiedExtents( IN AFSFcb *Fcb,
                         IN ULONG   Count,
                         IN AFSFileExtentCB *Result);

void
AFSReferenceActiveExtents( IN AFSExtent *StartExtent,
                           IN ULONG ExtentsCount);

void
AFSDereferenceActiveExtents( IN AFSExtent *StartExtent,
                             IN ULONG ExtentsCount);

void
AFSRemoveEntryDirtyList( IN AFSFcb *Fcb,
                         IN AFSExtent *Extent);

AFSExtent *
ExtentFor( PLIST_ENTRY le, ULONG SkipList );

AFSExtent *
NextExtent( AFSExtent *Extent, ULONG SkipList );

ULONG
AFSConstructCleanByteRangeList( AFSFcb * pFcb,
                                AFSByteRange ** pByteRangeList);

#if GEN_MD5
void
AFSSetupMD5Hash( IN AFSFcb *Fcb,
                 IN AFSExtent *StartExtent,
                 IN ULONG ExtentsCount,
                 IN void *SystemBuffer,
                 IN LARGE_INTEGER *ByteOffset,
                 IN ULONG ByteCount);
#endif

//
//
// AFSIoSupp.cpp Prototypes
//
NTSTATUS
AFSGetExtents( IN  AFSFcb *pFcb,
               IN  PLARGE_INTEGER Offset,
               IN  ULONG Length,
               IN  AFSExtent *From,
               OUT ULONG *ExtentCount,
               OUT ULONG *RunCount);

NTSTATUS
AFSSetupIoRun( IN PDEVICE_OBJECT CacheDevice,
               IN PIRP           MasterIrp,
               IN PVOID          SystemBuffer,
               IN OUT AFSIoRun  *IoRun,
               IN PLARGE_INTEGER Start,
               IN ULONG          Length,
               IN AFSExtent     *From,
               IN OUT ULONG     *RunCount);

NTSTATUS
AFSStartIos( IN FILE_OBJECT     *CacheFileObject,
             IN UCHAR            FunctionCode,
             IN ULONG            IrpFlags,
             IN AFSIoRun        *IoRun,
             IN ULONG            Count,
             IN OUT AFSGatherIo *Gather);

VOID
AFSCompleteIo( IN AFSGatherIo *Gather,
               IN NTSTATUS Status);

NTSTATUS
AFSProcessExtentRun( IN PVOID          SystemBuffer,
                     IN PLARGE_INTEGER Start,
                     IN ULONG          Length,
                     IN AFSExtent     *From,
                     IN BOOLEAN        WriteRequest);

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
AFSInitFcb( IN AFSDirectoryCB   *DirEntry);

NTSTATUS
AFSInitVolume( IN GUID *AuthGroup,
               IN AFSFileID *RootFid,
               OUT AFSVolumeCB **VolumeCB);

NTSTATUS
AFSRemoveVolume( IN AFSVolumeCB *VolumeCB);

NTSTATUS
AFSInitRootFcb( IN ULONGLONG ProcessID,
                IN AFSVolumeCB *VolumeCB);

void
AFSRemoveRootFcb( IN AFSFcb *RootFcb);

NTSTATUS
AFSInitCcb( IN OUT AFSCcb **Ccb);

void
AFSRemoveFcb( IN AFSFcb **Fcb);

NTSTATUS
AFSRemoveCcb( IN AFSFcb *Fcb,
              IN AFSCcb *Ccb);

NTSTATUS
AFSInsertCcb( IN AFSFcb *Fcb,
              IN AFSCcb *Ccb);

//
// AFSNameSupport.cpp Prototypes
//

NTSTATUS
AFSLocateNameEntry( IN GUID *AuthGroup,
                    IN PFILE_OBJECT FileObject,
                    IN UNICODE_STRING *RootPathName,
                    IN UNICODE_STRING *ParsedPathName,
                    IN AFSNameArrayHdr *NameArray,
                    IN ULONG Flags,
                    OUT AFSVolumeCB **VolumeCB,
                    IN OUT AFSDirectoryCB **ParentDirectoryCB,
                    OUT AFSDirectoryCB **DirectoryCB,
                    OUT PUNICODE_STRING ComponentName);

NTSTATUS
AFSCreateDirEntry( IN GUID            *AuthGroup,
                   IN AFSObjectInfoCB *ParentObjectInfo,
                   IN AFSDirectoryCB *ParentDirCB,
                   IN PUNICODE_STRING FileName,
                   IN PUNICODE_STRING ComponentName,
                   IN ULONG Attributes,
                   IN OUT AFSDirectoryCB **DirEntry);

void
AFSInsertDirectoryNode( IN AFSObjectInfoCB *ParentObjectInfo,
                        IN AFSDirectoryCB *DirEntry,
                        IN BOOLEAN InsertInEnumList);

NTSTATUS
AFSDeleteDirEntry( IN AFSObjectInfoCB *ParentObjectInfo,
                   IN AFSDirectoryCB *DirEntry);

NTSTATUS
AFSRemoveDirNodeFromParent( IN AFSObjectInfoCB *ParentObjectInfo,
                            IN AFSDirectoryCB *DirEntry,
                            IN BOOLEAN RemoveFromEnumList);

NTSTATUS
AFSFixupTargetName( IN OUT PUNICODE_STRING FileName,
                    IN OUT PUNICODE_STRING TargetFileName);

NTSTATUS
AFSParseName( IN PIRP Irp,
              IN GUID *AuthGroup,
              OUT PUNICODE_STRING FileName,
              OUT PUNICODE_STRING ParsedFileName,
              OUT PUNICODE_STRING RootFileName,
              OUT ULONG *ParseFlags,
              OUT AFSVolumeCB **VolumeCB,
              OUT AFSDirectoryCB **ParentDirectoryCB,
              OUT AFSNameArrayHdr **NameArray);

NTSTATUS
AFSCheckCellName( IN GUID *AuthGroup,
                  IN UNICODE_STRING *CellName,
                  OUT AFSDirectoryCB **ShareDirEntry);

NTSTATUS
AFSBuildMountPointTarget( IN GUID *AuthGroup,
                          IN AFSDirectoryCB *DirectoryCB,
                          OUT AFSVolumeCB **VolumeCB);

NTSTATUS
AFSBuildRootVolume( IN GUID *AuthGroup,
                    IN AFSFileID *FileId,
                    OUT AFSVolumeCB **TargetVolumeCB);

NTSTATUS
AFSProcessDFSLink( IN AFSDirectoryCB *DirEntry,
                   IN PFILE_OBJECT FileObject,
                   IN UNICODE_STRING *RemainingPath,
                   IN GUID *AuthGroup);

//
// AFSNetworkProviderSupport.cpp
//

NTSTATUS
AFSAddConnection( IN AFSNetworkProviderConnectionCB *ConnectCB,
                  IN OUT PULONG ResultStatus,
                  IN OUT ULONG_PTR *ReturnOutputBufferLength);

NTSTATUS
AFSCancelConnection( IN AFSNetworkProviderConnectionCB *ConnectCB,
                     IN OUT AFSCancelConnectionResultCB *ConnectionResult,
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

BOOLEAN
AFSIsDriveMapped( IN WCHAR DriveMapping);

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

NTSTATUS
AFSShareRead( IN PDEVICE_OBJECT DeviceObject,
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

NTSTATUS
AFSShareWrite( IN PDEVICE_OBJECT DeviceObject,
               IN PIRP Irp);

//
// AFSFileInfo.cpp Prototypes
//

NTSTATUS
AFSQueryFileInfo( IN PDEVICE_OBJECT DeviceObject,
                  IN PIRP Irp);

NTSTATUS
AFSQueryBasicInfo( IN PIRP Irp,
                   IN AFSDirectoryCB *DirectoryCB,
                   IN OUT PFILE_BASIC_INFORMATION Buffer,
                   IN OUT PLONG Length);

NTSTATUS
AFSQueryStandardInfo( IN PIRP Irp,
                      IN AFSDirectoryCB *DirectoryCB,
                      IN OUT PFILE_STANDARD_INFORMATION Buffer,
                      IN OUT PLONG Length);

NTSTATUS
AFSQueryInternalInfo( IN PIRP Irp,
                      IN AFSFcb *Fcb,
                      IN OUT PFILE_INTERNAL_INFORMATION Buffer,
                      IN OUT PLONG Length);

NTSTATUS
AFSQueryEaInfo( IN PIRP Irp,
                IN AFSDirectoryCB *DirectoryCB,
                IN OUT PFILE_EA_INFORMATION Buffer,
                IN OUT PLONG Length);

NTSTATUS
AFSQueryPositionInfo( IN PIRP Irp,
                      IN AFSFcb *Fcb,
                      IN OUT PFILE_POSITION_INFORMATION Buffer,
                      IN OUT PLONG Length);

NTSTATUS
AFSQueryNameInfo( IN PIRP Irp,
                  IN AFSDirectoryCB *DirectoryCB,
                  IN OUT PFILE_NAME_INFORMATION Buffer,
                  IN OUT PLONG Length);

NTSTATUS
AFSQueryShortNameInfo( IN PIRP Irp,
                       IN AFSDirectoryCB *DirectoryCB,
                       IN OUT PFILE_NAME_INFORMATION Buffer,
                       IN OUT PLONG Length);

NTSTATUS
AFSQueryNetworkInfo( IN PIRP Irp,
                     IN AFSDirectoryCB *DirectoryCB,
                     IN OUT PFILE_NETWORK_OPEN_INFORMATION Buffer,
                     IN OUT PLONG Length);

NTSTATUS
AFSQueryStreamInfo( IN PIRP Irp,
                    IN AFSDirectoryCB *DirectoryCB,
                    IN OUT FILE_STREAM_INFORMATION *Buffer,
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
AFSQueryAttribTagInfo( IN PIRP Irp,
                       IN AFSDirectoryCB *DirectoryCB,
                       IN OUT FILE_ATTRIBUTE_TAG_INFORMATION *Buffer,
                       IN OUT PLONG Length);

NTSTATUS
AFSQueryRemoteProtocolInfo( IN PIRP Irp,
                            IN AFSDirectoryCB *DirectoryCB,
                            IN OUT FILE_REMOTE_PROTOCOL_INFORMATION *Buffer,
                            IN OUT PLONG Length);

NTSTATUS
AFSQueryPhysicalNameInfo( IN PIRP Irp,
                          IN AFSDirectoryCB *DirectoryCB,
                          IN OUT PFILE_NETWORK_PHYSICAL_NAME_INFORMATION Buffer,
                          IN OUT PLONG Length);

NTSTATUS
AFSSetFileInfo( IN PDEVICE_OBJECT DeviceObject,
                IN PIRP Irp);

NTSTATUS
AFSSetBasicInfo( IN PIRP Irp,
                 IN AFSDirectoryCB *DirectoryCB);

NTSTATUS
AFSSetDispositionInfo( IN PIRP Irp,
                       IN AFSDirectoryCB *DirectoryCB);

NTSTATUS
AFSSetRenameInfo( IN PIRP Irp);

NTSTATUS
AFSSetPositionInfo( IN PIRP Irp,
                    IN AFSDirectoryCB *DirectoryCB);

NTSTATUS
AFSSetAllocationInfo( IN PIRP Irp,
                      IN AFSDirectoryCB *DirectoryCB);

NTSTATUS
AFSSetEndOfFileInfo( IN PIRP Irp,
                     IN AFSDirectoryCB *DirectoryCB);

NTSTATUS
AFSProcessShareSetInfo( IN IRP *Irp,
                        IN AFSFcb *Fcb,
                        IN AFSCcb *Ccb);

NTSTATUS
AFSProcessShareQueryInfo( IN IRP *Irp,
                          IN AFSFcb *Fcb,
                          IN AFSCcb *Ccb);

NTSTATUS
AFSProcessPIOCtlQueryInfo( IN IRP *Irp,
                           IN AFSFcb *Fcb,
                           IN AFSCcb *Ccb,
                           IN OUT LONG *Length);

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

AFSDirectoryCB *
AFSLocateNextDirEntry( IN AFSObjectInfoCB *ObjectInfo,
                       IN AFSCcb *Ccb);

AFSDirectoryCB *
AFSLocateDirEntryByIndex( IN AFSObjectInfoCB *ObjectInfo,
                          IN AFSCcb *Ccb,
                          IN ULONG DirIndex);

NTSTATUS
AFSSnapshotDirectory( IN AFSFcb *Fcb,
                      IN AFSCcb *Ccb,
                      IN BOOLEAN ResetIndex);

NTSTATUS
AFSFsRtlNotifyFullChangeDirectory( IN AFSObjectInfoCB *ObjectInfo,
                                   IN AFSCcb *Context,
                                   IN BOOLEAN WatchTree,
                                   IN ULONG CompletionFilter,
                                   IN PIRP NotifyIrp);

void
AFSFsRtlNotifyFullReportChange( IN AFSObjectInfoCB *ObjectInfo,
                                IN AFSCcb *Ccb,
                                IN ULONG NotifyFilter,
                                IN ULONG NotificationAction);

BOOLEAN
AFSNotifyReportChangeCallback( IN void *NotifyContext,
                               IN void *FilterContext);

BOOLEAN
AFSIsNameInSnapshot( IN AFSSnapshotHdr *SnapshotHdr,
                     IN ULONG HashIndex);

//
// AFSFSControl.cpp Prototypes
//

NTSTATUS
AFSFSControl( IN PDEVICE_OBJECT DeviceObject,
              IN PIRP Irp);

NTSTATUS
AFSProcessUserFsRequest( IN PIRP Irp);

NTSTATUS
AFSProcessShareFsCtrl( IN IRP *Irp,
                       IN AFSFcb *Fcb,
                       IN AFSCcb *Ccb);

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
AFSExceptionFilter( IN CHAR *FunctionString,
                    IN ULONG Code,
                    IN PEXCEPTION_POINTERS ExceptPtrs);

BOOLEAN
AFSAcquireExcl( IN PERESOURCE Resource,
                IN BOOLEAN wait);

BOOLEAN
AFSAcquireSharedStarveExclusive( IN PERESOURCE Resource,
                                 IN BOOLEAN Wait);

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

ULONG
AFSGenerateCRC( IN PUNICODE_STRING FileName,
                IN BOOLEAN UpperCaseName);

void *
AFSLockSystemBuffer( IN PIRP Irp,
                     IN ULONG Length);

void *
AFSLockUserBuffer( IN void *UserBuffer,
                   IN ULONG BufferLength,
				   OUT MDL ** Mdl);

void *
AFSMapToService( IN PIRP Irp,
                 IN ULONG ByteCount);

NTSTATUS
AFSUnmapServiceMappedBuffer( IN void *MappedBuffer,
                             IN PMDL Mdl);

NTSTATUS
AFSInitializeLibraryDevice( void);

NTSTATUS
AFSRemoveLibraryDevice( void);

NTSTATUS
AFSDefaultDispatch( IN PDEVICE_OBJECT DeviceObject,
                    IN PIRP Irp);

NTSTATUS
AFSInitializeGlobalDirectoryEntries( void);

AFSDirectoryCB *
AFSInitDirEntry( IN AFSObjectInfoCB *ParentObjectInfo,
                 IN PUNICODE_STRING FileName,
                 IN PUNICODE_STRING TargetName,
                 IN AFSDirEnumEntry *DirEnumEntry,
                 IN ULONG FileIndex);

BOOLEAN
AFSCheckForReadOnlyAccess( IN ACCESS_MASK DesiredAccess,
                           IN BOOLEAN DirectoryEntry);

NTSTATUS
AFSEvaluateNode( IN GUID *AuthGroup,
                 IN AFSDirectoryCB *DirEntry);

NTSTATUS
AFSValidateSymLink( IN GUID *AuthGroup,
                    IN AFSDirectoryCB *DirEntry);

NTSTATUS
AFSInvalidateCache( IN AFSInvalidateCacheCB *InvalidateCB);

NTSTATUS
AFSInvalidateObject( IN OUT AFSObjectInfoCB **ppObjectInfo,
                     IN     ULONG Reason);

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
                IN ACCESS_MASK GrantedAccess,
                IN BOOLEAN DirectoryEntry);

NTSTATUS
AFSGetDriverStatus( IN AFSDriverStatusRespCB *DriverStatus);

NTSTATUS
AFSSubstituteSysName( IN UNICODE_STRING *ComponentName,
                      IN UNICODE_STRING *SubstituteName,
                      IN ULONG StringIndex);

NTSTATUS
AFSSubstituteNameInPath( IN OUT UNICODE_STRING *FullPathName,
                         IN OUT UNICODE_STRING *ComponentName,
                         IN UNICODE_STRING *SubstituteName,
                         IN OUT UNICODE_STRING *RemainingPath,
                         IN BOOLEAN FreePathName);

NTSTATUS
AFSInvalidateVolume( IN AFSVolumeCB *VolumeCB,
                     IN ULONG Reason);

VOID
AFSInvalidateAllVolumes( VOID);

NTSTATUS
AFSVerifyEntry( IN GUID *AuthGroup,
                IN AFSDirectoryCB *DirectoryCB);

NTSTATUS
AFSSetVolumeState( IN AFSVolumeStatusCB *VolumeStatus);

NTSTATUS
AFSSetNetworkState( IN AFSNetworkStatusCB *NetworkStatus);

NTSTATUS
AFSValidateDirectoryCache( IN AFSObjectInfoCB *ObjectInfo,
                           IN GUID *AuthGroup);

BOOLEAN
AFSIsVolumeFID( IN AFSFileID *FileID);

BOOLEAN
AFSIsFinalNode( IN AFSFcb *Fcb);

NTSTATUS
AFSUpdateMetaData( IN AFSDirectoryCB *DirEntry,
                   IN AFSDirEnumEntry *DirEnumEntry);

NTSTATUS
AFSValidateEntry( IN AFSDirectoryCB *DirEntry,
                  IN GUID *AuthGroup,
                  IN BOOLEAN FastCall);

AFSDirectoryCB *
AFSGetSpecialShareNameEntry( IN UNICODE_STRING *ShareName,
                             IN UNICODE_STRING *SecondaryName);

NTSTATUS
AFSInitializeSpecialShareNameList( void);

void
AFSWaitOnQueuedFlushes( IN AFSFcb *Fcb);

void
AFSWaitOnQueuedReleases( void);

BOOLEAN
AFSIsEqualFID( IN AFSFileID *FileId1,
               IN AFSFileID *FileId2);

NTSTATUS
AFSResetDirectoryContent( IN AFSObjectInfoCB *ObjectInfoCB);

NTSTATUS
AFSEnumerateGlobalRoot( IN GUID *AuthGroup);

BOOLEAN
AFSIsRelativeName( IN UNICODE_STRING *Name);

void
AFSUpdateName( IN UNICODE_STRING *Name);

NTSTATUS
AFSUpdateTargetName( IN OUT UNICODE_STRING *TargetName,
                     IN OUT ULONG *Flags,
                     IN WCHAR *NameBuffer,
                     IN USHORT NameLength);

AFSNameArrayHdr *
AFSInitNameArray( IN AFSDirectoryCB *DirectoryCB,
                  IN ULONG InitialElementCount);

NTSTATUS
AFSPopulateNameArray( IN AFSNameArrayHdr *NameArray,
                      IN UNICODE_STRING *Path,
                      IN AFSDirectoryCB *DirectoryCB);

NTSTATUS
AFSPopulateNameArrayFromRelatedArray( IN AFSNameArrayHdr *NameArray,
                                      IN AFSNameArrayHdr *RelatedNameArray,
                                      IN AFSDirectoryCB *DirectoryCB);

NTSTATUS
AFSFreeNameArray( IN AFSNameArrayHdr *NameArray);

NTSTATUS
AFSInsertNextElement( IN AFSNameArrayHdr *NameArray,
                      IN AFSDirectoryCB *DirEntry);

AFSDirectoryCB *
AFSBackupEntry( IN AFSNameArrayHdr *NameArray);

AFSDirectoryCB *
AFSGetParentEntry( IN AFSNameArrayHdr *NameArray);

void
AFSResetNameArray( IN AFSNameArrayHdr *NameArray,
                   IN AFSDirectoryCB *DirEntry);

void
AFSDumpNameArray( IN IN AFSNameArrayHdr *NameArray);

void
AFSSetEnumerationEvent( IN AFSFcb *Fcb);

void
AFSClearEnumerationEvent( IN AFSFcb *Fcb);

BOOLEAN
AFSIsEnumerationInProcess( IN AFSObjectInfoCB *ObjectInfo);

NTSTATUS
AFSVerifyVolume( IN ULONGLONG ProcessId,
                 IN AFSVolumeCB *VolumeCB);

NTSTATUS
AFSInitPIOCtlDirectoryCB( IN AFSObjectInfoCB *ObjectInfo);

NTSTATUS
AFSRetrieveFileAttributes( IN AFSDirectoryCB *ParentDirectoryCB,
                           IN AFSDirectoryCB *DirectoryCB,
                           IN UNICODE_STRING *ParentPathName,
                           IN AFSNameArrayHdr *RelatedNameArray,
                           IN GUID           *AuthGroup,
                           OUT AFSFileInfoCB *FileInfo);

AFSObjectInfoCB *
AFSAllocateObjectInfo( IN AFSObjectInfoCB *ParentObjectInfo,
                       IN ULONGLONG HashIndex);

void
AFSDeleteObjectInfo( IN AFSObjectInfoCB *ObjectInfo);

NTSTATUS
AFSEvaluateRootEntry( IN AFSDirectoryCB *DirectoryCB,
                      OUT AFSDirectoryCB **TargetDirEntry);

NTSTATUS
AFSCleanupFcb( IN AFSFcb *Fcb,
               IN BOOLEAN ForceFlush);

NTSTATUS
AFSUpdateDirEntryName( IN AFSDirectoryCB *DirectoryCB,
                       IN UNICODE_STRING *NewFileName);

NTSTATUS
AFSReadCacheFile( IN void *ReadBuffer,
                  IN LARGE_INTEGER *ReadOffset,
                  IN ULONG RequestedDataLength,
                  IN OUT PULONG BytesRead);

NTSTATUS
AFSIrpComplete( IN PDEVICE_OBJECT DeviceObject,
                IN PIRP           Irp,
                IN PVOID          Context);

BOOLEAN
AFSIsDirectoryEmptyForDelete( IN AFSFcb *Fcb);

void
AFSRemoveNameEntry( IN AFSObjectInfoCB *ParentObjectInfo,
                    IN AFSDirectoryCB *DirEntry);

LARGE_INTEGER
AFSGetAuthenticationId( void);

void
AFSUnwindFileInfo( IN AFSFcb *Fcb,
                   IN AFSCcb *Ccb);

BOOLEAN
AFSValidateDirList( IN AFSObjectInfoCB *ObjectInfo);

PFILE_OBJECT
AFSReferenceCacheFileObject( void);

void
AFSReleaseCacheFileObject( IN PFILE_OBJECT CacheFileObject);

NTSTATUS
AFSInitializeLibrary( IN AFSLibraryInitCB *LibraryInit);

NTSTATUS
AFSCloseLibrary( void);

NTSTATUS
AFSDefaultLogMsg( IN ULONG Subsystem,
                  IN ULONG Level,
                  IN PCCH Format,
                  ...);

NTSTATUS
AFSGetObjectStatus( IN AFSGetStatusInfoCB *GetStatusInfo,
                    IN ULONG InputBufferLength,
                    IN AFSStatusInfoCB *StatusInfo,
                    OUT ULONG *ReturnLength);

NTSTATUS
AFSCheckSymlinkAccess( IN AFSDirectoryCB *ParentDirectoryCB,
                       IN UNICODE_STRING *ComponentName);

NTSTATUS
AFSRetrieveFinalComponent( IN UNICODE_STRING *FullPathName,
                           OUT UNICODE_STRING *ComponentName);

void
AFSDumpTraceFiles_Default( void);

void *
AFSLibExAllocatePoolWithTag( IN POOL_TYPE  PoolType,
                             IN SIZE_T  NumberOfBytes,
                             IN ULONG  Tag);

BOOLEAN
AFSValidNameFormat( IN UNICODE_STRING *FileName);

NTSTATUS
AFSCreateDefaultSecurityDescriptor( void);

void
AFSRetrieveParentPath( IN UNICODE_STRING *FullFileName,
                       OUT UNICODE_STRING *ParentPath);

NTSTATUS
AFSRetrieveValidAuthGroup( IN AFSFcb *Fcb,
                           IN AFSObjectInfoCB *ObjectInfo,
                           IN BOOLEAN WriteAccess,
                           OUT GUID *AuthGroup);

NTSTATUS
AFSPerformObjectInvalidate( IN AFSObjectInfoCB *ObjectInfo,
                            IN ULONG InvalidateReason);

//
// AFSWorker.cpp Prototypes
//

NTSTATUS
AFSInitializeWorkerPool( void);

NTSTATUS
AFSRemoveWorkerPool( void);

NTSTATUS
AFSInitWorkerThread( IN AFSWorkQueueContext *PoolContext,
                     IN PKSTART_ROUTINE WorkerRoutine);

NTSTATUS
AFSInitVolumeWorker( IN AFSVolumeCB *VolumeCB);

NTSTATUS
AFSShutdownWorkerThread( IN AFSWorkQueueContext *PoolContext);

NTSTATUS
AFSShutdownIOWorkerThread( IN AFSWorkQueueContext *PoolContext);

NTSTATUS
AFSShutdownVolumeWorker( IN AFSVolumeCB *VolumeCB);

void
AFSWorkerThread( IN PVOID Context);

void
AFSIOWorkerThread( IN PVOID Context);

void
AFSPrimaryVolumeWorkerThread( IN PVOID Context);

void
AFSVolumeWorkerThread( IN PVOID Context);

NTSTATUS
AFSInsertWorkitem( IN AFSWorkItem *WorkItem);

NTSTATUS
AFSInsertIOWorkitem( IN AFSWorkItem *WorkItem);

NTSTATUS
AFSInsertWorkitemAtHead( IN AFSWorkItem *WorkItem);

AFSWorkItem *
AFSRemoveWorkItem( void);

AFSWorkItem *
AFSRemoveIOWorkItem( void);

NTSTATUS
AFSQueueWorkerRequest( IN AFSWorkItem *WorkItem);

NTSTATUS
AFSQueueIOWorkerRequest( IN AFSWorkItem *WorkItem);

NTSTATUS
AFSQueueWorkerRequestAtHead( IN AFSWorkItem *WorkItem);

NTSTATUS
AFSShutdownVolumeWorker( IN AFSVolumeCB *VolumeCB);

NTSTATUS
AFSQueueFlushExtents( IN AFSFcb *Fcb,
                      IN GUID *AuthGroup);

NTSTATUS
AFSQueueGlobalRootEnumeration( void);

NTSTATUS
AFSQueuePurgeObject( IN AFSFcb *Fcb);

NTSTATUS
AFSQueueStartIos( IN PFILE_OBJECT CacheFileObject,
                  IN UCHAR FunctionCode,
                  IN ULONG RequestFLags,
                  IN AFSIoRun *IoRuns,
                  IN ULONG RunCount,
                  IN AFSGatherIo *GatherIo);

NTSTATUS
AFSQueueInvalidateObject( IN AFSObjectInfoCB *ObjectInfo,
                          IN ULONG InvalidateReason);

//
// AFSMD5Support.cpp Prototypes
//

void
AFSGenerateMD5( IN char *DataBuffer,
                IN ULONG Length,
                OUT UCHAR *MD5Digest);

};

#endif /* _AFS_COMMON_H */
