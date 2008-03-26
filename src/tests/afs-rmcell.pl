#!/usr/bin/env perl
#
# Remove cell files from this machine. Use with caution!
#

use warnings;
use strict;
use OpenAFS::Dirpath;
use OpenAFS::OS;
use OpenAFS::ConfigUtils;
use Term::ReadLine;
use Getopt::Long;
use Pod::Usage;

=head1  NAME

   afs-rmcell - Delete AFS cell files from this machine.

=head1 SYNOPSIS

B<afs-rmcell> [B<--batch>] [B<--partition-id>=letter] [B<--help>] [B<--debug>]

=head1 DESCRIPTION

This script destroys the AFS database and volume files on this machine.
Use with caution!

=cut

my $debug = 0;
my $help = 0;
my $batch = 0;
my $partition_id = 'a';
my $path = $OpenAFS::Dirpath::openafsdirpath;
my $ostype = $path->{'ostype'};

#-----------------------------------------------------------------------------------
# main script

GetOptions(
  "debug" => \$debug,
  "help" => \$help,
  "batch" => \$batch,
  "partition-id=s" => \$partition_id,
  "ostype=s" => \$ostype,
);

$OpenAFS::ConfigUtils::debug = $debug;

if ($help) {
  pod2usage(1);
  exit 0;
}

if ($> != 0) {
  die "error: This script should run as root.\n";
}

# To be on the safe side, we do no accept the full partition name, just the letter id.
# You'll have to manually delete volume files for unconventional partition names.
unless ($partition_id=~/^(([a-z])|([a-h][a-z])|([i][a-v]))$/) {
  die "error: Invalid partition id specified.\n".
      "info: Please specify a valid partition abbreviation, for example --partition-id='a' for /vicepa\n";
}

unless ($batch) {
  my $rl = new Term::ReadLine('afs-rmcell');
  print "\n*** WARNING!! WARNING!! WARNING!! *** \n";
  print "You are about to permanently DESTROY the OpenAFS configuration, database, and volumes on this machine!\n\n";
  my $answer = $rl->readline("Do you really want to destroy the AFS cell data? (y/n) [n] ");
  unless ($answer=~/^y/i ) {
    print "info: Aborted.\n";
    exit 0;
  }
}

my $os = OpenAFS::OS::create('ostype'=>$ostype, 'debug'=>$debug);

# make sure the client init script has the correct paths.
$os->configure_client(); 

run($os->command('client-stop'));
run($os->command('fileserver-stop'));
run($os->command('client-forcestop'));

$os->remove("$path->{'afsdbdir'}/prdb.DB0");
$os->remove("$path->{'afsdbdir'}/prdb.DBSYS1");
$os->remove("$path->{'afsdbdir'}/vldb.DB0");
$os->remove("$path->{'afsdbdir'}/vldb.DBSYS1");
$os->remove("$path->{'afsbosconfigdir'}/BosConfig");
$os->remove("$path->{'afslogsdir'}/*");
$os->remove("$path->{'afslocaldir'}/*");
$os->remove("$path->{'afsconfdir'}/UserList");
$os->remove("$path->{'afsconfdir'}/ThisCell");
$os->remove("$path->{'afsconfdir'}/CellServDB");
$os->remove("$path->{'afsconfdir'}/KeyFile");
$os->remove("$path->{'afsconfdir'}/krb.conf");
$os->remove("/vicep$partition_id/AFSIDat ");
$os->remove("/vicep$partition_id/V*.vol");
$os->remove("/vicep$partition_id/Lock");

