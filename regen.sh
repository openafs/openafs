echo "Updating configuration..."
echo "Running aclocal"
aclocal -I src/cf
echo "Running autoconf"
autoconf
echo "Running autoheader"
autoheader
#echo "Running automake"
#automake
