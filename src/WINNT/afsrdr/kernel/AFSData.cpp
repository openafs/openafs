//
// File: AFSData.cpp
//

#define NO_EXTERN

#include "AFSCommon.h"

extern "C" {

PDRIVER_OBJECT      AFSDriverObject = NULL;

PDEVICE_OBJECT      AFSDeviceObject = NULL;

PDEVICE_OBJECT      AFSRDRDeviceObject = NULL;

FAST_IO_DISPATCH    AFSFastIoDispatch;

unsigned long       AFSCRCTable[ 256];

UNICODE_STRING      AFSRegistryPath;

ULONG               AFSDebugFlags = 0;

ULONG               AFSDebugLevel = 0;

CACHE_MANAGER_CALLBACKS AFSCacheManagerCallbacks;

PEPROCESS           AFSSysProcess = NULL;

HANDLE              AFSMUPHandle = NULL;

UNICODE_STRING      AFSServerName;

ERESOURCE           AFSProviderListLock;

AFSProviderConnectionCB   *AFSProviderConnectionList = NULL;

AFSFcb             *AFSAllRoot = NULL;

UNICODE_STRING      AFSPIOCtlName;

ULONG               AFSAllocationMemoryLevel = 0;

}
