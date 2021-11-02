AC_DEFUN([OPENAFS_HEADER_CHECKS],[
dnl checks for header files.
AC_HEADER_SYS_WAIT
AC_HEADER_DIRENT
AC_CHECK_HEADERS([ \
    arpa/inet.h \
    arpa/nameser.h \
    curses.h\
    direct.h \
    errno.h \
    fcntl.h \
    grp.h \
    math.h \
    mntent.h \
    ncurses.h \
    ncurses/ncurses.h \
    netdb.h \
    netinet/in.h \
    pthread_np.h \
    pwd.h \
    regex.h \
    security/pam_appl.h \
    signal.h \
    stdint.h \
    stdio_ext.h \
    stdlib.h \
    string.h \
    strings.h \
    sys/bitypes.h \
    sys/bswap.h \
    sys/dk.h \
    sys/fcntl.h \
    sys/file.h \
    sys/fs_types.h \
    sys/fstyp.h \
    sys/ioctl.h \
    sys/ipc.h \
    sys/lockf.h \
    sys/map.h \
    sys/mount.h \
    sys/mntent.h \
    sys/mnttab.h \
    sys/pag.h \
    sys/param.h \
    sys/resource.h \
    sys/select.h \
    sys/statfs.h \
    sys/statvfs.h \
    sys/socket.h \
    sys/sysctl.h \
    sys/time.h \
    sys/types.h \
    sys/uio.h \
    sys/un.h \
    sys/vfs.h \
    syslog.h \
    termios.h \
    time.h \
    ucontext.h \
    unistd.h \
    windows.h \
])

AC_CHECK_HEADERS([resolv.h], [], [], [AC_INCLUDES_DEFAULT
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif])

AC_CHECK_HEADERS([net/if.h],[],[],[AC_INCLUDES_DEFAULT
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif])

AC_CHECK_HEADERS([netinet/if_ether.h],[],[],[AC_INCLUDES_DEFAULT
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef HAVE_NET_IF_H
# include <net/if.h>
#endif])

AC_CHECK_HEADERS([security/pam_modules.h],[],[],[AC_INCLUDES_DEFAULT
#ifdef HAVE_SECURITY_PAM_APPL_H
# include <security/pam_appl.h>
#endif])

AC_CHECK_HEADERS(linux/errqueue.h,,,[#include <linux/types.h>
#include <linux/time.h>])

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

AC_DEFUN([OPENAFS_NETDB_CHECKS],[
AC_CHECK_DECLS([h_errno], [], [], [
#include <sys/types.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
])
])
