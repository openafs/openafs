// Stress.cpp : Defines the entry point for the console application.
//

#include <afsconfig.h>
#include <afs/param.h>
#include <roken.h>

#include <windows.h>
#include <stdlib.h>
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
    time_t  SecondsToRun;
    int     NewSessionDeadlock;
    int     NewSessionDeadlockCount;
    time_t  SecondsToDelay;
    int     MiniDump;
    int     rc;
    char    HostName[512];
    char    LogName[512];
    char    command[512];
    char    WorkingDirectory[512];
    char    LoggingDrive[512];
    char    EnvVariable[512];
    time_t  StartTime;
    HANDLE  hStdin;


    Count = 0;
    SecondsDelay = 15;
    NewSessionDeadlock = 0;
    SecondsToRun = 30 * 60;
    memset(HostName, '\0', sizeof(HostName));
    memset(command, '\0', sizeof(command));
    strlcpy(LoggingDrive, "C", sizeof(LoggingDrive));
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
	    if (strlcpy(HostName, argv[i+1], sizeof(HostName)) >= sizeof(HostName)) {
		fprintf(stderr, "Local host name exceeds maximum length: %s\n", argv[i+1]);
		exit(-1);
	    }
        }
        if (!stricmp(argv[i], "-d"))
        {
	    if (strlcpy(LoggingDrive, argv[i+1], sizeof(LoggingDrive)) >= sizeof(LoggingDrive)) {
		fprintf(stderr, "Local drive letter exceeds maximum length: %s\n", argv[i+1]);
		exit(-1);
	    }
	    if (strlen(LoggingDrive) == 1) {
		if (strlcat(LoggingDrive,":", sizeof(LoggingDrive)) >= sizeof(LoggingDrive)) {
		    fprintf(stderr, "Local drive letter exceeds maximum length: %s\n", argv[i+1]);
		    exit(-1);
		}
	    }
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

    rc = system("fs trace -on");
    rc = system("fs trace -reset");

    GetCurrentDirectory(sizeof(WorkingDirectory), WorkingDirectory);
    if (WorkingDirectory[0] != LoggingDrive[0])
        WorkingDirectory[0] = LoggingDrive[0];
    if (snprintf(command, sizeof(command), "rmdir /Q /S %s\\DumpAfsLogDir", WorkingDirectory) >= sizeof(command)) {
	fprintf(stderr, "Command string exceeds maximum length: %s..\n", command);
	exit(1);
    }
    rc = system(command);
    if (snprintf(LogName, sizeof(LogName), "%s\\DumpAfsLogDir", WorkingDirectory) >= sizeof(command)) {
	fprintf(stderr, "Path to logname exceeds maximum length: %s..\n", LogName);
	exit(1);
    }
    if (snprintf(command, sizeof(command), "mkdir %s\\DumpAfsLogDir", WorkingDirectory) >= sizeof(command)) {
	fprintf(stderr, "Command string exceeds maximum length: %s..\n", command);
	exit(1);
    }
    rc = system(command);

    time(&StartTime);
    SecondsToRun += StartTime;
    while (1)
    {
        if (MiniDump)
        {
            printf("\n");
            rc = system("fs minidump");
            ExpandEnvironmentStrings("%windir%", EnvVariable, sizeof(EnvVariable));
	    if (strlcat(EnvVariable, "\\TEMP\\afsd.dmp", sizeof(EnvVariable)) >= sizeof(EnvVariable)) {
		fprintf(stderr, "Path name exceeds maximum length: %s..\n", EnvVariable);
		exit(1);
	    }
	    if (snprintf(command, sizeof(command), "copy /Y %s %s\\DumpAfsLogDir", EnvVariable, WorkingDirectory) >= sizeof(command)) {
		fprintf(stderr, "Command string exceeds maximum length: %s..\n", command);
		exit(1);
	    }
            printf("%s\n", command);
            rc = system(command);
	    if (snprintf(command, sizeof(command), "rename %s\\DumpAfsLogDir\\afsd.dmp afsd_%05d.dmp", WorkingDirectory, Count) >= sizeof(command)) {
		fprintf(stderr, "Command string exceeds maximum length: %s..\n", command);
		exit(1);
	    }
            printf("%s\n", command);
            rc = system(command);
        }

        printf("\n");
        rc = system("fs trace -dump");
        ExpandEnvironmentStrings("%windir%", EnvVariable, sizeof(EnvVariable));
	if (strlcat(EnvVariable, "\\TEMP\\afsd.log", sizeof(EnvVariable)) >= sizeof(EnvVariable)) {
	    fprintf(stderr, "Path name exceeds maximum length: %s..\n", EnvVariable);
	    exit(1);
	}
        if (snprintf(command, sizeof(command), "copy /Y %s %s\\DumpAfsLogDir", EnvVariable, WorkingDirectory) >= sizeof(command)) {
	    fprintf(stderr, "Command string exceeds maximum length: %s..\n", command);
	    exit(1);
        }
        printf("%s\n", command);
        rc = system(command);
	if (snprintf(command, sizeof(command), "rename %s\\DumpAfsLogDir\\afsd.log afsd_%05d.log", WorkingDirectory, Count) >= sizeof(command)) {
	    fprintf(stderr, "Command string exceeds maximum length: %s..\n", command);
	    exit(1);
	}
        printf("%s\n\n", command);
        rc = system(command);
        ++Count;
        time(&StartTime);
        if (StartTime > SecondsToRun)
            break;
        printf("Type Q to stop DumpAfsLog\n");
        SecondsToDelay = StartTime + SecondsDelay;
        while (1)
        {
            time(&StartTime);
            if (StartTime > SecondsToDelay)
                break;
            rc = GetConsoleInput(hStdin);
            Sleep(500);
        }
        rc = GetConsoleInput(hStdin);
    }

    rc = system("fs trace -off");
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

