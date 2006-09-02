dnl LINUX_BUILD_VNODE_FROM_INODE (configdir, outputdir, tmpldir)
dnl		defaults: (src/config, src/afs/LINUX, src/afs/linux)

AC_DEFUN([LINUX_BUILD_VNODE_FROM_INODE], [
AC_MSG_CHECKING(whether to build osi_vfs.h)
configdir=ifelse([$1], ,[src/config],$1)
outputdir=ifelse([$2], ,[src/afs/LINUX],$2)
tmpldir=ifelse([$3], ,[src/afs/LINUX],$3)
mkdir -p $outputdir
cp  $tmpldir/osi_vfs.hin $outputdir/osi_vfs.h
# chmod +x $configdir/make_vnode.pl
# $configdir/make_vnode.pl -i $LINUX_KERNEL_PATH -t ${tmpldir} -o $outputdir
])
