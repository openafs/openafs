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
    AFSFcb *pFcb = (AFSFcb *)FileObject->FsContext;
    LARGE_INTEGER liLength;

    if( pFcb != NULL)
    {

        //
        // check for any outstanding locks on the node
        //

        if( CheckForReadOperation) 
        {

            if( FsRtlFastCheckLockForRead( &pFcb->Specific.File.FileLock,
                                           FileOffset,
                                           &liLength,
                                           LockKey,
                                           FileObject,
                                           PsGetCurrentProcess())) 
            {

                bStatus = TRUE;
            }
            else
            {

                bStatus = FALSE;
            }
        } 
        else 
        {

            if( FsRtlFastCheckLockForWrite( &pFcb->Specific.File.FileLock,
                                            FileOffset,
                                            &liLength,
                                            LockKey,
                                            FileObject,
                                            PsGetCurrentProcess())) 
            {

                bStatus = TRUE;
            }
            else
            {

                bStatus = FALSE;
            }
        }
    }

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

    AFSAcquireExcl( &pFcb->NPFcb->Resource,
                    TRUE);

    if( PsGetCurrentProcessId() != AFSSysProcess)
    {

        //
        // Save off the last writer
        //

        pFcb->Specific.File.ExtentProcessId = (ULONGLONG)PsGetCurrentProcessId();
    }

    return;
}

void
AFSFastIoReleaseFile( IN struct _FILE_OBJECT *FileObject)
{

    AFSFcb *pFcb = (AFSFcb *)FileObject->FsContext;

    if( ExIsResourceAcquiredExclusiveLite( &pFcb->NPFcb->Resource))
    {

        AFSReleaseResource( &pFcb->NPFcb->Resource);
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

    AFSAcquireExcl( &pFcb->NPFcb->Resource,
                    TRUE);

    ntStatus = STATUS_SUCCESS;

    *ResourceToRelease = &pFcb->NPFcb->Resource;

    return ntStatus;
}

NTSTATUS
AFSFastIoReleaseForModWrite( IN struct _FILE_OBJECT *FileObject,
                             IN struct _ERESOURCE *ResourceToRelease,
                             IN struct _DEVICE_OBJECT *DeviceObject)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    AFSReleaseResource( ResourceToRelease);

    return ntStatus;
}

NTSTATUS
AFSFastIoAcquireForCCFlush( IN struct _FILE_OBJECT *FileObject,
                            IN struct _DEVICE_OBJECT *DeviceObject)
{

    NTSTATUS ntStatus = STATUS_FILE_LOCK_CONFLICT;
    AFSFcb *pFcb = (AFSFcb *)FileObject->FsContext;

    if( !ExIsResourceAcquiredSharedLite( &pFcb->NPFcb->Resource))
    {

        AFSAcquireExcl( &pFcb->NPFcb->Resource,
                        TRUE);
    }
    else
    {

        AFSAcquireShared( &pFcb->NPFcb->Resource,
                          TRUE);
    }
    
    ntStatus = STATUS_SUCCESS;

    AFSAcquireShared( &pFcb->NPFcb->PagingResource,
                      TRUE);

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

    if( ExIsResourceAcquiredExclusiveLite( &pFcb->NPFcb->Resource) ||
        ExIsResourceAcquiredSharedLite( &pFcb->NPFcb->Resource))
    {

        AFSReleaseResource( &pFcb->NPFcb->Resource);
    }
    else
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_ERROR,
                      "AFSFastIoReleaseForCCFlush Called for non-acquired main resource Fcb\n");
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

//
// Cache manager callback routines
//

BOOLEAN
AFSAcquireFcbForLazyWrite( IN PVOID Fcb,
                           IN BOOLEAN Wait)
{

    BOOLEAN bStatus = FALSE;
    AFSFcb *pFcb = (AFSFcb *)Fcb;
    BOOLEAN bReleaseMain = FALSE, bReleasePaging = FALSE;

    //
    // Try and acquire the Fcb resource
    //
    ASSERT( NULL == pFcb->Specific.File.LazyWriterThread);

    pFcb->Specific.File.LazyWriterThread = PsGetCurrentThread();

    if( AFSAcquireShared( &pFcb->NPFcb->Resource,
                          Wait))
    {

        bReleaseMain = TRUE;

        //
        // Try and grab the paging
        //

        if( AFSAcquireShared( &pFcb->NPFcb->PagingResource,
                              Wait))
        {

            bReleasePaging = TRUE;

            //
            // All is well ...
            //

            bStatus = TRUE;

            IoSetTopLevelIrp( (PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);
        }
    }

    if( !bStatus)
    {

        if( bReleaseMain)
        {

            AFSReleaseResource( &pFcb->NPFcb->Resource);
        }

        if( bReleasePaging)
        {

            AFSReleaseResource( &pFcb->NPFcb->PagingResource);
        }
    }

    return bStatus;
}

VOID
AFSReleaseFcbFromLazyWrite( IN PVOID Fcb)
{

    AFSFcb *pFcb = (AFSFcb *)Fcb;

    IoSetTopLevelIrp( NULL);

    ASSERT( PsGetCurrentThread() == pFcb->Specific.File.LazyWriterThread);

    pFcb->Specific.File.LazyWriterThread = NULL;


    AFSReleaseResource( &pFcb->NPFcb->PagingResource);

    AFSReleaseResource( &pFcb->NPFcb->Resource);

    return;
}

BOOLEAN
AFSAcquireFcbForReadAhead( IN PVOID Fcb,
                           IN BOOLEAN Wait)
{

    BOOLEAN bStatus = FALSE;
    AFSFcb *pFcb = (AFSFcb *)Fcb;

    if( AFSAcquireShared( &pFcb->NPFcb->Resource,
                          Wait))
    {

        bStatus = TRUE;

        IoSetTopLevelIrp( (PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);
    }

    return bStatus;
}

VOID
AFSReleaseFcbFromReadAhead( IN PVOID Fcb)
{

    AFSFcb *pFcb = (AFSFcb *)Fcb;

    IoSetTopLevelIrp( NULL);

    AFSReleaseResource( &pFcb->NPFcb->Resource);

    return;
}
