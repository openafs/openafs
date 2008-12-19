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
// File: AFSCleanup.cpp
//

#include "AFSCommon.h"

//
// Function: AFSCleanup
//
// Description: 
//
//      This function is the IRP_MJ_CLEAUP dispatch handler
//
// Return:
//
//       A status is returned for the handling of this request
//

NTSTATUS
AFSCleanup( IN PDEVICE_OBJECT DeviceObject,
            IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)DeviceObject->DeviceExtension;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSFcb *pFcb = NULL;    
    AFSCcb *pCcb = NULL;
    PFILE_OBJECT pFileObject = NULL;
    AFSFcb *pRootFcb = NULL;
    AFSDeviceExt *pControlDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    IO_STATUS_BLOCK stIoSB;

    __try
    {

        //
        // Set some initial variables to make processing easier
        //

        pFileObject = pIrpSp->FileObject;

        if( DeviceObject == AFSDeviceObject)
        {
            
            if( FlagOn( (ULONG_PTR)pFileObject->FsContext, AFS_CONTROL_INSTANCE))
            {

                //
                // This is the process which was registered for the callback pool so cleanup the pool
                //

                AFSCleanupIrpPool();
            }

            if( FlagOn( (ULONG_PTR)pFileObject->FsContext, AFS_REDIRECTOR_INSTANCE))
            {

                //
                // Close the redirector
                //

                AFSCloseRedirector();
            }

            try_return( ntStatus);
        }

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;

        if( pFcb == NULL)
        {

            try_return( ntStatus);
        }

        pRootFcb = pFcb->RootFcb;

        pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

        //
        // Perform the cleanup functionality depending on the type of node it is
        //

        switch( pFcb->Header.NodeTypeCode)
        {

            case AFS_ROOT_ALL:
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSCleanup Acquiring GlobalRoot lock %08lX EXCL %08lX\n",
                              &pFcb->NPFcb->Resource,
                              PsGetCurrentThread());

                ASSERT( pFcb->OpenHandleCount != 0);

                InterlockedDecrement( &pFcb->OpenHandleCount);

                break;
            }

            case AFS_IOCTL_FCB:
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSCleanup Acquiring PIOCtl lock %08lX EXCL %08lX\n",
                              &pFcb->NPFcb->Resource,
                              PsGetCurrentThread());

                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                  TRUE);

                ASSERT( pFcb->OpenHandleCount != 0);

                InterlockedDecrement( &pFcb->OpenHandleCount);

                //
                // Decrement the open child handle count
                //

                if( pFcb->ParentFcb != NULL)
                {
                   
                    InterlockedDecrement( &pFcb->ParentFcb->Specific.Directory.ChildOpenHandleCount);
                }
                    
                //
                // And finally, release the Fcb if we acquired it.
                //

                AFSReleaseResource( &pFcb->NPFcb->Resource);

                break;
            }

            //
            // This Fcb represents a file
            //

            case AFS_FILE_FCB:
            {
                           
                LARGE_INTEGER liTime;

                //
                // We may be performing some cleanup on the Fcb so grab it exclusive to ensure no collisions
                //

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSCleanup Acquiring Fcb lock %08lX EXCL %08lX\n",
                              &pFcb->NPFcb->Resource,
                              PsGetCurrentThread());

                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                TRUE);

                //
                // Uninitialize the cache map. This call is unconditional.
                //

                AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSCleanup Tearing down cache map for Fcb %08lX FileObject %08lX\n",
                              pFcb,
                              pFileObject);

                CcUninitializeCacheMap( pFileObject, 
                                        NULL, 
                                        NULL);

                //
                // Unlock all outstanding locks on the file, again, unconditionally
                //

                (VOID) FsRtlFastUnlockAll( &pFcb->Specific.File.FileLock,
                                           pFileObject,
                                           IoGetRequestorProcess( Irp),
                                           NULL);

                //
                // Tell the service to unlock all on the file
                //

                AFSProcessRequest( AFS_REQUEST_TYPE_BYTE_RANGE_UNLOCK_ALL,
                                   AFS_REQUEST_FLAG_SYNCHRONOUS,
                                   0,
                                   &pFcb->DirEntry->DirectoryEntry.FileName,
                                   &pFcb->DirEntry->DirectoryEntry.FileId,
                                   NULL,
                                   0,
                                   NULL,
                                   NULL);

                //
                // Perform some final common processing
                //

                ASSERT( pFcb->OpenHandleCount != 0);

                InterlockedDecrement( &pFcb->OpenHandleCount);
                    
                if( BooleanFlagOn( pFcb->Flags, AFS_UPDATE_WRITE_TIME))
                {

                    KeQuerySystemTime( &pFcb->DirEntry->DirectoryEntry.LastWriteTime);

                    //
                    // Convert it to a local time
                    //

                    ExSystemTimeToLocalTime( &pFcb->DirEntry->DirectoryEntry.LastWriteTime,
                                             &pFcb->DirEntry->DirectoryEntry.LastWriteTime);              

                    pFcb->DirEntry->DirectoryEntry.LastAccessTime = pFcb->DirEntry->DirectoryEntry.LastWriteTime;

                    pFcb->DirEntry->DirectoryEntry.ChangeTime = pFcb->DirEntry->DirectoryEntry.LastWriteTime;
                }
                else
                {

                    KeQuerySystemTime( &pFcb->DirEntry->DirectoryEntry.LastAccessTime);

                    //
                    // Convert it to a local time
                    //

                    ExSystemTimeToLocalTime( &pFcb->DirEntry->DirectoryEntry.LastAccessTime,
                                             &pFcb->DirEntry->DirectoryEntry.LastAccessTime);              
                }

                //
                // If the count has dropped to zero and there is a pending delete
                // then delete the node
                //

                if( pFcb->OpenHandleCount == 0 &&
                    BooleanFlagOn( pFcb->Flags, AFS_FCB_PENDING_DELETE))
                {

                    //
                    // Before telling the server about the deleted file, tear down all extents for
                    // the file
                    //

                    AFSDbgLogMsg( AFS_SUBSYSTEM_EXTENT_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSCleanup Tearing down extents for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  &pFcb->DirEntry->DirectoryEntry.FileName,
                                  pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                  pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                  pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                  pFcb->DirEntry->DirectoryEntry.FileId.Unique);        

                    AFSTearDownFcbExtents( pFcb);

                    //
                    // Try and notify the service about the delete
                    // We need to drop the locks on the Fcb since this may cause re-entrancy
                    // for the invalidation of the node
                    //

                    AFSReleaseResource( &pFcb->NPFcb->Resource);

                    ntStatus = AFSNotifyDelete( pFcb);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSCleanup Acquiring Fcb (DELETE) lock %08lX EXCL %08lX\n",
                                  &pFcb->NPFcb->Resource,
                                  PsGetCurrentThread());

                    AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                      TRUE);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSCleanup Failed to notify service of deleted file %wZ Status %08lX\n", 
                                      &pCcb->FullFileName,
                                      ntStatus);

                        ntStatus = STATUS_SUCCESS;

                        ClearFlag( pFcb->Flags, AFS_FCB_PENDING_DELETE);
                    }
                    else
                    {

                        SetFlag( pFcb->Flags, AFS_FCB_DELETED);

                        ASSERT( pFcb->ParentFcb != NULL);

                        FsRtlNotifyFullReportChange( pFcb->ParentFcb->NPFcb->NotifySync,
                                                     &pFcb->ParentFcb->NPFcb->DirNotifyList,												     
                                                     (PSTRING)&pCcb->FullFileName,
                                                     (USHORT)(pCcb->FullFileName.Length - pFcb->DirEntry->DirectoryEntry.FileName.Length),
                                                     (PSTRING)NULL,
                                                     (PSTRING)NULL,
                                                     (ULONG)FILE_NOTIFY_CHANGE_FILE_NAME,
                                                     (ULONG)FILE_ACTION_REMOVED,
                                                     (PVOID)NULL );
                    }
                }
                else
                {

                    //
                    // If this is the last open handle close then try and flush any dirty data 
                    // out prior to flushing it to the server
                    //

                    if( pFcb->OpenHandleCount == 0)
                    {

                        __try
                        {

                            CcFlushCache( &pFcb->NPFcb->SectionObjectPointers, 
                                          NULL, 
                                          0, 
                                          &stIoSB);
                        }
                        __except( EXCEPTION_EXECUTE_HANDLER)
                        {
                        
                            ntStatus = GetExceptionCode();
                        }
                    }

                    //
                    // Attempt to flush any dirty extents to the server. This may be a little 
                    // agressive, to flush whenever the handle is close, but it ensures
                    // coherency.
                    //

                    if( pFcb->Specific.File.ExtentsDirtyCount)
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_EXTENT_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSCleanup Flushing extents for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      &pFcb->DirEntry->DirectoryEntry.FileName,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Unique);        

                        AFSFlushExtents( pFcb);
                    }

                    if( pFcb->OpenHandleCount == 0)
                    {

                        //
                        // Wait for any outstanding queued flushes to complete
                        //

                        AFSWaitOnQueuedFlushes( pFcb);
                    }
                }

                //
                // If there have been any updates to the node then push it to 
                // the service
                //

                if( BooleanFlagOn( pFcb->Flags, AFS_FILE_MODIFIED))
                {

                    ULONG ulNotifyFilter = 0;

                    //
                    // Update the change time
                    //

                    pFcb->DirEntry->DirectoryEntry.ChangeTime = pFcb->DirEntry->DirectoryEntry.LastAccessTime;

                    //
                    // Need to drop our lock on the Fcb while this call is made since it could
                    // result in the service invalidating the node requiring the lock
                    //

                    AFSReleaseResource( &pFcb->NPFcb->Resource);

                    ntStatus = AFSUpdateFileInformation( DeviceObject,
                                                           pFcb);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSCleanup Acquiring Fcb (FILEINFO) lock %08lX EXCL %08lX\n",
                                  &pFcb->NPFcb->Resource,
                                  PsGetCurrentThread());

                    AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                      TRUE);

                    if( NT_SUCCESS( ntStatus))
                    {

                        ClearFlag( pFcb->Flags, AFS_FILE_MODIFIED);
                    }

    				ulNotifyFilter |= (FILE_NOTIFY_CHANGE_ATTRIBUTES);

                    ASSERT( pFcb->ParentFcb != NULL);

                    FsRtlNotifyFullReportChange( pFcb->ParentFcb->NPFcb->NotifySync,
                                                 &pFcb->ParentFcb->NPFcb->DirNotifyList,
                                                 (PSTRING)&pCcb->FullFileName,
                                                 (USHORT)(pCcb->FullFileName.Length - pFcb->DirEntry->DirectoryEntry.FileName.Length),
                                                 (PSTRING)NULL,
                                                 (PSTRING)NULL,
                                                 (ULONG)ulNotifyFilter,
                                                 (ULONG)FILE_ACTION_MODIFIED,
                                                 (PVOID)NULL);
                }

                //
                // Remove the share access at this time since we may not get the close for sometime on this FO.
                //

                IoRemoveShareAccess( pFileObject, 
                                     &pFcb->ShareAccess);

                //
                // Decrement the open child handle count
                //

                if( pFcb->ParentFcb != NULL)
                {
                   
                    InterlockedDecrement( &pFcb->ParentFcb->Specific.Directory.ChildOpenHandleCount);
                }
                    
                //
                // And finally, release the Fcb if we acquired it.
                //

                AFSReleaseResource( &pFcb->NPFcb->Resource);

                break;
            }
            
            //
            // Root or directory node
            //

            case AFS_ROOT_FCB:
            {

                //
                // Set the root Fcb to this node
                //

                pRootFcb = pFcb;

                //
                // Fall through to below
                //
            }

            case AFS_DIRECTORY_FCB:
            {

                //
                // We may be performing some cleanup on the Fcb so grab it exclusive to ensure no collisions
                //

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSCleanup Acquiring Dcb lock %08lX EXCL %08lX\n",
                              &pFcb->NPFcb->Resource,
                              PsGetCurrentThread());

                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                  TRUE);

                //
                // Perform some final common processing
                //

                ASSERT( pFcb->OpenHandleCount != 0);

                InterlockedDecrement( &pFcb->OpenHandleCount);
                    
                KeQuerySystemTime( &pFcb->DirEntry->DirectoryEntry.LastAccessTime);

                //
                // Convert it to a local time
                //

                ExSystemTimeToLocalTime( &pFcb->DirEntry->DirectoryEntry.LastAccessTime,
                                         &pFcb->DirEntry->DirectoryEntry.LastAccessTime);              

                //
                // If the count has dropped to zero and there is a pending delete
                // then delete the node
                //

                if( pFcb->OpenHandleCount == 0 &&
                    BooleanFlagOn( pFcb->Flags, AFS_FCB_PENDING_DELETE))
                {

                    //
                    // Try and notify the service about the delete
                    // Need to drop the lock so we don;t lock when the
                    // invalidate call is made
                    //

                    AFSReleaseResource( &pFcb->NPFcb->Resource);

                    ntStatus = AFSNotifyDelete( pFcb);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSCleanup Acquiring Dcb (DELETE) lock %08lX EXCL %08lX\n",
                                  &pFcb->NPFcb->Resource,
                                  PsGetCurrentThread());

                    AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                      TRUE);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSCleanup Failed to notify service of deleted directory %wZ Status %08lX\n", 
                                      &pCcb->FullFileName,
                                      ntStatus);

                        ntStatus = STATUS_SUCCESS;

                        ClearFlag( pFcb->Flags, AFS_FCB_PENDING_DELETE);
                    }
                    else
                    {

                        SetFlag( pFcb->Flags, AFS_FCB_DELETED);

                        ASSERT( pFcb->ParentFcb != NULL);

                        FsRtlNotifyFullReportChange( pFcb->ParentFcb->NPFcb->NotifySync,
                                                     &pFcb->ParentFcb->NPFcb->DirNotifyList,
                                                     (PSTRING)&pCcb->FullFileName,
                                                     (USHORT)(pCcb->FullFileName.Length - pFcb->DirEntry->DirectoryEntry.FileName.Length),
                                                     (PSTRING)NULL,
                                                     (PSTRING)NULL,
                                                     (ULONG)FILE_NOTIFY_CHANGE_FILE_NAME,
                                                     (ULONG)FILE_ACTION_REMOVED,
                                                     (PVOID)NULL );
                    }
                }

                //
                // If there have been any updates to the node then push it to 
                // the service
                //

                else if( BooleanFlagOn( pFcb->Flags, AFS_FILE_MODIFIED))
                {

                    ULONG ulNotifyFilter = 0;
                    AFSFcb *pParentDcb = pRootFcb;

                    //
                    // Update the change time
                    //

                    pFcb->DirEntry->DirectoryEntry.ChangeTime = pFcb->DirEntry->DirectoryEntry.LastAccessTime;

                    //
                    // Need to drop our lock on the Fcb while this call is made since it could
                    // result in the service invalidating the node requiring the lock
                    //

                    AFSReleaseResource( &pFcb->NPFcb->Resource);

                    ntStatus = AFSUpdateFileInformation( DeviceObject,
                                                         pFcb);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSCleanup Acquiring Dcb (FILEINFO) lock %08lX EXCL %08lX\n",
                                  &pFcb->NPFcb->Resource,
                                  PsGetCurrentThread());

                    AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                      TRUE);

                    if( NT_SUCCESS( ntStatus))
                    {

                        ClearFlag( pFcb->Flags, AFS_FILE_MODIFIED);
                    }

                    ulNotifyFilter |= (FILE_NOTIFY_CHANGE_ATTRIBUTES);                    

                    if( pFcb->ParentFcb != NULL)
                    {

                        pParentDcb = pFcb->ParentFcb;
                    }

                    FsRtlNotifyFullReportChange( pParentDcb->NPFcb->NotifySync,
                                                 &pParentDcb->NPFcb->DirNotifyList,
                                                 (PSTRING)&pCcb->FullFileName,
                                                 (USHORT)(pCcb->FullFileName.Length - pFcb->DirEntry->DirectoryEntry.FileName.Length),
                                                 (PSTRING)NULL,
                                                 (PSTRING)NULL,
                                                 (ULONG)ulNotifyFilter,
                                                 (ULONG)FILE_ACTION_MODIFIED,
                                                 (PVOID)NULL);
                }

                //
                // Release the notification for this directory if there is one
                //

                FsRtlNotifyCleanup( pFcb->NPFcb->NotifySync,
                                    &pFcb->NPFcb->DirNotifyList,
                                    pCcb);

                //
                // Remove the share access at this time since we may not get the close for sometime on this FO.
                //

                IoRemoveShareAccess( pFileObject, 
                                     &pFcb->ShareAccess);

                //
                // Decrement the open child handle count
                //

                if( pFcb->ParentFcb != NULL)
                {
                   
                    InterlockedDecrement( &pFcb->ParentFcb->Specific.Directory.ChildOpenHandleCount);
                }

                //
                // And finally, release the Fcb if we acquired it.
                //

                AFSReleaseResource( &pFcb->NPFcb->Resource);

                break;
            }

            case AFS_SPECIAL_SHARE_FCB:
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSCleanup Acquiring SPECIAL SHARE lock %08lX EXCL %08lX\n",
                              &pFcb->NPFcb->Resource,
                              PsGetCurrentThread());

                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                TRUE);

                ASSERT( pFcb->OpenHandleCount != 0);

                InterlockedDecrement( &pFcb->OpenHandleCount);

                //
                // Decrement the open child handle count
                //

                if( pFcb->ParentFcb != NULL)
                {
                   
                    InterlockedDecrement( &pFcb->ParentFcb->Specific.Directory.ChildOpenHandleCount);
                }
                    
                //
                // And finally, release the Fcb if we acquired it.
                //

                AFSReleaseResource( &pFcb->NPFcb->Resource);

                break;
            }

            default:

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSCleanup Processing unknown node type %d\n", 
                              pFcb->Header.NodeTypeCode);

                break;
        }


try_exit:

        if( pFileObject != NULL)
        {

            //
            // Setup the fileobject flags to indicate cleanup is complete.
            //

            SetFlag( pFileObject->Flags, FO_CLEANUP_COMPLETE);
        }

        //
        // Complete the request
        //

        AFSCompleteRequest( Irp, ntStatus);
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgLogMsg( 0,
                      0,
                      "EXCEPTION - AFSCleanup\n");
    }

    return ntStatus;
}
