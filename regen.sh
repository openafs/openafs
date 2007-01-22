#!/bin/sh
echo "Updating configuration..."
echo "Running aclocal"
aclocal -I src/cf
echo "Running autoconf"
autoconf
echo "Running autoconf for configure-libafs"
autoconf configure-libafs.in > configure-libafs
chmod +x configure-libafs
echo "Running autoconf for configure-libktrace"
autoconf configure-libktrace.in > configure-libktrace
chmod +x configure-libktrace
echo "Running autoheader"
autoheader
#echo "Running automake"
#automake

echo "Deleting autom4te.cache directory"
rm -r autom4te.cache

# Rebuild the man pages, to not require those building from source to have
# pod2man available.
if test -d doc/man-pages ; then
    echo "Building man pages"
    (cd doc/man-pages && ./generate-man)
fi
