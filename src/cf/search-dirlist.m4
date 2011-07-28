# OPENAFS_SEARCH_DIRLIST(VARIABLE, [PATHS TO SEARCH], [TARGET FILE])
AC_DEFUN([OPENAFS_SEARCH_DIRLIST],
[openafs_dirpath=no
  for openafs_var in [$2]; do
    if test -r "$openafs_var/[$3]"; then
      openafs_dirpath=$openafs_var
      break
    fi
  done
  if test x"$openafs_dirpath" = xno; then
    [$1]=
  else
    [$1]=$openafs_dirpath
  fi
])
