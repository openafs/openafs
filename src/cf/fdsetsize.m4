AC_DEFUN([FD_SET_OBEYS_FD_SETSIZE], [
AC_MSG_CHECKING(for default FD_SETSIZE)
AC_CACHE_VAL(ac_cv_default_fdsetsize,
[
AC_TRY_RUN(
changequote(<<, >>)dnl
<<
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
int main(int argc, char *argv[])
{
    FILE *fp;
    int ac_cv_default_fdsetsize;
#if defined(FD_SETSIZE)
    ac_cv_default_fdsetsize = FD_SETSIZE;
#else
    ac_cv_default_fdsetsize = (sizeof(fd_set)*8);
#endif
    if ((fp = fopen("conftestval", "w")) == NULL)
        exit(1);
    fprintf(fp, "%d\n", ac_cv_default_fdsetsize);
    fclose(fp);
    exit(0);
}
>>
changequote([, ])dnl
,
ac_cv_default_fdsetsize=`cat conftestval`,
ac_cv_default_fdsetsize=-1,
ac_cv_default_fdsetsize=1024
)
AC_MSG_RESULT([$ac_cv_default_fdsetsize])
])

AC_MSG_CHECKING(for default fd_set size)
AC_CACHE_VAL(ac_cv_default_fd_set_size,
[
AC_TRY_RUN(
changequote(<<, >>)dnl
<<
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
int main(int argc, char *argv[])
{
    FILE *fp;
    int ac_cv_default_fd_set_size;
    ac_cv_default_fd_set_size = (sizeof(fd_set));

    if ((fp = fopen("conftestval", "w")) == NULL)
        exit(1);
    fprintf(fp, "%d\n", ac_cv_default_fd_set_size);
    fclose(fp);
    exit(0);
}
>>
changequote([, ])dnl
,
ac_cv_default_fd_set_size=`cat conftestval`,
ac_cv_default_fd_set_size=-1,
ac_cv_default_fd_set_size=128
)
AC_MSG_RESULT([$ac_cv_default_fd_set_size])
])

AC_MSG_CHECKING(for sizeof fd_set obeying FD_SETSIZE)
AC_CACHE_VAL(ac_cv_fd_set_size_obeys,
[
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS -DORIG_FD_SETSIZE=$ac_cv_default_fdsetsize -DORIG_fd_set_size=$ac_cv_default_fd_set_size"
AC_TRY_RUN([
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#if (ORIG_FD_SETSIZE < 2048) 
#define FD_SETSIZE 2048
#define __FD_SETSIZE 2048
#else
#define FD_SETSIZE 1024
#define __FD_SETSIZE 1024
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
int main() {
    if ((ORIG_fd_set_size < 0) || (ORIG_FD_SETSIZE < 0))
       exit(1);       
    exit((sizeof(fd_set) == ORIG_fd_set_size) ? 1 : 0);
}
],
ac_cv_fd_set_size_obeys=yes,
ac_cv_fd_set_size_obeys=no,
ac_cv_fd_set_size_obeys=yes)
CPPFLAGS=$save_CPPFLAGS
AC_MSG_RESULT([$ac_cv_fd_set_size_obeys])
])

])

