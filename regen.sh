#!/bin/sh -e

while getopts "q" flag
do
    case "$flag" in
	q)
	    skipman=1;
	    ;;
	*)
	    echo "Usage ./regen.sh [-q]"
	    echo "	-q skips man page generation"
	    exit
	    ;;
    esac
done

echo "Updating configuration..."
echo "Running aclocal"
if which aclocal > /dev/null 2>&1; then
  aclocal -I src/cf
elif which aclocal-1.10 > /dev/null 2>&1; then
  aclocal-1.10 -I src/cf
else
  echo "No aclocal found on your system (looked for aclocal & aclocal-1.10)"
  exit 1
fi

echo "Running autoconf"
autoconf
echo "Running autoconf for configure-libafs"
autoconf configure-libafs.ac > configure-libafs
chmod +x configure-libafs
echo "Running autoheader"
autoheader
#echo "Running automake"
#automake

echo "Deleting autom4te.cache directory"
rm -rf autom4te.cache

if [ $skipman ] ; then
    echo "Skipping man page build"
else 
    # Rebuild the man pages, to not require those building from source to have
    # pod2man available.
    if test -d doc/man-pages ; then
        echo "Building man pages"
        perl doc/man-pages/merge-pod doc/man-pages/pod1/*.in
        perl doc/man-pages/merge-pod doc/man-pages/pod8/*.in
        (cd doc/man-pages && ./generate-man)
    fi
fi
