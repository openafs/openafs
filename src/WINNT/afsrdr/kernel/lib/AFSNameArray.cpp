/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kernel Drivers, LLC.
 * Copyright (c) 2009, 2010, 2011, 2012, 2013 Your File System, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
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
// File: AFSNameArray.cpp
//

#include "AFSCommon.h"

AFSNameArrayHdr *
AFSInitNameArray( IN AFSDirectoryCB *DirectoryCB,
                  IN ULONG InitialElementCount)
{

    AFSNameArrayHdr *pNameArray = NULL;
    AFSNameArrayCB *pCurrentElement = NULL;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    LONG lCount;

    __Enter
    {

        if( InitialElementCount == 0)
        {

            InitialElementCount = pDevExt->Specific.RDR.NameArrayLength;
        }

        pNameArray = (AFSNameArrayHdr *)AFSExAllocatePoolWithTag( PagedPool,
                                                                  sizeof( AFSNameArrayHdr) +
                                                                    (InitialElementCount * sizeof( AFSNameArrayCB)),
                                                                  AFS_NAME_ARRAY_TAG);

        if( pNameArray == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitNameArray Failed to allocate name array\n"));

            try_return( pNameArray);
        }

        RtlZeroMemory( pNameArray,
                       sizeof( AFSNameArrayHdr) +
                          (InitialElementCount * sizeof( AFSNameArrayCB)));

        pNameArray->MaxElementCount = InitialElementCount;

        if( DirectoryCB != NULL)
        {

            pCurrentElement = &pNameArray->ElementArray[ 0];

            pNameArray->CurrentEntry = pCurrentElement;

            pNameArray->Count = 1;

            pNameArray->LinkCount = 0;

            lCount = InterlockedIncrement( &DirectoryCB->NameArrayReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInitNameArray [NA:%p] Increment count on %wZ DE %p Cnt %d\n",
                          pNameArray,
                          &DirectoryCB->NameInformation.FileName,
                          DirectoryCB,
                          lCount));

            pCurrentElement->DirectoryCB = DirectoryCB;

            pCurrentElement->Component = DirectoryCB->NameInformation.FileName;

            pCurrentElement->FileId = DirectoryCB->ObjectInformation->FileId;

            if( pCurrentElement->FileId.Vnode == 1)
            {

                SetFlag( pCurrentElement->Flags, AFS_NAME_ARRAY_FLAG_ROOT_ELEMENT);
            }

            AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInitNameArray [NA:%p] Element[0] DE %p FID %08lX-%08lX-%08lX-%08lX %wZ Type %d\n",
                          pNameArray,
                          pCurrentElement->DirectoryCB,
                          pCurrentElement->FileId.Cell,
                          pCurrentElement->FileId.Volume,
                          pCurrentElement->FileId.Vnode,
                          pCurrentElement->FileId.Unique,
                          &pCurrentElement->DirectoryCB->NameInformation.FileName,
                          pCurrentElement->DirectoryCB->ObjectInformation->FileType));
        }

try_exit:

        NOTHING;
    }

    return pNameArray;
}

NTSTATUS
AFSPopulateNameArray( IN AFSNameArrayHdr *NameArray,
                      IN UNICODE_STRING *Path,
                      IN AFSDirectoryCB *DirectoryCB)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSNameArrayCB *pCurrentElement = NULL;
    LONG lCount;

    __Enter
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSPopulateNameArray [NA:%p] passed Path %wZ DE %p FID %08lX-%08lX-%08lX-%08lX %wZ Type %d\n",
                      NameArray,
                      &Path,
                      DirectoryCB,
                      DirectoryCB->ObjectInformation->FileId.Cell,
                      DirectoryCB->ObjectInformation->FileId.Volume,
                      DirectoryCB->ObjectInformation->FileId.Vnode,
                      DirectoryCB->ObjectInformation->FileId.Unique,
                      &DirectoryCB->NameInformation.FileName,
                      DirectoryCB->ObjectInformation->FileType));

        //
        // Init some info in the header
        //

        pCurrentElement = &NameArray->ElementArray[ 0];

        NameArray->CurrentEntry = pCurrentElement;

        //
        // The first entry points at the root
        //

        pCurrentElement->DirectoryCB = DirectoryCB->ObjectInformation->VolumeCB->DirectoryCB;

        lCount = InterlockedIncrement( &pCurrentElement->DirectoryCB->NameArrayReferenceCount);

        AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSPopulateNameArray [NA:%p] Increment count on volume %wZ DE %p Cnt %d\n",
                      NameArray,
                      &pCurrentElement->DirectoryCB->NameInformation.FileName,
                      pCurrentElement->DirectoryCB,
                      lCount));

        pCurrentElement->Component = DirectoryCB->ObjectInformation->VolumeCB->DirectoryCB->NameInformation.FileName;

        pCurrentElement->FileId = DirectoryCB->ObjectInformation->VolumeCB->ObjectInformation.FileId;

        pCurrentElement->Flags = 0;

        if( pCurrentElement->FileId.Vnode == 1)
        {

            SetFlag( pCurrentElement->Flags, AFS_NAME_ARRAY_FLAG_ROOT_ELEMENT);
        }

        NameArray->Count = 1;

        NameArray->LinkCount = 0;

        AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSPopulateNameArray [NA:%p] Element[0] DE %p FID %08lX-%08lX-%08lX-%08lX %wZ Type %d\n",
                      NameArray,
                      pCurrentElement->DirectoryCB,
                      pCurrentElement->FileId.Cell,
                      pCurrentElement->FileId.Volume,
                      pCurrentElement->FileId.Vnode,
                      pCurrentElement->FileId.Unique,
                      &pCurrentElement->DirectoryCB->NameInformation.FileName,
                      pCurrentElement->DirectoryCB->ObjectInformation->FileType));

        //
        // If the root is the parent then we are done ...
        //

        if( &DirectoryCB->ObjectInformation->VolumeCB->ObjectInformation == DirectoryCB->ObjectInformation)
        {
            try_return( ntStatus);
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSPopulateNameArrayFromRelatedArray( IN AFSNameArrayHdr *NameArray,
                                      IN AFSNameArrayHdr *RelatedNameArray,
                                      IN AFSDirectoryCB *DirectoryCB)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSNameArrayCB *pCurrentElement = NULL, *pCurrentRelatedElement = NULL;
    LONG lCount;

    __Enter
    {

        if ( DirectoryCB)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSPopulateNameArray [NA:%p] passed RelatedNameArray %p DE %p FID %08lX-%08lX-%08lX-%08lX %wZ Type %d\n",
                          NameArray,
                          RelatedNameArray,
                          DirectoryCB,
                          DirectoryCB->ObjectInformation->FileId.Cell,
                          DirectoryCB->ObjectInformation->FileId.Volume,
                          DirectoryCB->ObjectInformation->FileId.Vnode,
                          DirectoryCB->ObjectInformation->FileId.Unique,
                          &DirectoryCB->NameInformation.FileName,
                          DirectoryCB->ObjectInformation->FileType));
        }
        else
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSPopulateNameArray [NA:%p] passed RelatedNameArray %p DE NULL\n",
                          NameArray,
                          RelatedNameArray));
        }

        //
        // Init some info in the header
        //

        pCurrentElement = &NameArray->ElementArray[ 0];

        pCurrentRelatedElement = &RelatedNameArray->ElementArray[ 0];

        NameArray->Count = 0;

        NameArray->LinkCount = RelatedNameArray->LinkCount;

        //
        // Populate the name array with the data from the related array
        //

        while( TRUE)
        {

            pCurrentElement->DirectoryCB = pCurrentRelatedElement->DirectoryCB;

            pCurrentElement->Component = pCurrentRelatedElement->DirectoryCB->NameInformation.FileName;

            pCurrentElement->FileId    = pCurrentElement->DirectoryCB->ObjectInformation->FileId;

            pCurrentElement->Flags = 0;

            if( pCurrentElement->FileId.Vnode == 1)
            {

                SetFlag( pCurrentElement->Flags, AFS_NAME_ARRAY_FLAG_ROOT_ELEMENT);
            }

            lCount = InterlockedIncrement( &pCurrentElement->DirectoryCB->NameArrayReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSPopulateNameArrayFromRelatedArray [NA:%p] Increment count on %wZ DE %p Cnt %d\n",
                          NameArray,
                          &pCurrentElement->DirectoryCB->NameInformation.FileName,
                          pCurrentElement->DirectoryCB,
                          lCount));

            lCount = InterlockedIncrement( &NameArray->Count);

            AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSPopulateNameArrayFromRelatedArray [NA:%p] Element[%d] DE %p FID %08lX-%08lX-%08lX-%08lX %wZ Type %d\n",
                          NameArray,
                          lCount - 1,
                          pCurrentElement->DirectoryCB,
                          pCurrentElement->FileId.Cell,
                          pCurrentElement->FileId.Volume,
                          pCurrentElement->FileId.Vnode,
                          pCurrentElement->FileId.Unique,
                          &pCurrentElement->DirectoryCB->NameInformation.FileName,
                          pCurrentElement->DirectoryCB->ObjectInformation->FileType));

            if( pCurrentElement->DirectoryCB == DirectoryCB ||
                NameArray->Count == RelatedNameArray->Count)
            {

                //
                // Done ...
                //

                break;
            }

            pCurrentElement++;

            pCurrentRelatedElement++;
        }

        NameArray->CurrentEntry = NameArray->Count > 0 ? pCurrentElement : NULL;
    }

    return ntStatus;
}

NTSTATUS
AFSFreeNameArray( IN AFSNameArrayHdr *NameArray)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSNameArrayCB *pCurrentElement = NULL;
    LONG lCount, lElement;

    __Enter
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSFreeNameArray [NA:%p]\n",
                      NameArray));

        for ( lElement = 0; lElement < NameArray->Count; lElement++)
        {

            pCurrentElement = &NameArray->ElementArray[ lElement];

            lCount = InterlockedDecrement( &pCurrentElement->DirectoryCB->NameArrayReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSFreeNameArray [NA:%p] Decrement count on %wZ DE %p Cnt %d\n",
                          NameArray,
                          &pCurrentElement->DirectoryCB->NameInformation.FileName,
                          pCurrentElement->DirectoryCB,
                          lCount));

            ASSERT( lCount >= 0);
        }

        AFSExFreePoolWithTag( NameArray, AFS_NAME_ARRAY_TAG);
    }

    return ntStatus;
}

NTSTATUS
AFSInsertNextElement( IN AFSNameArrayHdr *NameArray,
                      IN AFSDirectoryCB *DirectoryCB)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSNameArrayCB *pCurrentElement = NULL;
    LONG lCount;

    __Enter
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInsertNextElement [NA:%p] passed DE %p FID %08lX-%08lX-%08lX-%08lX %wZ Type %d\n",
                      NameArray,
                      DirectoryCB,
                      DirectoryCB->ObjectInformation->FileId.Cell,
                      DirectoryCB->ObjectInformation->FileId.Volume,
                      DirectoryCB->ObjectInformation->FileId.Vnode,
                      DirectoryCB->ObjectInformation->FileId.Unique,
                      &DirectoryCB->NameInformation.FileName,
                      DirectoryCB->ObjectInformation->FileType));

        if( NameArray->Count == (LONG) NameArray->MaxElementCount)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInsertNextElement [NA:%p] Name has reached Maximum Size\n",
                          NameArray));

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        for ( lCount = 0; lCount < NameArray->Count; lCount++)
        {

            if ( AFSIsEqualFID( &NameArray->ElementArray[ lCount].FileId,
                                &DirectoryCB->ObjectInformation->FileId) )
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSInsertNextElement [NA:%p] DE %p recursion Status %08X\n",
                              NameArray,
                              DirectoryCB,
                              STATUS_ACCESS_DENIED));

                try_return( ntStatus = STATUS_ACCESS_DENIED);
            }
        }

        if( NameArray->Count > 0)
        {

            NameArray->CurrentEntry++;
        }
        else
        {
            NameArray->CurrentEntry = &NameArray->ElementArray[ 0];
        }

        pCurrentElement = NameArray->CurrentEntry;

        lCount = InterlockedIncrement( &NameArray->Count);

        lCount = InterlockedIncrement( &DirectoryCB->NameArrayReferenceCount);

        AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInsertNextElement [NA:%p] Increment count on %wZ DE %p Cnt %d\n",
                      NameArray,
                      &DirectoryCB->NameInformation.FileName,
                      DirectoryCB,
                      lCount));

        ASSERT( lCount > 0);

        pCurrentElement->DirectoryCB = DirectoryCB;

        pCurrentElement->Component = DirectoryCB->NameInformation.FileName;

        pCurrentElement->FileId = DirectoryCB->ObjectInformation->FileId;

        pCurrentElement->Flags = 0;

        if( pCurrentElement->FileId.Vnode == 1)
        {

            SetFlag( pCurrentElement->Flags, AFS_NAME_ARRAY_FLAG_ROOT_ELEMENT);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInsertNextElement [NA:%p] Element[%d] DE %p FID %08lX-%08lX-%08lX-%08lX %wZ Type %d\n",
                      NameArray,
                      NameArray->Count - 1,
                      pCurrentElement->DirectoryCB,
                      pCurrentElement->FileId.Cell,
                      pCurrentElement->FileId.Volume,
                      pCurrentElement->FileId.Vnode,
                      pCurrentElement->FileId.Unique,
                      &pCurrentElement->DirectoryCB->NameInformation.FileName,
                      pCurrentElement->DirectoryCB->ObjectInformation->FileType));

try_exit:

        NOTHING;
    }

    return ntStatus;
}

AFSDirectoryCB *
AFSBackupEntry( IN AFSNameArrayHdr *NameArray)
{

    AFSDirectoryCB *pDirectoryCB = NULL;
    AFSNameArrayCB *pCurrentElement = NULL;
    BOOLEAN         bVolumeRoot = FALSE;
    LONG lCount;

    __Enter
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSBackupEntry [NA:%p]\n",
                      NameArray));

        if( NameArray->Count == 0)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSBackupEntry [NA:%p] No more entries\n",
                          NameArray));

            try_return( pCurrentElement);
        }

        lCount = InterlockedDecrement( &NameArray->CurrentEntry->DirectoryCB->NameArrayReferenceCount);

        AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSBackupEntry [NA:%p] Decrement count on %wZ DE %p Cnt %d\n",
                      NameArray,
                      &NameArray->CurrentEntry->DirectoryCB->NameInformation.FileName,
                      NameArray->CurrentEntry->DirectoryCB,
                      lCount));

        ASSERT( lCount >= 0);

        NameArray->CurrentEntry->DirectoryCB = NULL;

        lCount = InterlockedDecrement( &NameArray->Count);

        if( lCount == 0)
        {
            NameArray->CurrentEntry = NULL;

            AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSBackupEntry [NA:%p] No more entries\n",
                          NameArray));
        }
        else
        {

            bVolumeRoot = BooleanFlagOn( NameArray->CurrentEntry->Flags, AFS_NAME_ARRAY_FLAG_ROOT_ELEMENT);

            NameArray->CurrentEntry--;

            pCurrentElement = NameArray->CurrentEntry;

            pDirectoryCB = pCurrentElement->DirectoryCB;

            AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSBackupEntry [NA:%p] Returning Element[%d] DE %p FID %08lX-%08lX-%08lX-%08lX %wZ Type %d\n",
                          NameArray,
                          NameArray->Count - 1,
                          pCurrentElement->DirectoryCB,
                          pCurrentElement->FileId.Cell,
                          pCurrentElement->FileId.Volume,
                          pCurrentElement->FileId.Vnode,
                          pCurrentElement->FileId.Unique,
                          &pCurrentElement->DirectoryCB->NameInformation.FileName,
                          pCurrentElement->DirectoryCB->ObjectInformation->FileType));

            //
            // If the entry we are removing is a volume root,
            // we must remove the mount point entry as well.
            // If the NameArray was constructed by checking the
            // share name via the service, the name array can
            // contain two volume roots in sequence without a
            // mount point separating them.
            //

            if ( bVolumeRoot &&
                 !BooleanFlagOn( NameArray->CurrentEntry->Flags, AFS_NAME_ARRAY_FLAG_ROOT_ELEMENT))
            {

                pDirectoryCB = AFSBackupEntry( NameArray);
            }
        }


try_exit:

        NOTHING;
    }

    return pDirectoryCB;
}

AFSDirectoryCB *
AFSGetParentEntry( IN AFSNameArrayHdr *NameArray)
{

    AFSDirectoryCB *pDirEntry = NULL;
    AFSNameArrayCB *pElement = NULL;

    __Enter
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSGetParentEntry [NA:%p]\n",
                      NameArray));

        if( NameArray->Count == 0 ||
            NameArray->Count == 1)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSGetParentEntry [NA:%p] No more entries\n",
                          NameArray));

            try_return( pDirEntry = NULL);
        }

        pElement = &NameArray->ElementArray[ NameArray->Count - 2];

        pDirEntry = pElement->DirectoryCB;

        AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSGetParentEntry [NA:%p] Returning Element[%d] DE %p FID %08lX-%08lX-%08lX-%08lX %wZ Type %d\n",
                      NameArray,
                      NameArray->Count - 2,
                      pElement->DirectoryCB,
                      pElement->FileId.Cell,
                      pElement->FileId.Volume,
                      pElement->FileId.Vnode,
                      pElement->FileId.Unique,
                      &pElement->DirectoryCB->NameInformation.FileName,
                      pElement->DirectoryCB->ObjectInformation->FileType));

try_exit:

        NOTHING;
    }

    return pDirEntry;
}

void
AFSResetNameArray( IN AFSNameArrayHdr *NameArray,
                   IN AFSDirectoryCB *DirectoryCB)
{

    AFSNameArrayCB *pCurrentElement = NULL;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    LONG lCount, lElement;

    __Enter
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSResetNameArray [NA:%p] passed DE %p FID %08lX-%08lX-%08lX-%08lX %wZ Type %d\n",
                      NameArray,
                      DirectoryCB,
                      DirectoryCB->ObjectInformation->FileId.Cell,
                      DirectoryCB->ObjectInformation->FileId.Volume,
                      DirectoryCB->ObjectInformation->FileId.Vnode,
                      DirectoryCB->ObjectInformation->FileId.Unique,
                      &DirectoryCB->NameInformation.FileName,
                      DirectoryCB->ObjectInformation->FileType));

        //
        // Dereference previous name array contents
        //

        for ( lElement = 0; lElement < NameArray->Count; lElement++)
        {

            pCurrentElement = &NameArray->ElementArray[ lElement];

            lCount = InterlockedDecrement( &pCurrentElement->DirectoryCB->NameArrayReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSResetNameArray [NA:%p] Decrement count on %wZ DE %p Cnt %d\n",
                          NameArray,
                          &pCurrentElement->DirectoryCB->NameInformation.FileName,
                          pCurrentElement->DirectoryCB,
                          lCount));

            ASSERT( lCount >= 0);
        }

        RtlZeroMemory( NameArray,
                       sizeof( AFSNameArrayHdr) +
                          ((pDevExt->Specific.RDR.NameArrayLength - 1) * sizeof( AFSNameArrayCB)));

        NameArray->MaxElementCount = pDevExt->Specific.RDR.NameArrayLength;

        if( DirectoryCB != NULL)
        {

            pCurrentElement = &NameArray->ElementArray[ 0];

            NameArray->CurrentEntry = pCurrentElement;

            NameArray->Count = 1;

            NameArray->LinkCount = 0;

            lCount = InterlockedIncrement( &DirectoryCB->NameArrayReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSResetNameArray [NA:%p] Increment count on %wZ DE %p Cnt %d\n",
                          NameArray,
                          &DirectoryCB->NameInformation.FileName,
                          DirectoryCB,
                          lCount));

            pCurrentElement->DirectoryCB = DirectoryCB;

            pCurrentElement->Component = DirectoryCB->NameInformation.FileName;

            pCurrentElement->FileId = DirectoryCB->ObjectInformation->FileId;

            pCurrentElement->Flags  = 0;

            if( pCurrentElement->FileId.Vnode == 1)
            {

                SetFlag( pCurrentElement->Flags, AFS_NAME_ARRAY_FLAG_ROOT_ELEMENT);
            }

            AFSDbgTrace(( AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSResetNameArray [NA:%p] Element[0] DE %p FID %08lX-%08lX-%08lX-%08lX %wZ Type %d\n",
                          NameArray,
                          pCurrentElement->DirectoryCB,
                          pCurrentElement->FileId.Cell,
                          pCurrentElement->FileId.Volume,
                          pCurrentElement->FileId.Vnode,
                          pCurrentElement->FileId.Unique,
                          &pCurrentElement->DirectoryCB->NameInformation.FileName,
                          pCurrentElement->DirectoryCB->ObjectInformation->FileType));
        }
    }

    return;
}

void
AFSDumpNameArray( IN AFSNameArrayHdr *NameArray)
{

    AFSNameArrayCB *pCurrentElement = NULL;

    pCurrentElement = &NameArray->ElementArray[ 0];

    AFSPrint("AFSDumpNameArray Start (%d)\n", NameArray->Count);

    while( pCurrentElement->DirectoryCB != NULL)
    {

        AFSPrint("FID %08lX-%08lX-%08lX-%08lX %wZ\n",
                  pCurrentElement->FileId.Cell,
                  pCurrentElement->FileId.Volume,
                  pCurrentElement->FileId.Vnode,
                  pCurrentElement->FileId.Unique,
                  &pCurrentElement->DirectoryCB->NameInformation.FileName);

        pCurrentElement++;
    }

    AFSPrint("AFSDumpNameArray End\n\n");

    return;
}
