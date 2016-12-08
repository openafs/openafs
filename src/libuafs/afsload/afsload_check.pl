#!/usr/bin/perl -w

use strict;

use AFS::Load::Config;

sub usage() {
	print STDERR "Usage: $0 -p <NP> <testconfig.conf>";
	exit(1);
}

if ($#ARGV < 2) {
	usage();
}

my $flag = $ARGV[0];
my $np = $ARGV[1];
my $conf_file = $ARGV[2];

if ($flag ne "-p") {
	usage();
}
if (!($np =~ m/^\d+$/)) {
	usage();
}

print "# Checking if config $conf_file is valid for $np processes...\n";

AFS::Load::Config::check_conf($np, $conf_file);

print "# Config file $conf_file has no fatal errors\n";
