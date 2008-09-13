//
// File: AFSRead.cpp
//

#include "AFSCommon.h"

static AFSExtent *NextExtent(AFSExtent *Extent);

//
// Called in the paging or non cached read and write paths to get the
// first and last extent in a span.  We also the count of how many
// discontiguous extents the are in the cache file.
//

NTSTATUS
AFSGetExtents( IN AFSFcb *Fcb,
               IN PLARGE_INTEGER Offset,
               IN ULONG Length,
               OUT AFSExtent *From,
               OUT ULONG *ExtentCount)
{
    NTSTATUS           ntStatus = STATUS_SUCCESS;
    ULONG              extentsCount = 0;
    AFSExtent         *pEndExtent = NULL;
    LARGE_INTEGER      liLastCacheOffset;

    *ExtentCount = 0;

    __Enter
    {
        ASSERT(AFSExtentContains(From, Offset));

        pEndExtent = From;
        extentsCount = 1;
        liLastCacheOffset.QuadPart = pEndExtent->CacheOffset.QuadPart + pEndExtent->Size;

        while ((Offset->QuadPart + Length) > 
               pEndExtent->FileOffset.QuadPart + pEndExtent->Size) {

            pEndExtent = NextExtent(pEndExtent);

            if (liLastCacheOffset.QuadPart != pEndExtent->CacheOffset.QuadPart) {
                //
                // Discontiguous (in the cache)
                //
                extentsCount += 1;
                
            }
            liLastCacheOffset.QuadPart = pEndExtent->CacheOffset.QuadPart + pEndExtent->Size;
        }

        *ExtentCount = extentsCount;
        ntStatus = STATUS_SUCCESS;

    try_exit:
        ;
    }
    return ntStatus;
}


NTSTATUS
AFSSetupIoRun( IN PDEVICE_OBJECT CacheDevice,
               IN PIRP           MasterIrp,
               IN PVOID          SystemBuffer,
               IN OUT AFSIoRun  *IoRuns,
               IN PLARGE_INTEGER Start,
               IN ULONG          Length,
               IN AFSExtent     *From,
               IN OUT ULONG     *ExtentsCount)
{
    //
    // Do all the work which we can prior to firing off the IRP
    // (allocate them, calculate offsets and so on)
    //
    LARGE_INTEGER  liCacheOffset;
    LARGE_INTEGER  liFileOffset = *Start;
    ULONG          ulCurrLength;
    ULONG          ulReadOffset;
    ULONG          ulCurrRun = 0;
    AFSExtent     *pExtent = From;
    AFSExtent     *pNextExtent;
    PMDL          *pMDL;
    BOOLEAN        done = FALSE;
    NTSTATUS       ntStatus = STATUS_SUCCESS;

    __Enter
    {
    
        while( !done)
        {
            ASSERT( liFileOffset.QuadPart >= pExtent->FileOffset.QuadPart );

            liCacheOffset = pExtent->CacheOffset;
            liCacheOffset.QuadPart += (liFileOffset.QuadPart - 
                                       pExtent->FileOffset.QuadPart);

            ulCurrLength = 0;

            //
            // While there is still data to process, loop
            //
            while ( (pExtent->FileOffset.QuadPart + pExtent->Size) <
                    (Start->QuadPart + Length) )
            {
                //
                // Collapse the read if we can
                //
                pNextExtent = NextExtent( pExtent );

                if (pNextExtent->CacheOffset.QuadPart != 
                    (pExtent->CacheOffset.QuadPart + pExtent->Size)) 
                {
                    //
                    // This extent and the next extent are not contiguous
                    //
                    break;
                }
                pExtent = pNextExtent;
            }

            //
            // So, this run goes from liCacheOffset to the end of pExtent
            //

            if ((pExtent->FileOffset.QuadPart + pExtent->Size) >= (Start->QuadPart + Length))
            {
                //
                // The last extent overruns the area we care
                // about, so trim it back.
                //
                ulCurrLength = (ULONG) (Start->QuadPart + Length - 
                                        liFileOffset.QuadPart);
                //
                // But we are done
                //
                done = TRUE;
            } 
            else 
            {
                //
                // Length is from the LiFileOffset to the end of this
                // extent.
                //
                ulCurrLength = (ULONG) (pExtent->FileOffset.QuadPart + pExtent->Size -
                                        liFileOffset.QuadPart);
                done = FALSE;
            }

            //
            // Calculate the offset into the buffer
            //
            ulReadOffset = (ULONG) (liFileOffset.QuadPart - Start->QuadPart);

            IoRuns[ulCurrRun].CacheOffset = liCacheOffset;
            IoRuns[ulCurrRun].ByteCount = ulCurrLength;

            IoRuns[ulCurrRun].ChildIrp = IoAllocateIrp( CacheDevice->StackSize + 1, 
                                                        FALSE);

            if (NULL == IoRuns[ulCurrRun].ChildIrp) 
            {
                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES );
            }

            IoRuns[ulCurrRun].ChildIrp->UserBuffer = ((PCHAR) SystemBuffer) + ulReadOffset;
            IoRuns[ulCurrRun].ChildIrp->Tail.Overlay.Thread = PsGetCurrentThread();
            IoRuns[ulCurrRun].ChildIrp->RequestorMode = KernelMode;
            IoRuns[ulCurrRun].ChildIrp->MdlAddress = NULL;
    
            ulCurrRun ++;
            if (!done)
            {
                ASSERT(pNextExtent != NULL && pNextExtent != pExtent);
                //
                // Move cursors forward
                //
                pExtent = pNextExtent;
                liFileOffset = pExtent->FileOffset;
            }
        }
        ASSERT(ulCurrRun <= *ExtentsCount);
try_exit:
        if (!NT_SUCCESS(ntStatus)) 
        {
            for (ulCurrRun = 0 ; ulCurrRun < *ExtentsCount; ulCurrRun++) 
            {
                if (IoRuns[ulCurrRun].ChildIrp) {
                    IoFreeIrp( IoRuns[ulCurrRun].ChildIrp );
                    IoRuns[ulCurrRun].ChildIrp = NULL;
                }
            }
        } else {
            *ExtentsCount = ulCurrRun;
        }
    }
    return ntStatus;
}

//
// We manage our own scatter / gather.  This completion 
// function calls into our generic function.
//

static
NTSTATUS
CompletionFunction(IN PDEVICE_OBJECT DeviceObject,
                   PIRP Irp,
                   PVOID Context) 
{
    AFSGatherIo *pGather = (AFSGatherIo *) Context;

    AFSCompleteIo(pGather, Irp->IoStatus.Status);

    if (Irp->MdlAddress) {
        if (Irp->MdlAddress->MdlFlags & MDL_PAGES_LOCKED) {
            MmUnlockPages(Irp->MdlAddress);
        }
        IoFreeMdl(Irp->MdlAddress);
        Irp->MdlAddress = NULL;
    }
    IoFreeIrp(Irp);

    //
    // Tell io manager that we own the Irp
    //
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS 
AFSStartIos( IN PFILE_OBJECT     CacheFileObject,
             IN UCHAR            FunctionCode,
             IN ULONG            IrpFlags,
             IN AFSIoRun        *IoRun,
             IN ULONG            Count,
             IN OUT AFSGatherIo *Gather)
{
    //
    // So we have to start off the IOs listed.  The Irps are all
    // partially setup and "Gather" Structure has one outstanding IO
    //
    PIO_STACK_LOCATION  pIoStackLocation = NULL;
    PIRP                pIrp;
    NTSTATUS            ntStatus;

    for (ULONG i = 0; i < Count; i++) {

        pIrp = IoRun[i].ChildIrp;

        pIoStackLocation = IoGetNextIrpStackLocation( pIrp);

        pIrp->Flags = IrpFlags;
        pIoStackLocation->MajorFunction = FunctionCode;
        pIoStackLocation->DeviceObject = CacheFileObject->Vpb->DeviceObject;
        pIoStackLocation->FileObject = CacheFileObject;
        pIoStackLocation->Parameters.Write.Length = IoRun[i].ByteCount;
        pIoStackLocation->Parameters.Write.ByteOffset = IoRun[i].CacheOffset;

        //
        // Bump the count *before* we start the IO, that way if it
        // completes real fast it will still not set the event
        //
        InterlockedIncrement(&Gather->Count);

        //
        // Set the completion routine.
        //

        IoSetCompletionRoutine( pIrp, 
                                CompletionFunction,
                                Gather, 
                                TRUE, 
                                TRUE, 
                                TRUE);

        //
        // Send it to the Cache
        //

        ntStatus = IoCallDriver( CacheFileObject->Vpb->DeviceObject,
                                 pIrp);

        if( !NT_SUCCESS( ntStatus))
        {
            break;
        }
    }
    return ntStatus;
}

VOID
AFSCompleteIo( IN AFSGatherIo *Gather, NTSTATUS Status )
{
    if (!NT_SUCCESS(Status)) {

        Gather->Status = Status;
    }

    if (0 == InterlockedDecrement( &Gather->Count )) {
        //
        // Last outstanding IO.  Complete the parent and
        // do whatever it takes to clean up
        //
        if (!NT_SUCCESS(Gather->Status))
        {
            Gather->MasterIrp->IoStatus.Status = Gather->Status;
            Gather->MasterIrp->IoStatus.Information = 0;
        }

        AFSCompleteRequest(Gather->MasterIrp, Gather->Status);

        if (Gather->Synchronous) 
        {
            //
            // Someone was waiting for us tell them.  They also own
            // the data structure so we don't free it
            //
            KeSetEvent( &Gather->Event,
                        0,
                        FALSE);
        } 
        else 
        {
            ExFreePoolWithTag( Gather, AFS_GATHER_TAG );
        }
    }
}

static AFSExtent *ExtentFor(PLIST_ENTRY le) 
{
    return CONTAINING_RECORD( le, AFSExtent, Lists[AFS_EXTENTS_LIST] );
}

static AFSExtent *NextExtent(AFSExtent *Extent)
{
    return ExtentFor(Extent->Lists[AFS_EXTENTS_LIST].Flink);
}
