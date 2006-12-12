#ifndef DOSDEFS_H
#define DOSDEFS_H

/* dos_ptr is the phys. addr. accepted by farpeek/farpoke functions, i.e.,
   dos_ptr = segment * 16 + offset */
#define dos_ptr unsigned long

/* get/set structure member of a struct in DOS memory */
#define get_dos_member_b(T, ptr, memb) _farpeekb(_dos_ds, (ptr) + (dos_ptr)&(((T*)0)->memb))
#define get_dos_member_w(T, ptr, memb) _farpeekw(_dos_ds, (ptr) + (dos_ptr)&(((T*)0)->memb))
#define get_dos_member_l(T, ptr, memb) _farpeekl(_dos_ds, (ptr) + (dos_ptr)&(((T*)0)->memb))

#define set_dos_member_b(T, ptr, memb, val) \
                  _farpokeb(_dos_ds, (ptr) + (dos_ptr)&(((T*)0)->memb), val)
#define set_dos_member_w(T, ptr, memb, val) \
                  _farpokew(_dos_ds, (ptr) + (dos_ptr)&(((T*)0)->memb), val)
#define set_dos_member_l(T, ptr, memb, val) \
                  _farpokel(_dos_ds, (ptr) + (dos_ptr)&(((T*)0)->memb), val)

typedef struct _filetime
{
  unsigned int dwLowDateTime;
  unsigned int dwHighDateTime;
} FILETIME;

#define FILE_ACTION_ADDED               0x00000001   
#define FILE_ACTION_REMOVED             0x00000002   
#define FILE_ACTION_MODIFIED            0x00000003   
#define FILE_ACTION_RENAMED_OLD_NAME    0x00000004   
#define FILE_ACTION_RENAMED_NEW_NAME    0x00000005   

#define FILE_NOTIFY_CHANGE_FILE_NAME    0x00000001   
#define FILE_NOTIFY_CHANGE_DIR_NAME     0x00000002   
#define FILE_NOTIFY_CHANGE_ATTRIBUTES   0x00000004   
#define FILE_NOTIFY_CHANGE_SIZE         0x00000008   
#define FILE_NOTIFY_CHANGE_LAST_WRITE   0x00000010   
#define FILE_NOTIFY_CHANGE_LAST_ACCESS  0x00000020   
#define FILE_NOTIFY_CHANGE_CREATION     0x00000040
#define FILE_NOTIFY_CHANGE_EA           0x00000080
#define FILE_NOTIFY_CHANGE_SECURITY     0x00000100   
#define FILE_NOTIFY_CHANGE_STREAM_NAME  0x00000200
#define FILE_NOTIFY_CHANGE_STREAM_SIZE  0x00000400
#define FILE_NOTIFY_CHANGE_STREAM_WRITE 0x00000800

#define ULONG unsigned long
#define USHORT unsigned short
#define WCHAR wchar_t

#define GetTickCount gettime_ms
#define GetCurrentTime gettime_ms

#define lstrcpy strcpy
#define strcmpi stricmp
#define lstrlen strlen
#define _stricmp stricmp
#define _strlwr strlwr
#define _strupr strupr

#endif
