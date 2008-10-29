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
// File: AFSClose.cpp
//

#include "AFSCommon.h"

//
// Function: AFSClose
//
// Description: 
//
//      This function is the IRP_MJ_CLOSE dispatch handler
//
// Return:
//
//       A status is returned for the handling of this request
//

NTSTATUS
AFSClose( IN PDEVICE_OBJECT DeviceObject,
          IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulRequestType = 0;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSFcb *pFcb = NULL;
    AFSDeviceExt *pDeviceExt = NULL;
    AFSCcb *pCcb = NULL;

    __try
    {

        if( DeviceObject == AFSDeviceObject)
        {

            try_return( ntStatus);
        }

        pDeviceExt = (AFSDeviceExt *)DeviceObject->DeviceExtension;

        pIrpSp = IoGetCurrentIrpStackLocation( Irp);

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;

        if( pFcb == NULL)
        {

            try_return( ntStatus);
        }

        //
        // Perform the cleanup functionality depending on the type of node it is
        //

        switch( pFcb->Header.NodeTypeCode)
        {

            case AFS_IOCTL_FCB:
            {

                AFSPIOCtlOpenCloseRequestCB stPIOCtlClose;
                AFSFileID stParentFileId;

                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                  TRUE);

                pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

                //
                // Send the close to the CM
                //

                RtlZeroMemory( &stPIOCtlClose,
                               sizeof( AFSPIOCtlOpenCloseRequestCB));

                stPIOCtlClose.RequestId = pCcb->PIOCtlRequestID;

                if( pFcb->RootFcb != NULL)
                {
                    stPIOCtlClose.RootId = pFcb->RootFcb->DirEntry->DirectoryEntry.FileId;
                }

                RtlZeroMemory( &stParentFileId,
                               sizeof( AFSFileID));

                if( pFcb->ParentFcb != NULL)
                {

                    AFSFcb *pParentDcb = pFcb->ParentFcb;

                    //
                    // The parent directory FID of the node
                    //        

                    if( pParentDcb->Header.NodeTypeCode != AFS_ROOT_ALL)
                    {
                        
                        ASSERT( pParentDcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY);

                        stParentFileId = pParentDcb->DirEntry->DirectoryEntry.FileId;
                    }
                }

                //
                // Issue the open request to the service
                //

                AFSProcessRequest( AFS_REQUEST_TYPE_PIOCTL_CLOSE,
                                   AFS_REQUEST_FLAG_SYNCHRONOUS,
                                   0,
                                   NULL,
                                   &stParentFileId,
                                   (void *)&stPIOCtlClose,
                                   sizeof( AFSPIOCtlOpenCloseRequestCB),
                                   NULL,
                                   NULL);

                //
                // If we ahve a Ccb then remove it from the Fcb chain
                //

                if( pCcb != NULL)
                {

                    //
                    // Remove the Ccb and de-allocate it
                    //

                    ntStatus = AFSRemoveCcb( pFcb,
                                             pCcb);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_WARNING,
                                      "AFSClose Failed to remove Ccb from Fcb Status %08lX\n", ntStatus);

                        //
                        // We can't actually fail a close operation so reset the status
                        //

                        ntStatus = STATUS_SUCCESS;
                    }
                }

                //
                // For these Fcbs we tear them down in line since they have not been
                // added to any of the Fcb lists for post processing
                //

                InterlockedDecrement( &pFcb->OpenReferenceCount);

                //
                // If this is not the root then decrement the open child reference count
                //

                if( pFcb->ParentFcb != NULL)
                {
                   
                    InterlockedDecrement( &pFcb->ParentFcb->Specific.Directory.ChildOpenReferenceCount);
                }

                if( pFcb->OpenReferenceCount == 0)
                {

                    AFSReleaseResource( &pFcb->NPFcb->Resource);

                    AFSRemoveFcb( pFcb);
                }
                else
                {

                    AFSReleaseResource( &pFcb->NPFcb->Resource);
                }

                break;
            }

            //
            // This Fcb represents a file
            //

            case AFS_FILE_FCB:

            //
            // Root or directory node
            //

            case AFS_ROOT_FCB:
            case AFS_DIRECTORY_FCB:
            case AFS_ROOT_ALL:
            {
            
                BOOLEAN bReleaseParent = FALSE;

                pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

                //
                // We may be performing some cleanup on the Fcb so grab it exclusive to ensure no collisions
                //

                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                TRUE);

                KeQueryTickCount( &pFcb->LastAccessCount);

                //
                // If this node is deleted then we will need to drop
                // the lock, grab the parent then grab the lock again
                //

                if( BooleanFlagOn( pFcb->Flags, AFS_FCB_DELETED))
                {

                    AFSReleaseResource( &pFcb->NPFcb->Resource);

                    ASSERT( pFcb->ParentFcb != NULL);

                    AFSAcquireExcl( &pFcb->ParentFcb->NPFcb->Resource,
                                      TRUE);

                    bReleaseParent = TRUE;

                    AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                      TRUE);
                }

                //
                // If we ahve a Ccb then remove it from the Fcb chain
                //

                if( pCcb != NULL)
                {

                    //
                    // Remove the Ccb and de-allocate it
                    //

                    ntStatus = AFSRemoveCcb( pFcb,
                                               pCcb);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_WARNING,
                                      "AFSClose Failed to remove Ccb from Fcb Status %08lX\n", ntStatus);

                        //
                        // We can't actually fail a close operation so reset the status
                        //

                        ntStatus = STATUS_SUCCESS;
                    }
                }

                //
                // Decrement the reference count on the Fcb
                //

                InterlockedDecrement( &pFcb->OpenReferenceCount);

                if( pFcb->OpenReferenceCount == 0)
                {
                    
                    if( BooleanFlagOn( pFcb->Flags, AFS_FCB_DELETED))
                    {

                        if( !BooleanFlagOn( pFcb->DirEntry->Flags, AFS_DIR_RELEASE_DIRECTORY_NODE))
                        {

                            //
                            // Remove the dir node from the parent but don't delete it since we may need
                            // information in the Fcb for flushing extents
                            //

                            AFSRemoveDirNodeFromParent( pFcb->ParentFcb,
                                                        pFcb->DirEntry);

                            SetFlag( pFcb->DirEntry->Flags, AFS_DIR_RELEASE_DIRECTORY_NODE);
                        }
                    }
                    else if( pFcb->Header.NodeTypeCode == AFS_FILE_FCB)
                    {

                        //
                        // Attempt to tear down our extent list for the file
                        // If there are remaining dirty extents then attempt to
                        // flush them as well
                        //

                        if( pFcb->Specific.File.ExtentsDirtyCount)
                        {

                            AFSFlushExtents( pFcb);
                        }

                        AFSTearDownFcbExtents( pFcb);
                    }
                }

                //
                // If this is not the root then decrement the open child reference count
                //

                if( pFcb->ParentFcb != NULL)
                {
                   
                    InterlockedDecrement( &pFcb->ParentFcb->Specific.Directory.ChildOpenReferenceCount);
                }

                AFSReleaseResource( &pFcb->NPFcb->Resource);

                if( bReleaseParent)
                {

                    AFSReleaseResource( &pFcb->ParentFcb->NPFcb->Resource);
                }

                break;
            }
           
            default:

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSClose Processing unknown node type %d\n", 
                                                    pFcb->Header.NodeTypeCode);

                break;
        }

try_exit:
        
        //
        // Complete the request
        //

        AFSCompleteRequest( Irp,
                              ntStatus);
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgLogMsg( 0,
                      0,
                      "EXCEPTION - AFSClose\n");
    }

    return ntStatus;
}
