//#define _WIN32_WINNT 0x0500
//#include <windows.h>

#include "includes.h"
#include <Psapi.h>
#include "common.h"
#ifdef HAVE_HESOID
    #include "ResolveLocker.h"
    extern int ResolveLocker(USER_OPTIONS *attachOption);
    extern void GetLockerInfo(char *Locker, char *Path);
#endif /* HAVE_HESOID */

#define MAX_THREADS 100

int opterr = 1;
int optind = 1;
int optopt;
int optreset;
char *optarg;

extern void LogStats(char *FileName, int ToLog, int Iteration,  int NumberOfProcesses, int NumberOfThreads, 
                     char *HostName, int ProcessNumber, struct cmd_struct CommandInfo[],
                     char *CommandLine, char *TargetDirectory);
extern int UpdateMasterLog(char *FileName, struct cmd_struct CommandInfo[]);
extern int BuildMasterStatLog(char *FileName, char *MoveFileName, int NumberOfProcesses, 
                              int NumberOfThreads, char *CommandLine, int LoopCount, 
                              char *TargetDirectory, int ProcessNumber);
extern void LogMessage(int ProcessNumber, char *HostName, char *FileName, char *message, int LogID);

DWORD WINAPI StressTestThread(LPVOID lpThreadParameter);
int   getopt(int, char**, char*);
DWORD FindProcessCount(char *ProcessName, HANDLE JobHandle);
void  show_results(char *CommandLine, char *TargetDirectory, struct cmd_struct CommandInfo[], 
                   char *HostName, int NumberOfThreads, int CurrentLoop, int LogID);

char    *ClientText = "streamfiles.txt";
char    PathToSecondDir[256];
int     ThreadStatus[MAX_HANDLES];
int     verbose;
int     BufferSize = 256*1024;
int     UseLocker = 0;
//int     UseUnicode = 0;
int     EndOnError;
int     AfsTrace;
int     ChronLog;
BOOL    PrintStats;
HANDLE  MutexHandle;
HANDLE  FileMutexHandle;
HANDLE  ChronMutexHandle;
HANDLE  OSMutexHandle;
HANDLE  ShutDownEventHandle;
HANDLE  PauseEventHandle;
HANDLE  ContinueEventHandle;
EXIT_STATUS ExitStatus[MAX_HANDLES];

double create_procs(char *Hostname, char *CommandLine, char *TargetDirectory, 
                    char *AfsLocker, char *Locker, char *HostName, 
                    int NumberOfThreads, int CurrentLoop, int LogID)
{
    int     i;
    int     status;
    int     count;
    int     ProcessID;
    char    EventName[512];
    HANDLE  hEventHandle[MAX_HANDLES];
    HANDLE  hThreadHandle[MAX_HANDLES];
    DWORD   dwThreadID[MAX_HANDLES];
    struct cmd_struct *CommandInfo;
    CRITICAL_SECTION CriticalSection;
    PARAMETERLIST    *pParameterList[MAX_HANDLES];
#ifdef HAVE_HESOID
    USER_OPTIONS     attachOption;
#endif

    InitializeCriticalSection(&CriticalSection); 
    for (i = 0; i < MAX_HANDLES; i++)
    {
        hEventHandle[i] = NULL;
        hThreadHandle[i] = NULL;
        pParameterList[i] = NULL;
        dwThreadID[i] = 0;
    }

    count = 0;
    CommandInfo = calloc(1, sizeof(struct cmd_struct) * (CMD_MAX_CMD + 1) * NumberOfThreads);
    ProcessID = getpid();

    for (i = 0; i < NumberOfThreads; i++) 
    {
        if (EndOnError)
        {
            if (ThreadStatus[count] == 0)
                continue;
        }
        sprintf(EventName, "%d%sEvent%05d", ProcessID, HostName, count);
        hEventHandle[count] = CreateEvent(NULL, FALSE, FALSE, EventName);
        if (hEventHandle[count] == NULL)
            continue;
        ResetEvent(hEventHandle[count]);
        
        pParameterList[count] = calloc(1, sizeof(PARAMETERLIST));
        pParameterList[count]->ProcessNumber = count;
        pParameterList[count]->CommandInfo = (struct cmd_struct *)(CommandInfo + (i * (CMD_MAX_CMD + 1)));
        pParameterList[count]->BufferSize = BufferSize;
        pParameterList[count]->PrintStats = PrintStats;
        pParameterList[count]->CurrentLoop = CurrentLoop;
        pParameterList[count]->AfsTrace = AfsTrace;
        pParameterList[count]->TargetDirectory = TargetDirectory;
        pParameterList[count]->pExitStatus = &ExitStatus[i];
        pParameterList[count]->pThreadStatus = &ThreadStatus[i];
        pParameterList[count]->CommandLine = CommandLine;
        pParameterList[count]->ClientText = ClientText;
        pParameterList[count]->PathToSecondDir = PathToSecondDir;
        pParameterList[count]->AfsLocker = AfsLocker;
        pParameterList[count]->HostName = HostName;
        pParameterList[count]->ProcessID = ProcessID;;
        pParameterList[count]->LogID = LogID;;

        ThreadStatus[count] = 0;
        hThreadHandle[count] = CreateThread(NULL, 0, &StressTestThread, (LPVOID)pParameterList[count], CREATE_SUSPENDED, &dwThreadID[count]);
        if (hThreadHandle[count] != NULL)
        {
            ResumeThread(hThreadHandle[count]);
            ThreadStatus[count] = 1;
            ++count;
        }
        else
        {
            CloseHandle(hEventHandle[count]);
            if (pParameterList[count] != NULL)
                free(pParameterList[count]);
            pParameterList[count] = NULL;
            hThreadHandle[count] = NULL;
            hEventHandle[count] = NULL;
        }
    }

    count = 0;
    for (i = 0; i < MAX_HANDLES; i++)
    {
        if (hEventHandle[i] != NULL)
            ++count;
    }
#ifdef HAVE_HESOID
    if (UseLocker)
    {
        int          rc;

        memset(&attachOption, '\0', sizeof(attachOption));
        strcpy(attachOption.Locker, Locker);
        strcpy(attachOption.type, "locker");
        if (rc = ResolveLocker(&attachOption))
        {
            if (!stricmp(attachOption.type, "AFS"))
            {
                printf("Unable to attach locker %s - AFS is not supported\n", Locker);
                exit(1);
            }
            strcpy(AfsLocker, attachOption.SubMount);
            memset(&attachOption, '\0', sizeof(attachOption));
            strcpy(attachOption.Locker, Locker);
            strcpy(attachOption.type, "locker");
            if (rc = attach(attachOption, 0, 0, Locker))
            {
                printf("Unable to attach locker %s\n", Locker);
                exit(1);
            }
        }
    }
#endif /* HAVE_HESOID */

    status = WaitForMultipleObjects(count, hEventHandle, TRUE, INFINITE);
    for (i = 0; i < MAX_HANDLES; i++)
    {
        if (hEventHandle[i] != NULL)
            CloseHandle(hEventHandle[i]);
        if (pParameterList[i] != NULL)
            free(pParameterList[i]);
        pParameterList[i] = NULL;
        hEventHandle[i] = NULL;
    }

    for (i = 0; i < NumberOfThreads; i++)
    {
        char    FileName[128];
        char    temp[512];

        if (strlen(ExitStatus[i].Reason))
        {
            sprintf(temp, "Thread %0d exited with reason: %s", i, ExitStatus[i].Reason);
        }
        else
        {
            sprintf(temp, "Thread %0d completed\n", i);
        }
        if (verbose)
            printf("%s", temp);
        sprintf(FileName, "Thread_%05d.log", i);
        LogMessage(i, HostName, FileName, temp, LogID);
        sprintf(temp, "Ended Iteration %0d\n\n", CurrentLoop);
        LogMessage(i, HostName, FileName, temp, LogID);
        CloseHandle(hEventHandle[i]);
    }
    show_results(CommandLine, TargetDirectory, CommandInfo, HostName, NumberOfThreads, CurrentLoop, LogID);
    free(CommandInfo);
    DeleteCriticalSection(&CriticalSection);

    return(0);
}

static void usage(void)
{

    fprintf(stderr, "usage: wintorture [options]\n");
    fprintf(stderr, "where options can be:\n");
    fprintf(stderr, "\t-b        Create a chronological log.\n");
    fprintf(stderr, "\t-c <txt>  Specifies the script txt file to use.\n");
    fprintf(stderr, "\t-e        End thread processing on an error.\n");
    fprintf(stderr, "\t-f <name> Target directory name.\n");
    fprintf(stderr, "\t-i <num>  Number of iterations of the stress test to run.\n");
    fprintf(stderr, "\t            This option will override the -m option.\n");
#ifdef HAVE_HESOID
    fprintf(stderr, "\t-l <path> AFS locker or AFS submount in which to create the target directory.\n");
#endif /* HAVE_HESOID */
    fprintf(stderr, "\t-m <num>  The number of minutes to run the stress test.\n");
    fprintf(stderr, "\t            This option will override the -i option.\n");
    fprintf(stderr, "\t-n <num>  The number of threads to run.\n");
    fprintf(stderr, "\t-p <path> UNC path to second directory.\n");
    fprintf(stderr, "\t-s        Output stats.\n");
    fprintf(stderr, "\t-t        Do AFS trace logging.\n");
    fprintf(stderr, "\t-u <UNC>  UNC path to target directory.\n");
    fprintf(stderr, "\t-v        Turn on verbose mode.\n");
    fprintf(stderr, "\nNOTE: The switches are not case sensitive.  You\n");
    fprintf(stderr, "\n      may use either upper or lower case letters.\n\n");
}

void show_results(char *CommandLine, char *TargetDirectory, struct cmd_struct *CommandInfo, 
                  char *HostName, int NumberOfThreads, int CurrentLoop, int LogID)
{
	struct cmd_struct TotalCommandInfo[CMD_MAX_CMD + 1];
	int         i;
    int         j;
	unsigned    grand_total = 0;
    char        FileName[256];
    char        WorkingDirectory[512];
    struct cmd_struct *FinalCmdInfo;

	for (j = 0; j <= CMD_MAX_CMD; j++) {
		TotalCommandInfo[j].count = 0;
		TotalCommandInfo[j].min_sec = 0;
		TotalCommandInfo[j].max_sec = 0;
		TotalCommandInfo[j].MilliSeconds = 0;
		TotalCommandInfo[j].total_sec = 0;
		TotalCommandInfo[j].total_sum_of_squares = 0;
		TotalCommandInfo[j].ErrorCount = 0;
		TotalCommandInfo[j].ErrorTime = 0;
	}

    memset(ExitStatus, '\0', sizeof(ExitStatus[0]) * MAX_HANDLES);

	for (j = 0; j < NumberOfThreads; j++) 
    { 
	    FinalCmdInfo = CommandInfo + (j * (CMD_MAX_CMD + 1));

		for (i = 0; i <= CMD_MAX_CMD; i++)
        {
		    TotalCommandInfo[i].count += FinalCmdInfo[i].count;
            TotalCommandInfo[i].total_sec += FinalCmdInfo[i].total_sec; 
            TotalCommandInfo[i].total_sum_of_squares += FinalCmdInfo[i].total_sum_of_squares; 
            TotalCommandInfo[i].ErrorCount += FinalCmdInfo[i].ErrorCount; 
            TotalCommandInfo[i].ErrorTime += FinalCmdInfo[i].ErrorTime; 
            grand_total += FinalCmdInfo[j].total_sec;
            if (!TotalCommandInfo[i].min_sec || (TotalCommandInfo[i].min_sec > FinalCmdInfo[i].min_sec))
                TotalCommandInfo[i].min_sec = FinalCmdInfo[i].min_sec;
            if (TotalCommandInfo[i].max_sec < FinalCmdInfo[i].max_sec)
                TotalCommandInfo[i].max_sec = FinalCmdInfo[i].max_sec;
        }
	}


    memset(WorkingDirectory, '\0', sizeof(WorkingDirectory));
    GetCurrentDirectory(sizeof(WorkingDirectory), WorkingDirectory);
    sprintf(FileName, "%s\\log%05d\\%s\\ProcessStats.log", WorkingDirectory, LogID, HostName);

    if (PrintStats)
        LogStats(FileName, 0, CurrentLoop, 1, NumberOfThreads, HostName, -1, TotalCommandInfo, 
                 CommandLine, TargetDirectory);
    LogStats(FileName, 1, CurrentLoop, 1, NumberOfThreads, HostName, -1, TotalCommandInfo, 
             CommandLine, TargetDirectory);

    sprintf(FileName, "%s\\log%05d\\%s", WorkingDirectory, LogID, "MasterStatLog.log");
    UpdateMasterLog(FileName, TotalCommandInfo);
    sprintf(FileName, "%s\\log%05d\\%s\\%s", WorkingDirectory, LogID, HostName, "MasterProcessStatLog.log");
    UpdateMasterLog(FileName, TotalCommandInfo);
}


int main(int argc, char *argv[])
{
    int         i;
    int         LoopCount;
    int         SecondsToRun;
    int         NumberOfIterations;
    int         StressTestUsed = 0;
    int         rc;
    int         opt;
    int         ErrorState;
    int         NumberOfProcesses;
    int         NumberOfThreads;
    int         IterationCount;
    int         CurrentLoop = 0;
    int         LogID;
    time_t      TotalTime;
    time_t      StartTime;
    time_t      EndTime;
    extern char *optarg;
    extern int  optind;
    FILE        *Mfp;
    FILE        *fp;
    char        *p;
    char        tbuffer[10];
    char        buffer[512];
    char        FileName[256];
    char        MoveFileName[256];
    char        Locker[64];
    char        command[512];
    char        DateTime[512];
    char        WorkingDirectory[512];
    char        CommandLine[512];
    char        TargetDirectory[512];
    char        AfsLocker[256];
    char        HostName[128];
    char        JobName[128];
    SYSTEMTIME  SystemTime;
    SYSTEMTIME  LocalTime;
    TIME_ZONE_INFORMATION TimeZoneInformation;
    HANDLE      ExitMutexHandle;
    HANDLE      JobHandle;


    memset(HostName, '\0', sizeof(HostName));
    memset(PathToSecondDir, '\0', sizeof(PathToSecondDir));
    memset(Locker, '\0', sizeof(Locker));
    SecondsToRun = 0;
    NumberOfIterations = 0;
    EndOnError = 0;
    UseLocker = 0;
    ChronLog = 0;
    AfsTrace = 0;
    verbose = 0;
    NumberOfThreads = 1;
    NumberOfProcesses = 1;
    NumberOfThreads = 1;
    LogID = 0;
    PrintStats = FALSE;

    while ((opt = getopt(argc, argv, "A:a:BbC:c:D:d:EeF:f:G:g:I:i:L:l:M:m:N:n:P:p:SsTtU:u:Vv")) != EOF)
    {

        switch (opt)
        {
            case 'a':
            case 'A':
                break;
            case 'b':
            case 'B':
                ChronLog = 1;
                break;
            case 'c':
            case 'C':
                ClientText = optarg;
                break;
            case 'd':
            case 'D':
                StressTestUsed = 1;
                NumberOfProcesses = atoi(optarg);
                break;
            case 'e':
            case 'E':
                EndOnError = 1;
                break;
            case 'f':
            case 'F':
                strcpy(HostName, optarg);
                for (i = 0; i < (int)strlen(HostName); i++)
                {
                    if ((HostName[i] == '\\') || (HostName[i] == '/'))
                    {
                        printf("\nInvalid -F usage...Subdirectories not allowed\n\n");
                        usage();
                        exit(1);
                    }
                }
                break;
            case 'g':
            case 'G':
                StressTestUsed = 1;
                LogID = atoi(optarg);
                break;
            case 'i':
            case 'I':
                SecondsToRun = 0;
                NumberOfIterations = atoi(optarg);
                if (NumberOfIterations < 0)
                    NumberOfIterations = 0;
                break;
#ifdef HAVE_HESOID
            case 'l':
            case 'L':
                strcpy(Locker, optarg);
                UseLocker = 1;
                break;
#endif /* HAVE_HESOID */
            case 'm':
            case 'M':
                NumberOfIterations = 0;
                SecondsToRun = atoi(optarg) * 60;
                if (SecondsToRun < 0)
                    SecondsToRun = 0;
                break;
            case 'n':
            case 'N':
                NumberOfThreads = atoi(optarg);
                break;
            case 'p':
            case 'P':
                strcpy(PathToSecondDir, optarg);
                for(p = PathToSecondDir; *p; p++)
                {
                    if(*p == '/')
                        *p = '\\';
                }
                break;
            case 's':
            case 'S':
                PrintStats = TRUE;
                break;
            case 't':
            case 'T':
                AfsTrace = 1;
                break;
            case 'u':
            case 'U':
                UseLocker = 0;
                strcpy(Locker, optarg);
                break;
            case 'v':
            case 'V':
                verbose = 1;
                break;
            default:
                usage();
                exit(1);
                break;
        }
    }

    if (strlen(HostName) == 0)
    {
        printf("You must use the -f option to specify a target directory\n\n");
        usage();
        exit(1);
    }
    if (strlen(Locker) == 0)
    {
        printf("You must use either the -u or the -l option\n\n");
        usage();
        exit(1);
    }

    memset(CommandLine, '\0', sizeof(CommandLine));
    for (i = 1; i < argc; i++)
    {
        if (StressTestUsed)
        {
            if (!stricmp(argv[i], "-f"))
            {
                char temp[64];

                strcpy(temp, argv[i + 1]);
                temp[strlen(temp) - 5] = '\0';
                strcat(CommandLine, argv[i]);
                strcat(CommandLine, " ");
                strcat(CommandLine, temp);
                strcat(CommandLine, " ");
                ++i;
                continue;
            }
        }
        strcat(CommandLine, argv[i]);
        strcat(CommandLine, " ");
    }

    argc -= optind;
    argv += optind;

    if (strlen(Locker) == 0)
    {
        usage();
        exit(1);
    }


    for(p = Locker; *p; p++)
    {
        if(*p == '/')
            *p = '\\';
    }
 
#ifdef HAVE_HESOID
    if (UseLocker)
    {
        char *sPtr;

        sprintf(AfsLocker, "\\\\afs\\%s", Locker);
        memset(buffer, '\0', sizeof(buffer));
        GetLockerInfo(Locker, buffer);
        if (strlen(buffer) != 0)
        {
            sPtr = strstr(buffer, "/afs/");
            sPtr += strlen("/afs/");
            strcpy(TargetDirectory, sPtr);
            sPtr = strchr(TargetDirectory, ' ');
            if (sPtr != NULL)
                (*sPtr) = '\0';
            while ((sPtr = strchr(TargetDirectory, '/')) != NULL)
                (*sPtr) = '\\';
        }
        else
        {
            strcpy(TargetDirectory, Locker);
        }
    }
    else
    {
#endif /* HAVE_HESOID */
        strcpy(AfsLocker, Locker);
        if (!strnicmp(Locker, "\\\\afs\\", strlen("\\\\afs\\")))
            strcpy(TargetDirectory, &Locker[strlen("\\\\afs\\")]);
        else
            strcpy(TargetDirectory, Locker);
#ifdef HAVE_HESOID
    }
#endif /* HAVE_HESOID */


    TotalTime = 0;
    LoopCount = 0;
    CurrentLoop = 0;

    ExitMutexHandle = CreateMutex(NULL, FALSE, "AfsExitEvent");
    ChronMutexHandle = CreateMutex(NULL, FALSE, "WinTortureChronMutex");
    MutexHandle = CreateMutex(NULL, FALSE, "WinTortureMutex");
    FileMutexHandle = CreateMutex(NULL, FALSE, "WinTortureFileMutex");
    OSMutexHandle = CreateMutex(NULL, FALSE, "WinTortureOSMutex");

    for (i = 0; i < MAX_THREADS; i++)
    {
        ThreadStatus[i] = 2;
    }

    sprintf(JobName, "%s%05d", "JOB", LogID);
    JobHandle = CreateJobObject(NULL, JobName);
    rc = AssignProcessToJobObject(JobHandle, GetCurrentProcess());

    GetCurrentDirectory(sizeof(WorkingDirectory), WorkingDirectory);
    sprintf(FileName, "%s\\log%05d", WorkingDirectory, LogID);
    CreateDirectory(FileName, NULL);
    sprintf(FileName, "%s\\test", WorkingDirectory);
    CreateDirectory(FileName, NULL);
    if (!StressTestUsed)
    {
        sprintf(FileName, "%s\\log%05d\\Chron.log", WorkingDirectory, LogID);
        DeleteFile(FileName);
    }

    sprintf(FileName, "%s\\log%05d\\%s", WorkingDirectory, LogID, HostName);
    sprintf(command, "rmdir /S /Q %s > %s\\test\\test", FileName, WorkingDirectory);
    system(command);

    ShutDownEventHandle = CreateEvent(NULL, TRUE, FALSE, "AfsShutdownEvent");
    PauseEventHandle = CreateEvent(NULL, TRUE, FALSE, "AfsPauseEvent");
    ContinueEventHandle = CreateEvent(NULL, TRUE, FALSE, "AfsContinueEvent");

    IterationCount = 0;
    time(&StartTime);
    if ((NumberOfIterations == 0) && (SecondsToRun == 0))
        NumberOfIterations = 1;
    else if (SecondsToRun != 0)
    {
        SecondsToRun += StartTime;
    }

    while (1)
    {
        if (SecondsToRun != 0)
        {
            time(&StartTime);
            if (StartTime > (time_t)SecondsToRun)
            {
                break;
            }
        }
        if (NumberOfIterations != 0)
        {
            if (LoopCount >= NumberOfIterations)
            {
                break;
            }
        }
        if (rc = WaitForSingleObject(ShutDownEventHandle, 0) == WAIT_OBJECT_0)
        {
            break;
        }
        ++LoopCount;
        CurrentLoop = LoopCount;

        _strtime(tbuffer);
        printf("\nIteration %d started at: %s\n", LoopCount, tbuffer);
        create_procs(HostName, CommandLine, TargetDirectory, AfsLocker, Locker, 
                     HostName, NumberOfThreads, CurrentLoop, LogID);
        _strtime(tbuffer);
        printf("Iteration %d ended at: %s\n", LoopCount, tbuffer);
        time(&EndTime);
        printf("Iteration %d lapse time: %ld seconds\n", LoopCount, EndTime - StartTime);
        TotalTime += EndTime - StartTime;
        sprintf(FileName, "%s\\log%05d\\IterationCount", WorkingDirectory, LogID);
        WaitForSingleObject(MutexHandle, 20 * 1000);
        if ((fp = fopen(FileName, "r")) != NULL)
        {
            fgets(buffer, sizeof(buffer), fp);
            IterationCount = atoi(buffer);
            fclose(fp);
        }
        ++IterationCount;
        fp = fopen(FileName, "w");
        fprintf(fp, "%d\n", IterationCount);
        fclose(fp);
        ReleaseMutex(MutexHandle);
        if (EndOnError)
        {
            for (i = 0; i < MAX_THREADS; i++)
            {
                if (ThreadStatus[i] == 1)
                {
                    break;
                }
            }
            if (i >= MAX_THREADS)
                break;
        }
        ErrorState = 0;
        for (i = 0; i < MAX_THREADS; i++)
        {
            if (ThreadStatus[i] == 0)
            {
                ErrorState = 1;
                ThreadStatus[i] = 1;
            }
        }
        if (ErrorState)
        {
            printf("\nSleeping for 3 minutes for error recovery\n\n");
            Sleep(3 * 60 * 1000);
        }
    }
    if (LoopCount == 0)
        return(0);

    sprintf(FileName, ".\\log%05d\\%s\\Master.log", LogID, HostName);
    Mfp = fopen(FileName, "w+");
    fprintf(Mfp, "Average Iteration Time = %.02f minutes\n\n", ((float)TotalTime/(float)(LoopCount))/60.0);
    for (i = 0; i < NumberOfThreads; i++)
    {
        sprintf(FileName, ".\\log%05d\\%s\\Thread_%05d.log", LogID, HostName, i);
        fp = fopen(FileName, "r");
        if (fp != NULL)
        {
            fprintf(Mfp, "START OF THREAD %d\n\n", i);
            while (fgets(buffer, 512, fp) != NULL)
            {
                fprintf(Mfp, "%s", buffer);
            }
            fclose(fp);
            fprintf(Mfp, "END OF THREAD %d\n\n", i);
        }
    }
    fclose(Mfp);

    memset(WorkingDirectory, '\0', sizeof(WorkingDirectory));
    GetCurrentDirectory(sizeof(WorkingDirectory), WorkingDirectory);

    sprintf(FileName, "%s\\log%05d\\%s\\%s", WorkingDirectory, LogID, HostName, "MasterProcessStatLog.log");
    sprintf(MoveFileName, "%s\\log%05d\\%s\\%s", WorkingDirectory, LogID, HostName, "MasterProcessStatLogRaw.log");
    BuildMasterStatLog(FileName, MoveFileName, NumberOfProcesses, NumberOfThreads, CommandLine, LoopCount, 
                       TargetDirectory, -1);

//    sprintf(DateTime, "%s-%04d%02d%02d-%02d%02d%02d", HostName,
//                                                      LocalTime.wYear, 
//                                                      LocalTime.wMonth, 
//                                                      LocalTime.wDay,
//                                                      LocalTime.wHour,
//                                                      LocalTime.wMinute,
//                                                      LocalTime.wSecond);
//    sprintf(command, "rename %s\\log%05d\\%s %s", WorkingDirectory, LogID, HostName, DateTime);
//    rc = system(command);

    WaitForSingleObject(ExitMutexHandle, 20 * 1000);
    Sleep(3 * 1000);

    GetSystemTime(&SystemTime);
    GetTimeZoneInformation(&TimeZoneInformation);
    SystemTimeToTzSpecificLocalTime(&TimeZoneInformation, &SystemTime, &LocalTime);

    NumberOfProcesses = 0;
    sprintf(FileName, "%s\\log%05d\\ProcessCount", WorkingDirectory, LogID);
    if ((fp = fopen(FileName, "r")) != NULL)
    {
        fgets(buffer, sizeof(buffer), fp);
        NumberOfProcesses = atoi(buffer);
        fclose(fp);
    }
    ++NumberOfProcesses;
    fp = fopen(FileName, "w");
    fprintf(fp, "%d\n", NumberOfProcesses);
    fclose(fp);

    if (FindProcessCount("wintorture.exe", JobHandle) == 1)
    {
        NumberOfProcesses = 0;
        sprintf(FileName, "%s\\log%05d\\ProcessCount", WorkingDirectory, LogID);
        sprintf(MoveFileName, "%s\\log%05d\\ProcessCountRaw", WorkingDirectory, LogID);
        if ((fp = fopen(FileName, "r")) != NULL)
        {
            fgets(buffer, sizeof(buffer), fp);
            NumberOfProcesses = atoi(buffer);
            fclose(fp);
            MoveFile(FileName, MoveFileName);
        }

        IterationCount = 0;
        sprintf(FileName, "%s\\log%05d\\IterationCount", WorkingDirectory, LogID);
        sprintf(MoveFileName, "%s\\log%05d\\IterationCountRaw", WorkingDirectory, LogID);
        if ((fp = fopen(FileName, "r")) != NULL)
        {
            fgets(buffer, sizeof(buffer), fp);
            IterationCount = atoi(buffer);
            fclose(fp);
            MoveFile(FileName, MoveFileName);
        }

        sprintf(FileName, "%s\\log%05d\\%s", WorkingDirectory, LogID, "MasterStatLog.log");
        sprintf(MoveFileName, "%s\\log%05d\\%s", WorkingDirectory, LogID, "MasterStatLogRaw.log");
        BuildMasterStatLog(FileName, MoveFileName, NumberOfProcesses, NumberOfThreads, 
                           CommandLine, IterationCount, TargetDirectory, -2);
        sprintf(DateTime, "%s%05d-%04d%02d%02d-%02d%02d%02d", "log",
                                                          LogID,
                                                          LocalTime.wYear, 
                                                          LocalTime.wMonth, 
                                                          LocalTime.wDay,
                                                          LocalTime.wHour,
                                                          LocalTime.wMinute,
                                                          LocalTime.wSecond);
        sprintf(command, "rename %s\\log%05d %s", WorkingDirectory, LogID, DateTime);
        rc = system(command);
        ResetEvent(ShutDownEventHandle);
        ResetEvent(PauseEventHandle);
        ResetEvent(ContinueEventHandle);
        CloseHandle(ShutDownEventHandle);
        CloseHandle(PauseEventHandle);
        CloseHandle(ContinueEventHandle);

    }

    ReleaseMutex(ExitMutexHandle);
    CloseHandle(JobHandle);
    return 0;
}

int ProcessNameAndID(DWORD processID, char *ProcessName, HANDLE JobHandle)
{
    char    szProcessName[1024] = "unknown";
    char    FileName[1024];
    char    WorkingDirectory[512];
    HANDLE  hProcess;
    int     Count;

    memset(WorkingDirectory, '\0', sizeof(WorkingDirectory));
    GetCurrentDirectory(sizeof(WorkingDirectory), WorkingDirectory);
    strcat(WorkingDirectory, "\\");
    Count = 0;
    hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
    if (hProcess)
    {
        HMODULE hMod;
        DWORD cbNeeded;

        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
        {
            memset(szProcessName, '\0', sizeof(szProcessName));
            GetModuleBaseName(hProcess, hMod, szProcessName, sizeof(szProcessName));
            if (!stricmp(szProcessName, ProcessName))
            {
                memset(FileName, '\0', sizeof(FileName));
                if (GetModuleFileNameEx(hProcess, hMod, FileName, sizeof(FileName) - 1))
                {
                    if (!strnicmp(WorkingDirectory, FileName, strlen(WorkingDirectory)))
                    {
                        ++Count;
                    }
                }
            }
        }
    }
    CloseHandle(hProcess);
    return(Count);
}

DWORD FindProcessCount(char *ProcessName, HANDLE JobHandle)
{
    DWORD       aProcesses[8092];
    DWORD       cbNeeded;
    DWORD       cProcesses;
    int         Count;
    int         rc;
    unsigned    int i;
    JOBOBJECT_BASIC_PROCESS_ID_LIST IdList;

    rc = QueryInformationJobObject(JobHandle, JobObjectBasicProcessIdList, &IdList, sizeof(IdList), NULL);
    return(IdList.NumberOfAssignedProcesses);

    if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
        return(1);

    Count = 0;
    cProcesses = cbNeeded / sizeof(DWORD);

    for (i = 0; i < cProcesses; i++)
    {

//        Count += ProcessNameAndID(aProcesses[i], ProcessName, JobHandle);
    }

    return(Count);
}

char *_progname(char *nargv0)
{
    char    *tmp;

    tmp = strrchr(nargv0, '/');
    if (tmp)
        tmp++;
    else
        tmp = nargv0;
    return(tmp);
}

#define BADCH   (int)'?'
#define BADARG  (int)':'
#define EMSG    ""

int getopt(int nargc, char *nargv[], char *ostr)
{
    static char *__progname = 0;
    static char *place = EMSG;		/* option letter processing */
    char *oli;				/* option letter list index */

    __progname = __progname?__progname:_progname(*nargv);

    if (optreset || !*place)
    {
        optreset = 0;
        if (optind >= nargc || *(place = nargv[optind]) != '-')
        {
            place = EMSG;
            return (-1);
        }
        if (place[1] && *++place == '-'	&& place[1] == '\0')
        {
            ++optind;
            place = EMSG;
            return (-1);
        }
    }
    if ((optopt = (int)*place++) == (int)':' || !(oli = strchr(ostr, optopt))) 
    {
        if (optopt == (int)'-')
            return (-1);
        if (!*place)
            ++optind;
        if (opterr && *ostr != ':')
            (void)fprintf(stderr, "%s: illegal option -- %c\n", __progname, optopt);
        return (BADCH);
    }
    if (*++oli != ':') 
    {
        optarg = NULL;
        if (!*place)
            ++optind;
    }
    else
    {
        if (*place)
            optarg = place;
        else if (nargc <= ++optind) 
        {
            place = EMSG;
            if (*ostr == ':')
                return (BADARG);
            if (opterr)
                (void)fprintf(stderr, "%s: option requires an argument -- %c\n", __progname, optopt);
            return (BADCH);
        }
        else
            optarg = nargv[optind];
        place = EMSG;
        ++optind;
    }
    return (optopt);
}
