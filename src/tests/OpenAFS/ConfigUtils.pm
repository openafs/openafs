# This is -*- perl -*-

package OpenAFS::ConfigUtils;

use strict;
use vars qw( @ISA @EXPORT @unwinds $debug);
@ISA = qw(Exporter);
require Exporter;
@EXPORT = qw(@unwinds run unwind);

$debug = 0;

#--------------------------------------------------------------------
# run(cmd) - run a command. Takes a command to be executed or 
#            a perl code reference to be eval'd.
sub run($) {
  my $cmd = shift;
  if (ref($cmd) eq 'CODE') {
    eval { &$cmd };
	if ($@) {
      die "ERROR: $@\n";
	}
  }
  else {
    if ($debug) {
      print "debug: $cmd\n";
    }
    my $rc = system($cmd);
    unless ($rc==0) {
      die "ERROR: Command failed: $cmd\nerror code=$?\n";
    }
  }
}

# This subroutine takes a command to run in case of failure.  After
# each succesful step, this routine should be run with a command to
# undo the successful step.
sub unwind($) {
   push @unwinds, $_[0];
}

1;
