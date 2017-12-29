AC_DEFUN([OPENAFS_TIVOLI_TESTS],[
dnl check for tivoli
AC_MSG_CHECKING(for tivoli tsm butc support)
XBSA_CFLAGS=""
if test "$enable_tivoli_tsm" = "yes"; then
        XBSADIR1=/usr/tivoli/tsm/client/api/bin/xopen
        XBSADIR2=/opt/tivoli/tsm/client/api/bin/xopen
        XBSADIR3=/usr/tivoli/tsm/client/api/bin/sample
        XBSADIR4=/opt/tivoli/tsm/client/api/bin/sample
        XBSADIR5=/usr/tivoli/tsm/client/api/bin64/sample
        XBSADIR6=/opt/tivoli/tsm/client/api/bin64/sample

        if test -r "$XBSADIR3/dsmapifp.h"; then
                XBSA_CFLAGS="-Dxbsa -DNEW_XBSA -I$XBSADIR3"
                XBSA_XLIBS="-ldl"
                AC_MSG_RESULT([yes, $XBSA_CFLAGS])
        elif test -r "$XBSADIR4/dsmapifp.h"; then
                XBSA_CFLAGS="-Dxbsa -DNEW_XBSA -I$XBSADIR4"
                XBSA_XLIBS="-ldl"
                AC_MSG_RESULT([yes, $XBSA_CFLAGS])
        elif test -r "$XBSADIR5/dsmapifp.h"; then
                XBSA_CFLAGS="-Dxbsa -DNEW_XBSA -I$XBSADIR5"
                XBSA_XLIBS="-ldl"
                AC_MSG_RESULT([yes, $XBSA_CFLAGS])
        elif test -r "$XBSADIR6/dsmapifp.h"; then
                XBSA_CFLAGS="-Dxbsa -DNEW_XBSA -I$XBSADIR6"
                XBSA_XLIBS="-ldl"
                AC_MSG_RESULT([yes, $XBSA_CFLAGS])
        elif test -r "$XBSADIR1/xbsa.h"; then
                XBSA_CFLAGS="-Dxbsa -I$XBSADIR1"
                XBSA_XLIBS=""
                AC_MSG_RESULT([yes, $XBSA_CFLAGS])
        elif test -r "$XBSADIR2/xbsa.h"; then
                XBSA_CFLAGS="-Dxbsa -I$XBSADIR2"
                XBSA_XLIBS=""
                AC_MSG_RESULT([yes, $XBSA_CFLAGS])
        else
                AC_MSG_RESULT([no, missing xbsa.h and dsmapifp.h header files])
        fi
else
        AC_MSG_RESULT([no])
fi
AC_SUBST(XBSA_CFLAGS)
AC_SUBST(XBSA_XLIBS)
XLIBS="$XBSA_XLIBS $XLIBS"
])
