#!/bin/ksh
# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

##Installation and setup script for AFS Web Secure for the APACHE server
##
PRODUCT_NAME="AFS Web Security Pack"

message="
Use this script for configuring and setting up $PRODUCT_NAME for use 
with the Apache Web server. Before executing this script, be sure to 
read the Transarc $PRODUCT_NAME documentation provided in the 
AFSWebSecure_docs.tar file (the html documentation for $PRODUCT_NAME) 

Users should have some familiarity with Apache configuration, 
especially for troubleshooting purposes. Users must also have 
appropriate permissions in the relevant directories. 

NOTE: This script does not configure other features of the Apache 
server or SSL implementation for Apache. See related documentation for 
setting up Apache with SSL. 

This script prompts you to perform three operations:

1) Set up the configuration file for compiling $PRODUCT_NAME into 
the Apache source code and linking it with the appropriate 
libraries. Configure and build the Apache server binary. 

2) Set up the Apache run-time directives configuration file with 
the directives required for $PRODUCT_NAME (configurable parameters) 
See the Transarc $PRODUCT_NAME documentation for explanations of these 
parameters and be sure you understand what each of them do. 

3) Set up the startup and shutdown scripts for the Web server and 
starts up the server if you want it to. 

Default parameters are provided wherever appropriate. Please make sure 
you read the prompts and give the FULL PATH name (PATHNAME) where required. 	
If you are familiar with Apache configuration and prefer to add the 
required parameters to the configuration files with a text editor, 
you can do so as with any other Apache module. 
"

case $# in
1) show_welcome=1 ;;
2) set -x;;
esac


PRODUCT="AFS_Web_Security_Pack"
COMPANY="IBM Transarc"
aplib=libapacheafs.a
syslib=libsys.a
cp=/bin/cp
rm=/bin/rm
mv=/bin/mv
more=/bin/more
AFSModule=mod_afs.o
prog=INSTALL.$PRODUCT

os=`/bin/uname -s`
osver=`/bin/uname -v`
osrev=`/bin/uname -r`

case $os in
AIX)	platform=${os}_${osver}.${osrev} ;;
SunOS)	platform=${os}_${osver} ;;
*)	echo "$prog:Unsupported Platform $os"; exit 0 ;;
esac

##
##Defaults
##
Log=$prog.log

ServerRoot_default=`pwd`
Apache_src_default=`pwd`/src

ConfigScript_default=Configure
Weblog_Starter_default=${ServerRoot_default}/weblog_starter
make_conf_default=Configuration
run_conf_default=httpd.conf
binary_default=httpsd
startscr_default=start.httpd
stopscr_default=stop.httpd
DocRoot_default=htdocs
fs=/usr/afsws/bin/fs
while [ ! -x $fs ]
do
  echo "$fs:No such file or directory"
  echo $ac_n "Enter full path and filename for the fs binary on"\
   " this AFS client machine: [$fs] $ac_c"
    read in
    test $in && fs=$in
done
	
default_cell_name=`$fs wscell | awk -F"'" '{ print $2 }'`
  case $default_cell_name in
    '') default_cell_name="CellName" ;;
  esac
  
## 
## Display a nice fancy header prompt
##
init_prompt()
{
  colen=${#COMPANY}
  title="$PRODUCT_NAME SETUP AND INSTALLATION"
  titlen=${#title}
  clear
  echo
  echo | awk 'BEGIN { len="'$colen'" ; pos=(80-len)/2 } 
    { for (i=0; i<pos; i++) { printf " " }
  } '
  echo $ac_n "$COMPANY $ac_c"
  echo
  echo | awk 'BEGIN { len="'$titlen'" ; pos=(80-len)/2 } 
  { for (i=0; i<pos; i++) { printf " " }
  } '
  echo $ac_n "$title $ac_c"
  echo
  echo | awk '{ for (i=0; i < 80; i++) { printf "-" } }'
  echo; echo
}
  

##
## Attempt to change the defaults in this file
##
change_default()
{
  args=2
  rn=change_defaults
  case $# in
    $args)	;;
    *)	echo "$rn: incorrect arguments:$#" ;
       return ;;
  esac
  file=$0
  tempfile=/tmp/$file.$$
  if [ ! -x $file ]
  then
    echo "$file not found for automatic update"
      return
  fi
  $cp -p $file $tempfile
  def=$1
  val=$2
  awk -F= 'BEGIN { d="'$def'"
                   v="'$val'"
           }
           {	
                   if ($1 == d) {
		     printf "# %s\n", $0
		     printf "%s=%s\n", d, v
		   }
		   else {
		     print $0
		   }
	   }' $tempfile > $file	
   $rm -f $tempfile
}


##
## Exit with the error message provided as the argument with exit status 1
##
die()
{
  echo "$*"
  exit 1
}

##
## Insert text into a file in various ways - wrapper around an awk script
##
insertfield()
{
  #	set -x
  rn=insertfield;
  args=4
  
  case $# in
    $args) 	;;
    *) 	die "incorrect number of arguments to $rn - $args expected. \
     $# given";;
  esac
  
  file=$1
  tempfile=/tmp/${file##*/}.$$
  line=$2
  type=$3
  field=$4

  case $type in
    "-f") 	#echo "$rn:$line in $file IN $field"
	  ;;
    "-p") 	#echo "$rn:$line in $file AFTER $field"
	  ;;
    "-n") 	#echo "$rn:$line in $file AT:$field"
	  ;;
    "-pn")     #echo "$rn:$line in $file AFTER LINE $field"
	   ;;
    *) 	die "$rn:unrecognized option $type";;
  esac
  
  $cp -p $file $tempfile
  retval=$?
  test $retval -eq 0 || die "$rn:copy failed"
  
  awk '
  BEGIN { 
    done = 0 
    type = "'$type'"
    field = "'$field'"
    line = "'$line'"
    version = "'$apache_version'"
  }
  {   if ( type == "-f" ) {
      split($0,str,"=")
      if ( field == str[1] ) { 
	printf "%s %s\n", $0, line	
        done=1 
      } 
      else {
	print $0
      }
    }
    else if ( type == "-n" ) {
      if (NR == field) {
	printf "%s\n%s\n", line, $0
        done=1
      }
      else {
	print $0
      }
    }
      else if ( type == "-p" ) {
	split($0,str,"=");
	if ( field == str[1] ) {
	  printf "%s\n%s=\n", $0, line
          done=1
	}
	else {
	  print $0
	}
      }
	else if ( type == "-pn") {
	  if ( field == $2 ) {
	    if ( version == "1.3") {
	      printf "%s\nAddModule %s\n", $0,line
	    }
	    else {
	      printf "%s\nModule afs_module\t   %s\n", $0,line
	    }
            done=1
	  }
	  else {
	    print $0
	  }
	}
    } 
    END { 
      if ( done != 1 ) {
	if ( type == "-f" ) {
	  printf "%s=%s\n",field,line
	}
	else {
	  printf "%s\n",line
	}
      }
    }
    ' $tempfile > $file
    
    retval=$?
    if [ $retval -eq 0 ]
    then
      $rm -f $tempfile
    else
      if [ -f $tempfile ]
      then
	$mv -f /tmp/$file.$$ $file
      fi
      die "rn:AWK ERROR"
    fi
}


##
## Used for Configuring run-time AFS directives
##
read_directives()
{
  rn=read_directives
  case $# in 
    0) die "$rn:Insufficient Arguments provided"
  esac
  
  file=$1;shift
  
  while test "$1" != ""
  do
    directive=$1;shift
    value=$1; shift	
    echo $ac_n "$directive [$value]:$ac_c"
    read in
    test $in && value=$in
    echo "$directive $value" >> $file
  done
}

##
## Given the Directive name ($2) it returns the value from the given
## file name ($1) - only for Directives beginning with SetAFS
##
get_afsdirective()
{
  args=2
  rn=get_afsdirective
  case $# in
    2)	;;
    *)	die "$rn:Incorrect args:$#" ;;
  esac
  
  awk '/SetAFS.*/ { 
    if ( $1 == "'$2'" ) { 
      print $2  
    }
  }' $1
}

##
## Given the Directive name ($2) it returns the value from the given
## file name ($1) - FOR ALL DIRECTIVES
##
get_directive()
{
  args=2
  rn=get_directive
  case $# in
    2)	;;
    *)	die "$rn:Incorrect args:$#" ;;
  esac
  
  awk '{ 
    if ( $1 == "'$2'" ) { 
      print $2  
    }
  }' $1
}

##
## Replace or insert single Directive value pair
##
put_directive()
{
  rn=put_directive
  args=3
  case $# in
    $args)	file=$1; directive=$2; value=$3 ;;
    *)	die "$rb:Incorrect args: $args - $#"
  esac
  ttempfile=/tmp/${prog}_${rn}.${file##*/}.$$
  ed=`grep $directive $file | grep -v '^#'`
  ediff=`echo $ed | awk '{ print $2 }'`
    echo $ac_n "$directive ($ediff) [$value]:$ac_c"
    read in
    case $in in 
      '') 	;;
      *)	value=$in ;;
    esac
    case $ediff in
      '') 	echo "$directive $value" >> $file
	        ;;
      *)  	if [ ! $ediff = $value ]
                then
		  awk '{ if ($1 == "'$directive'") {
		    printf "%s %s\n", $1, "'$value'"
                  }
		  else { print $0 }
		  }' $file > $ttempfile
		  $mv -f $ttempfile $file
		  retval=$?
		  case $retval in
		    0) 	;;
		    *)	die "$rn: mv failed: $retval"
		  esac
		  fi ;;
    esac
}


##
## Replace directives if they exist with new values or create the Directive 
## value pair if none exists in the run-time configuration file
##
put_directives()
{
  rn=put_directives
  case $# in
    0) 	die "$rn: incorrect arguments: $#" ;;
  esac
  
  file=$1 ; shift 
  ttempfile=/tmp/${prog}_${rn}.${file##*/}.$$
  
  echo; echo
  echo "Use the following syntax"
  echo "<Enter> or <space> to accept [default] value"
  echo "<-> to keep (Existing) value"
  echo "<#> to comment out directive" 
  
  while test "$1" != ""
  do
    directive=$1;shift
    value=$1; shift	
    edir=`grep $directive $file | grep -v '^#'`
    ediff=`echo $edir | awk '{ print $2 }'`
      case $ediff in
	'')	t=`grep $directive $file|grep '^#'` ;
	    case $t in
	      '') 	existprompt="NO EXISTING DIRECTIVE" ;;
	      *)  	existprompt="DIRECTIVE COMMENTED OUT" ;;
	    esac
	    ;;
	*)	existprompt=$ediff
	   ;;
      esac
      echo $ac_n "$directive ($existprompt) [$value]:$ac_c"
      read in
      case $in in 
	'') 	echo "Default $value unchanged"
	    ;;
	'#')	echo "This directive $directive will be commented out"
	ediff='#'			
	;;
	'-')	echo "Existing $ediff unchanged" ;
	     if [ ! -z $ediff ]
	     then
	       value=$ediff
	     else
	       value='#'
	     fi
	     ;;
	*)	value=$in 
	   ;;
      esac
      case $ediff in
	'') 	#search for commented out directive first
	    if [ ! $value = '#' ]
	    then
	      edir=`grep $directive $file|grep $value|grep '^#'`
	      case $edir in
		'')	echo "$directive $value" >> $file
		    ;; 	
		*) 	edir=`echo $edir|sed 's/.*#//g'|sed 's/^ //g'` 
		   edir2=`echo $edir|awk '{ print $2 }'`
		     if [ $edir2 = $value ]
		     then
		       #TO DO
		       echo "$directive $value" >> $file
		       #echo "Uncommented: $edir"
		     fi
		     ;;
	      esac
	    fi
	    ;;
	'#')	echo "Commenting out $directive" ;
       	        awk '{ if ($1 == "'$directive'") {
		  printf "# %s\n", $0
                }
		else { print $0 }
		}' $file > $ttempfile ;
		$mv -f $ttempfile $file ;
		retval=$? ;	
		case $retval in
		  0) 	;;
		  *)	die "$rn: mv failed: $retval" ;;
		esac 
		;;
	
	*)  	if [ ! $ediff = $value ]
 	        then
		  echo "Changing: $directive $ediff $value" 
		  awk '{ if ($1 == "'$directive'") {
		    printf "%s %s\n", $1, "'$value'"
		  }
		  else { print $0 }
		  }' $file > $ttempfile
		  
		  $mv -f $ttempfile $file
		  retval=$?	
		  case $retval in
		    0) 	;;
		    *)	die "$rn: mv failed: $retval" ;;
		  esac
		  fi ;;
      esac
  done
}


set_compiler()
{
  rn=set_compiler
  args=1
  case $# in
    $args) 	;;
    *) 	die "$rn: Incorrect args:$#"
  esac
  
  case $os in
    AIX)	insertfield $1 /bin/cc -f CC ;;
    *)	;;
  esac
}

##
## Add a module to the Configuration file - prompt to go to main menu and 
## reconfigure and re-compile. If module commented out - uncomment it
##
add_module()
{
  rn=add_module
  args=3
  case $# in
    $args)	f=$1; mn=$2 mf=$3 ;;
    *)	die "$rn: Incorrect args:$#"
  esac
  
  cmf=$mf.c
  omf=$mf.o
  if [ ! -f $Apache_src/$cmf ]
  then
    echo "Module file $Apache_src/$cmf does not exist"
    echo "Ensure you have this C source file in the " \
     "apache src ($Apache_src)directory"
    return 2
  fi		

  tf=/tmp/${prog}_${rn}
  ed=`grep $mn $f`
  case $ed in
    '')	echo "Module $mn not found";
	echo "Choose a module after which module you wish to insert " \
	 "this module. Note that modules are prioritised bottom-up";
	echo "See Apache documentation for more details";
	  val=Y;
	  modulelist=`grep Module $f | grep -v '^#'` ;
	  echo $ac_n "View a list of current modules: [$val] $ac_c";
	  read in; test $in && val=$in;
	  case $val in
	    y|Y)	echo "$modulelist" | more 
		 ;;
	    *)	;;
	  esac
	  echo $ac_n "Enter module name to enter this module after: $ac_c"
	  read aftm
	  test $aftm || { echo "No module entered ... returning"; return 2; }
	    eg=`echo "$modulelist" | grep $aftm` ;
	    case $eg in
	      '')	echo "Module $aftm not found in list"
		  echo "$modulelist" 
		  echo "... returning";
		  return 2;;
	      *)	echo "$aftm found in $modulelist"
		 ;;
	    esac
	    $cp -p $f $tf
	    awk '
	    BEGIN {
	      after="'$aftm'"
	      mname="'$mn'"
	      mfile="'$omf'"
            }
	    {
	      if ( $2 == after ) {
		printf "%s\nModule %s\t%s\n", $0,mname,mfile 
	      }
	      else {
		print $0
	      }
	    }' $tf  > f;
	    $rm -f $tf;	
	    return 1
	    ;;
    *)	jk=`echo $ed | grep '^#'` ;
       case $jk in
	 '')	echo "Module already exists: $ed. Returning";
	     return 0
	     ;;
	 *)	$cp -p $f $tf;
	    awk '{ if (( $2 == "'$mn'" )||( $3 == "'$mn'" )) {
	      printf "Module %s\t%s\n", "'$mn'", "'$omf'" 
	    }
	    else {
	      print $0
	    }	
	  }' $tf > $f;
	  $rm -f $tf;
	  return 1;
	  ;;
       esac
  esac
}

config_makefile()
{
  init_prompt;
  echo ; echo
  echo "Configuring Makefile"
  echo;
  
  mc=' '
  make_conf_dir=${make_conf%/*}
  while [[ ! -f $mc ]]
  do
    echo $ac_n "Configuration file name [$make_conf]:$ac_c"
    read uconf
    case $uconf in
      '')	;;
      /*)	make_conf=$uconf ; 
	  make_conf_dir=${make_conf%/*}
	  ;;
      *) 	make_conf=${make_conf_dir}/${uconf};
	 make_conf_dir=${make_conf%/*}
	 ;;
    esac
    mc=$make_conf
    test -f $mc || echo "$mc: does not exist"
  done
  
  # Back up original Configuration file
  backup=$make_conf.BEFOREAFS
  number=0
  while [ -f $backup ]
  do	
    backup=$make_conf.BEFOREAFS.$number
    number=$(($number+1))			
  done	
  
  $cp -p $make_conf $backup
  
  ( do_reconfig_makefile $make_conf )	
  
  case $? in 
    0) 	echo "Configured $make_conf successfully";;
    *) 	$mv -f $backup $make_conf ; 
       die "Configuration for Makefile FAILED: Exiting" ;;
  esac
  
  ConfigScript=$ConfigScript_default
  val=Y
  echo $ac_n "Run Configuration script ($ConfigScript):"\
   " [$val] $ac_c"
  read i
  test $i && val=$i
  if [ $val = Y ] || [ $val = y ]
  then
    while [[ ! -x ${make_conf_dir}/${ConfigScript} ]]
    do
      echo "$ConfigScript: not found or not "\
       "executable in $make_conf_dir"
      echo $ac_n "Enter Configure script name"\
       ":[$ConfigScript]$ac_c"
      read in
      test $in && ConfigScript=$in
    done
    
    ( cd $make_conf_dir; $ConfigScript -file $make_conf )
    
    case $? in
      0) echo "Configuration succesful";;
      *) die "Configuration FAILED - Exiting";;
    esac
  fi
  val=Y
  echo $ac_n "Compile: [$val] $ac_c"
  read i
  test $i && val=$i
  if [ $val = Y ] || [ $val = y ]
  then
    ( cd $Apache_src ; make )
    test $? != 0 && die "ERROR: make failed"
  fi
}


configure_and_compile()
{
  ConfigScript=$ConfigScript_default
  val=Y
  echo $ac_n "Run Configuration script ($ConfigScript):[$val] $ac_c"
  read i
  test $i && val=$i
  if [ $val = Y ] || [ $val = y ]
  then
    while [[ ! -x ${make_conf_dir}/${ConfigScript} ]]
    do
      echo "$ConfigScript: not found or not "\
       "executable in $make_conf_dir"
      echo $ac_n "Enter Configure script name"\
       ":[$ConfigScript]$ac_c"
      read in
      test $in && ConfigScript=$in
    done
    
    ( cd $make_conf_dir; ./$ConfigScript -file $make_conf )

    case $? in
      0) echo "Configuration successful";;
      *) die "Configuration FAILED - Exiting";;
    esac
  fi
  val=Y
  echo $ac_n "Compile: [$val] $ac_c"
  read i
  test $i && val=$i
  if [ $val = Y ] || [ $val = y ]
  then
    ( cd $Apache_src ; make )
    test $? != 0 && die "ERROR: make failed"
  fi
}


##
## Make a symbolic link from arg1 to arg2
## check if it exists first and present options if it does
##
make_symlink()
{
  rn=make_symlink
  case $# in
    2)	target=$2; source=$1 ;;
    *)	die "$rn:Incorrect number of args:$#" ;;	
  esac
  if [ -h $source ]
  then
    ## symlink already exists - leave it or change it?
    echo "Symbolic Link already exists" ;
    ls -l $source ;
    return 1
  else
    if [ -d $target ]
    then
      echo "Creating Symbolic Link $source->$target"
    else
      val=N ;
      echo $ac_n "WARNING: $target is not a directory. Create anyway:[$val]" $ac_c;
      read in ;
      test $in && val=$in ;
      case $val in
	N|n)	return 1;;
	*)	;;
      esac
    fi	 
    ln -s $target $source ;
    case $? in
      0) 	;;
      *) 	return 2 ;;
    esac						
  fi
}



##
## Edit run time configuration file
##
do_run_time() 
{
  rn=do_run_time;
  case $# in
    1) 	conf=$1;;
    2) 	conf=$1; tempfile=$2;;
    *) 	die "$rn:Incorrect number of args:$#" ;;
  esac
  
  test -f $conf || die "No Configuration file called $conf found"
  
  ##
  ## Check for AFS Directives
  ##
  ap=SetAFS
  afsdirective="AFSAuthType AFS"
  line=`grep -i "$afsdirective" $conf | grep -v '^#'`				
  case $line in
    "")	#echo "Editing $conf to add AFS Directives" 
	;;
    *) 	echo "AFS Directives already found in file $conf \n\n$line" ;
       echo ;
       exit 10 ;;
  esac
  
  tempfile=${tempfile:-/tmp/$prog.$$}
  #echo "Tempfile:$tempfile"
  
  echo "##" >> $tempfile
  echo "##AFS Authentication Directives for $PRODUCT_NAME $product_version" >> $tempfile
  echo "##Inserted by installation script $prog\n##" >> $tempfile
  echo >> $tempfile
  
  afsdirectives="${ap}DefaultCell $default_cell_name ${ap}CacheExpiration 120 ${ap}TokenExpiration 600 ${ap}WeblogPath $Weblog_Starter_default" ;
  AFSModuleFile=afs_module.c 
  
  echo "<ifmodule $AFSModuleFile>" >> $tempfile
  read_directives $tempfile $afsdirectives
  
  ##
  ## Check to ensure WebLogPath exists and is executable
  ##
  loc=`get_directive $tempfile ${ap}WeblogPath`
  while [[ ! -x $loc ]]
  do
    echo "${ap}WeblogPath: $loc Does not exist or not executable"
    echo "Make sure you copy both binaries weblog and "\
     "weblog_starter into the same dir that you specify the"\
     " FULL path to here - see installation instructions"
    echo $ac_n "Enter full path to weblog_starter: [$loc] $ac_c"
    read in 
    test $in && loc=$in
    new_directive="${ap}WeblogPath $loc"
    #		echo $tempfile; cat $tempfile 
    put_directives $tempfile $new_directive
    #		echo $tempfile; cat $tempfile
  done
  
  
  ##
  ## Get the SetAFSLocation directive's value and put AFSAuthType AFS 
  ##
  AFSAuthDirective="AFSAuthType AFS"
  loc=/afs
  echo $ac_n "Enter a Location for which AFS Authentication is desired: [$loc] $ac_c"
  read in
  test $in && loc=$in
  ##
  ## Make a symlink from documentRoot to afs mount points
  ##
  val=Y
  echo $ac_n "Make a symbolic link from the DocumentRoot " \
   "directory to the directory where the AFS cell mount points" \
   " for this AFS client machine are stored: [$val] $ac_c"
  read in
  test $in && val=$in
  case $val in
    y|Y) echo "Making symbolic link"
	 directive=`grep -i "DocumentRoot" $conf | grep -v '^#'`;
	 if [ ! "$directive" ] || [ "$directive" = "" ] || [ -z "$directive" ]
	 then
	   echo "No directive for DocumentRoot exists in $conf"
	   echo $ac_n "Document Root Directory (full path):[$DocRoot] $ac_c"
	   read in
	   test $in && DocRoot=$in
	   echo "DocumentRoot $DocRoot" >> $tempfile
	 else	
	   DocRoot=`echo $directive | awk '{ print $2 }'`
	 fi;
	 test -d "$DocRoot" || die "$DocRoot not a Directory";
	 mtp=$DocRoot/$loc ;

	 afsdir=/afs ;
	 echo $ac_n "Enter directory where AFS mount points are: [$afsdir] $ac_c";
	 read in ;
	 test $in && afsdir=$in ;
	 test -d $afsdir || die "ERROR:$afsdir is not a directory";
	 make_symlink $mtp $afsdir
	 case $? in
	   0) 	;;
	   1) 	echo "Symlink Exists or cancelled" ;;	
	   *) 	die "ERROR making symlink from $mtp to $afsdir."\
	        "Exiting"
	      ;;
	 esac			
	 ;;
    *)	;;
  esac					
    
  echo "<Location $loc>" >> $tempfile
  echo $AFSAuthDirective >> $tempfile
  authname_default='"AFS Login (user@<%ccellname>)"'
  echo "Set the AuthName directive to customize the Login prompt" \
   "that users will see"
  echo "You may use text intersperced with the following " \
   "expressions (these should be enclosed in angle brackets):"
  echo  "%c to specify the AFS cell name where the requested file resides"
  echo "%u to specify the first two components of the URL"
  echo "%d to specify the default cell" 
  echo "Consult documentation for more details"	
  echo $ac_n "AFSLoginPrompt [$authname_default]" $ac_c
  read n
  case $n in
    '') ;;
    *)  authname_default="$n" ;;
  esac
  echo AFSLoginPrompt $authname_default >> $tempfile
  echo "</Location>" >> $tempfile
  
  val=N
  echo $ac_n "Log access denied requests: [$val] $ac_c"
  read in ; test $in && val=$in
  case $val in 
    y|Y) accesslogfile=$ServerRoot/logs/forbiddenlog ;
	 echo $ac_n "Log file: [$accesslogfile]" $ac_c;
	 read in ; test $in && accesslogfile=$in;
	 echo "SetAFSAccessLog $accesslogfile">>$tempfile
	 ;;
    *)	;;
  esac
  
  if [ $apache_version != 1.3 ]
  then     
    ud=`grep -i UserDir $conf`
    case $ud in
      '') val=Y ;
	  echo $ac_n "Add support for User Directories: [$val] $ac_c";
	  read in ; test $in && val=$in
	  case $val in 
	    y|Y) userdir=/usr/ ;
		 echo $ac_n "UserDir: [$userdir]" $ac_c;
		 read in ; test $in && userdir=$in;
		 echo "UserDir $userdir">>$tempfile;
		 echo "Need to ensure userdir_module is included"
		 echo "in the Makefile configuration .... "
		 mc=' ';
		 make_conf_dir=${make_conf%/*} ;
		 while [[ ! -f $mc ]]
		 do
		   echo $ac_n "Makefile Configuration file name [$make_conf]:$ac_c"
		   read uconf
		   case $uconf in
		     '') ;;
		     /*) make_conf=$uconf ; 
			 make_conf_dir=${make_conf%/*};
			 ;;
		     *)  make_conf=${make_conf_dir}/${uconf};
			 make_conf_dir=${make_conf%/*}; 
			 ;;
		   esac
		   mc=$make_conf
		   test -f $mc || echo "$mc: does not exist"
		 done
		 add_module $make_conf userdir_module mod_userdir;
		 case $? in
		   1) echo "Need to reconfigure and recompile";
		      configure_and_compile
		      ;;
		   2) echo "Failed to add module userdir_module"
		      ;;
		   0) ;;
		 esac
		 ;;
	    *)		;;
	  esac ;;
      *) ;;
    esac
  fi # /* if Apache 1.3 */
  
  echo "</ifmodule>" >> $tempfile
  echo "##\n##End AFS Authentication Directives for $PRODUCT_NAME \n##" >> $tempfile
  
  ##
  ## Append tempfile to run time config file and remove tempfile
  ##
  cat $tempfile >> $conf
  $rm -f $tempfile
}


##
## Configures the Configuration file for the Apache Makefile
##
do_config_makefile()
{
  rn=do_config_makefile
  case $# in
    1) ;;
    *) die "Incorrect number of args given to $rn: $#";;
  esac
  
  conf=$1
  
  ##
  ## see if there is a line for EXTRA_LIBS and add 
  ## EXTRA_LIBS=libsys.a and libapacheafs.a 
  ## or append to whatever was there before 
  ##
  currfield=EXTRA_LIBS
  elib=`grep $currfield $conf | grep -v '^#'`
  case $elib in
    '') 	prevfield="EXTRA_CFLAGS";
	echo "WARNING: No $currfield line was found in $conf" ;
	echo "Attempting to insert $currfield after $prevfield" ;
	echo "Ensure you have a $currfield entry in $conf" ;
	r=Y ;
	echo $ac_n "Go ahead with this: [$r] $ac_c" ;
	read in ;
	test $in && r=$in ;
	if [ $r != "Y" ] || [ $r != "y" ]
	then 
	  die "Edit $conf and add $currfield ="
	fi ;
	insertfield $conf $currfield -p $prevfield 
	;;
    *)	;;
  esac	
  
  elib=`awk '
  { 	
    split($0,str,"="); 
    if (str[1] == "'$currfield'") 
      { 
	split($0,str,"=");
	print str[2]	 
      } 
  } ' $conf`
  
  libpath=$ServerRoot
  for lib in $aplib $syslib
  do
    elibval=`echo $elib | grep "$lib"`
    if [ ! "$elibval" ]||[ "$elibval" = "" ]||[ -z "$elibval" ] 
    then
      #echo "$lib doesn't exist in $currfield $elib"
      libfull=' '
      until [[ -f $libfull ]]
      do
	case $lib in
	  $syslib) libpath="/usr/afsws/lib/afs/"
		   ;;
	  $aplib)	 libpath=$ServerRoot ;;
	esac
	
	echo $ac_n "Enter only the path" \
	 " (not the filename) for $lib:[$libpath]$ac_c "
	read in
	test $in && libpath=$in
	libfull=$libpath/$lib
	if [ ! -f $libfull ] 
	then 
	  echo "$libfull DOESN'T EXIST"
	fi
      done
      insertfield $conf $libfull -f $currfield
    else
      echo "$lib Exists in $elib" >> $Log
    fi
  done
      
  ##
  ## Check for afs_module
  ##
  if [ $apache_version = 1.3 ]
  then
    AFSModule=modules/extra/afs_module.o
  else
    AFSModule=afs_module.o 
  fi
  
  AFSModuleName="afs_module"
  tmod=`grep $AFSModuleName $conf | grep -v '^#' | awk '{ if ($2 == "'$AFSModuleName'" ) printf "%d:%s\n", NR, $0 }'`
  if [ "$tmod" ]
  then 
    echo "AFS module already exists in file $conf" >> $Log
    echo "Line: $tmod" >> $Log
  else
    ##
    ## List the active modules and prompt user where to insert AFS MODULE
    ##
    echo
    echo
    echo "YOUR LIST OF CURRENT MODULES IS"
    awk '{ if ($1 == "Module" || $1 == "AddModule") print $0 }' $conf | $more
    if [ $apache_version = 1.3 ]
    then
      defmod=modules/standard/mod_auth.o
    else
      defmod=auth_module
    fi
    echo $ac_n "After which module would you like to put the AFS Module [$defmod]: $ac_c"
    read after_module
    test $after_module || after_module=$defmod
    tmod=`awk '{ if ($2 == "'$after_module'") print $0 }' $conf`
    echo $tmod
    test ! "$tmod" || test "$tmod" = "" || test -z "$tmod" && die "Module $after_module NOT FOUND IN MODULE LIST"
    while true
    do
      case $tmod in
	'')	;;
	*)	break ;;
      esac
    done	
    insertfield $conf "$AFSModule" -pn $after_module
    fi
    
    ##
    ## Attempt to set the correct compiler according to OSTYPE
    ##
    set_compiler $conf
}
	    
##
## Edit the start and stop scripts
##
do_start_stop()
{
  rn=do_start_stop
  args=3
  case $# in
    $args) 	file=$2 ;;
    *) 	die "rn: Incorrect number of parameters supplied: $#";;
  esac
  
  case $1 in
    START)	start=1; bin=$3 ;;
    STOP)	start=0; run_conf=$3; edit=0 ;;
    E_STOP)	start=0; run_conf=$3; edit=1 ;;
    *)	die "$rn: Incorrect type of ARG passed: $1" ;;
  esac
  
  until [[ -f $run_conf ]]
  do
    echo; echo
    echo "No run_time configuration file $run_conf found"
    echo $ac_n "Run-time conf file:[$run_conf]$ac_c"
    read in
    test $in && run_conf=$in
  done	
  
  if [ $start = 1 ]
  then
    echo "#!/bin/sh" > $file 
    echo "##" >> $file
    echo "##Apache $PRODUCT_NAME StartUp" >> $file
    echo "##" >> $file
    echo "$bin -f $run_conf" >> $file
    echo >> $file
    chmod +x $file
    
  else 
    pfl=`grep -i pidfile $run_conf | grep -v "#"`
    
    test ! "$pfl" || test "$pfl" = "" || test -z "$pfl" && die "No PidFile specified in $run_conf. Edit this file and specify the PidFile directive. Exiting"
    pid_file=`echo $pfl| awk '{ print $2 }'` 
      
      test ! "$pid_file" || test "$pid_file" = "" || test -z "$pid_file" && die "No PidFile specified in $run_conf. Edit this file and specify the PidFile directive. Exiting"
      
      
      if [ $edit = 0 ] 
      then
	echo "#!/bin/sh" > $file 
	echo "##" >> $file
	echo "##Apache $PRODUCT_NAME Shutdown">>$file
	echo "##" >> $file
	echo "kill -TERM \`cat $pid_file.afs\`">>$file
	echo "kill -TERM \`cat $pid_file\`" >> $file
	echo >> $file
	chmod +x $file
	
      else 
	if [ ! -x $file ]
	then
	  die "$file: does not exist"\
	   " or is not executable. Exiting"
	fi
	
	t=`grep -i "\.afs" $file`
	if [ ! "$t" ] || [ -z "$t" ] || [ "$t" = "" ]
	then
	  temp=/tmp/${prog}stopd.$$
	  $cp -f $file $temp
	  awk '{ line[NR]=$0 }
	    END { 
	      printf "kill -TERM `%s.afs`\n", "'$pid_file'"
	      for (i=1; i <= NR; i++) {	
		printf "%s\n", line[i] }
              }' $temp > $file
	      $rm $temp 
	fi			 
      fi
  fi
}


configure_makefile_forwebsecure()
{
  val=Y
  echo $ac_n "Change Apache Configuration file [$val]:$ac_c"
  read in
  test $in && val=$in
  
  if [ $val = Y ] || [ $val = y ]
  then
    mc=' '
    make_conf_dir=${make_conf%/*}
    while [[ ! -f $mc ]]
    do
      echo $ac_n "Configuration file name [$make_conf]:$ac_c"
      read uconf
      case $uconf in
	'')	;;
	/*)	make_conf=$uconf ; 
	    make_conf_dir=${make_conf%/*};
	    ;;
	*) 	make_conf=${make_conf_dir}/${uconf};
	   make_conf_dir=${make_conf%/*}; 
	   ;;
      esac
      mc=$make_conf
      test -f $mc || echo "$mc: does not exist"
    done
    
    # Back up original Configuration file
    backup=$make_conf.BEFOREAFS
    number=0
    while [ -f $backup ]
    do
      backup=$make_conf.BEFOREAFS.$number
      number=$(($number+1))
    done
    
    $cp -p $make_conf $backup
    
    ( do_config_makefile $make_conf )	
    
    case $? in 
      0) 	echo "Configured $make_conf successfully";;
      *) 	$mv -f $backup $make_conf ; 
	 die "Configuration for Makefile FAILED: Exiting" ;;
    esac
    
    ConfigScript=$ConfigScript_default
    val=Y
    echo $ac_n "Run Configuration script ($ConfigScript):"\
     " [$val] $ac_c"
    read i
    test $i && val=$i
    if [ $val = Y ] || [ $val = y ] 
    then
      while [[ ! -x ${make_conf_dir}/${ConfigScript} ]]
      do
	echo "$ConfigScript: not found or not "\
	 "executable in $make_conf_dir"
	echo $ac_n "Enter Configure script name"\
	 ":[$ConfigScript]$ac_c"
	read in
	test $in && ConfigScript=$in
      done
      
      ( cd $make_conf_dir; ./$ConfigScript -file $make_conf )
      
      case $? in
	0) echo "Configuration successful";;
	*) die "Configuration FAILED - Exiting";;
      esac
    fi

    # Before compiling - check afs_module is in appropriate directory
    if [ $apache_version = 1.3 ]
    then
      sd=${Apache_src}/modules/extra/afs_module.c
    else
      sd=${Apache_src}/afs_module.c
    fi
    while [ ! -f $sd ]
    do
      echo "File $sd does not exist"
      val=`pwd`/afs_module.c
      while [ ! -f $val ]
      do
	echo $ac_n "Enter the location of the source file afs_module.c that came with the $PRODUCT_NAME distribution [ $val ] $ac_c"
	read in
	test $in && val=$in
      done
      echo "Copying $val to $sd"
      cp -p $val $sd
    done
    
    val=Y
    echo $ac_n "Compile: [$val] $ac_c"
    read i
    test $i && val=$i
    if [ $val = Y ] || [ $val = y ]
    then
      ( cd $Apache_src ; make )
      test $? != 0 && die "ERROR: make failed"
      echo "Successful compilation."
      echo "NOTE: If necessary do a make install after this script finishes"
      fi
    fi
}

configure_runtime_forwebsecure()
{
  rc=' '
  run_conf_dir=${run_conf%/*}
  while [[ ! -f $rc ]]
  do
    echo $ac_n "Run time configuration file name "\
     "[$run_conf]:$ac_c"
    read uconf
    case $uconf in
      '')	;;
      /*)	run_conf=$uconf ; 
	  run_conf_dir=${run_conf%/*};
	  ;;
      *) 	run_conf=${run_conf_dir}/${uconf};
	 run_conf_dir=${run_conf%/*}; 
	 ;;
    esac
    rc=$run_conf
    test -f $rc || echo "$rc: does not exist"
  done
  
  # Back up original Configuration file
  backup=$run_conf.BEFOREAFS
  number=0
  while [ -f $backup ]
  do
    backup=$run_conf.BEFOREAFS.$number
    number=$(($number+1))
  done
  
  $cp -p $run_conf $backup
  
  tempfile=/tmp/$prog.$$
  #( do_run_time $run_conf $tempfile )
  ( do_run_time $run_conf )
  case $? in
    0) 	echo "Run time configuration complete."\
     " File $run_conf" ;;
       10) 	$rm -f $backup;
       echo "Run-Time configuration not required" ;; 
    *) 	$rm -f $tempfile; 
       mv -f $backup $run_conf;
       die "RUN_TIME CONFIGURATION FAILED";;
  esac
  #	fi
}



unconfigure()
{
  if [ -f $Apache_src/*.BEFOREAFS* ]
  then
    eg=`grep "mod_afs.o" $Apache_src/*`
    case $eg in
      '') ;;
      *)
	 echo "Found backup config file for Makefile Configuration" ;
	 /bin/ls -l $Apache_src/*.BEFORE* ;
	 echo "You may have a previously installed version of AFS WebSecure" ;
	 echo "This may conflict with installing this version." ;
	 echo "Please restore all the configuration files to the state they" \
	  "were before WebSecure was installed. " \
	  "Remove *.BEFOREAFS* in the Apachesrc directory and re-run this script";
	 die "Exiting..."		
	 ;;
    esac
  fi

  if [ -f $ServerRoot/conf/*.BEFOREAFS* ]
  then
    eg=`grep -i setafslocation $ServerRoot/conf/*`
    case $eg in
      '') 	;;
      *) echo "Found backup for run-time configuration" ;
	/bin/ls -l $ServerRoot/conf/*.BEFOREAFS* 
	echo "You may have a previously installed version of AFS WebSecure"
	echo "Please restore all the configuration files to the state " \
	 "they were before WebSecure was installed. " \
	 "Remove *.BEFOREAFS* in the Apache conf directory " \
	 "and re-run this script." ;
	die "Exiting ... " ;;
      *)	;;
    esac
  fi	
}


install_web_secure()
{
  product_version=1.0
  unconfigure
  
  configure_makefile_forwebsecure
  configure_runtime_forwebsecure
  echo "$product Configuration Complete" 
}


make_start_stop()
{
  A="Create new Startup Script"
  B="Create new ShutDown Script"
  C="Edit existing Shutdown Script to shutdown AFS Web Secure"
  
  select c in "$A" "$B" "$C" 
  do
    case $c in
      $A) echo $ac_n "Full path to webserver binary:"\
       " [$binary]$ac_c" ;
	  read in;
	  test $in && binary=$in;
	  if [ ! -x $binary ]
	  then
	    echo; echo
	    echo "$binary does not exist"
	    echo "Check the path or compile Apache first"
	    return
	  fi
	  echo $ac_n "Full path to new script: [$startscr]$ac_c";
	  read in;
	  test $in && startscr=$in;
	  if [ -x $startscr ]
	  then
	    die "$startscr already exists. Exiting"
	  fi;
	  ( do_start_stop START $startscr $binary );
	  break	
	  ;;
      $B) echo $ac_n "Full path to new script:[$stopscr] $ac_c";
	  read in;
	  test $in && stopscr=$in;
	  if [ -x $stopscr ]
	  then
	    die "$stopscr already exists. Exiting"
	  fi;
	  ( do_start_stop STOP $stopscr $run_conf );
	  break
	  ;;
      $C) echo $ac_n "Existing script:[$stopscr] $ac_c";
	  read in;
	  test $in && stopscr=$in;
	  if [ ! -x $stopscr ]
	  then
	    die "$stopscr doesn't exist. Exiting"
	  fi;
	  ( do_start_stop E_STOP $stopscr $run_conf );
	  break 
	  ;;
    esac
  done
  
  ret=$?
  if [ $ret -eq 0 ] 
  then 
    echo "Done"
  else
    die "ERROR: Editing Start and Stop scripts. Exiting"
  fi
}

start_apache()
{
  val=Y
  echo $ac_n "Start up the server: [$val] $ac_c"
  read in
  test $in && val=$in
  if [ $val = Y ] || [ $val = y ] 
  then
    exec $startscr 
  fi
}



main()
{
  
  if [ $firsttime = 0 ]
  then
    firsttime=1
    clear
  else 
    prompt=M
    echo; echo; echo; echo
    echo | awk '{ for (i=0; i < 80; i++) { printf "-" } }'
      echo
      echo $ac_n "Main menu (M) or Exit (X): [$prompt] $ac_c"
      read in
      test $in && prompt=$in
      test $prompt = 'X' || test $prompt = 'x' && exit 0;
  fi
  init_prompt
  echo; echo; echo 
  
  A="AFS Web Secure Setup"
  E="Make or Edit start and stop scripts"
  F="Start up Apache Server"
  G="Exit"
  
  select choice in "$A" "$E" "$F" "$G"
  do
    case $choice in
      $A)	install_web_secure ;;
      $E)	make_start_stop ;;
      $F)	start_apache ;;
      $G)	exit 0 ;;
    esac
    break
  done
}

welcome_screen()
{
  echo; echo; echo; echo
  init_prompt
  echo "Thank you for downloading $PRODUCT_NAME for Apache."
  echo "This script will help you install this product."
  echo "Please read the documentation accompanying this product before continuing."
  echo "Also ensure that you have installed the source files, "
  echo "libraries and binaries in the recommended directories"
  echo
  echo "Contact $COMPANY for licensing information"
    echo "11 Stanwix Street"
    echo  "Pittsburgh, PA  15222"
    
    echo
    val=Y; echo $ac_n "Continue? [$val] $ac_c"
    read in ; test $in && val=$in
    case $val in
      Y|y) 	clear ; break ;;
      *) 	echo "Exiting $prog" ; exit 0 ;;
    esac
    
    echo "$message" | more
    echo
    
    val=Y; echo $ac_n "Continue? [$val] $ac_c"
    read in ; test $in && val=$in
    case $val in
      Y|y) 	clear; break ;;
      *)	exit 0;;
    esac
}

#############################################################################
##
## HERE IS WHERE IT ACTUALL STARTS
##
############################################################################


#setting the nonew line stuff
case `echo -n` in
  -n) 	ac_n= ; ac_c='\c' ;;
  *)    	ac_n='-n' ; ac_c= ;;
esac

case $show_welcome in
  1) ;;
  *) welcome_screen ;;
esac

##
## Initialize parameters to defaults
##
ServerRoot=$ServerRoot_default
sr=' '
until [[ -d $sr ]]
do
  echo $ac_n "Enter Server Root directory: [$ServerRoot] $ac_c"
  read in
  case $in in
    '') 	;;
    *)	ServerRoot=$in
       ;;
  esac
  test -d $ServerRoot || echo "$ServerRoot: No such directory"
  sr=$ServerRoot
done
test $in && change_default ServerRoot_default $ServerRoot

Apache_src=$Apache_src_default
as=' '
until [[ -d $as ]] 
do
  
  echo $ac_n "Enter Apache src directory: [$Apache_src] $ac_c"
  read in
  case $in in
    '')	;;
    *)	Apache_src=$in
       ;;
  esac
  test -d $Apache_src || echo "$Apache_src: No such directory"
  as=$Apache_src
done
echo
test $in && change_default Apache_src_default $Apache_src


make_conf=$Apache_src/$make_conf_default
run_conf=$ServerRoot/conf/$run_conf_default
binary=$ServerRoot/$binary_default
startscr=$ServerRoot/$startscr_default
stopscr=$ServerRoot/$stopscr_default
DocRoot=$ServerRoot/$DocRoot_default

firsttime=0

##
## Determine which version of Apache this is
## default to 1.3 since that is what everyone should be running!
##
apache_version=1.3
if [ ! -d $Apache_src/modules/standard ]
then
  ## prompt user to confirm Apache 1.2
  echo $ac_n "Is this Apache 1.2.X?: [N] $ac_c"
  read n
  case $n in
    y|Y) apache_version=1.2;;
  esac
fi

echo "Apache Version: $apache_version" >> $Log

echo `date` >> $Log
while true
do
  main
done










