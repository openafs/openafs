
# OPENAFS_SUMMARY_CHECK_NAME
# --------------------------
#
# Check whether namei fileserver is enabled for this platform and configure
# options.  When namei is enabled with a configure option, the AFS_NAMEI_ENV
# will be defined for the test program. AFS_NAMEI_ENV can also be defined in
# the platform and sysname param headers for this platform. Avoid including the
# afs/afs_sysnames.h header since it has not been installed to the
# `include/afs' system directory yet (and is not needed for this namei check).
#
# Note that, if set, AFS_PARAM_COMMON is the header filename, including the .h
# suffix.
#
# This macro should be used before AC_OUTPUT so the results will be saved
# in the configure cache.
#
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

#
# OPENAFS_SUMMARY
# ---------------
#
# Print the configure summary.
#
AC_DEFUN([OPENAFS_SUMMARY],[
  AS_IF([test "x${LIB_curses}" = "x"],
    [summary_build_scout="no"],
    [summary_build_scout="yes"])
  AS_IF([test "x${LIB_curses}" = "x"],
    [summary_build_afsmonitor="no"],
    [summary_build_afsmonitor="yes"])
  AS_IF([test "x${DOXYGEN}" = "x"],
    [summary_doxygen="no"],
    [summary_doxygen="yes"])
  AS_IF([test "${summary_doxygen}" = "yes" -a "${HAVE_DOT}" = "yes"],
    [summary_doxygen_graphs="yes"],
    [summary_doxygen_graphs="no"])
  AS_IF([test "x${MAN_PAGES}" = "x"],
    [summary_man_pages="no"],
    [summary_man_pages="yes"])
  AS_IF([test "x${MAN_PAGES_HTML}" = "x"],
    [summary_man_pages_html="no"],
    [summary_man_pages_html="yes"])
  AS_IF([test "x$CTFCONVERT" != "x" -a "x$CTFMERGE" != "x"],
    [summary_ctf_tools="yes"],
    [summary_ctf_tools="no"])
  AS_IF([test "x$ENABLE_KERNEL_MODULE" != "x"],
    [summary_kernel_module="yes"],
    [summary_kernel_module="no"])

  AC_MSG_NOTICE([
***************************************************************
OpenAFS configure summary

Identification
  version                : ${PACKAGE_VERSION}
  sysname                : ${AFS_SYSNAME}

Compiler
  C compiler             : ${CC}
  Extra flags (XCFLAGS)  : ${XCFLAGS}

Linker
  krb5                   : ${KRB5_LIBS}
  curses                 : ${LIB_curses}
  afsdb                  : ${LIB_AFSDB}
  crypt                  : ${LIB_crypt}
  hcrypto                : ${LIB_hcrypto}
  intl                   : ${LIB_libintl}

Debug symbols
  userspace              : ${enable_debug}
  kernel                 : ${enable_debug_kernel}
  ctf-tools              : ${summary_ctf_tools}

Options
  transarc paths         : ${enable_transarc_paths}
  namei fileserver       : ${openafs_cv_summary_check_namei}
  use unix sockets       : ${USE_UNIX_SOCKETS}
  ptserver supergroups   : ${enable_supergroups}
  pthreaded ubik         : ${enable_pthreaded_ubik}
  install kauth          : ${INSTALL_KAUTH}
  ubik read while write  : ${enable_ubik_read_while_write}

Build components
  kernel module          : ${summary_kernel_module}
  scout                  : ${summary_build_scout}
  afsmonitor             : ${summary_build_afsmonitor}
  pam                    : ${HAVE_PAM}
  login                  : ${BUILD_LOGIN}

Generate documents
  doxygen pages          : ${summary_doxygen}
  doxygen diagrams       : ${summary_doxygen_graphs}
  man pages, roff format : ${summary_man_pages}
  man pages, html format : ${summary_man_pages_html}
***************************************************************
  ])
])
