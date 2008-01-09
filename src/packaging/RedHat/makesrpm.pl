#!/usr/bin/perl

use strict;
use warnings;

use IO::Dir;
use IO::File;
use File::Path;
use File::Copy;
use File::Temp;

# Build an SRPM for OpenAFS, given a src and doc tarball, release notes,
# and ChangeLog.

my $srcball = shift;
my $docball = shift;
my $relnotes = shift;
my $changelog = shift;

if (!$srcball && !$docball) {
  printf "Usage:  <version> <src.tar.gz> <doc.tar.gz> [<relnotes> [<changelog>]]\n";
  exit(1);
}

if (! -f $srcball) {
  die "Unable to open $srcball\n";
}

my $tmpdir = File::Temp::tempdir(CLEANUP => 1);

system("tar -C $tmpdir -xvjf $srcball \*/configure.in ".
       "\*/src/packaging/RedHat > /dev/null")==0
  or die "Unable to unpack src tar ball\n";

my $dirh = IO::Dir->new($tmpdir);
my $vdir;
while (defined($vdir = $dirh->read) && $vdir=~/^\./) {};

die "Unable to find unpacked source code\n" if !$vdir;

my $srcdir = $tmpdir."/".$vdir;

# Work out which version we're dealing with from the configure.in file
my $version;
my $fh = new IO::File $srcdir."/configure.in"
  or die "Unable to find unpacked configure.in file";
while(<$fh>) {
  if (/AM_INIT_AUTOMAKE\(openafs,(.*)\)/) {
    $version = $1;
    last;
  }
}
undef $fh;

# Build the RPM root

print "Building version $version\n";
File::Path::mkpath([ $tmpdir."/rpmdir/SPECS",
		     $tmpdir."/rpmdir/SRPMS",
		     $tmpdir."/rpmdir/SOURCES"], 0, 0755);

File::Copy::copy($srcball, 
		 $tmpdir."/rpmdir/SOURCES/openafs-$version-src.tar.bz2")
  or die "Unable to copy $srcball into position\n";

File::Copy::copy($docball,
		 $tmpdir."/rpmdir/SOURCES/openafs-$version-doc.tar.bz2")
  or die "Unable to copy $docball into position\n";


# Populate it with all the stuff in the packaging directory, except the 
# specfile
my $pkgdirh = IO::Dir->new($srcdir."/src/packaging/RedHat")
  or die "Unable to find RedHat packaging directory\n";
my $file;
while (defined($file = $pkgdirh->read)) {
  if (-f $srcdir."/src/packaging/RedHat/".$file) {
     next if $file eq "openafs.spec.in";

     print "Copying $file into place\n";
     File::Copy::copy($srcdir."/src/packaging/RedHat/".$file, 
		      $tmpdir."/rpmdir/SOURCES/".$file);
  }
}
undef $dirh;

# Some files need particular modes.
chmod 0755, $tmpdir."/rpmdir/SOURCES/openafs-kernel-version.sh";
chmod 0755, $tmpdir."/rpmdir/SOURCES/openafs-kvers-is.sh";

# Create the specfile. Use sed for this, cos its easier
system("cat ".$srcdir."/src/packaging/RedHat/openafs.spec.in | ".
       "sed -e 's/\@VERSION\@/$version/g' ".
       "    -e 's/\%define afsvers.*/%define afsvers $version/g' ".
       "    -e 's/\%define pkgvers.*/%define pkgvers $version/g' > ".
       $tmpdir."/rpmdir/SPECS/openafs.spec") == 0
  or die "sed failed : $!\n";

if ($relnotes) {
  File::Copy::copy($relnotes,
		   $tmpdir."/rpmdir/SOURCES/RELNOTES-$version")
  or die "Unable to copy $relnotes into position\n";
} else {
  print "WARNING: No release notes provided. Using empty file\n";
  system("touch $tmpdir/rpmdir/SOURCES/RELNOTES-$version");
}

if ($changelog) {
  File::Copy::copy($changelog,
		   $tmpdir."/rpmdir/SOURCES/ChangeLog")
  or die "Unable to copy $changelog into position\n";
} else {
  print "WARNING: No changelog provided. Using empty file\n";
  system("touch $tmpdir/rpmdir/SOURCES/ChangeLog");
}

# Build an RPM
system("rpmbuild -bs --define \"_topdir $tmpdir/rpmdir\" ".
       "$tmpdir/rpmdir/SPECS/openafs.spec > /dev/null") == 0
  or die "rpmbuild failed : $!\n";

# Copy it out to somewhere useful
File::Copy::copy("$tmpdir/rpmdir/SRPMS/openafs-$version-1.1.src.rpm",
	         "openafs-$version-1.1.src.rpm")
  or die "Unable to copy output RPM : $!\n";

print "SRPM is openafs-$version-1.1.src.rpm\n";

