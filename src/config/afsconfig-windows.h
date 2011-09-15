/* src/config/afsconfig.h.in.  Generated automatically from configure.in by autoheader.  */
/* Modified for Win2000 build*/

/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
#undef _ALL_SOURCE
#endif

/* Define to empty if the keyword does not work.  */
#undef const

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#undef HAVE_SYS_WAIT_H

/* Define as __inline if that's what the C compiler calls it.  */
#define inline __inline

/* Define if on MINIX.  */
#undef _MINIX

/* Define to `int' if <sys/types.h> doesn't define.  */
#undef pid_t

/* Define if the system does not provide POSIX.1 features except
   with this defined.  */
#undef _POSIX_1_SOURCE

/* Define if you need to in order for stat and other things to work.  */
#undef _POSIX_SOURCE

/* Define as the return type of signal handlers (int or void).  */
#undef RETSIGTYPE


/* The size of `long', as computed by sizeof. */
#define SIZEOF_LONG 4

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
//#undef size_t unsigned int

/* Define if you have the ANSI C header files.  */
#undef STDC_HEADERS

/* Define if lex declares yytext as a char * by default, not a char[].  */
#undef YYTEXT_POINTER

/* Define if you have the connect function.  */
#undef HAVE_CONNECT

/* Define if you have the error_message function.  */
#define HAVE_ERROR_MESSAGE

/* Define if you have the gethostbyname function.  */
#define HAVE_GETHOSTBYNAME

/* Define if you have the res_search function.  */
#undef HAVE_RES_SEARCH

/* Define if you have the snprintf function.  */
#undef HAVE_SNPRINTF

/* Define if you have the socket function.  */
#define HAVE_SOCKET

/* Define if you have the strnlen function.  */
#define HAVE_STRNLEN 1

/* Define if you have the <direct.h> header file.  */
#undef HAVE_DIRECT_H

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H

/* Define if you have the <mntent.h> header file.  */
#undef HAVE_MNTENT_H

/* Define if you have the <ndir.h> header file.  */
#undef HAVE_NDIR_H

/* Define if you have the <netdb.h> header file.  */
#undef HAVE_NETDB_H

/* Define if you have the <netinet/in.h> header file.  */
#undef HAVE_NETINET_IN_H

/* Define if you have the <security/pam_modules.h> header file.  */
#undef HAVE_SECURITY_PAM_MODULES_H

/* Define if you have the <signal.h> header file.  */
#define HAVE_SIGNAL_H

/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H

/* Define if you have the <strings.h> header file.  */
#undef HAVE_STRINGS_H

/* Define if you have the <sys/dir.h> header file.  */
#undef HAVE_SYS_DIR_H

/* Define if you have the <sys/fcntl.h> header file.  */
#define HAVE_SYS_FCNTL_H

/* Define if you have the <sys/file.h> header file.  */
#undef HAVE_SYS_FILE_H

/* Define if you have the <sys/fs_types.h> header file.  */
#undef HAVE_SYS_FS_TYPES_H

/* Define if you have the <sys/mntent.h> header file.  */
#undef HAVE_SYS_MNTENT_H

/* Define if you have the <sys/mnttab.h> header file.  */
#undef HAVE_SYS_MNTTAB_H

/* Define if you have the <sys/mount.h> header file.  */
#undef HAVE_SYS_MOUNT_H

/* Define if you have the <sys/ndir.h> header file.  */
#undef HAVE_SYS_NDIR_H

/* Define if you have the <sys/param.h> header file.  */
#undef HAVE_SYS_PARAM_H

/* Define if you have the <sys/time.h> header file.  */
#undef HAVE_SYS_TIME_H

/* Define if you have the <sys/vfs.h> header file.  */
#undef HAVE_SYS_VFS_H

/* Define if you have the <termios.h> header file.  */
#undef HAVE_TERMIOS_H

/* Define if you have the <unistd.h> header file.  */
#undef HAVE_UNISTD_H

/* Define if you have the <windows.h> header file.  */
#define HAVE_WINDOWS_H

/* Define if you have the <winsock2.h> header file.  */
#define HAVE_WINSOCK2_H

/* Define if you have vsnprintf */
#define HAVE_VSNPRINTF 1

/* Define if you have snprintf */
#define HAVE_SNPRINTF 1

/* Name of package */
#undef PACKAGE

/* Version number of package */
#undef VERSION

/* define if target is big endian */
#undef WORDS_BIGENDIAN

/* define if sys/param.h defines the endiness */
#undef ENDIANESS_IN_SYS_PARAM_H

/* define if struct ufsvfs has vfs_dqrwlock */
#undef HAVE_VFS_DQRWLOCK

/* define if you have pread() and pwrite() */
#define HAVE_PIO 1

#undef PACKAGE
#undef VERSION

#undef HAVE_CONNECT
//#undef HAVE_GETHOSTBYNAME
#undef HAVE_RES_SEARCH
//#undef HAVE_SOCKET

#ifdef ENDIANESS_IN_SYS_PARAM_H
# ifndef KERNEL
#  include <sys/types.h>
#  include <sys/param.h>
#  if BYTE_ORDER == BIG_ENDIAN
#  define WORDS_BIGENDIAN 1
#  endif
# endif
#endif

#define AFS_NAMEI_ENV 1

#undef FAST_RESTART
#undef BITMAP_LATER

#undef INODE_SETATTR_NOT_VOID
#undef STRUCT_INODE_HAS_I_BYTES
#undef STRUCT_ADDRESS_SPACE_HAS_PAGE_LOCK

/* glue for RedHat kernel bug */
#undef ENABLE_REDHAT_BUILDSYS

#if defined(ENABLE_REDHAT_BUILDSYS) && defined(KERNEL) && defined(REDHAT_FIX)
#include "redhat-fix.h"
#endif

/* Windows does not provide socklen_t prior to WDK 6.0 */
#define HAVE_SOCKLEN_T 1
typedef int socklen_t;

#if (_MSC_VER < 1400)
typedef int errno_t;
#endif

/*
 * UNIX mode symbol definitions
 */
#ifndef S_IRWXU
#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100
#endif

#ifndef S_IRWXG
#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010
#endif

#ifndef S_IRWXO
#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001
#endif

#ifndef S_ISUID
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000
#endif

#ifndef S_ISDIR
#define S_ISDIR(m)      (((m)&(S_IFMT)) == (S_IFDIR))
#endif

#define HAVE_CONIO_H 1
#define HAVE_KRB5_CREDS_KEYBLOCK_ENCTYPE 1
