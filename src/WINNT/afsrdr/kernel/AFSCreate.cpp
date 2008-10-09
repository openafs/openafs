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
// File: AFSCreate.cpp
//

#include "AFSCommon.h"

//
// Function: AFSCreate
//
// Description:
//
//      This function is the dispatch handler for the IRP_MJ_CREATE requests. It makes the determination to 
//      which interface this request is destined. 
//
// Return:
//
//      A status is returned for the function. The Irp completion processing is handled in the specific
//      interface handler.
//

NTSTATUS
AFSCreate( IN PDEVICE_OBJECT DeviceObject,
           IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    __try
    {

        if( DeviceObject == AFSDeviceObject)
        {

            ntStatus = AFSControlDeviceCreate( Irp);

            try_return( ntStatus);
        }

        ntStatus = AFSCommonCreate( DeviceObject,
                                    Irp);

try_exit:

        NOTHING;
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSCreate\n");

        ntStatus = STATUS_ACCESS_DENIED;
    }

    //
    // Complete the request
    //

    AFSCompleteRequest( Irp,
                          ntStatus);

    return ntStatus;
}

NTSTATUS
AFSCommonCreate( IN PDEVICE_OBJECT DeviceObject,
                 IN PIRP Irp)
{

    NTSTATUS            ntStatus = STATUS_SUCCESS;
    UNICODE_STRING      uniFileName;
    ULONG               ulCreateDisposition = 0;
    ULONG               ulOptions = 0;
    BOOLEAN             bNoIntermediateBuffering = FALSE;
    FILE_OBJECT        *pFileObject = NULL;    
    IO_STACK_LOCATION  *pIrpSp;
    AFSFcb             *pFcb = NULL, *pParentDcb = NULL;
    AFSCcb             *pCcb = NULL;
    AFSDeviceExt       *pDeviceExt = NULL;
    ULONG               ulOpenOptions = 0;
    BOOLEAN             bOpenTargetDirectory = FALSE, bReleaseVolume = FALSE;
    BOOLEAN             bReleaseParent = FALSE, bReleaseFcb = FALSE;
    PACCESS_MASK        pDesiredAccess = NULL;
    BOOLEAN             bFreeNameString = FALSE;
    UNICODE_STRING      uniComponentName, uniTargetName, uniPathName, uniFullFileName;
    AFSFcb             *pRootFcb = NULL;
    BOOLEAN             bReleaseRootFcb = FALSE;
    UNICODE_STRING      uniSubstitutedPathName;

    __Enter
    {

        pIrpSp = IoGetCurrentIrpStackLocation( Irp);

        pDeviceExt = (AFSDeviceExt *)DeviceObject->DeviceExtension;

        ulCreateDisposition = (pIrpSp->Parameters.Create.Options >> 24) & 0x000000ff;

        ulOptions = pIrpSp->Parameters.Create.Options;
           
        bNoIntermediateBuffering = BooleanFlagOn( ulOptions, FILE_NO_INTERMEDIATE_BUFFERING);

        bOpenTargetDirectory = BooleanFlagOn( pIrpSp->Flags, SL_OPEN_TARGET_DIRECTORY);

        pFileObject = pIrpSp->FileObject;

        uniFileName.Length = uniFileName.MaximumLength = 0;
        uniFileName.Buffer = NULL;

        uniSubstitutedPathName.Buffer = NULL;
        uniSubstitutedPathName.Length = 0;

        if( pFileObject == NULL ||
            pFileObject->FileName.Buffer == NULL)
        {

            Irp->IoStatus.Information = FILE_OPENED;

            try_return( ntStatus);
        }

        pDesiredAccess = &pIrpSp->Parameters.Create.SecurityContext->DesiredAccess;

        //
        // Validate that the AFS Root has been initialized
        //

        if( AFSGlobalRoot == NULL ||
            !BooleanFlagOn( AFSGlobalRoot->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
        {

            //
            // If we have a root node then try to enumerate it
            //

            if( AFSGlobalRoot != NULL)
            {

                AFSAcquireExcl( AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                TRUE);

                //
                // Check again in case we raced with another request
                //

                if( !BooleanFlagOn( AFSGlobalRoot->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
                {

                    //
                    // Initialize the root information
                    //

                    AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.ContentIndex = 1;

                    //
                    // Enumerate the shares in the volume
                    //

                    ntStatus = AFSEnumerateDirectory( &AFSGlobalRoot->DirEntry->DirectoryEntry.FileId,
                                                      &AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr,
                                                      &AFSGlobalRoot->Specific.Directory.DirectoryNodeListHead,
                                                      &AFSGlobalRoot->Specific.Directory.DirectoryNodeListTail,
                                                      NULL,
                                                      NULL);

                    if( NT_SUCCESS( ntStatus))
                    {

                        //
                        // Indicate the node is initialized
                        //

                        SetFlag( AFSGlobalRoot->Flags, AFS_FCB_DIRECTORY_ENUMERATED);
                    }
                }

                AFSReleaseResource( AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.TreeLock);
            }

            if( AFSGlobalRoot == NULL ||
                !BooleanFlagOn( AFSGlobalRoot->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
            {

                DbgPrint("AFSCommonCreate Failed to init root\n");

                try_return( ntStatus = STATUS_DEVICE_NOT_READY);
            }
        }

        //
        // Go and parse the name for processing
        //

        ntStatus = AFSParseName( Irp,
                                 &uniFileName,
                                 &uniFullFileName,
                                 &bFreeNameString,
                                 &pRootFcb,
                                 &pParentDcb);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // If the returned root Fcb is NULL then we are dealing with the \\Server\GlobalRoot 
        // name 
        //

        if( pRootFcb == NULL)
        {

            //
            // Remove any leading or trailing slashes
            //

            if( uniFileName.Length >= sizeof( WCHAR) &&
                uniFileName.Buffer[ (uniFileName.Length/sizeof( WCHAR)) - 1] == L'\\')
            {

                uniFileName.Length -= sizeof( WCHAR);
            }

            if( uniFileName.Length >= sizeof( WCHAR) &&
                uniFileName.Buffer[ 0] == L'\\')
            {

                uniFileName.Buffer = &uniFileName.Buffer[ 1];

                uniFileName.Length -= sizeof( WCHAR);
            }

            //
            // If there is a remaining portion returned for this request then
            // check if it is for the PIOCtl interface
            //

            if( uniFileName.Length > 0)
            {

                //
                // We don't accept any other opens off of the AFS Root
                //

                ntStatus = STATUS_OBJECT_NAME_NOT_FOUND;

                //
                // If this is an open on "_._AFS_IOCTL_._" then perform handling on it accordingly
                //

                if( RtlCompareUnicodeString( &AFSPIOCtlName,
                                             &uniFileName,
                                             TRUE) == 0)
                {

                    ntStatus = AFSOpenIOCtlFcb( Irp,
                                                AFSGlobalRoot,
                                                &pFcb,
                                                &pCcb);
                }

                try_return( ntStatus);
            }

            ntStatus = AFSOpenAFSRoot( Irp,
                                       &pFcb,
                                       &pCcb);

            try_return( ntStatus);
        }

        //
        // We have our root node exclusive now
        //

        bReleaseRootFcb = TRUE;

        //
        // Be sure the root volume of this request is ONLINE
        //

        ASSERT( pRootFcb->RootFcb != NULL);
        
        if( BooleanFlagOn( pRootFcb->RootFcb->Flags, AFS_FCB_VOLUME_OFFLINE))
        {

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        //
        // Perform some initial sanity checks
        //

        if( uniFileName.Buffer == NULL ||
            ( uniFileName.Length == sizeof(WCHAR) &&
              uniFileName.Buffer[0] == L'\\') )
        {

            //
            // Check for the delete on close flag for the root
            //

            if( BooleanFlagOn( ulOptions, FILE_DELETE_ON_CLOSE )) 
            {

                AFSPrint("AFSCommonCreate Attempt to open root as delete on close\n");

                try_return( ntStatus = STATUS_CANNOT_DELETE );
            }

            //
            // If this is the target directory, then bail
            //

            if( bOpenTargetDirectory) 
            {

                AFSPrint("AFSCommonCreate Attempt to open root as target directory\n");
                   
                try_return( ntStatus = STATUS_INVALID_PARAMETER);
            }

            //
            // Go and open the root of the volume
            //

            ntStatus = AFSOpenRoot( Irp,
                                    pRootFcb,
                                    &pCcb);

            if( NT_SUCCESS( ntStatus))
            {

                pFcb = pRootFcb;

                //
                // Need to release the Fcb below
                //

                bReleaseFcb = TRUE;
            }

            try_return( ntStatus);
        }

        //
        // If this is a target directory open then fixup the name to not include
        // the final component
        //

        if( bOpenTargetDirectory)
        {

            AFSFixupTargetName( &uniFileName,
                                &uniTargetName);

            //
            // Adjust the length in the fileobject so we process it
            // correctly below.
            //

            pFileObject->FileName.Length -= uniTargetName.Length;

            //
            // If only the root remains then update the parent
            //

            if( uniFileName.Length == sizeof( WCHAR))
            {

                pFcb = pRootFcb;

                bReleaseFcb = TRUE;
            }
        }

        //
        // Attempt to locate the node in the name tree if this is not a target
        // open and the target is not the root
        //

        if( uniFileName.Length > sizeof( WCHAR))
        {

            uniSubstitutedPathName = uniFileName;

            ntStatus = AFSLocateNameEntry( pRootFcb,
                                           pFileObject,
                                           &uniFileName,
                                           &pParentDcb,
                                           &pFcb,
                                           &uniComponentName);

            if( !NT_SUCCESS( ntStatus) &&
                ntStatus != STATUS_OBJECT_NAME_NOT_FOUND)
            {

                uniSubstitutedPathName.Buffer = NULL;

                //
                // The routine above released the root while walking the
                // branch
                //

                bReleaseRootFcb = FALSE;

                try_return( ntStatus);
            }

            //
            // If we re-allocated the name then update our substitue name
            //

            if( uniSubstitutedPathName.Buffer != uniFileName.Buffer)
            {

                uniSubstitutedPathName = uniFileName;
            }
            else
            {

                uniSubstitutedPathName.Buffer = NULL;
            }

            //
            // If the parent is not the root then we'll release the parent
            // not the root since it was already released
            //

            if( pParentDcb != pRootFcb)
            {

                bReleaseParent = TRUE;

                bReleaseRootFcb = FALSE;
            }
        }

        if( pFcb != NULL)
        {

            bReleaseFcb = TRUE;

            //
            // Is the node pending delete?
            //

            if( BooleanFlagOn( pFcb->Flags, AFS_FCB_PENDING_DELETE))
            {

                try_return( ntStatus = STATUS_FILE_DELETED);
            }
        }

        if( bOpenTargetDirectory)
        {

            if( pFcb == NULL)
            {

                //
                // Opening relative to a root of a volume
                //

                pFcb = pParentDcb;

                //
                // Need to release the Fcb below
                //

                bReleaseFcb = TRUE;

                bReleaseParent = FALSE;
            }

            //
            // OK, open the target directory
            //

            ntStatus = AFSOpenTargetDirectory( Irp,
                                               pFcb,
                                               &uniTargetName,
                                               &pCcb);

            try_return( ntStatus);
        }

        //
        // Based on the options passed in, process the file accordingly.
        //

        if( ulCreateDisposition == FILE_CREATE ||
            ( ( ulCreateDisposition == FILE_OPEN_IF ||
                ulCreateDisposition == FILE_OVERWRITE_IF) &&
              pFcb == NULL))
        {

            //
            // If this is a create request and we have an Fcb then
            // fail it
            //

            if( pFcb != NULL)
            {

                //
                // Check if the returned Fcb is an exact match on the name. If it is
                // then fail with a collision otherwise we will insert the new entry
                // as a case sensitive entry
                //

                if( RtlCompareUnicodeString( &pFcb->DirEntry->DirectoryEntry.FileName,
                                             &uniComponentName,
                                             FALSE) == 0)
                {

                    try_return( ntStatus = STATUS_OBJECT_NAME_COLLISION);
                }

                //
                // Drop the Fcb and attmept to create the new entry
                //

                AFSReleaseResource( &pFcb->NPFcb->Resource);

                bReleaseFcb = FALSE;

                pFcb = NULL;
            }

            //
            // OK, go and create the node
            //

            ntStatus = AFSProcessCreate( Irp,
                                         pParentDcb,
                                         &uniFileName,
                                         &uniComponentName,
                                         &uniFullFileName,
                                         &pFcb,
                                         &pCcb);

            try_return( ntStatus);
        }
            
        if( pFcb == NULL)
        {

            //
            // If there is no more name to be processed then this is a root open
            //

            if( uniComponentName.Length == 0)
            {

                //
                // Go and open the root of the volume
                //

                ntStatus = AFSOpenRoot( Irp,
                                        pParentDcb,
                                        &pCcb);

                if( NT_SUCCESS( ntStatus))
                {

                    pFcb = pParentDcb;

                    //
                    // Need to release the Fcb below
                    //

                    bReleaseFcb = TRUE;

                    bReleaseParent = FALSE;
                }

                try_return( ntStatus);
            }

            //
            // If this is an open on "_._AFS_IOCTL_._" then perform handling on it accordingly
            //

            if( RtlCompareUnicodeString( &AFSPIOCtlName,
                                         &uniComponentName,
                                         TRUE) == 0)
            {

                ntStatus = AFSOpenIOCtlFcb( Irp,
                                            pParentDcb,
                                            &pFcb,
                                            &pCcb);

                try_return( ntStatus);
            }

            //
            // Not found
            //

            try_return( ntStatus = STATUS_OBJECT_NAME_NOT_FOUND);
        }

        if( ulCreateDisposition == FILE_OVERWRITE ||
            ulCreateDisposition == FILE_SUPERSEDE ||
            ulCreateDisposition == FILE_OVERWRITE_IF)
        {

            //
            // Go process a file for overwrite or supersede.
            //

            ntStatus = AFSProcessOverwriteSupersede( Irp,
                                                     pParentDcb,
                                                     pFcb,
                                                     &pCcb);

            try_return( ntStatus);
        }

        //
        // Trying to open the file
        //

        ntStatus = AFSProcessOpen( Irp,
                                   pParentDcb,
                                   pFcb,
                                   &pCcb);

try_exit:

        if( NT_SUCCESS( ntStatus))
        {

            if( pCcb != NULL)
            {

                //
                // If we have a substitue name then use it
                //

                if( uniSubstitutedPathName.Buffer != NULL)
                {

                    pCcb->FullFileName = uniSubstitutedPathName;

                    SetFlag( pCcb->Flags, CCB_FLAG_FREE_FULL_PATHNAME);

                    bFreeNameString = FALSE;
                }
                else
                {

                    pCcb->FullFileName = uniFullFileName;

                    if( bFreeNameString)
                    {

                        SetFlag( pCcb->Flags, CCB_FLAG_FREE_FULL_PATHNAME);

                        bFreeNameString = FALSE;
                    }
                }
            }

            //
            // If we make it here then init the FO for the request.
            //

            pFileObject->FsContext = (void *)pFcb;

            pFileObject->FsContext2 = (void *)pCcb;

            if( pFcb != NULL)
            {

                //
                // For files setup the SOP's
                //

                if( pFcb->Header.NodeTypeCode == AFS_FILE_FCB)
                {

                    pFileObject->SectionObjectPointer = &pFcb->NPFcb->SectionObjectPointers;
                }

                //
                // If the user did not request nobuffering then mark the FO as cacheable
                //

                if( !bNoIntermediateBuffering) 
                {

                    pFileObject->Flags |= FO_CACHE_SUPPORTED;             
                }

                //
                // If the file was opened for execution then we need to set the bit in the FO
                //

                if( BooleanFlagOn( *pDesiredAccess, 
                                   FILE_EXECUTE)) 
                {

                    SetFlag( pFileObject->Flags, FO_FILE_FAST_IO_READ);
                }
            }
        }
        else
        {

            //
            // Free up the sub name if we have one
            //

            if( uniSubstitutedPathName.Buffer != NULL)
            {

                ExFreePool( uniSubstitutedPathName.Buffer);
            }
        }

        if( bFreeNameString)
        {

            ExFreePool( uniFullFileName.Buffer);
        }

        //
        // Release the Fcbs
        //

        if( bReleaseParent)
        {

            AFSReleaseResource( &pParentDcb->NPFcb->Resource);
        }

        if( bReleaseFcb)
        {

            AFSReleaseResource( &pFcb->NPFcb->Resource);
        }

        if( bReleaseRootFcb &&
            pFcb != pRootFcb)
        {

            AFSReleaseResource( &pRootFcb->NPFcb->Resource);
        }


        //
        // Setup the Irp for completion, the Information has been set previously
        //

        Irp->IoStatus.Status = ntStatus;
    }

    return ntStatus;
}

NTSTATUS
AFSOpenAFSRoot( IN PIRP Irp,
                IN AFSFcb **Fcb,
                IN AFSCcb **Ccb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    __Enter
    {

        //
        // Initialize the Ccb for the file.
        //

        ntStatus = AFSInitCcb( AFSGlobalRoot,
                               Ccb);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSOpenAFSRoot Failed to initialize Ccb Status %08lX\n", ntStatus);

            try_return( ntStatus);
        }

        //
        // Increment the open count on this Fcb
        //

        InterlockedIncrement( &AFSGlobalRoot->OpenReferenceCount);

        InterlockedIncrement( &AFSGlobalRoot->OpenHandleCount);

        *Fcb = AFSGlobalRoot;

        //
        // Return the open result for this file
        //

        Irp->IoStatus.Information = FILE_OPENED;

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSOpenRoot( IN PIRP Irp,
             IN AFSFcb *RootFcb,
             IN AFSCcb **Ccb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    PFILE_OBJECT pFileObject = NULL;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    PACCESS_MASK pDesiredAccess = NULL;
    USHORT usShareAccess;
    BOOLEAN bRemoveAccess = FALSE;
    BOOLEAN bAllocatedCcb = FALSE;

    __Enter
    {

        pDesiredAccess = &pIrpSp->Parameters.Create.SecurityContext->DesiredAccess;
        usShareAccess = pIrpSp->Parameters.Create.ShareAccess;

        pFileObject = pIrpSp->FileObject;

        //
        // If we have no FID for this root then try to evaluate it
        //

        if( RootFcb->DirEntry->DirectoryEntry.FileId.Hash == 0)
        {

            AFSDirEnumEntry *pDirEnumCB = NULL;            

            ntStatus = AFSEvaluateTargetByID( &RootFcb->DirEntry->DirectoryEntry.FileId,
                                              &pDirEnumCB);

            if( !NT_SUCCESS( ntStatus))
            {

                try_return( ntStatus);
            }

            //
            // If the target fid is zero it could not be evaluated
            //

            if( pDirEnumCB->TargetFileId.Hash == 0)
            {

                ExFreePool( pDirEnumCB);

                try_return( ntStatus = STATUS_NO_SUCH_FILE);
            }

            //
            // Update the target fid in the volume entry and Fcb
            //
                
            RootFcb->DirEntry->DirectoryEntry.FileId = pDirEnumCB->TargetFileId;

            RootFcb->DirEntry->DirectoryEntry.DataVersion = pDirEnumCB->DataVersion;

            ExFreePool( pDirEnumCB);
        }

        //
        // If there are current opens on the Fcb, check the access. 
        //

        if( RootFcb->OpenHandleCount > 0)
        {

            ntStatus = IoCheckShareAccess( *pDesiredAccess,
                                           usShareAccess,
                                           pFileObject,
                                           &RootFcb->ShareAccess,
                                           FALSE);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSPrint("AFSOpenRoot Access check failure Status %08lX\n", ntStatus);

                try_return( ntStatus);
            }
        }

        //
        // TODO: Pass this open to AFS CM
        //




        //
        // Initialize the Ccb for the file.
        //

        ntStatus = AFSInitCcb( RootFcb,
                               Ccb);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSOpenRoot Failed to initialize Ccb Status %08lX\n", ntStatus);

            try_return( ntStatus);
        }

        bAllocatedCcb = TRUE;

        //
        // OK, update the share access on the fileobject
        //

        if( RootFcb->OpenHandleCount > 0)
        {

            IoUpdateShareAccess( pFileObject, 
                                 &RootFcb->ShareAccess);
        }
        else
        {

            //
            // Set the access
            //

            IoSetShareAccess( *pDesiredAccess,
                              usShareAccess,
                              pFileObject,
                              &RootFcb->ShareAccess);
        }

        //
        // Increment the open count on this Fcb
        //

        InterlockedIncrement( &RootFcb->OpenReferenceCount);

        InterlockedIncrement( &RootFcb->OpenHandleCount);

        //
        // Return the open result for this file
        //

        Irp->IoStatus.Information = FILE_OPENED;

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( bAllocatedCcb)
            {

                AFSRemoveCcb( RootFcb,
                                *Ccb);

                *Ccb = NULL;
            }

            if( bRemoveAccess)
            {

                IoRemoveShareAccess( pFileObject, 
                                     &RootFcb->ShareAccess);
            }

            Irp->IoStatus.Information = 0;
        }
    }

    return ntStatus;
}

NTSTATUS
AFSProcessCreate( IN PIRP             Irp,
                  IN AFSFcb          *ParentDcb,
                  IN PUNICODE_STRING  FileName,
                  IN PUNICODE_STRING  ComponentName,
                  IN PUNICODE_STRING  FullFileName,
                  IN OUT AFSFcb      **Fcb,
                  IN OUT AFSCcb      **Ccb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    PFILE_OBJECT pFileObject = NULL;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    ULONG ulOptions = 0;
    ULONG ulShareMode = 0;
    ULONG ulAccess = 0;
    ULONG ulAttributes = 0;
    LARGE_INTEGER   liAllocationSize = {0,0};
    BOOLEAN bFileCreated = FALSE, bRemoveFcb = FALSE, bAllocatedCcb = FALSE;
    PACCESS_MASK pDesiredAccess = NULL;
    USHORT usShareAccess;
    AFSDirEntryCB *pDirEntry = NULL;

    __Enter
    {

        pDesiredAccess = &pIrpSp->Parameters.Create.SecurityContext->DesiredAccess;
        usShareAccess = pIrpSp->Parameters.Create.ShareAccess;

        pFileObject = pIrpSp->FileObject;

        //
        // Extract out the options
        //

        ulOptions = pIrpSp->Parameters.Create.Options;

        //
        // We pass all attributes they want to apply to the file to the create
        //

        ulAttributes = pIrpSp->Parameters.Create.FileAttributes;

        //
        // If this is a directory create then set the attribute correctly
        //

        if( ulOptions & FILE_DIRECTORY_FILE)
        {

            ulAttributes |= FILE_ATTRIBUTE_DIRECTORY;
        }

        //
        // Allocate and insert the direntry into the parent node
        //

        ntStatus = AFSCreateDirEntry( ParentDcb,
                                      FileName,
                                      ComponentName,
                                      ulAttributes,
                                      &pDirEntry);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        bFileCreated = TRUE;

        //
        // Allocate and initialize the Fcb for the file.
        //

        ntStatus = AFSInitFcb( ParentDcb,
                               pDirEntry,
                               Fcb);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSProcessCreate Failed to initialize Fcb Status %08lX\n", ntStatus);

            try_return( ntStatus);
        }

        bRemoveFcb = TRUE;

        //
        // Initialize the Ccb for the file.
        //

        ntStatus = AFSInitCcb( *Fcb,
                               Ccb);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSProcessCreate Failed to initialize Ccb Status %08lX\n", ntStatus);

            try_return( ntStatus);
        }

        bAllocatedCcb = TRUE;

        //
        // If this is a file, update the headers filesizes. 
        //

        if( (*Fcb)->Header.NodeTypeCode == AFS_FILE_FCB)
        {

            //
            // Update the sizes with the information passed in
            //

            (*Fcb)->Header.AllocationSize.QuadPart  = 0;
            (*Fcb)->Header.FileSize.QuadPart        = 0;
            (*Fcb)->Header.ValidDataLength.QuadPart = 0;
        }

        //
        // Update teh last write time of the . and .. entries for the parent directory
        // If this is not the root directory
        //

        else if( (*Fcb)->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
        {

            AFSDirEntryCB *pParentNode = ParentDcb->Specific.Directory.DirectoryNodeListHead;

            //
            // Add the . and .. entries to the entry
            //

            ntStatus = AFSInitializeDirectory( *Fcb);

            if( !NT_SUCCESS( ntStatus))
            {

                try_return( ntStatus);
            }

            SetFlag( (*Fcb)->Flags, AFS_FCB_DIRECTORY_ENUMERATED);

            //
            // Update the . and .. entries if this is not the root
            //

            if( ParentDcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
            {

                ASSERT( pParentNode != NULL);

                KeQuerySystemTime( &pParentNode->DirectoryEntry.LastWriteTime);

                //
                // Convert it to a local time
                //

                ExSystemTimeToLocalTime( &pParentNode->DirectoryEntry.LastWriteTime,
                                         &pParentNode->DirectoryEntry.LastWriteTime);

                //
                // The .. entry
                //

                pParentNode = (AFSDirEntryCB *)pParentNode->ListEntry.fLink;

                if( pParentNode != NULL)
                {

                    KeQuerySystemTime( &pParentNode->DirectoryEntry.LastWriteTime);

                    //
                    // Convert it to a local time
                    //

                    ExSystemTimeToLocalTime( &pParentNode->DirectoryEntry.LastWriteTime,
                                             &pParentNode->DirectoryEntry.LastWriteTime);
                }
            }

            //
            // And finally the parents directory entry itself
            //

            KeQuerySystemTime( &ParentDcb->DirEntry->DirectoryEntry.LastWriteTime);

            ExSystemTimeToLocalTime( &ParentDcb->DirEntry->DirectoryEntry.LastWriteTime,
                                     &ParentDcb->DirEntry->DirectoryEntry.LastWriteTime);
        }

        //
        // Notify the system of the addition
        //

        FsRtlNotifyFullReportChange( ParentDcb->NPFcb->NotifySync,
									 &ParentDcb->NPFcb->DirNotifyList,
									 (PSTRING)FullFileName,
									 (USHORT)(FullFileName->Length - (*Fcb)->DirEntry->DirectoryEntry.FileName.Length),
									 (PSTRING)NULL,
									 (PSTRING)NULL,
									 (ULONG)FILE_NOTIFY_CHANGE_DIR_NAME,
									 (ULONG)FILE_ACTION_ADDED,
									 (PVOID)NULL);

        //
        // Save off the access for the open
        //

        IoSetShareAccess( *pDesiredAccess,
                          usShareAccess,
                          pFileObject,
                          &(*Fcb)->ShareAccess);

        //
        // Increment the open count on this Fcb
        //

        InterlockedIncrement( &(*Fcb)->OpenReferenceCount);

        InterlockedIncrement( &(*Fcb)->OpenHandleCount);

        //
        // Increment the open reference and handle on the parent node
        //

        (*Fcb)->ParentFcb->Specific.Directory.ChildOpenHandleCount++;

        (*Fcb)->ParentFcb->Specific.Directory.ChildOpenReferenceCount++;

        if( ulOptions & FILE_DELETE_ON_CLOSE)
        {

            //
            // Mark it for delete on close
            //

            SetFlag( (*Fcb)->Flags, AFS_FCB_PENDING_DELETE);
        }

        //
        // Return the open result for this file
        //

        Irp->IoStatus.Information = FILE_CREATED;

try_exit:

        //
        // If we created the Fcb we need to release the resources
        //

        if( bRemoveFcb)
        {

            AFSReleaseResource( &(*Fcb)->NPFcb->Resource);
        }

        if( !NT_SUCCESS( ntStatus))
        {

            if( bFileCreated)
            {

                //
                // If the entry was not inserted then don't remove it
                //

                if( BooleanFlagOn( pDirEntry->Flags, AFS_DIR_RELEASE_DIRECTORY_NODE))
                {

                    ExFreePool( pDirEntry);
                }
                else
                {

                    //
                    // Remove the dir entry from the parent
                    //

                    AFSDeleteDirEntry( ParentDcb,
                                       pDirEntry);
                }

                if( *Fcb != NULL)
                {

                    (*Fcb)->DirEntry = NULL;
                }
            }

            if( bAllocatedCcb)
            {

                AFSRemoveCcb( *Fcb,
                              *Ccb);

                *Ccb = NULL;
            }

            if( bRemoveFcb)
            {

                //
                // Mark the Fcb as invalid so our workler thread will clean it up
                //

                (*Fcb)->Header.NodeTypeCode = AFS_INVALID_FCB;
            }

            *Fcb = NULL;

            *Ccb = NULL;
        }
    }

    return ntStatus;
}

NTSTATUS
AFSOpenTargetDirectory( IN PIRP Irp,
                        IN AFSFcb *Fcb,
                        IN PUNICODE_STRING TargetName,
                        IN OUT AFSCcb **Ccb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    PFILE_OBJECT pFileObject = NULL;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    PACCESS_MASK pDesiredAccess = NULL;
    USHORT usShareAccess;
    BOOLEAN bRemoveAccess = FALSE;
    BOOLEAN bAllocatedCcb = FALSE;
    ULONG   ulFileIndex = 0;
    AFSDirEntryCB *pDirEntry = NULL;

    __try
    {

        pDesiredAccess = &pIrpSp->Parameters.Create.SecurityContext->DesiredAccess;
        usShareAccess = pIrpSp->Parameters.Create.ShareAccess;

        pFileObject = pIrpSp->FileObject;

        if( Fcb->Header.NodeTypeCode != AFS_DIRECTORY_FCB &&
            Fcb->Header.NodeTypeCode != AFS_ROOT_FCB)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //
        // If there are current opens on the Fcb, check the access. 
        //

        if( Fcb->OpenHandleCount > 0)
        {

            ntStatus = IoCheckShareAccess( *pDesiredAccess,
                                           usShareAccess,
                                           pFileObject,
                                           &Fcb->ShareAccess,
                                           FALSE);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSPrint("AFSOpenTargetDirectory Access check failure Status %08lX\n", ntStatus);

                try_return( ntStatus);
            }
        }

        //
        // Initialize the Ccb for the file.
        //

        ntStatus = AFSInitCcb( Fcb,
                               Ccb);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSOpenTargetDirectory Failed to initialize Ccb Status %08lX\n", ntStatus);

            try_return( ntStatus);
        }

        bAllocatedCcb = TRUE;

        //
        // We do a quick check to see if the target name is currently active
        //

        ulFileIndex = AFSGenerateCRC( TargetName,
                                      FALSE);

        AFSLocateCaseSensitiveDirEntry( Fcb->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                        ulFileIndex,
                                        &pDirEntry);

        if( pDirEntry != NULL)       
        {

            //
            // Set the return status accordingly
            //

            Irp->IoStatus.Information = FILE_EXISTS;
        }
        else
        {

            ulFileIndex = AFSGenerateCRC( TargetName,
                                          TRUE);

            AFSLocateCaseInsensitiveDirEntry( Fcb->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead,
                                              ulFileIndex,
                                              &pDirEntry);

            if( pDirEntry != NULL)
            {

                Irp->IoStatus.Information = FILE_EXISTS;
            }
            else
            {

                Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
            }
        }

        //
        // Update the filename in the fileobject for rename processing
        //

        RtlCopyMemory( pFileObject->FileName.Buffer,
                       TargetName->Buffer,
                       TargetName->Length);

        pFileObject->FileName.Length = TargetName->Length;

        //
        // OK, update the share access on the fileobject
        //

        if( Fcb->OpenHandleCount > 0)
        {

            IoUpdateShareAccess( pFileObject, 
                                 &Fcb->ShareAccess);
        }
        else
        {

            //
            // Set the access
            //

            IoSetShareAccess( *pDesiredAccess,
                              usShareAccess,
                              pFileObject,
                              &Fcb->ShareAccess);
        }

        //
        // Increment the open count on this Fcb
        //

        InterlockedIncrement( &Fcb->OpenReferenceCount);

        InterlockedIncrement( &Fcb->OpenHandleCount);

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( bAllocatedCcb)
            {

                AFSRemoveCcb( Fcb,
                                *Ccb);

                *Ccb = NULL;
            }

            if( bRemoveAccess)
            {

                IoRemoveShareAccess( pFileObject, 
                                     &Fcb->ShareAccess);
            }
        }
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSOpenTargetDirectory\n");
    }

    return ntStatus;
}

NTSTATUS
AFSProcessOpen( IN PIRP Irp,
                IN AFSFcb *ParentDcb,
                IN OUT AFSFcb *Fcb,
                IN OUT AFSCcb **Ccb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    PFILE_OBJECT pFileObject = NULL;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    PACCESS_MASK pDesiredAccess = NULL;
    USHORT usShareAccess;
    BOOLEAN bAllocatedCcb = FALSE;
    ULONG ulAdditionalFlags = 0, ulOptions = 0;
    BOOLEAN bDecrementOpenRefCount = FALSE;
    AFSFileOpenCB   stOpenCB;
    AFSFileOpenResultCB stOpenResultCB;
    ULONG       ulResultLen = 0;

    __Enter
    {

        pDesiredAccess = &pIrpSp->Parameters.Create.SecurityContext->DesiredAccess;
        usShareAccess = pIrpSp->Parameters.Create.ShareAccess;

        pFileObject = pIrpSp->FileObject;

        //
        // Extract out the options
        //

        ulOptions = pIrpSp->Parameters.Create.Options;

        //
        // If there are current opens on the Fcb, check the access. 
        //

        if( Fcb->OpenHandleCount > 0)
        {

            ntStatus = IoCheckShareAccess( *pDesiredAccess,
                                           usShareAccess,
                                           pFileObject,
                                           &Fcb->ShareAccess,
                                           FALSE);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSPrint("AFSProcessOpen Access check failure Status %08lX\n", ntStatus);

                try_return( ntStatus);
            }
        }

        //
        // Additional checks
        //

        if( Fcb->Header.NodeTypeCode == AFS_FILE_FCB)
        {

            //
            // If the caller is asking for write access then try to flush the image section
            //

            if( FlagOn( *pDesiredAccess, FILE_WRITE_DATA) || 
                (ulOptions & FILE_DELETE_ON_CLOSE)) 
            {

                InterlockedIncrement( &Fcb->OpenReferenceCount);

                bDecrementOpenRefCount = TRUE;

                if( !MmFlushImageSection( &Fcb->NPFcb->SectionObjectPointers,
                                          MmFlushForWrite)) 
                {

                    ntStatus = (ulOptions & FILE_DELETE_ON_CLOSE) ? STATUS_CANNOT_DELETE :
                                                                            STATUS_SHARING_VIOLATION;

                    AFSPrint("AFSProcessOpen Failed to purge image section Status %08lX\n", ntStatus);

                    try_return( ntStatus);
                }
            }

            if( ulOptions & FILE_DIRECTORY_FILE)
            {

                try_return( ntStatus = STATUS_OBJECT_NAME_INVALID);
            }
        }
        else if( Fcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB ||
                 Fcb->Header.NodeTypeCode == AFS_ROOT_FCB)
        {

            ASSERT( BooleanFlagOn( Fcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED));

            if( ulOptions & FILE_NON_DIRECTORY_FILE)
            {

                try_return( ntStatus = STATUS_FILE_IS_A_DIRECTORY);
            }
        }
        else
        {

            ASSERT( FALSE);
        }

        //
        // Check with the service that we can open the file
        //

        stOpenCB.ParentId = ParentDcb->DirEntry->DirectoryEntry.FileId;

        stOpenCB.DesiredAccess = *pDesiredAccess;

        stOpenCB.ShareAccess = usShareAccess;

        stOpenResultCB.GrantedAccess = 0;

        ulResultLen = sizeof( AFSFileOpenResultCB);

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_OPEN_FILE,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      0,
                                      &Fcb->DirEntry->DirectoryEntry.FileName,
                                      &Fcb->DirEntry->DirectoryEntry.FileId,
                                      (void *)&stOpenCB,
                                      sizeof( AFSFileOpenCB),
                                      (void *)&stOpenResultCB,
                                      &ulResultLen);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSProcessOpen Failed to open file in service Status %08lX\n", ntStatus);

            try_return( ntStatus);
        }

        //
        // Check if there is a conflict
        //

        if( !AFSCheckAccess( *pDesiredAccess,
                             stOpenResultCB.GrantedAccess))
        {

            AFSPrint("AFSProcessOpen Failed access from service Desired %08lX Granted %08lX\n", 
                                                                    *pDesiredAccess,
                                                                    stOpenResultCB.GrantedAccess);

            try_return( ntStatus = STATUS_ACCESS_DENIED);
        }

        //
        // Initialize the Ccb for the file.
        //

        ntStatus = AFSInitCcb( Fcb,
                               Ccb);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSProcessOpen Failed to initialize Ccb Status %08lX\n", ntStatus);

            try_return( ntStatus);
        }

        bAllocatedCcb = TRUE;

        //
        // Perform the access check on the target if this is a mount point or symlink
        //

        if( Fcb->OpenHandleCount > 0)
        {

            IoUpdateShareAccess( pFileObject, 
                                 &Fcb->ShareAccess);
        }
        else
        {

            //
            // Set the access
            //

            IoSetShareAccess( *pDesiredAccess,
                              usShareAccess,
                              pFileObject,
                              &Fcb->ShareAccess);
        }

        //
        // Increment the open count on this Fcb
        //

        InterlockedIncrement( &Fcb->OpenReferenceCount);

        InterlockedIncrement( &Fcb->OpenHandleCount);

        //
        // Increment the open reference and handle on the parent node
        //

        Fcb->ParentFcb->Specific.Directory.ChildOpenHandleCount++;

        Fcb->ParentFcb->Specific.Directory.ChildOpenReferenceCount++;

        if( ulOptions & FILE_DELETE_ON_CLOSE)
        {

            //
            // Mark it for delete on close
            //

            SetFlag( Fcb->Flags, AFS_FCB_PENDING_DELETE);
        }

        //
        // If they are asking for write access then store away the PID
        //

        if( Fcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_FILE &&
            !AFSCheckForReadOnlyAccess( *pDesiredAccess))
        {

            Fcb->Specific.File.ModifyProcessId = (ULONGLONG)PsGetCurrentProcessId();
        }

        //
        // Return the open result for this file
        //

        Irp->IoStatus.Information = FILE_OPENED;

try_exit:

        if( bDecrementOpenRefCount)
        {

            InterlockedDecrement( &Fcb->OpenReferenceCount);
        }

        if( !NT_SUCCESS( ntStatus))
        {

            if( bAllocatedCcb)
            {

                AFSRemoveCcb( Fcb,
                              *Ccb);
            }

            *Ccb = NULL;
        }
    }

    return ntStatus;
}

NTSTATUS
AFSProcessOverwriteSupersede( IN PIRP             Irp,
                              IN AFSFcb        *ParentDcb,
                              IN AFSFcb        *Fcb,
                              IN AFSCcb       **Ccb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    PFILE_OBJECT pFileObject = NULL;
    LARGE_INTEGER liZero = {0,0};
    BOOLEAN bReleasePaging = FALSE;
    ULONG   ulAttributes = 0;
    LARGE_INTEGER liTime;
    ULONG ulCreateDisposition = 0;
    BOOLEAN bAllocatedCcb = FALSE;
    PACCESS_MASK pDesiredAccess = NULL;
    USHORT usShareAccess;

    __Enter
    {

        pDesiredAccess = &pIrpSp->Parameters.Create.SecurityContext->DesiredAccess;
        usShareAccess = pIrpSp->Parameters.Create.ShareAccess;

        pFileObject = pIrpSp->FileObject;

        ulAttributes = pIrpSp->Parameters.Create.FileAttributes;

        ulCreateDisposition = (pIrpSp->Parameters.Create.Options >> 24) & 0x000000ff;

        if( Fcb->OpenHandleCount > 0)
        {

            ntStatus = IoCheckShareAccess( *pDesiredAccess,
                                           usShareAccess,
                                           pFileObject,
                                           &Fcb->ShareAccess,
                                           FALSE);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSPrint("AFSProcessOverwriteSupersede Access check failure Status %08lX\n", ntStatus);

                try_return( ntStatus);
            }
        }

        //
        //  Before we actually truncate, check to see if the purge
        //  is going to fail.
        //

        if( !MmCanFileBeTruncated( &Fcb->NPFcb->SectionObjectPointers,
                                   &liZero)) 
        {

            try_return( ntStatus = STATUS_USER_MAPPED_FILE);
        }

        //
        // Initialize the Ccb for the file.
        //

        ntStatus = AFSInitCcb( Fcb,
                               Ccb);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSProcessOverwriteSupersede Failed to initialize Ccb Status %08lX\n", ntStatus);

            try_return( ntStatus);
        }

        bAllocatedCcb = TRUE;

        //
        // Need to purge any data currently in the cache
        //

        CcPurgeCacheSection( &Fcb->NPFcb->SectionObjectPointers, 
                             NULL, 
                             0, 
                             FALSE);

        AFSAcquireExcl( Fcb->Header.PagingIoResource,
                        TRUE);

        bReleasePaging = TRUE;

        Fcb->Header.FileSize.LowPart = 0;
        Fcb->Header.ValidDataLength.LowPart = 0;
        Fcb->Header.AllocationSize.LowPart = 0;

        Fcb->DirEntry->DirectoryEntry.EndOfFile.QuadPart = 0;
        Fcb->DirEntry->DirectoryEntry.AllocationSize.QuadPart = 0;

        pFileObject->SectionObjectPointer = &Fcb->NPFcb->SectionObjectPointers;

        pFileObject->FsContext = (void *)Fcb;

        pFileObject->FsContext2 = (void *)*Ccb;

        //
        // Set the update flag accordingly
        //

        SetFlag( Fcb->Flags, AFS_FILE_MODIFIED);

        CcSetFileSizes( pFileObject,
                        (PCC_FILE_SIZES)&Fcb->Header.AllocationSize);
        
        AFSReleaseResource( Fcb->Header.PagingIoResource);

        bReleasePaging = FALSE;
    
        ulAttributes |= FILE_ATTRIBUTE_ARCHIVE;

        if( ulCreateDisposition == FILE_SUPERSEDE) 
        {

            Fcb->DirEntry->DirectoryEntry.FileAttributes = ulAttributes;

        } 
        else 
        {

            Fcb->DirEntry->DirectoryEntry.FileAttributes |= ulAttributes;
        }

        KeQuerySystemTime( &liTime);

        ExSystemTimeToLocalTime( &liTime,
                                 &Fcb->DirEntry->DirectoryEntry.LastWriteTime);

        Fcb->DirEntry->DirectoryEntry.LastAccessTime = Fcb->DirEntry->DirectoryEntry.LastWriteTime;

        //
        // Save off the access for the open
        //

        if( Fcb->OpenHandleCount > 0)
        {

            IoUpdateShareAccess( pFileObject, 
                                 &Fcb->ShareAccess);
        }
        else
        {

            //
            // Set the access
            //

            IoSetShareAccess( *pDesiredAccess,
                              usShareAccess,
                              pFileObject,
                              &Fcb->ShareAccess);
        }

        //
        // Return teh correct action
        //

        if( ulCreateDisposition == FILE_SUPERSEDE) 
        {

            Irp->IoStatus.Information = FILE_SUPERSEDED;
        } 
        else 
        {

            Irp->IoStatus.Information = FILE_OVERWRITTEN;
        }

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( bAllocatedCcb)
            {

                AFSRemoveCcb( Fcb,
                              *Ccb);

                *Ccb = NULL;
            }
        }

        if( bReleasePaging)
        {

            AFSReleaseResource( Fcb->Header.PagingIoResource);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSControlDeviceCreate( IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    __Enter
    {

        //
        // For now, jsut let the open happen
        //

        Irp->IoStatus.Information = FILE_OPENED;
    }

    return ntStatus;
}

NTSTATUS
AFSOpenIOCtlFcb( IN PIRP Irp,
                 IN AFSFcb *ParentDcb,
                 OUT AFSFcb **Fcb,
                 OUT AFSCcb **Ccb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    PFILE_OBJECT pFileObject = NULL;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    BOOLEAN bRemoveFcb = FALSE, bAllocatedCcb = FALSE;
    UNICODE_STRING uniFullFileName;
    AFSPIOCtlOpenCloseRequestCB stPIOCtlOpen;
    AFSFileID stFileID;

    __Enter
    {

        pFileObject = pIrpSp->FileObject;

        //
        // Allocate and initialize the Fcb for the file.
        //

        ntStatus = AFSInitFcb( ParentDcb,
                               NULL,
                               Fcb);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSOpenIOCtlFcb Failed to initialize Fcb Status %08lX\n", ntStatus);

            try_return( ntStatus);
        }

        bRemoveFcb = TRUE;

        (*Fcb)->Header.NodeTypeCode = AFS_IOCTL_FCB;

        //
        // Initialize the Ccb for the file.
        //

        ntStatus = AFSInitCcb( *Fcb,
                               Ccb);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSOpenIOCtlFcb Failed to initialize Ccb Status %08lX\n", ntStatus);

            try_return( ntStatus);
        }

        bAllocatedCcb = TRUE;

        //
        // Set the PIOCtl index
        //

        (*Ccb)->PIOCtlRequestID = InterlockedIncrement( &ParentDcb->Specific.Directory.PIOCtlIndex);

        RtlZeroMemory( &stPIOCtlOpen,
                       sizeof( AFSPIOCtlOpenCloseRequestCB));

        stPIOCtlOpen.RequestId = (*Ccb)->PIOCtlRequestID;

        if( ParentDcb->RootFcb != NULL)
        {
            stPIOCtlOpen.RootId = ParentDcb->RootFcb->DirEntry->DirectoryEntry.FileId;
        }

        RtlZeroMemory( &stFileID,
                       sizeof( AFSFileID));

        //
        // The parent directory FID of the node
        //        

        if( ParentDcb->Header.NodeTypeCode != AFS_ROOT_ALL)
        {
            
            if( ParentDcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY)
            {

                //
                // Just the FID of the node
                //

                stFileID = ParentDcb->DirEntry->DirectoryEntry.FileId;
            }
            else
            {

                ntStatus = AFSRetrieveTargetFID( ParentDcb,
                                                 &stFileID);

                if( !NT_SUCCESS( ntStatus))
                {

                    try_return( ntStatus);
                }
            }
        }

        //
        // Issue the open request to the service
        //

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_PIOCTL_OPEN,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      0,
                                      NULL,
                                      &stFileID,
                                      (void *)&stPIOCtlOpen,
                                      sizeof( AFSPIOCtlOpenCloseRequestCB),
                                      NULL,
                                      NULL);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // Increment the open count on this Fcb
        //

        InterlockedIncrement( &(*Fcb)->OpenReferenceCount);

        InterlockedIncrement( &(*Fcb)->OpenHandleCount);

        //
        // Increment the open reference and handle on the parent node
        //

        (*Fcb)->ParentFcb->Specific.Directory.ChildOpenHandleCount++;

        (*Fcb)->ParentFcb->Specific.Directory.ChildOpenReferenceCount++;

        //
        // Return the open result for this file
        //

        Irp->IoStatus.Information = FILE_OPENED;

try_exit:

        //
        // If we created the Fcb we need to release the resources
        //

        if( bRemoveFcb)
        {

            AFSReleaseResource( &(*Fcb)->NPFcb->Resource);
        }

        if( !NT_SUCCESS( ntStatus))
        {

            if( bAllocatedCcb)
            {

                AFSRemoveCcb( *Fcb,
                              *Ccb);

                *Ccb = NULL;
            }

            if( bRemoveFcb)
            {

                //
                // Need to tear down this Fcb since it is not in the tree for the worker thread
                //

                AFSRemoveFcb( *Fcb);
            }

            *Fcb = NULL;

            *Ccb = NULL;
        }
    }

    return ntStatus;
}