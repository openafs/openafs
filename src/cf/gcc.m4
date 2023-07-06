dnl Checks for specific gcc behavior

dnl Helper to test for UAF warning message
dnl _OPENAFS_UAF_COMPILE_IFELSE([success], [fail])
AC_DEFUN([_OPENAFS_UAF_COMPILE_IFELSE],[
    AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM([[
		#include <stdlib.h>
		struct gcc_check {
		    char *ptr;
		};
		void test(struct gcc_check *p, char *cp, int size)
		{
		    p->ptr = realloc(cp, size);
		    if (p->ptr == NULL && size != 0) {
			free(cp);	/* If compiler has UAF bug this will be flagged */
		    }
		}
	    ]]
	)],
	[$1],
	[$2]
    )
])

dnl Check to see if the GCC compiler incorrectly flags use-after-free (UAF).
dnl This false positive has been observed with gcc 12 when
dnl optimization is disabled (-O0) and gcc 13.
AC_DEFUN([OPENAFS_GCC_UAF_BUG_CHECK],[
    CFLAGS_USE_AFTER_FREE_GCCBUG=
    AS_IF([test "x$GCC" = "xyes"], [
	AC_MSG_CHECKING([gcc use-after-free warning bug])
	ac_save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS -Wall -Werror -O0 -U_FORTIFY_SOURCE"
	_OPENAFS_UAF_COMPILE_IFELSE(
	    [AC_MSG_RESULT(no)],
	    [
		dnl Compiler flagged an error.  Run one more check to ensure
		dnl the error was only the false positive for a UAF.
		AX_APPEND_COMPILE_FLAGS([-Wno-use-after-free],
					[CFLAGS_USE_AFTER_FREE_GCCBUG], [-Werror])
		CFLAGS=" $CFLAGS $CFLAGS_USE_AFTER_FREE_GCCBUG"
		_OPENAFS_UAF_COMPILE_IFELSE(
		    [AC_MSG_RESULT(yes)],
		    [AC_MSG_ERROR([Unexpected compiler error while testing for gcc use-after-free bug])])
	    ]
	)
	CFLAGS="$ac_save_CFLAGS"
    ])
    AC_SUBST([CFLAGS_USE_AFTER_FREE_GCCBUG])
])
