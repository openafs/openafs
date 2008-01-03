#!/usr/bin/perl

use strict;
use warnings;

my $specdir="/usr/src/redhat/SPECS";

my $fedoraversion=`rpm -q fedora-release --queryformat %{VERSION}`;
die "Unable to determine fedora version" if !$fedoraversion;

my %list;
foreach my $variant ('', 'PAE', 'kdump', 'xen') {
  print "Working out variants - currently processing $variant\n";
  my $append = "";
  $append="-$variant" if $variant;
  my @results=split(' ', `rpm -q --queryformat "%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH} " kernel$append-devel`);
  if ($?) {
    print "RPM lookup for variant $variant failed. Skipping\n";
    next;
  }
  print join(' ', @results)."\n";
  foreach my $package (@results) {
    print "Got $package\n";
    $package=~/([^\-]*\-[^\-]*)\.([^\.]*)$/;
    my ($version, $arch) = ($1,$2);
    die "Couldn't extract version and architecture" if !$version or !$arch;
    my @archs=map {/^.*\-([^\-]*)/;$1;} split(' ',`ls -d /usr/src/kernels/$version$append-*`);
    foreach my $arch (@archs) {
      print "Adding $variant, version $version for $arch\n";
      $list{$arch} = {} if !$list{$arch};
      $list{$arch}{$version} =[] if !$list{$arch}{$version};
      push @{$list{$arch}{$version}}, $variant;
    }
  } 
}
# Build the base package
print "Building the base system\n";
system("rpmbuild -ba --define \"fedorakmod 1\" --define \"osvers fc$fedoraversion\" $specdir/openafs.spec") == 0 or exit 1;

print "Building kernel modules\n";

foreach my $arch (keys(%list)) {
  foreach my $version (keys(%{$list{$arch}})) {
    my $variants = join(" ", map {$_ or "''"} @{$list{$arch}{$version}});
    print "Building with $version for $arch with variants $variants\n";
    system("rpmbuild -bb --define \"fedorakmod 1\" --define \"osvers fc$fedoraversion\" --target $arch --define \"build_modules 1\" --define \"kernvers $version\" --define \"kvariants $variants\" $specdir/openafs.spec") == 0 or exit 1;
  }
}

