/*
 * Copyright (c) 2008, 2009, 2010, 2011, 2012, 2013 Kernel Drivers, LLC.
 * Copyright (c) 2009, 2010, 2011, 2012, 2013 Your File System, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
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

//
// AFSInit.cpp Prototypes
//

NTSTATUS
DriverEntry( IN PDRIVER_OBJECT DriverObj,
             IN PUNICODE_STRING RegPath);

void
AFSUnload( IN PDRIVER_OBJECT DriverObject);

//
// AFSAuthGroupSupport.cpp
//

void
AFSRetrieveAuthGroup( IN ULONGLONG ProcessId,
                      IN ULONGLONG ThreadId,
                      OUT GUID *AuthGroup);

BOOLEAN
AFSIsLocalSystemAuthGroup( IN GUID *AuthGroup);

BOOLEAN
AFSIsLocalSystemSID( IN UNICODE_STRING *SIDString);

BOOLEAN
AFSIsNoPAGAuthGroup( IN GUID *AuthGroup);

NTSTATUS
AFSCreateSetProcessAuthGroup( AFSAuthGroupRequestCB *CreateSetAuthGroup);

NTSTATUS
AFSQueryProcessAuthGroupList( IN GUID *GUIDList,
                              IN ULONG BufferLength,
                              OUT ULONG_PTR *ReturnLength);

NTSTATUS
AFSSetActiveProcessAuthGroup( IN AFSAuthGroupRequestCB *ActiveAuthGroup);

NTSTATUS
AFSResetActiveProcessAuthGroup( IN AFSAuthGroupRequestCB *ActiveAuthGroup);

NTSTATUS
AFSCreateAuthGroupForSIDorLogonSession( IN AFSAuthGroupRequestCB *AuthGroupRequestCB,
                                        IN BOOLEAN bLogonSession);

NTSTATUS
AFSQueryAuthGroup( IN AFSAuthGroupRequestCB *AuthGroupRequestCB,
                   OUT GUID *AuthGroupGUID,
                   OUT ULONG_PTR *ReturnLength);

//
// AFSBTreeSupport.cpp Prototypes
//

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
// AFSCommSupport.cpp Prototypes
//

NTSTATUS
AFSReleaseFid( IN AFSFileID *FileId);

NTSTATUS
AFSProcessRequest( IN ULONG RequestType,
                   IN ULONG RequestFlags,
                   IN GUID *AuthGroup,
                   IN PUNICODE_STRING FileName,
                   IN AFSFileID *FileId,
                   IN WCHAR * Cell,
                   IN ULONG   CellLength,
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
AFSControlDeviceCreate( IN PIRP Irp);

NTSTATUS
AFSOpenRedirector( IN PIRP Irp);

NTSTATUS
AFSInitRdrFcb( OUT AFSFcb **RdrFcb);

void
AFSRemoveRdrFcb( IN OUT AFSFcb **RdrFcb);

//
// AFSClose.cpp Prototypes
//

NTSTATUS
AFSClose( IN PDEVICE_OBJECT DeviceObject,
          IN PIRP Irp);

NTSTATUS
AFSCommonClose( IN PDEVICE_OBJECT DeviceObject,
                IN PIRP Irp);

//
// AFSNetworkProviderSupport.cpp
//

NTSTATUS
AFSAddConnectionEx( IN UNICODE_STRING *RemoteName,
                    IN ULONG DisplayType,
                    IN ULONG Flags);

void
AFSInitializeConnectionInfo( IN AFSProviderConnectionCB *Connection,
                             IN ULONG DisplayType);

//
// AFSRead.cpp Prototypes
//

NTSTATUS
AFSRead( IN PDEVICE_OBJECT DeviceObject,
         IN PIRP Irp);

//
// AFSWrite.cpp Prototypes
//

NTSTATUS
AFSWrite( IN PDEVICE_OBJECT DeviceObject,
                IN PIRP Irp);

//
// AFSFileInfo.cpp Prototypes
//

NTSTATUS
AFSQueryFileInfo( IN PDEVICE_OBJECT DeviceObject,
                  IN PIRP Irp);

NTSTATUS
AFSSetFileInfo( IN PDEVICE_OBJECT DeviceObject,
                IN PIRP Irp);

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
AFSSetVolumeInfo( IN PDEVICE_OBJECT DeviceObject,
                  IN PIRP Irp);

//
// AFSDirControl.cpp Prototypes
//

NTSTATUS
AFSDirControl( IN PDEVICE_OBJECT DeviceObject,
               IN PIRP Irp);

//
// AFSFSControl.cpp Prototypes
//

NTSTATUS
AFSFSControl( IN PDEVICE_OBJECT DeviceObject,
              IN PIRP Irp);

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

NTSTATUS
AFSCommonCleanup( IN PDEVICE_OBJECT DeviceObject,
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
AFSRemoveControlDevice( void);

NTSTATUS
AFSDefaultDispatch( IN PDEVICE_OBJECT DeviceObject,
                    IN PIRP Irp);

NTSTATUS
AFSIrpComplete( IN PDEVICE_OBJECT DeviceObject,
                IN PIRP           Irp,
                IN PVOID          Context);

void
AFSInitServerStrings( void);

NTSTATUS
AFSReadServerName( void);

NTSTATUS
AFSReadMountRootName( void);

NTSTATUS
AFSSetSysNameInformation( IN AFSSysNameNotificationCB *SysNameInfo,
                          IN ULONG SysNameInfoBufferLength);

void
AFSResetSysNameList( IN AFSSysNameCB *SysNameList);

NTSTATUS
AFSSendDeviceIoControl( IN DEVICE_OBJECT *TargetDeviceObject,
                        IN ULONG IOControl,
                        IN void *InputBuffer,
                        IN ULONG InputBufferLength,
                        IN OUT void *OutputBuffer,
                        IN ULONG OutputBufferLength,
                        OUT ULONG *ResultLength);

void *
AFSExAllocatePoolWithTag( IN POOL_TYPE  PoolType,
                          IN SIZE_T  NumberOfBytes,
                          IN ULONG  Tag);

void
AFSExFreePoolWithTag( IN void *Buffer, IN ULONG Tag);

NTSTATUS
AFSShutdownRedirector( void);

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

NTSTATUS
AFSGetCallerSID( OUT UNICODE_STRING *SIDString,
                 OUT BOOLEAN *pbImpersonation);

ULONG
AFSGetSessionId( IN HANDLE ProcessId,
                 OUT BOOLEAN *pbImpersonation);

NTSTATUS
AFSCheckThreadDacl( OUT GUID *AuthGroup);

NTSTATUS
AFSProcessSetProcessDacl( IN AFSProcessCB *ProcessCB);

NTSTATUS
AFSSetReparsePointPolicy( IN AFSSetReparsePointPolicyCB *Policy);

NTSTATUS
AFSGetReparsePointPolicy( OUT AFSGetReparsePointPolicyCB *Policy);

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

//
// AFSLibrarySupport.cpp Prototypes
//

NTSTATUS
AFSLoadLibrary( IN ULONG Flags,
                IN UNICODE_STRING *ServicePath);

NTSTATUS
AFSUnloadLibrary( IN BOOLEAN CancelQueue);

NTSTATUS
AFSCheckLibraryState( IN PIRP Irp);

NTSTATUS
AFSClearLibraryRequest( void);

NTSTATUS
AFSQueueLibraryRequest( IN PIRP Irp);

NTSTATUS
AFSProcessQueuedResults( IN BOOLEAN CancelRequest);

NTSTATUS
AFSSubmitLibraryRequest( IN PIRP Irp);

NTSTATUS
AFSInitializeLibrary( IN AFSFileID *GlobalRootFid,
                      IN BOOLEAN QueueRootEnumeration);

NTSTATUS
AFSConfigLibraryDebug( void);

//
// AFSRDRSupport.cpp Prototypes
//

NTSTATUS
AFSInitRDRDevice( void);

NTSTATUS
AFSRDRDeviceControl( IN PDEVICE_OBJECT DeviceObject,
                     IN PIRP Irp);

NTSTATUS
AFSInitializeRedirector( IN AFSRedirectorInitInfo *CacheFileInfo);

NTSTATUS
AFSCloseRedirector( void);

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
AFSGetTraceConfig( OUT AFSTraceConfigCB *TraceInfo);

NTSTATUS
AFSGetTraceBuffer( IN ULONG TraceBufferLength,
                   OUT void *TraceBuffer,
                   OUT ULONG_PTR *CopiedLength);

void
AFSTagInitialLogEntry( void);

void
AFSDumpTraceFiles( void);

NTSTATUS
AFSInitializeDumpFile( void);

//
// AFSProcessSupport.cpp Prototypes
//

void
AFSProcessNotify( IN HANDLE  ParentId,
                  IN HANDLE  ProcessId,
                  IN BOOLEAN  Create);

void
AFSProcessNotifyEx( IN OUT PEPROCESS Process,
                    IN     HANDLE ProcessId,
                    IN OUT PPS_CREATE_NOTIFY_INFO CreateInfo);

void
AFSProcessCreate( IN HANDLE ParentId,
                  IN HANDLE ProcessId,
                  IN HANDLE CreatingProcessId,
                  IN HANDLE CreatingThreadId);

void
AFSProcessDestroy( IN HANDLE ProcessId);

GUID *
AFSValidateProcessEntry( IN HANDLE  ProcessId,
                         IN BOOLEAN bProcessTreeLocked);

BOOLEAN
AFSIs64BitProcess( IN ULONGLONG ProcessId);

AFSProcessCB *
AFSInitializeProcessCB( IN ULONGLONG ParentProcessId,
                        IN ULONGLONG ProcessId);

AFSThreadCB *
AFSInitializeThreadCB( IN AFSProcessCB *ProcessCB,
                       IN ULONGLONG ThreadId);

BOOLEAN
AFSIsUser( IN PSID Sid);

BOOLEAN
AFSIsInGroup(IN PSID Sid);

VOID
AFSRegisterService( void);

VOID
AFSDeregisterService( void);

BOOLEAN
AFSIsService( void);

};

#endif /* _AFS_COMMON_H */
