AC_DEFUN([OPENAFS_LEX],[
AC_PROG_LEX
dnl if we are flex, be lex-compatible
OPENAFS_LEX_IS_FLEX([AC_SUBST([LEX], ["$LEX -l"])])
])
