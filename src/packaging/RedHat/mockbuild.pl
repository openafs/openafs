#!/usr/bin/perl
# mockbuild - mass RPM build driver for use with mock and many kernel-devel
#             RPMs
# from Simon Wilkinson for OpenAFS
# for "modern" yum, use "showdupesfromrepos=1" in all cfg files in /etc/mock

use strict;
use warnings;

use Getopt::Long;
use File::Path;
use IO::Dir;

my $suser="nsu";
my $rootbase="/var/lib/mock/";
my $resultbase="/tmp/result/";
my $stashbase="/disk/scratch/repository/";
my $mockcommand = "/usr/bin/mock";
my $buildall = 0;

my @newrpms;

my %platconf = ( "fedora-5-i386" => { osver => "fc5",
				      kmod => '1',
				      basearch => 'i386',
				      updaterepo => 'updates-released',
				      results => 'fc5/i386' },
		 "fedora-5-x86_64" => { osver => "fc5",
				       kmod => '1',
				       basearch => 'x86_64',
				       updaterepo => 'updates-released',
				       results => 'fc5/x86_64' },
		 "fedora-6-i386" => { osver => "fc6", 
				      kmod => '1', 
				      basearch => 'i386',
				      updaterepo => 'updates-released',
				      results => "fc6/i386" },
		 "fedora-6-x86_64" => { osver => "fc6",
					kmod => '1',
					basearch => 'x86_64',
				        updaterepo => 'updates-released',
					results => "fc6/x86_64" },
		 "fedora-7-i386" => { osver => "fc7", 
				      kmod => '1', 
				      basearch => 'i386',
				      updaterepo => 'updates-released',
				      results => "fc7/i386" },
		 "fedora-7-x86_64" => { osver => "fc7",
					kmod => '1',
					basearch => 'x86_64',
				        updaterepo => 'updates-released',
					results => "fc7/x86_64" },
		 "fedora-8-i386" => { osver => "fc8", 
				      kmod => '1', 
				      basearch => 'i386',
				      updaterepo => 'updates-released',
				      results => "fc8/i386" },
		 "fedora-8-x86_64" => { osver => "fc8",
					kmod => '1',
					basearch => 'x86_64',
				        updaterepo => 'updates-released',
					results => "fc8/x86_64" },
		 "centos-4-i386" => { osver => "el4",
				     kmod => '1',
				     basearch => 'i386',
				     updaterepo => 'update',
				     results => 'el4/i386' },
		 "centos-4-x86_64" => { osver => "el4",
				       kmod => '1',
				       basearch => 'x86_64',
				       updaterepo => 'update',
				       results => "el4/x86_64" },
		 "centos-5-i386" => { osver => "el5", 
				      kmod => '1', 
				      basearch => 'i386',
				      updaterepo => 'update',
				      results => "el5/i386" },
		 "centos-5-x86_64" => { osver => "el5",
					kmod => '1',
				   	basearch => 'x86_64',
				        updaterepo => 'update',
					results => "el5/x86_64" },
#		 "fedora-development-i386" => { osver => "fcd",
#					  kmod => '1',
#					  basearch => 'i386',
#					  results => 'fedora-devel/i386'},
#		 "fedora-development-x86_64" => { osver => "fcd",
#					    kmod => '1',
#					    basearch => 'x86_64',
#					    results => 'fedora-devel/x86_64'} 
);

# The following are kernels that we can't successfully build modules against
# due to issues in the packaged kernel-devel RPM.

my %badkernels = (
	"2.6.21-2950.fc8" => { "xen" => 1} # Missing build ID
);

my @platforms = @ARGV;
my $srpm = pop @platforms;

if (!$srpm || $#platforms==-1) {
  print "Usage: $0 <platform> [<platform> [<platform> ...] ]  <srpm>\n";
  print "    Platform may be:\n";
  foreach ("all", sort(keys(%platconf))) { print "        ".$_."\n"; };
  exit(1);
}

my $oafsversion = `rpm -q --queryformat=%{VERSION} -p $srpm` or die $!;
chomp $oafsversion;
my $oafsrelease = `rpm -q --queryformat=%{RELEASE} -p $srpm` or die $!;
chomp $oafsrelease;
$oafsrelease=~s/^[^\.]*\.(.*)$/$1/;

print "Release is $oafsrelease\n";

if ($platforms[0] eq "all" and $#platforms == 0) {
  @platforms = keys(%platconf);
}

foreach my $platform (@platforms) {
  print "*******************************************************************\n";
  print "Building for $platform\n";
  die "Unknown platform $platform" if !$platconf{$platform};
  my $osver = $platconf{$platform}{'osver'};
  my $root = $rootbase.$platform."/root";
  my $mockresults = $rootbase.$platform."/result";
  my $resultdir = $resultbase.$platconf{$platform}{'results'};
  my $basearch = $platconf{$platform}{'basearch'};
  my $rpmstashdir = $stashbase.$platconf{$platform}{'results'}."/";

  my $yumcachedir;
  if (exists($platconf{$platform}{'updaterepo'})) {
    $yumcachedir = $rootbase."cache/".$platform."/yum_cache/".
		   $platconf{$platform}{'updaterepo'}."/packages/";
  }

  File::Path::mkpath($resultdir, 0, 0755);

  if (system($mockcommand, "-r", $platform, "init") != 0) {
    die "Initialising the root failed: $?\n";
  }

  my %modulelist;

  print "-------------------------------------------------------------------\n";
  print "Finding available kernel modules\n";

  my $arbitraryversion = "";
  my $modules=`$suser -c 'yum --installroot $root provides kernel-devel'`;
  if ($modules eq "") {
      $modules=`$suser -c 'yum -d 2 --installroot $root provides kernel-devel'`;
      my $modulen;
      my %modulel;
      foreach $modulen (split(/\n/, $modules)) {
	  my ($pk, $colon, $comment)=split(/\s+/, $modulen);
	  if ($pk =~ /^kernel/) {
	      $modulel{$pk} = "$pk";
	  } 
      }
      $modulen=join(" ", keys(%modulel));
      $modules=`$suser -c 'yum --installroot $root list $modulen'`;
  }
  foreach my $module (split(/\n/, $modules)) {
      my ($package, $version, $repo)=split(/\s+/, $module);
      my ($arch) = ($package=~/\.(.*)$/);
      my ($variant) = ($package=~/kernel-(.*)-devel/);
      $variant = "" if !defined($variant);
      next if ($package !~ /^kernel/);
      next if ($arch eq "noarch");
      next 
	  if (exists($badkernels{$version}) && ($badkernels{$version}{$variant}));
      if ($platform=~/fedora-5/) {
	  next if ($variant eq "xen0"); # Fedora 5 has some bad xen0 kernel-devels
	  next if ($variant eq "smp");
      }
      print "$arch : $variant : $version\n";
      $modulelist{$arch} ={} if !$modulelist{$arch};
      $modulelist{$arch}{$version} = {} if !$modulelist{$arch}{$version};
      $modulelist{$arch}{$version}{$variant} = 1;
      $arbitraryversion = $version;
  }

  if (!$arbitraryversion) {
    die "Unable to find a valid kernel-devel package for $platform\n";
  }

  print "-------------------------------------------------------------------\n";
  print "Building the userland RPMs\n";
  my @rpms = ('', '-authlibs', '-authlibs-devel', '-client', '-compat',
	      '-debuginfo', '-devel', '-docs', '-kernel-source', '-kpasswd',
	      '-krb5', '-server');

  my $missing = 0;
  foreach my $rpm (@rpms) {
    if (! -f $resultdir."/openafs".$rpm."-".$oafsversion."-".$osver.".".
	     $oafsrelease.".".$basearch.".rpm") {
      $missing++;
      print $resultdir."/openafs".$rpm."-".$oafsversion."-".$osver.".".
	    $oafsrelease.".".$basearch.".rpm is missing!\n"
    }
  }
  if ($missing) {
    system($mockcommand." -r ".$platform." rebuild ".
		        ' --define "fedorakmod 1" '.
		        ' --define "kernvers '.$arbitraryversion.'" '.
		        ' --define "osvers '.$osver.'" '.
		        ' --define "build_modules 0" '.
		        ' --define "build_userspace 1" '.
		        ' --define "build_authlibs 1" '.
		        $srpm) == 0
      or die "build failed with : $!\n";
    foreach my $rpm (@rpms) {
      system("cp ".$mockresults."/openafs".$rpm."-".$oafsversion."-".
		   $osver.".".$oafsrelease.".".$basearch.".rpm ".
		   $resultdir) == 0
          or die "Copy failed with : $!\n";
      push @newrpms, $mockresults."/openafs".$rpm."-".$oafsversion."-".
		     $osver.".".$oafsrelease.".".$basearch.".rpm";
    }
  } else {
    print "All userland RPMs present for $platform. Skipping build\n";
  }

   print "-------------------------------------------------------------------\n";
  print "Building kernel modules\n";

 foreach my $arch (keys(%modulelist)) {
    foreach my $version (keys(%{$modulelist{$arch}})) {
      my $kversion = $version;
      $kversion=~s/-/_/g;
      my @tobuild;

      if ($buildall) {
	@tobuild = keys(%{$modulelist{$arch}{$version}});
      } else {
        foreach my $variant (keys(%{$modulelist{$arch}{$version}})) {
          my $dvariant=$variant;
          $dvariant.="-" if ($dvariant);
          if (!-f $resultdir."/kmod-openafs-".$dvariant.
		  $oafsversion."-".$oafsrelease.".".$kversion.".".
		  $arch.".rpm") {
	    push @tobuild, $variant;
	    print $resultdir."/kmod-openafs-".$dvariant.
                  $oafsversion."-".$oafsrelease.".".$kversion.".".
                  $arch.".rpm is missing\n";
          }
        }
      }

      if ($#tobuild>=0) {
        my $variants = join(" ", map {$_ or '\\\\\"\\\\\"'} @tobuild);
        print "Building $arch module for kernel $version with variants $variants\n";
        system ("setarch $arch $mockcommand -r $platform rebuild ".
			     " --arch ".$arch.
			     ' --define "fedorakmod 1" '.
			     ' --define "osvers '.$osver.'" '.
			     ' --define "kernvers '.$version.'" '.
			     ' --define "kvariants '.$variants.'" '.
			     ' --define "build_modules 1" '.
			     ' --define "build_userspace 0" '.
			     $srpm) == 0
          or die "Build failed with : $!\n";
        foreach my $variant (@tobuild) {
          if ($variant) {
            $variant.="-";
          }
          system("cp ".$mockresults."/kmod-openafs-".$variant.$oafsversion."-".$oafsrelease.".".$kversion.".".$arch.".rpm $resultdir") == 0
            or die "Copy failed with : $!\n";
	  push @newrpms, $mockresults."/kmod-openafs-".$variant.$oafsversion."-".$oafsrelease.".".$kversion.".".$arch.".rpm";
        }
      } else {
         print "All kernel modules already built for $version on $arch\n";
      }
    }
  }
  print "-------------------------------------------------------------------\n";
  print "Creating repository data\n";
  system ("cd $resultdir; createrepo .") == 0
    or die "Unable to build repository data\n";

  if ($yumcachedir) {
    print "-------------------------------------------------------------------\n";
    print "Stashing kernel-devel RPMs\n";
  
    my $changed;
    my $dirh = IO::Dir->new($yumcachedir);
    if (defined($dirh)) {
      my $file;
      while (defined($file = $dirh->read)) {
        if ( $file=~/^kernel-devel/ &&
              -f $yumcachedir.$file && ! -f $rpmstashdir.$file) {
          print "Stashing $file for later use\n";
          system("cp ".$yumcachedir.$file." ".$rpmstashdir.$file) == 0
            or die "Copy failed with : $!\n";
          $changed++;
        }
      }
    }
 
    if ($changed) {
      print "Updating stash repodata\n";
      system ("cd $rpmstashdir; createrepo .") == 0
        or die "Unable to update RPM stash repository data\n";
    }
  }
}

print "=====================================================================\n";
print "All builds complete\nBuilt:\n";
print join("\n",@newrpms);

