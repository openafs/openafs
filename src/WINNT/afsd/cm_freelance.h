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

#define AFS_FREELANCE_INI "afs_freelance.ini"

#endif // _CM_FREELANCE_H
