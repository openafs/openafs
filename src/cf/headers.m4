AC_DEFUN([OPENAFS_HEADER_CHECKS],[
dnl checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_DIRENT
AC_CHECK_HEADERS([ \
	curses.h \
	direct.h \
	fcntl.h \
	grp.h \
	mntent.h \
	ncurses.h \
	ncurses/ncurses.h \
	netinet/in.h \
	netdb.h \
	regex.h \
	search.h \
	security/pam_modules.h \
	signal.h \
	stdio_ext.h \
	stdlib.h \
	string.h \
	strings.h \
	sys/bitypes.h \
	sys/fcntl.h \
	sys/file.h \
	sys/fs_types.h \
	sys/fstyp.h \
	sys/ipc.h \
	sys/mntent.h \
	sys/mnttab.h \
	sys/mount.h \
	sys/pag.h \
	sys/param.h \
	sys/resource.h \
	sys/statfs.h \
	sys/statvfs.h \
	sys/time.h \
	sys/vfs.h \
	termios.h \
	ucontext.h \
	unistd.h \
	windows.h \
])

AC_CHECK_HEADERS(linux/errqueue.h,,,[#include <linux/types.h>])

AC_CHECK_TYPES([fsblkcnt_t],,,[
#include <sys/types.h>
#ifdef HAVE_SYS_BITYPES_H
#include <sys/bitypes.h>
#endif
#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif
])
])
