// Description: public header for the locker library

#ifndef LOCKER_H
#define LOCKER_H

typedef struct {
    int     OutputHesResolve;
    int     MountType;
    int     UseUnc;
    int     useHostname;
    int     AuthReq;
    int     UnAuth;
    int     addtoPath;
    int     deletefromPath;
    int     ForceDismount;
    int     OutputToScreen;
    int     SwitchCount;
    char    Host[512];
    char    PassWord[128];
    char    UserName[128];
    char    DiskDrive[128];
    char    Locker[512];
    char    SubMount[64];
    char    type[16];
    char    FileType[8];
} USER_OPTIONS;

int  attach(USER_OPTIONS attachOption, int addtoPath, int addtoFront, char *appName);
int detach(USER_OPTIONS detachOption, int DeleteFromPath, char *appName);

#endif