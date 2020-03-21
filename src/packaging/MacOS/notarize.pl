#!/usr/bin/perl
#
# Copyright (c) 2019 Sine Nomine Associates
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
# IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Description:
#
# Script used to automatically notarize the OpenAFS package.
#
# On success, the following output can be expected:
#
# $ sudo notarize.pl foo@bar.com "@keychain:PASSWORD" OpenAFS.dmg
#
# notarize.pl: submitting package...
# notarize.pl: checking status...
# notarize.pl: checking status...
# notarize.pl: checking status...
# (...)
# notarize.pl: checking status...
# notarize.pl: package successfully notarized

use strict;
use File::Which;

sub usage {
    print(STDERR "usage: notarize.pl <username> <password> <package>\n");
    print(STDERR "\tusername: apple id\n");
    print(STDERR "\tpassword: password of your apple id account\n");
    print(STDERR "\tpackage: package to be notarized\n");
    print(STDERR "\tnote: must be root\n");
    print(STDERR "\t      <password> can be a reference to a keychain item.\n");
    print(STDERR "\t      <password> as cleartext is not recommended.\n");
    print(STDERR "e.g.: \$ sudo notarize.pl foo\@bar.com \"\@keychain:PASSWORD\" OpenAFS.dmg\n\n");
    exit(1);
}

sub check_prerequisites {
    my (@ARGS) = @_;

    if ($> != 0) {
	print(STDERR "error: must be root\n\n");
	usage();
    }
    if (scalar @ARGS != 3) {
	print(STDERR "error: check arguments\n\n");
	usage();
    }
    if (!which('xcrun')) {
	print(STDERR "error: xcrun not found in \$PATH\n\n");
	usage();
    }
    if (not -e $ARGS[2]) {
	print(STDERR "error: package not found\n\n");
    }
}

sub submit_package {
    my ($username, $password, $package) = @_;

    print(STDOUT "notarize.pl: submitting package...\n");

    my $output = qx(xcrun altool -t osx -f "$package" --primary-bundle-id org.openafs.OpenAFS --notarize-app --username "$username" --password "$password" 2>&1);
    my $exitcode = $? >> 8;

    if ($exitcode) {
	print(STDERR "error: $exitcode\n");
	print(STDERR $output);
	exit(1);
    }
    # $output looks like the following sample:
    #
    # No errors uploading 'OpenAFS.dmg'.
    # RequestUUID = 565a4d1b-9608-47a6-aba9-53136c991bb8
    $output =~ m{RequestUUID = ([A-Za-z0-9\-]+)};
    if (not defined $1) {
	print(STDERR "error: uuid not found\n");
	exit(1);
    }
    return $1;
}

sub check_status {
    my ($username, $password, $uuid) = @_;
    my $output;
    my $status;
    my $exitcode;

    while (1) {
	print(STDOUT "notarize.pl: checking status...\n");
	$output = qx(xcrun altool --notarization-info "$uuid" --username "$username" --password "$password" 2>&1);
	$exitcode = $? >> 8;

	if ($exitcode) {
	    print(STDERR "error: $exitcode\n");
	    print(STDERR $output);
	    exit(1);
	}
	# $output looks like the following samples:
	#
	# First, second, ..., (N-1)'th attempts:
	#
	# No errors getting notarization info.
	#
	# Date: 2019-11-26 21:07:46 +0000
	# Hash: 4e10ebb01518de9eb007d4579006acda2d6ff773fe040d97786bcc686ec93gg1
	# RequestUUID: 565a4d1b-9608-47a6-aba9-53136c991bb8
	# Status: in progress
	#
	# N'th attempt:
	#
	# No errors getting notarization info.
	#
	# Date: 2019-11-26 21:07:46 +0000
	# Hash: 4e10ebb01518de9eb007d4579006acda2d6ff773fe040d97786bcc686ec93gg1
	# RequestUUID: 565a4d1b-9608-47a6-aba9-53136c991bb8
	# Status: in progress
	# Status Code: 0
	# Status Message: Package Approved
	$output =~ m{Status Code: (\d+)};
	if (defined $1) {
	    $status = $1;
	    last;
	}
	sleep(5);
    }
    if ($status) {
	print(STDERR "error: $status (uuid: $uuid)\n");
	print(STDERR $output);
	exit(1);
    }
}

sub notarize_package {
    my ($package, $uuid) = @_;

    print(STDOUT "notarize.pl: notarizing package...\n");

    my $output = qx(xcrun stapler staple -v "$package" 2>&1);
    my $exitcode = $? >> 8;

    if ($exitcode) {
	print(STDERR "error: package could not be notarized (uuid: $uuid)\n");
	print(STDERR $output);
	exit(1);
    }
    print(STDOUT "notarize.pl: package successfully notarized\n");
}

sub main {
    my (@ARGS) = @_;

    check_prerequisites(@ARGS);
    my $username = $ARGS[0];
    my $password = $ARGS[1];
    my $package = $ARGS[2];

    my $uuid = submit_package($username, $password, $package);
    check_status($username, $password, $uuid);
    notarize_package($package, $uuid);

    exit(0);
}
main(@ARGV);
