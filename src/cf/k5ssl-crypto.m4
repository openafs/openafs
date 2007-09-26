dnl Copyright (c) 2007
dnl The Regents of the University of Michigan
dnl ALL RIGHTS RESERVED
dnl
dnl Permission is granted to use, copy, create derivative works
dnl and redistribute this software and such derivative works
dnl for any purpose, so long as the name of the University of
dnl Michigan is not used in any advertising or publicity
dnl pertaining to the use or distribution of this software
dnl without specific, written prior authorization.  If the
dnl above copyright notice or any other identification of the
dnl University of Michigan is included in any copy of any
dnl portion of this software, then the disclaimer below must
dnl also be included.
dnl
dnl This software is provided as is, without representation
dnl from the University of Michigan as to its fitness for any
dnl purpose, and without warranty by the University of
dnl Michigan of any kind, either express or implied, including
dnl without limitation the implied warranties of
dnl merchantability and fitness for a particular purpose.  The
dnl regents of the University of Michigan shall not be liable
dnl for any damages, including special, indirect, incidental, or
dnl consequential damages, with respect to any claim arising
dnl out of or in connection with the use of the software, even
dnl if it has been or is hereafter advised of the possibility of
dnl such damages.
dnl
AC_DEFUN([OPENAFS_ENABLE_K5SSL_CRPYTO], [#start
AC_ARG_ENABLE( k5ssl-crypto,
[  --enable-k5ssl-crypto                {,no,only}{des,des3,rc4,aes}],, enable_k5ssl_crypto="default")
temp="$enable_k5ssl_crypto"
k5ssl_defaults='k5ssl_crypto_aes=1
k5ssl_crypto_des3=1
k5ssl_crypto_des=1
k5ssl_crypto_rc4=1
k5ssl_crypto_cast=0
k5ssl_crypto_rc6=0'
k5ssl_all_off=`echo "$k5ssl_defaults" | egrep '^k5ssl_crypto_' | sed 's/1/0/'`
k5ssl_all_on=`echo "$k5ssl_defaults" | egrep '^k5ssl_crypto_' | sed 's/0/1/'`
eval $k5ssl_defaults
temp="`echo "$temp"|tr 'A-Z' 'a-z'`"
while test "X" != "X$temp"
do 
tmp="`echo "$temp"|sed 's;[[^0-9a-z]].*;;'`"
temp="`echo "$temp"|sed 's;^[[0-9a-z]]*[[^0-9a-z]]*;;'`"
if test "X" = "X$tmp"; then continue; fi
if test "Xdefault" = "X$tmp"; then eval $k5ssl_defaults; continue; fi
if test "Xall" = "X$tmp"; then eval $k5ssl_all_on; continue; fi
k5ssl_yesno=1
if expr "X$tmp" : "Xno" >/dev/null; then k5ssl_yesno=0; tmp=`echo "$tmp"|sed 's;^no;;'`;fi
if expr "X$tmp" : "X.*exp$" >/dev/null; then k5ssl_yesno=2; tmp=`echo "$tmp"|sed 's;exp$;;'`;fi
if expr "X$tmp" : "Xonly" >/dev/null; then eval $k5ssl_all_off; tmp="`echo $tmp|sed 's;^only;;'`";fi
if test "X" = "X$tmp"; then continue; fi
if test "Xold" = "X$tmp"
then
	k5ssl_crypto_cast=${k5ssl_yesno}
	k5ssl_crypto_rc6=${k5ssl_yesno}
elif test "Xmissing" != "X`eval echo '${'k5ssl_crypto_${tmp}':-missing}'`"; then 
	eval k5ssl_crypto_${tmp}=${k5ssl_yesno}
else
	echo that is not yet a crypto system.
	exit 1
fi
done
unset k5ssl_defaults k5ssl_all_off
if test $k5ssl_crypto_aes -eq 0; then ENABLE_AES=#; else ENABLE_AES=''; fi
if test $k5ssl_crypto_des3 -eq 0; then ENABLE_DES3=#; else ENABLE_DES3=''; fi
if test $k5ssl_crypto_des -eq 0; then ENABLE_DES=#; else ENABLE_DES=''; fi
if test $k5ssl_crypto_rc4 -eq 0; then ENABLE_RC4=#; else ENABLE_RC4=''; fi
if test $k5ssl_crypto_cast -eq 0; then ENABLE_CAST=#; else ENABLE_CAST=''; fi
if test $k5ssl_crypto_rc6 -eq 0; then ENABLE_RC6=#; else ENABLE_RC6=''; fi
if test $k5ssl_crypto_des -eq 0 -a $k5ssl_crypto_des3 -eq 0; then ENABLE_DES_DES3=#; else ENABLE_DES_DES3=''; fi
if test $k5ssl_crypto_des -eq 0 -a $k5ssl_crypto_cast -eq 0 -a $k5ssl_crypto_rc6 -eq 0; then ENABLE_K5SSL_CRYPT=#; else ENABLE_K5SSL_CRYPT=''; fi
if test $k5ssl_crypto_aes -eq 0; then ENABLE_K5SSL_PKCS5=#; else ENABLE_K5SSL_PKCS5=''; fi
AC_SUBST(ENABLE_AES)
AC_SUBST(ENABLE_DES3)
AC_SUBST(ENABLE_DES)
AC_SUBST(ENABLE_RC4)
AC_SUBST(ENABLE_CAST)
AC_SUBST(ENABLE_RC6)
AC_SUBST(ENABLE_DES_DES3)
AC_SUBST(ENABLE_K5SSL_CRYPT)
AC_SUBST(ENABLE_K5SSL_PKCS5)
## set | egrep '^k5ssl_crypto_'
AC_CONFIG_COMMANDS([src/k5ssl/k5s_config.h],[
dnl this is not part of the file.
if test X$with_ssl != Xyes; then
	k5ssl_crypto_using_ssl='#else
#define USING_SSL 1
'
fi
echo "#define USING_K5SSL 1
#if defined(KERNEL) && !defined(UKERNEL)
#define USING_FAKESSL 1
$k5ssl_crypto_using_ssl#endif

#define KRB5_AES_SUPPORT $k5ssl_crypto_aes
#define KRB5_DES3_SUPPORT $k5ssl_crypto_des3
#define KRB5_DES_SUPPORT $k5ssl_crypto_des
#define KRB5_RC4_SUPPORT $k5ssl_crypto_rc4
#define KRB5_CAST_SUPPORT $k5ssl_crypto_cast
#define KRB5_RC6_SUPPORT $k5ssl_crypto_rc6
#define KRB5_ECEKE_SUPPORT 0" >src/k5ssl/k5s_config.h
],[dnl
k5ssl_crypto_aes=$k5ssl_crypto_aes
k5ssl_crypto_des3=$k5ssl_crypto_des3
k5ssl_crypto_des=$k5ssl_crypto_des
k5ssl_crypto_rc4=$k5ssl_crypto_rc4
k5ssl_crypto_cast=$k5ssl_crypto_cast
k5ssl_crypto_rc6=$k5ssl_crypto_rc6])
#end]
)
