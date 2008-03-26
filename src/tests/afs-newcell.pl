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

=head1  NAME

   afs-newcell - Set up initial database server for AFS cell.

=head1 SYNOPSIS

B<afs-newcell>
       B<--batch>
       B<--debug>
       B<--dont-unwind>
       B<--help>
       B<--ostype>=os
       B<--server>=hostname
       B<--cellname>=cell
       B<--partition>=partition
       B<--admin>=administrator
       B<--kerberos-type>=authentication_type
       B<--kerberos-realm>=realm_name
       B<--kerberos-keytab>=keytab_file
       B<--skip-make-keyfile>
       B<--with-dafs>
       B<--options-fileserver>=options
       B<--options-volserver>=options
       B<--options-salvageserver>=options
	   B<--options-salvager>=options 

=head1 DESCRIPTION

This script sets up the initial AFS database and configures the first
database/file server. It also sets up an AFS cell's root volumes.  It assumes
that you already have a fileserver and database servers installed.  The
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
administravie user.  For example if jruser is an administrator then it would be
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
    run("$path->{'afssrvsbindir'}/vos create $srv $part $vol -maxquota 0 -localauth");
    unwind("$path->{'afssrvsbindir'}/vos remove $srv $part $vol -localauth");
    run("$path->{'afssrvbindir'}/fs mkm $mnt $vol ");
    run("$path->{'afssrvbindir'}/fs sa $mnt system:anyuser rl");
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
my $dont_unwind = 0;
my $help = 0;
my $ostype = $path->{'ostype'};
my $server = 'localhost';
my $cellname = 'testcell';
my $partition = '/vicepa';
my $admin = 'admin';
my $kerberos_type = 'MIT';
my $kerberos_realm = 'TESTCELL';
my $kerberos_keytab = "$path->{'afsconfdir'}/krb5.keytab";
my $skip_make_keyfile = 0;
my $with_dafs = 0;
my $options_fileserver = '';
my $options_volserver = '';
my $options_salvageserver = '';
my $options_salvager = '';

$server = `hostname`;
chomp $server;

GetOptions (
       "batch" => \$batch, 
       "debug!" => \$debug,
       "dont-unwind!" => \$dont_unwind,
       "help" => \$help,
       "ostype=s" => \$ostype,
       "server=s" => \$server,
       "cellname=s" => \$cellname, 
       "partition=s" => \$partition,
       "admin=s" => \$admin,
       "kerberos-type=s" => \$kerberos_type,
       "kerberos-realm=s" => \$kerberos_realm,
       "kerberos-keytab=s" => \$kerberos_keytab,
       "skip-make-keyfile" => \$skip_make_keyfile,
       "with-dafs" => \$with_dafs,
       "options-fileserver=s" => \$options_fileserver,
       "options-volserver=s" => \$options_volserver,
       "options-salvageserver=s" => \$options_salvageserver,
       "options-salvager=s" => \$options_salvager,
       );

if ($help) {
  pod2usage(1);
  exit 0;
}

# print debug messages when running commands.
$OpenAFS::ConfigUtils::debug = $debug;

#
# Verify we have a clean slate before starting.
#
my @problems = ();
my $try_rm_cell = 0;

if ($> != 0) {
  push(@problems, "This script should run as root.");
}

my @afsconfigfiles = (
  "$path->{'afsconfdir'}/ThisCell",
  "$path->{'afsconfdir'}/CellServDB",
  "$path->{'afsconfdir'}/UserList",
  "$path->{'afsdbdir'}/prdb.DB0",
  "$path->{'afsbosconfigdir'}/BosConfig",
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

#
# Interactive mode
#
unless ($batch) {

  print <<eoreqs;
                           REQUIREMENTS

The following requirements must be meet before running
this script. See 'pod2text $0' for more details.

1) A filesystem must be mounted on /vicepa. (See
   the --partition option for alternative mount points.)

2) The OpenAFS client and server binaries must be installed.
   The init scripts to start and stop the client and servers 
   must be installed and configured. OpenAFS/OS.pm must be 
   configured for your system. There should be no remants
   from a previous cell. Run afs-rmcell to remove any.

3) A Kerberos realm with Kerberos4 support must be available.
   Supported Kerberos implementations are Heimdal with
   Kth-kerberos compatibility, MIT Kerberos 5, and 
   Kaserver (deprecated). OpenAFS/Auth.pm must be configured
   for your system.

4) A Kerberos keytab file containing the afs principal 
   and the administrator principal must be be present at
   $path->{'afsconfdir'}/krb5.keytab.
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

  print "\nServer options:\n"; 
  $ostype = prompt("Which OS?", $ostype);
  $server = prompt("What server name should be used?", $server);
  $cellname = prompt("What cellname should be used?", $cellname);
  $partition = prompt("What vice partition?", $partition);
  $admin = prompt("What administrator username?", $admin);
  if($admin =~ /@/) {
    die "error: Please specify the username without the realm name.\n";
  }

  print "\nKerberos options:\n";
  $kerberos_type = prompt("Which Kerberos is to be used?", $kerberos_type);
  if ($kerberos_type!~/kaserver/i) {
    $kerberos_realm  = prompt("What Kerberos realm?", $kerberos_realm);
    $kerberos_keytab = prompt("What keytab file?", $kerberos_keytab);
    $answer = prompt("Create OpenAFS KeyFile from a keytab? (yes/no)", "yes");
    $skip_make_keyfile = ($answer=~/^y/i) ? 0 : 1;
  }

  print "\nFileserver options:\n";
  $answer = prompt("Use DAFS fileserver (requires DAFS build option)? (yes/no)", "no");
  $with_dafs = ($answer=~/^y/i) ? 1 : 0;
  $options_fileserver = prompt("fileserver options:", $options_fileserver);
  $options_volserver = prompt("volserver options:",  $options_volserver);
  $options_salvageserver = prompt("salvageserver options:",  $options_salvageserver);
  $options_salvager = prompt("salvager options:", $options_salvager);

  print "\nConfirmation:\n";
  print "OS Type                : $ostype\n";
  print "Server name            : $server\n";
  print "Cell name              : $cellname\n";
  print "Partition              : $partition\n";
  print "Administrator          : $admin\n";
  print "Kerberos               : $kerberos_type\n";
  if ($kerberos_type!~/kaserver/i) {
    print "Realm                  : $kerberos_realm\n";
    print "Keytab file            : $kerberos_keytab\n";
    print "Make KeyFile           : ", $skip_make_keyfile ? "yes" : "no", "\n";
  }
  print "DAFS fileserver        : ", $with_dafs ? "yes" : "no", "\n";
  print "fileserver options     : $options_fileserver\n";
  print "volserver options      : $options_volserver\n";
  print "salvagerserver options : $options_salvageserver\n";
  print "salvager options       : $options_salvager\n";
  print "\n";

  $answer = prompt("Continue? (yes/no)", "yes");
  unless ($answer=~/^y/i ) {
    print "OK: Aborted.\n";
  exit 0;
  }

  # Save the options for the next time.
  $answer = prompt("Save as command-line options? (yes/no)", "yes");
  if ($answer=~/^y/i ) {
    my $switches = "";
    $switches .= "--batch";
    $switches .= " --debug"                                          if $debug;
    $switches .= " --dont_unwind"                                    if $dont_unwind;
    $switches .= " --ostype='$ostype'"                               if $ostype;
    $switches .= " --server='$server'"                               if $server;
    $switches .= " --cellname='$cellname'"                           if $cellname;
    $switches .= " --partition='$partition'"                         if $partition;
    $switches .= " --admin='$admin'"                                 if $admin;
    $switches .= " --kerberos-type='$kerberos_type'"                 if $kerberos_type;
    $switches .= " --kerberos-realm='$kerberos_realm'"               if $kerberos_realm;
    $switches .= " --kerberos-keytab='$kerberos_keytab'"             if $kerberos_keytab;
    $switches .= " --skip-make-keyfile"                              if $skip_make_keyfile;
    $switches .= " --with-dafs"                                      if $with_dafs;
    $switches .= " --options-fileserver='$options_fileserver'"       if $options_fileserver;
    $switches .= " --options-volserver='$options_volserver'"         if $options_volserver;;
    $switches .= " --options-salvageserver='$options_salvageserver'" if $options_salvageserver;;
    $switches .= " --options-salvager='$options_salvager'"           if $options_salvager;
  
    my $conf = prompt("Filename for save?", "afs-newcell.conf");
    open(CONF, "> $conf") or die "error: Cannot open file $conf: $!\n";
    print CONF "$switches\n";
    close CONF;
  }
}

if ($debug) {
  print "debug: afs-newcell options\n";
  print "debug:  \$batch = '$batch'\n";
  print "debug:  \$debug = '$debug'\n";
  print "debug:  \$dont_unwind = '$dont_unwind'\n";
  print "debug:  \$help = '$help'\n";
  print "debug:  \$ostype = '$ostype'\n";
  print "debug:  \$server = '$server'\n";
  print "debug:  \$cellname = '$cellname'\n";
  print "debug:  \$partition = '$partition'\n";
  print "debug:  \$admin = '$admin'\n";
  print "debug:  \$kerberos_type = '$kerberos_type'\n";
  print "debug:  \$kerberos_realm = '$kerberos_realm'\n";
  print "debug:  \$kerberos_keytab = '$kerberos_keytab'\n";
  print "debug:  \$skip_make_keyfile = '$skip_make_keyfile'\n";
  print "debug:  \$with_dafs = '$with_dafs'\n";
  print "debug:  \$options_fileserver = '$options_fileserver'\n";
  print "debug:  \$options_volserver = '$options_volserver'\n";
  print "debug:  \$options_salvageserver = '$options_salvageserver'\n";
  print "debug:  \$options_salvager = '$options_salvager'\n";
}

# 
# Create an auth object for the type of kerberos
# to be used for authentication in our cell.
#
my $auth = OpenAFS::Auth::create(
      'debug'=>$debug,
      'type'=>$kerberos_type, 
      'cell'=>$cellname,
      'realm'=>$kerberos_realm,
      'keytab'=>$kerberos_keytab,
      );

my $os = OpenAFS::OS::create(
      'debug'=>$debug,
      'ostype'=>$ostype,
      );

#
# Sanity checks before we begin. Make sure we have correct
# binaries, directories, and permissions.
#

my $bosserver = "$path->{'afssrvsbindir'}/bosserver";
my $bos       = "$path->{'afssrvbindir'}/bos";
my $fs        = "$path->{'afssrvbindir'}/fs";
my $pts       = "$path->{'afssrvbindir'}/pts";
my $vos       = "$path->{'afssrvsbindir'}/vos";

check_program($bosserver);
check_program($bos);
check_program($fs);
check_program($pts);
check_program($vos);


#
# Sanity check admin username and convert kerberos 5 notation to afs.
#
if ($admin =~ /@/) {
   die "error: Please specify the username without the realm name.\n";
}
my $username = $admin;
$username=~s:/:.:g;   # convert kerberos separators to afs separators.

# Shutdown the client and server, if running.
run($os->command('client-stop'));
run($os->command('fileserver-stop'));

#
# Attempt the client setup for this system before we try to create the cell.
#
$os->configure_client();

#
# Create the initial server configuration and the server administrator, temporarily running
# with -noauth.
#

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

print "debug: Starting bosserver...\n" if $debug;
run("$path->{'afssrvsbindir'}/bosserver -noauth");
    unwind($os->command('remove', "$path->{'afsconfdir'}/ThisCell"));
    unwind($os->command('remove', "$path->{'afsconfdir'}/CellServDB"));
    unwind($os->command('remove', "$path->{'afsconfdir'}/UserList"));
    unwind($os->command('remove', "$path->{'afsbosconfigdir'}/BosConfig"));
    unwind($os->command('fileserver-stop'));

run("$bos setcellname $server $cellname -localauth");
run("$bos addhost $server $server -localauth");
run("$bos adduser $server $username -localauth");
    unwind("$bos removeuser $server $username -localauth");

#
# Create the AFS KeyFile. (This must be done after bosserver creates the configuration files.)
#
unless ($skip_make_keyfile) {
  print "debug: Making the keyfile...\n" if $debug;
  $auth->make_keyfile();
}
unless ( -f "$path->{'afsconfdir'}/KeyFile") {
  die "You do not have an AFS keyfile.  Please create this using asetkey or the bos addkey command.\n";
}

# make the krb.conf file if the realm name is different than the cell name.
$auth->make_krb_config();

#
# Start up the ptserver and vlserver.
#
print "debug: Starting the ptserver and vlserver...\n" if $debug;
run("$bos create $server ptserver simple $path->{'afssrvlibexecdir'}/ptserver -localauth");
    unwind($os->command('remove', "$path->{'afsdbdir'}/prdb.DB0"));
    unwind($os->command('remove', "$path->{'afsdbdir'}/prdb.DBSYS1"));
    unwind("$bos delete $server ptserver -localauth");
    unwind("$bos stop $server ptserver -localauth");

run("$path->{'afssrvbindir'}/bos create $server vlserver simple $path->{'afssrvlibexecdir'}/vlserver -localauth");
    unwind($os->command('remove', "$path->{'afsdbdir'}/vldb.DB0"));
    unwind($os->command('remove', "$path->{'afsdbdir'}/vldb.DBSYS1"));
    unwind("$bos delete $server vlserver -localauth");
    unwind("$bos stop $server vlserver -localauth");

#
# Start the file server.
#
print "debug: Starting the fileserver...\n" if $debug;
if ($with_dafs) {
  run( "$bos create $server dafs dafs ".
       "-cmd $path->{'afssrvlibexecdir'}/fileserver $options_fileserver ".
       "-cmd $path->{'afssrvlibexecdir'}/volserver $options_volserver ".
       "-cmd $path->{'afssrvlibexecdir'}/salvageserver $options_salvageserver".
       "-cmd $path->{'afssrvlibexecdir'}/salvager $options_salvager".
     "-localauth");
}
else {
  run( "$bos create $server fs fs ".
       "-cmd $path->{'afssrvlibexecdir'}/fileserver $options_fileserver ".
       "-cmd $path->{'afssrvlibexecdir'}/volserver $options_volserver ".
       "-cmd $path->{'afssrvlibexecdir'}/salvager $options_salvager ".
     "-localauth");
}
  unwind("$bos delete $server fs -localauth ");
  unwind("$bos stop $server fs -localauth ");

#
# Create the AFS administrator (with the same name as the server administrator).
#
print "debug: Creating users...\n" if $debug;
sleep(10); # wait to avoid "no quorum elected" errors.

run("$pts createuser -name $username -cell $cellname -noauth");
run("$pts adduser $username system:administrators -cell $cellname -noauth");
run("$pts membership $username -cell $cellname -noauth");

#
# Create the root afs volume.
#
print "debug: Creating root.afs volume...\n" if $debug;
run("$vos create $server $partition root.afs -cell $cellname -noauth");
    unwind($os->command('remove', "$partition/AFSIDat "));
    unwind($os->command('remove', "$partition/V*.vol"));
    unwind($os->command('remove', "$partition/Lock"));
    unwind("$vos remove $server $partition root.afs -cell $cellname -noauth");

# The initial configuration is done, turn on authorization checking.
#run("$bos setauth $server -authrequired on -cell $cellname -localauth");
#    unwind("$bos setauth $server -authrequired off -cell $cellname -localauth");


#
# Bring up the AFS client.
#
print "debug: Starting the OpenAFS client...\n" if $debug;
run($os->command('client-start'));
    unwind($os->command('client-stop'));

#
# Run as the administrator.
#
$auth->authorize($admin);

#
# Create the root cell volumes, read-only and read-write.
#
print "debug: Creating the root volumes...\n" if $debug;
run("$fs setacl /afs system:anyuser rl");

run("$vos create $server $partition root.cell -localauth");
    unwind("$vos remove $server $partition root.cell -localauth");

run("$fs mkmount /afs/$cellname root.cell -cell $cellname -fast");
    unwind("$fs rmmount /afs/$cellname");

run("$fs setacl /afs/$cellname system:anyuser rl");
run("$fs mkmount /afs/.$cellname root.cell -cell $cellname -rw");
    unwind("$fs rmmount /afs/.$cellname");

#run("$fs mkmount /afs/.root.afs root.afs -rw");
#    unwind("$fs rmmmount /afs/.root.afs");

#
# Create some volumes in our new cell.
#
print "debug: Creating the test volumes...\n" if $debug;
mkvol("user", "/afs/$cellname/user", $server, $partition);
mkvol("service", "/afs/$cellname/service", $server, $partition);
mkvol("unrep", "/afs/$cellname/unreplicated", $server, $partition);

# make a read-only volume
mkvol("rep", "/afs/$cellname/.replicated", $server, $partition);
run("$fs mkmount /afs/$cellname/replicated rep.readonly");
run("$vos addsite $server $partition rep -localauth");
run("$vos release rep -localauth");
    unwind("$vos remove $server $partition rep.readonly -localauth");


#
# Create readonly volumes of our roots.
#
run("$vos addsite $server $partition root.afs -localauth");
run("$vos addsite $server $partition root.cell -localauth");
run("$vos release root.afs -localauth");
run("$vos release root.cell -localauth");
    unwind("$vos remove $server $partition root.cell.readonly -localauth");
    unwind("$vos remove $server $partition root.afs.readonly -localauth");

# done.
@unwinds = (); # clear unwinds

END {
  if (!$dont_unwind && scalar @unwinds) {
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
