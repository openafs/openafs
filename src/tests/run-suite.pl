#!/usr/bin/env perl
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
          $shutdown_needed $csdb);
my $rl = new Term::ReadLine('run-suite');

=head1  NAME

   run-suite - Set up AFS cell and test.

=head1 SYNOPSIS

B<run-suite> [B<--cellname> cellname] [B<--cachesize> size]

=head1 DESCRIPTION


This script sets up an AFS cell, then runs a suite of tests against the 
cell to verify the build.

The B<cellname> option specifies the name of the cell.

The B<cachesize> option specifies the size of the AFS cache.

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
           "cellname=s" => \$cellname, 
	   "cachesize=s" => \$cachesize,
           "partition=s" => \$part,
           "admin=s" => \$admin);

if ($> != 0) {
  die "This script should almost always be run as root.\n";
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
$admin = "admin" unless $admin;
$admin =~ s:/:.:g;
if($admin =~ /@/) {
die "The administrative user must be in the same realm as the cell and no realm may be specified.\n";
}

$cellname = $rl->readline("What cellname should be used? ") unless $cellname;
die "Please specify a cellname\n" unless $cellname;

unlink "$openafsdirpath->{'viceetcdir'}/CellServDB";
unlink "$openafsdirpath->{'viceetcdir'}/ThisCell";

my $lcell = "${cellname}";

#let bosserver create symlinks
run("$openafsinitcmd->{'filesrv-start'}");
unwind("$openafsinitcmd->{'filesrv-stop'}");
$shutdown_needed = 1;
run ("$openafsdirpath->{'afssrvbindir'}/bos setcellname $server $lcell -localauth ||true");
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
unwind("$openafsdirpath->{'afssrvbindir'}/bos stop $server ptserver -localauth -wait");

run("$openafsdirpath->{'afssrvbindir'}/bos create $server vlserver simple $openafsdirpath->{'afssrvlibexecdir'}/vlserver -localauth");
unwind("$openafsdirpath->{'afssrvbindir'}/bos delete $server vlserver -localauth");
unwind("$openafsdirpath->{'afssrvbindir'}/bos stop $server vlserver -localauth -wait");

run( "$openafsdirpath->{'afssrvbindir'}/bos create $server fs fs ".
     "-cmd $openafsdirpath->{'afssrvlibexecdir'}/fileserver ".
     "-cmd $openafsdirpath->{'afssrvlibexecdir'}/volserver ".
     "-cmd $openafsdirpath->{'afssrvlibexecdir'}/salvager -localauth");
unwind( "$openafsdirpath->{'afssrvbindir'}/bos delete $server fs -localauth ");
unwind( "$openafsdirpath->{'afssrvbindir'}/bos stop $server fs -localauth -wait");

print "Waiting for database elections: ";
sleep(60);
print "done.\n";
# Past this point we want to control when bos shutdown happens
$shutdown_needed = 0;
unwind( "$openafsdirpath->{'afssrvbindir'}/bos shutdown $server -localauth ");
run("$openafsdirpath->{'afssrvsbindir'}/vos create $server a root.afs -localauth");
# bring up client

$cachesize = $rl->readline("What size cache (in 1k blocks)? ") unless $cachesize;
die "Please specify a cache size\n" unless $cachesize;

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

$part = "a" unless $part;

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

run("$openafsinitcmd->{'client-restart'}");

system ("pagsh -c './test-front.sh $lcell'");

@unwinds = ();
END {
# If we fail before all the instances are created, we need to perform 
# our own bos shutdown
    system("$openafsdirpath->{'afssrvbindir'}/bos shutdown $server -localauth") if $shutdown_needed;
  run(pop @unwinds) while @unwinds;
  }

