#!/usr/bin/env perl -w
# Copyright (C) 2000 by Sam Hartman
# This file may be copied either under the terms of the GNU GPL or the IBM Public License
# either version 2 or later of the GPL or version 1.0 or later of the IPL.

use Term::ReadLine;
use strict;
use OpenAFS::ConfigUtils;
use OpenAFS::Dirpath;
use OpenAFS::OS;
use OpenAFS::Auth;
use Getopt::Long;
use vars qw($admin $server $cellname $cachesize $part
          $requirements_met  $shutdown_needed $csdb);
my $rl = new Term::ReadLine('afs-newcell');

=head1  NAME

   afs-newcell - Set up initial database server for AFS cell.

=head1 SYNOPSIS

B<afs-newcell> [B<--requirements-met>] [B<--admin> admin_user] [B<--cellname> cellname] [B<--cachesize> size] [B<--partition> partition-letter]

=head1 DESCRIPTION


This script sets up the initial AFS database and configures the first
database/file server. It also sets up an AFS cell's root volumes.  It 
assumes that you already have a fileserver and database servers.  The 
fileserver should have an empty root.afs. This script creates root.cell, 
user, service and populates root.afs.  

The B<requirements-met> option specifies that the initial requirements
have been met and that the script can proceed without displaying the
initial banner or asking for confirmation.

The B<admin> option specifies the name of the administrative user.
This user will be given system:administrators and susers permission in
the cell.

The B<cellname> option specifies the name of the cell.

The B<cachesize> option specifies the size of the AFS cache.

=head1 AUTHOR

Sam Hartman <hartmans@debian.org>

=cut

# main script

# mkvol(volume, mount)
sub mkvol($$) {
    my ($vol, $mnt) = @_;
    run("$openafsdirpath->{'afssrvsbindir'}/vos create $server $part $vol -localauth");
    unwind("$openafsdirpath->{'afssrvsbindir'}/vos remove $server $part $vol -localauth");
    run("$openafsdirpath->{'afssrvbindir'}/fs mkm $mnt $vol ");
    run("$openafsdirpath->{'afssrvbindir'}/fs sa $mnt system:anyuser rl");
}

GetOptions (
           "requirements-met" => \$requirements_met, 
           "cellname=s" => \$cellname, 
	   "cachesize=s" => \$cachesize,
           "partition=s" => \$part,
           "admin=s" => \$admin);

unless ($requirements_met) {
  print <<eoreqs;
                           Prerequisites

In order to set up a new AFS cell, you must meet the following:

1) You need a working Kerberos realm with Kerberos4 support.  You
   should install Heimdal with Kth-kerberos compatibility or MIT
   Kerberos5.

2) You need to create the single-DES AFS key and load it into
   $openafsdirpath->{'afsconfdir'}/KeyFile.  If your cell's name is the same as
   your Kerberos realm then create a principal called afs.  Otherwise,
   create a principal called afs/cellname in your realm.  The cell
   name should be all lower case, unlike Kerberos realms which are all
   upper case.  You can use asetkey from the openafs-krb5 package, or
   if you used AFS3 salt to create the key, the bos addkey command.

3) This machine should have a filesystem mounted on /vicepa.  If you
   do not have a free partition, on Linux you can create a large file by using
   dd to extract bytes from /dev/zero.  Create a filesystem on this file
   and mount it using -oloop.  

4) You will need an administrative principal created in a Kerberos
   realm.  This principal will be added to susers and
   system:administrators and thus will be able to run administrative
   commands.  Generally the user is a root instance of some administravie
   user.  For example if jruser is an administrator then it would be
   reasonable to create jruser/root and specify jruser/root as the user
   to be added in this script.

5) The AFS client must not be running on this workstation.  It will be
   at the end of this script.

eoreqs

  $_ = $rl->readline("Do you meet these requirements? [y/n] ");
  unless (/^y/i ) {
    print "Run this script again when you meet the requirements\n";
    exit(1);
  }
       
  if ($> != 0) {
    die "This script should almost always be run as root.  Use the --requirements-met option to run as non-root.\n";
  }
}
open(MOUNT, "mount |") or die "Failed to run mount: $!\n";
while(<MOUNT>) {
  if(m:^AFS:) {
    print "The AFS client is currently running on this workstation.\n";
    print "Please restart this script after running $openafsinitcmd->{'client-stop'}\n";
    exit(1);
  }
  if(m:^/afs on AFS:) {
    print "The AFS client is currently running on this workstation.\n";
    print "Please restart this script after running $openafsinitcmd->{'client-stop'}\n";
    exit(1);
  }
}
close MOUNT;

unless ( -f "$openafsdirpath->{'afsconfdir'}/KeyFile") {
  print "You do not have an AFS keyfile.  Please create this using asetkey from openafs-krb5 or 
the bos addkey command";
  exit(1);
}

print "If the fileserver is not running, this may hang for 30 seconds.\n";
run("$openafsinitcmd->{'filesrv-stop'}");
$server = `hostname`;
chomp $server;
$admin = $rl->readline("What administrative principal should be used? ") unless $admin;
 die "Please specify an administrative user\n" unless $admin;
$admin =~ s:/:.:g;
if($admin =~ /@/) {
die "The administrative user must be in the same realm as the cell and no realm may be specified.\n";
}

$cellname = $rl->readline("What cellname should be used? ") unless $cellname;
die "Please specify a cellname\n" unless $cellname;

if (! -f "$openafsdirpath->{'afsconfdir'}/ThisCell") {
    open(CELL, "> $openafsdirpath->{'afsconfdir'}/ThisCell");
    print CELL "${cellname}";
    close CELL;
}

open(CELL, "$openafsdirpath->{'afsconfdir'}/ThisCell") or
    die "Cannot open $openafsdirpath->{'afsconfdir'}/ThisCell: $!\n";

my $lcell = <CELL>;
chomp $lcell;
close CELL;

run( "echo \\>$lcell >$openafsdirpath->{'afsconfdir'}/CellServDB");
$csdb = `host $server|awk '{print $4 " #" $1}'`;
run( "echo $csdb >>$openafsdirpath->{'afsconfdir'}/CellServDB");
run("$openafsinitcmd->{'filesrv-start'}");
unwind("$openafsinitcmd->{'filesrv-stop'}");
$shutdown_needed = 1;
run ("$openafsdirpath->{'afssrvbindir'}/bos addhost $server $server -localauth ||true");
run("$openafsdirpath->{'afssrvbindir'}/bos adduser $server $admin -localauth");
unwind("$openafsdirpath->{'afssrvbindir'}/bos removeuser $server $admin -localauth");
if ( -f "$openafsdirpath->{'afsdbdir'}/prdb.DB0" ) {
  die "Protection database already exists; cell already partially created\n";
 }
open(PRDB, "|$openafsdirpath->{'afssrvsbindir'}/pt_util -p $openafsdirpath->{'afsdbdir'}/prdb.DB0 -w ")
or die "Unable to start pt_util: $!\n";
print PRDB "$admin 128/20 1 -204 -204\n";
print PRDB "system:administrators 130/20 -204 -204 -204\n";
print PRDB" $admin 1\n";
close PRDB;
unwind( "rm $openafsdirpath->{'afsdbdir'}/prdb* ");
# Start up ptserver and vlserver
run("$openafsdirpath->{'afssrvbindir'}/bos create $server ptserver simple $openafsdirpath->{'afssrvlibexecdir'}/ptserver -localauth");
unwind("$openafsdirpath->{'afssrvbindir'}/bos delete $server ptserver -localauth");

run("$openafsdirpath->{'afssrvbindir'}/bos create $server vlserver simple $openafsdirpath->{'afssrvlibexecdir'}/vlserver -localauth");
unwind("$openafsdirpath->{'afssrvbindir'}/bos delete $server vlserver -localauth");

run( "$openafsdirpath->{'afssrvbindir'}/bos create $server fs fs ".
     "-cmd $openafsdirpath->{'afssrvlibexecdir'}/fileserver ".
     "-cmd $openafsdirpath->{'afssrvlibexecdir'}/volserver ".
     "-cmd $openafsdirpath->{'afssrvlibexecdir'}/salvager -localauth");
unwind( "$openafsdirpath->{'afssrvbindir'}/bos delete $server fs -localauth ");

print "Waiting for database elections: ";
sleep(30);
print "done.\n";
# Past this point we want to control when bos shutdown happens
$shutdown_needed = 0;
unwind( "$openafsdirpath->{'afssrvbindir'}/bos shutdown $server -localauth ");
run("$openafsdirpath->{'afssrvsbindir'}/vos create $server a root.afs -localauth");
# bring up client

$cachesize = $rl->readline("What size cache (in 1k blocks)? ") unless $cachesize;
die "Please specify a cache size\n" unless $cachesize;

run("echo $lcell >$openafsdirpath->{'viceetcdir'}/ThisCell");
run("cp $openafsdirpath->{'afsconfdir'}/CellServDB $openafsdirpath->{'viceetcdir'}/CellServDB");
run("echo /afs:/usr/vice/cache:${cachesize} >$openafsdirpath->{'viceetcdir'}/cacheinfo");
run("$openafsinitcmd->{'client-forcestart'}");
my $afs_running = 0;
open(MOUNT, "mount |") or die "Failed to run mount: $!\n";
while(<MOUNT>) {
if(m:^AFS:) {
       $afs_running = 1;
}
       }
unless ($afs_running) {
print "*** The AFS client failed to start.\n";
print  "Please fix whatever problem kept it from running.\n";
       exit(1);
}
unwind("$openafsinitcmd->{'client-stop'}");

unless ($part) {
    $part = $rl    ->readline("What partition? [a] ");
    $part = "a" unless $part;
}

&OpenAFS::Auth::authadmin();

run("$openafsdirpath->{'afssrvbindir'}/fs sa /afs system:anyuser rl");

run("$openafsdirpath->{'afssrvsbindir'}/vos create $server $part root.cell -localauth");
unwind("$openafsdirpath->{'afssrvsbindir'}/vos remove $server $part root.cell -localauth");
# We make root.cell s:anyuser readable after we mount in the next
# loop.
open(CELLSERVDB, "$openafsdirpath->{'viceetcdir'}/CellServDB")
    or die "Unable to open $openafsdirpath->{'viceetcdir'}/CellServDB: $!\n";
while(<CELLSERVDB>) {
    chomp;
    if (/^>\s*([a-z0-9_\-.]+)/ ) {
       run("$openafsdirpath->{'afssrvbindir'}/fs mkm /afs/$1 root.cell -cell $1 -fast");
       unwind ("$openafsdirpath->{'afssrvbindir'}/fs rmm /afs/$1");
   }
}

run("$openafsdirpath->{'afssrvbindir'}/fs sa /afs/$lcell system:anyuser rl");
run ("$openafsdirpath->{'afssrvbindir'}/fs mkm /afs/.$lcell root.cell -cell $lcell -rw");
unwind ("$openafsdirpath->{'afssrvbindir'}/fs rmm /afs/.$lcell");
run("$openafsdirpath->{'afssrvbindir'}/fs mkm /afs/.root.afs root.afs -rw");
unwind ("$openafsdirpath->{'afssrvbindir'}/fs rmm /afs/.root.afs");

mkvol( "user", "/afs/$lcell/user" );
unwind( "$openafsdirpath->{'afssrvsbindir'}/vos remove $server $part user -localauth ");

mkvol( "service", "/afs/$lcell/service" );
unwind( "$openafsdirpath->{'afssrvsbindir'}/vos remove $server $part service -localauth ");

mkvol( "rep", "/afs/$lcell/.replicated" );
unwind( "$openafsdirpath->{'afssrvsbindir'}/vos remove $server $part rep -localauth ");
run( "$openafsdirpath->{'afssrvbindir'}/fs mkm /afs/$lcell/replicated rep.readonly " );

run( "$openafsdirpath->{'afssrvsbindir'}/vos addsite $server $part rep -localauth" );
run( "$openafsdirpath->{'afssrvsbindir'}/vos release rep -localauth" );
unwind( "$openafsdirpath->{'afssrvsbindir'}/vos remove $server $part rep.readonly -localauth ");

mkvol( "unrep", "/afs/$lcell/unreplicated" );
unwind( "$openafsdirpath->{'afssrvsbindir'}/vos remove $server $part unrep -localauth ");

$lcell =~ /^([^.]*)/;
my $cellpart = $1;
run("ln -s /afs/$lcell /afs/$cellpart");
unwind ("rm /afs/$cellpart");
run( "ln -s /afs/.$lcell /afs/.$cellpart" );
unwind ("rm /afs/.$cellpart");

run( "$openafsdirpath->{'afssrvsbindir'}/vos addsite $server $part root.afs -localauth" );
run( "$openafsdirpath->{'afssrvsbindir'}/vos addsite $server $part root.cell -localauth" );
run( "$openafsdirpath->{'afssrvsbindir'}/vos release root.afs -localauth" );
run( "$openafsdirpath->{'afssrvsbindir'}/vos release root.cell -localauth" );
unwind( "$openafsdirpath->{'afssrvsbindir'}/vos remove $server $part root.cell.readonly -localauth ");
unwind( "$openafsdirpath->{'afssrvsbindir'}/vos remove $server $part root.afs.readonly -localauth ");



@unwinds = ();
END {
# If we fail before all the instances are created, we need to perform 
# our own bos shutdown
    system("$openafsdirpath->{'afssrvbindir'}/bos shutdown $server -localauth") if $shutdown_needed;
  run(pop @unwinds) while @unwinds;
  }
