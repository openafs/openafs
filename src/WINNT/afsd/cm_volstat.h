/* Copyright 2007 Secure Endpoints Inc.
 *
 * BSD 2-part License 
 */

/* This header file provides the definitions and prototypes 
 * which specify the AFS Cache Manager Volume Status Event
 * Notification API
 */


extern long cm_VolStatus_Initialization(void);

extern long cm_VolStatus_Finalize(void);

extern long cm_VolStatus_Service_Started(void);

extern long cm_VolStatus_Service_Stopping(void);

#ifdef _WIN64
extern long cm_VolStatus_Network_Started(const char * netbios32, const char * netbios64);

extern long cm_VolStatus_Network_Stopped(const char * netbios32, const char * netbios64);
#else /* _WIN64 */
extern long cm_VolStatus_Network_Started(const char * netbios);

extern long cm_VolStatus_Network_Stopped(const char * netbios);
#endif /* _WIN64 */

extern long cm_VolStatus_Network_Addr_Change(void);

extern long cm_VolStatus_Change_Notification(afs_uint32 cellID, afs_uint32 volID, enum volstatus status);

extern long __fastcall cm_VolStatus_Path_To_ID(const char * share, const char * path, afs_uint32 * cellID, afs_uint32 * volID);

extern long __fastcall cm_VolStatus_Path_To_DFSlink(const char * share, const char * path, afs_uint32 *pBufSize, char *pBuffer);

#define DLL_VOLSTATUS_FUNCS_VERSION 1
typedef struct dll_VolStatus_Funcs {
    afs_uint32          version;
    long (__fastcall * dll_VolStatus_Service_Started)(void);
    long (__fastcall * dll_VolStatus_Service_Stopped)(void);
    long (__fastcall * dll_VolStatus_Network_Started)(const char *netbios32, const char *netbios64);
    long (__fastcall * dll_VolStatus_Network_Stopped)(const char *netbios32, const char *netbios64);
    long (__fastcall * dll_VolStatus_Network_Addr_Change)(void);
    long (__fastcall * dll_VolStatus_Change_Notification)(afs_uint32 cellID, afs_uint32 volID, enum volstatus status);
} dll_VolStatus_Funcs_t;

#define CM_VOLSTATUS_FUNCS_VERSION 1
typedef struct cm_VolStatus_Funcs {
    afs_uint32          version;
    long (__fastcall * cm_VolStatus_Path_To_ID)(const char * share, const char * path, afs_uint32 * cellID, afs_uint32 * volID);
    long (__fastcall * cm_VolStatus_Path_To_DFSlink)(const char * share, const char * path, afs_uint32 *pBufSize, char *pBuffer);
} cm_VolStatus_Funcs_t;

