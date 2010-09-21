dnl Force $INSTALL to be an absolute path; some of the libafs build
dnl gets confused by a relative $INSTALL
AC_DEFUN([OPENAFS_FORCE_ABS_INSTALL],[
    if test "$INSTALL" = "${srcdir}/build-tools/install-sh -c" ||
       test "$INSTALL" = "build-tools/install-sh -c" ; then

	INSTALL=`cd "$srcdir"; pwd`/build-tools/install-sh
	if test -f "$INSTALL" ; then :; else
	    AC_MSG_ERROR([Error translating install-sh to an absolute path: $INSTALL does not exist?])
	fi
	INSTALL="$INSTALL -c"
    fi
])
