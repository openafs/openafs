AC_DEFUN([OPENAFS_SUMMARY],[
    # Print a configuration summary
echo 
echo "**************************************"
echo configure summary
echo
AS_IF([test $LIB_curses],[
echo "LIB_curses :                $LIB_curses" ],[
echo "XXX LIB_curses  not found! not building scout and afsmonitor!"
])
echo 
echo "**************************************"
])
