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
# $ sudo notarize.pl keychain-profile OpenAFS.dmg
#
# notarize.pl: processing package...
# notarize.pl: package successfully notarized

use strict;
use File::Which;

sub usage {
    print(STDERR "usage: notarize.pl <profile> <package>\n");
    print(STDERR "\tprofile: keychain-profile for the credentials to be used\n");
    print(STDERR "\tpackage: package to be notarized\n");
    print(STDERR "\tnote: must be root\n");
    print(STDERR "e.g.: \$ sudo notarize.pl keychain-profile OpenAFS.dmg\n\n");
    exit(1);
}

sub check_prerequisites {
    my ($profile, $package) = @_;

    if ($> != 0) {
	print(STDERR "error: must be root\n\n");
	usage();
    }
    if (!which('xcrun')) {
	print(STDERR "error: xcrun not found in \$PATH\n\n");
	usage();
    }

    # Check if the given keychain-profile exists
    my $output = qx(xcrun notarytool history --keychain-profile "$profile" 2>&1);
    my $exitcode = $? >> 8;

    if ($exitcode) {
	print(STDERR "error: $exitcode\n");
	print(STDERR $output);
	exit(1);
    }

    # Check if the given package exists
    if (not -e $package) {
	print(STDERR "error: package not found\n\n");
	exit(1);
    }
}

sub process_package {
    my ($profile, $package) = @_;

    print(STDOUT "notarize.pl: processing package...\n");

    # returns after submitting and processing the package, or times out if it
    # takes longer than 5 minutes
    my $output = qx(xcrun notarytool submit "$package" --keychain-profile "$profile" --wait --timeout 5m 2>&1);
    my $exitcode = $? >> 8;

    if ($exitcode) {
	print(STDERR "error: $exitcode\n");
	print(STDERR $output);
	exit(1);
    }
    # $output looks like the following sample:
    #
    # Conducting pre-submission checks for OpenAFS.dmg and initiating connection
    # to the Apple notary service...
    # Submission ID received
    #   id: fe4249d2-c3f7-428e-8bcd-8af665e57189
    # Successfully uploaded file
    #   id: fe4249d2-c3f7-428e-8bcd-8af665e57189
    #   path: ./OpenAFS.dmg
    # Waiting for processing to complete. Wait timeout is set to 300.0 second(s).
    # Current status: In Progress... Current status: Accepted...... Processing complete
    #   id: fe4249d2-c3f7-428e-8bcd-8af665e57189
    #   status: Accepted
    $output =~ m{id: ([A-Za-z0-9\-]+)};
    if (not defined $1) {
	print(STDERR "error: uuid not found\n");
	exit(1);
    }
    return $1;
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

    if (scalar @ARGS != 2) {
	print(STDERR "error: check arguments\n\n");
	usage();
    }
    my $profile = $ARGS[0];
    my $package = $ARGS[1];

    check_prerequisites($profile, $package);

    my $uuid = process_package($profile, $package);
    notarize_package($package, $uuid);

    exit(0);
}
main(@ARGV);
