#!/usr/bin/perl
#
# A perl script that checks to ensure the udebug output for a vlserver
# claims that a quorum has been elected.
#
# openafs-tools, Version 1.2.2

# Copyright 2002, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html
#

$serverName = $ARGV[0];

$afscodeFileDir = "/usr/afs/tools/install/";
$udebugOutput = $afscodeFileDir . "udebug.out.$$";

$foundQuorum = 0;
$recovery = 0;

while( !($foundQuorum and $recovery) ) {

    system( "/usr/afs/bin/udebug $serverName vlserver &> $udebugOutput" ) == 0
	or (system( "rm -f $udebugOutput" ) == 0 
	    and die "check_udebug: the call to udebug (for server $serverName) failed or was killed\n");

    open( UDEBUG, "<$udebugOutput");
    @udebug = <UDEBUG>;
    close(UDEBUG);

    $newServerName = $serverName;

    foreach $line (@udebug) {

	# check the udebug output.  if this is the sync site, we've 
	# found our quorum.  otherwise, if a last yes has been cast
	# we'll check if that site is the sync site.  otherwise, if
	# the last yes vote has not been cast, we'll keep on 
	# checking this site until it is.
	if( $line =~ m/^I am sync site(.*)/ ) {
	    $foundQuorum = 1;
	} elsif( $line =~ m/^Last yes vote for ([^\s]*) .*/ ) {
	    $newServerName = $1;
        } elsif( $line =~ m/^Recovery state (.*)\n$/ ) {
	    if( $1 != 0 ) {
		$recovery = 1;
	    }
	}
	
    }

    # if this isn't the sync site, try somewhere else.
    if( !$foundQuorum ) {
	$serverName = $newServerName;
    }

}

system( "rm -f $udebugOutput" );

#return once we've found the sync site.
