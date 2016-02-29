AC_DEFUN([OPENAFS_C_STRUCT_LABEL_CHECK],[
dnl Check to see if the compiler support labels in structs
AC_MSG_CHECKING(for label support in structs)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
extern void osi_UFSOpen(void);
struct labeltest {
   void (*open) (void);
};
struct labeltest struct_labeltest = {
   .open       = osi_UFSOpen,
}
]])],[AC_MSG_RESULT(yes)
    AC_DEFINE(HAVE_STRUCT_LABEL_SUPPORT, 1, [Define to 1 if your compiler supports labels in structs.])
],[AC_MSG_RESULT(no)
])
])
