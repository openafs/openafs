#!/usr/bin/perl

# mkdest: make AFS platform directory for build

if ($#ARGV >= 0) {
  $srcdir = $ARGV[0];
}
else {
  $srcdir = "/tmp/src";
}
$dir = $ENV{PWD};

print "Create build tree from the AFS source tree $srcdir\n";
print "Create links in platform tree $dir\n";

print "continue (y/n) ? ";

chop ($ans = <STDIN>);
exit if ($ans ne "y");

mkdir "dest", 0755 || die "Can't create directory dest\n";
mkdir "obj", 0755 || die "Can't create directory obj\n";
chdir "obj" || die "Can't change to directory obj\n";

&dodir($srcdir, "..");

sub dodir {
  local($dir,$destpath) = @_;
  print "$dir\n";
  local($a);
  local($i);

  #if (-e "$dir/RCS") {
    ## Make RCS link and check out all files in this directory
    #system("ln -sf $dir/RCS");
    #system("co RCS/*");
  #}

  opendir(DIR,$dir) || die "Can't open directory $dir\n";
  local(@allfiles) = readdir(DIR);
  closedir(DIR);

  $thisdir = &lastcomp($dir);
  system("ln -s $destpath/dest DEST");
  system("ln -s $destpath/../src/$thisdir SRC");

  foreach $a (@allfiles) {
    #print "$a\n";
    if (! -d "$dir/$a") {
      system("ln -s SRC/$a");
      next;
    }
    next if $a eq '.';
    next if $a eq '..';
    next if $a eq 'RCS';

    # Make this subdirectory on local copy
    mkdir $a, 0755 || die "Can't create directory $dir/$a\n";
    chdir $a || die "Can't change to directory $dir/$a\n";

    # Recursively process this directory
    &dodir("$dir/$a", "$destpath/..");
    chdir '..';
  }
}

sub lastcomp {
  local($dir) = @_;

  $_ = $dir;
  if (/$srcdir\/(\S+)/) {
    $dir = $1;
  }
  else {
    $dir = $_;
  }
  return $dir;
}
