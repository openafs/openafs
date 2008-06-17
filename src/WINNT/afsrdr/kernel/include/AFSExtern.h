#ifndef _AFS_EXTERN_H
#define _AFS_EXTERN_H
//
// File: AFSExtern.h
//
//

extern "C" {

extern PDRIVER_OBJECT      AFSDriverObject;

extern PDEVICE_OBJECT      AFSDeviceObject;

extern PDEVICE_OBJECT      AFSRDRDeviceObject;

extern FAST_IO_DISPATCH    AFSFastIoDispatch;

extern unsigned long       AFSCRCTable[];

extern UNICODE_STRING      AFSRegistryPath;

extern ULONG               AFSDebugFlags;

extern ULONG               AFSDebugLevel;

extern CACHE_MANAGER_CALLBACKS AFSCacheManagerCallbacks;

extern PEPROCESS           AFSSysProcess;

extern HANDLE              AFSMUPHandle;

extern UNICODE_STRING      AFSServerName;

extern ERESOURCE           AFSProviderListLock;

extern AFSProviderConnectionCB   *AFSProviderConnectionList;

extern AFSFcb             *AFSAllRoot;

extern UNICODE_STRING      AFSPIOCtlName;

}

#endif /* _AFS_EXTERN_H */
