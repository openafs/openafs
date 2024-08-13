AC_DEFUN([OPENAFS_TOPDIRS],[
TOP_SRCDIR="${srcdir}/src"
dnl
dnl If we're using ./configure, need a more reasonable TOP_SRCDIR, since relative links don't work everywhere
dnl
case $TOP_SRCDIR in
        /*)
                ;;
        *)
                TOP_SRCDIR=`cd $TOP_SRCDIR; pwd`
                ;;
esac

TOP_OBJDIR=`pwd`
TOP_INCDIR="${TOP_OBJDIR}/include"
TOP_LIBDIR="${TOP_OBJDIR}/lib"
])
