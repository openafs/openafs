AC_DEFUN([OPENAFS_C_FLEXIBLE_ARRAY],[
  dnl Check to see if the compiler support C99 flexible arrays, e.g., var[]
  AC_MSG_CHECKING([for C99 flexible arrays])
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
      struct flexarraytest {
          int flag;
          int numbers[];
      };
      ]], [[]])
    ],
    [AC_MSG_RESULT([yes])
     AC_DEFINE([HAVE_FLEXIBLE_ARRAY], [1],
       [Define to 1 if your compiler supports C99 flexible arrays.])
    ],[AC_MSG_RESULT([no])]
  )
])
