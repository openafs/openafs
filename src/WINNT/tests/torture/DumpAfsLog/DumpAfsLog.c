// Stress.cpp : Defines the entry point for the console application.
//

#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <stdio.h>
#include <io.h>

void usage(void);
int  GetConsoleInput(HANDLE hStdin);

int main(int argc, char* argv[])
{
    int     i;
    int     Count;
    int     SecondsDelay;
    int     SecondsToRun;
    int     NewSessionDeadlock;
    int     NewSessionDeadlockCount;
    int     SecondsToDelay;
    int     MiniDump;
    int     rc;
    char    HostName[512];
    char    LogName[512];
    char    command[512];
    char    WorkingDirectory[512];
    char    LoggingDrive[512];
    char    EnvVariable[512];
//    char    Buffer[512];
//    char    FileName[32];
//    char    *pPtr;
    time_t  StartTime;
//    FILE    *fp;
    HANDLE  hStdin; 


    Count = 0;
    SecondsDelay = 15;
    NewSessionDeadlock = 0;
    SecondsToRun = 30 * 60;
    memset(HostName, '\0', sizeof(HostName));
    memset(command, '\0', sizeof(command));
    strcpy(LoggingDrive, "C");
    MiniDump = 0;

    for (i = 0; i < argc; i++)
    {
        if (!stricmp(argv[i], "-e"))
        {
            MiniDump = 1;
        }
        if (!stricmp(argv[i], "-s"))
        {
            SecondsDelay = atoi(argv[i+1]);
        }
        if (!stricmp(argv[i], "-h"))
        {
            strcpy(HostName, argv[i+1]);
        }
        if (!stricmp(argv[i], "-d"))
        {
            strcpy(LoggingDrive, argv[i+1]);
            if (strlen(LoggingDrive) == 1)
                strcat(LoggingDrive,":");
        }
        if (!stricmp(argv[i], "-m"))
        {
            SecondsToRun = atoi(argv[i+1]) * 60;
        }
        if (!stricmp(argv[i], "-n"))
        {
            NewSessionDeadlock = 1;
            NewSessionDeadlockCount = atoi(argv[i+1]);
            if (NewSessionDeadlockCount == 0)
                NewSessionDeadlockCount = 20;
        }
        if (!stricmp(argv[i], "-?") || !stricmp(argv[i], "/?") || 
            !stricmp(argv[i], "?") || !stricmp(argv[i], "help"))
        {
             usage();
             exit(1);
        }
    }

    hStdin = GetStdHandle(STD_INPUT_HANDLE);

    sprintf(command, "fs trace -on");
    rc = system(command);
    sprintf(command, "fs trace -reset");
    rc = system(command);

    GetCurrentDirectory(sizeof(WorkingDirectory), WorkingDirectory);
    if (WorkingDirectory[0] != LoggingDrive[0])
        WorkingDirectory[0] = LoggingDrive[0];
    sprintf(command, "rmdir /Q /S %s\\DumpAfsLogDir", WorkingDirectory);
    rc = system(command);
    sprintf(LogName, "%s\\DumpAfsLogDir", WorkingDirectory);
    sprintf(command, "mkdir %s\\DumpAfsLogDir", WorkingDirectory);
    rc = system(command);

    time(&StartTime);
    SecondsToRun += StartTime;
    while (1)
    {
        if (MiniDump)
        {
            printf("\n");
            sprintf(command, "fs minidump");
            rc = system(command);
            ExpandEnvironmentStrings("%windir%", EnvVariable, sizeof(EnvVariable));
            strcat(EnvVariable, "\\TEMP\\afsd.dmp");
            sprintf(command, "copy /Y %s %s\\DumpAfsLogDir", EnvVariable, WorkingDirectory);
            printf("%s\n", command);
            rc = system(command);
            sprintf(command, "rename %s\\DumpAfsLogDir\\afsd.dmp afsd_%05d.dmp", WorkingDirectory, Count);
            printf("%s\n", command);
            rc = system(command);
        }

        printf("\n");
        sprintf(command, "fs trace -dump");
        rc = system(command);
        ExpandEnvironmentStrings("%windir%", EnvVariable, sizeof(EnvVariable));
        strcat(EnvVariable, "\\TEMP\\afsd.log");
        sprintf(command, "copy /Y %s %s\\DumpAfsLogDir", EnvVariable, WorkingDirectory);
        printf("%s\n", command);
        rc = system(command);
//        sprintf(command, "crlf.pl -d %s\\DumpAfsLogDir\\afsd.log", WorkingDirectory);
//        printf("%s\n", command);
//        rc = system(command);
        sprintf(command, "rename %s\\DumpAfsLogDir\\afsd.log afsd_%05d.log", WorkingDirectory, Count);
        printf("%s\n\n", command);
        rc = system(command);
/*
        if (strlen(HostName) != 0)
        {
            sprintf(command, "cmdebug %s -long | grep refcnt > %s\\DumpAfsLogDir\\refcnt_%05d.log",
                    HostName, WorkingDirectory, Count);
            printf("%s\n", command);
            rc = system(command);
        }
        sprintf(FileName, "%s\\DumpAfsLogDir\\afsd_%05d.log", WorkingDirectory, Count);

        if (NewSessionDeadlock)
        {
            fp = fopen(FileName, "r");
            if (fp != NULL)
            {
                while (fgets(Buffer, 512, fp))
                {
//                  if (pPtr = strstr(Buffer, "RecordRacingRevoke"))
//                  {
//                      if (pPtr = strstr(Buffer, "activeCalls "))
//                      {
//                          pPtr += strlen("activeCalls ");
//                          if (atoi(pPtr) > 0)
//                          {
//                              SetEvent(ShutDownEventHandle);
//                              break;
//                          }
//                      }
//                  }
                    if (pPtr = strstr(Buffer, "New Session lsn "))
                    {
                        printf("%s", Buffer);
                        pPtr += strlen("New Session lsn "   );
                        if (atoi(pPtr) > NewSessionDeadlockCount)
                        {
                            break;
                        }
                    }
//                  if (pPtr = strstr(Buffer, "Racing revoke scp"))
//                  {
//                      SetEvent(ShutDownEventHandle);
//                      break;
//                  }
                }

            }
            fclose(fp);
        }
*/
        ++Count;
        time(&StartTime);
        if (StartTime > (time_t)SecondsToRun)
            break;
        printf("Type Q to stop DumpAfsLog\n");
        SecondsToDelay = StartTime + SecondsDelay;
        while (1)
        {
            time(&StartTime);
            if (StartTime > (time_t)SecondsToDelay)
                break;
            rc = GetConsoleInput(hStdin);
            Sleep(500);
        }
        rc = GetConsoleInput(hStdin);
    }

    sprintf(command, "fs trace -off");
    rc = system(command);
	return(0);

}

void usage(void)
{

    printf("usage: wintorture [options]\n");
    printf("where options can be:\n");
    printf("\t-d <drive> Local drive letter where to place log files\n");
    printf("\t-e         Enable fs minidump\n");
    printf("\t-h <host>  Local host name.  Used to run cmdebug\n");
    printf("\t-m <min>   Number of minutes to run this program\n");
//    printf("\t-n <num>  Stop processing on \"New Session lsn XX\" deadlock\n");
    printf("\t-s <sec>   Seconds delay between dumping AFS logs\n");
    printf("\t            15 seconds delay is the default\n");
}

int GetConsoleInput(HANDLE hStdin)
{
    INPUT_RECORD    InputRecord[128];
    DWORD           InputRecordCount;
    BOOL            rc;
    int             i;
    int             RetCode;
    char            ReadChar;

    InputRecordCount = 0;
    RetCode = 0;
    if (!(rc = PeekConsoleInput(hStdin, InputRecord, 128, &InputRecordCount)))
        return(0);
    if (InputRecordCount == 0)
        return(0);
    rc = ReadConsoleInput(hStdin, InputRecord, 128, &InputRecordCount);
    for (i = 0; i < (int)InputRecordCount; i++) 
    {
        switch(InputRecord[i].EventType) 
        { 
            case KEY_EVENT:
                if (InputRecord[i].Event.KeyEvent.bKeyDown)
                {
                    ReadChar = InputRecord[i].Event.KeyEvent.uChar.AsciiChar;
                    if ((ReadChar == 'q') || (ReadChar == 'Q'))
                    {
                        printf("Stop request received\n");
                        exit(0);
                    }
                }
                break; 
            default: 
                break; 
        }
    }
    return(RetCode);
}

