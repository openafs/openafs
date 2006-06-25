// Stress.cpp : Defines the entry point for the console application.
//

#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <stdio.h>
#include <io.h>

#define SUSPEND_PROCESSING  'p'
#define CONTINUE_PROCESSING 'c'
#define END_PROCESSING      'e'

void usage(void);
int  GetConsoleInput(HANDLE hStdin);
void ProcessRequest(char RequestType, HANDLE ShutDownEventHandle, HANDLE PauseEventHandle, HANDLE ContinueEventHandle);

int main(int argc, char* argv[])
{
    char    Character;
    HANDLE  ShutDownEventHandle;
    HANDLE  PauseEventHandle;
    HANDLE  ContinueEventHandle;
    HANDLE  hStdin; 

    hStdin = GetStdHandle(STD_INPUT_HANDLE);


    ShutDownEventHandle = CreateEvent(NULL, TRUE, FALSE, "AfsShutdownEvent");
    PauseEventHandle = CreateEvent(NULL, TRUE, FALSE, "AfsPauseEvent");
    ContinueEventHandle = CreateEvent(NULL, TRUE, FALSE, "AfsContinueEvent");

    while (1)
    {
        printf("\nType p to pause the stress test\n");
        printf("Type c to continue the stress test\n");
        printf("Type e to end the stress test\n");
        printf("Type q to quit StopStressTest\n");
        Character = (char)GetConsoleInput(hStdin);

        if (Character == 'q')
            break;
        if ((Character == SUSPEND_PROCESSING) || (Character == END_PROCESSING) || (Character == CONTINUE_PROCESSING))
            ProcessRequest(Character, ShutDownEventHandle, PauseEventHandle, ContinueEventHandle);
        else
            printf("\nInvalid selection....\n");

    }

    CloseHandle(ShutDownEventHandle);
    CloseHandle(PauseEventHandle);
    CloseHandle(ContinueEventHandle);
	return(0);

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
    while (1)
    {
        rc = ReadConsoleInput(hStdin, InputRecord, 1, &InputRecordCount);
        if (InputRecord[0].EventType == KEY_EVENT)
        {
            if (InputRecord[0].Event.KeyEvent.bKeyDown)
                break;
        }
    }
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
                        printf("\nQuit request received\n");
                        exit(0);
                    }
                    else if ((ReadChar == 'c') || (ReadChar == 'C'))
                    {
                        printf("\nContinue stress test request received\n");
                        RetCode = CONTINUE_PROCESSING;
                    }
                    else if ((ReadChar == 'p') || (ReadChar == 'P'))
                    {
                        printf("\nPause stress test request received\n");
                        RetCode = SUSPEND_PROCESSING;
                    }
                    else if ((ReadChar == 'e') || (ReadChar == 'E'))
                    {
                        RetCode = END_PROCESSING;
                        printf("\nEnd stress test request received\n");
                    }
                }
                break; 
            default: 
                break; 
        }
    }
    return(RetCode);
}

void ProcessRequest(char RequestType, HANDLE ShutDownEventHandle, HANDLE PauseEventHandle, HANDLE ContinueEventHandle)
{
    static int LastRequest = 0;

    if (RequestType == CONTINUE_PROCESSING)
    {
        if (LastRequest == END_PROCESSING)
        {
            printf("Invalid request, all process are currently ending\n");
            return;
        }
        ResetEvent(PauseEventHandle);
        SetEvent(ContinueEventHandle);
        printf("Continue processing event has been set\n");
        LastRequest = RequestType;
    }
    if (RequestType == SUSPEND_PROCESSING)
    {
        if (LastRequest == END_PROCESSING)
        {
            printf("Invalid request, all processes are currently ending\n");
            return;
        }
        ResetEvent(ContinueEventHandle);
        SetEvent(PauseEventHandle);
        printf("Suspend processing event has been set\n");
        LastRequest = RequestType;
    }
    else if (RequestType == END_PROCESSING)
    {
        SetEvent(ShutDownEventHandle);
        ResetEvent(PauseEventHandle);
        SetEvent(ContinueEventHandle);
        printf("End processing event has been set\n");
        LastRequest = RequestType;
    }
}
