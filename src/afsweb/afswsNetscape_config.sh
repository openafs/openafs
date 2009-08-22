#!/bin/sh
# $Header$
#
# Installation and Configuration script for AFS Web Security Pack
#
# History:
#
#	19-Mar-1998	Created. (wvh@transarc.com)
#
#       06-May-1998	Added tar file stuff and tweaked to add CellServDb 
#			  edits, etc. 
#
# The suggested procedure would be to install everything from the CD
#  (maybe using tar?), and then run the this script.
#
# Suggested final installation heirarchy would look like:
#
#	/usr/<something>/etc - conf files for AFSWebSecure
#	/usr/<something>/lib - location of shared library
#	/usr/<something>/cache - real cache or symlink elsewhere
#	/usr/<something>/log - log files
#
##########################################################################
#
#  Actual install script begins here.
#
##########################################################################

PRODUCT="AFS Web Security Pack"

#
# Handle various types of failures
#
trap 'echo "" ; \
      echo "$PRODUCT Installation cancelled." ; \
      echo "" ; \
      exit 1' 1 2 3 6 9 12 13 15 16 17 24 25 29

rm -f /tmp/newconf

#
# Figure out what OS we're running on so that we try to
#  exec the right binaries in the script. (The uname
#  binary lives in the same place on AIX and SunOS 
#  (Solaris) systems, else we'd have to conditionalize 
#  that, too.)
#
OSTYPE=`/usr/bin/uname | sed -e 's;\([a-zA-Z]*\).*;\1;'`

if [ x$OSTYPE = "xSunOS" ] ; then
   HOSTNAME=/usr/ucb/hostname
   NSLOOKUP=/usr/sbin/nslookup
   EX=/usr/ucb/ex
   ECHO=/bin/echo
   ECHOFLAG="\c"
   AFSWSLIB=nsafs.so
elif [ x$OSTYPE = "xAIX" ] ; then
   HOSTNAME=/usr/bin/hostname
   NSLOOKUP=/usr/bin/nslookup
   EX=/usr/bin/ex
   ECHO=echo
   ECHOFLAG=""
   AFSWSLIB=nsafs.a
else
   $ECHO "	ERROR: unsupported OSTYPE \"$OSTYPE\" returned by /usr/bin/uname."
   exit 1
fi

#
# Check for the existence of the file to install from, and exit 
# if not in pwd.
#
	if [ ! -f ns_afswebsecure.tar ] ; then
	   $ECHO "  File ns_afswebsecure.tar not found in working directory."
	   $ECHO ""
	   exit 1
	fi
#
#   Find out where the server is installed
#
	$ECHO "Enter the directory where the Web server's bin directory is located [/usr/netscape/suitespot]: " $ECHOFLAG
	read ws_srvdir
	if [ x$ws_srvdir = "x" ] ; then
	   ws_srvdir="/usr/netscape/suitespot"
	fi

	$ECHO ""
#
#   Find out the name of the Web server. Sets three variables for
#    the hostname: lhn, which is the full host name; shn, which is
#    the short (non-domain) host name; and dhn, which is the domain
#    name. This uses nslookup, since we can't depend on the host
#    actually being in /etc/hosts due to DNS and YP.
#
	tmp=`$HOSTNAME`
	lhn=`$NSLOOKUP $tmp | grep Name: | sed -e 's;Name:[ 	]*;;'`
        shn=`echo $lhn | sed -e 's;\([-_a-zA-Z0-9]*\).*;\1;'`
        dhn=`echo $lhn | sed -e "s;$shn\.;;"`

# 	echo ""
#	echo "Long host name is $lhn."
#	echo "Short host name is $shn."
#	echo "Domain name is $dhn."
#	echo ""
	
	$ECHO "Enter the Web server to use to access AFS data [$shn]: "  $ECHOFLAG
	read ws_wsname
	case $ws_wsname in
	'')	;;
	*)	shn=$ws_wsname;;
	esac

	ws_conf=$ws_srvdir"/https-"$shn"/config/obj.conf"

	if [ ! -f $ws_conf ] ; then
   	   $ECHO "	ERROR: server config file \"$ws_conf\" not found."
	   exit 1
	else
   	   $ECHO ""
	fi
#
# Make sure we can actually modify the obj.conf file
#

	touch $ws_conf 1> /dev/null 2> /dev/null
	if [ $? !=  0 ] ; then
	   $ECHO "	ERROR: No write permission to configuration file $ws_conf."
	   exit 1
	fi

#
# Check if AFS WebSecure is already installed
#
	grep nsafs $ws_conf 1> /dev/null 2> /dev/null
	if [ $? = 0 ] ; then
	   $ECHO "	ERROR: $PRODUCT entries already exist in $ws_conf."
	   exit 1
	fi

#
#   Find out where the AFSWebSecure stuff should be installed.
#

	$ECHO "Enter the $PRODUCT installation directory [/usr/afsweb]:  " $ECHOFLAG
	read ws_wsdir
	if [ x$ws_wsdir = "x" ] ; then
	   ws_wsdir="/usr/afsweb"
	fi

#
# Install WebSecure from tar file in same directory from which the 
#  ws_config script was started. The convoluted directory creation
#  loop lets people install in a directory tree that doesn't exist
#  yet, which would not typically be the case, but...
#
	if [ ! -d $ws_wsdir ] ; then
	   base=""
	   root=""
	   target=$ws_wsdir
	   while [ x$target != "x" ] ; do
	         base=`echo $target | sed -e 's;\(/[^/]*\)[-/a-zA-Z0-9_]*;\1;'`
		 target=`echo $target | sed -e 's;\(/[^/]*\)\([-/a-zA-Z0-9_]*\);\2;'`
	         if [ ! -d $root$base ] ; then
		     echo "Creating $root$base"
		     mkdir $root$base 1> /dev/null 2> /dev/null
		     if [ $? != 0 ] ; then
		        $ECHO "ERROR: Could not create $ws_wsdir. Check permissions and retry."
			exit 1
		     fi
	      	 fi
		 root=$root$base
	   done
	fi

	$ECHO ""
	wd=`pwd`
	cd $ws_wsdir
        tar xvf $wd"/ns_afswebsecure.tar"

#
# Use this as a check that everything went OK.
#
	if [ ! -f $ws_wsdir"/lib/${AFSWSLIB}" ] ; then
   	   $ECHO "	ERROR: $PRODUCT shared library \"$ws_wsdir/lib/${AFSWSLIB}\" not found."
	   exit 1
	else
	   $ECHO ""
	fi

#
# Ensure that the cache dir and log files are owned by the user
# specified in magnus.conf that this server runs as
#
        ws_mconf=$ws_srvdir/https-$shn/config/magnus.conf
	
        if [ ! -f $ws_mconf ] ; then
           $ECHO "      ERROR: server config file \"$ws_mconf\" not found."
           exit 1
        else
           $ECHO ""
        fi

	username=`grep -i User $ws_mconf | awk '{ print $2 }'`
        chown $username $ws_wsdir/cache $ws_wsdir/log	
	
#
# Enter the size of the AFS cache in 1K blocks
#

	$ECHO "Enter the size of the $PRODUCT cache in 1K blocks [5000]: " $ECHOFLAG
	read ws_csize
	if [ x$ws_csize = "x" ] ; then
	   ws_csize="5000"
	fi
        $ECHO ""

	expr $ws_csize + 1 1> /dev/null 2> /dev/null
	if [ $? != 0 ] ; then
	   $ECHO "	ERROR: Cache size must be numeric. You entered \"$ws_csize\"."
	   exit 1
	fi

#
# Set default value for advanced options - number of files in the cache
#  and number of receiving threads.
#
	ws_cfiles=`expr $ws_csize / 10`
        ws_numt="2"

#
# Enter the number of AFS daemon processes
#

	$ECHO "Enter the number of AFS daemons to run [3]: " $ECHOFLAG 
	read ws_numd
	if [ x$ws_numd = "x" ] ; then
	   ws_numd="3"
	fi
        $ECHO ""

	expr $ws_numd + 1 1> /dev/null 2> /dev/null
	if [ $? != 0 ] ; then
	   $ECHO "	ERROR: The number of AFS daemons must be numeric. You entered \"$ws_numd\"."
	   exit 1
	fi

#
# Enter the number of entries in the AFS Stat cache
#  Default is the number of files in the cache.
#

	$ECHO "Enter the number of entries in the AFS stat cache [$ws_cfiles]: " $ECHOFLAG 
	read ws_nums
	if [ x$ws_nums = "x" ] ; then
	   ws_nums=$ws_cfiles
	fi
        $ECHO ""

	expr $ws_nums + 1 1> /dev/null 2> /dev/null
	if [ $? != 0 ] ; then
	   $ECHO "	ERROR: The number of entries in the stat cache must be numeric. You entered \"$ws_nums\"."
	   exit 1
	fi

#
#   Find out which cells $PRODUCT should work with
#

	$ECHO "Enter the AFS cell to access using $PRODUCT [$dhn]: "  $ECHOFLAG
	read ws_cell
	if [ x$ws_cell = "x" ] ; then
	   ws_cell=$dhn
	fi

#
# Create appropriate ThisCell file
#
	echo $ws_cell > $ws_wsdir"/etc/ThisCell"

#
# If necessary, add entries to generic cellServDB file
#
	grep $ws_cell $ws_wsdir"/etc/CellServDB" 1> /dev/null 2> /dev/null
	if [ $? != 0 ] ; then
	   $ECHO "	Cell $ws_cell not found in default CellServDB file."
	   $ECHO ""
	   $ECHO "Enter a one-line text description for this cell: " $ECHOFLAG
	   read exp
	   $ECHO ">$ws_cell		   # $exp" >> $ws_wsdir"/etc/CellServDB"
	   $ECHO ""
           $ECHO "Enter the IP address of a database server for this cell ('.' to exit): " $ECHOFLAG
      	   read ws_dbsrv 
	   while [ x$ws_dbsrv != "x." ] ; do
		 ws_dbname=`grep $ws_dbsrv /etc/hosts | sed -e 's;[0-9\.]*[ 	]*\([a-z\.A-Z0-9]*\).*;\1;'`
		 if [ x$ws_dbname = "x" ] ; then
		    ws_dbname="Unknown"
		 fi
		 $ECHO "   Appending $ws_dbname, IP address $ws_dbsrv."
		 $ECHO "$ws_dbsrv		#$ws_dbname" >> $ws_wsdir"/etc/CellServDB"
                 $ECHO "Enter the IP address of a database server for this cell ('.' to exit): " $ECHOFLAG
	   	 read ws_dbsrv 
	   done
	else
	    $ECHO "	Found cell $ws_cell in $ws_wsdir/etc/CellServDB."
	fi		 

	$ECHO ""
#
#
# "Advanced" configuration items - optional. These are:
#
#	- number of RX threads
#	- number of files in the AFS WebSecure cache 
#
# 	
# 	$ECHO "Do you want to configure advanced options? (y/n): "  $ECHOFLAG
# 	read ans
# 	let=`echo $ans | sed -e 's;\(.\).*;\1;' | tr [A-Z] [a-z]`
# 	$ECHO ""
# 	if [ x$let = "xy" ] ; then
# 
#
# Enter the number of files in the AFS cache - default is ws_csize/10
#
# 	
# 		$ECHO "Enter the number of files in the Websecure cache [$ws_cfiles]: "  $ECHOFLAG
# 		read ws_cfiles
# 		if [ x$ws_cfiles = "x" ] ; then
# 		   ws_cfiles=`expr $ws_csize / 10`
# 	     	fi
# 		$ECHO ""
# 
# 		expr $ws_cfiles + 1 1> /dev/null 2> /dev/null
# 		if [ $? != 0 ] ; then
# 		   $ECHO "	ERROR: Number of cache files must be numeric. You entered \"$ws_cfiles\"."
# 		   exit 1
# 		fi
# 
#
# Enter the number of threads to process RX packets
#
# 
# 		$ECHO "Enter the number of threads to process RX requests [2]: "  $ECHOFLAG
# 		    read ws_numt
# 		if [ x$ws_numt = "x" ] ; then
# 	            ws_numt="2"
# 		    fi
# 	        $ECHO ""
# 
# 	        expr $ws_numt + 1 1> /dev/null 2> /dev/null
#                 if [ $? != 0 ] ; then
# 	           $ECHO "	ERROR: The number of RX threads must be numeric. You entered \"$ws_numt\"."
#                    exit 1
#              	fi
# 
# 	fi
# 
# 
# Now that we have all the info we need, time to actually edit the obj.conf file
#
# Back up the old one if no previous backup exists
#
if [ -f $ws_conf".pre_ws" ] ; then
   $ECHO "NOTE: Backup file \"$ws_conf.pre_ws\" already exists."
   $ECHO "       Not backing up current obj.conf file."
else
    cp $ws_conf $ws_conf".pre_ws"
    $ECHO "	Creating backup copy of obj.conf file..."
fi

$EX $ws_conf << END_OF_FILE 1> /dev/null
1
/mime-types
a
Init fn="load-modules" shlib="$ws_wsdir/lib/${AFSWSLIB}" funcs="nsafs-init,nsafs-basic,nsafs-mount,nsafs-public,nsafs-nolinks,nsafs-check,nsafs-nocheck,nsafs-find-index,nsafs-send,nsafs-force-type,nsafs-put,nsafs-delete,nsafs-move,nsafs-index,nsafs-mkdir,nsafs-rmdir"
Init fn="nsafs-init" cell="$ws_cell" blocks="$ws_csize" files="$ws_cfiles" stat="$ws_nums" daemons="$ws_numd" cachedir="$ws_wsdir/cache" confdir="$ws_wsdir/etc" logfile="$ws_wsdir/log/nsafs.log" exp-max="120" timeout="30" rcvthreads="$ws_numt"
.
/default
a
AuthTrans fn="nsafs-basic"
NameTrans fn="nsafs-mount" mount="/afs"
PathCheck fn="nsafs-public" public="/afs"
PathCheck fn="nsafs-nolinks" nolinks="/afs/$ws_cell/usr"
.
/Service
i
Service method="(GET|HEAD)" fn="nsafs-send" index="fancy"
.
w! /tmp/newconf
q
END_OF_FILE

cp /tmp/newconf $ws_conf

$ECHO ""

$ECHO "  Installation of $PRODUCT complete."
