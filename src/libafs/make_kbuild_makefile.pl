#!/usr/bin/perl
# make_kbuild_makefile.pl
# Generate a Makefile for use with the Linux 2.6+ kernel build system
#
# Usage: make_kbuild_makefile.pl ${KDIR} ${TARG} Makefiles...
#
# The specified makefiles will be scanned for variable values
# The module ${TARG} will be built in ${TOP_SRCDIR}/src/libafs/${KDIR}.
# It will include objects listed in ${AFSAOBJS} and ${AFSNFSOBJS}
# The afspag.ko module will be built from objects listed in ${AFSPAGOBJS}.
# Appropriate source files for each object will be symlinked into ${KDIR}
# EXTRA_CFLAGS will be set to ${CFLAGS} ${COMMON_INCLUDE}
# Any CFLAGS_* and AFLAGS_* variables will be copied

# Produces ${KDIR}/Makefile, suitable for use with kbuild

use IO::File;


if (@ARGV < 3) {
  die "Usage: $0 KDIR TARG Makefiles...\n";
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
@libafs_objs = (split(' ', $vars{AFSAOBJS}), split(' ', $vars{AFSNFSOBJS}));
@afspag_objs = (split(' ', $vars{AFSPAGOBJS}));

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

%all_objs = map(($_ => 1), @libafs_objs, @afspag_objs);

foreach (keys %all_objs) {
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

%remap = ('h' => 'linux', 'netinet' => 'linux', 'sys' => 'linux');
if (-f "$vars{LINUX_KERNEL_PATH}/include/linux/types.h") {
  foreach $src (keys %remap) {
    if (-e "$KDIR/$src" || -l "$KDIR/$src") {
      unlink("$KDIR/$src") or die "$KDIR/$src: $!\n";
    }
    symlink("$vars{LINUX_KERNEL_PATH}/include/linux", "$KDIR/$src")
      or die "$KDIR/$src: $!\n";
  }
} else {
  foreach $src (keys %remap) {
    system ('rm', '-rf', "$KDIR/$src"); # too crude?
    mkdir("$KDIR/$src", 0777) or die "$KDIR/$src: $!\n";
  }
  %seen = ();
  @q = <$KDIR/*.[Sc]>;
  @include_dirs = map { /^\// ? $_ : "$KDIR/$_" }
    split /[\s\\]*-I/, $vars{COMMON_INCLUDE};
  push @include_dirs, "$vars{TOP_SRCDIR}/../include/rx", "$vars{TOP_SRCDIR}/rx";
  while (@q) {
    $src = shift @q;
    $content = new IO::File($src, O_RDONLY) or die "$src: $!\n";
  LINE:
    while (<$content>) {
      chomp;
      if (/^\s*#\s*include\s*[<"](?:\.\.\/)?([^\/>"]*)(.*?)[>"]/) {
	$inc = "$1$2";
	if (exists $seen{$inc}) {
	  next;
	} elsif (exists $remap{$1}  &&  $2 !~ /.\//) {
	  $H = new IO::File("$KDIR/$inc", O_WRONLY|O_CREAT|O_TRUNC, 0666)
	    or die "$KDIR/$inc: $!\n";
	  print $H "#include <linux$2>\n";
	  $H->close() or die "$KDIR/$inc: $!\n";
	} else {
	  for $dir (@include_dirs) {
	    if (-f "$dir/$inc") {
	      push @q, "$dir/$inc";
	      $seen{$inc} = 1;
	      next LINE;
	    }
	  }
	  if ($1 =~ /^(arpa|asm|.*fs|i?net|kern|ksys|linux|mach|rpc|scsi|vm)$/
	      ||  !length($2)) {
	    # Safe to ignore silently.
	  } else {
	    warn "Ignoring $_ ($inc not found)\n";
	  }
	}
	$seen{$inc} = 1;
      } elsif (/^\s*#\s*include/) {
	warn "Ignoring $_ (unrecognized syntax)\n";
      }
    }
  }
}

$cflags = "$vars{CFLAGS} $vars{COMMON_INCLUDE}";
$cflags =~ s#-I(?!/)#-I$KDIR/#g;
$cflags =~ s/\s+/ \\\n /g;
$F = new IO::File("$KDIR/Makefile", O_WRONLY|O_CREAT|O_TRUNC, 0666)
     or die "$KDIR/Makefile: $!\n";
foreach (sort keys %vars) {
  next unless /^[AC]FLAGS_/;
  print $F "$_ = $vars{$_}\n";
}
print $F "EXTRA_CFLAGS=$cflags\n";
print $F "obj-m := $TARG.o afspag.o\n";
print $F "$TARG-objs := ", join("\\\n $_", @libafs_objs), "\n";
print $F "afspag-objs := ", join("\\\n $_", @afspag_objs), "\n";
print $F "\n$MakefileVersion\n";
$F->close() or die "$KDIR/Makefile: $!\n";

