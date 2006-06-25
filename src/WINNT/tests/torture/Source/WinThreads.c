#include <windows.h>
#include <includes.h>
#include "common.h"
#ifdef HAVE_HESOID
    #include "ResolveLocker.h"
    int ResolveLocker(USER_OPTIONS *attachOption);
#endif
#ifdef HAVE_AFS_SOURCE
    #include <smb_iocons.h>
    #include <afsint.h>
    #include <pioctl_nt.h>
#else
    #define VIOCGETVOLSTAT  0x7
    #define afs_int32   int

    struct VolumeStatus {
	    afs_int32 Vid;
	    afs_int32 ParentId;
	    char Online;
	    char InService;
	    char Blessed;
	    char NeedsSalvage;
	    afs_int32 Type;
	    afs_int32 MinQuota;
	    afs_int32 MaxQuota;
	    afs_int32 BlocksInUse;
	    afs_int32 PartBlocksAvail;
	    afs_int32 PartMaxBlocks;
    };

    typedef struct VolumeStatus VolumeStatus;

    typedef struct ViceIoctl {
	    long in_size;
            long out_size;
            void *in;
	    void *out;
    } viceIoctl_t;
#endif
long pioctl(char *pathp, long opcode, struct ViceIoctl *blob, int follow);

#define MAX_PARAMS 20
#define ival(s) strtol(s, NULL, 0)
#define AFSDLL "afsauthent.dll"

#define WINTORTURE_ASFDLL_ONLINE        1
#define WINTORTURE_ASFDLL_OFFLINE       2
#define WINTORTURE_ASFDLL_NOTFOUND      3
#define WINTORTURE_ASFPIOCTL_NOTFOUND   4

extern int verbose;

extern void EndSecondTime(int cmd);
extern void StartSecondTime(int cmd);
extern void LogMessage(int ProcessNumber, char *HostName, char *FileName, char *message, int LogID);
extern void LogStats(char *FileName, int ToLog, int Iteration,  int NumberOfProcesses, int NumberOfThreads,
                     char *HostName, int ProcessNumber, struct cmd_struct ThreadCommandInfo[],
                     char *CommandLine, char *TargetDirectory);
extern void SubstituteString(char *s,const char *pattern,const char *insert, size_t len);

int  IsOnline(char *strPath);
BOOL run_netbench(int client, char *ClientTxt, char *PathToSecondDir);

HANDLE  MutexHandle;
HANDLE  FileMutexHandle;
HANDLE  ShutDownEventHandle;
HANDLE  PauseEventHandle;
HANDLE  ContinueEventHandle;
HANDLE  OSMutexHandle;

__declspec( thread ) int    AfsTrace;
__declspec( thread ) int    CurrentLoop;
__declspec( thread ) int    ProcessNumber = 0;
__declspec( thread ) int    LogID = 0;
__declspec( thread ) int    LineCount = 0;
__declspec( thread ) int    *pThreadStatus;
__declspec( thread ) DWORD  TickCount1,TickCount2, MilliTickStart;
__declspec( thread ) char   *IoBuffer = NULL;
__declspec( thread ) char   AfsLocker[256];
__declspec( thread ) char   OriginalAfsLocker[256];
__declspec( thread ) char   HostName[256];
__declspec( thread ) struct cmd_struct ThreadCommandInfo[CMD_MAX_CMD + 1];
__declspec( thread ) FTABLE ftable[MAX_FILES];
__declspec( thread ) HANDLE hWinEventHandle;
__declspec( thread ) EXIT_STATUS *pExitStatus;
__declspec( thread ) DWORD  LastKnownError;


DWORD WINAPI StressTestThread(LPVOID lpThreadParameter)
{
    int         j;
    int         rc;
    int         count;
    int         BufferSize;
    int         ProcessID;
    char        EventName[64];
    char        FileName[256];
    char        CommandLine[512];
    char        TargetDirectory[512];
    char        WorkingDirectory[512];
    char        ClientText[128];
    char        PathToSecondDir[256];
    char        temp[256];
    BOOL        PrintStats;
    PARAMETERLIST *pParameterList;
    struct cmd_struct *WinCommandInfo;

    pParameterList = (PARAMETERLIST *)lpThreadParameter;
    pThreadStatus = pParameterList->pThreadStatus;
    BufferSize = pParameterList->BufferSize;
    ProcessNumber = pParameterList->ProcessNumber;
    PrintStats = pParameterList->PrintStats;
    CurrentLoop = pParameterList->CurrentLoop;
    ProcessID = pParameterList->ProcessID;
    LogID = pParameterList->LogID;
    AfsTrace = pParameterList->AfsTrace;
    pExitStatus = pParameterList->pExitStatus;
    strcpy(TargetDirectory, pParameterList->TargetDirectory);
    strcpy(CommandLine, pParameterList->CommandLine);
    strcpy(ClientText, pParameterList->ClientText);
    strcpy(PathToSecondDir, pParameterList->PathToSecondDir);
    strcpy(AfsLocker, pParameterList->AfsLocker);
    strcpy(HostName, pParameterList->HostName);
    WinCommandInfo = ( struct cmd_struct *)pParameterList->CommandInfo;

    sprintf(EventName, "%d%sEvent%05d", ProcessID, HostName, ProcessNumber);
    hWinEventHandle = OpenEvent(EVENT_ALL_ACCESS, TRUE, EventName);

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);
    GetCurrentDirectory(sizeof(WorkingDirectory), WorkingDirectory);
    sprintf(temp, "%s\\log%05d\\%s", WorkingDirectory, LogID, HostName);
    CreateDirectory(temp, NULL);


    memset(ftable, '\0', sizeof(ftable[0]) * MAX_FILES);
    IoBuffer = malloc(BufferSize);
    if (!IoBuffer)
    {
        strcpy(pExitStatus->Reason, "Unable to allocate buffer");
        pExitStatus->ExitStatus = 1;
        SetEvent(hWinEventHandle);
        ExitThread(1);
    }

    ShutDownEventHandle = CreateEvent(NULL, TRUE, FALSE, "AfsShutdownEvent");
    PauseEventHandle = CreateEvent(NULL, TRUE, FALSE, "AfsPauseEvent");
    ContinueEventHandle = CreateEvent(NULL, TRUE, FALSE, "AfsContinueEvent");
    OSMutexHandle = CreateMutex(NULL, FALSE, "WinTortureOSMutex");
    MutexHandle = CreateMutex(NULL, FALSE, "WinTortureMutex");

    strcpy(OriginalAfsLocker, AfsLocker);

    while (1)
    {
        LastKnownError = 0;
        for (j = 0; j <= CMD_MAX_CMD; j++)
            {
                WinCommandInfo[j].count = 0;
                WinCommandInfo[j].min_sec = 0;
                WinCommandInfo[j].max_sec = 0;
                WinCommandInfo[j].MilliSeconds = 0;
                WinCommandInfo[j].total_sec = 0;
                WinCommandInfo[j].total_sum_of_squares = 0;
                WinCommandInfo[j].ErrorCount = 0;
                WinCommandInfo[j].ErrorTime = 0;
                ThreadCommandInfo[j].count = 0;
                ThreadCommandInfo[j].min_sec = 1000;
                ThreadCommandInfo[j].max_sec = 0;
                ThreadCommandInfo[j].MilliSeconds = 0;
                ThreadCommandInfo[j].total_sec = 0;
                ThreadCommandInfo[j].total_sum_of_squares = 0;
                ThreadCommandInfo[j].ErrorCount = 0;
                ThreadCommandInfo[j].ErrorTime = 0;
            }

        run_netbench(ProcessNumber, ClientText, PathToSecondDir);
        if (LastKnownError != ERROR_NETNAME_DELETED)
            break;
        sprintf(temp, "entered error %d processing\n", LastKnownError);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);

        count = strlen(pExitStatus->Reason);
        if (count != 0)
            memset(pExitStatus->Reason, '\0', count);
        pExitStatus->ExitStatus = 0;
        (*pThreadStatus) = 1;
        count = 0;
        
        while ((rc = IsOnline(OriginalAfsLocker)) != WINTORTURE_ASFDLL_ONLINE)
        {
            if ((count > 3) || (rc == WINTORTURE_ASFDLL_NOTFOUND) || (rc == WINTORTURE_ASFPIOCTL_NOTFOUND))
            {
                LastKnownError = 0;
                strcpy(temp, "AFS client appears to be off-line\n");
                strcpy(pExitStatus->Reason, temp);
                pExitStatus->ExitStatus = 1;
                (*pThreadStatus) = 0;
                printf("%s", temp);
                LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
                strcpy(temp, "Stress test is terminating\n");
                printf(temp);
                LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
                break;
            }
            strcpy(temp, "AFS is online, sleeping 10 seconds before continuing\n");
            LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
            ++count;
            Sleep(10 * 1000);
        }
        sprintf(temp, "leaving error %d processing\n", LastKnownError);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        if (count > 3)
            break;
    }
    free(IoBuffer);

    for (j = 0; j <= CMD_MAX_CMD; j++)
    {
        WinCommandInfo[j].count += ThreadCommandInfo[j].count;
        WinCommandInfo[j].min_sec += ThreadCommandInfo[j].min_sec;
        WinCommandInfo[j].max_sec += ThreadCommandInfo[j].max_sec;
        WinCommandInfo[j].total_sec += ThreadCommandInfo[j].total_sec;
        WinCommandInfo[j].total_sum_of_squares += ThreadCommandInfo[j].total_sum_of_squares;
        WinCommandInfo[j].ErrorCount += ThreadCommandInfo[j].ErrorCount;
        WinCommandInfo[j].ErrorTime += ThreadCommandInfo[j].ErrorTime;
    }

    memset(WorkingDirectory, '\0', sizeof(WorkingDirectory));
    GetCurrentDirectory(sizeof(WorkingDirectory), WorkingDirectory);
    sprintf(FileName, "%s\\log%05d\\%s\\Thread_%05d_Stats.log", WorkingDirectory, LogID, HostName, ProcessNumber);

    if (PrintStats)
    {
        WaitForSingleObject(MutexHandle, 4 * 1000);
        LogStats(FileName, 0, CurrentLoop, 0, 0, HostName, ProcessNumber, ThreadCommandInfo, CommandLine, TargetDirectory);
        ReleaseMutex(MutexHandle);
    }

    LogStats(FileName, 1, CurrentLoop, 0, 0, HostName, ProcessNumber, ThreadCommandInfo,CommandLine, TargetDirectory);

    SetEvent(hWinEventHandle);
    CloseHandle(hWinEventHandle);
//    CloseHandle(OSMutexHandle);
//    CloseHandle(MutexHandle);

    ExitThread(0);
    return(0);
}

BOOL run_netbench(int client, char *ClientText, char *PathToSecondDir)
{
    pstring line;
    pstring line1;
    char    cname[20];
    char    *params[MAX_PARAMS];
    char    temp[256];
    char    FileName[256];
    char    *pPtr;
    int     rc;
    int     i;
    int     IncreaseBy;
    BOOL    correct = TRUE;
    DWORD   dwFlags = 0;
    DWORD   NumberOfBytes;
    DWORD   TotalBytesRead;
    HANDLE  hFile;
    enum    states bm_state;
    CRITICAL_SECTION CriticalSection;


    InitializeCriticalSection(&CriticalSection);

    sprintf(cname, "client%d", client);

    sprintf(temp, "Started Iteration %d\n", CurrentLoop);
    sprintf(FileName, "Thread_%05d.log", ProcessNumber);
    LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
    sprintf(temp, "Thread %d started\n", ProcessNumber);
    sprintf(FileName, "Thread_%05d.log", ProcessNumber);
    LogMessage(ProcessNumber, HostName, FileName, temp, LogID);

    hFile = CreateFile(ClientText, GENERIC_READ | STANDARD_RIGHTS_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL); 

    if (hFile == INVALID_HANDLE_VALUE) 
    {
        perror(ClientText);
        return(-1);
    }

    StartSecondTime(CMD_NONAFS);
    TotalBytesRead = 0;
    while (1)
    {
        memset(line, '\0', sizeof(line));
        NumberOfBytes = 0;
        rc = ReadFile(hFile, line, 128, &NumberOfBytes, NULL);
        if (rc && NumberOfBytes == 0)
            break;
        pPtr = strchr(line, '\n');
        IncreaseBy = 0;
        if (pPtr != NULL)
        {
            IncreaseBy += 1;
            (*pPtr) = '\0';
            if ((*(pPtr - 1)) == '\r')
            {
                IncreaseBy += 1;
                (*(pPtr - 1)) = '\0';
            }
        }
        TotalBytesRead += (strlen(line) + IncreaseBy);
        SetFilePointer(hFile, TotalBytesRead, 0, FILE_BEGIN);
        strcpy(line1, line);
        if (rc = WaitForSingleObject(PauseEventHandle, 0) == WAIT_OBJECT_0)
        {
            strcpy(temp, "AFS suspend request received\n");
            sprintf(FileName, "Thread_%05d.log", ProcessNumber);
            LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
            while (WaitForSingleObject(ContinueEventHandle, 5000) == WAIT_TIMEOUT);
            strcpy(temp, "AFS continue request received\n");
            sprintf(FileName, "Thread_%05d.log", ProcessNumber);
            LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        }
        if (rc = WaitForSingleObject(ShutDownEventHandle, 0) == WAIT_OBJECT_0)
        {
            strcpy(temp, "AFS shutdown request received\n");
            sprintf(FileName, "Thread_%05d.log", ProcessNumber);
            LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
            break;
        }
        LineCount++;
        if (strlen(line) == 0)
            continue;
        if (line[0] == '#')
            continue;
        /*printf("[%d] %s\n", LineCount, line);*/

        for (i = 0; i < MAX_PARAMS; i++)
            params[i] = NULL;

        SubstituteString(line,"client1", cname, sizeof(line));
        sprintf(temp, "%s%05d", HostName, LogID);
        SubstituteString(line,"clients", temp, sizeof(line));
        SubstituteString(line,"\\\\afs\\locker", AfsLocker, sizeof(line));
        if (strlen(PathToSecondDir) != 0)
    		SubstituteString(line,"\\\\afs\\lcolby", PathToSecondDir, sizeof(line));

        if (verbose)
        {
            EnterCriticalSection(&CriticalSection);
            printf("Thread%05d - %-6d - %s\n", ProcessNumber, LineCount, line);
            LeaveCriticalSection(&CriticalSection);
        }

        pPtr = line;
        i = 0;
        while (pPtr != NULL)
        {
            if ((*pPtr) == ' ')
            {
                (*pPtr) = '\0';
                ++pPtr;
                continue;
            }
            params[i] = pPtr;
            ++i;
            pPtr = strstr(pPtr, " ");
            if (pPtr != NULL)
            {
                (*pPtr) = '\0';
                ++pPtr;
            }
            
        }

        params[i] = "";

        if (i < 1) 
            continue;

        if (!strncmp(params[0],"SMB", 3)) 
        {
            printf("ERROR: You are using a dbench 1 load file\n");
            if (GetHandleInformation((HANDLE)hWinEventHandle, &dwFlags))
                break;
		}
        if (!strcmp(params[0], "BM_SETUP"))
        {
            bm_state = BM_SETUP;
        } 
        else if (!strcmp(params[0], "BM_WARMUP"))
        {
            bm_state = BM_WARMUP;
        } 
        else if (!strcmp(params[0], "BM_MEASURE"))
        {
            bm_state = BM_MEASURE;
            if (verbose)
                fprintf(stderr, "setting state to BM_MEASURE\n");
        } 
        else if (!strcmp(params[0],"RECONNECT")) 
        {
            if (verbose)
                fprintf(stderr, "Reconnecting ...\n");
        } 
        else if (!strcmp(params[0], "SYNC"))
        {
            int length = atoi(params[1]), st = 0;
            if (verbose)
                fprintf(stderr, "Syncing for %d seconds\n", length);
        } 
        else if (!strcmp(params[0],"NTCreateX")) 
        {
            if (nb_createx(params[1], ival(params[2]), ival(params[3]), ival(params[4])) == -1)
                break;
        } 
        else if (!stricmp(params[0],"SetLocker"))
        {
            if (nb_SetLocker(params[1]) == -1)
                break;
        } 
        else if (!stricmp(params[0],"Xrmdir")) 
        {
            if (nb_Xrmdir(params[1], params[2]) == -1)
                break;
        } 
        else if (!stricmp(params[0],"Mkdir")) 
        {
            if (nb_Mkdir(params[1]) == -1)
                break;
        } 
        else if (!stricmp(params[0],"Attach")) 
        {
            if (nb_Attach(params[1], params[2]) == -1)
                break;
        } 
        else if (!stricmp(params[0],"Detach")) 
        {
            if (nb_Detach(params[1], params[2]) == -1)
                break;
        } 
        else if (!stricmp(params[0],"CreateFile")) 
        {
            if (nb_CreateFile(params[1], atol(params[2])) == -1)
                break;
        } 
        else if (!stricmp(params[0],"CopyFiles")) 
        {
            if (nb_CopyFile(params[1], params[2]) == -1)
                break;
        } 
        else if (!stricmp(params[0],"DeleteFiles")) 
        {
            if (nb_DeleteFile(params[1]) == -1)
                break;
        } 
        else if (!stricmp(params[0],"Move")) 
        {
            if (nb_Move(params[1], params[2]) == -1)
                break;
        } 
        else if (!stricmp(params[0],"Xcopy")) 
        {
            if (nb_xcopy(params[1], params[2]) == -1)
                break;
        } 
        else if (!strcmp(params[0],"Close")) 
        {
            if (nb_close(ival(params[1])) == -1)
                break;
        } 
        else if (!strcmp(params[0],"Rename")) 
        {
            if (nb_rename(params[1], params[2]) == -1)
                break;
        } 
        else if (!strcmp(params[0],"Unlink")) 
        {
            if (nb_unlink(params[1]) == -1)
                break;
        } 
        else if (!strcmp(params[0],"Deltree"))
        {
            if (nb_deltree(params[1]) == -1)
                break;
        } 
        else if (!strcmp(params[0],"Rmdir")) 
        {
            if (nb_rmdir(params[1]) == -1)
                break;
        } 
        else if (!strcmp(params[0],"QUERY_PATH_INFORMATION"))
        {
            if (nb_qpathinfo(params[1], ival(params[2])) == -1)
                break;
        } 
        else if (!strcmp(params[0],"QUERY_FILE_INFORMATION")) 
        {
            if (nb_qfileinfo(ival(params[1])) == -1)
                break;
        } 
        else if (!strcmp(params[0],"QUERY_FS_INFORMATION")) 
        {
            if (nb_qfsinfo(ival(params[1])) == -1)
                break;
        } 
        else if (!strcmp(params[0],"FIND_FIRST")) 
        {
            if (nb_findfirst(params[1]) == -1)
                break;
        } 
        else if (!strcmp(params[0],"WriteX")) 
        {
            if (nb_writex(ival(params[1]), ival(params[2]), ival(params[3]), ival(params[4])) == -1)
                break;
        } 
        else if (!strcmp(params[0],"ReadX")) 
        {
            if (nb_readx(ival(params[1]), ival(params[2]), ival(params[3]), ival(params[4])) == -1)
                break;
        } 
        else if (!strcmp(params[0],"Flush")) 
        {
            if (nb_flush(ival(params[1])) == -1)
                break;
        } 
        else if (!strcmp(params[0],"LockingX")) 
        {
            if (nb_lock(ival(params[1]), ival(params[2]), ival(params[3]), ival(params[4]),
                        (unsigned char)ival(params[5]), ival(params[6])) == -1)
                break;
        } 
        else 
        {
            printf("Unknown operation %s\n", params[0]);
            printf("Line read = %s\n", line1);
            break;
        }
    }
    CloseHandle(hFile);

//    nb_cleanup(cname);

    EndSecondTime(CMD_NONAFS);

    for (i = 0; i < MAX_FILES; i++) 
    {
        if (ftable[i].handle > 0)
            nb_close(ftable[i].handle);
    }

	DeleteCriticalSection(&CriticalSection);

    return(correct);
}

typedef long ( __cdecl  *PPIOCTL)(char *pathp, long opcode, struct ViceIoctl *blobp, int follow);

int IsOnline(char *strPath)
{
    int     bret;
    char    space[2048];
    int     code;
    int     rc;
    struct  ViceIoctl blob;
    struct  VolumeStatus *status;
    PPIOCTL ppioctl;
    HINSTANCE hAfsDll;

    rc = WaitForSingleObject(OSMutexHandle, 5 * 1000);
    bret = FALSE;
    hAfsDll = NULL;
    hAfsDll = LoadLibrary(AFSDLL);
    if (hAfsDll)
    {
        ppioctl = NULL;
        ppioctl = (PPIOCTL)GetProcAddress(hAfsDll, "pioctl");
        if (ppioctl != NULL)
        {
            blob.in_size = 0;
            blob.out_size = sizeof(space);
            blob.out = space;
            if (!(code = ppioctl(strPath, VIOCGETVOLSTAT, &blob, 1)))
            {
                bret = WINTORTURE_ASFDLL_ONLINE;
                status = (VolumeStatus *)space;
                if (!status->Online || !status->InService || !status->Blessed || status->NeedsSalvage)
                    bret = WINTORTURE_ASFDLL_OFFLINE;
            }
        }
        else
            bret = WINTORTURE_ASFPIOCTL_NOTFOUND;

        FreeLibrary(hAfsDll);
        hAfsDll = NULL;
    }
    else
        bret = WINTORTURE_ASFDLL_NOTFOUND;

    if (rc == WAIT_OBJECT_0)
        ReleaseMutex(OSMutexHandle);
    return(bret);
}

