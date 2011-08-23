#!/usr/bin/perl
# make_h_tree.pl
# Generate an h tree that includes the appropriate sys headers
#
# Usage: make_h_tree.pl ${SRC} ...
#
# The specified makefiles will be scanned for variable values.  The h
# directory will be created under the current directory and populated with
# stubs that include the actual header file for every header included by any
# source file in the ${SRC} directories.  This is an ugly hack to work around
# the naming of header files using h instead of their proper names elsewhere
# in the code.

use IO::File;

if (@ARGV < 1) {
  die "Usage: $0 SRC ...\n";
}

%remap = ('h' => 'sys');
foreach $src (keys %remap) {
    mkdir($src, 0777) or die "$src: $!\n";
%seen = ();
@q = map { glob ("$_/*.[Sc]") } @ARGV;
  while (@q) {
    $src = shift @q;
    $content = new IO::File($src, O_RDONLY) or die "$src: $!\n";
  LINE:
    while (<$content>) {
      chomp;
      if (/^\s*\#\s*include\s*[<\"](?:\.\.\/)?([^\/>\"]*)(.*?)[>\"]/) {
	$inc = "$1$2";
	if (exists $seen{$inc}) {
	  next;
	} elsif (exists $remap{$1}  &&  $2 !~ /.\//) {
	  $H = new IO::File("$inc", O_WRONLY|O_CREAT|O_TRUNC, 0666)
	    or die "$inc: $!\n";
	  print $H "#include <sys$2>\n";
	  $H->close() or die "$inc: $!\n";
          $seen{$inc} = 1;
	}
      }
    }
  }
}
