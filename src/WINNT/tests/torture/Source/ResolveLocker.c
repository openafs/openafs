/* 
 * MODULE: locker.c.
 *
 * Copyright (C) 1988-1998 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * Description: functions used by add, attach, detach, and shell extensions
 */
#ifdef HAVE_HESOID

#ifdef _WIN32_WINNT
    #undef _WIN32_WINNT
#endif

#define _WIN32_WINNT 0x0500

#include <windows.h>
#include <stdio.h>
#include <hesiod.h>
#include <locker.h>


char **ResolveHesName(char *locker);
void GetLockerInfo(char *Locker, char *Path);

int  ResolveLocker(USER_OPTIONS *attachOption)
{
    char    HostName[128];
    char    path[2048];
    char    MountDir[2048];
    char    ReparseDir[MAX_PATH];
    char    AccessMode[16];
    char    temp[4096];
    char    FileSystem[16];
    int     i;

    memset(path, '\0', sizeof(path));
    if (!stricmp(attachOption->type, "locker"))
    {
        GetLockerInfo(attachOption->Locker, path);
        if (strlen(path) == 0)
        {
            return(FALSE);
        }
        memset(MountDir, 0, sizeof(MountDir));
        memset(AccessMode, 0, sizeof(AccessMode));
        memset(ReparseDir, 0, sizeof(ReparseDir));
        memset(temp, 0, sizeof(temp));
        sscanf(path, "%s %s", FileSystem, temp);

        if (!strcmp(FileSystem, "AFS"))
        {
            sprintf(attachOption->SubMount, "\\\\afs\\%s", attachOption->Locker);
            for (i = 0; i < (int)strlen(attachOption->SubMount); i++)
            {
                if (attachOption->SubMount[i] == '/')
                    attachOption->SubMount[i] = '\\';
            }
            strcpy(attachOption->type, "AFS");
            return(TRUE);
        }
        else if (!strcmp(FileSystem, "NFS"))
        {
            if (strlen(attachOption->SubMount) != 0)
            {
                return(FALSE);
            }
            if (sscanf(path, "%s %s %s %s %s", FileSystem, temp, HostName,
                       AccessMode, ReparseDir) != 5)
            {
                return(FALSE);
            }
            sprintf(attachOption->SubMount, "\\\\%s%s", HostName, temp);
            for (i = 0; i < (int)strlen(attachOption->SubMount); i++)
            {
                if (attachOption->SubMount[i] == '/')
                    attachOption->SubMount[i] = '\\';
            }
            strcpy(attachOption->FileType, "NFS");
            return(TRUE);
        }
        else
        {
            return(FALSE);
        }
    }
    else
    {
        return(FALSE);
    }

//    rc = resource_mount(attachOption, addtoPath, addtoFront, appName,
//                        MountDir, HostName, FileSystem);
    return(TRUE);
}

void GetLockerInfo(char *Locker, char *Path)
{
    char**  cpp;
    char    cWeight[3];
    char    cPath[256];
    int     i;
    int     last_weight;

    cpp = NULL;
    cpp = ResolveHesName(Locker);

    if (cpp != NULL)
    {
        memset(cWeight, 0, sizeof(cWeight));
        memset(cPath, 0, sizeof(cPath));
        last_weight = 1000;
        i = 0;
        while (cpp[i] != NULL)
        {
            memset(cPath, '\0', sizeof(cPath));
            if (sscanf(cpp[i], "%*s %s", cPath))
            {
                if (strnicmp(cpp[i], "AFS", strlen("AFS")) == 0)
                {
                    memset(cWeight, '\0', sizeof(cWeight));
                    if (sscanf(cpp[i], "%*s %*s %*s %*s %s", cWeight))
                    {
                        if (atoi(cWeight) < last_weight)
                        {
                            strcpy(Path, cpp[i]);
                            last_weight = (int)atoi(cWeight);
                        }
                    }
                    else
                        strcpy(Path, cpp[i]);
                    }
                if (strnicmp(cpp[i], "NFS", strlen("NFS")) == 0)
                    strcpy(Path, cpp[i]);
            }
            ++i;
        }
    }
    return;
}

char **ResolveHesName(char *locker)
{
    char   type[16];
    char** cpp;
    strcpy(type, "filsys");

    cpp = NULL;
    cpp = hes_resolve(locker, type);

    return cpp;
}
#endif /*HAVE_HESOID */