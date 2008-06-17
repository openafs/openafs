//
// File: AFSCommSupport.cpp
//

#include "AFSCommon.h"

VOID
AFSReferenceExtents( IN AFSFcb *Fcb)
{
    AFSNonPagedFcb    *pNPFcb = Fcb->NPFcb;

    //
    // Protect with resource SHARED - no problems with multiple
    // operations doing this but we need to appear to be atomic from
    // the point of view of a dereference
    //
    AFSAcquireShared( &pNPFcb->Specific.File.ExtentsResource, TRUE );

    InterlockedIncrement( &pNPFcb->Specific.File.ExtentsRefCount );
    KeClearEvent( &pNPFcb->Specific.File.ExtentsNotBusy );

    AFSReleaseResource( &pNPFcb->Specific.File.ExtentsResource );
}

VOID
AFSDereferenceExtents( IN AFSFcb *Fcb)
{
    AFSNonPagedFcb *pNPFcb = Fcb->NPFcb;
    LONG            i;

    //
    // Grab resource Shared and decrement. If we were zero then
    // Increment and do it again with the lock Ex (guarantees only one
    // 1->0 transition)
    //

    AFSAcquireShared( &pNPFcb->Specific.File.ExtentsResource, TRUE );

    i = InterlockedDecrement( &pNPFcb->Specific.File.ExtentsRefCount );

    if (0 != i) 
    {
        //
        // All done
        //
        AFSReleaseResource( &pNPFcb->Specific.File.ExtentsResource );
        return;
    }

    InterlockedIncrement( &pNPFcb->Specific.File.ExtentsRefCount );

    AFSReleaseResource( &pNPFcb->Specific.File.ExtentsResource );

    AFSAcquireExcl( &pNPFcb->Specific.File.ExtentsResource, TRUE );

    i = InterlockedDecrement( &pNPFcb->Specific.File.ExtentsRefCount );

    if (0 != i) 
    {
        //
        // All done
        //
        AFSReleaseResource( &pNPFcb->Specific.File.ExtentsResource );
        return;
    }

    KeSetEvent( &pNPFcb->Specific.File.ExtentsNotBusy,
                0,
                FALSE );

    AFSReleaseResource( &pNPFcb->Specific.File.ExtentsResource );
}

//
// Returns with Extents lock EX and noone using them.

VOID 
AFSLockForExtentsTrim( IN AFSFcb *Fcb)
{
    NTSTATUS          ntStatus;
    AFSNonPagedFcb *pNPFcb = Fcb->NPFcb;

    while (TRUE) 
    {

        ntStatus = KeWaitForSingleObject( &pNPFcb->Specific.File.ExtentsNotBusy,
                                          Executive,
                                          KernelMode,
                                          FALSE,
                                          NULL);
        
        
        if (!NT_SUCCESS(ntStatus)) 
        {
            //
            // try again
            //
            continue;
        }

        //
        // Lock resource EX and look again
        //
        AFSAcquireExcl( &pNPFcb->Specific.File.ExtentsResource, TRUE );

        if (KeReadStateEvent( &pNPFcb->Specific.File.ExtentsNotBusy )) {

            return;

        }

        //
        // Try again
        //
        AFSReleaseResource( &pNPFcb->Specific.File.ExtentsResource );
    }
}

NTSTATUS AFSTearDownFcbExtents( IN AFSFcb *Fcb ) 
{
    AFSListEntry        *le;
    AFSExtent           *pEntry;
    ULONG                ulCount = 0;
    size_t               sz;
    AFSReleaseExtentsCB *pRelease = NULL;
    BOOLEAN              locked = FALSE;
    NTSTATUS             ntStatus;
    HANDLE               hModifyProcessId = PsGetCurrentProcessId();

    __Enter
    {
        //
        // Ensure that noone is working with the extents and grab the
        // lock
        //

        AFSLockForExtentsTrim( Fcb );

        locked = TRUE;

        le = (AFSListEntry *) Fcb->Specific.File.ExtentsList.fLink;

        while (le != &Fcb->Specific.File.ExtentsList) 
        {
            ulCount ++;
            le = (AFSListEntry *) le->fLink;
        }

        sz = sizeof( AFSReleaseExtentsCB ) + (ulCount * sizeof ( AFSFileExtentCB ));

        pRelease = (AFSReleaseExtentsCB*) ExAllocatePoolWithTag( NonPagedPool,
                                                                 sz,
                                                                 AFS_EXTENT_RELEASE_TAG);
        if (NULL == pRelease) 
        {
            try_return ( ntStatus = STATUS_INSUFFICIENT_RESOURCES );
        }

        pRelease->Flags = AFS_EXTENT_FLAG_RELEASE;
        pRelease->ExtentCount = ulCount;

        le = (AFSListEntry *) Fcb->Specific.File.ExtentsList.fLink;
        ulCount = 0;
        while (le != &Fcb->Specific.File.ExtentsList) 
        {
            pEntry = CONTAINING_RECORD( le, AFSExtent, List );
        
            pRelease->FileExtents[ulCount].Flags = AFS_EXTENT_FLAG_RELEASE;
            pRelease->FileExtents[ulCount].Length = pEntry->Size;
            pRelease->FileExtents[ulCount].CacheOffset = pEntry->CacheOffset;
            pRelease->FileExtents[ulCount].FileOffset = pEntry->FileOffset;

            ulCount ++;
            le = (AFSListEntry *) le->fLink;

            //
            // And Free the extent
            //
            ExFreePoolWithTag( pEntry, AFS_EXTENT_TAG );
        }

        //
        // Empty list
        //

        Fcb->Specific.File.ExtentsList.fLink = &Fcb->Specific.File.ExtentsList;
        Fcb->Specific.File.ExtentsList.fLink = &Fcb->Specific.File.ExtentsList;

        AFSReleaseResource( &Fcb->NPFcb->Specific.File.ExtentsResource );
        locked = FALSE;

        if( Fcb->Specific.File.ModifyProcessId != 0)
        {

            hModifyProcessId = Fcb->Specific.File.ModifyProcessId;
        }

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_RELEASE_FILE_EXTENTS,
                                      0,
                                      hModifyProcessId,
                                      NULL,
                                      &Fcb->DirEntry->DirectoryEntry.FileId,
                                      pRelease,
                                      sz,
                                      NULL,
                                      NULL);

try_exit:

        if (locked)
        {
            AFSReleaseResource( &Fcb->NPFcb->Specific.File.ExtentsResource );
        }

        if (pRelease) {
            ExFreePoolWithTag( pRelease, AFS_EXTENT_RELEASE_TAG );
        }
    }
    return ntStatus;
}

PAFSExtent
AFSExtentForOffset( IN AFSFcb *Fcb, 
                    IN PLARGE_INTEGER Offset) 
{
    ASSERT( ExIsResourceAcquiredLite( &Fcb->NPFcb->Specific.File.ExtentsResource ));
    //
    // Return the extent that contains the offset

    //
    AFSListEntry *le = (AFSListEntry *) Fcb->Specific.File.ExtentsList.fLink;

    while (le != &Fcb->Specific.File.ExtentsList) 
    {
        AFSExtent *entry;
        
        entry = CONTAINING_RECORD( le, AFSExtent, List );

        if (Offset->QuadPart < entry->FileOffset.QuadPart) 
        {
            //
            // No applicable extent
            //
            return NULL;
        }

        //
        // We start after this extent - carry on round
        //
        if (Offset->QuadPart >= (entry->FileOffset.QuadPart + entry->Size)) 
        {
            
            le = (AFSListEntry *) le->fLink;
            continue;
        }

        return entry;
    }
    return NULL;
}

static BOOLEAN DoExtentsMapRegion(IN AFSFcb *Fcb, 
                                  IN PLARGE_INTEGER Offset, 
                                  IN ULONG Size,
                                  OUT AFSExtent **LastExtent)
{
    //
    // Return TRUE region is completely mapped.  FALSE
    // otherwise.  If the region isn't mapped then the last
    // extent to map part of the region is returned
    //
    AFSExtent *entry;
    AFSExtent *newEntry;
    BOOLEAN retVal = FALSE;

    *LastExtent = NULL;

    AFSAcquireShared( &Fcb->NPFcb->Specific.File.ExtentsResource, TRUE );

    __Enter
    {
        entry = AFSExtentForOffset(Fcb, Offset);

        if (NULL == entry) 
        {
            try_return (retVal = FALSE);
        }

        while (TRUE) 
        {

            ASSERT(Offset->QuadPart >= entry->FileOffset.QuadPart);

            if ((entry->FileOffset.QuadPart + entry->Size) >= 
                (Offset->QuadPart + Size)) 
            {
                //
                // The end is inside the extent
                //
                try_return (retVal = TRUE);
            }
        
            if (entry->List.fLink == &Fcb->Specific.File.ExtentsList) 
            {
                //
                // Run out of extents
                //
                *LastExtent = entry;
                try_return (retVal = FALSE);
            }

            newEntry = CONTAINING_RECORD( entry->List.fLink, AFSExtent, List );

            if (newEntry->FileOffset.QuadPart !=
                (entry->FileOffset.QuadPart + entry->Size)) 
            {
                //
                // Gap
                //
                *LastExtent = entry;
                try_return (retVal = FALSE);
            }

            entry = newEntry;
        }
try_exit:

    AFSReleaseResource( &Fcb->NPFcb->Specific.File.ExtentsResource );    
    }
    return retVal;
}
    
NTSTATUS
AFSRequestExtents( IN AFSFcb *Fcb, 
                   IN PLARGE_INTEGER Offset, 
                   IN ULONG Size,
                   OUT BOOLEAN *FullyMapped)
{
    LARGE_INTEGER        newStart;
    NTSTATUS             ntStatus = STATUS_SUCCESS;
    AFSExtent           *pExtent;
    AFSRequestExtentsCB  request;
    ULONG                newSize;
    AFSNonPagedFcb      *pNPFcb = Fcb->NPFcb;
    //
    // Check our extents, then fire off a request if we need to.
    //
    ASSERT( !ExIsResourceAcquiredLite( &pNPFcb->Specific.File.ExtentsResource ));

    *FullyMapped = DoExtentsMapRegion( Fcb, Offset, Size, &pExtent );

    if (*FullyMapped) 
    {
        return STATUS_SUCCESS;
    } 

    //
    // So we need to queue a request. Since we will be clearing the
    // ExtentsRequestComplete event we need to do with with the lock
    // EX
    //
    
    while (TRUE) 
    {

        ntStatus = KeWaitForSingleObject( &pNPFcb->Specific.File.ExtentsRequestComplete,
                                          Executive,
                                          KernelMode,
                                          FALSE,
                                          NULL);
        if (!NT_SUCCESS(ntStatus)) 
        {
            //
            // try again
            //
            continue;
        }

        //
        // Lock resource EX and look again
        //
        AFSAcquireExcl( &pNPFcb->Specific.File.ExtentsResource, TRUE );

        if (KeReadStateEvent( &pNPFcb->Specific.File.ExtentsRequestComplete )) {

            break;
        }
        AFSReleaseResource( &pNPFcb->Specific.File.ExtentsResource );
    }

    __Enter
    {
        //
        // We have the lock Ex and there is no filling going on.
        // Check again to see whether things have moved since we last
        // checked
        //
        *FullyMapped = DoExtentsMapRegion(Fcb, Offset, Size, &pExtent);

        if (*FullyMapped) {
            
            try_return (ntStatus = STATUS_SUCCESS);

        }

        if (pExtent) 
        {
            newStart = pExtent->FileOffset;
            newStart.QuadPart += pExtent->Size;
        } 
        else 
        {
            newStart = *Offset;
        }

        ASSERT( (Offset->QuadPart + Size) > newStart.QuadPart );

        newSize = (ULONG) (Offset->QuadPart + Size - newStart.QuadPart);

        RtlZeroMemory( &request, sizeof( AFSRequestExtentsCB ));
        request.ByteOffset = newStart;
        request.Length = newSize;

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_REQUEST_FILE_EXTENTS,
                                      0,
                                      0,
                                      NULL,
                                      &Fcb->DirEntry->DirectoryEntry.FileId,
                                      &request,
                                      sizeof( AFSRequestExtentsCB ),
                                      NULL,
                                      NULL);

        if (NT_SUCCESS( ntStatus )) 
        {
            KeClearEvent( &pNPFcb->Specific.File.ExtentsRequestComplete );
        }

try_exit:

    AFSReleaseResource( &pNPFcb->Specific.File.ExtentsResource );    
    }
    return ntStatus;
}

NTSTATUS
AFSProcessExtentsResult( IN AFSFcb *Fcb,
                         IN ULONG   Count,
                         IN AFSFileExtentCB *Result)
{
    NTSTATUS          ntStatus = STATUS_SUCCESS;
    AFSFileExtentCB  *pFileExtents = Result;
    AFSExtent        *pExtent;
    AFSListEntry     *le;
    AFSNonPagedFcb   *pNPFcb = Fcb->NPFcb;
    ULONG             fileExtentsUsed = 0;

    //
    // Grab the extents exclusive for the duration
    //
    AFSAcquireExcl( &pNPFcb->Specific.File.ExtentsResource, TRUE );

    __Enter 
    {
        //
        // Find where to put it
        //
        le = &Fcb->Specific.File.ExtentsList;

        if (le->fLink != &Fcb->Specific.File.ExtentsList) 
        {

            pExtent = CONTAINING_RECORD( le->fLink, AFSExtent, List );

        } 
        else 
        {

            pExtent = NULL;
        }

        while (fileExtentsUsed < Count) 
        {

            //
            // Loop invariant - le points to where to insert after and
            // pExtent points to le->fLink
            //
            
            ASSERT (NULL == pExtent ||
                    le->fLink == &pExtent->List);
 
            if (NULL == pExtent ||
                pExtent->FileOffset.QuadPart > pFileExtents->FileOffset.QuadPart)
            {
                //
                // We need to insert a new extent at le.  Start with
                // some sanity check on spanning
                //
                if (NULL != pExtent &&
                    ((pFileExtents->FileOffset.QuadPart + pFileExtents->Length) >
                     pExtent->FileOffset.QuadPart))
                {
                    //
                    // File Extents overlaps pExtent
                    //
                    ASSERT( (pFileExtents->FileOffset.QuadPart + pFileExtents->Length) <=
                            pExtent->FileOffset.QuadPart);
            
                    try_return (ntStatus = STATUS_INVALID_PARAMETER);
                }

                //
                // File offset is entirely in front of this extent.  Create
                // a new one (remember le is the previous list entry)
                //
                pExtent = (AFSExtent *) ExAllocatePoolWithTag( PagedPool,
                                                               sizeof( AFSExtent ),
                                                               AFS_EXTENT_TAG );
                if (NULL  == pExtent) 
                {
                    try_return (ntStatus = STATUS_INSUFFICIENT_RESOURCES );
                }
    
                RtlZeroMemory( pExtent, sizeof( AFSExtent ));

                pExtent->FileOffset = pFileExtents->FileOffset;
                pExtent->CacheOffset = pFileExtents->CacheOffset;
                pExtent->Size = pFileExtents->Length;

                //
                // Insert into list
                //

                pExtent->List.fLink = le->fLink;
                pExtent->List.bLink = le;
                le->fLink = &pExtent->List;
                ((AFSListEntry*)pExtent->List.fLink)->bLink = &pExtent->List;

                //
                // Do not move the cursor - we will do it next time
                //
            } 
            else if (pExtent->FileOffset.QuadPart == pFileExtents->FileOffset.QuadPart)
            {

                if (pExtent->Size != pFileExtents->Length) 
                {

                    ASSERT (pExtent->Size == pFileExtents->Length);
                
                    try_return (ntStatus = STATUS_INVALID_PARAMETER);
                }
            
                //
                // Move both cursors forward
                //
                fileExtentsUsed++;
                le = &pExtent->List;

                //
                // setup pExtent if there is one
                //
                if (le->fLink != &Fcb->Specific.File.ExtentsList)
                {
                    pExtent = CONTAINING_RECORD( le->fLink, AFSExtent, List );
                } 
                else 
                {
                    pExtent = NULL;
                }
            } 
            else 
            {

                ASSERT( pExtent->FileOffset.QuadPart < pFileExtents->FileOffset.QuadPart );

                //
                // Sanity check on spanning
                //
                if ((pExtent->FileOffset.QuadPart + pExtent->Size) >
                    pFileExtents->FileOffset.QuadPart) 
                {

                    ASSERT( (pExtent->FileOffset.QuadPart + pExtent->Size) <=
                            pFileExtents->FileOffset.QuadPart);
            
                    try_return (ntStatus = STATUS_INVALID_PARAMETER);
                }
            
                //
                // Move le and pExtent forward
                //
                le = &pExtent->List;
                if (le->fLink != &Fcb->Specific.File.ExtentsList) 
                {
                    pExtent = CONTAINING_RECORD( le->fLink, AFSExtent, List );
                } 
                else 
                {
                    pExtent = NULL;
                }
            }
        }
        //
        // All done, signal that we are done drop the lock, exit
        //
try_exit:
        KeSetEvent( &pNPFcb->Specific.File.ExtentsRequestComplete,
                    0,
                    FALSE);

        AFSReleaseResource( &pNPFcb->Specific.File.ExtentsResource );
    }
    return ntStatus;
}

NTSTATUS
AFSProcessSetFileExtents( IN AFSSetFileExtentsCB *SetExtents )
{
    AFSFcb       *pFcb = NULL;
    NTSTATUS      ntStatus;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;

        

    IN AFSTreeHdr *pFileIdTree;

    __Enter 
    {
        AFSAcquireShared( &pDevExt->Specific.RDR.FileIDTreeLock, TRUE);

        ntStatus = AFSLocateFileIDEntry( pDevExt->Specific.RDR.FileIDTree.TreeHead,
                                         SetExtents->FileID.Hash,
                                         &pFcb);

        AFSReleaseResource( &pDevExt->Specific.RDR.FileIDTreeLock );

        if (!NT_SUCCESS(ntStatus)) 
        {
            try_return( ntStatus );
        }

        if (NULL == pFcb)
        {
            try_return( ntStatus = STATUS_UNSUCCESSFUL );
        }

        ntStatus = AFSProcessExtentsResult ( pFcb, 
                                             SetExtents->ExtentCount, 
                                             SetExtents->FileExtents );
    try_exit:
        ;
    }

    return ntStatus;
}
    
NTSTATUS
AFSWaitForExtentMapping( AFSFcb *Fcb )
{
    NTSTATUS ntStatus;

    ntStatus = KeWaitForSingleObject( &Fcb->NPFcb->Specific.File.ExtentsRequestComplete,
                                      Executive,
                                      KernelMode,
                                      FALSE,
                                      NULL);
    return ntStatus;
}

NTSTATUS
AFSFlushExtents( IN AFSFcb *Fcb)
{

    AFSNonPagedFcb      *pNPFcb = Fcb->NPFcb;
    AFSExtent           *pExtent;
    AFSListEntry        *le;
    AFSReleaseExtentsCB *pRelease = NULL;
    ULONG                count = 0;
    ULONG                total = 0;
    ULONG                sz = 0;
    NTSTATUS             ntStatus;
    HANDLE               hModifyProcessId = PsGetCurrentProcessId();
    
    __Enter
    {
        //
        // Lock extents while we count and set up the array to send to
        // the service
        //
        
        AFSAcquireShared( &pNPFcb->Specific.File.ExtentsResource, TRUE);

        le = (AFSListEntry *) Fcb->Specific.File.ExtentsList.fLink;

        while (le != &Fcb->Specific.File.ExtentsList) 
        {
            pExtent = CONTAINING_RECORD( le, AFSExtent, List );

            if (BooleanFlagOn(pExtent->Flags, AFS_EXTENT_DIRTY))
            {
                count ++;
            }
            le = (AFSListEntry *) le->fLink;
        }
        total = count;
            
        sz = sizeof( AFSReleaseExtentsCB ) + (total * sizeof ( AFSFileExtentCB ));

        pRelease = (AFSReleaseExtentsCB*) ExAllocatePoolWithTag( NonPagedPool,
                                                                 sz,
                                                                 AFS_EXTENT_RELEASE_TAG);
        if (NULL == pRelease) 
        {
            try_return ( ntStatus = STATUS_INSUFFICIENT_RESOURCES );
        }

        pRelease->Flags = AFS_EXTENT_FLAG_DIRTY;
        pRelease->ExtentCount = count;

        le = (AFSListEntry *) Fcb->Specific.File.ExtentsList.fLink;
        count = 0;

        while (le != &Fcb->Specific.File.ExtentsList) 
        {
            pExtent = CONTAINING_RECORD( le, AFSExtent, List );

            if (BooleanFlagOn(pExtent->Flags, AFS_EXTENT_DIRTY))
            {
        
                pRelease->FileExtents[count].Flags = AFS_EXTENT_FLAG_DIRTY;
                pRelease->FileExtents[count].Length = pExtent->Size;
                pRelease->FileExtents[count].CacheOffset = pExtent->CacheOffset;
                pRelease->FileExtents[count].FileOffset = pExtent->FileOffset;

                count ++;
                ASSERT( count <= total );

                //
                // Clear the flag in advance of the write - we'll put it baxck if it failed
                //
                pExtent->Flags &= ~AFS_EXTENT_DIRTY;
            }
            le = (AFSListEntry *) le->fLink;
        }

        if( Fcb->Specific.File.ModifyProcessId != 0)
        {

            hModifyProcessId = Fcb->Specific.File.ModifyProcessId;
        }

        //
        // Fire off the request syncrhonously
        //

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_RELEASE_FILE_EXTENTS,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      hModifyProcessId,
                                      NULL,
                                      &Fcb->DirEntry->DirectoryEntry.FileId,
                                      pRelease,
                                      sz,
                                      NULL,
                                      NULL);

    try_exit:

        if (!NT_SUCCESS(ntStatus) && NULL != pRelease) {
            //
            // Go back and set the extents dirty again
            //
            le = (AFSListEntry *) Fcb->Specific.File.ExtentsList.fLink;
            count = 0;

            while (le != &Fcb->Specific.File.ExtentsList && count <= total) 
            {
                pExtent = CONTAINING_RECORD( le, AFSExtent, List );

                if (pRelease->FileExtents[count].CacheOffset.QuadPart == pExtent->CacheOffset.QuadPart) {
                    
                    pExtent->Flags |= AFS_EXTENT_DIRTY;
                    count ++;
                }
                le = (AFSListEntry *) le->fLink;
            }
        }

        AFSReleaseResource( &pNPFcb->Specific.File.ExtentsResource );

        if (pRelease) {
            ExFreePoolWithTag( pRelease, AFS_EXTENT_RELEASE_TAG );
        }
    }
    return ntStatus;
}

VOID
AFSMarkDirty( IN AFSFcb *Fcb,
              IN PLARGE_INTEGER Offset,
              IN ULONG   Count)
{
    LARGE_INTEGER        liEnd;
    AFSExtent           *pExtent;
    AFSListEntry        *le;

    liEnd.QuadPart = Offset->QuadPart + Count;

    AFSAcquireShared( &Fcb->NPFcb->Specific.File.ExtentsResource, TRUE);

    le = (AFSListEntry *) Fcb->Specific.File.ExtentsList.fLink;

    while (le != &Fcb->Specific.File.ExtentsList) 
    {
        pExtent = CONTAINING_RECORD( le, AFSExtent, List );
        
        if (pExtent->FileOffset.QuadPart >= liEnd.QuadPart) 
        {
            //
            // All done !
            //
            break;
        }
             
        if (Offset->QuadPart < (pExtent->FileOffset.QuadPart + pExtent->Size))
        {
            pExtent->Flags |= AFS_EXTENT_DIRTY;
        }

        le = (AFSListEntry *) le->fLink;
    }

    AFSReleaseResource( &Fcb->NPFcb->Specific.File.ExtentsResource );
    
}
