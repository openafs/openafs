#ifndef _CM_FREELANCE_H
#define _CM_FREELANCE_H 1


typedef struct cm_localMountPoint {
	char* namep;
	char* mountPointStringp;
	struct cm_localMountPoint* next;
} cm_localMountPoint_t;

extern int cm_getNoLocalMountPoints();
extern long cm_InitLocalMountPoints();
extern int cm_getLocalMountPointChange();
extern int cm_reInitLocalMountPoints();
extern cm_localMountPoint_t* cm_getLocalMountPoint(int vnode);
extern void cm_InitFreelance();
extern long cm_FreelanceRemoveMount(char *toremove);
extern long cm_FreelanceAddMount(char *filename, char *cellname, char *volume, cm_fid_t *fidp);

#define AFS_FREELANCE_INI "afs_freelance.ini"
#define AFS_FAKE_ROOT_VOL_ID  0x00000001
#endif // _CM_FREELANCE_H
