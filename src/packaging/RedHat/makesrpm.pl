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
my $cellservdb = shift;

if (!$srcball && !$docball) {
  printf "Usage:  makesrpm <src.tar.bz2> <doc.tar.bz2> [<relnotes> [<changelog> [<cellservdb>]]]\n";
  exit(1);
}

if (! -f $srcball) {
  die "Unable to open $srcball\n";
}

my $tmpdir = File::Temp::tempdir(CLEANUP => 1);

system("tar -C $tmpdir -xvjf $srcball '\*/configure.ac' ".
       "'\*/src/packaging/RedHat' ".
       "'\*/.version' ".
       "'\*/build-tools' > /dev/null")==0
  or die "Unable to unpack src tar ball\n";

my $dirh = IO::Dir->new($tmpdir);
my $vdir;
while (defined($vdir = $dirh->read) && $vdir=~/^\./) {};

die "Unable to find unpacked source code\n" if !$vdir;

my $srcdir = $tmpdir."/".$vdir;

# Work out which version we're dealing with from the configure.ac file
my $afsversion;
my $linuxver;
my $linuxrel;
my $fh = new IO::File $srcdir."/configure.ac"
  or die "Unable to find unpacked configure.ac file";
while(<$fh>) {
  next if (/^\s*\#/);

  if (/AM_INIT_AUTOMAKE\(openafs,(.*)\)/) {
    $afsversion = $1;
    next;
  }
}
undef $fh;

if (not defined($afsversion)) {
  $afsversion = `"$srcdir/build-tools/git-version" "$srcdir"`;
}

# Build the Linux version and release information from the package version
# We need to handle a number of varieties of package version -
# Normal: 1.7.0
# Prereleases: 1.7.0pre1
# Development trees: 1.7.0dev
# and RPMS which are built from trees midway between heads, such as 
# 1.7.0-45-gabcdef or 1.7.0pre1-37-g12345 or 1.7.0dev-56-g98765

if ($afsversion=~m/(.*)(pre[0-9]+)/) {
    $linuxver=$1;
    $linuxrel="0.$2";
} elsif ($afsversion=~m/(.*)dev/) {
    $linuxver=$1;
    $linuxrel="0.dev";
} else {
    $linuxver=$afsversion;
    $linuxrel=1;
}

if ($afsversion=~m/(.*)-([0-9]+)-(g[a-f0-9]+)$/) {
    $linuxver = $1 if ($linuxver eq $afsversion);
    $linuxrel.=".$2.$3";
}

print "Linux release is $linuxrel\n";
print "Linux version is $linuxver\n";

# Figure out a major, minor and release so that we know which version we're
# building, and therefore what the srpm is going to be called
$linuxver=~/([0-9]+)\.([0-9]+)\.([0-9]+)/;
my $major = $1;
my $minor = $2;
my $patchlevel = $3;

# Build the RPM root

print "Building version $afsversion\n";
File::Path::mkpath([ $tmpdir."/rpmdir/SPECS",
		     $tmpdir."/rpmdir/SRPMS",
		     $tmpdir."/rpmdir/SOURCES"], 0, 0755);

File::Copy::copy($srcball, 
		 $tmpdir."/rpmdir/SOURCES/openafs-$afsversion-src.tar.bz2")
  or die "Unable to copy $srcball into position\n";

File::Copy::copy($docball,
		 $tmpdir."/rpmdir/SOURCES/openafs-$afsversion-doc.tar.bz2")
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
       "sed -e 's/\@VERSION\@/$afsversion/g' ".
       "    -e 's/\@LINUX_PKGVER\@/$linuxver/g' ".
       "    -e 's/\@LINUX_PKGREL\@/$linuxrel/g' ".
       "    -e 's/\%define afsvers.*/%define afsvers $afsversion/g' ".
       "    -e 's/\%define pkgvers.*/%define pkgvers $linuxver/g' > ".
       $tmpdir."/rpmdir/SPECS/openafs.spec") == 0
  or die "sed failed : $!\n";

if ($cellservdb) {
    File::Copy::copy($cellservdb,
		     $tmpdir."/rpmdir/SOURCES/$cellservdb")
	or die "Unable to copy $cellservdb into position\n";
} else {
    system("cd ".$tmpdir."/rpmdir/SOURCES && wget `cat ".$srcdir."/src/packaging/RedHat/openafs.spec.in |grep dl/cellservdb |awk '{print \$2}'`")
}

if ($relnotes) {
  File::Copy::copy($relnotes,
		   $tmpdir."/rpmdir/SOURCES/RELNOTES-$afsversion")
  or die "Unable to copy $relnotes into position\n";
} else {
  print "WARNING: No release notes provided. Using empty file\n";
  system("touch $tmpdir/rpmdir/SOURCES/RELNOTES-$afsversion");
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
system("rpmbuild -bs --nodeps --define \"dist %undefined\" ".
       "--define \"build_modules 0\" ".
       "--define \"_topdir $tmpdir/rpmdir\" ".
       "$tmpdir/rpmdir/SPECS/openafs.spec > /dev/null") == 0
  or die "rpmbuild failed : $!\n";

# Copy it out to somewhere useful
if (!defined($major) || $major > 1 || ($major == 1 && $minor >= 6)) {
  File::Copy::copy("$tmpdir/rpmdir/SRPMS/openafs-$linuxver-$linuxrel.src.rpm",
	           "openafs-$linuxver-$linuxrel.src.rpm")
    or die "Unable to copy output RPM : $!\n";

  print "SRPM is openafs-$linuxver-$linuxrel.src.rpm\n";
} else {
  File::Copy::copy("$tmpdir/rpmdir/SRPMS/openafs-$linuxver-1.$linuxrel.src.rpm",
	           "openafs-$linuxver-1.$linuxrel.src.rpm")
    or die "Unable to copy output RPM : $!\n";

  print "SRPM is openafs-$linuxver-1.$linuxrel.src.rpm\n";
}


