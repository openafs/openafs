#ifndef _SCOMMON_H_
#define _SCOMMON_H_

enum states {BM_SETUP, BM_WARMUP, BM_MEASURE};

#define PSTRING_LEN 2048
#define FSTRING_LEN 512

#define MAX_HANDLES 128
#define MAX_FILES 1000
#define ssize_t SSIZE_T

#define aRONLY (1L<<0)		/* 0x01 */
#define aHIDDEN (1L<<1)		/* 0x02 */
#define aSYSTEM (1L<<2)		/* 0x04 */
#define aVOLID (1L<<3)		/* 0x08 */
#define aDIR (1L<<4)		/* 0x10 */
#define aARCH (1L<<5)		/* 0x20 */

#define FILE_DIRECTORY_FILE       0x0001
#define FILE_WRITE_THROUGH        0x0002
#define FILE_SEQUENTIAL_ONLY      0x0004
#define FILE_NON_DIRECTORY_FILE   0x0040
#define FILE_NO_EA_KNOWLEDGE      0x0200
#define FILE_EIGHT_DOT_THREE_ONLY 0x0400
#define FILE_RANDOM_ACCESS        0x0800
#define FILE_DELETE_ON_CLOSE      0x1000

#define CMD_CLOSE           0
#define CMD_DELTREE         1
#define CMD_FIND_FIRST      2
//#define CMD_FLUSH           3
//#define CMD_LOCKINGX        4
#define CMD_NTCREATEX       3
#define CMD_QUERY_FILE_INFO 4
#define CMD_QUERY_FS_INFO   5
#define CMD_QUERY_PATH_INFO 6
#define CMD_READX           7
#define CMD_RENAME          8
#define CMD_RMDIR           9
#define CMD_UNLINK          10
#define CMD_WRITEX          11
#define CMD_XCOPY           12 
#define CMD_DELETEFILES     13 
#define CMD_COPYFILES       14
#define CMD_ATTACH          15
#define CMD_DETACH          16
#define CMD_MKDIR           17
#define CMD_XRMDIR          18
#define CMD_SETLOCKER       19
#define CMD_CREATEFILE      20
#define CMD_MOVE            21
#define CMD_NONAFS          22
#define CMD_MAX_CMD         23 /* KEEP THIS UP TO DATE! */

typedef DWORD NTSTATUS;

typedef char pstring[PSTRING_LEN];
typedef char fstring[FSTRING_LEN];

typedef struct file_info
{
	DWORD mode;
	time_t mtime;
	time_t atime;
	time_t ctime;
	pstring name;
//	char short_name[13*3];
} file_info;

struct cmd_struct {
  int   count;
  int   ErrorCount;
  DWORD ErrorTime;
  DWORD MilliSeconds;
  DWORD min_sec;
  DWORD max_sec;
  DWORD total_sec;
  DWORD total_sum_of_squares;
};

static struct 
{
	char *name; /* name used in results */
	char *disable_name; /* name used in disable (-d) option */
	unsigned id; /* cmd id */
    char *ms_api;
} cmd_names[] = {
	{"Close", "CLOSE", CMD_CLOSE, "(CloseHandle)"},
	{"DelTree", "DELTREE", CMD_DELTREE, "(FindFirstFile/FindNextFile/DeleteFile/RemoveDirectory)"},
	{"Find First", "FIND_FIRST", CMD_FIND_FIRST, "(FindFirstFile/FindNextFile)"},
//	{"Flush", "FLUSH", CMD_FLUSH},
//	{"Locking & X", "LOCKINGX", CMD_LOCKINGX},
	{"NT Create & X", "NTCREATEX", CMD_NTCREATEX, "(CreateFile/CreateDirectory)"},
	{"Query File Info", "QUERY_FILE_INFO", CMD_QUERY_FILE_INFO, "(GetFileAttributesEx)"},
	{"Query File System Info", "QUERY_FS_INFO", CMD_QUERY_FS_INFO, "(GetDiskFreeSpaceEx)"},
	{"Query Path Info", "QUERY_PATH_INFO", CMD_QUERY_PATH_INFO, "(GetFileAttributesEx)"},
	{"Read & X", "READX", CMD_READX, "(SetFilePointer/ReadFile)"},
	{"Rename", "RENAME", CMD_RENAME, "(MoveFileEx)"},
	{"Rmdir", "RMDIR", CMD_RMDIR, "(RemoveDirectory)"},
	{"Unlink","UNLINK", CMD_UNLINK, "(DeleteFile)"},
	{"Write & X", "WRITEX", CMD_WRITEX, "(SetFilePointer/WriteFile/FlushFileBuffers)"},
    {"CopyEx", "XCOPY", CMD_XCOPY, "(DOS xcopy)"},
    {"DeleteFiles", "DELETEFILES", CMD_DELETEFILES, "(DOS del)"},
    {"CopyFiles", "COPYFILES", CMD_COPYFILES, "(DOS copy)"},
    {"Attach", "ATTACH", CMD_ATTACH, "(M.I.T.)"},
    {"Detach", "DETACH", CMD_DETACH, "(M.I.T.)"},
    {"Mkdir", "MKDIR", CMD_MKDIR, "(DOS mkdir)"},
    {"Xrmdir", "XRMDIR", CMD_XRMDIR, "(DOS rmdir)"},
    {"SetLocker", "SETLOCKER", CMD_SETLOCKER, "(M.I.T.)"},
    {"CreateFile", "CREATEFILE", CMD_CREATEFILE, "(CreateFile/SetFilePointer/WriteFile/FlushFileBuffers/CloseHandle)"},
    {"Move", "MOVE", CMD_MOVE, "(DOS move)"},
    {"NonAFS", "NonAFS", CMD_NONAFS, ""},
	{NULL, NULL, 0, NULL}
};

typedef struct {
    int     ExitStatus;
    char    Reason[1024];
} EXIT_STATUS;

typedef struct {
        BOOL                PrintStats;
    	BOOL                *child_status_out;
        int                 AfsTrace;
        int                 ProcessNumber;
        int                 BufferSize;
        int                 CurrentLoop;
        int                 ProcessID;
        int                 *pThreadStatus;
        int                 LogID;
        struct cmd_struct   *CommandInfo;
        char                *TargetDirectory;
        char                *CommandLine;
        char                *ClientText;
        char                *PathToSecondDir;
        char                *AfsLocker;
        char                *HostName;
        EXIT_STATUS         *pExitStatus;
        } PARAMETERLIST;

typedef struct {
	int fd;
	int handle;
    char *name; 
	int reads, writes;
} FTABLE;


#endif