#include <afs/param.h>
#include <afs/stds.h>

#ifndef DJGPP
#include <windows.h>
#include <winreg.h>
#include <winsock2.h>
#else
#include <netdb.h>
#endif /* !DJGPP */
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include <WINNT/afsreg.h>
#include "afsd.h"
#include <rx/rx.h>

#ifdef AFS_FREELANCE_CLIENT
#include "cm_freelance.h"
#include "stdio.h"

extern void afsi_log(char *pattern, ...);

int cm_noLocalMountPoints;
int cm_fakeDirSize;
int cm_fakeDirCallback=0;
int cm_fakeGettingCallback=0;
cm_localMountPoint_t* cm_localMountPoints;
osi_mutex_t cm_Freelance_Lock;
int cm_localMountPointChangeFlag = 0;
int cm_freelanceEnabled = 1;
time_t FakeFreelanceModTime = 0x3b49f6e2;

static int freelance_ShutdownFlag = 0;
#if !defined(DJGPP)
static HANDLE hFreelanceChangeEvent = 0;
static HANDLE hFreelanceSymlinkChangeEvent = 0;
#endif

void cm_InitFakeRootDir();

#if !defined(DJGPP)
void cm_FreelanceChangeNotifier(void * parmp) {
    HKEY   hkFreelance = 0;

    if (RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
                      AFSREG_CLT_OPENAFS_SUBKEY "\\Freelance",
                      0,
                      KEY_NOTIFY,
                      &hkFreelance) == ERROR_SUCCESS) {

        hFreelanceChangeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (hFreelanceChangeEvent == NULL) {
            RegCloseKey(hkFreelance);
            return;
        }
    }

    while ( TRUE ) {
    /* check hFreelanceChangeEvent to see if it is set. 
     * if so, call cm_noteLocalMountPointChange()
     */
        if (RegNotifyChangeKeyValue( hkFreelance,   /* hKey */
                                     FALSE,         /* bWatchSubtree */
                                     REG_NOTIFY_CHANGE_LAST_SET, /* dwNotifyFilter */
                                     hFreelanceChangeEvent, /* hEvent */
                                     TRUE          /* fAsynchronous */
                                     ) != ERROR_SUCCESS) {
            RegCloseKey(hkFreelance);
            CloseHandle(hFreelanceChangeEvent);
            hFreelanceChangeEvent = 0;
            return;
        }

        if (WaitForSingleObject(hFreelanceChangeEvent, INFINITE) == WAIT_OBJECT_0)
        {
            if (freelance_ShutdownFlag == 1) {     
                RegCloseKey(hkFreelance);          
                CloseHandle(hFreelanceChangeEvent);
                hFreelanceChangeEvent = 0;         
                return;                            
            }                                      
            cm_noteLocalMountPointChange();
        }
    }
}

void cm_FreelanceSymlinkChangeNotifier(void * parmp) {
    HKEY   hkFreelance = 0;

    if (RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
                      AFSREG_CLT_OPENAFS_SUBKEY "\\Freelance\\Symlinks",
                      0,
                      KEY_NOTIFY,
                      &hkFreelance) == ERROR_SUCCESS) {

        hFreelanceSymlinkChangeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (hFreelanceSymlinkChangeEvent == NULL) {
            RegCloseKey(hkFreelance);
            return;
        }
    }

    while ( TRUE ) {
    /* check hFreelanceSymlinkChangeEvent to see if it is set. 
     * if so, call cm_noteLocalMountPointSymlinkChange()
     */
        if (RegNotifyChangeKeyValue( hkFreelance,   /* hKey */
                                     FALSE,         /* bWatchSubtree */
                                     REG_NOTIFY_CHANGE_LAST_SET, /* dwNotifyFilter */
                                     hFreelanceSymlinkChangeEvent, /* hEvent */
                                     TRUE          /* fAsynchronous */
                                     ) != ERROR_SUCCESS) {
            RegCloseKey(hkFreelance);
            CloseHandle(hFreelanceSymlinkChangeEvent);
            hFreelanceSymlinkChangeEvent = 0;
            return;
        }

        if (WaitForSingleObject(hFreelanceSymlinkChangeEvent, INFINITE) == WAIT_OBJECT_0)
        {
            if (freelance_ShutdownFlag == 1) {     
                RegCloseKey(hkFreelance);          
                CloseHandle(hFreelanceSymlinkChangeEvent);
                hFreelanceSymlinkChangeEvent = 0;         
                return;                            
            }                                      
            cm_noteLocalMountPointChange();
        }
    }
}
#endif

void                                          
cm_FreelanceShutdown(void)                    
{                                             
    freelance_ShutdownFlag = 1;               
#if !defined(DJGPP)                           
    if (hFreelanceChangeEvent != 0)           
        thrd_SetEvent(hFreelanceChangeEvent); 
    if (hFreelanceSymlinkChangeEvent != 0)           
        thrd_SetEvent(hFreelanceSymlinkChangeEvent); 
#endif                                        
}                                             

void cm_InitFreelance() {
#if !defined(DJGPP)
    thread_t phandle;
    int lpid;
#endif

    lock_InitializeMutex(&cm_Freelance_Lock, "Freelance Lock");

    // make sure we sync the data version to the cached root scache_t                  
    if (cm_data.rootSCachep && cm_data.rootSCachep->fid.cell == AFS_FAKE_ROOT_CELL_ID) 
        cm_data.fakeDirVersion = cm_data.rootSCachep->dataVersion;                          
                                                                                      
    // yj: first we make a call to cm_initLocalMountPoints
    // to read all the local mount points from the registry
    cm_InitLocalMountPoints();

    // then we make a call to InitFakeRootDir to create
    // a fake root directory based on the local mount points
    cm_InitFakeRootDir();
    // --- end of yj code

#if !defined(DJGPP)
    /* Start the registry monitor */
    phandle = thrd_Create(NULL, 65536, (ThreadFunc) cm_FreelanceChangeNotifier,
                          NULL, 0, &lpid, "cm_FreelanceChangeNotifier");
    osi_assertx(phandle != NULL, "cm_FreelanceChangeNotifier thread create failure");
    thrd_CloseHandle(phandle);

    phandle = thrd_Create(NULL, 65536, (ThreadFunc) cm_FreelanceSymlinkChangeNotifier,
                          NULL, 0, &lpid, "cm_FreelanceSymlinkChangeNotifier");
    osi_assertx(phandle != NULL, "cm_FreelanceSymlinkChangeNotifier thread create failure");
    thrd_CloseHandle(phandle);
#endif
}

/* yj: Initialization of the fake root directory */
/* to be called while holding freelance lock unless during init. */
void cm_InitFakeRootDir() {
    int i, t1, t2;
    char* currentPos;
    int noChunks;

    // allocate space for the fake info
    cm_dirHeader_t fakeDirHeader;
    cm_dirEntry_t fakeEntry;
    cm_pageHeader_t fakePageHeader;

    // i'm going to calculate how much space is needed for
    // this fake root directory. we have these rules:
    // 1. there are cm_noLocalMountPoints number of entries
    // 2. each page is CM_DIR_PAGESIZE in size
    // 3. the first 13 chunks of the first page are used for
    //    some header stuff
    // 4. the first chunk of all subsequent pages are used
    //    for page header stuff
    // 5. a max of CM_DIR_EPP entries are allowed per page
    // 6. each entry takes 1 or more chunks, depending on 
    //    the size of the mount point string, as determined
    //    by cm_NameEntries
    // 7. each chunk is CM_DIR_CHUNKSIZE bytes

    int CPP = CM_DIR_PAGESIZE / CM_DIR_CHUNKSIZE;
    int curChunk = 13;	// chunks 0 - 12 are used for header stuff
                        // of the first page in the directory
    int curPage = 0;
    int curDirEntry = 0;
    int curDirEntryInPage = 0;
    int sizeOfCurEntry;
    int dirSize;

    /* Reserve 2 directory chunks for "." and ".." */
    curChunk += 2;

    while (curDirEntry<cm_noLocalMountPoints) {
        sizeOfCurEntry = cm_NameEntries((cm_localMountPoints+curDirEntry)->namep, 0);
        if ((curChunk + sizeOfCurEntry >= CPP) ||
             (curDirEntryInPage + 1 >= CM_DIR_EPP)) {
            curPage++;
            curDirEntryInPage = 0;
            curChunk = 1;
        }
        curChunk += sizeOfCurEntry;
        curDirEntry++;
        curDirEntryInPage++;
    }

    dirSize = (curPage+1) *  CM_DIR_PAGESIZE;
    cm_FakeRootDir = malloc(dirSize);
    cm_fakeDirSize = dirSize;

    // yj: when we get here, we've figured out how much memory we need and 
    // allocated the appropriate space for it. we now prceed to fill
    // it up with entries.
    curPage = 0;
    curDirEntry = 0;
    curDirEntryInPage = 0;
    curChunk = 0;

    // fields in the directory entry that are unused.
    fakeEntry.flag = 1;
    fakeEntry.length = 0;
    fakeEntry.next = 0;
    fakeEntry.fid.unique = htonl(1);

    // the first page is special, it uses fakeDirHeader instead of fakePageHeader
    // we fill up the page with dirEntries that belong there and we make changes
    // to the fakeDirHeader.header.freeBitmap along the way. Then when we're done
    // filling up the dirEntries in this page, we copy the fakeDirHeader into 
    // the top of the page.

    // init the freeBitmap array
    for (i=0; i<8; i++) 
        fakeDirHeader.header.freeBitmap[i]=0;

    fakeDirHeader.header.freeBitmap[0] = 0xff;
    fakeDirHeader.header.freeBitmap[1] = 0x7f;


    // we start counting at 13 because the 0th to 12th chunks are used for header
    curChunk = 13;

    // stick the first 2 entries "." and ".." in
    fakeEntry.fid.unique = htonl(1);
    fakeEntry.fid.vnode = htonl(1);
    strcpy(fakeEntry.name, ".");
    currentPos = cm_FakeRootDir + curPage * CM_DIR_PAGESIZE + curChunk * CM_DIR_CHUNKSIZE;
    memcpy(currentPos, &fakeEntry, CM_DIR_CHUNKSIZE);
    curChunk++; curDirEntryInPage++;
    strcpy(fakeEntry.name, "..");
    currentPos = cm_FakeRootDir + curPage * CM_DIR_PAGESIZE + curChunk * CM_DIR_CHUNKSIZE;
    memcpy(currentPos, &fakeEntry, CM_DIR_CHUNKSIZE);
    curChunk++; curDirEntryInPage++;

    // keep putting stuff into page 0 if
    // 1. we're not done with all entries
    // 2. we have less than CM_DIR_EPP entries in page 0
    // 3. we're not out of chunks in page 0

    while( (curDirEntry<cm_noLocalMountPoints) && 
           (curDirEntryInPage < CM_DIR_EPP) &&
           (curChunk + cm_NameEntries((cm_localMountPoints+curDirEntry)->namep, 0) <= CPP)) 
    {       

        noChunks = cm_NameEntries((cm_localMountPoints+curDirEntry)->namep, 0);
        fakeEntry.fid.vnode = htonl(curDirEntry + 2);
        currentPos = cm_FakeRootDir + curPage * CM_DIR_PAGESIZE + curChunk * CM_DIR_CHUNKSIZE;

        memcpy(currentPos, &fakeEntry, CM_DIR_CHUNKSIZE);
        strcpy(currentPos + 12, (cm_localMountPoints+curDirEntry)->namep);
        curDirEntry++;
        curDirEntryInPage++;
        for (i=0; i<noChunks; i++) {
            t1 = (curChunk + i) / 8;
            t2 = curChunk + i - (t1*8);
            fakeDirHeader.header.freeBitmap[t1] |= (1 << t2);
        }
        curChunk+=noChunks;
    }

    // when we get here, we're done with filling in the entries for page 0
    // copy in the header info

    memcpy(cm_FakeRootDir, &fakeDirHeader, 13 * CM_DIR_CHUNKSIZE);

    curPage++;

    // ok, page 0's done. Move on to the next page.
    while (curDirEntry<cm_noLocalMountPoints) {
        // setup a new page
        curChunk = 1;			// the zeroth chunk is reserved for page header
        curDirEntryInPage = 0; 
        for (i=0; i<8; i++) {
            fakePageHeader.freeBitmap[i]=0;
        }
        fakePageHeader.freeCount = 0;
        fakePageHeader.pgcount = 0;
        fakePageHeader.tag = htons(1234);

        // while we're on the same page...
        while ( (curDirEntry<cm_noLocalMountPoints) &&
                (curDirEntryInPage < CM_DIR_EPP) &&
                (curChunk + cm_NameEntries((cm_localMountPoints+curDirEntry)->namep, 0) <= CPP))
        {
            // add an entry to this page

            noChunks = cm_NameEntries((cm_localMountPoints+curDirEntry)->namep, 0);
            fakeEntry.fid.vnode=htonl(curDirEntry+2);
            currentPos = cm_FakeRootDir + curPage * CM_DIR_PAGESIZE + curChunk * CM_DIR_CHUNKSIZE;
            memcpy(currentPos, &fakeEntry, CM_DIR_CHUNKSIZE);
            strcpy(currentPos + 12, (cm_localMountPoints+curDirEntry)->namep);
            curDirEntry++;
            curDirEntryInPage++;
            for (i=0; i<noChunks; i++) {
                t1 = (curChunk + i) / 8;
                t2 = curChunk + i - (t1*8);
                fakePageHeader.freeBitmap[t1] |= (1 << t2);
            }
            curChunk+=noChunks;
        }
        memcpy(cm_FakeRootDir + curPage * CM_DIR_PAGESIZE, &fakePageHeader, sizeof(fakePageHeader));

        curPage++;
    }

    // we know the fakeDir is setup properly, so we claim that we have callback
    osi_Log0(afsd_logp,"cm_InitFakeRootDir fakeDirCallback=1");
    cm_fakeDirCallback=1;

    // when we get here, we've set up everything! done!
}

int cm_FakeRootFid(cm_fid_t *fidp)
{
    fidp->cell = AFS_FAKE_ROOT_CELL_ID;            /* root cell */
    fidp->volume = AFS_FAKE_ROOT_VOL_ID;           /* root.afs ? */
    fidp->vnode = 0x1;
    fidp->unique = 0x1;
    return 0;
}
  
/* called directly from ioctl */
/* called while not holding freelance lock */
int cm_noteLocalMountPointChange(void) {
    lock_ObtainMutex(&cm_Freelance_Lock);
    cm_data.fakeDirVersion++;
    cm_localMountPointChangeFlag = 1;
    lock_ReleaseMutex(&cm_Freelance_Lock);
    return 1;
}

int cm_getLocalMountPointChange() {
    return cm_localMountPointChangeFlag;
}

int cm_clearLocalMountPointChange() {
    cm_localMountPointChangeFlag = 0;
    return 0;
}

int cm_reInitLocalMountPoints() {
    cm_fid_t aFid;
    int i, hash;
    cm_scache_t *scp, **lscpp, *tscp;
	
    osi_Log0(afsd_logp,"----- freelance reinitialization starts ----- ");

    // first we invalidate all the SCPs that were created
    // for the local mount points

    osi_Log0(afsd_logp,"Invalidating local mount point scp...  ");

    aFid.cell = AFS_FAKE_ROOT_CELL_ID;
    aFid.volume=AFS_FAKE_ROOT_VOL_ID;
    aFid.unique=0x1;
    aFid.vnode=0x2;

    lock_ObtainWrite(&cm_scacheLock);
    lock_ObtainMutex(&cm_Freelance_Lock);  /* always scache then freelance lock */
    for (i=0; i<cm_noLocalMountPoints; i++) {
        hash = CM_SCACHE_HASH(&aFid);
        for (scp=cm_data.scacheHashTablep[hash]; scp; scp=scp->nextp) {
            if (scp->fid.volume == aFid.volume &&
                 scp->fid.vnode == aFid.vnode &&
                 scp->fid.unique == aFid.unique 
                 ) {

                // mark the scp to be reused
                cm_HoldSCacheNoLock(scp);
                lock_ReleaseWrite(&cm_scacheLock);
                lock_ObtainMutex(&scp->mx);
                cm_DiscardSCache(scp);
                lock_ReleaseMutex(&scp->mx);
                cm_CallbackNotifyChange(scp);
                lock_ObtainWrite(&cm_scacheLock);
                cm_ReleaseSCacheNoLock(scp);

                // take the scp out of the hash
                for (lscpp = &cm_data.scacheHashTablep[hash], tscp = cm_data.scacheHashTablep[hash]; 
                     tscp; 
                     lscpp = &tscp->nextp, tscp = tscp->nextp) {
                    if (tscp == scp) {
                        *lscpp = scp->nextp;
			lock_ObtainMutex(&scp->mx);
                        scp->flags &= ~CM_SCACHEFLAG_INHASH;
			lock_ReleaseMutex(&scp->mx);
                        break;
                    }
                }
            }
        }
        aFid.vnode = aFid.vnode + 1;
    }
    lock_ReleaseWrite(&cm_scacheLock);
    osi_Log0(afsd_logp,"\tall old scp cleared!");

    // we must free the memory that was allocated in the prev
    // cm_InitLocalMountPoints call
    osi_Log0(afsd_logp,"Removing old localmountpoints...  ");
    free(cm_localMountPoints);
    osi_Log0(afsd_logp,"\tall old localmountpoints cleared!");

    // now re-init the localmountpoints
    osi_Log0(afsd_logp,"Creating new localmountpoints...  ");
    cm_InitLocalMountPoints();
    osi_Log0(afsd_logp,"\tcreated new set of localmountpoints!");

    // now we have to free the memory allocated in cm_initfakerootdir
    osi_Log0(afsd_logp,"Removing old fakedir...  ");
    free(cm_FakeRootDir);
    osi_Log0(afsd_logp,"\t\told fakedir removed!");

    // then we re-create that dir
    osi_Log0(afsd_logp,"Creating new fakedir...  ");
    cm_InitFakeRootDir();
    osi_Log0(afsd_logp,"\t\tcreated new fakedir!");

    lock_ReleaseMutex(&cm_Freelance_Lock);

    osi_Log0(afsd_logp,"----- freelance reinit complete -----");
    return 0;
}


// yj: open up the registry and read all the local mount 
// points that are stored there. Part of the initialization
// process for the freelance client.
/* to be called while holding freelance lock unless during init. */
long cm_InitLocalMountPoints() {
    FILE *fp;
    int i;
    char line[512];
    char*t, *t2;
    cm_localMountPoint_t* aLocalMountPoint;
    char hdir[120];
    long code;
    char rootCellName[256];
#if !defined(DJGPP)
    HKEY hkFreelance = 0, hkFreelanceSymlinks = 0;
    DWORD dwType, dwSize;
    DWORD dwMountPoints = 0;
    DWORD dwIndex;
    DWORD dwSymlinks = 0;
    FILETIME ftLastWriteTime;
#endif

#if !defined(DJGPP)
    if (RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
                      AFSREG_CLT_OPENAFS_SUBKEY "\\Freelance",
                      0,
                      KEY_READ|KEY_WRITE|KEY_QUERY_VALUE,
                      &hkFreelance) == ERROR_SUCCESS) {

        RegQueryInfoKey( hkFreelance,
                         NULL,  /* lpClass */
                         NULL,  /* lpcClass */
                         NULL,  /* lpReserved */
                         NULL,  /* lpcSubKeys */
                         NULL,  /* lpcMaxSubKeyLen */
                         NULL,  /* lpcMaxClassLen */
                         &dwMountPoints, /* lpcValues */
                         NULL,  /* lpcMaxValueNameLen */
                         NULL,  /* lpcMaxValueLen */
                         NULL,  /* lpcbSecurityDescriptor */
                         &ftLastWriteTime /* lpftLastWriteTime */
                         );

        smb_UnixTimeFromLargeSearchTime(&FakeFreelanceModTime, &ftLastWriteTime);

        if ( dwMountPoints == 0 ) {
            rootCellName[0] = '.';
            code = cm_GetRootCellName(&rootCellName[1]);
            if (code == 0) {
                cm_FreelanceAddMount(&rootCellName[1], &rootCellName[1], "root.cell.", 0, NULL);
                cm_FreelanceAddMount(rootCellName, &rootCellName[1], "root.cell.", 1, NULL);
                cm_FreelanceAddMount(".root", &rootCellName[1], "root.afs.", 1, NULL);
                dwMountPoints = 3;
            }
        }

        if (RegCreateKeyEx( HKEY_LOCAL_MACHINE, 
                          AFSREG_CLT_OPENAFS_SUBKEY "\\Freelance\\Symlinks",
                          0,
                          NULL,
                          REG_OPTION_NON_VOLATILE,
                          KEY_READ|KEY_WRITE|KEY_QUERY_VALUE,
                          NULL,
                          &hkFreelanceSymlinks,
                          NULL) == ERROR_SUCCESS) {

            RegQueryInfoKey( hkFreelanceSymlinks,
                             NULL,  /* lpClass */
                             NULL,  /* lpcClass */
                             NULL,  /* lpReserved */
                             NULL,  /* lpcSubKeys */
                             NULL,  /* lpcMaxSubKeyLen */
                             NULL,  /* lpcMaxClassLen */
                             &dwSymlinks, /* lpcValues */
                             NULL,  /* lpcMaxValueNameLen */
                             NULL,  /* lpcMaxValueLen */
                             NULL,  /* lpcbSecurityDescriptor */
                             NULL   /* lpftLastWriteTime */
                             );
        }

        // get the number of entries there are from the first line
        // that we read
        cm_noLocalMountPoints = dwMountPoints + dwSymlinks;

        // create space to store the local mount points
        cm_localMountPoints = malloc(sizeof(cm_localMountPoint_t) * cm_noLocalMountPoints);
        aLocalMountPoint = cm_localMountPoints;

        // now we read n lines and parse them into local mount points
        // where n is the number of local mount points there are, as
        // determined above.
        // Each line in the ini file represents 1 local mount point and 
        // is in the format xxx#yyy:zzz, where xxx is the directory
        // entry name, yyy is the cell name and zzz is the volume name.
        // #yyy:zzz together make up the mount point.
        for ( dwIndex = 0 ; dwIndex < dwMountPoints; dwIndex++ ) {
            TCHAR szValueName[16];
            DWORD dwValueSize = 16;
            dwSize = sizeof(line);
            if (RegEnumValue( hkFreelance, dwIndex, szValueName, &dwValueSize, NULL,
                          &dwType, line, &dwSize))
            {
                afsi_log("RegEnumValue(hkFreelance) failed");
                cm_noLocalMountPoints--;
                continue;
            }

            afsi_log("Mountpoint[%d] = %s",dwIndex, line);

            /* find the trailing dot; null terminate after it */
            t2 = strrchr(line, '.');
            if (t2)
                *(t2+1) = '\0';

            for ( t=line;*t;t++ ) {
                if ( !isprint(*t) ) {
                    afsi_log("error occurred while parsing mountpoint entry [%d]: non-printable character", dwIndex);
                    fprintf(stderr, "error occurred while parsing mountpoint entry [%d]: non-printable character", dwIndex);
                    cm_noLocalMountPoints--;
                    continue;
                }
            }

            // line is not empty, so let's parse it
            t = strchr(line, '#');
            if (!t)
                t = strchr(line, '%');
            // make sure that there is a '#' or '%' separator in the line
            if (!t) {
                afsi_log("error occurred while parsing mountpoint entry [%d]: no # or %% separator", dwIndex);
                fprintf(stderr, "error occurred while parsing mountpoint entry [%d]: no # or %% separator", dwIndex);
                cm_noLocalMountPoints--;
                continue;
            }

            aLocalMountPoint->fileType = CM_SCACHETYPE_MOUNTPOINT;
            aLocalMountPoint->namep=malloc(t-line+1);
            strncpy(aLocalMountPoint->namep, line, t-line);
            aLocalMountPoint->namep[t-line] = '\0';
		
            /* copy the mount point string */
            aLocalMountPoint->mountPointStringp=malloc(strlen(t));
            strncpy(aLocalMountPoint->mountPointStringp, t, strlen(t)-1);
            aLocalMountPoint->mountPointStringp[strlen(t)-1] = '\0';
    
            osi_Log2(afsd_logp,"found mount point: name %s, string %s",
                      osi_LogSaveString(afsd_logp,aLocalMountPoint->namep),
                      osi_LogSaveString(afsd_logp,aLocalMountPoint->mountPointStringp));

            aLocalMountPoint++;
        }

        for ( dwIndex = 0 ; dwIndex < dwSymlinks; dwIndex++ ) {
            TCHAR szValueName[16];
            DWORD dwValueSize = 16;
            dwSize = sizeof(line);
            if (RegEnumValue( hkFreelanceSymlinks, dwIndex, szValueName, &dwValueSize, NULL,
                              &dwType, line, &dwSize))
            {
                afsi_log("RegEnumValue(hkFreelanceSymlinks) failed");
                cm_noLocalMountPoints--;
                continue;
            }

            afsi_log("Symlink[%d] = %s",dwIndex, line);

            /* find the trailing dot; null terminate after it */
            t2 = strrchr(line, '.');
            if (t2)
                *(t2+1) = '\0';

            for ( t=line;*t;t++ ) {
                if ( !isprint(*t) ) {
                    afsi_log("error occurred while parsing symlink entry [%d]: non-printable character", dwIndex);
                    fprintf(stderr, "error occurred while parsing symlink entry [%d]: non-printable character", dwIndex);
                    cm_noLocalMountPoints--;
                    continue;
                }
            }

            // line is not empty, so let's parse it
            t = strchr(line, ':');

            // make sure that there is a ':' separator in the line
            if (!t) {
                afsi_log("error occurred while parsing symlink entry [%d]: no ':' separator", dwIndex);
                fprintf(stderr, "error occurred while parsing symlink entry [%d]: no ':' separator", dwIndex);
                cm_noLocalMountPoints--;
                continue;
            }

            aLocalMountPoint->fileType = CM_SCACHETYPE_SYMLINK;
            aLocalMountPoint->namep=malloc(t-line+1);
            strncpy(aLocalMountPoint->namep, line, t-line);
            aLocalMountPoint->namep[t-line] = '\0';
		
            /* copy the symlink string */
            aLocalMountPoint->mountPointStringp=malloc(strlen(t)-1);
            strncpy(aLocalMountPoint->mountPointStringp, t+1, strlen(t)-2);
            aLocalMountPoint->mountPointStringp[strlen(t)-2] = '\0';
    
            osi_Log2(afsd_logp,"found symlink: name %s, string %s",
                      osi_LogSaveString(afsd_logp,aLocalMountPoint->namep),
                      osi_LogSaveString(afsd_logp,aLocalMountPoint->mountPointStringp));

            aLocalMountPoint++;
        }

        if ( hkFreelanceSymlinks )
            RegCloseKey( hkFreelanceSymlinks );
        RegCloseKey(hkFreelance);
        return 0;
    }
#endif

    /* What follows is the old code to read freelance mount points 
     * out of a text file modified to copy the data into the registry
     */
    cm_GetConfigDir(hdir);
    strcat(hdir, AFS_FREELANCE_INI);
    // open the ini file for reading
    fp = fopen(hdir, "r");
    if (!fp) {
        /* look in the Windows directory where we used to store the file */
        GetWindowsDirectory(hdir, sizeof(hdir));
        strcat(hdir,"\\");
        strcat(hdir, AFS_FREELANCE_INI);
        fp = fopen(hdir, "r");
    }

#if !defined(DJGPP)
    RegCreateKeyEx( HKEY_LOCAL_MACHINE, 
                    AFSREG_CLT_OPENAFS_SUBKEY "\\Freelance",
                    0,
                    NULL,
                    REG_OPTION_NON_VOLATILE,
                    KEY_READ|KEY_WRITE,
                    NULL,
                    &hkFreelance,
                    NULL);
    dwIndex = 0;
#endif

    if (!fp) {
#if !defined(DJGPP)
        RegCloseKey(hkFreelance);
#endif
        rootCellName[0] = '.';
      	code = cm_GetRootCellName(&rootCellName[1]);
        if (code == 0) {
            cm_FreelanceAddMount(&rootCellName[1], &rootCellName[1], "root.cell.", 0, NULL);
            cm_FreelanceAddMount(rootCellName, &rootCellName[1], "root.cell.", 1, NULL);
            cm_FreelanceAddMount(".root", &rootCellName[1], "root.afs.", 1, NULL);
        }
        return 0;
    }

    // we successfully opened the file
    osi_Log0(afsd_logp,"opened afs_freelance.ini");
	
    // now we read the first line to see how many entries
    // there are
    fgets(line, sizeof(line), fp);

    // if the line is empty at any point when we're reading
    // we're screwed. report error and return.
    if (*line==0) {
        afsi_log("error occurred while reading afs_freelance.ini");
        fprintf(stderr, "error occurred while reading afs_freelance.ini");
        return -1;
    }

    // get the number of entries there are from the first line
    // that we read
    cm_noLocalMountPoints = atoi(line);

    if (cm_noLocalMountPoints > 0) {
        // create space to store the local mount points
        cm_localMountPoints = malloc(sizeof(cm_localMountPoint_t) * cm_noLocalMountPoints);
        aLocalMountPoint = cm_localMountPoints;
    }

    // now we read n lines and parse them into local mount points
    // where n is the number of local mount points there are, as
    // determined above.
    // Each line in the ini file represents 1 local mount point and 
    // is in the format xxx#yyy:zzz, where xxx is the directory
    // entry name, yyy is the cell name and zzz is the volume name.
    // #yyy:zzz together make up the mount point.
    for (i=0; i<cm_noLocalMountPoints; i++) {
        fgets(line, sizeof(line), fp);
        // check that the line is not empty
        if (line[0]==0) {
            afsi_log("error occurred while parsing entry in %s: empty line in line %d", AFS_FREELANCE_INI, i);
            fprintf(stderr, "error occurred while parsing entry in afs_freelance.ini: empty line in line %d", i);
            return -1;
        }

        /* find the trailing dot; null terminate after it */
        t2 = strrchr(line, '.');
        if (t2)
            *(t2+1) = '\0';

#if !defined(DJGPP)
        if ( hkFreelance ) {
            char szIndex[16];
            /* we are migrating to the registry */
            sprintf(szIndex,"%d",dwIndex++);
            dwType = REG_SZ;
            dwSize = (DWORD)strlen(line) + 1;
            RegSetValueEx( hkFreelance, szIndex, 0, dwType, line, dwSize);
        }
#endif 

        // line is not empty, so let's parse it
        t = strchr(line, '#');
        if (!t)
            t = strchr(line, '%');
        // make sure that there is a '#' or '%' separator in the line
        if (!t) {
            afsi_log("error occurred while parsing entry in %s: no # or %% separator in line %d", AFS_FREELANCE_INI, i);
            fprintf(stderr, "error occurred while parsing entry in afs_freelance.ini: no # or %% separator in line %d", i);
            return -1;
        }
        aLocalMountPoint->namep=malloc(t-line+1);
        memcpy(aLocalMountPoint->namep, line, t-line);
        *(aLocalMountPoint->namep + (t-line)) = 0;

        aLocalMountPoint->mountPointStringp=malloc(strlen(line) - (t-line) + 1);
        memcpy(aLocalMountPoint->mountPointStringp, t, strlen(line)-(t-line)-1);
        *(aLocalMountPoint->mountPointStringp + (strlen(line)-(t-line)-1)) = 0;

        osi_Log2(afsd_logp,"found mount point: name %s, string %s",
                  aLocalMountPoint->namep,
                  aLocalMountPoint->mountPointStringp);

        aLocalMountPoint++;
    }
    fclose(fp);
#if !defined(DJGPP)
    if ( hkFreelance ) {
        RegCloseKey(hkFreelance);
        DeleteFile(hdir);
    }
#endif
    return 0;
}

int cm_getNoLocalMountPoints() {
    return cm_noLocalMountPoints;
}

#if !defined(DJGPP)
long cm_FreelanceMountPointExists(char * filename, int prefix_ok)
{
    char* cp;
    char line[512];
    char shortname[200];
    int found = 0;
    HKEY hkFreelance = 0;
    DWORD dwType, dwSize;
    DWORD dwMountPoints;
    DWORD dwIndex;
        
    lock_ObtainMutex(&cm_Freelance_Lock);

    if (RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
                      AFSREG_CLT_OPENAFS_SUBKEY "\\Freelance",
                      0,
                      KEY_READ|KEY_QUERY_VALUE,
                      &hkFreelance) == ERROR_SUCCESS) 
    {
        RegQueryInfoKey( hkFreelance,
                         NULL,  /* lpClass */
                         NULL,  /* lpcClass */
                         NULL,  /* lpReserved */
                         NULL,  /* lpcSubKeys */
                         NULL,  /* lpcMaxSubKeyLen */
                         NULL,  /* lpcMaxClassLen */
                         &dwMountPoints, /* lpcValues */
                         NULL,  /* lpcMaxValueNameLen */
                         NULL,  /* lpcMaxValueLen */
                         NULL,  /* lpcbSecurityDescriptor */
                         NULL   /* lpftLastWriteTime */
                         );

        for ( dwIndex = 0; dwIndex < dwMountPoints; dwIndex++ ) {
            TCHAR szValueName[16];
            DWORD dwValueSize = 16;
            dwSize = sizeof(line);
            RegEnumValue( hkFreelance, dwIndex, szValueName, &dwValueSize, NULL,
                          &dwType, line, &dwSize);

            cp=strchr(line, '#');
            if (!cp)
                cp=strchr(line, '%');
            memcpy(shortname, line, cp-line);
            shortname[cp-line]=0;

            if (!strcmp(shortname, filename)) {
                found = 1;
                break;
            }
        }
        for ( dwIndex = 0; dwIndex < dwMountPoints; dwIndex++ ) {
            TCHAR szValueName[16];
            DWORD dwValueSize = 16;
            dwSize = sizeof(line);
            RegEnumValue( hkFreelance, dwIndex, szValueName, &dwValueSize, NULL,
                          &dwType, line, &dwSize);

            cp=strchr(line, '#');
            if (!cp)
                cp=strchr(line, '%');
            memcpy(shortname, line, cp-line);
            shortname[cp-line]=0;

            if (!stricmp(shortname, filename)) {
                found = 1;
                break;
            }

            if (prefix_ok && strlen(shortname) - strlen(filename) == 1 && !strncmp(shortname, filename, strlen(filename))) {
                found = 1;
                break;
            }
        }
        RegCloseKey(hkFreelance);
    }

    lock_ReleaseMutex(&cm_Freelance_Lock);

    return found;
}

long cm_FreelanceSymlinkExists(char * filename, int prefix_ok)
{
    char* cp;
    char line[512];
    char shortname[200];
    int found = 0;
    HKEY hkFreelance = 0;
    DWORD dwType, dwSize;
    DWORD dwSymlinks;
    DWORD dwIndex;
        
    lock_ObtainMutex(&cm_Freelance_Lock);

    if (RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
                      AFSREG_CLT_OPENAFS_SUBKEY "\\Freelance\\Symlinks",
                      0,
                      KEY_READ|KEY_QUERY_VALUE,
                      &hkFreelance) == ERROR_SUCCESS) 
    {
        RegQueryInfoKey( hkFreelance,
                         NULL,  /* lpClass */
                         NULL,  /* lpcClass */
                         NULL,  /* lpReserved */
                         NULL,  /* lpcSubKeys */
                         NULL,  /* lpcMaxSubKeyLen */
                         NULL,  /* lpcMaxClassLen */
                         &dwSymlinks, /* lpcValues */
                         NULL,  /* lpcMaxValueNameLen */
                         NULL,  /* lpcMaxValueLen */
                         NULL,  /* lpcbSecurityDescriptor */
                         NULL   /* lpftLastWriteTime */
                         );

        for ( dwIndex = 0; dwIndex < dwSymlinks; dwIndex++ ) {
            TCHAR szValueName[16];
            DWORD dwValueSize = 16;
            dwSize = sizeof(line);
            RegEnumValue( hkFreelance, dwIndex, szValueName, &dwValueSize, NULL,
                          &dwType, line, &dwSize);

            cp=strchr(line, ':');
            memcpy(shortname, line, cp-line);
            shortname[cp-line]=0;

            if (!strcmp(shortname, filename)) {
                found = 1;
                break;
            }

            if (prefix_ok && strlen(shortname) - strlen(filename) == 1 && !strncmp(shortname, filename, strlen(filename))) {
                found = 1;
                break;
            }
        }
        for ( dwIndex = 0; dwIndex < dwSymlinks; dwIndex++ ) {
            TCHAR szValueName[16];
            DWORD dwValueSize = 16;
            dwSize = sizeof(line);
            RegEnumValue( hkFreelance, dwIndex, szValueName, &dwValueSize, NULL,
                          &dwType, line, &dwSize);

            cp=strchr(line, ':');
            memcpy(shortname, line, cp-line);
            shortname[cp-line]=0;

            if (!stricmp(shortname, filename)) {
                found = 1;
                break;
            }
        }
        RegCloseKey(hkFreelance);
    }

    lock_ReleaseMutex(&cm_Freelance_Lock);

    return found;
}
#endif

long cm_FreelanceAddMount(char *filename, char *cellname, char *volume, int rw, cm_fid_t *fidp)
{
    FILE *fp;
    char hfile[120];
    char line[512];
    char fullname[200];
    int n;
    int alias = 0;
#if !defined(DJGPP)
    HKEY hkFreelance = 0;
    DWORD dwType, dwSize;
    DWORD dwMountPoints;
    DWORD dwIndex;
#endif

    /* before adding, verify the cell name; if it is not a valid cell,
       don't add the mount point.
       allow partial matches as a means of poor man's alias. */
    /* major performance issue? */
    osi_Log4(afsd_logp,"Freelance Add Mount request: filename=%s cellname=%s volume=%s %s",
              osi_LogSaveString(afsd_logp,filename), 
              osi_LogSaveString(afsd_logp,cellname), 
              osi_LogSaveString(afsd_logp,volume), 
              rw ? "rw" : "ro");

    if ( filename[0] == '\0' || cellname[0] == '\0' || volume[0] == '\0' )
        return -1;

    if (cellname[0] == '.') {
        if (!cm_GetCell_Gen(&cellname[1], fullname, CM_FLAG_CREATE))
            return -1;
    } else {
        if (!cm_GetCell_Gen(cellname, fullname, CM_FLAG_CREATE))
            return -1;
    }

#if !defined(DJGPP)
    if ( cm_FreelanceMountPointExists(filename, 0) ||
         cm_FreelanceSymlinkExists(filename, 0) )
        return -1;
#endif
    
    osi_Log1(afsd_logp,"Freelance Adding Mount for Cell: %s", 
              osi_LogSaveString(afsd_logp,cellname));

    lock_ObtainMutex(&cm_Freelance_Lock);

#if !defined(DJGPP)
    if (RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
                      AFSREG_CLT_OPENAFS_SUBKEY "\\Freelance",
                      0,
                      KEY_READ|KEY_WRITE|KEY_QUERY_VALUE,
                      &hkFreelance) == ERROR_SUCCESS) {

        RegQueryInfoKey( hkFreelance,
                         NULL,  /* lpClass */
                         NULL,  /* lpcClass */
                         NULL,  /* lpReserved */
                         NULL,  /* lpcSubKeys */
                         NULL,  /* lpcMaxSubKeyLen */
                         NULL,  /* lpcMaxClassLen */
                         &dwMountPoints, /* lpcValues */
                         NULL,  /* lpcMaxValueNameLen */
                         NULL,  /* lpcMaxValueLen */
                         NULL,  /* lpcbSecurityDescriptor */
                         NULL   /* lpftLastWriteTime */
                         );

        if (rw)
            sprintf(line, "%s%%%s:%s", filename, fullname, volume);
        else
            sprintf(line, "%s#%s:%s", filename, fullname, volume);

        /* If we are adding a new value, there must be an unused name
         * within the range 0 to dwMountPoints 
         */
        for ( dwIndex = 0; dwIndex <= dwMountPoints; dwIndex++ ) {
            char szIndex[16];
            char szMount[1024];

            dwSize = sizeof(szMount);
            sprintf(szIndex, "%d", dwIndex);
            if (RegQueryValueEx( hkFreelance, szIndex, 0, &dwType, szMount, &dwSize) != ERROR_SUCCESS) {
                /* found an unused value */
                dwType = REG_SZ;
                dwSize = (DWORD)strlen(line) + 1;
                RegSetValueEx( hkFreelance, szIndex, 0, dwType, line, dwSize);
                break;
            } else {
                int len = (int)strlen(filename);
                if ( dwType == REG_SZ && !strncmp(filename, szMount, len) && 
                     (szMount[len] == '%' || szMount[len] == '#')) {
                    /* Replace the existing value */
                    dwType = REG_SZ;
                    dwSize = (DWORD)strlen(line) + 1;
                    RegSetValueEx( hkFreelance, szIndex, 0, dwType, line, dwSize);
                    break;
                }
            }
        }
        RegCloseKey(hkFreelance);
    } else 
#endif
    {
        cm_GetConfigDir(hfile);
        strcat(hfile, AFS_FREELANCE_INI);
        fp = fopen(hfile, "r+");
        if (!fp)
            return CM_ERROR_INVAL;
        fgets(line, sizeof(line), fp);
        n = atoi(line);
        n++;
        fseek(fp, 0, SEEK_SET);
        fprintf(fp, "%d", n);
        fseek(fp, 0, SEEK_END);
        if (rw)
            fprintf(fp, "%s%%%s:%s\n", filename, fullname, volume);
        else
            fprintf(fp, "%s#%s:%s\n", filename, fullname, volume);
        fclose(fp);
    }
    lock_ReleaseMutex(&cm_Freelance_Lock);

    /* cm_reInitLocalMountPoints(); */
    if (fidp) {
        fidp->unique = 1;
        fidp->vnode = cm_noLocalMountPoints + 1;   /* vnode value of last mt pt */
    }
    cm_noteLocalMountPointChange();
    return 0;
}

long cm_FreelanceRemoveMount(char *toremove)
{
    int i, n;
    char* cp;
    char line[512];
    char shortname[200];
    char hfile[120], hfile2[120];
    FILE *fp1, *fp2;
    int found=0;
#if !defined(DJGPP)
    HKEY hkFreelance = 0;
    DWORD dwType, dwSize;
    DWORD dwMountPoints;
    DWORD dwIndex;
#endif

    lock_ObtainMutex(&cm_Freelance_Lock);

#if !defined(DJGPP)
    if (RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
                      AFSREG_CLT_OPENAFS_SUBKEY "\\Freelance",
                      0,
                      KEY_READ|KEY_WRITE|KEY_QUERY_VALUE,
                      &hkFreelance) == ERROR_SUCCESS) {

        RegQueryInfoKey( hkFreelance,
                         NULL,  /* lpClass */
                         NULL,  /* lpcClass */
                         NULL,  /* lpReserved */
                         NULL,  /* lpcSubKeys */
                         NULL,  /* lpcMaxSubKeyLen */
                         NULL,  /* lpcMaxClassLen */
                         &dwMountPoints, /* lpcValues */
                         NULL,  /* lpcMaxValueNameLen */
                         NULL,  /* lpcMaxValueLen */
                         NULL,  /* lpcbSecurityDescriptor */
                         NULL   /* lpftLastWriteTime */
                         );

        for ( dwIndex = 0; dwIndex < dwMountPoints; dwIndex++ ) {
            TCHAR szValueName[16];
            DWORD dwValueSize = 16;
            dwSize = sizeof(line);
            RegEnumValue( hkFreelance, dwIndex, szValueName, &dwValueSize, NULL,
                          &dwType, line, &dwSize);

            cp=strchr(line, '#');
            if (!cp)
                cp=strchr(line, '%');
            memcpy(shortname, line, cp-line);
            shortname[cp-line]=0;

            if (!strcmp(shortname, toremove)) {
                RegDeleteValue( hkFreelance, szValueName );
                break;
            }
        }
        RegCloseKey(hkFreelance);
    } else 
#endif
    {
        cm_GetConfigDir(hfile);
        strcat(hfile, AFS_FREELANCE_INI);
        strcpy(hfile2, hfile);
        strcat(hfile2, "2");
        fp1=fopen(hfile, "r+");
        if (!fp1)
            return CM_ERROR_INVAL;
        fp2=fopen(hfile2, "w+");
        if (!fp2) {
            fclose(fp1);
            return CM_ERROR_INVAL;
        }

        fgets(line, sizeof(line), fp1);
        n=atoi(line);
        fprintf(fp2, "%d\n", n-1);

        for (i=0; i<n; i++) {
            fgets(line, sizeof(line), fp1);
            cp=strchr(line, '#');
            if (!cp)
                cp=strchr(line, '%');
            memcpy(shortname, line, cp-line);
            shortname[cp-line]=0;

            if (strcmp(shortname, toremove)==0) {

            } else {
                found = 1;
                fputs(line, fp2);
            }
        }

        fclose(fp1);
        fclose(fp2);
        if (!found)
            return CM_ERROR_NOSUCHFILE;

        unlink(hfile);
        rename(hfile2, hfile);
    }
    
    lock_ReleaseMutex(&cm_Freelance_Lock);
    cm_noteLocalMountPointChange();
    return 0;
}

long cm_FreelanceAddSymlink(char *filename, char *destination, cm_fid_t *fidp)
{
    char line[512];
    char fullname[200];
    int alias = 0;
#if !defined(DJGPP)
    HKEY hkFreelanceSymlinks = 0;
    DWORD dwType, dwSize;
    DWORD dwSymlinks;
    DWORD dwIndex;
#endif

    /* before adding, verify the filename.  If it is already in use, either as 
     * as mount point or a cellname, do not permit the creation of the symlink.
     */
    osi_Log2(afsd_logp,"Freelance Add Symlink request: filename=%s destination=%s",
              osi_LogSaveString(afsd_logp,filename), 
              osi_LogSaveString(afsd_logp,destination));
    
    if ( filename[0] == '\0' || destination[0] == '\0' )
        return CM_ERROR_INVAL;

    fullname[0] = '\0';
    if (filename[0] == '.') {
        cm_GetCell_Gen(&filename[1], fullname, CM_FLAG_CREATE);
        if (stricmp(&filename[1],fullname) == 0)
            return CM_ERROR_EXISTS;
    } else {
        cm_GetCell_Gen(filename, fullname, CM_FLAG_CREATE);
        if (stricmp(filename,fullname) == 0)
            return CM_ERROR_EXISTS;
    }

#if !defined(DJGPP)
    if ( cm_FreelanceMountPointExists(filename, 0) ||
         cm_FreelanceSymlinkExists(filename, 0) )
        return CM_ERROR_EXISTS;
#endif

    lock_ObtainMutex(&cm_Freelance_Lock);

#if !defined(DJGPP)
    if (RegCreateKeyEx( HKEY_LOCAL_MACHINE, 
                        AFSREG_CLT_OPENAFS_SUBKEY "\\Freelance\\Symlinks",
                        0,
                        NULL,
                        REG_OPTION_NON_VOLATILE,
                        KEY_READ|KEY_WRITE|KEY_QUERY_VALUE,
                        NULL,
                        &hkFreelanceSymlinks,
                        NULL) == ERROR_SUCCESS) {

        RegQueryInfoKey( hkFreelanceSymlinks,
                         NULL,  /* lpClass */
                         NULL,  /* lpcClass */
                         NULL,  /* lpReserved */
                         NULL,  /* lpcSubKeys */
                         NULL,  /* lpcMaxSubKeyLen */
                         NULL,  /* lpcMaxClassLen */
                         &dwSymlinks, /* lpcValues */
                         NULL,  /* lpcMaxValueNameLen */
                         NULL,  /* lpcMaxValueLen */
                         NULL,  /* lpcbSecurityDescriptor */
                         NULL   /* lpftLastWriteTime */
                         );

        sprintf(line, "%s:%s.", filename, destination);

        /* If we are adding a new value, there must be an unused name
         * within the range 0 to dwSymlinks 
         */
        for ( dwIndex = 0; dwIndex <= dwSymlinks; dwIndex++ ) {
            char szIndex[16];
            char szLink[1024];

            dwSize = sizeof(szLink);
            sprintf(szIndex, "%d", dwIndex);
            if (RegQueryValueEx( hkFreelanceSymlinks, szIndex, 0, &dwType, szLink, &dwSize) != ERROR_SUCCESS) {
                /* found an unused value */
                dwType = REG_SZ;
                dwSize = (DWORD)strlen(line) + 1;
                RegSetValueEx( hkFreelanceSymlinks, szIndex, 0, dwType, line, dwSize);
                break;
            } else {
                int len = (int)strlen(filename);
                if ( dwType == REG_SZ && !strncmp(filename, szLink, len) && szLink[len] == ':') {
                    /* Replace the existing value */
                    dwType = REG_SZ;
                    dwSize = (DWORD)strlen(line) + 1;
                    RegSetValueEx( hkFreelanceSymlinks, szIndex, 0, dwType, line, dwSize);
                    break;
                }
            }
        }
        RegCloseKey(hkFreelanceSymlinks);
    } 
#endif
    lock_ReleaseMutex(&cm_Freelance_Lock);

    /* cm_reInitLocalMountPoints(); */
    if (fidp) {
        fidp->unique = 1;
        fidp->vnode = cm_noLocalMountPoints + 1;   /* vnode value of last mt pt */
    }
    cm_noteLocalMountPointChange();
    return 0;
}

long cm_FreelanceRemoveSymlink(char *toremove)
{
    char* cp;
    char line[512];
    char shortname[200];
    int found=0;
#if !defined(DJGPP)
    HKEY hkFreelanceSymlinks = 0;
    DWORD dwType, dwSize;
    DWORD dwSymlinks;
    DWORD dwIndex;
#endif

    lock_ObtainMutex(&cm_Freelance_Lock);

#if !defined(DJGPP)
    if (RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
                      AFSREG_CLT_OPENAFS_SUBKEY "\\Freelance\\Symlinks",
                      0,
                      KEY_READ|KEY_WRITE|KEY_QUERY_VALUE,
                      &hkFreelanceSymlinks) == ERROR_SUCCESS) {

        RegQueryInfoKey( hkFreelanceSymlinks,
                         NULL,  /* lpClass */
                         NULL,  /* lpcClass */
                         NULL,  /* lpReserved */
                         NULL,  /* lpcSubKeys */
                         NULL,  /* lpcMaxSubKeyLen */
                         NULL,  /* lpcMaxClassLen */
                         &dwSymlinks, /* lpcValues */
                         NULL,  /* lpcMaxValueNameLen */
                         NULL,  /* lpcMaxValueLen */
                         NULL,  /* lpcbSecurityDescriptor */
                         NULL   /* lpftLastWriteTime */
                         );

        for ( dwIndex = 0; dwIndex < dwSymlinks; dwIndex++ ) {
            TCHAR szValueName[16];
            DWORD dwValueSize = 16;
            dwSize = sizeof(line);
            RegEnumValue( hkFreelanceSymlinks, dwIndex, szValueName, &dwValueSize, NULL,
                          &dwType, line, &dwSize);

            cp=strchr(line, ':');
            memcpy(shortname, line, cp-line);
            shortname[cp-line]=0;

            if (!strcmp(shortname, toremove)) {
                RegDeleteValue( hkFreelanceSymlinks, szValueName );
                break;
            }
        }
        RegCloseKey(hkFreelanceSymlinks);
    }
#endif
    
    lock_ReleaseMutex(&cm_Freelance_Lock);
    cm_noteLocalMountPointChange();
    return 0;
}
#endif /* AFS_FREELANCE_CLIENT */
