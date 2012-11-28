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

//
// File: AFSFastIoSupport.cpp
//

#include "AFSCommon.h"

BOOLEAN
AFSFastIoCheckIfPossible( IN struct _FILE_OBJECT *FileObject,
                          IN PLARGE_INTEGER FileOffset,
                          IN ULONG Length,
                          IN BOOLEAN Wait,
                          IN ULONG LockKey,
                          IN BOOLEAN CheckForReadOperation,
                          OUT PIO_STATUS_BLOCK IoStatus,
                          IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}

BOOLEAN
AFSFastIoRead( IN struct _FILE_OBJECT *FileObject,
               IN PLARGE_INTEGER FileOffset,
               IN ULONG Length,
               IN BOOLEAN Wait,
               IN ULONG LockKey,
               OUT PVOID Buffer,
               OUT PIO_STATUS_BLOCK IoStatus,
               IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}

BOOLEAN
AFSFastIoWrite( IN struct _FILE_OBJECT *FileObject,
                IN PLARGE_INTEGER FileOffset,
                IN ULONG Length,
                IN BOOLEAN Wait,
                IN ULONG LockKey,
                IN PVOID Buffer,
                OUT PIO_STATUS_BLOCK IoStatus,
                IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}

BOOLEAN
AFSFastIoQueryBasicInfo( IN struct _FILE_OBJECT *FileObject,
                         IN BOOLEAN Wait,
                         OUT PFILE_BASIC_INFORMATION Buffer,
                         OUT PIO_STATUS_BLOCK IoStatus,
                         IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}

BOOLEAN
AFSFastIoQueryStandardInfo( IN struct _FILE_OBJECT *FileObject,
                            IN BOOLEAN Wait,
                            OUT PFILE_STANDARD_INFORMATION Buffer,
                            OUT PIO_STATUS_BLOCK IoStatus,
                            IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}

BOOLEAN
AFSFastIoLock( IN struct _FILE_OBJECT *FileObject,
               IN PLARGE_INTEGER FileOffset,
               IN PLARGE_INTEGER Length,
               IN PEPROCESS ProcessId,
               IN ULONG Key,
               IN BOOLEAN FailImmediately,
               IN BOOLEAN ExclusiveLock,
               OUT PIO_STATUS_BLOCK IoStatus,
               IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}

BOOLEAN
AFSFastIoUnlockSingle( IN struct _FILE_OBJECT *FileObject,
                       IN PLARGE_INTEGER FileOffset,
                       IN PLARGE_INTEGER Length,
                       IN PEPROCESS ProcessId,
                       IN ULONG Key,
                       OUT PIO_STATUS_BLOCK IoStatus,
                       IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}

BOOLEAN
AFSFastIoUnlockAll( IN struct _FILE_OBJECT *FileObject,
                    IN PEPROCESS ProcessId,
                    OUT PIO_STATUS_BLOCK IoStatus,
                    IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}

BOOLEAN
AFSFastIoUnlockAllByKey( IN struct _FILE_OBJECT *FileObject,
                         IN PVOID ProcessId,
                         IN ULONG Key,
                         OUT PIO_STATUS_BLOCK IoStatus,
                         IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}

BOOLEAN
AFSFastIoDevCtrl( IN struct _FILE_OBJECT *FileObject,
                  IN BOOLEAN Wait,
                  IN PVOID InputBuffer OPTIONAL,
                  IN ULONG InputBufferLength,
                  OUT PVOID OutputBuffer OPTIONAL,
                  IN ULONG OutputBufferLength,
                  IN ULONG IoControlCode,
                  OUT PIO_STATUS_BLOCK IoStatus,
                  IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}

void
AFSFastIoAcquireFile( IN struct _FILE_OBJECT *FileObject)
{

    AFSFcb *pFcb = (AFSFcb *)FileObject->FsContext;

    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSFastIoAcquireFile Acquiring Fcb SectionObject lock %08lX EXCL %08lX\n",
                  &pFcb->NPFcb->SectionObjectResource,
                  PsGetCurrentThread());

    AFSAcquireExcl( &pFcb->NPFcb->SectionObjectResource,
                    TRUE);

    return;
}

void
AFSFastIoReleaseFile( IN struct _FILE_OBJECT *FileObject)
{

    AFSFcb *pFcb = (AFSFcb *)FileObject->FsContext;

    if( ExIsResourceAcquiredExclusiveLite( &pFcb->NPFcb->SectionObjectResource))
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSFastIoReleaseFile Releasing Fcb SectionObject lock %08lX EXCL %08lX\n",
                      &pFcb->NPFcb->SectionObjectResource,
                      PsGetCurrentThread());

        AFSReleaseResource( &pFcb->NPFcb->SectionObjectResource);
    }

    return;
}

VOID
AFSFastIoDetachDevice( IN struct _DEVICE_OBJECT *SourceDevice,
                       IN struct _DEVICE_OBJECT *TargetDevice)
{

    return;
}

BOOLEAN
AFSFastIoQueryNetworkOpenInfo( IN struct _FILE_OBJECT *FileObject,
                               IN BOOLEAN Wait,
                               OUT struct _FILE_NETWORK_OPEN_INFORMATION *Buffer,
                               OUT struct _IO_STATUS_BLOCK *IoStatus,
                               IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}

BOOLEAN
AFSFastIoMdlRead( IN struct _FILE_OBJECT *FileObject,
                  IN PLARGE_INTEGER FileOffset,
                  IN ULONG Length,
                  IN ULONG LockKey,
                  OUT PMDL *MdlChain,
                  OUT PIO_STATUS_BLOCK IoStatus,
                  IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}

BOOLEAN
AFSFastIoMdlReadComplete( IN struct _FILE_OBJECT *FileObject,
                          IN PMDL MdlChain,
                          IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}

BOOLEAN
AFSFastIoPrepareMdlWrite( IN struct _FILE_OBJECT *FileObject,
                          IN PLARGE_INTEGER FileOffset,
                          IN ULONG Length,
                          IN ULONG LockKey,
                          OUT PMDL *MdlChain,
                          OUT PIO_STATUS_BLOCK IoStatus,
                          IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}

BOOLEAN
AFSFastIoMdlWriteComplete( IN struct _FILE_OBJECT *FileObject,
                           IN PLARGE_INTEGER FileOffset,
                           IN PMDL MdlChain,
                           IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}

NTSTATUS
AFSFastIoAcquireForModWrite( IN struct _FILE_OBJECT *FileObject,
                             IN PLARGE_INTEGER EndingOffset,
                             OUT struct _ERESOURCE **ResourceToRelease,
                             IN struct _DEVICE_OBJECT *DeviceObject)
{

    NTSTATUS ntStatus = STATUS_FILE_LOCK_CONFLICT;
    AFSFcb *pFcb = (AFSFcb *)FileObject->FsContext;

    __Enter
    {

        if( AFSAcquireExcl( &pFcb->NPFcb->Resource,
                            BooleanFlagOn( FileObject->Flags, FO_SYNCHRONOUS_IO)))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSFastIoAcquireForModWrite Acquired Fcb SectionObject lock %08lX EXCL %08lX\n",
                          &pFcb->NPFcb->SectionObjectResource,
                          PsGetCurrentThread());

            ntStatus = STATUS_SUCCESS;

            *ResourceToRelease = &pFcb->NPFcb->SectionObjectResource;
        }
    }

    return ntStatus;
}

NTSTATUS
AFSFastIoReleaseForModWrite( IN struct _FILE_OBJECT *FileObject,
                             IN struct _ERESOURCE *ResourceToRelease,
                             IN struct _DEVICE_OBJECT *DeviceObject)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSFastIoReleaseForModWrite Releasing lock %08lX EXCL %08lX\n",
                  ResourceToRelease,
                  PsGetCurrentThread());

    AFSReleaseResource( ResourceToRelease);

    return ntStatus;
}

NTSTATUS
AFSFastIoAcquireForCCFlush( IN struct _FILE_OBJECT *FileObject,
                            IN struct _DEVICE_OBJECT *DeviceObject)
{

    NTSTATUS ntStatus = STATUS_FILE_LOCK_CONFLICT;
    AFSFcb *pFcb = (AFSFcb *)FileObject->FsContext;

    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSFastIoAcquireForCCFlush Acquiring Fcb PagingIo lock %08lX SHARED %08lX\n",
                  &pFcb->NPFcb->PagingResource,
                  PsGetCurrentThread());

    AFSAcquireShared( &pFcb->NPFcb->PagingResource,
                      TRUE);

    if( !ExIsResourceAcquiredSharedLite( &pFcb->NPFcb->SectionObjectResource))
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSFastIoAcquireForCCFlush Acquiring Fcb SectionObject lock %08lX EXCL %08lX\n",
                      &pFcb->NPFcb->SectionObjectResource,
                      PsGetCurrentThread());

        AFSAcquireExcl( &pFcb->NPFcb->SectionObjectResource,
                        TRUE);
    }
    else
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSFastIoAcquireForCCFlush Acquiring Fcb SectionObject lock %08lX SHARED %08lX\n",
                      &pFcb->NPFcb->SectionObjectResource,
                      PsGetCurrentThread());

        AFSAcquireShared( &pFcb->NPFcb->SectionObjectResource,
                          TRUE);
    }

    ntStatus = STATUS_SUCCESS;

    //
    // Set the TopLevelIrp field for this caller
    //

    if( IoGetTopLevelIrp() == NULL)
    {

        IoSetTopLevelIrp( (PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);
    }

    return ntStatus;
}

NTSTATUS
AFSFastIoReleaseForCCFlush( IN struct _FILE_OBJECT *FileObject,
                            IN struct _DEVICE_OBJECT *DeviceObject)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFcb *pFcb = (AFSFcb *)FileObject->FsContext;

    if( IoGetTopLevelIrp() == (PIRP)FSRTL_CACHE_TOP_LEVEL_IRP)
    {

        IoSetTopLevelIrp( NULL);
    }

    if( ExIsResourceAcquiredSharedLite( &pFcb->NPFcb->PagingResource))
    {

        AFSReleaseResource( &pFcb->NPFcb->PagingResource);
    }
    else
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_ERROR,
                      "AFSFastIoReleaseForCCFlush Called for non-acquired paging resource Fcb\n");
    }

    if( ExIsResourceAcquiredExclusiveLite( &pFcb->NPFcb->SectionObjectResource) ||
        ExIsResourceAcquiredSharedLite( &pFcb->NPFcb->SectionObjectResource))
    {

        AFSReleaseResource( &pFcb->NPFcb->SectionObjectResource);
    }
    else
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_ERROR,
                      "AFSFastIoReleaseForCCFlush Called for non-acquired SectionObject resource Fcb\n");
    }

    return ntStatus;
}

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
                         IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}

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
                          IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}

BOOLEAN
AFSFastIoMdlReadCompleteCompressed( IN struct _FILE_OBJECT *FileObject,
                                    IN PMDL MdlChain,
                                    IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}

BOOLEAN
AFSFastIoMdlWriteCompleteCompressed( IN struct _FILE_OBJECT *FileObject,
                                     IN PLARGE_INTEGER FileOffset,
                                     IN PMDL MdlChain,
                                     IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}

BOOLEAN
AFSFastIoQueryOpen( IN struct _IRP *Irp,
                    OUT PFILE_NETWORK_OPEN_INFORMATION NetworkInformation,
                    IN struct _DEVICE_OBJECT *DeviceObject)
{

    BOOLEAN bStatus = FALSE;

    return bStatus;
}
