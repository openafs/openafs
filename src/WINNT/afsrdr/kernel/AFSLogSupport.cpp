
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

#include "AFSCommon.h"

#ifdef AFS_DEBUG_LOG

NTSTATUS
AFSDbgLogMsg( IN PCCH Format,
              ...)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    va_list va_args;
    ULONG ulBytesWritten = 0;

    __Enter
    {

        if( AFSDbgBuffer == NULL)
        {

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        AFSAcquireExcl( &AFSDbgLogLock,
                        TRUE);

        if( AFSDbgLogRemainingLength < 255)
        {

            AFSDbgLogRemainingLength = AFS_DBG_LOG_LENGTH;

            AFSDbgCurrentBuffer = AFSDbgBuffer;
        }

        RtlStringCchPrintfA( AFSDbgCurrentBuffer,
                             10,
                             "%08lX:", 
                             AFSDbgLogCounter++); 

        AFSDbgCurrentBuffer += 9;

        AFSDbgLogRemainingLength -= 9;

        va_start( va_args, Format);

        ntStatus = RtlStringCbVPrintfA( AFSDbgCurrentBuffer,
                                        AFSDbgLogRemainingLength,
                                        Format,
                                        va_args);

        if( ntStatus == STATUS_BUFFER_OVERFLOW)
        {

            AFSDbgLogRemainingLength = AFS_DBG_LOG_LENGTH;

            AFSDbgCurrentBuffer = AFSDbgBuffer;

            ntStatus = RtlStringCbVPrintfA( AFSDbgCurrentBuffer,
                                            AFSDbgLogRemainingLength,
                                            Format,
                                            va_args);
        }

        if( NT_SUCCESS( ntStatus))
        {

            RtlStringCbLengthA( AFSDbgCurrentBuffer,
                                AFSDbgLogRemainingLength,
                                (size_t *)&ulBytesWritten);

            AFSDbgCurrentBuffer += ulBytesWritten;

            AFSDbgLogRemainingLength -= ulBytesWritten;
        }

        va_end( va_args);

try_exit:

        AFSReleaseResource( &AFSDbgLogLock);
    }

    return ntStatus;
}

NTSTATUS
AFSInitializeDbgLog()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    AFSDbgBuffer = (char *)ExAllocatePoolWithTag( NonPagedPool,
                                                  AFS_DBG_LOG_LENGTH,
                                                  AFS_GENERIC_MEMORY_TAG);

    if( AFSDbgBuffer != NULL)
    {

        ExInitializeResourceLite( &AFSDbgLogLock);

        AFSDbgCurrentBuffer = AFSDbgBuffer;

        AFSDbgLogRemainingLength = AFS_DBG_LOG_LENGTH;
    }

    return ntStatus;
}

NTSTATUS
AFSTearDownDbgLog()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    if( AFSDbgBuffer != NULL)
    {

        ExDeleteResourceLite( &AFSDbgLogLock);

        ExFreePool( AFSDbgBuffer);

        AFSDbgBuffer = NULL;
    }

    return ntStatus;
}

#endif
