#!/usr/bin/perl
#
# A perl script that will replace the line in /etc/fstab
# corresponding to the device given by the first argument,
# with a new line mounting that device to the second
# argument, or add it if necessary.
#
# openafs-tools, Version 1.2.2

# Copyright 2001, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html
#

open( FSTAB, "</etc/fstab");
@fstab = <FSTAB>;
close(FSTAB);

open( FSTAB, ">/etc/fstab");

$replaced = 0;

foreach $line (@fstab) {
  @splitline = split(/\s+/, $line);
  if( $splitline[0] eq "/dev/$ARGV[0]" ) {
      print FSTAB "/dev/$ARGV[0]\t\t$ARGV[1]\t\t\text2\tdefaults     0  2\n";
      $replaced = 1;
  } else {
      print FSTAB $line;
  }
}

if( $replaced == 0 ) {
    print FSTAB "/dev/$ARGV[0]\t\t$ARGV[1]\t\t\text2\tdefaults     0  2\n";
}
