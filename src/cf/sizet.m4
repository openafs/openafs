AC_DEFUN([OPENAFS_PRINTF_TAKES_Z_LEN],
[
AC_CACHE_CHECK([whether printf understands the %z length modifier],
[openafs_cv_printf_takes_z], [
        AC_TRY_RUN([
#include <stdio.h>
#include <string.h>

int main(void) {
        char buf[8];
        memset(buf, 0, sizeof(buf));
        snprintf(buf, 8, "%zu", sizeof(char));
        if (buf[0] == '1' && buf[1] == '\0') {
                return 0;
        } else {
                return 1;
        }
}],
                [openafs_cv_printf_takes_z="yes"],
                [openafs_cv_printf_takes_z="no"],
                [openafs_cv_printf_takes_z="no"])
])

if test "x$openafs_cv_printf_takes_z" = "xyes"; then
        AC_DEFINE([PRINTF_TAKES_Z_LEN], 1, [define if printf and friends understand the %z length modifier])
fi
])
