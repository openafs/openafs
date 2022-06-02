dnl
dnl GCC-style function __attribute__ checks.
dnl
dnl Define HAVE___ATTRIBUTE__ if and only if we specifically support the
dnl `format' function attribute. This is done for the imported roken
dnl headers, which use that symbol to conditionally declare functions with
dnl printf-like arguments. This is the only use of function attributes in
dnl roken.  The HAVE___ATTRIBUTE__ symbol is not used in the OpenAFS code.
dnl
AC_DEFUN([OPENAFS_C_ATTRIBUTE], [
  AX_GCC_FUNC_ATTRIBUTE([always_inline])
  AX_GCC_FUNC_ATTRIBUTE([format])
  AX_GCC_FUNC_ATTRIBUTE([nonnull])
  AX_GCC_FUNC_ATTRIBUTE([noreturn])
  AX_GCC_FUNC_ATTRIBUTE([unused])

  AS_IF([test "$ax_cv_have_func_attribute_format" = "yes"], [
    AC_DEFINE([HAVE___ATTRIBUTE__], [1],
      [define if your compiler has __attribute__((format))])
  ])
])
