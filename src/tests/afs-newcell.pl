#!/usr/bin/env perl
# Copyright (C) 2000 by Sam Hartman
# This file may be copied either under the terms of the GNU GPL or the IBM Public License
# either version 2 or later of the GPL or version 1.0 or later of the IPL.

use warnings;
use strict;
use Term::ReadLine;
use OpenAFS::ConfigUtils;
use OpenAFS::Dirpath;
use OpenAFS::OS;
use OpenAFS::Auth;
use Getopt::Long;
use Pod::Usage;
use Socket;

=head1  NAME

   afs-newcell - Set up the initial database and file server for a new OpenAFS cell.

=head1 SYNOPSIS

B<afs-newcell>
       [ B<--batch> ]
       [ B<--debug> ]
       [ B<--unwind> ]
       [ B<--help> ]
       [ B<--server>=hostname ]
       [ B<--cellname>=cell ]
       [ B<--partition>=partition ]
       [ B<--admin>=administrator ]
       [ B<--kerberos-type>=authentication_type ]
       [ B<--kerberos-realm>=realm_name ]
       [ B<--kerberos-keytab>=keytab_file ]
       [ B<--with-dafs> ]
       [ B<--options-ptserver>=options ]
       [ B<--options-vlserver>=options ]
       [ B<--options-fileserver>=options ]
       [ B<--options-volserver>=options ] 
       [ B<--options-salvageserver>=options ]
       [ B<--options-salvager>=options ]

=head1 DESCRIPTION

This script sets up the initial AFS database and configures the first
database/file server. It also sets up an AFS cell's root volumes.  The
fileserver and database server binaries must already be installed.  The
fileserver should have an empty root.afs. This script creates root.cell, user,
service and populates root.afs.  

The B<batch> option specifies that the initial requirements have been met and
that the script can proceed without displaying the initial banner or asking for
confirmation.

The B<admin> option specifies the name of the administrative user.
This user will be given system:administrators and susers permission in
the cell.

The B<cellname> option specifies the name of the cell.

The B<partition> option specifies the partition letter of the AFS partition. The
default value is 'a', for /vicepa.

=head1 PREREQUISITES

The following requirements must be met before running
this script.

This machine must have a working, empty filesystem mounted on /vicepa. A
different partition letter may be specified by the partition command line
option. For example, to use /vicepb, specify '--partition=b' on the command
line.

The OpenAFS client and server binaries must be installed on this machine. 

A working Kerberos realm with Kerberos4 support must be available. Supported
Kerberos implementations are Heimdal with Kth-kerberos compatibility, MIT
Kerberos5, and Kaserver (deprecated).  

Create the single-DES AFS key and write it to a keytab file using the kerberos
kadmin program.
   
Create a principal called afs/cellname in your realm.  The cell name should be
all lower case, unlike Kerberos realms which are all upper case.  You can use
the asetkey command or if you used AFS3 salt to create the key, the bos addkey
command. The asetkey command is built when OpenAFS built with Kerberos support.
The asetkey command requires a cell configuration.

You will need an administrative principal created in a Kerberos realm.  This
principal will be added to system:administrators and thus will be able to run
administrative commands.  Generally the user is a root instance of some
administrative user.  For example if jruser is an administrator then it would be
reasonable to create jruser/root and specify jruser/root as the user to be
added in this script using the 'admin' command line option.  You will also need
to create a keyfile for this adminstrative user which is used by the script to
obtain a ticket. The keyfile must be located in
$openafsdirpath->{'afsconfdir'}/krb5.conf.

The AFS client must not be running on this workstation.  It will be started
during the execution of this script.

=head1 AUTHOR

Sam Hartman <hartmans@debian.org>

=cut


my $term = new Term::ReadLine('afs-newcell');
my $path = $OpenAFS::Dirpath::openafsdirpath;

#-----------------------------------------------------------------------------------
# prompt(message, default_value)
#
sub prompt($$) {
  my ($message,$default) = @_;
  my $value = $term->readline("$message [$default] ");
  unless ($value) {
    $value = $default;
  }
  return $value;
}

#-----------------------------------------------------------------------------------
# mkvol(volume, mount, partition)
#
sub mkvol($$$$) {
    my ($vol, $mnt, $srv, $part) = @_;
    run("$path->{'afssrvsbindir'}/vos create $srv $part $vol -maxquota 0");
    unwind("$path->{'afssrvsbindir'}/vos remove $srv $part $vol");
    run("$path->{'afssrvbindir'}/fs mkmount $mnt $vol ");
    run("$path->{'afssrvbindir'}/fs setacl $mnt system:anyuser rl");
}

#-----------------------------------------------------------------------------------
# check_program($prog) - verify the program is installed.
#
sub check_program($) {
  my ($program) = @_;
  unless ( -f $program ) {
     die "error: Missing program: $program\n";
  }
  unless ( -x $program ) {
     die "error: Not executable: $program\n";
  }
}

#-----------------------------------------------------------------------------------
# main script

# options
my $batch = 0;
my $debug = 0;
my $unwind = 1;
my $help = 0;
my $cellname = 'testcell';
my $partition = 'a';
my $admin = 'admin';
my $kerberos_type = 'MIT';
my $kerberos_realm = 'TESTCELL';
my $kerberos_keytab = "$path->{'afsconfdir'}/krb5.keytab";
my $with_dafs = 0;
my $options_ptserver = '';
my $options_vlserver = '';
my $options_fileserver = '';
my $options_volserver = '';
my $options_salvageserver = '';
my $options_salvager = '';

my $server = `hostname -f`;
chomp $server;

GetOptions (
       "batch!" => \$batch, 
       "debug!" => \$debug,
       "unwind!" => \$unwind,
       "help" => \$help,
       "server=s" => \$server,
       "cellname=s" => \$cellname, 
       "partition=s" => \$partition,
       "admin=s" => \$admin,
       "kerberos-type=s" => \$kerberos_type,
       "kerberos-realm=s" => \$kerberos_realm,
       "kerberos-keytab=s" => \$kerberos_keytab,
       "with-dafs" => \$with_dafs,
       "options-ptserver=s" => \$options_ptserver,
       "options-vlserver=s" => \$options_vlserver,
       "options-fileserver=s" => \$options_fileserver,
       "options-volserver=s" => \$options_volserver,
       "options-salvageserver=s" => \$options_salvageserver,
       "options-salvager=s" => \$options_salvager,
       );

if ($help) {
  pod2usage(1);
  exit 0;
}

# To print debug messages in the run() calls.
$OpenAFS::ConfigUtils::debug = $debug;

#-----------------------------------------------------------------------------
# Prereq: Must be root and must not already have a cell configuration.
#
my @problems = ();
my $try_rm_cell = 0;

if ($> != 0) {
  push(@problems, "You must be root to run this script.");
}

my @afsconfigfiles = (
  "$path->{'afsconfdir'}/ThisCell",
  "$path->{'afsconfdir'}/CellServDB",
  "$path->{'afsconfdir'}/UserList",
  "$path->{'afsdbdir'}/prdb.DB0",
  "$path->{'afsbosconfigdir'}/BosConfig",
  "$path->{'afsddir'}/ThisCell",
  "$path->{'afsddir'}/CellServDB",
);
foreach my $configfile (@afsconfigfiles) {
  if ( -f $configfile ) {
    push(@problems, "Configuration file already exists, $configfile.");
    $try_rm_cell = 1;
  }
}

if (@problems) {
  foreach my $problem (@problems) {
    print "error: $problem\n";
  }
  print "info: Try running afs-rmcell.pl\n" if $try_rm_cell;
  exit 1;
}

#-----------------------------------------------------------------------------
# Prereq: System requirements notification.
#
unless ($batch) {

  print <<eoreqs;
                           REQUIREMENTS

The following requirements must be meet before running
this script. See 'pod2text $0' for more details.

1) A filesystem must be mounted on /vicepa. (See
   the --partition option for alternative mount points.)

2) The OpenAFS client and server binaries must be installed.
   There should be no remnants from a previous cell. 
   Run afs-rmcell to remove any.

3) A Kerberos realm with Kerberos 4 support must be available.
   Supported Kerberos implementations are Heimdal with
   Kth-kerberos compatibility, MIT Kerberos 5, and 
   Kaserver (deprecated). 

4) A Kerberos keytab file containing the afs principal 
   and the administrator principal must be be present.
   See the asetkey man page for information about creating the
   keytab file.  The default name of the administrator 
   principal is 'admin'. See the --admin option for
   alternative names.

eoreqs

  my $answer = prompt("Does your system meet these requirements? (yes/no)", "no");
  unless ($answer=~/^y/i ) {
    print "OK: Aborted.\n";
    exit 0;
  }
}

#-----------------------------------------------------------------------------
# Prereq: Verify required binaries, directories, and permissions.
#
my $bosserver = "$path->{'afssrvsbindir'}/bosserver";
my $bos       = "$path->{'afssrvbindir'}/bos";
my $fs        = "$path->{'afssrvbindir'}/fs";
my $pts       = "$path->{'afssrvbindir'}/pts";
my $vos       = "$path->{'afssrvsbindir'}/vos";
my $afsrc     = "$path->{'initdir'}/afs.rc";
my $aklog     = "$path->{'afswsbindir'}/aklog";
my $tokens    = "$path->{'afswsbindir'}/tokens";
my $klog      = "$path->{'afswsbindir'}/klog";
my $kas       = "$path->{'afssrvsbindir'}/kas";

check_program($bosserver);
check_program($bos);
check_program($fs);
check_program($pts);
check_program($vos);
check_program($afsrc);
check_program($tokens);

#-----------------------------------------------------------------------------
# Prereq: Cell configuration
#
if ($batch) {
  if ($kerberos_type!~/kaserver/i) {
    check_program($aklog);
    unless ( -f $kerberos_keytab ) {
      die "error: Missing keytab file: $kerberos_keytab\n";
    }
  }
}
else {
  my $answer;
  get_options: {
    $answer = prompt("Print afs-newcell debugging messages? (yes/no)", $debug ? "yes" : "no");
    $debug = ($answer=~/^y/i) ? 1 : 0;

    print "\nServer options:\n"; 
    $server = prompt("What server name should be used?", $server);
    $cellname = prompt("What cellname should be used?", $cellname);
    $partition = prompt("What vice partition?", $partition);
    $admin = prompt("What administrator username?", $admin);
    if($admin =~ /@/) {
      die "error: Please specify the username without the realm name.\n";
    }
  
    print "\nKerberos options:\n";
    $kerberos_type = prompt("Which Kerberos is to be used?", $kerberos_type);
    if ($kerberos_type=~/kaserver/i) {
      check_program($klog);
      check_program($kas);
    }
    else {
      check_program($aklog);
      $kerberos_realm = $cellname;
      $kerberos_realm =~ tr/a-z/A-Z/;
      $kerberos_realm = prompt("What Kerberos realm?", $kerberos_realm);
      get_keytab: {
        $kerberos_keytab = prompt("What keytab file?", $kerberos_keytab);
        unless ( -f $kerberos_keytab ) {
          print "Cannot find keytab file $kerberos_keytab\n";
          redo get_keytab;
        }
      }
    }
  
    print "\nDatabase Server options:\n";
    $options_ptserver = prompt("ptserver options:", $options_ptserver);
    $options_vlserver = prompt("vlserver options:", $options_vlserver);

    print "\nFileserver options:\n";
    $answer = prompt("Use DAFS fileserver (requires DAFS build option)? (yes/no)", "no");
    $with_dafs = ($answer=~/^y/i) ? 1 : 0;
    $options_fileserver = prompt("fileserver options:", $options_fileserver);
    $options_volserver = prompt("volserver options:",  $options_volserver);
    $options_salvageserver = prompt("salvageserver options:",  $options_salvageserver);
    $options_salvager = prompt("salvager options:", $options_salvager);
  
    print "\nConfirmation:\n";
    print "Server name            : $server\n";
    print "Cell name              : $cellname\n";
    print "Partition              : $partition\n";
    print "Administrator          : $admin\n";
    print "Kerberos               : $kerberos_type\n";
    if ($kerberos_type!~/kaserver/i) {
      print "Realm                  : $kerberos_realm\n";
      print "Keytab file            : $kerberos_keytab\n";
    }
    print "DAFS fileserver        : ", $with_dafs ? "yes" : "no", "\n";
    print "ptserver options       : $options_ptserver\n";
    print "vlserver options       : $options_vlserver\n";
    print "fileserver options     : $options_fileserver\n";
    print "volserver options      : $options_volserver\n";
    print "salvagerserver options : $options_salvageserver\n";
    print "salvager options       : $options_salvager\n";
    print "\n";
  
    $answer = prompt("Correct? (yes/no/quit)", "yes");
    exit(0)          if $answer=~/^q/i;
    redo get_options if $answer!~/^y/i;
  }

  # Save the options as a shell script for the next run.
  $answer = prompt("Save these options? (yes/no)", "yes");
  if ($answer=~/^y/i ) {
    my $script = '';
    get_script_name: {
      $script = prompt("File name for save?", "run-afs-newcell.sh");
      last get_script_name if ! -f $script;

      $answer = prompt("File $script already exists. Overwrite? (yes/no/quit)", "no");
      exit(0)              if $answer=~/^q/i;
      last get_script_name if $answer=~/^yes/i;
      redo get_script_name;
    }

    my @switches = ();
    push(@switches, "--batch"); # automatically added to the script
    push(@switches, "--debug")                                          if $debug;
    push(@switches, "--nounwind")                                       unless $unwind;
    push(@switches, "--server='$server'")                               if $server;
    push(@switches, "--cellname='$cellname'")                           if $cellname;
    push(@switches, "--partition='$partition'")                         if $partition;
    push(@switches, "--admin='$admin'")                                 if $admin;
    push(@switches, "--kerberos-type='$kerberos_type'")                 if $kerberos_type;
    push(@switches, "--kerberos-realm='$kerberos_realm'")               if $kerberos_realm;
    push(@switches, "--kerberos-keytab='$kerberos_keytab'")             if $kerberos_keytab;
    push(@switches, "--with-dafs")                                      if $with_dafs;
    push(@switches, "--options-ptserver='$options_ptserver'")           if $options_ptserver;
    push(@switches, "--options-vlserver='$options_vlserver'")           if $options_vlserver;
    push(@switches, "--options-fileserver='$options_fileserver'")       if $options_fileserver;
    push(@switches, "--options-volserver='$options_volserver'")         if $options_volserver;;
    push(@switches, "--options-salvageserver='$options_salvageserver'") if $options_salvageserver;;
    push(@switches, "--options-salvager='$options_salvager'")           if $options_salvager;
  
    open(SCRIPT, "> $script") or die "error: Cannot open file $script: $!\n";
    print SCRIPT "#!/bin/sh\n";
    print SCRIPT "perl afs-newcell.pl \\\n";
    print SCRIPT join(" \\\n", map("  $_", @switches));
    print SCRIPT "\n\n";
    close SCRIPT;
    chmod(0755, $script);
  }
}

if ($debug) {
  print "debug: afs-newcell options\n";
  print "debug:  \$batch = '$batch'\n";
  print "debug:  \$debug = '$debug'\n";
  print "debug:  \$unwind = '$unwind'\n";
  print "debug:  \$help = '$help'\n";
  print "debug:  \$server = '$server'\n";
  print "debug:  \$cellname = '$cellname'\n";
  print "debug:  \$partition = '$partition'\n";
  print "debug:  \$admin = '$admin'\n";
  print "debug:  \$kerberos_type = '$kerberos_type'\n";
  print "debug:  \$kerberos_realm = '$kerberos_realm'\n";
  print "debug:  \$kerberos_keytab = '$kerberos_keytab'\n";
  print "debug:  \$with_dafs = '$with_dafs'\n";
  print "debug:  \$options_pteserver = '$options_ptserver'\n";
  print "debug:  \$options_pteserver = '$options_vlserver'\n";
  print "debug:  \$options_fileserver = '$options_fileserver'\n";
  print "debug:  \$options_volserver = '$options_volserver'\n";
  print "debug:  \$options_salvageserver = '$options_salvageserver'\n";
  print "debug:  \$options_salvager = '$options_salvager'\n";
}


#-----------------------------------------------------------------------------
# Prereq: Sanity check the forward and reverse name resolution.
#
if ($server eq 'localhost') {
  die "error: localhost is not a valid --server parameter. Use the ip hostname of this machine.\n";
}
my $packed_ip = gethostbyname($server);
unless (defined $packed_ip) {
  die "error: gethostbyname failed, $?\n";
}
my $ip_from_name = inet_ntoa($packed_ip);
print "debug: $server ip address is $ip_from_name\n" if $debug;
if ($ip_from_name=~/^127/) {
  die "error: Loopback address $ip_from_name cannot not be used for server $server. Check your /etc/hosts file.\n";
}

my $name_from_ip  = gethostbyaddr($packed_ip, AF_INET);
print "debug: hostname of $ip_from_name is $name_from_ip\n" if $debug;
if ($name_from_ip ne $server) {
  die "error: Name from ip $name_from_ip does not match ip from name $ip_from_name for --server $server. ".
      " Use the correct --server parameter and verify forward and reverse name resolution is working.\n";
}

#-----------------------------------------------------------------------------
# Prereq: The vice partition must be available and empty.
#
unless ($partition=~/^(([a-z])|([a-h][a-z])|([i][a-v]))$/) {
  die "error: Invalid partition id specified: $partition. Valid values are a..z and aa..iv\n";
}
unless ( -d "/vicep$partition" ) {
  die "error: Missing fileserver partition, /vicep$partition\n";
}
if ( -d "/vicep$partition/AFSIDat" ) {
  die "error: Fileserver partition is not empty. /vicep$partition/AFSIDat needs to be removed.\n";
}
open(LS, "ls /vicep$partition |") or 
  die "error: ls /vicep$partition failed, $!\n";
while (<LS>) {
  chomp;
  if (/^V\d+.vol$/) {
    die "error: Fileserver partition, /vicep$partition, is not empty.\n";
  }
}
close LS;

# Prereq: authorization and platform specific objects.
my $auth = OpenAFS::Auth::create(
      'debug'=>$debug,
      'type'=>$kerberos_type, 
      'cell'=>$cellname,
      'realm'=>$kerberos_realm,
      'keytab'=>$kerberos_keytab,
      'admin'=>$admin,
      );

my $os = OpenAFS::OS::create(
      'debug'=>$debug,
      );

#-----------------------------------------------------------------------------
# Prereq: Sanity check admin username and convert kerberos 5 notation to afs.
#
if ($admin =~ /@/) {
   die "error: Please specify the username without the realm name.\n";
}
my $username = $admin;
$username=~s:/:.:g;   # convert kerberos separators to afs separators.

#-----------------------------------------------------------------------------
# Prereq: Save the setup configuration in a form that is easily
# read by the shell scripts.
#
open(CONF, "> run-tests.conf") or die "error: Cannot open file run-tests.conf for writing: $!\n";
  print CONF <<"__CONF__";
CELLNAME=$cellname
PARTITION=$partition
ADMIN=$admin
KERBEROS_TYPE=$kerberos_type
KERBEROS_REALM=$kerberos_realm
KERBEROS_KEYTAB=$kerberos_keytab
__CONF__
close CONF;

unless ($batch) {
  my $answer = prompt("Last chance to cancel before setup begins. Continue? (yes/no)", "yes");
  exit(0) unless $answer=~/^y/i;
}

#-----------------------------------------------------------------------------
# Prereq: Shutdown the client and server, if running.
#
run($os->command('client-stop'));
run($os->command('fileserver-stop'));

#-----------------------------------------------------------------------------
# Prereq: Verify the server processes are not running.
#
foreach my $program ('bosserver', 'ptserver', 'vlserver', 'kaserver', 'fileserver') {
  die "error: program is already running, $program\n" if $os->number_running($program);
}

#-----------------------------------------------------------------------------
# Perform Platform-Specific Procedures
$os->configure_client();

#-----------------------------------------------------------------------------
# WORKAROUND:
# bosserver attempts to create the following directories with these limited 
# permissions. However, bosserver does not create parent directories as needed, so
# the directories are not successfully created when they are more than one level
# deep. 
run("mkdir -m 0775 -p $path->{'afsconfdir'}");
run("mkdir -m 0700 -p $path->{'afslocaldir'}");
run("mkdir -m 0700 -p $path->{'afsdbdir'}");
run("mkdir -m 0755 -p $path->{'afslogsdir'}"); 
run("mkdir -m 0777 -p $path->{'viceetcdir'}");

# In case the directories were created earlier with the wrong permissions.
run("chmod 0775 $path->{'afsconfdir'}");
run("chmod 0700 $path->{'afslocaldir'}");
run("chmod 0700 $path->{'afsdbdir'}");
run("chmod 0755 $path->{'afslogsdir'}"); 
run("chmod 0777 $path->{'viceetcdir'}");

#-----------------------------------------------------------------------------
# Starting the BOS Server
#
# Start the bosserver and create the initial server configuration.
# Authorization is disabled by the -noauth flag.
#
print "debug: Starting bosserver...\n" if $debug;
run("$path->{'afssrvsbindir'}/bosserver -noauth");
if ($unwind) {
    unwind($os->command('remove', "$path->{'afsconfdir'}/ThisCell"));
    unwind($os->command('remove', "$path->{'afsconfdir'}/CellServDB"));
    unwind($os->command('remove', "$path->{'afsconfdir'}/UserList"));
    unwind($os->command('remove', "$path->{'afsconfdir'}/KeyFile"));
    unwind($os->command('remove', "$path->{'afsbosconfigdir'}/BosConfig"));
    unwind($os->command('fileserver-stop'));
}
sleep(10); # allow bosserver some time to start accepting connections...

#-----------------------------------------------------------------------------
# Defining Cell Name and Membership for Server Processes
#
run("$bos setcellname $server $cellname -noauth");
run("$bos addhost $server $server -noauth");
run("$bos adduser $server $username -noauth");
if ($unwind) {
    unwind("$bos removeuser $server $username -noauth");
}

# WORKAROUND:
# The initial bosserver startup may create CellServDB entry which does
# not match the host name retured by gethostbyaddr(). This entry will
# cause ptserver/vlserver quorum errors and so is removed.
open(HOSTS, "$bos listhosts $server |") or 
  die "error: failed to run bos listhosts, $?\n";
my @hosts = <HOSTS>;
close HOSTS;
foreach (@hosts) {
  chomp;
  if (/^\s+Host \d+ is (.*)/) {
    my $host = $1;
    print "debug: bos listhosts: host=[$host]\n" if $debug; 
    if ($host ne $name_from_ip) {
      print "debug: removing invalid host '$host' from CellServDB.\n" if $debug;
      run("$bos removehost $server $host -noauth");
    }
  }
}

#-----------------------------------------------------------------------------
# Starting the Database Server Processes
#
print "debug: Starting the ptserver and vlserver...\n" if $debug;
run("$bos create $server ptserver simple -cmd \"$path->{'afssrvlibexecdir'}/ptserver $options_ptserver\" -noauth"); 
if ($unwind) {
    unwind($os->command('remove', "$path->{'afsdbdir'}/prdb.DB0"));
    unwind($os->command('remove', "$path->{'afsdbdir'}/prdb.DBSYS1"));
    unwind("$bos delete $server ptserver -noauth");
    unwind("$bos stop $server ptserver -noauth");
}

run("$bos create $server vlserver simple -cmd \"$path->{'afssrvlibexecdir'}/vlserver $options_vlserver\" -noauth");
if ($unwind) {
    unwind($os->command('remove', "$path->{'afsdbdir'}/vldb.DB0"));
    unwind($os->command('remove', "$path->{'afsdbdir'}/vldb.DBSYS1"));
    unwind("$bos delete $server vlserver -noauth");
    unwind("$bos stop $server vlserver -noauth");
}

if ($kerberos_type =~ /kaserver/i) {
  print "warning: kaserver is deprecated!\n";
  run("$bos create $server kaserver simple -cmd \"$path->{'afssrvlibexecdir'}/kaserver $options_vlserver\" -noauth");
  if ($unwind) {
    unwind($os->command('remove', "$path->{'afsdbdir'}/kaserver.DB0"));
    unwind($os->command('remove', "$path->{'afsdbdir'}/kaserver.DBSYS1"));
    unwind("$bos delete $server kaserver -noauth");
    unwind("$bos stop $server kaserver -noauth");
  }
}

sleep(10); # to allow the database servers to start servicing requests.

#-----------------------------------------------------------------------------
# Initializing Cell Security
#
# Create the AFS administrative account and the AFS server encryption key.
# Make the krb.conf file if the realm name is different than the cell name.

$auth->make_krb_config();
$auth->make_keyfile();
unless ( -f "$path->{'afsconfdir'}/KeyFile") {
  die "Failed to create $path->{'afsconfdir'}/KeyFile. Please create this using asetkey or the bos addkey command.\n";
}

print "debug: Creating admin user...\n" if $debug;
run("$pts createuser -name $username -cell $cellname -noauth");
run("$pts adduser $username system:administrators -cell $cellname -noauth");
run("$pts membership $username -cell $cellname -noauth");

print "debug: Restarting the database servers to use the new encryption key.\n" if $debug;
run("$bos restart $server -all -noauth");
sleep(10); # to allow the database servers to start servicing requests.

#-----------------------------------------------------------------------------
# Starting the File Server, Volume Server, and Salvager
#
print "debug: Starting the fileserver...\n" if $debug;
if ($with_dafs) {
  run( "$bos create $server dafs dafs ".
       "-cmd \"$path->{'afssrvlibexecdir'}/fileserver $options_fileserver\" ".
       "-cmd \"$path->{'afssrvlibexecdir'}/volserver $options_volserver\" ".
       "-cmd \"$path->{'afssrvlibexecdir'}/salvageserver $options_salvageserver\" ".
       "-cmd \"$path->{'afssrvlibexecdir'}/salvager $options_salvager\" ".
       "-noauth");
  if ($unwind) {
    unwind("$bos delete $server dafs -noauth");
    unwind("$bos stop $server dafs -noauth");
  }
}
else {
  run( "$bos create $server fs fs ".
       "-cmd \"$path->{'afssrvlibexecdir'}/fileserver $options_fileserver\" ".
       "-cmd \"$path->{'afssrvlibexecdir'}/volserver $options_volserver\" ".
       "-cmd \"$path->{'afssrvlibexecdir'}/salvager $options_salvager\" ".
       "-noauth");
  if ($unwind) {
    unwind("$bos delete $server fs -noauth");
    unwind("$bos stop $server fs -noauth");
  }
}

# Create the root.afs volume.
print "debug: Creating root.afs volume...\n" if $debug;
run("$vos create $server $partition root.afs -cell $cellname -noauth");
if ($unwind) {
    unwind($os->command('remove', "$partition/AFSIDat "));
    unwind($os->command('remove', "$partition/V*.vol"));
    unwind($os->command('remove', "$partition/Lock"));
    unwind("$vos remove $server $partition root.afs -cell $cellname -localauth");
}

#-----------------------------------------------------------------------------
# Installing Client Functionality
#
print "debug: Starting the OpenAFS client...\n" if $debug;
run($os->command('client-start'));
if ($unwind) {
    unwind($os->command('client-stop'));
}

# Run as the administrator.
$auth->authorize();

#-----------------------------------------------------------------------------
# Configuring the Top Levels of the AFS Filespace
#
print "debug: Creating the volumes...\n" if $debug;
run("$fs setacl /afs system:anyuser rl");

run("$vos create $server $partition root.cell");
if ($unwind) {
    unwind("$vos remove $server $partition root.cell -localauth");
}

run("$fs mkmount /afs/$cellname root.cell -cell $cellname -fast");
if ($unwind) {
    unwind("$fs rmmount /afs/$cellname");
}

run("$fs setacl /afs/$cellname system:anyuser rl");
run("$fs mkmount /afs/.$cellname root.cell -cell $cellname -rw");
if ($unwind) {
    unwind("$fs rmmount /afs/.$cellname");
}

run("$fs examine /afs");
run("$fs examine /afs/$cellname");

run("$vos addsite $server $partition root.afs");
run("$vos addsite $server $partition root.cell");
run("$vos release root.cell");
run("$vos release root.afs");

run("$fs checkvolumes"); # so client notices the releases
print "debug: the following should show root.afs.readonly\n" if $debug;
run("$fs examine /afs");
print "debug: the following should show root.cell.readonly\n" if $debug;
run("$fs examine /afs/$cellname");
print "debug: the following should show root.cell\n" if $debug;
run("$fs examine /afs/.$cellname");

# Create some volumes in our new cell.
print "debug: Creating the test volumes...\n" if $debug;
mkvol("user",    "/afs/.$cellname/user",         $server, $partition);
mkvol("service", "/afs/.$cellname/service",      $server, $partition);
mkvol("unrep",   "/afs/.$cellname/unreplicated", $server, $partition);
mkvol("rep",     "/afs/.$cellname/replicated",   $server, $partition);

run("$vos addsite $server $partition rep");
if ($unwind) {
    unwind("$vos remsite $server $partition rep");
}
run("$vos release rep");
run("$fs mkmount /afs/.$cellname/.replicated rep -rw");
run("$fs setacl  /afs/.$cellname/.replicated system:anyuser rl");

# Show the new volumes in the read-only path.
run("$vos release root.cell"); 

# done.
@unwinds = (); # clear unwinds
print "info: DONE\n";

END {
  if ($unwind && scalar @unwinds) {
    print "\ninfo: Error encountered, unwinding...\n"; 
    while (@unwinds) {
      eval { 
        run(pop(@unwinds));
      };
      if ($@) {
        print "warn: Unwind command failed.\n$@\n"; 
      }
    }
  }
}
