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

                DbgPrint("AFSCleanup Closing redirector\n");

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

                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                TRUE);

                InterlockedDecrement( &pFcb->OpenHandleCount);

                AFSReleaseResource( &pFcb->NPFcb->Resource);

                break;
            }

            case AFS_IOCTL_FCB:
            {

                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                  TRUE);

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
                           
                //
                // We may be performing some cleanup on the Fcb so grab it exclusive to ensure no collisions
                //

                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                  TRUE);

                //
                // Uninitialize the cache map. This call is unconditional.
                //

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
                // Perform some final common processing
                //

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

                    if( pFcb->Header.NodeTypeCode == AFS_FILE_FCB)
                    {

                        //
                        // We will attmept to flush extents prior to tearing them down in the 
                        // event that the delete fails to the service. And we need to do this
                        // before telling the service the file is deleted otherwise the extent
                        // flushing will fail
                        //

                        if( pFcb->Specific.File.ExtentsDirtyCount)
                        {

                            AFSFlushExtents( pFcb);
                        }

                        AFSTearDownFcbExtents( pFcb);
                    }

                    //
                    // Try and notify the service about the delete
                    // We need to drop teh locks on the Fcb since this may cause re-entrancy
                    // for the invalidation of the node
                    //

                    AFSReleaseResource( &pFcb->NPFcb->Resource);

                    ntStatus = AFSNotifyDelete( pFcb);

                    AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                      TRUE);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSPrint("AFSCleanup Failed to notify service of deleted file %wZ Status %08lX\n", 
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
                // If there have been any updates to teh node then push it to 
                // the service
                //

                else if( BooleanFlagOn( pFcb->Flags, AFS_FILE_MODIFIED))
                {

                    ULONG ulNotifyFilter = 0;

                    //
                    // Update teh change time
                    //

                    pFcb->DirEntry->DirectoryEntry.ChangeTime = pFcb->DirEntry->DirectoryEntry.LastAccessTime;

                    //
                    // Need to drop our lock on teh Fcb while this call is made since it could
                    // result in the service invalidating the node requiring the lock
                    //

                    AFSReleaseResource( &pFcb->NPFcb->Resource);

                    ntStatus = AFSUpdateFileInformation( DeviceObject,
                                                           pFcb);

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

                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                  TRUE);

                //
                // Perform some final common processing
                //

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

                    AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                      TRUE);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSPrint("AFSCleanup Failed to notify service of deleted directory %wZ Status %08lX\n", 
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
                // If there have been any updates to teh node then push it to 
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
                    // Need to drop our lock on teh Fcb while this call is made since it could
                    // result in the service invalidating the node requiring the lock
                    //

                    AFSReleaseResource( &pFcb->NPFcb->Resource);

                    ntStatus = AFSUpdateFileInformation( DeviceObject,
                                                           pFcb);

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

            default:

                AFSPrint("AFSCleanup Processing unknown node type %d\n", pFcb->Header.NodeTypeCode);

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

        AFSCompleteRequest( Irp,
                              ntStatus);
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSCleanup\n");
    }

    return ntStatus;
}
