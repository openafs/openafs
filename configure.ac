AC_PREREQ([2.64])
AC_INIT([OpenAFS],[m4_esyscmd(build-tools/git-version .)],[openafs-bugs@openafs.org],[],[http://www.openafs.org/])
AC_CONFIG_AUX_DIR([build-tools])
AC_CONFIG_MACRO_DIR([src/cf])
AC_CONFIG_SRCDIR([src/config/stds.h])

AC_CONFIG_HEADER([src/config/afsconfig.h])
MACOS_VERSION=1.9.1

AC_SUBST([MACOS_VERSION])

dnl If the user hasn't specified CFLAGS don't let configure pick -g -O2
AS_IF([test -z "$CFLAGS"], [CFLAGS=" "])

AC_USE_SYSTEM_EXTENSIONS

AC_PROG_CC

AC_PATH_PROGS([PATH_CPP], [cpp], [${CC-cc} -E], [$PATH:/lib:/usr/ccs/lib])
AC_SUBST([PATH_CPP])
OPENAFS_CONFIGURE_COMMON
OPENAFS_KRB5
OPENAFS_GSS

dnl Checks for summary
OPENAFS_SUMMARY_CHECK_NAMEI

AS_IF([test -d "$srcdir/doc/man-pages"],
    [MAN_MAKEFILE="doc/man-pages/Makefile doc/man-pages/install-man"],
    [MAN_MAKEFILE=])
AS_IF([test -f "$srcdir/doc/xml/AdminGuide/Makefile.in"],
    [ADMINGUIDE_MAKEFILE="doc/xml/AdminGuide/Makefile"],
    [ADMINGUIDE_MAKEFILE=])
AS_IF([test -f "$srcdir/doc/xml/QuickStartUnix/Makefile.in"],
    [QSUNIX_MAKEFILE="doc/xml/QuickStartUnix/Makefile"],
    [QSUNIX_MAKEFILE=])
AS_IF([test -f "$srcdir/doc/xml/UserGuide/Makefile.in"],
    [USERGUIDE_MAKEFILE="doc/xml/UserGuide/Makefile"],
    [USERGUIDE_MAKEFILE=])
AS_IF([test -f "$srcdir/doc/xml/AdminRef/Makefile.in"],
    [ADMINREF_MAKEFILE="doc/xml/AdminRef/Makefile"],
    [ADMINREF_MAKEFILE=])
AS_IF([test -f "$srcdir/doc/xml/mobi-fixup.xsl.in"],
    [MOBI_FIXUP_XSL="doc/xml/mobi-fixup.xsl"],
    [MOBI_FIXUP_XSL=])
AS_IF([test -f "$srcdir/doc/doxygen/Doxyfile.in"],
    [DOXYFILE="doc/doxygen/Doxyfile"],
    [DOXYFILE=])

AC_CONFIG_FILES([
    Makefile
    ${MAN_MAKEFILE}
    ${ADMINGUIDE_MAKEFILE}
    ${QSUNIX_MAKEFILE}
    ${USERGUIDE_MAKEFILE}
    ${ADMINREF_MAKEFILE}
    ${MOBI_FIXUP_XSL}
    ${DOXYFILE}
    src/afs/Makefile
    src/afsd/Makefile
    src/afsmonitor/Makefile
    src/afsweb/Makefile
    src/aklog/Makefile
    src/audit/Makefile
    src/auth/Makefile
    src/auth/test/Makefile
    src/bozo/Makefile
    src/bozo/test/Makefile
    src/bu_utils/Makefile
    src/bubasics/Makefile
    src/bucoord/Makefile
    src/budb/Makefile
    src/tbudb/Makefile
    src/butc/Makefile
    src/butm/Makefile
    src/cmd/Makefile
    src/cmd/test/Makefile
    src/comerr/Makefile
    src/comerr/test/Makefile
    src/config/Makefile
    src/config/Makefile.config
    src/config/Makefile.libtool
    src/config/Makefile.lwp
    src/config/Makefile.lwptool
    src/config/Makefile.pthread
    src/config/Makefile.version-CML
    src/config/Makefile.version-NOCML
    src/config/cc-wrapper
    src/config/shlib-build
    src/config/shlib-install
    src/crypto/hcrypto/Makefile
    src/crypto/rfc3961/Makefile
    src/dir/Makefile
    src/dir/test/Makefile
    src/dviced/Makefile
    src/dvolser/Makefile
    src/export/Makefile
    src/finale/Makefile
    src/fsint/Makefile
    src/fsprobe/Makefile
    src/gtx/Makefile
    src/JAVA/libjafs/Makefile
    src/kauth/Makefile
    src/kauth/test/Makefile
    src/kopenafs/Makefile
    src/libacl/Makefile
    src/libacl/test/Makefile
    src/libadmin/Makefile
    src/libadmin/adminutil/Makefile
    src/libadmin/bos/Makefile
    src/libadmin/cfg/Makefile
    src/libadmin/cfg/test/Makefile
    src/libadmin/client/Makefile
    src/libadmin/kas/Makefile
    src/libadmin/pts/Makefile
    src/libadmin/samples/Makefile
    src/libadmin/test/Makefile
    src/libadmin/vos/Makefile
    src/libafs/Makefile.common
    src/libafs/MakefileProto.${MKAFS_OSTYPE}
    ${DARWIN_PLIST}
    src/libafsauthent/Makefile
    src/libafsrpc/Makefile
    src/libuafs/Makefile.common
    src/libuafs/MakefileProto.${MKAFS_OSTYPE}
    src/log/Makefile
    src/log/test/Makefile
    src/lwp/Makefile
    src/lwp/test/Makefile
    src/opr/Makefile
    src/packaging/MacOS/OpenAFS-debug.Description.plist
    src/packaging/MacOS/OpenAFS-debug.Info.plist
    src/packaging/MacOS/OpenAFS.Description.plist
    src/packaging/MacOS/OpenAFS.Info.plist
    src/packaging/MacOS/OpenAFS.info
    src/packaging/MacOS/buildpkg.sh
    src/packaging/MacOS/pkgbuild.sh
    src/pam/Makefile
    src/platform/Makefile
    src/platform/${MKAFS_OSTYPE}/Makefile
    src/platform/DARWIN/growlagent/Makefile
    src/procmgmt/Makefile
    src/procmgmt/test/Makefile
    src/ptserver/Makefile
    src/tptserver/Makefile
    src/roken/Makefile
    src/rx/Makefile
    src/rx/bulk.example/Makefile
    src/rx/bulktest/Makefile
    src/rx/multi.example/Makefile
    src/rx/simple.example/Makefile
    src/rx/test/Makefile
    src/rxdebug/Makefile
    src/rxgen/Makefile
    src/rxgk/Makefile
    src/rxkad/Makefile
    src/rxkad/test/Makefile
    src/rxosd/Makefile
    src/rxstat/Makefile
    src/scout/Makefile
    src/sys/Makefile
    src/tbutc/Makefile
    src/tests/Makefile
    src/tests/run-tests
    src/tools/Makefile
    src/tools/dumpscan/Makefile
    src/tools/rxperf/Makefile
    src/tsalvaged/Makefile
    src/tsm41/Makefile
    src/tvolser/Makefile
    src/ubik/Makefile
    src/tubik/Makefile
    src/update/Makefile
    src/usd/Makefile
    src/usd/test/Makefile
    src/uss/Makefile
    src/util/Makefile
    src/util/test/Makefile
    src/libafscp/Makefile
    src/venus/Makefile
    src/venus/test/Makefile
    src/vfsck/Makefile
    src/viced/Makefile
    src/vlserver/Makefile
    src/tvlserver/Makefile
    src/vol/Makefile
    src/volser/Makefile
    src/xstat/Makefile
    src/helper-splint.sh
    tests/Makefile
    tests/auth/Makefile
    tests/cmd/Makefile
    tests/common/Makefile
    tests/opr/Makefile
    tests/rx/Makefile
    tests/tap/Makefile
    tests/util/Makefile
    tests/volser/Makefile])
AC_CONFIG_COMMANDS([default],[chmod a+x src/config/shlib-build
 chmod a+x src/config/shlib-install
 chmod a+x src/config/cc-wrapper],[])
AC_OUTPUT

# print a final summary
OPENAFS_SUMMARY
