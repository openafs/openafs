echo "Running aclocal"
aclocal -I cf
echo "Running autoconf"
autoconf
echo "Running autoheader"
autoheader
echo "Running automake"
automake
