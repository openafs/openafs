#!/bin/sh -e

echo "Updating configuration..."

echo "Running libtoolize"
if which libtoolize > /dev/null 2>&1; then
    libtoolize -c -f -i
elif which glibtoolize > /dev/null 2>&1; then
    glibtoolize -c -f -i
else
  echo "No libtoolize found on your system (looked for libtoolize & glibtoolize)"
  exit 1
fi

M4_INCS="-I src/cf"
M4_INCS="$M4_INCS -I src/external/rra-c-util/m4"
M4_INCS="$M4_INCS -I src/external/autoconf-archive/m4"

echo "Running aclocal"
if which aclocal > /dev/null 2>&1; then
  aclocal $M4_INCS
elif which aclocal-1.10 > /dev/null 2>&1; then
  aclocal-1.10 $M4_INCS
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
