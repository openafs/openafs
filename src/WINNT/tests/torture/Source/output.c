#include <windows.h>
#include <includes.h>
#include "common.h"

extern int  ChronLog;
extern int  CurrentLoop;
extern HANDLE ChronMutexHandle;
extern HANDLE MutexHandle;
extern HANDLE FileMutexHandle;

void LogStats(char *FileName, int ToLog, int Iteration, int NumberOfProcesses, int NumberOfThreads, 
              char *HostName, int ProcessNumber, struct cmd_struct CommandInfo[],
              char *CommandLine, char *TargetDirectory)
{
	struct cmd_struct TotalCommandInfo[CMD_MAX_CMD + 1];
	int         i;
    int         j;
    int         LineCount;
	unsigned    grand_total = 0;
    char        AsciiTime[32];
    FILE        *fp;
    struct tm   *newtime;
    time_t      aclock;

    LineCount = 1;
    time(&aclock);
    newtime = localtime(&aclock);
    strcpy(AsciiTime, asctime(newtime));
    AsciiTime[strlen(AsciiTime) - 1] = '\0';

    fp = stdout;
    if (ToLog)
        fp = fopen(FileName, "a+");

	for (j = 0; j <= CMD_MAX_CMD; j++)
    {
		TotalCommandInfo[j].count = 0;
		TotalCommandInfo[j].min_sec = 0;
		TotalCommandInfo[j].ErrorTime = 0;
		TotalCommandInfo[j].max_sec = 0;
		TotalCommandInfo[j].MilliSeconds = 0;
		TotalCommandInfo[j].total_sec = 0;
		TotalCommandInfo[j].total_sum_of_squares = 0;
		TotalCommandInfo[j].ErrorCount = 0;
		TotalCommandInfo[j].ErrorTime = 0;
	}

    for (i = 0; i <= CMD_MAX_CMD; i++)
    {
        TotalCommandInfo[i].count += CommandInfo[i].count;
        TotalCommandInfo[i].total_sec += CommandInfo[i].total_sec; 
        TotalCommandInfo[i].ErrorCount += CommandInfo[i].ErrorCount; 
        TotalCommandInfo[i].ErrorTime += CommandInfo[i].ErrorTime; 
        grand_total += CommandInfo[i].total_sec;
        if (!TotalCommandInfo[i].min_sec || (TotalCommandInfo[i].min_sec > CommandInfo[i].min_sec))
            TotalCommandInfo[i].min_sec = CommandInfo[i].min_sec;
        if (TotalCommandInfo[i].min_sec == 1000)
            TotalCommandInfo[i].min_sec = 0;
        if (TotalCommandInfo[i].max_sec < CommandInfo[i].max_sec)
            TotalCommandInfo[i].max_sec = CommandInfo[i].max_sec;
    }
    if (ProcessNumber > -1)
    {
    	fprintf(fp, "Iteration %d Statistics\n", Iteration);
        ++LineCount;
    }
    else if (ProcessNumber == -1)
    {
    	fprintf(fp, "Process Statistics for Iteration %d\n", Iteration);
        ++LineCount;
    }
    else if (ProcessNumber == -2)
    {
    	fprintf(fp, "Test Statistics for all processes - %d Iterations \n", Iteration);
        ++LineCount;
    }
    fprintf(fp, "Date: %s\n", AsciiTime);
    ++LineCount;
    fprintf(fp, "Total Time: %d seconds (%4.1f minutes)\n", grand_total, grand_total/60.0);
    ++LineCount;
    if ((ProcessNumber == -2) || (ProcessNumber == -1))
    {
        fprintf(fp, "Number of Processes: %d\n", NumberOfProcesses);
        fprintf(fp, "Number of Threads/Process: %d\n", NumberOfThreads);
        ++LineCount;
        ++LineCount;
    }

    if (strlen(TargetDirectory) != 0)
    {
        fprintf(fp, "Target directory: %s\n", TargetDirectory);
        ++LineCount;
    }
    if (strlen(CommandLine) != 0)
    {
        fprintf(fp, "Command Line: %s\n", CommandLine);
        ++LineCount;
    }
    fprintf(fp, "\n  Command:                          Count  Min Latency Max Latency Ave Latency    Cost      Seconds     Error     Error  \n");
    fprintf(fp, "                                            (seconds)   (seconds)   (seconds)                           Count    seconds\n");
    ++LineCount;
    ++LineCount;

    for (i = LineCount; i < 12; i++)
        fprintf(fp, "\n");

    for (i = 0; i <= CMD_MAX_CMD; i++)
    {
        fprintf(fp, "%-30s   %8d    %8d  %10d  %10.2f   %5.1f%%   %10d  %8d  %8d  %s\n",
                cmd_names[i].name, 
                TotalCommandInfo[i].count, 
                TotalCommandInfo[i].min_sec,
                TotalCommandInfo[i].max_sec,
                (TotalCommandInfo[i].count ? (float)TotalCommandInfo[i].total_sec/(float)TotalCommandInfo[i].count : 0),
                (grand_total ? 100.0*TotalCommandInfo[i].total_sec/grand_total : 0),
                TotalCommandInfo[i].total_sec,
                TotalCommandInfo[i].ErrorCount,
                TotalCommandInfo[i].ErrorTime,
                cmd_names[i].ms_api);
    }
    fprintf(fp, "\n");
    if (ToLog)
        fclose(fp);
}

void LogMessage(int ProcessNumber, char *HostName, char *FileName, char *message, int LogID)
{
    char        NewMessage[1024];
    char        AsciiTime[32];
    char        NewFileName[256];
    char        WorkingDirectory[512];
    FILE        *fp;
    struct tm   *newtime;
    time_t      aclock;
    DWORD       rc;

    if (ChronLog)
    {
        while(1)
        {
            rc = WaitForSingleObject(ChronMutexHandle, 4 * 1000);
            if ((rc == WAIT_OBJECT_0) || (rc == WAIT_TIMEOUT))
                break;
        }
    }
    time(&aclock);
    newtime = localtime(&aclock);
    strcpy(AsciiTime, asctime(newtime));
    AsciiTime[strlen(AsciiTime) - 1] = '\0';
    GetCurrentDirectory(sizeof(WorkingDirectory), WorkingDirectory);
    sprintf(NewMessage, "%s - %s", AsciiTime, message);
    sprintf(NewFileName, "%s\\log%05d\\%s\\%s", WorkingDirectory, LogID, HostName, FileName);
    fp = fopen(NewFileName, "a+");
    if (fp != NULL)
    {
        fprintf(fp, "%s", NewMessage);
        fclose(fp);
    }
    if (ChronLog)
    {
        sprintf(NewMessage, "%s %s:%d- %s", AsciiTime, HostName, ProcessNumber, message);
        sprintf(NewFileName, "%s\\log%05d\\Chron.log", WorkingDirectory, LogID);
        fp = fopen(NewFileName, "a+");
        fprintf(fp, "%s", NewMessage);
        fclose(fp);
        ReleaseMutex(ChronMutexHandle);
    }
}

void DumpAFSLog(char *HostName, int LogID)
{
    char    EnvVariable[512];
    char    WorkingDirectory[512];
    char    command[512];
    int     rc;

    WaitForSingleObject(FileMutexHandle, 4 * 1000);
    memset(WorkingDirectory, '\0', sizeof(WorkingDirectory));
    GetCurrentDirectory(sizeof(WorkingDirectory), WorkingDirectory);
    sprintf(command, "fs trace -dump > .\\test\\%s", HostName);
    rc = system(command);
    rc = GetWindowsDirectory(EnvVariable, sizeof(EnvVariable));
    strcat(EnvVariable, "\\TEMP\\afsd.log");
    sprintf(command, "move %s %s\\log%05d\\%s > .\\test\\%s", EnvVariable, WorkingDirectory, LogID, HostName, HostName);
    rc = system(command);
    sprintf(command, "rename %s\\log%05d\\%s\\afsd.log afsd_%s_iteration%d.log", 
            WorkingDirectory, LogID, HostName, HostName, CurrentLoop);
    rc = system(command);
    ReleaseMutex(FileMutexHandle);
}

int UpdateMasterLog(char *FileName, struct cmd_struct CommandInfo[])
{
    FILE    *fp;
    char    Buffer[32];
    int     i;
    struct cmd_struct TotalCommandInfo[CMD_MAX_CMD + 1];

    WaitForSingleObject(FileMutexHandle, 4 * 1000);

    for (i = 0; i <= CMD_MAX_CMD; i++)
    {
        TotalCommandInfo[i].count = 0;
        TotalCommandInfo[i].min_sec = 0;
        TotalCommandInfo[i].max_sec = 0;
        TotalCommandInfo[i].MilliSeconds = 0;
        TotalCommandInfo[i].total_sec = 0;
        TotalCommandInfo[i].ErrorCount = 0;
        TotalCommandInfo[i].ErrorTime = 0;
        TotalCommandInfo[i].total_sum_of_squares = 0;
    }

    fp = fopen(FileName, "r");
    if (fp != NULL)
    {
        for (i = 0; i <= CMD_MAX_CMD; i++)
        {
            fgets(Buffer, 16, fp);
            TotalCommandInfo[i].count = atoi(Buffer);
            fgets(Buffer, 16, fp);
            TotalCommandInfo[i].min_sec = atoi(Buffer);
            fgets(Buffer, 16, fp);
            TotalCommandInfo[i].max_sec = atoi(Buffer);
            fgets(Buffer, 16, fp);
            TotalCommandInfo[i].total_sec = atoi(Buffer);
            fgets(Buffer, 16, fp);
            TotalCommandInfo[i].total_sum_of_squares = atoi(Buffer);
            fgets(Buffer, 16, fp);
            TotalCommandInfo[i].ErrorCount = atoi(Buffer);
            fgets(Buffer, 16, fp);
            TotalCommandInfo[i].ErrorTime = atoi(Buffer);
        }
        fclose(fp);
    }

    for (i = 0; i <= CMD_MAX_CMD; i++)
    {
        TotalCommandInfo[i].count += CommandInfo[i].count;
        if (!TotalCommandInfo[i].min_sec || (TotalCommandInfo[i].min_sec > CommandInfo[i].min_sec))
            TotalCommandInfo[i].min_sec = CommandInfo[i].min_sec;
        if (TotalCommandInfo[i].max_sec < CommandInfo[i].max_sec)
            TotalCommandInfo[i].max_sec = CommandInfo[i].max_sec;
        TotalCommandInfo[i].total_sec += CommandInfo[i].total_sec;
        TotalCommandInfo[i].total_sum_of_squares += CommandInfo[i].total_sum_of_squares;
        TotalCommandInfo[i].ErrorCount += CommandInfo[i].ErrorCount;
        TotalCommandInfo[i].ErrorTime += CommandInfo[i].ErrorTime;
    }


    fp = fopen(FileName, "w");
    for (i = 0; i <= CMD_MAX_CMD; i++)
    {
        fprintf(fp, "%ld\n", TotalCommandInfo[i].count);
        fprintf(fp, "%ld\n", TotalCommandInfo[i].min_sec);
        fprintf(fp, "%ld\n", TotalCommandInfo[i].max_sec);
        fprintf(fp, "%ld\n", TotalCommandInfo[i].total_sec);
        fprintf(fp, "%ld\n", TotalCommandInfo[i].total_sum_of_squares);
        fprintf(fp, "%ld\n", TotalCommandInfo[i].ErrorCount);
        fprintf(fp, "%ld\n", TotalCommandInfo[i].ErrorTime);
    }
    fclose(fp);
    ReleaseMutex(FileMutexHandle);
    return(0);
}

int BuildMasterStatLog(char *FileName, char*MoveFileName, int NumberOfProcesses, 
                       int NumberOfThreads, char *CommandLine, int Iterations, 
                       char *TargetDirectory, int ProcessNumber)
{
    FILE    *fp;
    char    Buffer[32];
    int     i;
    struct cmd_struct TotalCommandInfo[CMD_MAX_CMD + 1];

    for (i = 0; i <= CMD_MAX_CMD; i++)
    {
        TotalCommandInfo[i].count = 0;
        TotalCommandInfo[i].min_sec = 0;
        TotalCommandInfo[i].MilliSeconds = 0;
        TotalCommandInfo[i].max_sec = 0;
        TotalCommandInfo[i].total_sec = 0;
        TotalCommandInfo[i].total_sum_of_squares = 0;
        TotalCommandInfo[i].ErrorCount = 0;
        TotalCommandInfo[i].ErrorTime = 0;
    }

    fp = fopen(FileName, "r");
    if (fp != NULL)
    {
        for (i = 0; i <= CMD_MAX_CMD; i++)
        {
            if (fgets(Buffer, 16, fp) != NULL)
                TotalCommandInfo[i].count = atoi(Buffer);
            if (fgets(Buffer, 16, fp) != NULL)
                TotalCommandInfo[i].min_sec = atoi(Buffer);
            if (fgets(Buffer, 16, fp) != NULL)
                TotalCommandInfo[i].max_sec = atoi(Buffer);
            if (fgets(Buffer, 16, fp) != NULL)
                TotalCommandInfo[i].total_sec = atoi(Buffer);
            if (fgets(Buffer, 16, fp) != NULL)
                TotalCommandInfo[i].total_sum_of_squares = atoi(Buffer);
            if (fgets(Buffer, 16, fp) != NULL)
                TotalCommandInfo[i].ErrorCount = atoi(Buffer);
            if (fgets(Buffer, 16, fp) != NULL)
                TotalCommandInfo[i].ErrorTime = atoi(Buffer);
        }
        fclose(fp);
        MoveFile(FileName, MoveFileName);
    }
    
    fp = fopen(FileName, "w");
    fclose(fp);
    LogStats(FileName, 1, Iterations, NumberOfProcesses, NumberOfThreads, "", ProcessNumber, TotalCommandInfo, 
             CommandLine, TargetDirectory);
    return(0);
}
