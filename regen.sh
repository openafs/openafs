echo "Updating configuration..."
echo "Running aclocal"
aclocal -I src/cf
echo "Running autoconf"
autoconf
echo "Running autoconf for configure-libafs"
autoconf configure-libafs.in > configure-libafs
chmod +x configure-libafs
echo "Running autoheader"
autoheader
#echo "Running automake"
#automake

# Rebuild the man pages, to not require those building from source to have
# pod2man available.
echo "Building man pages"
if test -d doc ; then
    mkdir -p doc/man-pages/man1 doc/man-pages/man5 doc/man-pages/man8
    for f in doc/man-pages/pod1/*.pod ; do
        pod2man -c 'AFS Command Reference' -r 'OpenAFS' -s 1 \
            -n `basename "$f" | sed 's/\.pod$//'` "$f" \
            > `echo "$f" | sed -e 's%pod1/%man1/%' -e 's/\.pod$/.1/'`
    done
    for f in doc/man-pages/pod5/*.pod ; do
        pod2man -c 'AFS File Reference' -r 'OpenAFS' -s 5 \
            -n `basename "$f" | sed 's/\.pod$//'` "$f" \
            > `echo "$f" | sed -e 's%pod5/%man5/%' -e 's/\.pod$/.5/'`
    done
    for f in doc/man-pages/pod8/*.pod ; do
        pod2man -c 'AFS Command Reference' -r 'OpenAFS' -s 8 \
            -n `basename "$f" | sed 's/\.pod$//'` "$f" \
            > `echo "$f" | sed -e 's%pod8/%man8/%' -e 's/\.pod$/.8/'`
    done
fi
