//
// File: AFSCommSupport.cpp
//
#include "AFSCommon.h"

static AFSExtent *ExtentFor( PLIST_ENTRY le, ULONG SkipList );
static AFSExtent *NextExtent( AFSExtent *Extent, ULONG SkipList );
static ULONG ExtentsMasks[AFS_NUM_EXTENT_LISTS] = AFS_EXTENTS_MASKS;
static VOID VerifyExtentsLists(AFSFcb *Fcb);

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
//

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
    LIST_ENTRY          *le;
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

        VerifyExtentsLists(Fcb);

        le = Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST].Flink;

        while (le != &Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST]) 
        {
            ulCount ++;
            le = le->Flink;
        }

        if (0 == ulCount)
        {
            try_return ( ntStatus = STATUS_SUCCESS);
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

        le = Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST].Flink;
        ulCount = 0;
        while (le != &Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST]) 
        {
            pEntry = ExtentFor( le, AFS_EXTENTS_LIST );
        
            pRelease->FileExtents[ulCount].Flags = AFS_EXTENT_FLAG_RELEASE;
            pRelease->FileExtents[ulCount].Length = pEntry->Size;
            pRelease->FileExtents[ulCount].CacheOffset = pEntry->CacheOffset;
            pRelease->FileExtents[ulCount].FileOffset = pEntry->FileOffset;

            ulCount ++;
            le = le->Flink;
        }

        AFSReleaseResource( &Fcb->NPFcb->Specific.File.ExtentsResource );
        locked = FALSE;

        if( Fcb->Specific.File.ModifyProcessId != 0)
        {

            hModifyProcessId = Fcb->Specific.File.ModifyProcessId;
        }

        //
        // Send the request down.  We could send it asynchronously
        // (there is nothing we can do if this fails since we are past
        // caring), but in case we are called in a path which can fail
        // sometime in the future we'll be careful.  This is usually
        // tear down and down in a worker thread so we are not
        // interrupting much latency after all.
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
        if (NT_SUCCESS(ntStatus)) {
            le = Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST].Flink;
            while (le != &Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST]) 
            {
                pEntry = ExtentFor( le, AFS_EXTENTS_LIST );

                le = le->Flink;
                //
                // And Free the extent
                //
                ExFreePoolWithTag( pEntry, AFS_EXTENT_TAG );
            }
            //
            // Reinitialize the skip lists
            //
            for (ULONG i = 0; i < AFS_NUM_EXTENT_LISTS; i++) 
            {
                InitializeListHead(&Fcb->Specific.File.ExtentsLists[i]);
            }
        }

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

static PAFSExtent
ExtentForOffsetInList( IN AFSFcb *Fcb, 
                       IN LIST_ENTRY *List,
                       IN ULONG ListNumber,
                       IN PLARGE_INTEGER Offset)
{
    //
    // Return the extent that maps the offset, that 
    //   - Contains the offset
    //   - or is immediately ahead of the offset (in this list) 
    //   - otherwise return NULL.
    //

    PLIST_ENTRY  pLe = List;
    AFSExtent   *pPrevious = NULL;

    ASSERT( ExIsResourceAcquiredLite( &Fcb->NPFcb->Specific.File.ExtentsResource ));

    while (pLe != &Fcb->Specific.File.ExtentsLists[ListNumber])
    {
        AFSExtent *entry;
        
        entry = ExtentFor( pLe, ListNumber );

        if (Offset->QuadPart < entry->FileOffset.QuadPart) 
        {
            //
            // Offset is ahead of entry.  Return previous
            //
            return pPrevious;
        }

        if (Offset->QuadPart >= (entry->FileOffset.QuadPart + entry->Size)) 
        {
            //
            // We start after this extent - carry on round
            //
            pPrevious = entry;
            pLe = pLe->Flink;
            continue;
        }

        //
        // Otherwise its a match
        //

        return entry;
    }

    //
    // Got to the end.  Return Previous
    //
    return pPrevious;
}

BOOLEAN 
AFSExtentContains( IN AFSExtent *Extent, IN PLARGE_INTEGER Offset) 
{
    if (NULL == Extent) 
    {
        return FALSE;
    }
    return (Extent->FileOffset.QuadPart <= Offset->QuadPart &&
            (Extent->FileOffset.QuadPart + Extent->Size) > Offset->QuadPart);
}


//
// Return the extent that contains the offset
//
PAFSExtent
AFSExtentForOffsetHint( IN AFSFcb *Fcb, 
                        IN PLARGE_INTEGER Offset,
                        IN BOOLEAN ReturnPrevious,
                        IN AFSExtent *Hint) 
{
    AFSExtent *pPrevious = Hint;
    LIST_ENTRY *pLe;
    LONG i;
    
    ASSERT( ExIsResourceAcquiredLite( &Fcb->NPFcb->Specific.File.ExtentsResource ));

    VerifyExtentsLists(Fcb);

    //
    // So we will go across the skip lists until we find an
    // appropriate entry (previous or direct match).  If it's a match
    // we are done, other wise we start on the next layer down
    //
    for (i = AFS_NUM_EXTENT_LISTS-1; i >= AFS_EXTENTS_LIST; i--)
    {
        if (NULL == pPrevious) 
        {
            //
            // We haven't found anything in the previous layers
            //
            pLe = Fcb->Specific.File.ExtentsLists[i].Flink;
        }
        else if (NULL == pPrevious->Lists[i].Flink) 
        {
            ASSERT(AFS_EXTENTS_LIST != i);
            //
            // The hint doesn't exist at this level, next one down
            //
            continue;
        }
        else
        {
            //
            // take the previous into the next
            //
            pLe = &pPrevious->Lists[i];
        }

        pPrevious = ExtentForOffsetInList( Fcb, pLe, i, Offset);

        if (NULL != pPrevious && AFSExtentContains(pPrevious, Offset)) 
        {
            //
            // Found it immediately.  Stop here
            //
            return pPrevious;
        }
    }

    if (NULL == pPrevious || ReturnPrevious )
    {       
        return pPrevious;
    }

    ASSERT( !AFSExtentContains(pPrevious, Offset) );

    return NULL;
}

PAFSExtent
AFSExtentForOffset( IN AFSFcb *Fcb, 
                    IN PLARGE_INTEGER Offset,
                    IN BOOLEAN ReturnPrevious)
{
    return AFSExtentForOffsetHint(Fcb, Offset, ReturnPrevious, NULL);
}


static BOOLEAN DoExtentsMapRegion(IN AFSFcb *Fcb, 
                                  IN PLARGE_INTEGER Offset, 
                                  IN ULONG Size,
                                  IN OUT AFSExtent **FirstExtent,
                                  OUT AFSExtent **LastExtent)
{
    //
    // Return TRUE region is completely mapped.  FALSE
    // otherwise.  If the region isn't mapped then the last
    // extent to map part of the region is returned.
    //
    // *LastExtent as input is where to start looking.
    // *LastExtent as output is either the extent which
    //  contains the Offset, or the last one which doesn't
    //
    AFSExtent *entry;
    AFSExtent *newEntry;
    BOOLEAN retVal = FALSE;
    
    AFSAcquireShared( &Fcb->NPFcb->Specific.File.ExtentsResource, TRUE );

    __Enter
    {
        entry = AFSExtentForOffsetHint(Fcb, Offset, TRUE, *FirstExtent);
        *FirstExtent = entry;

        if (NULL == entry || !AFSExtentContains(entry, Offset)) 
        {
            try_return (retVal = FALSE);
        }
        
        ASSERT(Offset->QuadPart >= entry->FileOffset.QuadPart);
            
        while (TRUE) 
        {
            if ((entry->FileOffset.QuadPart + entry->Size) >= 
                (Offset->QuadPart + Size)) 
            {
                //
                // The end is inside the extent
                //
                try_return (retVal = TRUE);
            }
        
            if (entry->Lists[AFS_EXTENTS_LIST].Flink == &Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST]) 
            {
                //
                // Run out of extents
                //
                try_return (retVal = FALSE);
            }

            newEntry = NextExtent( entry, AFS_EXTENTS_LIST );

            if (newEntry->FileOffset.QuadPart !=
                (entry->FileOffset.QuadPart + entry->Size)) 
            {
                //
                // Gap
                //
                try_return (retVal = FALSE);
            }

            entry = newEntry;
        }
try_exit:

    AFSReleaseResource( &Fcb->NPFcb->Specific.File.ExtentsResource );    
    }
    *LastExtent = entry;
    return retVal;
}
    
//
// Given an FCB and an Offset we look to see whether there extents to
// Map them all.  If there are then we return TRUE to fullymapped
// and *FirstExtent points to the first extent to map the extent.
// If not then we return FALSE, but we request the extents to be mapped.
// Further *FirstExtent (if non null) is the last extent which doesn't
// map the extent.
//
// Finally on the way *in* if *FirstExtent is non null it is where we start looking
//
NTSTATUS
AFSRequestExtents( IN AFSFcb *Fcb, 
                   IN PLARGE_INTEGER Offset, 
                   IN ULONG Size,
                   OUT BOOLEAN *FullyMapped,
                   OUT AFSExtent **FirstExtent)
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

    //
    // We start off knowing nothing about where we will go.
    //
    *FirstExtent = NULL;

    *FullyMapped = DoExtentsMapRegion( Fcb, Offset, Size, FirstExtent, &pExtent );

    if (*FullyMapped) 
    {
        ASSERT(AFSExtentContains(*FirstExtent, Offset));
        LARGE_INTEGER end = *Offset;
        end.QuadPart += (Size-1);
        ASSERT(AFSExtentContains(pExtent, &end));

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
        // checked (note that FirstExtent may have been set up by
        // previous request and so things maybe a bunch faster
        //
        *FullyMapped = DoExtentsMapRegion(Fcb, Offset, Size, FirstExtent, &pExtent);

        if (*FullyMapped) {
            ASSERT(AFSExtentContains(*FirstExtent, Offset));
            LARGE_INTEGER end = *Offset;
            end.QuadPart += (Size-1);
            ASSERT(AFSExtentContains(pExtent, &end));
            
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
    LIST_ENTRY       *le;
    AFSNonPagedFcb   *pNPFcb = Fcb->NPFcb;
    ULONG             fileExtentsUsed = 0;
    BOOLEAN           bFoundExtent = FALSE;
    LIST_ENTRY       *pSkipEntries[AFS_NUM_EXTENT_LISTS] = { 0 };

    //
    // Grab the extents exclusive for the duration
    //
    AFSAcquireExcl( &pNPFcb->Specific.File.ExtentsResource, TRUE );

    __Enter 
    {
        //
        // Find where to put the extents
        //
        for (ULONG i = AFS_EXTENTS_LIST; i < AFS_NUM_EXTENT_LISTS; i++)
        {
            pSkipEntries[i] = Fcb->Specific.File.ExtentsLists[i].Flink;
        }

        le = pSkipEntries[AFS_EXTENTS_LIST];

        if (le == &Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST]) 
        {
            //
            // No extents.  Insert at head of list (which is where the skip lists point!)
            //
            pExtent = NULL;
        } 
        else if (0 != pFileExtents->FileOffset.QuadPart) 
        {
            //
            // We want to find the best extents immediately *behind* this offset
            //
            LARGE_INTEGER offset = pFileExtents->FileOffset;

            //
            // Ask in the top skip list first, then work down
            //
            for (LONG i = AFS_NUM_EXTENT_LISTS-1; i >= AFS_EXTENTS_LIST; i--)
            {
                pExtent = ExtentForOffsetInList( Fcb,
                                                 pSkipEntries[i],
                                                 i,
                                                 &offset);
                
                if (NULL == pExtent) 
                {
                    //
                    // No dice.  Header has to become the head of the list
                    //
                    pSkipEntries[i] = &Fcb->Specific.File.ExtentsLists[i];
                    //
                    // And as  a loop invariant we should never have found an extent
                    //
                    ASSERT(!bFoundExtent);
                }
                else
                {
                    //
                    // pExtent is where to start to insert at this level
                    //
                    pSkipEntries[i] = &pExtent->Lists[i];
                        
                    //
                    // And also where to start to look at the next level
                    //
                        
                    if (i > AFS_EXTENTS_LIST)
                    {
                        pSkipEntries[i-1] = &pExtent->Lists[i-1];
                    }
                    bFoundExtent = TRUE;
                }
            }
                
            if (NULL == pExtent) 
            {
                pExtent = ExtentFor( le, AFS_EXTENTS_LIST);
            } 
            else
            {
                le = pExtent->Lists[AFS_EXTENTS_LIST].Blink;
            }
        }
        else 
        {
            // 
            // Looking at offset 0, so we must start at the beginning
            //
            pExtent = ExtentFor(le, AFS_EXTENTS_LIST);
            le = le->Blink;

            //
            // And set up the skip lists
            //

            for (ULONG i = AFS_EXTENTS_LIST; i < AFS_NUM_EXTENT_LISTS; i++)
            {
                pSkipEntries[i] = &Fcb->Specific.File.ExtentsLists[i];
            }
        }

        while (fileExtentsUsed < Count) 
        {

            //
            // Loop invariant - le points to where to insert after and
            // pExtent points to le->fLink
            //
            
            ASSERT (NULL == pExtent ||
                    le->Flink == &pExtent->Lists[AFS_EXTENTS_LIST]);
 
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
                InsertHeadList(le, &pExtent->Lists[AFS_EXTENTS_LIST]);
                ASSERT(le->Flink == &pExtent->Lists[AFS_EXTENTS_LIST]);
                ASSERT(0 == (pExtent->FileOffset.LowPart & ExtentsMasks[AFS_EXTENTS_LIST]));

                //
                // Do not move the cursor - we will do it next time
                //

                //
                // And into the (upper) skip lists - we will move the cursors
                //
                for (ULONG i = AFS_NUM_EXTENT_LISTS-1; i > AFS_EXTENTS_LIST; i--)
                {
                    if (0 == (pExtent->FileOffset.LowPart & ExtentsMasks[i]))
                    {
                        InsertHeadList(pSkipEntries[i], &pExtent->Lists[i]);
                        pSkipEntries[i] = pSkipEntries[i]->Flink;
                    }
                }

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
                le = &pExtent->Lists[AFS_EXTENTS_LIST];

                //
                // setup pExtent if there is one
                //
                if (le->Flink != &Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST])
                {
                    pExtent = NextExtent( pExtent, AFS_EXTENTS_LIST ) ;
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
                le = &pExtent->Lists[AFS_EXTENTS_LIST];
                if (le->Flink != &Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST])
                {
                    pExtent = NextExtent( pExtent, AFS_EXTENTS_LIST ) ;
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
        VerifyExtentsLists(Fcb);

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
AFSProcessReleaseFileExtents( IN AFSReleaseFileExtentsCB *Extents )
{
    AFSFcb              *pFcb = NULL;
    AFSDeviceExt        *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    NTSTATUS             ntStatus;

    __Enter 
    {
    
        AFSAcquireShared( &pDevExt->Specific.RDR.FileIDTreeLock, TRUE);
    
        ntStatus = AFSLocateFileIDEntry( pDevExt->Specific.RDR.FileIDTree.TreeHead,
                                         Extents->FileID.Hash,
                                         &pFcb);

        AFSReleaseResource( &pDevExt->Specific.RDR.FileIDTreeLock );

        if (!NT_SUCCESS(ntStatus)) 
        {
            try_return( ntStatus );
        }
        
        ntStatus = AFSProcessReleaseFileExtentsByFcb( Extents, pFcb);

    try_exit:
        ;
    }

    return ntStatus;
}

//
// Helper fuction for Usermode initiate release of extents
//
NTSTATUS
AFSProcessReleaseFileExtentsByFcb( IN AFSReleaseFileExtentsCB *Extents,
                                   IN AFSFcb *Fcb)
{
    ULONG                ulSz;
    AFSExtent           *pExtent;
    LIST_ENTRY          *le;
    LIST_ENTRY          *leNext;
    AFSReleaseExtentsCB *pReleaseExtents = NULL;
    BOOLEAN              locked = FALSE;
    ULONG                ulExtentCount = 0;
    NTSTATUS             ntStatus;

    __Enter 
    {
        ulSz = sizeof (AFSReleaseExtentsCB) + Extents->ExtentCount * sizeof (AFSFileExtentCB);

        pReleaseExtents = (AFSReleaseExtentsCB*) ExAllocatePoolWithTag( PagedPool,
                                                                        ulSz,
                                                                        AFS_EXTENT_RELEASE_TAG);
        if (NULL == pReleaseExtents)
        {
            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES );
        }

        RtlZeroMemory( pReleaseExtents, ulSz);

        //
        // Flush out anything which might be dirty
        //

        ntStatus = AFSFlushExtents( Fcb );

        if (!NT_SUCCESS(ntStatus)) 
        {
            try_return( ntStatus );
        }

        //
        // Make sure noone is using the extents and return with the
        // lock held
        //

        AFSLockForExtentsTrim( Fcb );

        locked = TRUE;

        //
        // iterate until we have dealt with all we were asked for or
        // are at the end of the list.  Note that this deals (albeit
        // badly) with out of order extents
        //
        pExtent = AFSExtentForOffset( Fcb,
                                      &Extents->FileExtents[0].FileOffset,
                                      FALSE );

        if (NULL == pExtent) 
        {
            le = Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST].Flink;
        } 
        else
        {
            le = &pExtent->Lists[AFS_EXTENTS_LIST];
        }
        ulExtentCount = 0;

        while (le != &Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST] && 
               ulExtentCount < Extents->ExtentCount) 
        {
            pExtent = ExtentFor( le, AFS_EXTENTS_LIST);
            
            if (pExtent->FileOffset.QuadPart < Extents->FileExtents[ulExtentCount].FileOffset.QuadPart) 
            {
                //
                // Skip forward through the extent list until we get
                // to the one we want
                //
                le = le->Flink;
            } 
            else if (pExtent->FileOffset.QuadPart > Extents->FileExtents[ulExtentCount].FileOffset.QuadPart) 
            {
                //
                // We don't have the extent asked for (or out of order)
                //
                ulExtentCount ++;
            } 
            else 
            {
                //
                // This is one to purge
                //
                pReleaseExtents->FileExtents[pReleaseExtents->ExtentCount] =
                    Extents->FileExtents[ulExtentCount];
                pReleaseExtents->FileExtents[pReleaseExtents->ExtentCount].Flags = 0;
                //
                // move forward all three cursors
                //
                le = le->Flink;
                ulExtentCount ++;
                pReleaseExtents->ExtentCount ++;
            }
        }

        //
        // Now try to give back the extents
        //
        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_RELEASE_FILE_EXTENTS,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      PsGetCurrentProcessId(),
                                      NULL,
                                      &Fcb->DirEntry->DirectoryEntry.FileId,
                                      pReleaseExtents,
                                      ulSz,
                                      NULL,
                                      NULL);

        if ( !NT_SUCCESS( ntStatus))
        {
            try_return( ntStatus );
        }

        //
        // Now loop again getting rid of the extents
        //
        pExtent = AFSExtentForOffset( Fcb,
                                      &Extents->FileExtents[0].FileOffset,
                                      FALSE );

        if (NULL == pExtent) 
        {
            le = Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST].Flink;
        } 
        else
        {
            le = &pExtent->Lists[AFS_EXTENTS_LIST];
        }
        ulExtentCount = 0;

        while (le != &Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST] && 
               ulExtentCount < Extents->ExtentCount) 
        {
            pExtent = ExtentFor( le, AFS_EXTENTS_LIST );
            
            if (pExtent->FileOffset.QuadPart < Extents->FileExtents[ulExtentCount].FileOffset.QuadPart) 
            {
                //
                // Skip forward through the extent list until we get
                // to the one we want
                //
                le = le->Flink;
            } 
            else if (pExtent->FileOffset.QuadPart > Extents->FileExtents[ulExtentCount].FileOffset.QuadPart) 
            {
                //
                // We don't have the extent asked for (or out of order)
                //
                ulExtentCount ++;
            } 
            else 
            {
                //
                // This is one to delete, move forward cursors
                //
                le = le->Flink;
                ulExtentCount ++;

                //
                // And unpick
                //
                for (ULONG i = 0; i < AFS_NUM_EXTENT_LISTS; i ++) 
                {
                    if (NULL != pExtent->Lists[i].Flink && !IsListEmpty(&pExtent->Lists[i]))
                    {
                        RemoveEntryList( &pExtent->Lists[i] );
                    }
                }

                //
                // and free
                //
                ExFreePoolWithTag( pExtent, AFS_EXTENT_TAG );
            }
        }

    try_exit:
        if (locked) 
        {
            AFSReleaseResource( &Fcb->NPFcb->Specific.File.ExtentsResource );
        }

        if (pReleaseExtents)
        {
            ExFreePool( pReleaseExtents );
        }
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
    LIST_ENTRY          *le;
    AFSReleaseExtentsCB *pRelease = NULL;
    ULONG                count = 0;
    ULONG                total = 0;
    ULONG                sz = 0;
    NTSTATUS             ntStatus;
    HANDLE               hModifyProcessId = PsGetCurrentProcessId();
    LARGE_INTEGER        liLastFlush;

    ASSERT( Fcb->Header.NodeTypeCode == AFS_FILE_FCB);

    //
    // Check, then clear the fcb wide flag
    //
    if (!Fcb->Specific.File.ExtentsDirty) 
    {
        return STATUS_SUCCESS;
    }

    Fcb->Specific.File.ExtentsDirty = FALSE;
    
    //
    // Save, then reset the flush time
    //
    liLastFlush = Fcb->Specific.File.LastServerFlush; 
    KeQuerySystemTime( &Fcb->Specific.File.LastServerFlush);

    __Enter
    {
        //
        // Lock extents while we count and set up the array to send to
        // the service
        //
        
        AFSAcquireShared( &pNPFcb->Specific.File.ExtentsResource, TRUE);

        le = Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST].Flink;

        while (le != &Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST])
        {
            pExtent = ExtentFor( le, AFS_EXTENTS_LIST );

            if (BooleanFlagOn(pExtent->Flags, AFS_EXTENT_DIRTY))
            {
                count ++;
            }
            le = le->Flink;
        }

        if (0 == count) 
        {
            //
            // Nothing to do
            //
            try_return( ntStatus = STATUS_SUCCESS);
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

        le = Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST].Flink;
        count = 0;

        while (le != &Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST])
        {
            pExtent = ExtentFor( le, AFS_EXTENTS_LIST );

            if (BooleanFlagOn(pExtent->Flags, AFS_EXTENT_DIRTY))
            {
        
                pRelease->FileExtents[count].Flags = AFS_EXTENT_FLAG_DIRTY;
                pRelease->FileExtents[count].Length = pExtent->Size;
                pRelease->FileExtents[count].CacheOffset = pExtent->CacheOffset;
                pRelease->FileExtents[count].FileOffset = pExtent->FileOffset;

                count ++;
                ASSERT( count <= total );

                //
                // Clear the flag in advance of the write - we'll put
                // it back if it failed.  If we do things this was we
                // know that the clear is pessimistic (any write which
                // happens from now on will set the flag dirty again).
                //
                pExtent->Flags &= ~AFS_EXTENT_DIRTY;
            }
            le = le->Flink;
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

        if (!NT_SUCCESS(ntStatus)) 
        {
            //
            // that failed, so undo all we did.
            //
            Fcb->Specific.File.ExtentsDirty = TRUE;
            Fcb->Specific.File.LastServerFlush = liLastFlush;

            if (NULL != pRelease) 
            {
                //
                // Go back and set the extents dirty again
                //
                le = Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST].Flink;
                count = 0;

                while (le != &Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST] && count <= total) 
                {
                    pExtent = ExtentFor( le, AFS_EXTENTS_LIST );

                    if (pRelease->FileExtents[count].CacheOffset.QuadPart == pExtent->CacheOffset.QuadPart) 
                    {
                        pExtent->Flags |= AFS_EXTENT_DIRTY;
                        count ++;
                    }
                    le = le->Flink;
                }
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
    LIST_ENTRY          *le;

    liEnd.QuadPart = Offset->QuadPart + Count;

    Fcb->Specific.File.ExtentsDirty = TRUE;

    AFSAcquireShared( &Fcb->NPFcb->Specific.File.ExtentsResource, TRUE);

    pExtent = AFSExtentForOffset( Fcb, Offset, TRUE);

    if (NULL == pExtent)
    {
        le = Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST].Flink;
    }
    else 
    {
        le = &pExtent->Lists[AFS_EXTENTS_LIST];
    }

    while (le != &Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST])
    {
        pExtent = ExtentFor( le, AFS_EXTENTS_LIST );
        
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

        le = le->Flink;
    }

    AFSReleaseResource( &Fcb->NPFcb->Specific.File.ExtentsResource );
    
}
//
// Helper functions
//

static AFSExtent *ExtentFor(PLIST_ENTRY le, ULONG SkipList) 
{
    return CONTAINING_RECORD( le, AFSExtent, Lists[SkipList] );
}

static AFSExtent *NextExtent(AFSExtent *Extent, ULONG SkipList)
{
    return ExtentFor(Extent->Lists[SkipList].Flink, SkipList);
}

static VOID VerifyExtentsLists(AFSFcb *Fcb)
{
    //
    // Check the ordering of the extents lists
    //
    ASSERT( ExIsResourceAcquiredLite( &Fcb->NPFcb->Specific.File.ExtentsResource ));
    

    for (ULONG listNo = 0; listNo < AFS_NUM_EXTENT_LISTS; listNo ++)
    {
        LARGE_INTEGER lastOffset;

        lastOffset.QuadPart = 0;

        for (PLIST_ENTRY pLe = Fcb->Specific.File.ExtentsLists[listNo].Flink; 
             pLe != &Fcb->Specific.File.ExtentsLists[listNo];
             pLe = pLe->Flink)
        {
            AFSExtent *pExtent;

            pExtent = ExtentFor(pLe, listNo);

            ASSERT(pLe->Flink->Blink == pLe);
            ASSERT(pLe->Blink->Flink == pLe);

            //
            // Should follow on from previous
            //
            ASSERT(pExtent->FileOffset.QuadPart >= lastOffset.QuadPart);
            lastOffset.QuadPart = pExtent->FileOffset.QuadPart + pExtent->Size;

            //
            // Should match alignment criteria
            //
            ASSERT( 0 == (pExtent->FileOffset.LowPart & ExtentsMasks[listNo]) );

            //
            // "lower" lists should be populated
            //
            for (LONG subListNo = listNo-1; subListNo > 0; subListNo --)
            {
                ASSERT( !IsListEmpty(&pExtent->Lists[subListNo]));
            }
        }
    }
}
