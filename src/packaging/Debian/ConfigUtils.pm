# This is -*- perl -*-

package Debian::OpenAFS::ConfigUtils;

use strict;
use vars qw( @ISA @EXPORT @unwinds);
@ISA = qw(Exporter);
require Exporter;
@EXPORT = qw(@unwinds run unwind);

sub run ($) {
  print join(' ', @_);
  print "\n";
  system (@_)  == 0
    or die "Failed: $?\n";
}

# This subroutine takes a command to run in case of failure.  After
# each succesful step, this routine should be run with a command to
# undo the successful step.

	 sub unwind($) {
	   push @unwinds, $_[0];
	 }

1;
