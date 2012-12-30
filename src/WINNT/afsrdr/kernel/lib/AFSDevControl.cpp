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
// File: AFSDevControl.cpp
//

#include "AFSCommon.h"

//
// Function: AFSDevControl
//
// Description:
//
//      This is the dipatch handler for the IRP_MJ_DEVICE_CONTROL requests.
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSDevControl( IN PDEVICE_OBJECT LibDeviceObject,
               IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(LibDeviceObject);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp;
    ULONG               ulIoControlCode = 0;

    __try
    {

        pIrpSp = IoGetCurrentIrpStackLocation( Irp);

        ulIoControlCode = pIrpSp->Parameters.DeviceIoControl.IoControlCode;

        switch( ulIoControlCode)
        {

            case IOCTL_AFS_INITIALIZE_LIBRARY_DEVICE:
            {

                AFSLibraryInitCB *pLibInitCB = (AFSLibraryInitCB *)Irp->AssociatedIrp.SystemBuffer;

                if ( Irp->RequestorMode != KernelMode)
                {

                    ntStatus = STATUS_ACCESS_DENIED;

                    break;
                }

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSLibraryInitCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSInitializeLibrary( pLibInitCB);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY | AFS_SUBSYSTEM_INIT_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSDevControl AFSInitializeLibrary failure %08lX\n",
                                  ntStatus);

                    break;
                }

                //
                // Initialize our global entries
                //

                ntStatus = AFSInitializeGlobalDirectoryEntries();

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY | AFS_SUBSYSTEM_INIT_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSDevControl AFSInitializeGlobalDirectoryEntries failure %08lX\n",
                                  ntStatus);

                    break;
                }

                ntStatus = AFSInitializeSpecialShareNameList();

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY | AFS_SUBSYSTEM_INIT_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSDevControl AFSInitializeSpecialShareNameList failure %08lX\n",
                                  ntStatus);

                    break;
                }

                break;
            }

            case IOCTL_AFS_ADD_CONNECTION:
            {

                AFSNetworkProviderConnectionCB *pConnectCB = (AFSNetworkProviderConnectionCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSNetworkProviderConnectionCB) ||
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength < (ULONG)FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                                                                            pConnectCB->RemoteNameLength ||
                    pIrpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof( ULONG))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSAddConnection( pConnectCB,
                                             (PULONG)Irp->AssociatedIrp.SystemBuffer,
                                             &Irp->IoStatus.Information);

                break;
            }

            case IOCTL_AFS_CANCEL_CONNECTION:
            {

                AFSNetworkProviderConnectionCB *pConnectCB = (AFSNetworkProviderConnectionCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSNetworkProviderConnectionCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSCancelConnection( pConnectCB,
                                                (AFSCancelConnectionResultCB *)Irp->AssociatedIrp.SystemBuffer,
                                                &Irp->IoStatus.Information);

                break;
            }

            case IOCTL_AFS_GET_CONNECTION:
            {

                AFSNetworkProviderConnectionCB *pConnectCB = (AFSNetworkProviderConnectionCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSNetworkProviderConnectionCB) ||
                    pIrpSp->Parameters.DeviceIoControl.OutputBufferLength == 0)
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSGetConnection( pConnectCB,
                                             (WCHAR *)Irp->AssociatedIrp.SystemBuffer,
                                             pIrpSp->Parameters.DeviceIoControl.OutputBufferLength,
                                             &Irp->IoStatus.Information);

                break;
            }

            case IOCTL_AFS_LIST_CONNECTIONS:
            {

                if( pIrpSp->Parameters.DeviceIoControl.OutputBufferLength == 0)
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSListConnections( (AFSNetworkProviderConnectionCB *)Irp->AssociatedIrp.SystemBuffer,
                                                pIrpSp->Parameters.DeviceIoControl.OutputBufferLength,
                                                &Irp->IoStatus.Information);

                break;
            }

            case IOCTL_AFS_GET_CONNECTION_INFORMATION:
            {

                AFSNetworkProviderConnectionCB *pConnectCB = (AFSNetworkProviderConnectionCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSNetworkProviderConnectionCB) ||
                    pIrpSp->Parameters.DeviceIoControl.OutputBufferLength == 0)
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSGetConnectionInfo( pConnectCB,
                                                 pIrpSp->Parameters.DeviceIoControl.OutputBufferLength,
                                                 &Irp->IoStatus.Information);

                break;
            }

            case IOCTL_AFS_SET_FILE_EXTENTS:
            {

                AFSSetFileExtentsCB *pExtents = (AFSSetFileExtentsCB*) Irp->AssociatedIrp.SystemBuffer;

                //
                // Check lengths twice so that if the buffer makes the
                // count invalid we will not Accvio
                //

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength <
                    ( FIELD_OFFSET( AFSSetFileExtentsCB, ExtentCount) + sizeof(ULONG)) ||
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength <
                    ( FIELD_OFFSET( AFSSetFileExtentsCB, ExtentCount) + sizeof(ULONG) +
                      sizeof (AFSFileExtentCB) * pExtents->ExtentCount))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSProcessSetFileExtents( pExtents );

                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = ntStatus;

                break;
            }

            case IOCTL_AFS_RELEASE_FILE_EXTENTS:
            {
                ntStatus = AFSProcessReleaseFileExtents( Irp);
                break;
            }

            case IOCTL_AFS_SET_FILE_EXTENT_FAILURE:
            {

                ntStatus = AFSProcessExtentFailure( Irp);

                break;
            }

            case IOCTL_AFS_INVALIDATE_CACHE:
            {

                AFSInvalidateCacheCB *pInvalidate = (AFSInvalidateCacheCB*)Irp->AssociatedIrp.SystemBuffer;

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSInvalidateCacheCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSInvalidateCache( pInvalidate);

                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = ntStatus;

                break;
            }

            case IOCTL_AFS_NETWORK_STATUS:
            {

                AFSNetworkStatusCB *pNetworkStatus = (AFSNetworkStatusCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSNetworkStatusCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                //
                // Set the network status
                //

                ntStatus = AFSSetNetworkState( pNetworkStatus);

                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = ntStatus;

                break;
            }

            case IOCTL_AFS_VOLUME_STATUS:
            {

                AFSVolumeStatusCB *pVolumeStatus = (AFSVolumeStatusCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSVolumeStatusCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSSetVolumeState( pVolumeStatus);

                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = ntStatus;

                break;
            }

            case IOCTL_AFS_STATUS_REQUEST:
            {

                if( pIrpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof( AFSDriverStatusRespCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSGetDriverStatus( (AFSDriverStatusRespCB *)Irp->AssociatedIrp.SystemBuffer);

                Irp->IoStatus.Information = sizeof( AFSDriverStatusRespCB);

                break;
            }

            case IOCTL_AFS_GET_OBJECT_INFORMATION:
            {


                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSGetStatusInfoCB) ||
                    pIrpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof( AFSStatusInfoCB))

                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSGetObjectStatus( (AFSGetStatusInfoCB *)Irp->AssociatedIrp.SystemBuffer,
                                               pIrpSp->Parameters.DeviceIoControl.InputBufferLength,
                                               (AFSStatusInfoCB *)Irp->AssociatedIrp.SystemBuffer,
                                               (ULONG *)&Irp->IoStatus.Information);

                break;
            }

            case 0x140390:      // IOCTL_LMR_DISABLE_LOCAL_BUFFERING
            {
                //
                // See http://msdn.microsoft.com/en-us/library/ee210753%28v=vs.85%29.aspx
                //
                // The IOCTL_LMR_DISABLE_LOCAL_BUFFERING control code is defined internally by
                // the system as 0x140390 and not in a public header file. It is used by
                // special-purpose applications to disable local client-side in-memory
                // caching of data when reading data from or writing data to a remote file.
                // After local buffering is disabled, the setting remains in effect until all
                // open handles to the file are closed and the redirector cleans up its internal
                // data structures.
                //
                // General-purpose applications should not use IOCTL_LMR_DISABLE_LOCAL_BUFFERING,
                // because it can result in excessive network traffic and associated loss of
                // performance. The IOCTL_LMR_DISABLE_LOCAL_BUFFERING control code should be used
                // only in specialized applications moving large amounts of data over the network
                // while attempting to maximize use of network bandwidth. For example, the CopyFile
                // and CopyFileEx functions use IOCTL_LMR_DISABLE_LOCAL_BUFFERING to improve large
                // file copy performance.
                //
                // IOCTL_LMR_DISABLE_LOCAL_BUFFERING is not implemented by local file systems and
                // will fail with the error ERROR_INVALID_FUNCTION. Issuing the
                // IOCTL_LMR_DISABLE_LOCAL_BUFFERING control code on remote directory handles will
                // fail with the error ERROR_NOT_SUPPORTED.
                //

                ntStatus = STATUS_NOT_SUPPORTED;

                break;
            }

         default:
            {
                //
                // Note that this code path is never executed - default behavior is caught in the
                // security checks in lib.  New Ioctl functions therefore have to be added here and
                // in ..\fs\AFSCommSupport.cpp
                //

                ntStatus = STATUS_NOT_IMPLEMENTED;

                break;
            }
        }

//try_exit:

    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()))
    {

        ntStatus = STATUS_UNSUCCESSFUL;

        AFSDbgLogMsg( 0,
                      0,
                      "EXCEPTION - AFSDevControl %08lX\n",
                      ulIoControlCode);

        AFSDumpTraceFilesFnc();
    }

    Irp->IoStatus.Status = ntStatus;

    AFSCompleteRequest( Irp,
                        ntStatus);

    return ntStatus;
}
