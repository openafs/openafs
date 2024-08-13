AC_DEFUN([OPENAFS_CROSSTOOLS],[
if test "x$with_crosstools_dir" != "x"; then
    if test -f "$with_crosstools_dir/rxgen" -a -f "$with_crosstools_dir/compile_et" -a -f "$with_crosstools_dir/config"; then
        COMPILE_ET_PATH=$with_crosstools_dir/compile_et
        CONFIGTOOL_PATH=$with_crosstools_dir/config
        RXGEN_PATH=$with_crosstools_dir/rxgen
    else
        AC_MSG_ERROR(Tools not found in $with_crosstools_dir)
    fi
else
    COMPILE_ET_PATH="${TOP_OBJDIR}/src/comerr/compile_et"
    CONFIGTOOL_PATH="${TOP_OBJDIR}/src/config/config"
    RXGEN_PATH="${TOP_OBJDIR}/src/rxgen/rxgen"
fi
AC_SUBST(COMPILE_ET_PATH)
AC_SUBST(CONFIGTOOL_PATH)
AC_SUBST(RXGEN_PATH)
])
