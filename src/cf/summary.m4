dnl
dnl configure summary
dnl
dnl
dnl OPENAFS_SUMMARY_CHECK_NAME
dnl
dnl Check whether namei fileserver is enabled for this platform/configure options.
dnl When namei is enabled with a configure option, the AFS_NAMEI_ENV will be defined
dnl for the test program. AFS_NAMEI_ENV can also be defined in the platform and sysname
dnl param headers for this platform. Avoid including the afs/afs_sysnames.h header since
dnl it has not been installed to the `include/afs' system directory yet (and is not
dnl needed for this namei check).
dnl
dnl Note that, if set, AFS_PARAM_COMMON is the header filename, including the .h suffix.
dnl
AC_DEFUN([OPENAFS_SUMMARY_CHECK_NAMEI],
  [AC_CACHE_CHECK([whether namei fileserver is enabled], [openafs_cv_summary_check_namei],
    [rm -f conftestparam.h; touch conftestparam.h;  # automatically cleaned up by configure
    AS_IF([test "x${AFS_PARAM_COMMON}" != "x" && test -f "src/config/${AFS_PARAM_COMMON}"],
      [grep -v '#include <afs/afs_sysnames.h>' "src/config/${AFS_PARAM_COMMON}" >> conftestparam.h])
    AS_IF([test "x${AFS_SYSNAME}" != "x" && test -f "src/config/param.${AFS_SYSNAME}.h"],
      [grep -v '#include <afs/afs_sysnames.h>' "src/config/param.${AFS_SYSNAME}.h" >> conftestparam.h])
    AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[
#define IGNORE_STDS_H
#include "conftestparam.h"
        ]], [[
#ifndef AFS_NAMEI_ENV
    namei_disabled
#endif
        ]])],
      [openafs_cv_summary_check_namei="yes"],
      [openafs_cv_summary_check_namei="no"])
  ])
])
dnl
dnl OPENAFS_SUMMARY
dnl
dnl Print the configure summary.
dnl
AC_DEFUN([OPENAFS_SUMMARY],[
  AS_IF([test "x${LIB_curses}" = "x"],
    [summary_build_scout="no"],
    [summary_build_scout="yes"])
  AS_IF([test "x${DOCBOOK_STYLESHEETS}" = "x"],
    [summary_docbook_stylesheets="no"],
    [summary_docbook_stylesheets="yes"])
  AS_IF([test "x${DOXYGEN}" = "x"],
    [summary_doxygen="no"],
    [summary_doxygen="yes"])
  AS_IF([test "${summary_doxygen}" = "yes" -a "${HAVE_DOT}" = "yes"],
    [summary_doxygen_graphs="yes"],
    [summary_doxygen_graphs="no"])
  AS_IF([test "x$CTFCONVERT" != "x" -a "x$CTFMERGE" != "x"],
    [summary_ctf_tools="yes"],
    [summary_ctf_tools="no"])

  cat <<EOF
***************************************************************
OpenAFS configure summary

  version : ${VERSION}
  sysname : ${AFS_SYSNAME}

debug:
  userspace              : ${enable_debug}
  kernel                 : ${enable_debug_kernel}
  ctf-tools              : ${summary_ctf_tools}
options:
  transarc paths         : ${enable_transarc_paths}
  namei fileserver       : ${openafs_cv_summary_check_namei}
  use unix sockets       : ${USE_UNIX_SOCKETS}
  ptserver supergroups   : ${enable_supergroups}
  pthreaded ubik         : ${enable_pthreaded_ubik}
  install kauth          : ${INSTALL_KAUTH}
  ubik read while write  : ${enable_ubik_read_while_write}
build:
  scout/afsmonitor       : ${summary_build_scout}
  pam                    : ${HAVE_PAM}
  login                  : ${BUILD_LOGIN}
  uss                    : ${BUILD_USS}
doc generation:
  docbook stylesheets    : ${summary_docbook_stylesheets}
  doxygen                : ${summary_doxygen}
  doxygen graphs         : ${summary_doxygen_graphs}
libraries:
  krb5    : ${KRB5_LIBS}
  curses  : ${LIB_curses}
  afsdb   : ${LIB_AFSDB}
  crypt   : ${LIB_crypt}
  hcrypto : ${LIB_hcrypto}
  intl    : ${LIB_libintl}
***************************************************************
EOF
])
