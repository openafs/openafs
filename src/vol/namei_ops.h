/* User space file system operations for Unix platforms. */

#ifndef _AFS_NAMEI_OPS_H_H_
#define _AFS_NAMEI_OPS_H_H_

#ifdef AFS_NAMEI_ENV

#ifdef notdef
/* We don't include Unix afssyscalls.h, so: */
#define VALID_INO(I) ((I != (__int64)-1) && (I != (__int64)0))

/* minimum size of string to hand to PrintInode */
#define AFS_INO_STR_LENGTH 32
typedef char afs_ino_str_t[AFS_INO_STR_LENGTH];

char *PrintInode(char *s, Inode ino);
#endif

/* Basic file operations */
extern FILE *namei_fdopen(IHandle_t *h, char *fdperms);
extern int namei_unlink(char *name);

/* Inode operations */
extern Inode namei_MakeSpecIno(int volid, int type);
extern Inode namei_icreate(IHandle_t *h, char *p, int p1, int p2, int p3, int p4);
extern FD_t namei_iopen(IHandle_t *h);
extern int namei_irelease(IHandle_t *h);
int namei_iread(IHandle_t *h, int offset, char *buf, int size);
int namei_iwrite(IHandle_t *h, int offset, char *buf, int size);
extern int namei_dec(IHandle_t *h, Inode ino, int p1);
extern int namei_inc(IHandle_t *h, Inode ino, int p1);
extern int namei_GetLinkCount(FdHandle_t *h, Inode ino, int lockit);
extern int namei_ViceREADME(char *partition);
#include "nfs.h"
#include "viceinode.h"
int namei_ListAFSFiles(char *dev,
		    int (*write_fun)(FILE *fp, struct ViceInodeInfo *info,
				     char *dir, char *file),
		    FILE *fp,
		    int (*judge_fun)(struct ViceInodeInfo *info, int vid),
		    int singleVolumeNumber);
int ListViceInodes(char *devname, char *mountedOn, char *resultFile,
		   int (*judgeInode)(struct ViceInodeInfo *info, int vid),
		   int singleVolumeNumber, int *forcep,
		   int forceR, char *wpath);


#define NAMEI_LCOMP_LEN 32
#define NAMEI_SCOMP_LEN 12
#define NAMEI_PATH_LEN 256
typedef struct {
    char n_base[NAMEI_LCOMP_LEN];
    char n_voldir1[NAMEI_SCOMP_LEN];
    char n_voldir2[NAMEI_LCOMP_LEN];
    char n_dir1[NAMEI_SCOMP_LEN];
    char n_dir2[NAMEI_SCOMP_LEN];
    char n_inode[NAMEI_LCOMP_LEN];
    char n_path[NAMEI_PATH_LEN];
} namei_t;
void namei_HandleToName(namei_t *name, IHandle_t *h);

#endif /* AFS_NAMEI_ENV */
 
#endif /* _AFS_NAMEI_OPS_H_H_ */
