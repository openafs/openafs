
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

NTSTATUS
AFSDbgLogMsg( IN ULONG Subsystem,
              IN ULONG Level,
              IN PCCH Format,
              ...)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    va_list va_args;
    ULONG ulBytesWritten = 0;
    BOOLEAN bReleaseLock = FALSE;
    char    *pCurrentTrace = NULL;

    __Enter
    {

        if( AFSDbgBuffer == NULL)
        {

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        if( Subsystem > 0 &&
            (Subsystem & AFSTraceComponent) == 0)
        {

            //
            // Not tracing this subsystem
            //

            try_return( ntStatus);
        }

        if( Level > 0 &&
            Level > AFSDebugLevel)
        {

            //
            // Not tracing this level
            //

            try_return( ntStatus);
        }

        AFSAcquireExcl( &AFSDbgLogLock,
                        TRUE);

        bReleaseLock = TRUE;

        //
        // Check again under lock
        //

        if( AFSDbgBuffer == NULL)
        {

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        if( AFSDbgLogRemainingLength < 255)
        {

            AFSDbgLogRemainingLength = AFSDbgBufferLength;

            AFSDbgCurrentBuffer = AFSDbgBuffer;

            SetFlag( AFSDbgLogFlags, AFS_DBG_LOG_WRAPPED);
        }

        pCurrentTrace = AFSDbgCurrentBuffer;

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

            RtlZeroMemory( AFSDbgCurrentBuffer,
                           AFSDbgLogRemainingLength);

            AFSDbgLogRemainingLength = AFSDbgBufferLength;

            AFSDbgCurrentBuffer = AFSDbgBuffer;

            SetFlag( AFSDbgLogFlags, AFS_DBG_LOG_WRAPPED);

            pCurrentTrace = AFSDbgCurrentBuffer;

            RtlStringCchPrintfA( AFSDbgCurrentBuffer,
                                 10,
                                 "%08lX:", 
                                 AFSDbgLogCounter++); 

            AFSDbgCurrentBuffer += 9;

            AFSDbgLogRemainingLength -= 9;

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

        if( BooleanFlagOn( AFSDebugFlags, AFS_DBG_TRACE_TO_DEBUGGER) &&
            pCurrentTrace != NULL)
        {

            DbgPrint( pCurrentTrace);
        }

try_exit:

        if( bReleaseLock)
        {

            AFSReleaseResource( &AFSDbgLogLock);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSInitializeDbgLog()
{

    NTSTATUS ntStatus = STATUS_INSUFFICIENT_RESOURCES;

    AFSAcquireExcl( &AFSDbgLogLock,
                    TRUE);

    if( AFSDbgBufferLength > 0)
    {

        AFSDbgBuffer = (char *)ExAllocatePoolWithTag( NonPagedPool,
                                                      AFSDbgBufferLength,
                                                      AFS_GENERIC_MEMORY_TAG);

        if( AFSDbgBuffer != NULL)
        {

            AFSDbgCurrentBuffer = AFSDbgBuffer;

            AFSDbgLogRemainingLength = AFSDbgBufferLength;

            ntStatus = STATUS_SUCCESS;
        }
    }

    AFSReleaseResource( &AFSDbgLogLock);

    if( NT_SUCCESS( ntStatus))
    {

        AFSTagInitialLogEntry();
    }

    return ntStatus;
}

NTSTATUS
AFSTearDownDbgLog()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    AFSAcquireExcl( &AFSDbgLogLock,
                    TRUE);

    if( AFSDbgBuffer != NULL)
    {

        ExFreePool( AFSDbgBuffer);
    }

    AFSDbgBuffer = NULL;

    AFSDbgCurrentBuffer = NULL;

    AFSDbgLogRemainingLength = 0;

    AFSReleaseResource( &AFSDbgLogLock);

    return ntStatus;
}

NTSTATUS
AFSConfigureTrace( IN AFSTraceConfigCB *TraceInfo)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    UNICODE_STRING uniString;

    __Enter
    {

        AFSAcquireExcl( &AFSDbgLogLock,
                        TRUE);

        if( TraceInfo->TraceLevel == AFSDebugLevel &&
            TraceInfo->TraceBufferLength == AFSDbgBufferLength &&
            TraceInfo->Subsystem == AFSTraceComponent)
        {

            //
            // Nothing to do
            //

            try_return( ntStatus);
        }

        //
        // Go update the registry with the new entries
        //

        if( TraceInfo->TraceLevel != (ULONG)-1 &&
            TraceInfo->TraceLevel != AFSDebugLevel)
        {

            AFSDebugLevel = TraceInfo->TraceLevel;

            RtlInitUnicodeString( &uniString,
                                  AFS_REG_DEBUG_LEVEL);

            ntStatus = AFSUpdateRegistryParameter( &uniString,
                                                   REG_DWORD,
                                                   &TraceInfo->TraceLevel,
                                                   sizeof( ULONG));

            if( !NT_SUCCESS( ntStatus))
            {

                DbgPrint("AFSConfigureTrace Failed to set debug level in registry Status %08lX\n", ntStatus);
            }
        }

        if( TraceInfo->Subsystem != (ULONG)-1 &&
            TraceInfo->Subsystem != AFSTraceComponent)
        {

            AFSTraceComponent = TraceInfo->Subsystem;

            RtlInitUnicodeString( &uniString,
                                  AFS_REG_DEBUG_SUBSYSTEM);

            ntStatus = AFSUpdateRegistryParameter( &uniString,
                                                   REG_DWORD,
                                                   &TraceInfo->Subsystem,
                                                   sizeof( ULONG));

            if( !NT_SUCCESS( ntStatus))
            {

                DbgPrint("AFSConfigureTrace Failed to set debug subsystem in registry Status %08lX\n", ntStatus);
            }
        }

        if( TraceInfo->DebugFlags != (ULONG)-1 &&
            TraceInfo->DebugFlags != AFSDebugFlags)
        {

            AFSDebugFlags = TraceInfo->DebugFlags;

            RtlInitUnicodeString( &uniString,
                                  AFS_REG_DEBUG_FLAGS);

            ntStatus = AFSUpdateRegistryParameter( &uniString,
                                                   REG_DWORD,
                                                   &TraceInfo->DebugFlags,
                                                   sizeof( ULONG));

            if( !NT_SUCCESS( ntStatus))
            {

                DbgPrint("AFSConfigureTrace Failed to set debug flags in registry Status %08lX\n", ntStatus);
            }
        }

        if( TraceInfo->TraceBufferLength != (ULONG)-1 &&
            TraceInfo->TraceBufferLength != AFSDbgBufferLength)
        {

            RtlInitUnicodeString( &uniString,
                                  AFS_REG_TRACE_BUFFER_LENGTH);

            ntStatus = AFSUpdateRegistryParameter( &uniString,
                                                   REG_DWORD,
                                                   &TraceInfo->TraceBufferLength,
                                                   sizeof( ULONG));

            if( !NT_SUCCESS( ntStatus))
            {

                DbgPrint("AFSConfigureTrace Failed to set debug buffer length in registry Status %08lX\n", ntStatus);
            }
        
            AFSDbgBufferLength = TraceInfo->TraceBufferLength * 1024;

            ClearFlag( AFSDbgLogFlags, AFS_DBG_LOG_WRAPPED);

            if( AFSDbgBuffer != NULL)
            {

                ExFreePool( AFSDbgBuffer);

                AFSDbgBuffer = NULL;

                AFSDbgCurrentBuffer = NULL;

                AFSDbgLogRemainingLength = 0;
            }

            if( AFSDbgBufferLength > 0)
            {

                AFSDbgBuffer = (char *)ExAllocatePoolWithTag( NonPagedPool,
                                                              AFSDbgBufferLength,
                                                              AFS_GENERIC_MEMORY_TAG);

                if( AFSDbgBuffer == NULL)
                {

                    AFSDbgBufferLength = 0;

                    try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
                }

                AFSDbgCurrentBuffer = AFSDbgBuffer;

                AFSDbgLogRemainingLength = AFSDbgBufferLength;

                AFSTagInitialLogEntry();
            }
        }

try_exit:

        AFSReleaseResource( &AFSDbgLogLock);
    }

    return ntStatus;
}

NTSTATUS
AFSGetTraceBuffer( IN ULONG TraceBufferLength,
                   OUT void *TraceBuffer,
                   OUT ULONG_PTR *CopiedLength)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulCopyLength = 0;
    char *pCurrentLocation = NULL;

    __Enter
    {

        AFSAcquireShared( &AFSDbgLogLock,
                          TRUE);

        if( TraceBufferLength < AFSDbgBufferLength)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //
        // If we have wrapped then copy in the remaining portion
        //

        pCurrentLocation = (char *)TraceBuffer;

        *CopiedLength = 0;

        if( BooleanFlagOn( AFSDbgLogFlags, AFS_DBG_LOG_WRAPPED))
        {

            ulCopyLength = AFSDbgLogRemainingLength;

            RtlCopyMemory( pCurrentLocation,
                           AFSDbgCurrentBuffer,
                           ulCopyLength);

            pCurrentLocation[ 0] = '0'; // The buffer is NULL terminated ...

            pCurrentLocation += ulCopyLength;

            *CopiedLength = ulCopyLength;
        }

        ulCopyLength = AFSDbgBufferLength - AFSDbgLogRemainingLength;

        if( ulCopyLength > 0)
        {

            RtlCopyMemory( pCurrentLocation,
                           AFSDbgBuffer,
                           ulCopyLength);

            *CopiedLength += ulCopyLength;
        }

try_exit:

        AFSReleaseResource( &AFSDbgLogLock);
    }

    return ntStatus;
}

void
AFSTagInitialLogEntry()
{

    LARGE_INTEGER liTime, liLocalTime;
    TIME_FIELDS timeFields;

    KeQuerySystemTime( &liTime);

    ExSystemTimeToLocalTime( &liTime,
                             &liLocalTime);

    RtlTimeToTimeFields( &liLocalTime,
                         &timeFields);

    AFSDbgLogMsg( 0,
                  0,
                  "AFS Log Initialized %d-%d-%d %d:%d Level %d Subsystems %08lX\n",
                  timeFields.Month,
                  timeFields.Day,
                  timeFields.Year,
                  timeFields.Hour,
                  timeFields.Minute,
                  AFSDebugLevel,
                  AFSTraceComponent);

    return;
}

