/*
 * Copyright (c) 2008, 2009 Kernel Drivers, LLC.
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
// File: AFSProcessSupport.cpp
//

#include "AFSCommon.h"

void
AFSProcessNotify( IN HANDLE  ParentId,
                  IN HANDLE  ProcessId,
                  IN BOOLEAN  Create)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSProcessCB *pProcessCB = NULL;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    __Enter
    {

        //
        // If this is a create notification then update our tree, otherwise remove the
        // entry
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSProcessNotify Acquiring Control ProcessTree.TreeLock lock %08lX EXCL %08lX\n",
                      pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                      PsGetCurrentThread());

        AFSAcquireExcl( pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                        TRUE);

        if( Create)
        {

            pProcessCB = (AFSProcessCB *)ExAllocatePoolWithTag( PagedPool,
                                                                sizeof( AFSProcessCB),
                                                                AFS_PROCESS_CB_TAG);

            if( pProcessCB != NULL)
            {

                RtlZeroMemory( pProcessCB,
                               sizeof( AFSProcessCB));

                pProcessCB->TreeEntry.HashIndex = (ULONGLONG)ProcessId;

                pProcessCB->ParentProcessId = (ULONGLONG)ParentId;

                if( pDeviceExt->Specific.Control.ProcessTree.TreeHead == NULL)
                {

                    pDeviceExt->Specific.Control.ProcessTree.TreeHead = (AFSBTreeEntry *)pProcessCB;
                }
                else
                {

                    AFSInsertHashEntry( pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                        &pProcessCB->TreeEntry);
                }
            }

            try_return( ntStatus);
        }
        
        //
        // It's a remove so pull the entry
        //

        ntStatus = AFSLocateTreeEntry( pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                       (ULONGLONG)ProcessId,
                                       (AFSBTreeEntry **)&pProcessCB);

        if( NT_SUCCESS( ntStatus) &&
            pProcessCB != NULL)
        {

            AFSRemoveHashEntry( &pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                (AFSBTreeEntry *)pProcessCB);

            ExFreePool( pProcessCB);
        }

try_exit:

        AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);

    }

    return;
}

void
AFSValidateProcessEntry()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSProcessCB *pProcessCB = NULL;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    ULONGLONG ullProcessID = (ULONGLONG)PsGetCurrentProcessId();

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSValidateProcessEntry Acquiring Control ProcessTree.TreeLock lock %08lX SHARED %08lX\n",
                      pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                      PsGetCurrentThread());

        AFSAcquireShared( pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                          TRUE);

        ntStatus = AFSLocateTreeEntry( pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                       (ULONGLONG)ullProcessID,
                                       (AFSBTreeEntry **)&pProcessCB);

        if( !NT_SUCCESS( ntStatus) ||
            pProcessCB == NULL)
        {

            //
            // Need to drop the lock and re-acquire exclusive to add this entry
            //

            AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSValidateProcessEntry Acquiring Control ProcessTree.TreeLock lock %08lX EXCL %08lX\n",
                          pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                          PsGetCurrentThread());

            AFSAcquireExcl( pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                            TRUE);

            ntStatus = AFSLocateTreeEntry( pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                           (ULONGLONG)ullProcessID,
                                           (AFSBTreeEntry **)&pProcessCB);

            if( !NT_SUCCESS( ntStatus) ||
                pProcessCB == NULL)
            {

                pProcessCB = (AFSProcessCB *)ExAllocatePoolWithTag( PagedPool,
                                                                    sizeof( AFSProcessCB),
                                                                    AFS_PROCESS_CB_TAG);

                if( pProcessCB != NULL)
                {

                    RtlZeroMemory( pProcessCB,
                                   sizeof( AFSProcessCB));

                    pProcessCB->TreeEntry.HashIndex = (ULONGLONG)ullProcessID;

                    if( pDeviceExt->Specific.Control.ProcessTree.TreeHead == NULL)
                    {

                        pDeviceExt->Specific.Control.ProcessTree.TreeHead = (AFSBTreeEntry *)pProcessCB;
                    }
                    else
                    {

                        AFSInsertHashEntry( pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                            &pProcessCB->TreeEntry);
                    }
                }
            }

            AFSConvertToShared( pDeviceExt->Specific.Control.ProcessTree.TreeLock);
        }

        //
        // Be sure we have things set up correctly for the process entry
        //

#if defined(_WIN64)

        if( !IoIs32bitProcess( NULL))
        {

            SetFlag( pProcessCB->Flags, AFS_PROCESS_FLAG_IS_64BIT);
        }

#endif

        AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);
    }

    return;
}

BOOLEAN
AFSIs64BitProcess( IN ULONGLONG ProcessId)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    BOOLEAN bIs64Bit = FALSE;
    AFSProcessCB *pProcessCB = NULL;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSIs64BitProcess Acquiring Control ProcessTree.TreeLock lock %08lX SHARED %08lX\n",
                      pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                      PsGetCurrentThread());

        AFSAcquireShared( pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                          TRUE);

        ntStatus = AFSLocateTreeEntry( pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                       (ULONGLONG)ProcessId,
                                       (AFSBTreeEntry **)&pProcessCB);

        if( pProcessCB != NULL)
        {

            bIs64Bit = BooleanFlagOn( pProcessCB->Flags, AFS_PROCESS_FLAG_IS_64BIT);
        }

        AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);
    }

    return bIs64Bit;
}


