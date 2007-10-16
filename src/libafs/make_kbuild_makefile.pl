#!/usr/bin/perl
# make_kbuild_makefile.pl
# Generate a Makefile for use with the Linux 2.6+ kernel build system
#
# Usage: make_kbuild_makefile.pl ${KDIR} ${TARG} Makefiles...
#
# The specified makefiles will be scanned for variable values
# The module ${TARG} will be built in ${TOP_SRCDIR}/src/libafs/${KDIR}.
# It will include objects listed in ${AFSAOBJS} and ${AFSNONFSOBJS}
# Appropriate source files for each object will be symlinked into ${KDIR}
# EXTRA_CFLAGS will be set to ${CFLAGS} ${COMMON_INCLUDE}

# Produces ${KDIR}/Makefile, suitable for use with kbuild

use IO::File;


if (@ARGV < 3) {
  die "Usage: echo objects... | $0 KDIR TARG Makefiles...\n";
}

($KDIR, $TARG, @Makefiles) = @ARGV;
$TARG =~ s/\.k?o$//;

## Read in all of the Makefiles given on the command line
## Our ultimate goal is to find the correct source file for each object.
## We make the following assumptions:
## - Every variable is defined before it is used.
## - Each of our objects has exactly one dependency, which is the name
##   of the source file that needs to be symlinked into $KDIR
foreach $mf (@Makefiles) {
  $F = new IO::File($mf, O_RDONLY) or die "$mf: $!\n";
  $text = '';
  while (<$F>) {
    chomp;
    $text .= $_;
    next if $text =~ s/\\$/ /;                 ## Continuation
    if ($text =~ /^#/) { $text = ''; next }    ## Comment
    #print STDERR "<< $text\n";

    $text =~ s/\$\((\w+)\)/$vars{$1}/g;        # Substitute variables
    $text =~ s/\$\{(\w+)\}/$vars{$1}/g;
    #print STDERR ">> $text\n";

    if ($text =~ /^\s*(\S+)\s*=/) {            ## Variable definition
      ($key, $value) = ($1, $');
      $value =~ s/^\s*//;                      # Remove leading and
      $value =~ s/\s*$//;                      # trailing whitespace
      $vars{$key} = $value;                    # Store it
    }
    elsif ($text =~ /^(\S+\.o):\s*(\S+\.c)/) {    ## Dependency
      $deps{$1} = $2;
    }
    elsif ($text =~ /^(\S+\.o):\s*(\S+\.s)/) {    ## Dependency
      $deps{$1} = $2;
    }
    $text = '';
  }
  $F->close();
}


$KDIR = "$vars{TOP_OBJDIR}/src/libafs/$KDIR";
@objects = (split(' ', $vars{AFSAOBJS}), split(' ', $vars{AFSNONFSOBJS}));

$MV = new IO::File("$vars{TOP_OBJDIR}/src/config/Makefile.version", O_RDONLY)
        or die "$vars{TOP_OBJDIR}/src/config/Makefile.version: $!\n";
while (<$MV>) {
  s#AFS_component_version_number#$KDIR/AFS_component_version_number#g;
  $MakefileVersion .= $_;
}
$MV->close();

if (! -d $KDIR) {
  mkdir($KDIR, 0777) or die "$KDIR: $!\n";
}


foreach (@objects) {
  die "No source known for $_\n" unless exists $deps{$_};
  if($deps{$_} =~ /\.s$/) {
     ($src = $_) =~ s/\.o$/.S/;
  } else {
     ($src = $_) =~ s/\.o$/.c/;
  }
  if (-e "$KDIR/$src" || -l "$KDIR/$src") {
    unlink("$KDIR/$src") or die "$KDIR/$src: $!\n";
  }
  next unless $deps{$_} =~ m#/#;
  symlink($deps{$_}, "$KDIR/$src") or die "$KDIR/$src: $!\n";
}

foreach $src (qw(h sys netinet)) {
  if (-e "$KDIR/$src" || -l "$KDIR/$src") {
    unlink("$KDIR/$src") or die "$KDIR/$src: $!\n";
  }
  symlink("$vars{LINUX_KERNEL_PATH}/include/linux", "$KDIR/$src")
    or die "$KDIR/$src: $!\n";
}

$cflags = "$vars{CFLAGS} $vars{COMMON_INCLUDE}";
$cflags =~ s#-I(?!/)#-I$KDIR/#g;
$cflags =~ s/\s+/ \\\n /g;
$F = new IO::File("$KDIR/Makefile", O_WRONLY|O_CREAT|O_TRUNC, 0666)
     or die "$KDIR/Makefile: $!\n";
print $F "EXTRA_CFLAGS=$cflags\n";
print $F "obj-m := $TARG.o\n";
print $F "$TARG-objs := ", join("\\\n $_", @objects), "\n";
print $F "\n$MakefileVersion\n";
$F->close() or die "$KDIR/Makefile: $!\n";

