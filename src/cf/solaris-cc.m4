AC_DEFUN([SOLARIS_PATH_CC], [
  AC_PATH_PROG([SOLARISCC], [cc], [],
    [m4_join([:],
      [/opt/SUNWspro/bin],
      [/opt/SunStudioExpress/bin],
      [/opt/developerstudio12.6/bin],
      [/opt/developerstudio12.5/bin],
      [/opt/solarisstudio12.4/bin],
      [/opt/solarisstudio12.3/bin],
      [/opt/solstudio12.2/bin],
      [/opt/sunstudio12.1/bin])])
   AS_IF([test "x$SOLARISCC" = "x"],
      [AC_MSG_FAILURE([m4_join([ ],
         [Could not find the solaris cc program.],
         [Please define the environment variable SOLARISCC to specify the path.])])])
])
