/*
 * $Revision: 1.3 $
 */

/*
 * libfs.h:
 *    Contains declarations and macro definitions utilized by source
 *    files that make up the libfs library.
 *
 *    This file is local to the build environment.
 *    One can include this header in their code with the statement:
 *        #include <sys/libfs.h>
 */

/*
 * This file is part of the HP-YUX source code and shouls not be released
 * to AFS  customers unless thay have a source license
 */

#ifndef _STDIO_INCLUDED
#  include <stdio.h>         /* For FILE definition */
#endif
#ifndef _SYS_FS_INCLUDED
#  include <sys/fs.h>        /* For UFS_*(fs) macro definitions */
#endif

/*
 * Useful macros used by many FS commands
 */

#define VALID_FS_MAGIC(fs)    UFS_VALID_FS_MAGIC(fs)   /* in <sys/fs.h> */
#define LONG_FILENAME_FS(fs)  UFS_LFN_FS(fs)           /* in <sys/fs.h> */

/*
 * Return Values from LongFilenameOK()
 */
#define LFN_ERROR             (-1)
#define LFN_NOT_OK		0
#define LFN_OK			1

/*
 * Values returned by setup_block_seek()
 */
#define BLKSEEK_PROCESSING_ERROR   (-2)
#define BLKSEEK_FILE_WRITEONLY     (-1)
#define BLKSEEK_NOT_ENABLED         0
#define BLKSEEK_ENABLED             1

/* 
 * extern declarations of libfs routines.
 */
typedef int   boolean;

#ifdef _PROTOTYPES
  extern boolean is_mounted(char *);
  extern boolean IsSwap(char *);
  extern FILE    *mnt_setmntent(char *, char *);
  extern int     LongFilenameOK(char *);
  extern char    *fserror(char *);
  extern void    set_fserror(char *);
  extern int     setup_block_seek(int);
  extern int     setup_block_seek_2(int,int *);
#else
  extern boolean is_mounted();
  extern boolean IsSwap();
  extern FILE    *mnt_setmntent();
  extern int     LongFilenameOK();
  extern char    *fserror();
  extern void    set_fserror();
  extern int     setup_block_seek();
  extern int     setup_block_seek_2();
#endif

