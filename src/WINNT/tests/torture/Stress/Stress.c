// Stress.cpp : Defines the entry point for the console application.
//

#include <windows.h>
#include <stdio.h>

int opterr = 1;
int optind = 1;
int optopt;
int optreset;
char *optarg;

int   getopt(int, char**, char*);
void usage(void);

int main(int argc, char* argv[])
{
    int                 i;
    int                 k;
    int                 opt;
    int                 LogID;
    int                 ProcessCount;
    int                 SecondsDelay;
    int                 DirCount;
    int                 DirFound;
    char                command[512];
    char                temp[512];
    char                TopDirectory[512];
    char                CurrentDirectory[256];
    PROCESS_INFORMATION ProcInfo;
    STARTUPINFO         StartupInfo;
    HANDLE              hArray[500];
    WIN32_FIND_DATA     FindFileData;
    HANDLE              fHandle;

    if (argc < 2)
    {
        usage();
        exit(1);
    }
    for (i = 0; i < 500; i++)
        hArray[i] = NULL;

    LogID = 0;
    ProcessCount = 0;
    DirFound = 0;
    DirCount = 0;
    SecondsDelay = 0;
    memset(command, '\0', sizeof(command));
    memset(TopDirectory, '\0', sizeof(TopDirectory));

    while ((opt = getopt(argc, argv, "A:a:BbC:c:D:d:EeF:f:I:i:L:l:M:m:N:n:P:p:SsTtU:u:Vv")) != EOF)
    {

        switch (opt)
        {
            case 'f':
            case 'F':
                DirFound = 1;
                strcpy(TopDirectory, optarg);
                break;
            case 'a':
            case 'A':
                SecondsDelay = atoi(optarg);
                break;
            case 'd':
            case 'D':
                ProcessCount = atoi(optarg);
            case 'b':
            case 'B':
            case 'c':
            case 'C':
            case 'e':
            case 'E':
            case 'i':
            case 'I':
            case 'l':
            case 'L':
            case 'm':
            case 'M':
            case 'n':
            case 'N':
            case 'p':
            case 'P':
            case 's':
            case 'S':
            case 't':
            case 'T':
            case 'u':
            case 'U':
            case 'v':
            case 'V':
                break;
            default:
                usage();
                exit(1);
                break;
        }
    }

    for (LogID = 0; LogID < 100; LogID++)
    {
        sprintf(CurrentDirectory, ".\\Log%05d", LogID);
        fHandle = FindFirstFile(CurrentDirectory, &FindFileData);
        if (fHandle == INVALID_HANDLE_VALUE)
            break;
        FindClose(fHandle);
    }
    if (LogID == 100)
    {
        printf("\nUnable to get a LogID.\n");
        exit(1);
    }

    if (!DirFound)
    {
        printf("\nYou must use the -f switch\n\n");
        usage();
        exit(1);
    }
    if (!ProcessCount)
    {
        printf("\nYou must use the -d switch\n\n");
        usage();
        exit(1);
    }
    if (ProcessCount > 500)
    {
        printf("\nA max of 500 processes allowed\n\n");
        usage();
        exit(1);
    }

    GetStartupInfo(&StartupInfo);
    memset(&StartupInfo, '\0', sizeof(StartupInfo));
    for (i = 0; i < ProcessCount; i++)
    {
        memset(command, '\0', sizeof(command));
        for (k = 0; k < argc; k++)
        {
            strcat(command, argv[k]);
            strcat(command, " ");
            if (!stricmp(argv[k], "-f"))
            {
                ++k;
                memset(temp, '\0', sizeof(temp));
                sprintf(temp, "%s%05d", argv[k], DirCount);
                strcat(command, temp);
                strcat(command, " ");
                ++DirCount;
            }
        }
        sprintf(temp, " -g %d", LogID);
        strcat(command, temp);
        StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
        StartupInfo.wShowWindow = SW_SHOWMINIMIZED;
        if (CreateProcess(".\\wintorture.exe", command, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &StartupInfo, &ProcInfo))
            hArray[i] = ProcInfo.hProcess;
        Sleep(SecondsDelay * 1000);
    }

	return(0);

}

void usage(void)
{

    printf("usage: stress [options]\n");
    printf("where options can be:\n");
    printf("\t-a <sec>  Seconds delay between starting detached processes\n");
    printf("\t-b        Create a chronological log\n");
    printf("\t-c <txt>  Specifies the script txt file to use\n");
    printf("\t-d <num>  Number of detached processes to run\n");
    printf("\t-e        End thread processing on an error.\n");
    printf("\t-f <name> Target directory name\n");
#ifdef HAVE_HESOID
    printf("\t-l <path> Interpert as an AFS locker or AFS submount\n");
#endif /* HAVE_HESOID */
    printf("\t-i <num>  Number of iterations of the stress test to run.\n");
    printf("\t            This option will override the -m option.\n");
    printf("\t-m <num>  The number of minutes to run\n");
    printf("\t-n <num>  The number of threads to run\n");
    printf("\t-p <path> UNC path to second directory\n");
    printf("\t-s        Output stats\n");
    printf("\t-t        Do AFS trace logging\n");
    printf("\t-u <UNC>  UNC path to target directory\n");
    printf("\t-v        Turn on verbose mode\n");
    printf("\nNOTE: The switches are not case sensitive.  You\n");
    printf("\n      may use either upper or lower case letters.\n\n");
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
