dnl
dnl java autoconf glue
dnl

AC_DEFUN([OPENAFS_JAVA],[

AC_ARG_WITH([java_home], [--with-java_home Support for JAVA])

if test X$with_java_home == X; then
	if test X$JAVA_HOME != X; then
		with_java_home="$JAVA_HOME"
	fi
fi
if test X$with_java_home != X; then
	if test X$with_java_home != Xyes; then
		JAVA_LIVES_HERE="$withval";
	else
		JAVA_LIVES_HERE="/usr";
	fi
	java_sane=1
	if test -x "$JAVA_LIVES_HERE/bin/javac"; then :; else
AC_MSG_WARN([Problem with "$JAVA_LIVES_HERE/bin/javac"])
		java_sane=0
	fi
	if test -x "$JAVA_LIVES_HERE/bin/javah"; then :; else
AC_MSG_WARN([Problem with "$JAVA_LIVES_HERE/bin/javah"])
		java_sane=0
	fi
	if test -f "$JAVA_LIVES_HERE/include/jni.h"; then :; else
AC_MSG_WARN([Missing file "$JAVA_LIVES_HERE/include/jni.h"])
		java_sane=0
	fi
	if test X$java_sane != X1; then
		AC_MSG_ERROR([Bad JAVA_HOME=$JAVA_LIVES_HERE; fixme or do --without-java_home])
	fi
	DISABLE_JAVA='#'
else
	ENABLE_JAVA='#'
fi

AC_SUBST(JAVA_LIVES_HERE)
AC_SUBST(ENABLE_JAVA)
AC_SUBST(DISABLE_JAVA)

])dnl
