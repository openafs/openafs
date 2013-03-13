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
// File: AFSIoSupport.cpp
//

#include "AFSCommon.h"

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
               OUT ULONG *ExtentCount,
               OUT ULONG *RunCount)
{
    UNREFERENCED_PARAMETER(Fcb);
    NTSTATUS           ntStatus = STATUS_SUCCESS;
    ULONG              extentsCount = 0, runCount = 0;
    AFSExtent         *pEndExtent = NULL;
    LARGE_INTEGER      liLastCacheOffset;

    *ExtentCount = 0;

    __Enter
    {
        ASSERT(AFSExtentContains(From, Offset));

        pEndExtent = From;
        extentsCount = 1;
        runCount = 1;
        liLastCacheOffset.QuadPart = pEndExtent->CacheOffset.QuadPart + pEndExtent->Size;

        while ((Offset->QuadPart + Length) >
               pEndExtent->FileOffset.QuadPart + pEndExtent->Size) {

            pEndExtent = NextExtent(pEndExtent, AFS_EXTENTS_LIST);

            if (liLastCacheOffset.QuadPart != pEndExtent->CacheOffset.QuadPart) {
                //
                // Discontiguous (in the cache)
                //
                runCount += 1;

            }

            extentsCount+= 1;

            liLastCacheOffset.QuadPart = pEndExtent->CacheOffset.QuadPart + pEndExtent->Size;
        }

        *ExtentCount = extentsCount;
        *RunCount = runCount;
        ntStatus = STATUS_SUCCESS;
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
               IN OUT ULONG     *RunCount)
{
    UNREFERENCED_PARAMETER(MasterIrp);
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
    AFSExtent     *pNextExtent = NULL;
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
                pNextExtent = NextExtent( pExtent, AFS_EXTENTS_LIST);

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
        ASSERT(ulCurrRun <= *RunCount);
try_exit:
        if (!NT_SUCCESS(ntStatus))
        {
            for (ulCurrRun = 0 ; ulCurrRun < *RunCount; ulCurrRun++)
            {
                if (IoRuns[ulCurrRun].ChildIrp) {
                    IoFreeIrp( IoRuns[ulCurrRun].ChildIrp );
                    IoRuns[ulCurrRun].ChildIrp = NULL;
                }
            }
        }
        else
        {
            *RunCount = ulCurrRun;
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
    UNREFERENCED_PARAMETER(DeviceObject);
    AFSGatherIo *pGather = (AFSGatherIo *) Context;

    AFSCompleteIo( pGather, Irp->IoStatus.Status);

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

    PIO_STACK_LOCATION  pIoStackLocation = NULL;
    PIRP                pIrp;
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PDEVICE_OBJECT      pDeviceObject = NULL;
    LONG                lCount;

    __Enter
    {

        pDeviceObject = IoGetRelatedDeviceObject( CacheFileObject);

        for (ULONG i = 0; i < Count; i++)
        {

            pIrp = IoRun[i].ChildIrp;

            pIoStackLocation = IoGetNextIrpStackLocation( pIrp);

            pIrp->Flags = IrpFlags;
            pIoStackLocation->MajorFunction = FunctionCode;
            pIoStackLocation->DeviceObject = pDeviceObject;
            pIoStackLocation->FileObject = CacheFileObject;
            pIoStackLocation->Parameters.Write.Length = IoRun[i].ByteCount;
            pIoStackLocation->Parameters.Write.ByteOffset = IoRun[i].CacheOffset;

            //
            // Bump the count *before* we start the IO, that way if it
            // completes real fast it will still not set the event
            //
            lCount = InterlockedIncrement(&Gather->Count);

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

            ntStatus = IoCallDriver( pDeviceObject,
                                     pIrp);

            if( !NT_SUCCESS( ntStatus))
            {
                break;
            }
        }
    }

    return ntStatus;
}

VOID
AFSCompleteIo( IN AFSGatherIo *Gather,
               IN NTSTATUS Status)
{
    LONG lCount;

    if (!NT_SUCCESS(Status)) {

        Gather->Status = Status;
    }

    lCount = InterlockedDecrement( &Gather->Count );

    if (0 == lCount) {
        //
        // Last outstanding IO.  Complete the parent and
        // do whatever it takes to clean up
        //
        if (!NT_SUCCESS(Gather->Status))
        {
            Gather->MasterIrp->IoStatus.Status = Gather->Status;
            Gather->MasterIrp->IoStatus.Information = 0;
        }

        if( Gather->CompleteMasterIrp)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCompleteIo Completing Irp %p Status %08lX\n",
                          Gather->MasterIrp,
                          Gather->Status));

            AFSCompleteRequest(Gather->MasterIrp, Gather->Status);
        }

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

            AFSExFreePoolWithTag( Gather, AFS_GATHER_TAG);
        }
    }
}

NTSTATUS
AFSProcessExtentRun( IN PVOID          SystemBuffer,
                     IN PLARGE_INTEGER Start,
                     IN ULONG          Length,
                     IN AFSExtent     *From,
                     IN BOOLEAN        WriteRequest)
{

    LARGE_INTEGER  liCacheOffset;
    LARGE_INTEGER  liFileOffset = *Start;
    ULONG          ulCurrLength;
    ULONG          ulOffset = 0;
    ULONG          ulCurrRun = 0;
    AFSExtent     *pExtent = From;
    AFSExtent     *pNextExtent = NULL;
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
                pNextExtent = NextExtent( pExtent, AFS_EXTENTS_LIST);

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
            ulOffset = (ULONG) (liFileOffset.QuadPart - Start->QuadPart);

            if( WriteRequest)
            {

#ifdef AMD64
                RtlCopyMemory( ((char *)AFSLibCacheBaseAddress + liCacheOffset.QuadPart),
                               ((char *)SystemBuffer + ulOffset),
                               ulCurrLength);
#else
                ASSERT( liCacheOffset.HighPart == 0);
                RtlCopyMemory( ((char *)AFSLibCacheBaseAddress + liCacheOffset.LowPart),
                               ((char *)SystemBuffer + ulOffset),
                               ulCurrLength);
#endif
            }
            else
            {

#ifdef AMD64
                RtlCopyMemory( ((char *)SystemBuffer + ulOffset),
                               ((char *)AFSLibCacheBaseAddress + liCacheOffset.QuadPart),
                               ulCurrLength);
#else
                ASSERT( liCacheOffset.HighPart == 0);
                RtlCopyMemory( ((char *)SystemBuffer + ulOffset),
                               ((char *)AFSLibCacheBaseAddress + liCacheOffset.LowPart),
                               ulCurrLength);
#endif
            }

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
    }

    return ntStatus;
}
