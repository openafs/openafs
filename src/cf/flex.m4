# OPENAFS_LEX_IS_FLEX([ACTION-IF-SUCCESS], [ACTION-IF-FAILURE])
AC_DEFUN([OPENAFS_LEX_IS_FLEX],
[AC_MSG_CHECKING([if lex is flex])
 AS_IF([echo '' | $LEX --version >&AS_MESSAGE_LOG_FD 2>&1],
    [AC_MSG_RESULT([yes])
     $1],
    [AC_MSG_RESULT([no])
     $2])])
