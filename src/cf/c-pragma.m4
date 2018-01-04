dnl
dnl Test for _Pragma and how we need to use it
dnl
AC_DEFUN([OPENAFS_C_PRAGMA_TAUTOLOGICAL_POINTER_COMPARE],[
AC_MSG_CHECKING(for _Pragma recognition of -Wtautological-pointer-compare)
AC_CACHE_VAL(ac_cv__Pragma_tautological_pointer_compare, [
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
_Pragma("clang diagnostic error \"-Wunknown-pragmas\"")
_Pragma("clang diagnostic ignored \"-Wtautological-pointer-compare\"")

void func(void)
{
	return;
}
]])],
[ac_cv__Pragma_tautological_pointer_compare=yes],
[ac_cv__Pragma_tautological_pointer_compare=no])])
AC_MSG_RESULT($ac_cv__Pragma_tautological_pointer_compare)
])

AC_DEFUN([_OPENAFS_C_PRAGMA], [
AC_MSG_CHECKING(for _Pragma)
AC_CACHE_VAL(ac_cv__Pragma, [
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
_Pragma("")

void func(void)
{
	return;
}
]])],
[ac_cv__Pragma=yes],
[ac_cv__Pragma=no])])
AC_MSG_RESULT($ac_cv__Pragma)])

AC_DEFUN([OPENAFS_C_PRAGMA], [
_OPENAFS_C_PRAGMA
if test "$ac_cv__Pragma" = "yes"; then
  AC_DEFINE(HAVE__PRAGMA, 1, [define if your compiler has _Pragma])
  OPENAFS_C_PRAGMA_TAUTOLOGICAL_POINTER_COMPARE
  if test "$ac_cv__Pragma_tautological_pointer_compare" = "yes"; then
    AC_DEFINE(HAVE__PRAGMA_TAUTOLOGICAL_POINTER_COMPARE, 1,
      [define if your compiler has _Pragma and recognizes -Wtautological-pointer-compare])
  fi
fi
])
