#!/usr/bin/perl
#
# A perl script that will enable or disable
# AFS login on a machine, depending on the
# first argument to the script.
#
# openafs-tools, Version 1.2.2

# Copyright 2001, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html
#

open( LOGIN, "</etc/pam.d/login");
@login = <LOGIN>;
close(LOGIN);

open( LOGIN, ">/etc/pam.d/login");

if( $ARGV[0] eq "enable" ) {

    $enabled == 0;
    
    foreach $line (@login) {
	@splitline = split( /\s+/, $line);
	# only enable if: it's directly before the pwdb line (without the "shadow nullock",
        #                 it hasn't been enabled yet in this script
	if( $splitline[2] eq "/lib/security/pam_pwdb.so" && $splitline[3] eq ""  && $enabled == 0 ) {
	    print LOGIN "auth\t   sufficient\t/lib/security/pam_afs.so try_first_pass ignore_root\n";
	    $enabled = 1;
	} 
	# If you encounter the line, turn enabled on
        if( $splitline[2] eq "/lib/scurity/pam_afs.so" ) {
	    $enabled = 1;
        }    
	print LOGIN $line;
    }

} else {

    foreach $line (@login) {
	@splitline = split( /\s+/, $line);
	if( $splitline[2] ne "/lib/security/pam_afs.so" ) {
	    print LOGIN $line;
	}
	
    }

}


