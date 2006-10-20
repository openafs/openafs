#ifndef _CM_FREELANCE_H
#define _CM_FREELANCE_H 1


typedef struct cm_localMountPoint {
    char*                       namep;
    char*                       mountPointStringp;
    unsigned int                fileType;
    struct cm_localMountPoint*  next;
} cm_localMountPoint_t;

extern int cm_getNoLocalMountPoints();
extern long cm_InitLocalMountPoints();
extern int cm_getLocalMountPointChange();
extern int cm_reInitLocalMountPoints();
extern void cm_InitFreelance();
extern void cm_FreelanceShutdown(void);
extern int cm_noteLocalMountPointChange(void);
extern long cm_FreelanceRemoveMount(char *toremove);
extern long cm_FreelanceAddMount(char *filename, char *cellname, char *volume, int rw, cm_fid_t *fidp);
extern long cm_FreelanceRemoveSymlink(char *toremove);
extern long cm_FreelanceAddSymlink(char *filename, char *destination, cm_fid_t *fidp);
extern int cm_clearLocalMountPointChange();
extern int cm_FakeRootFid(cm_fid_t *fidp);

#define AFS_FREELANCE_INI "afs_freelance.ini"
#define AFS_FAKE_ROOT_CELL_ID 0xFFFFFFFF
#define AFS_FAKE_ROOT_VOL_ID  0xFFFFFFFF

extern time_t FakeFreelanceModTime;
#endif // _CM_FREELANCE_H
