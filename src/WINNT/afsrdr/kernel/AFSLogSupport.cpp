
#include "AFSCommon.h"

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
